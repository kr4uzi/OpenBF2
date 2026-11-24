// Commander UAV vehicle lifecycle.

#include "UAVVehicle.h"
#include "UAVVehicleManager.h"
#include "CommanderToolsManager.h"

#include "../../../Core/Ids.h"
#include "../TemplateLeaves.h"                    // UAVVehicleTemplate - the ctor's upcast
#include "../IPlayer.h"                           // getId / setHitIndicator
#include "../ObjectLayoutAccess.h"                // handleCollision's template read (+0x14)
#include "../ObjectTemplate.h"                    // getClassID on the read template
#include "../../IGame.h"                          // game - sendGameEventIfPlaying (+0x88)
#include "../../Main/Game.h"
#include "../../Common/GameEvent.h"               // PostRemoteEvent
#include "../../../Core/EventManager.h"
#include "../../../Scene/Object/ObjectManager.h"  // objectManager - postMessage/removeAllMessagesTo
#include "../../../Scene/Object/PlayerManager.h"  // playerManager->getPlayers(int) (+0x48), IPlayer
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../RemoteControlledObjectTemplate.h"
#include "../Projectiles/GenericProjectile.h"

#include <string>

namespace dice {
namespace hfe {
namespace world {

// (C2) / 0x81CC8E0 (C1), 63 B: Bundle, vptrs, the template pointer - AND nothing else.
// m_turnRate and m_team stay uninitialised until the manager's spawn path pokes them.
UAVVehicle::UAVVehicle(const UAVVehicleTemplate* objectTemplate)
    : Bundle(objectTemplate)
{
    m_template = objectTemplate;                  // +0x13C
}

UAVVehicle::~UAVVehicle() {}

// Notifies teammates that the UAV is gone, cancels pending messages and destroys the object.
void UAVVehicle::destroy(bool force)
{
    const std::list<IPlayer*>& players = playerManager->getPlayers(m_team);
    for (std::list<IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        if ((*it)->getId() != 255)
        {
            PostRemoteEvent* event = new PostRemoteEvent(
                static_cast<EventCategory>(2), 82u, false, 0, 0u, 0.0f);
            game->sendGameEventIfPlaying((*it)->getId(), event);
        }
    }

#if !DEDICATED
    // BF2.exe 0x5C610C..0x5C6151: notify the hosting teammate after the remote sends.
    if (!isDedicated() && playerManager->getLocalHumanPlayer()
        && playerManager->getLocalHumanPlayer()->getTeam() == m_team)
    {
        eventManager->postEvent(ECHud, 82u, 0, 0.0f);
    }
#endif

    objectManager->removeAllMessagesTo(this);
    SimpleObject::destroy(force);
}

// wake the bundle, re-arm the 1.0 s heartbeat (the SupplyObject cadence).
void UAVVehicle::enableCachedObject()
{
    Bundle::enableCachedObject();
    objectManager->postMessage(this, static_cast<TemplateMessage>(29), 1.0f);
}

// leave the manager slot, then the ordinary next-override.
void UAVVehicle::disableCachedObject()
{
    destroyUAVVehicle();
    SimpleObject::disableCachedObject();
}

// message 24 kills the drone. NOT 29 - the heartbeat enableCachedObject arms
// dispatches into... nothing here; 29 falls through unanswered. [UNRESOLVED] who posts 24.
void UAVVehicle::handleMessage(TemplateMessage message, IPlayer*, bool)
{
    if (message == static_cast<TemplateMessage>(24))
        destroyUAVVehicle();
}

// handleCollision - 0x81CCB60, 177 B. A projectile that hits the drone is told it hit armor;
// everything else passes to Bundle. The projectile arm is the typed form of the three recovered
// interface calls: isCreatedByLocalPlayer(), getPlayer(), then setHitIndicator(true). Retail does not
// null-check the returned player. Returns false on this arm, as shipped.
bool UAVVehicle::handleCollision(IObject* object, const Vec3& a, const Vec3& b, const Vec3& c,
                                 int d, int e, CollisionFaceInfo* f1, CollisionFaceInfo* f2)
{
    typedef object_detail::LayoutAccess L;
    if (!object || static_cast<const ObjectTemplate*>(L::objectTemplate(object))->getClassID()
                       != CID_GenericProjectileTemplate)
        return Bundle::handleCollision(object, a, b, c, d, e, f1, f2);

    GenericProjectile* const projectile = static_cast<GenericProjectile*>(object);
    if (projectile->isCreatedByLocalPlayer())
        projectile->getPlayer()->setHitIndicator(true);
    return false;
}

// destroyUAVVehicle - 0x81CCC20, 81 B. If the manager exists and either team slot is this drone,
// remove it from that slot. Null manager (this tree, today) -> no-op, as shipped.
void UAVVehicle::destroyUAVVehicle()
{
    if (uavVehicleManager)
    {
        if (uavVehicleManager->getUAVVehicleForTeam(1) == this)
            uavVehicleManager->removeUAVVehicle(1);
        if (uavVehicleManager->getUAVVehicleForTeam(2) == this)
            uavVehicleManager->removeUAVVehicle(2);
    }
}

// . `deltaTime` is unused: the manager drives one fixed velocity/turn update per
// tick. Loss of the active timer or of all UAV-control-object armor removes the drone.
void UAVVehicle::update(float)
{
    Vec3 centre;
    centre.x = 0.0f;
    centre.y = 0.0f;
    centre.z = 0.0f;
    if (!commanderToolsManager
        || !commanderToolsManager->getUAVActive(m_team, &centre)
        || !commanderToolsManager->getUAVArmor(m_team))
    {
        destroyUAVVehicle();
        return;
    }

    const RemoteControlledObjectTemplate* const controlTemplate =
        static_cast<const RemoteControlledObjectTemplate*>(
            objectTemplateManager->getTemplate(std::string("UAVControlObject")));
    IPhysicsNode* const physicsNode = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(this));

    Vec3 rotationalSpeed;
    rotationalSpeed.x = 0.0f;
    rotationalSpeed.y = m_turnRate;
    rotationalSpeed.z = 0.0f;
    physicsNode->setRotationalSpeed(rotationalSpeed);

    const Mat4& transform = physicsNode->getAbsoluteTransformation();
    Vec3 positionalSpeed;
    positionalSpeed.x = transform.m[8] * controlTemplate->uavVehicleSpeed;
    positionalSpeed.y = transform.m[9] * controlTemplate->uavVehicleSpeed;
    positionalSpeed.z = transform.m[10] * controlTemplate->uavVehicleSpeed;
    physicsNode->setPositionalSpeed(positionalSpeed);
}

} // namespace world
} // namespace hfe
} // namespace dice
