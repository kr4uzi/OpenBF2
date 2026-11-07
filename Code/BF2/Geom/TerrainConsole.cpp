
// Native terrain console bindings. Creation is always available; other commands
// require the terrain singleton.

#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/PluginAbi.h"

#include "Terrain.h"
#include "MeshTemplate.h"                     // RaMaterialParameters - the terrain's material block
#include "ITerrainEditable.h"                 // terrainWaterColor's target (IID 820005)

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <string>
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../Math/Vec3.h"

#include <cstring>
#include <string>

namespace dice {
namespace hfe {
namespace geom {

namespace {

// Stands in for the original's file-scope list (the static at 0x1024CCD8).
const char kTerrainConsoleTag[] = "geom.terrain";

// init - ctor 0x100B6868, executor slot +0x14.
void consoleInitTerrain()
{
    terrain()->init();
}

// load - ctor 0x100B68D5, executor PE 0x100B6946 (slot +0x18 with the argument copied to a
// std::string through the copy constructor, IAT 0x101DB198), parser PE 0x100B6985 (one persistent
// std::string token).
void consoleLoadTerrain(const std::string& path)
{
    terrain()->load(std::string(path));
}

// forceRenderStyle - ctor 0x100B502C, executor get slot +0x24, set slot +0x20.
int  getForceRenderStyle() { return terrain()->getRenderStyle(); }
void setForceRenderStyle(int style) { terrain()->setRenderStyle(style); }

// sunColor - ctor 0x100B6137, executor get slot +0x74 (the stored colour times
// four), set slot +0x70 with the three components.
Vec3 getSunColor()
{
    Vec3 v;
    terrain()->getSunColor(&v);
    return v;
}
void setSunColor(const Vec3& v) { terrain()->setSunColor(v.x, v.y, v.z); }

// GIColor - ctor 0x100B62AD, executor get slot +0x80 (twice the stored colour),
// set slot +0x7C.
Vec3 getGIColor()
{
    Vec3 v;
    terrain()->getGIColorTimes2(&v);
    return v;
}
void setGIColor(const Vec3& v) { terrain()->setGIColor(v.x, v.y, v.z); }

// waterSunIntensity - ctor 0x100B5A6B, executor the get reads the material block's
// +0x50; the set (0x100B5B72) writes the vec4 (1, intensity, 0.8, 1) over the block's +0x4C.
float getWaterSunIntensity()
{
    return static_cast<const RaMaterialParameters*>(terrain()->m_materialParameters)->pointColor.value[1];   // +0x50
}
// PE 0x100B5B72
void setWaterSunIntensity(float intensity)
{
    const float record[4] = { 1.0f, intensity, 0.8f, 1.0f };
    ::memcpy(static_cast<RaMaterialParameters*>(terrain()->m_materialParameters)->pointColor.value,
             record, sizeof(record));                                                               // +0x4C
}

// The water properties (ctors 0x100B513C .. 0x100B5EB4), active while the terrain exists
// (0x100B509D). Unlike the renderer's copies of the first six (RendererConsole.cpp) these arms
// call the terrain without a further null test.

// waterQuality - ctor 0x100B513C, executor 0x100B51FA: the int +0x704 read and stored directly.
int  getWaterQuality() { return terrain()->m_waterQuality; }
void setWaterQuality(int quality) { terrain()->m_waterQuality = quality; }

// waterScroll - ctor 0x100B523D, executor 0x100B5328: slots +0x88 / +0x8C.
Vec2 getWaterScroll()
{
    Vec2 v;
    return *terrain()->getWaterScroll(&v);
}
void setWaterScroll(const Vec2& v) { terrain()->setWaterScroll(v.x, v.y); }

// waterAnimSpeed - ctor 0x100B5398, executor 0x100B5456: slots +0x90 / +0x94.
float getWaterAnimSpeed() { return terrain()->getWaterAnimSpeed(); }
void  setWaterAnimSpeed(float v) { terrain()->setWaterAnimSpeed(v); }

// waterNormalMap - ctor 0x100B54A5, executor 0x100B5522: slots +0x98 / +0x9C (by value).
std::string getWaterNormalMap() { return terrain()->getWaterNormalMap(); }
void setWaterNormalMap(const std::string& name) { terrain()->setWaterNormalMap(name); }

// waterSpecularColor - ctor 0x100B5624, executor 0x100B5781: slots +0xA0 / +0xA4.
Vec4 getWaterSpecularColor()
{
    Vec4 v;
    return *terrain()->getWaterSpecularColor(&v);
}
void setWaterSpecularColor(const Vec4& v) { terrain()->setWaterSpecularColor(v.x, v.y, v.z, v.w); }

// waterColor - ctor 0x100B57F9, executor 0x100B58F1: the non-virtual 0x100DB980 / 0x100DB9A0.
Vec3 getWaterColor() { return terrain()->getWaterColor(); }
void setWaterColor(const Vec3& v) { terrain()->setWaterColor(v); }

// waterSpecularPower - ctor 0x100B595E, executor 0x100B5A1C: slots +0xA8 / +0xAC.
float getWaterSpecularPower() { return terrain()->getWaterSpecularPower(); }
void  setWaterSpecularPower(float v) { terrain()->setWaterSpecularPower(v); }

// waterAlphaHeightMul / waterAlphaAdd - ctors 0x100B5BA5 / 0x100B5CA9, executors 0x100B5C63 /
// 0x100B5D67: the floats +0x738 / +0x73C read and stored directly.
float getWaterAlphaHeightMul() { return terrain()->m_waterAlphaHeightMul; }
void  setWaterAlphaHeightMul(float v) { terrain()->m_waterAlphaHeightMul = v; }
float getWaterAlphaAdd() { return terrain()->m_waterAlphaAdd; }
void  setWaterAlphaAdd(float v) { terrain()->m_waterAlphaAdd = v; }

// waterTransparency / waterReflectivity - ctors 0x100B5DAD / 0x100B5EB4, executors 0x100B5E6B /
// 0x100B5F72: the water record's above-water floats.
float getWaterTransparency() { return terrain()->getWaterTransparency(); }
void  setWaterTransparency(float v) { terrain()->setWaterTransparency(v); }
float getWaterReflectivity() { return terrain()->getWaterReflectivity(); }
void  setWaterReflectivity(float v) { terrain()->setWaterReflectivity(v); }

// sub_100B0CE5 - the terrain's ITerrainEditable (IID 820005), or null without a terrain or when
// the terrain is the game's plain Terrain.
ITerrainEditable* editableTerrain()
{
    if (!g_Terrain)
        return 0;
    return static_cast<ITerrainEditable*>(g_Terrain->queryInterface(kIID_ITerrainEditable));
}

// terrainWaterColor - ctor 0x100B5FBB, active 0x100B6040 (an editable terrain), executor
// 0x100B60BF: ITerrainEditable +0x13C / +0x138, the interface fetched again for each arm.
Vec3 getTerrainWaterColor() { return editableTerrain()->getTerrainWaterColor(); }
void setTerrainWaterColor(const Vec3& v) { editableTerrain()->setTerrainWaterColor(v); }

// GIColorHigh / GIColorLow - ctors 0x100B6426 / 0x100B658C, executors 0x100B651E / 0x100B6684:
// the reads copy +0x308 / +0x314 inline, the writes call 0x100311D0 / 0x10031200.
Vec3 getGIColorHigh() { return terrain()->m_giColorHigh; }
void setGIColorHigh(const Vec3& v) { terrain()->setGIColorHigh(v); }
Vec3 getGIColorLow() { return terrain()->m_giColorLow; }
void setGIColorLow(const Vec3& v) { terrain()->setGIColorLow(v); }

// ambientColor - ctor 0x100B66F2, executor 0x100B67EA: 0x100DB030 / 0x100DB000.
Vec3 getAmbientColor()
{
    Vec3 v;
    return *terrain()->getAmbientColor(&v);
}
void setAmbientColor(const Vec3& v) { terrain()->setAmbientColor(v.x, v.y, v.z); }

#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
// drawPrimaryLightmaps / drawSurroundingLightmaps - the 2006 retail editor's bool properties (its renderer's binding
// constructors 0x101DD570 / 0x101DDBF0, which set and read its Terrain +0x261 / +0x263); 1.50 binds neither. The
// retail editor set them before tracing the terrain's lightmaps. They switch the two gates the terrain light sampler
// reads when it gathers its patches - +0x358 the primary patches, +0x359 the ring patches - which also gate the
// terrain's draw. [CHOSEN, not recovered] bound in the editor's flavour only; [INFERRED] the 1.50 fields they map to.
bool getDrawPrimaryLightmaps() { return terrain()->m_drawMain != 0; }
void setDrawPrimaryLightmaps(bool on) { terrain()->m_drawMain = on ? 1 : 0; }
bool getDrawSurroundingLightmaps() { return terrain()->m_flag359 != 0; }
void setDrawSurroundingLightmaps(bool on) { terrain()->m_flag359 = on ? 1 : 0; }
#endif

BF2_CONSOLE_OBJECT("terrain", 2, 0)

// create - PE 0x100B4F0B (constructor), 0x100B4F7F (executor), 0x100B4FB7 (argument conversion).
// Written out rather than through BF2_CONSOLE_METHOD_1_STATIC_ACTIVE: the constructor takes the
// defaults' static name (dword_10247C20, "terrain" as passed) where the macro reads the lowercased
// copy. Always active (0x100B4F7C). The executor copies the argument (IAT 0x101DB198), clears the
// result flag and hands the copy to ModuleGeom's registration (0x100B118C).
class TerrainCreateBinding : public ::dice::hfe::ConsoleObjectBaseImpl
{
public:
    // PE 0x100B4F0B
    TerrainCreateBinding()
    {
        m_objectName     = consoleDefaultStaticName();
        m_access         = consoleDefaultAccess();
        m_category       = consoleDefaultCategory();
        m_functionName   = "create";
        m_type           = 0;
        m_minNoArgs      = 1;
        m_maxNoArgs      = 1;
        m_returnType     = "void";
        m_hasReturnValue = false;
        m_isDynamic      = false;
        m_argTypes[0]    = "std::string";
        m_argNeedsConversion[0] = (std::string(m_argTypes[0]) == "std::string") ? 0 : -1;
        consoleBindingList(kTerrainConsoleTag).push_back(this);
    }
    virtual bool isObjectActive() const { return true; }
    // PE 0x100B4F7F
    virtual void executeObjectMethod()
    {
        if (m_argCount != 1)
            return;
        const std::string className(BF2_CC_ARG(std::string, 0));
        m_hasReturnValue = false;
        registerTerrainSingleton(className);
    }
    // PE 0x100B4FB7
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};
TerrainCreateBinding s_terrainCreateBinding;

// PE 0x100B50A9
BF2_CONSOLE_PROPERTY_FN(forceRenderStyle, int, terrain() != 0,
                        getForceRenderStyle(), setForceRenderStyle, kTerrainConsoleTag)
// PE 0x100B51AD
BF2_CONSOLE_PROPERTY_FN(waterQuality, int, terrain() != 0,
                        getWaterQuality(), setWaterQuality, kTerrainConsoleTag)
// PE 0x100B52BD / 0x100B5328.
BF2_CONSOLE_PROPERTY_FN(waterScroll, Vec2, terrain() != 0,
                        getWaterScroll(), setWaterScroll, kTerrainConsoleTag)
// PE 0x100B5409: the argument conversion.
BF2_CONSOLE_PROPERTY_FN(waterAnimSpeed, float, terrain() != 0,
                        getWaterAnimSpeed(), setWaterAnimSpeed, kTerrainConsoleTag)
// PE 0x100B5522 / 0x100B55AF.
BF2_CONSOLE_PROPERTY_FN(waterNormalMap, std::string, terrain() != 0,
                        getWaterNormalMap(), setWaterNormalMap, kTerrainConsoleTag)
// PE 0x100B5706 / 0x100B5781.
// PE 0x100B56AE
BF2_CONSOLE_PROPERTY_FN(waterSpecularColor, Vec4, terrain() != 0,
                        getWaterSpecularColor(), setWaterSpecularColor, kTerrainConsoleTag)
// PE 0x100B587E / 0x100B58F1.
BF2_CONSOLE_PROPERTY_FN(waterColor, Vec3, terrain() != 0,
                        getWaterColor(), setWaterColor, kTerrainConsoleTag)
// PE 0x100B59CF: the argument conversion.
BF2_CONSOLE_PROPERTY_FN(waterSpecularPower, float, terrain() != 0,
                        getWaterSpecularPower(), setWaterSpecularPower, kTerrainConsoleTag)
// PE 0x100B5ADC
BF2_CONSOLE_PROPERTY_FN(waterSunIntensity, float, terrain() != 0,
                        getWaterSunIntensity(), setWaterSunIntensity, kTerrainConsoleTag)
// PE 0x100B5C16
BF2_CONSOLE_PROPERTY_FN(waterAlphaHeightMul, float, terrain() != 0,
                        getWaterAlphaHeightMul(), setWaterAlphaHeightMul, kTerrainConsoleTag)
// PE 0x100B5CA9, 0x100B5D1A
BF2_CONSOLE_PROPERTY_FN(waterAlphaAdd, float, terrain() != 0,
                        getWaterAlphaAdd(), setWaterAlphaAdd, kTerrainConsoleTag)
// PE 0x100B5E1E
BF2_CONSOLE_PROPERTY_FN(waterTransparency, float, terrain() != 0,
                        getWaterTransparency(), setWaterTransparency, kTerrainConsoleTag)
// PE 0x100B5EB4, 0x100B5F25
BF2_CONSOLE_PROPERTY_FN(waterReflectivity, float, terrain() != 0,
                        getWaterReflectivity(), setWaterReflectivity, kTerrainConsoleTag)
// PE 0x100B604C
// PE 0x100B60BF
BF2_CONSOLE_PROPERTY_FN(terrainWaterColor, Vec3, editableTerrain() != 0,
                        getTerrainWaterColor(), setTerrainWaterColor, kTerrainConsoleTag)
// PE 0x100B61BC / 0x100B622F.
BF2_CONSOLE_PROPERTY_FN(sunColor, Vec3, terrain() != 0, getSunColor(), setSunColor, kTerrainConsoleTag)
// PE 0x100B6332 / 0x100B63A5.
BF2_CONSOLE_PROPERTY_FN(GIColor, Vec3, terrain() != 0, getGIColor(), setGIColor, kTerrainConsoleTag)
// PE 0x100B64AB
// PE 0x100B651E
BF2_CONSOLE_PROPERTY_FN(GIColorHigh, Vec3, terrain() != 0,
                        getGIColorHigh(), setGIColorHigh, kTerrainConsoleTag)
// PE 0x100B658C, 0x100B6684, 0x100B6611
BF2_CONSOLE_PROPERTY_FN(GIColorLow, Vec3, terrain() != 0,
                        getGIColorLow(), setGIColorLow, kTerrainConsoleTag)
// PE 0x100B6777: the argument conversion.
// PE 0x100B67EA
BF2_CONSOLE_PROPERTY_FN(ambientColor, Vec3, terrain() != 0,
                        getAmbientColor(), setAmbientColor, kTerrainConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(init, consoleInitTerrain, terrain() != 0, kTerrainConsoleTag)
// PE 0x100B6985
// PE 0x100B6946
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(load, consoleLoadTerrain, terrain() != 0, std::string, kTerrainConsoleTag)
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
BF2_CONSOLE_PROPERTY_FN(drawPrimaryLightmaps, bool, terrain() != 0,
                        getDrawPrimaryLightmaps(), setDrawPrimaryLightmaps, kTerrainConsoleTag)
BF2_CONSOLE_PROPERTY_FN(drawSurroundingLightmaps, bool, terrain() != 0,
                        getDrawSurroundingLightmaps(), setDrawSurroundingLightmaps, kTerrainConsoleTag)
#endif

} // anonymous namespace

bool registerTerrainConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kTerrainConsoleTag));
}

bool unregisterTerrainConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kTerrainConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
