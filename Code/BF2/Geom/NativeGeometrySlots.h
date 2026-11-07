// [CHOSEN, not recovered] Logical slots in the native static renderer's explicit
// geometry tables. These tables retain PE order; Clang does not lay them out.
// tools/macos/vtable_audit.py checks every named entry against its providers.
#ifndef BF2_GEOM_NATIVEGEOMETRYSLOTS_H
#define BF2_GEOM_NATIVEGEOMETRYSLOTS_H
namespace dice { namespace hfe { namespace geom { namespace native_geometry {
constexpr unsigned SetFov = 4;                 // PE +0x10 / ELF +0x10
constexpr unsigned GetFov = 5;                 // PE +0x14 (no ELF caller read)
constexpr unsigned GetShadowRecord = 10;        // PE +0x28 (renderer-internal; no ELF caller read)
constexpr unsigned GetTemplate = 14;
constexpr unsigned GetBoundingRadius = 17;     // PE +0x44 / ELF +0x48
constexpr unsigned GetGeometryTransform = 18;  // PE +0x48 (renderer-internal; no ELF caller read)
constexpr unsigned SetPartTransform = 19;
constexpr unsigned SetGeometryTransform = 20;  // PE +0x50 / ELF +0x50 (both ports, GenericFireArm/Object)
constexpr unsigned SetBoneTransforms = 21;
constexpr unsigned GatherUsedBones = 22;       // PE +0x58 / ELF +0x5C (SkinnedMesh)
constexpr unsigned GetPartCount = 23;
constexpr unsigned SetExcludedPart = 24;       // PE +0x60 / ELF +0x64 (SkinnedMesh)
constexpr unsigned SetSubgeom = 27;            // PE +0x6C / ELF +0x70
constexpr unsigned GetSubgeom = 28;
constexpr unsigned GetSubgeomCount = 29;       // PE +0x74 / ELF +0x78
constexpr unsigned SetLod = 30;
constexpr unsigned GetLod = 31;
constexpr unsigned GetLodCount = 32;
constexpr unsigned SetUvTranslation = 36;      // PE +0x90 / ELF +0x94
constexpr unsigned SetUvTransformation = 38;   // PE +0x98 / ELF +0x9C
constexpr unsigned SetOwnerObject = 39;        // PE +0x9C (the ELF arm kept +0x9C)
constexpr unsigned UpdateFlags = 48;           // PE +0xC0 / ELF +0xC4 (StaticMesh)
constexpr unsigned SetNextGeometry = 50;       // PE +0xC8 / ELF +0xCC
constexpr unsigned GetNextGeometry = 51;       // PE +0xCC / ELF +0xD0
constexpr unsigned SetFirstPerson = 53;        // PE +0xD4 / ELF +0xD8 (MeshParticleMesh)
constexpr unsigned SetLightSourceMask = 56;    // PE +0xE0 / ELF +0xE4 (StaticMesh +0x28C)
constexpr unsigned GetLightSourceMask = 57;    // PE +0xE4 / ELF +0xE8 (7 when unsupported)
// The selector's input remains the original four-byte PE/ELF offset pair.
constexpr unsigned slot(unsigned pe, unsigned) { return pe / 4; }
} } } }
#endif
