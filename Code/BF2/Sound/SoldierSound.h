// [INFERRED] Source placement.

// Per-soldier sound playback state.


#ifndef SOUND_SOLDIERSOUND_H     // [INFERRED] guard style
#define SOUND_SOLDIERSOUND_H

#include "ISoundNode.h"
#include "../Math/Vec3.h"

namespace dice {
namespace hfe {

class ISound;

namespace world { class Soldier; class SimpleObjectTemplate; }

class SoldierSound : public ISoundNode
{
public:
    explicit SoldierSound(world::Soldier* soldier);
    virtual ~SoldierSound();

    // ISoundNode, the only two virtual slots
    // getTransform is the soldier's absolute transformation, taken live - the sound node does not
    // cache a position of its own. getVelocity is seven bytes returning null, the same "no velocity"
    // answer RotationalBundle gives.
    virtual const Mat4& getTransform();
    virtual const Math::Vec3* getVelocity();

    // Everything else is NON-virtual: the Soldier calls it directly
    // Creates the five named sound children before soldier tweaks configure them.
    static void initTemplates(world::SimpleObjectTemplate* objectTemplate);
    void update(float dt, int* out);
    void updateContactMaterial(int material);

    void jump();
    void trigger3PLanding();
    void trigger1PLanding();
    void playJumpSample();
    bool allowMovementSounds() const;

    void die(bool firstPersonOnly);
    void revive();
    void dieHit(int material, float unused);
    void bulletFlyBy(int material, const Vec3& at);

    void playDrowningSound();
    void stopDrowningSound();

    // returns by value through an sret, and it is a constant, not state: (+/-0.05, 0, 0.4). The
    // bool picks the sign of x, so the two strokes are 10 cm apart and both 40 cm in front.
    Vec3 getSwimStrokePos(bool leftStroke) const;

private:

    // Nine sound slots: four movement samples and five sounds initialized by the constructor.

    /* +0x04 */ world::Soldier* m_soldier;
    /* +0x08 */ ISound* m_sounds[9];      // [0..3] updateContactMaterial; [4] Death, [5] Revive,
                                          // [6] Swim, [7] Swim_Sprint, [8] Swim_Still
    /* +0x2C */ int   m_lastMaterial;     // ctor -1 [CHOSEN] name - updateContactMaterial's field
    /* +0x30 */ float m_movementSpeedEvaluator; // movement speed; also the Death sound's shared
                                                // volume/pitch evaluator, as shipped
    /* +0x34 */ bool  m_phaseLatch;        // ctor true; half-animation foot/swim-stroke latch
    /* +0x35 */ bool  m_drowningPlaying;  // ctor false - playDrowningSound's latch
};

} // namespace hfe
} // namespace dice

#endif // SOUND_SOLDIERSOUND_H
