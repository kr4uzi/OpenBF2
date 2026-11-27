// Path:     attested - both Debug sites name "Game/Objects/Game/ArtilleryHandler.cpp".
// Contents: recovered from the symbolised BF2 1.50 ELF (0x816C0F0..0x816DFEB), with the
// firearm sequence and matrix/Euler call order cross-checked in bf2_w32ded.exe.

#include "ArtilleryHandler.h"

#include "RemoteControlledObject.h"
#include "TargetObject.h"
#include "../../../Core/Debug.h"
#include "../../../Core/EventManager.h"
#include "../../../Core/IBase.h"
#include "../../../Core/Ids.h"
#include "../../../Core/System.h"
#include "../../../IO/Network/INetworkable.h"
#include "../../../Physics/BasicPhysicsSystem.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Scene/Object/ObjectPredicator.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../../Common/GameEvent.h"
#include "../../GameEvents/ArtilleryEvent.h"
#include "../../GameLogic/SquadManager/ISquadManager.h"
#include "../../IGame.h"
#include "../../IGameLogic.h"
#include "../../Main/Game.h"
#include "../Base/SimpleObjectTemplate.h"
#include "../GameLogicComp/IArmorGLComp.h"
#include "../IObjectTemplate.h"
#include "../IPlayer.h"
#include "../RemoteControlledObjectTemplate.h"
#include "../TargetObjectTemplate.h"
#include "../Weapons/GenericFireArm.h"

#include <cfloat>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace dice {
namespace hfe {

extern IBase* squadManager;                         // Core/Globals.cpp

namespace world {

extern IBase* heightmapCluster;                     // Core/Globals.cpp

namespace {

const int WeaponMessageFire = 7;                    // [UNRESOLVED] TemplateMessage enumerator name

class RCPredicator : public ObjectPredicator
{
public:
    RCPredicator() : m_unused(0), m_flags(0) {}

    virtual bool predicate(IObject* object)
    {
        if (!object || !object->getTemplate())
            return false;
        return const_cast<IObjectTemplate*>(object->getTemplate())
                   ->queryImplementation(CID_RemoteControlledObjectTemplate) != 0;
    }

    virtual unsigned int getFlags() const { return m_flags; }
    virtual bool performCulling(IObject*) { return true; }

private:
    int          m_unused;                          // original local object +0x04, ctor 0
    unsigned int m_flags;                           // original local object +0x08, ctor 0
};

IArmorGLComp* armorOf(IObject* object)
{
    return static_cast<IArmorGLComp*>(object->getArmorComponent());
}

void normalizeRetail(Vec3& value)
{
    const float lengthSquared =
        value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSquared - 1.0f) < FLT_EPSILON)
        return;
    if (std::fabs(lengthSquared) >= FLT_EPSILON)
    {
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        value.x *= inverseLength;
        value.y *= inverseLength;
        value.z *= inverseLength;
    }
    else
    {
        value = Vec3::getZero();
    }
}

float calcParabellicXDistance(float gravity, float velocity, float angle, float height)
{
    const float flightTerm = 2.0f * velocity * std::sin(angle) / gravity;
    const float time = -0.5f * flightTerm +
        std::sqrt(0.5f * flightTerm * (0.5f * flightTerm) -
                  (-2.0f * height) / gravity);
    return std::cos(angle) * velocity * time;
}

struct ArtilleryHudPayload
{
    Vec3 position;
    bool accepted;
};

} // anonymous namespace

// ArtilleryManager - two owned handlers, one per one-based team.
ArtilleryManager::ArtilleryManager()
{
    m_handlers[0] = 0;
    m_handlers[1] = 0;
    if (!artilleryManager)
        artilleryManager = this;
}

ArtilleryManager::~ArtilleryManager()
{
    if (m_handlers[0])
        delete m_handlers[0];
    if (m_handlers[1])
        delete m_handlers[1];
    if (artilleryManager == this)
        artilleryManager = 0;
}

void ArtilleryManager::update(float deltaTime)
{
    if (m_handlers[0])
        m_handlers[0]->update(deltaTime);
    if (m_handlers[1])
        m_handlers[1]->update(deltaTime);
}

