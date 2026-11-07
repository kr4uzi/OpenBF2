// Bone-animation loading, frame storage, and pose sampling.

#include "BoneAnimation.h"
#include "AnimationManager.h"
#include <sstream>

#include "../../Core/Debug.h"
#include "../../IO/File/File.h"
#include "../../IO/Stream/IStream.h"

#include <cstring>                    // memcpy - the whole reader is one
#include <cmath>                      // modff / fabsf - the frame cache and getValue
#include <cfloat>                     // FLT_EPSILON - getValue's translation test
#include <cstdlib>                    // rand - getValue's root-bone jitter
#include "../../Math/Quat.h"          // Quat::slerp
#include "../../Math/Vec3.h"
#include "../../Platform/MsvcRand.h"  // [CHOSEN, not recovered] the client CRT's rand (MSVCR71)


namespace dice {
namespace anim {


// Initializes animation metadata. load() must populate sample buffers and counts before sampling.

BoneAnimation::BoneAnimation()
    : m_loaded(false)                     // +0x00
    , looping(true)                       // +0x01 the odd one out
    , originalLength(0.0f)                // +0x04
    , length(0.0f)                        // +0x08
    // m_invLength (+0x0C) deliberately not initialised - see the header.
    , m_blendInTime(0.0f)
    , m_lastTime(-1.0f)                   // BF800000h
    , m_baseBone(-1)
    , m_boneIdToIndex(0)
    , m_jitterChance(0)                   // +0x44
    , m_jitterAmplitude(0.0f)             // +0x48
    , m_memoryUsed(0)
{
    m_compressed.frameCount    = 0;       // +0x3C
    m_compressed.boneCount     = 0;       // +0x3E
    m_compressed.precisionBits = 15;      // +0x3F 0x0F
    m_compressed.boneData      = 0;       // +0x40
}


// Cleans up only loaded animations because loading initializes the bone-id storage.

BoneAnimation::~BoneAnimation()
{
    if (m_loaded)
        cleanup();
}


// Releases per-bone sample buffers and the channel array.

void BoneAnimation::cleanup()
{
    if (m_boneIdToIndex)
        operator delete[](m_boneIdToIndex);
    if (m_boneIds)
        operator delete[](m_boneIds);

    for (int i = 0; i < m_compressed.boneCount; ++i)
    {
        if (m_compressed.boneData[i].samples)
            operator delete[](m_compressed.boneData[i].samples);
    }
    if (m_compressed.boneData)
        operator delete[](m_compressed.boneData);

    m_memoryUsed = 0;                     // the only field cleared - see above
}


// Stores length and its reciprocal without checking for zero.

void BoneAnimation::setLength(float length_)
{
    length = length_;
    m_invLength = 1.0f / length_;         // unguarded - see above
}


// Version-4 .baf loader: quantized quaternion/translation channels at 24 frames per second.
// Shared file/cursor storage makes loading non-reentrant. Already-loaded animations return true.
// Missing files report a fatal error; a rejected version retains the allocated file buffer.
// The file flags are [UNRESOLVED]. This implementation bounds channel copies, unlike retail.
namespace {
// Shared parser cursors; loading is not reentrant.
unsigned char* s_fileBase = 0;
unsigned char* s_cursor   = 0;

template <typename T> T readField()
{
    T v;
    memcpy(&v, s_cursor, sizeof(T));
    s_cursor += sizeof(T);
    return v;
}
} // namespace

bool BoneAnimation::load(const std::string& path)
{
    if (m_loaded)
        return true;                       // idempotent - see above

    hfe::io::File file;
    if (!file.open(path, hfe::io::IFileSystem::FILEMODE_READ,
                   static_cast<hfe::io::IFileSystem::FileAccess>(0), 0))
    {
        // Error aborts. The return below never runs.
        hfe::Debug(hfe::DEBUGTYPE_6, "Animation/BoneAnimation/BoneAnimation.cpp", 122, "")
            << "BoneAnimation file " << path << " not found";
        return false;
    }

    hfe::io::IStream* stream = file.getStream();
    const unsigned int size = static_cast<unsigned int>(file.getSize());
    s_fileBase = new unsigned char[size];
    s_cursor   = s_fileBase;
    stream->read(s_cursor, size);          // the whole file, one call

    if (readField<int>() != 4)
        return false;                      // s_fileBase leaks here - dice's

    // The bone id list
    m_boneCount = readField<unsigned short>();
    m_boneIds   = new unsigned short[m_boneCount];
    m_maxBoneId = 0;
    m_memoryUsed += 2 * m_boneCount;
    m_minBoneId = 4711;                    // the joke constant, as an impossible minimum

    for (int i = 0; i < m_boneCount; ++i)
    {
        const unsigned short id = readField<unsigned short>();
        m_boneIds[i] = id;
        if (m_maxBoneId < id) m_maxBoneId = id;
        if (m_minBoneId > id) m_minBoneId = id;
    }

    // The reverse table
    // 0xFFFF for every ID this animation does not carry. See the header: this is where the
    // sentinel the skeleton's blended fan-out skips actually lives.
    m_boneIdToIndex = new unsigned short[m_maxBoneId + 1];
    m_memoryUsed += 2 * m_maxBoneId + 2;
    for (int i = 0; i <= m_maxBoneId; ++i)
        m_boneIdToIndex[i] = 0xFFFF;
    for (int i = 0; i < m_boneCount; ++i)
    {
        if (m_boneIds[i] <= m_maxBoneId)   // always true; dice tests it anyway
            m_boneIdToIndex[m_boneIds[i]] = static_cast<unsigned short>(i);
    }

    // The header tail
    const int frames = readField<int>();
    const unsigned char flags = readField<unsigned char>();
    m_compressed.frameCount    = static_cast<unsigned short>(frames);          // narrowed from 32 bits
    m_compressed.boneCount = static_cast<unsigned char>(m_boneCount);      // narrowed to 8 bits
    m_compressed.precisionBits        = flags;

    // The per-bone channel data
    m_compressed.boneData = new BoneData[m_boneCount];
    m_memoryUsed += 16 * m_boneCount;

    for (int b = 0; b < m_boneCount; ++b)
    {
        const unsigned short sampleCount = readField<unsigned short>();
        m_compressed.boneData[b].samples = new unsigned short[sampleCount];
        m_memoryUsed += 2 * sampleCount;

        unsigned int offset = 0;
        for (int c = 0; c <= 6; ++c)       // seven channels; six boundaries recorded
        {
            unsigned int count = readField<unsigned short>();
            // this clamp is this tree'S - see the block comment. dice copies unconditionally.
            if (offset + count > sampleCount)
                count = (offset < sampleCount) ? (sampleCount - offset) : 0;

            memcpy(m_compressed.boneData[b].samples + offset, s_cursor, 2 * count);
            s_cursor += 2 * count;
            offset += count;
            if (c <= 5)
                m_compressed.boneData[b].channelEnd[c] = static_cast<unsigned short>(offset);
        }
    }

    if (s_fileBase)
        operator delete[](s_fileBase);

    // The length
    // twenty-four frames PER second, and the file's length only becomes the active length when
    // nothing has set one - which is what makes `animationBundle.setAnimationLength` stick.
    originalLength = static_cast<float>(m_compressed.frameCount) / 24.0f;
    if (length == 0.0f)
    {
        length = originalLength;
        m_invLength = 1.0f / originalLength;
    }

    m_loaded = true;                       // last, and it is the gate for everything else
    return true;
}


// Cleans and reloads an already-loaded animation; returns false if it has never been loaded.

bool BoneAnimation::reload(const std::string& path)
{
    if (!m_loaded)
        return false;

    cleanup();
    m_loaded = false;
    load(path);                            // result discarded
    return true;
}

// seven bytes - empty on this build. The per-bone form below it is the real work.
void BoneAnimation::applyOnSkeleton(Skeleton& /*skeleton*/, float, float, int) { }


// Updates the shared frame cache for the requested sample time.

void BoneAnimation::updateFrameCache(float time)
{
    if (time == m_lastTime)
        return;

    const int frameCount = m_compressed.frameCount;
    const bool loops = looping;
    m_lastTime = time;
    const float normalised = time * m_invLength;
    int span = frameCount;
    m_frameA = -1;
    if (!loops)
    {
        span = frameCount - 1;
        if (normalised < 1.0f)
        {
            if (normalised <= 0.0f)
            {
                m_frameFraction = 0.0f;
                m_frameA = 0;
                m_frameB = 1;
            }
        }
        else
        {
            m_frameFraction = 1.0f;
            m_frameA = frameCount - 2;
            m_frameB = frameCount - 1;
        }
    }
    if (m_frameA == -1)
    {
        float integral;
        float fraction = modff(normalised, &integral);
        if (fraction < 0.0f)
            fraction = fraction + 1.0f;
        const float scaled = fraction * static_cast<float>(span);
        m_frameFraction = modff(scaled, &integral);
        m_frameA = static_cast<int>(integral);                    // cvttss2si - truncation
        if (m_frameA >= frameCount)
            m_frameA = 0;
        m_frameB = m_frameA + 1;
        if (m_frameB >= frameCount)
            m_frameB = 0;
    }
}


// Samples a bone's rotation and translation at time.
// A bone absent from the animation yields identity rotation and zero translation.

void BoneAnimation::getValue(int bone, float time, hfe::Quat& rotation, hfe::Vec3& position,
                             float jitter)
{
    if (!m_loaded)
        return;
    updateFrameCache(time);

    if (bone > m_maxBoneId || m_boneIdToIndex[bone] == 0xFFFF)
    {
        rotation.w = 1.0f;
        rotation.x = 0.0f;
        rotation.y = 0.0f;
        rotation.z = 0.0f;
        position.x = 0.0f;
        position.y = 0.0f;
        position.z = 0.0f;
        return;
    }
    const int index = m_boneIdToIndex[bone];

    hfe::Quat a;
    a.x = 0.0f; a.y = 0.0f; a.z = 0.0f; a.w = 1.0f;
    hfe::Quat b;
    b.x = 0.0f; b.y = 0.0f; b.z = 0.0f; b.w = 1.0f;
    m_compressed.GetQuat(index, m_frameA, a);
    m_compressed.GetQuat(index, m_frameB, b);
    rotation.slerp(a, b, m_frameFraction);

    hfe::Vec3 ta;
    ta.x = 0.0f; ta.y = 0.0f; ta.z = 0.0f;
    hfe::Vec3 tb;
    tb.x = 0.0f; tb.y = 0.0f; tb.z = 0.0f;
    m_compressed.GetTrans(index, m_frameA, ta);
    m_compressed.GetTrans(index, m_frameB, tb);

    const bool same = fabsf(ta.x - tb.x) <= FLT_EPSILON
                   && fabsf(ta.y - tb.y) <= FLT_EPSILON
                   && fabsf(ta.z - tb.z) <= FLT_EPSILON;
    if (!same)
    {
        const float fraction = m_frameFraction;
        position.x = ta.x + (tb.x - ta.x) * fraction;
        position.y = ta.y + (tb.y - ta.y) * fraction;
        position.z = ta.z + (tb.z - ta.z) * fraction;
    }
    else
    {
        position.x = ta.x;
        position.y = ta.y;
        position.z = ta.z;
    }

    if (bone == 0 && m_jitterChance != 0 && bf2crt::rand() % 10 < m_jitterChance)
    {
        const float amplitudeX = jitter * m_jitterAmplitude;
        position.x = amplitudeX * static_cast<float>(bf2crt::rand() % 256 - 128) * 0.00390625f + position.x;
        const float amplitudeY = jitter * m_jitterAmplitude;
        position.y = 0.00390625f * (static_cast<float>(bf2crt::rand() % 256 - 128) * amplitudeY) + position.y;
    }
}

// Preserves the authored noice spelling; noise fields require positive frequency. Writes no trailing separator.
void BoneAnimation::makeScript(hfe::io::IStream* stream)
{
    std::ostringstream output;
    output << "animationSystem.createAnimation " << animationManager->getName(this) << std::endl;
    if (!looping) output << "animationManager.looping 0" << std::endl;
    if (m_baseBone >= 0)
        output << "animationManager.ignoreMotherOrientation " << m_baseBone << std::endl;
    if (static_cast<float>(m_jitterChance) > 0.0f)
    {
        output << "animationManager.noiceFreq " << m_jitterChance << std::endl;
        output << "animationManager.noiceAmplitude " << m_jitterAmplitude << std::endl;
    }
    if (originalLength != length) output << "animationManager.length " << length << std::endl;
    if (m_blendInTime != 0.0f) output << "animationManager.fadeInTime " << m_blendInTime << std::endl;
    const std::string text = output.str();
    stream->write(text.c_str(), static_cast<unsigned int>(text.size()));
}

} // namespace anim
} // namespace dice
