// Delayed artillery ballistic tracing.

#include "PreDestinedFollowComp.h"

#include <new>
#include <sstream>
#include <string>

#include "../GenericProjectileTemplate.h"
#include "../ProjectileCompTemplates.h"
#include "../../IObject.h"
#include "../../ObjectLayoutAccess.h"
#include "../../../IGameLogic.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Physics/BasicPhysicsSystem.h"
#include "../../../../Physics/IPhysicsNode.h"
#if !DEDICATED
#include "../../../Main/Game.h"
#include "../../../../Core/SmartItf.h"
#include "../../../../GeomMisc/DebugRenderer.h"
#endif

namespace dice {
namespace hfe {
namespace world {

// ELF C2 0x8297C90 (301 B; old-libstdc++ allocation path included), C1 0x8297DC0 (134 B), and
// . The list accounts for the only layout difference: 0x40 bytes in retail ELF and
// 0x48 in retail PE. Both component reference counts start at one.
PreDestinedFollowComp::PreDestinedFollowComp(PreDestinedFollowCompTemplate* objectTemplate)
    : m_refCount(1)
    , m_template(objectTemplate)
    , m_targetObject()
    , m_delayTick(0)
    , m_firstFollow(true)
    , m_launchSpeed()
    , m_launchWorldTime(0.0f)
    , m_debugPoints()
    , m_debugPointCounter(0)
{
    ProjectileComp::m_refCount = 1;
    m_object = 0;
    m_initObject = 0;
}

// Generated list/WeakPtr/base destruction is exactly the ELF D2/D1 pair at 0x8297E50/0x8297EC0
// and PE 0x5BB760. The target's control block is released after the debug list is destroyed.
PreDestinedFollowComp::~PreDestinedFollowComp() {}

void PreDestinedFollowComp::addRef() { ++m_refCount; }
int PreDestinedFollowComp::getRef() { return m_refCount; }

int PreDestinedFollowComp::release()
{
    const int remaining = m_refCount - 1;
    const bool last = (m_refCount == 1);
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* PreDestinedFollowComp::queryInterface(unsigned int iid) const
{
    PreDestinedFollowComp* const self = const_cast<PreDestinedFollowComp*>(this);
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return static_cast<IObjectComponent*>(self);
    if (iid == IID_IProjectileComp || iid == IID_IProjectileFollowComp)
        return static_cast<IProjectileFollowComp*>(self);
    return 0;
}

unsigned int PreDestinedFollowComp::getInterfaceID() const
{
    return IID_IProjectileFollowComp;
}

// Yes, false: GenericProjectile drives this specialised component directly. This seven-byte
// shipped answer must not be changed merely because updateComponent has a real body.
bool PreDestinedFollowComp::hasUpdate() const
{
    return false;
}

// . The deadline comparison is unsigned (`ja` is the early-out), including the
// wraparound behaviour. Both formal arguments are ABI-real; only seconds is forwarded.
void PreDestinedFollowComp::updateComponent(float seconds, unsigned int)
{
    IObject* const target = m_targetObject.get();
    if (target && m_delayTick <= static_cast<unsigned int>(WorldPref::getTickCount()))
        (void)followTarget(target, seconds);
}

// / PE equivalent. Retail clears only these two fields: it does not reset the delay,
// launch sample, list, or debug counter.
void PreDestinedFollowComp::activate()
{
    m_targetObject = WeakPtr<IObject>();
    m_firstFollow = true;
}

void PreDestinedFollowComp::setTargetObject(IObject* target)
{
    m_targetObject = target ? target->getWeakPtr() : WeakPtr<IObject>();
}

void PreDestinedFollowComp::setDelayTick(unsigned int tick)
{
    m_delayTick = tick;
}

// , 225 B. This is intentionally a debug sampler rather than a
// homing solver. The target is never dereferenced; its non-null state gates the work and return.
// Both ports keep the otherwise-discarded virtual calls and ballistic calculations, so they remain
// here even though no renderer consumes the locals in the dedicated image.
bool PreDestinedFollowComp::followTarget(IObject* target, float)
{
    if (!target)
        return false;

    const float gravity =
        static_cast<IPhysicsSystem*>(basicPhysicsSystem)->getGravity()
        * static_cast<const GenericProjectileTemplate*>(m_initObject->getTemplate())
              ->m_gravityModifier;
    GenericProjectile* const projectile = static_cast<GenericProjectile*>(m_initObject);

    if (m_firstFollow)
    {
        m_firstFollow = false;
        m_launchWorldTime = static_cast<float>(WorldPref::getWorldTime());

        // [PLATFORM] ELF dispatches the primary GenericProjectile slot; PE dispatches the adjusted
        // IProjectileObject slot. Both call the same getLauncherWeapon body and discard its result.
        (void)projectile->getLauncherWeapon();

        IPhysicsNode* const node = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(m_initObject));
        m_launchSpeed = node->getPositionalSpeed();

        // The ELF reads the file-local counter directly. As in GenericProjectile's other recovered
        // debug arm, the public GameLogic accessor preserves the shared value in this source tree.
        if (!gameLogic->getDebugMissiles())
            return true;
        m_debugPoints.clear();
    }
    else
    {
        if (!gameLogic->getDebugMissiles())
            return true;

        if (++m_debugPointCounter > 5)
        {
            m_debugPointCounter = 0;
            m_debugPoints.insert(m_debugPoints.begin(), m_initObject->getAbsolutePosition());
        }
#if !DEDICATED
        // BF2.exe 0x0061C391..0x0061C484: only subsequent follow updates
        // publish the saved points. The singleton is borrowed and unguarded.
        if (!isDedicated())
        {
            geom::IDebugRenderer* renderer = static_cast<geom::IDebugRenderer*>(
                classManager->getSingleton("DebugRenderer"));
            for (std::list<Vec3>::const_iterator it = m_debugPoints.begin();
                 it != m_debugPoints.end(); ++it)
            {
                Mat4 transform = {};
                transform.m[0] = transform.m[5] = transform.m[10] = transform.m[15] = 1.0f;
                transform.m[12] = it->x;
                transform.m[13] = it->y;
                transform.m[14] = it->z;
                renderer->queueGeometry(geom::CID_DebugSphereMeshTemplate, transform, "");
            }
        }
#endif
    }

    if (gameLogic->getDebugMissiles())
    {
        IPhysicsNode* const node = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(m_initObject));
        const Vec3 currentSpeed = node->getPositionalSpeed();
        const Vec3& currentPosition = m_initObject->getAbsolutePosition();
        const Vec3 startPosition = projectile->getStartPos();
        const Vec3 travelled = { currentPosition.x - startPosition.x,
                                 currentPosition.y - startPosition.y,
                                 currentPosition.z - startPosition.z };

        const float elapsed =
            static_cast<float>(WorldPref::getWorldTime()) - m_launchWorldTime;
        const Vec3 predictedSpeed = { m_launchSpeed.x,
                                      m_launchSpeed.y + gravity * elapsed,
                                      m_launchSpeed.z };
        const Vec3 predictedTravel = { m_launchSpeed.x * elapsed,
                                       m_launchSpeed.y * elapsed
                                           + elapsed * elapsed * gravity * 0.5f,
                                       m_launchSpeed.z * elapsed };
        (void)currentSpeed;
        (void)travelled;
        (void)predictedSpeed;
        (void)predictedTravel;
    }

