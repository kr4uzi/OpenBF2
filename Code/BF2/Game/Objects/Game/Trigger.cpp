// Authored trigger volumes, their template properties and effector dispatch.

#include "Trigger.h"
#include "../../../IO/Stream/IStream.h"
#include <sstream>
#include "TriggerManager.h"
#include "TriggerableManager.h"
#include "AmbientEffectAreaManager.h"
#include "AmbientEffectArea.h"
#include "../ObjectLayoutAccess.h"
#include "../IPlayer.h"
#include "../Weapons/GenericFireArm.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectPredicator.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Core/Debug.h"

#include "../../../Core/Ids.h"
#include "../../../Core/System.h"                 // WorldPref::getTickCount / getTickTime
#include "../../IGameLogic.h"                     // gameLogic->isServer() - isValid's server arm
#include "../IObject.h"
#include "../../../Math/Vec3.h"
#if !DEDICATED
#include "../../Main/Game.h"
#endif

#include <cmath>
#include <list>

namespace dice {
namespace hfe {

extern IGameLogic* gameLogic;                     // ds:0x8ACBFA0 - Core/Globals.cpp

namespace world {

// TriggerTemplate

// (C2) / 0x81C27A0 (C1), 129 B: BundleTemplate, the four vptrs, then the eight fields in
// this order (the ctor's own store order): refcount 1, averageTimeBetweenTests 0, chance 0,
// events 0, triggerId -1, minimumTimeBetweenTriggering 1.0f, radius 0, triggerForEach... false.
TriggerTemplate::TriggerTemplate()
{
    m_refCount = 1;                                  // +0x17C
    m_events = 0;                                    // +0x180
    m_radius = 0.0f;                                 // +0x184
    m_triggerId = -1;                                // +0x188
    m_chance = 0.0f;                                 // +0x18C
    m_averageTimeBetweenTests = 0.0f;                // +0x190
    m_minimumTimeBetweenTriggering = 1.0f;           // +0x194
    m_triggerForEachActivatingPlayer = false;        // +0x198
}

// (D1) / 65 B (D0): vptr restores, then BundleTemplate's dtor.
TriggerTemplate::~TriggerTemplate() {}

unsigned int TriggerTemplate::getClassID() const { return CID_TriggerTemplate; }

// the one-cid hop into Bundle's chain (the BundleTemplate-leaf shape, TemplateLeaves.cpp).
void* TriggerTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_TriggerTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}

// new(0x16C) Trigger(this).
IObject* TriggerTemplate::createObject() const
{
    return new Trigger(this);
}

// clamp to [0, 1] - `chance`'s SET arm; the GET arm reads the field raw.
void TriggerTemplate::setChance(float chance)
{
    if (chance < 0.0f)
        m_chance = 0.0f;
    else if (chance > 1.0f)
        m_chance = 1.0f;
    else
        m_chance = chance;
}

// The re-implemented quartet - the twelve-leaf shape (TemplateLeaves.cpp's BF2_LEAF_IBASE_IMPL),
// refcount at +0x17C; release deletes on the 1->0 edge through the D0 slot; queryInterface is the
// standard bundle-leaf table (IBase/IObjectTemplate -> this, ISimpleObjectTemplate -> +0x30,
// IBundleTemplate -> +0x164, IConsoleSaveable -> +0x08), which BundleTemplate's own answers.
void TriggerTemplate::addRef()  { ++m_refCount; }
int  TriggerTemplate::getRef()  { return m_refCount; }
int  TriggerTemplate::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}
void* TriggerTemplate::queryInterface(unsigned int iid) const { return BundleTemplate::queryInterface(iid); }

// Trigger

// (C2) / 0x81C19C0 (C1), 167 B, read in full: Bundle, the two vptrs, then the own state
// in the ctor's store order - refcount 1, template, then every dword +0x144..+0x164 zeroed (the two
// vectors' begin/end/cap and the two ticks), the +0x158 byte 0, and +0x168 = 0x759DC150.
Trigger::Trigger(const TriggerTemplate* objectTemplate)
    : Bundle(objectTemplate)
{
    m_refCount = 1;                                  // +0x13C
    m_template = objectTemplate;                     // +0x140
    m_lastTestTick = 0;                              // +0x144
    m_lastTriggerTick = 0;                           // +0x148
    // m_effectors (+0x14C..+0x154) and m_objectsInArea (+0x15C..+0x164): the ctor zeroes all six
    // words - an empty std::vector each, which is what their default construction is.
    _bool158 = false;                                // +0x158
    m_lastRandom = 0x759DC150;                       // +0x168
}

// (D1) / 0x81C1B90 (D0), 137 / 146 B: vptr restores, `if (triggerManager)
// removeTrigger(this)`, free the two vectors' storage, Bundle's dtor (D0 adds operator delete).
Trigger::~Trigger()
{
    if (triggerManager)
        triggerManager->removeTrigger(this);
    // m_objectsInArea and m_effectors free themselves - the ELF frees them by hand, in that order,
    // between the manager call and Bundle's dtor; the compiler's member teardown is the same order
    // (reverse declaration) and the same effect.
}

// The quartet - refcount at +0x13C, the ControlPoint arrangement.
void Trigger::addRef()  { ++m_refCount; }
int  Trigger::getRef()  { return m_refCount; }
int  Trigger::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}

