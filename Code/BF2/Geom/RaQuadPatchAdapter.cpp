// Attested Geom/RaQuadPatchAdapter.cpp (retail path at PE 0x102220E0).
#ifdef BF2_IMAGE_RENDDX9
#include "RaQuadPatchAdapter.h"
#include "MeshTemplate.h"
#include "../RendDX9/Ra/RaShader.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/ThunkAbi.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <new>

#if defined(_MSC_VER)
// The UCRT exports the CRT assertion handler the shipped image calls but no longer declares it in
// <assert.h>.
extern "C" void __cdecl _assert(const char* expression, const char* file, unsigned line);
#endif

namespace dice { namespace hfe {
namespace dx9rend {
void vertexBufferBindStream(void*, unsigned int);
void indexBufferBind(void*);
void vertexDeclarationBind(void*);
}
namespace geom {
namespace Ra = dx9rend::Ra;
void materialRestoreBaseVtable(void*);
void geometryCopyLightingParameters(Ra::RaLightingParameters&, const Ra::RaLightingParameters&);

namespace {
const char kPath[] = "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\RaQuadPatchAdapter.cpp";
const char kHeader[] = "c:\\dice\\projects\\bf2branches\\patch_1_50\\code\\bf2\\geom\\RaQuadPatchAdapter.h";

void check(bool value, const char* expression, const char* file, int line)
{
    if (value)
        return;
#if defined(_MSC_VER)
    _assert(expression, file, static_cast<unsigned>(line));
#else
    // [CHOSEN, not recovered] The native port has no MSVCR71 assertion dialog.
    std::fprintf(stderr, "Assertion failed: %s, file %s, line %d\n", expression, file, line);
    std::fflush(stderr);
    std::abort();
#endif
}

// PE 0x101BC290. Assignment preserves each holder's binder table.
Ra::RaObjectParameters* assignObject(Ra::RaObjectParameters& d, const Ra::RaObjectParameters& s)
{
    d.registry = s.registry;
    std::memcpy(d.world.value, s.world.value, 64);
    std::memcpy(d.viewProjection.value, s.viewProjection.value, 64);
    std::memcpy(d.worldViewProjection.value, s.worldViewProjection.value, 64);
    std::memcpy(d.shadowProjMat.value, s.shadowProjMat.value, 64);
    std::memcpy(d.shadowOccProjMat.value, s.shadowOccProjMat.value, 64);
    std::memcpy(d.shadowTrapMat.value, s.shadowTrapMat.value, 64);
    d.geomBones.a = s.geomBones.a;
    d.geomBones.b = s.geomBones.b;
    d.transparency.value = s.transparency.value;
    std::memcpy(d.objectSpaceCamPos.value, s.objectSpaceCamPos.value, 16);
    d.windSpeed.value = s.windSpeed.value;
    std::memcpy(d.windDir.value, s.windDir.value, 12);
    Ra::raAssignParamBytes(d.userData.value, s.userData.value);
    d.heightOverTerrain.value = s.heightOverTerrain.value;
    std::memcpy(d.hemiMapSkyColor.value, s.hemiMapSkyColor.value, 16);
    std::memcpy(d.objectSpaceWorldUp.value, s.objectSpaceWorldUp.value, 12);
    d.tail220 = s.tail220;
    return &d;
}

// PE 0x101BC5B0. The source is a constructed adapter's object block.
Ra::RaObjectParameters* copyConstructObject(Ra::RaObjectParameters& d, const Ra::RaObjectParameters& s)
{
    d.vptr = s.vptr;
    d.registry = s.registry;
    d.world = s.world;
    d.viewProjection = s.viewProjection;
    d.worldViewProjection = s.worldViewProjection;
    d.shadowProjMat = s.shadowProjMat;
    d.shadowOccProjMat = s.shadowOccProjMat;
    d.shadowTrapMat = s.shadowTrapMat;
    d.geomBones = s.geomBones;
    d.transparency = s.transparency;
    d.objectSpaceCamPos = s.objectSpaceCamPos;
    d.windSpeed = s.windSpeed;
    d.windDir = s.windDir;
    d.userData.vptr = s.userData.vptr;
    new (&d.userData.value) std::vector<unsigned char>(s.userData.value);
    d.heightOverTerrain = s.heightOverTerrain;
    d.hemiMapSkyColor = s.hemiMapSkyColor;
    d.objectSpaceWorldUp = s.objectSpaceWorldUp;
    d.tail220 = s.tail220;
    return &d;
}

extern "C" int __cdecl _purecall(void);
void* RD9_THUNK adapterDestroy(void* self RD9_THUNK_EDX, unsigned char flags);

// PE 0x101BBB50; the shader argument is consumed but never read.
bool RD9_THUNK adapterBind(void* self RD9_THUNK_EDX, void*)
{
    RaQuadPatchAdapter& a = *static_cast<RaQuadPatchAdapter*>(self);
    check(a.compactVertices != 0, "m_pCompactVertexData", kPath, 79);
    check(a.indices != 0, "m_pLODIndices", kPath, 80);
    check(a.declaration != 0, "m_pNewDecl", kPath, 81);
    unsigned int stream = 0;
    if (a.vertices)
    {
        dx9rend::vertexBufferBindStream(a.vertices, stream);
        stream = 1;
    }
    dx9rend::vertexBufferBindStream(a.compactVertices, stream);
    dx9rend::indexBufferBind(a.indices);
    dx9rend::vertexDeclarationBind(a.declaration);
    a.bound = 1;
    return true;
}

// PE 0x10051870.
int RD9_THUNK adapterVertexCount(void* s RD9_THUNK_EDX) { return static_cast<RaQuadPatchAdapter*>(s)->vertexCount; }
// PE 0x101BC210.
int RD9_THUNK adapterIndexCount(void* s RD9_THUNK_EDX)
{ return static_cast<int>(static_cast<unsigned int>(static_cast<RaQuadPatchAdapter*>(s)->primitiveCount) * 3u); }
// PE 0x101A9A10.
int RD9_THUNK adapterPrimitiveCount(void* s RD9_THUNK_EDX) { return static_cast<RaQuadPatchAdapter*>(s)->primitiveCount; }
// PE 0x10051710.
int RD9_THUNK adapterPrimitiveType(void* s RD9_THUNK_EDX) { return static_cast<RaQuadPatchAdapter*>(s)->primitiveType; }
// PE 0x1010DE20.
int RD9_THUNK adapterVertexStart(void* s RD9_THUNK_EDX) { return static_cast<RaQuadPatchAdapter*>(s)->vertexStart; }
// PE 0x101C42F0.
int RD9_THUNK adapterIndexOffset(void* s RD9_THUNK_EDX) { return static_cast<RaQuadPatchAdapter*>(s)->indexOffset; }
// PE 0x10051D70.
int RD9_THUNK adapterIndexStart(void* s RD9_THUNK_EDX) { return static_cast<RaQuadPatchAdapter*>(s)->indexStart; }

// PE 0x10222130 and 0x10222154: only the stream-binding slot differs.
void* const kBaseTable[9] = {
    reinterpret_cast<void*>(&adapterDestroy), reinterpret_cast<void*>(&_purecall),
    reinterpret_cast<void*>(&adapterVertexCount), reinterpret_cast<void*>(&adapterIndexCount),
    reinterpret_cast<void*>(&adapterPrimitiveCount), reinterpret_cast<void*>(&adapterPrimitiveType),
    reinterpret_cast<void*>(&adapterVertexStart), reinterpret_cast<void*>(&adapterIndexOffset),
    reinterpret_cast<void*>(&adapterIndexStart)
};
void* const kAdapterTable[9] = {
    reinterpret_cast<void*>(&adapterDestroy), reinterpret_cast<void*>(&adapterBind),
    reinterpret_cast<void*>(&adapterVertexCount), reinterpret_cast<void*>(&adapterIndexCount),
    reinterpret_cast<void*>(&adapterPrimitiveCount), reinterpret_cast<void*>(&adapterPrimitiveType),
    reinterpret_cast<void*>(&adapterVertexStart), reinterpret_cast<void*>(&adapterIndexOffset),
    reinterpret_cast<void*>(&adapterIndexStart)
};

// PE 0x101BC260.
void* RD9_THUNK adapterDestroy(void* self RD9_THUNK_EDX, unsigned char flags)
{
    static_cast<RaQuadPatchAdapter*>(self)->~RaQuadPatchAdapter();
    if (flags & 1)
        ::renddx9abi::engineFree(self);
    return self;
}

struct Chunk
{
    RaQuadPatchAdapter adapters[64];
    int used;
    // PE 0x101BC4A0, 0x101BC220, 0x101BC9C0: the default constructor (64 adapters built forward, then
    // used = 0 at +0x9500) and the compiler-generated destructor (adapters destroyed in reverse) and
    // copy constructor (the 64 adapters copied through PE 0x101BC930, then used).
    Chunk() : used(0) {}
};

class Pool
{
public:
    // PE 0x101BCB50. The shipped reservation uses remainder, not division.
    explicit Pool(int reservation)
    {
        int count = reservation % 64;
        if (count < 1)
            count = 1;
        for (int i = 0; i != count; ++i)
            appendChunk();
        reset();
    }

