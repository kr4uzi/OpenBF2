// Latches a collision target, then attaches and broadcasts on the next update.

#include "StickyCollisionComp.h"

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>

#include "../ProjectileCompTemplates.h"
#include "../GenericProjectile.h"
#include "../../IObject.h"
#include "../../IObjectTemplate.h"
#include "../../ObjectLayoutAccess.h"
#include "../../Game/StickyProjectileManager.h"
#include "../../../GameEvents/StickyProjectileEvent.h"
#include "../../../IGame.h"
#include "../../../IGameLogic.h"
#include "../../../Utility/Vars.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Math/LocalBoundingBox.h"
#include "../../../../Physics/ICollisionMesh.h"
#include "../../../../Physics/IPhysicsNode.h"
#include "../../../../Physics/Material.h"
#include "../../../../Physics/MaterialManager.h"
#include "../../../../Scene/Object/ObjectUtil.h"

namespace dice {
namespace hfe {
namespace world {

// Defined as `IBase*` in Core/Globals.cpp - MSVC encodes a data symbol's type into its decorated
// name, so it must be declared exactly as defined and downcast at the use site.
extern IBase* materialManager;                       // ds:0x8AD2820

namespace {

inline MaterialManager* materials()
{
    return static_cast<MaterialManager*>(materialManager);
}

// The ELF static initializer at 0x828D7F0 stores this live cvar handle at ds:0x8AB01CC. The
// collision body dereferences it on every hit, so later console changes take effect immediately.
float* const stickDefaultDistance = Vars::getFloat("stick-default-distance", 0.05f);

inline IPhysicsNode* physicsNode(IObject* object)
{
    return static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(object));
}

void normaliseInPlace(Vec3& v)
{
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (fabsf(lengthSq - 1.0f) < FLT_EPSILON)
        return;
    if (fabsf(lengthSq) < FLT_EPSILON)
    {
        v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;
        return;
    }
    const float inverse = 1.0f / sqrtf(lengthSq);
    v.x *= inverse; v.y *= inverse; v.z *= inverse;
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    Vec3 out;
    out.x = a.y * b.z - a.z * b.y;
    out.y = a.z * b.x - a.x * b.z;
    out.z = a.x * b.y - a.y * b.x;
    return out;
}

} // namespace

// StickyCollisionComp::StickyCollisionComp - 0x828B850, 265 B. Most of its length is the Mat4: the
// shipped code zeroes all sixteen words and then copies Mat4::getIdentity() in row by row.
StickyCollisionComp::StickyCollisionComp(StickyCollisionCompTemplate* objectTemplate)
    : m_refCount(1)
    , m_template(objectTemplate)
    , m_dword1C(0)
    , m_pendingStick(false)
    , m_physicsDisabled(false)
{
    ProjectileComp::m_refCount = 1;
    m_object = 0;
    m_initObject = 0;
    m_stickTransform = Mat4::getIdentity();
}

// BF2.exe 0x617210 / dedicated PE 0x5B66F0 / ELF 0x828BA70: unregister before
// releasing the host handle, including destruction that did not pass through resetProjectile.
StickyCollisionComp::~StickyCollisionComp()
{
    if (m_initObject && stickyProjectileManager)
        stickyProjectileManager->removeStickyProjectile(
            static_cast<GenericProjectile*>(m_initObject));
}

void StickyCollisionComp::addRef() { ++m_refCount; }
int  StickyCollisionComp::getRef() { return m_refCount; }

int StickyCollisionComp::release()
{
    const int remaining = m_refCount - 1;
    const bool last = (m_refCount == 1);
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* StickyCollisionComp::queryInterface(unsigned int iid) const
{
    StickyCollisionComp* const self = const_cast<StickyCollisionComp*>(this);

    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return static_cast<IObjectComponent*>(self);
    if (iid == IID_IProjectileComp || iid == IID_IProjectileCollisionComp)
        return static_cast<IProjectileCollisionComp*>(self);
    return 0;
}

unsigned int StickyCollisionComp::getInterfaceID() const
{
    return IID_IProjectileCollisionComp;
}

// true, unlike DefaultCollisionComp's false - this component has real per-tick work.
bool StickyCollisionComp::hasUpdate() const { return true; }

float StickyCollisionComp::getHealEffect(Vec3&) const { return 0.0f; }

bool StickyCollisionComp::hasCollisionEffect() const
{
    return m_template->getHasCollisionEffect();
}

bool StickyCollisionComp::getIsBouncing() const
{
    return m_template->getIsBouncing();
}

// . Byte for byte the same body DefaultCollisionComp has: a bouncing round does not
// bounce off water.
int StickyCollisionComp::getDefaultCollisionType(int collisionType) const
{
    if (!m_template->getIsBouncing())
        return 0;

    Material* const material =
        materials()->getMaterialPtr(static_cast<unsigned int>(collisionType));
    return material->getHasWaterPhysics() ? 0 : 1;
}

// / 0x828DAC0. The host is the weak handle's second word; the transform is the member.
IObject* StickyCollisionComp::getHostObject() const
{
    return m_hostHandle.get();
}

const Mat4& StickyCollisionComp::getStickTransformation() const { return m_stickTransform; }

// . Registering with the manager is ALL activate does - the manager owns the set
// of live sticky projectiles that map teardown and round end sweep.
void StickyCollisionComp::activate()
{
    if (stickyProjectileManager)
        stickyProjectileManager->addStickyProjectile(
            static_cast<GenericProjectile*>(m_initObject));
}

// Applies a pending attachment unless the projectile is awaiting destruction.
// The server broadcasts it; a locally predicting client only applies it locally.
void StickyCollisionComp::updateComponent(float, unsigned int)
{
    IObject* const projectile = m_initObject;

    if (!m_pendingStick)
        return;
    if (!gameLogic->isServer() &&
        !static_cast<GenericProjectile*>(projectile)->getLocalPredict())
        return;
    if (projectile->getObjectInDestroyList())
        return;

    m_pendingStick = false;

    IObject* const host = getHostObject();
    if (host)
        stick(host, m_stickTransform);
    else
        stickAbsolute(m_stickTransform);

    if (!static_cast<GenericProjectile*>(projectile)->getLocalPredict())
    {
        StickyProjectileEvent* const event = new StickyProjectileEvent(
            static_cast<GenericProjectile*>(projectile), host, m_stickTransform);
        game->sendGameEventToAllPlaying(event);
    }
}

// stick - 0x828BD00, 255 B. Re-parent onto the host and freeze the physics.
// THE weak handle IS released twice on the success path in the shipped body: once for the old
// handle it replaces, and once more on the NEW handle immediately after storing it. That second
// release balances the addRef taken from getWeakPtr's temporary, so the stored handle ends up
// holding exactly one reference. Reproduced as written - it looks like a leak-and-double-free and
// is neither.
void StickyCollisionComp::stick(IObject* host, const Mat4& transform)
{
    if (!host)
        return;

    IObject* const projectile = m_initObject;

    IObject* const oldParent = projectile->getParent();
    if (oldParent)
        oldParent->removeChild(projectile);

    m_hostHandle = host->getWeakPtr();

    host->addChild(projectile);
    projectile->setRelativeTransformation(transform);

    IPhysicsNode* const node = physicsNode(projectile);
    if (node && !m_physicsDisabled)
    {
        node->setHasSeparatePhysicsUpdate(true);
        node->setEnabled(false);
        m_physicsDisabled = true;
    }
}

// stickAbsolute - 0x828BE00, 170 B. The no-host case: keep world coordinates, drop the parent, and
// clear flag bit 8.
// THE parent IS only dropped when WE ARE NOT locally predicting. A predicting client leaves the
// hierarchy alone and only moves the projectile.
void StickyCollisionComp::stickAbsolute(const Mat4& transform)
{
    IObject* const projectile = m_initObject;

    if (!static_cast<GenericProjectile*>(projectile)->getLocalPredict())
    {
        IObject* const parent = projectile->getParent();
        if (parent)
            parent->removeChild(projectile);
    }

    projectile->setAbsoluteTransformation(transform);
    projectile->updateFlags(0, 0x100);

    IPhysicsNode* const node = physicsNode(projectile);
    if (node && !m_physicsDisabled)
    {
        node->setHasSeparatePhysicsUpdate(true);
        node->setEnabled(false);
        m_physicsDisabled = true;
    }
}

// restoreProjectileState - 0x828CD90, 72 B. IT reads m_object (+0x08), NOT m_initObject - the
// only body in this class that uses the other pointer, and exactly the confusion WeaponComp.h warns
// about. Preserved as written.
void StickyCollisionComp::restoreProjectileState(bool keepParent)
{
    if (!m_object)
        return;

    IObject* const parent = m_object->getParent();
    if (parent && !keepParent)
        parent->removeChild(m_object);
}

void StickyCollisionComp::preResetProjectile(bool keepParent)
{
    if (m_initObject && stickyProjectileManager)
        stickyProjectileManager->removeStickyProjectile(
            static_cast<GenericProjectile*>(m_initObject));
    restoreProjectileState(keepParent);
}

// . preReset plus the physics restore, in that order.
void StickyCollisionComp::resetProjectile(bool keepParent)
{
    if (m_initObject && stickyProjectileManager)
        stickyProjectileManager->removeStickyProjectile(
            static_cast<GenericProjectile*>(m_initObject));
    restoreProjectileState(keepParent);

    IObject* const projectile = m_initObject;
    IPhysicsNode* const node = physicsNode(projectile);

    if (m_physicsDisabled)
    {
        projectile->updateFlags(0x100, 0);
        m_physicsDisabled = false;
        node->setHasSeparatePhysicsUpdate(false);
        node->setEnabled(true);
    }
}

// handleCollision - 0x828BEB0, 3,564 B. IT decides; IT does NOT stick.
bool StickyCollisionComp::handleCollision(IObject* target, const Vec3& velocity, const Vec3& normal,
                                          const Vec3& collisionOffset, int collisionType, int targetMaterial,
                                          int collisionTypeOverride,
                                          CollisionFaceInfo* face1, CollisionFaceInfo* face2)
{
    // already stuck? A projectile with a parent has nothing more to decide.
    if (!m_object || m_object->getParent())
        return false;

    IObject* const projectile = m_initObject;

    // The same one-second self-hit window DefaultCollisionComp uses.
    if (target)
    {
        IObject* const firingRoot =
            static_cast<GenericProjectile*>(projectile)->getLauncherVehicle();
        if (firingRoot && firingRoot == getRootParent(target) &&
            WorldPref::getWorldTime() -
                static_cast<GenericProjectile*>(projectile)->getWorldLaunchTime() < 1.0)
        {
            return false;
        }
    }

    int type = collisionType;
    if (collisionTypeOverride != -1)
        type = collisionTypeOverride;

    // THE normal IS passed RAW here, unlike DefaultCollisionComp which renormalises first. The
    // returned byte is tested immediately in both ports; a rejected GameLogic collision stops all
    // sticky processing.
    const bool handled = gameLogic->handleCollisionForProjectile(
        projectile, target, velocity, normal, collisionOffset, type, targetMaterial, face1, face2);
    if (!handled)
        return handled;

    // A player-control object that IS NOT THE base template takes THE HIT BUT IS never stuck TO.
    if (target && target->queryInterface(IID_IPlayerControlObject))
    {
        if (target->getTemplate()->getClassID() != CID_PlayerControlObjectTemplate)
            return handled;
    }

    if (m_pendingStick)
        return false;

    // THE angle gate. maxStickAngle is a cosine (see setMaxStickAngle), compared straight
    // against the collision normal's Y. Default -1.0f accepts every surface.
    if (m_template->getMaxStickAngle() > normal.y)
    {
        m_pendingStick = false;
        return true;
    }

    // build an orthonormal frame whose up axis is the collision normal
    Mat4 frame = m_object->getAbsoluteTransformation();

    Vec3 row0;
    row0.x = frame.m[0]; row0.y = frame.m[1]; row0.z = frame.m[2];
    normaliseInPlace(row0);

    Vec3 up = normal;
    normaliseInPlace(up);

    Vec3 row2 = cross(row0, up);
    normaliseInPlace(row2);

    row0 = cross(up, row2);
    normaliseInPlace(row0);

    up = cross(row2, row0);
    normaliseInPlace(up);

    frame.m[0] = row0.x; frame.m[1] = row0.y; frame.m[2] = row0.z;
    frame.m[4] = up.x;   frame.m[5] = up.y;   frame.m[6] = up.z;
    frame.m[8] = row2.x; frame.m[9] = row2.y; frame.m[10] = row2.z;

    m_object->setAbsoluteTransformation(frame);

    // HOW FAR TO push OUT along THE normal. A projectile with its own collision LOD 1 uses the
    // negated second component of that geometry's extent; everything else uses the global default.
    float pushOut = *stickDefaultDistance;
    if (m_object->hasCollisionLod(1))
    {
        ICollisionMesh* const mesh = static_cast<ICollisionMesh*>(
            object_detail::LayoutAccess::collisionMesh(m_object));
        pushOut = -mesh->getBoundingBox(1)->m_min.y;
    }

    const float along = normal.x * collisionOffset.x +
                        normal.y * collisionOffset.y +
                        normal.z * collisionOffset.z + pushOut;

    if (!gameLogic->isServer() &&
        !static_cast<GenericProjectile*>(projectile)->getLocalPredict())
    {
        return handled;
    }

    // choose what to stick TO
    IObject* const host = findClosestParentHavingNetworkable(target);

    bool notControlPoint = true;
    if (host && host->getTemplate())
        notControlPoint = host->getTemplate()->getClassID() != CID_ControlPointTemplate;

    if (host && !static_cast<GenericProjectile*>(projectile)->getLocalPredict() && notControlPoint)
    {
        // THE rotational bundle. Stick to the moving part, not the hull.
        IObject* const bundle =
            findClosestChildOfType(host, target, CID_RotationalBundleTemplate);

        const Mat4 bundleInverse = bundle->getInvAbsoluteTransformation();

        Mat4 world = m_object->getAbsoluteTransformation();
        world.m[12] += along * normal.x;
        world.m[13] += along * normal.y;
        world.m[14] += along * normal.z;

        Mat4 local = Mat4::getIdentity();
        local.mult(world, bundleInverse);
        m_stickTransform = local;

        m_hostHandle = bundle->getWeakPtr();
    }
    else
    {
        m_stickTransform = projectile->getAbsoluteTransformation();
        m_stickTransform.m[12] += along * normal.x;
        m_stickTransform.m[13] += along * normal.y;
        m_stickTransform.m[14] += along * normal.z;

        m_hostHandle = WeakPtr<IObject>();
    }

    // Water rejection uses the struck surface's material, not the projectile's material.
    Material* const material = materials()->getMaterialPtr(static_cast<unsigned int>(targetMaterial));
    if (!material || material->getHasWaterPhysics())
        return handled;                         // nothing sticks to water

    // A template that MAY NOT stick TO vehicles walks up to the host's root to check - so C4
    // thrown at a wheel is refused because the wheel's root is the vehicle.
    if (!m_template->getStickToVehicles() && host)
    {
        if (getRootParent(host)->queryInterface(IID_IPlayerControlObject))
            return handled;
    }

    // The pair reads oddly but is exact: stickToTerrain permits the no-target case and
    // stickToObjects permits the target case, and both conditions must hold.
    if ((m_template->getStickToTerrain() || target) &&
        (m_template->getStickToStaticObjects() || !target))
    {
        m_pendingStick = true;
        return false;                           // scheduled - updateComponent finishes it
    }

    return handled;
}

// maxStickAngle starts AT -1.0f - a cosine, not degrees, and one that accepts every surface.
// ALL three permission bytes start true.
StickyCollisionCompTemplate::StickyCollisionCompTemplate()
    : m_refCount(1)
    , m_hasCollisionEffect(false)
    , m_isBouncing(false)
    , m_maxStickAngle(-1.0f)
    , m_stickToTerrain(true)
    , m_stickToStaticObjects(true)
    , m_stickToVehicles(true)
{
}

void StickyCollisionCompTemplate::addRef() { ++m_refCount; }
int StickyCollisionCompTemplate::getRef() { return m_refCount; }

int StickyCollisionCompTemplate::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        ::operator delete(this);
    return remaining;
}

void* StickyCollisionCompTemplate::queryInterface(unsigned int iid) const
{
    StickyCollisionCompTemplate* const self =
        const_cast<StickyCollisionCompTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent ||
        iid == IID_IProjectileCompTemplate)
        return self;
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int StickyCollisionCompTemplate::getClassID() const
{
    return CID_StickyCollisionCompTemplate;
}

unsigned int StickyCollisionCompTemplate::getInterfaceID()
{
    return IID_IProjectileCollisionCompTemplate;
}

IObjectComponent* StickyCollisionCompTemplate::createComponent()
{
    return new StickyCollisionComp(this);
}

void StickyCollisionCompTemplate::objectCreated(IObject*) {}
void StickyCollisionCompTemplate::objectDestroyed(IObject*) {}
void StickyCollisionCompTemplate::setTemplate(IObjectTemplate*) {}

void* StickyCollisionCompTemplate::queryImplementation(unsigned int classId)
{
    return classId == getClassID() ? this : 0;
}

bool StickyCollisionCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    if (m_hasCollisionEffect)
        script << "ObjectTemplate.collision.hasCollisionEffect " << m_hasCollisionEffect << std::endl;
    if (m_isBouncing)
        script << "ObjectTemplate.collision.bouncing " << m_isBouncing << std::endl;

