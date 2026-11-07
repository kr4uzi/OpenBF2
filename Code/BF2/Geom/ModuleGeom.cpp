// Path:     attested - "C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\Geom\ModuleGeom.cpp"
//           appears verbatim in RendDX9.dll at 0x1020DB18.
// Contents: recovered from RendDX9.dll (PE, symbols stripped).

// See Code/BF2/RendDX9/BF2Render.cpp for the BF2_IMAGE_RENDDX9 guard and why it is here.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginModules.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Core/EventManager.h"
#include "XpLightManager.h"
#include "HemiMapManager/HemiMapDependencies.h"
#include "HemiMapManager/HemiMapManager.h"
#include "Terrain.h"
#include "ITerrainEditable.h"
#include "TerrainEditable.h"

namespace dice {
namespace hfe {
namespace geom {

extern IBase* g_GeometryTemplateManager;

bool registerLightManagerConsoleObjects();
bool unregisterLightManagerConsoleObjects();
bool registerShadowManagerConsoleObjects();
bool unregisterShadowManagerConsoleObjects();
bool registerHemiMapManagerConsoleObjects();
bool unregisterHemiMapManagerConsoleObjects();
bool registerGeometryTemplateConsoleObjects();
bool unregisterGeometryTemplateConsoleObjects();
bool registerSkyDomeConsoleObjects();
bool unregisterSkyDomeConsoleObjects();
bool registerTerrainConsoleObjects();
bool unregisterTerrainConsoleObjects();
bool registerTerrainCullerConsoleObjects();
bool unregisterTerrainCullerConsoleObjects();
bool registerNametagManagerConsoleObjects();      // Geom/NametagManagerConsole.cpp - `nametags`
bool unregisterNametagManagerConsoleObjects();
bool registerUndergrowthConsoleObjects();       // Geom/UndergrowthSystemConsole.cpp - `undergrowth`
bool unregisterUndergrowthConsoleObjects();
bool registerDecalManagerConsoleObjects();      // Geom/DecalSystem/DecalManagerConsole.cpp - `DecalManager`
bool unregisterDecalManagerConsoleObjects();
bool registerOverGrowthConsoleObjects();        // Geom/Overgrowth/OverGrowthConsole.cpp - `Overgrowth`, `OvergrowthType`
bool unregisterOverGrowthConsoleObjects();
bool registerObjectGeometryConsoleObjects();   // Geom/ObjectGeometryConsole.cpp - `Object.geometry.*`
bool unregisterObjectGeometryConsoleObjects();
bool registerStaticMeshRendererConsoleObjects();   // Geom/StaticMeshRendererConsole.cpp - the atlas rows
bool unregisterStaticMeshRendererConsoleObjects();
bool registerSkinnedMeshRendererConsoleObjects();
bool unregisterSkinnedMeshRendererConsoleObjects();
bool registerParticleSystemManagerConsoleObjects();
bool unregisterParticleSystemManagerConsoleObjects();
bool registerWeatherManagerConsoleObjects();   // Geom/WeatherManagerConsole.cpp - `weatherManager`
bool unregisterWeatherManagerConsoleObjects();
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
// Geom/LightmapGeneration/LightmapGenerationConsole.cpp - the 2006 editor's `lightmapGeneration`
// rows, editor flavour only ([CHOSEN, not recovered]: 1.50 publishes none).
bool registerLightmapGenerationConsoleObjects();
bool unregisterLightmapGenerationConsoleObjects();
#endif

// Both already in Code/BF2/Core/Ids.h from the ELF, and both confirmed here:
//   the classId is read AS data, not as an immediate - getClassList does `push ds:dword_1021AEF4`
//   at 0x100C7FE2, and that dword is 29 A3 0C 87 = 0x870CA329.
//   the iid is the one queryInterface (0x100C7ED0) tests, as the signed -1382652484.
// RoadCompiledRenderer.cpp - see the registration below.
extern const unsigned int CID_RoadCompiledRenderer;
IBase* __fastcall createRoadCompiledRenderer(unsigned int iid, IBase* outer);

extern const unsigned int CID_LightGeometry;
IBase* __fastcall createLightGeometry(unsigned int iid, IBase* outer);

// RoadCompiledTemplate.cpp - GeometryTemplate.RoadCompiled (821901), the template every stock road
// names. see the class list below.
extern const unsigned int CID_RoadCompiledTemplate;
IBase* __fastcall createRoadCompiledTemplate(unsigned int iid, IBase* outer);

// RoadEditableRenderer.cpp / RoadTemplate.cpp - the editor's road queue (821913, the
// "RoadEditableRenderer" singleton below) and dice.hfe.geom.Roads.RoadTemplate (821008).
// Both classes are registered in the class list below.
extern const unsigned int CID_RoadEditableRenderer;
IBase* __fastcall createRoadEditableRenderer(unsigned int iid, IBase* outer);
extern const unsigned int CID_RoadTemplate;
IBase* __fastcall createRoadTemplate(unsigned int iid, IBase* outer);
// Editable-road class and factory.
extern const unsigned int CID_RoadEditable;
IBase* __fastcall createRoadEditable(unsigned int iid, IBase* outer);

// BundledMeshOptimizer.cpp / StaticMeshOptimizer.cpp - the two GeometryOptimizer classes
// Renderer::initialize registers, runs and clears (class ids dword_1021AF0C / dword_1021AF14).
IBase* __fastcall createBundledMeshOptimizer(unsigned int iid, IBase* outer);
IBase* __fastcall createStaticMeshOptimizer(unsigned int iid, IBase* outer);

// DecalSystem/DecalManager.cpp - because joining A server crashed without it:
// BF2's in-game HUD does getSingleton("DecalManager") and dereferences the result unchecked
// (0x004F6FD2). The singleton entry is already in getSingletonList; this supplies its factory.
extern const unsigned int CID_DecalManager;
IBase* __fastcall createDecalManager(unsigned int iid, IBase* outer);

// DecalSystem/DecalShadowManager.cpp. Its constructor registers "DecalShadowTemplate" with the
// decal manager, which the singleton list creates first.
IBase* __fastcall createDecalShadowManager(unsigned int iid, IBase* outer);

// Factories for the particle, static-mesh and skinned-mesh renderer singletons.
extern const unsigned int CID_ParticleSystemManager;
IBase* __fastcall createParticleSystemManager(unsigned int iid, IBase* outer);
extern const unsigned int CID_StaticMeshRenderer;
IBase* __fastcall createStaticMeshRenderer(unsigned int iid, IBase* outer);
extern const unsigned int CID_SkinnedMeshRenderer;
IBase* __fastcall createSkinnedMeshRenderer(unsigned int iid, IBase* outer);

// Sky-dome factory; construction requires shader loading and parameter lookup.
IBase* __fastcall createSkyDome(unsigned int iid, IBase* outer);

// Factory for the undergrowth singleton and its grass resources.
IBase* __fastcall createUndergrowthSystem(unsigned int iid, IBase* outer);

// Factories for the storm (Geom/WeatherManager.cpp) and its lightning (Geom/LightningManager.cpp).
IBase* __fastcall createLightningManager(unsigned int iid, IBase* outer);
IBase* __fastcall createWeatherManager(unsigned int iid, IBase* outer);
// Geom/Rain/RainManager.cpp.
IBase* __fastcall createRainManager(unsigned int iid, IBase* outer);

// Factory for static-mesh templates and their material/technique state.
IBase* __fastcall createStaticMeshTemplate(unsigned int iid, IBase* outer);

// Factories for skinned and bundled mesh templates used by world geometry.
IBase* __fastcall createSkinnedMeshTemplate(unsigned int iid, IBase* outer);
IBase* __fastcall createBundledMeshTemplate(unsigned int iid, IBase* outer);

// Factory for mesh-particle templates, published with MeshParticlesRenderer.
IBase* __fastcall createMeshParticleMeshTemplate(unsigned int iid, IBase* outer);

// Factory for scene traversal and geometry submission. Occlusion fallback reports and
// continues with all frustum-visible candidates.
// [RETIRE-WHEN:
//     RW-objectdrawer-occluders] Replace the fallback after silhouette construction and
// occluder culling are implemented and validated.
IBase* __fastcall createObjectDrawer(unsigned int iid, IBase* outer);

// Factory for the manager coordinating nametag updates and renderer buffering.
IBase* __fastcall createNametagManager(unsigned int iid, IBase* outer);

// The core-event handler publishes the SwiffRender singleton dynamically.
extern const unsigned int CID_SwiffRender;
extern const unsigned int IID_ISwiffRender;
IBase* __fastcall createSwiffRender(unsigned int iid, IBase* outer);

// GeomMisc/MenuRend/Video.cpp - the Bink movie pair. FSMoviePlayer and SwiffRender create Video by
// name; Video::close releases its VideoTarget.
extern const unsigned int CID_VideoTarget;
IBase* __fastcall createVideoTarget(unsigned int iid, IBase* outer);
extern const unsigned int CID_Video;
IBase* __fastcall createVideo(unsigned int iid, IBase* outer);

// GeometryTemplateManager.cpp - now in its attested source home.  The first create/find/active and
// ownership tranche made the old in-ModuleGeom skeleton too large to keep here honestly (RW-51).
extern const unsigned int IID_IGeometryTemplateManager;
extern const unsigned int CID_GeometryTemplateManager;
IBase* __fastcall createGeometryTemplateManager(unsigned int iid, IBase* outer);

// dice.hfe.geom.ObjectDrawer, declared in PluginModules.h. The class id is read from this module's
// own getClassList (sub_100518A0(-2029213122, "dice.hfe.geom.ObjectDrawer", ...)); the interface id
// is the one Renderer::initialize pairs with it at 0x1002CB3C.

extern const unsigned int CID_ObjectDrawer  = 2265754174u;
extern const unsigned int IID_IObjectDrawer = 3087129852u;


// The sixteen singleton globals ModuleGeom registers. Each is the IBase a SingletonRegInfo points
// at, so ClassManager writes the created object straight into it.

LightManager* g_LightManager    = 0;
IBase* g_GeometryTemplateManager = 0;
IBase* g_StaticMeshRenderer     = 0;
IBase* g_SkinnedMeshRenderer    = 0;
IBase* g_ParticleSystemManager  = 0;
IHemiMapManager* g_HemiMapManager = 0;
IBase* g_DecalManager           = 0;
IBase* g_DecalShadowManager     = 0;
IBase* g_RoadCompiledRenderer   = 0;
IBase* g_RoadEditableRenderer   = 0;
IBase* g_NametagManager         = 0;
IBase* g_SkyDome                = 0;
IBase* g_UndergrowthSystem      = 0;
IBase* g_LightningManager       = 0;
IBase* g_WeatherManager         = 0;
IBase* g_RainManager            = 0;

// dword_1024B930. Terrain is registered dynamically rather than through the sixteen-entry static
// list above: the level's Terrain.con runs `terrain.create Terrain`, whose executor calls the
// body below. Null until then, and again after close() unregisters the singleton.
IBase* g_Terrain                = 0;

// Replaces Terrain with the requested registered geometry class. Unknown names publish nothing.
void registerTerrainSingleton(const std::string& className)
{
    const bool exists =
        classManager->getSingleton(renddx9abi::HostString("Terrain")) != 0;
    if (exists)
        classManager->unregisterSingleton(renddx9abi::HostString("Terrain"));

    std::string fullName("dice.hfe.geom.");
    fullName += className;
    unsigned int classId = 0;
    if (classManager->getClassID(renddx9abi::HostString(fullName.c_str()), &classId))
    {
        SingletonRegInfo info;
        info.name     = "Terrain";
        info.instance = &g_Terrain;
        info.iid      = 820004u;
        info.classId  = classId;
        info.flags    = 2;
        classManager->registerSingleton(info);
    }
}

HemiTerrainOverride beginHemiTerrainOverride()
{
    // Temporarily selects technique zero only when the terrain exposes ITerrainEditable.
    HemiTerrainOverride state = { 0, 0 };
    if (!g_Terrain)
        return state;
    ITerrainEditable* const editable =
        static_cast<ITerrainEditable*>(g_Terrain->queryInterface(kIID_ITerrainEditable));
    state.editable = editable;
    if (!editable)
        return state;
    state.previousTechnique = editable->getShaderTechnique();             // +0x8C
    editable->setShaderTechnique(0);                                      // +0x88
    return state;
}

void endHemiTerrainOverride(const HemiTerrainOverride& state)
{
    // PE 0x1012BC94..0x1012BCA1, after the state pop: the saved technique back.
    if (state.editable)
        state.editable->setShaderTechnique(state.previousTechnique);
}


// dice::hfe::geom::ModuleGeom - primary vtable 0x101E2EC4, object 12 bytes


RD9_MODULE_IBASE_BODIES(ModuleGeom)

// The SwiffRender singleton slot the core-event handler publishes into. dword_1024B920.
IBase* g_SwiffRender = 0;


// Event (10, 1) registers and immediately creates SwiffRender after SwiffPlayer loads.
// Event (10, 2) unregisters it and clears the singleton pointer.

// PE 0x100B0EB2. Shipped defect: the retail body dereferences the class manager and the event manager
// without a null test; the guards below keep the native backends from faulting before startup order
// publishes them (memory-safety exception).
void ModuleGeom::handleCoreModuleEvent(int category, int id, int a3, int a4)
{
    if (category != 10)
        return;

    if (id == 1)
    {
        if (!classManager)
            return;
        SingletonRegInfo info;
        info.name     = "SwiffRender";
        info.instance = &g_SwiffRender;
        info.iid      = IID_ISwiffRender;
        info.classId  = CID_SwiffRender;
        info.flags    = 2;
        classManager->registerSingleton(info);
        ::renddx9abi::logf("ModuleGeom::handleCoreModuleEvent(10,1): SwiffRender registered -> "
                           "instance 0x%08X", ::renddx9abi::addr(g_SwiffRender));
    }
    else if (id == 2)
    {
        if (classManager)
            classManager->unregisterSingleton(renddx9abi::HostString("SwiffRender"));
        g_SwiffRender = 0;
    }
}

// Appends the 32 geometry classes in retail order, each with its shipped name and class id.
// PE 0x100C7F43.
void ModuleGeom::getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::geom::ModuleGeom::getClassList", 0x10);

