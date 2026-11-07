// Renderer-owned postprocessing state, TV submission and tinnitus frame history.
#ifndef BF2_RENDDX9_POSTPRODUCTIONFRAMEWORK_H
#define BF2_RENDDX9_POSTPRODUCTIONFRAMEWORK_H
#include <string>

namespace dice { namespace hfe { namespace dx9rend {
struct EngineTexture;

float postProductionGetTVInterference();
void postProductionSetTVInterference(float value);
float postProductionGetTVGranularity();
void postProductionSetTVGranularity(float value);
float postProductionGetTVAmbient();
void postProductionSetTVAmbient(float value);
void postProductionGetTVColor(float rgb[3]);
void postProductionSetTVColor(const float rgb[3]);
bool postProductionGetTVEnabled();
void postProductionSetTVEnabled(bool enabled);

void postProductionPrepare();
void postProductionPass();
void postProductionFinish();
// renderScene's post-production steps: the owner's glow byte (+0x24) from the flags and the glow
// pass after the sky (sub_1004EF80).
void postProductionSetGlowEnabled(unsigned char enabled);
void postProductionSceneGlowPass();

// The mode selector (dword_1023F3D8): create and destroy in DX9RendModule::init / close, the
// per-frame preset choice (sub_1004DF30) and the first-frame step after beginScene (sub_1004DF10).
void* postProductionModeSelectorCreate();
void postProductionModeSelectorDestroy(void* selector);
void postProductionModeSelectorUpdate(void* selector);
void postProductionModeSelectorFrameStart(void* selector);

// Renderer storage remains private to BF2Render.cpp. Selection does not change
// the retained scene target; setTarget updates both ownership and selection only
// when the requested texture differs from the retained one.
bool rendererPostMultisampleActive();
EngineTexture* rendererPostSceneTexture();
EngineTexture* rendererPostHistoryTexture(unsigned index);
double rendererPostTime();
void rendererPostSelectBackBuffer();
void rendererPostSetTarget(EngineTexture* texture);
void rendererPostResolveScene();

bool postProductionGetGlowEnabled();
void postProductionSetConsoleGlowEnabled(bool enabled);
float postProductionGetGlowStrength();
void postProductionSetGlowStrength(float value);
bool postProductionGetFakeHDREnabled();
void postProductionSetFakeHDREnabled(bool enabled);
float postProductionGetLightBlurStrength();
void postProductionSetLightBlurStrength(float value);
float postProductionGetTearGasStrength();
void postProductionSetTearGasStrength(float value);
void postProductionGetAmbientIlluminationRange(float range[4]);
void postProductionSetAmbientIlluminationRange(const float range[4]);
void postProductionSetNightVisionGradient(const std::string& name);

// The environment snapshot is shared by explicit night mode and the night-vision selector.
unsigned int rendererGetNightMode();
void rendererSetNightMode(unsigned int mode);
void rendererSetFakeHDRBlendingTimeLight(double value);
void rendererSetFakeHDRBlendingTimeDark(double value);
void rendererSetFakeHDRBlendingFactor(float value);
const std::string& rendererGetWatermarkFilename();
void rendererSetWatermarkFilename(const std::string& name);
void postProductionGetFogState(float state[4]);
void postProductionSetFogState(const float state[4]);
void postProductionGetWaterFogColorA255(float rgb[3]);
void postProductionSetWaterFogColorB255(const float rgb[3]);
void postProductionSetWaterFogColorA255(const float rgb[3]);
void postProductionGetWaterFogRangeA(float range[4]);
void postProductionSetWaterFogRangeB(const float range[4]);
void postProductionSetWaterFogRangeA(const float range[4]);
bool postProductionGetNightVisionEnabled();
void postProductionSetNightVisionEnabled(bool enabled);

} } }
#endif
