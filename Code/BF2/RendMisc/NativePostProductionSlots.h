// [CHOSEN, not recovered] Caller-shaped metadata for the explicit native
// post-production table. This is not a C++ virtual interface or an object layout.
#ifndef BF2_NATIVEPOSTPRODUCTIONSLOTS_H
#define BF2_NATIVEPOSTPRODUCTIONSLOTS_H
#include "../Math/Vec3.h"
namespace dice { namespace hfe { namespace dx9rend { namespace native_post {
constexpr unsigned slot(unsigned peOrdinal) { return peOrdinal; }
struct Calls
{
    void setFrequency(float);
    void setRoll(float);
    void setNoise(float);
    void setFilter(float);
    void setFog(const Vec4*);
    void setBlend(float);
    void setGlow(float);
    void setBlur(float);
    void setEnabled(bool);
    void setCapture(bool);
};
} } } }
#endif
