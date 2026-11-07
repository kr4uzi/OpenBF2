// [INFERRED] split path and method names. Client-only native suffix through PE+C8.
// Full instructions and signature evidence read.
// The three bookmark source spellings are inferred, with their x86 machine contracts
// proved by complete bodies and callers.
#include "LocalProfileManager.h"
#include "ProfileManager.h"           // loginOnline / loginOnlineAlternate: the account login and its profile record
#include <vector>
#include "../Menu/ISwiffHost.h"        // validateProfileName goes to the typed host
#if !DEDICATED
#include "AudioSettings.h"
#include "GameServerSettings.h"
#include "GeneralSettings.h"
#include "GlobalSettings.h"
#include "HapticSettings.h"
#include "VideoSettings.h"
#include "Main/BF2ClientSeams.h"
#include "../Core/ClassManager.h"
#include "../IO/File/FileManager.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>          // readVoiceSettings' strlen
#include <sstream>          // the voice-settings text, written and parsed

namespace dice { namespace hfe {
extern ClassManager* classManager;

// Applies settings, discarding individual results. ControlSettings is applied separately.
bool LocalProfileManager::applySettings(bool audioOnly)
{
    if (!m_currentProfile)
        return false; // PE4A77E3..4A77EC
    IAudioSettings* audio = static_cast<IAudioSettings*>(g_audioSettings);
    if (audioOnly) {
        audio->apply(false); // PE4A77FF discards the result.
        return true;
    }
    static_cast<IGlobalSettings*>(g_globalSettings)->apply();
    static_cast<IGeneralSettings*>(g_generalSettings)->apply();
    static_cast<IGameServerSettings*>(g_gameServerSettings)->apply();
    audio->apply(false);
    static_cast<IVideoSettings*>(g_videoSettings)->apply();           // +1C 4A783E
    static_cast<IHapticSettings*>(g_hapticSettings)->apply();
    return true;
}

bool LocalProfileManager::hasDefaultVoiceSettings()
{
    std::string path = getProfilesPath();
    path += "BattleCommoDefaultUserSettings.con";
    return io::fileManager->fileExists(path); // PE4A7AD0; no singleton guard
}

unsigned int LocalProfileManager::getNumDemoBookmarks()
{
    return static_cast<unsigned int>(m_demoBookmarks.size()); // PE4A84A0
}

LocalDemoBookmark* LocalProfileManager::getDemoBookmark(unsigned int index)
{
    return m_demoBookmarks.data() + index; // PE4A9680 is deliberately unchecked
}

bool LocalProfileManager::removeDemoBookmark(unsigned int index)
{
    // PE4ABA08 calls the virtual count, then uses an unsigned comparison.
    if (index >= getNumDemoBookmarks())
        return false;
    m_demoBookmarks.erase(m_demoBookmarks.begin() + index);
    return true;
}

// : no host, no validation; otherwise the host's rule set (Menu/SwiffHost.cpp +0x26C).
bool LocalProfileManager::validateProfileName(const std::string& name, std::string& error)
{
    IBase* host = classManager->getSingleton(std::string("SwiffHost"));
    if (!host)
        return false; // PE4A7D7F..4A7D89
    return static_cast<ui::ISwiffHost*>(host)->validateProfileName(name, error);   // +0x26C, PE4A7D98
}

namespace {
// Retrieves an online account by email or nickname and selects or creates its local
// profile. Refuses while a profile is current. Saves account metadata and retains
// the password only when requested; failed profile lookup/creation logs out.
bool loginOnlineAccount(LocalProfileManager& self, LocalProfile*& current, bool& dirty, bool byNick,
                        const std::string& name, const std::string& password, bool remember)
{
    if (current)
        return false;
    IProfileManager* online = static_cast<IProfileManager*>(
        classManager->getSingleton(std::string("ProfileManager")));
    const bool loggedIn = byNick ? online->loginAlternate(name, password)
                                 : online->loginAccount(name, password, std::string(""));
    if (!loggedIn)
        return false;
    ProfileInformation info;
    if (!online->getProfileInformation(online->getCurrentProfile()->profileId, info))
    {
        online->logout();
        return false;
    }
    std::vector<LocalProfile> profiles;
    (void) self.getLocalProfiles(profiles, true);
    int found = -1;
    for (std::size_t index = 0; index < profiles.size(); ++index)
    {
        if (profiles[index].gamespyNick == info.uniqueNick)
        {
            found = (int) index;
            break;
        }
    }
    if (found == -1)
    {
        if (!self.createProfile(info.uniqueNick, info.uniqueNick))
        {
            online->logout();
            return false;
        }
    }
    else
    {
        online->logout();
        if (!self.login(found, name, password))
            return false;
    }
    current->gamespyNick = info.uniqueNick;
    current->email       = info.email;
    if (remember)
        current->password = password;
    dirty = true;
    self.saveCurrentProfile();
    return true;
}
}

bool LocalProfileManager::loginOnline(const std::string& email, const std::string& password, bool remember)
{
    return loginOnlineAccount(*this, m_currentProfile, m_profilesDirty, false, email, password, remember);
}

bool LocalProfileManager::loginOnlineAlternate(const std::string& nick, const std::string& password, bool remember)
{
    return loginOnlineAccount(*this, m_currentProfile, m_profilesDirty, true, nick, password, remember);
}

// Voice defaults live outside profiles in BattleCommoDefaultUserSettings.con.
// The file contains four plain values, not console commands, and transfers the
// four pending AudioSettings VOIP fields.
namespace {
// VC7.1's num_get leaves its target untouched when an extraction fails; C++11 changed that to store 0.
// readVoiceSettings extracts straight into its caller's references, so a short or malformed file keeps
// loadVoiceSettings' seeded defaults in retail - reproduced by extracting into a copy and committing
// only on success (the same reasoning as Core/ConsoleObjectMacro.h's BF2_CC_CONVERT). [CHOSEN] spelling.
template <typename T>
void extractKeepingOnFailure(std::istream& in, T& target)
{
    T value = target;
    in >> value;
    if (!in.fail())
        target = value;
}

AudioSettings* voiceAudioSettings()                                     // ds:0x987384, re-read per use
{
    return static_cast<AudioSettings*>(static_cast<IAudioSettings*>(g_audioSettings));
}
}

// Reads voice settings for the current profile and applies them through AudioSettings when successful.
// Requires AudioSettings; rereads its singleton before each call.
bool LocalProfileManager::loadVoiceSettings()
{
    if (!m_currentProfile)
        return false;
    float playback = 1.0f;
    float capture = 1.0f;
    float threshold = 0.1f;
    bool boost = false;
    if (!readVoiceSettings(playback, capture, threshold, boost))                 // +0xA0
        return false;
    voiceAudioSettings()->setVoipPlaybackVolume(playback);                      // +0x44
    voiceAudioSettings()->setVoipCaptureVolume(capture);                        // +0x4C
    voiceAudioSettings()->setVoipCaptureThreshold(threshold);                   // +0x54
    voiceAudioSettings()->setVoipBoostEnabled(boost);                           // +0x5C
    static_cast<IAudioSettings*>(g_audioSettings)->apply(true);                 // +0x18, discarded
    return true;
}

// Saves four voice settings, one per line, to BattleCommoDefaultUserSettings.con at the profiles root.
bool LocalProfileManager::saveVoiceSettings()
{
    std::stringstream script(std::ios::in | std::ios::out);
    script << voiceAudioSettings()->getVoipPlaybackVolume() << std::endl;       // +0x40
    script << voiceAudioSettings()->getVoipCaptureVolume() << std::endl;        // +0x48
    script << voiceAudioSettings()->getVoipCaptureThreshold() << std::endl;     // +0x50
    script << voiceAudioSettings()->getVoipBoostEnabled() << std::endl;         // +0x58
    const std::string fileName("BattleCommoDefaultUserSettings.con");
    return writeProfileFile(fileName, script.str());
}

// Reads four voice defaults from a 512-byte buffer. Returns false for a missing,
// unreadable, or empty file; otherwise returns true even if extraction fails.
// A full buffer lacks a terminator and retains the original strlen overrun risk.
bool LocalProfileManager::readVoiceSettings(float& playback, float& capture, float& threshold, bool& boost)
{
    if (!hasDefaultVoiceSettings())                                              // +0x94
        return false;
    std::string path(io::fileManager->getBaseSystem()->getHomeSubstitution());  // +0x54, +0x48
    path += "\\Profiles\\BattleCommoDefaultUserSettings.con";
    char buffer[512] = { 0 };
    unsigned int bytesRead = 0;
    if (!io::fileManager->getBaseSystem()->readFile(path, buffer, 0x200, bytesRead))   // +0x54, +0x2C
        return false;
    if (std::strlen(buffer) == 0)
        return false;
    const std::string text(buffer);
    std::istringstream in(text, std::ios::in);
    extractKeepingOnFailure(in, playback);
    extractKeepingOnFailure(in, capture);
    extractKeepingOnFailure(in, threshold);
    extractKeepingOnFailure(in, boost);
    return true;
}

// Logs in the first matching cached profile; refuses if one is already active.
// With a password, matches the GameSpy nickname; without one, matches a password-free local profile name.
bool LocalProfileManager::quickLogin(const std::string& name, const std::string& password)
{
    if (m_currentProfile) {
        reportProfileError(std::string("Unable to quickLogin while allready logged in"));
        return false;
    }
    if (password != "") {
        classManager->getSingleton(std::string("ProfileManager")); // PE4AAD75: the result is not used
        std::vector<LocalProfile> profiles;
        getLocalProfiles(profiles, true);                          // +48
        for (unsigned int i = 0; i < profiles.size(); ++i) {
            if (profiles[i].gamespyNick == name)
                return login(static_cast<int>(i), profiles[i].email, password); // +1C 4AAE0A
        }
        return false;
    }
    std::vector<LocalProfile> profiles;
    getLocalProfiles(profiles, true);
    for (unsigned int i = 0; i < profiles.size(); ++i) {
        if (profiles[i].profileName == name && profiles[i].password == "") {
            std::string emptyName("");
            std::string emptyPassword("");
            return login(static_cast<int>(i), emptyName, emptyPassword);      // +1C 4AAF15
        }
    }
    return false;
}

// PE4A9440 (80 B): the GlobalSettings file name, then its script, through the profile writer
// (whose constant true is returned).
// PE 619360: one load of +34h (the body is shared by identical-code folding).
int LocalProfileManager::getLoginProfileIndex()
{
    return m_field34;
}

// PE 4A7DB0: a copy of +38h into the caller's return string.
std::string LocalProfileManager::getLoginPassword()
{
    return m_field38;
}

bool LocalProfileManager::saveGlobalSettings()
{
    IGlobalSettings* global = static_cast<IGlobalSettings*>(g_globalSettings);
    std::string file = global->getFileName();                   // +10
    return writeProfileFile(file, global->getSettingsScript()); // +14, then 4A82F0
}

} }
#endif
