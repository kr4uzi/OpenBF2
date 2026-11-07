// Circular debug line strips. Header placement [INFERRED].
#ifndef BF2_GEOMMISC_DEBUGCIRCLEDRAWER_H
#define BF2_GEOMMISC_DEBUGCIRCLEDRAWER_H

#include "../Core/IBase.h"
#include "../Math/Vec3.h"

namespace dice { namespace hfe { namespace geom {

// PE table 0x1020CB98. The Boolean replaces vertex heights with terrain heights;
// the count includes both endpoints. Names of the two drawing overloads are [INFERRED].
class IDebugCircleDrawer : public IBase
{
public:
    virtual void drawCircle(const Vec3& normal, const Vec3& centre, float radius,
                            unsigned int colour, bool terrain,
                            unsigned int vertexCount) = 0; // +0x10
    virtual void drawArc(const Vec3& normal, const Vec3& reference, const Vec3& centre,
                         float radius, unsigned int colour, bool terrain,
                         unsigned int vertexCount, float startDegrees,
                         float sweepDegrees) = 0; // +0x14
    // [CHOSEN, not recovered] Name for the PE deleting-destructor entry; keeping
    // it explicit avoids the additional Itanium destructor slot in the native ABI.
    virtual void* destroy(unsigned int flags) = 0; // +0x18
};

extern const unsigned int CID_DebugCircleDrawer;
IBase* __fastcall createDebugCircleDrawer(unsigned int iid, IBase* outer);

} } }
#endif