    // PE 0x101BCAD0. Copies a fully constructed 64-adapter chunk before destroying the temporary.
    void appendChunk()
    {
        chunks.push_back(Chunk());
        vacant = chunks.begin();
        for (std::size_t i = 1; i < chunks.size(); ++i)
            ++vacant;
    }

    // PE 0x101BC1D0.
    void reset()
    {
        for (std::list<Chunk>::iterator i = chunks.begin(); i != chunks.end(); ++i)
            i->used = 0;
        vacant = chunks.begin();
        acquired = 0;
    }

    // PE 0x101BCBD0 and 0x101BCC30: the vacant-chunk walk and the slot step (used < 64 hands out
    // chunk + 0x254 * used).
    RaQuadPatchAdapter* acquire()
    {
        if (chunks.empty())
            appendChunk();
        check(vacant != chunks.end(), "m_VacantChunk != m_Chunks.end()", kHeader, 80);
        if (vacant->used >= 64)
        {
            ++vacant;
            if (vacant == chunks.end())
                appendChunk();
        }
        RaQuadPatchAdapter* result = 0;
        if (vacant->used < 64)
            result = &vacant->adapters[vacant->used++];
        ++acquired;
        check(result != 0, "t", kHeader, 59);
        return result;
    }

    // PE 0x101BC8B0. std::list destruction releases each chunk's array in reverse order.
    std::list<Chunk> chunks;
    std::list<Chunk>::iterator vacant;
    unsigned int acquired;
};

struct Settings
{
    RaQuadPatchQueue* target;            // +0x000
    RaQuadPatchAdapter adapter;          // +0x004
    RaMaterialParameters* material;      // +0x258
    Ra::RaObjectParameters* object;      // +0x25C
    Ra::RaLightingParameters lighting;   // +0x260
    Ra::RaLightRecord light;             // +0x370

