// Camera interface used by vehicle and overhead cameras. Preserve virtual declaration order.
// [INFERRED] Source placement. Return types retain the qualifications on their declarations.

#ifndef GAME_OBJECTS_ICAMERAOBJECT_H     // [INFERRED] guard style
#define GAME_OBJECTS_ICAMERAOBJECT_H

#include "../../Core/IBase.h"
#include "../../Math/Vec3.h"
#include "../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace world {

class IObject;

// BF2.exe reader 0x509B12 and writer 0x506AD7 establish the shared camera vocabulary.
enum CameraViewMode : int
{
    CVMFirstPerson = 0, CVMInside = 1, CVMNose = 2, CVMChase = 3,
    CVMFrontChase = 4, CVMFlyBy = 5, CVMTrace = 6, CVMExternTrace = 7
};

class ICameraObject : public IBase
{
public:
    virtual const Mat4& getTransformation(float) = 0;                            // +0x10
    virtual void setInterpolatedMode(bool) = 0;                                  // +0x14
    virtual bool setViewMode(CameraViewMode mode, IObject* object, bool) = 0;    // +0x18
    virtual CameraViewMode getViewMode() const = 0;                              // +0x1C
    virtual bool isActive() const = 0;                                           // +0x20
    virtual void setIsActive(bool) = 0;                                          // +0x24
    virtual void setCameraId(int id) = 0;                                        // +0x28
    virtual int  getCameraId() = 0;                                              // +0x2C
    virtual void prevViewMode() = 0;                                             // +0x30
    virtual void setTraceObjectId(unsigned int id) = 0;                          // +0x34
    virtual unsigned int getTraceObjectId() = 0;                                 // +0x38
    virtual void setExternCameraTrans(Mat4 transform) = 0;                       // +0x3C BY value (mangling)
    virtual Mat4* getExternCameraTrans() = 0;                                    // +0x40 [CHOSEN ret]
    virtual bool getToggleMouseLook() = 0;                                       // +0x44
    virtual int  getPcoId() = 0;                                                 // +0x48
    virtual void handleExplosion(float) = 0;                                     // +0x4C
    virtual void resetCameraView(bool) = 0;                                      // +0x50
    virtual void updateCameraView(bool) = 0;                                     // +0x54
    virtual float getActiveFOV() const = 0;                                      // +0x58
    virtual bool getAllowDynamicLights() const;                                  // +0x5C (default) 0x81E5670: true
    virtual bool getAllowDynamicSun() const;                                     // +0x60 (default) 0x81E5680: true
    virtual const Vec3& getStaticSunDirection() const = 0;                       // +0x64
    virtual const Vec3& getStaticSunColor() const = 0;                           // +0x68
    virtual const Vec3& getStaticSunDamageColor() const = 0;                     // +0x6C
    virtual float getInsideDamageFrequency() const;                              // +0x70 (default) 0x81E5690: 0.0f
    virtual float getInsideDamageThreshold() const;                              // +0x74 (default) 0x81E56A0: 0.0f
    virtual const Vec3& getStaticSunAmbientColor() const = 0;                    // +0x78
    virtual bool allowCameraModeCycle();                                         // +0x7C (default) 0x81DFB20: true
    virtual bool isOverheadCamera() const;                                       // +0x80 (default) 0x81DFB30: false

protected:
    ~ICameraObject() {}
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_ICAMERAOBJECT_H
