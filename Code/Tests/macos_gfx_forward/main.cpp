// [CHOSEN, not recovered] Unit test of the G2w forwarding wrappers over fake "real" objects.
// The fake SDK and fake objects are generated test fixtures (gen_d3d9_forward.py --fixture-dir);
// nothing here touches a GPU, a DLL or the network.
#include "FakeObjects.h"
#include "../../BF2/Platform/Gfx/D3D9Forward/ForwardRuntime.h"
#include <cstdio>
#include <cstring>

using bf2fake::P;

extern "C" {
void* bf2fwd_adopt_IDirect3D9(void*);
void* bf2fwd_real_IDirect3D9(void*);
void* bf2fwd_real_IDirect3DDevice9(void*);
void* bf2fwd_real_IDirect3DTexture9(void*);
void* bf2fwd_real_IDirect3DSurface9(void*);
void* bf2fwd_real_ID3DXEffect(void*);
void* bf2fwd_real_ID3DXBuffer(void*);
void* bf2fwd_real_IDirect3DBaseTexture9(void*);
}

static int g_failed = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_failed; } } while (0)
#define CHECK_EQ(a, b) do { std::string x_ = (a), y_ = (b); if (x_ != y_) { std::printf("FAIL %s:%d\n  got      %s\n  expected %s\n", __FILE__, __LINE__, x_.c_str(), y_.c_str()); ++g_failed; } } while (0)

static std::string Last() { return bf2fake::Log().empty() ? std::string("<none>") : bf2fake::Log().back(); }
static bool StartsWith(const std::string& s, const std::string& p) { return s.compare(0, p.size(), p) == 0; }

// --- the stand-in for the real d3d9.dll / d3dx9_25.dll, bound through bf2fwd_GetProc -----------
static void* g_lastDeviceArg = nullptr;
static HRESULT WINAPI StubCreateBuffer(DWORD bytes, void** out) {
    bf2fake::Log().push_back("stub D3DXCreateBuffer(" + std::to_string(bytes) + ")");
    *out = new bf2fake::Fake_ID3DXBuffer();
    return S_OK;
}
static HRESULT WINAPI StubCreateEffect(void* dev, LPCVOID, UINT len, const D3DXMACRO*, ID3DXInclude* inc, DWORD flags,
                                       ID3DXEffectPool*, void** fx, void** errors) {
    g_lastDeviceArg = dev;
    bf2fake::Log().push_back("stub D3DXCreateEffect(len=" + std::to_string(len) + ",inc=" + P(inc) + ",flags=" + std::to_string(flags) + ")");
    *fx = new bf2fake::Fake_ID3DXEffect();
    if (errors) *errors = nullptr;
    return S_OK;
}
static void* WINAPI StubCreate9(UINT v) {
    bf2fake::Log().push_back("stub Direct3DCreate9(" + std::to_string(v) + ")");
    return new bf2fake::Fake_IDirect3D9();
}
static D3DXMATRIX* WINAPI StubMultiply(D3DXMATRIX* o, const D3DXMATRIX* a, const D3DXMATRIX* b) {
    bf2fake::Log().push_back("stub D3DXMatrixMultiply(" + P(o) + "," + P(a) + "," + P(b) + ")");
    return o;
}
extern "C" void* bf2fwd_GetProc(const char* module, const char* name) {
    std::string n = name;
    if (n == "Direct3DCreate9") { CHECK_EQ(module, "d3d9.dll"); return reinterpret_cast<void*>(&StubCreate9); }
    CHECK_EQ(module, "d3dx9_25.dll");
    if (n == "D3DXCreateBuffer") return reinterpret_cast<void*>(&StubCreateBuffer);
    if (n == "D3DXCreateEffect") return reinterpret_cast<void*>(&StubCreateEffect);
    if (n == "D3DXMatrixMultiply") return reinterpret_cast<void*>(&StubMultiply);
    std::printf("FAIL unexpected GetProc %s\n", name);
    std::abort();
}

