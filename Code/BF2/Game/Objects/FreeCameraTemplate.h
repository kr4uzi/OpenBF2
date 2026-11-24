// Detached spectator/debug camera template.
// [INFERRED] source placement.

#ifndef GAME_OBJECTS_FREECAMERATEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_FREECAMERATEMPLATE_H

#include "Base/BundleTemplate.h"

namespace dice {
namespace hfe {
namespace world {

class FreeCameraTemplate : public BundleTemplate
{
public:
    // the header had a layout and no methods at all.
    FreeCameraTemplate();                            // refcount 1, two 1.2f, flags 0x42
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;
    virtual unsigned int getClassID() const;
    virtual ~FreeCameraTemplate();
    virtual void* queryImplementation(unsigned int);
    virtual IObject* createObject() const;           // new(0x294) FreeCamera [FRONTIER]
    virtual bool makeScript(io::IStream* stream) override;

protected:
    // The object reads its two FOVs directly, the same arrangement OverheadCameraTemplate has
    // with OverheadCamera.
    friend class FreeCamera;

    /* 380 */ int   m_refCount;
    /* 384 */ float m_defaultFoV;
    /* 388 */ float m_activeFoV;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_FREECAMERATEMPLATE_H
