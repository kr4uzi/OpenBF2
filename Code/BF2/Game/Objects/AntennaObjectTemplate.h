// Configures antenna sway for flexible vehicle props. Member names follow console properties.
// [INFERRED] Source placement; total original object size remains unconfirmed.

#ifndef GAME_OBJECTS_ANTENNAOBJECTTEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_ANTENNAOBJECTTEMPLATE_H

#include "Base/SimpleObjectTemplate.h"

namespace dice {
namespace anim { class Skeleton; }
namespace hfe {
namespace world {

class AntennaObjectTemplate : public SimpleObjectTemplate
{
    friend class AntennaObject;             // the ctor copies the template skeleton
    friend struct GameTemplateConsoleAccess;  // recovered ObjectTemplate.antenna.* executors
public:
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;
    AntennaObjectTemplate();                          // read in full
    virtual unsigned int getClassID() const;
    virtual ~AntennaObjectTemplate();
    virtual void* queryImplementation(unsigned int);
    virtual IObject* createObject() const;    // new(0x174) AntennaObject(this) -
    virtual bool  makeScript(io::IStream*);

    void setSkeleton(const std::string& name);
    const std::string& getSkeleton() const;

protected:
    // from the ctor (117 B), which writes both before any named field:
    /* 0x164 */ int   m_refCount;           // ctor 1 - the quartet's count, at sizeof(base)
    /* 0x168 */ anim::Skeleton* m_skeleton;  // ctor 0 - AntennaObject's ctor copies it (`antenna.skeleton` -> setSkeleton stores here)

    // Symmetric sway limits for the X and Z axes, serialized as two slash-separated components.
    /* 0x16C */ float maxSwayAngle[2];      // .con: antenna.maxSwayAngle  (+0x16C, +0x170)

    // Sway coefficients. speedFactor defaults to 1 so unauthored antennas still respond to velocity.
    /* 0x174 */ float stiffness;            // .con: antenna.stiffness            - scales the angle
    /* 0x178 */ float damping;              // .con: antenna.damping              - scales the angular velocity
    /* 0x17C */ float accelerationFactor;   // .con: antenna.accelerationFactor   - scales the local dv
    /* 0x180 */ float speedFactor;          // .con: antenna.speedFactor          - scales the local velocity

    // The skeleton property resolves through setSkeleton; makeScript emits antenna settings only when it is present.

    // makeScript preserves two original quirks: it omits speedFactor and writes an empty skeleton name.
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_ANTENNAOBJECTTEMPLATE_H