// --- an engine-side state manager (subset interface) ---------------------------------------------
struct EngineStateManager final : ID3DXEffectStateManager {
    long refs = 1;
    std::string last;
    IDirect3DBaseTexture9* seenTexture = nullptr;
    void* seenReal = nullptr;   // the real object behind the wrapper, read during the call
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; }
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE, const D3DMATRIX*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL9*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetLight(DWORD, const D3DLIGHT9*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD, BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE s, DWORD v) override {
        last = "SetRenderState(" + std::to_string(s) + "," + std::to_string(v) + ")"; return 7; }
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD stage, IDirect3DBaseTexture9* t) override {
        last = "SetTexture(" + std::to_string(stage) + ")"; seenTexture = t;
        if (t) { seenReal = bf2fwd_real_IDirect3DBaseTexture9(t); (void)t->GetLevelCount(); }   // wrapper is live now
        return S_OK; }
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD s, D3DSAMPLERSTATETYPE t, DWORD v) override {
        last = "SetSamplerState(" + std::to_string(s) + "," + std::to_string(t) + "," + std::to_string(v) + ")"; return S_OK; }
    HRESULT STDMETHODCALLTYPE SetNPatchMode(FLOAT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetFVF(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT r, const FLOAT* d, UINT c) override {
        last = "VSF(" + std::to_string(r) + "," + P(d) + "," + std::to_string(c) + ")"; return S_OK; }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(UINT, const INT*, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT, const BOOL*, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT, const FLOAT*, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(UINT, const INT*, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT, const BOOL*, UINT) override { return S_OK; }
};

int main() {
    using namespace bf2fake;
    // ---- Direct3DCreate9 -> wrapper; plain calls, scalar returns ------------------------------------
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(d3d != nullptr);
    CHECK_EQ(Log()[0], "stub Direct3DCreate9(32)");
    auto* fakeD3d = static_cast<Fake_IDirect3D9*>(bf2fwd_real_IDirect3D9(d3d));
    CHECK(fakeD3d->refs() == 1);
    CHECK(d3d->GetAdapterCount() == 0);
    CHECK_EQ(Last(), "IDirect3D9::GetAdapterCount()");
    d3d->CheckDeviceFormat(1, D3DDEVTYPE_REF, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_DXT5);
    CHECK_EQ(Last(), "IDirect3D9::CheckDeviceFormat(1,2,22,1,3," + std::to_string(D3DFMT_DXT5) + ")");

    // ---- CreateDevice: struct pointer, handle, out interface wrapped ---------------------------------
    D3DPRESENT_PARAMETERS pp{};
    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, reinterpret_cast<HWND>(0x1234), 0x40, &pp, &dev);
    CHECK(hr == S_OK && dev != nullptr);
    CHECK(StartsWith(Last(), "IDirect3D9::CreateDevice(0,1,0x1234,64," + P(&pp) + ","));
    auto* fakeDev = static_cast<Fake_IDirect3DDevice9*>(bf2fwd_real_IDirect3DDevice9(dev));
    CHECK(fakeDev->refs() == 1);

    // ---- COM reference counts are the real counts ------------------------------------------------------
    CHECK(dev->AddRef() == 2 && fakeDev->refs() == 2);
    CHECK(dev->Release() == 1 && fakeDev->refs() == 1);

    // ---- value arguments, order, struct pointers ---------------------------------------------------------
    D3DVIEWPORT9 vp{1, 2, 640, 480, 0.f, 1.f};
    dev->SetViewport(&vp);
    CHECK_EQ(Last(), "IDirect3DDevice9::SetViewport(" + P(&vp) + ")");
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
    CHECK_EQ(Last(), "IDirect3DDevice9::SetRenderState(22,3)");
    dev->SetSamplerState(3, D3DSAMP_MIPMAPLODBIAS, 0xdeadbeefu);
    CHECK_EQ(Last(), "IDirect3DDevice9::SetSamplerState(3,8," + std::to_string(0xdeadbeefu) + ")");
    dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff102030u, 1.0f, 0);
    CHECK_EQ(Last(), "IDirect3DDevice9::Clear(0,0x0,3," + std::to_string(0xff102030u) + ",1,0)");
    dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, -5, 1, 2, 3, 4);
    CHECK_EQ(Last(), "IDirect3DDevice9::DrawIndexedPrimitive(5,-5,1,2,3,4)");
    dev->SetNPatchMode(0.5f);
    CHECK_EQ(Last(), "IDirect3DDevice9::SetNPatchMode(0.5)");

    // ---- textures and surfaces: one wrapper per real object -----------------------------------------
    IDirect3DTexture9* tex = nullptr;
    hr = dev->CreateTexture(256, 128, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    CHECK(hr == S_OK && tex);
    CHECK(StartsWith(Last(), "IDirect3DDevice9::CreateTexture(256,128,1,1,21,0,"));
    CHECK(tex->GetType() == D3DRTYPE_TEXTURE);
    auto* fakeTex = static_cast<Fake_IDirect3DTexture9*>(bf2fwd_real_IDirect3DTexture9(tex));
    IDirect3DSurface9 *s1 = nullptr, *s2 = nullptr;
    CHECK(tex->GetSurfaceLevel(0, &s1) == S_OK && tex->GetSurfaceLevel(0, &s2) == S_OK);
    CHECK(s1 && s1 == s2);
    auto* fakeSurf = static_cast<Fake_IDirect3DSurface9*>(bf2fwd_real_IDirect3DSurface9(s1));
    CHECK(fakeSurf->refs() == 3);                       // owner + two renderer references
    CHECK(s1->Release() == 2);
    CHECK(s2->Release() == 1);                          // wrapper gone, real object (owner's) lives on
    IDirect3DSurface9* s3 = nullptr;
    CHECK(tex->GetSurfaceLevel(0, &s3) == S_OK && s3 != nullptr);
    CHECK(bf2fwd_real_IDirect3DSurface9(s3) == fakeSurf);
    CHECK(fakeSurf->refs() == 2);

    // ---- QueryInterface wraps (same wrapper), unknown IID is refused and balanced -----------------
    void* q = nullptr;
    CHECK(s3->QueryInterface(Iid("IDirect3DSurface9"), &q) == S_OK && q == s3 && fakeSurf->refs() == 3);
    CHECK(static_cast<IUnknown*>(q)->Release() == 2);
    CHECK(s3->QueryInterface(Iid("IDirect3DTexture9"), &q) == E_NOINTERFACE && q == nullptr && fakeSurf->refs() == 2);
    void* c = reinterpret_cast<void*>(1);
    CHECK(s3->GetContainer(Iid("IDirect3DTexture9"), &c) == S_OK && c == nullptr);   // fake writes nothing

    // ---- wrapper arguments are unwrapped to the real objects ---------------------------------------------
    dev->SetTexture(2, tex);
    CHECK_EQ(Last(), "IDirect3DDevice9::SetTexture(2," + P(fakeTex) + ")");
    dev->SetTexture(2, nullptr);
    CHECK_EQ(Last(), "IDirect3DDevice9::SetTexture(2,0x0)");
    dev->SetRenderTarget(1, s3);
    CHECK_EQ(Last(), "IDirect3DDevice9::SetRenderTarget(1," + P(fakeSurf) + ")");
    dev->StretchRect(s3, nullptr, s3, nullptr, D3DTEXF_LINEAR);
    CHECK_EQ(Last(), "IDirect3DDevice9::StretchRect(" + P(fakeSurf) + ",0x0," + P(fakeSurf) + ",0x0,2)");

    // ---- abstract out-interface (BaseTexture): dispatch by resource type gives the existing wrapper --
    fakeTex->AddRef();
    fakeDev->children["GetTexture"] = fakeTex;          // the device "has" this texture bound
    IDirect3DBaseTexture9* bt = nullptr;
    CHECK(dev->GetTexture(0, &bt) == S_OK);
    CHECK(bt == static_cast<IDirect3DBaseTexture9*>(tex));
    CHECK(fakeTex->refs() == 3);                        // creation + children + GetTexture
    CHECK(bt->Release() == 2);

    // ---- D3DX: free functions, buffers, effects, include pass-through ------------------------------------
    ID3DXBuffer* buf = nullptr;
    CHECK(D3DXCreateBuffer(64, &buf) == S_OK && buf);
    CHECK(static_cast<Fake_ID3DXBuffer*>(bf2fwd_real_ID3DXBuffer(buf))->refs() == 1);
    buf->GetBufferSize();
    CHECK_EQ(Last(), "ID3DXBuffer::GetBufferSize()");
    CHECK(buf->Release() == 0);

    ID3DXEffect* fx = nullptr;
    ID3DXInclude* inc = reinterpret_cast<ID3DXInclude*>(0x40);
    CHECK(D3DXCreateEffect(dev, "x", 3, nullptr, inc, 9, nullptr, &fx, nullptr) == S_OK && fx);
    CHECK(g_lastDeviceArg == fakeDev);
    CHECK(Last() == "stub D3DXCreateEffect(len=3,inc=0x40,flags=9)");
    auto* fakeFx = static_cast<Fake_ID3DXEffect*>(bf2fwd_real_ID3DXEffect(fx));
    fx->SetTechnique("MyTech");
    CHECK_EQ(Last(), "ID3DXEffect::SetTechnique('MyTech')");
    IDirect3DDevice9* fxDev = nullptr;
    fakeDev->AddRef();
    fakeFx->children["GetDevice"] = fakeDev;
    CHECK(fx->GetDevice(&fxDev) == S_OK && fxDev == dev);
    fxDev->Release();

    // ---- effect state manager: real D3DX -> adapter -> engine's subset manager ------------------------------
    EngineStateManager mgr;
    CHECK(fx->SetStateManager(&mgr) == S_OK);
    CHECK(mgr.refs == 2);                               // the adapter holds one
    auto* adapter = static_cast<real::ID3DXEffectStateManager*>(LastPtrs().at(0));
    CHECK(adapter->SetRenderState(real::D3DRS_ZENABLE, 1) == 7 && mgr.last == "SetRenderState(7,1)");
    CHECK(adapter->SetSamplerState(4, real::D3DSAMP_ADDRESSU, 3) == S_OK && mgr.last == "SetSamplerState(4,1,3)");
    float consts[8] = {};
    adapter->SetVertexShaderConstantF(5, consts, 2);
    CHECK(mgr.last == "VSF(5," + P(consts) + ",2)");
    auto* fresh = new Fake_IDirect3DTexture9();         // an unwrapped real texture D3DX hands to the manager
    CHECK(adapter->SetTexture(6, fresh) == S_OK);
    CHECK(mgr.last == "SetTexture(6)" && mgr.seenTexture != nullptr);
    CHECK(mgr.seenReal == static_cast<void*>(fresh));
    CHECK(fresh->refs() == 1);                          // the temporary wrapper reference was released
    void* qa = nullptr;
    CHECK(adapter->QueryInterface(Iid("IUnknown"), &qa) == S_OK && qa == adapter);
    CHECK(adapter->Release() == 1);
    CHECK(adapter->QueryInterface(Iid("IDirect3DTexture9"), &qa) == E_NOINTERFACE);
    fresh->Release();
    CHECK(fx->SetStateManager(nullptr) == S_OK);
    CHECK_EQ(Last(), "ID3DXEffect::SetStateManager(0x0)");
    CHECK(adapter->Release() == 0);                     // D3DX lets go of the adapter
    CHECK(mgr.refs == 1);

    D3DXMATRIX a, b, o;
    CHECK(D3DXMatrixMultiply(&o, &a, &b) == &o);
    CHECK_EQ(Last(), "stub D3DXMatrixMultiply(" + P(&o) + "," + P(&a) + "," + P(&b) + ")");

    // ---- release everything: no real object and no wrapper may remain ---------------------------------------------
    for (auto& kv : fakeDev->children) kv.second->Release();   // the fake device's own hold on the texture
    fakeDev->children.clear();
    CHECK(fx->Release() == 0);                          // its children (the device) are released with it
    CHECK(s3->Release() == 1);                          // the texture still owns the surface
    CHECK(tex->Release() == 0);
    CHECK(dev->Release() == 0);
    CHECK(d3d->Release() == 0);
    std::printf("live fakes after teardown: %zu, wrappers: %zu\n", Alive().size(), bf2fwd::Registry::Get().map.size());
    CHECK(bf2fwd::Registry::Get().map.empty());
    if (g_failed) { std::printf("%d check(s) FAILED\n", g_failed); return 1; }
    std::printf("macos_gfx_forward: all checks passed\n");
    return 0;
}
