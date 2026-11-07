// Path:     [INFERRED] companion to the attested Geom/ModuleGeom.cpp.
// Contents: LightManager console publication recovered from RendDX9.dll.

// The shipped TU installs SetConsoleClassDefaults("lightManager", 2, 0), then constructs its
// generated bindings in the order retained below.  ModuleGeom::init registers this TU's list with
// the host console registry; ModuleGeom::close unregisters it.  Publication is native-only because
// ConsoleObject and the registry both carry host-toolchain std::string/container ABI.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"


#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "XpLightManager.h"

namespace dice {
namespace hfe {
namespace geom {

extern LightManager* g_LightManager;

namespace {

const char kLightManagerConsoleTag[] = "geom.lightManager";
const char kShadowManagerConsoleTag[] = "geom.shadowManager";

inline LightManager* lightManager()
{
    return g_LightManager;
}

void consoleInitializeLightManager()
{
    initializeLightManager(lightManager());
}

BF2_CONSOLE_OBJECT("lightManager", 2, 0)

// PE constructor 0x100BC86C / executor 0x100BC8C1.  The executor calls the direct initializer,
// not a virtual slot, and is inactive while the LightManager singleton is absent.
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(init, consoleInitializeLightManager,
                                   lightManager() != 0, kLightManagerConsoleTag)

// PE 0x100BC8DC, 0x100BC961, 0x100BC9D4: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(skyColor, Vec3, lightManager(),
                     setSkyColor, getSkyColor, kLightManagerConsoleTag)
// PE 0x100BCA44, 0x100BCAC9, 0x100BCB3C: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(ambientColor, Vec3, lightManager(),
                     setAmbientColor, getAmbientColor, kLightManagerConsoleTag)
// PE 0x100BCBAC, 0x100BCC31, 0x100BCCA4: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(treeAmbientColor, Vec3, lightManager(),
                     setTreeAmbientColor, getTreeAmbientColor, kLightManagerConsoleTag)
// PE 0x100BCD14, 0x100BCD9E, 0x100BCE11: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(treeSunColor, Vec3, lightManager(),
                     setTreeSunColor, getTreeSunColor, kLightManagerConsoleTag)
// PE 0x100BCE87, 0x100BCF0C, 0x100BCF7F: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(treeSkyColor, Vec3, lightManager(),
                     setTreeSkyColor, getTreeSkyColor, kLightManagerConsoleTag)
// PE 0x100BCFF5, 0x100BD07A, 0x100BD0ED: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(staticSunColor, Vec3, lightManager(),
                     setStaticSunColor, getStaticSunColor, kLightManagerConsoleTag)
// PE 0x100BD163, 0x100BD1E8, 0x100BD25B: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(staticSkyColor, Vec3, lightManager(),
                     setStaticSkyColor, getStaticSkyColor, kLightManagerConsoleTag)
// PE 0x100BD2D1, 0x100BD359, 0x100BD3CC: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(staticSpecularColor, Vec3, lightManager(),
                     setStaticSpecularColor, getStaticSpecularColor, kLightManagerConsoleTag)
// PE 0x100BD442, 0x100BD4C7, 0x100BD53A: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(singlePointColor, Vec3, lightManager(),
                     setSinglePointColor, getSinglePointColor, kLightManagerConsoleTag)
// PE 0x100BD5B0, 0x100BD68D, 0x100BD700: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(dynamicPointColor, Vec3, lightManager(),
                     setDynamicPointColor, getDynamicPointColor, kLightManagerConsoleTag)
// PE 0x100BD776, 0x100BD7FB, 0x100BD86E: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(effectSunColor, Vec3, lightManager(),
                     setEffectSunColor, getEffectSunColor, kLightManagerConsoleTag)
// PE 0x100BD8E4, 0x100BD969, 0x100BD9DC: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(effectShadowColor, Vec3, lightManager(),
                     setEffectShadowColor, getEffectShadowColor, kLightManagerConsoleTag)
// PE 0x100BDA52, 0x100BDAC3: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(defaultEffectLightAffectionFactor, float, lightManager(),
                     setDefaultEffectLightAffectionFactor,
                     getDefaultEffectLightAffectionFactor, kLightManagerConsoleTag)

