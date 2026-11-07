// Single-shot firing and template properties.

#include "SingleFireComp.h"
#include "../../GeometryPartAccess.h"   // named geometry operations (non-MSVC)
#if defined(BF2_STATIC_PLUGINS) && !defined(_MSC_VER)
// [CHOSEN, not recovered] Native static geometry tables retain PE slot order.
#include "../../../../Geom/NativeGeometrySlots.h"
#endif

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "../../SingleFireCompTemplate.h"
#include "../GenericFireArm.h"
#include "../../Game/Soldier.h"
#include "../../IObjectTemplate.h"
#include "../../IPlayer.h"
#include "../../ObjectLayoutAccess.h"
#include "../../../../Core/Debug.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"

namespace dice {
namespace hfe {
namespace world {

namespace {

// geom::IGeometry has not yet acquired a typed declaration in this tree.  These three calls are
// nevertheless completely bounded: SingleFireComp uses only getSubgeom, setLod and getLodCount.
// The slot shift is a real destructor-ABI difference, cross-checked at every call in both images:
// PE/MSVC             ELF/Itanium
// getSubgeom            +0x70               +0x74
// setLod                +0x78               +0x7C
// getLodCount           +0x80               +0x84
#ifdef _MSC_VER
typedef int  (__thiscall *GeometryGetIntFn)(void*);
typedef void (__thiscall *GeometrySetLodFn)(void*, int);
const std::size_t kGeometryGetSubgeomSlot = 0x70 / 4;
const std::size_t kGeometrySetLodSlot     = 0x78 / 4;
const std::size_t kGeometryGetLodCountSlot = 0x80 / 4;
#endif

#if defined(_MSC_VER)
void** geometryVtable(void* geometry)
{
    return *reinterpret_cast<void***>(geometry);
}

int geometryGetSubgeom(void* geometry)
{
    return reinterpret_cast<GeometryGetIntFn>(
        geometryVtable(geometry)[kGeometryGetSubgeomSlot])(geometry);
}

void geometrySetLod(void* geometry, int lod)
{
    reinterpret_cast<GeometrySetLodFn>(
        geometryVtable(geometry)[kGeometrySetLodSlot])(geometry, lod);
}

int geometryGetLodCount(void* geometry)
{
    return reinterpret_cast<GeometryGetIntFn>(
        geometryVtable(geometry)[kGeometryGetLodCountSlot])(geometry);
}
#else
// Non-MSVC: the named geometry_part operations (static native: Geom/NativeGeometrySlots.h).
int geometryGetSubgeom(void* geometry) { return geometry_part::getSubgeom(geometry); }
void geometrySetLod(void* geometry, int lod) { geometry_part::setLod(geometry, lod); }
int geometryGetLodCount(void* geometry) { return geometry_part::getLodCount(geometry); }
#endif

} // namespace

// SingleFireComp runtime

// ELF C2/C1 0x826B760/0x826B800 (150 B), PE 0x5A0D90.  The launch-delay start tick is
// deliberately absent from the initializer list: neither shipped constructor writes it, and the
// adjacent false validity byte prevents a read until fire() has populated it.
SingleFireComp::SingleFireComp(SingleFireCompTemplate* objectTemplate)
    : FireComp(objectTemplate)
    , m_refCount(1)
    , m_fireCompleteAtTick(0)
    , m_waitingForFireLaunchDelay(false)
    , m_enabledTimeEventActions{false, false, false}
    , m_requestingFire(false)
    , m_dropWeaponFxPending(false)
    , m_dropWeaponFxPosition{0.0f, 0.0f, 0.0f}
    , m_dropWeaponFxAtTick(0)
    , m_singleFireTemplate(objectTemplate)
{
}

SingleFireComp::~SingleFireComp()
{
}

void SingleFireComp::addRef()
{
    ++m_refCount;
}

int SingleFireComp::getRef()
{
    return m_refCount;
}

int SingleFireComp::release()
{
    const int remaining = m_refCount - 1;
    const bool last = m_refCount == 1;
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

// .  WeaponComp/IObjectComponent is the primary face; the two weapon
// interfaces must return the adjusted IWeaponFireComp face at +0x10 in both shipped x86 builds.
void* SingleFireComp::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return const_cast<SingleFireComp*>(this);
    if (iid == IID_IWeaponComp || iid == IID_IWeaponFireComp)
    {
        return static_cast<IWeaponFireComp*>(
            const_cast<SingleFireComp*>(this));
    }
    return 0;
}

// .  The expiry comparison is strict and unsigned: a delay completing
// on the current tick remains pending through that tick and advances on the next one.
bool SingleFireComp::preUpdate()
{
    if (FireComp::preUpdate())
        return true;

    if (!m_waitingForFireLaunchDelay)
        return false;

    const unsigned int completesAt =
        m_fireLaunchDelayStartedAtTick
        + static_cast<unsigned int>(m_singleFireTemplate->getFireLaunchDelayInTicks());
    if (completesAt >= static_cast<unsigned int>(WorldPref::getTickCount()))
        return false;

    m_fireLaunchDelayPending = false;
    m_waitingForFireLaunchDelay = false;
    setFireState(static_cast<FireState>(4));
    return true;
}

// .  Keep the shipped repeated tick reads: each scheduled arm obtains
// the current tick independently rather than sharing the updateComponent tick argument.
void SingleFireComp::updateComponent(float seconds, unsigned int flags)
{
    if (m_dropWeaponFxPending
        && m_dropWeaponFxAtTick < static_cast<unsigned int>(WorldPref::getTickCount()))
    {
        const unsigned int elapsed =
            static_cast<unsigned int>(WorldPref::getTickCount()) - m_dropWeaponFxAtTick;
        const Vec3& absolutePosition = firearm()->getAbsolutePosition();
        const float multiplier = 1.0f / static_cast<float>(elapsed);
        Vec3 velocity = {
            (m_dropWeaponFxPosition.x - absolutePosition.x) * multiplier,
            (m_dropWeaponFxPosition.y - absolutePosition.y) * multiplier,
            (m_dropWeaponFxPosition.z - absolutePosition.z) * multiplier
        };

        IPlayer* const player = firearm()->getPlayer();
        if (player)
        {
            IObject* const vehicle = player->getVehicle();
            if (vehicle
                && vehicle->getTemplate()->getClassID() == CID_SoldierTemplate
                && static_cast<Soldier*>(vehicle)->getActiveWeapon(0) == firearm())
            {
                static_cast<Soldier*>(vehicle)->dropWeaponFx(
                    true, velocity, m_singleFireTemplate->getDropLod());
            }
        }
        m_dropWeaponFxPending = false;
    }

    if (static_cast<int>(getFireState()) > 3
        && m_fireCompleteAtTick <= static_cast<unsigned int>(WorldPref::getTickCount()))
    {
        if (m_singleFireTemplate->getBusyUntilButtonRelease() && !m_requestingFire)
        {
            firearm()->updateAcceptStateSyncAtTick(false);
        }
        else
        {
            IWeaponComp* const self =
                static_cast<IWeaponComp*>(static_cast<IWeaponFireComp*>(this));
            if (!firearm()->busyExcept(self, false))
                fireComplete(m_fireCompleteAtTick);
        }
    }

    const std::vector<WEventStruct>& events = m_singleFireTemplate->getWeaponTimeEvents();
    for (std::vector<WEventStruct>::const_iterator it = events.begin(); it != events.end(); ++it)
    {
        const int action = static_cast<int>(it->action);
        if (!m_enabledTimeEventActions[action])
            continue;

        unsigned int baseTick;
        if (it->trigger == WTPostFire)
            baseTick = m_fireAtTick;
        else if (it->trigger == WTReload)
            baseTick = m_reloadAtTick;
        else
            continue;

        if (baseTick + static_cast<unsigned int>(it->ticks)
            <= static_cast<unsigned int>(WorldPref::getTickCount()))
        {
            handleTimeEventAction(*it);
        }
    }

    FireComp::updateComponent(seconds, flags);
}

// .  Event actions deliberately index the three-byte enable array
// without a bounds check at the common tail; an unknown action reaches the fatal retail Debug
// first under normal debug settings.
void SingleFireComp::handleTimeEventAction(const WEventStruct& event)
{
    switch (event.action)
    {
    case WAShow:
        if (m_singleFireTemplate->getHideLod() != 0)
        {
            void* const geometry =
                object_detail::LayoutAccess::geometry(static_cast<IObject*>(firearm()));
            if (!geometry)
                break;

            int lod = 0;
            if (getObject())
            {
                if (m_singleFireTemplate->getHideLod() < geometryGetLodCount(geometry))
                {
                    const IObjectTemplate* const objectTemplate = getObject()->getTemplate();
                    if (objectTemplate
                        && firearm()->getTotalAmmo() <= 0
                        && objectTemplate->getName() == "insgr_rpg")
                    {
                        lod = m_singleFireTemplate->getHideLod();
                    }
                }
            }
            geometrySetLod(geometry, lod);
        }
        else
        {
            firearm()->updateFlags(2, 0);
        }
        break;

    case WAHide:
        if (m_singleFireTemplate->getHideLod() == 0)
        {
            firearm()->updateFlags(0, 2);
            break;
        }
        else
        {
            void* const geometry =
                object_detail::LayoutAccess::geometry(static_cast<IObject*>(firearm()));
            if (!geometry)
                break;

            if (m_singleFireTemplate->getHideLod() < geometryGetLodCount(geometry))
            {
                geometrySetLod(geometry, m_singleFireTemplate->getHideLod());
                break;
            }

            const std::string& objectName = firearm()->getTemplate()->getName();
            Debug(DEBUGTYPE_6,
                  "Game/Objects/Weapons/FireComponents/SingleFireComp.cpp", 149, "")
                << " subgeom " << geometryGetSubgeom(geometry)
                << " lod " << m_singleFireTemplate->getHideLod()
                << " not available. Object=" << objectName
                << " lodcount=" << geometryGetLodCount(geometry);
        }
        break;

    case WADrop:
        if (firearm()->getPlayer())
        {
            m_dropWeaponFxPosition = firearm()->getAbsolutePosition();
            m_dropWeaponFxAtTick = static_cast<unsigned int>(WorldPref::getTickCount());
            m_dropWeaponFxPending = true;
        }
        break;

    default:
        Debug(DEBUGTYPE_6,
              "Game/Objects/Weapons/FireComponents/SingleFireComp.cpp", 193, "")
            << "Unknown event: " << static_cast<int>(event.action);
        break;
    }

    m_enabledTimeEventActions[static_cast<int>(event.action)] = false;
}

bool SingleFireComp::requestFire(bool first, bool second)
{
    bool accepted = FireComp::requestFire(first, second);

    if (m_singleFireTemplate->getOnlyFireWhenProne())
    {
        IPlayer* const player = firearm()->getPlayer();
        if (player)
        {
            IObject* const vehicle = player->getVehicle();
            if (vehicle
                && vehicle->getTemplate()->getClassID() == CID_SoldierTemplate
                && static_cast<Soldier*>(vehicle)->getPose() != SoldierPose_Prone)
            {
                accepted = false;
            }
        }
    }

    if (!second && accepted)
        m_requestingFire = false;
    return accepted;
}

bool SingleFireComp::launchesProjectiles(int) const
{
    return static_cast<int>(getFireState()) <= 3
        || m_fireCompleteAtTick == 0
        || !m_singleFireTemplate->getBusyUntilButtonRelease()
        || m_requestingFire;
}

void SingleFireComp::outOfAmmo(bool value)
{
    FireComp::outOfAmmo(value);
}

void SingleFireComp::enable()
{
    if (m_fireCompleteAtTick != 0)
    {
        const unsigned int currentTick = static_cast<unsigned int>(WorldPref::getTickCount());
        const unsigned int completionTick = m_fireCompleteAtTick;
        if (completionTick <= currentTick)
        {
            fireCompleteUpdateStates();
            setFireState(static_cast<FireState>(0));
            m_signalComplete = true;
            m_signalCompleteAtTick = completionTick;
        }
    }

    FireComp::enable();
    m_requestingFire = true;
    m_dropWeaponFxPending = false;
    disableTimeEvents();

    WEventStruct event = { WAShow, WTReload, 0.0f, 0 };
    handleTimeEventAction(event);
}

int SingleFireComp::fire(bool, int)
{
    m_signalFire = false;
    m_signalComplete = false;
    m_firedProjectiles.clear();

    bool startDelay = m_fireStartDelay;
    if (m_fireLaunchDelayPending)
    {
        if (startDelay)
            return false;

        if (m_sequenceNumber == 0 && m_template->getFireStartDelay().value != 0.0f)
        {
            m_fireStartDelay = true;
            m_fireAtTick = static_cast<unsigned int>(WorldPref::getTickCount());
            m_fireStartDelayTicks = static_cast<unsigned int>(getFireStartDelayInTicks());
            setFireState(static_cast<FireState>(1));
            return false;
        }
    }

    if (startDelay)
        return false;

    m_fireStartDelay = false;
    if (static_cast<int>(getFireState()) <= 2
        && !m_waitingForFireLaunchDelay
        && m_singleFireTemplate->getFireLaunchDelayInTicks() != 0)
    {
        m_fireLaunchDelayStartedAtTick =
            static_cast<unsigned int>(WorldPref::getTickCount());
        m_waitingForFireLaunchDelay = true;
        setFireState(static_cast<FireState>(3));

        for (IObject* child = firearm()->getChild(); child; child = child->getSibling())
        {
            child->handleMessage(static_cast<TemplateMessage>(0), firearm()->getPlayer(), false);
        }
        return true;
    }

    m_fireAtTick = static_cast<unsigned int>(WorldPref::getTickCount());
    enableTimeEvents(WTPostFire);
    m_fireCompleteAtTick =
        m_fireAtTick + static_cast<unsigned int>(m_singleFireTemplate->getTimePerRound());
    setFireState(static_cast<FireState>(4));
    launchProjectile(-1);
    return m_singleFireTemplate->getFireLaunchDelayInTicks() == 0;
}

void SingleFireComp::enableTimeEvents(WeaponTrigger trigger)
{
    const std::vector<WEventStruct>& events = m_singleFireTemplate->getWeaponTimeEvents();
    for (std::vector<WEventStruct>::const_iterator it = events.begin(); it != events.end(); ++it)
    {
        if (it->trigger == trigger)
            m_enabledTimeEventActions[static_cast<int>(it->action)] = true;
    }
}

void SingleFireComp::disableTimeEvents()
{
    m_enabledTimeEventActions[0] = false;
    m_enabledTimeEventActions[1] = false;
    m_enabledTimeEventActions[2] = false;
}

void SingleFireComp::fireComplete(unsigned int tick)
{
    firearm()->getPlayer();
    FireComp::fireComplete(tick);
    fireCompleteUpdateStates();
}

void SingleFireComp::fireCompleteUpdateStates()
{
    m_fireCompleteAtTick = 0;
    m_waitingForFireLaunchDelay = false;
    enableTimeEvents(WTFireComplete);
    m_requestingFire = false;
}

bool SingleFireComp::launchProjectile(int index)
{
    return FireComp::launchProjectile(index);
}

void SingleFireComp::postReload()
{
    m_postReloadState = 0;
    FireComp::postReload();
}

void SingleFireComp::reload(ReloadState state)
{
    FireComp::reload(state);
    if (static_cast<int>(state) != 2)
        enableTimeEvents(WTReload);
}

void SingleFireComp::fireStop(bool value)
{
    FireComp::fireStop(value);
    m_requestingFire = true;
}

bool SingleFireComp::busy()
{
    return static_cast<int>(getFireState()) > 1;
}

bool SingleFireComp::isFireLaunchDelay() const
{
    return static_cast<int>(getFireState()) <= 2
        && !m_waitingForFireLaunchDelay
        && m_singleFireTemplate->getFireLaunchDelayInTicks() != 0;
}

// SingleFireCompTemplate

SingleFireCompTemplate::SingleFireCompTemplate()
    : FireCompTemplate()
    , m_refCount(1)
    , m_weaponTimeEvents()
    , m_busyUntilButtonRelease(false)
    , m_onlyFireWhenProne(false)
    , m_hideLod(0)
    , m_dropLod(0)
{
}

SingleFireCompTemplate::~SingleFireCompTemplate()
{
}

void SingleFireCompTemplate::addRef()
{
    ++m_refCount;
}

int SingleFireCompTemplate::getRef()
{
    return m_refCount;
}

int SingleFireCompTemplate::release()
{
    const int remaining = m_refCount - 1;
    const bool last = m_refCount == 1;
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* SingleFireCompTemplate::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase
        || iid == IID_IObjectTemplateComponent
        || iid == IID_IWeaponCompTemplate)
    {
        return const_cast<SingleFireCompTemplate*>(this);
    }
    if (iid == io::IID_IConsoleSaveable)
    {
        return static_cast<io::IConsoleSaveable*>(
            const_cast<SingleFireCompTemplate*>(this));
    }
    return 0;
}

unsigned int SingleFireCompTemplate::getClassID() const
{
    return CID_SingleFireCompTemplate;
}

void* SingleFireCompTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_SingleFireCompTemplate || classId == CID_FireCompTemplate)
        return this;
    return 0;
}