void ArtilleryManager::init()
{
    if (m_handlers[0])
        delete m_handlers[0];
    if (m_handlers[1])
        delete m_handlers[1];
    m_handlers[0] = new ArtilleryHandler(1);
    m_handlers[1] = new ArtilleryHandler(2);
}

void ArtilleryManager::removeAll()
{
    if (m_handlers[0])
        delete m_handlers[0];
    if (m_handlers[1])
        delete m_handlers[1];
    m_handlers[0] = 0;
    m_handlers[1] = 0;
}

void ArtilleryManager::reset()
{
    if (gameLogic)
    {
        removeAll();
        init();
    }
}

void ArtilleryManager::refresh()
{
    if (m_handlers[0])
        m_handlers[0]->findArtilleryPieces();
    if (m_handlers[1])
        m_handlers[1]->findArtilleryPieces();
}

// ArtilleryHandler
ArtilleryHandler::ArtilleryHandler(int team)
    : m_team(team)
    , m_minIntervalTicks(0)
    , m_target()
    , m_fireAtTick(0)
    , m_firePending(false)
    , m_activationScale(2.0f)
    , m_fireDelayTicks(60)
    , m_missionActive(false)
{
    findArtilleryPieces();

    TargetObjectTemplate* objectTemplate = static_cast<TargetObjectTemplate*>(
        objectTemplateManager->getTemplate(std::string("UniqueTarget")));
    if (objectTemplate)
    {
        TargetObject* target;
        if (objectTemplate->targetType == 3)
        {
            target = static_cast<TargetObject*>(objectTemplate->createObject());
            if (target)
                target->init();
        }
        else
        {
            if (objectTemplate->getNetworkableInfo().empty())
            {
                Debug(DEBUGTYPE_6, "Game/Objects/Game/ArtilleryHandler.cpp", 189, "")
                    << "Target object must be networkable.";
            }
            const Vec3 zero = Vec3::getZero();
            target = static_cast<TargetObject*>(
                game->createObjectOnAllClients(objectTemplate, zero, zero, true, 0, 0));
        }

        m_targetObject = target->getWeakPtr();
        target->setTargetActive(false, true);
    }
}

ArtilleryHandler::~ArtilleryHandler()
{
    IObject* target = m_targetObject.get();
    if (target)
        game->destroyObject(target);
}

void ArtilleryHandler::findArtilleryPieces()
{
    m_artillery.clear();

    typedef std::map<unsigned int, IObject*> RemoteObjectMap;
    RemoteObjectMap* objects =
        static_cast<RemoteObjectMap*>(gameLogic->getRemoteControlledObjectMap());
    float longestInterval = 0.0f;

    for (RemoteObjectMap::iterator it = objects->begin(); it != objects->end(); ++it)
    {
        RemoteControlledObject* object = static_cast<RemoteControlledObject*>(it->second);
        const RemoteControlledObjectTemplate* objectTemplate =
            static_cast<const RemoteControlledObjectTemplate*>(object->getTemplate());
        if (objectTemplate->rcType == 1 && object->getTeam() == m_team)
        {
            IArmorGLComp* armor = armorOf(object);
            if (!armor || !armor->isDestroyed())
            {
                ArtilleryObjectStruct entry;
                entry.artillery = object;
                entry.horizontalTurnObject = 0;
                entry.verticalTurnObject = 0;
                entry.missionFailed = false;
                findHelperObjects(entry);
                m_artillery.push_back(entry);

                if (objectTemplate->minTimeBetweenActivation > longestInterval)
                    longestInterval = objectTemplate->minTimeBetweenActivation;
            }
        }
    }

    m_minIntervalTicks = static_cast<int>(longestInterval * 30.0f);
}

