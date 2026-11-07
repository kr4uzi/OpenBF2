// Path: [CHOSEN, not recovered] shared narrow seams for the still-opaque geometry interface.
// Soldier ELF 0x81B5D60/client PE 0x61F630 and Armor ELF 0x8218F30/client PE 0x685770
// independently prove these slots. RendDX9 BundledMesh table 0x1021B350 supplies the callees.
// No positional C++ interface, concrete renderer dependency or geometry layout is invented.
#ifndef BF2_GAME_OBJECTS_GEOMETRYPARTACCESS_H
#define BF2_GAME_OBJECTS_GEOMETRYPARTACCESS_H

#include "../../Math/Mat4.h"
#include <cstring>
#include <string>
#if defined(BF2_STATIC_PLUGINS) && !defined(_MSC_VER)
// [CHOSEN, not recovered] Static native geometry providers retain explicit PE tables.
#include "../../Geom/NativeGeometrySlots.h"
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
#define BF2_GEOMETRY_PART_CALL __thiscall
#else
#define BF2_GEOMETRY_PART_CALL
#endif

namespace dice { namespace hfe { namespace world { namespace geometry_part {

template<class Function> inline Function slot(void* geometry, unsigned int pe, unsigned int elf)
{
#if defined(_MSC_VER)
    const unsigned int offset = pe;
#elif defined(BF2_STATIC_PLUGINS)
    // [CHOSEN, not recovered] This is a function table, not an ELF class vtable.
    const unsigned int offset = 4 * geom::native_geometry::slot(pe, elf);
#else
    const unsigned int offset = elf;
#endif
    // The evidence is a 32-bit byte offset. Convert it to a logical ordinal before
    // indexing a native pointer table; sizeof(void*) would halve every x64 ordinal.
    return reinterpret_cast<Function>((*static_cast<void***>(geometry))[offset / 4]);
}

inline int count(void* geometry)
{
    typedef int (BF2_GEOMETRY_PART_CALL* Function)(void*);
    return slot<Function>(geometry, 0x5C, 0x60)(geometry); // Bundled 0x10103DA0
}

inline void setTransform(void* geometry, const Mat4* transform, int part)
{
    typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, const Mat4*, int);
    // Unlike the adjacent palette slot, the individual-part slot shifts by eight bytes.
    // keeps the pointer, not a copy; callers supply stable matrix storage.
    slot<Function>(geometry, 0x4C, 0x54)(geometry, transform, part);
}

inline int getLod(void* geometry)
{
    typedef int (BF2_GEOMETRY_PART_CALL* Function)(void*);
    return slot<Function>(geometry, 0x7C, 0x80)(geometry);
}

inline int getLodCount(void* geometry)
{
    typedef int (BF2_GEOMETRY_PART_CALL* Function)(void*);
    return slot<Function>(geometry, 0x80, 0x84)(geometry);
}

inline void setLod(void* geometry, int lod)
{
    typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, int);
    slot<Function>(geometry, 0x78, 0x7C)(geometry, lod);
}

inline void* getTemplate(void* geometry)
{
    typedef void* (BF2_GEOMETRY_PART_CALL* Function)(void*);
    return slot<Function>(geometry, 0x38, 0x3C)(geometry);
}

// Named geometry-instance operations used by game code. Native static builds select the named
// ordinal of the renderer's explicit PE-order tables (Geom/NativeGeometrySlots.h, checked by
// tools/macos/vtable_audit.py against BundledMesh/StaticMesh/SkinnedMesh/MeshParticleMesh);
// MSVC keeps the PE byte offset and other non-MSVC builds the ELF one. Callers never index a
// geometry table themselves (the gate's raw-slot scan rejects new sites).
#if defined(BF2_STATIC_PLUGINS) && !defined(_MSC_VER)
#define BF2_GEOMETRY_OP(pe, elf, name) slot<Function>(geometry, 4 * geom::native_geometry::name, 4 * geom::native_geometry::name)
#else
#define BF2_GEOMETRY_OP(pe, elf, name) slot<Function>(geometry, pe, elf)
#endif
inline float getBoundingRadius(void* geometry)
{ typedef float (BF2_GEOMETRY_PART_CALL* Function)(void*); return BF2_GEOMETRY_OP(0x44, 0x48, GetBoundingRadius)(geometry); }
inline void setFov(void* geometry, float fov)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, float); BF2_GEOMETRY_OP(0x10, 0x10, SetFov)(geometry, fov); }
inline float getFov(void* geometry)
{ typedef float (BF2_GEOMETRY_PART_CALL* Function)(void*); return BF2_GEOMETRY_OP(0x14, 0x14, GetFov)(geometry); }
inline void setGeometryTransform(void* geometry, const Mat4* transform)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, const Mat4*); BF2_GEOMETRY_OP(0x50, 0x50, SetGeometryTransform)(geometry, transform); }
inline void setBoneTransforms(void* geometry, const Mat4* transforms, int count)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, const Mat4*, int); BF2_GEOMETRY_OP(0x54, 0x58, SetBoneTransforms)(geometry, transforms, count); }
// SkinnedMesh +0x58: appends the used bone indices to the engine's int vector (opaque here).
inline void gatherUsedBones(void* geometry, void* intVector)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, void*); BF2_GEOMETRY_OP(0x58, 0x5C, GatherUsedBones)(geometry, intVector); }
inline void setExcludedPart(void* geometry, int part, bool enable)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, int, bool); BF2_GEOMETRY_OP(0x60, 0x64, SetExcludedPart)(geometry, part, enable); }
inline void setSubgeom(void* geometry, int index)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, int); BF2_GEOMETRY_OP(0x6C, 0x70, SetSubgeom)(geometry, index); }
inline int getSubgeom(void* geometry)
{ typedef int (BF2_GEOMETRY_PART_CALL* Function)(void*); return BF2_GEOMETRY_OP(0x70, 0x74, GetSubgeom)(geometry); }
inline int getSubgeomCount(void* geometry)
{ typedef int (BF2_GEOMETRY_PART_CALL* Function)(void*); return BF2_GEOMETRY_OP(0x74, 0x78, GetSubgeomCount)(geometry); }
inline void setUvTranslation(void* geometry, int index, float x, float y)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, int, float, float); BF2_GEOMETRY_OP(0x90, 0x94, SetUvTranslation)(geometry, index, x, y); }
inline void setUvTransformation(void* geometry, int index, const Mat4* transform)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, int, const Mat4*); BF2_GEOMETRY_OP(0x98, 0x9C, SetUvTransformation)(geometry, index, transform); }
inline void* setOwnerObject(void* geometry, void* object)
{ typedef void* (BF2_GEOMETRY_PART_CALL* Function)(void*, void*); return BF2_GEOMETRY_OP(0x9C, 0x9C, SetOwnerObject)(geometry, object); }
inline void updateFlags(void* geometry, unsigned int set, unsigned int clear)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, unsigned int, unsigned int); BF2_GEOMETRY_OP(0xC0, 0xC4, UpdateFlags)(geometry, set, clear); }
inline void* setNextGeometry(void* geometry, void* next)
{ typedef void* (BF2_GEOMETRY_PART_CALL* Function)(void*, void*); return BF2_GEOMETRY_OP(0xC8, 0xCC, SetNextGeometry)(geometry, next); }
inline void* getNextGeometry(void* geometry)
{ typedef void* (BF2_GEOMETRY_PART_CALL* Function)(void*); return BF2_GEOMETRY_OP(0xCC, 0xD0, GetNextGeometry)(geometry); }
inline void setFirstPerson(void* geometry, unsigned int value)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, unsigned int); BF2_GEOMETRY_OP(0xD4, 0xD8, SetFirstPerson)(geometry, value); }
inline void setLightSourceMask(void* geometry, int mask)
{ typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, int); BF2_GEOMETRY_OP(0xE0, 0xE4, SetLightSourceMask)(geometry, mask); }
inline int getLightSourceMask(void* geometry)
{ typedef int (BF2_GEOMETRY_PART_CALL* Function)(void*); return BF2_GEOMETRY_OP(0xE4, 0xE8, GetLightSourceMask)(geometry); }
#undef BF2_GEOMETRY_OP

