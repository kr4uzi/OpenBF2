// [TEMP] [CHOSEN, not recovered] Typed request-path observers and isolated object lifetimes.
// Production request routing, AI dispatch, couriers and policy bodies are never overridden.
#include <map>
#include <set>
#include <list>
#include <vector>
#include <string>
#include <utility>
#include <cmath>
#define private public
#include "request_observers.h"
#include "../BF2/Game/Objects/Game/ArtilleryHandler.h"
#undef private
#include "../BF2/AI/AIInterface/Main/AIEventListener.h"
#include "../BF2/AI/AIInterface/SingletonServer.h"
#include "../BF2/Game/GameEvents/UAVEvent.h"
#include "../BF2/Game/GameEvents/ArtilleryEvent.h"
#include "../BF2/Game/GameEvents/SupplyDropEvent.h"
#include "../BF2/Game/GameEvents/DropVehicleEvent.h"
#include "../BF2/Game/Objects/Game/SupplyDropManager.h"
#include "../BF2/Game/Main/Game.h"

using EngineSquadManager = dice::hfe::SquadManager;
using AISquadManager = dice::hfe::ai::SquadManager;
namespace {
int checks, broadcasts, aiEvents, hudLeader, hudCommander, assetCalls, warnings;
int remoteLeader, remoteCommander, assetType, assetTeam, assetSquad;
int authorized[4]{};
bool guns = true;
bool dedicatedMode = false;
int humanCount = 1;
std::string mapMode = "sp1";
struct Grant { int type; int team; Vec2 position; };
std::vector<Grant> pendingGrants;
Vec2 assetPosition;
double gameTime = 100;
std::string trace;
Squad* activeSquad;
SAI* policy;
AIEventListener* listener;
}
[[noreturn]] void unexpected(const char* name)
{
    std::fprintf(stderr, "FAIL %s\n", name);
    std::abort();
}
static void check(bool condition, const char* name)
{
    if (!condition) unexpected(name);
    ++checks;
}
void broadcast(GameEvent* event)
{
    check(event->getType() == 24, "request broadcast uses event 24");
    trace += 'B';
    ++broadcasts;
    event->release();
}
void remote(int id, GameEvent* event)
{
    auto* notification = dynamic_cast<PostRemoteEvent*>(event);
    check(notification && notification->m_eventCategory == ECHud, "remote HUD notification");
    if (notification->m_eventId == 30) {
        check(notification->m_payloadBitSize == 4, "leader notification width");
        remoteLeader = id;
        trace += 'L';
    } else if (notification->m_eventId == 29) {
        check(notification->m_payloadBitSize == 8, "commander notification width");
        int payload[2];
        std::memcpy(payload, notification->m_payload, sizeof(payload));
        check(payload[0] == 3 && payload[1] >= 0 && payload[1] < 4, "commander squad and type");
        remoteCommander = id;
        trace += 'C';
    } else unexpected("unexpected remote notification");
    event->release();
}
SAI* activeSAI(int side) { check(side == 2, "courier team"); return policy; }
double now() { return gameTime; }
bool artilleryAvailable() { return guns; }
const std::string& currentGameMode() { return mapMode; }
void useAsset(int type, int team, int squad, Vec2 position)
{
    ++assetCalls;
    assetType = type;
    assetTeam = team;
    assetSquad = squad;
    assetPosition = position;
    trace += 'A';
    pendingGrants.push_back(Grant{type, team, position});
}

// The engine commander queues these events for a later update with local sender -1.
static void dispatchGrants()
{
    for (const auto& grant : pendingGrants) {
        switch (grant.type) {
        case 0: {
            ArtilleryEvent event;
            event.m_team = grant.team;
            event.m_artilleryType = ArtilleryEvent::ArtilleryTypeRequestFire;
            event.m_target = grant.position;
            event.executeServer(-1);
            break;
        }
        case 1: {
            UAVEvent event(grant.position.x, grant.position.y, grant.team, false);
            event.executeServer(-1);
            break;
        }
        case 2: {
            DropVehicleEvent event(grant.position.x, grant.position.y, grant.team, false);
            event.executeServer(-1);
            break;
        }
        case 3: {
            SupplyDropEvent event(grant.position.x, grant.position.y, grant.team);
            event.executeServer(-1);
            break;
        }
        default: unexpected("queued asset type");
        }
    }
    pendingGrants.clear();
}

