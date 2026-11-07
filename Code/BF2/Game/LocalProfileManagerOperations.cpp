// [INFERRED] path/names. Complete client instructions 4AA370, 4AA980, 4AAF70,
//  and helper4B11A0 read, including the complete login.
// Only the reachable prefix before an untyped
// dependency is implemented in the other operations.
#include "LocalProfileManager.h"
#if !DEDICATED
#include "ProfileManager.h"           // IProfileManager::login - the GameSpy presence login
#include "AudioSettings.h"            // the six settings owners createProfile puts back to defaults
#include "ControlSettings.h"
#include "GameServerSettings.h"
#include "GeneralSettings.h"
#include "GlobalSettings.h"
#include "HapticSettings.h"
#include "VideoSettings.h"
#include "MapList.h"                  // the new profile's map list: re-pointed, emptied, rewritten
#include "GameServer/ServerSettings.h" // m_configFile, ds:0x9A4D98
#include "Main/BF2ClientSeams.h"      // the settings singletons
#include "../Core/SettingsRepostitory.h" // settings->stringSet("GSPlayerName", ...)
#include "../Core/StringUtil.h"       // compareIgnoreCase, PE431950
#include <vector>
#include "../Menu/ISwiffHost.h"       // postMessage - reportProfileError's {3, message, 3} record
#include "DemoRecorder/DemoLibrary.h"  // IDemoLibrary::refresh - login's demo-directory rescan
#include "../Core/ClassManager.h"
#include "../Core/EventManager.h"
#include "../IO/File/FileManager.h"   // deleteProfile: io::fileManager +0x48 / +0x20
#include <cstdio>
#include <cstdlib>
#include <new>