#if defined(_MSC_VER) || defined(BF2_STATIC_PLUGINS)
// Renderer-internal reads (DecalShadowManager::addShadow, PE 0x10127260). No ELF image calls these
// slots, so no ELF offset is attested and they exist only where the PE table order applies.
#if defined(BF2_STATIC_PLUGINS) && !defined(_MSC_VER)
#define BF2_RENDERER_GEOMETRY_OP(pe, name) slot<Function>(geometry, 4 * geom::native_geometry::name, 4 * geom::native_geometry::name)
#else
#define BF2_RENDERER_GEOMETRY_OP(pe, name) slot<Function>(geometry, pe, pe)
#endif
// +0x28: the shadow record the geometry draws into (StaticMesh +0x34C, SkinnedMesh +0x33C,
// BundledMesh +0x2B4).
inline void* getShadowRecord(void* geometry)
{ typedef void* (BF2_GEOMETRY_PART_CALL* Function)(void*); return BF2_RENDERER_GEOMETRY_OP(0x28, GetShadowRecord)(geometry); }
// +0x48: the stored world transform, or the shared identity when none was set.
inline const Mat4* getGeometryTransform(void* geometry)
{ typedef const Mat4* (BF2_GEOMETRY_PART_CALL* Function)(void*); return BF2_RENDERER_GEOMETRY_OP(0x48, GetGeometryTransform)(geometry); }
#undef BF2_RENDERER_GEOMETRY_OP
#endif

inline bool templateNameEquals(void* objectTemplate, const char* name)
{
    // MeshTemplate::getName (+0x18) returns the renderer's std::string: the client and its
    // plugins share one STL (issue #7).
    typedef const std::string& (BF2_GEOMETRY_PART_CALL* Function)(void*);
    return slot<Function>(objectTemplate, 0x18, 0x18)(objectTemplate) == name;
}

} } } }
#undef BF2_GEOMETRY_PART_CALL
#endif
