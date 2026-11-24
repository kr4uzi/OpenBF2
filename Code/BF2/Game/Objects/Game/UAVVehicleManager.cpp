// Per-team UAV management.

#include "UAVVehicleManager.h"

#include "CommanderToolsManager.h"
#include "UAVVehicle.h"
#include "../IObject.h"
#include "../IObjectTemplate.h"
#include "../RemoteControlledObjectTemplate.h"
#include "../../Common/GameEvent.h"
#include "../../IGame.h"
#include "../../IGameLogic.h"
#include "../../Main/Game.h"
#include "../../../Core/EventManager.h"
#include "../../../Core/IBase.h"
#include "../../../IO/Network/INetworkable.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include <cmath>
#include <string>

namespace dice {
namespace hfe {
namespace world {

extern IBase* heightmapCluster;                    // ds:0x8ABAF24, Core/Globals.cpp

namespace {

int networkableIdOrMinusOne(IObject* object)
{
    int id = -1;
    io::INetworkable* const networkable = object->getNetworkable();
    if (networkable)
        id = networkable->m_networkableId;
    return id;
}

} // anonymous namespace

UAVVehicleManager::UAVVehicleManager()
{
    m_uavVehicles[0] = 0;
    m_uavVehicles[1] = 0;
    m_uavActive[0] = false;
    m_uavActive[1] = false;

    if (!uavVehicleManager)
    {
        eventManager->registerEventHandler(ECGameLogic, this, 0);
        uavVehicleManager = this;
    }
}

UAVVehicleManager::~UAVVehicleManager()
{
    if (m_uavVehicles[0])
        delete m_uavVehicles[0];
    if (m_uavVehicles[1])
        delete m_uavVehicles[1];
    m_uavActive[0] = false;
    m_uavActive[1] = false;

    if (uavVehicleManager == this)
    {
        eventManager->unregisterEventHandler(ECGameLogic, this);
        uavVehicleManager = 0;
    }
}

void UAVVehicleManager::init()
{
    // Retail does not clear either pointer after deleting it. A later create overwrites the slot;
    // retaining the dangling value in the interim is observable and therefore preserved.
    if (m_uavVehicles[0])
        delete m_uavVehicles[0];
    if (m_uavVehicles[1])
        delete m_uavVehicles[1];
    m_uavActive[0] = false;
    m_uavActive[1] = false;
}

void UAVVehicleManager::reset()
{
    if (gameLogic)
    {
        // Both calls exist in retail even though init/removeAll have the same body.
        removeAll();
        init();
    }
}

void UAVVehicleManager::removeAll()
{
    // Byte-identical behavior to init in the ELF, including the retained pointer values.
    if (m_uavVehicles[0])
        delete m_uavVehicles[0];
    if (m_uavVehicles[1])
        delete m_uavVehicles[1];
    m_uavActive[0] = false;
    m_uavActive[1] = false;
}

void UAVVehicleManager::removeUAVVehicle(int team)
{
    UAVVehicle* const vehicle = m_uavVehicles[team - 1];
    int payload[2];
    payload[0] = networkableIdOrMinusOne(vehicle);
    payload[1] = team;

    game->sendGameEventToAll(
        new PostRemoteEvent(ECGameLogic, 20u, false, payload, 8u, 0.0f));
#if !DEDICATED
    // BF2.exe 0x5C61E0..0x5C61FB: remove the local marker/reset reload before destruction.
    if (!isDedicated())
        eventManager->postEvent(ECGameLogic, 20u, payload, 0.0f);
#endif
    objectManager->destroyObject(vehicle);
    m_uavVehicles[team - 1] = 0;
    m_uavActive[team - 1] = false;
}

void UAVVehicleManager::handleEvent(EventCategory category, unsigned int id,
                                    EventNode*, void* userData)
{
    if (category == ECGameLogic && id == 17u)
    {
        CommanderToolNotification* const notification =
            static_cast<CommanderToolNotification*>(userData);
        createUAVVehicle(notification->position, notification->id);
    }
}

void UAVVehicleManager::update(float deltaTime)
{
    if (m_uavActive[0] == true)
        m_uavVehicles[0]->update(deltaTime);
    if (m_uavActive[1] == true)
        m_uavVehicles[1]->update(deltaTime);
}

void UAVVehicleManager::createUAVVehicle(Vec3& position, int team)
{
    RemoteControlledObjectTemplate* const controlTemplate =
        static_cast<RemoteControlledObjectTemplate*>(
            objectTemplateManager->getTemplate(std::string("UAVControlObject")));
    if (!controlTemplate)
        return;

    IObjectTemplate* const vehicleTemplate =
        objectTemplateManager->getTemplate(controlTemplate->uavVehicleTemplate);
    Vec3 rotation;
    rotation.x = 0.0f;
    rotation.y = 0.0f;
    rotation.z = 0.0f;

    m_uavVehicles[team - 1] = static_cast<UAVVehicle*>(
        game->createObjectOnAllClients(vehicleTemplate, position, rotation, true, 0, 0));
    initUAVVehicle(team);

    int payload[2];
    payload[0] = networkableIdOrMinusOne(m_uavVehicles[team - 1]);
    payload[1] = team;
    game->sendGameEventToAll(
        new PostRemoteEvent(ECGameLogic, 19u, false, payload, 8u, 0.0f));
#if !DEDICATED
    // BF2.exe 0x5C632C..0x5C6347: register the UAV locally after the remote broadcast.
    if (!isDedicated())
        eventManager->postEvent(ECGameLogic, 19u, payload, 0.0f);
#endif
}

void UAVVehicleManager::initUAVVehicle(int team)
{
    UAVVehicle* const vehicle = m_uavVehicles[team - 1];
    vehicle->m_team = team;
    m_uavActive[team - 1] = true;

    RemoteControlledObjectTemplate* const controlTemplate =
        static_cast<RemoteControlledObjectTemplate*>(
            objectTemplateManager->getTemplate(std::string("UAVControlObject")));
    const float speed = controlTemplate->uavVehicleSpeed;
    const float radius = controlTemplate->uavVehicleRadius > 0.0f
        ? controlTemplate->uavVehicleRadius : 0.1f;
    const float sampleResolution = controlTemplate->uavVehicleSampleResolution > 0.0f
        ? controlTemplate->uavVehicleSampleResolution : 1.0f;

    Vec3 position = vehicle->getAbsolutePosition();
    float bankArgument = -1.0f;
    const float ratio = speed / (radius * 30.0f);
    if (ratio >= -1.0f)
    {
        bankArgument = ratio;
        if (ratio > 1.0f)
            bankArgument = 1.0f;
    }
    vehicle->m_turnRate =
        static_cast<float>(std::asin(bankArgument) * 57.295776) / -1.9f;

    const float sampleRadius = radius + 5.0f;
    const float sampleRadiusSquared = sampleRadius * sampleRadius;
    IHeightmapCluster* const cluster = static_cast<IHeightmapCluster*>(heightmapCluster);
    float maximumHeight = cluster->getHeightInWorldCoords(position);
    const float waterHeight = cluster->getWaterLevelInWorldCoords(position);
    if (waterHeight > maximumHeight)
        maximumHeight = waterHeight;

    // The shipped sampler is intentionally not a circle equation: X uses sqrt(r^2 + index), while
    // Z advances index/resolution*r. The four sign passes are (+,+), (-,+), (+,-), (-,-).
    int xSign = 1;
    int zSign = 1;
    for (int pass = 0; pass <= 3; ++pass)
    {
        for (int sampleIndex = 0;
             sampleResolution > static_cast<float>(sampleIndex);
             ++sampleIndex)
        {
            Vec3 sample;
            sample.x = static_cast<float>(
                std::sqrt(sampleRadiusSquared + static_cast<float>(sampleIndex)))
                * static_cast<float>(xSign) + position.x;
            sample.y = position.y;
            sample.z = static_cast<float>(sampleIndex) / sampleResolution * sampleRadius
                * static_cast<float>(zSign) + position.z;
            const float terrainHeight = cluster->getHeightInWorldCoords(sample);
            if (terrainHeight > maximumHeight)
                maximumHeight = terrainHeight;
        }
        zSign |= xSign;
        xSign = -xSign;
    }

    position.y = maximumHeight + controlTemplate->uavVehicleFlightHeight
        + gameLogic->getExtraUAVVehicleFlightHeight();
    position.x += sampleRadius - 5.0f;
    vehicle->setAbsolutePosition(position);
}

} // namespace world
} // namespace hfe
} // namespace dice