// , three cases: IBase / IObject -> this; IConsoleSaveable -> the +0x80 sub-object; else 0.
void* Trigger::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)   return const_cast<Trigger*>(this);
    if (iid == IID_IObject) return const_cast<Trigger*>(this);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<Trigger*>(this));   // this + 0x80
    return 0;
}

// Bundle::init, then `if (triggerManager) triggerManager->addTrigger(this)`.
// GameLogic now constructs the world manager before level creation. Object.create lands here.
void Trigger::init()
{
    Bundle::init();
    if (triggerManager)
        triggerManager->addTrigger(this);
}

// each - both are bare forwards to the base the vtable already had there (Bundle has no
// destroy; SimpleObject's is what Bundle inherits). Written because the ELF overrides the slots.
void Trigger::destroy(bool force)                                    { SimpleObject::destroy(force); }
void Trigger::handleMessage(TemplateMessage message, IPlayer* player, bool flag)
                                                                     { Bundle::handleMessage(message, player, flag); }

// each: the template's events mask (+0x180) and radius (+0x184).
int   Trigger::getTypes() const  { return m_template->getEvents(); }
float Trigger::getRadius() const { return m_template->getRadius(); }

// |absolutePosition - point| < radius, strictly less (`fcomip` then `ja` on the
// radius side): a point exactly on the sphere is outside. sqrt is taken, not compared squared.
bool Trigger::isInArea(const Vec3& point) const
{
    const Vec3& position = getAbsolutePosition();
    const float dx = position.x - point.x;
    const float dy = position.y - point.y;
    const float dz = position.z - point.z;
    return m_template->getRadius() > std::sqrt(dz * dz + dy * dy + dx * dx);
}

// a trigger with no effectors is never valid; with effectors it is valid on the
// server unconditionally and on a client only while _bool158 is clear (see the header's note on
// that byte - it is never set, so in practice "has effectors").
bool Trigger::isValid()
{
    if (m_effectors.empty())
        return false;
    return gameLogic->isServer() || !_bool158;
}

// Performs an overflow-safe Lehmer multiplication modulo 2^31 - 1.
// Trigger rolls seed from the current tick, so repeated tests within one tick use the same value.
namespace {
inline unsigned int lehmerStep(unsigned int x, unsigned int factor)
{
    // (x * factor) mod (2^31 - 1), the ELF's exact operation order and widths.
    const unsigned int lo = (x & 0xFFFFu) * factor;             // movzx / imul
    const unsigned int hi = (x >> 16) * factor;                 // shr 16 / imul
    const int carry = static_cast<int>(lo >> 16) + static_cast<int>(hi);   // lea ebx,[edx+eax]
    unsigned int r = (lo & 0xFFFFu)
                   + ((static_cast<unsigned int>(carry) & 0x7FFFu) << 16)
                   + static_cast<unsigned int>(carry >> 15);
    const int reduced = static_cast<int>(r - 0x7FFFFFFFu);      // sub / cmovs
    return reduced < 0 ? r : static_cast<unsigned int>(reduced);
}
} // namespace

bool Trigger::tryRandomTrigger(float chance)
{
    if (!(chance < 1.0f))                            // ucomiss / jb: >= 1.0 (and NaN) -> true
        return true;
    unsigned int x = (static_cast<unsigned int>(WorldPref::getTickCount()) & 0x3FFu) + 3793797u;
    x = lehmerStep(x, 24112u);
    x = lehmerStep(x, 26143u);
    m_lastRandom = static_cast<int>(x);              // +0x168 stored, never read back
    const int value = ((static_cast<int>(x) >> 7) | 1) + 1;
    const float f = static_cast<float>(value) * 5.9604645e-08f + 0.0f;   // 2^-24, ds:0x8891064
    return chance > f;
}

