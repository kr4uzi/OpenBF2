// Path:     attested - "C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\Geom\MeshOptimizer.cpp"
//           appears verbatim in RendDX9.dll at 0x10221E70.
// Contents: the mesh-optimizer base, recovered from RendDX9.dll (PE, symbols stripped).
// Reads and allocations are unchecked, as shipped: the file path expects well-formed mesh content.

#ifdef BF2_IMAGE_RENDDX9

#include "MeshOptimizer.h"

#include "../RendDX9/RendDX9Base.h"
#include "../Core/StringUtil.h"         // extractFileName / extractFilePath (sub_10075190 / sub_100751A0)
#include "../IO/Stream/IStream.h"
#include "../IO/File/IFileSystem.h"            // io::FileInfo, findFiles (+0x34)
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <d3dx9.h>

#include <cstring>
#include <list>
#include <map>
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

// dword_1021AE54, the optimizer interface id.
const unsigned int kIID_IGeometryOptimizer = 0x138E7u;

// D3DLOCK_NOSYSLOCK, the lock flag every optimizer buffer lock passes.
const unsigned long kLockNoSysLock = 0x800u;

// D3DXMESHOPT_STRIPREORDER. The optimize flags only ever hold D3DXMESHOPT_VERTEXCACHE, so the
// strip branches compare against a value they never see.
const unsigned int kMeshOptStripReorder = 0x08000000u;

renddx9abi::HostFileManager* files()
{
    return ::dice::hfe::fileManager;                               // dword_10244AC4
}

// A length-prefixed name: count, then the bytes, staged through a NUL-terminated operator new[]
// copy as shipped.
void readName(renddx9abi::HostStream* stream, std::string& out)
{
    unsigned int length;
    stream->read(&length, 4);
    char* const raw = new char[length + 1];
    stream->read(raw, length);
    raw[length] = 0;
    out = std::string(raw);
    delete[] raw;
}

void writeName(renddx9abi::HostStream* stream, const std::string& name)
{
    unsigned int length = static_cast<unsigned int>(name.size());
    stream->write(&length, 4);
    stream->write(name.data(), length);
}

// The final path component without its last extension: the shared stem helper (Core
// extractFileNameWithoutExt; RendDX9 carries its own copy), extractFileName then rfind('.').
std::string fileStemOf(const std::string& path)
{
    const std::string name = extractFileName(path);
    const std::string::size_type dot = name.rfind('.');
    if (dot == std::string::npos)
        return name;
    return name.substr(0, dot);
}

} // anonymous namespace


MeshOptimizerMaterial::MeshOptimizerMaterial()
    : alphaMode(0), vertexStart(0), indexStart(0), indexCount(0), vertexCount(0),
      unused5C(0), unused60(0)
{
    std::memset(&bounds, 0, sizeof(bounds));
    bounds.dirty = 1;
}

void MeshOptimizerMaterial::clear()
{
    shaderFile.clear();
    technique.clear();
    std::vector<std::string>().swap(textures);
    alphaMode = 0;
    vertexStart = 0;
    indexStart = 0;
    indexCount = 0;
    vertexCount = 0;
    unused5C = 0;
    unused60 = 0;
    std::memset(&bounds, 0, sizeof(bounds));
}

BundledMeshOptimizerLod::BundledMeshOptimizerLod()
    : nodeCount(0)
{
    std::memset(&bounds, 0, sizeof(bounds));
    bounds.dirty = 1;
}

StaticMeshOptimizerLod::StaticMeshOptimizerLod()
    : matrixCount(0)
{
    std::memset(&bounds, 0, sizeof(bounds));
    bounds.dirty = 1;
}

void StaticMeshOptimizerLod::clear()
{
    matrixCount = 0;
    std::vector<MeshOptimizerMatrix>().swap(matrices);
    std::memset(&bounds, 0, sizeof(bounds));
    std::vector<IDirect3DIndexBuffer9*>().swap(strips);
}


