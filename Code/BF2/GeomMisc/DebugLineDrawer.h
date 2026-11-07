// Debug line and triangle drawing. Header placement and drawing names [INFERRED].
#ifndef BF2_GEOMMISC_DEBUGLINEDRAWER_H
#define BF2_GEOMMISC_DEBUGLINEDRAWER_H

#include "../Core/IBase.h"
#include "../Math/Vec3.h"
#include <vector>

struct D3DXVECTOR3;

namespace dice { namespace hfe { namespace geom {

// PE table 0x1020C758. D3DX overloads consume contiguous three-float positions;
// the Vec3 overload converts to that vector before dispatching to drawRibbon.
class IDebugLineDrawer : public IBase
{
public:
    virtual void drawD3dxLine(const std::vector<D3DXVECTOR3>& points,
                             unsigned int colour, float width) = 0;             // +0x10
    virtual void drawD3dxLine(unsigned int count, const D3DXVECTOR3* points,
                             unsigned int colour, float width) = 0;             // +0x14
    virtual void drawD3dxLine(float x0, float y0, float z0, float x1, float y1,
                             float z1, unsigned int colour, float width) = 0;    // +0x18
    // The four-byte ZBUFFER value is forwarded unchanged to the shader parameter.
    virtual void drawThinLine(float x0, float y0, float z0, float x1, float y1,
                              float z1, unsigned int colour,
                              unsigned int depthValue) = 0;                     // +0x1C
    virtual void drawRibbon(const std::vector<D3DXVECTOR3>& points,
                            unsigned int colour, float width, bool depth) = 0;  // +0x20
    virtual void drawLine(const std::vector<Vec3>& points, unsigned int colour,
                          float width, bool depth) = 0;                          // +0x24
    virtual void drawTriangle(const Vec3& a, const Vec3& b, const Vec3& c,
                              unsigned int colour, bool depth) = 0;             // +0x28
    // Colours are per triangle, with the last repeated when the vector is shorter.
    virtual void drawTriangles(const std::vector<Vec3>& points,
                               const std::vector<unsigned int>& colours,
                               bool depth) = 0;                                 // +0x2C
    virtual void drawMesh(Vec3* points, unsigned int* indices, int triangleCount,
                          unsigned int* colours, bool depth) = 0;               // +0x30
    virtual void drawLineList(const std::vector<Vec3>& points,
                              const std::vector<unsigned int>& colours,
                              float width, bool depth) = 0;                      // +0x34
    // [CHOSEN, not recovered] Explicit PE deleting-destructor entry, avoiding
    // the extra Itanium destructor slot in the native interface.
    virtual void* destroy(unsigned int flags) = 0;                                // +0x38
    virtual void drawScreenLine(float x0, float y0, float x1, float y1,
                                unsigned int colour, float width) = 0;          // +0x3C
};

extern const unsigned int CID_DebugLineDrawer;
IBase* __fastcall createDebugLineDrawer(unsigned int iid, IBase* outer);

} } }
#endif
