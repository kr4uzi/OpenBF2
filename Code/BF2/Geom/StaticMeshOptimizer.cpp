// Path:     attested - "C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\Geom\StaticMeshOptimizer.cpp"
//           appears verbatim in RendDX9.dll at 0x1021D290.
// Contents: dice.hfe.geom.GeometryOptimizer.StaticMeshOptimizer, recovered from RendDX9.dll
//           (PE, symbols stripped). Primary table 0x1021D2E0, 0x154 bytes, factory 0x100B0916.

#ifdef BF2_IMAGE_RENDDX9

#include "MeshOptimizer.h"

#include "../RendDX9/RendDX9Base.h"
#include "../IO/Stream/IStream.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
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

// Reflection class id at 0x1021AF14 (Core/Ids.h geom::CID_StaticMeshOptimizer).
const unsigned int kClassId = 0x870CAA3Bu;

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

class StaticMeshOptimizer : public MeshOptimizer
{
public:
    StaticMeshOptimizer();
    ~StaticMeshOptimizer() override;

    void addRef() override;                                          // +0x00 0x10134EB0
    int  getRef() override;                                          // +0x04 0x10138220
    int  release() override;                                         // +0x08 0x10134EC0
    unsigned int getClassId() const override;                        // +0x10 0x10138210
    bool optimize() override;                                        // +0x14 0x10134F10
    bool optimizeFile(const std::string& path) override;             // +0x18 0x101355F0
    bool readOutputHeader(renddx9abi::HostStream* stream) override;  // +0x28 0x10134F20
    void readGeometry(renddx9abi::HostStream* stream) override;      // +0x34 0x10135DF0
    bool buildMeshes() override;                                     // +0x40 0x10135A00
    void optimizeMeshes() override;                                  // +0x44 0x10135700
    void writeHeader(renddx9abi::HostStream* stream) override;       // +0x48 0x10134F50
    void writeGeometry(renddx9abi::HostStream* stream) override;     // +0x4C 0x101354D0
    void writeMaterial(renddx9abi::HostStream* stream, MeshOptimizerMaterial& material,
                       unsigned int& indexBase) override;            // +0x54 0x10134FF0

private:
    // The class's own IBase count; the base's +0x04 count stays at 1.
    int          m_classRefCount;                                    // +0x114
    std::string  m_string118;    // +0x118 [CHOSEN, not recovered] name; constructed and destroyed only
    unsigned int m_unused134;    // +0x134 [CHOSEN, not recovered] name; never accessed
    unsigned int m_unused138;    // +0x138 [CHOSEN, not recovered] name; never accessed
    unsigned int m_stripIndexCount;                                  // +0x13C
    Vec3*        m_positions;    // +0x140 one position per vertex, operator new[]
    std::vector<std::vector<StaticMeshOptimizerLod> > m_geometryLods; // +0x144 [geometry][lod]
};

// PE 0x10135CE0. The folder strings are assigned again after the base constructor.
// [CHOSEN, not recovered] the two unused dwords start at zero; retail leaves them unset.
StaticMeshOptimizer::StaticMeshOptimizer()
    : m_classRefCount(1), m_unused134(0), m_unused138(0), m_stripIndexCount(0), m_positions(0)
{
    m_shaderFolder = std::string("Shaders/");
    m_meshFolder = std::string("Meshes/");
}

// PE 0x10135D80 (deleting form 0x10138230). The position array and the strip buffers are not
// released.
StaticMeshOptimizer::~StaticMeshOptimizer()
{
}

// PE 0x10134EB0, shared with BundledMeshOptimizer.
void StaticMeshOptimizer::addRef()
{
    ++m_classRefCount;
}

// PE 0x10138220, shared with BundledMeshOptimizer.
int StaticMeshOptimizer::getRef()
{
    return m_classRefCount;
}

// PE 0x10134EC0, shared with BundledMeshOptimizer. The last release runs the virtual deleting
// destructor.
int StaticMeshOptimizer::release()
{
    const int remaining = --m_classRefCount;
    if (!remaining)
        delete this;
    return remaining;
}

// PE 0x10138210
unsigned int StaticMeshOptimizer::getClassId() const
{
    return kClassId;
}

// PE 0x10134F10, `xor al, al`.
bool StaticMeshOptimizer::optimize()
{
    return false;
}

// PE 0x101355F0
bool StaticMeshOptimizer::optimizeFile(const std::string& path)
{
    const bool result = MeshOptimizer::optimizeFile(path);
    releaseLodMeshes();
    return result;
}

// PE 0x10134F20. An existing output of version 3 or 4 is always rewritten.
bool StaticMeshOptimizer::readOutputHeader(renddx9abi::HostStream* stream)
{
    const bool result = MeshOptimizer::readOutputHeader(stream);
    if (m_head == 0 && m_version >= 3 && m_version < 5)
        return true;
    return result;
}