// PE 0x101B6CC0. The two folder strings are assigned from temporaries after construction.
// [CHOSEN, not recovered] The header, buffer and flag scalars start at zero; retail leaves
// +0x44..+0x7C and +0xC0..+0xC8 unset until a file is read. m_vertexDeclaration is zeroed by the
// shipped constructor too.
MeshOptimizer::MeshOptimizer()
    : m_refCount(1), m_byte08(0),
      m_head(0), m_version(0), m_deviceId(0), m_driverVersion(0), m_header54(0), m_headerFlag(0),
      m_elementCount(0), m_elements(0), m_primitiveType(0), m_vertexStride(0), m_vertexCount(0),
      m_indexCount(0), m_u2(0), m_vertexData(0), m_indexData(0), m_vertexDeclaration(0),
      m_unusedC0(0), m_unusedC4(0), m_optimizeFlags(0)
{
    m_shaderFolder = std::string("Shaders/");
    m_meshFolder = std::string("Meshes/");
}

// PE 0x101B6DA0. Only the members are destroyed: the element, vertex and index arrays and the
// vertex declaration are not released here.
MeshOptimizer::~MeshOptimizer()
{
}

void MeshOptimizer::operator delete(void* memory)
{
    ::renddx9abi::engineFree(memory);
}

// PE 0x1000CB20, the shared IBase increment.
void MeshOptimizer::addRef()
{
    ++m_refCount;
}

// PE 0x101A9A10, the shared count getter.
int MeshOptimizer::getRef()
{
    return m_refCount;
}

// PE 0x101B8E50. The last release runs the virtual deleting destructor (slot +0x20).
int MeshOptimizer::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        delete this;
    return remaining;
}

// PE 0x10134E70, shared by all three tables.
void* MeshOptimizer::queryInterface(unsigned int iid) const
{
    if (iid == 1u || iid == kIID_IGeometryOptimizer)
        return const_cast<MeshOptimizer*>(this);
    return 0;
}

// PE 0x100ED3B0, the shared `xor eax, eax` body.
unsigned int MeshOptimizer::getClassId() const
{
    return 0;
}

// PE 0x101B5D10. GeometryTemplateManager::runOptimizers' only call; it does no work.
bool MeshOptimizer::optimize()
{
    return false;
}

// PE 0x101B6940. Unreachable in 1.50: no code or data references. The file list is
// IFileManager::findFiles(directory, true) (+0x34); a zero-size entry is skipped; the extension is
// found anywhere in the name (find(ext, 0, size)), then the plain file name must equal the stem
// plus the extension. Matches are keyed by `directory + name` in a string map whose value stays 0.
bool MeshOptimizer::optimizeDirectory(const std::string& directory, const std::string& extension)
{
    std::list<io::FileInfo> listed;
    files()->findFiles(directory, true, listed);
    std::map<std::string, int> paths;
    for (std::list<io::FileInfo>::const_iterator it = listed.begin(); it != listed.end(); ++it)
    {
        if (it->size == 0)
            continue;
        if (it->name.find(extension.c_str(), 0, extension.size()) == std::string::npos)
            continue;
        const std::string stem = fileStemOf(it->name);
        const std::string fileName = extractFileName(it->name);
        bool matches = false;
        if (std::strncmp(stem.c_str(), "gpu_", 4) != 0)
            matches = fileName.compare(std::string(stem) += extension) == 0;
        if (matches)
        {
            const std::string path = std::string(directory) += it->name;
            paths.insert(std::pair<const std::string, int>(path, 0));
        }
    }
    std::map<std::string, int>::iterator path = paths.begin();
    if (path == paths.end() || !optimizeFile(path->first))
        return false;
    // Shipped: every later file is optimized but its result is discarded.
    for (++path; path != paths.end(); ++path)
        optimizeFile(path->first);
    return true;
}

