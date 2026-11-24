// [TEMP] [CHOSEN, not recovered] Bounded production-console fixture. No application,
// sockets, renderer, audio device or game content. Stream and receiver observers model
// only command arguments/results and configuration I/O; this is not a gameplay test.
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/ConsoleObjectMacro.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/Main/Game.h"
#include "../BF2/Game/GameLogic/GameLogic.h"
#include "../BF2/Game/GameLogic/Player.h"
#include "../BF2/Game/GameLogic/VoiceMessages.h"
#include "../BF2/Game/GameServer/ServerSettings.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/IO/File/FileManager.h"
#include "../BF2/IO/Stream/IStream.h"
#include "../BF2/IO/Input/PlayerInput.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../BF2/AI/AIInterface/Module/BFConsole.h"
#include "../BF2/AI/AIInterface/BFKnowledge/KitTemplateManager.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace dice { namespace hfe {
extern ClassManager* classManager;
ConsoleObjectList& gameLogicConsoleObjects();
namespace world { ConsoleObjectList& objectManagerConsoleObjects(); }
namespace ai { std::ostream& operator<<(std::ostream&, KitType); }
} }
using namespace dice::hfe;
namespace {
unsigned checks;
void check(bool good, const char* message)
{
    ++checks;
    if (!good) { std::fprintf(stderr, "FAIL: %s\n", message); std::_Exit(2); }
}
ConsoleObject* row(const char* object, const char* command)
{
    for (ConsoleObject* entry : mainConsoleObjects->getConsoleObjects())
        if (std::strcmp(entry->getObjectName(), object) == 0 &&
            std::strcmp(entry->getFunctionName(), command) == 0)
            return entry;
    std::fprintf(stderr, "Missing: %s.%s\n", object, command);
    std::_Exit(2);
}
std::string invoke(ConsoleObject* command, std::initializer_list<std::string> arguments)
{
    std::vector<std::string> args(arguments);
    return command->execute(args.data(), static_cast<int>(args.size()));
}
struct MemoryStream : io::INonlinearStream
{
    std::string text;
    unsigned position = 0, releases = 0;
    bool shortWrite = false;
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return ++releases; }
    void* queryInterface(unsigned) const override { std::abort(); }
    unsigned read(void* out, unsigned size) override
    {
        size = std::min(size, remaining());
        std::memcpy(out, text.data() + position, size); position += size;
        return size;
    }
    unsigned write(const void* data, unsigned size) override
    {
        text.assign(static_cast<const char*>(data), size);
        return shortWrite && size ? size - 1 : size;
    }
    unsigned remaining() const { return static_cast<unsigned>(text.size()) - position; }
    int skip(unsigned n) override { position += n; return position; }
    int getAvailableBytes() override { return remaining(); }
    io::IStream* clone() override { std::abort(); }
    bool canRead() override { return true; }
    bool canWrite() override { return true; }
    int seek(io::SeekOrigin origin, int n) override
    {
        position = (origin == io::SEEKORIGIN_BEGIN ? 0 : origin == io::SEEKORIGIN_CURRENT ? position : text.size()) + n;
        return position;
    }
    int getPosition() override { return position; }
    int getSize() override { return text.size(); }
};
struct Files : io::FileManager
{
    MemoryStream input, output;
    std::string path;
    bool exists = true, failOpen = false;
    bool fileExists(const std::string& name) override { path = name; return exists; }
    io::IStream* open(const std::string& name, io::FileMode mode, io::FileAccess access, unsigned flags) override
    {
        path = name;
        if (failOpen) return nullptr;
        if (static_cast<int>(mode) == 2)
        {
            check(static_cast<int>(access) == 0 && flags == 2, "load opens console script for reading");
            input.position = 0;
            return &input;
        }
        check(static_cast<int>(mode) == 0 && static_cast<int>(access) == 2 && flags == 0,
              "save preserves mode/access/flags");
        return &output;
    }
};
struct Logic : GameLogic
{
    // Unused pure slots must stop if this fixture accidentally reaches them.
#define UNUSED_SLOT(result, name, args) result name args override { std::abort(); }
    UNUSED_SLOT(void, addRef, ())
    UNUSED_SLOT(int, getRef, ())
    UNUSED_SLOT(int, release, ())
    UNUSED_SLOT(void*, queryInterface, (unsigned) const)
    UNUSED_SLOT(void, runNextLevel, ())
    UNUSED_SLOT(void, setNextLevel, (int))
    UNUSED_SLOT(bool, selectKitAndUnlockLevel, (world::IPlayer*, int, int))
    UNUSED_SLOT(void, setTicketState, (unsigned, int))
    UNUSED_SLOT(int, getTicketState, (unsigned))
    UNUSED_SLOT(void, setDefaultNumberOfTickets, (unsigned, unsigned))
    UNUSED_SLOT(unsigned, getDefaultNumberOfTickets, (unsigned))
    UNUSED_SLOT(void, setDefaultNumberOfTicketsEx, (unsigned, unsigned, unsigned))
    UNUSED_SLOT(unsigned, getDefaultNumberOfTicketsEx, (unsigned, unsigned))
    UNUSED_SLOT(void, setDefaultTimeToNextAIWave, (float))
    UNUSED_SLOT(float, getDefaultTimeToNextAIWave, ())
    UNUSED_SLOT(void, setTicketLossAtEndPerMin, (float))
    UNUSED_SLOT(float, getTicketLossAtEndPerMin, ())
    UNUSED_SLOT(void, setTicketLossPerMin, (unsigned, float))
    UNUSED_SLOT(float, getTicketLossPerMin, (unsigned))
    UNUSED_SLOT(void, playEffect, (Vec3, const world::IObjectTemplate*, Vec3))
    UNUSED_SLOT(bool, handleCollisionForProjectile, (world::IObject*, world::IObject*, const Vec3&, const Vec3&, const Vec3&, int, int, world::CollisionFaceInfo*, world::CollisionFaceInfo*))
    UNUSED_SLOT(bool, handleCollisionForReplenisher, (bool, world::IObject*, world::IObject*, const Vec3&, const Vec3&, const Vec3&, int, int))
    UNUSED_SLOT(bool, handleCollisionForResurrecter, (world::IObject*, world::IObject*, float, float, const Vec3&, const Vec3&, const Vec3&, int, int))
    UNUSED_SLOT(void, handleCollision, (world::IObject*, world::IObject*, const Vec3&, const Vec3&, const Vec3&, int, int, world::CollisionFaceInfo*, world::CollisionFaceInfo*))
    UNUSED_SLOT(void, spawnMultipleObjects, (const world::IObjectTemplate*, int))
    UNUSED_SLOT(world::IObject*, spawnObject, (const std::string&, Vec3, Vec3, int))
    UNUSED_SLOT(world::IObject*, spawnObject, (const world::IObjectTemplate*, Vec3, Vec3, int))
    UNUSED_SLOT(void, checkPlayerTriggers, (world::IPlayer*, world::IObject*, io::PlayerInput, bool))
    UNUSED_SLOT(void, toggleLocalFreeCamera, ())
    UNUSED_SLOT(TicketHistory&, getTicketHistory, ())
    UNUSED_SLOT(void, setTicketHistory, (const TicketHistory&))
#undef UNUSED_SLOT
    int calls = 0, team = -1, kit = -1;
    std::string kitName, soldierName;
    void setKit(int t, int k, const std::string& name, const std::string& soldier) override
    { ++calls; team = t; kit = k; kitName = name; soldierName = soldier; }
    void voice(VoiceMessages* value) { m_voiceMessages = value; }
};
struct PlayerObserver : Player
{
    PlayerObserver() : Player(-1, nullptr) {}
    Vec3 position;
    std::string moveTo(const Vec3& value) override { position = value; return "moved"; }
};
struct Players : world::PlayerManager
{
    PlayerObserver* player = nullptr;
    int creates = 0;
    world::IPlayer* createPlayer(const std::string& type, const std::string& name, int id) override
    {
        check(type == "Player" && name == "Console Player" && id == -1, "Player.create forwards both strings and automatic ID");
        ++creates;
        return player;
    }
    world::IPlayer* getActivePlayer() const override { return player; }
};
}