    return true;
}

// The template is a state-free twelve-slot leaf. makeScript still writes the empty generated
// string through IStream and checks the exact byte count, matching ELF 0x82983E0.
PreDestinedFollowCompTemplate::PreDestinedFollowCompTemplate()
    : m_refCount(1)
{
}

void PreDestinedFollowCompTemplate::addRef() { ++m_refCount; }
int PreDestinedFollowCompTemplate::getRef() { return m_refCount; }

int PreDestinedFollowCompTemplate::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        ::operator delete(this);
    return remaining;
}

void* PreDestinedFollowCompTemplate::queryInterface(unsigned int iid) const
{
    PreDestinedFollowCompTemplate* const self =
        const_cast<PreDestinedFollowCompTemplate*>(this);
    if (iid == IID_IBase
        || iid == IID_IObjectTemplateComponent
        || iid == IID_IProjectileCompTemplate)
    {
        return static_cast<IObjectTemplateComponent*>(self);
    }
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int PreDestinedFollowCompTemplate::getClassID() const
{
    return CID_PreDestinedFollowCompTemplate;
}

unsigned int PreDestinedFollowCompTemplate::getInterfaceID()
{
    return IID_IProjectileFollowCompTemplate;
}

IObjectComponent* PreDestinedFollowCompTemplate::createComponent()
{
    return new PreDestinedFollowComp(this);
}

void PreDestinedFollowCompTemplate::objectCreated(IObject*) {}
void PreDestinedFollowCompTemplate::objectDestroyed(IObject*) {}
void PreDestinedFollowCompTemplate::setTemplate(IObjectTemplate*) {}

void* PreDestinedFollowCompTemplate::queryImplementation(unsigned int classId)
{
    return classId == getClassID() ? this : 0;
}

bool PreDestinedFollowCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
