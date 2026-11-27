// [TEMP] [CHOSEN, not recovered] Bounded production-console fixture. No application,
// sockets, renderer, audio device or game content. Stream and receiver observers model
// only command arguments/results and configuration I/O; this is not a gameplay test.
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/ConsoleObjectMacro.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/Main/Game.h"
#include "../BF2/Game/Python/PythonHost.h"
#include "../BF2/Game/GameLogic/GameLogic.h"
#include "../BF2/Game/GameLogic/ServerGameLogic.h"
#include "../BF2/Game/GameLogic/ScoreSystem/ScoreManager.h"
#include "../BF2/Game/GameLogic/SquadManager/ISquadManager.h"
#include "../BF2/Game/GameServer/GameServer.h"
#include "../BF2/Game/Common/GameEvent.h"
#include "../BF2/Game/Objects/Object.h"
#include "../BF2/Game/Objects/ObjectLayoutAccess.h"
#include "../BF2/Game/Objects/ICameraObject.h"
#include "../BF2/Physics/PhysicsNode.h"
#include "../BF2/Physics/PhysicsNodeManager.h"
#include "../BF2/Game/GameLogic/Player.h"
#include "../BF2/Game/GameLogic/VoiceMessages.h"
#include "../BF2/Game/GameServer/ServerSettings.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/IO/File/FileManager.h"
#include "../BF2/IO/Stream/IStream.h"
#include "../BF2/IO/Input/PlayerInput.h"
#include "../BF2/IO/Network/NetworkManager.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/BinTreeCuller.h"
#include "../BF2/Scene/Object/QuadTreeCuller.h"
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
extern IBase* squadManager;
ConsoleObjectList& gameLogicConsoleObjects();
ConsoleObjectList& moduleGameConsoleObjects();
ConsoleObjectList& serverSettingsConsoleObjects();
namespace world { extern IBase* scoreManager; }
namespace world { extern PhysicsNodeManager* physicsNodeManager; }
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
bool at(const Vec3& value, float x, float y, float z)
{
    return value.x == x && value.y == y && value.z == z;
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
    void roundState(int status, int phase) { m_gameStatus = status; m_beginRoundState = phase; }
};
struct PlayerObserver : Player
{
    PlayerObserver() : Player(-1, nullptr) {}
    Vec3 position;
    world::IObject* vehicle = nullptr;
    world::ICameraObject* camera = nullptr;
    world::IObject* getVehicle() const override { return vehicle; }
    world::ICameraObject* getCamera() const override { return camera; }
    world::IObject* getFreeCamera() const override { std::abort(); }
    std::string moveTo(const Vec3& value) override { position = value; return "moved"; }
};
struct Players : world::PlayerManager
{
    PlayerObserver* player = nullptr;
    PlayerObserver* local = nullptr;
    int creates = 0;
    std::list<world::IPlayer*> members;
    int memberReads = 0, memberTeam = -1, memberSquad = -1;
    const std::list<world::IPlayer*>& getPlayersInSquad(int team, int squad) override
    { ++memberReads; memberTeam = team; memberSquad = squad; return members; }
    world::IPlayer* getLocalHumanPlayer() override { return local; }
    world::IPlayer* createPlayer(const std::string& type, const std::string& name, int id) override
    {
        check(type == "Player" && name == "Console Player" && id == -1, "Player.create forwards both strings and automatic ID");
        ++creates;
        return player;
    }
    world::IPlayer* getActivePlayer() const override { return player; }
};

