// Path: [INFERRED] header beside MeshOptimizer.cpp, whose path is attested at RendDX9 0x10221E70.
// Shared base of the two mesh optimizers (dice.hfe.geom.GeometryOptimizer.*), recovered from
// RendDX9.dll 1.50 (PE only). Retail layout: 0x114 bytes, primary table 0x10221EB8 (22 slots),
// constructor 0x101B6CC0, destructor 0x101B6DA0.
//
// An optimizer rewrites one .staticMesh/.bundledMesh into a `gpu_` copy beside it: it reads the
// source, builds one D3DX mesh per LOD, vertex-cache optimizes it for the current adapter and
// writes the result with the adapter's device id in the header. In 1.50 nothing calls the file
// path: Renderer::initialize creates both optimizers, calls optimize() (which returns false) and
// releases them. The record types below are the in-memory form of the file's tables.

#ifndef BF2_GEOM_MESHOPTIMIZER_H
#define BF2_GEOM_MESHOPTIMIZER_H

#include "GeometryTemplateManager.h"
#include "MeshTemplate.h"

#include "../../RendDX9Abi/HostFileAbi.h"

#include <string>
#include <vector>

struct ID3DXMesh;
struct IDirect3DIndexBuffer9;

namespace dice {
namespace hfe {
namespace geom {

// One material record of a LOD, 0xA0 bytes in retail. The file stores the alpha mode, the two
// names, the texture list and the four range counts; the bounds are filled by
// StaticMeshOptimizer only.
struct MeshOptimizerMaterial
{
    // The prototype every resize copies (0x101B6FB0..0x101B701B): empty names, no textures and
    // zero bounds with the dirty byte set. Retail leaves the alpha-mode and range dwords unset;
    // [CHOSEN, not recovered] they start at zero (readMaterials clears every record anyway).
    MeshOptimizerMaterial();

    // The `rep stosd` over the whole record that readMaterials runs after the resize
    // (0x101B7052). [CHOSEN, not recovered] The names and the texture vector are reset as objects
    // instead of being overwritten with zero bytes; every scalar, including the bounds' dirty
    // byte, ends at zero as in retail.
    void clear();

    std::string              shaderFile;   // +0x00
    std::string              technique;    // +0x1C
    std::vector<std::string> textures;     // +0x38
    unsigned int             alphaMode;    // +0x48
    unsigned int             vertexStart;  // +0x4C
    unsigned int             indexStart;   // +0x50
    unsigned int             indexCount;   // +0x54
    unsigned int             vertexCount;  // +0x58
    // [CHOSEN, not recovered] names: no instruction reads or writes these two dwords; the file's
    // two trailing material dwords are skipped on read and written as zero.
    unsigned int             unused5C;     // +0x5C
    unsigned int             unused60;     // +0x60
    MeshBounds               bounds;       // +0x64
};

// Per-LOD optimizer state, 0x14 bytes in retail. `alphaMode` summarizes the LOD's materials while
// they are read; `materialCount` is set by buildMeshes.
struct MeshOptimizerLod
{
    ID3DXMesh*   mesh;            // +0x00  D3DXCreateMesh output
    ID3DXMesh*   optimizedMesh;   // +0x04  Optimize output
    unsigned int optimizeFlags;   // +0x08
    unsigned int alphaMode;       // +0x0C
    unsigned int materialCount;   // +0x10
};

// One D3DVERTEXELEMENT9 as stored in the file (8 bytes).
struct MeshOptimizerVertexElement
{
    unsigned short stream;
    unsigned short offset;
    unsigned char  type;
    unsigned char  method;
    unsigned char  usage;
    unsigned char  usageIndex;
};

// BundledMeshOptimizer's per-LOD record, 0x50 bytes in retail.
struct BundledMeshOptimizerLod
{
    // The resize prototype (0x10133738..0x10133772): zero bounds with the dirty byte set and no
    // strips. The node count is left unset there; [CHOSEN, not recovered] it starts at zero.
    BundledMeshOptimizerLod();

