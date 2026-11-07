// Path:     [INFERRED] Code/BF2/Geom/BundledMeshOptimizer.cpp, by analogy with the attested
//           StaticMeshOptimizer.cpp (RendDX9 0x1021D290); no __FILE__ string names this file.
// Contents: dice.hfe.geom.GeometryOptimizer.BundledMeshOptimizer, recovered from RendDX9.dll
//           (PE, symbols stripped). Primary table 0x1021D208, 0x134 bytes, factory 0x100B08D4.

#ifdef BF2_IMAGE_RENDDX9

#include "MeshOptimizer.h"

#include "../RendDX9/RendDX9Base.h"
#include "../IO/Stream/IStream.h"
#include "../../RendDX9Abi/BfMemory.h"
#include <d3dx9.h>

#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_rendererBase;                                       // dword_1023B970
IDirect3DVertexDeclaration9* vertexDeclarationD3D(void* wrapper);  // BufferManager.cpp
} // namespace dx9rend

namespace geom {

namespace {

// Reflection class id at 0x1021AF0C (Core/Ids.h geom::CID_BundledMeshOptimizer).
const unsigned int kClassId = 0x870CAA39u;

const unsigned long kLockNoSysLock = 0x800u;                 // D3DLOCK_NOSYSLOCK
const unsigned int kMeshOptVertexCache = 0x04000000u;        // D3DXMESHOPT_VERTEXCACHE
const unsigned int kMeshOptStripReorder = 0x08000000u;       // D3DXMESHOPT_STRIPREORDER
const unsigned int kMeshSystemMem = 0x110u;                  // D3DXMESH_SYSTEMMEM
const unsigned int kMeshIbManaged = 0x200u;                  // D3DXMESH_IB_MANAGED

IDirect3DDevice9* rendererDevice()
{
    return static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device;
}

// The 0x40000-byte stack array optimizeMeshes hands to GenerateAdjacency.
// [CHOSEN, not recovered] memory safety: it lives on the heap and grows to three entries per face
// when a LOD has more faces than the shipped array holds.
std::vector<DWORD> adjacencyBuffer(unsigned int faceCount)
{
    const unsigned int shipped = 0x10000u;
    return std::vector<DWORD>(faceCount * 3 > shipped ? faceCount * 3 : shipped);
}

class BundledMeshOptimizer : public MeshOptimizer
{
public:
    BundledMeshOptimizer();
    ~BundledMeshOptimizer() override;