// ELF81C1C60. Both diagnostics and false stores are retail. Query appends;
// repeated setup deliberately retains existing effectors.
void Trigger::findEffectors()
{
    _bool158 = false;
    if (!triggerableManager)
        Debug(static_cast<DebugType>(6), "Game/Objects/Game/Trigger.cpp", 61, "")
            << "Could not get triggerableManager" << "\n";
    if (!ambientEffectAreaManager)
        Debug(static_cast<DebugType>(6), "Game/Objects/Game/Trigger.cpp", 64, "")
            << "Could not get ambientEffectAreaManager" << "\n";
    if (triggerableManager)
    {
        triggerableManager->getTriggerablesWithTriggerId(m_template->m_triggerId, m_effectors);
        if (ambientEffectAreaManager)
            ambientEffectAreaManager->getAreasWithTriggerId(m_template->m_triggerId, m_effectors);
        for (auto it = m_effectors.begin(); it != m_effectors.end(); ++it)
        {
            IObject* object = *it;
            if (object_detail::LayoutAccess::objectTemplate(object)->getClassID() == CID_AmbientEffectAreaTemplate
                && !static_cast<AmbientEffectArea*>(object)->isNetworked())
            {
                _bool158 = false;
                break;
            }
        }
    }
}

// ELF81C1F10: exported namespace function, not an anonymous-namespace helper.
void getAllPlayers(IObject* object, std::list<IPlayer*>& players)
{
    IPlayerControlObject* control = static_cast<IPlayerControlObject*>(object->queryInterface(IID_IPlayerControlObject));
    if (control)
    {
        IPlayer* player = control->getOccupingPlayer();
        if (player && player->getIsAlive())
            players.push_back(player);
    }
    for (IObject* child = object->getChild(); child; child = object_detail::LayoutAccess::sibling(child))
        getAllPlayers(child, players);
}

// ELF table88A2080, bodies81C3530/81C35B0/81C35C0. Fourth slot inherited.
class PCOWithPlayerPredicator : public ObjectPredicator
{
public:
    virtual bool predicate(IObject* object)
    {
        std::list<IPlayer*> players;
        getAllPlayers(object, players);
        return players.size() != 0;
    }
    virtual unsigned int getFlags() const { return 0; }
    virtual bool performCulling(IObject*) { return true; }
};

// ELF81C2010 / PE5A07E0. No manager guard; the grid query owns output clearing.
bool Trigger::checkTriggerArea()
{
    PCOWithPlayerPredicator predicator;
    const Vec3& position = getAbsolutePosition();
    objectManager->getObjectsFromGrid(position, m_template->m_radius, m_objectsInArea, &predicator);
    return !m_objectsInArea.empty();
}

// ELF81C2120 / PE5A0B50. Unsigned subtraction, conversion before getTickTime.
// A NaN minimum does not prevent dispatch in either binary.
void Trigger::updateOnEvent(TriggerType type, IBase* source)
{
    const float ticks = static_cast<float>(static_cast<unsigned int>(WorldPref::getTickCount())
        - static_cast<unsigned int>(m_lastTriggerTick));
    const float elapsed = static_cast<float>(ticks * WorldPref::getTickTime());
    if (m_template->m_minimumTimeBetweenTriggering > elapsed)
        return;
    IBase* cause = 0;
    Vec3 position = {0.0f, 0.0f, 0.0f};
    if (type == TriggerType_Weapon)
    {
        // ELF primary +1C0 is GenericFireArm::getPlayer; PE uses its IWeaponObject
        // face at +170, slot +64. doFire is the sole type-2 producer in both ports.
        cause = static_cast<GenericFireArm*>(static_cast<IObject*>(source))->getPlayer();
    }
    else if (type == TriggerType_ControlPoint)
    {
        position = static_cast<IObject*>(source)->getAbsolutePosition();
        cause = source;
    }
    else if (type == TriggerType_Object)
        position = static_cast<IObject*>(source)->getAbsolutePosition();
    if (tryRandomTrigger(m_template->m_chance))
        executeTrigger(cause, position, -1);
}

