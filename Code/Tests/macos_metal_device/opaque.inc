// [TEMP] [CHOSEN, not recovered] Retail fix "The back buffer is presented opaque": the alpha-only
// draw RendDX9Draw.cpp issues before every Present, with the same shader tokens and quad
// (RendDX9/OpaqueBackBufferAlpha.h) and the same states, into an A8R8G8B8 target that holds
// alpha-0 pixels. The A8R8G8B8 capture must come out alpha 255 with every RGB byte unchanged.
#include "../../BF2/RendDX9/OpaqueBackBufferAlpha.h"
static bool opaqueCapture(IDirect3DDevice9* d, IDirect3DSurface9* target, unsigned w, unsigned h,
                          unsigned& alphaMin, unsigned& alphaMax, bool& rgbKept, unsigned rgb) {
    IDirect3DSurface9* sys = nullptr; D3DLOCKED_RECT r = {};
    if (d->CreateOffscreenPlainSurface(w,h,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&sys,nullptr) != D3D_OK) return false;
    bool ok = d->GetRenderTargetData(target,sys) == D3D_OK && sys->LockRect(&r,nullptr,D3DLOCK_READONLY) == D3D_OK;
    alphaMin = 255; alphaMax = 0; rgbKept = true;
    if (ok) {
        for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) {
            unsigned v; std::memcpy(&v,static_cast<const unsigned char*>(r.pBits)+y*r.Pitch+x*4,4);
            const unsigned a = v >> 24; alphaMin = a < alphaMin ? a : alphaMin; alphaMax = a > alphaMax ? a : alphaMax;
            const unsigned expected = (x < w/2) ? rgb : (rgb ^ 0x00ffffffu);
            rgbKept = rgbKept && (v & 0x00ffffffu) == expected;
        }
        sys->UnlockRect();
    }
    sys->Release(); return ok;
}
static int opaqueAlphaChecks(IDirect3DDevice9* d) {
    using namespace dice::hfe::dx9rend;
    const unsigned w = 32, h = 16, rgb = 0x00336699u;
    IDirect3DSurface9 *target = nullptr, *oldTarget = nullptr;
    IDirect3DVertexShader9* vs = nullptr; IDirect3DPixelShader9* ps = nullptr; IDirect3DVertexDeclaration9* decl = nullptr;
    const D3DVERTEXELEMENT9 elements[2] = { {0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0}, D3DDECL_END() };
    CHECK(d->GetRenderTarget(0,&oldTarget) == D3D_OK);
    CHECK(d->CreateRenderTarget(w,h,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr) == D3D_OK);
    CHECK(d->CreateVertexShader(reinterpret_cast<const DWORD*>(opaque_alpha::kVertexShader),&vs) == D3D_OK);
    CHECK(d->CreatePixelShader(reinterpret_cast<const DWORD*>(opaque_alpha::kPixelShader),&ps) == D3D_OK);
    CHECK(d->CreateVertexDeclaration(elements,&decl) == D3D_OK);
    CHECK(d->SetRenderTarget(0,target) == D3D_OK);
    // The frame: alpha 0 everywhere (as on shadowed High terrain), two RGB halves.
    const RECT left = {0,0,LONG(w/2),LONG(h)}, right = {LONG(w/2),0,LONG(w),LONG(h)};
    CHECK(d->Clear(1,reinterpret_cast<const D3DRECT*>(&left),D3DCLEAR_TARGET,rgb,1.0f,0) == D3D_OK);
    CHECK(d->Clear(1,reinterpret_cast<const D3DRECT*>(&right),D3DCLEAR_TARGET,rgb ^ 0x00ffffffu,1.0f,0) == D3D_OK);
    unsigned alphaMin = 0, alphaMax = 0; bool rgbKept = false;
    CHECK(opaqueCapture(d,target,w,h,alphaMin,alphaMax,rgbKept,rgb) && alphaMax == 0 && rgbKept);   // control
    // The present step, as forceBackBufferAlphaOpaque sets it (a deliberately hostile prior state).
    CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(27),TRUE) == D3D_OK);   // D3DRS_ALPHABLENDENABLE
    CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(7),1) == D3D_OK);       // D3DRS_ZENABLE
    CHECK(d->BeginScene() == D3D_OK);
    for (unsigned i = 0; i < opaque_alpha::kRenderStateCount; ++i)
        CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(opaque_alpha::kRenderStates[i][0]),
                                opaque_alpha::kRenderStates[i][1]) == D3D_OK);
    CHECK(d->SetVertexShader(vs) == D3D_OK && d->SetPixelShader(ps) == D3D_OK && d->SetVertexDeclaration(decl) == D3D_OK);
    CHECK(d->SetStreamSource(0,nullptr,0,0) == D3D_OK);
    const D3DVIEWPORT9 full = {0,0,w,h,0.0f,1.0f};
    CHECK(d->SetViewport(&full) == D3D_OK);
    CHECK(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,opaque_alpha::kQuadPrimitives,opaque_alpha::kQuad,opaque_alpha::kQuadStride) == D3D_OK);
    CHECK(d->EndScene() == D3D_OK);
    CHECK(opaqueCapture(d,target,w,h,alphaMin,alphaMax,rgbKept,rgb) && alphaMin == 255 && rgbKept);
    d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(168),0xf); d->SetVertexShader(nullptr);   // all channels
    d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(22),3);                                  // D3DCULL_CCW
    d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(7),1);                                   // ZENABLE, device default
    d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(14),1);                                  // ZWRITEENABLE d->SetPixelShader(nullptr);
    d->SetVertexDeclaration(nullptr);
    CHECK(d->SetRenderTarget(0,oldTarget) == D3D_OK);
    oldTarget->Release(); decl->Release(); ps->Release(); vs->Release();
    CHECK(target->Release() == 0);
    std::puts("PASS: alpha-only present draw: alpha-0 A8R8G8B8 frame captures alpha 255, RGB unchanged");
    return 0;
}