struct Server : GameServer
{
    mutable int interfaceQueries = 0;
    void* queryInterface(unsigned iid) const override
    { ++interfaceQueries; return GameServer::queryInterface(iid); }
    // Constructor/reset broadcasts are consumed here; this fixture has no network peers.
    void sendGameEventToAllPlaying(GameEvent* event) override { delete event; }
    void sendGameEventToAllConnected(GameEvent* event) override { delete event; }
};
struct SquadPlayer : PlayerObserver
{
    int id = 31, team = 2, squad = 7;
    std::string name = "Squad Player";
    mutable std::vector<int> queries;
    int getId() const override { queries.push_back(1); return id; }
    int getSquadId() const override { queries.push_back(2); return squad; }
    int getTeam() const override { queries.push_back(3); return team; }
    const std::string& getName() const override { return name; }
};
struct SquadInfo : ISquadInfo
{
    int id = 0;
    std::string name;
    bool isPrivate = false;
    const std::string& getSquadName() override { return name; }
    bool getIsPrivate() override { return isPrivate; }
    int getSquadId() override { return id; }
    bool getIsAccepted(SquadOrderType, OrderType) override { std::abort(); }
    SquadOrder* getSquadOrders() override { std::abort(); }
    SquadOrder* getCommanderOrders() override { std::abort(); }
    SquadOrder* getRequest(SquadOrderType) override { std::abort(); }
    int getSpawnGroupId() override { std::abort(); }
    void addSquadLeaderSpawnPoint(world::IPlayer*) override { std::abort(); }
    void removeSquadLeaderSpawnPoint(world::IPlayer*) override { std::abort(); }
};
struct Squads : ISquadManager
{
    // Only listing and renaming belong to this fixture; other interface calls must stop.
#define UNUSED_SQUAD_SLOT(result, name, args) result name args override { std::abort(); }
    UNUSED_SQUAD_SLOT(void, addRef, ())
    UNUSED_SQUAD_SLOT(int, getRef, ())
    UNUSED_SQUAD_SLOT(int, release, ())
    UNUSED_SQUAD_SLOT(void*, queryInterface, (unsigned) const)
    UNUSED_SQUAD_SLOT(void, reset, ())
    UNUSED_SQUAD_SLOT(void, reAddDynamicSpawnGroups, ())
    UNUSED_SQUAD_SLOT(void, sendSquadManagerState, (int))
    UNUSED_SQUAD_SLOT(void, setIsPrivate, (int, int, bool, bool))
    UNUSED_SQUAD_SLOT(bool, setIsAccepted, (int, int, bool, OrderType, SquadOrderType, bool))
    UNUSED_SQUAD_SLOT(void, inviteToSquad, (int, int, int, bool, bool))
    UNUSED_SQUAD_SLOT(bool, addToSquad, (int, int, int))
    UNUSED_SQUAD_SLOT(void, removeFromSquad, (int, int, int, bool))
    UNUSED_SQUAD_SLOT(void, setSquadLeader, (int, int, int, bool))
    UNUSED_SQUAD_SLOT(int, getSquadLeader, (int, int))
    UNUSED_SQUAD_SLOT(int, getFirstUnusedSquad, (int) const)
    UNUSED_SQUAD_SLOT(void, issueSquadOrder, (int, int, SquadOrder&, bool))
    UNUSED_SQUAD_SLOT(void, issueCommanderOrder, (int, int, SquadOrder&, bool))
    UNUSED_SQUAD_SLOT(void, issueRequest, (int, int, SquadOrder&, bool))
    UNUSED_SQUAD_SLOT(void, resetSquad, (int, int))
    UNUSED_SQUAD_SLOT(void, findNewSquadLeader, (int, int, int))
    UNUSED_SQUAD_SLOT(void, applyForCommander, (int, int))
    UNUSED_SQUAD_SLOT(int, popNextCommanderApplicant, (int))
    UNUSED_SQUAD_SLOT(void, acceptCommander, (int, int))
    UNUSED_SQUAD_SLOT(void, declineCommander, (int, int))
    UNUSED_SQUAD_SLOT(void, resignCommander, (int))
    UNUSED_SQUAD_SLOT(void, setCommander, (int, int))
    UNUSED_SQUAD_SLOT(int, getCommander, (int))
    UNUSED_SQUAD_SLOT(void*, getCommanderCam, (int))
    UNUSED_SQUAD_SLOT(void, setCommanderToSoldierMode, (int))
    UNUSED_SQUAD_SLOT(void, setCommanderToOverheadCamMode, (int))
    UNUSED_SQUAD_SLOT(void, setCommanderOverheadCamZoomMode, (int, bool))
    UNUSED_SQUAD_SLOT(void, removeCommanderApplicant, (int, int))
    UNUSED_SQUAD_SLOT(void, addOrderMarker, (int, SquadOrder&))
    UNUSED_SQUAD_SLOT(void, removeAllOrderMarkers, ())
    UNUSED_SQUAD_SLOT(void, removeOrderMarker, (int))
    UNUSED_SQUAD_SLOT(void, updateOrderMarkers, (world::IPlayer*))
#undef UNUSED_SQUAD_SLOT
    std::map<int, ISquadInfo*> entries;
    std::vector<int> reads;
    int renames = 0;
    bool accept = true;
    ISquadInfo* getSquadInfo(int team, int squad) override
    { check(team == 2, "squad listing forwards team"); reads.push_back(squad); return entries[squad]; }
    bool setSquadName(int team, int squad, int player, const std::string& name, bool flag) override
    {
        ++renames;
        check(team == 2 && squad == 7 && player == 31 && name == "Two words" && !flag,
              "rename forwards local player identity and forces false");
        return accept;
    }
};
struct SquadEvents : EventManager
{
    int refusals = 0;
    unsigned postEvent(EventCategory category, unsigned id, void* data, float delay) override
    {
        check(category == ECHud && id == 15 && !data && delay == 0.0f,
              "rename refusal posts the immediate HUD notification");
        ++refusals;
        return 0;
    }
};
struct Scores : world::ScoreManager
{
    unsigned reads = 0;
    world::TeamScore* getTeamScore(unsigned team) override
    { ++reads; return world::ScoreManager::getTeamScore(team); }
};
struct Classes : ClassManager
{
    IBase* createInstance(unsigned cid, unsigned iid, IBase* outer) override
    {
        if (cid == world::CID_BinTreeCuller) return world::BinTreeCuller::create(iid, outer);
        if (cid == world::CID_QuadTreeCuller) return world::QuadTreeCuller::create(iid, outer);
        return ClassManager::createInstance(cid, iid, outer);
    }
};
std::vector<int> teleportCalls;
struct Node : world::PhysicsNode
{
    void reset(bool full) override
    { check(full, "teleport requests a full physics reset"); teleportCalls.push_back(2); }
};
struct TeleportObject : world::Object, world::ICameraObject
{
    TeleportObject() : world::Object(nullptr) {}
    Vec3 position;
    world::IPhysicsNode* installOnMove = nullptr;
    void setAbsolutePosition(const Vec3& value) override
    {
        position = value;
        teleportCalls.push_back(1);
        if (installOnMove)
            world::object_detail::LayoutAccess::physicsNode(this) = installOnMove;
    }
    void* queryInterface(unsigned iid) const override
    {
        check(iid == world::IID_IObject, "camera teleport queries IObject");
        teleportCalls.push_back(3);
        return static_cast<world::IObject*>(const_cast<TeleportObject*>(this));
    }
#define UNUSED_CAMERA(result, name, args) result name args override { std::abort(); }
    UNUSED_CAMERA(void, addRef, ())
    UNUSED_CAMERA(int, getRef, ())
    UNUSED_CAMERA(int, release, ())
    UNUSED_CAMERA(const Mat4&, getTransformation, (float))
    UNUSED_CAMERA(void, setInterpolatedMode, (bool))
    UNUSED_CAMERA(bool, setViewMode, (world::CameraViewMode, world::IObject*, bool))
    UNUSED_CAMERA(world::CameraViewMode, getViewMode, () const)
    UNUSED_CAMERA(bool, isActive, () const)
    UNUSED_CAMERA(void, setIsActive, (bool))
    UNUSED_CAMERA(void, setCameraId, (int))
    UNUSED_CAMERA(int, getCameraId, ())
    UNUSED_CAMERA(void, prevViewMode, ())
    UNUSED_CAMERA(void, setTraceObjectId, (unsigned))
    UNUSED_CAMERA(unsigned, getTraceObjectId, ())
    UNUSED_CAMERA(void, setExternCameraTrans, (Mat4))
    UNUSED_CAMERA(Mat4*, getExternCameraTrans, ())
    UNUSED_CAMERA(bool, getToggleMouseLook, ())
    UNUSED_CAMERA(int, getPcoId, ())
    UNUSED_CAMERA(void, handleExplosion, (float))
    UNUSED_CAMERA(void, resetCameraView, (bool))
    UNUSED_CAMERA(void, updateCameraView, (bool))
    UNUSED_CAMERA(float, getActiveFOV, () const)
    UNUSED_CAMERA(const Vec3&, getStaticSunDirection, () const)
    UNUSED_CAMERA(const Vec3&, getStaticSunColor, () const)
    UNUSED_CAMERA(const Vec3&, getStaticSunDamageColor, () const)
    UNUSED_CAMERA(const Vec3&, getStaticSunAmbientColor, () const)
#undef UNUSED_CAMERA
};
}

