// Wheel-spring integration and wheel rotation. [INFERRED] Source path.

#include "PhysicsSpring.h"

#include <math.h>

#include "BasicPhysicsSystem.h"                              // basicPhysicsSystem->getGravity (+0x20)
#include "IResponsePhysics.h"                                // get/setPermanentGrip (+0x80 / +0x84)
#include "../Core/Debug.h"                                   // the "not mobile" abort
#include "../Core/Ids.h"
#include "../Game/Objects/EngineTemplate.h"                  // the engine template's differential (+0x3CC)
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IObjectTemplate.h"                 // getName, for that abort
#include "../Game/Objects/Game/EngineBase.h"                 // the driven-wheel branch
#include "../Game/Objects/Game/Spring.h"                     // m_occupied (+0x13E), getEngine (+0x180)
#include "../Game/Objects/ObjectLayoutAccess.h"              // the object's +0x04 flags, +0x44, +0x48
#include "../Game/Objects/TemplateLeaves.h"                  // SpringTemplate::getStrength / getDamping
#include "../Game/Utility/Vars.h"
#include "../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace world {

extern IBase* basicPhysicsSystem;                            // ds:0x8B2784C - Core/Globals.cpp

namespace {
    // Shared sleep overrides and spring-integration tuning handles, initialized in their original order.
    int* const   g_phyForceSleeping = hfe::Vars::getInt("phy-force-sleeping", 0);     // ds:0x8AB0840
    int* const   g_phyForceAwake    = hfe::Vars::getInt("phy-force-awake", 0);        // ds:0x8AB0844
    float* const g_phyFriWheel      = hfe::Vars::getFloat("phy-fri-wheel", 0.99f);    // ds:0x8AB0848
    float* const g_phyFriWheel2     = hfe::Vars::getFloat("phy-fri-wheel2", 1.0f);    // ds:0x8AB084C
    float* const g_wheelInertia     = hfe::Vars::getFloat("wheel-inertia", 0.1f);     // ds:0x8AB0850
}

// PhysicsNode(); the vptr; the own block zeroed, the count 1, the template
// remembered - and PhysicsNode's fixToParent (+0x11C) cleared.
PhysicsSpring::PhysicsSpring(const SpringTemplate* springTemplate, const IObject*)
    : PhysicsNode()
{
    m_refCount = 1;                                                                  // +0x22C
    m_relationToParentPosition.x = m_relationToParentPosition.y = m_relationToParentPosition.z = 0.0f;   // +0x234
    m_deflection.x = m_deflection.y = m_deflection.z = 0.0f;         // +0x240
    m_float24C = 0.0f;
    m_wheelSpin = 0.0f;
    m_wheelAngle = 0.0f;
    m_fixToParent = false;                                                           // +0x11C
    m_template = springTemplate;                                                     // +0x230
}

PhysicsSpring::~PhysicsSpring() {}                                                   // 0x82AB820: ~PhysicsNode

// The quartet - 0x82AB870 / 0x82AB890 / 0x82AB8A0 / 0x82AB8E0.
void PhysicsSpring::addRef()  { ++m_refCount; }
int  PhysicsSpring::getRef()  { return m_refCount; }
int  PhysicsSpring::release()
{
    const int left = m_refCount - 1;
    const bool last = (m_refCount == 1);
    m_refCount = left;
    if (last)
        delete this;                                                                 // vslot +0x14
    return left;
}

void* PhysicsSpring::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IPhysicsNode)
        return const_cast<PhysicsSpring*>(this);
    return 0;
}


void PhysicsSpring::setRelationToParentPosition(const Vec3& position)
{
    m_relationToParentPosition = position;
}

// ELF 0x082AA770 / PE 0x00683970. The inherited reset(bool) does not clear this state.
void PhysicsSpring::reset()
{
    PhysicsNode::reset(true);
    m_deflection.x = m_deflection.y = m_deflection.z = 0.0f;
    m_float24C = 0.0f;
    m_wheelSpin = 0.0f;
    m_wheelAngle = 0.0f;
}