    float clamped = m_maxStickAngle;
    if (clamped < -1.0f)
        clamped = -1.0f;
    else if (clamped > 1.0f)
        clamped = 1.0f;
    const float degrees = acosf(clamped) * 57.295776f;

    // Retail compares the converted degree value with -1.0f, not with the stored-cosine default.
    // Consequently the constructor default (-1 cosine => 180 degrees) is emitted.
    if (fabsf(degrees - -1.0f) > FLT_EPSILON)
    {
        clamped = m_maxStickAngle;
        if (clamped < -1.0f)
            clamped = -1.0f;
        else if (clamped > 1.0f)
            clamped = 1.0f;
        script << "ObjectTemplate.collision.maxStickAngle "
               << acosf(clamped) * 57.295776f << std::endl;
    }

    if (m_stickToStaticObjects)
        script << "ObjectTemplate.collision.stickToStaticObjects "
               << m_stickToStaticObjects << std::endl;
    if (m_stickToVehicles)
        script << "ObjectTemplate.collision.stickToVehicles " << m_stickToVehicles << std::endl;
    if (m_stickToTerrain)
        script << "ObjectTemplate.collision.stickToTerrain " << m_stickToTerrain << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

// . degrees IN, cosine OUT - clamped to [-180, 180] first. The runtime never sees
// an angle; it compares the stored cosine against a normal component directly.
void StickyCollisionCompTemplate::setMaxStickAngle(float degrees)
{
    float clamped;
    if (degrees < -180.0f)
        clamped = -180.0f;
    else if (degrees > 180.0f)
        clamped = 180.0f;
    else
        clamped = degrees;

    m_maxStickAngle = cosf(clamped * 0.017453292f);
}

// . zero, not the sticky type - `xor eax, eax`. The component's own
// getDefaultCollisionType(int) is what actually answers for a sticky projectile.
int StickyCollisionCompTemplate::getDefaultCollisionType() const { return 0; }

#if defined(_M_IX86)
static_assert(sizeof(StickyCollisionComp) == 0x6C,
              "StickyCollisionComp must match the retail x86 layout");
static_assert(sizeof(StickyCollisionCompTemplate) == 0x18,
              "StickyCollisionCompTemplate must match the retail x86 layout");
#endif

} // namespace world
} // namespace hfe
} // namespace dice
