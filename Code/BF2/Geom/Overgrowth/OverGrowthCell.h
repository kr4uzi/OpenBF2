// Internal overgrowth grid-cell, patch and batched-stream declarations.
#ifndef BF2_GEOM_OVERGROWTHCELL_H
#define BF2_GEOM_OVERGROWTHCELL_H

#ifdef BF2_IMAGE_RENDDX9
#include "OverGrowth.h"
#include "../BoxCorners.h"                    // OverGrowthBoxCorners
#include "../MeshTemplate.h"                   // MeshBounds
#include "../../Math/Mat4.h"
#include "../../RendDX9/Ra/RaShader.h"         // RaObjectParameters
#include "../../RendDX9/Ra/RaStreamDataEditable.h"

namespace dice { namespace hfe { namespace geom {


// One patch of the countX * countZ grid at OverGrowth +0xE0 - 0x98 bytes in the PE, allocated with
// operator new[] (the element count ahead of the first element). Construction sub_100CAFF0: position
// and the vector zeroed, the bounds zeroed with their dirty byte set; the coverage is NOT constructed.
// The destructor is its vector's. Field names are [INFERRED].
struct OverGrowthPatch
{
    OverGrowthPatch();
    ~OverGrowthPatch();

    Vec3                     position;       // +0x00  the patch corner, y the terrain height
    OverGrowthInstanceVector instances;      // +0x0C
    float                    coverage[16];   // +0x1C  share of the patch's texels per material id
    MeshBounds               bounds;         // +0x5C  60 bytes, ends at +0x98
};


// Cell vertex stream plus object parameters. Extra buffer-management slots extend the
// current base declaration while retaining compatibility positions.

class OverGrowthCellMesh : public dx9rend::Ra::RaStreamDataEditable
{
public:
    OverGrowthCellMesh(unsigned int vertexCapacity, unsigned int indexCapacity,
                       const std::vector<const dx9rend::Ra::RaVertexElementDesc*>& elements);

    void* destroy(unsigned char flags) override;                     // +0x00
    virtual void releaseBuffers();                                   // +0x30  10048D60
    virtual unsigned int getFreeVertexCount() const;                  // +0x34
    virtual unsigned int getVertexCapacity() const;                   // +0x38
    virtual unsigned int getIndexCapacity() const;                    // +0x3C
    virtual void createBuffers();                                    // +0x40  10048C70

    // +0x1A4 in the PE: the Ra object block, constructed in place by sub_100458E0.
    alignas(dx9rend::Ra::RaObjectParameters)
    unsigned char m_objectParameters[sizeof(dx9rend::Ra::RaObjectParameters)];

    dx9rend::Ra::RaObjectParameters& objectParameters()
    { return *reinterpret_cast<dx9rend::Ra::RaObjectParameters*>(m_objectParameters); }
};


// Visible grid cell with bounds, camera fade, instance list, and leaf/trunk streams.
// The eight-slot interface overlays its abstract base (0x2AC bytes in the PE). Field names are
// [INFERRED].

struct OverGrowthCell
{
    OverGrowthCell();                                                 // 100D0C90 (+ base 1015E6D0)
    ~OverGrowthCell();                                                // 100D18B0, reached through destroy

    virtual void* destroy(unsigned char flags);                       // +0x00
    virtual bool  appendGeometry(void* geometryTemplate, const Mat4* transform, float scale,
                                 int unused, float shadow);           // +0x04
    virtual void  lock();                                             // +0x08
    virtual void  unlock();                                           // +0x0C
    virtual void  draw(IRenderView* view, const Vec3* position, float fade); // +0x10
    virtual unsigned int getVertexCapacity();                         // +0x14
    virtual unsigned int getIndexCapacity();                          // +0x18
    virtual void  clearFarList();                                     // +0x1C

    unsigned char      visible;           // +0x04  in range and built this frame
    unsigned char      built;             // +0x05  its streams hold the current cell
    unsigned char      empty;             // +0x06  1 - no far instance to draw
    unsigned char      pad07;
    Vec3               corner;            // +0x08  the cell's corner, y the terrain height
    float              size;              // +0x14  the cell edge
    float              fadeFactor;        // +0x18  the distance / height fade
    float              cameraDistance;    // +0x1C  the camera's distance to the centre
    MeshBounds         bounds;            // +0x20
    int                cellX;             // +0x5C  world cell, 0x2CEE9D7 when unplaced
    int                cellZ;             // +0x60
    unsigned char      hasNear;           // +0x64  some instances are drawn as full meshes
    unsigned char      pad65[3];
    // +0x68 - the base's Ra object block (sub_100458E0 / sub_1004D900).
    alignas(dx9rend::Ra::RaObjectParameters)
    unsigned char      objectParameters[sizeof(dx9rend::Ra::RaObjectParameters)];
    std::vector<OverGrowthInstance*> farList;   // +0x28C  the instances batched into the streams
    unsigned int       unresolved29C;     // +0x29C  zeroed by the constructor and rebuildCells [UNRESOLVED]
    OverGrowthPatch*   lastPatch;         // +0x2A0  the patch updateCell last took
    OverGrowthCellMesh* leaf;             // +0x2A4  the non-"tile_" materials
    OverGrowthCellMesh* trunk;            // +0x2A8  the "tile_" materials
};

// The allocation postLoadResources makes per grid slot (0x100CD6DE..0x100CD6FB): operator new(0x2AC),
// then the constructor when the heap answered (OverGrowthCells.cpp).
OverGrowthCell* createOverGrowthCell();

// sub_1015EEB0 - the abstract base's updateCells: the camera ring of N x N cells and each cell's build.
// OverGrowth::updateCells calls it first; render calls it directly when +0xD5 is clear
// (OverGrowthCells.cpp).
void overGrowthBaseUpdateCells(OverGrowth* self, IRenderView* view);


// Placement helpers both halves share, defined once in OverGrowthCells.cpp.

// The 96-byte eight-corner box: the local bounds' corners carried through a world matrix
// (geom::BoxCorners, BoxCorners.h).
typedef BoxCorners OverGrowthBoxCorners;

// Zeroes the box, then fills it with the corners of `bounds` transformed by `world`.
void constructBoxCorners(OverGrowthBoxCorners& box, const MeshBounds& bounds, const Mat4& world);

// Grows `bounds` by the box corners' component-wise minimum, then their maximum.
void boundsExpandByCorners(MeshBounds& bounds, const OverGrowthBoxCorners& box);

// World (x, z) to density/shadow map texel (ix, iz) and the fractions (fx, fz). An index equal to
// the map width or height is not rejected (the retail clamp's off-by-one).
void shadowCoords(OverGrowth* self, float x, float z, int* ix, int* iz, float* fx, float* fz);

// The bilinear shadow-map value at world (x, z); a tap outside the map counts as 0.
float sampleShadow(OverGrowth* self, float x, float z);

// Fills the patch's sixteen per-material coverage shares from the density map.
void computeCoverage(OverGrowth* self, OverGrowthPatch* patch);

// The instance's default constructor: identity transform, unit scale, no geometry. +0x54 (the
// shadow value) is left unwritten.
void constructInstance(OverGrowthInstance& instance);

} } }
#endif
#endif