// PE 0x100BDC57, +0x1FC/+0x200.
// PE 0x100BDB5F, 0x100BDBE4: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(singlePointColorHigh, Vec3, lightManager(),
                     setSinglePointColorHigh, getSinglePointColorHigh, kLightManagerConsoleTag)
// PE 0x100BDDC5, +0x204/+0x208.
// PE 0x100BDCCD, 0x100BDD52: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(singlePointColorLow, Vec3, lightManager(),
                     setSinglePointColorLow, getSinglePointColorLow, kLightManagerConsoleTag)
// PE 0x100BDF33, +0x20C/+0x210.
// PE 0x100BDE3B, 0x100BDEC0: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(dynamicPointColorHigh, Vec3, lightManager(),
                     setDynamicPointColorHigh, getDynamicPointColorHigh, kLightManagerConsoleTag)
// PE 0x100BE0A1, +0x214/+0x218.
// PE 0x100BDFA9, 0x100BE02E: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(dynamicPointColorLow, Vec3, lightManager(),
                     setDynamicPointColorLow, getDynamicPointColorLow, kLightManagerConsoleTag)
// PE 0x100BE20F, +0x21C/+0x220.
// PE 0x100BE117, 0x100BE19C: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(skyColorHigh, Vec3, lightManager(),
                     setSkyColorHigh, getSkyColorHigh, kLightManagerConsoleTag)
// PE 0x100BE37D, +0x224/+0x228.
// PE 0x100BE285, 0x100BE30A: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(skyColorLow, Vec3, lightManager(),
                     setSkyColorLow, getSkyColorLow, kLightManagerConsoleTag)
// PE 0x100BE4EB, +0x22C/+0x230.
// PE 0x100BE3F3, 0x100BE478: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(staticSkyColorHigh, Vec3, lightManager(),
                     setStaticSkyColorHigh, getStaticSkyColorHigh, kLightManagerConsoleTag)
// PE 0x100BE659, +0x234/+0x238.
// PE 0x100BE561, 0x100BE5E6: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(staticSkyColorLow, Vec3, lightManager(),
                     setStaticSkyColorLow, getStaticSkyColorLow, kLightManagerConsoleTag)
// PE 0x100BE7C7, +0x244/+0x248. Retail publishes low before high for the sun pairs.
// PE 0x100BE6CF, 0x100BE754: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(sunColorLow, Vec3, lightManager(),
                     setSunColorLow, getSunColorLow, kLightManagerConsoleTag)
// PE 0x100BE935, +0x23C/+0x240.
// PE 0x100BE83D, 0x100BE8C2: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(sunColorHigh, Vec3, lightManager(),
                     setSunColorHigh, getSunColorHigh, kLightManagerConsoleTag)

// PE 0x100BEAA3, +0x254/+0x258.
// PE 0x100BE9AB, 0x100BEA30: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(sunSpecColorLow, Vec3, lightManager(),
                     setSunSpecColorLow, getSunSpecColorLow, kLightManagerConsoleTag)
// PE 0x100BEC11, +0x24C/+0x250.
// PE 0x100BEB19, 0x100BEB9E: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(sunSpecColorHigh, Vec3, lightManager(),
                     setSunSpecColorHigh, getSunSpecColorHigh, kLightManagerConsoleTag)
// PE 0x100BED51 calls the double accessors directly, without virtual dispatch.
// PE 0x100BEC87, 0x100BED04: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(staticLightBlendingTime, double, lightManager(),
                     setStaticLightBlendingTime, getStaticLightBlendingTime,
                     kLightManagerConsoleTag)

// Ground-hemi string property, available while the light manager exists.
void setGroundHemiName(const std::string& name)
{
    lightManager()->setGroundHemi(name);
}

std::string getGroundHemiName()
{
    const std::string& name = lightManager()->getGroundHemi();
    return std::string(name.c_str(), name.size());
}

// PE 0x100BED9B, 0x100BEE82, 0x100BEE18: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(groundHemi, std::string, lightManager() != 0,
                        getGroundHemiName(), setGroundHemiName, kLightManagerConsoleTag)

