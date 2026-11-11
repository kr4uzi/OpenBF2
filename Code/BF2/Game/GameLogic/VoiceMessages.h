// Per-level radio and voice registry owned by GameLogic. Console scripts populate
// message names, samples and per-team voices before finalize creates the sounds.
// Speaker handles are weak. The help-playing query remains a named frontier.
// STL-containing layouts differ across platforms; use typed fields.
#ifndef GAME_GAMELOGIC_VOICEMESSAGES_H     // [INFERRED] guard style
#define GAME_GAMELOGIC_VOICEMESSAGES_H

#include "../../Core/WeakPtr.h"
#include "../../Sound/ISoundEvaluator.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace dice {
namespace hfe {

namespace Math { struct Vec3; }
using Math::Vec3;
namespace world { class Soldier; class IObject; }
class ISoundEngine;
class ISound;

class VoiceMessages
{
    // [INFERRED] source-level access mechanism; the shipped GameLogic body reads m_helpSound
    // directly and no out-of-line VoiceMessages accessor exists. Friendship changes no layout.
    friend class GameLogic;

public:
    // Three slash-terminated sample roots: the two teams' radio languages and the local VO language.
    VoiceMessages(const std::string& team1RadioPath, const std::string& team2RadioPath,
                  const std::string& voPath);
    ~VoiceMessages();

    // the console-driven builders (GameLogicConsole.cpp, `gamelogic.messages.*`)
    void setVolumes(float voVolume, float radioVolume);
    int  addMessage(const std::string& name);
    int  addRadioVoice(const std::string& voice, const std::string& radioSample,
                       const std::string& text, const std::string& localVOSample,
                       int voFlags);
    int  addRadioFilter(const std::string& name, const std::string& sample);
    int  addHelpMessage(const std::string& name, const std::string& sample);

    // Builds every sound from the sample lists and then empties the lists (bar the local VO pairs).
    bool finalize(ISoundEngine* engine);

    // the lookups
    // Each id is written only when its pointer is non-null: -1 for a name that is not registered,
    // and for the filter -2 when the name is empty. Returns true when nothing asked for came back -1.
    bool namesToIds(int* messageId, int* voiceId, int* filterId,
                    const std::string& message, const std::string& voice,
                    const std::string& filter);
    int  getDefaultFilter() const;
    std::string getSpeech(int speechId);
    int  getSpeechId(int message, int voice);
    int  messageVoiceIndex(int message, int voice);
    int  teamToIndex(int team);

    // the playback side
    // The two radio overloads are written. Game::playbackRadioMessage materialises and tests all
    // four return values, fixing these as int independently of the Itanium manglings.
    int  playRadioMessage(int, int, int, int);
    int  playRadioMessage(int, const std::string&, const std::string&,
                          const std::string&);
    int  playSoldierVO(int, int, int, const Vec3&, world::Soldier*);
    int  playHelpMessage(const std::string&); // BF2.exe 0x4F8110; client body beside PlayerVOHelp.
    bool isHelpVOPlaying() const;                                               //  [FRONTIER]
    int  getFreeVOSoundIndex(const Vec3&, world::Soldier*);

    // One (message, voice) cell of the speech table: the first speech id and how many follow it.
    // `cmp word ptr [ebx+84h], 0` / `movsx edx, word ptr [ecx+2]` in addRadioVoice (0x8121874 /
    // ) fix both halves as signed 16-bit.
    struct SpeechRange
    {
        short first;
        short count;
    };

private:
    void appendSlash(std::string& path);

    std::string m_radioPath[2];                             // +0x0000
    std::string m_voPath;                                   // +0x0008
    float       m_voVolume;                                 // +0x000C
    float       m_radioVolume;                              // +0x0010
    FixedVolumeEvaluator m_volumeEvaluator;                 // +0x0014
    std::map<std::string, int> m_messages;                  // +0x0018
    std::map<std::string, int> m_voices;                    // +0x0024
    std::map<std::string, int> m_radioFilters;              // +0x0030
    std::map<std::string, int> m_helpMessages;              // +0x003C
    std::vector<std::string> m_speechTexts;                 // +0x0048
    std::vector<std::string> m_radioSamples[2];             // +0x0054, +0x0060
    std::vector<std::string> m_filterSamples;               // +0x006C
    std::vector<std::string> m_helpSamples;                 // +0x0078
    SpeechRange m_speech[256 * 8];                          // +0x0084 .. +0x2083
    ISound*     m_filterSound;                              // +0x2084
    ISound*     m_radioSound[2];                            // +0x2088, +0x208C
    ISound*     m_helpSound;                                // +0x2090
    std::vector<std::pair<std::string, int> > m_localVO[2]; // +0x2094, +0x20A0
    ISound*     m_localVOSound[2][2];                       // +0x20AC .. +0x20B8
    WeakPtr<world::IObject> m_voSpeaker[2];                 // +0x20BC, +0x20C0
    float       m_voRange;                                  // +0x20C4
};

} // namespace hfe
} // namespace dice

#endif // GAME_GAMELOGIC_VOICEMESSAGES_H
