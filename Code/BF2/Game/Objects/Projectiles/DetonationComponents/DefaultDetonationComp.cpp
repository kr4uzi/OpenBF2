// Default projectile detonation and impact-effect handling.

#include "DefaultDetonationComp.h"

#include <vector>

#include "../ProjectileCompTemplates.h"
#include "../../IObject.h"
#include "../../IObjectTemplate.h"
#include "../../../../Core/Ids.h"
#include "../../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include <cmath>
#include <cfloat>                                       // FLT_EPSILON - makeScript's radius guard
#include <sstream>                                      // makeScript's ostringstream
#include <string>
#include "../../../../IO/Stream/IStream.h"             // makeScript's sink
#include "../GenericProjectile.h"
#include "../../ObjectLayoutAccess.h"                   // the projectile's +0x44 physics node
#include "../../../Utility/Vars.h"                      // the five detonation console tunables
#include "../../../IGameLogic.h"
#include "../../../../Core/System.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Platform/X87Compat.h"
#include "../../../../Physics/HeightmapCluster.h"
#include "../../../../Physics/IPhysicsNode.h"
#include "../../../../Physics/Material.h"
#include "../../../../Physics/MaterialManager.h"
#include "../../../../Physics/ICollisionMesh.h"
#include "../../../../Math/LocalBoundingBox.h"
#include "../../../../Scene/Object/ObjectManager.h"
#include "../../../../Scene/Object/ObjectPredicator.h"
#include "../../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../IPlayerControlObjectTemplate.h"
#include "../../IPlayer.h"
#include "../../Game/Soldier.h"
#include "../../../GameServer/ServerInfo.h"
#include "../../../../Core/EventManager.h"
#include "../../../../AI/AIInterface/AIInstance.h"
#include "../../../Main/Game.h"
#include "../../Fx/FxTrace.h"                    // [TEMP] BF2_TRACE_FX - detonate / playDetonationEffect