    ::renddx9abi::HostVectorWriter<ClassRegInfo> out(hostVector);

    {
        ClassRegInfo info;
        info.classId = 821004u;
        info.name    = "dice.hfe.geom.Terrain";
        info.create  = RD9_CLASS_FACTORY(&createTerrain);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 821005u;
        info.name    = "dice.hfe.geom.TerrainEditable";
        info.create  = RD9_CLASS_FACTORY(&createTerrainEditable);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_GeometryTemplateManager;
        info.name    = "dice.hfe.geom.GeometryTemplateManager";
        info.create  = RD9_CLASS_FACTORY(&createGeometryTemplateManager);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 0x870CA651u;
        info.name    = "dice.hfe.geom.GeometryTemplate.BundledMesh";
        info.create  = RD9_CLASS_FACTORY(&createBundledMeshTemplate);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 0x870CA655u;
        info.name    = "dice.hfe.geom.GeometryTemplate.MeshParticleMesh";
        info.create  = RD9_CLASS_FACTORY(&createMeshParticleMeshTemplate);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 0x870CA652u;
        info.name    = "dice.hfe.geom.GeometryTemplate.SkinnedMesh";
        info.create  = RD9_CLASS_FACTORY(&createSkinnedMeshTemplate);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 0x870CA653u;
        info.name    = "dice.hfe.geom.GeometryTemplate.StaticMesh";
        info.create  = RD9_CLASS_FACTORY(&createStaticMeshTemplate);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_RoadCompiledTemplate;
        info.name    = "dice.hfe.geom.GeometryTemplate.RoadCompiled";
        info.create  = RD9_CLASS_FACTORY(&createRoadCompiledTemplate);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_StaticMeshRenderer;
        info.name    = "dice.hfe.geom.StaticMeshRenderer";
        info.create  = RD9_CLASS_FACTORY(&createStaticMeshRenderer);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_SkinnedMeshRenderer;
        info.name    = "dice.hfe.geom.SkinnedMeshRenderer";
        info.create  = RD9_CLASS_FACTORY(&createSkinnedMeshRenderer);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_ParticleSystemManager;
        info.name    = "dice.hfe.geom.ParticleSystemManager";
        info.create  = RD9_CLASS_FACTORY(&createParticleSystemManager);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_HemiMapManager;
        info.name    = "dice.hfe.geom.HemiMapManager";
        info.create  = RD9_CLASS_FACTORY(&createHemiMapManager);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_DecalManager;
        info.name    = "dice.hfe.geom.DecalManager";
        info.create  = RD9_CLASS_FACTORY(&createDecalManager);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 821020u;                                             // dword_1021AEF0
        info.name    = "dice.hfe.geom.DecalShadowManager";
        info.create  = RD9_CLASS_FACTORY(&createDecalShadowManager);       // 0x100B0793
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_RoadCompiledRenderer;
        info.name    = "dice.hfe.geom.RoadCompiledRenderer";
        info.create  = RD9_CLASS_FACTORY(&createRoadCompiledRenderer);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_RoadEditableRenderer;
        info.name    = "dice.hfe.geom.RoadEditableRenderer";
        info.create  = RD9_CLASS_FACTORY(&createRoadEditableRenderer);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 0x870CAA39u;
        info.name    = "dice.hfe.geom.GeometryOptimizer.BundledMeshOptimizer";
        info.create  = RD9_CLASS_FACTORY(&createBundledMeshOptimizer);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 0x870CAA3Bu;
        info.name    = "dice.hfe.geom.GeometryOptimizer.StaticMeshOptimizer";
        info.create  = RD9_CLASS_FACTORY(&createStaticMeshOptimizer);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_RoadTemplate;
        info.name    = "dice.hfe.geom.Roads.RoadTemplate";
        info.create  = RD9_CLASS_FACTORY(&createRoadTemplate);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_RoadEditable;
        info.name    = "dice.hfe.geom.Roads.RoadEditable";
        info.create  = RD9_CLASS_FACTORY(&createRoadEditable);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 852419u;
        info.name    = "dice.hfe.geom.NametagManager";
        info.create  = RD9_CLASS_FACTORY(&createNametagManager);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_LightManager;
        info.name    = "dice.hfe.geom.LightManager";
        info.create  = RD9_CLASS_FACTORY(&createLightManager);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 0x870CAA3Eu;
        info.name    = "dice.hfe.geom.ObjectDrawer";
        info.create  = RD9_CLASS_FACTORY(&createObjectDrawer);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        // The literal, not a named constant, to match how getSingletonList below spells the same
        // id: Core/Ids.h is not included in this translation unit and SkyDome.cpp defines no
        // exported id of its own.
        info.classId = 0x870CAA3Du;
        info.name    = "dice.hfe.geom.SkyDome";
        info.create  = RD9_CLASS_FACTORY(&createSkyDome);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_LightGeometry;
        info.name    = "dice.hfe.geom.LightGeometry";
        info.create  = RD9_CLASS_FACTORY(&createLightGeometry);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 822323005u;
        info.name    = "dice.hfe.geom.UndergrowthSystem";
        info.create  = RD9_CLASS_FACTORY(&createUndergrowthSystem);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_SwiffRender;
        info.name    = "dice.hfe.geom.SwiffRender";
        info.create  = RD9_CLASS_FACTORY(&createSwiffRender);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_VideoTarget;                                     // dword_1021AF34
        info.name    = "dice.hfe.geom.VideoTarget";
        info.create  = RD9_CLASS_FACTORY(&createVideoTarget);              // 0x100B0C67
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_Video;                                           // dword_1021AF38
        info.name    = "dice.hfe.geom.Video";
        info.create  = RD9_CLASS_FACTORY(&createVideo);                    // 0x100B0CA6
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 822323012u;                                          // dword_1021AF40
        info.name    = "dice.hfe.geom.LightningManager";
        info.create  = RD9_CLASS_FACTORY(&createLightningManager);         // 0x100B0BA4
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 822323011u;                                          // dword_1021AF3C
        info.name    = "dice.hfe.geom.WeatherManager";
        info.create  = RD9_CLASS_FACTORY(&createWeatherManager);           // 0x100B0BE6
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 822323013u;                                          // dword_1021AF44
        info.name    = "dice.hfe.geom.RainManager";
        info.create  = RD9_CLASS_FACTORY(&createRainManager);              // 0x100B0C25
        out.push_back(info);
    }

