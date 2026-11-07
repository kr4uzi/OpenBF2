// Client-only music playback integration.

#include "MusicPlayer.h"

#if !DEDICATED

#include "../../Core/EventManager.h"
#include "../../Core/ClassManager.h"
#include "../../Core/SmartItf.h"
#include "../../Core/Debug.h"
#include "../../Game/Main/Game.h"
#include "../../Menu/ISwiffHost.h"
#include "../BfSound.h"
#include <cstdio>
#include <cstdlib>

namespace dice {
namespace hfe {

// vptr, m_state = 0, then
// eventManager->registerEventHandler((EventCategory)10, this, 100);   // vslot +0x2C
MusicPlayer::MusicPlayer()
    : m_state(0)
{
    eventManager->registerEventHandler(static_cast<EventCategory>(10), this, 100);
}

// BF2.exe 0x7E1900, called directly by BF2Engine::shutdown before operator delete.
MusicPlayer::~MusicPlayer()
{
    eventManager->unregisterEventHandler(static_cast<EventCategory>(10), this);   // vslot +0x30
}

// Loads mod music through the facade, then starts the engine. Requires sound globals and ModInfo.
void MusicPlayer::startMenuMusic()
{
    ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));
    if (!host)
        return;
    const IModInfo::Mod* mod = static_cast<IModInfo*>(host->getModInfo())->getCurrentMod();
    if (mod && g_bfSound->loadMusic(mod->music))
    {
        g_bfSound->engine()->playMusic();
        m_state = 1; // The playMusic result is deliberately ignored.
    }
}

// Strips seven leading characters and the final character without validating a prefix or slash.
void MusicPlayer::startMapMusic(const char* levelPath)
{
    ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));
    if (!host)
        return;
    std::string mapName(levelPath);
    mapName.erase(0, 7);
    if (!mapName.empty())
        mapName.erase(mapName.size() - 1);
    const IModInfo::Map* map = static_cast<IModInfo*>(host->getModInfo())->getMap(mapName);
    if (map)
    {
        ISoundEngine* engine = g_bfSound->engine(); // PE queries the engine before c_str().
        if (engine->loadMusic(map->music.c_str()))
        {
            g_bfSound->engine()->playMusic(); // A second engine() call, as shipped.
            m_state = 2;
        }
    }
}

// Reports an invalid music event through Debug.
void MusicPlayer::invalidEvent(int mode)
{
    Debug(DebugTypeError,
          "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\BfSound\\MusicPlayer.cpp",
          155, "") << "Event " << mode << " is not a valid event in state " << m_state;
}

// Changes music state without a dedicated-mode guard. Requires a valid level-path string.
void MusicPlayer::setState(int mode, const char* levelPath)
{
    // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: music-event-trace] Diagnostic
    // only, and off unless BF2_TRACE_MUSIC is set. The retail state table refuses
    // event 0 in state 1 with a Debug that aborts, so when that fires the useful
    // question is which of the posters got there first. Same env-gated shape as
    // BF2_TRACE_CONSOLE in Core/ConsoleObjectBase.cpp.
    {
        static int traceArmed = -1;
        if (traceArmed < 0) traceArmed = std::getenv("BF2_TRACE_MUSIC") ? 1 : 0;
        if (traceArmed) {
            std::fprintf(stderr, "[MUSIC] setState(mode=%d) in state %d\n", mode, m_state);
            std::fflush(stderr);
        }
    }
    if (!classManager->getSingleton("SwiffHost"))
        return;
    switch (m_state)
    {
    case 0:
        switch (mode)
        {
        case 0: startMenuMusic(); return;
        case 1: case 3: return;
        case 2: startMapMusic(levelPath); return;
        default: invalidEvent(mode); return;
        }
    case 1:
        switch (mode)
        {
        case 1: case 3:
            g_bfSound->engine()->stopMusic(5.0f);
            m_state = 0;
            return;
        case 2:
            g_bfSound->engine()->stopMusic(0.0f);
            startMapMusic(levelPath);
            return;
        default: invalidEvent(mode); return;
        }
    case 2:
        switch (mode)
        {
        case 0:
            g_bfSound->engine()->stopMusic(0.0f);
            m_state = 1; // Stored even when the following mod/music lookup fails.
            startMenuMusic();
            return;
        case 1: case 3:
            g_bfSound->engine()->stopMusic(5.0f);
            m_state = 0;
            return;
        case 2:
            g_bfSound->engine()->stopMusic(0.0f);
            startMapMusic(levelPath);
            return;
        default: invalidEvent(mode); return;
        }
    default: invalidEvent(mode); return;
    }
}

// Event 3 requires string data and evaluates c_str before host lookup. Event 1 is ignored.
void MusicPlayer::handleEvent(EventCategory category, unsigned int id, EventNode*, void* data)
{
    if (isDedicated() || category != static_cast<EventCategory>(10))
        return;
    if (id == 3)
    {
        const char* levelPath = static_cast<const std::string*>(data)->c_str();
        if (!classManager->getSingleton("SwiffHost"))
            return;
        switch (m_state)
        {
        case 0: break;
        case 1: case 2: g_bfSound->engine()->stopMusic(0.0f); break;
        default: invalidEvent(2); return;
        }
        startMapMusic(levelPath); // Repeats the SwiffHost lookup inside the helper.
    }
    else if (id == 5)
    {
        if (!classManager->getSingleton("SwiffHost"))
            return;
        switch (m_state)
        {
        case 0: return;
        case 1: case 2:
            g_bfSound->engine()->stopMusic(5.0f);
            m_state = 0;
            return;
        default: invalidEvent(3); return;
        }
    }
}

} // namespace hfe
} // namespace dice

#endif // !DEDICATED