namespace dice { namespace hfe {
#if !DEDICATED
bool isDedicated() { return dedicatedMode; }
#endif
IGame* game = nullptr;
EventManager* eventManager = nullptr;
SettingsRepostitory* settings = nullptr;
IBase* squadManager = nullptr;
// Narrow fixture lifetimes omit world/spawn construction. Only fields used below are initialized.
Squad::Squad(Team*) : m_leaderPlayerId(7) {}
Squad::~Squad() {}
EngineSquadManager::SquadManager() {}
EngineSquadManager::~SquadManager() {}
Squad* EngineSquadManager::getSquad(int team, int squad)
{ return team == 2 && squad == 3 ? activeSquad : nullptr; }
EventManager::EventManager() {}
EventManager::~EventManager() {}
namespace world {
PlayerManager* playerManager = nullptr;
PlayerManager::PlayerManager() {}
PlayerManager::~PlayerManager() {}
ArtilleryManager* artilleryManager = nullptr;
SupplyDropManager* supplyDropManager = nullptr;
ArtilleryManager::ArtilleryManager() : m_handlers{} {}
ArtilleryManager::~ArtilleryManager() {}
ArtilleryHandler::ArtilleryHandler(int) {}
ArtilleryHandler::~ArtilleryHandler() {}
bool ArtilleryHandler::setTarget(Vec2& target)
{
    check(target.x == 123 && target.y == 678, "artillery target");
    return true;
}
void ArtilleryHandler::requestFire() { ++authorized[0]; }
void ArtilleryHandler::failedFire() { unexpected("artillery failedFire"); }
SupplyDropManager::SupplyDropManager() {}
SupplyDropManager::~SupplyDropManager() {}
bool SupplyDropManager::requestSupplyDrop(unsigned char team, Vec2& position)
{
    check(team == 2 && position.x == 123 && position.y == 678, "supply target/team");
    ++authorized[3];
    return true;
}
}
namespace ai {
IBase* g_aiInstance = nullptr;
IAIMain* IAIMain::m_instance = nullptr;
IAIEventCourier* BFSingletonServer::m_iaiEventCourier = nullptr;
void* CommonSingletonServer::m_iBFTimer = nullptr;
void* CommonSingletonServer::m_iBFCommander = nullptr;
IAIMain::IAIMain() {}
IAIMain::~IAIMain() {}
IAIEventCourier::IAIEventCourier() {}
IAIEventCourier::~IAIEventCourier() {}
AIEventCourier::AIEventCourier() {}
AIEventCourier::~AIEventCourier() {}
IBFTimer::IBFTimer() {}
IBFTimer::~IBFTimer() {}
IBFCommander::IBFCommander() {}
IBFCommander::~IBFCommander() {}
ISAI::~ISAI() {}
SAI::SAI(int side, int, int, Vec2, Vec2)
    : m_time68(0), m_time70(0), m_time78(0), m_time80(0), m_side(side), m_flag1D1(false) {}
SAI::~SAI()
{
    for (auto* denial : m_denyRequests) delete denial;
}
AISquadManager* AISquadManager::m_instance = nullptr;
AISquadManager::SquadManager() {}
AISquadManager::~SquadManager() {}
void AISquadManager::incomingArtillery(Vec3, int side)
{ check(side == 2, "artillery warning team"); ++warnings; }
void AISquadManager::unAssignSquads(int side)
{ check(side == 2, "commander resignation unassigns the team's squads"); }
} } }

