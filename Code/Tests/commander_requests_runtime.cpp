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

using EngineSquadManager = dice::hfe::SquadManager;
using AISquadManager = dice::hfe::ai::SquadManager;
namespace {
int checks, broadcasts, aiEvents, hudLeader, hudCommander, assetCalls, warnings;
int remoteLeader, remoteCommander, assetType, assetTeam, assetSquad;
int authorized[4]{};
bool guns = true;
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
void useAsset(int type, int team, int squad, Vec2 position)
{
    ++assetCalls;
    assetType = type;
    assetTeam = team;
    assetSquad = squad;
    assetPosition = position;
    trace += 'A';
}

namespace dice { namespace hfe {
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
    strategy.m_flag1D1 = true;
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
    // The shipped local dispatch passes -1; the asset event compares its low byte
    // against the full commander ID. Do not silently turn that into an authorization bypass.
    UAVEvent uav(123, 678, 2, false);
    trace.clear();
    uav.executeServer(-1);
    check(trace.empty(), "empty commander (-1) differs from local sender byte 255");
    manager.commander = 11;
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
    for (int commander : {-1, 11, 255}) {
        manager.commander = commander;
        for (int sender : {-257, -1, 0, 7, 11, 255, 267, 511}) {
            for (int type = 0; type < 4; ++type) {
                const int before = authorized[type];
                requests[type]->executeServer(sender);
                const bool expected = commander == static_cast<unsigned char>(sender);
                check(authorized[type] - before == (expected ? 1 : 0),
                      "retail authorization compares full commander with sender byte");
            }
        }
    }
    // A scan is not an AI asset request and remains refused without a commander.
    manager.commander = -1;
    trace.clear();
    scan.executeServer(-1);
    check(trace.empty(), "no automatic scan without a commander");
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
    std::printf("PASS %d checks: requests, AI policy, human commander, coop and asset authority\n", checks);
}