void ArtilleryHandler::findHelperObjects(ArtilleryObjectStruct& artillery)
{
    std::vector<IObject*> children;
    RCPredicator predicator;

    if (artillery.artillery->m_owner)
    {
        getChilds(artillery.artillery->m_owner, children, &predicator, 0);
        for (unsigned int i = 0; i < children.size(); ++i)
        {
            RemoteControlledObject* helper =
                static_cast<RemoteControlledObject*>(children[i]);
            const RemoteControlledObjectTemplate* objectTemplate =
                static_cast<const RemoteControlledObjectTemplate*>(helper->getTemplate());
            if (objectTemplate->rcType == 4)
                artillery.horizontalTurnObject = helper;
            else if (objectTemplate->rcType == 5)
                artillery.verticalTurnObject = helper;
        }
        return;
    }

    Debug(DEBUGTYPE_6, "Game/Objects/Game/ArtilleryHandler.cpp", 308, "")
        << "RC object has no host!";
}

void ArtilleryHandler::update(float deltaTime)
{
    // BF2.exe 0x5C5332..0x5C5335 / ELF update export 0x0816CD20:
    // the scheduled tick comparison is unsigned.
    if (m_firePending && static_cast<unsigned int>(m_fireAtTick) <
        static_cast<unsigned int>(WorldPref::getTickCount()))
    {
        m_firePending = false;
        doFire();
    }

    if (m_missionActive)
    {
        bool stillTurning = false;
        for (std::list<ArtilleryObjectStruct>::iterator it = m_artillery.begin();
             it != m_artillery.end(); ++it)
        {
            if (it->horizontalTurnObject)
            {
                const int status = it->horizontalTurnObject->updateMission(deltaTime);
                if (status == 1)
                    stillTurning = true;
                else if (status == 3)
                    it->missionFailed = true;
            }
            if (it->verticalTurnObject)
            {
                const int status = it->verticalTurnObject->updateMission(deltaTime);
                if (status == 1)
                    stillTurning = true;
                else if (status == 3)
                    it->missionFailed = true;
            }
        }
        if (!stillTurning)
            m_missionActive = false;
    }
}

bool ArtilleryHandler::setTarget(Vec2& target)
{
    IHeightmapCluster* cluster = static_cast<IHeightmapCluster*>(heightmapCluster);
    const float terrainHeight = cluster->getHeightInWorldCoords(target.x, target.y);
    const float waterHeight = cluster->getWaterLevelInWorldCoords(target.x, target.y);

    Vec3 position;
    position.x = target.x;
    position.y = waterHeight <= terrainHeight ? terrainHeight : waterHeight;
    position.z = target.y;

    TargetObject* targetObject = static_cast<TargetObject*>(m_targetObject.get());
    targetObject->setAbsolutePosition(position);
    targetObject->setTargetActive(true, true);
    if (setTarget(targetObject))
    {
        m_target = target;
        return true;
    }
    return false;
}

bool ArtilleryHandler::setTarget(IObject* target)
{
    int attempts = 0;
    int successes = 0;

    for (std::list<ArtilleryObjectStruct>::iterator it = m_artillery.begin();
         it != m_artillery.end(); ++it)
    {
        RemoteControlledObject* artillery = it->artillery;
        GenericFireArm* firearm = static_cast<GenericFireArm*>(artillery->m_weapon);
        it->missionFailed = false;

        float gravity = static_cast<IPhysicsSystem*>(basicPhysicsSystem)->getGravity();
        if (firearm)
        {
            firearm->setTargetObject(target);
            IObjectTemplate* projectileTemplate = firearm->getProjectileTemplate();
            if (projectileTemplate)
            {
                SimpleObjectTemplate* simpleTemplate = static_cast<SimpleObjectTemplate*>(
                    projectileTemplate->queryImplementation(CID_SimpleObjectTemplate));
                if (simpleTemplate)
                    gravity *= simpleTemplate->m_gravityModifier;
            }
        }

        if (artillery->m_owner)
        {
            Vec3 from = artillery->getAbsolutePosition();
            Vec3 to = target->getAbsolutePosition();
            IWeaponObject* weapon = artillery->m_weapon;
            Vec3 projectileDirection = Vec3::getZero();
            Vec3 horizontalDirection = Vec3::getZero();
            Vec3 verticalDirection = Vec3::getZero();

            ++attempts;
            if (!calcArtilleryAngles(from, to, firearm->getVelocity(), gravity,
                                     it->horizontalTurnObject, it->verticalTurnObject,
                                     projectileDirection, horizontalDirection,
                                     verticalDirection))
                return successes == attempts;

            ++successes;
            it->horizontalTurnObject->initMission(horizontalDirection);
            it->verticalTurnObject->initMission(verticalDirection);

            for (int projectile = 0; projectile < firearm->getBurstSize(); ++projectile)
            {
                Vec3 deviatedTarget = weapon->deviatePosition(to, projectile);
                Vec3 ignoredHorizontal = Vec3::getZero();
                Vec3 ignoredVertical = Vec3::getZero();
                calcArtilleryAngles(from, deviatedTarget, firearm->getVelocity(), gravity,
                                    it->horizontalTurnObject, it->verticalTurnObject,
                                    projectileDirection, ignoredHorizontal, ignoredVertical);
                weapon->setProjectileDeviationFixed(projectileDirection, projectile);
            }

            m_missionActive = true;
        }
    }

    return successes == attempts;
}

