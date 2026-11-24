// Commander vehicle-drop behavior.

#include "DropVehicle.h"

#include "CommanderToolsManager.h"
#include "PlayerControlObject.h"
#include "../TemplateLeaves.h"
#include "../GameLogicComp/IArmorGLComp.h"
#include "../ObjectLayoutAccess.h"
#include "../../Common/GameEvent.h"
#include "../../IGame.h"
#include "../../IGameLogic.h"
#include "../../../Core/Ids.h"
#include "../../../IO/Network/INetworkable.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#if !DEDICATED
#include "../../SingletonServer.h"
#endif

#include <string>

namespace dice {
namespace hfe {
namespace world {

// . m_vehicle and m_stationaryTime are the two holes in both
// constructors and intentionally retain operator-new residue until their normal writers run.
DropVehicle::DropVehicle(const DropVehicleTemplate* objectTemplate)
    : SimpleObject(objectTemplate)
{
    m_parachute = 0;
    m_onGroundCount = 0;
    m_vehicleExpired = false;
    m_unusedVehicleCount = 0;
    m_team = 1;
    m_unusedVehicleExpired = false;
    m_landed = false;
    m_previousPosition.x = 0.0f;
    m_previousPosition.y = 0.0f;
    m_previousPosition.z = 0.0f;
    Object::updateFlags(0x800u, 0u);
    m_dropVehicleTemplate = objectTemplate;
#if !DEDICATED
    // BF2.exe 0x005E9F26..0x005E9F33: register the controller, not its vehicle.
    if (SingletonServer::m_iNametagManager)
        SingletonServer::m_iNametagManager->addTouchItem(this);
#endif
}

DropVehicle::~DropVehicle()
{
}

// . Returning the stock precedes the ordinary object teardown.
void DropVehicle::destroy(bool force)
{
    if (commanderToolsManager)
        commanderToolsManager->addDropVehicleToTeam(m_team);
    SimpleObject::destroy(force);
}

// . The repeated object/component reads and the two deliberately
// unchecked pointers (the airborne physics node and landed parachute) are shipped behavior.
void DropVehicle::handleUpdate(float deltaTime, unsigned int)
{
    IObject* const originalVehicle = m_vehicle;
    if (!originalVehicle || !originalVehicle->getArmorComponent())
    {
        objectManager->destroyObject(this);
        m_vehicle = 0;
        return;
    }

    IArmorGLComp* const initialArmor =
        static_cast<IArmorGLComp*>(originalVehicle->getArmorComponent());
#if !DEDICATED
    // BF2.exe 0x005E9FAA..0x005E9FC8: retire the drop marker before following the vehicle.
    if (initialArmor->isDestroyed()
        || static_cast<PlayerControlObject*>(originalVehicle)->vehicleIsUsed())
    {
        if (SingletonServer::m_iNametagManager)
            SingletonServer::m_iNametagManager->removeItem(2, this);
    }
#else
    if (!initialArmor->isDestroyed())
        static_cast<PlayerControlObject*>(originalVehicle)->vehicleIsUsed();
#endif

    setAbsolutePosition(m_vehicle->getAbsolutePosition());
    if (!gameLogic->isServer())
        return;

    if (m_landed)
    {
        if (m_vehicle->getTemplate()->getClassID() == CID_PlayerControlObjectTemplate)
        {
            if (static_cast<PlayerControlObject*>(originalVehicle)->vehicleIsUsed())
                m_unusedVehicleCount = 0;
            else
                ++m_unusedVehicleCount;

            m_unusedVehicleExpired =
                m_unusedVehicleCount > 30 * m_dropVehicleTemplate->m_destroyVehicleTime;
            if (m_unusedVehicleExpired)
                objectManager->destroyObject(m_vehicle);
        }
        return;
    }

    IPhysicsNode* const physicsNode = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(m_vehicle));
    if (physicsNode->getIsInAir())
        m_onGroundCount = 0;
    else
        ++m_onGroundCount;

    static int onGroundMaxCount = 1;
    m_landed = m_onGroundCount > onGroundMaxCount;

    IArmorGLComp* const currentArmor =
        static_cast<IArmorGLComp*>(m_vehicle->getArmorComponent());
    if (currentArmor)
        m_landed = currentArmor->isDestroyed() || m_landed;

    const Vec3& currentPosition = getAbsolutePosition();
    const float deltaX = currentPosition.x - m_previousPosition.x;
    const float deltaY = currentPosition.y - m_previousPosition.y;
    const float deltaZ = currentPosition.z - m_previousPosition.z;
    const float movementSquared = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
    if (movementSquared < 0.01f)
        m_stationaryTime += deltaTime;
    else
        m_stationaryTime = 0.0f;

    if (m_stationaryTime > 0.5f)
        m_landed = true;

    const Vec3& sampledPosition = getAbsolutePosition();
    m_previousPosition.x = sampledPosition.x;
    m_previousPosition.y = sampledPosition.y;
    m_previousPosition.z = sampledPosition.z;

    if (!m_landed)
        return;

    objectManager->destroyObject(m_parachute);
    m_parachute = 0;

    int networkableId = -1;
    io::INetworkable* const networkable = m_vehicle->getNetworkable();
    if (networkable)
    {
        networkableId = networkable->m_networkableId;
        PostRemoteEvent* const event =
            new PostRemoteEvent(ECGameLogic, 23u, false,
                                &networkableId, 4u, 0.0f);
        game->sendGameEventToAll(event);
    }
}

// . The template lookup is intentionally unguarded before createObject;
// shipped content must define "supply_parachute" whenever this controller is used.
void DropVehicle::setVehicle(IObject* vehicle)
{
    m_vehicle = vehicle;
    IObjectTemplate* const parachuteTemplate =
        objectTemplateManager->getTemplate(std::string("supply_parachute"));
    m_parachute = parachuteTemplate->createObject();

    if (m_vehicle && m_parachute)
    {
        m_parachute->init();
        m_vehicle->addChild(m_parachute);
        Vec3 relativePosition = { 0.0f, 2.5f, 0.0f };
        m_parachute->setRelativePosition(relativePosition);
    }
}

} // namespace world
} // namespace hfe
} // namespace dice