IObjectComponent* SingleFireCompTemplate::createComponent()
{
    return new SingleFireComp(this);
}

void SingleFireCompTemplate::addWeaponTimeEvent(WeaponTrigger trigger,
                                                 WeaponAction action,
                                                 float value)
{
    WEventStruct event = {
        action,
        trigger,
        value,
        static_cast<int>(value * 30.0f)
    };

    // Preserve the shipped iterator restart exactly.  erase's returned iterator is discarded;
    // the loop restarts at begin and then the for increment advances once, skipping the new first
    // element.  This is an observable retail quirk, not a cleanup opportunity.
    for (std::vector<WEventStruct>::iterator it = m_weaponTimeEvents.begin();
         it != m_weaponTimeEvents.end(); ++it)
    {
        if (it->action == action)
        {
            m_weaponTimeEvents.erase(it);
            it = m_weaponTimeEvents.begin();
        }
    }

    m_weaponTimeEvents.push_back(event);
}

float SingleFireCompTemplate::getWeaponTimeEventValue(WeaponAction action,
                                                       WeaponTrigger trigger)
{
    for (std::vector<WEventStruct>::iterator it = m_weaponTimeEvents.begin();
         it != m_weaponTimeEvents.end(); ++it)
    {
        if (it->action == action && it->trigger == trigger)
            return it->seconds;
    }
    return -1.0f;
}

