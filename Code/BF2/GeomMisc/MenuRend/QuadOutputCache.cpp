// Captures menu geometry into reusable vertex buffers. Method names are [INFERRED].
#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../RendDX9/ShaderParameter.h"
#include "../Debug.h"
#include "IQuadDrawer.h"
#include <map>
#include <vector>
#include <new>

namespace dice { namespace hfe {
namespace dx9rend {
extern void* g_shaderManager;
void* shaderManagerGetEffect(void*, const char*);
void* bufferManagerCreateVertexBuffer(unsigned int, unsigned int, unsigned int, const char*);
void* bufferManagerCreateIndexBuffer(unsigned int, unsigned int, unsigned int);
void* vertexBufferLock(void*, unsigned int, unsigned int, unsigned int);
bool vertexBufferUnlock(void*);
void vertexBufferBindStream(void*, unsigned int);
void vertexBufferRelease(void*);
void* indexBufferLock(void*, unsigned int, unsigned int, unsigned int);
bool indexBufferUnlock(void*);
void indexBufferBind(void*);
void indexBufferRelease(void*);
void rendererDrawIndexedPrimitive(unsigned int, unsigned int, int, unsigned int,
                                  unsigned int, unsigned int);
}
namespace geom {
extern IBase* g_QuadDrawer;
void* memeTextureEngineTexture(const class MemeTexture*);

// [CHOSEN, not recovered] Named owner-TU access replaces the retail QuadDrawer offsets.
unsigned char& quadDrawerCaptureMode(void*);
unsigned int& quadDrawerCaptureVertexCount(void*);
void* quadDrawerCapturedTexture(void*);
void quadDrawerCopyCaptureVertex(void*, unsigned int, float*);

namespace {
void* g_quadCacheIndices = 0; // PE 0x1024A2D8

static void cacheError(int line, const char* message)
{
    ::renddx9abi::Debug(static_cast<DebugType>(6),
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\MenuRend\\QuadOutputCache.cpp",
        line, "") << message;
}
}

// The retail table at 0x1020B444 has exactly these twelve slots and no destructor.
// [CHOSEN, not recovered] Native STL storage keeps the original container operations.
class QuadOutputCache : public IBase
{
public:
    QuadOutputCache();
    void addRef() { ++m_refCount; } // PE 0x1000CB20, shared
    int getRef() { return m_refCount; } // PE 0x101A9A10, shared
    int release();
    void* queryInterface(unsigned int iid) const;
    virtual void initialize(unsigned int vertexCapacity, int bufferCount);
    virtual void beginCapture();
    virtual int endCapture();
    virtual void replace(int oldId, int newId);
    virtual void remove(int id);
    virtual void inspect(int id);
    virtual void clear();
    virtual void draw();

private:
    ~QuadOutputCache();
    int m_refCount;                       // PE +0x04
    std::map<int, int> m_captures;         // PE +0x08, id -> original vertex count
    std::vector<int> m_freeIds;           // PE +0x14
    // [UNRESOLVED] No element access; construction/destruction establish vector storage only.
    std::vector<unsigned int> m_unused;   // PE +0x24
    std::vector<void*> m_buffers;         // PE +0x34
    std::vector<void*> m_textures;        // PE +0x44, borrowed MemeTexture pointers
    unsigned int m_vertexCapacity;        // PE +0x54
    int m_bufferCount;                    // PE +0x58
    int m_captureField;                   // PE +0x5C, beginCapture writes zero
    void* m_effect;                       // PE +0x60
    dx9rend::ShaderParameter* m_textureParameter; // PE +0x64
};

// PE 0x10089B00
QuadOutputCache::QuadOutputCache() : m_refCount(1)
{
    // Remaining scalars and resource pointers stay unwritten until initialize.
    cacheError(25, "tl: Who's using the quad cache| Show yourself!!");
}

// PE 0x100899D0
QuadOutputCache::~QuadOutputCache()
{
    for (int i = 0; i < m_bufferCount; ++i)
        if (m_buffers[i])
            dx9rend::vertexBufferRelease(m_buffers[i]);
    if (g_quadCacheIndices)
    {
        dx9rend::indexBufferRelease(g_quadCacheIndices);
        g_quadCacheIndices = 0;
    }
    m_textureParameter->release();
    static_cast<IBase*>(m_effect)->release();
    // Automatic member destruction releases textures, buffers, unused storage,
    // free ids and then the capture map, in that order.
}

// PE 0x1008ABE0
int QuadOutputCache::release()
{
    const int count = --m_refCount;
    if (!count)
    {
        this->~QuadOutputCache();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// PE 0x1008A980
void* QuadOutputCache::queryInterface(unsigned int iid) const
{
    return iid == 1u || iid == 80030u ? const_cast<QuadOutputCache*>(this) : 0;
}

// PE 0x10089BD0
void QuadOutputCache::initialize(unsigned int vertexCapacity, int bufferCount)
{
    {
        const std::string name("Shaders/PortedMenuShader");
        m_effect = dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, name.c_str());
    }
    m_textureParameter = dx9rend::effectGetShaderParameterTyped(
        static_cast<dx9rend::Effect*>(m_effect), std::string("TEXLAYER0"));
    for (int i = 0; i < bufferCount; ++i)
    {
        m_freeIds.push_back(i);
        void* const buffer = dx9rend::bufferManagerCreateVertexBuffer(vertexCapacity, 44, 8, "QuadOutputCache");
        m_buffers.push_back(buffer);
        m_textures.push_back(0);
    }
    if (!g_quadCacheIndices)
    {
        g_quadCacheIndices = dx9rend::bufferManagerCreateIndexBuffer(0x5556, 8, 101);
        unsigned short* indices = static_cast<unsigned short*>(dx9rend::indexBufferLock(g_quadCacheIndices, 0, 0, 0));
        if (indices)
        {
            unsigned int vertex = 0;
            for (int i = 0; i < 0x5555; i += 6, indices += 6, vertex += 4)
            {
                indices[0] = static_cast<unsigned short>(vertex);
                indices[1] = static_cast<unsigned short>(vertex + 1);
                indices[2] = static_cast<unsigned short>(vertex + 2);
                indices[3] = static_cast<unsigned short>(vertex + 2);
                indices[4] = static_cast<unsigned short>(vertex + 3);
                indices[5] = static_cast<unsigned short>(vertex);
            }
        }
        dx9rend::indexBufferUnlock(g_quadCacheIndices);
    }
    m_bufferCount = bufferCount;
    m_vertexCapacity = vertexCapacity;
}

// PE 0x10089730
void QuadOutputCache::beginCapture()
{
    if (quadDrawerCaptureMode(g_QuadDrawer))
        cacheError(108, "beginCapture called twice");
    static_cast<IQuadDrawer*>(g_QuadDrawer)->flush();
    quadDrawerCaptureMode(g_QuadDrawer) = 1;
    m_captureField = 0;
}

// PE 0x10089F90
int QuadOutputCache::endCapture()
{
    if (!quadDrawerCaptureMode(g_QuadDrawer))
        cacheError(121, "endCapture");
    if (!quadDrawerCaptureVertexCount(g_QuadDrawer) || !quadDrawerCapturedTexture(g_QuadDrawer))
    {
        quadDrawerCaptureMode(g_QuadDrawer) = 0;
        return -1;
    }
    const int id = m_freeIds.back();
    m_textures[id] = quadDrawerCapturedTexture(g_QuadDrawer);
    if (!m_freeIds.empty())
        m_freeIds.pop_back();
    float* output = static_cast<float*>(dx9rend::vertexBufferLock(m_buffers[id], 0, 0, 0));
    if (output)
    {
        unsigned int source = 0;
        // shipped defect: the loop counter advances by four while source advances by six.
        for (unsigned int count = 0; count < quadDrawerCaptureVertexCount(g_QuadDrawer); count += 4, source += 6)
        {
            quadDrawerCopyCaptureVertex(g_QuadDrawer, source, output);
            quadDrawerCopyCaptureVertex(g_QuadDrawer, source + 1, output + 11);
            quadDrawerCopyCaptureVertex(g_QuadDrawer, source + 2, output + 22);
            quadDrawerCopyCaptureVertex(g_QuadDrawer, source + 4, output + 33);
            output += 44;
        }
    }
    dx9rend::vertexBufferUnlock(m_buffers[id]);
    unsigned int& count = quadDrawerCaptureVertexCount(g_QuadDrawer);
    int& destination = m_captures[id];
    destination = static_cast<int>(count);
    quadDrawerCaptureVertexCount(g_QuadDrawer) = 0;
    quadDrawerCaptureMode(g_QuadDrawer) = 0;
    return id;
}

// PE 0x10089E50
void QuadOutputCache::replace(int oldId, int newId)
{
    cacheError(182, "Don't call right now..");
    const auto found = m_captures.find(oldId);
    if (found != m_captures.end())
    {
        m_freeIds.push_back(found->first);
        m_captures.erase(found);
        inspect(newId);
    }
}

// PE 0x10089E00
void QuadOutputCache::remove(int id)
{
    const auto found = m_captures.find(id);
    if (found != m_captures.end())
    {
        m_freeIds.push_back(found->first);
        m_captures.erase(found);
    }
}

// PE 0x100897D0
void QuadOutputCache::inspect(int /*id*/)
{
    cacheError(198, "Don't call right now..");
    for (auto it = m_captures.begin(); it != m_captures.end(); ++it)
    {
        // The original only increments the iterator; it neither reads nor changes values.
    }
}

// PE 0x10089F10
void QuadOutputCache::clear()
{
    for (auto it = m_captures.begin(); it != m_captures.end(); )
    {
        m_freeIds.push_back(it->first);
        it = m_captures.erase(it);
    }
}

// PE 0x100898B0
void QuadOutputCache::draw()
{
    dx9rend::effectSetTechniqueByName(m_effect, std::string("QuadCache"));
    const int passes = static_cast<int>(dx9rend::effectBegin(m_effect));
    for (auto it = m_captures.begin(); it != m_captures.end(); ++it)
    {
        dx9rend::vertexBufferBindStream(m_buffers[it->first], 0);
        dx9rend::indexBufferBind(g_quadCacheIndices);
        m_textureParameter->setValue(memeTextureEngineTexture(
            static_cast<MemeTexture*>(m_textures[it->first])), 0);
        for (int pass = 0; pass < passes; ++pass)
        {
            dx9rend::effectBeginPass(m_effect, static_cast<unsigned int>(pass));
            dx9rend::rendererDrawIndexedPrimitive(static_cast<unsigned int>(it->second / 3),
                4, 0, 0, m_vertexCapacity, 0);
        }
    }
    dx9rend::effectEnd(m_effect);
}

// PE 0x1007C900
IBase* __fastcall createQuadOutputCache(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(QuadOutputCache));
    IBase* const object = storage ? new (storage) QuadOutputCache : 0;
    IBase* const result = static_cast<IBase*>(object->queryInterface(iid));
    if (!result)
        object->release();
    return result;
}

} } } // namespace dice::hfe::geom
#endif
