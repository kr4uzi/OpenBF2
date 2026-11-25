// [TEMP] [CHOSEN, not recovered] Multisampled surfaces (rend-msaa), run on a
// device whose back buffer is 4x: creation and refusals, clear + StretchRect resolve, the D3D9
// GetRenderTargetData refusal, a mismatched render target/depth pair, StretchRect into the
// multisampled back buffer, and the screenshot copy (SCRATCH surface, D3DXLoadSurfaceFromSurface,
// PNG and JPEG files; RendDX9 PE 0x10005200).
#include "../../BF2/Platform/Gfx/D3DX9Subset.h"
#include <string>
#include <unistd.h>
static unsigned msaaPixel(IDirect3DDevice9* d, IDirect3DSurface9* target, unsigned w, unsigned h) {
    IDirect3DSurface9* sys = nullptr; D3DLOCKED_RECT r = {}; unsigned v = 0;
    if (d->CreateOffscreenPlainSurface(w,h,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&sys,nullptr) != D3D_OK) return 0;
    if (d->GetRenderTargetData(target,sys) == D3D_OK && sys->LockRect(&r,nullptr,D3DLOCK_READONLY) == D3D_OK) {
        std::memcpy(&v,r.pBits,4); sys->UnlockRect();
    }
    sys->Release(); return v;
}
static int msaaSurfaceChecks(IDirect3DDevice9* d) {
    IDirect3DSurface9 *ms = nullptr, *msDepth = nullptr, *single = nullptr, *refused = nullptr, *back = nullptr, *depth = nullptr;
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONMASKABLE,0,FALSE,&refused,nullptr) == D3DERR_NOTAVAILABLE && !refused);
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_4_SAMPLES,1,FALSE,&refused,nullptr) == D3DERR_NOTAVAILABLE && !refused);
    CHECK(d->CreateDepthStencilSurface(8,8,D3DFMT_D24S8,D3DMULTISAMPLE_NONMASKABLE,0,FALSE,&refused,nullptr) == D3DERR_NOTAVAILABLE);
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_4_SAMPLES,0,FALSE,&ms,nullptr) == D3D_OK);
    CHECK(d->CreateDepthStencilSurface(8,8,D3DFMT_D24S8,D3DMULTISAMPLE_4_SAMPLES,0,FALSE,&msDepth,nullptr) == D3D_OK);
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&single,nullptr) == D3D_OK);
    D3DSURFACE_DESC desc = {};
    CHECK(ms->GetDesc(&desc) == D3D_OK && desc.MultiSampleType == D3DMULTISAMPLE_4_SAMPLES && desc.Usage == D3DUSAGE_RENDERTARGET);
    CHECK(msDepth->GetDesc(&desc) == D3D_OK && desc.MultiSampleType == D3DMULTISAMPLE_4_SAMPLES);
    D3DLOCKED_RECT locked = {};
    CHECK(ms->LockRect(&locked,nullptr,D3DLOCK_READONLY) == D3DERR_INVALIDCALL);
    CHECK(d->GetRenderTarget(0,&back) == D3D_OK && d->GetDepthStencilSurface(&depth) == D3D_OK);
    // A matched multisampled pair: clear, then StretchRect resolves into the single-sample target.
    CHECK(d->SetRenderTarget(0,ms) == D3D_OK && d->SetDepthStencilSurface(msDepth) == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xff336699u,1.0f,0) == D3D_OK &&
          d->EndScene() == D3D_OK);
    const RECT half = {0,0,4,8};
    CHECK(d->Clear(1,reinterpret_cast<const D3DRECT*>(&half),D3DCLEAR_TARGET,0xff112233u,1.0f,0) == D3D_OK); // a partial clear of samples
    CHECK(d->StretchRect(ms,nullptr,single,nullptr,D3DTEXF_NONE) == D3D_OK);
    IDirect3DSurface9* sys = nullptr;
    CHECK(d->CreateOffscreenPlainSurface(8,8,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&sys,nullptr) == D3D_OK);
    CHECK(d->GetRenderTargetData(ms,sys) == D3DERR_INVALIDCALL);       // D3D9: the render target is multisampled
    CHECK(d->GetRenderTargetData(back,sys) == D3DERR_INVALIDCALL);     // the 4x back buffer too
    CHECK(d->GetRenderTargetData(single,sys) == D3D_OK && sys->LockRect(&locked,nullptr,D3DLOCK_READONLY) == D3D_OK);
    const unsigned* row = static_cast<const unsigned*>(locked.pBits);
    CHECK(row[0] == 0xff112233u && row[7] == 0xff336699u);
    CHECK(sys->UnlockRect() == D3D_OK && sys->Release() == 0);
    // A mismatched pair (single-sample target, 4x depth) clears both buffers.
    CHECK(d->SetRenderTarget(0,single) == D3D_OK);
    CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff00ff00u,0.5f,0) == D3D_OK);
    CHECK(msaaPixel(d,single,8,8) == 0xff00ff00u);
    // StretchRect into a multisampled surface stops by name; into the 4x back buffer it lands in
    // every sample, so a later clear-free pass keeps it and the present shows it.
    CHECK(d->SetRenderTarget(0,back) == D3D_OK && d->SetDepthStencilSurface(depth) == D3D_OK);
    D3DSURFACE_DESC bd = {}; CHECK(back->GetDesc(&bd) == D3D_OK);
    IDirect3DSurface9* fill = nullptr;
    CHECK(d->CreateRenderTarget(bd.Width,bd.Height,bd.Format,D3DMULTISAMPLE_NONE,0,FALSE,&fill,nullptr) == D3D_OK);
    CHECK(d->SetRenderTarget(0,fill) == D3D_OK && d->SetDepthStencilSurface(nullptr) == D3D_OK &&
          d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff804020u,1.0f,0) == D3D_OK);
    CHECK(d->SetRenderTarget(0,back) == D3D_OK && d->SetDepthStencilSurface(depth) == D3D_OK);
    CHECK(d->StretchRect(fill,nullptr,back,nullptr,D3DTEXF_NONE) == D3D_OK);
    CHECK(fill->Release() == 0);
    // A partial colour clear loads the samples and resolves them: the copy must be in the samples.
    const RECT corner = {0,0,1,1};
    CHECK(d->Clear(1,reinterpret_cast<const D3DRECT*>(&corner),D3DCLEAR_TARGET,0xff010203u,1.0f,0) == D3D_OK);
    // Terrain::drawMain's copy under multisampling: the X8R8G8B8 back buffer into an A8R8G8B8 target.
    if (bd.Format == D3DFMT_X8R8G8B8) {
        IDirect3DSurface9* argb = nullptr;
        CHECK(d->CreateRenderTarget(bd.Width,bd.Height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&argb,nullptr) == D3D_OK);
        CHECK(d->StretchRect(back,nullptr,argb,nullptr,D3DTEXF_NONE) == D3D_OK);
        CHECK(msaaPixel(d,argb,bd.Width,bd.Height) == 0xff010203u);
        CHECK(argb->Release() == 0);
    }
    // The screenshot copy: SCRATCH surface, back buffer load, PNG and JPEG files.
    IDirect3DSurface9 *capture = nullptr, *chainBack = nullptr; IDirect3DSwapChain9* chain = nullptr;
    CHECK(d->CreateOffscreenPlainSurface(bd.Width,bd.Height,D3DFMT_A8R8G8B8,D3DPOOL_SCRATCH,&capture,nullptr) == D3D_OK);
    CHECK(capture->GetDesc(&desc) == D3D_OK && desc.Pool == D3DPOOL_SCRATCH);
    CHECK(d->StretchRect(back,nullptr,capture,nullptr,D3DTEXF_NONE) == D3DERR_INVALIDCALL); // SCRATCH: no device copies
    CHECK(d->GetSwapChain(0,&chain) == D3D_OK && chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&chainBack) == D3D_OK);
    CHECK(D3DXLoadSurfaceFromSurface(capture,nullptr,nullptr,chainBack,nullptr,nullptr,D3DX_FILTER_NONE,0) == D3D_OK);
    CHECK(capture->LockRect(&locked,nullptr,D3DLOCK_READONLY) == D3D_OK);
    const unsigned* pixels = static_cast<const unsigned*>(locked.pBits);
    CHECK(pixels[0] == 0xff010203u && pixels[bd.Width*(bd.Height-1)+bd.Width-1] == 0xff804020u);
    CHECK(capture->UnlockRect() == D3D_OK);
    char temp[] = "/private/tmp/openbf2-msaa-XXXXXX"; CHECK(mkdtemp(temp));
    const std::string png = std::string(temp)+"\\screen000.png", jpg = std::string(temp)+"/screen000.jpg";
    CHECK(D3DXSaveSurfaceToFileA(png.c_str(),D3DXIFF_PNG,capture,nullptr,nullptr) == D3D_OK); // backslash separator
    CHECK(D3DXSaveSurfaceToFileA(jpg.c_str(),D3DXIFF_JPG,capture,nullptr,nullptr) == D3D_OK);
    D3DXIMAGE_INFO info = {};
    const std::string pngPath = std::string(temp)+"/screen000.png";
    CHECK(D3DXGetImageInfoFromFileA(pngPath.c_str(),&info) == D3D_OK && info.ImageFileFormat == D3DXIFF_PNG &&
          info.Width == bd.Width && info.Height == bd.Height);
    CHECK(D3DXGetImageInfoFromFileA(jpg.c_str(),&info) == D3D_OK && info.ImageFileFormat == D3DXIFF_JPG && info.Width == bd.Width);
    unlink(pngPath.c_str()); unlink(jpg.c_str()); rmdir(temp);
    CHECK(capture->Release() == 0);
    chainBack->Release(); chain->Release();
    single->Release(); msDepth->Release(); ms->Release(); back->Release(); depth->Release();
    std::puts("PASS: 4x render target/depth surfaces, refusals, StretchRect resolve, mismatched clear, back-buffer copy, screenshot PNG/JPEG");
    return 0;
}
// The overlay bracket of Renderer::renderFrame under multisampling (RendDX9 PE 0x100306D2 and
// 0x10030AE7): GetDepthStencilSurface, SetDepthStencilSurface(0), the overlay passes, then the
// saved surface back. A reference left on the implicit depth-stencil surface keeps the device
// referenced (the final release at shutdown fails, RendDX9Init.cpp:738) and refuses Reset; the
// bracket must leave the device count where it was.
static int msaaOverlayDepthChecks(IDirect3DDevice9* d, D3DPRESENT_PARAMETERS& pp) {
    const ULONG before = (d->AddRef(), d->Release());
    IDirect3DSurface9* saved = nullptr;
    for (unsigned frame = 0; frame < 3; ++frame) {
        saved = nullptr;
        CHECK(d->BeginScene() == D3D_OK && d->GetDepthStencilSurface(&saved) == D3D_OK && saved);
        CHECK(d->SetDepthStencilSurface(nullptr) == D3D_OK);
        CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff203040u,1.0f,0) == D3D_OK);
        CHECK(d->SetDepthStencilSurface(saved) == D3D_OK);
        CHECK(saved->Release() == 0);
        CHECK(d->EndScene() == D3D_OK && d->Present(nullptr,nullptr,nullptr,nullptr) == D3D_OK);
    }
    CHECK((d->AddRef(), d->Release()) == before);
    // One frame without the release: the device stays referenced and Reset is refused.
    CHECK(d->GetDepthStencilSurface(&saved) == D3D_OK && (d->AddRef(), d->Release()) == before + 1);
    CHECK(d->Reset(&pp) == D3DERR_INVALIDCALL);
    CHECK(saved->Release() == 0 && (d->AddRef(), d->Release()) == before);
    CHECK(d->Reset(&pp) == D3D_OK);
    std::puts("PASS: multisampled overlay depth-stencil bracket leaves the device count unchanged; a leaked depth reference refuses Reset");
    return 0;
}
// D3DRS_MULTISAMPLEANTIALIAS FALSE on the multisampled back buffer (RendDX9 PE 0x100306D2: the HUD,
// name tags, debug text and console run with it). A glyph quad as Font::Ported draws it: D3D9
// screen coordinates on whole pixels (edges through pixel centres), u biased half a texel, linear
// filtering, an atlas whose cells touch (Fonts/BF1942.font has no gutter). D3D9 writes every sample
// with the pixel centre's coverage and value: exactly the cell's four texels, nothing beside them.
// With multisampling on, the edge pixels are half covered and the right one shows the next cell.
static int msaaAliasedGlyphChecks(IDirect3DDevice9* d) {
    IDirect3DSurface9 *back = nullptr, *single = nullptr, *sys = nullptr;
    IDirect3DTexture9* atlas = nullptr;
    CHECK(d->GetRenderTarget(0,&back) == D3D_OK);
    D3DSURFACE_DESC bd = {}; CHECK(back->GetDesc(&bd) == D3D_OK && bd.MultiSampleType != D3DMULTISAMPLE_NONE);
    CHECK(d->CreateRenderTarget(bd.Width,bd.Height,bd.Format,D3DMULTISAMPLE_NONE,0,FALSE,&single,nullptr) == D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(bd.Width,bd.Height,bd.Format,D3DPOOL_SYSTEMMEM,&sys,nullptr) == D3D_OK);
    CHECK(d->CreateTexture(8,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&atlas,nullptr) == D3D_OK);
    { D3DLOCKED_RECT lock = {}; CHECK(atlas->LockRect(0,&lock,nullptr,0) == D3D_OK);
      unsigned* p = static_cast<unsigned*>(lock.pBits);
      for (int i = 0; i < 8; ++i) p[i] = i < 4 ? 0xffff0000u : 0xffffffffu;   // cell A red, cell B white
      CHECK(atlas->UnlockRect(0) == D3D_OK); }
    struct V { float x,y,z,rhw,u,v; };
    const float u0 = 0.5f / 8.0f, u1 = u0 + 4.0f / 8.0f;
    const V quad[4] = {{2,2,0.5f,1,u0,0.5f},{6,2,0.5f,1,u1,0.5f},{2,6,0.5f,1,u0,0.5f},{6,6,0.5f,1,u1,0.5f}};
    CHECK(d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK);
    CHECK(d->SetRenderState(D3DRS_ZENABLE,0) == D3D_OK && d->SetRenderState(D3DRS_CULLMODE,1) == D3D_OK);
    CHECK(d->SetTexture(0,atlas) == D3D_OK);
    const DWORD stage[4][2] = {{1,2},{2,2},{4,2},{5,2}};   // COLOROP/ALPHAOP SELECTARG1, ARG1 TEXTURE
    for (const auto& st : stage) CHECK(d->SetTextureStageState(0,static_cast<D3DTEXTURESTAGESTATETYPE>(st[0]),st[1]) == D3D_OK);
    CHECK(d->SetSamplerState(0,D3DSAMP_ADDRESSU,3) == D3D_OK && d->SetSamplerState(0,D3DSAMP_ADDRESSV,3) == D3D_OK);
    CHECK(d->SetSamplerState(0,D3DSAMP_MAGFILTER,2) == D3D_OK && d->SetSamplerState(0,D3DSAMP_MINFILTER,2) == D3D_OK &&
          d->SetSamplerState(0,D3DSAMP_MIPFILTER,0) == D3D_OK);
    CHECK(d->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1) == D3D_OK);
    unsigned row[2][8] = {};
    for (int pass = 0; pass < 2; ++pass) {
        CHECK(d->SetRenderTarget(0,back) == D3D_OK);
        CHECK(d->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,pass ? FALSE : TRUE) == D3D_OK);
        CHECK(d->BeginScene() == D3D_OK && d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff0000ffu,1.0f,0) == D3D_OK);
        CHECK(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(V)) == D3D_OK && d->EndScene() == D3D_OK);
        CHECK(d->StretchRect(back,nullptr,single,nullptr,D3DTEXF_NONE) == D3D_OK);
        CHECK(d->GetRenderTargetData(single,sys) == D3D_OK);
        D3DLOCKED_RECT locked = {}; CHECK(sys->LockRect(&locked,nullptr,D3DLOCK_READONLY) == D3D_OK);
        const unsigned* line = reinterpret_cast<const unsigned*>(static_cast<const unsigned char*>(locked.pBits) + locked.Pitch * 3);
        for (int x = 0; x < 8; ++x) row[pass][x] = line[x] & 0xffffffu;
        CHECK(sys->UnlockRect() == D3D_OK);
    }
    std::printf("glyph row, multisampling on:  %06x %06x | %06x %06x %06x %06x | %06x %06x\n",
                row[0][0],row[0][1],row[0][2],row[0][3],row[0][4],row[0][5],row[0][6],row[0][7]);
    std::printf("glyph row, MULTISAMPLEANTIALIAS FALSE: %06x %06x | %06x %06x %06x %06x | %06x %06x\n",
                row[1][0],row[1][1],row[1][2],row[1][3],row[1][4],row[1][5],row[1][6],row[1][7]);
    // FALSE: the cell's texels exactly, the background beside them.
    CHECK(row[1][1] == 0x0000ffu && row[1][6] == 0x0000ffu);
    for (int x = 2; x < 6; ++x) CHECK(row[1][x] == 0xff0000u);
    // On: the right edge pixel is half covered with the next cell's white (the console's stray marks).
    CHECK(row[0][6] != 0x0000ffu && ((row[0][6] >> 16) & 0xff) > 0x40 && ((row[0][6] >> 16) & 0xff) < 0xc0);
    CHECK(d->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,TRUE) == D3D_OK && d->SetTexture(0,nullptr) == D3D_OK);
    atlas->Release(); sys->Release(); single->Release(); back->Release();
    std::puts("PASS: MULTISAMPLEANTIALIAS FALSE draws a whole-pixel glyph quad on the 4x back buffer without edge coverage or bleed");
    return 0;
}