// PE 0x101B5E30
bool MeshOptimizer::optimizeFile(const std::string& path)
{
    const bool result = processFile(path);
    if (m_vertexDeclaration)
        dx9rend::vertexDeclarationRelease(m_vertexDeclaration);
    m_vertexDeclaration = 0;
    return result;
}

// PE 0x101B5F90. The output is `<directory>/gpu_<file>`. An existing, newer output is only
// re-read for its header; otherwise the source is processed. A source that cannot be opened
// counts as processed, so the output is then written from the current state.
bool MeshOptimizer::processFile(const std::string& path)
{
    m_sourcePath = path;
    dx9rend::pathReplaceSeparators(m_sourcePath, '/');
    const std::string fileName = extractFileName(m_sourcePath);
    const std::string directory = extractFilePath(m_sourcePath, '/');
    m_outputPath = directory + std::string("gpu_") + fileName;

    if (files()->fileExists(renddx9abi::makeHostString(m_outputPath))
        && files()->compareFileTime(renddx9abi::makeHostString(m_outputPath),
                                    renddx9abi::makeHostString(m_sourcePath)))
    {
        if (!checkOutput())
            return false;
    }
    else if (!processSource())
    {
        return false;
    }

    writeOutput();
    return true;
}

// PE 0x101B5D20. The read sequence after the header; the result is buildMeshes'.
bool MeshOptimizer::loadBody(renddx9abi::HostStream* stream)
{
    readVertexData(stream);
    readIndexTail(stream);
    readGeometry(stream);
    readMaterials(stream);
    if (!buildMeshes())
        return false;
    optimizeMeshes();
    return true;
}

// PE 0x101B5E90. Opens the source for reading (mode 2); a missing file answers true.
bool MeshOptimizer::processSource()
{
    renddx9abi::HostStream* const stream = files()->open(
        renddx9abi::makeHostString(m_sourcePath), static_cast<renddx9abi::HostFileMode>(2),
        static_cast<renddx9abi::HostFileAccess>(0), 0);
    bool result = true;
    if (!stream)
        return result;
    if (!readHeader(stream))
    {
        stream->release();
        return false;
    }
    if (!loadBody(stream))
        result = false;
    stream->release();
    return result;
}

// PE 0x101B5EF0. A missing output answers true; an output whose header no longer matches the
// adapter sends the source through processSource.
bool MeshOptimizer::checkOutput()
{
    renddx9abi::HostStream* const stream = files()->open(
        renddx9abi::makeHostString(m_outputPath), static_cast<renddx9abi::HostFileMode>(2),
        static_cast<renddx9abi::HostFileAccess>(0), 0);
    if (!stream)
        return true;
    if (!readOutputHeader(stream))
    {
        stream->release();
        return false;
    }
    stream->release();
    return processSource();
}

// PE 0x101B5F40. Opens the output for writing (mode 0, access 2).
void MeshOptimizer::writeOutput()
{
    renddx9abi::HostStream* const stream = files()->open(
        renddx9abi::makeHostString(m_outputPath), static_cast<renddx9abi::HostFileMode>(0),
        static_cast<renddx9abi::HostFileAccess>(2), 0);
    if (!stream)
        return;
    writeHeader(stream);
    writeGeometry(stream);
    writeMaterials(stream);
    stream->release();
}

// PE 0x101B6100. Head 0 and version 3 or later are required. The adapter is the first identifier
// the renderer enumerated; a file already stamped with its device id needs no work.
bool MeshOptimizer::readHeader(renddx9abi::HostStream* stream)
{
    const D3DADAPTER_IDENTIFIER9* const adapter =
        static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->adapterInfo.data();
    stream->read(&m_head, 4);
    stream->read(&m_version, 4);
    if (m_head != 0 || m_version < 3)
        return false;
    stream->read(&m_deviceId, 4);
    stream->read(&m_driverVersion, 4);
    stream->read(&m_header54, 4);
    stream->read(&m_headerFlag, 1);
    if (adapter->DeviceId == m_deviceId)
        return false;
    m_deviceId = adapter->DeviceId;
    // movzx of the word at identifier +0x422: the high half of DriverVersion.LowPart.
    m_driverVersion = static_cast<unsigned short>(adapter->DriverVersion.LowPart >> 16);
    return true;
}