    unsigned int                        nodeCount;   // +0x00
    MeshBounds                          bounds;      // +0x04
    std::vector<IDirect3DIndexBuffer9*> strips;      // +0x40  one per material after strip conversion
};

// One 4x4 matrix as stored in the file.
struct MeshOptimizerMatrix
{
    float m[16];
};

// StaticMeshOptimizer's per-LOD record, 0x60 bytes in retail.
struct StaticMeshOptimizerLod
{
    // The resize prototype (0x10135E65..0x10135EA8): no matrices, zero bounds with the dirty byte
    // set, no strips. The matrix count is left unset there; [CHOSEN, not recovered] it starts at
    // zero.
    StaticMeshOptimizerLod();

    // The `rep stosd` over the whole record that readGeometry runs after the resize (0x10135ED9).
    // [CHOSEN, not recovered] The two vectors are reset as objects instead of being overwritten
    // with zero bytes; the count and the bounds, including the dirty byte, end at zero.
    void clear();

    unsigned int                        matrixCount; // +0x00
    std::vector<MeshOptimizerMatrix>    matrices;    // +0x04
    MeshBounds                          bounds;      // +0x14
    std::vector<IDirect3DIndexBuffer9*> strips;      // +0x50  one per material after strip conversion
};

typedef std::vector<MeshOptimizerMaterial>  MeshOptimizerMaterialVector;
typedef std::vector<MeshOptimizerLod>       MeshOptimizerLodVector;

class MeshOptimizer : public IGeometryOptimizer
{
public:
    MeshOptimizer();

    void  addRef() override;                                        // +0x00 0x1000CB20
    int   getRef() override;                                        // +0x04 0x101A9A10
    int   release() override;                                       // +0x08 0x101B8E50
    void* queryInterface(unsigned int iid) const override;          // +0x0C 0x10134E70
    unsigned int getClassId() const override;                       // +0x10 0x100ED3B0
    bool  optimize() override;                                      // +0x14 0x101B5D10

    // Processes `path`, then releases the vertex declaration. Returns processFile's result.
    virtual bool optimizeFile(const std::string& path);             // +0x18 0x101B5E30
    // Rewrites `path` into its `gpu_` copy unless that copy is newer and already matches the
    // adapter. Returns false when reading failed.
    virtual bool processFile(const std::string& path);              // +0x1C 0x101B5F90
    virtual ~MeshOptimizer();                                       // +0x20 0x101B8E70

    // Optimizes every non-empty `<stem><extension>` file below `directory` (recursive, `gpu_`
    // stems skipped) through optimizeFile. Only the first file's result decides the answer; an
    // empty set answers false. Non-virtual; unreachable in 1.50. [INFERRED] name.
    bool optimizeDirectory(const std::string& directory, const std::string& extension);
    // Reads the file header. True when the file must be (re)optimized for this adapter; the
    // device id and driver version are then replaced with the adapter's.
    virtual bool readHeader(renddx9abi::HostStream* stream);        // +0x24 0x101B6100
    // The header check applied to an existing `gpu_` copy.
    virtual bool readOutputHeader(renddx9abi::HostStream* stream);  // +0x28 0x101B5D70
    virtual void readVertexData(renddx9abi::HostStream* stream);    // +0x2C 0x101B6520
    virtual void readIndexTail(renddx9abi::HostStream* stream);     // +0x30 0x101B5D80
    virtual void readGeometry(renddx9abi::HostStream* stream);      // +0x34 0x101B5DA0
    virtual void readMaterials(renddx9abi::HostStream* stream);     // +0x38 0x101B6E80
    virtual void readMaterial(renddx9abi::HostStream* stream, MeshOptimizerMaterial& material,
                              MeshOptimizerLod& lod);               // +0x3C 0x101B6760
    // Creates one D3DX mesh per LOD. False refuses the file.
    virtual bool buildMeshes();                                     // +0x40 0x101B5DB0
    virtual void optimizeMeshes();                                  // +0x44 0x101B5DC0
    virtual void writeHeader(renddx9abi::HostStream* stream);       // +0x48 0x101B5DD0
    virtual void writeGeometry(renddx9abi::HostStream* stream);     // +0x4C 0x101B6650
    virtual void writeMaterials(renddx9abi::HostStream* stream);    // +0x50 0x101B6420
    // Writes one material; its index start becomes `indexBase`, which then advances by the
    // material's index count.
    virtual void writeMaterial(renddx9abi::HostStream* stream, MeshOptimizerMaterial& material,
                               unsigned int& indexBase);            // +0x54 0x101B61B0

