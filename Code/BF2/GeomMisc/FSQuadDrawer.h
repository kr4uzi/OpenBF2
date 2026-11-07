// Full-screen quad and grid submission for the FSQuadDrawer effect: the hemi blit, the
// post-production entry points and the texture-size helpers below.

#ifndef BF2_GEOMMISC_FSQUADDRAWER_H
#define BF2_GEOMMISC_FSQUADDRAWER_H

namespace dice {
namespace hfe {

namespace dx9rend {
struct EngineTexture;
}

namespace Math {
struct Vec2;
}

namespace geom {

class FSQuadDrawer;

// dword_10249978 in RendDX9.dll.  ModuleGeomMisc::init publishes it and close destroys it.  Retail
// close deliberately leaves the pointer value behind after freeing the object; the implementation
// preserves that writeback quirk.
extern FSQuadDrawer* g_FSQuadDrawer;

// The allocation/construction and destruction/free halves of ModuleGeomMisc::init/close.
void initializeFSQuadDrawer();
void destroyFSQuadDrawer();

// PE HemiMapManager::makeHemiMap passes (17, source, destination, true, 0) to the private general
// FSQuadDrawer submission method.  This is that exact, deliberately narrow operation.
void fsQuadDrawerDrawHemiMap(dx9rend::EngineTexture* source,
                             dx9rend::EngineTexture* destination);

// The published drawer's drawViewportQuad(grid): the viewport-sized quad (grid 0: one quad,
// 1: 10x10), drawn inside the effect pass the caller began.
void fsQuadDrawerDrawCurrentEffect(unsigned int grid = 0);
// The published drawer's source pass with (0, source, true, true, 0, 0): pass 0 into the selected
// back buffer, state pushed.
void fsQuadDrawerBlitToBackBuffer(dx9rend::EngineTexture* source);
// The published drawer's drawTexturePass(9, source, destination, true, 1): pass 9 on the 10x10
// grid, as the post-production history capture at 0x10050F1A calls it.
void fsQuadDrawerCaptureTinnitus(dx9rend::EngineTexture* source,
                                 dx9rend::EngineTexture* destination);
// drawTexturePass(pass, source, destination, pushState, grid): `pass` from the source into the
// destination texture at the source's size; skipped when the effect has no such pass.
void fsQuadDrawerTexturePass(unsigned int pass, dx9rend::EngineTexture* source,
                             dx9rend::EngineTexture* destination, bool pushState,
                             unsigned int grid);
// sub_10085CF0(pass, source, selectBackBuffer, pushState, grid, customMatrix): `pass` at the
// source's size into the current render target, or the back buffer when selectBackBuffer.
void fsQuadDrawerSourcePass(unsigned int pass, dx9rend::EngineTexture* source,
                            bool selectBackBuffer, bool pushState, unsigned int grid,
                            const void* customMatrix);
// The drawer's 0x50-byte settings block (FSQuadDrawer +0x14): the nine horizontal and nine
// vertical glow weights, the high-pass gate and the blur strength.
struct FSQuadDrawerSettings
{
    float weights[18];
    float highPassGate;                   // HIGHPASSGATE
    float blurStrength;                   // BLURSTRENGTH
};
// A copy of the published drawer's settings (settings()) / setSettings (replace, then rebuild
// every cached glow-tap table).
FSQuadDrawerSettings fsQuadDrawerGetSettings();
void fsQuadDrawerSetSettings(const FSQuadDrawerSettings& settings);
// Replaces the blur strength of the drawer's settings block (and rebuilds its glow taps).
void fsQuadDrawerSetBlurStrength(float strength);

// sub_10084660: {1 / width, 1 / height} of a texture, from the low 16 bits of each dimension.
// Retail StaticMeshMaterial binds it as the detail-map texel size.
Math::Vec2 textureReciprocalSize(const dx9rend::EngineTexture* texture);
// sub_100846A0: {0.5 / width, 0.5 / height} of a texture (low 16 bits), the half-texel offset that
// retail Renderer::renderFrame (the accumulation-map projection), Terrain and RoadCompiledRenderer
// upload.
Math::Vec2 textureHalfTexelSize(const dx9rend::EngineTexture* texture);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOMMISC_FSQUADDRAWER_H
