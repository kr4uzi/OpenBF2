// [TEMP] [CHOSEN, not recovered] An X8R8G8B8 colour target (the windowed back buffer) has no alpha:
// a draw whose pixel shader outputs alpha 0 must not store it (DeviceDraw.inc). Observable as
// D3D9 defines it: a later DESTALPHA blend reads 1.
namespace x8 {
// vs.1.1: dcl_position0 v0; mov oPos, v0 (the renderer's antialiasing-probe stream).
const unsigned kVertexShader[] = {0xFFFE0101u,0x0000001Fu,0x80000000u,0x900F0000u,0x00000001u,0xC00F0000u,0x90E40000u,0x0000FFFFu};
// ps.1.1: def c0, 1, 1, 1, <alpha>; mov r0, c0 (the alpha word is patched per shader).
const unsigned kPixelShader[] = {0xFFFF0101u,0x00000051u,0xA00F0000u,0x3F800000u,0x3F800000u,0x3F800000u,0x3F800000u,
                                 0x00000001u,0x800F0000u,0xA0E40000u,0x0000FFFFu};
const float kQuad[12] = {-2,2,0.5f, 2,2,0.5f, -2,-2,0.5f, 2,-2,0.5f};   // beyond the clip space
} // namespace x8
static int x8AlphaChecks(IDirect3DDevice9* d) {
    const unsigned w = 32, h = 16;
    const auto rs = [&](unsigned s, unsigned v) { return d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(s), v) == D3D_OK; };
    unsigned zeroAlphaTokens[sizeof(x8::kPixelShader)/sizeof(unsigned)];
    std::memcpy(zeroAlphaTokens,x8::kPixelShader,sizeof(zeroAlphaTokens));
    zeroAlphaTokens[6] = 0;                                                    // def c0, 1, 1, 1, 0
    IDirect3DVertexShader9* vs = nullptr; IDirect3DPixelShader9 *white = nullptr, *zeroAlpha = nullptr;
    IDirect3DVertexDeclaration9* decl = nullptr; IDirect3DSurface9 *old = nullptr, *xrgb = nullptr, *sys = nullptr;
    const D3DVERTEXELEMENT9 elements[2] = { {0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0}, D3DDECL_END() };
    CHECK(d->CreateVertexShader(reinterpret_cast<const DWORD*>(x8::kVertexShader),&vs) == D3D_OK);
    CHECK(d->CreatePixelShader(reinterpret_cast<const DWORD*>(x8::kPixelShader),&white) == D3D_OK);
    CHECK(d->CreatePixelShader(reinterpret_cast<const DWORD*>(zeroAlphaTokens),&zeroAlpha) == D3D_OK);
    CHECK(d->CreateVertexDeclaration(elements,&decl) == D3D_OK);
    CHECK(d->GetRenderTarget(0,&old) == D3D_OK);
    CHECK(d->CreateRenderTarget(w,h,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&xrgb,nullptr) == D3D_OK);
    CHECK(d->SetRenderTarget(0,xrgb) == D3D_OK);
    const D3DVIEWPORT9 full = {0,0,w,h,0.0f,1.0f};
    CHECK(d->SetViewport(&full) == D3D_OK && d->Clear(0,nullptr,D3DCLEAR_TARGET,0,1.0f,0) == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK);
    // A depth-writing draw is never skipped while its pipeline builds (DrawBridge.inc).
    CHECK(rs(7,1) && rs(23,8) && rs(14,1) && rs(22,1) && rs(27,FALSE) && rs(15,FALSE) && rs(28,FALSE));
    CHECK(d->SetVertexShader(vs) == D3D_OK && d->SetVertexDeclaration(decl) == D3D_OK && d->SetStreamSource(0,nullptr,0,0) == D3D_OK);
    CHECK(d->SetPixelShader(zeroAlpha) == D3D_OK);                             // RGB 1, alpha 0, all channels
    CHECK(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,x8::kQuad,12) == D3D_OK);
    CHECK(rs(27,TRUE) && rs(19,7) && rs(20,1));                                // SRCBLEND DESTALPHA, DESTBLEND ZERO
    CHECK(d->SetPixelShader(white) == D3D_OK);                                 // RGB 1 * dest alpha
    CHECK(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,x8::kQuad,12) == D3D_OK);
    CHECK(d->EndScene() == D3D_OK);
    D3DLOCKED_RECT lr = {};
    CHECK(d->CreateOffscreenPlainSurface(w,h,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&sys,nullptr) == D3D_OK);
    CHECK(d->GetRenderTargetData(xrgb,sys) == D3D_OK && sys->LockRect(&lr,nullptr,D3DLOCK_READONLY) == D3D_OK);
    bool allWhite = true;
    for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) {
        unsigned v; std::memcpy(&v,static_cast<const unsigned char*>(lr.pBits)+y*lr.Pitch+x*4,4);
        allWhite = allWhite && (v & 0x00ffffffu) == 0x00ffffffu;
    }
    CHECK(sys->UnlockRect() == D3D_OK);
    CHECK(allWhite);
    rs(27,FALSE); rs(19,2); rs(20,1); rs(22,3); rs(23,4);                       // device defaults
    d->SetVertexShader(nullptr); d->SetPixelShader(nullptr); d->SetVertexDeclaration(nullptr);
    CHECK(d->SetRenderTarget(0,old) == D3D_OK);
    sys->Release(); zeroAlpha->Release(); white->Release(); decl->Release(); vs->Release(); old->Release();
    CHECK(xrgb->Release() == 0);
    std::puts("PASS: an X8R8G8B8 target keeps no shader alpha (DESTALPHA reads 1)");
    return 0;
}