struct ObservedAI : IAIInstance {
    bool initialized = true;
    bool init() override { unexpected("AI init"); }
    bool clear() override { unexpected("AI clear"); }
    bool shutdown() override { unexpected("AI shutdown"); }
    int getSettingsFromMenu(bool) override { unexpected("AI menu"); }
    int update(double) override { unexpected("AI update"); }
    bool isInitialized() const override { return initialized; }
    bool isShuttingDown() const override { unexpected("AI shutdown query"); }
};
struct Manager : EngineSquadManager {
    int commander = -1;
    int getCommander(int team) override { check(team == 2, "commander lookup team"); return commander; }
};
struct Bus : EventManager {
    unsigned int postEvent(EventCategory category, unsigned int id, void* data, float delay) override
    {
        check(delay == 0, "immediate notification");
        if (category == ECAI) {
            check(id == 15, "asset requests use AI event 15, not new-orders event 16");
            struct Payload { SquadOrder order; int team; int squad; Vec3 position; };
            static_assert(sizeof(Payload) == 0x44, "request payload width");
            const auto& payload = *static_cast<const Payload*>(data);
            check(payload.team == 2 && payload.squad == 3 && payload.order.targetNetId == 19,
                  "AI payload team, squad and target");
            check(payload.order.flag2C && payload.order.points[2].z == 4
                  && payload.position.x == 123 && payload.position.y == 45 && payload.position.z == 678,
                  "AI payload whole order and first point");
            ++aiEvents;
            trace += 'I';
            listener->handleEvent(category, id, nullptr, data);
        } else if (category == ECHud && id == 30) {
            ++hudLeader;
            trace += 'L';
        } else if (category == ECHud && id == 29) {
            ++hudCommander;
            trace += 'C';
        } else if (category == ECGameLogic && id == 17) {
            ++authorized[1];
            trace += 'U';
        } else if (category == ECGameLogic && id == 21) {
            ++authorized[2];
        } else unexpected("unexpected local notification");
        return 0;
    }
    bool unregisterEvent(EventCategory, unsigned int) override { return true; }
    bool unregisterEventHandler(EventCategory, IEventListener*) override { return true; }
};
struct Policy : SAI { Policy() : SAI(2, 0, 0, Vec2{}, Vec2{}) {} };
struct Courier : AIEventCourier {};
struct ObservedPlayers : world::PlayerManager {
    int getNumberOfHumanPlayers() const override { return humanCount; }
};