    ::renddx9abi::logf("ModuleGeom::getClassList: %u of the 32 shipped classes registered",
                       out.size());
}


// getSingletonList, 0x100C7974. sixteen entries, all flags == 2, in the binary's order. The iid /
// classId pairs are read from the sub_100518D0(name, instance, iid, classId, flags) call sites.

namespace {

struct GeomSingleton
{
    const char*  name;
    IBase**      instance;
    unsigned int iid;
    unsigned int classId;
};

} // anonymous namespace

// PE 0x100C7974: the sixteen-entry table above, read in full.
void ModuleGeom::getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::geom::ModuleGeom::getSingletonList", 0x14);

    // Interface and class IDs retain their unsigned 32-bit bit patterns.
    static const GeomSingleton entries[16] =
    {
        { "LightManager",           reinterpret_cast<IBase**>(&g_LightManager),
                                                                  0xB801DCF9u,  852417u    },
        { "GeometryTemplateManager", &g_GeometryTemplateManager, 0xAD9665BCu, 0x870CA329u },
        { "StaticMeshRenderer",     &g_StaticMeshRenderer,          820013u,  821014u    },
        // iid 1 - IID_IBase - and that is what the binary passes, not a typo here.
        { "SkinnedMeshRenderer",    &g_SkinnedMeshRenderer,              1u,  821015u    },
        { "ParticleSystemManager",  &g_ParticleSystemManager,       820016u,  821017u    },
        { "HemiMapManager",         reinterpret_cast<IBase**>(&g_HemiMapManager),
                                                                       820019u,  821019u    },
        { "DecalManager",           &g_DecalManager,                820018u,  821018u    },
        { "DecalShadowManager",     &g_DecalShadowManager,          820021u,  821020u    },
        { "RoadCompiledRenderer",   &g_RoadCompiledRenderer,        820013u,  821912u    },
        { "RoadEditableRenderer",   &g_RoadEditableRenderer,        820020u,  821913u    },
        { "NametagManager",         &g_NametagManager,          0xB801DCFEu,  852419u    },
        { "SkyDome",                &g_SkyDome,                 0xB801DCFBu, 0x870CAA3Du },
        { "UndergrowthSystem",      &g_UndergrowthSystem,       0xB801DCFFu,  822323005u },
        { "LightningManager",       &g_LightningManager,        0xB801DD06u,  822323012u },
        { "WeatherManager",         &g_WeatherManager,          0xB801DD05u,  822323011u },
        { "RainManager",            &g_RainManager,             0xB801DD07u,  822323013u },
    };