// The sleep quartet. phy-force-awake == 1 pins the node awake (sleepiness = the timeout, the object
// ticks); phy-force-sleeping == 1 pins it asleep. Otherwise: going to sleep clears the object's
// 0x800 tick flag, and setSleepiness(<= 0) on a ticking object additionally seats the object at
// relation - parentInverse3x3 * defaultPosition (its rest position); waking raises the flag when
// the node was asleep and reloads the timeout.
void PhysicsSpring::setIsSleeping()
{
    if (*g_phyForceAwake == 1)
    {
        m_sleepiness = m_sleepTimeout;                                               // +0x68 <- +0x64
        m_object->updateFlags(0x800u, 0u);                                           // +0x10 object
        return;
    }
    m_sleepiness = 0;
    m_object->updateFlags(0u, 0x800u);
    m_wheelAngle = 0.0f;
}

void PhysicsSpring::setSleepiness(int sleepiness)
{
    if (*g_phyForceAwake == 1)
    {
        m_sleepiness = m_sleepTimeout;
        m_object->updateFlags(0x800u, 0u);
        return;
    }
    if (*g_phyForceSleeping == 1)
    {
        m_sleepiness = 0;
        m_object->updateFlags(0u, 0x800u);
        return;
    }
    if (sleepiness <= 0)
    {
        if (object_detail::LayoutAccess::flags(m_object) & 0x800u)                   // object +0x04
        {
            m_object->updateFlags(0u, 0x800u);
            const Mat4& inv = m_object->getParent()->getInvAbsoluteTransformation();  // parent's slot 34
            const float* m = inv.m;
            Vec3 d;
            d.x = m[0] * m_deflection.x + m[4] * m_deflection.y + m[8]  * m_deflection.z;
            d.y = m[1] * m_deflection.x + m[5] * m_deflection.y + m[9]  * m_deflection.z;
            d.z = m[2] * m_deflection.x + m[6] * m_deflection.y + m[10] * m_deflection.z;
            Vec3 rest;
            rest.x = m_relationToParentPosition.x - d.x;
            rest.y = m_relationToParentPosition.y - d.y;
            rest.z = m_relationToParentPosition.z - d.z;
            m_object->setRelativePosition(rest);                                     // slot 57
            m_wheelAngle = 0.0f;
        }
    }
    else
    {
        if (!(object_detail::LayoutAccess::flags(m_object) & 0x800u))
            m_object->updateFlags(0x800u, 0u);
    }
    m_sleepiness = sleepiness;
}

void PhysicsSpring::setIsSleepier()
{
    if (*g_phyForceAwake == 1)
        return;
    if (m_sleepiness > 1)
    {
        --m_sleepiness;
    }
    else if (m_sleepiness == 1)
    {
        m_sleepiness = 0;
        m_object->updateFlags(0u, 0x800u);
        m_wheelAngle = 0.0f;
    }
}

void PhysicsSpring::setIsAwake()
{
    if (*g_phyForceSleeping == 1)
        return;
    if (m_sleepiness <= 0)
        m_object->updateFlags(0x800u, 0u);
    m_sleepiness = m_sleepTimeout;
}

// with a positive template strength the rest deflection is
// -gravity / (4 strength) * 1.5, else 0.15; clamped to [0, 1] and scaled by the static `mod` (-1.0
// in the image); stored as the y of a (0, y, 0) default position, which is returned.
const Vec3& PhysicsSpring::calcDefaultPosition(const SpringTemplate* springTemplate, const IObject*)
{
    static const float mod = -1.0f;                                                  // calcDefaultPosition(...)::mod
    const float strength = springTemplate->getStrength();                            // template +0x198
    float y;
    if (strength > 0.0f)
        y = -static_cast<IPhysicsSystem*>(basicPhysicsSystem)->getGravity() / (strength * 4.0f) * 1.5f;   // +0x20
    else
        y = 0.15000001f;
    if (y < 0.0f)      y = 0.0f;
    else if (y > 1.0f) y = 1.0f;
    m_deflection.x = 0.0f;
    m_deflection.z = 0.0f;
    m_deflection.y = y * mod;
    return m_deflection;
}

// the object's physics bounding radius, at least 0.1.
float PhysicsSpring::getWheelRadius() const
{
    const float r = m_object->getPhysicBoundingRadius();                             // slot 38
    return r >= 0.1f ? r : 0.1f;
}