    void addRef() override;                                          // +0x00 0x10134EB0
    int  getRef() override;                                          // +0x04 0x10138220
    int  release() override;                                         // +0x08 0x10134EC0
    unsigned int getClassId() const override;                        // +0x10 0x10134EA0
    bool optimize() override;                                        // +0x14 0x10132A00
    bool optimizeFile(const std::string& path) override;             // +0x18 0x10133240
    bool readHeader(renddx9abi::HostStream* stream) override;        // +0x24 0x10132A10
    void readGeometry(renddx9abi::HostStream* stream) override;      // +0x34 0x101336C0
    bool buildMeshes() override;                                     // +0x40 0x10132A40
    void optimizeMeshes() override;                                  // +0x44 0x10133350
    void writeGeometry(renddx9abi::HostStream* stream) override;     // +0x4C 0x10133170

private:
    // The class's own IBase count; the base's +0x04 count stays at 1.
    int          m_classRefCount;                                    // +0x114
    unsigned int m_unused118;    // +0x118 [CHOSEN, not recovered] name; never accessed
    unsigned int m_unused11C;    // +0x11C [CHOSEN, not recovered] name; never accessed
    unsigned int m_stripIndexCount;                                  // +0x120
    std::vector<std::vector<BundledMeshOptimizerLod> > m_geometryLods; // +0x124 [geometry][lod]
};

// PE 0x101335D0. The folder strings are assigned again after the base constructor.
// [CHOSEN, not recovered] the two unused dwords start at zero; retail leaves them unset.
BundledMeshOptimizer::BundledMeshOptimizer()
    : m_classRefCount(1), m_unused118(0), m_unused11C(0), m_stripIndexCount(0)
{
    m_shaderFolder = std::string("Shaders/");
    m_meshFolder = std::string("Meshes/");
}

// PE 0x10133660 (deleting form 0x10134EE0). The strip buffers held in the LOD records are not
// released.
BundledMeshOptimizer::~BundledMeshOptimizer()
{
}

// PE 0x10134EB0
void BundledMeshOptimizer::addRef()
{
    ++m_classRefCount;
}

// PE 0x10138220
int BundledMeshOptimizer::getRef()
{
    return m_classRefCount;
}

// PE 0x10134EC0. The last release runs the virtual deleting destructor.
int BundledMeshOptimizer::release()
{
    const int remaining = --m_classRefCount;
    if (!remaining)
        delete this;
    return remaining;
}

// PE 0x10134EA0
unsigned int BundledMeshOptimizer::getClassId() const
{
    return kClassId;
}

// PE 0x10132A00, `xor al, al`.
bool BundledMeshOptimizer::optimize()
{
    return false;
}

// PE 0x10133240
bool BundledMeshOptimizer::optimizeFile(const std::string& path)
{
    const bool result = MeshOptimizer::optimizeFile(path);
    releaseLodMeshes();
    return result;
}

// PE 0x10132A10. A bundled mesh needs version 5 or later.
bool BundledMeshOptimizer::readHeader(renddx9abi::HostStream* stream)
{
    const bool result = MeshOptimizer::readHeader(stream);
    if (m_head == 0 && m_version <= 4)
        return false;
    return result;
}

// PE 0x101336C0. Per LOD: the bounds (the 36-byte form before version 10), then the node count.
void BundledMeshOptimizer::readGeometry(renddx9abi::HostStream* stream)
{
    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    m_geometryLods.resize(geometryCount, std::vector<BundledMeshOptimizerLod>());
    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        m_geometryLods[geometry].resize(lodCount, BundledMeshOptimizerLod());
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            BundledMeshOptimizerLod& lod = m_geometryLods[geometry][lodIndex];
            if (m_head == 0 && m_version < 10)
                meshBoundsReadLegacy(lod.bounds, stream);
            else
                meshBoundsReadMinMax(lod.bounds, stream);
            stream->read(&lod.nodeCount, 4);
        }
    }
}

// PE 0x10132A40. One system-memory D3DX mesh per LOD from the LOD's vertex and index ranges, with
// one attribute per material. A LOD without materials or with alpha mode 1 refuses the file. The
// D3DX results are not checked.
bool BundledMeshOptimizer::buildMeshes()
{
    D3DVERTEXELEMENT9 elements[256];
    UINT elementCount;
    dx9rend::vertexDeclarationD3D(m_vertexDeclaration)->GetDeclaration(elements, &elementCount);

    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            MeshOptimizerMaterialVector& materials = m_materials[geometry][lodIndex];
            MeshOptimizerLod& lod = m_lods[geometry][lodIndex];
            if (materials.empty())
                return false;
            lod.materialCount = static_cast<unsigned int>(materials.size());
            if (lod.alphaMode == 1)
                return false;

            unsigned int indexSum = 0;
            unsigned int vertexSum = 0;
            for (unsigned int index = 0; index < lod.materialCount; ++index)
            {
                indexSum += materials[index].indexCount;
                vertexSum += materials[index].vertexCount;
            }
            const unsigned int faceCount = indexSum / 3;
            D3DXCreateMesh(faceCount, vertexSum, kMeshSystemMem, elements, rendererDevice(),
                           &lod.mesh);

            void* vertices;
            lod.mesh->LockVertexBuffer(kLockNoSysLock, &vertices);
            if (m_vertexData)
                std::memcpy(vertices, m_vertexData + materials[0].vertexStart * m_vertexStride,
                            m_vertexStride * vertexSum);
            lod.mesh->UnlockVertexBuffer();

            void* indices;
            lod.mesh->LockIndexBuffer(kLockNoSysLock, &indices);
            if (m_indexData)
                std::memcpy(indices, m_indexData + materials[0].indexStart, faceCount * 3 * 2);
            lod.mesh->UnlockIndexBuffer();

            DWORD* attributes;
            lod.mesh->LockAttributeBuffer(kLockNoSysLock, &attributes);
            unsigned int face = 0;
            unsigned int end = 0;
            for (unsigned int index = 0; index < lod.materialCount; ++index)
            {
                end += materials[index].indexCount / 3;
                for (; face < end; ++face)
                    attributes[face] = index;
            }
            lod.mesh->UnlockAttributeBuffer();
        }
    }
    return true;
}

