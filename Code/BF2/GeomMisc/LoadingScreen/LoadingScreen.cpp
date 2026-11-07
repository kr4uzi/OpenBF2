// Startup loading image and its full-screen quad.
#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ThunkAbi.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../RendDX9/Texture.h"
#include "../../RendDX9/ShaderParameter.h"
#include "../../RendMisc/IRenderer.h"
#include <new>

namespace dice { namespace hfe {
namespace dx9rend {
extern void* g_shaderManager;
extern void* g_rendererBase;
void* shaderManagerGetEffect(void*, const char*);
int bufferManagerSwapPool(int);
void* bufferManagerCreateVertexBuffer(unsigned int, unsigned int, unsigned int, const char*);
void* vertexBufferLock(void*, unsigned int, unsigned int, unsigned int);
bool vertexBufferUnlock(void*);
void vertexBufferRelease(void*);
void vertexBufferBindStream(void*, unsigned int);
void vertexDeclarationRelease(void*);
void vertexDeclarationBind(void*);
void textureManagerEvictFromCache(void*);
unsigned int effectBegin(void*);
void effectBeginPass(void*, unsigned int);
void effectEnd(void*);
void rendererDrawPrimitive(unsigned int, unsigned int, unsigned int);
}
namespace geom {

// The primary table at PE 0x1020D310 has eight slots; the listener has three.
// [INFERRED] Private operation names; the destructor's flags are an unsigned byte.
class LoadingScreen
{
public:
    LoadingScreen();
    void addRef() { ++m_refCount; } // PE 0x1001A010
    int getRef() const { return m_refCount; } // PE 0x10051870
    int release();
    void* queryInterface(unsigned int iid) const;
    bool initialize();
    void releaseAll();
    void draw();
    void* destroy(unsigned char flags);
private:
    const void* m_vptr;
    const void* m_listenerVptr;
    int m_refCount;
    dx9rend::EngineTexture* m_texture; // PE +0x0C
    void* m_buffer; // PE +0x10
    dx9rend::Effect* m_effect; // PE +0x14
    dx9rend::ShaderParameter *m_transform, *m_textureParameter; // PE +0x18/+0x1C
    void* m_declaration; // PE +0x20
};

namespace {
void RD9_THUNK loadingAddRef(void* p RD9_THUNK_EDX) { static_cast<LoadingScreen*>(p)->addRef(); }
int RD9_THUNK loadingGetRef(void* p RD9_THUNK_EDX) { return static_cast<LoadingScreen*>(p)->getRef(); }
int RD9_THUNK loadingRelease(void* p RD9_THUNK_EDX) { return static_cast<LoadingScreen*>(p)->release(); }
void* RD9_THUNK loadingQuery(void* p RD9_THUNK_EDX, unsigned int iid) { return static_cast<LoadingScreen*>(p)->queryInterface(iid); }
bool RD9_THUNK loadingInitialize(void* p RD9_THUNK_EDX) { return static_cast<LoadingScreen*>(p)->initialize(); }
void RD9_THUNK loadingReleaseAll(void* p RD9_THUNK_EDX) { static_cast<LoadingScreen*>(p)->releaseAll(); }
void RD9_THUNK loadingDraw(void* p RD9_THUNK_EDX) { static_cast<LoadingScreen*>(p)->draw(); }
void* RD9_THUNK loadingDestroy(void* p RD9_THUNK_EDX, unsigned char flags) { return static_cast<LoadingScreen*>(p)->destroy(flags); }
void* const kLoadingTable[8] = {
    reinterpret_cast<void*>(&loadingAddRef), reinterpret_cast<void*>(&loadingGetRef),
    reinterpret_cast<void*>(&loadingRelease), reinterpret_cast<void*>(&loadingQuery),
    reinterpret_cast<void*>(&loadingInitialize), reinterpret_cast<void*>(&loadingReleaseAll),
    reinterpret_cast<void*>(&loadingDraw), reinterpret_cast<void*>(&loadingDestroy)
};
// PE 0x100A1CB0, PE 0x100A1CC0, PE 0x100A1CD0: each body is RET.
void RD9_THUNK loadingLost(void* RD9_THUNK_EDX) {}
void RD9_THUNK loadingReset(void* RD9_THUNK_EDX) {}
void RD9_THUNK loadingListenerDestroy(void* RD9_THUNK_EDX) {}
void* const kLoadingListenerTable[3] = {
    reinterpret_cast<void*>(&loadingLost), reinterpret_cast<void*>(&loadingReset),
    reinterpret_cast<void*>(&loadingListenerDestroy)
};

class LoadingView
{
public:
    virtual void addRef() = 0;
    virtual int getRef() const = 0;
    virtual int release() = 0;
    virtual void* queryInterface(unsigned int) const = 0;
    virtual bool initialize() = 0;
    virtual void releaseAll() = 0;
    virtual void draw() = 0;
    virtual void* destroy(unsigned char) = 0;
};

struct LoadingVertex { float x, y, z; unsigned int color; float u, v; };
static_assert(sizeof(LoadingVertex) == 24, "loading-screen vertex stride");
}

// PE 0x100A1D90
LoadingScreen::LoadingScreen()
    : m_vptr(kLoadingTable), m_listenerVptr(kLoadingListenerTable), m_refCount(1),
      m_texture(0), m_buffer(0), m_effect(0), m_transform(0), m_textureParameter(0), m_declaration(0)
{
}

// PE 0x100A1F50
void* LoadingScreen::queryInterface(unsigned int iid) const
{
    return iid == 1 || iid == 80046 ? const_cast<LoadingScreen*>(this) : 0;
}

// PE 0x100A1F80
int LoadingScreen::release()
{
    const int count = --m_refCount;
    if (!count) reinterpret_cast<LoadingView*>(this)->destroy(1);
    return count;
}

// PE 0x100A1B20
bool LoadingScreen::initialize()
{
    using namespace dx9rend;
    m_effect = static_cast<Effect*>(shaderManagerGetEffect(g_shaderManager, "Shaders/LoadingScreen"));
    if (!m_effect) return false;
    m_transform = effectGetShaderParameterTyped(m_effect, std::string("TRANSFORM"));
    if (!m_transform) return false;
    m_textureParameter = effectGetShaderParameterTyped(m_effect, std::string("TEXTURE"));
    if (!m_textureParameter) return false;
    m_texture = static_cast<EngineTexture*>(textureManagerGetTexture(std::string("Menu/Startup/loadingscreen.dds")));
    if (!m_texture) return false;
    static const D3DVERTEXELEMENT9 declaration[] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    m_declaration = vertexDeclarationCreate(declaration);
    const int previousPool = bufferManagerSwapPool(1);
    m_buffer = bufferManagerCreateVertexBuffer(4, 24, 8, "VB");
    bufferManagerSwapPool(previousPool);
    LoadingVertex* const vertices = static_cast<LoadingVertex*>(vertexBufferLock(m_buffer, 0, 0, 0));
    if (!vertices) return false;
    vertices[0] = LoadingVertex{-1, -1, 0, 0xFFFFFFFFu, 0, 1};
    vertices[1] = LoadingVertex{1, -1, 0, 0xFFFFFFFFu, 1, 1};
    vertices[2] = LoadingVertex{1, 1, 0, 0xFFFFFFFFu, 1, 0};
    vertices[3] = LoadingVertex{-1, 1, 0, 0xFFFFFFFFu, 0, 0};
    vertexBufferUnlock(m_buffer);
    return true;
}

// PE 0x100A1CE0
void LoadingScreen::releaseAll()
{
    using namespace dx9rend;
    if (m_texture)
    {
        EngineTexture* released = m_texture;
        textureReleaseRef(released);
        textureManagerEvictFromCache(m_texture);
    }
    if (m_declaration) vertexDeclarationRelease(m_declaration);
    if (m_transform) m_transform->release();
    if (m_textureParameter) m_textureParameter->release();
    if (m_buffer) vertexBufferRelease(m_buffer);
    if (m_effect) effectRelease(m_effect);
    // Shipped defect: released members remain published, so repeated teardown releases them again.
}

// PE 0x100A1DF0
void LoadingScreen::draw()
{
    using namespace dx9rend;
    stateManagerPush();
    const unsigned int rgb[3] = {255, 0, 0};
    static_cast<IRenderer*>(g_rendererBase)->beginScene(rgb, 1); // Result deliberately discarded.
    const D3DMATRIX identity = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    static_cast<IDirect3DDevice9*>(renderDevice())->SetTransform(D3DTS_PROJECTION, &identity);
    static_cast<IDirect3DDevice9*>(renderDevice())->SetTransform(D3DTS_WORLD, &identity);
    static_cast<IDirect3DDevice9*>(renderDevice())->SetTransform(D3DTS_VIEW, &identity);
    m_transform->setValue(&identity, 0);
    m_textureParameter->setValue(m_texture, 0);
    vertexBufferBindStream(m_buffer, 0);
    vertexDeclarationBind(m_declaration);
    const unsigned int passes = effectBegin(m_effect);
    for (unsigned int pass = 0; pass < passes; ++pass)
    {
        effectBeginPass(m_effect, pass);
        rendererDrawPrimitive(2, 6, 0);
    }
    effectEnd(m_effect);
    static_cast<IRenderer*>(g_rendererBase)->endScene();
    static_cast<IRenderer*>(g_rendererBase)->present(2);
    stateManagerPop();
}

// PE 0x100A1FA0
void* LoadingScreen::destroy(unsigned char flags)
{
    void* const result = this;
    m_vptr = kLoadingTable;
    m_listenerVptr = kLoadingListenerTable;
    releaseAll();
    if (flags & 1) ::renddx9abi::engineFree(this);
    return result;
}

// PE 0x1007CFE0
IBase* __fastcall createLoadingScreen(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(LoadingScreen));
    LoadingScreen* const self = storage ? new (storage) LoadingScreen : 0;
    void* const result = reinterpret_cast<LoadingView*>(self)->queryInterface(iid);
    if (!result) reinterpret_cast<LoadingView*>(self)->release();
    return static_cast<IBase*>(result);
}

} } }
#endif