// Binding 29 is the connected stock path: PE 0x1015BD70/0x1015BD80 forward it to the published
// HemiMapManager without a null guard.
// PE 0x100BEEF7, 0x100BEF68: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(hemiLerpBias, float, lightManager(),
                     setHemiLerpBias, getHemiLerpBias, kLightManagerConsoleTag)

// Binding 30 follows hemiLerpBias in the shipped static-construction order.
// PE 0x100BEFFE, 0x100BF06F: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(lightCullDistance, float, lightManager(),
                     setLightCullDistance, getLightCullDistance, kLightManagerConsoleTag)

// PE 0x100BF1C3, +0x5C/+0x60.
// PE 0x100BF105, 0x100BF176: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(maxLightsPerBundledMesh, int, lightManager(),
                     setMaxLightsPerBundledMesh, getMaxLightsPerBundledMesh, kLightManagerConsoleTag)
// PE 0x100BF2C7, +0x64/+0x68.
// PE 0x100BF209, 0x100BF27A: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(maxLightsPerStaticMesh, int, lightManager(),
                     setMaxLightsPerStaticMesh, getMaxLightsPerStaticMesh, kLightManagerConsoleTag)
// PE 0x100BF3CB, +0x6C/+0x70.
// PE 0x100BF30D, 0x100BF37E: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(maxLightsPerSkinnedMesh, int, lightManager(),
                     setMaxLightsPerSkinnedMesh, getMaxLightsPerSkinnedMesh, kLightManagerConsoleTag)
// PE 0x100BF4CF, +0x74/+0x78.
// PE 0x100BF411, 0x100BF482: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(maxLightsPerTerrainPatch, int, lightManager(),
                     setMaxLightsPerTerrainPatch, getMaxLightsPerTerrainPatch, kLightManagerConsoleTag)

// PE constructor 0x100BF515, executor 0x100BF5D3, .CRT entry 0x102366E4. Reads call +0x80;
// writes pass the parsed float to +0x7C.
// PE 0x100BF586: argument conversion.
BF2_CONSOLE_PROPERTY(skinnedMeshShaderLodDistance, float, lightManager(),
                     setSkinnedMeshShaderLodDistance, getSkinnedMeshShaderLodDistance,
                     kLightManagerConsoleTag)

// PE 0x100BF762; this pair has the getter first, +0x84, then the setter, +0x88.
// PE 0x100BF61F, 0x100BF6F7: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(lightMapAmbientAlphaCutoffRange, Vec2, lightManager(),
                     setLightMapAmbientAlphaCutoffRange, getLightMapAmbientAlphaCutoffRange,
                     kLightManagerConsoleTag)

// PE 0x100BF7CB, 0x100BF850, 0x100BF8C3: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(sunDirection, Vec3, lightManager(),
                     setSunDirection, getSunDirection, kLightManagerConsoleTag)
// PE 0x100BF947, 0x100BF9CC, 0x100BFA3F: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(sunColor, Vec3, lightManager(),
                     setSunColor, getSunColor, kLightManagerConsoleTag)
// PE 0x100BFAC3, 0x100BFB48, 0x100BFBBB: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(sunSpecColor, Vec3, lightManager(),
                     setSunSpecColor, getSunSpecColor, kLightManagerConsoleTag)
// PE 0x100BFC3F, 0x100BFCB0, 0x100BFCFD: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY(enableSun, bool, lightManager(),
                     setSunEnabled, getSunEnabled, kLightManagerConsoleTag)

// PE 0x100BFE12, +0x11C/+0x120.
// PE 0x100BFD54, 0x100BFDC5: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(enableStencilCullerReset, bool, lightManager(),
                     setStencilCullerResetEnabled, getStencilCullerResetEnabled,
                     kLightManagerConsoleTag)

// PE 0x100BFF80, +0x2D4/+0x2D8: the same LOD limit the StaticMesh shadow draw uses.
// PE 0x100BFE69, 0x100BFF33: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(maxLodForMaterialBatching, int, lightManager(),
                     setShadowMergeLodLimit, getShadowMergeLodLimit, kLightManagerConsoleTag)

