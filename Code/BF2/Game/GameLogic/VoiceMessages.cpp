// Collects level voice definitions and resolves message IDs to randomized speech ranges.
// Radio samples for each message/voice pair must remain contiguous, and team sample lists
// stay aligned with speech texts. Dedicated servers retain the registry through the null sound engine.

#include "VoiceMessages.h"
#include "../../Core/StringUtil.h"      // getLowerCase - addHelpMessage's key
#include "../../Sound/BfSound.h"        // g_bfSound - engine() (+0x28) for the quality test
#include "../../Sound/NullSound.h"      // ISoundEngine / ISoundTemplate / ISound
#include "../../Math/Vec3.h"            // the listener position - playSoldierVO, getFreeVOSoundIndex
#include "../Objects/Game/Soldier.h"    // Soldier::getTeam (+0x1F0) / say - the local soldier VO
#include <cmath>                        // sqrt - playSoldierVO's range test
#include <cstdlib>                      // rand - getSpeechId
#include <cstring>                      // memset - the speech table
#include "../../Platform/MsvcRand.h"  // [CHOSEN, not recovered] the client CRT's rand (MSVCR71)

namespace dice {
namespace hfe {

namespace {
// Casts an object known to be a Soldier, retaining native base adjustment.
// [INFERRED] Return type from its callers.
world::Soldier* iobjectToSoldier(world::IObject* object)
{
    return static_cast<world::Soldier*>(object);
}
} // anonymous namespace

// Normalizes all supplied paths with trailing slashes, clears the speech table to
// 1, and initializes sound state with a 15 m voice range.
VoiceMessages::VoiceMessages(const std::string& team1RadioPath, const std::string& team2RadioPath,
                             const std::string& voPath)
    : m_voVolume(1.0f),                                      // +0x0C
      m_radioVolume(1.0f),                                   // +0x10
      m_filterSound(0),                                      // dword 2081
      m_helpSound(0),                                        // dword 2084
      m_voRange(15.0f)                                       // dword 2097 = 0x41700000
{
    m_radioSound[0] = 0;                                     // dwords 2082, 2083
    m_radioSound[1] = 0;
    for (int team = 0; team < 2; ++team)                     // dwords 2091..2094
        for (int k = 0; k < 2; ++k)
            m_localVOSound[team][k] = 0;

    m_radioPath[0] = team1RadioPath;                         // std::string::assign(this, a2)
    m_radioPath[1] = team2RadioPath;                         // assign(this + 4, a3)
    appendSlash(m_radioPath[0]);                             // the two-iteration appendSlash loop
    appendSlash(m_radioPath[1]);
    m_voPath = voPath;                                       // assign(this + 8, a4)
    appendSlash(m_voPath);

    std::memset(m_speech, 0xFF, sizeof(m_speech));           // memset(this + 132, 255, 0x2000):
                                                             // every range starts {-1, -1}
}

// Releases each team's radio and local voice sounds, then filter and help sounds,
// before member-owned weak references and containers are destroyed.
VoiceMessages::~VoiceMessages()
{
    for (int team = 0; team < 2; ++team)
    {
        if (m_radioSound[team])
            m_radioSound[team]->release();                   // slot +0x04
        for (int k = 0; k < 2; ++k)
            if (m_localVOSound[team][k])
                m_localVOSound[team][k]->release();
    }
    if (m_filterSound)
        m_filterSound->release();
    if (m_helpSound)
        m_helpSound->release();
}

// appendSlash - ELF 0x8123580, 108 B. A non-empty path gets a trailing '/' unless it has one.
// (The _M_leak_hard in the decompilation is the COW string un-sharing before the last-char read.)
void VoiceMessages::appendSlash(std::string& path)
{
    if (!path.empty() && path[path.size() - 1] != '/')
        path.append(1, '/');
}

// setVolumes - ELF 0x8121580, 72 B. Both clamped to [0, 1] with the x87 compare-and-select the
// decompiler shows; VoiceMessages.con sets 1.0 1.0.
void VoiceMessages::setVolumes(float voVolume, float radioVolume)
{
    float vo;
    if (voVolume < 0.0f)
        vo = 0.0f;
    else
        vo = (voVolume <= 1.0f) ? voVolume : 1.0f;
    m_voVolume = vo;                                         // +0x0C

    float radio;
    if (0.0f > radioVolume)
        radio = 0.0f;
    else
        radio = (radioVolume > 1.0f) ? 1.0f : radioVolume;
    m_radioVolume = radio;                                   // +0x10
}

// addMessage - ELF 0x81215D0, 391 B. The new id is the current count; the 257th message is refused
// (`cmp count, 0FFh; ja` - unsigned). A name already present is reassigned the new id: the
// lower_bound / insert_unique pair is followed by an unconditional store into the node's value.
int VoiceMessages::addMessage(const std::string& name)
{
    const unsigned int id = static_cast<unsigned int>(m_messages.size());   // dword 7
    if (id > 0xFF)
        return -1;
    m_messages[name] = static_cast<int>(id);
    return static_cast<int>(id);
}

// Adds a speech sample to the last message's voice range and returns its id. Requires
// an existing message, room in the voice table, and contiguous samples per pair.
// Sound quality 2 keeps only the first sample. Both radio and local-VO paths use
// the radio root; empty samples retain their slots.
int VoiceMessages::addRadioVoice(const std::string& voice, const std::string& radioSample,
                                 const std::string& text, const std::string& localVOSample,
                                 int voFlags)
{
    if (m_voices.size() > 7 || m_messages.empty())
        return -1;

    std::map<std::string, int>::iterator v = m_voices.find(voice);
    if (v == m_voices.end())
    {
        const int voiceIndex = static_cast<int>(m_voices.size());     // the count before the insert
        m_voices[voice] = voiceIndex;
        v = m_voices.find(voice);
    }

    const int speechId = static_cast<int>(m_speechTexts.size());
    SpeechRange& cell = m_speech[messageVoiceIndex(static_cast<int>(m_messages.size()) - 1,
                                                   v->second)];       // lea ebx, [this + idx*4]; +84h
    if (cell.first < 0)                                               // cmp word ptr [ebx+84h], 0; js
    {
        cell.first = static_cast<short>(speechId);
        cell.count = 0;
    }
    else
    {
        if (g_bfSound->engine()->getQualityLevel() == 2)
            return -1;
        if (speechId != cell.first + cell.count)
            return -1;
    }

    for (int team = 0; team < 2; ++team)
    {
        if (!radioSample.empty())
            m_radioSamples[team].push_back(m_radioPath[team] + radioSample);
        else
            m_radioSamples[team].push_back(std::string());

        if (!localVOSample.empty())
            m_localVO[team].push_back(std::make_pair(m_radioPath[team] + localVOSample, voFlags));
        else
            m_localVO[team].push_back(std::make_pair(std::string(), 1));
    }

    m_speechTexts.push_back(text);
    ++cell.count;                                                     // movzx / inc / mov [edx+2], ax
    return speechId;
}

// addRadioFilter - ELF 0x8121EB0, 533 B. Index = the current sample count; the name is inserted if
// absent and its value stored unconditionally (a repeated name is re-pointed at the new sample);
// the sample path is stored exactly as given - VoiceMessages.con passes "common/sound/..." in full.
int VoiceMessages::addRadioFilter(const std::string& name, const std::string& sample)
{
    const int index = static_cast<int>(m_filterSamples.size());       // dwords 27/28
    m_radioFilters[name] = index;                                     // the tree at +0x30
    m_filterSamples.push_back(sample);                                // _M_insert_aux(this + 108)
    return index;
}

// addHelpMessage - ELF 0x81220D0, 846 B. The key is lowercased (dice::hfe::getLowerCase); the
// sample is rooted at m_voPath, or stored empty when empty. Same insert-or-repoint as the others.
int VoiceMessages::addHelpMessage(const std::string& name, const std::string& sample)
{
    const int index = static_cast<int>(m_helpSamples.size());         // dwords 30/31
    m_helpMessages[getLowerCase(name)] = index;                       // the tree at +0x3C
    if (!sample.empty())
        m_helpSamples.push_back(m_voPath + sample);                   // operator+(this + 8, a3)
    else
        m_helpSamples.push_back(std::string());
    return index;
}

// Builds radio, filter, local-voice and help sounds from the collected samples.
// Local voice is skipped at quality level 2. Empty names become null except in the filter list.
// Radio/local sounds borrow their volume evaluators; help uses its own volume group.
// Clears consumed sample lists and returns true.
bool VoiceMessages::finalize(ISoundEngine* engine)
{
    std::vector<const char*> names;
    names.reserve(m_radioSamples[0].size());

    // 1. the radio filter
    for (unsigned int i = 0; i < m_filterSamples.size(); ++i)
        names.push_back(m_filterSamples[i].c_str());
    {
        ISoundTemplate* t = engine->createTemplate();                         // ISoundEngine +0x10
        t->setLoopCount(1);                                                   // +0x18
        t->setIs3dSound(false);                                               // +0x68
        t->setFilenames(names.empty() ? 0 : &names[0], static_cast<int>(names.size()));   // +0x10
        t->setReverbLevel(0.0f);                                              // +0x78
        t->setGlobalEffectLevel(1.0f);                                        // +0xB0
        m_filterSound = t->createSound();                                     // +0xD0 -> dword 2081
        m_filterSound->setVolumeEvaluator(&m_volumeEvaluator, &m_radioVolume);   // ISound +0x44
        t->setIsUsed();                                                       // +0xB8
        t->release();                                                         // +0x04
    }

    // 2. the radio voices, per team
    for (int team = 0; team < 2; ++team)
    {
        names.clear();
        for (unsigned int i = 0; i < m_radioSamples[team].size(); ++i)
        {
            const std::string& s = m_radioSamples[team][i];
            names.push_back(s.empty() ? 0 : s.c_str());
        }
        ISoundTemplate* t = engine->createTemplate();
        t->setLoopCount(1);
        t->setIs3dSound(false);
        t->setFilenames(names.empty() ? 0 : &names[0], static_cast<int>(names.size()));
        t->setReverbLevel(0.0f);
        t->setIsUsed();                                                       // before the effect level
        t->setGlobalEffectLevel(1.0f);
        m_radioSound[team] = t->createSound();                                // dwords 2082/2083
        m_radioSound[team]->setVolumeEvaluator(&m_volumeEvaluator, &m_radioVolume);
        t->release();
    }

    // 3. the local (3D) voice-overs, per team - skipped at quality level 2
    if (g_bfSound->engine()->getQualityLevel() != 2)                          //  +0x28, +0x0C
    {
        for (int team = 0; team < 2; ++team)
        {
            names.clear();
            for (unsigned int i = 0; i < m_localVO[team].size(); ++i)
            {
                const std::string& s = m_localVO[team][i].first;
                names.push_back(s.empty() ? 0 : s.c_str());
            }
            ISoundTemplate* t = engine->createTemplate();
            t->setLoopCount(1);
            t->setIs3dSound(true);
            t->setFilenames(names.empty() ? 0 : &names[0], static_cast<int>(names.size()));
            t->setReverbLevel(1.0f);
            t->setGlobalEffectLevel(1.0f);
            t->setMinDistance(3.0f);                                          // +0x58
            t->setStopType(0);                                                // +0x70
            t->setIsUsed();
            for (int k = 0; k < 2; ++k)                                       // dwords 2091 + 2*team + k
            {
                m_localVOSound[team][k] = t->createSound();
                m_localVOSound[team][k]->setVolumeEvaluator(&m_volumeEvaluator, &m_voVolume);
            }
            t->release();
        }
    }

    // 4. the help voice-overs
    names.clear();
    for (unsigned int i = 0; i < m_helpSamples.size(); ++i)
    {
        const std::string& s = m_helpSamples[i];
        names.push_back(s.empty() ? 0 : s.c_str());
    }
    {
        ISoundTemplate* t = engine->createTemplate();
        t->setLoopCount(1);
        t->setIs3dSound(false);
        t->setFilenames(names.empty() ? 0 : &names[0], static_cast<int>(names.size()));
        t->setReverbLevel(0.0f);
        t->setGlobalEffectLevel(0.0f);
        t->setStopType(0);
        t->setVolumeGroup(2);                                                 // +0x48
        t->setIsUsed();
        m_helpSound = t->createSound();                                       // dword 2084
        t->release();
    }

    // the sample lists are consumed
    m_radioSamples[0].clear();
    m_radioSamples[1].clear();
    m_filterSamples.clear();
    m_helpSamples.clear();
    return true;
}

// namesToIds - ELF 0x8122B40, 283 B / PE 0x46FF10. Three independent lookups, each only when its
// out-pointer is given: -1 for a miss; the filter additionally answers -2 for an empty name
// without looking. True when nothing requested is -1 (so a -2 filter still passes).
bool VoiceMessages::namesToIds(int* messageId, int* voiceId, int* filterId,
                               const std::string& message, const std::string& voice,
                               const std::string& filter)
{
    if (messageId)
    {
        std::map<std::string, int>::iterator it = m_messages.find(message);
        *messageId = (it == m_messages.end()) ? -1 : it->second;
    }
    if (voiceId)
    {
        std::map<std::string, int>::iterator it = m_voices.find(voice);
        *voiceId = (it == m_voices.end()) ? -1 : it->second;
    }
    if (filterId)
    {
        if (!filter.empty())
        {
            std::map<std::string, int>::iterator it = m_radioFilters.find(filter);
            *filterId = (it == m_radioFilters.end()) ? -1 : it->second;
        }
        else
        {
            *filterId = -2;
        }
    }
    return (!messageId || *messageId != -1)
        && (!voiceId   || *voiceId   != -1)
        && (!filterId  || *filterId  != -1);
}

// getDefaultFilter - ELF 0x8122C60, 26 B: `2 * (filters.count != 0) - 2` - 0 once any filter is
// registered, -2 before (the same "no filter" value namesToIds answers for an empty name).
int VoiceMessages::getDefaultFilter() const
{
    return m_radioFilters.empty() ? -2 : 0;
}

// Selects and plays a speech sample, looping any requested filter to cover it.
// Preserve the truncation-plus-one loop count. The name overload returns -1
// without playing if any name is unknown.
int VoiceMessages::playRadioMessage(int team, int message, int voice, int filter)
{
    const int teamIndex = teamToIndex(team);
    if (teamIndex < 0)
        return -1;

    const int speechId = getSpeechId(message, voice);
    if (m_radioSound[teamIndex]->playSample(speechId, 1) && filter >= 0)
    {
        const float* const radioInfo =
            static_cast<const float*>(m_radioSound[teamIndex]->getSampleInfo(speechId));
        const float* const filterInfo =
            static_cast<const float*>(m_filterSound->getSampleInfo(filter));
        if (!radioInfo || !filterInfo)
            return -1;

        int loopCount = 1;
        if (filterInfo[1] > 0.0f)
        {
            loopCount = static_cast<int>((radioInfo[1] - filterInfo[0]) / filterInfo[1]) + 1;
            if (loopCount <= 0)
                loopCount = 1;
        }
        m_filterSound->playSample(filter, loopCount);
    }
    return speechId;
}

int VoiceMessages::playRadioMessage(int team, const std::string& message,
                                    const std::string& voice, const std::string& filter)
{
    int messageId;
    int voiceId;
    int filterId;
    if (!namesToIds(&messageId, &voiceId, &filterId, message, voice, filter))
        return -1;
    return playRadioMessage(team, messageId, voiceId, filterId);
}

// Returns speech text, or an empty string for an out-of-range ID.
std::string VoiceMessages::getSpeech(int speechId)
{
    if (speechId >= 0 && static_cast<unsigned int>(speechId) < m_speechTexts.size())
        return m_speechTexts[speechId];
    return std::string();
}

int VoiceMessages::getSpeechId(int message, int voice)
{
    const SpeechRange& cell = m_speech[messageVoiceIndex(message, voice)];
    if (cell.first < 0 || cell.count <= 0)
        return -1;
    if (cell.count > 1)
        return cell.first + bf2crt::rand() % cell.count;
    return cell.first;
}

int VoiceMessages::messageVoiceIndex(int message, int voice)
{
    return voice | (8 * message);
}

int VoiceMessages::teamToIndex(int team)
{
    if (static_cast<unsigned int>(team - 1) < 2)
        return team - 1;
    return -1;
}

// Plays nearby soldier speech, returning negative when the caller should fall back to radio.
// Requires a soldier on team 1 or 2. Flags come from the first team's list and control
// team/range restrictions; NaN distance does not reject playback. A voice slot may evict a farther speaker.
int VoiceMessages::playSoldierVO(int team, int message, int voice, const Vec3& listener,
                                 world::Soldier* soldier)
{
    const int speechId = getSpeechId(message, voice);
    if (speechId < 0)
        return -1;

    const int teamIndex = teamToIndex(soldier->getTeam());                // Soldier +0x1F0
    int fileCount = 0;                                                     // written, never read
    const char** const files =
        m_localVOSound[teamIndex][0]->getTemplate()->getFilenames(&fileCount);   // +0x70, +0x14
    if (!files)
        return -1;
    if (!files[speechId])
        return -1;

    const int flags = m_localVO[0][speechId].second;
    if (!(flags & 1) && soldier->getTeam() != team)                        // Soldier +0x1F0 again
        return -1;
    if (!(flags & 2))
    {
        const Vec3& position = soldier->getAbsolutePosition();            // +0x6C
        const float dx = position.x - listener.x;
        const float dy = position.y - listener.y;
        const float dz = position.z - listener.z;
        if (std::sqrt(dz * dz + (dy * dy + dx * dx)) > m_voRange)          // +0x20C4
            return -1;
    }

    soldier->say(0, 0);
    const int slot = getFreeVOSoundIndex(listener, soldier);
    if (slot < 0)
        return -1;
    m_voSpeaker[slot] = soldier->getWeakPtr();                             // +0x1C, by value
    soldier->say(m_localVOSound[teamIndex][slot], speechId);
    return speechId;
}

// Selects one of two voice slots. Reclaims silent/dead speakers first; otherwise
// evicts only a speaker farther from the listener than the requester. Ties return
// 1. Both team sounds are tested, and eviction does not stop them immediately.
int VoiceMessages::getFreeVOSoundIndex(const Vec3& listener, world::Soldier* soldier)
{
    for (int slot = 0; slot <= 1; ++slot)
    {
        bool playing = false;
        for (int team = 0; team <= 1; ++team)
            playing |= m_localVOSound[team][slot]->isPlaying();          // +0x38
        if (!playing)
            m_voSpeaker[slot] = WeakPtr<world::IObject>();
    }

    for (int slot = 0; slot <= 1; ++slot)
    {
        if (!m_voSpeaker[slot].get())
        {
            for (int team = 0; team <= 1; ++team)
                m_localVOSound[team][slot]->stop();                       // +0x30
            return slot;
        }
    }

    float farthest = -1.0f;                                               // flt_888B590
    int farthestSlot = -1;
    for (int slot = 0; slot <= 1; ++slot)
    {
        const Vec3& position =
            iobjectToSoldier(m_voSpeaker[slot].get())->getAbsolutePosition();   // +0x6C
        const float dx = position.x - listener.x;
        const float dy = position.y - listener.y;
        const float dz = position.z - listener.z;
        const float distanceSq = dz * dz + (dy * dy + dx * dx);
        if (distanceSq > farthest)
        {
            farthest = distanceSq;
            farthestSlot = slot;
        }
    }

    const Vec3& position = soldier->getAbsolutePosition();                // +0x6C
    const float dx = listener.x - position.x;
    const float dy = listener.y - position.y;
    const float dz = listener.z - position.z;
    const float distanceSq = dz * dz + (dy * dy + dx * dx);
    if (!(farthest > distanceSq))
        return -1;

    iobjectToSoldier(m_voSpeaker[farthestSlot].get())->say(0, 0);
    m_voSpeaker[farthestSlot] = WeakPtr<world::IObject>();
    return farthestSlot;
}

} // namespace hfe
} // namespace dice
