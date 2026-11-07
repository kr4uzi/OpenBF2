// BF2Audio sound instances; excluded from host images to keep DLL-owned state separate.
#if defined(BF2_IMAGE_BF2AUDIO)
#include "Sound.h"
#include "Sample.h"
#include "SoundFile.h"
#include "SoundEvaluator.h"
#include "BfSoundEngine.h"
#include "SoundTrace.h"
#include "SoundEngineMath.h"
#include "../../Platform/MsvcRand.h"   // [CHOSEN, not recovered] MSVCR71 rand range (RAND_MAX 0x7FFF)

#include <climits>
#include <cmath>
#include <cstdlib>
#include <string>

namespace dice {
namespace hfe {

namespace {
const char* const kFile = "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\SoundEngine\\Sound.cpp";
}

int g_soundCount = 0;
// Sound-node transform and velocity snapshot.
SoundNodeSnapshot::SoundNodeSnapshot()
{
    for (int i = 0; i < 16; ++i)
        m_matrix.m[i] = 0.0f;
    m_matrix.m[0] = 1.0f; m_matrix.m[5] = 1.0f; m_matrix.m[10] = 1.0f; m_matrix.m[15] = 1.0f;
    m_velocity.x = 0.0f; m_velocity.y = 0.0f; m_velocity.z = 0.0f;
    m_obstruction = 0.0f;
}

SoundNodeSnapshot g_nullSoundNode;
// Sound-template defaults.


SoundTemplate::SoundTemplate()
    : m_refCount(0), m_loopCount(1), m_minDistance(1.0f), m_halfVolumeDistance(0.0f),
      m_maxDistance(0.0f), m_rolloff(0.0f), m_is3d(true), m_stopType(1), m_reverbLevel(1.0f),
      m_volume(1.0f), m_pitch(1.0f), m_pan(0.5f), m_volumeEvaluator(0), m_pitchEvaluator(0),
      m_radius(0.0f), m_coneInnerAngle(360.0f), m_coneOuterAngle(360.0f), m_coneOuterVolume(1.0f),
      m_globalEffectLevel(1.0f), m_flags(0), m_volumeGroup(0)
{
    m_position.x = m_position.y = m_position.z = 0.0f;
    m_direction.x = m_direction.y = m_direction.z = 0.0f;
    m_maxSamples[0] = m_maxSamples[1] = m_maxSamples[2] = INT_MAX;
    recomputeDistances();
}

// Releases the files, then the two evaluators.
SoundTemplate::~SoundTemplate()
{
    clearFiles();
    deleteEvaluator(m_pitchEvaluator);
    deleteEvaluator(m_volumeEvaluator);
}

// Final release deletes the template.
void SoundTemplate::addRef()  { ++m_refCount; }
void SoundTemplate::release()
{
    if (--m_refCount <= 0)
        delete this;
}

// a single name goes through the array form; null clears.
void SoundTemplate::setFilename(const char* name)
{
    if (name)
        setFilenames(&name, 1);
    else
        setFilenames(0, 0);
}

const char* SoundTemplate::getFilename() const
{
    return m_fileNames.empty() ? "" : m_fileNames[0];
}

// Every name becomes (or finds) an engine SoundFile; the file learns which
// quality levels want it from this template's per-quality sample limits, and a template with any
// flag set marks its files "keep at 22 kHz". A name the engine cannot create is logged and a null
// entry keeps the index stable. Empty names are null entries too, silently.
void SoundTemplate::setFilenames(const char** names, int count)
{
    clearFiles();
    for (int i = 0; i < count; ++i)
    {
        const char* name = names[i];
        SoundFile* file = 0;
        if (name && *name)
        {
            file = g_soundEngineInstance->getOrCreateSoundFile(std::string(name));
            if (file)
            {
                if (i < m_maxSamples[0]) file->m_qualityMask |= 1;
                if (i < m_maxSamples[1]) file->m_qualityMask |= 2;
                if (i < m_maxSamples[2]) file->m_qualityMask |= 4;
                if (getFlags())
                    file->m_flags2 |= 1;
            }
            else
            {
                g_soundTrace->warning(sndFormat("Failed to create soundFile for: %s", names[i]), kFile, 924);
            }
        }
        m_files.push_back(file);
        m_fileNames.push_back(file ? file->m_name.c_str() : 0);
    }
}

const char** SoundTemplate::getFilenames(int* count) const
{
    if (count)
        *count = (int) m_fileNames.size();
    if (m_fileNames.empty())
        return 0;
    return const_cast<const char**>(&m_fileNames[0]);
}

// the volume feeds the rolloff, so the distances are recomputed.
void SoundTemplate::setVolume(float v)
{
    m_volume = clamp01(v);
    recomputeDistances();
}

// A null point array does not delete the old evaluator - it drops the pointer.
void SoundTemplate::setVolumeEnvelope(float xMin, float xMax, float yMin, float yMax, bool randomize,
                                      unsigned int count, const SoundEnvelopePoint* points)
{
    if (points)
    {
        SoundEvaluator* e = new SoundEvaluator(xMin, xMax, yMin, yMax, randomize, count,
                                               reinterpret_cast<const EnvelopePoint*>(points));
        assignEvaluator(m_volumeEvaluator, e);
    }
    else
    {
        m_volumeEvaluator = 0;
    }
}

const SoundEnvelopePoint* SoundTemplate::getVolumeEnvelope(float* xMin, float* xMax, float* yMin, float* yMax,
                                                           bool* randomize, unsigned int* count)
{
    SoundEvaluator* e = m_volumeEvaluator;
    if (!e)
        return 0;
    if (xMin) *xMin = e->m_xMin;
    if (xMax) *xMax = e->m_xMax;
    if (yMin) *yMin = e->m_yMin;
    if (yMax) *yMax = e->m_yMax;
    if (randomize) *randomize = e->m_randomize;
    if (count) *count = (unsigned int) e->m_points.size();
    return e->m_points.empty() ? 0 : reinterpret_cast<const SoundEnvelopePoint*>(&e->m_points[0]);
}

void SoundTemplate::setPitch(float p)
{
    m_pitch = p >= 0.01f ? p : 0.01f;
}

void SoundTemplate::setPitchEnvelope(float xMin, float xMax, float yMin, float yMax, bool randomize,
                                     unsigned int count, const SoundEnvelopePoint* points)
{
    if (points)
    {
        SoundEvaluator* e = new SoundEvaluator(xMin, xMax, yMin, yMax, randomize, count,
                                               reinterpret_cast<const EnvelopePoint*>(points));
        assignEvaluator(m_pitchEvaluator, e);
    }
    else
    {
        m_pitchEvaluator = 0;
    }
}

const SoundEnvelopePoint* SoundTemplate::getPitchEnvelope(float* xMin, float* xMax, float* yMin, float* yMax,
                                                          bool* randomize, unsigned int* count)
{
    SoundEvaluator* e = m_pitchEvaluator;
    if (!e)
        return 0;
    if (xMin) *xMin = e->m_xMin;
    if (xMax) *xMax = e->m_xMax;
    if (yMin) *yMin = e->m_yMin;
    if (yMax) *yMax = e->m_yMax;
    if (randomize) *randomize = e->m_randomize;
    if (count) *count = (unsigned int) e->m_points.size();
    return e->m_points.empty() ? 0 : reinterpret_cast<const SoundEnvelopePoint*>(&e->m_points[0]);
}

// eight groups; anything else is ignored.
void SoundTemplate::setVolumeGroup(int g)
{
    if ((unsigned int) g < 8u)
        m_volumeGroup = g;
}

// Clamps properties to [0, 1].
void SoundTemplate::setPan(float p)               { m_pan = clamp01(p); }
void SoundTemplate::setReverbLevel(float r)       { m_reverbLevel = clamp01(r); }
void SoundTemplate::setConeOuterVolume(float v)   { m_coneOuterVolume = clamp01(v); }
void SoundTemplate::setGlobalEffectLevel(float l) { m_globalEffectLevel = clamp01(l); }

void SoundTemplate::setMinDistance(float d)        { m_minDistance = d; recomputeDistances(); }
void SoundTemplate::setHalfVolumeDistance(float d) { m_halfVolumeDistance = d; recomputeDistances(); }

// 0, 1 or 2; negatives clamp to 0, anything above to 2.
void SoundTemplate::setStopType(int t)
{
    if (t < 3)
        m_stopType = t < 0 ? 0 : t;
    else
        m_stopType = 2;
}

void SoundTemplate::setPosition(const Vec3& p)
{
    m_position = p;
}

// Stores zero for squared length <= 0.01; otherwise normalizes direction.
void SoundTemplate::setDirection(const Vec3& d)
{
    const float lengthSq = d.x * d.x + d.y * d.y + d.z * d.z;
    if (lengthSq <= 0.01f)
    {
        m_direction.x = 0.0f; m_direction.y = 0.0f; m_direction.z = 0.0f;
    }
    else
    {
        m_direction = d;
        sndmath::normalizeIfNeeded(m_direction);
    }
}

void SoundTemplate::setRadius(float r)
{
    m_radius = r;
    if (r < 0.0f)
        m_radius = 0.0f;
}

// Clamps cone angle to [0, 360].
void SoundTemplate::setConeInnerAngle(float a)
{
    if (a >= 360.0f) a = 360.0f;
    if (a < 0.0f)    a = 0.0f;
    m_coneInnerAngle = a;
}

void SoundTemplate::setConeOuterAngle(float a)
{
    if (a >= 360.0f) a = 360.0f;
    if (a < 0.0f)    a = 0.0f;
    m_coneOuterAngle = a;
}

// every file counts one more user.
void SoundTemplate::setIsUsed()
{
    for (unsigned int i = 0; i < m_files.size(); ++i)
        if (m_files[i])
            m_files[i]->setIsUsed();
}

// a negative limit means unlimited.
void SoundTemplate::setMaxSamples(SoundQuality q, int n)
{
    m_maxSamples[q] = n < 0 ? INT_MAX : n;
}

// Outside interactive mode a template without flags just answers whether the
// file exists; otherwise the file is loaded on demand. No null test on the file - an index past
// the quality limit dereferences null, as shipped.
bool SoundTemplate::checkSampleAvailable(int index) const
{
    if (!g_soundEngineInstance->getInteractiveMode() && !(getFlags() & 1))
        return getSoundFile(index) != 0;
    SoundFile* file = getSoundFile(index);
    if (file->m_loaded)
        return getSoundFile(index) != 0;
    file->load(g_soundEngineInstance->m_sampleRate, g_soundEngineInstance->m_bits);
    if (file->m_loaded)
        return getSoundFile(index) != 0;
    return false;
}

ISound* SoundTemplate::createSound()
{
    Sound* s = new Sound(this);
    s->addRef();
    return s;
}

SoundFile* SoundTemplate::getSoundFile(int index) const
{
    if (index < 0)
        return 0;
    const int count = (int) m_files.size();
    int limit = m_maxSamples[g_soundEngineInstance->getQualityLevel()];
    if (limit >= count)
        limit = count;
    if (index >= limit)
        return 0;
    return m_files[index];
}

// Multiplies by the pitch rather than dividing - transcribed as shipped.
int SoundTemplate::getLengthFrames(int index) const
{
    SoundFile* file = m_files[index];
    if (!file)
        return 0;
    if (m_loopCount)
        return (int) (30.0 / (double) (unsigned int) file->m_format.samplesPerSecond
                      * (double) (unsigned int) (m_loopCount * file->m_numSamples) * m_pitch);
    return INT_MAX;
}

int SoundTemplate::getSampleCount() const
{
    const int count = (int) m_files.size();
    const int limit = m_maxSamples[g_soundEngineInstance->getQualityLevel()];
    return limit >= count ? count : limit;
}

// an inner angle below 360 and a direction longer than the same 0.01 threshold
// setDirection applies (a template with an angle but no direction has no cone).
bool SoundTemplate::hasCone() const
{
    if (!(m_coneInnerAngle < 360.0f))
        return false;
    const float lengthSq = m_direction.x * m_direction.x + m_direction.y * m_direction.y + m_direction.z * m_direction.z;
    return lengthSq > 0.01f;
}

// The rolloff is what makes the volume halve at the half-volume distance
// (0.511872 = the inverse-distance-clamped model's constant); the cut-off is where the
// "max-distance-cutoff" fraction of the volume remains.
void SoundTemplate::recomputeDistances()
{
    if (m_minDistance < 0.01f)
        m_minDistance = 0.01f;
    if (m_minDistance + 0.01f >= m_halfVolumeDistance)
    {
        m_halfVolumeDistance = 0.0f;
        m_rolloff = 1.0f;
    }
    else
    {
        const float v = m_volume * 0.51187199f;
        if (v <= 1.1920929e-7f)
            m_rolloff = 1.0f;
        else
            m_rolloff = m_minDistance / (m_halfVolumeDistance - m_minDistance) * (1.0f / v - 1.0f);
    }
    m_maxDistance = (1.0f / (g_soundProperties->get(SoundProperties::MaxDistanceCutoff) * m_rolloff)
                     - 1.0f / m_rolloff + 1.0f) * m_minDistance;
}

void SoundTemplate::clearFiles()
{
    for (unsigned int i = 0; i < m_files.size(); ++i)
        if (m_files[i])
            m_files[i]->release();
    std::vector<SoundFile*>().swap(m_files);
    std::vector<const char*>().swap(m_fileNames);
}


// Sound


Sound::Sound(SoundTemplate* t)
    : m_refCount(0), m_playingCount(0), m_template(t), m_parent(&g_nullSoundNode),
      m_volumeEvaluator(0), m_volumeParam(0), m_pitchEvaluator(0), m_pitchParam(0),
      m_obstructionEvaluator(0), m_loopOverride(0), m_lastAttachFrame(-1), m_slotIndex(0),
      m_snapshot(0), m_soundFlags(0), m_lastRandomIndex(-1), m_is1P(false)
{
    m_samples[0] = m_samples[1] = 0;
    m_slotDistance[0] = m_slotDistance[1] = 0.0f;
    m_slotFlags[0] = m_slotFlags[1] = 0;
    m_userData[0] = m_userData[1] = m_userData[2] = 0;
    m_template->addRef();
    ++g_soundCount;
}

// The snapshot is freed raw (no destructor in the image - it has none to run).
Sound::~Sound()
{
    if (m_template)
        m_template->release();
    --g_soundCount;
    delete m_snapshot;
}

int Sound::addRef()
{
    return ++m_refCount;
}

// The last release deletes a silent sound; a sounding one is detached to a
// snapshot node instead - and when that is impossible (looping, or custom evaluators) its samples
// are force-stopped so that detachSample can finish the job.
int Sound::release()
{
    const int r = --m_refCount;
    if (r <= 0)
    {
        if (m_playingCount <= 0)
        {
            delete this;
        }
        else if (!detachToSnapshot() && g_soundEngineInstance->isInitialized())
        {
            g_soundEngineInstance->stopSound(this, true);
        }
        return 0;
    }
    return r;
}

void Sound::play()
{
    const int loops = (int) m_template->getLoopCount();
    const int index = randomIndex();
    if (g_soundEngineInstance->isInitialized() && m_template->getSoundFile(index))
    {
        m_loopOverride = loops;
        g_soundEngineInstance->playSample(this, index, 0);
    }
}

void Sound::play(int loops)
{
    const int index = randomIndex();
    if (g_soundEngineInstance->isInitialized() && m_template->getSoundFile(index))
    {
        m_loopOverride = loops;
        g_soundEngineInstance->playSample(this, index, 0);
    }
}

void Sound::playAt(const Vec3& pos)
{
    const int loops = (int) m_template->getLoopCount();
    const int index = randomIndex();
    if (g_soundEngineInstance->isInitialized() && m_template->getSoundFile(index))
    {
        m_loopOverride = loops;
        g_soundEngineInstance->playSample(this, index, &pos);
    }
}

void Sound::playAt(const Vec3& pos, int loops)
{
    const int index = randomIndex();
    if (g_soundEngineInstance->isInitialized() && m_template->getSoundFile(index))
    {
        m_loopOverride = loops;
        g_soundEngineInstance->playSample(this, index, &pos);
    }
}

// After initialization, returns 1 for an existing sample index even if scheduling fails.
int Sound::playSample(int index)
{
    const int loops = (int) m_template->getLoopCount();
    if (!g_soundEngineInstance->isInitialized())
        return 0;
    if (!m_template->getSoundFile(index))
        return 0;
    m_loopOverride = loops;
    g_soundEngineInstance->playSample(this, index, 0);
    return 1;
}

int Sound::playSample(int index, int loops)
{
    if (!g_soundEngineInstance->isInitialized())
        return 0;
    if (!m_template->getSoundFile(index))
        return 0;
    m_loopOverride = loops;
    g_soundEngineInstance->playSample(this, index, 0);
    return 1;
}

int Sound::playSampleAt(const Vec3& pos, int index)
{
    const int loops = (int) m_template->getLoopCount();
    if (!g_soundEngineInstance->isInitialized())
        return 0;
    if (!m_template->getSoundFile(index))
        return 0;
    m_loopOverride = loops;
    g_soundEngineInstance->playSample(this, index, &pos);
    return 1;
}

int Sound::playSampleAt(const Vec3& pos, int index, int loops)
{
    if (!g_soundEngineInstance->isInitialized())
        return 0;
    if (!m_template->getSoundFile(index))
        return 0;
    m_loopOverride = loops;
    g_soundEngineInstance->playSample(this, index, &pos);
    return 1;
}

void Sound::stop()
{
    if (g_soundEngineInstance->isInitialized())
        g_soundEngineInstance->stopSound(this, false);
}

void Sound::forceStop()
{
    if (g_soundEngineInstance->isInitialized())
        g_soundEngineInstance->stopSound(this, true);
}

void Sound::setParent(ISoundNode* n)
{
    m_parent = n ? n : &g_nullSoundNode;
}

// Uses the sound's own evaluator when set, otherwise the template's.
void Sound::setVolumeEvaluator(ISoundEvaluator* e, const float* param)
{
    m_volumeEvaluator = e;
    m_volumeParam = param;
}

ISoundEvaluator* Sound::getVolumeEvaluator() const
{
    if (m_volumeEvaluator)
        return m_volumeEvaluator;
    return (ISoundEvaluator*) m_template->getVolumeEvaluator();
}

void Sound::setPitchEvaluator(ISoundEvaluator* e, const float* param)
{
    m_pitchEvaluator = e;
    m_pitchParam = param;
}

ISoundEvaluator* Sound::getPitchEvaluator() const
{
    if (m_pitchEvaluator)
        return m_pitchEvaluator;
    return (ISoundEvaluator*) m_template->getPitchEvaluator();
}

// the three seconds (intro, loop, outro) of the file at `index`.
void* Sound::getSampleInfo(int index) const
{
    if (m_template->getSoundFile(index))
        return &m_template->getSoundFile(index)->m_sampleInfo;
    return 0;
}

// Returns the parent's transform/velocity, or identity/zero defaults without a parent.
const Mat4* Sound::getParentMatrix() const
{
    if (!m_parent)
        return &g_identityMatrix;
    const Mat4* m = &m_parent->getTransform();
    return m ? m : &g_identityMatrix;
}

const Vec3* Sound::getParentVelocity() const
{
    if (!m_parent)
        return &g_zeroVector;
    const Vec3* v = m_parent->getVelocity();
    return v ? v : &g_zeroVector;
}

// The template volume, times the evaluator's answer for the value the
// parameter pointer holds (or `x`), times the engine's group volume.
float Sound::computeVolume(float x, int sample)
{
    float v = m_template->getVolume();
    ISoundEvaluator* e = getVolumeEvaluator();
    if (e)
    {
        if (m_volumeParam)
            x = *m_volumeParam;
        v = e->getValue(x, (int) std::floor(x) + sample) * v;
    }
    return g_soundEngineInstance->getVolumeForGroup(m_template->getVolumeGroup()) * v;
}

// As above for the pitch, clamped to 0.3..2.0.
float Sound::computePitch(float x, int sample)
{
    float p = m_template->getPitch();
    ISoundEvaluator* e = getPitchEvaluator();
    if (e)
    {
        if (m_pitchParam)
            x = *m_pitchParam;
        p = e->getValue(x, (int) std::floor(x) + sample) * p;
    }
    if (p >= 2.0f)
        p = 2.0f;
    if (p < 0.3f)
        return 0.3f;
    return p;
}

// Forget the slot the sample held; and if that was the last live sample of a
// sound already released, the sound dies here.
void Sound::detachSample(Sample* s)
{
    int i = 0;
    for (; m_samples[i] != s; ++i)
    {
        if (i + 1 >= 2)
            goto count;
    }
    m_samples[i] = 0;
    m_slotIndex = i;
count:
    if (m_playingCount-- == 1 && !m_refCount)
        delete this;
}

// The template position (plus the play*At offset) through the parent's
// transform; a zero local position takes the translation row directly.
Vec3* Sound::computePosition(Vec3* out, const Vec3* offset)
{
    if (!m_parent)
    {
        *out = m_template->getPosition();
        return out;
    }
    Vec3 p = m_template->getPosition();
    if (offset)
    {
        p.x += offset->x; p.y += offset->y; p.z += offset->z;
    }
    if (sndmath::nearlyEqual(p, g_zeroVector))
    {
        const Mat4* m = getParentMatrix();
        out->x = m->m[12]; out->y = m->m[13]; out->z = m->m[14];
        return out;
    }
    const Mat4* m = getParentMatrix();
    sndmath::transformPoint(*m, p, *out);
    return out;
}

// Which of the two sample slots a new play* takes. Mode is the low byte of the
// sound flags: 0 (or no position) = the round-robin slot; 1 = score the two slots against the new
// sound's placement relative to the listener (front/back, left/right, age, distance) and take the
// better one, if any beats the newcomer; anything else = refuse.
int Sound::chooseSlot(const Vec3* position)
{
    int mode = (int) (unsigned char) m_soundFlags;
    if (!position)
        mode = 0;
    if (mode == 0)
        return m_slotIndex;
    if (mode != 1)
        return -1;

    Vec3 p;
    computePosition(&p, position);
    const Vec3* listener = g_soundEngineInstance->getListenerPosition();
    const Vec3* facing   = g_soundEngineInstance->getListenerOrientationFacing();
    const Vec3* up       = g_soundEngineInstance->getListenerOrientationUp();
    Vec3 side;
    side.x = facing->z * up->y - facing->y * up->z;
    side.y = facing->x * up->z - up->x * facing->z;
    side.z = up->x * facing->y - facing->x * up->y;
    Vec3 d;
    d.x = p.x - listener->x;
    d.y = p.y - listener->y;
    d.z = p.z - listener->z;

    const float kFrontBack = g_soundProperties->get(SoundProperties::PrioKFrontBack);
    const int   front      = (d.z * facing->z + d.y * facing->y + d.x * facing->x < 0.0f) ? 0 : 2;
    const float kRightLeft = g_soundProperties->get(SoundProperties::PrioKRightLeft);
    const bool  right      = d.z * side.z + d.x * side.x + d.y * side.y > 0.0f;
    const int   current    = m_slotIndex;
    int slot = -1;
    const float distance   = std::sqrt(d.z * d.z + d.y * d.y + d.x * d.x);

    if (m_samples[current] == 0)
    {
        slot = current;
    }
    else
    {
        const float kAge       = g_soundProperties->get(SoundProperties::PrioKAge);
        const float ageOnlyDst = g_soundProperties->get(SoundProperties::PrioAgeOnlyDistance);
        const float kDistance  = g_soundProperties->get(SoundProperties::PrioKDistance);
        float fb = kFrontBack;
        if (!front)
            fb = -fb;
        const float newTerm  = distance > ageOnlyDst ? 1.0f / (ageOnlyDst - distance + 1.0f) : 1.0f;
        const float newScore = fb + newTerm * kDistance + kAge;
        float base[2];
        base[current]     = -kAge;
        base[1 - current] = 0.0f;
        float best = 1000.0f;
        for (int i = 0; i < 2; ++i)
        {
            float s = base[i];
            if (right == ((m_slotFlags[i] & 1) != 0))
                s -= kRightLeft;
            s = (m_slotFlags[i] & 2) ? s + kFrontBack : s - kFrontBack;
            const float term = distance > ageOnlyDst ? 1.0f / (ageOnlyDst - m_slotDistance[i] + 1.0f) : 1.0f;
            s += term * kDistance;
            if (newScore >= s && s < best)
            {
                best = s;
                slot = i;
            }
        }
        if (slot < 0)
            return slot;
    }
    m_slotDistance[slot] = distance;
    m_slotFlags[slot]    = front | (right ? 1 : 0);
    return slot;
}

// A random file index that differs from the last one; 0 when there is at most
// one. (The count is re-read on every retry, as shipped.)
int Sound::randomIndex()
{
    int n = m_template->getSampleCount();
    if (n <= 1)
        return 0;
    int r;
    do
    {
        n = m_template->getSampleCount();
        r = bf2crt::rand() % n;
    }
    while (r == m_lastRandomIndex);
    m_lastRandomIndex = r;
    return r;
}

// Refuse for a released sound without a snapshot, and for a second attach in
// the same engine update unless flag 0x100 allows it. A taken slot's sample is force-stopped.
bool Sound::attachSample(Sample* s)
{
    if ((m_refCount <= 0 && !m_snapshot)
        || (!(m_soundFlags & 0x100) && m_lastAttachFrame == g_soundEngineInstance->m_updateCount))
        return false;
    const int slot = chooseSlot(s->getPositionRef());
    if (slot < 0)
        return false;
    m_slotIndex = slot;
    if (m_samples[slot])
        g_soundEngineInstance->stopSample(m_samples[slot], true);
    m_samples[m_slotIndex] = s;
    m_slotIndex = (m_slotIndex + 1) % 2;
    m_lastAttachFrame = g_soundEngineInstance->m_updateCount;
    return true;
}

// Only a finite, evaluator-free sound can outlive its owner: its parent's
// transform and velocity are frozen into a fresh snapshot node, the sound is re-parented to it,
// and the obstruction evaluator's answer at the frozen position is frozen too.
bool Sound::detachToSnapshot()
{
    if (!m_loopOverride)
        return false;
    if (getVolumeEvaluator() != (ISoundEvaluator*) m_template->getVolumeEvaluator())
        return false;
    if (getPitchEvaluator() != (ISoundEvaluator*) m_template->getPitchEvaluator())
        return false;
    SoundNodeSnapshot* snapshot = new SoundNodeSnapshot;
    if (snapshot != m_snapshot)
        delete m_snapshot;
    m_snapshot = snapshot;
    snapshot->m_matrix   = *getParentMatrix();
    snapshot->m_velocity = *getParentVelocity();
    setParent(snapshot ? static_cast<ISoundNode*>(snapshot) : 0);
    if (getObstructionEvaluator())
    {
        ISoundObstructionEvaluator* e = getObstructionEvaluator();
        snapshot->m_obstruction = e->evaluate(this, (const Math::Vec3*) &snapshot->m_matrix.m[12]);
        setObstructionEvaluator(snapshot);
    }
    else
    {
        snapshot->m_obstruction = 0.0f;
    }
    return true;
}

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_BF2AUDIO