int main()
{
    Manager manager;
    Squad squad(nullptr);
    Bus bus;
    ObservedGame transport;
    ObservedAI ai;
    ObservedMain main;
    ObservedTimer timer;
    ObservedCommander assets;
    Policy strategy;
    Courier courier;
    AIEventListener events;
    AIRandom random;
    AISquadManager aiSquads;
    SettingsRepostitory repository;
    world::ArtilleryManager artillery;
    world::ArtilleryHandler battery(2);
    world::SupplyDropManager supplies;
    ObservedPlayers players;
    world::playerManager = &players;
    artillery.m_handlers[1] = &battery;
    world::artilleryManager = &artillery;
    world::supplyDropManager = &supplies;
    activeSquad = &squad;
    policy = &strategy;
    listener = &events;
    game = &transport;
    eventManager = &bus;
    settings = &repository;
    squadManager = &manager;
    g_aiInstance = reinterpret_cast<IBase*>(&ai);
    IAIMain::m_instance = &main;
    BFSingletonServer::m_iaiEventCourier = &courier;
    CommonSingletonServer::m_iBFTimer = &timer;
    CommonSingletonServer::m_iBFCommander = &assets;
    AISquadManager::m_instance = &aiSquads;
    SquadOrder request{};
    request.points[0] = Vec3{123, 45, 678};
    request.points[1] = Vec3{9, 8, 7};
    request.points[2] = Vec3{6, 5, 4};
    request.targetNetId = 19;
    request.flag2C = true;
    for (int type = 0; type < 4; ++type) {
        request.orderId = type;
        trace.clear();
        manager.issueRequest(2, 3, request, false);
        check(trace == "BIAL", "broadcast, AI policy, local leader order");
        check(assetType == type && assetTeam == 2 && assetSquad == 3, "asset type/team/squad");
        check(assetPosition.x == 123 && assetPosition.y == 678, "request first point x/z");
        check(squad.m_requests[type].targetNetId == 19 && squad.m_requests[type].points[2].z == 4,
              "whole request preserved");
    }
    check(aiEvents == 4 && assetCalls == 4 && warnings == 1 && remoteCommander == 0,
          "no player commander still reaches strategic AI");
    check(hudLeader == 4 && remoteLeader == 0, "active AI in conquest uses local HUD routing");
    check(strategy.m_time80 == 130 && strategy.m_time68 == 160 && strategy.m_time70 == 160
          && strategy.m_time78 == 160, "retail asset timers");
    // Seed without a wall-clock read. All cooldown denials need one inline random draw.
    random.m_seed = 42;
    random.init();
    for (int type = 0; type < 4; ++type) {
        request.orderId = type;
        manager.issueRequest(2, 3, request, false);
    }
    check(assetCalls == 4 && strategy.m_denyRequests.size() == 4, "cooldown queues refusals");
    for (auto* denial : strategy.m_denyRequests)
        check(denial->m_playerId == 3 && denial->m_deadline >= 102 && denial->m_deadline <= 105,
              "refusal carries squad and retail delay");
    gameTime = 200;
    guns = false;
    request.orderId = 0;
    manager.issueRequest(2, 3, request, false);
    check(assetCalls == 4 && strategy.m_denyRequests.back()->m_type == 2,
          "no surviving guns queues artillery unavailable");
    guns = true;
    manager.commander = 11;
    strategy.setPlayerIsCommander(true);
    trace.clear();
    manager.issueRequest(2, 3, request, false);
    check(trace == "BILC" && remoteCommander == 11 && assetCalls == 4,
          "human commander receives request and disables automatic action");
    repository.stringSet("GSGameMode", "GPM_COOP");
    const int beforeRemoteCoop = hudLeader;
    trace.clear();
    manager.issueRequest(2, 3, request, false);
    check(trace == "BILC" && remoteLeader == 7 && hudLeader == beforeRemoteCoop,
          "coop leader notification is remote");
    squad.m_leaderPlayerId = 255;
    const int coopLocalBefore = hudLeader;
    trace.clear();
    manager.issueRequest(2, 3, request, false);
    check(trace == "BILC" && hudLeader == coopLocalBefore + 1,
          "coop local leader notification is local");
    squad.m_leaderPlayerId = 7;
    ai.initialized = false;
    trace.clear();
    manager.issueRequest(2, 3, request, false);
    check(trace == "BLC", "inactive AI does not receive request");
    g_aiInstance = nullptr;
    manager.commander = -1;
    trace.clear();
    manager.issueRequest(2, 3, request, false);
    check(trace == "BL", "no AI/no commander preserves HUD-only behavior");
    squad.m_leaderPlayerId = 255;
    const int localBefore = hudLeader;
    manager.issueRequest(2, 3, request, false);
    check(hudLeader == localBefore + 1, "no AI local leader notification");
    trace.clear();
    manager.issueRequest(2, 3, request, true);
    manager.issueRequest(2, 0, request, false);
    manager.issueRequest(2, 99, request, false);
    check(trace.empty(), "suppressed/zero/missing squad sends nothing");
    squad.m_leaderPlayerId = -1;
    manager.issueRequest(2, 3, request, false);
    check(trace == "B", "leaderless squad has no AI request");
    // Only local AI dispatch gains authority while the commander seat is empty.
    UAVEvent uav(123, 678, 2, false);
    trace.clear();
    uav.executeServer(-1);
    check(trace == "U", "single-player mode accepts the local AI UAV grant in either image");
    manager.commander = 11;
    trace.clear();
    uav.executeServer(11);
    check(trace == "U", "assigned commander can launch UAV");
    ArtilleryEvent fire;
    fire.m_team = 2;
    fire.m_artilleryType = ArtilleryEvent::ArtilleryTypeRequestFire;
    fire.m_target = Vec2{123, 678};
    SupplyDropEvent supply(123, 678, 2);
    DropVehicleEvent vehicle(123, 678, 2, false);
    UAVEvent scan(123, 678, 2, true);
    GameEvent* requests[] = {&fire, &uav, &vehicle, &supply};
    struct Session { const char* mode; int humans; bool dedicated; bool singlePlayer; };
    const Session sessions[] = {
        {"sp1", 1, false, true}, {"sp2", 1, false, true}, {"sp3", 1, false, true},
        {"SP1", 1, false, true}, {"Sp2", 1, false, true}, {"sP3", 1, false, true},
        {"gpm_coop", 1, false, false}, {"GPM_COOP", 2, false, false},
        {"gpm_cq", 1, false, false}, {"sp1", 2, false, true}, {"sp1", 0, false, true},
        {"sp1", 1, true, true}, {"sp2", 1, true, true}, {"sp3", 1, true, true},
        {"sp1", 2, true, true}, {"gpm_cq", 2, true, false},
        {"gpm_coop", 1, true, false}, {"sp10", 1, false, false}, {"", 1, false, false},
    };
    for (const auto& session : sessions) {
        mapMode = session.mode;
        humanCount = session.humans;
        dedicatedMode = session.dedicated;
        for (int commander : {-1, 11, 255}) {
            manager.commander = commander;
            for (int sender : {-257, -1, 0, 7, 11, 255, 267, 511}) {
                for (int type = 0; type < 4; ++type) {
                    const int before = authorized[type];
                    requests[type]->executeServer(sender);
                    const bool expected = commander == static_cast<unsigned char>(sender)
                        || (session.singlePlayer && commander == -1 && sender == -1
                            && type != 2);
                    check(authorized[type] - before == (expected ? 1 : 0),
                          "retail authority except local single-player grants for three assets");
                }
            }
        }
    }
    mapMode = "sp1";
    humanCount = 1;
    dedicatedMode = false;
    // A scan is not an AI asset request and remains refused without a commander.
    manager.commander = -1;
    trace.clear();
    scan.executeServer(-1);
    check(trace.empty(), "no automatic scan without a commander");
    // Missing session data cannot turn a local AI sender into a commander.
    const int beforeMissingGame = authorized[0] + authorized[1] + authorized[3];
    game = nullptr;
    uav.executeServer(-1);
    fire.executeServer(-1);
    supply.executeServer(-1);
    check(trace.empty() && authorized[0] + authorized[1] + authorized[3] == beforeMissingGame,
          "missing game refuses the local AI grant");
    game = &transport;
    world::playerManager = nullptr;
    const int beforeMissingPlayers[3] = {authorized[0], authorized[1], authorized[3]};
    uav.executeServer(-1);
    fire.executeServer(-1);
    supply.executeServer(-1);
    check(authorized[0] == beforeMissingPlayers[0] + 1 &&
          authorized[1] == beforeMissingPlayers[1] + 1 &&
          authorized[3] == beforeMissingPlayers[2] + 1,
          "single-player grants do not depend on the player manager");
    world::playerManager = &players;
    strategy.m_flag1D1 = false;
    strategy.m_time68 = strategy.m_time70 = strategy.m_time78 = strategy.m_time80 = 500;
    gameTime = 500;
    const int beforeDeadline = assetCalls;
    g_aiInstance = reinterpret_cast<IBase*>(&ai);
    ai.initialized = true;
    squad.m_leaderPlayerId = 7;
    for (int type = 0; type < 4; ++type) {
        request.orderId = type;
        manager.issueRequest(2, 3, request, false);
    }
    check(assetCalls == beforeDeadline, "all cooldowns refuse at their exact deadline");
    gameTime = 500.01;
    for (int type = 0; type < 4; ++type) {
        request.orderId = type;
        manager.issueRequest(2, 3, request, false);
    }
    check(assetCalls == beforeDeadline + 4, "all cooldowns grant after their deadline");
    check(strategy.m_time80 == gameTime + 30 && strategy.m_time68 == gameTime + 60
          && strategy.m_time70 == gameTime + 60 && strategy.m_time78 == gameTime + 60,
          "independent cooldowns rearm from the grant time");

    // Carry the same ready AI request through the asset events in each kind of session.
    // Initial/resumed waits are 10 seconds for UAV and 60 seconds for other assets.
    for (const auto& session : sessions) {
        mapMode = session.mode;
        humanCount = session.humans;
        dedicatedMode = session.dedicated;
        manager.commander = -1;
        for (int type = 0; type < 4; ++type) {
            pendingGrants.clear();
            gameTime = 1000;
            strategy.setPlayerIsCommander(true);
            strategy.setPlayerIsCommander(false);
            check(strategy.m_time80 == 1010 && strategy.m_time68 == 1060 &&
                  strategy.m_time70 == 1060 && strategy.m_time78 == 1060,
                  "commander resignation restarts the initial waits");
            request.orderId = type;
            gameTime = type == 1 ? 1010 : 1060;
            const int callsBefore = assetCalls;
            const int authorityBefore = authorized[type];
            manager.issueRequest(2, 3, request, false);
            dispatchGrants();
            check(assetCalls == callsBefore && authorized[type] == authorityBefore,
                  "initial wait refuses at its exact deadline");
            gameTime += 0.01;
            manager.issueRequest(2, 3, request, false);
            check(assetCalls == callsBefore + 1 && authorized[type] == authorityBefore,
                  "ready AI grant queues before asset dispatch");
            dispatchGrants();
            const bool delivered = session.singlePlayer && type != 2;
            check(authorized[type] == authorityBefore + (delivered ? 1 : 0),
                  "ready grants reach three asset managers only in single player");
            manager.issueRequest(2, 3, request, false);
            dispatchGrants();
            check(assetCalls == callsBefore + 1,
                  "accepted and refused grants both keep retail cooldown rearming");
        }
        manager.commander = 11;
        strategy.setPlayerIsCommander(true);
        gameTime = 2000;
        const int callsBefore = assetCalls;
        for (int type = 0; type < 4; ++type) {
            request.orderId = type;
            manager.issueRequest(2, 3, request, false);
        }
        check(assetCalls == callsBefore && pendingGrants.empty(),
              "human commander disables every automatic asset in every session");
    }
    std::printf("PASS %d checks: requests, waits, single-player grants on any host, co-op/conquest refusal, human commander\n",
                checks);
}
