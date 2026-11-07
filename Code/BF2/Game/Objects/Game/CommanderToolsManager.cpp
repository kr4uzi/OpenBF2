// Commander-tool state and event handling.

#include "CommanderToolsManager.h"

#include "DropVehicle.h"
#include "../../../Core/EventManager.h"
#include "../../../Core/Ids.h"
#include "../../../Core/System.h"
#include "../../../IO/Network/INetworkable.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectPredicator.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../Common/GameEvent.h"
#include "../../IGame.h"
#include "../../IGameLogic.h"
#include "../GameLogicComp/IArmorGLComp.h"
#include "../IObject.h"
#include "../IObjectTemplate.h"
#include "../ObjectLayoutAccess.h"
#include "../IPlayer.h"
#include "../RemoteControlledObjectTemplate.h"
#include "RemoteControlledObject.h"
#include "../../../Scene/Object/PlayerManager.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <map>
#include <string>
#include <vector>
#include "../../../Platform/MsvcRand.h"  // [CHOSEN, not recovered] the client CRT's rand (MSVCR71)

namespace dice {
namespace hfe {
namespace world {

extern IBase* heightmapCluster;                  // ds:0x8ABAF24, Core/Globals.cpp

namespace {

struct DropVehicleHudPayload
{
    int objectId;
    float remainingFraction;
    Vec3 position;
};

} // anonymous namespace

CommanderToolState::CommanderToolState()
{
    // The constructor alone clears the centre; reset deliberately leaves it untouched.
    m_uavPosition.x = 0.0f;
    m_uavPosition.y = 0.0f;
    m_uavPosition.z = 0.0f;
    reset();
}

CommanderToolState::~CommanderToolState()
{
}

void CommanderToolState::reset()
{
    m_uavActiveTime = 0.0f;
    m_scanActiveTime = 0.0f;
    m_uavReloadTime = 0.0f;
    m_scanReloadTime = 0.0f;
    m_artilleryArmor = 0.0f;
    m_uavArmor = 0.0f;
    m_satelliteArmor = 0.0f;
    m_artilleryCount = 0;
    m_uavCount = 0;
    m_satelliteCount = 0;
    m_dropVehicleCount = 0;
}

void CommanderToolState::resetToysStats()
{
    m_artilleryArmor = 0.0f;
    m_uavArmor = 0.0f;
    m_satelliteArmor = 0.0f;
    m_artilleryCount = 0;
    m_uavCount = 0;
    m_satelliteCount = 0;
}

void CommanderToolState::update(float deltaTime)
{
    // Retail's order is active UAV, active scan, UAV reload, scan reload. Timers are not clamped:
    // the last subtraction can leave a small negative value, and the readiness predicates use it.
    if (m_uavActiveTime > 0.0f)
        m_uavActiveTime -= deltaTime;
    if (m_scanActiveTime > 0.0f)
        m_scanActiveTime -= deltaTime;
    if (m_uavReloadTime > 0.0f)
        m_uavReloadTime -= deltaTime;
    if (m_scanReloadTime > 0.0f)
        m_scanReloadTime -= deltaTime;
}

bool CommanderToolState::launchUAV(const Vec3& position)
{
    if (!getUAVIsReady())
        return false;

    // Both active-time calls are present in the ELF. Do not fold the second into the first: a
    // virtual implementation with side effects would make that observable.
    m_uavActiveTime = gameLogic->getUAVActiveTime();
    const float activeTime = gameLogic->getUAVActiveTime();
    m_uavReloadTime = gameLogic->getUAVReloadTime() + activeTime;
    m_uavPosition = position;
    return true;
}

bool CommanderToolState::startScan()
{
    if (!getScanIsReady())
        return false;

    m_scanActiveTime = gameLogic->getSatelliteScanActiveTime();
    const float activeTime = gameLogic->getSatelliteScanActiveTime();
    m_scanReloadTime = gameLogic->getSatelliteScanReloadTime() + activeTime;
    return true;
}

// . The unusual rotation of (distance,distance), rather than (distance,0), and the
// half-open modulo expression are both shipped behavior.
Vec3 CommanderToolState::CalculateRandomDropVehicleDeviation(int maximum, int minimum)
{
    const int distance = minimum + bf2crt::rand() % (maximum - minimum);
    const double angle = static_cast<double>(bf2crt::rand() % 360) / 360.0 * 6.28;
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);

