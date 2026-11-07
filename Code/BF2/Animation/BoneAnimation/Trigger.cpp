// Animation trigger lifecycle, tree structure, playback, and script serialization.

#include "Trigger.h"

#include "AnimationSystem.h"
#include "Bundle.h"
#include "TriggerManager.h"
#include "ValueHolder.h"
#include "BundlePlayer.h"
#include "../../Game/Utility/Vars.h"      // Vars::getFloat - the three cvar handles
#include <cmath>                          // asinf / fabsf
#include <cstdlib>                        // rand - Random / Idle triggers
#include <sstream>
#include "../../IO/Stream/IStream.h"
#include "../../Platform/MsvcRand.h"  // [CHOSEN, not recovered] the client CRT's rand (MSVCR71)

namespace dice {
namespace anim {


// Console-backed movement phase seeds; turn triggers also use the left offset.

namespace {
float* const anim_test             = hfe::Vars::getFloat("anim_test", 0.25f);
float* const anim_startLeftOffset  = hfe::Vars::getFloat("anim_startLeftOffset", 0.2f);
float* const anim_startRightOffset = hfe::Vars::getFloat("anim_startRightOffset", 0.7f);
} // namespace


// Initializes an empty trigger; managers and parent relationships are resolved externally.

Trigger::Trigger()
    : m_prio(0)
    , fadeInTime(0.0f)
    , m_isChild(false)
    , m_underIdle(false)
    , m_alreadySaved(false)
{
}


// Destroys only the containers. Delete through TriggerManager to release the child subtree; bundles belong to BundleManager.

Trigger::~Trigger()
{
}

bool Trigger::getAlreadySaved() const        { return m_alreadySaved; }
void Trigger::setAlreadySaved(bool saved)    { m_alreadySaved = saved; }
Trigger::TriggerType Trigger::getType() const { return TRIGGERTYPE_BASE; }  // `return 0`
std::string Trigger::getTypeName() const     { return std::string("Trigger"); }

// Binary serialization is intentionally empty for all triggers.
void Trigger::readBinary(hfe::io::IStream* /*stream*/)  { }
void Trigger::writeBinary(hfe::io::IStream* /*stream*/) { }

// Empty on the base; MovementTrigger is the only override.
void Trigger::activate() { }


// Attaches a child trigger and updates its relationship to this trigger.

void Trigger::addChild(Trigger* child)
{
    m_children.push_back(child);
    child->m_isChild = true;

    if (getType() == TRIGGERTYPE_IDLE || m_underIdle)
        child->m_underIdle = true;                 // one level only - see above
}


// Resolves trigger and bundle names through the owning managers.

void Trigger::addChild(const std::string& name)
{
    AnimationSystemTemplate* templ = animationSystemTemplateManager->getActive();
    Trigger* child = templ->getTriggerManager()->getTriggerByName(name, false);
    if (child)
        addChild(child);                           // through the vtable, +0x2C
}

void Trigger::removeChild(const std::string& name)
{
    AnimationSystemTemplate* templ = animationSystemTemplateManager->getActive();
    Trigger* child = templ->getTriggerManager()->getTriggerByName(name, false);
    if (child)
        removeChildByPtr(child);                   // through the vtable, +0x38
}

// Find, clear the child's flag, erase.
// it clears m_isChild even if the child is still someone else'S. Nothing recovered builds that
// shape - a trigger with two parents - but the flag is per-child, not per-edge, so it could not
// survive one.
void Trigger::removeChildByPtr(Trigger* child)
{
    if (!child)
        return;

    for (std::vector<Trigger*>::iterator it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (*it == child)
        {
            child->m_isChild = false;              // see above
            m_children.erase(it);
            return;
        }
    }
}

// No duplicate check here either.
void Trigger::addBundle(Bundle* bundle)
{
    m_bundles.push_back(bundle);
}

void Trigger::addBundle(const std::string& name)
{
    AnimationSystemTemplate* templ = animationSystemTemplateManager->getActive();
    Bundle* bundle = templ->getBundleManager()->getBundleByName(name);
    if (bundle)
        addBundle(bundle);                         // through the vtable, +0x3C
}

void Trigger::removeBundle(const std::string& name)
{
    AnimationSystemTemplate* templ = animationSystemTemplateManager->getActive();
    Bundle* bundle = templ->getBundleManager()->getBundleByName(name);
    if (bundle)
        removeBundleByPtr(bundle);                 // through the vtable, +0x48
}

// Erases the first match only, which matters given addBundle allows duplicates.
void Trigger::removeBundleByPtr(Bundle* bundle)
{
    if (!bundle)
        return;

    for (std::vector<Bundle*>::iterator it = m_bundles.begin(); it != m_bundles.end(); ++it)
    {
        if (*it == bundle)
        {
            m_bundles.erase(it);
            return;
        }
    }
}


// Plays every bundle at this trigger's priority, using depth-first priority ordering.

void Trigger::applyAnimations(AnimationSystem& system)
{
    for (std::vector<Bundle*>::iterator it = m_bundles.begin(); it != m_bundles.end(); ++it)
        system.playBundle(this, *it, m_prio, -1.0f, 1.0f);
}

// PoseTrigger / RandomTrigger - no members, no constructor
Trigger::TriggerType PoseTrigger::getType() const   { return TRIGGERTYPE_POSE; }
std::string PoseTrigger::getTypeName() const        { return std::string("PoseTrigger"); }
Trigger::TriggerType RandomTrigger::getType() const { return TRIGGERTYPE_RANDOM; }
std::string RandomTrigger::getTypeName() const      { return std::string("RandomTrigger"); }

// Starts the next idle time at the minimum of its 5-10 second range.
IdleTrigger::IdleTrigger()
    : m_idleTimeMin(5.0f)
    , m_idleTimeMax(10.0f)
    , m_nextIdleTime(5.0f)
{
}
Trigger::TriggerType IdleTrigger::getType() const { return TRIGGERTYPE_IDLE; }
std::string IdleTrigger::getTypeName() const      { return std::string("IdleTrigger"); }

MessageTrigger::MessageTrigger()
    : m_message(0)
{
}
Trigger::TriggerType MessageTrigger::getType() const { return TRIGGERTYPE_MESSAGE; }
std::string MessageTrigger::getTypeName() const      { return std::string("MessageTrigger"); }

// Appends through the base implementation, including calls dispatched from the name overload.
void MessageTrigger::addBundle(Bundle* bundle)
{
    Trigger::addBundle(bundle);
}

std::string SwitchMessageTrigger::getTypeName() const { return std::string("SwitchMessageTrigger"); }

MovementTrigger::MovementTrigger()
    : m_valueHolder(0)
    , m_triggerOnAcceleration(false)
    , m_useDirection(false)
{
}
Trigger::TriggerType MovementTrigger::getType() const { return TRIGGERTYPE_MOVEMENT; }
std::string MovementTrigger::getTypeName() const      { return std::string("MovementTrigger"); }


// Selects this trigger's value holder for console operations. With no holder, the previous selection remains active.

void MovementTrigger::activate()
{
    if (m_valueHolder)
        valueHolderManager->setActive(m_valueHolder);   // no else-clear - see above
}

// `get`, not createValueHolder - a name ValueHolders.inc never declared silently
// yields a null holder, and the trigger keeps working with nothing to read.
void MovementTrigger::setValueHolder(const std::string& name)
{
    m_valueHolder = valueHolderManager->get(name);
}

// The round trip, and it inherits findName's "ValueHolder not found" on a miss.
const std::string& MovementTrigger::getValueHolder() const
{
    return valueHolderManager->findName(m_valueHolder);
}

void MovementTrigger::clearValueHolder()                    { m_valueHolder = 0; }
void MovementTrigger::setTriggerOnAcceleration(bool on)     { m_triggerOnAcceleration = on; }
bool MovementTrigger::getTriggerOnAcceleration() const      { return m_triggerOnAcceleration; }
void MovementTrigger::setUseDirection(bool on)              { m_useDirection = on; }
bool MovementTrigger::getUseDirection() const               { return m_useDirection; }

std::string ForwardTrigger::getTypeName() const    { return std::string("ForwardTrigger"); }
std::string SideTrigger::getTypeName() const       { return std::string("SideTrigger"); }
std::string UpTrigger::getTypeName() const         { return std::string("UpTrigger"); }
std::string TurnTrigger::getTypeName() const       { return std::string("TurnTrigger"); }
std::string LookAroundTrigger::getTypeName() const { return std::string("LookAroundTrigger"); }


// Trigger evaluation and animation application.

bool Trigger::update(AnimationSystem& system)
{
    if (fadeInTime > 0.0f)
        system.setFadeInTime(this);

    for (std::vector<Trigger*>::iterator it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (!(*it)->update(system))                     // vtable +0x4C
            return false;
    }
    applyAnimations(system);                            // vtable +0x50
    return true;
}

// the pose indexes the children (clamped to the last one), and the child must
// agree before this trigger's bundles play. Calls the base's applyAnimations non-virtually.
bool PoseTrigger::update(AnimationSystem& system)
{
    if (!m_children.empty())
    {
        unsigned int index = system.m_triggerPose;                          // +0x04
        const unsigned int count = static_cast<unsigned int>(m_children.size());
        if (index >= count)
            index = count - 1;
        if (!m_children[index]->update(system))
            return false;
    }
    Trigger::applyAnimations(system);
    return true;
}

// One bundle, picked by rand() % count.
void RandomTrigger::applyAnimations(AnimationSystem& system)
{
    if (m_bundles.empty())
        return;
    const unsigned int pick = static_cast<unsigned int>(bf2crt::rand()) % static_cast<unsigned int>(m_bundles.size());
    system.playBundle(this, m_bundles[pick], m_prio, -1.0f, 1.0f);
}


// Runs a random idle bundle and child triggers once the selected threshold is reached within the idle window.
// Chooses the next threshold from the configured range.

bool IdleTrigger::update(AnimationSystem& system)
{
    const float idle = system.getIdleTime();
    if (idle != 0.0f && m_idleTimeMin < m_idleTimeMax)
    {
        if (m_nextIdleTime > system.getIdleTime() || system.getIdleTime() > m_idleTimeMax)
            return true;

        m_nextIdleTime = static_cast<float>(bf2crt::rand() % 100) / 100.0f * (m_idleTimeMax - m_idleTimeMin)
                       + m_idleTimeMin;
        if (!m_bundles.empty())
        {
            const unsigned int pick =
                static_cast<unsigned int>(bf2crt::rand()) % static_cast<unsigned int>(m_bundles.size());
            system.playBundle(this, m_bundles[pick], m_prio, -1.0f, 1.0f);
        }
        return Trigger::update(system);
    }
    return true;
}

// No message means "always"; otherwise the mask must be set this frame.
bool MessageTrigger::update(AnimationSystem& system)
{
    const int message = m_message;
    if (message == 0)
        return Trigger::update(system);
    if ((system.m_triggerMessages & message) != 0)                           // +0x08
        return Trigger::update(system);
    return true;
}


// Selects a child from message state and plays the first bundle forward or backward on an edge.
// Transition playback uses priority 10000, stopping other bundles.

bool SwitchMessageTrigger::update(AnimationSystem& system)
{
    if (!m_children.empty())
    {
        unsigned int index = 0;
        if ((system.m_triggerMessages & m_message) == 0)
            index = (m_children.size() >= 2) ? 1u : 0u;
        if (!m_children[index]->update(system))
            return false;
    }
    if (!m_bundles.empty())
    {
        const int now = system.m_triggerMessages & m_message;                // +0x08
        if (now != (system.m_lastTriggerMessages & m_message))               // +0x0C
        {
            const float speed = now ? 1.0f : -1.0f;
            system.playBundle(this, m_bundles[0], 10000, 1.0f, speed);
        }
    }
    return true;
}


// No holder or equal bounds always passes. A negative minimum reverses the comparison bounds:
// for a backward range, min must be nearer zero than max.

bool MovementTrigger::isWithinRange(float value) const
{
    const ValueHolder* const holder = m_valueHolder;
    if (holder)
    {
        float high = holder->max;
        if (holder->min != high)
        {
            float low;
            if (holder->min < 0.0f)
            {
                low = holder->max;
                high = holder->min;
            }
            else
                low = holder->min;
            if (low > value || value > high)
                return false;
        }
    }
    return true;
}


// Requires all passOnMessage bits and no stopOnMessage bits, then tests movement speed.
// A missing holder imposes no message gate.

bool MovementTrigger::update(AnimationSystem& system)
{
    const ValueHolder* const holder = m_valueHolder;
    if (!holder
        || ((holder->passOnMessage == 0 || (system.m_triggerMessages & holder->passOnMessage) != 0)
            && (holder->stopOnMessage == 0 || (system.m_triggerMessages & holder->stopOnMessage) == 0)))
    {
        if (isWithinRange(system.m_movementSpeed))                           // +0x20 vtable +0x70
            return Trigger::update(system);
    }
    return true;
}


// Applies directionally blended movement animations.

void MovementTrigger::applyAnimations(AnimationSystem& system)
{
    if (m_bundles.empty())
        return;

    const ValueHolder* const holder = m_valueHolder;
    if (!holder && m_bundles.size() == 2)
    {
        const float threshold = *anim_test;
        Bundle* pick;
        if (threshold > system.m_movementPhase)
        {
            system.m_movementPhase = *anim_startLeftOffset;
            pick = m_bundles[0];
        }
        else if (system.m_movementPhase <= 1.0f - threshold)
        {
            system.m_movementPhase = *anim_startRightOffset;
            pick = m_bundles[1];
        }
        else
        {
            system.m_movementPhase = *anim_startLeftOffset;
            pick = m_bundles[0];
        }
        system.playBundle(this, pick, m_prio, -1.0f, 1.0f);
        return;
    }

    float speed = 1.0f;
    if (holder && holder->normal != 0.0f)
        speed = system.m_movementSpeed / holder->normal;

    BundlePlayer* const player = system.playBundle(this, m_bundles[0], m_prio, -1.0f, speed);
    if (player->m_bundle->isLooping)
        player->m_isMovement = true;

    int animationCount = 0;
    const Bundle* const bundle = m_bundles[0];
    for (Bundle::AnimationList::const_iterator it = bundle->m_animations.begin();
         it != bundle->m_animations.end(); ++it)
        ++animationCount;

    if (animationCount == 4)
    {
        float forward = fabsf(system.m_movement.z);                          // +0x1C
        if (forward < -1.0f)
            forward = -1.0f;
        else if (forward > 1.0f)
            forward = 1.0f;
        const float blend = asinf(forward) * 0.63661975f;                    // * 2/pi
        const bool backward = system.m_movement.z < -0.01f;
        const int side = (system.m_movement.x < 0.01f ? 1 : 0) + 2;           // +0x14
        if (player->m_blendIndexA == -1)
            player->m_time = system.m_movementPhase * player->m_bundle->length;
        player->setBlendAnimations(blend, side, backward ? 1 : 0);

        const float sideComponent = system.m_movement.x;
        system.m_movementPhase = player->m_time / player->m_bundle->length;
        if (sideComponent < -0.5f)
            system.m_movementPhase = *anim_startRightOffset;
        else if (sideComponent > 0.5f)
            system.m_movementPhase = *anim_startLeftOffset;
    }
}

// Tests forward velocity or acceleration and plays every bundle.
bool ForwardTrigger::update(AnimationSystem& system)
{
    float value = system.m_movementSpeed * system.m_movement.z;              // +0x20 * +0x1C
    if (m_triggerOnAcceleration)
        value = system.m_movementRate.z;                                     // +0x2C
    if (isWithinRange(value))
        return Trigger::update(system);
    return true;
}

void ForwardTrigger::applyAnimations(AnimationSystem& system)
{
    if (m_bundles.empty())
        return;
    const ValueHolder* const holder = m_valueHolder;
    float speed = 1.0f;
    if (holder && holder->normal != 0.0f)
        speed = system.m_movementSpeed / holder->normal;
    for (std::vector<Bundle*>::iterator it = m_bundles.begin(); it != m_bundles.end(); ++it)
    {
        BundlePlayer* const player = system.playBundle(this, *it, m_prio, -1.0f, speed);
        if (player->m_bundle->isLooping)
            player->m_isMovement = true;
    }
}

// Tests side velocity or acceleration; useDirection tests magnitude.
// Playback with useDirection scales speed by the vertical movement component.
bool SideTrigger::update(AnimationSystem& system)
{
    float value;
    if (m_triggerOnAcceleration)
        value = system.m_movementRate.x;                                     // +0x24
    else
        value = system.m_movementSpeed * system.m_movement.x;               // +0x20 * +0x14
    if (m_useDirection)
        value = fabsf(value);
    if (isWithinRange(value))
        return Trigger::update(system);
    return true;
}

void SideTrigger::applyAnimations(AnimationSystem& system)
{
    if (m_bundles.empty())
        return;
    const ValueHolder* const holder = m_valueHolder;
    float speed = 1.0f;
    if (holder && holder->normal != 0.0f)
        speed = system.m_movementSpeed / holder->normal;
    if (m_useDirection)
        speed = speed * system.m_movement.y;                                 // +0x18 see above
    for (std::vector<Bundle*>::iterator it = m_bundles.begin(); it != m_bundles.end(); ++it)
    {
        BundlePlayer* const player = system.playBundle(this, *it, m_prio, -1.0f, speed);
        if (player->m_bundle->isLooping)
            player->m_isMovement = true;
    }
}

// Tests vertical movement or its rate. Directional playback retains the vertical sign, so falling plays backward.
bool UpTrigger::update(AnimationSystem& system)
{
    float value;
    if (m_triggerOnAcceleration)
        value = system.m_movementRate.y;                                     // +0x28
    else
        value = system.m_movement.y;                                         // +0x18
    if (m_useDirection)
        value = fabsf(value);
    if (isWithinRange(value))
        return Trigger::update(system);
    return true;
}

void UpTrigger::applyAnimations(AnimationSystem& system)
{
    if (m_bundles.empty())
        return;
    const ValueHolder* const holder = m_valueHolder;
    float speed = 1.0f;
    if (holder && holder->normal > 0.0f)
        speed = fabsf(system.m_movement.y / holder->normal);
    if (m_useDirection)
    {
        float sign = 1.0f;
        if (system.m_movement.y <= 0.0f)
        {
            sign = -1.0f;
            if (system.m_movement.y >= 0.0f)
                sign = 0.0f;
        }
        speed = speed * sign;
    }
    for (std::vector<Bundle*>::iterator it = m_bundles.begin(); it != m_bundles.end(); ++it)
    {
        BundlePlayer* const player = system.playBundle(this, *it, m_prio, -1.0f, speed);
        if (player->m_bundle->isLooping)
            player->m_isMovement = true;
    }
}

// Tests look.y and plays bundles at absolute turn rate / normal, resetting phase to the left offset.
bool TurnTrigger::update(AnimationSystem& system)
{
    if (isWithinRange(system.m_look.y))                                      // +0x34
        return Trigger::update(system);
    return true;
}

void TurnTrigger::applyAnimations(AnimationSystem& system)
{
    if (m_bundles.empty())
        return;
    const ValueHolder* const holder = m_valueHolder;
    float speed = 1.0f;
    if (holder && holder->normal != 0.0f)
        speed = fabsf(system.m_look.y) / holder->normal;
    for (std::vector<Bundle*>::iterator it = m_bundles.begin(); it != m_bundles.end(); ++it)
        system.playBundle(this, *it, m_prio, -1.0f, speed);
    system.m_movementPhase = *anim_startLeftOffset;
}

// Maps clamped look.y to playback speed and look.x to the blend of animations 0 and 1.
bool LookAroundTrigger::update(AnimationSystem& system)
{
    return Trigger::update(system);
}

void LookAroundTrigger::applyAnimations(AnimationSystem& system)
{
    if (m_bundles.empty())
        return;
    float yaw = system.m_look.x;                                             // +0x30
    if (yaw < -1.0f)
        yaw = -1.0f;
    else if (yaw > 1.0f)
        yaw = 1.0f;
    float pitch = system.m_look.y;                                           // +0x34
    if (pitch < -1.0f)
        pitch = -1.0f;
    else if (pitch > 1.0f)
        pitch = 1.0f;
    const float speed = pitch * 0.5f + 0.5f;
    for (std::vector<Bundle*>::iterator it = m_bundles.begin(); it != m_bundles.end(); ++it)
    {
        BundlePlayer* const player = system.playBundle(this, *it, m_prio, -1.0f, speed);
        player->setBlendAnimations(yaw * 0.5f + 0.5f, 0, 1);
    }
}

// Marks saved before fetching managers. Preserve the two separate getActive calls and their order.
void Trigger::makeScript(hfe::io::IStream* stream)
{
    m_alreadySaved = true;
    std::ostringstream output;
    TriggerManager* const triggers = animationSystemTemplateManager->getActive()->getTriggerManager();
    BundleManager* const bundles = animationSystemTemplateManager->getActive()->getBundleManager();
    const std::string& name = triggers->getName(this);
    output << "animationSystem.createTrigger " << getTypeName() << " " << name << std::endl;
    if (fadeInTime > 0.0f) output << "animationTrigger.fadeInTime " << fadeInTime << std::endl;
    for (auto it = m_children.begin(); it != m_children.end(); ++it)
        output << "animationTrigger.addChild " << triggers->getName(*it) << std::endl;
    for (auto it = m_bundles.begin(); it != m_bundles.end(); ++it)
        output << "animationTrigger.addBundle " << bundles->getBundleName(*it) << std::endl;
    const std::string text = output.str();
    stream->write(text.c_str(), static_cast<unsigned int>(text.size()));
}

// . Resolve the active manager anew for each bundle.
std::string Trigger::listBundles() const
{
    std::stringstream output;
    for (auto it = m_bundles.begin(); it != m_bundles.end(); ++it)
        output << animationSystemTemplateManager->getActive()->getBundleManager()->getBundleName(*it) << "\n";
    return output.str();
}

// . The base and derived blocks are separate writes.
void IdleTrigger::makeScript(hfe::io::IStream* stream)
{
    Trigger::makeScript(stream);
    std::ostringstream output;
    output << "animationTrigger.idleTime " << m_idleTimeMin << '/' << m_idleTimeMax << std::endl;
    const std::string text = output.str();
    stream->write(text.c_str(), static_cast<unsigned int>(text.size()));
}

// . Message is a signed int, including its high-bit values.
void MessageTrigger::makeScript(hfe::io::IStream* stream)
{
    Trigger::makeScript(stream);
    std::ostringstream output;
    output << "animationTrigger.message " << m_message << std::endl;
    const std::string text = output.str();
    stream->write(text.c_str(), static_cast<unsigned int>(text.size()));
}

// . Acceleration is emitted only inside the holder-present arm;
// direction is independent. The second write occurs even when its string is empty.
void MovementTrigger::makeScript(hfe::io::IStream* stream)
{
    Trigger::makeScript(stream);
    std::ostringstream output;
    if (m_valueHolder)
    {
        output << "animationTrigger.valueHolder " << getValueHolder() << std::endl;
        if (m_triggerOnAcceleration)
            output << "animationTrigger.triggerOnAcceleration " << m_triggerOnAcceleration << std::endl;
    }
    if (m_useDirection) output << "animationTrigger.useDirection 1" << std::endl;
    const std::string text = output.str();
    stream->write(text.c_str(), static_cast<unsigned int>(text.size()));
}

} // namespace anim
} // namespace dice
