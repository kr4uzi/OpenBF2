// Path: [INFERRED] companion header for the owner in PlayerControlObject.cpp.
// The ELF emits updateState (81A2430) with the PCO TU and its pco-min-playing-ticks
// global. Dedicated PE 592D30/592C80 agrees. Client PE ctor 5F4CB0 and caller
// 5FA8DE (allocation 0x34) identify every native pointer-bearing field below.
#ifndef GAME_OBJECTS_GAME_GROUNDCONTACTSOUND_H
#define GAME_OBJECTS_GAME_GROUNDCONTACTSOUND_H

#include "../../../Math/Math.h"
#include "../../../Sound/ObjectSoundNode.h"

namespace dice { namespace hfe {
class ISound;
namespace world {
class PlayerControlObject;

// Field names are [INFERRED] from the ctor/tick/state operations. This carrier has
// no virtual table; its embedded node borrows the owning object.
class GroundContactSound
{
public:
#if !DEDICATED
    GroundContactSound(IObject* owner, float volume);
#endif
    void updateState();

private:
    friend class PlayerControlObject;
    ISound* m_sound;                  // retail +00, owns one sound reference
    bool m_hasContact;                // +04
    bool m_materialChanged;           // +05
    int m_material;                   // +08
    int m_otherMaterial;              // +0C
    float m_input;                    // +10, controller target
    float m_volume;                   // +14, ctor's template groundContactVolume
    ObjectSoundNodeFixed m_node;      // +18: node vptr and borrowed IObject*
    int m_state;                      // +20, 0 waits; 1 updates
    int m_ticks;                      // +24
    PController m_controller;         // +28, includes a native borrowed float pointer
};

} } }

static_assert(sizeof(void*) != 4 || sizeof(dice::hfe::world::GroundContactSound) == 0x34,
              "GroundContactSound retail layout");
// [CHOSEN, not recovered] Native pointers widen the sound, node and controller.
static_assert(sizeof(void*) != 8 || sizeof(dice::hfe::world::GroundContactSound) == 0x48,
              "GroundContactSound native layout");
#endif
