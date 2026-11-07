// [CHOSEN, not recovered] Shared native declaration of the existing six-slot
// scale interface used by world objects and MeshParticleMesh's secondary table.
#ifndef BF2_GEOM_ISCALEABLE_H
#define BF2_GEOM_ISCALEABLE_H
#include "../Core/IBase.h"
#include "../Math/Vec3.h"
namespace dice { namespace hfe { namespace geom {
class IScaleable : public IBase
{
public:
    virtual void setScale(const Vec3&) = 0;
    virtual const Vec3& getScale() const = 0;
};
} } }
#endif
