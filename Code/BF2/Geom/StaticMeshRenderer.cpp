// Static-mesh renderer singleton: vertex declarations, shadow effects, and lightmap atlases.
// [CHOSEN] Internal layout is local; the two interface tables preserve the compatibility ABI.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <string>
#include "../Core/EventManager.h"          // the ECCore registration
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "HemiMapManager/HemiMapDependencies.h"
#include "StaticMeshRenderer.h"
#include "StaticMeshTemplate.h"
#include "BundledMeshStateManager.h"
#include "XpLightManager.h"

#include "../../RendDX9Abi/PluginEntry.h"   // fileManager - the atlas builders' listing
#include "../RendDX9/TextureAtlas/TextureAtlas.h"

#include <cstddef>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <new>
#include <string>
#include <string.h>
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_shaderManager;       // ShaderManager.cpp, PE 0x1023B97C
extern void* g_rendererBase;        // RendDX9Init.cpp, the lightmap loader's +0x102 gate
}

namespace geom {

extern LightManager* g_LightManager;
extern IBase* g_StaticMeshRenderer;

// getClassList: sub_100518A0(821014, "dice.hfe.geom.StaticMeshRenderer", factory). getSingletonList
// { "StaticMeshRenderer", &g_StaticMeshRenderer, iid 820013, classId 821014, flags 2 }.
// queryInterface (0x10145610, shared MeshRendererCommon body) accepts iid 1 and 820013.
extern const unsigned int CID_StaticMeshRenderer  = 821014u;
extern const unsigned int IID_IStaticMeshRenderer = 820013u;

// The factory allocation, 0x100B068E (retail 0xA0).
static const unsigned int kStaticMeshRendererSize = sizeof(StaticMeshRenderer);

// Shared bundled-mesh shadow handles owned by StaticMeshRenderer.
dx9rend::Effect* g_bundledShadowEffect = 0;
dx9rend::ShaderParameter* g_bundledShadowAlphaThreshold = 0;
dx9rend::ShaderParameter* g_bundledTexLayer0 = 0;
dx9rend::ShaderParameter* g_bundledOneBoneSkinning = 0;
dx9rend::ShaderParameter* g_bundledViewI = 0;

namespace {

void setShadowTechnique(dx9rend::Effect* effect)
{
    dx9rend::effectSetTechniqueByName(
        effect, std::string(dx9rend::deviceCapsIsNvidia()
                                ? "DrawShadowMapNV"
                                : "DrawShadowMap"));
}

dx9rend::ShaderParameter* getEffectParameter(dx9rend::Effect* effect, const char* semantic)
{
    return dx9rend::effectGetShaderParameterTyped(effect, std::string(semantic));
}

// PE 0x10121A90 (the retail body also stores the two vtable pointers first; the object is freed
// right after, so nothing observes them)
void destroyStaticMeshRenderer(StaticMeshRenderer* self)
{
    dx9rend::vertexDeclarationRelease(self->m_decl2C);
    dx9rend::vertexDeclarationRelease(self->m_decl30);
    dx9rend::vertexDeclarationRelease(self->m_decl34);
    dx9rend::vertexDeclarationRelease(self->m_decl38);
    dx9rend::vertexDeclarationRelease(self->m_decl3C);

    if (self->m_worldViewProj) self->m_worldViewProj->release();
    if (self->m_viewI) self->m_viewI->release();
    if (self->m_paraboloid) self->m_paraboloid->release();
    if (self->m_paraboloidZ) self->m_paraboloidZ->release();
    if (self->m_parallax) self->m_parallax->release();
    if (self->m_shadowEffect) dx9rend::effectRelease(self->m_shadowEffect);

    // this non-intuitive order is shipped ordering.
    if (g_bundledTexLayer0) g_bundledTexLayer0->release();
    if (g_bundledOneBoneSkinning) g_bundledOneBoneSkinning->release();
    if (g_bundledShadowAlphaThreshold) g_bundledShadowAlphaThreshold->release();
    if (g_bundledViewI) g_bundledViewI->release();
    if (g_bundledShadowEffect) dx9rend::effectRelease(g_bundledShadowEffect);

    // PE 0x10121BD8: `mov ecx, dword_102651C0 / test ecx,ecx / call sub_101A9150 / push / engineFree`.
    // The StaticMeshStateManager is torn down directly - its member teardown followed by the raw
    // free - not through its release slot, and the singleton cell is left pointing at freed memory
    // exactly as shipped. This sits after the bundled-effect releases and before the event-manager
    // unregister and the ten cache releases.
    if (g_staticMeshStateManager)
    {
        staticMeshStateManagerDestroyMembers(g_staticMeshStateManager);
        ::renddx9abi::engineFree(g_staticMeshStateManager);
    }

    // Remove the core-event listener before releasing owned renderer resources.
    eventManager->unregisterEventHandler(
        ECCore, reinterpret_cast<IEventListener*>(&self->m_ehVptr));

    // each of the ten lightmap atlases is released
    // through its own vtable +0x08 (TextureAtlas release, 0x10017410), in order and with NO null
    // test, exactly as shipped. Shutdown only - the map-change path never destroys this singleton.
    for (int i = 0; i < 10; ++i)
    {
        dx9rend::TextureAtlas* const atlas = self->m_lightmapAtlases[i];
        typedef int (__thiscall* AtlasReleaseFn)(void*);
        reinterpret_cast<AtlasReleaseFn>(
            static_cast<void**>(atlas->vptr)[0x08 / 4])(atlas);
    }
    self->m_name.~basic_string();
}

// Primary spine - refcount at +0x28.
int RD9_THUNK smr_addRef(void* self RD9_THUNK_EDX) { return ++static_cast<StaticMeshRenderer*>(self)->m_refCount; }
int RD9_THUNK smr_getRc (void* self RD9_THUNK_EDX) { return   static_cast<StaticMeshRenderer*>(self)->m_refCount; }
// PE 0x10123980
int RD9_THUNK smr_release(void* self RD9_THUNK_EDX)
{
    StaticMeshRenderer* p = static_cast<StaticMeshRenderer*>(self);
    const int remaining = --p->m_refCount;
    if (remaining == 0)
    {
        destroyStaticMeshRenderer(p);
        ::renddx9abi::engineFree(self);
    }
    return remaining;
}
// accepts iid 1 and 820013, returns this.
void* RD9_THUNK smr_qi(void* self RD9_THUNK_EDX, unsigned int iid)
{
    if (iid == RD9_IID_IBASE || iid == IID_IStaticMeshRenderer)
        return self;
    return 0;
}
// +0x10 nullsub_53 (empty), +0x14 `return 0`.
void RD9_THUNK smr_null10(void* RD9_THUNK_EDX) { }
int  RD9_THUNK smr_ret0  (void* RD9_THUNK_EDX) { return 0; }
// +0x18 setEnabled (byte store at +0x04), +0x1C getEnabled.
void RD9_THUNK smr_setEnabled(void* self RD9_THUNK_EDX, int v)
{ static_cast<StaticMeshRenderer*>(self)->setEnabled(static_cast<unsigned char>(v)); }
int  RD9_THUNK smr_getEnabled(void* self RD9_THUNK_EDX)
{ return static_cast<StaticMeshRenderer*>(self)->getEnabled(); }
// +0x24 getName - return the address of the embedded name string (this+0x08).
void* RD9_THUNK smr_getName(void* self RD9_THUNK_EDX) { return &static_cast<StaticMeshRenderer*>(self)->m_name; }

// PE 0x10121A10 tail-jumps to basic_string::operator=(const basic_string&).
std::string& RD9_THUNK smr_setName(void* self RD9_THUNK_EDX, const std::string& name)
{
    return static_cast<StaticMeshRenderer*>(self)->m_name = name;
}


// Releases loaded lightmap-atlas pages on CEUnloadLevel so the next level reloads them.
// Requires all ten atlas objects; ignores other event categories and ids.

// PE 0x10121A50
void RD9_THUNK smr_handleEvent(void* self RD9_THUNK_EDX, int category, unsigned int id, void*, void*)
{
    if (category != 5 || id != 13)                                     // ECCore / CEUnloadLevel
        return;

    StaticMeshRenderer* const renderer = reinterpret_cast<StaticMeshRenderer*>(
        static_cast<unsigned char*>(self) - offsetof(StaticMeshRenderer, m_ehVptr));   // +0x24
    for (unsigned int i = 0; i < 10u; ++i)
    {
        dx9rend::TextureAtlas* const atlas = renderer->m_lightmapAtlases[i];   // [esi]
        if (dx9rend::textureAtlasEntryCount(atlas) > 0u)                       // [ecx+10h], jbe
            dx9rend::textureAtlasReleaseAll(atlas);
    }
}

const void* const kPrimaryVtable[10] =
{
    reinterpret_cast<void*>(&smr_addRef),        // +0x00
    reinterpret_cast<void*>(&smr_getRc),         // +0x04
    reinterpret_cast<void*>(&smr_release),       // +0x08
    reinterpret_cast<void*>(&smr_qi),            // +0x0C
    reinterpret_cast<void*>(&smr_null10),        // +0x10  nullsub_53
    reinterpret_cast<void*>(&smr_ret0),          // +0x14  return 0
    reinterpret_cast<void*>(&smr_setEnabled),    // +0x18  setEnabled([+4])
    reinterpret_cast<void*>(&smr_getEnabled),    // +0x1C  getEnabled
    reinterpret_cast<void*>(&smr_setName),       // +0x20  setName
    reinterpret_cast<void*>(&smr_getName),       // +0x24  getName -> this+8
};

const void* const kEventHandlerVtable[1] =
{
    reinterpret_cast<void*>(&smr_handleEvent),   // +0x00  handleECCoreEvent
};

} // anonymous namespace

void bundledMeshSetHemiEnabled(unsigned char enabled)
{
    // PE 0x1012B170/0x1012B460 dereference dword_102597F4 without a guard and call +0x10. The
    // singleton is the template-owned BundledMeshStateManager, not a separate Hemi object.
    g_bundledMeshStateManager->setEnabled(enabled != 0);
}

void staticMeshRendererSetHemiEnabled(unsigned char enabled)
{
    // Intentionally unguarded: PE calls singleton slot +0x18 without checking it.
    static_cast<StaticMeshRenderer*>(static_cast<void*>(g_StaticMeshRenderer))->setEnabled(enabled);
}

namespace {
// PE 0x101220F0: geom_StaticMeshRenderer_ctor. In the shipped order: the name, the vtables and the
// reference count; the lightmap switches; the ten lightmap atlases; the StaticMeshStateManager
// singleton; the enabled byte; the five vertex declarations; the core-event registration; the
// StaticMesh effect with its LightManager registration, technique and retained parameters; and the
// first-use BundledMesh effect with its own.
void constructStaticMeshRenderer(StaticMeshRenderer* self)
{
    self->m_vptr = const_cast<const void**>(kPrimaryVtable);
    new (&self->m_name) std::string();
    self->m_ehVptr = const_cast<const void**>(kEventHandlerVtable);
    self->m_refCount = 1;
    self->m_pad05[0] = 0; self->m_pad05[1] = 0; self->m_pad05[2] = 0;

    self->m_flags60[0] = 1;
    self->m_flags60[1] = 0;
    self->m_flags60[2] = 1;
    self->m_flags60[3] = 0;
    self->m_flags60[4] = 1;
    self->m_byte94  = 0;
    self->m_dword98 = 9;
    self->m_dword9C = 0;

    // PE 0x10122159..0x1012218C - `10 times { engineAlloc(0x14); sub_10015E60 }`: the ten
    // lightmap TextureAtlas objects at +0x68..+0x8C, constructed in order.  An allocation failure
    // stores a null, as shipped.  ; see the header for the identification.
    for (unsigned int i = 0; i < 10u; ++i)
    {
        void* const storage = ::renddx9abi::engineAlloc(sizeof(dx9rend::TextureAtlas));
        self->m_lightmapAtlases[i] = storage ? dx9rend::textureAtlasCtor(storage) : 0;
    }

    // PE 0x1012218C..0x101221B5, immediately after the ten engineAlloc(0x14) cache objects and
    // before the vertex declarations: the StaticMeshStateManager singleton (retail 0x38 bytes) is
    // created lazily, and an allocation failure publishes a NULL singleton rather than leaving the
    // cell alone.
    if (!g_staticMeshStateManager)
    {
        void* const storage = ::renddx9abi::engineAlloc(sizeof(StaticMeshStateManager));
        g_staticMeshStateManager = storage ? staticMeshStateManagerConstruct(storage) : 0;
    }
    self->m_enabled = 1;

    static const D3DVERTEXELEMENT9 kStaticDecl[] =
    {
        {0, 0,  D3DDECLTYPE_SHORT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 8,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0},
        {0, 12, D3DDECLTYPE_SHORT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    static const D3DVERTEXELEMENT9 kLightMapDecl[] =
    {
        {0, 0,  D3DDECLTYPE_SHORT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 8,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0},
        {0, 12, D3DDECLTYPE_SHORT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
        {1, 0,  D3DDECLTYPE_SHORT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    static const D3DVERTEXELEMENT9 kFloatStreamDecl[] =
    {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    static const D3DVERTEXELEMENT9 kShortStreamDecl[] =
    {
        {0, 0, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0, D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };

    // the shipped creation order is +0x2C, +0x34, +0x30
    self->m_decl2C = dx9rend::vertexDeclarationCreate(kStaticDecl);
    self->m_decl34 = dx9rend::vertexDeclarationCreate(kStaticDecl);
    self->m_decl30 = dx9rend::vertexDeclarationCreate(kLightMapDecl);
    self->m_decl38 = dx9rend::vertexDeclarationCreate(kFloatStreamDecl);
    self->m_decl3C = dx9rend::vertexDeclarationCreate(kShortStreamDecl);

    // Register the core-event listener before acquiring shadow effects.
    eventManager->registerEventHandler(
        ECCore, reinterpret_cast<IEventListener*>(&self->m_ehVptr), 0);

    self->m_shadowEffect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
        dx9rend::g_shaderManager, "mods/xpack/Shaders/StaticMesh"));
    LightManager* const lightManager = g_LightManager;
    lightManager->registerShadowEffect(
        static_cast<int>(0x870CA653u),
        self->m_shadowEffect);
    if (lightManager->getDynamicShadowsEnabled())
        setShadowTechnique(self->m_shadowEffect);
    self->m_worldViewProj = getEffectParameter(self->m_shadowEffect, "WorldViewProjection");
    self->m_viewI         = getEffectParameter(self->m_shadowEffect, "ViewI");
    self->m_paraboloid    = getEffectParameter(self->m_shadowEffect, "ParaboloidValues");
    self->m_paraboloidZ   = getEffectParameter(self->m_shadowEffect, "ParaboloidZValues");
    self->m_parallax      = getEffectParameter(self->m_shadowEffect, "PARALLAXSCALEBIAS");

    if (!g_bundledShadowEffect)
    {
        g_bundledShadowEffect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
            dx9rend::g_shaderManager, "mods/xpack/Shaders/BundledMesh"));
        lightManager->registerShadowEffect(
            static_cast<int>(0x870CA651u),
            g_bundledShadowEffect);
        if (lightManager->getDynamicShadowsEnabled() && g_bundledShadowEffect)
            setShadowTechnique(g_bundledShadowEffect);
        g_bundledShadowAlphaThreshold =
            getEffectParameter(g_bundledShadowEffect, "SHADOWALPHATHRESHOLD");
        g_bundledTexLayer0 = getEffectParameter(g_bundledShadowEffect, "TEXLAYER0");
        g_bundledOneBoneSkinning =
            getEffectParameter(g_bundledShadowEffect, "matONEBONESKINNING");
        g_bundledViewI = getEffectParameter(g_bundledShadowEffect, "ViewI");
    }
}
} // anonymous namespace


// StaticMeshRenderer_tryLoadLightmapTexture, PE 0x10121F30.  Attested to
// this file's line 355 by its own diagnostic.  See the header for the contract.

// The base byte at +0x102 (g_rendererBase) gates only the diagnostic - BF2Render.cpp's step
// 14 tests the same byte - the flag clear happens either way.  The page texture is addRef'd
// through its vtable slot +0x08, which for the shared three-slot texture table IS addRef
// (Texture.h: "{getRef-at+4, nullsub, addRef-at+4}" - slot +0x08 bumps the count at +0x04).

bool staticMeshRendererTryLoadLightmapTexture(StaticMeshRenderer* self,
                                              const std::string& directory,
                                              const std::string& objectName,
                                              float outOffset[4],
                                              dx9rend::EngineTexture** outTexture)
{
    dx9rend::TextureAtlas* const atlas = self->m_lightmapAtlases[0];
    if (dx9rend::textureAtlasEntryCount(atlas) == 0)
    {
        std::string taiPath(directory);
        taiPath += "LightmapAtlas.tai";
        if (!dx9rend::textureAtlasLoadTAI(atlas, taiPath))              // TextureAtlas_loadTAI
        {
            self->m_flags60[4] = 0;
            const dx9rend::RendDX9Base* const base =
                static_cast<const dx9rend::RendDX9Base*>(dx9rend::g_rendererBase);
            if (!base->flag102)
            {
                ::renddx9abi::logf("StaticMeshRenderer.cpp:355 No lightmapatlas found, no "
                                   "lightmaps will be used.");
            }
            return false;
        }
        ++self->m_atlasesLoaded;
    }

    std::string name(directory);
    name += objectName;
    name += ".dds";
    dx9rend::pathToLower(name);

    dx9rend::TextureAtlasEntry* const entry = dx9rend::textureAtlasFind(atlas, name);
    if (!entry)
        return false;

    outOffset[0] = entry->width;
    outOffset[1] = entry->height;
    outOffset[2] = entry->uOffset;
    outOffset[3] = entry->vOffset;
    *outTexture  = entry->page;
    dx9rend::textureAddRef(entry->page);                                 // page vslot +0x08
    return true;
}


namespace {


// The part after the last '\\' or '/', or the whole path when neither occurs.
std::string lightmapBaseName(const std::string& path)
{
    int cut = static_cast<int>(path.rfind('\\'));
    const int forward = static_cast<int>(path.rfind('/'));
    if (forward > cut)
        cut = forward;
    if (cut == -1)
        return path;
    return path.substr(static_cast<std::string::size_type>(cut) + 1);
}

// The base name without its last '.' and what follows it.
std::string lightmapStem(const std::string& path)
{
    const std::string base = lightmapBaseName(path);
    const std::string::size_type dot = base.rfind('.');
    if (dot == std::string::npos)
        return base;
    return base.substr(0, dot);
}

// Appends directory + name when the listed file `name` is one of the lightmaps both atlas builders
// pack (see StaticMeshRenderer.h).
void collectLightmapFile(const std::string& name, unsigned int size, const std::string& directory,
                         const std::string& extension, std::list<std::string>& files)
{
    if (size == 0)
        return;
    if (name.find(extension.c_str(), 0, extension.size()) == std::string::npos)
        return;
    const std::string stem = lightmapStem(name);
    const std::string base = lightmapBaseName(name);
    if (stem.find("LightmapAtlas", 0, 13) != std::string::npos)
        return;
    std::string expected(stem);
    expected += extension;
    if (base.compare(expected) != 0)
        return;
    std::string path(directory);
    path += name.c_str();
    files.push_back(path);
}

// The builders' shared prologue: `directory` with a trailing '/' unless it already ends in a
// separator, and the lightmap files listed under it (recursively, in the FileManager's order).
void collectLightmapFiles(const std::string& directory, const std::string& extension,
                          std::string& normalised, std::list<std::string>& files)
{
    normalised = directory;
    if (normalised.size() != 0)
    {
        const char last = normalised.c_str()[normalised.size() - 1];
        if (last != '\\' && last != '/')
            normalised += std::string("/");
    }
    const std::string ext(extension.c_str(), extension.size());
    std::list<io::FileInfo> listed;
    fileManager->findFiles(std::string(normalised.c_str(), normalised.size()), true, listed);   // +0x34
    for (std::list<io::FileInfo>::const_iterator it = listed.begin(); it != listed.end(); ++it)
        collectLightmapFile(it->name, it->size, normalised, ext, files);
}

// The batch limit: a batch's command line stays below this many characters.
const unsigned int kAtlasCommandLineLimit = 32300u;

} // anonymous namespace

// PE 0x10123510 (the file collection is inlined: 'LightmapAtlas' stem filter of length 13, then
// the ' ERROR '-default Editor_LevelSavePath and textureAtlasBuild)
bool staticMeshRendererLoadAllLightMapFilesDll(StaticMeshRenderer* self,
                                               const std::string& directory,
                                               const std::string& extension)
{
    std::string normalised;
    std::list<std::string> files;
    collectLightmapFiles(directory, extension, normalised, files);

    std::string savePath;
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", " ERROR ", savePath);
    std::string atlasName(normalised);
    atlasName += "LightmapAtlas";
    return dx9rend::textureAtlasBuild(self->m_lightmapAtlases[0], savePath, atlasName, savePath,
                                      files, false);
}

// PE 0x10122C90: lists the lightmaps under the directory and generates the atlas batches, one
// dice_TextureAtlasTool command line of at most 32300 characters per batch (the command text is built
// and discarded, as shipped); false as soon as one batch fails.
bool staticMeshRendererLoadAllLightMapFiles(StaticMeshRenderer* self,
                                            const std::string& directory,
                                            const std::string& extension)
{
    std::string normalised;
    std::list<std::string> files;
    collectLightmapFiles(directory, extension, normalised, files);

    std::string savePath;
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", " ERROR ", savePath);
    std::list<std::string>::const_iterator next = files.begin();
    for (int batch = 0; ; ++batch)
    {
        std::list<std::string> batchFiles;
        char number[256];
        std::sprintf(number, "%d", batch);
        std::string command("dice_TextureAtlasTool -maxres 2048 -o ");
        command += normalised.c_str();
        command += "LightmapAtlas";
        command += number;
        command += " -output_path ";
        command += savePath.c_str();
        command += " -input_path ";
        command += savePath.c_str();
        command += " ";
        while (next != files.end() && command.size() < kAtlasCommandLineLimit)
        {
            command += next->c_str();
            command += " ";
            batchFiles.push_back(*next);
            ++next;
        }

        std::string atlasName(normalised);
        atlasName += "LightmapAtlas";
        std::string batchName(atlasName);
        batchName += number;
        if (!dx9rend::textureAtlasGenerate(self->m_lightmapAtlases[0], savePath, batchName, savePath,
                                           batchFiles, false))
            return false;
        if (next == files.end())
            return true;
    }
}

// RefFactory_geom_StaticMeshRenderer, 0x100B068E.
IBase* __fastcall createStaticMeshRenderer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* storage = ::renddx9abi::engineAlloc(kStaticMeshRendererSize);
    if (!storage)
        return 0;
    ::memset(storage, 0, kStaticMeshRendererSize);
    StaticMeshRenderer* self = static_cast<StaticMeshRenderer*>(storage);
    constructStaticMeshRenderer(self);

    void* itf = smr_qi(self RD9_THUNK_ARG, iid);
    if (!itf)
        smr_release(self RD9_THUNK_ARG);
    return static_cast<IBase*>(itf);
}

namespace {

// [INFERRED] console placement. The native split keeps the ten properties before the atlas rows.
const char kStaticMeshRendererPropertiesTag[] = "geom.staticMeshRenderer.properties";

// PE 0x100B0D43: every activity test and execution looks the singleton up by name.
StaticMeshRenderer* consoleStaticMeshRenderer()
{
    IBase* const found = classManager->getSingleton(::renddx9abi::HostString("StaticMeshRenderer"));
    return static_cast<StaticMeshRenderer*>(static_cast<void*>(found));
}

bool getConsoleStaticRendering()
{
    StaticMeshRenderer* const self = consoleStaticMeshRenderer();
    typedef int (__thiscall* Fn)(void*);
    return reinterpret_cast<Fn>(const_cast<void*>(self->m_vptr[0x1C / 4]))(self) != 0;
}

void setConsoleStaticRendering(bool value)
{
    StaticMeshRenderer* const self = consoleStaticMeshRenderer();
    typedef void (__thiscall* Fn)(void*, int);
    reinterpret_cast<Fn>(const_cast<void*>(self->m_vptr[0x18 / 4]))(self, value);
}

int getConsoleMaxLightMappedLod() { return static_cast<int>(consoleStaticMeshRenderer()->m_dword98); }
void setConsoleMaxLightMappedLod(int value)
{
    consoleStaticMeshRenderer()->m_dword98 = static_cast<unsigned int>(value);
}

bool getConsoleAllowAnisoFiltering()
{
    return *reinterpret_cast<const unsigned char*>(&consoleStaticMeshRenderer()->m_dword9C) != 0;
}

void setConsoleAllowAnisoFiltering(bool value)
{
    const unsigned char byte = value;
    ::memcpy(&consoleStaticMeshRenderer()->m_dword9C, &byte, sizeof(byte));
}

// PE 0x10121C30. [INFERRED] name; ECX carries the string result, not a renderer receiver.
std::string __fastcall convertStaticMeshLightmapName(std::string name)
{
    const std::string::size_type underscore = name.rfind('_');
    if (underscore == std::string::npos)
        return name;

    const unsigned int first = static_cast<unsigned int>(underscore) + 1u;
    unsigned int cursor = first;
    if (name.c_str()[cursor] == '-')
        ++cursor;
    while (name.c_str()[cursor] != 0 && name.c_str()[cursor] != '-')
        ++cursor;
    if (name.c_str()[cursor] == 0)
        return name;

    const unsigned int second = ++cursor;
    if (name.c_str()[cursor] == '-')
        ++cursor;
    while (name.c_str()[cursor] != 0 && name.c_str()[cursor] != '-')
        ++cursor;
    if (name.c_str()[cursor] == 0)
        return name;

    const unsigned int third = ++cursor;
    if (name.c_str()[cursor] == '-')
        ++cursor;
    while (std::isdigit(name.c_str()[cursor]))
        ++cursor;
    if (name.c_str()[cursor] == 0)
        return name;

    if (static_cast<int>(first) < static_cast<int>(second)
        && static_cast<int>(second) < static_cast<int>(third)
        && static_cast<int>(third) < static_cast<int>(cursor))
    {
        const int x = std::atoi(name.substr(first, second - first).c_str());
        const int y = std::atoi(name.substr(second, third - second).c_str());
        const int z = std::atoi(name.substr(third, cursor - third).c_str());
        // shipped defect: both writes are unbounded in the 24-byte buffer.
        char converted[24];
        std::sprintf(converted, "=00=%d=%d=%d", x, y, z);
        ::strcat(converted, name.substr(cursor).c_str());
        return name.substr(0, first - 1u) + converted;
    }
    return name;
}

BF2_CONSOLE_OBJECT("StaticMeshRenderer", 2, 0)

// PE 0x100BA64D.
// PE 0x100BA58F, 0x100BA600: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(enableRendering, bool, consoleStaticMeshRenderer() != 0,
                        getConsoleStaticRendering(), setConsoleStaticRendering,
                        kStaticMeshRendererPropertiesTag)
// PE 0x100BA760 / 0x100BA868 / 0x100BA96A.
// PE 0x100BA6A2, 0x100BA713: constructor, argument conversion.
BF2_CONSOLE_FIELD(noLods, bool, consoleStaticMeshRenderer(), m_byte94, kStaticMeshRendererPropertiesTag)
// PE 0x100BA7AA, 0x100BA81B: constructor, argument conversion.
BF2_CONSOLE_FIELD(materialCullingEnabled, bool, consoleStaticMeshRenderer(), m_flags60[0], kStaticMeshRendererPropertiesTag)
// PE 0x100BA8AC, 0x100BA91D: constructor, argument conversion.
BF2_CONSOLE_FIELD(enableLightMapAtlas, bool, consoleStaticMeshRenderer(), m_flags60[4], kStaticMeshRendererPropertiesTag)

// setArgFromString keeps one persistent std::string token. The executor copies the argument before
// publishing the return flag.
class StaticMeshConvertLightmapNameBinding : public ConsoleObjectBaseImpl
{
public:
    // PE 0x100BA9AE: binding constructor.
    StaticMeshConvertLightmapNameBinding()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "convertLightmapName";
        m_type = 0;
        m_minNoArgs = m_maxNoArgs = 1;
        m_returnType = "std::string";
        m_hasReturnValue = false;
        m_isDynamic = true;
        m_argTypes[0] = "std::string";
        m_argNeedsConversion[0] = 0;
        consoleBindingList(kStaticMeshRendererPropertiesTag).push_back(this);
    }
    virtual bool isObjectActive() const { return consoleStaticMeshRenderer() != 0; }
    // PE 0x100BAA2B
    virtual void executeObjectMethod()
    {
        if (m_argCount != 1)
            return;
        const std::string value(BF2_CC_ARG(std::string, 0));
        m_hasReturnValue = true;
        consoleStaticMeshRenderer();
        m_returnValue = convertStaticMeshLightmapName(value);
    }
    // PE 0x100BAA93
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};

StaticMeshConvertLightmapNameBinding s_staticMeshConvertLightmapNameBinding;

// PE 0x100BABC6 / 0x100BACD4.
// PE 0x100BAB08, 0x100BAB79: constructor, argument conversion.
BF2_CONSOLE_FIELD(enableNewLightmapNames, bool, consoleStaticMeshRenderer(), m_flags60[2], kStaticMeshRendererPropertiesTag)
// PE 0x100BAC0A, 0x100BAC87: constructor, argument conversion.
BF2_CONSOLE_FIELD(enableUnifiedIndexlist, bool, consoleStaticMeshRenderer(), m_flags60[3], kStaticMeshRendererPropertiesTag)
// PE 0x100BADD6.
// PE 0x100BAD18, 0x100BAD89, 0x100BFEDA: constructor, argument conversion, result formatting.
BF2_CONSOLE_PROPERTY_FN(maxLightMappedLod, int, consoleStaticMeshRenderer() != 0,
                        getConsoleMaxLightMappedLod(), setConsoleMaxLightMappedLod,
                        kStaticMeshRendererPropertiesTag)
// PE 0x100BAEDE / 0x100BAFE0. The latter accesses only the low byte at +0x9C.
// PE 0x100BAE20, 0x100BAE91: constructor, argument conversion.
BF2_CONSOLE_FIELD(enableLightIndexes, bool, consoleStaticMeshRenderer(), m_flags60[1], kStaticMeshRendererPropertiesTag)
// PE 0x100BAF22, 0x100BAF93: constructor, argument conversion.
BF2_CONSOLE_PROPERTY_FN(allowAnisoFiltering, bool, consoleStaticMeshRenderer() != 0,
                        getConsoleAllowAnisoFiltering(), setConsoleAllowAnisoFiltering,
                        kStaticMeshRendererPropertiesTag)

} // anonymous namespace

// Native per-file publication; ModuleGeom's original list also contained the two atlas rows.
bool registerStaticMeshRendererPropertyConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(consoleBindingList(kStaticMeshRendererPropertiesTag));
}

bool unregisterStaticMeshRendererPropertyConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(consoleBindingList(kStaticMeshRendererPropertiesTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