// PE 0x101B5D70, a tail jump to slot +0x24.
bool MeshOptimizer::readOutputHeader(renddx9abi::HostStream* stream)
{
    return readHeader(stream);
}

// PE 0x101B6520. LOD counts, the vertex declaration, then the vertex and index arrays. Earlier
// arrays are not freed; the element array is kept for writeGeometry.
void MeshOptimizer::readVertexData(renddx9abi::HostStream* stream)
{
    unsigned int geometryCount;
    stream->read(&geometryCount, 4);
    m_lodCounts.resize(geometryCount, 0);
    for (unsigned int i = 0; i < geometryCount; ++i)
        stream->read(&m_lodCounts[i], 4);

    stream->read(&m_elementCount, 4);
    m_elements = new MeshOptimizerVertexElement[m_elementCount];
    for (unsigned int i = 0; i < m_elementCount; ++i)
        stream->read(&m_elements[i], 8);
    m_vertexDeclaration = dx9rend::vertexDeclarationCreate(m_elements);

    stream->read(&m_primitiveType, 4);
    stream->read(&m_vertexStride, 4);
    stream->read(&m_vertexCount, 4);
    m_vertexData = new unsigned char[m_vertexStride * m_vertexCount];
    if (m_vertexData)
        stream->read(m_vertexData, m_vertexStride * m_vertexCount);

    stream->read(&m_indexCount, 4);
    m_indexData = new unsigned short[m_indexCount];
    if (m_indexData)
        stream->read(m_indexData, m_indexCount * 2);
}

// PE 0x101B5D80
void MeshOptimizer::readIndexTail(renddx9abi::HostStream* stream)
{
    stream->read(&m_u2, 4);
}

// PE 0x101B5DA0, an empty `ret 4`: the base reads no per-LOD geometry.
void MeshOptimizer::readGeometry(renddx9abi::HostStream*)
{
}

// PE 0x101B6E80. Sizes the material and LOD tables from the LOD counts, then reads each LOD's
// material count and materials. Each LOD record and each material is cleared before it is read.
void MeshOptimizer::readMaterials(renddx9abi::HostStream* stream)
{
    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    m_materials.resize(geometryCount, std::vector<MeshOptimizerMaterialVector>());
    m_lods.resize(geometryCount, MeshOptimizerLodVector());

    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        m_materials[geometry].resize(lodCount, MeshOptimizerMaterialVector());
        const MeshOptimizerLod prototype = { 0, 0, 0, 0, 0 };
        m_lods[geometry].resize(lodCount, prototype);

        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            MeshOptimizerLod& lod = m_lods[geometry][lodIndex];
            lod.mesh = 0;
            lod.optimizedMesh = 0;
            lod.optimizeFlags = 0;
            lod.alphaMode = 0;
            lod.materialCount = 0;

            unsigned int materialCount;
            stream->read(&materialCount, 4);
            m_materials[geometry][lodIndex].resize(materialCount, MeshOptimizerMaterial());
            for (unsigned int index = 0; index < materialCount; ++index)
            {
                MeshOptimizerMaterial& material = m_materials[geometry][lodIndex][index];
                material.clear();
                readMaterial(stream, material, lod);
            }
        }
    }
}