    ::renddx9abi::HostVectorWriter<SingletonRegInfo> out(hostVector);

    for (int i = 0; i < 16; ++i)
    {
        SingletonRegInfo info;
        info.name     = entries[i].name;
        info.instance = entries[i].instance;
        info.iid      = entries[i].iid;
        info.classId  = entries[i].classId;
        info.flags    = 2;
        out.push_back(info);
    }
}

// PE 0x100B0DF6. The console lists are registered per translation unit natively (see below).
bool ModuleGeom::init()
{
    RD9_TRACE("dice::hfe::geom::ModuleGeom::init", 0x18);

    // PE 0x100B0DF6 first passes this TU's ConsoleObject list (unk_102543C4) to
    // ConsoleObjects::registerConsoleObjects at 0x1007B570, then registers the category-10 event
    // handler below.  Native mode shares the rebuilt host's STL ABI; retail-compatible mode cannot
    // safely publish current-toolchain ConsoleObject/container layouts to the VC7.1 host.
    registerLightManagerConsoleObjects();
    registerWeatherManagerConsoleObjects();
    registerShadowManagerConsoleObjects();
    registerHemiMapManagerConsoleObjects();
    registerGeometryTemplateConsoleObjects();
    registerSkyDomeConsoleObjects();
    registerTerrainConsoleObjects();
    registerTerrainCullerConsoleObjects();
    registerNametagManagerConsoleObjects();
    registerUndergrowthConsoleObjects();
    registerDecalManagerConsoleObjects();
    registerOverGrowthConsoleObjects();
    registerObjectGeometryConsoleObjects();
    registerStaticMeshRendererConsoleObjects();
    registerSkinnedMeshRendererConsoleObjects();
    registerParticleSystemManagerConsoleObjects();
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    registerLightmapGenerationConsoleObjects();
#endif

    // EventManager::registerEventHandler registers this module's instruction-proven one-slot
    // ECCoreEventHandler subobject (this+4) for
    // category 10, priority 0. That registration routes BF2's initMainMenuSwiff (10,1) post into
    // handleCoreModuleEvent above.
    if (eventManager)
    {
        eventManager->registerEventHandler(
            static_cast<EventCategory>(10),
            reinterpret_cast<IEventListener*>(static_cast<ECCoreEventHandler*>(this)), 0);
        ::renddx9abi::logf("ModuleGeom::init: category-10 core event handler registered "
                           "(the SwiffRender publisher)");
    }

    const char* const names[16] =
    {
        "LightManager", "GeometryTemplateManager", "StaticMeshRenderer", "SkinnedMeshRenderer",
        "ParticleSystemManager", "HemiMapManager", "DecalManager", "DecalShadowManager",
        "RoadCompiledRenderer", "RoadEditableRenderer", "NametagManager", "SkyDome",
        "UndergrowthSystem", "LightningManager", "WeatherManager", "RainManager"
    };
    IBase* const values[16] =
    {
        static_cast<IBase*>(g_LightManager), g_GeometryTemplateManager,
        g_StaticMeshRenderer, g_SkinnedMeshRenderer,
        g_ParticleSystemManager, static_cast<IBase*>(g_HemiMapManager),
        g_DecalManager, g_DecalShadowManager,
        g_RoadCompiledRenderer, g_RoadEditableRenderer, g_NametagManager, g_SkyDome,
        g_UndergrowthSystem, g_LightningManager, g_WeatherManager, g_RainManager
    };
    for (int i = 0; i < 16; ++i)
        ::renddx9abi::logf("ModuleGeom::init: %-24s %s", names[i],
                           values[i] ? "created" : "NULL (no class registered)");

    return true;
}

// PE 0x100B0E28.
bool ModuleGeom::close()
{
    RD9_TRACE("dice::hfe::geom::ModuleGeom::close", 0x1C);

    // PE 0x100B0E28 unregisters the category-10 handler, removes this TU's console list through
    // ConsoleObjects::unregisterConsoleObjects at 0x1007B6D0, then unregisters Terrain if present.
    if (eventManager)
    {
        eventManager->unregisterEventHandler(
            static_cast<EventCategory>(10),
            reinterpret_cast<IEventListener*>(static_cast<ECCoreEventHandler*>(this)));
    }
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    unregisterLightmapGenerationConsoleObjects();
#endif
    unregisterParticleSystemManagerConsoleObjects();
    unregisterSkinnedMeshRendererConsoleObjects();
    unregisterStaticMeshRendererConsoleObjects();
    unregisterObjectGeometryConsoleObjects();
    unregisterOverGrowthConsoleObjects();
    unregisterDecalManagerConsoleObjects();
    unregisterUndergrowthConsoleObjects();
    unregisterNametagManagerConsoleObjects();
    unregisterTerrainCullerConsoleObjects();
    unregisterTerrainConsoleObjects();
    unregisterSkyDomeConsoleObjects();
    unregisterGeometryTemplateConsoleObjects();
    unregisterHemiMapManagerConsoleObjects();
    unregisterShadowManagerConsoleObjects();
    unregisterWeatherManagerConsoleObjects();
    unregisterLightManagerConsoleObjects();
    if (classManager && classManager->getSingleton(renddx9abi::HostString("Terrain")))
        classManager->unregisterSingleton(renddx9abi::HostString("Terrain"));
    return true;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