extern "C" int bf2ConsoleCommandsMain(int, char**)
{
#if !DEDICATED
    setDedicated(true); // Constructor-only environment: prevents renderer/tutorial initialization.
#endif
    g_system = new System;
    classManager = new Classes;
    eventManager = new EventManager;
    settings = new SettingsRepostitory;
    io::mainConsole = new io::OldConsole;
    io::mainConsole->setAccess(2);
    Files* files = new Files;
    io::fileManager = files;
    mainConsoleObjects->registerConsoleObjects(gameLogicConsoleObjects());
    mainConsoleObjects->registerConsoleObjects(moduleGameConsoleObjects());
    mainConsoleObjects->registerConsoleObjects(serverSettingsConsoleObjects());
    mainConsoleObjects->registerConsoleObjects(world::objectManagerConsoleObjects());
    mainConsoleObjects->registerConsoleObjects(ai::kitTemplateConsoleObjects());

    struct Metadata { const char* object; const char* command; int type, min, max; const char* result; bool dynamic; int access; };
    const Metadata expected[] = {
        {"squadManager", "changeSquadName", 0, 2, 2, "void", false, 2},
        {"squadManager", "listSquadMembers", 0, 2, 2, "std::string", true, 2},
        {"squadManager", "listSquads", 0, 1, 1, "std::string", true, 2},
        {"pythonHost", "sendCommand", 0, 1, 9, "bool", true, 2},
        {"gameLogic", "getTeamLanguage", 0, 1, 1, "std::string", true, 2},
        {"gameLogic", "getTeamName", 0, 1, 1, "std::string", true, 2},
        {"gameLogic", "defaultLevel", 1, 0, 1, "std::string", true, 2},
        {"gameLogic", "teleportTo", 0, 1, 1, "void", false, 2},
        {"gameLogic", "teleportCameraTo", 0, 1, 1, "void", false, 2},
        {"gameLogic", "getNumberOfTickets", 0, 1, 1, "U32", true, 2},
        {"sv", "maxConnectionType", 1, 0, 1, "ConnectionType", true, 1},
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
    game = nullptr;
    for (const char* name : {"changeSquadName", "listSquadMembers", "listSquads"})
        check(!row("squadManager", name)->isActive(), "squad commands require the global game");
    check(std::strcmp(row("squadManager", "changeSquadName")->getArgType(0), "std::string") == 0 &&
          std::strcmp(row("squadManager", "changeSquadName")->getArgType(1), "bool") == 0 &&
          std::strcmp(row("squadManager", "listSquadMembers")->getArgType(0), "int") == 0 &&
          std::strcmp(row("squadManager", "listSquadMembers")->getArgType(1), "int") == 0 &&
          std::strcmp(row("squadManager", "listSquads")->getArgType(0), "int") == 0,
          "squad argument metadata preserves retail types");
    for (const char* name : {"teleportTo", "teleportCameraTo", "getNumberOfTickets"})
        check(!row("gameLogic", name)->isActive(), "server command requires game logic");
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
    serverSettings = new ServerSettings;
    check(line("sv.maxConnectionType") == "CTLanT1", "default connection ceiling prints its name");
    const char* connections[] = {"CTConnecting", "CTModem28Kbps", "CTModem56Kbps",
        "CTCable128Kbps", "CTCable256Kbps", "CTLanT1"};
    for (int i = 0; i < 6; ++i)
    {
        check(line(std::string("sv.maxConnectionType ") + connections[i]).empty() &&
              serverSettings->m_maxConnectionType == i && line("sv.maxConnectionType") == connections[i],
              "connection names parse and print their exact values");
    }
    for (const char* invalid : {"unknown", "ctconnecting", "4", "6", "-1", "CTNumTypes", ""})
    {
        invoke(row("sv", "maxConnectionType"), {"CTLanT1"});
        check(invoke(row("sv", "maxConnectionType"), {invalid}).empty() &&
              serverSettings->m_maxConnectionType == 6 && line("sv.maxConnectionType") == "CTNumTypes",
              "unknown/numeric/empty connection token replaces the prior value with sentinel");
    }
    for (int invalid : {-1, 6, 99})
    {
        serverSettings->m_maxConnectionType = invalid;
        check(line("sv.maxConnectionType") == "CTNumTypes", "out-of-range stored connection prints sentinel");
    }
    files->input.text = "sv.maxConnectionType CTCable256Kbps\n";
    std::string scriptResult;
    check(io::mainConsole->run("settings/serversettings.con", "", "", "", "", "", "", "", "", scriptResult) &&
          serverSettings->m_maxConnectionType == 4,
          "serversettings script restores the named connection ceiling");
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
    settings->boolSet("GSIsServer", false);
    for (const char* name : {"teleportTo", "teleportCameraTo", "getNumberOfTickets"})
        check(!row("gameLogic", name)->isActive(), "live game logic without GSIsServer stays inactive");
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
    // Real Game receivers with observed squad/player/event collaborators, followed by
    // ServerGameLogic position/physics/camera checks. No server initialization or sockets.
    world::objectManager = new world::ObjectManager;
    gameEventRegistry = new GameEventRegistry;
    Server* squadGame = new Server;
    game = squadGame;
    Squads* squads = new Squads;
    squadManager = nullptr;
    for (const char* name : {"changeSquadName", "listSquadMembers", "listSquads"})
    {
        check(row("squadManager", name)->isActive(), "squad binding gate tests only game");
        check(!row("squadManager", name)->hasRange(), "squad arguments have no range clamp");
        check(mainConsoleObjects->getConsoleObject("game", name) == nullptr,
              "squad commands belong only to squadManager");
    }
    check(line("squadManager.listSquads 2").empty(), "absent squad manager returns an empty list");
    squadManager = squads;
    SquadInfo* publicSquad = new SquadInfo;
    publicSquad->id = 0; publicSquad->name = "Public Squad";
    SquadInfo* privateSquad = new SquadInfo;
    privateSquad->id = 7; privateSquad->name = "Private Squad";
    privateSquad->isPrivate = true;
    squads->entries[0] = publicSquad;
    squads->entries[7] = privateSquad;
    squads->entries[8] = publicSquad;
    io::mainConsole->setAccess(1);
    line("squadManager.listSquads 2");
    check(squads->reads.empty(), "squad commands reject access below level two");
    io::mainConsole->setAccess(2);
    check(line("squadManager.listSquads +2") ==
          "id:0 Public Squad is public.\nid:7 Private Squad is private.\n" &&
          squads->reads == std::vector<int>({0, 1, 2, 3, 4, 5, 6, 7}),
          "squad listing preserves text, gaps, order and the eight-ID limit");
    check(row("squadManager", "listSquads")->hasReturnValue(), "squad list publishes its string result");
    SquadPlayer* firstMember = new SquadPlayer;
    SquadPlayer* secondMember = new SquadPlayer;
    secondMember->id = 4; secondMember->name = "Other Player";
    players->members = {firstMember, secondMember};
    check(line("squadManager.listSquadMembers +2 7") == "id:31 Squad Player\nid:4 Other Player\n" &&
          players->memberTeam == 2 && players->memberSquad == 7,
          "member listing forwards integers and preserves player-manager order and text");
    players->members.clear();
    check(line("squadManager.listSquadMembers -2 -1").empty() &&
          players->memberTeam == -2 && players->memberSquad == -1,
          "empty member list and signed arguments are preserved");
    const int memberReads = players->memberReads;
    squads->reads.clear();
    invoke(row("squadManager", "listSquads"), {});
    invoke(row("squadManager", "listSquads"), {"2", "7"});
    invoke(row("squadManager", "listSquadMembers"), {"2"});
    invoke(row("squadManager", "listSquadMembers"), {"2", "7", "0"});
    invoke(row("squadManager", "changeSquadName"), {"Two words"});
    invoke(row("squadManager", "changeSquadName"), {"Two words", "0", "extra"});
    check(squads->reads.empty() && players->memberReads == memberReads && squads->renames == 0,
          "wrong argument counts do not dispatch squad receivers");
    EventManager* savedEvents = eventManager;
    SquadEvents* squadEvents = new SquadEvents;
    eventManager = squadEvents;
    firstMember->queries.clear();
    players->local = nullptr;
    line("squadManager.changeSquadName \"Two words\" 1");
    players->local = firstMember;
    squadManager = nullptr;
    line("squadManager.changeSquadName \"Two words\" 0");
    check(squads->renames == 0 && squadEvents->refusals == 0 && firstMember->queries.empty(),
          "rename requires both the local human and squad manager before reading player identity");
    squadManager = squads;
    firstMember->queries.clear();
    check(line("squadManager.changeSquadName \"Two words\" 1").empty() && squads->renames == 1 &&
          squadEvents->refusals == 0 && firstMember->queries == std::vector<int>({1, 2, 3}) &&
          !row("squadManager", "changeSquadName")->hasReturnValue(),
          "successful rename preserves player-query order and returns void");
    squads->accept = false;
    line("squadManager.changeSquadName \"Two words\" 0");
    check(squads->renames == 2 && squadEvents->refusals == 1,
          "failed rename notifies the HUD for either accepted Boolean value");
    check(squadGame->interfaceQueries == 0, "squad bindings dispatch directly through the global game");
    eventManager = savedEvents;
    squadManager = nullptr;
    players->local = nullptr;
    io::networkManager = new io::NetworkManager;
    Scores* scores = new Scores;
    world::scoreManager = static_cast<world::IScoreManager*>(scores);
    gameLogic = nullptr;
    ServerGameLogic* serverLogic = new ServerGameLogic;
    gameLogic = serverLogic;
    settings->boolSet("GSIsServer", true);
    for (const char* name : {"teleportTo", "teleportCameraTo", "getNumberOfTickets"})
        check(row("gameLogic", name)->isActive(), "server receiver enables command");
    scores->getTeamScore(1)->m_tickets = 173;
    scores->getTeamScore(2)->m_tickets = 291;
    check(line("gameLogic.getNumberOfTickets 1") == "173" &&
          line("gameLogic.getNumberOfTickets 2") == "291", "ticket query returns current team score");
    for (const char* invalid : {"0", "3", "4294967295"})
        check(invoke(row("gameLogic", "getNumberOfTickets"), {invalid}) == "0", "invalid team returns zero");
    const unsigned reads = scores->reads;
    invoke(row("gameLogic", "getNumberOfTickets"), {});
    invoke(row("gameLogic", "getNumberOfTickets"), {"1", "2"});
    check(scores->reads == reads, "ticket query rejects wrong arity before score lookup");
    teleportCalls.clear();
    line("gameLogic.teleportTo 1/2/3");
    line("gameLogic.teleportCameraTo 1/2/3");
    check(teleportCalls.empty(), "teleports do nothing without local human player");
    players->local = new PlayerObserver;
    line("gameLogic.teleportTo 1/2/3");
    line("gameLogic.teleportCameraTo 1/2/3");
    check(teleportCalls.empty(), "teleports guard missing vehicle and camera");
    TeleportObject* vehicle = new TeleportObject;
    TeleportObject* camera = new TeleportObject;
    players->local->vehicle = vehicle;
    players->local->camera = camera;
    line("gameLogic.teleportTo 10/20/30");
    check(teleportCalls == std::vector<int>({1}) && at(vehicle->position, 10,20,30),
          "teleport moves the local vehicle without requiring physics");
    world::physicsNodeManager = new world::PhysicsNodeManager;
    Node* node = new Node;
    vehicle->installOnMove = node;
    teleportCalls.clear();
    line("gameLogic.teleportTo 40/50/60");
    check(teleportCalls == std::vector<int>({1,2}) && at(vehicle->position, 40,50,60),
          "teleport reads cached physics after moving and resets it in order");
    teleportCalls.clear();
    line("gameLogic.teleportCameraTo 70/80/90");
    check(teleportCalls == std::vector<int>({3,1}) && at(camera->position, 70,80,90) &&
          at(vehicle->position, 40,50,60), "camera teleport queries the current camera and moves only it");
    teleportCalls.clear();
    invoke(row("gameLogic", "teleportTo"), {});
    invoke(row("gameLogic", "teleportCameraTo"), {"1/2/3", "4/5/6"});
    check(teleportCalls.empty(), "teleports reject wrong arity");

    ConsoleObject* send = row("pythonHost", "sendCommand");
    check(send->isActive(), "Python command requires the host pointer, not initialization");
    for (int i = 0; i < 9; ++i)
        check(std::strcmp(send->getArgType(i), "std::string") == 0,
              "Python command exposes nine raw string arguments");
    Py_NoSiteFlag = 1;
    Py_Initialize();
    check(!python::pythonHost()->isInitialized() && line("pythonHost.sendCommand beforeInit") == "1",
          "uninitialized host still returns true after attempting dispatch");
    // In-memory bf2 supplies only initialization prerequisites; all host registration,
    // tuple construction and callback dispatch below use the production implementation.
    PyObject* module = PyImport_AddModule(const_cast<char*>("bf2"));
    PyObject* globals = PyModule_GetDict(module);
    PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());
    PyObject* setup = PyRun_String(
        "def playerConvFunc(id):\n return id\ndef init_module():\n pass\n",
        Py_file_input, globals, globals);
    check(setup != nullptr, "in-memory bf2 initialization prerequisites");
    Py_DECREF(setup);
    check(python::pythonHost()->initialize(false), "production PythonHost initializes");
    gameLogic = logic;
    logic->roundState(0, 0);
    check(line("pythonHost.sendCommand noHandlers") == "1", "dispatch without handlers returns true");
    setup = PyRun_String(
        "import host\nseen = []\ngated = []\n"
        "def receive(command, args):\n seen.append((command, args))\n"
        "def receivePlaying(command, args):\n gated.append((command, args))\n"
        "def fail(command, args):\n raise RuntimeError('fixture callback')\n"
        "host.registerHandler('ConsoleSendCommand', fail, 1)\n"
        "host.registerHandler('ConsoleSendCommand', receive, 1)\n"
        "host.registerHandler('ConsoleSendCommand', receivePlaying)\n",
        Py_file_input, globals, globals);
    check(setup != nullptr, "production host registers ConsoleSendCommand handlers");
    Py_DECREF(setup);
    PyObject* seen = PyDict_GetItemString(globals, "seen");
    for (int count = 1; count <= 9; ++count)
    {
        std::vector<std::string> args;
        for (int i = 0; i < count; ++i)
            args.push_back("argument " + std::to_string(i));
        check(send->execute(args.data(), count) == "1", "every accepted Python command arity returns true");
        PyObject* record = PyList_GetItem(seen, count - 1);
        check(record && PyTuple_Size(record) == 2 &&
              std::string(PyString_AsString(PyTuple_GetItem(record, 0))) == args[0],
              "ConsoleSendCommand carries command first");
        PyObject* values = PyTuple_GetItem(record, 1);
        check(PyTuple_Check(values) && PyTuple_Size(values) == count - 1,
              "omitted arguments are padded empty and trimmed from the tuple");
        for (int i = 1; i < count; ++i)
            check(std::string(PyString_AsString(PyTuple_GetItem(values, i - 1))) == args[i],
                  "all optional strings preserve order and spaces");
        check(!PyErr_Occurred(), "a failing handler is cleared and later handlers still run");
    }
    check(PyList_Size(seen) == 9, "one callback per accepted arity");
    check(line("pythonHost.sendCommand \"quoted command\" \"\" \"two words\" \"\" \"\"") == "1",
          "real console preserves quoted strings and explicit empty arguments");
    PyObject* record = PyList_GetItem(seen, 9);
    PyObject* values = PyTuple_GetItem(record, 1);
    check(std::string(PyString_AsString(PyTuple_GetItem(record, 0))) == "quoted command" &&
          PyTuple_Size(values) == 2 &&
          std::string(PyString_AsString(PyTuple_GetItem(values, 0))).empty() &&
          std::string(PyString_AsString(PyTuple_GetItem(values, 1))) == "two words",
          "tuple trims only trailing empties, preserving interior empties");
    check(values->ob_refcnt == 1 && PyTuple_GetItem(values, 1)->ob_refcnt == 1,
          "outer tuple transfers nested tuple and string ownership without extra references");
    check(line("pythonHost.sendCommand alone") == "1" &&
          PyTuple_Size(PyTuple_GetItem(PyList_GetItem(seen, 10), 1)) == 0,
          "short calls do not reuse optional arguments from earlier calls");
    check(invoke(send, {"", "", ""}) == "1" &&
          std::string(PyString_AsString(PyTuple_GetItem(PyList_GetItem(seen, 11), 0))).empty() &&
          PyTuple_Size(PyTuple_GetItem(PyList_GetItem(seen, 11), 1)) == 0,
          "empty command is still dispatched with an empty optional tuple");
    invoke(send, {});
    invoke(send, {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"});
    check(PyList_Size(seen) == 12, "Python command rejects zero and ten arguments");
    PyObject* gated = PyDict_GetItemString(globals, "gated");
    gameLogic = logic;
    logic->roundState(0, 3);
    check(line("pythonHost.sendCommand outsideRound") == "1" && PyList_Size(gated) == 0,
          "ordinary handlers stay gated outside playing status");
    logic->roundState(GameStatus_Playing, 0);
    check(line("pythonHost.sendCommand beforeBeginRound") == "1" && PyList_Size(gated) == 0,
          "ordinary handlers stay gated before running begin-round state");
    logic->roundState(GameStatus_Playing, 3);
    check(line("pythonHost.sendCommand playing payload") == "1" && PyList_Size(gated) == 1,
          "ordinary handlers receive console commands in a running round");
    mainConsoleObjects->unregisterConsoleObjects(moduleGameConsoleObjects());
    check(mainConsoleObjects->getConsoleObject("pythonHost", "sendCommand") == nullptr,
          "ModuleGame command list unregisters");
    for (const char* name : {"changeSquadName", "listSquadMembers", "listSquads"})
        check(!mainConsoleObjects->getConsoleObject("squadManager", name), "ModuleGame squad commands unregister");
    std::printf("PASS console commands: %u checks (DEDICATED=%d)\n", checks, DEDICATED);
    std::fflush(stdout);
    std::_Exit(0); // No full-engine teardown for this bounded, pre-init fixture.
}