// PE 0x101B6760. The LOD adopts the first alpha mode that differs from its own unless it already
// holds 1; any adoption sets the vertex-cache optimize flag. The two trailing dwords are skipped.
void MeshOptimizer::readMaterial(renddx9abi::HostStream* stream, MeshOptimizerMaterial& material,
                                 MeshOptimizerLod& lod)
{
    stream->read(&material.alphaMode, 4);
    if (lod.alphaMode != material.alphaMode && lod.alphaMode != 1)
    {
        lod.alphaMode = material.alphaMode;
        m_optimizeFlags = 0x04000000u;
    }

    readName(stream, material.shaderFile);
    readName(stream, material.technique);

    int textureCount;
    stream->read(&textureCount, 4);
    material.textures.resize(textureCount);
    for (int i = 0; i < textureCount; ++i)
        readName(stream, material.textures[i]);

    stream->read(&material.vertexStart, 4);
    stream->read(&material.indexStart, 4);
    stream->read(&material.indexCount, 4);
    stream->read(&material.vertexCount, 4);
    stream->skip(4);
    stream->skip(4);
}

// PE 0x101B5DB0, `xor al, al`: the base builds nothing and refuses the file.
bool MeshOptimizer::buildMeshes()
{
    return false;
}

// PE 0x101B5DC0, an empty `ret`.
void MeshOptimizer::optimizeMeshes()
{
}

// PE 0x101B5DD0
void MeshOptimizer::writeHeader(renddx9abi::HostStream* stream)
{
    stream->write(&m_head, 4);
    stream->write(&m_version, 4);
    stream->write(&m_deviceId, 4);
    stream->write(&m_driverVersion, 4);
    stream->write(&m_header54, 4);
    stream->write(&m_headerFlag, 1);
}

// PE 0x101B6650. The element array is freed after it is written and the pointer kept. The LOD
// count vector is resized to its own size, as shipped.
void MeshOptimizer::writeGeometry(renddx9abi::HostStream* stream)
{
    unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    stream->write(&geometryCount, 4);
    m_lodCounts.resize(geometryCount, 0);
    for (unsigned int i = 0; i < geometryCount; ++i)
        stream->write(&m_lodCounts[i], 4);

    stream->write(&m_elementCount, 4);
    for (unsigned int i = 0; i < m_elementCount; ++i)
        stream->write(&m_elements[i], 8);
    delete[] m_elements;

    stream->write(&m_primitiveType, 4);
    stream->write(&m_vertexStride, 4);
    stream->write(&m_vertexCount, 4);
    stream->write(m_vertexData, m_vertexStride * m_vertexCount);
    stream->write(&m_indexCount, 4);
    stream->write(m_indexData, m_indexCount * 2);
}

// PE 0x101B6420. Every LOD's material count, then its materials; the index base runs across all
// geometries and LODs.
void MeshOptimizer::writeMaterials(renddx9abi::HostStream* stream)
{
    unsigned int indexBase = 0;
    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            MeshOptimizerMaterialVector& materials = m_materials[geometry][lodIndex];
            unsigned int materialCount = static_cast<unsigned int>(materials.size());
            stream->write(&materialCount, 4);
            for (unsigned int index = 0; index < materialCount; ++index)
                writeMaterial(stream, materials[index], indexBase);
        }
    }
}

// PE 0x101B61B0. The obsolete specular LUT name is reported and replaced, then every
// SpecularLUT_pow36 name is redirected to the absolute common texture. The two trailing dwords
// are written as zero.
void MeshOptimizer::writeMaterial(renddx9abi::HostStream* stream, MeshOptimizerMaterial& material,
                                  unsigned int& indexBase)
{
    stream->write(&material.alphaMode, 4);
    writeName(stream, material.shaderFile);
    writeName(stream, material.technique);

    int textureCount = static_cast<int>(material.textures.size());
    stream->write(&textureCount, 4);
    for (int i = 0; i < textureCount; ++i)
    {
        std::string name(material.textures[i]);
        if (name.find("default_sdgbmfbf_color_lut", 0, 26) != std::string::npos)
        {
            ::renddx9abi::logf("MeshOptimizer.cpp:533: Mesh %s has specular lut "
                               "default_sdgbmfbf_color_lut which is obsolete, please re-export",
                               m_sourcePath.c_str());
            name = "_SpecularLUT_pow36_";
        }
        if (name.find("SpecularLUT_pow36", 0, 17) != std::string::npos)
            name = "@@ABSOLUTE@@Common/Textures/SpecularLUT_pow36.dds";
        writeName(stream, name);
    }

    unsigned int zero = 0;
    stream->write(&material.vertexStart, 4);
    material.indexStart = indexBase;
    stream->write(&material.indexStart, 4);
    stream->write(&material.indexCount, 4);
    stream->write(&material.vertexCount, 4);
    stream->write(&zero, 4);
    stream->write(&zero, 4);
    indexBase += material.indexCount;
}