// PE 0x10135DF0. Per LOD, cleared after the resize: the bounds (the 36-byte form before
// version 10), the matrix count and the matrices, which start as identity. The version is raised
// to 5, then every vertex's first 12 bytes are gathered into m_positions.
void StaticMeshOptimizer::readGeometry(renddx9abi::HostStream* stream)
{
    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    m_geometryLods.resize(geometryCount, std::vector<StaticMeshOptimizerLod>());
    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        m_geometryLods[geometry].resize(lodCount, StaticMeshOptimizerLod());
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            StaticMeshOptimizerLod& lod = m_geometryLods[geometry][lodIndex];
            lod.clear();
            if (m_head == 0 && m_version < 10)
                meshBoundsReadLegacy(lod.bounds, stream);
            else
                meshBoundsReadMinMax(lod.bounds, stream);
            stream->read(&lod.matrixCount, 4);
            MeshOptimizerMatrix identity;
            std::memset(&identity, 0, sizeof(identity));
            identity.m[0] = identity.m[5] = identity.m[10] = identity.m[15] = 1.0f;
            lod.matrices.resize(lod.matrixCount, identity);
            for (unsigned int index = 0; index < lod.matrixCount; ++index)
                stream->read(&lod.matrices[index], 0x40);
        }
    }

    if (m_version < 5)
        m_version = 5;

    Vec3* const staging = new Vec3[m_vertexCount];
    Vec3* const positions = new Vec3[m_vertexCount];
    const unsigned int count = m_vertexCount;
    for (unsigned int index = 0; index < count; ++index)
    {
        positions[index].x = 0.0f;
        positions[index].y = 0.0f;
        positions[index].z = 0.0f;
    }
    m_positions = positions;
    for (unsigned int index = 0; index < m_vertexCount; ++index)
        std::memcpy(&staging[index], m_vertexData + index * m_vertexStride, 12);
    std::memcpy(m_positions, staging, m_vertexCount * 12);
    delete[] staging;
}

// PE 0x10135A00. One system-memory D3DX mesh per LOD, as in BundledMeshOptimizer, except that the
// LOD table is resized again, an empty material list is accepted, and a LOD with alpha mode 1
// leaves its last material out of the mesh (a single such material refuses the file).
bool StaticMeshOptimizer::buildMeshes()
{
    D3DVERTEXELEMENT9 elements[256];
    UINT elementCount;
    dx9rend::vertexDeclarationD3D(m_vertexDeclaration)->GetDeclaration(elements, &elementCount);

    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    m_lods.resize(m_lodCounts.size(), MeshOptimizerLodVector());
    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        const MeshOptimizerLod prototype = { 0, 0, 0, 0, 0 };
        m_lods[geometry].resize(lodCount, prototype);
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            MeshOptimizerMaterialVector& materials = m_materials[geometry][lodIndex];
            MeshOptimizerLod& lod = m_lods[geometry][lodIndex];
            const unsigned int materialCount = static_cast<unsigned int>(materials.size());
            lod.materialCount = materialCount;
            if (lod.alphaMode == 1)
            {
                if (materialCount == 1)
                    return false;
                lod.materialCount = materialCount - 1;
            }

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

// PE 0x10135700. BundledMeshOptimizer's pass plus per-material bounds: after the rebase every
// index of a material expands that material's bounds by m_positions[index]. Shipped: the rebased
// index is relative to the LOD's first vertex while m_positions starts at the mesh's first vertex.
// The position array is freed before the shared repack.
void StaticMeshOptimizer::optimizeMeshes()
{
    m_optimizeFlags = kMeshOptVertexCache;
    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            StaticMeshOptimizerLod& record = m_geometryLods[geometry][lodIndex];
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

            position = 0;
            end = 0;
            for (unsigned int index = 0; index < lod.materialCount; ++index)
            {
                if (m_primitiveType == 4)
                    end += materials[index].indexCount;
                for (; position < end; ++position)
                    meshBoundsExpand(materials[index].bounds, m_positions[indices[position]]);
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
    delete[] m_positions;
    m_positions = 0;
    repackBuffers(m_stripIndexCount, m_geometryLods);
}

// PE 0x10134F50. Versions before 5 are reported, then the base header is written.
void StaticMeshOptimizer::writeHeader(renddx9abi::HostStream* stream)
{
    if (m_version < 5)
        ::renddx9abi::logf("StaticMeshOptimizer.cpp:561: saveHeader %u", m_version);
    MeshOptimizer::writeHeader(stream);
}

// PE 0x101354D0. The base tables, the index tail, then per LOD the bounds (the defective 36-byte
// writer before version 10, min/max otherwise), the matrix count and the matrices. The repacked
// vertex and index arrays are freed afterwards and the pointers kept.
void StaticMeshOptimizer::writeGeometry(renddx9abi::HostStream* stream)
{
    MeshOptimizer::writeGeometry(stream);
    stream->write(&m_u2, 4);
    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            StaticMeshOptimizerLod& lod = m_geometryLods[geometry][lodIndex];
            if (m_head == 0 && m_version < 10)
                meshBoundsWriteLegacy(lod.bounds, stream);
            else
                meshBoundsWriteMinMax(lod.bounds, stream);
            stream->write(&lod.matrixCount, 4);
            for (unsigned int index = 0; index < lod.matrixCount; ++index)
                stream->write(&lod.matrices[index], 0x40);
        }
    }
    delete[] m_vertexData;
    delete[] m_indexData;
}

// PE 0x10134FF0. The base record, then the material bounds through the defective 36-byte writer.
void StaticMeshOptimizer::writeMaterial(renddx9abi::HostStream* stream,
                                        MeshOptimizerMaterial& material, unsigned int& indexBase)
{
    MeshOptimizer::writeMaterial(stream, material, indexBase);
    meshBoundsWriteLegacy(material.bounds, stream);
}

} // anonymous namespace

// PE 0x100B0916
IBase* __fastcall createStaticMeshOptimizer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(StaticMeshOptimizer));
    StaticMeshOptimizer* const self = storage ? new (storage) StaticMeshOptimizer() : 0;
    IBase* const result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