namespace dice { namespace hfe {
extern ClassManager* classManager;

// Loads and applies a profile; the middle argument is unused.
// Online profiles must pass presence login. Requires current-profile, ProfileManager, and DemoLibrary objects.
bool LocalProfileManager::login(int profileIndex, const std::string&, const std::string& password)
{
    if (m_currentProfile) return false;
    std::vector<LocalProfile> profiles;
    getLocalProfiles(profiles, true);
    // PE4AA3C6 uses an unsigned comparison, so negative indices are rejected.
    if (static_cast<unsigned int>(profileIndex) >= profiles.size()) return false;
    void* allocation = ::operator new(sizeof(LocalProfile));
    m_currentProfile = allocation ? new (allocation) LocalProfile : nullptr;
    // The shipped allocation-null defect is retained before the string copy.
    m_currentProfile->name = profiles[profileIndex].name;
    loadCurrentProfile();                                                           // +3C; result discarded
    if (m_currentProfile->gamespyNick != "") {
        IBase* profileManager = classManager->getSingleton(std::string("ProfileManager"));
        // Requires the presence manager singleton.
        if (!static_cast<IProfileManager*>(profileManager)->login(m_currentProfile->gamespyNick, password)) {
            shutdown();
            return false;
        }
    }
    m_field34 = profileIndex;
    m_field38 = password;
    eventManager->registerEventHandler(ECGame, static_cast<IEventListener*>(this), 0);
    IBase* demoLibrary = classManager->getSingleton(std::string("DemoLibrary"));
    static_cast<IDemoLibrary*>(demoLibrary)->refresh();                             // +10 4AA4CD; not null-tested, result ignored
    m_profilesDirty = true;
    return applySettings(false);
}

// Creates and selects an offline profile in the first unused numbered directory.
// Refuses while a profile is selected or the name is invalid. Resets settings and control maps,
// updates configuration paths, applies/saves settings and leaves the new profile logged in.
// The apply/save results are discarded.
bool LocalProfileManager::createProfile(const std::string& profileName, const std::string& nick)
{
    if (m_currentProfile) return false;
    std::string error;
    if (!validateProfileName(profileName, error)) {                                  // +BC 4AA9AA
        reportProfileError(error);
        return false;
    }
    std::vector<LocalProfile> profiles;
    getLocalProfiles(profiles, false);                                               // +48 4AA9E3; result discarded

    // ..4AAA6E. The shipped loop rebuilds the candidate string on every pass and restarts
    // the scan at the first record after a collision; keeping one string across the passes is the
    // only difference and nothing observes it.
    std::string directory;
    for (int number = 1; ; ++number) {
        char buffer[260];
        ::sprintf(buffer, "%.4d", number);
        directory = buffer;
        unsigned int index = 0;
        // PE431950 uses _stricmp on both c_str values, including NUL truncation.
        while (index < profiles.size() && compareIgnoreCase(directory, profiles[index].name) != 0)
            ++index;
        if (index == profiles.size())
            break;
    }

    static_cast<IGeneralSettings*>(g_generalSettings)->setDefaults();                // +1C 4AAA78
    static_cast<IGameServerSettings*>(g_gameServerSettings)->reset();                // +1C 4AAA83
    static_cast<IGameServerSettings*>(g_gameServerSettings)->setVoipQuality(5);      // +DC 4AAA90
    static_cast<IAudioSettings*>(g_audioSettings)->setDefaults();
    static_cast<IVideoSettings*>(g_videoSettings)->setDefaults();
    static_cast<IHapticSettings*>(g_hapticSettings)->setDefaults();                  // +1C 4AAAB4
    IControlSettings* control = static_cast<IControlSettings*>(g_controlSettings);
    control->resetControlMaps();
    control->setDefaults();

    void* allocation = ::operator new(sizeof(LocalProfile));                         // 0xB0 retail
    m_currentProfile = allocation ? new (allocation) LocalProfile : nullptr;
    // As in createDefaultProfile, the shipped body writes through the pointer with no
    // allocation-failure guard.
    m_currentProfile->name = directory;
    m_currentProfile->profileName = profileName;                                     // +1C 4AAB43
    m_currentProfile->nick = nick;
    m_currentProfile->gamespyNick = "";
    m_currentProfile->password = "";                                                 // +8C 4AAB78
    m_currentProfile->email = "";
    // +24 by value, the same spelling loadCurrentProfile uses for the same slot.
    static_cast<IGlobalSettings*>(g_globalSettings)->setDefaultUser(std::string(m_currentProfile->name));
    setMapListPath();
    mapList->clear(false);
    mapList->save();
    serverSettings->m_configFile = getProfilesPath() + "Default/ServerSettings.con";  // +4C, then 9A4D98
    if (hasDefaultVoiceSettings())
        loadVoiceSettings();                                                         // +98 4AAC42; result discarded
    if (m_currentProfile->gamespyNick != "")
        settings->stringSet("GSPlayerName", m_currentProfile->gamespyNick);
    else
        settings->stringSet("GSPlayerName", m_currentProfile->nick);
    applySettings(false);                                                            // +58 4AACA1; result discarded
    saveCurrentProfile();                                                            // +40 4AACA8; result discarded
    m_profilesDirty = true;
    return true;
}

// Posts a profile error when the movie host exists; without it, does nothing.
void LocalProfileManager::reportProfileError(const std::string& message)
{
    IBase* host = classManager->getSingleton(std::string("SwiffHost"));
    if (host) static_cast<ui::ISwiffHost*>(host)->postMessage(SwiffHostMessage(3, message, 3));   // +0x1E4
}

bool LocalProfileManager::deleteProfile(int profileIndex)
{
    if (m_currentProfile) {
        reportProfileError(std::string("Unable to delete local profile while you are connected"));
        return false;
    }
    std::vector<LocalProfile> profiles;
    getLocalProfiles(profiles, true);
    // Unlike login, PE4AAFEC uses a signed comparison and has no negative guard.
    if (profileIndex >= static_cast<int>(profiles.size())) return false;
    // PE4AB003..4AB10C. A deleted default user is forgotten first (Global.con's +0x20/+0x24,
    // the empty string by value).
    IGlobalSettings* global = static_cast<IGlobalSettings*>(g_globalSettings);
    if (profiles[profileIndex].name == global->getDefaultUser())
        global->setDefaultUser(std::string(""));
    saveGlobalSettings();                                                            // +C4 4AB05F; result discarded
    // The profile directory under the file manager's own home substitution (+0x48 on
    // io::fileManager itself, not its base system), with the shipped backslashes.
    std::string path = std::string(io::fileManager->getHomeSubstitution()) + "\\Profiles\\";
    std::string directory = path + profiles[profileIndex].name;
    // PE4AB0D1: erase(size(), 1) - a no-op the shipped body performs.
    directory.erase(directory.size(), 1);
    io::fileManager->deleteFile(directory);                                          // +0x20; result discarded
    m_profilesDirty = true;                                                          // +0x21
    return true;
}

// Creates an online account only for a current profile without a GameSpy nickname.
// On success, stores nickname, password, and email, then saves the profile.
bool LocalProfileManager::createOnlineAccount(const std::string& nick, const std::string& email,
                                              const std::string& password, const std::string& country)
{
    if (!m_currentProfile || m_currentProfile->gamespyNick != "") return false;
    IBase* profileManager = classManager->getSingleton(std::string("ProfileManager"));
    if (!static_cast<IProfileManager*>(profileManager)->createOnlineAccount(nick, email, password, country))
        return false;
    m_currentProfile->gamespyNick = nick;
    m_currentProfile->password    = password;
    m_currentProfile->email       = email;
    saveCurrentProfile();                                                  // virtual +0x40
    return true;
}

} }
#endif