// The shared tail of both derived optimizeFile overrides. VC7.1's vector::clear also frees the
// storage.
void MeshOptimizer::releaseLodMeshes()
{
    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());
    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        MeshOptimizerLodVector& lods = m_lods[geometry];
        const unsigned int lodCount = m_lodCounts[geometry];
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            MeshOptimizerLod& lod = lods[lodIndex];
            if (lod.optimizedMesh)
            {
                lod.optimizedMesh->Release();
                lod.optimizedMesh = 0;
            }
            if (lod.mesh)
            {
                lod.mesh->Release();
                lod.mesh = 0;
            }
        }
        lods.clear();
    }
    m_lodCounts.clear();
}

// PE 0x10132CC0 / 0x10135020. The vertex pass copies each LOD's optimized vertices, plus, for an
// alpha-mode-1 LOD, the raw vertices of the material StaticMeshOptimizer::buildMeshes left out of
// the D3DX mesh (materials[materialCount]). The index pass rebases each material's indices onto
// the LOD's first vertex start, from the strip buffers when the LOD was strip-optimized and from
// the optimized mesh otherwise.
template <class LodRecord>
void MeshOptimizer::repackBuffers(unsigned int stripIndexCount,
                                  std::vector<std::vector<LodRecord> >& geometryLods)
{
    const unsigned int vertexBytes = m_vertexCount * m_vertexStride;
    unsigned int indexTotal = m_indexCount;
    if (stripIndexCount != 0 && stripIndexCount != m_indexCount)
        indexTotal = stripIndexCount;
#if defined(_WIN32)
    SYSTEM_INFO systemInfo;
    ::GetSystemInfo(&systemInfo);       // shipped: the result is never read
#endif
    // [PLATFORM] The native ports have no GetSystemInfo; the call has no effect on the result.

    unsigned char* const vertices = new unsigned char[vertexBytes];
    unsigned char* vertexCursor = vertices;
    unsigned int vertexOffset = 0;
    const unsigned int geometryCount = static_cast<unsigned int>(m_lodCounts.size());

    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            MeshOptimizerLod& lod = m_lods[geometry][lodIndex];
            MeshOptimizerMaterialVector& materials = m_materials[geometry][lodIndex];
            unsigned int bytes = 0;
            for (unsigned int index = 0; index < lod.materialCount; ++index)
                bytes += materials[index].vertexCount * m_vertexStride;

            void* source;
            lod.optimizedMesh->LockVertexBuffer(kLockNoSysLock, &source);
            std::memcpy(vertexCursor, source, bytes);
            vertexOffset += bytes;
            vertexCursor += bytes;
            lod.optimizedMesh->UnlockVertexBuffer();

            if (lod.alphaMode == 1)
            {
                const unsigned int tail = materials[lod.materialCount].vertexCount * m_vertexStride;
                std::memcpy(vertexCursor, m_vertexData + vertexOffset, tail);
                vertexOffset += tail;
                vertexCursor += tail;
            }
        }
    }

    delete[] m_vertexData;
    m_vertexData = vertices;

    unsigned short* const indices = new unsigned short[indexTotal];
    unsigned short* indexCursor = indices;
    unsigned int indexOffset = 0;   // bytes

    for (unsigned int geometry = 0; geometry < geometryCount; ++geometry)
    {
        const unsigned int lodCount = m_lodCounts[geometry];
        for (unsigned int lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            MeshOptimizerLod& lod = m_lods[geometry][lodIndex];
            MeshOptimizerMaterialVector& materials = m_materials[geometry][lodIndex];
            LodRecord& record = geometryLods[geometry][lodIndex];
            const unsigned int baseVertex = materials[0].vertexStart;

            if (lod.optimizeFlags == kMeshOptStripReorder)
            {
                // Dead in 1.50 (see kMeshOptStripReorder). Each strip buffer is rebased in place,
                // copied, unlocked and released; its pointer stays in the record.
                for (unsigned int index = 0; index < record.strips.size(); ++index)
                {
                    IDirect3DIndexBuffer9* const strip = record.strips[index];
                    D3DINDEXBUFFER_DESC description;
                    strip->GetDesc(&description);            // shipped: never read
                    void* locked;
                    strip->Lock(0, 0, &locked, kLockNoSysLock);
                    unsigned short* const source = static_cast<unsigned short*>(locked);
                    const unsigned int bytes = materials[index].indexCount * 2;
                    for (unsigned int i = 0; i < materials[index].indexCount; ++i)
                        source[i] = static_cast<unsigned short>(
                            source[i] + static_cast<unsigned short>(baseVertex
                                                                    - materials[index].vertexStart));
                    std::memcpy(indexCursor, source, bytes);
                    indexOffset += bytes;
                    indexCursor += materials[index].indexCount;
                    strip->Unlock();
                    strip->Release();
                }
                continue;
            }

            void* locked;
            lod.optimizedMesh->LockIndexBuffer(kLockNoSysLock, &locked);
            unsigned short* const source = static_cast<unsigned short*>(locked);
            unsigned int end = 0;
            unsigned int position = 0;
            unsigned int bytes = 0;
            for (unsigned int index = 0; index < lod.materialCount; ++index)
            {
                end += materials[index].indexCount;
                bytes += materials[index].indexCount * 2;
                for (; position < end; ++position)
                    source[position] = static_cast<unsigned short>(
                        source[position] + static_cast<unsigned short>(baseVertex
                                                                   - materials[index].vertexStart));
            }
            std::memcpy(indexCursor, source, bytes);
            indexOffset += bytes;
            indexCursor += end;
            lod.optimizedMesh->UnlockIndexBuffer();

            if (lod.alphaMode == 1)
            {
                // Shipped: the left-out material's index count is multiplied by m_u2, and only the
                // product is checked against indexTotal; a refusal returns with the new index array
                // neither published nor freed.
                const unsigned int tail = materials[lod.materialCount].indexCount * m_u2;
                if (tail > indexTotal)
                    return;
                // [CHOSEN, not recovered] memory safety: retail copies `tail` indices from
                // m_indexData + indexOffset / 2 to the cursor without checking either array's end;
                // a copy that would leave either array takes the same refusal instead.
                const unsigned int written = static_cast<unsigned int>(indexCursor - indices);
                if (tail > indexTotal - written || indexOffset / 2 > m_indexCount
                    || tail > m_indexCount - indexOffset / 2)
                    return;
                std::memcpy(indexCursor, m_indexData + indexOffset / 2, tail * 2);
                indexOffset += tail * 2;
                indexCursor += tail;
            }
        }
    }

    delete[] m_indexData;
    m_indexData = indices;
    m_indexCount = indexTotal;
}

template void MeshOptimizer::repackBuffers<BundledMeshOptimizerLod>(
    unsigned int, std::vector<std::vector<BundledMeshOptimizerLod> >&);
template void MeshOptimizer::repackBuffers<StaticMeshOptimizerLod>(
    unsigned int, std::vector<std::vector<StaticMeshOptimizerLod> >&);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