bool ArtilleryHandler::calcArtilleryAngles(Vec3& from, Vec3& to, float velocity, float gravity,
                                            RemoteControlledObject* horizontalTurnObject,
                                            RemoteControlledObject* verticalTurnObject,
                                            Vec3& projectileDirection,
                                            Vec3& horizontalDirection,
                                            Vec3& verticalDirection)
{
    Vec3 direction;
    direction.x = to.x - from.x;
    direction.y = to.y - from.y;
    direction.z = to.z - from.z;
    normalizeRetail(direction);

    const float height = std::fabs(to.y - from.y);
    const float deltaX = to.x - from.x;
    const float deltaZ = to.z - from.z;
    const float horizontalDistance = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
    const float quarterPi = 0.78539819f;
    if (horizontalDistance >
        calcParabellicXDistance(gravity, velocity, quarterPi, height))
        return false;

    const float velocitySquared = velocity * velocity;
    const float factor = 2.0f * velocitySquared / (gravity * horizontalDistance);
    const float halfFactor = 0.5f * factor;
    const float launchAngle = std::atan(
        -halfFactor + std::sqrt(halfFactor * halfFactor -
            (velocitySquared * (-2.0f * height) /
                 (horizontalDistance * horizontalDistance * gravity) + 1.0f)));
    (void)calcParabellicXDistance(gravity, velocity, launchAngle, height);

    const Mat4& horizontalTransform =
        horizontalTurnObject->m_owner->getAbsoluteTransformation();
    const float scale = std::sqrt(
        (direction.y * direction.y - horizontalTransform.m[9] * horizontalTransform.m[9]) /
            (direction.x * direction.x + direction.z * direction.z) + 1.0f);
    horizontalDirection.x = direction.x * scale;
    horizontalDirection.y = horizontalTransform.m[9];
    horizontalDirection.z = direction.z * scale;

    const Mat4& verticalTransform =
        verticalTurnObject->m_owner->getAbsoluteTransformation();
    verticalDirection.x = verticalTransform.m[8];
    verticalDirection.y = verticalTransform.m[9];
    verticalDirection.z = verticalTransform.m[10];
    verticalDirection.y = std::tan(launchAngle) *
        std::sqrt(verticalDirection.x * verticalDirection.x +
                  verticalDirection.z * verticalDirection.z);
    normalizeRetail(verticalDirection);

    float cosine = direction.z /
        std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (cosine < -1.0f)
        cosine = -1.0f;
    else if (cosine > 1.0f)
        cosine = 1.0f;
    float yaw = std::acos(cosine);
    if (direction.x < 0.0f)
        yaw = 6.2831855f - yaw;

    Mat4 projectileTransform = Mat4::getIdentity();
    setRotationZXY(projectileTransform, 0.0f,
                   launchAngle * 57.295776f, -yaw * 57.295776f);
    projectileDirection.x = projectileTransform.m[8];
    projectileDirection.y = projectileTransform.m[9];
    projectileDirection.z = projectileTransform.m[10];
    return true;
}

