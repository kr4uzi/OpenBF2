// [TEMP] Headless menu collaborators. Only the setup/console/device boundaries are fixtures;
// class rows, ClassManager, both menu classes and all their virtual dispatch are production code.
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include "../BF2/Menu/EndOfRoundLogic.h"
#include "../BF2/Menu/LoadGameLogic.h"
#include "../BF2/Menu/ISwiffPlayer.h"
#include "../BF2/Menu/ISwiffHost.h"
#include "../BF2/Menu/ISwiffRender.h"
#include "../BF2/Menu/IMenuTeamManager.h"
#include "../BF2/Game/ControlSettings.h"
#include "../BF2/Core/Modules.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Game/Main/BF2Engine.h"
#include "../BF2/Game/Main/BF2EngineSetup.h"
#include "../BF2/Game/Main/BF2Console.h"
#include "../BF2/Game/Main/Game.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/IO/Input/GameInput.h"

using namespace dice::hfe;
static unsigned checks;
static void check(bool value, const char* label)
{
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
}
[[noreturn]] static void unexpected(const char* name)
{
    std::fprintf(stderr, "[TEMP] unexpected dependency: %s\n", name);
    std::abort();
}
#include "menu_logic_fakes.h"

void* LoadLibrary(const char*) { unexpected("LoadLibrary"); }
void* GetProcAddress(void*, const char*) { unexpected("GetProcAddress"); }
bool FreeLibrary(void*) { unexpected("FreeLibrary"); }

static std::vector<std::string> calls;
static bool runtimeDedicated;
static int swiffStarts, swiffStops, consoleToggles;
static int translatedKey, translationResult;
static unsigned short translatedChar;

namespace dice { namespace hfe {
ClassManager* classManager;
EventManager* eventManager;
IGame* game;
BF2Engine* bf2Engine;
IBase* g_controlSettings;
namespace io {
IBase* inputDeviceManager; IBase* controlMapManager; OldConsole* mainConsole;
OldConsole::OldConsole() {}
OldConsole::~OldConsole() {}
OldConsole::Level::Level() {}
}
char consoleCharBuffer[256];
int consoleCharCount, consoleCharRead, consoleCharWrite;
bool isDedicated() { return runtimeDedicated; }
// Engine/setup carriers and side-effect recorders: no application, plugin, console or window starts.
BF2Engine::BF2Engine(BF2*) : bf2EngineSetup(0), window(0) {}
BF2Engine::~BF2Engine() {}
BF2EngineSetup::BF2EngineSetup(BF2Engine*) {}
BF2EngineSetup::~BF2EngineSetup() {}
bool BF2EngineSetup::initSwiff() { ++swiffStarts; calls.push_back("swiff-start"); return false; }
void BF2EngineSetup::shutdownSwiff() { ++swiffStops; calls.push_back("swiff-stop"); }
BF2Console::BF2Console() : m_consoleActive(false) {}
BF2Console::~BF2Console() {}
void BF2Console::setConsoleActive(bool active) { m_consoleActive = active; ++consoleToggles; }
EventManager::EventManager() {}
EventManager::~EventManager() {}
int menuScanCodeToChar(unsigned int key, unsigned short* out)
{
    if (static_cast<int>(key) != translatedKey) return 0;
    out[0] = translatedChar;
    return translationResult;
}
struct BF2EngineFixture
{
    static void setup(BF2Engine& engine, BF2EngineSetup& setup) { engine.bf2EngineSetup = &setup; }
};
struct MenuLogicFixture
{
    template<class T> static bool empty(const T& m)
    { return m.m_refCount == 1 && !m.m_swiffActive && !m.m_frameActive && !m.m_player && !m.m_host
          && !m.m_render && !m.m_console && m.m_activeControlMapName.empty(); }
    template<class T> static bool active(const T& m) { return m.m_swiffActive; }
    template<class T> static bool frame(const T& m) { return m.m_frameActive; }
    template<class T> static std::string map(const T& m) { return m.m_activeControlMapName; }
    static void layout()
    {
        // libc++ LP64 is a separate native contract, never a validation of the retail allocation.
        static_assert(sizeof(void*) == 8 && sizeof(std::string) == 24, "macOS LP64 fixture");
#define OFFSET(T, F, O) static_assert(offsetof(T, F) == O, #T "::" #F)
#define PREFIX(T) OFFSET(T, m_refCount, 8); OFFSET(T, m_swiffActive, 12); OFFSET(T, m_frameActive, 13); \
    OFFSET(T, m_player, 16); OFFSET(T, m_host, 24); OFFSET(T, m_render, 32); \
    OFFSET(T, m_console, 40); OFFSET(T, m_activeControlMapName, 48)
        PREFIX(EndOfRoundLogic); PREFIX(LoadGameLogic);
        OFFSET(EndOfRoundLogic, m_mouseDown, 72);
        OFFSET(EndOfRoundLogic, m_mouseX, 76);
        OFFSET(EndOfRoundLogic, m_mouseY, 80);
        static_assert(sizeof(EndOfRoundLogic) == 88, "native EndOfRoundLogic extent");
        static_assert(sizeof(LoadGameLogic) == 72, "native LoadGameLogic extent");
#undef PREFIX
#undef OFFSET
    }
};
} }