// Applies suspension force to the root, reseats the wheel and advances its spin for dt.
// Follows root sleepiness with a one-tick offset; sleeping springs do no work.
// Unoccupied gripping wheels lose spin. Geared wheel spin follows differential RPM through inertia;
// only that branch converts linear speed by the wheel radius. Preserve the anchor/arm arithmetic.
void PhysicsSpring::updatePhysics(float dt)
{
    const Spring* spring = static_cast<const Spring*>(m_object);

    IResponsePhysics* response =
        static_cast<IResponsePhysics*>(object_detail::LayoutAccess::responsePhysics(m_object));   // object +0x48
    if (response && (response->getPermanentGrip() & 0x10) != 0)                      // +0x80
    {
        if (spring->m_occupied)                                                      // object +0x13E
        {
            response->setPermanentGrip(20);                                          // +0x84
        }
        else
        {
            response->setPermanentGrip(18);
            m_wheelSpin = 0.89999998f * m_wheelSpin;
        }
    }

    IPhysicsNode* root =
        static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(getRootParent(m_object)));   // root +0x44
    if (!root)
        return;

    if (!root->getIsMobile())                                                        // +0x134
    {
        Debug(DEBUGTYPE_6, "Game/Objects/Vehicle/PhysicsSpring.cpp", 107, "")
            << "Root parent \"" << getRootParent(m_object)->getTemplate()->getName()
            << "\" not mobile, but have a child of type spring.";
    }

    const int follow = root->getSleepiness() - 1;                                    // +0x128
    setSleepiness(follow >= 0 ? follow : 0);
    if (getIsSleeping())                                                             // +0x11C
        return;

    const Vec3 previous = m_deflection;                                              // last tick's

    // The anchor: the parent's 3x3 applied to the relation, plus the parent's position.
    const float* const pm = m_object->getParent()->getAbsoluteTransformation().m;    // parent's slot 33
    Vec3 offset;
    offset.x = pm[0] * m_relationToParentPosition.x + pm[4] * m_relationToParentPosition.y
             + pm[8]  * m_relationToParentPosition.z;
    offset.y = pm[1] * m_relationToParentPosition.x + pm[5] * m_relationToParentPosition.y
             + pm[9]  * m_relationToParentPosition.z;
    offset.z = pm[2] * m_relationToParentPosition.x + pm[6] * m_relationToParentPosition.y
             + pm[10] * m_relationToParentPosition.z;

    const Vec3& parentPosition = m_object->getParent()->getAbsolutePosition();        // slot 27
    Vec3 anchor;
    anchor.x = parentPosition.x + offset.x;
    anchor.y = parentPosition.y + offset.y;
    anchor.z = parentPosition.z + offset.z;

    // The arm the force is applied at, measured from the root.
    const Vec3& rootPosition = root->getAbsolutePosition();                          // +0x48
    Vec3 arm;
    arm.x = anchor.x - rootPosition.x;
    arm.y = anchor.y - rootPosition.y;
    arm.z = anchor.z - rootPosition.z;

    // and straight back again - see the header block.
    const Vec3& rootPositionAgain = root->getAbsolutePosition();
    anchor.x = rootPositionAgain.x + arm.x;
    anchor.y = rootPositionAgain.y + arm.y;
    anchor.z = rootPositionAgain.z + arm.z;

    const Vec3& wheelPosition = m_object->getAbsolutePosition();                     // slot 27
    m_deflection.x = anchor.x - wheelPosition.x;
    m_deflection.y = anchor.y - wheelPosition.y;
    m_deflection.z = anchor.z - wheelPosition.z;

    // The wheel goes back to its rest offset - identity, translated by the relation.
    Mat4 seat = Mat4::getIdentity();
    seat.m[12] = m_relationToParentPosition.x;
    seat.m[13] = m_relationToParentPosition.y;
    seat.m[14] = m_relationToParentPosition.z;
    m_object->setRelativeTransformation(seat);                                       // slot 59

    const float invDt = 1.0f / dt;
    Vec3 velocity;
    velocity.x = (m_deflection.x - previous.x) * invDt;
    velocity.y = (m_deflection.y - previous.y) * invDt;
    velocity.z = (m_deflection.z - previous.z) * invDt;

    const float strength = m_template->getStrength();                                // template +0x198
    const float k = static_cast<IPhysicsSystem*>(basicPhysicsSystem)->getGravity()    // +0x20
                  / -9.8199997f * strength;
    const float damping = m_template->getDamping();                                  // template +0x194

    Vec3 force;
    force.x = k * m_deflection.x + damping * velocity.x;
    force.y = k * m_deflection.y + damping * velocity.y;
    force.z = k * m_deflection.z + damping * velocity.z;

    Vec3 reaction;
    reaction.x = -force.x;
    reaction.y = -force.y;
    reaction.z = -force.z;
    root->addAccelerationAtRelativePosition(arm, reaction);                          // +0x94

    EngineBase* engine = static_cast<EngineBase*>(spring->getEngine());              // Spring's own slot +0x180
    IResponsePhysics* grip =
        static_cast<IResponsePhysics*>(object_detail::LayoutAccess::responsePhysics(m_object));

    // Retail tests AL after getEngineType(), not the full int. c_ETNewCar2 is stored as 0xC2,
    // whose low byte is signed-negative; it must leave before the old geared-wheel coupling.
    if (engine && (static_cast<unsigned int>(engine->getEngineType()) & 0x80u) != 0) // engine +0x1AC
    {
        m_wheelSpin = *g_phyFriWheel2 * m_wheelSpin;
        rotateWheel(m_wheelSpin, dt);
        return;
    }

    m_wheelSpin = *g_phyFriWheel * m_wheelSpin;
    if (grip && (grip->getPermanentGrip() & 0x88) != 0 && engine && (engine->getEngineType() & 0x40) != 0)
    {
        float spin = m_wheelSpin;
        if (engine->m_byte1DC)                                                       // engine +0x1DC
        {
            const float scaled = m_wheelSpin * (dt * 30.0f * 0.89999998f);
            spin = fabsf(m_wheelSpin) >= 0.1f ? scaled : 0.0f;
        }

        const float ratio = 3.5f * engine->m_template->m_differential                // engine template +0x3CC
                          / engine->getGearRatio(engine->m_gear);                    // +0x1A4, engine +0x1E8
        float driven = engine->getCurrentDifferentialRPM(getRelativePosition().x)     // this +0x5C
                     * ratio * engine->m_soundInput                                  // engine +0x1E4
                     + (1.0f - engine->m_soundInput) * spin;
        if (fabsf(driven) < 0.001f)
            driven = 0.0f;

        m_wheelSpin = (driven - spin) * (dt * 30.0f * *g_wheelInertia) + spin;
    }
    rotateWheel(m_wheelSpin / getWheelRadius(), dt);
}