void SingleFireCompTemplate::setDropWeaponAfterFiringDelay(float value)
{
    addWeaponTimeEvent(WTPostFire, WADrop, value);
}

void SingleFireCompTemplate::setHideWeaponAfterFiringDelay(float value)
{
    addWeaponTimeEvent(WTPostFire, WAHide, value);
}

void SingleFireCompTemplate::setShowWeaponAfterReloadDelay(float value)
{
    addWeaponTimeEvent(WTReload, WAShow, value);
}

void SingleFireCompTemplate::setHideWeaponAfterReloadDelay(float value)
{
    addWeaponTimeEvent(WTReload, WAHide, value);
}

void SingleFireCompTemplate::setDropWeaponAfterReloadDelay(float value)
{
    addWeaponTimeEvent(WTReload, WADrop, value);
}

float SingleFireCompTemplate::getDropWeaponAfterFiringDelay()
{
    return getWeaponTimeEventValue(WADrop, WTPostFire);
}

float SingleFireCompTemplate::getHideWeaponAfterFiringDelay()
{
    return getWeaponTimeEventValue(WAHide, WTPostFire);
}

float SingleFireCompTemplate::getShowWeaponAfterReloadDelay()
{
    return getWeaponTimeEventValue(WAShow, WTReload);
}

float SingleFireCompTemplate::getHideWeaponAfterReloadDelay()
{
    return getWeaponTimeEventValue(WAHide, WTReload);
}