// The last binding before weatherManager's defaults. The public console spelling
// is instruction-proven as dynamicPointScale even though the evidence-scoped interface wrapper
// describes the underlying value as a dynamic-light range scale.
// PE 0x100BFFCC, 0x100C003D: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(dynamicPointScale, float, lightManager(),
                     setDynamicLightRangeScale, getDynamicLightRangeScale,
                     kLightManagerConsoleTag)

// A separate shipped console name for the same LightManager singleton. PE 101D1AEA installs
// SetConsoleClassDefaults("shadowManager", 2, 0), after weatherManager and before HemiMapManager.
// The property constructors occupy .CRT entries 0x10236760..0x10236800 in retail order.
BF2_CONSOLE_OBJECT("shadowManager", 2, 0)

// PE 0x100C17FE, +0x9C/+0xA0.
// PE 0x100C1740, 0x100C17B1: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowMapSize, int, lightManager(),
                     setShadowMapSize, getShadowMapSize, kShadowManagerConsoleTag)
// PE 0x100C1919, +0xA4/+0xA8.
// PE 0x100C185B, 0x100C18CC: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowMapObjectSize, int, lightManager(),
                     setShadowMapObjectSize, getShadowMapObjectSize, kShadowManagerConsoleTag)
// PE 0x100C1A23, +0xAC/+0xB0.
// PE 0x100C1965, 0x100C19D6: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowMapBitDepth, int, lightManager(),
                     setShadowMapBitDepth, getShadowMapBitDepth, kShadowManagerConsoleTag)
// PE 0x100C1B2D, +0xB4/+0xB8.
// PE 0x100C1A6F, 0x100C1AE0: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowMapDepthDistance, float, lightManager(),
                     setShadowDistance, getShadowDistance, kShadowManagerConsoleTag)
// PE 0x100C1C3A, +0xC4/+0xC8.
// PE 0x100C1B7C, 0x100C1BED: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowMapIncludeStaticMeshes, bool, lightManager(),
                     setShadowStaticFeature, getShadowStaticFeature, kShadowManagerConsoleTag)
// PE 0x100C1D4F, +0xCC/+0xD0.
// PE 0x100C1C91, 0x100C1D02: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowShowFrustums, bool, lightManager(),
                     setShadowShowFrustums, getShadowShowFrustums, kShadowManagerConsoleTag)
// PE 0x100C1E64, +0xD4/+0xD8.
// PE 0x100C1DA6, 0x100C1E17: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowIntensityBias, float, lightManager(),
                     setShadowIntensityBias, getShadowIntensityBias, kShadowManagerConsoleTag)
// PE 0x100C1F71, +0xDC/+0xE0. Keep retail's lowercase m in lightmap.
// PE 0x100C1EB3, 0x100C1F24: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(lightmapIntensityBias, float, lightManager(),
                     setLightMapIntensityBias, getLightMapIntensityBias, kShadowManagerConsoleTag)
// PE 0x100C207E, +0xE4/+0xE8.
// PE 0x100C1FC0, 0x100C2031: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(dropShadowClipHeightOffset, float, lightManager(),
                     setDropShadowClipHeightOffset, getDropShadowClipHeightOffset, kShadowManagerConsoleTag)
// PE 0x100C218B, +0xEC/+0xF0.
// PE 0x100C20CD, 0x100C213E: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(dynamicShadowsEnabled, bool, lightManager(),
                     setDynamicShadowsEnabled, getDynamicShadowsEnabled, kShadowManagerConsoleTag)

// PE 0x100C22A0, +0xF4/+0xF8.
// PE 0x100C21E2, 0x100C2253: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(dynamicStaticMeshShadowsEnabled, bool, lightManager(),
                     setDynamicStaticMeshShadowsEnabled, getDynamicStaticMeshShadowsEnabled,
                     kShadowManagerConsoleTag)
// PE 0x100C23B5, +0xFC/+0x100.
// PE 0x100C22F7, 0x100C2368: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(dynamic1pShadowsEnabled, bool, lightManager(),
                     setDynamic1pShadowsEnabled, getDynamic1pShadowsEnabled, kShadowManagerConsoleTag)
// PE 0x100C24CA, +0x104/+0x108.
// PE 0x100C240C, 0x100C247D: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(allowAlphaShadows, bool, lightManager(),
                     setAllowAlphaShadows, getAllowAlphaShadows, kShadowManagerConsoleTag)