    // PE 0x101BC410. The light record's +0x38 word stays untouched.
    Settings()
    {
        std::memset(&light, 0, 14 * sizeof(float));
        light.float3C = 1.0f;
        light.float40 = 1.0f;
    }
};

struct Server
{
    Pool pool;
    RaQuadPatchAdapter* last;
    Settings settings;
    Server() : pool(64), last(0) {}
};
Server* g_server = 0; // PE 0x1026743C

// PE 0x101BBDD0 and the identical lazy creation arms of raQuadPatchAdapterSetSettings and
// raQuadPatchAdapterSubmit.
Server* server()
{
    if (!g_server)
    {
        void* storage = ::renddx9abi::engineAlloc(sizeof(Server));
        g_server = storage ? new (storage) Server : 0;
    }
    return g_server;
}

// PE 0x101BC7C0.
void submit(Settings& settings, RaQuadPatchAdapter* adapter, int count, int vertices,
            int type, int firstVertex, int offset, int firstIndex)
{
    *adapter = settings.adapter;
    adapter->primitiveCount = count;
    adapter->vertexCount = vertices;
    adapter->primitiveType = type;
    adapter->vertexStart = firstVertex;
    adapter->indexOffset = offset;
    adapter->indexStart = firstIndex;
    adapter->bound = 0;
    check(settings.object != 0, "geometrySettings", kPath, 123);
    assignObject(adapter->object.get(), *settings.object);
    dx9rend::RenderQueueEntry* entry = dx9rend::renderEntryPoolAcquire();
    entry->material = adapter;
    entry->objectParameters = &adapter->object.get();
    entry->materialParameters = settings.material;
    geometryCopyLightingParameters(entry->lighting, settings.lighting);
    entry->shader = static_cast<Ra::RaShader*>(raMaterialShaderSlot(*settings.material));
    *static_cast<Ra::RaLightRecord*>(entry->lightArray.lights.value) = settings.light; // PE 0x100FF680
    check(settings.target != 0, "targetManager", kPath, 135);
    settings.target->add(entry);
}
} // anonymous namespace

// PE 0x101BBC60.
RaQuadPatchAdapter::RaQuadPatchAdapter()
    : vptr(kBaseTable), primitiveCount(0), vertexCount(0), primitiveType(4),
      vertexStart(0), indexOffset(0), indexStart(0), bound(0)
{
    Ra::raObjectParametersConstruct(object.bytes);
    vertices = compactVertices = indices = declaration = 0;
    vptr = kAdapterTable;
}

// PE 0x101BBCB0.
RaQuadPatchAdapter::RaQuadPatchAdapter(void* v, void* c, void* i, void* d)
    : vptr(kBaseTable), primitiveCount(0), vertexCount(0), primitiveType(4),
      vertexStart(0), indexOffset(0), indexStart(0), bound(0)
{
    Ra::raObjectParametersConstruct(object.bytes);
    vertices = v;
    vptr = kAdapterTable;
    compactVertices = c;
    indices = i;
    declaration = d;
    check(compactVertices != 0, "m_pCompactVertexData", kPath, 68);
    check(indices != 0, "m_pLODIndices", kPath, 69);
    check(declaration != 0, "m_pNewDecl", kPath, 70);
}

// PE 0x101BC930.
RaQuadPatchAdapter::RaQuadPatchAdapter(const RaQuadPatchAdapter& s)
    : vptr(kBaseTable), primitiveCount(s.primitiveCount), vertexCount(s.vertexCount),
      primitiveType(s.primitiveType), vertexStart(s.vertexStart), indexOffset(s.indexOffset),
      indexStart(s.indexStart), bound(s.bound)
{
    copyConstructObject(object.get(), s.object.get());
    vptr = kAdapterTable;
    vertices = s.vertices;
    compactVertices = s.compactVertices;
    indices = s.indices;
    declaration = s.declaration;
}

// PE 0x101BBC10 (also 0x101BBBF0, a second copy of the same body).
RaQuadPatchAdapter::~RaQuadPatchAdapter()
{
    vptr = kBaseTable;
    Ra::raObjectParametersDestroy(object.bytes);
    materialRestoreBaseVtable(this);
}

// PE 0x101BBB20: unreachable in 1.50: no code or data references. Stores the six draw-range words
// in field order (+0x04..+0x18) and clears the bound flag (+0x1C).
void RaQuadPatchAdapter::setDrawRange(int count, int vertices, int type, int firstVertex,
                                      int offset, int firstIndex)
{
    primitiveCount = count;
    vertexCount = vertices;
    primitiveType = type;
    vertexStart = firstVertex;
    indexOffset = offset;
    indexStart = firstIndex;
    bound = 0;
}

// PE 0x101BC530.
RaQuadPatchAdapter& RaQuadPatchAdapter::operator=(const RaQuadPatchAdapter& s)
{
    primitiveCount = s.primitiveCount;
    vertexCount = s.vertexCount;
    primitiveType = s.primitiveType;
    vertexStart = s.vertexStart;
    indexOffset = s.indexOffset;
    indexStart = s.indexStart;
    bound = s.bound;
    assignObject(object.get(), s.object.get());
    vertices = s.vertices;
    compactVertices = s.compactVertices;
    indices = s.indices;
    declaration = s.declaration;
    return *this;
}

// PE 0x101BBE20.
void raQuadPatchAdapterSetSettings(RaQuadPatchQueue* target, const RaQuadPatchAdapter& adapter,
                                  void* material, Ra::RaObjectParameters* object,
                                  const Ra::RaLightingParameters& lighting, const Ra::RaLightRecord& light)
{
    Settings& settings = server()->settings;
    settings.target = target;
    settings.adapter = adapter;
    settings.object = object;
    settings.material = static_cast<RaMaterialParameters*>(material);
    geometryCopyLightingParameters(settings.lighting, lighting);
    settings.light = light; // PE 0x100FF680
}

// PE 0x101BBEB0. A bound last adapter signals that the previous queued group was consumed.
void raQuadPatchAdapterSubmit(int count, int vertices, int type, int firstVertex, int offset, int firstIndex)
{
    Server* owner = server();
    if (owner->last && owner->last->bound)
        owner->pool.reset();
    RaQuadPatchAdapter* adapter = owner->pool.acquire();
    owner->last = adapter;
    Settings& settings = server()->settings;
    submit(settings, adapter, count, vertices, type, firstVertex, offset, firstIndex);
}

// [CHOSEN, not recovered] Source bridge for the assertion at PE 0x1018D9EA.
void raQuadPatchAdapterCheckWaterQueue(RaQuadPatchQueue* target)
{
    check(target != 0, "raShaderServer->m_specialManagers[RSMTWater]",
          "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\QuadPatch.cpp", 1761);
}

// PE 0x101BBD70. Settings lighting, prototype, pooled chunks, list storage, then the server.
void raQuadPatchAdapterServerDestroy()
{
    Server* const owner = g_server;
    if (owner)
    {
        owner->~Server();
        ::renddx9abi::engineFree(owner);
    }
    g_server = 0;
}
} } }
#endif