float SingleFireCompTemplate::getDropWeaponAfterReloadDelay()
{
    return getWeaponTimeEventValue(WADrop, WTReload);
}

bool SingleFireCompTemplate::makeScript(io::IStream* stream)
{
    if (!FireCompTemplate::makeScript(stream))
        return false;

    std::ostringstream script;
    if (!m_weaponTimeEvents.empty())
    {
        float value = getDropWeaponAfterFiringDelay();
        if (value != -1.0f)
            script << "ObjectTemplate.fire.dropWeaponAfterFiringDelay " << value << std::endl;

        value = getHideWeaponAfterFiringDelay();
        if (value != -1.0f)
            script << "ObjectTemplate.fire.hideWeaponAfterFiringDelay " << value << std::endl;

        value = getShowWeaponAfterReloadDelay();
        if (value != -1.0f)
            script << "ObjectTemplate.fire.showWeaponAfterReloadDelay " << value << std::endl;

        value = getHideWeaponAfterReloadDelay();
        if (value != -1.0f)
            script << "ObjectTemplate.fire.hideWeaponAfterReloadDelay " << value << std::endl;

        value = getDropWeaponAfterReloadDelay();
        if (value != -1.0f)
            script << "ObjectTemplate.fire.dropWeaponAfterReloadDelay " << value << std::endl;
    }

    if (m_dropLod > 0)
        script << "ObjectTemplate.fire.dropLod " << m_dropLod << std::endl;
    if (m_hideLod > 0)
        script << "ObjectTemplate.fire.hideLod " << m_hideLod << std::endl;
    if (m_busyUntilButtonRelease)
        script << "ObjectTemplate.fire.busyUntilButtonRelease 1" << std::endl;
    if (m_onlyFireWhenProne)
        script << "ObjectTemplate.fire.onlyFireWhenProne 1" << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