void ArtilleryHandler::requestFire()
{
    // BF2.exe 0x5C5479..0x5C5489 / ELF requestFire export 0x0816DC00:
    // both tick differences use unsigned arithmetic, including a future fire tick.
    if (static_cast<unsigned int>(WorldPref::getTickCount()) -
            static_cast<unsigned int>(m_fireAtTick) <
        static_cast<unsigned int>(m_minIntervalTicks) -
            static_cast<unsigned int>(m_fireDelayTicks))
    {
        failedFire();
        return;
    }

    m_firePending = true;
    m_fireAtTick = WorldPref::getTickCount() + m_fireDelayTicks;
    IObject* target = m_targetObject.get();
    if (target)
        reportFire(target->getAbsolutePosition(), true);
}

void ArtilleryHandler::failedFire()
{
    const Vec3 zero = Vec3::getZero();
    reportFire(zero, false);
}

void ArtilleryHandler::reportFire(const Vec3& position, bool accepted)
{
    IPlayer* const localHuman = playerManager->getLocalHumanPlayer();
    const std::list<IPlayer*>& players = playerManager->getPlayers(m_team);

    ArtilleryHudPayload payload;
    payload.position = position;
    payload.accepted = accepted;
    for (std::list<IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
#if !DEDICATED
        // BF2.exe 0x5C4C03..0x5C4C40: the host's HUD receives the report
        // locally; other players retain the connected-client event path.
        if (!isDedicated() && (!localHuman || (*it)->getId() == localHuman->getId()))
        {
            if (!isDedicated())
                eventManager->postEvent(ECHud, 42u, &payload, 0.0f);
            continue;
        }
#endif
        PostRemoteEvent* event =
            new PostRemoteEvent(ECHud, 42u, 0, &payload, 16u, 0.0f);
        game->sendGameEventIfConnected((*it)->getId(), event);
    }
}

void ArtilleryHandler::doFire()
{
    if (m_missionActive)
    {
        m_firePending = true;
        return;
    }

    bool firedAny = false;
    const int tick = WorldPref::getTickCount();
    int index = 0;
    for (std::list<ArtilleryObjectStruct>::iterator it = m_artillery.begin();
         it != m_artillery.end(); ++it, ++index)
    {
        RemoteControlledObject* artillery = it->artillery;
        if (!it->missionFailed &&
            gameLogic->getRemoteControlledObject(artillery->m_objectId))
        {
            IObject* owner = artillery->m_owner;
            IArmorGLComp* armor = owner ? armorOf(owner) : 0;
            if (!owner || !armor || !armor->isDestroyed())
            {
                firedAny = true;
                GenericFireArm* firearm =
                    static_cast<GenericFireArm*>(artillery->m_weapon);
                firearm->m_dword1C4 = index + tick;

                IObject* root = getRootParent(artillery);
                io::INetworkable* networkable = root->getNetworkable();
                if (networkable)
                {
                    ArtilleryEvent* event = new ArtilleryEvent();
                    event->m_team = 0;
                    event->m_fireTick = index + tick;
                    event->m_artilleryType = ArtilleryEvent::ArtilleryTypeDidFire;
                    event->m_weaponNetId = networkable->m_networkableId;
                    game->sendGameEventToAllConnected(event);
                }

                const int commanderId =
                    static_cast<ISquadManager*>(squadManager)->getCommander(m_team);
                IPlayer* commander = playerManager->getPlayerFromId(commanderId);
                firearm->enableWeapon();
                firearm->setPlayer(commander);
                firearm->m_byte208 = true;
                firearm->handleMessage(
                    static_cast<TemplateMessage>(WeaponMessageFire), 0, false);
            }
        }
    }

    if (!firedAny)
        failedFire();
}

bool ArtilleryHandler::haveAnyArtillery()
{
    int count = 0;
    for (std::list<ArtilleryObjectStruct>::iterator it = m_artillery.begin();
         it != m_artillery.end(); ++it)
    {
        RemoteControlledObject* artillery = it->artillery;
        if (artillery && artillery->m_owner)
        {
            IArmorGLComp* armor = armorOf(artillery->m_owner);
            if (armor && !armor->isDestroyed())
                ++count;
        }
    }
    return count > 0;
}

} // namespace world
} // namespace hfe
} // namespace dice