    Vec3 result;
    result.x = static_cast<float>(distance * cosine - distance * sine);
    result.y = 0.0f;
    result.z = static_cast<float>(distance * sine + distance * cosine);
    return result;
}

// . The vertical line includes collision meshes and ignores Soldier
// templates at the ObjectManager layer. A hit only blocks the grid when its root is a PCO, a
// player object, or carries the cached strategic-object component.
bool CommanderToolState::CheckForCollisionWithStrategicObjects(const Vec3& position)
{
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart = 0;
    ObjectFlagPredicator predicator(0x100u);
    const Vec3 from = { position.x, 2000.0f, position.z };
    const Vec3 direction = { 0.0f, -4500.0f, 0.0f };

    if (!objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                      from, direction, &predicator,
                                      true, false, false, false, CID_SoldierTemplate))
        return false;
    if (!hitObject)
        return false;

    IObject* const root = getRootParent(hitObject);
    if (root->queryInterface(IID_IPlayerControlObject))
        return true;
    if (root->queryInterface(IID_IPlayer))
        return true;
    return object_detail::LayoutAccess::strategicObjectComponent(root) != 0;
}

// . The two networkable fetches deliberately have different null
// handling: the notification payload tolerates no networkable, while the final return does not.
int CommanderToolState::dropVehicle(const Vec3& position, int team)
{
    Vec3 landingPosition = position;
    findValidLandingPositionForDropVehicle(landingPosition);

    IObjectTemplate* const vehicleTemplate =
        objectTemplateManager->getTemplate(gameLogic->getTeamDropVehicle(team));
    const Vec3 zero = { 0.0f, 0.0f, 0.0f };
    IObject* const vehicle = game->createObjectOnAllClients(
        vehicleTemplate, landingPosition, zero, true, 0, 0);
    if (!vehicle)
        return -1;

    int networkableId = -1;
    io::INetworkable* const networkable = vehicle->getNetworkable();
    if (networkable)
        networkableId = networkable->m_networkableId;
    PostRemoteEvent* const event =
        new PostRemoteEvent(ECGameLogic, 22u, false,
                            &networkableId, 4u, 0.0f);
    game->sendGameEventToAll(event);

    IObjectTemplate* const controllerTemplate =
        objectTemplateManager->getTemplate(std::string("dropVehicle"));
    DropVehicle* const controller =
        static_cast<DropVehicle*>(controllerTemplate->createObject());
    controller->init();
    controller->setVehicle(vehicle);
    controller->m_team = team;
    ++m_dropVehicleCount;
    return vehicle->getNetworkable()->m_networkableId;
}

// . Retail leaks override LOD 1 on the invalid-name return; restoring
// it there would be a behavior change. The list getter and template/name reads stay in their
// original per-object/per-entry nesting.
bool CommanderToolState::IsVehiclePermittedtoLand(Vec3& position)
{
    ObjectFlagPredicator predicator(0x100u);
    const Vec3 from = { position.x, 2000.0f, position.z };
    const Vec3 direction = { 0.0f, -4500.0f, 0.0f };
    std::vector<IObject*> objects;

    objectManager->setOverrideLineTestLod(1);
    objectManager->getObjectsFromGrid(from, direction, objects, &predicator,
                                      true, 0, false);

    for (std::vector<IObject*>::const_iterator objectIt = objects.begin();
         objectIt != objects.end(); ++objectIt)
    {
        IObject* const root = getRootParent(*objectIt);
        if (!root)
            continue;

        std::list<std::pair<std::string, bool> >* const invalidObjects =
            gameLogic->GetInvalidVehicleDropObjectList();
        for (std::list<std::pair<std::string, bool> >::const_iterator invalidIt =
                 invalidObjects->begin();
             invalidIt != invalidObjects->end(); ++invalidIt)
        {
            const IObjectTemplate* const objectTemplate = root->getTemplate();
            if (objectTemplate
                && invalidIt->first == objectTemplate->getName()
                && invalidIt->second == true)
            {
                return false; // shipped: override line-test LOD remains 1
            }
        }
    }

    objectManager->setOverrideLineTestLod(-1);
    return true;
}

// . The float output is never written in either shipped build.
bool CommanderToolState::CheckVehicleDropGrid(const Vec3& position, float&)
{
    for (int x = 0; x < 6; ++x)
    {
        for (int z = 0; z < 6; ++z)
        {
            const Vec3 sample = {
                position.x + static_cast<float>(x) * 0.5f - 1.5f,
                position.y,
                position.z + static_cast<float>(z) * 0.5f - 1.5f
            };
            if (CheckForCollisionWithStrategicObjects(sample))
                return true;
        }
    }
    return false;
}

// . CheckVehicleDropGrid never writes its float reference in either
// port. [PLATFORM] The ELF later compares that untouched stack value against terrain/water. MSVC
// coalesces the reference target with arg_0's stack home, stores terrain there, and emits only a
// redundant self-compare at 0x56597F..0x56598F; reading an indeterminate C++ value here would add
// behavior that the shipped PE does not have. The radii start in the surprising (3,4) order,
// making CalculateRandomDropVehicleDeviation's modulo divisor negative.
void CommanderToolState::findValidLandingPositionForDropVehicle(Vec3& position)
{
    Vec3 deviation = { 0.0f, 0.0f, 0.0f };
    float firstRadius = 3.0f;
    float secondRadius = 4.0f;
    float gridHeight; // passed in both ports; CheckVehicleDropGrid never assigns it
    bool found = false;

    do
    {
        const Vec3 candidate = {
            position.x + deviation.x,
            position.y + deviation.y,
            position.z + deviation.z
        };
        if (CheckVehicleDropGrid(candidate, gridHeight))
        {
            deviation = CalculateRandomDropVehicleDeviation(
                static_cast<int>(firstRadius), static_cast<int>(secondRadius));
            firstRadius += 0.1f;
            secondRadius += 0.1f;
        }
        else
        {
            found = true;
        }
    }
    while (!found);

    position.x += deviation.x;
    position.y += deviation.y;
    position.z += deviation.z;

    IHeightmapCluster* const cluster =
        static_cast<IHeightmapCluster*>(heightmapCluster);
    const float terrainHeight = cluster->getHeightInWorldCoords(position.x, position.z);
    const float waterHeight = cluster->getWaterLevelInWorldCoords(position);
    float landingHeight = waterHeight > terrainHeight ? waterHeight : terrainHeight;
#if !defined(_WIN32)
    // [PLATFORM] Only the shipped ELF effectively reads the untouched output value here.
    if (gridHeight > landingHeight)
        landingHeight = gridHeight;
#endif
    position.y = gameLogic->getVehicleDropHeight() + landingHeight;
}

// . The first pass forces collision LOD 1 and the second runs at the
// normal (-1) override; the second height is always published even when it is the -9999 fallback.
bool CommanderToolState::isXZIntersectionForVehicleVsDecalCloserThan(
    const Vec3& position, float distance, float& height)
{
    const Vec3 from = { position.x, 2000.0f, position.z };
    ObjectFlagPredicator predicator(0x100u);
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart = 0;

    objectManager->setOverrideLineTestLod(1);
    Vec3 direction = { 0.0f, -9999.0f, 0.0f };
    const bool vehicleHit = objectManager->intersectLine(
        hitObject, hitPoint, hitNormal, hitPart, from, direction, &predicator,
        false, false, false, false, CID_SoldierTemplate);
    const float vehicleHeight = vehicleHit ? hitPoint.y : -9999.0f;

    objectManager->setOverrideLineTestLod(-1);
    direction.x = 0.0f;
    direction.y = -9999.0f;
    direction.z = 0.0f;
    const bool decalHit = objectManager->intersectLine(
        hitObject, hitPoint, hitNormal, hitPart, from, direction, &predicator,
        false, false, false, false, CID_SoldierTemplate);
    height = decalHit ? hitPoint.y : -9999.0f;
    return std::fabs(vehicleHeight - height) <= distance;
}

bool CommanderToolState::getUAVIsReady() const
{
    return m_uavCount != 0 && m_uavArmor > 0.0f && m_uavReloadTime <= 0.0f;
}

bool CommanderToolState::getScanIsReady() const
{
    return m_satelliteCount != 0 && m_satelliteArmor > 0.0f && m_scanReloadTime <= 0.0f;
}

bool CommanderToolState::getUAVActive(Vec3* position) const
{
    if (!(m_uavActiveTime > 0.0f))
        return false;
    *position = m_uavPosition;
    return true;
}

bool CommanderToolState::getScanActive() const
{
    return m_scanActiveTime > 0.0f;
}

CommanderToolsManager::CommanderToolsManager()
{
    if (!commanderToolsManager)
    {
        eventManager->registerEventHandler(ECGameLogic, this, 0);
        commanderToolsManager = this;
    }

    m_lastDropVehicleTimes[0] = 0.0;
    m_lastDropVehicleTimes[1] = 0.0;
    m_lastDropVehicleTimes[2] = 0.0;
}

CommanderToolsManager::~CommanderToolsManager()
{
    if (commanderToolsManager == this)
    {
        eventManager->unregisterEventHandler(ECGameLogic, this);
        commanderToolsManager = 0;
    }
}

void CommanderToolsManager::handleEvent(EventCategory category, unsigned int id,
                                        EventNode*, void* userData)
{
    if (category != ECGameLogic)
        return;

    const CommanderToolNotification* const notification =
        static_cast<const CommanderToolNotification*>(userData);
    if (id == 18u)
        handleSatelliteEvent(notification->id);
    else if (id == 21u)
        handleDropVehicleEvent(notification->position, notification->id);
    else if (id == 17u)
        handleUAVEvent(notification->position, notification->id);
}

void CommanderToolsManager::update(float deltaTime)
{
    m_teamStates[0].update(deltaTime);
    m_teamStates[1].update(deltaTime);
    updateCommanderToys();
}

void CommanderToolsManager::reset()
{
    m_teamStates[0].reset();
    m_teamStates[1].reset();
}

void CommanderToolsManager::handleUAVEvent(const Vec3& position, int team)
{
    // Retail indexes directly here; UAVEvent's commander/team gate is the upstream validator.
    if (!m_teamStates[team - 1].launchUAV(position))
        return;

    const std::list<IPlayer*>& players = playerManager->getPlayers(team);
    for (std::list<IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        IPlayer* const player = *it;
        if (player->getId() != 255)
        {
            PostRemoteEvent* const event =
                new PostRemoteEvent(ECHud, 40u, false,
                                    const_cast<Vec3*>(&position), 12u, 0.0f);
            game->sendGameEventIfPlaying(player->getId(), event);
        }
    }
}

void CommanderToolsManager::handleSatelliteEvent(int playerId)
{
    IPlayer* player = playerId == -1
        ? playerManager->getLocalHumanPlayer()
        : playerManager->getPlayerFromId(playerId);

    if (!player || !player->getIsCommander())
        return;

    const int team = player->getTeam();
    if (team <= 0 || !m_teamStates[team - 1].startScan())
        return;

    if (playerId != -1)
    {
        PostRemoteEvent* const event =
            new PostRemoteEvent(ECHud, 41u, false, 0, 0u, 0.0f);
        game->sendGameEventIfPlaying(playerId, event);
    }
}

void CommanderToolsManager::handleDropVehicleEvent(const Vec3& position, int team)
{
    const double now = g_system->getExactTime();
    if (now - m_lastDropVehicleTimes[team] < 59.900002)
        return;

    m_lastDropVehicleTimes[team] = now;
    const int objectId = m_teamStates[team - 1].dropVehicle(position, team);
    if (objectId == -1)
        return;

    (void)playerManager->getLocalHumanPlayer();             // retained, unused retail call
    const std::list<IPlayer*>& players = playerManager->getPlayers(team);

    DropVehicleHudPayload payload;
    payload.objectId = objectId;
    payload.remainingFraction = getPercentOfDropVehicles(team);
    payload.position = position;

    for (std::list<IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        PostRemoteEvent* const event =
            new PostRemoteEvent(ECHud, 74u, false, &payload, 20u, 0.0f);
        game->sendGameEventIfConnected((*it)->getId(), event);
    }
}

bool CommanderToolsManager::getUAVActive(int team, Vec3* position)
{
    if (static_cast<unsigned int>(team - 1) > 1u)
        return false;
    return m_teamStates[team - 1].getUAVActive(position);
}

bool CommanderToolsManager::getUAVArmor(int team)
{
    if (static_cast<unsigned int>(team - 1) > 1u)
        return false;
    return m_teamStates[team - 1].m_uavArmor > 0.0f;
}

bool CommanderToolsManager::getSatelliteScanActive(int team)
{
    if (static_cast<unsigned int>(team - 1) > 1u)
        return false;
    return m_teamStates[team - 1].getScanActive();
}

float CommanderToolsManager::getPercentOfDropVehicles(int team)
{
    // No team or divide-by-zero guard exists in either shipped port.
    const float maximum = static_cast<float>(gameLogic->getNumDropVehiclesForTeam(team));
    return (maximum - static_cast<float>(m_teamStates[team - 1].m_dropVehicleCount)) / maximum;
}

void CommanderToolsManager::addDropVehicleToTeam(int team)
{
    --m_teamStates[team - 1].m_dropVehicleCount;
    (void)playerManager->getLocalHumanPlayer();             // retained, unused retail call
    const std::list<IPlayer*>& players = playerManager->getPlayers(team);
    float remainingFraction = getPercentOfDropVehicles(team);

    for (std::list<IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        PostRemoteEvent* const event =
            new PostRemoteEvent(ECHud, 75u, false, &remainingFraction, 4u, 0.0f);
        game->sendGameEventIfConnected((*it)->getId(), event);
    }
}

void CommanderToolsManager::updateCommanderToys()
{
    typedef std::map<unsigned int, IObject*> RemoteObjectMap;
    RemoteObjectMap* const objects =
        static_cast<RemoteObjectMap*>(gameLogic->getRemoteControlledObjectMap());

    m_teamStates[0].resetToysStats();
    m_teamStates[1].resetToysStats();

    float currentArtillery[2] = {0.0f, 0.0f};
    float currentSatellite[2] = {0.0f, 0.0f};
    float currentUAV[2] = {0.0f, 0.0f};

    for (RemoteObjectMap::iterator it = objects->begin(); it != objects->end(); ++it)
    {
        IObject* const object = it->second;
        const RemoteControlledObjectTemplate* const objectTemplate =
            static_cast<const RemoteControlledObjectTemplate*>(object->getTemplate());
        const int type = objectTemplate->rcType;
        const int team = static_cast<RemoteControlledObject*>(object)->getTeam();
        if (static_cast<unsigned int>(team - 1) > 1u)
            continue;
        if (type != 1 && type != 2 && type != 3)
            continue;

        float currentHitPoints = 0.0f;
        float maximumHitPoints = 0.0f;
        IArmorGLComp* const armor = static_cast<IArmorGLComp*>(
            getRootParent(object)->getArmorComponent());
        if (armor)
        {
            currentHitPoints = armor->getHitPoints();
            if (currentHitPoints <= 0.0f)
                currentHitPoints = 0.0f;
            maximumHitPoints = armor->getMaxHitPoints();
        }

        CommanderToolState& state = m_teamStates[team - 1];
        if (type == 1)
        {
            state.m_artilleryArmor += maximumHitPoints;
            ++state.m_artilleryCount;
            currentArtillery[team - 1] += currentHitPoints;
        }
        else if (type == 2)
        {
            state.m_satelliteArmor += maximumHitPoints;
            ++state.m_satelliteCount;
            currentSatellite[team - 1] += currentHitPoints;
        }
        else
        {
            state.m_uavArmor += maximumHitPoints;
            ++state.m_uavCount;
            currentUAV[team - 1] += currentHitPoints;
        }
    }

    // This is intentionally maximum/current, not the conventional current/maximum. The `fdivr`
    // sequence is the same in both shipped ports and Player replicates these exact float bits.
    for (int teamIndex = 0; teamIndex < 2; ++teamIndex)
    {
        CommanderToolState& state = m_teamStates[teamIndex];
        if (currentArtillery[teamIndex] <= 0.0f)
            state.m_artilleryArmor = 1.0f;
        else
            state.m_artilleryArmor /= currentArtillery[teamIndex];

        if (currentSatellite[teamIndex] <= 0.0f)
            state.m_satelliteArmor = 1.0f;
        else
            state.m_satelliteArmor /= currentSatellite[teamIndex];

        if (currentUAV[teamIndex] <= 0.0f)
            state.m_uavArmor = 1.0f;
        else
            state.m_uavArmor /= currentUAV[teamIndex];
    }
}

} // namespace world
} // namespace hfe
} // namespace dice