extern "C" int bf2ConsoleCommandsMain(int, char**)
{
#if !DEDICATED
    setDedicated(true); // Constructor-only environment: prevents renderer/tutorial initialization.
#endif
    g_system = new System;
    classManager = new ClassManager;
    eventManager = new EventManager;
    settings = new SettingsRepostitory;
    io::mainConsole = new io::OldConsole;
    io::mainConsole->setAccess(2);
    Files* files = new Files;
    io::fileManager = files;
    mainConsoleObjects->registerConsoleObjects(gameLogicConsoleObjects());
    mainConsoleObjects->registerConsoleObjects(world::objectManagerConsoleObjects());
    mainConsoleObjects->registerConsoleObjects(ai::kitTemplateConsoleObjects());

    struct Metadata { const char* object; const char* command; int type, min, max; const char* result; bool dynamic; int access; };
    const Metadata expected[] = {
        {"gameLogic", "getTeamLanguage", 0, 1, 1, "std::string", true, 2},
        {"gameLogic", "getTeamName", 0, 1, 1, "std::string", true, 2},
        {"gameLogic", "defaultLevel", 1, 0, 1, "std::string", true, 2},
        {"gameLogic", "messages.playRadioMessage", 0, 4, 4, "int", true, 2},
        {"gameLogic", "messages.playHelpMessage", 0, 1, 1, "int", true, 2},
        {"player", "pickup", 0, 1, 1, "void", false, 2},
        {"player", "moveTo", 0, 1, 1, "std::string", true, 2},
        {"player", "create", 0, 2, 2, "IPlayer*", false, 2},
        {"object", "template", 2, 0, 0, "const IObjectTemplate*", true, 2},
        {"objectTemplate", "writeBinary", 0, 1, 1, "void", false, 2},
        {"objectTemplate", "readBinary", 0, 1, 1, "void", false, 2},
        {"kitTemplate", "getType", 0, 0, 0, "KitType", true, 2},
        {"reservedSlots", "load", 0, 0, 0, "bool", true, 1},
        {"reservedSlots", "save", 0, 0, 0, "bool", true, 1},
        {"reservedSlots", "configFile", 1, 0, 1, "std::string", true, 1},
        {"reservedSlots", "addNick", 0, 1, 1, "void", false, 1},
        {"reservedSlots", "removeNick", 0, 1, 1, "void", false, 1},
        {"reservedSlots", "clear", 0, 0, 0, "void", false, 1},
        {"reservedSlots", "list", 0, 0, 0, "std::string", true, 1},
    };
    for (const Metadata& m : expected)
    {
        ConsoleObject* c = row(m.object, m.command);
        check(c->getType() == m.type && c->getMinNoArgs() == m.min && c->getMaxNoArgs() == m.max &&
              std::strcmp(c->getReturnType(), m.result) == 0 && c->isDynamic() == m.dynamic &&
              c->getAccess() == m.access && c->getCategory() == 0, m.command);
    }

    gameLogic = nullptr;
    world::playerManager = nullptr;
    check(!row("gameLogic", "getTeamName")->isActive() && !row("gameLogic", "messages.playHelpMessage")->isActive(), "absent game-logic gates");
    check(!row("player", "pickup")->isActive() && !row("player", "moveTo")->isActive(), "absent player-manager gates");
    check(row("player", "create")->isActive(), "Player.create is always active");
    check(!row("kitTemplate", "getType")->isActive(), "absent kit-manager gate");

    const auto line = [](const std::string& text) {
        std::string out;
        io::mainConsole->executeLine(text, out, false, false, false, false, false);
        return out;
    };
    io::mainConsole->setAccess(1);
    check(line("reservedSlots.configFile mods/bf2/settings/reservedSlots.con").empty(), "admin sets reserved-list path");
    check(line("reservedSlots.configFile") == "mods/bf2/settings/reservedSlots.con", "admin reads reserved-list path");
    line("reservedSlots.clear");
    files->input.text = "reservedSlots.addNick \"TAG Alice\"\nreservedSlots.addNick Alice\nreservedSlots.addNick alice";
    io::mainConsole->setUseRelativePaths(true);
    io::mainConsole->setWorkingPath("Levels/unused/");
    check(line("reservedSlots.load") == "1", "reserved script loads through real console");
    check(files->path == "mods/bf2/settings/reservedSlots.con" && io::mainConsole->getUseRelativePaths(), "load uses configured path and restores relative setting");
    check(line("reservedSlots.list") == "Alice\nalice\n", "prefix stripping, deduplication, case and order");
    check(reservedSlots->isNickReserved("OTHER Alice") && !reservedSlots->isNickReserved("ALICE"), "reserved admission lookup sees loaded nicknames");
    line("reservedSlots.removeNick \"TAG Alice\"");
    check(line("reservedSlots.list") == "Alice\nalice\n", "removal compares exact stored spelling");
    line("reservedSlots.removeNick Alice");
    check(line("reservedSlots.list") == "alice\n", "removal removes matching nickname");
    check(line("reservedSlots.save") == "1" && files->output.text == "reservedSlots.addNick \"alice\"\n" && files->output.releases == 1, "save serializes quoted script and releases stream");
    files->output.shortWrite = true;
    check(line("reservedSlots.save") == "0" && files->output.releases == 2, "short write fails and releases stream");
    files->failOpen = true;
    check(line("reservedSlots.save") == "0", "failed save open fails visibly");
    check(line("reservedSlots.load") == "1", "load retains shipped disregard of console-run result");
    files->failOpen = false;
    files->exists = false;
    check(line("reservedSlots.load") == "0", "missing reserved file fails");
    files->exists = true;
    check(line("reservedSlots.list") == "alice\n", "failed loads preserve names");
    line("reservedSlots.configFile \"\"");
    check(line("reservedSlots.save") == "0" && line("reservedSlots.load") == "0", "empty configuration path fails");
    line("reservedSlots.clear");
    check(line("reservedSlots.list").empty(), "clear empties the list");
    io::mainConsole->setAccess(2);

    Logic* logic = new Logic;
    gameLogic = logic;
    logic->setTeamName(1, "US");
    logic->setTeamLanguage(1, "English");
    check(line("gameLogic.getTeamName 1") == "US" && line("gameLogic.getTeamLanguage 1") == "English", "team string getters dispatch");
    line("gameLogic.defaultLevel strike_at_karkand");
    check(line("gameLogic.defaultLevel") == "strike_at_karkand", "default-level read/write dispatch");
    ConsoleObject* setKit = row("gameLogic", "setKit");
    check(std::strcmp(setKit->getArgType(1), "KitType") == 0, "setKit keeps named argument metadata");
    const char* kitNames[] = {"Specops", "Sniper", "Assault", "Support", "Engineer", "Medic", "AT"};
    for (int k = 0; k < 7; ++k)
        for (const std::string& spelling : {std::string(kitNames[k]), "KT" + std::string(kitNames[k]), std::to_string(k)})
        {
            invoke(setKit, {"1", spelling, "US_Medic", "us_light_soldier"});
            check(logic->team == 1 && logic->kit == k && logic->kitName == "US_Medic" && logic->soldierName == "us_light_soldier", "setKit converts names and numbers");
        }
    for (const char* invalid : {"unknown", "medic", "KTExtended1", "7", "", "KTNoKit"})
    {
        invoke(setKit, {"1", "Medic", "US_Medic", "us_light_soldier"});
        invoke(setKit, {"1", invalid, "US_Medic", "us_light_soldier"});
        check(logic->kit == 13, "invalid/empty kit overwrites prior converted value");
    }
    int calls = logic->calls;
    invoke(setKit, {"1", "Medic"});
    check(logic->calls == calls, "setKit rejects wrong arity");
    check(!row("gameLogic", "messages.playRadioMessage")->isActive(), "missing voice-owner gate");
    logic->voice(new VoiceMessages("", "", ""));
    check(row("gameLogic", "messages.playRadioMessage")->isActive() && row("gameLogic", "messages.playHelpMessage")->isActive(), "voice-owner enables playback commands");
    check(line("gameLogic.messages.playRadioMessage 1 missing missing missing") == "-1", "radio binding returns real missing-name refusal");
    check(line("gameLogic.messages.playHelpMessage missing") == "-1", "help binding returns real missing-key refusal without audio");

    Players* players = new Players;
    world::playerManager = players;
    check(!row("player", "moveTo")->isActive(), "empty active-player gate");
    players->player = new PlayerObserver;
    std::ostringstream pointer;
    pointer << static_cast<world::IPlayer*>(players->player);
    check(invoke(row("player", "create"), {"Player", "Console Player"}) == pointer.str() && players->creates == 1, "Player.create publishes typed pointer");
    invoke(row("player", "create"), {"Player"});
    check(players->creates == 1, "Player.create rejects wrong arity");
    check(line("player.moveTo 10/20/30") == "moved" && players->player->position.x == 10 && players->player->position.y == 20 && players->player->position.z == 30, "moveTo dispatches through concrete player's virtual");

    world::objectTemplateManager = new world::ObjectTemplateManager;
    check(line("ObjectTemplate.writeBinary unused.bin").empty() && line("ObjectTemplate.readBinary unused.bin").empty(), "cache commands retain retail no-op behavior");
    ai::KitTemplateManager* kits = new ai::KitTemplateManager;
    check(!row("kitTemplate", "getType")->isActive(), "empty active-kit gate");
    const char* types[] = {"KTSpecops", "KTSniper", "KTAssault", "KTSupport", "KTEngineer", "KTMedic", "KTAT", "KTExtended1", "KTExtended2", "KTExtended3", "KTExtended4", "KTExtended5", "KTExtended6"};
    for (int k = 0; k < 13; ++k)
    {
        check(kits->create(types[k], static_cast<KitType>(k)) ==
              "KitTemplate '" + std::string(types[k]) + "' successfully created", "create typed AI kit");
        check(line("kitTemplate.getType") == types[k], "AI kit type formatter names stock and extended kits");
    }
    for (int invalid : {-1, 13, 99})
    {
        std::ostringstream out;
        out << "prefix";
        ai::operator<<(out, static_cast<KitType>(invalid));
        check(out.str() == "prefix", "invalid AI kit values append no token");
    }
    std::printf("PASS console commands: %u checks (DEDICATED=%d)\n", checks, DEDICATED);
    std::fflush(stdout);
    std::_Exit(0); // No full-engine teardown for this bounded, pre-init fixture.
}