struct Player : ui::UnusedISwiffPlayer
{
    ui::ISwiffHost* host = nullptr;
    ui::ISwiffRender* render = nullptr;
    std::string movie;
    float sx = 0, sy = 0, dt = 0;
    int w = 400, h = 300, displays = 0, legacy = 0;
    struct Mouse { int x,y,buttons; };
    std::vector<Mouse> mouse;
    std::vector<std::pair<int,int>> keys;
    std::vector<int> chars;
    void setSwiffHost(ui::ISwiffHost* v) override { host = v; calls.push_back("host"); }
    void setRender(ui::ISwiffRender* v) override { render = v; calls.push_back("render"); }
    bool loadMovie(const char* v) override { movie = v; calls.push_back(movie); return false; }
    int getMovieWidth() override { calls.push_back("width"); return w; }
    int getMovieHeight() override { calls.push_back("height"); return h; }
    void setScale(float x, float y) override { sx=x; sy=y; calls.push_back("scale"); }
    void update(float v) override { dt=v; calls.push_back("movie-update"); }
    void display() override { ++displays; }
    void queueMouseState(int x,int y,int b) override { mouse.push_back({x,y,b}); }
    void notifyKeyState(int key,int down) override { keys.push_back({key,down}); }
    void notifyChar(int c) override { chars.push_back(c); }
    void legacyModeOn() override { ++legacy; }
};
struct Host : ui::UnusedISwiffHost
{
    bool pending = false, flag28 = true;
    int pendingReads = 0;
    std::vector<int> events;
    bool getFlag7B8() override { ++pendingReads; return pending; }
    void setFlag28(bool v) override { flag28 = v; calls.push_back("flag28"); }
    int pushEvent(MenuEvent& e) override { events.push_back(e.type); return 0; }
};
struct Render : ui::UnusedISwiffRender
{
    bool succeeds = true;
    int starts = 0, ends = 0;
    int init() override { calls.push_back("render-init"); return 0; }
    int beginFrame() override { ++starts; return succeeds; }
    int endFrame() override { ++ends; return 0; }
};
struct Settings : UnusedIControlSettings
{
    int backupsDeleted = 0;
    bool apply() override { calls.push_back("apply"); return false; }
    void deleteControlMapBackups() override { ++backupsDeleted; calls.push_back("delete-backups"); }
};
struct Map : io::UnusedIControlMap
{
    std::string name = "fixture-map";
    const std::string& getName() const override { calls.push_back("map-name"); return name; }
};
struct Maps : io::UnusedIControlMapManager
{
    io::IControlMap* active = nullptr;
    int reads = 0;
    io::IControlMap* getActive() const override
    { ++const_cast<Maps*>(this)->reads; calls.push_back("active-map"); return active; }
};
struct Teams : UnusedIMenuTeamManager
{
    int inits = 0;
    bool init() override { ++inits; calls.push_back("teams"); return false; }
};
struct Keyboard : io::UnusedIKeyboardDevice
{
    bool pressed[256] = {}, released[256] = {};
    bool keyPressed(io::InputDeviceKeys k) const override { return pressed[static_cast<unsigned>(k)]; }
    bool keyReleased(io::InputDeviceKeys k) const override { return released[static_cast<unsigned>(k)]; }
};
struct Mouse : io::UnusedIAdvancedInputDevice
{
    int activations = 0, deactivations = 0;
    float x = 1.25f, y = -1.25f;
    bool down = true;
    bool activate() override { ++activations; calls.push_back("mouse-activate"); return false; }
    bool deActivate() override { ++deactivations; calls.push_back("mouse-deactivate"); return false; }
    float readAxis(io::InputDeviceAxes axis,bool scaled) const override
    { check(!scaled, "unscaled mouse axes"); return axis == io::IDAxis_0 ? x : y; }
    bool buttonDown(io::InputDeviceButtons b) const override
    { check(static_cast<unsigned>(b) == 1, "left mouse button"); return down; }
};
struct Devices : io::UnusedIInputDeviceManager
{
    Mouse mouse;
    Keyboard keyboard;
    bool hasMouse = true, hasKeyboard = true;
    int updates = 0, allActivated = 0;
    float dt = 0;
    io::IInputDevice* getInputDevice(io::InputDeviceFlags flag) const override
    {
        if (flag == io::IDFMouse) return hasMouse ? const_cast<Mouse*>(&mouse) : nullptr;
        check(flag == io::IDFKeyboard, "device identity");
        return hasKeyboard ? const_cast<Keyboard*>(&keyboard) : nullptr;
    }
    bool activateDevices(unsigned mask) override
    { check(mask == ~0u, "all devices activated"); ++allActivated; calls.push_back("activate-all"); return false; }
    void update(float v) override { ++updates; dt=v; calls.push_back("input-update"); }
};
struct Console : io::OldConsole
{
    int macros = 0, inputs = 0;
    void updateKeyMacros(const io::GameInput&) override { ++macros; }
    void updateGameInput(const io::GameInput&) override { ++inputs; }
};
struct Events : EventManager
{
    std::vector<unsigned> posted;
    unsigned postEvent(EventCategory category,unsigned id,void* data,float delay) override
    {
        check(static_cast<int>(category) == 10 && !data && delay == 0, "menu notification payload");
        posted.push_back(id); calls.push_back(id == 1 ? "enable" : "disable"); return 0;
    }
};
// Real ClassManager receives just the two rows selected from the production module list.
struct Rows : IClassServer
{
    std::vector<ClassRegInfo> rows;
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned) const override { unexpected("Rows::queryInterface"); }
    void getClassList(std::vector<ClassRegInfo>& out) override { out = rows; }
    void getSingletonList(std::vector<SingletonRegInfo>&) override {}
};