    // Objects come from the engine heap: the factories use engineAlloc and the deleting
    // destructors free through operator delete (sub_1005F360).
    static void operator delete(void* memory);

protected:
    // The release loop shared by both derived optimizeFile overrides:
    // releases every LOD's optimized and source mesh, then clears the per-geometry LOD vectors and
    // the LOD counts.
    void releaseLodMeshes();

    // The repack both derived optimizeMeshes bodies end with (0x10132CC0 for BundledMeshOptimizer,
    // 0x10135020 for StaticMeshOptimizer; the bodies differ only in the derived offsets). Gathers
    // every LOD's optimized vertices and indices into new staging arrays, then replaces
    // m_vertexData, m_indexData and m_indexCount. `stripIndexCount` is the derived strip total;
    // `geometryLods` holds the derived LOD records with their strip index buffers.
    template <class LodRecord>
    void repackBuffers(unsigned int stripIndexCount,
                       std::vector<std::vector<LodRecord> >& geometryLods);

    int                       m_refCount;           // +0x04
    unsigned char             m_byte08;             // +0x08  [CHOSEN, not recovered] name; only the constructor writes it
    std::string               m_sourcePath;         // +0x0C
    std::string               m_outputPath;         // +0x28
    unsigned int              m_head;               // +0x44
    unsigned int              m_version;            // +0x48
    unsigned int              m_deviceId;           // +0x4C
    unsigned int              m_driverVersion;      // +0x50
    unsigned int              m_header54;           // +0x54
    unsigned char             m_headerFlag;         // +0x58
    unsigned int              m_elementCount;       // +0x5C
    MeshOptimizerVertexElement* m_elements;         // +0x60  operator new[]
    unsigned int              m_primitiveType;      // +0x64  4 list, 5 after strip conversion
    unsigned int              m_vertexStride;       // +0x68
    unsigned int              m_vertexCount;        // +0x6C
    unsigned int              m_indexCount;         // +0x70
    unsigned int              m_u2;                 // +0x74
    unsigned char*            m_vertexData;         // +0x78  operator new[]
    unsigned short*           m_indexData;          // +0x7C  operator new[]
    void*                     m_vertexDeclaration;  // +0x80  BufferManager declaration wrapper
    std::vector<unsigned int> m_lodCounts;          // +0x84  LODs per geometry
    std::vector<MeshOptimizerLodVector> m_lods;     // +0x94  [geometry][lod]
    std::string               m_stringA4;           // +0xA4  [CHOSEN, not recovered] name; constructed and destroyed only
    unsigned int              m_unusedC0;           // +0xC0  [CHOSEN, not recovered] name; never accessed
    unsigned int              m_unusedC4;           // +0xC4  [CHOSEN, not recovered] name; never accessed
    unsigned int              m_optimizeFlags;      // +0xC8  only ever 0x04000000 (D3DXMESHOPT_VERTEXCACHE)
    std::vector<std::vector<MeshOptimizerMaterialVector> > m_materials; // +0xCC [geometry][lod][material]
    std::string               m_shaderFolder;       // +0xDC  "Shaders/"
    std::string               m_meshFolder;         // +0xF8  "Meshes/"

private:
    bool loadBody(renddx9abi::HostStream* stream);  // 0x101B5D20
    bool processSource();                           // 0x101B5E90
    bool checkOutput();                             // 0x101B5EF0
    void writeOutput();                             // 0x101B5F40

    MeshOptimizer(const MeshOptimizer&);
    MeshOptimizer& operator=(const MeshOptimizer&);
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_MESHOPTIMIZER_H