// PE 0x10133350. Per LOD: rebase each material's indices onto the LOD's first vertex start (only
// for triangle lists), generate adjacency, vertex-cache optimize into a second mesh and, when the
// flags ask for strip reordering, convert every material subset to one strip. Ends with the
// shared repack. A LOD with alpha mode 1 and one material returns before the repack.
void BundledMeshOptimizer::optimizeMeshes()
{
    m_optimizeFlags = kMeshOptVertexCache;
    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            BundledMeshOptimizerLod& record = m_geometryLods[geometry][lodIndex];
            MeshOptimizerLod& lod = m_lods[geometry][lodIndex];
            MeshOptimizerMaterialVector& materials = m_materials[geometry][lodIndex];
            if (lod.alphaMode == 1 && lod.materialCount == 1)
                return;

            void* locked;
            lod.mesh->LockIndexBuffer(kLockNoSysLock, &locked);
            unsigned short* const indices = static_cast<unsigned short*>(locked);
            const unsigned int baseVertex = materials[0].vertexStart;
            unsigned int position = 0;
            unsigned int end = 0;
            unsigned int indexSum = 0;
            for (unsigned int index = 0; index < lod.materialCount; ++index)
            {
                indexSum += materials[index].indexCount;
                if (m_primitiveType == 4)
                    end += materials[index].indexCount;
                for (; position < end; ++position)
                    indices[position] = static_cast<unsigned short>(
                        indices[position] + static_cast<unsigned short>(
                                                materials[index].vertexStart - baseVertex));
            }
            lod.mesh->UnlockIndexBuffer();

            std::vector<DWORD> adjacency = adjacencyBuffer(indexSum / 3);
            lod.mesh->GenerateAdjacency(0.0f, &adjacency[0]);
            lod.optimizeFlags = m_optimizeFlags;
            if (lod.alphaMode == 1)
                lod.optimizeFlags = kMeshOptVertexCache;
            lod.mesh->Optimize(lod.optimizeFlags | kMeshSystemMem, &adjacency[0], 0, 0, 0,
                               &lod.optimizedMesh);

            if (lod.optimizeFlags == kMeshOptStripReorder)
            {
                // Dead in 1.50: m_optimizeFlags only ever holds the vertex-cache flag.
                void* unused;
                lod.optimizedMesh->LockIndexBuffer(kLockNoSysLock, &unused); // shipped: never read or unlocked
                record.strips.resize(lod.materialCount, 0);
                DWORD stripIndexCount = 0;
                for (unsigned int index = 0; index < lod.materialCount; ++index)
                {
                    D3DXConvertMeshSubsetToSingleStrip(lod.optimizedMesh, index, kMeshIbManaged,
                                                       &record.strips[index], &stripIndexCount);
                    m_primitiveType = 5;
                    materials[index].indexCount = stripIndexCount;
                    m_stripIndexCount += stripIndexCount;
                }
            }
        }
    }
    repackBuffers(m_stripIndexCount, m_geometryLods);
}

// PE 0x10133170. The base tables, the index tail, then per LOD the bounds through the defective
// 36-byte writer and the node count. The repacked vertex and index arrays are freed afterwards and
// the pointers kept.
void BundledMeshOptimizer::writeGeometry(renddx9abi::HostStream* stream)
{
    MeshOptimizer::writeGeometry(stream);
    stream->write(&m_u2, 4);
    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            BundledMeshOptimizerLod& lod = m_geometryLods[geometry][lodIndex];
            meshBoundsWriteLegacy(lod.bounds, stream);
            stream->write(&lod.nodeCount, 4);
        }
    }
    delete[] m_vertexData;
    delete[] m_indexData;
}

} // anonymous namespace

// PE 0x100B08D4
IBase* __fastcall createBundledMeshOptimizer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(BundledMeshOptimizer));
    BundledMeshOptimizer* const self = storage ? new (storage) BundledMeshOptimizer() : 0;
    IBase* const result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