// PE 0x100C25DF, +0x10C/+0x110.
// PE 0x100C2521, 0x100C2592: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(alphaShadowThreshold, float, lightManager(),
                     setAlphaShadowThreshold, getAlphaShadowThreshold, kShadowManagerConsoleTag)
// PE 0x100C26EC, +0x114/+0x118.
// PE 0x100C262E, 0x100C269F: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowUpdateFactor, float, lightManager(),
                     setShadowUpdateFactor, getShadowUpdateFactor, kShadowManagerConsoleTag)
// PE 0x100C27F9, +0x124/+0x128.
// PE 0x100C273B, 0x100C27AC: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowUpdatesEnabled, bool, lightManager(),
                     setShadowUpdatesEnabled, getShadowUpdatesEnabled, kShadowManagerConsoleTag)
// PE 0x100C290E, +0x12C/+0x130.
// PE 0x100C2850, 0x100C28C1: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowMapDepthBias, float, lightManager(),
                     setShadowDepthBias, getShadowDepthBias, kShadowManagerConsoleTag)
// PE 0x100C2A1B, +0x134/+0x138.
// PE 0x100C295D, 0x100C29CE: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowMapSlopeBias, float, lightManager(),
                     setShadowSlopeScaleDepthBias, getShadowSlopeScaleDepthBias, kShadowManagerConsoleTag)
// PE 0x100C2B28; getter +0x13C precedes setter +0x140.
// PE 0x100C2A6A, 0x100C2ADB: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowOccluderOversizeFactor, float, lightManager(),
                     setShadowOccluderOversizeFactor, getShadowOccluderOversizeFactor,
                     kShadowManagerConsoleTag)
// PE 0x100C2C35; getter +0x144 precedes setter +0x148.
// PE 0x100C2B77, 0x100C2BE8: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(useLSSMOptimizations, bool, lightManager(),
                     setUseLSSMOptimizations, getUseLSSMOptimizations, kShadowManagerConsoleTag)

// PE 0x100C2D4A, +0x150/+0x14C.
// PE 0x100C2C8C, 0x100C2CFD: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(usePSMOptimizations, bool, lightManager(),
                     setUsePSMOptimizations, getUsePSMOptimizations, kShadowManagerConsoleTag)
// PE 0x100C2E5F, +0x158/+0x154.
// PE 0x100C2DA1, 0x100C2E12: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowPSMUseSlideback, bool, lightManager(),
                     setShadowPSMUseSlideback, getShadowPSMUseSlideback, kShadowManagerConsoleTag)
// PE 0x100C2F74, +0x160/+0x15C.
// PE 0x100C2EB6, 0x100C2F27: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowPSMUseUnitCubeClipping, bool, lightManager(),
                     setShadowPSMUseUnitCubeClipping, getShadowPSMUseUnitCubeClipping,
                     kShadowManagerConsoleTag)
// PE 0x100C3089, +0x168/+0x164.
// PE 0x100C2FCB, 0x100C303C: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowPSMZNearOffset, float, lightManager(),
                     setShadowPSMZNearOffset, getShadowPSMZNearOffset, kShadowManagerConsoleTag)
// PE 0x100C3196, +0x170/+0x16C.
// PE 0x100C30D8, 0x100C3149: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowPSMZFarOffset, float, lightManager(),
                     setShadowPSMZFarOffset, getShadowPSMZFarOffset, kShadowManagerConsoleTag)
// PE 0x100C32A3, +0x178/+0x174.
// PE 0x100C31E5, 0x100C3256: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowPSMZMinInfinityZ, float, lightManager(),
                     setShadowPSMZMinInfinityZ, getShadowPSMZMinInfinityZ, kShadowManagerConsoleTag)
// PE 0x100C33B0, +0x180/+0x17C.
// PE 0x100C32F2, 0x100C3363: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowPSMFovY, float, lightManager(),
                     setShadowPSMFovY, getShadowPSMFovY, kShadowManagerConsoleTag)