// Advances and wraps wheel angle, then rotates its mesh about local X. rotateUV wheels
// retain the angle without changing the mesh. Uses the relative transform when parented
// and the absolute transform otherwise.
void PhysicsSpring::rotateWheel(float angularStep, float dt)
{
    const float advanced = dt * angularStep + m_wheelAngle;
    m_wheelAngle = advanced;
    if (advanced <= -3.1415927f)
        m_wheelAngle = advanced + 6.2831855f;
    else if (advanced >= 3.1415927f)
        m_wheelAngle = advanced - 6.2831855f;

    // The ELF reads the template off the object (+0x14); this class's own +0x230 copy is the same
    // pointer - SpringTemplate::setPhysicsNodeComponent is the only builder and passes itself.
    if (m_template->m_rotateUV)                                                      // template +0x19C
        return;

    const float degrees = 57.295776f * m_wheelAngle;
    const bool parented = m_object->getParent() != 0;                                // object +0x28

    Mat4 transform;
    for (int i = 0; i < 16; ++i) transform.m[i] = 0.0f;
    transform = parented ? m_object->getRelativeTransformation()                     // slot 58
                         : m_object->getAbsoluteTransformation();                    // slot 33

    if (fabsf(degrees) > 1.19209290e-7f)
    {
        Mat4 rotation = Mat4::getIdentity();
        Vec3 axis;                                                                   // the transform's first row
        axis.x = transform.m[0];
        axis.y = transform.m[1];
        axis.z = transform.m[2];
        setRotateAboutLine(rotation, degrees, axis);

        const Mat4 before = transform;
        transform.rotMult(before, rotation);
    }

    if (parented)
        m_object->setRelativeTransformation(transform);                              // slot 59
    else
        m_object->setAbsoluteTransformation(transform);                              // slot 35
}

} // namespace world
} // namespace hfe
} // namespace dice