namespace dice {
namespace hfe {
namespace world {

// Console tunables for mine polling and collision-face offset; values are accessed through Vars handles.
// Polling defaults to one sweep per 0.25 seconds.
float* const g_detonationPollInterval = Vars::getFloat("weapon-detonationTriggerFreq", 0.25f);            // ds:0x8AB0278
float* const g_detonationPushBase     = Vars::getFloat("detonation-collface-offset", 0.07f);              // ds:0x8AB027C
float* const g_detonationPushRange    = Vars::getFloat("detonation-collface-offset-speedmod", 0.05f);     // ds:0x8AB0280
float* const g_detonationSpeedCap     = Vars::getFloat("detonation-collface-offset-speedmod-max", 500.0f);// ds:0x8AB0284
int*   const g_detonationDebug        = Vars::getInt("weapon-detonationDebug", 0);                        // ds:0x8AB0288

// Both globals below are declared as `IBase*` in Core/Globals.cpp - MSVC encodes a data symbol's
// type into its decorated name, so they must be declared exactly as defined and downcast here.
extern IBase* materialManager;                       // ds:0x8AD2820
extern IBase* heightmapCluster;                      // ds:0x8ABAF24

// GenericProjectile is the only IProjectileObject implementor. This typed secondary-to-primary
// downcast is defined beside that class and replaces the ABI-specific +0x54 subtable call.
IPlayer* projectileOwner(void* projectile);

namespace {

inline MaterialManager* materials()
{
    return static_cast<MaterialManager*>(materialManager);
}

// THE heightmap slot needed care. The ELF calls slot 39 of the cluster's table; IHeightmapCluster
// numbers its own declarations from ELF slot 5 (the IBase quartet takes 0-3 and the Itanium
// destructor pair takes 4/5), so ELF 39 is this header's 34th declaration -
// getWaterLevelInWorldCoords(const Vec3&). A naive index would have landed five slots early on
// getUsePhongNormalInterpolation, which takes no argument and returns bool.
inline IHeightmapCluster* heightmap()
{
    return static_cast<IHeightmapCluster*>(heightmapCluster);
}

// it was a named seam for the projectile's +0x44 physics node, which is
// exactly what object_detail::LayoutAccess::physicsNode reads. Same resolution as the +0x1CC seam in
// DefaultCollisionComp: the field was already modelled, only this file had not been pointed at it.
inline IPhysicsNode* projectilePhysicsNode(IObject* projectile)
{
    return static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(projectile));
}

inline ICollisionMesh* collisionMesh(IObject* object)
{
    return static_cast<ICollisionMesh*>(object_detail::LayoutAccess::collisionMesh(object));
}

inline Vec3 transformPoint(const Mat4& transform, const Vec3& point)
{
    Vec3 result;
    result.x = transform.m[0] * point.x + transform.m[4] * point.y
             + transform.m[8] * point.z + transform.m[12];
    result.y = transform.m[1] * point.x + transform.m[5] * point.y
             + transform.m[9] * point.z + transform.m[13];
    result.z = transform.m[2] * point.x + transform.m[6] * point.y
             + transform.m[10] * point.z + transform.m[14];
    return result;
}

inline float vectorLength(const Vec3& value)
{
    return sqrtf(value.x * value.x + value.y * value.y + value.z * value.z);
}

// The same inlined Vec3 normaliser used by the shipped predicate: already-unit values are left
// untouched and degenerate values become exactly zero.
inline void normaliseIfNeeded(Vec3& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (fabsf(lengthSquared - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(lengthSquared) >= FLT_EPSILON)
        {
            const float inverseLength = 1.0f / sqrtf(lengthSquared);
            value.x *= inverseLength;
            value.y *= inverseLength;
            value.z *= inverseLength;
        }
        else
        {
            value.x = 0.0f;
            value.y = 0.0f;
            value.z = 0.0f;
        }
    }
}

} // namespace

// Lazily resolves both effect-template names into shared template caches and initializes detonation timers.
// Unresolved names stay null without a diagnostic and are retried by later component construction.
DefaultDetonationComp::DefaultDetonationComp(DefaultDetonationCompTemplate* objectTemplate)
    : m_refCount(1)
    , m_template(objectTemplate)
    , m_detonate(false)
    , m_pollTimer(0.0f)
    , m_float28(0.0f)
    , m_byte2C(false)
    , m_timeUntilCanDetonate(0.0f)
{
    ProjectileComp::m_refCount = 1;
    m_object = 0;
    m_initObject = 0;

    // THE lazy resolution, exactly as the ELF orders it: the second pair is tested first, and the
    // first pair only afterwards. Both go through objectTemplateManager's +0x24 slot.
    if (!m_template->getEndEffect())
        m_template->setEndEffect(objectTemplateManager->getTemplate(m_template->getEndEffectName()));
    if (!m_template->getWaterEndEffect())
        m_template->setWaterEndEffect(
            objectTemplateManager->getTemplate(m_template->getWaterEndEffectName()));

    m_pollTimer = *g_detonationPollInterval;       // the console var holds a float*
    m_float28 = m_template->getTriggerTime();
    m_timeUntilCanDetonate = m_template->getTimeUntilCanDetonate();
}

DefaultDetonationComp::~DefaultDetonationComp() {}

void DefaultDetonationComp::addRef() { ++m_refCount; }
int  DefaultDetonationComp::getRef() { return m_refCount; }

int DefaultDetonationComp::release()
{
    const int remaining = m_refCount - 1;
    const bool last = (m_refCount == 1);
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* DefaultDetonationComp::queryInterface(unsigned int iid) const
{
    DefaultDetonationComp* const self = const_cast<DefaultDetonationComp*>(this);

    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return static_cast<IObjectComponent*>(self);
    if (iid == IID_IProjectileComp || iid == IID_IProjectileDetonationComp)
        return static_cast<IProjectileDetonationComp*>(self);
    return 0;
}

unsigned int DefaultDetonationComp::getInterfaceID() const
{
    return IID_IProjectileDetonationComp;
}

bool DefaultDetonationComp::getDetonate() const { return m_detonate; }
void DefaultDetonationComp::setDetonate(bool value) { m_detonate = value; }

// the read surface
// Every one of these is a two-instruction pass-through in the ELF. The offsets are the recovered
// template offsets; the names come from the 56 attested `ObjectTemplate.detonation.*` console
// properties catalogued in the component console registrations.
bool  DefaultDetonationComp::getPlayMaterialEffectAtSurface() const { return m_template->getPlayMaterialEffectAtSurface(); }
bool  DefaultDetonationComp::hasExplosion() const                   { return m_template->getExplosionMaterial() != -1; }
int   DefaultDetonationComp::getExplosionMaterial() const           { return m_template->getExplosionMaterial(); }
float DefaultDetonationComp::getExplosionRadius() const             { return m_template->getExplosionRadius(); }
float DefaultDetonationComp::getExplosionInnerConeRadius() const    { return m_template->getExplosionInnerConeRadius(); }
float DefaultDetonationComp::getFlashbangRadiusWithNightVision() const { return m_template->getFlashbangRadiusWithNightVision(); }

float DefaultDetonationComp::getFlashbangBillboardMinAlpha() const     { return m_template->getFlashbangBillboardMinAlpha(); }
float DefaultDetonationComp::getFlashbangBillboardMaxAlpha() const     { return m_template->getFlashbangBillboardMaxAlpha(); }
float DefaultDetonationComp::getFlashbangBillboardMaxRampTime() const  { return m_template->getFlashbangBillboardMaxRampTime(); }
float DefaultDetonationComp::getFlashbangBillboardMinHoldTime() const  { return m_template->getFlashbangBillboardMinHoldTime(); }
float DefaultDetonationComp::getFlashbangBillboardMaxHoldTime() const  { return m_template->getFlashbangBillboardMaxHoldTime(); }
float DefaultDetonationComp::getFlashbangBillboardMinHealTime() const  { return m_template->getFlashbangBillboardMinHealTime(); }
float DefaultDetonationComp::getFlashbangBillboardMaxHealTime() const  { return m_template->getFlashbangBillboardMaxHealTime(); }

float DefaultDetonationComp::getFlashbangAdditiveGlowMinAlpha() const    { return m_template->getFlashbangAdditiveGlowMinAlpha(); }
float DefaultDetonationComp::getFlashbangAdditiveGlowMaxAlpha() const    { return m_template->getFlashbangAdditiveGlowMaxAlpha(); }
float DefaultDetonationComp::getFlashbangAdditiveGlowMaxRampTime() const { return m_template->getFlashbangAdditiveGlowMaxRampTime(); }
float DefaultDetonationComp::getFlashbangAdditiveGlowMinHoldTime() const { return m_template->getFlashbangAdditiveGlowMinHoldTime(); }
float DefaultDetonationComp::getFlashbangAdditiveGlowMaxHoldTime() const { return m_template->getFlashbangAdditiveGlowMaxHoldTime(); }
float DefaultDetonationComp::getFlashbangAdditiveGlowMinHealTime() const { return m_template->getFlashbangAdditiveGlowMinHealTime(); }
float DefaultDetonationComp::getFlashbangAdditiveGlowMaxHealTime() const { return m_template->getFlashbangAdditiveGlowMaxHealTime(); }

float DefaultDetonationComp::getFlashbangMotionBlurMinAlpha() const    { return m_template->getFlashbangMotionBlurMinAlpha(); }
float DefaultDetonationComp::getFlashbangMotionBlurMaxAlpha() const    { return m_template->getFlashbangMotionBlurMaxAlpha(); }
float DefaultDetonationComp::getFlashbangMotionBlurMaxRampTime() const { return m_template->getFlashbangMotionBlurMaxRampTime(); }
float DefaultDetonationComp::getFlashbangMotionBlurMinHoldTime() const { return m_template->getFlashbangMotionBlurMinHoldTime(); }
float DefaultDetonationComp::getFlashbangMotionBlurMaxHoldTime() const { return m_template->getFlashbangMotionBlurMaxHoldTime(); }
float DefaultDetonationComp::getFlashbangMotionBlurMinHealTime() const { return m_template->getFlashbangMotionBlurMinHealTime(); }
float DefaultDetonationComp::getFlashbangMotionBlurMaxHealTime() const { return m_template->getFlashbangMotionBlurMaxHealTime(); }

float DefaultDetonationComp::getExplosionSoldierLineOfSight() const      { return m_template->getExplosionSoldierLineOfSight(); }
float DefaultDetonationComp::getExplosionSoldierLineOfSightTimer() const { return m_template->getExplosionSoldierLineOfSightTimer(); }
float DefaultDetonationComp::getExplosionSoldierLineOfSightHealRate() const { return m_template->getExplosionSoldierLineOfSightHealRate(); }
float DefaultDetonationComp::getExplosionLineOfSightMinimumDamage() const  { return m_template->getExplosionLineOfSightMinimumDamage(); }
float DefaultDetonationComp::getExplosionConeAngle() const              { return m_template->getExplosionConeAngle(); }
float DefaultDetonationComp::getExplosionYMod() const                   { return m_template->getExplosionYMod(); }
float DefaultDetonationComp::getExplosionForce() const                  { return m_template->getExplosionForce(); }
float DefaultDetonationComp::getExplosionDamage() const                 { return m_template->getExplosionDamage(); }
float DefaultDetonationComp::getDetectionRadius() const                 { return m_template->getDetectionRadius(); }
bool  DefaultDetonationComp::getShowMineIndicationIcon() const          { return m_template->getShowMineIndicationIcon(); }
int   DefaultDetonationComp::getRadiusTriggerType() const               { return m_template->getRadiusTriggerType(); }
IObjectTemplate* DefaultDetonationComp::getEndEffect() const            { return m_template->getEndEffect(); }

// this ONE reads THE component, not the template.
float DefaultDetonationComp::getTimeUntilCanDetonate() const { return m_timeUntilCanDetonate; }

// [PLATFORM] Dedicated ELF 0x828F450 is empty. Client PE 0x617AE0 creates the effect,
// constructs its frame from the projectile and supplied normal, initializes, then starts it.
#if DEDICATED
void DefaultDetonationComp::playDetonationEffect(IObjectTemplate*, Vec3, Vec3, IPlayer*, int) {}
#else
void DefaultDetonationComp::playDetonationEffect(IObjectTemplate* effect, Vec3 position,
                                                Vec3 direction, IPlayer*, int)
{
    if (isDedicated())
        return;
    const Vec3 rotation = {m_template->getEffectRotation(0), m_template->getEffectRotation(1),
                           m_template->getEffectRotation(2)};
    if (vectorLength(rotation) > 0.0f)
    {
        Mat4 rotationMatrix = Mat4::getIdentity();
        setRotationZXY(rotationMatrix, -rotation.z, -rotation.y, -rotation.x);
        direction = transformPoint(rotationMatrix, direction);
    }
    if (effect)
    {
        IObject* object = effect->createObject();
        // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - FxTrace.h
        BF2_FX_TRACE("playDetonationEffect: %s createObject -> %s", effect->getName().c_str(),
                     object ? "object" : "NULL");
        if (object)
        {
            Mat4 frame = m_initObject->getAbsoluteTransformation();
            frame.m[12] = position.x; frame.m[13] = position.y; frame.m[14] = position.z;
            frame.m[4] = direction.x; frame.m[5] = direction.y; frame.m[6] = direction.z;
            Vec3 right = {direction.y*frame.m[10] - direction.z*frame.m[9],
                          direction.z*frame.m[8] - direction.x*frame.m[10],
                          direction.x*frame.m[9] - direction.y*frame.m[8]};
            normaliseIfNeeded(right);
            frame.m[0] = right.x; frame.m[1] = right.y; frame.m[2] = right.z;
            Vec3 forward = {right.y*direction.z - right.z*direction.y,
                            right.z*direction.x - right.x*direction.z,
                            right.x*direction.y - right.y*direction.x};
            normaliseIfNeeded(forward);
            frame.m[8] = forward.x; frame.m[9] = forward.y; frame.m[10] = forward.z;
            object->setAbsoluteTransformation(frame);
            object->init();
            object->handleMessage(static_cast<TemplateMessage>(0), 0, false);
            // Both class queries survive in the client body; player/material arguments are dead.
            const IObjectTemplate* createdTemplate = object_detail::LayoutAccess::objectTemplate(object);
            if (createdTemplate->getClassID() != CID_EffectBundleTemplate)
                createdTemplate->getClassID();
        }
    }
}
#endif

// hasUpdate - 0x828DEB0, 59 B. this IS WHY A grenade never ticks. Three template tests, any one
// of which arms the per-tick path: a proximity distance to a tracked target, a trigger radius, or an
// arming delay. A plain grenade or rocket sets none of them, never enters updateComponent, and
// detonates purely through the collision path.
bool DefaultDetonationComp::hasUpdate() const
{
    if (m_template->getDetonateDistanceToTarget() > 0.0f)
        return true;
    if (m_template->getTriggerRadius() > 0.0f)
        return true;
    return m_template->getTimeUntilCanDetonate() > 0.0f;
}

// . One store: a projectile coming out of the cache has not detonated.
void DefaultDetonationComp::activate()
{
    m_detonate = false;
}

// . The weak-handle assignment written out inline, exactly as StickyCollisionComp
// does it. A null target clears the handle rather than being ignored.
void DefaultDetonationComp::setTargetObject(IObject* target)
{
    m_targetHandle = target ? target->getWeakPtr() : WeakPtr<IObject>();
}

// Detonates unless depth exceeds the template limit, then applies collision-normal offset, explosion and effects.
// Preserve repeated player/team queries. Surface impact requires a fresh tick and eligible water/depth state;
// requested underwater surface impacts move to water level with vertical normal/direction.
// Dedicated builds omit the presentation effect.
void DefaultDetonationComp::detonate(IObject* source, int collisionType, Vec3 position,
                                     unsigned int tick, Vec3 normal, CollisionFaceInfo* face)
{
    IObject* const projectile = m_initObject;

    Material* const material =
        materials()->getMaterialPtr(static_cast<unsigned int>(collisionType));

    // The impact speed along the collision normal, negated - the physics node belongs to m_object.
    float alongNormal = 0.0f;
    IPhysicsNode* const node = m_object ? projectilePhysicsNode(m_object) : 0;
    if (node)
    {
        const Vec3& speed = node->getPositionalSpeed();
        alongNormal = -(speed.x * normal.x + speed.y * normal.y + speed.z * normal.z);
    }

    if (static_cast<GenericProjectile*>(projectile)->getUnderWater() >
            m_template->getMaxDepthForExplosion())
    {
        // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - FxTrace.h
        BF2_FX_TRACE("detonation refused (depth): %s underWater=%.2f maxDepth=%.2f",
                     projectile->getTemplate()->getName().c_str(),
                     static_cast<GenericProjectile*>(projectile)->getUnderWater(),
                     m_template->getMaxDepthForExplosion());
        return;
    }

    float t = fabsf(alongNormal) / *g_detonationSpeedCap;
    if (t > 1.0f)
        t = 1.0f;
    const float pushOut = t * *g_detonationPushRange + *g_detonationPushBase;
    position.x += normal.x * pushOut;
    position.y += normal.y * pushOut;
    position.z += normal.z * pushOut;

    m_detonate = true;

    const int explosionMaterial = getExplosionMaterial();

    if (hasExplosion())
    {
        // Made and discarded in the shipped body, then made again below. Preserved.
        static_cast<GenericProjectile*>(projectile)->getPlayer();
        static_cast<GenericProjectile*>(projectile)->getTeam();

        gameLogic->handleExplosion(
            position,
            getExplosionRadius(),
            explosionMaterial,
            getExplosionDamage(),
            static_cast<GenericProjectile*>(projectile)->getPlayer(),
            0,
            static_cast<GenericProjectile*>(projectile)->getTeam(),
            static_cast<GenericProjectile*>(projectile)->getLauncherWeapon(),
            static_cast<GenericProjectile*>(projectile)->getLauncherVehicle(),
            getExplosionForce(),
            projectile,
            source,
            getExplosionYMod(),
            static_cast<GenericProjectile*>(projectile)->canTK(),
            false);
    }

    // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - FxTrace.h. No dereference the stages below do
    // not already make: the material and the node are only reported as present or absent.
    BF2_FX_TRACE("detonation: %s hasExplosion=%d material=%d collisionType=%d useMM=%d "
                 "tickFresh=%d materialPtr=%d node=%d",
                 projectile->getTemplate()->getName().c_str(), hasExplosion() ? 1 : 0,
                 explosionMaterial, collisionType, m_template->getUseMMOnEndEffect() ? 1 : 0,
                 tick + 10 >= static_cast<unsigned int>(WorldPref::getTickCount()) ? 1 : 0,
                 material ? 1 : 0, node ? 1 : 0);

    // NaN depth passes the effect gate.
    if (m_template->getUseMMOnEndEffect() &&
        tick + 10 >= static_cast<unsigned int>(WorldPref::getTickCount()) &&
        (material->getHasWaterPhysics() || !(node->getUnderWater() > 0.5f)))
    {
        Vec3 impactPosition = position;
        Vec3 impactNormal = normal;

        // The projectile's third row, negated - the direction it was travelling.
        const Mat4& transform = projectile->getAbsoluteTransformation();
        Vec3 impactDirection;
        impactDirection.x = -transform.m[8];
        impactDirection.y = -transform.m[9];
        impactDirection.z = -transform.m[10];

        // THE splash relocation.
        if (node->getUnderWater() > 0.0f && m_template->getPlayMaterialEffectAtSurface())
        {
            impactPosition.y = heightmap()->getWaterLevelInWorldCoords(position);
            impactNormal.x = 0.0f;  impactNormal.y = 1.0f;  impactNormal.z = 0.0f;
            impactDirection.x = 0.0f; impactDirection.y = -1.0f; impactDirection.z = 0.0f;
        }

        // Preserve the unused transform query; impactDirection retains the selected impact/splash direction.
        projectile->getAbsoluteTransformation();

        // The template's `effectRotation` Vec3 at +0x74, handed straight to doImpact. Both
        // shipped console executors copy their Vec3 argument to these exact three floats.
        Vec3 effectRotation;
        effectRotation.x = m_template->getEffectRotation(0);
        effectRotation.y = m_template->getEffectRotation(1);
        effectRotation.z = m_template->getEffectRotation(2);

        IProjectileObject* const asProjectile = projectile
            ? static_cast<IProjectileObject*>(static_cast<GenericProjectile*>(projectile))
            : 0;

        gameLogic->doImpact(explosionMaterial, collisionType,
                            static_cast<MaterialCollisionType>(0),
                            impactPosition, impactNormal, impactDirection,
                            projectile, source, asProjectile,
                            face, effectRotation, true);
    }

    const Vec3& here = projectile->getAbsolutePosition();
    const float waterLevel = heightmap()->getWaterLevelInWorldCoords(here);
    IObjectTemplate* endEffect = m_template->getEndEffect();
    // Client PE 0x618264..0x61827E and ELF 0x828EEEE..0x828EFCE select water for
    // negative or unordered depth. The dedicated PE drops this inert effect selection.
    if (!bf2x87::greaterEqual(here.y - waterLevel, 0.0f) && m_template->getWaterEndEffect())
        endEffect = m_template->getWaterEndEffect();

    Vec3 effectDirection;
    // The template's `useCollisionNormal` byte at +0x71 selects the effect's orientation.
    if (m_template->getUseCollisionNormal())
    {
        effectDirection = normal;
    }
    else
    {
        const Mat4& transform = projectile->getAbsoluteTransformation();
        effectDirection.x = transform.m[8];
        effectDirection.y = transform.m[9];
        effectDirection.z = transform.m[10];
    }

    // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - FxTrace.h
    BF2_FX_TRACE("end effect: %s -> %s (%s) at (%.1f %.1f %.1f) dir=(%.2f %.2f %.2f)",
                 projectile->getTemplate()->getName().c_str(),
                 endEffect ? endEffect->getName().c_str() : "(null)",
                 m_template->getEndEffectName().c_str(), position.x, position.y, position.z,
                 effectDirection.x, effectDirection.y, effectDirection.z);
    playDetonationEffect(endEffect, position, effectDirection,
                         static_cast<GenericProjectile*>(projectile)->getPlayer(),
                         explosionMaterial);
}

// Counts down arming and poll timers, then checks tracked-target distance before nearby candidates.
// The debug path explodes every tick without setting m_detonate. Radius sweeps schedule message 31
// after the trigger delay, notify AI when present and latch the completed sweep.
void DefaultDetonationComp::updateComponent(float seconds, unsigned int)
{
    GenericProjectile* const projectile = static_cast<GenericProjectile*>(m_initObject);

    if (*g_detonationDebug)
    {
        gameLogic->handleExplosion(
            projectile->getAbsolutePosition(),
            getExplosionRadius(),
            -1,
            getExplosionDamage(),
            projectile->getPlayer(),
            0,
            projectile->getTeam(),
            projectile->getLauncherWeapon(),
            projectile->getLauncherVehicle(),
            getExplosionForce(),
            m_initObject,
            0,
            getExplosionYMod(),
            projectile->canTK(),
            false);
    }

    if (m_float28 > 0.0f)
        m_float28 -= seconds;
    if (m_timeUntilCanDetonate > 0.0f)
        m_timeUntilCanDetonate -= seconds;

    m_pollTimer -= seconds;
    if (m_pollTimer >= 0.0f)
        return;

    m_pollTimer = *g_detonationPollInterval;

    if (m_template->getDetonateDistanceToTarget() > 0.0f)
    {
        IObject* const target = m_targetHandle.get();
        if (target && m_timeUntilCanDetonate <= 0.0f)
        {
            const Vec3& projectilePosition = m_initObject->getAbsolutePosition();
            const Vec3& targetPosition = target->getAbsolutePosition();
            Vec3 distance;
            distance.x = targetPosition.x - projectilePosition.x;
            distance.y = targetPosition.y - projectilePosition.y;
            distance.z = targetPosition.z - projectilePosition.z;
            if (vectorLength(distance) < m_template->getDetonateDistanceToTarget())
            {
                Vec3 normal = { 0.0f, 0.0f, 0.0f };
                detonate(0, m_initObject->getMaterial(), projectilePosition, 0, normal, 0);
                return;
            }
        }
    }

    const float triggerRadius = m_template->getTriggerRadius();
    if (triggerRadius != 0.0f)
    {
        std::vector<IObject*> candidates;
        ObjectInterfacePredicator predicator(0x1000108u, IID_IPlayerControlObject);
        objectManager->getObjectsFromGrid(
            m_initObject->getAbsolutePosition(), triggerRadius, candidates, &predicator);

        for (std::vector<IObject*>::iterator it = candidates.begin(); it != candidates.end(); ++it)
        {
            IObject* candidate = *it;
            if (candidate->getTemplate()->getClassID() == CID_LadderContainerTemplate)
            {
                IPlayerControlObject* const ladder = static_cast<IPlayerControlObject*>(
                    candidate->queryInterface(IID_IPlayerControlObject));
                candidate = ladder->getOccupingObject();
            }

            if (!checkIfObjectCanTrigger(candidate))
                continue;

            if (m_float28 <= 0.0f)
                objectManager->postMessage(m_initObject, static_cast<TemplateMessage>(31), 0.0f);

            if (ai::g_aiInstance)
            {
                unsigned int objectId = m_initObject->getObjectId();
                eventManager->postEvent(ECAI, 21, &objectId, 0.0f);
            }
            break;
        }

        m_byte2C = true;
    }
}

// checkIfObjectCanTrigger - ELF 0x828E390, 2,162 B / PE 0x5B6B10, 1,240 B. This is the complete
// mine/claymore proximity predicate: trigger-family filters, arming and friendly-mine rules,
// angular cone, local mine bounds, airborne policy, two-stage soldier LOS fallback, and minimum
// victim speed. Repeated template/player/ray calls that are observable in retail are preserved.
bool DefaultDetonationComp::checkIfObjectCanTrigger(IObject* candidate)
{
    if (!candidate)
        return false;

    const bool hasPlayerControlTemplate =
        candidate->getTemplate()->queryInterface(IID_IPlayerControlObjectTemplate) != 0;
    IPhysicsNode* const node = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(candidate));
    if (!node)
        return false;

    const int triggerType = m_template->getRadiusTriggerType();
    if (triggerType == 0)
        return false;

    if (triggerType == 2 && hasPlayerControlTemplate
        && candidate->getTemplate()->getClassID() == CID_ParachuteTemplate)
    {
        return false;
    }
    if (m_template->getRadiusTriggerType() == 1
        && candidate->getTemplate()->getClassID() != CID_SoldierTemplate)
    {
        return false;
    }
    if (m_template->getRadiusTriggerType() == 2
        && (candidate->getTemplate()->getClassID() == CID_SoldierTemplate
            || (m_template->getRadiusTriggerType() == 2
                && candidate->getTemplate()->getClassID() == CID_LadderContainerTemplate)))
    {
        return false;
    }

    if (getTimeUntilCanDetonate() > 0.0f)
        return false;

    IPlayerControlObject* const playerControl = static_cast<IPlayerControlObject*>(
        candidate->queryInterface(IID_IPlayerControlObject));
    if (playerControl && playerControl->getOccupingPlayer())
    {
        IPlayer* const player = playerControl->getOccupingPlayer();
        bool testFriendlyOwner = player->getIsAIPlayer();
        if (!testFriendlyOwner && serverInfo && !serverInfo->m_block.friendlyFireWithMines
            && m_template->getAllowFriendlyFireDetonation())
        {
            testFriendlyOwner = true;
        }

        if (testFriendlyOwner)
        {
            // No m_object guard in either shipped port: component initialisation guarantees it.
            void* const projectileFace = m_object->queryInterface(IID_IProjectileObject);
            if (projectileFace)
            {
                IPlayer* const owner = projectileOwner(projectileFace);
                if (owner && owner->getTeam() == playerControl->getOccupingPlayer()->getTeam())
                    return false;
            }
        }
    }

    const float triggerAngle = m_template->getTriggerAngle();
    if (triggerAngle > 0.0f)
    {
        const Vec3 candidatePosition = candidate->getAbsolutePosition();
        (void)m_initObject->getAbsolutePosition();             // made and discarded in retail
        const Vec3 local = transformPoint(
            m_initObject->getInvAbsoluteTransformation(), candidatePosition);

        if (!candidate->hasCollisionLod(1) || local.z < 0.0f)
            return false;

        ICollisionMesh* const mesh = collisionMesh(candidate);
        const LocalBoundingBox* const firstBox = mesh->getBoundingBox(1);
        const Vec3& firstSize = const_cast<LocalBoundingBox*>(firstBox)->getSize();
        const float radians = triggerAngle * 0.017453292f;
        if (fabsf(local.x) - firstSize.x * 0.5f
            > tanf(radians) * fabsf(local.z))
        {
            return false;
        }

        const LocalBoundingBox* const secondBox = mesh->getBoundingBox(1);
        const Vec3& secondSize = const_cast<LocalBoundingBox*>(secondBox)->getSize();
        if (fabsf(local.y) - secondSize.y * 0.5f
            > fabsf(tanf(radians) * local.z))
        {
            return false;
        }
    }

    if (candidate->getTemplate()->getClassID() != CID_SoldierTemplate)
    {
        const Vec3 projectilePosition = m_initObject->getAbsolutePosition();
        const Vec3 candidatePosition = candidate->getAbsolutePosition();
        Vec3 direction;
        direction.x = candidatePosition.x - projectilePosition.x;
        direction.y = candidatePosition.y - projectilePosition.y;
        direction.z = candidatePosition.z - projectilePosition.z;
        const float distance = vectorLength(direction);
        normaliseIfNeeded(direction);

        float reach = m_template->getTriggerRadius();
        if (reach > distance)
            reach = distance;
        Vec3 endpoint;
        endpoint.x = projectilePosition.x + direction.x * reach;
        endpoint.y = projectilePosition.y + direction.y * reach;
        endpoint.z = projectilePosition.z + direction.z * reach;
        const Vec3 local = transformPoint(candidate->getInvAbsoluteTransformation(), endpoint);

        LocalBoundingBox* const box = candidate->getMineBoundingBox();
        if (local.x < box->m_min.x || local.y < box->m_min.y || local.z < box->m_min.z
            || box->m_max.x < local.x || box->m_max.y < local.y || box->m_max.z < local.z)
        {
            return false;
        }
        if (!m_template->getRadiusDetonateWithAirVehicles() && node->getIsInAir())
            return false;
    }

    const Vec3 candidatePosition = candidate->getAbsolutePosition();
    const Vec3 projectilePosition = m_initObject->getAbsolutePosition();
    IObject* hit = checkIfRayHitAnyObject(
        projectilePosition, candidatePosition, m_initObject, false);
    if (hit && hit != candidate)
    {
        if (candidate->getTemplate()->getClassID() != CID_SoldierTemplate)
            return false;

        const Vec3 rayPoint = static_cast<Soldier*>(candidate)->getSkeletonRayTestPoint(
            Soldier::SkeletonRayTestPointFeet);
        hit = checkIfRayHitAnyObject(projectilePosition, rayPoint, 0, false);
        if (hit != candidate)
            return false;
    }

    return vectorLength(node->getAbsolutePositionalSpeed())
        >= m_template->getTriggerVictimMinSpeed();
}

// Default detonation template settings, component construction and serialization.

// . Every default below is the constructor's own store, cross-checked field by
// field against the 45 pass-through getters on the component.
DefaultDetonationCompTemplate::DefaultDetonationCompTemplate()
    : m_refCount(1)
    , m_endEffect(0)
    , m_waterEndEffect(0)
    , m_playMaterialEffectAtSurface(false)
    , m_explosionMaterial(-1)                    // hasExplosion() is `!= -1`, so a template that
                                                 // never authors a material never explodes
    , m_explosionForce(150.0f)
    , m_explosionYMod(1.0f)
    , m_explosionRadius(10.0f)
    , m_explosionInnerConeRadius(0.0f)
    , m_flashbangRadiusWithNightVision(0.0f)
    , m_explosionSoldierLineOfSight(0.0f)
    , m_explosionSoldierLineOfSightTimer(0.0f)
    , m_explosionSoldierLineOfSightHealRate(0.0005f)
    , m_explosionLineOfSightMinimumDamage(0.3f)
    , m_explosionConeAngle(0.0f)
    , m_explosionDamage(500.0f)
    , m_maxDepthForExplosion(2.0f)
    , m_triggerRadius(0.0f)
    , m_detonateDistanceToTarget(0.0f)
    , m_minDetonationSpeed(0.0f)
    , m_detectionRadius(1.5f)
    , m_radiusDetonateWithAirVehicles(false)
    , m_showMineIndicationIcon(false)
    , m_useMMOnEndEffect(false)
    , m_triggerVictimMinSpeed(0.0f)
    , m_radiusTriggerType(0)
    , m_detonateOnWaterCollision(true)           // the only two bytes that start true
    , m_useCollisionNormal(true)
    , m_triggerAngle(0.0f)
    , m_triggerTime(0.0f)
    , m_allowFriendlyFireDetonation(false)
    , m_timeUntilCanDetonate(0.0f)
{
    m_effectRotation[0] = 0.0f;
    m_effectRotation[1] = 0.0f;
    m_effectRotation[2] = 0.0f;

    // 21 floats, three channels of seven: billboard, additive glow, motion blur. Every value is
    // the constructor's own; the server never reads any of them, but each is authored per weapon.
    static const float kFlashbangDefaults[21] = {
        0.5f, 1.0f, 0.12f, 2.0f,  6.0f, 2.0f,  6.0f,     // billboard
        0.5f, 1.0f, 0.02f, 1.0f,  5.0f, 1.0f,  5.0f,     // additive glow
        0.96f, 1.0f, 0.02f, 2.0f, 10.0f, 2.0f, 10.0f     // motion blur
    };
    for (int i = 0; i < 21; ++i)
        m_flashbang[i] = kFlashbangDefaults[i];
}

void DefaultDetonationCompTemplate::addRef() { ++m_refCount; }
int  DefaultDetonationCompTemplate::getRef() { return m_refCount; }

// . Same shape as the collision template's: returns the decremented count and deletes at
// zero, reading the value out of the register the decrement left it in.
int DefaultDetonationCompTemplate::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        ::operator delete(this);
    return remaining;
}

// . Identical IID set to DefaultCollisionCompTemplate: three answer with the primary
// address, IConsoleSaveable with the second base at +4.
void* DefaultDetonationCompTemplate::queryInterface(unsigned int iid) const
{
    DefaultDetonationCompTemplate* const self =
        const_cast<DefaultDetonationCompTemplate*>(this);

    if (iid == IID_IBase
        || iid == IID_IObjectTemplateComponent
        || iid == IID_IProjectileCompTemplate)
    {
        return self;
    }
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int DefaultDetonationCompTemplate::getClassID() const
{
    return CID_DefaultDetonationCompTemplate;
}

unsigned int DefaultDetonationCompTemplate::getInterfaceID()
{
    return IID_IProjectileDetonationCompTemplate;
}

// . `operator new(0x34)` - the component's proven 52 bytes.
IObjectComponent* DefaultDetonationCompTemplate::createComponent()
{
    return new DefaultDetonationComp(this);
}

void DefaultDetonationCompTemplate::objectCreated(IObject*) {}             // empty
void DefaultDetonationCompTemplate::objectDestroyed(IObject*) {}           // empty
void DefaultDetonationCompTemplate::setTemplate(IObjectTemplate*) {}       // empty

// . The same self-querying form the collision template uses - the class id comes back
// through the vtable, not from a constant.
void* DefaultDetonationCompTemplate::queryImplementation(unsigned int classId)
{
    return classId == getClassID() ? this : 0;
}

// the four appended virtual accessors
void  DefaultDetonationCompTemplate::setDetectionRadius(float value)
{
    m_detectionRadius = value;
}
float DefaultDetonationCompTemplate::getDetectionRadius() const
{
    return m_detectionRadius;
}
void  DefaultDetonationCompTemplate::setShowMineIndicationIcon(bool value)
{
    m_showMineIndicationIcon = value;
}
bool  DefaultDetonationCompTemplate::getShowMineIndicationIcon() const
{
    return m_showMineIndicationIcon;
}

// Angle setters store degrees, mapping negatives and values >=180 to zero.
// Preserve the upper-bound reset; these setters do not store cosines.
namespace {
// The clamp both setters share, written once. `lower` is the ELF's second temporary; it is only
// ever 0.0f, and it is what turns a negative angle into zero.
inline float clampAuthoredAngle(float degrees)
{
    float lower = 0.0f;
    float value = degrees;
    if (degrees >= 180.0f)
    {
        value = 0.0f;                            // zero, not 180 - see the block above
        lower = 0.0f;
    }
    if (lower > value)
        value = lower;
    return value;
}
} // namespace

void DefaultDetonationCompTemplate::setExplosionConeAngle(float degrees)
{
    m_explosionConeAngle = clampAuthoredAngle(degrees);                    // +0x48
}

void DefaultDetonationCompTemplate::setTriggerAngle(float degrees)
{
    m_triggerAngle = clampAuthoredAngle(degrees);                          // +0x80
}

// Serializes nondefault detonation settings, retaining swapped depth/radius comparison defaults.
// Trigger type, victim speed and air-vehicle behavior serialize only inside the trigger-radius guard.
// Effect rotation uses slash-separated components.
bool DefaultDetonationCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;

    if (m_explosionMaterial != -1)
        script << "ObjectTemplate.detonation.explosionMaterial " << m_explosionMaterial << std::endl;
    if (m_explosionRadius != 10.0f)
        script << "ObjectTemplate.detonation.explosionRadius " << m_explosionRadius << std::endl;
    if (m_explosionInnerConeRadius != 0.0f)
        script << "ObjectTemplate.detonation.explosionInnerConeRadius " << m_explosionInnerConeRadius << std::endl;
    if (m_explosionSoldierLineOfSight != 0.0f)
        script << "ObjectTemplate.detonation.explosionSoldierLineOfSight " << m_explosionSoldierLineOfSight << std::endl;
    if (m_explosionSoldierLineOfSightTimer != 0.0f)
        script << "ObjectTemplate.detonation.explosionSoldierLineOfSightTimer " << m_explosionSoldierLineOfSightTimer << std::endl;
    if (m_explosionSoldierLineOfSightHealRate != 0.0005f)
        script << "ObjectTemplate.detonation.explosionSoldierLineOfSightHealRate " << m_explosionSoldierLineOfSightHealRate << std::endl;
    if (m_explosionLineOfSightMinimumDamage != 0.3f)
        script << "ObjectTemplate.detonation.explosionLineOfSightMinimumDamage " << m_explosionLineOfSightMinimumDamage << std::endl;
    if (m_explosionConeAngle != 0.0f)
        script << "ObjectTemplate.detonation.explosionConeAngle " << m_explosionConeAngle << std::endl;
    if (m_explosionYMod != 1.0f)
        script << "ObjectTemplate.detonation.explosionYMod " << m_explosionYMod << std::endl;
    if (m_explosionForce != 150.0f)
        script << "ObjectTemplate.detonation.explosionForce " << m_explosionForce << std::endl;
    if (m_explosionDamage != 500.0f)
        script << "ObjectTemplate.detonation.explosionDamage " << m_explosionDamage << std::endl;
    if (m_detonateDistanceToTarget != 0.0f)
        script << "ObjectTemplate.detonation.detonateDistanceToTarget " << m_detonateDistanceToTarget << std::endl;
    if (m_minDetonationSpeed != 0.0f)
        script << "ObjectTemplate.detonation.minDetonationSpeed " << m_minDetonationSpeed << std::endl;
    if (!m_detonateOnWaterCollision)             // emitted only when false, and prints the field
        script << "ObjectTemplate.detonation.detonateOnWaterCollision " << m_detonateOnWaterCollision << std::endl;
    if (!m_endEffectName.empty())
        script << "ObjectTemplate.detonation.endEffectTemplate " << m_endEffectName << std::endl;
    if (!m_waterEndEffectName.empty())
        script << "ObjectTemplate.detonation.waterEndEffectTemplate " << m_waterEndEffectName << std::endl;
    if (m_useMMOnEndEffect)
        script << "ObjectTemplate.detonation.useMMOnEndEffect " << m_useMMOnEndEffect << std::endl;

    // the three nested lines - see the header block.
    if (m_triggerRadius != 0.0f)
    {
        script << "ObjectTemplate.detonation.triggerRadius " << m_triggerRadius << std::endl;
        if (m_radiusTriggerType)
            // calls the RadiusTriggerType writer, not ostream's integer overload.
            script << "ObjectTemplate.detonation.triggerType " << static_cast<RadiusTriggerType>(m_radiusTriggerType) << std::endl;
        if (m_triggerVictimMinSpeed != 0.0f)
            script << "ObjectTemplate.detonation.triggerVictimMinSpeed " << m_triggerVictimMinSpeed << std::endl;
        if (m_radiusDetonateWithAirVehicles)
            script << "ObjectTemplate.detonation.radiusDetonateWithAirVehicles " << m_radiusDetonateWithAirVehicles << std::endl;
    }

    // compared against 2.0f while the constructor seeds 1.5f - see the header block.
    if (fabsf(m_detectionRadius - 2.0f) > FLT_EPSILON)
        script << "ObjectTemplate.detonation.detectionRadius " << m_detectionRadius << std::endl;

    if (m_showMineIndicationIcon)
        script << "ObjectTemplate.detonation.showMineIndicationIcon 1" << std::endl;

    if (sqrtf(m_effectRotation[0] * m_effectRotation[0]
            + m_effectRotation[1] * m_effectRotation[1]
            + m_effectRotation[2] * m_effectRotation[2]) > 0.0f)
    {
        script << "ObjectTemplate.detonation.effectRotation "
               << m_effectRotation[0] << '/' << m_effectRotation[1] << '/' << m_effectRotation[2]
               << std::endl;
    }

    if (!m_useCollisionNormal)
        script << "ObjectTemplate.detonation.useCollisionNormal 0" << std::endl;
    if (m_triggerAngle != 0.0f)
        script << "ObjectTemplate.detonation.triggerAngle " << m_triggerAngle << std::endl;

    // compared against 1.5f while the constructor seeds 2.0f - the mirror of the defect above.
    if (m_maxDepthForExplosion != 1.5f)
        script << "ObjectTemplate.detonation.maxDepthForExplosion " << m_maxDepthForExplosion << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