// PE 0x100C34BD, +0x188/+0x184.
// PE 0x100C33FF, 0x100C3470: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowTSMFocus, float, lightManager(),
                     setShadowTSMFocus, getShadowTSMFocus, kShadowManagerConsoleTag)
// PE 0x100C35CA, +0x190/+0x18C.
// PE 0x100C350C, 0x100C357D: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadow80Line, float, lightManager(),
                     setShadow80Line, getShadow80Line, kShadowManagerConsoleTag)
// PE 0x100C36D7, +0x194/+0x198.
// PE 0x100C3619, 0x100C368A: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowMergeDynamicShadowFrustums, bool, lightManager(),
                     setShadowMergeDynamicShadowFrustums, getShadowMergeDynamicShadowFrustums,
                     kShadowManagerConsoleTag)

// PE 0x100C37EC, +0x274/+0x278.
// PE 0x100C372E, 0x100C379F: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowFromTerrainLod, int, lightManager(),
                     setTerrainShadowLod, getTerrainShadowLod, kShadowManagerConsoleTag)
// PE 0x100C38F6, +0x27C/+0x280.
// PE 0x100C3838, 0x100C38A9: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowFromStaticMeshLod, int, lightManager(),
                     setStaticMeshShadowLod, getStaticMeshShadowLod, kShadowManagerConsoleTag)
// PE 0x100C3A00, +0x284/+0x288.
// PE 0x100C3942, 0x100C39B3: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowFromBundledMeshLod, int, lightManager(),
                     setBundledMeshShadowLod, getBundledMeshShadowLod, kShadowManagerConsoleTag)
// PE 0x100C3B0A, +0x28C/+0x290.
// PE 0x100C3A4C, 0x100C3ABD: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowFromSkinnedMeshLod, int, lightManager(),
                     setSkinnedMeshShadowLod, getSkinnedMeshShadowLod, kShadowManagerConsoleTag)
// PE 0x100C3C14, +0x294/+0x298.
// PE 0x100C3B56, 0x100C3BC7: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowOverrideQuality, int, lightManager(),
                     setShadowOverrideQuality, getShadowOverrideQuality, kShadowManagerConsoleTag)
// PE 0x100C3D1E, +0x29C/+0x2A0. Shipped defect: the public spelling is Lenght.
// PE 0x100C3C60, 0x100C3CD1: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowTestBelowRayLenght, float, lightManager(),
                     setShadowRoofRayLength, getShadowRoofRayLength, kShadowManagerConsoleTag)
// PE 0x100C3E2B, +0x2A4/+0x2A8.
// PE 0x100C3D6D, 0x100C3DDE: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowSunScalePower, float, lightManager(),
                     setShadowSunScalePower, getShadowSunScalePower, kShadowManagerConsoleTag)
// PE 0x100C3F65, +0x2BC/+0x2C0.
// PE 0x100C3E7A, 0x100C3EFA: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowSunHeightOverFade, Vec2, lightManager(),
                     setShadowSunHeightOverFade, getShadowSunHeightOverFade, kShadowManagerConsoleTag)
// PE 0x100C408C, +0x2AC/+0x2B0.
// PE 0x100C3FCE, 0x100C403F: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowSunExtrusionLength, float, lightManager(),
                     setShadowPullbackDistance, getShadowPullbackDistance, kShadowManagerConsoleTag)
// PE 0x100C4199, +0x2B4/+0x2B8.
// PE 0x100C40DB, 0x100C414C: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(shadowUseSweepTest, bool, lightManager(),
                     setShadowUseSweepTest, getShadowUseSweepTest, kShadowManagerConsoleTag)
// PE 0x100C42AE, +0x2E4/+0x2E8.
// PE 0x100C41F0, 0x100C4261: constructor, argument conversion.
BF2_CONSOLE_PROPERTY(occludedSkyNightIntensity, float, lightManager(),
                     setOccludedSkyNightIntensity, getOccludedSkyNightIntensity, kShadowManagerConsoleTag)

} // anonymous namespace

bool registerLightManagerConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kLightManagerConsoleTag));
}

bool unregisterLightManagerConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kLightManagerConsoleTag));
}

bool registerShadowManagerConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kShadowManagerConsoleTag));
}

bool unregisterShadowManagerConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kShadowManagerConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