// ELF81C2230 / PE5A0E50. Position precedes the player read. The for-each
// flag is tested even when handleTrigger's time/chance gate declines.
void Trigger::updateOnTime()
{
    if (!m_lastTestTick)
        m_lastTestTick = WorldPref::getTickCount();
    const float ticks = static_cast<float>(static_cast<unsigned int>(WorldPref::getTickCount())
        - static_cast<unsigned int>(m_lastTriggerTick));
    const float elapsed = static_cast<float>(ticks * WorldPref::getTickTime());
    if (m_template->m_minimumTimeBetweenTriggering > elapsed)
        return;
    std::vector<IObject*> unused; // Empty local in ELF81C2297..22A8.
    for (auto it = m_objectsInArea.begin(); it != m_objectsInArea.end(); ++it)
    {
        IObject* object = *it;
        const int events = m_template->m_events;
        if (events & 4)
        {
            IPhysicsNode* node = static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(object));
            if (!node)
                continue;
            const Vec3& speed = node->getAbsolutePositionalSpeed();
#if defined(_WIN32)
            // [PLATFORM] PE5A0EE5..0EFD sums y*y + z*z + x*x.
            const float length = std::sqrt(speed.y * speed.y + speed.z * speed.z + speed.x * speed.x);
#else
            // ELF81C22F2..230A sums x*x + y*y + z*z.
            const float length = std::sqrt(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z);
#endif
            if (!(length > 0.1f)) // Unordered is also skipped in both images.
                continue;
        }
        else if (!events)
            continue;
        IPlayerControlObject* control = static_cast<IPlayerControlObject*>(object->queryInterface(IID_IPlayerControlObject));
        if (control)
        {
            const Vec3& position = object->getAbsolutePosition();
            IPlayer* player = control->getOccupingPlayer();
            handleTrigger(player, position);
            if (!m_template->m_triggerForEachActivatingPlayer)
                break;
        }
    }
    m_lastTestTick = WorldPref::getTickCount();
}

// ELF81C2410 / PE5A0C30: no boolean return materialization; sole caller ignores EAX.
// This delta is signed, unlike the two minimum-interval gates.
void Trigger::handleTrigger(IPlayer* player, const Vec3& position)
{
    const int ticks = static_cast<int>(static_cast<unsigned int>(WorldPref::getTickCount())
        - static_cast<unsigned int>(m_lastTestTick));
    if (ticks <= 0)
        return;
    const float elapsed = static_cast<float>(static_cast<float>(ticks) * WorldPref::getTickTime());
    float average = m_template->m_averageTimeBetweenTests;
    if (average < 1.0f)
        average = 1.0f;
    const float fraction = m_template->m_chance / average;
    const float exponent = elapsed < 0.0f ? 0.0f : elapsed > 100.0f ? 100.0f : elapsed;
#if defined(_WIN32)
    // [PLATFORM] PE uses CRT _CIpow (double); ELF calls powf.
    const float chance = static_cast<float>(1.0 - std::pow(1.0 - fraction, static_cast<double>(exponent)));
#else
    const float chance = 1.0f - std::pow(1.0f - fraction, exponent);
#endif
    if (tryRandomTrigger(chance))
        executeTrigger(player, position, -1);
}

// ELF81C25F0 / PE5A0AC0. Only index -1 dispatches; other values still stamp.
// Keep both unused clock reads, and re-read vector storage after the virtual call.
void Trigger::executeTrigger(IBase* source, const Vec3& position, int effectorIndex)
{
    if (effectorIndex == -1)
    {
        for (std::size_t i = 0; i < m_effectors.size(); ++i)
        {
            const IObjectTemplate* objectTemplate = object_detail::LayoutAccess::objectTemplate(m_effectors[i]);
            const bool ambient = const_cast<IObjectTemplate*>(objectTemplate)->queryImplementation(
                CID_AmbientEffectAreaTemplate) != 0;
#if !DEDICATED
            // BF2.exe 0x601D3D..0x601D8F: test mode after the template query on every effector.
            if (!isDedicated() && !ambient)
                m_effectors[i]->handleMessage(
                    static_cast<TemplateMessage>(0), static_cast<IPlayer*>(source), false);
            else
#endif
            if (ambient)
                static_cast<AmbientEffectArea*>(m_effectors[i])->handleAmbientMessage(
                    static_cast<TemplateMessage>(0), static_cast<IPlayer*>(source), position);
        }
    }
    WorldPref::getTickCount();
    WorldPref::getTickTime();
    m_lastTriggerTick = WorldPref::getTickCount();
}

// ID/events/minimum interval always emit; events prints unsigned.
bool TriggerTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream)) return false;
    std::ostringstream out;
    out << "ObjectTemplate.triggerId " << m_triggerId << std::endl;
    if (m_radius != 0.0f) out << "ObjectTemplate.radius " << m_radius << std::endl;
    out << "ObjectTemplate.events " << static_cast<unsigned int>(m_events) << std::endl;
    if (m_averageTimeBetweenTests > 0.0f) out << "ObjectTemplate.averageTimeBetweenTests " << m_averageTimeBetweenTests << std::endl;
    if (m_triggerForEachActivatingPlayer) out << "ObjectTemplate.triggerForEachActivatingPlayer 1" << std::endl;
    if (m_chance != 0.0f) out << "ObjectTemplate.chance " << m_chance << std::endl;
    out << "ObjectTemplate.minimumTimeBetweenTriggering " << m_minimumTimeBetweenTriggering << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

} // namespace world
} // namespace hfe
} // namespace dice
