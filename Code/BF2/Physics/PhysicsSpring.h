// Wheel-suspension node with spring deflection and angular spin. Header path [INFERRED].
// Uses its own reference counter. Sleep transitions also update the object's tick flag and placement;
// force-sleep/force-awake settings override the countdown.
// The current deflection persists between ticks; wheel angle is a wrapped float, not an integer.
// The constructor clears fix-to-parent so suspension can move. No [FRONTIER] body remains here.

#ifndef PHYSICS_PHYSICSSPRING_H     // [INFERRED] guard style
#define PHYSICS_PHYSICSSPRING_H

#include "PhysicsNode.h"

namespace dice {
namespace hfe {
class SpringNetworkable;
namespace world {

class SpringTemplate;

class PhysicsSpring : public PhysicsNode
{
public:
    PhysicsSpring(const SpringTemplate* springTemplate, const IObject* object);   // 0x82AA630 (C2) / 0x82AA6B0 (C1), 119 B - the object is not read
    virtual ~PhysicsSpring();

    // The quartet on the +0x22C count.
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;                        // IBase / IPhysicsNode

    virtual void  updatePhysics(float dt);
    virtual void  setIsSleeping();
    virtual void  setSleepiness(int sleepiness);
    virtual void  setIsSleepier();
    virtual void  setIsAwake();

    // Generic node resets preserve suspension deflection and wheel spin.
    using PhysicsNode::reset;
    virtual void  reset();                                                       // ELF +0x1C0 / PE +0x1BC

    void  setRelationToParentPosition(const Vec3& position);
    void  rotateWheel(float angularStep, float dt);                              // written
    float getWheelRadius() const;
    const Vec3& calcDefaultPosition(const SpringTemplate* springTemplate, const IObject* object);   // (the object is not read)

    // ELF 824C2A0/824C660 snapshots/applies the same deflection and spin fields directly.
    // [CHOSEN, not recovered] Access spelling; native field access replaces retail byte offsets.
    friend class ::dice::hfe::SpringNetworkable;
    friend class ResponsePhysics;               // addFriction / addFrictionNew read and write m_wheelSpin,
                                                // read m_wheelAngle

protected:
    /* 0x22C */ int   m_refCount;
    /* 0x230 */ const SpringTemplate* m_template;
    /* 0x234 */ Vec3  m_relationToParentPosition;
    /* 0x240 */ Vec3  m_deflection;
    /* 0x24C */ float m_float24C;
    /* 0x250 */ float m_wheelSpin;
    /* 0x254 */ float m_wheelAngle;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // PHYSICS_PHYSICSSPRING_H