int main()
{
    MenuLogicFixture::layout();
    Devices devices;
    Settings controls;
    Maps maps;
    Map map;
    Teams teams;
    Player player;
    Host host;
    Render render;
    Events events;
    Console console;
    ClassManager manager;
    BF2Engine engine(nullptr);
    BF2EngineSetup setup(&engine);
    BF2EngineFixture::setup(engine, setup);
    bf2Engine = &engine; classManager = &manager; eventManager = &events;
    io::inputDeviceManager = &devices; io::controlMapManager = &maps;
    io::mainConsole = &console; g_controlSettings = &controls;
    IBase* singletons[] = {&player, &host, &render, &teams};
    const char* names[] = {"SwiffPlayer", "SwiffHost", "SwiffRender", "MenuTeamManager"};
    for (int i=0; i<4; ++i) manager.registerSingleton({names[i], &singletons[i], 0, 0, 4});
    ModuleGame module;
    std::vector<ClassRegInfo> rows;
    module.getClassList(rows);
    size_t end = 0;
    for (; end < rows.size() && rows[end].classId != CID_EndOfRoundLogic; ++end) {}
    check(end > 0 && end+2 < rows.size(), "both production registrations exist");
    check(rows[end-1].classId == CID_BetweenRoundsLogic && rows[end+1].classId == CID_LoadGameLogic
          && rows[end+2].classId == CID_LocalProfileManager, "retail registration order");
    check(rows[end].name == "dice.hfe.EndOfRoundLogic" && rows[end].create == &EndOfRoundLogic::create
          && rows[end+1].name == "dice.hfe.LoadGameLogic" && rows[end+1].create == &LoadGameLogic::create,
          "class names and factory identities");
    Rows selected;
    selected.rows = {rows[end], rows[end+1]};
    manager.registerServer(&selected);
    runtimeDedicated = true; rows.clear(); module.getClassList(rows);
    for (const auto& row : rows)
        check(row.classId != CID_EndOfRoundLogic && row.classId != CID_LoadGameLogic, "runtime dedicated rows excluded");
    runtimeDedicated = false;
    io::GameInput input = {};
    for (unsigned cid : {CID_EndOfRoundLogic, CID_LoadGameLogic})
    {
        const bool isEnd = cid == CID_EndOfRoundLogic;
        auto factory = isEnd ? &EndOfRoundLogic::create : &LoadGameLogic::create;
        check(!factory(IID_IMenuLogic, &player), "aggregation refused");
        const int deletedBefore = controls.backupsDeleted;
        check(!factory(9999, nullptr) && controls.backupsDeleted == deletedBefore+1,
              "unsupported IID releases and tears down the new object");
        for (int missing = 0; missing < 3; ++missing)
        {
            IBase* saved = singletons[missing]; singletons[missing] = nullptr;
            auto logic = static_cast<IMenuLogic*>(manager.createInstance(cid, IID_IMenuLogic, nullptr));
            const int stopped = swiffStops;
            check(!logic->init(), "singleton failure reported");
            check(events.posted.back() == 1, "failure retains enable until teardown");
            check(logic->release() == 0 && swiffStops == stopped+1 && events.posted.back() == 2,
                  "partial initialization tears down once");
            singletons[missing] = saved;
        }
        auto logic = static_cast<IMenuLogic*>(manager.createInstance(cid, IID_IMenuLogic, nullptr));
        check(logic && logic->getClassId() == cid && logic->getRef() == 1, "factory and class ID");
        check(logic->queryInterface(1) == logic && logic->queryInterface(IID_IMenuLogic) == logic
              && !logic->queryInterface(3), "only IBase and IMenuLogic exposed");
        if (isEnd) check(MenuLogicFixture::empty(*static_cast<EndOfRoundLogic*>(logic)), "end constructor fields");
        else check(MenuLogicFixture::empty(*static_cast<LoadGameLogic*>(logic)), "load constructor fields");
        logic->addRef(); check(logic->release() == 1, "reference ownership");
        maps.active = &map; maps.reads = 0; calls.clear();
        check(logic->init(), "ignored helper failure results preserve retail success");
        std::vector<std::string> expected = {"swiff-start", "enable", "host"};
        if (!isEnd) expected.push_back("flag28");
        if (!isEnd) expected.push_back("teams");
        for (const char* c : {"active-map", "active-map", "map-name", "apply", "render-init", "render"}) expected.push_back(c);
        expected.push_back(isEnd ? "EndOfRound.swf" : "loadGame.swf");
        for (const char* c : {"width", "height", "scale"}) expected.push_back(c);
        expected.push_back(isEnd ? "mouse-deactivate" : "activate-all");
        check(calls == expected && maps.reads == 2, "initialization order and repeated active-map lookup");
        check(player.sx == 2 && player.sy == 2 && logic->getSwiffPlayer() == &player
              && logic->getSwiffHost() == &host && logic->getSwiffRender() == &render,
              "movie scale and borrowed collaborators");
        if (isEnd) check(MenuLogicFixture::map(*static_cast<EndOfRoundLogic*>(logic)) == map.name, "end map backup");
        else check(MenuLogicFixture::map(*static_cast<LoadGameLogic*>(logic)) == map.name, "load map backup");
        calls.clear(); logic->update(0.125);
        check(calls == (isEnd ? std::vector<std::string>{"movie-update"}
                              : std::vector<std::string>{"movie-update", "input-update"}), "update dispatch order");
        check(player.dt == 0.125f && (isEnd || devices.dt == player.dt), "delta forwarding");
        render.succeeds = false; check(!logic->beginFrame(), "failed renderer begin");
        render.succeeds = true; check(logic->beginFrame(), "begin without movie-loaded or host calls");
        if (isEnd) check(MenuLogicFixture::frame(*static_cast<EndOfRoundLogic*>(logic)), "end frame flag set");
        else check(MenuLogicFixture::frame(*static_cast<LoadGameLogic*>(logic)), "load frame flag set");
        const int displayed = player.displays, ended = render.ends;
        logic->display(); logic->endFrame();
        check(player.displays == displayed+1 && render.ends == ended+1, "frame display and completion");
        player.mouse.clear(); player.keys.clear(); player.chars.clear(); host.events.clear();
        logic->onResize(1600, 1200); logic->onEscape();
        logic->onMouseMove(17, 29); logic->onMouseButtonDown(); logic->handleInput(input);
        check(player.mouse.size() == 1 && player.mouse[0].x == (isEnd ? 17 : 37)
              && player.mouse[0].y == (isEnd ? 29 : -37) && player.mouse[0].buttons == 1,
              "cached end pointer or sampled/truncated load pointer");
        logic->onMouseButtonUp(); devices.mouse.down = false; logic->handleInput(input);
        check(player.mouse.back().buttons == 0, "button release"); devices.mouse.down = true;
        const int legacy = player.legacy; logic->onMouseDoubleClick();
        check(player.legacy == legacy + (isEnd ? 1 : 0), "double click behavior");
        devices.keyboard.pressed[15] = true; devices.keyboard.pressed[30] = true;
        devices.keyboard.released[28] = true; translatedKey = 30; translationResult = 1; translatedChar = 0xE4;
        logic->handleInput(input);
        check(player.chars == std::vector<int>{0xE4}
              && player.keys == std::vector<std::pair<int,int>>{{15,1},{28,0}}, "Latin-1 text and key transitions");
        devices.keyboard = Keyboard(); player.keys.clear(); player.chars.clear();
        devices.keyboard.pressed[30] = true; translatedChar = 0x80; logic->handleInput(input);
        check(player.keys.empty() && player.chars.empty(), "nonprintable translated byte discarded");
        translationResult = 0; logic->handleInput(input);
        check(player.keys == std::vector<std::pair<int,int>>{{30,1}}, "untranslated scan code forwarded");
        devices.keyboard = Keyboard(); player.keys.clear(); devices.hasKeyboard = false;
        logic->handleInput(input); devices.hasKeyboard = true;
        devices.keyboard.pressed[1] = true;
        // Non-null game sentinel is only tested for presence; no game method may be called.
        game = reinterpret_cast<IGame*>(1); host.pending = true;
        logic->handleInput(input);
        check(host.events == (isEnd ? std::vector<int>{} : std::vector<int>{9}), "Escape pending-request rule");
        host.events.clear(); host.pending = false; logic->handleInput(input);
        check(host.events == std::vector<int>{isEnd ? 15 : 9}, "Escape cancellation event");
        game = nullptr; host.events.clear(); devices.keyboard = Keyboard();
        if (!isEnd)
        {
            BF2Console bfconsole;
            logic->setConsole(&bfconsole); check(!logic->getConsole(), "stored console getter stays null");
            input.mappingFlags = 2; input.inputs[1] = 0.5f;
            const int toggles = consoleToggles; logic->handleInput(input);
            check(consoleToggles == toggles, "console threshold is strictly greater");
            input.inputs[1] = 1; consoleCharCount = 5; consoleCharWrite = 8; consoleCharRead = 2;
            const size_t samples = player.mouse.size(); const int inputs = console.inputs;
            logic->handleInput(input);
            check(bfconsole.getConsoleActive() && consoleCharCount == 0 && consoleCharRead == 8
                  && console.inputs == inputs+1 && player.mouse.size() == samples, "console toggle drains ring and captures input");
            input.inputs[1] = std::numeric_limits<float>::quiet_NaN(); logic->handleInput(input);
            check(consoleToggles == toggles+1, "unordered console input does not toggle");
            logic->setConsole(nullptr); input = {};
        }
        const size_t posted = events.posted.size(); const int stops = swiffStops;
        logic->deactivate(); check(events.posted.size() == posted+1 && swiffStops == stops+1, "explicit deactivation");
        check(logic->release() == 0 && events.posted.size() == posted+1 && swiffStops == stops+1,
              "destructor repeats backup cleanup without duplicate disable or unload");
        maps.active = nullptr;
    }
    manager.unregisterServer(&selected);
    std::printf("%u menu logic checks passed (native sizes EndOfRoundLogic=88, LoadGameLogic=72)\n", checks);
}
