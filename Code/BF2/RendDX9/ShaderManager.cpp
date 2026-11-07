// Path:     attested - "C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\RendDX9\ShaderManager.cpp",
//           quoted with lines 950 and 980, both "Undefined texture filter quality."
// Contents: shader cache, compiler defines and device/file-change notifications.

// See Code/BF2/RendDX9/BF2Render.cpp for the BF2_IMAGE_RENDDX9 guard and why it is here.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../Core/StringUtil.h"         // extractFileExt (sub_10075180)
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include <string>
#include "../../RendDX9Abi/Frontier.h"
#include "RendDX9Base.h"
#include "ShaderLoading.h"
#include "DiceEffect.h"
#include "../RendMisc/IRenderer.h"
#include "../GeomMisc/Debug.h"
#include "../Core/SettingsRepostitory.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/EngineFile.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"

#include "../IO/File/FileManager.h"
#include "../IO/File/FileChangeManager.h"

#include <map>
#include <new>
#include <set>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace dice {
namespace hfe {

extern IBase* fileChangeManager;

namespace dx9rend {

extern void* g_deviceCaps;
void rendererPatchModPath(std::string& path);
extern void* g_rendererBase;   // RendDX9Init.cpp, dword_1023B970 - the two A2M-mask reads below
namespace Ra {
extern void* g_effectManager;
unsigned char effectManagerFlag40();   // Ra/EffectManager.cpp - its +0x40 byte
}

namespace {

// [CHOSEN, not recovered] ABI adapter for the two recovered FileManager calls at
// PE 10022F17 and 10022653. The retail table uses +90 and a VC7.1 string return.
// Native IFileSystem has an inherited destructor before FileManager's additions;
// its +90 is hasMountedArchives, so native calls must use the real declaration.
// [RETIRE-WHEN: RW-49] Keep the retail branch and its string carrier unchanged.
renddx9abi::HostString stripShaderModPath(const char* path)
{
    renddx9abi::HostString in(path);
    return static_cast<io::FileManager*>(::dice::hfe::fileManager)->stripModPath(in);
}

struct CapsView { D3DCAPS9 caps; D3DADAPTER_IDENTIFIER9 adapter; int gpuFamily; };

const CapsView* capsOf() { return static_cast<const CapsView*>(g_deviceCaps); }

// The caps accessors below are file-local copies of the retail DeviceCaps readers, which have no null
// guard (PE 0x100011C0 vendor == 0x10DE, 0x10001180 MaxAnisotropy, 0x10001190 MAGFANISOTROPIC bit 26,
// 0x100011A0 MINFANISOTROPIC bit 10). The null checks here are the existing [CHOSEN, not recovered]
// omission of a null dereference before DeviceCaps is published.
bool isNvidia()          { const CapsView* c = capsOf(); return c && c->adapter.VendorId == 4318u; }
bool isAti()             { const CapsView* c = capsOf(); return c && c->adapter.VendorId == 4098u; }

// The shader-major readers are RendDX9Init.cpp's deviceCapsVertexShaderMajor (retail sub_100010B0)
// and deviceCapsPixelShaderMajor (retail sub_100010D0); the calls below use them directly.

// PE 0x10001250.
int  gpuFamily()         { return capsOf()->gpuFamily; }
// PE 0x10001180.
unsigned int maxAniso()  { const CapsView* c = capsOf(); return c ? c->caps.MaxAnisotropy : 0u; }
// PE 0x100011A0.
bool minAnisoOk()        { const CapsView* c = capsOf(); return c && (c->caps.TextureFilterCaps & 0x400u) != 0u; }
// PE 0x10001190.
bool magAnisoOk()        { const CapsView* c = capsOf(); return c && (c->caps.TextureFilterCaps & 0x4000000u) != 0u; }

// PE sub_1000A6B0 on dword_1023B970, is multisampling active: the forced-on byte
// +0x514 wins, then the forced-off +0x515, else the first entry of the multisample list (+0xC0 /
// +0xC4, 8-byte descriptors) has a type above zero. The same file-local copy BF2Render.cpp,
// TerrainDraw.cpp and FSQuadDrawer.cpp carry. Both +0x514 / +0x515 and the list are written by
// VideoOptions' resolution change (sub_1000F8D0 and its probe sub_1000A700), so this is no longer
// constant false.
bool isMultisampleActive()
{
    const RendDX9Base* const base = static_cast<const RendDX9Base*>(g_rendererBase);
    if (base->forceMultisample)
        return true;
    if (base->disableMultisample)
        return false;
    return multiSampleListActive(base);
}

// The transparency-AA code the "%X" formatter reads - the dword at 0x1023D090 / 0x1023D094.
unsigned int transparencyAACode(const unsigned char (&code)[4])
{
    unsigned int value;
    ::memcpy(&value, code, sizeof(value));
    return value;
}

struct ShaderDefine { const char* name; const char* value; };

#include "ShaderManagerDefines.inc"

extern void* const kShaderManagerVtable[5];
extern void* const kShaderManagerIncludeVtable[2];
extern void* const kShaderManagerListenerVtable[3];

} // anonymous namespace


// Shader cache, macro state and include/device-listener interfaces.
// Preserve the three interface subobjects and retained device reference.

class Effect;

struct ShaderManager
{
    // Install the interfaces and initial reference before container construction.
    ShaderManager()
        : vptr(const_cast<void**>(kShaderManagerVtable))
        , vptr2(const_cast<void**>(kShaderManagerIncludeVtable))
        , vptrDeviceLost(const_cast<void**>(kShaderManagerListenerVtable))
        , refCount(1), device(0)
        , flag78(0), flag79(0), flag7A(0), flag7B(0), flag7C(0), flag7D(0), flag7E(1), flag7F(0)
        , flag80(1), flags84(0), currentEffect98(0), statesEffect9C(0), rawEffectA0(0)
        , disableStateManagerA4(0)
    {
        // PE 0x10024A40: native member initialization precedes device retention below.
        pad81[0] = pad81[1] = pad81[2] = 0;
        padA5[0] = padA5[1] = padA5[2] = 0;
    }

    void*         vptr;
    void*         vptr2;
    void*         vptrDeviceLost;
    int           refCount;                     // +0x0C
    void*         device;                       // +0x10
    // The lowered, mod-stripped effect name -> its Effect wrapper (strcmp order).
    std::map<std::string, Effect*> effectCache;   // +0x14  node 0x30
    // The other five containers are constructed with it, as 0x10024A40 does (sub_10024D40 for the
    // four 0x30-node maps, sub_10024E70 for the 0x14-node one, sub_1001BA10 for the 0x48-node one),
    // because the device-lost listener walks +0x48 unguarded. Its generated-effect factory
    // stops at native effect-pool support; cache members retain their positional names.
    std::map<std::string, Effect*> cache20;    // +0x20  borrowed, normalized effect aliases
    std::string  name;                         // +0x2C
    // Owned generated effects and their borrowed, normalized reload aliases.
    std::map<std::string, DiceEffect*> map48;  // +0x48
    std::map<std::string, DiceEffect*> map54;  // +0x54
    std::set<IUnknown*> map60;                 // +0x60  D3DXEffectPool*, PE 0x10023231..0x10023252
    std::map<std::string, std::string> map6C; // +0x6C  node 0x48 - two strings per node
    unsigned char flag78;                       // +0x78  isNvidia, written at the very end
    unsigned char flag79;
    unsigned char flag7A;                       // +0x7A  -> flags |= 0x20
    unsigned char flag7B;                       // +0x7B  -> if zero, byte at +0x85 |= 1
    unsigned char flag7C;                       // +0x7C  -> flags |= 4
    unsigned char flag7D;
    unsigned char flag7E;                       // ctor: 1
    unsigned char flag7F;                       // +0x7F  -> debugterrain
    unsigned char flag80;                       // ctor: 1
    unsigned char pad81[3];
    unsigned int  flags84;                      // +0x84
    // The flat define list: name, value, name, value, ...
    std::vector<std::string> defines;          // +0x88
    // Current wrapper, states effect, and raw effect used for pre-draw CommitChanges.
    // A nonzero disable flag bypasses the shared StateManager.
    void*         currentEffect98;
    DiceEffect*   statesEffect9C;
    void*         rawEffectA0;
    unsigned char disableStateManagerA4;
    unsigned char padA5[3];
};

void* g_shaderManager = 0;   // dword_1023B97C

extern IBase* g_Renderer;    // BF2Render.cpp - renderDevice() reaches the device through it

// Effect.cpp
class Effect;
Effect* createEffectObject();
bool effectBind(Effect* self, void* d3dxEffect);
void* effectD3dxHandle(void* effect);   // wrapper+0x08, the ID3DXEffect* (the listener walks)
const std::string& effectName(void* effect);   // wrapper+0x0C, the effect's name
void deobfuscateShader(unsigned char* buf, unsigned int size);   // the include Open uses it too

namespace {

bool shaderManagerReloadEffect(ShaderManager* sm, const std::string& path);
bool shaderManagerReloadDiceEffect(ShaderManager* sm, const std::string& path);

// PE 0x10132630 / 0x10051710.
int RD9_THUNK sm_addRef(void* self RD9_THUNK_EDX)
{
    return ++static_cast<ShaderManager*>(self)->refCount;
}
int RD9_THUNK sm_getRef(void* self RD9_THUNK_EDX)
{
    return static_cast<ShaderManager*>(self)->refCount;
}

// PE 0x100248E0, the destructor body: leave the file-change manager, then destroy the containers
// in reverse member order. Cached effects belong to the destroy broadcast; this destructor does
// not release the device.
void destructShaderManager(ShaderManager* sm)
{
    static_cast<io::IFileChangeManager*>(fileChangeManager)->removeListener(
        reinterpret_cast<io::IFileChangeListener*>(sm));
    sm->~ShaderManager();
}

// PE 0x10025950.
int RD9_THUNK sm_release(void* self RD9_THUNK_EDX)
{
    ShaderManager* const sm = static_cast<ShaderManager*>(self);
    const int remaining = --sm->refCount;
    if (!remaining)
    {
        destructShaderManager(sm);
        ::renddx9abi::engineFree(sm);
    }
    return remaining;
}

// PE 0x10025930.
void* RD9_THUNK sm_queryInterface(void* self RD9_THUNK_EDX, unsigned int iid)
{
    return iid == RD9_IID_IBASE ? self : 0;
}

// PE 0x10023690. Only the two recognized effect extensions trigger reloads.
bool shaderManagerReloadPath(ShaderManager* sm, const std::string& path)
{
    bool matches;
    {
        std::string extension;
        extension = extractFileExt(path);
        matches = extension == "fx";
    }
    if (matches)
    {
        return shaderManagerReloadEffect(sm, path);
    }
    {
        std::string extension;
        extension = extractFileExt(path);
        matches = extension == "dfx";
    }
    if (matches)
        return shaderManagerReloadDiceEffect(sm, path);
    return false;
}

// PE 0x10024870. The second callback argument is ignored.
void RD9_THUNK sm_onFileChange(void* self RD9_THUNK_EDX, const std::string& path,
                              unsigned int)
{
    shaderManagerReloadPath(static_cast<ShaderManager*>(self), path);
}

// PE 0x101DFE64. The primary face is also the file-change listener.
void* const kShaderManagerVtable[5] =
{
    reinterpret_cast<void*>(&sm_addRef),
    reinterpret_cast<void*>(&sm_getRef),
    reinterpret_cast<void*>(&sm_release),
    reinterpret_cast<void*>(&sm_queryInterface),
    reinterpret_cast<void*>(&sm_onFileChange)
};

// Appends one string to the flat define vector; callers alternate names and values.
void pushDefine(ShaderManager* sm, const char* text)
{
    sm->defines.push_back(std::string(text));
}

// Destroys the define strings; leaves the effect cache intact.
void clearDefines(ShaderManager* sm)
{
    sm->defines.clear();
}

} // anonymous namespace


// ShaderManager_updateShaderDefines, 0x10023730.

// Clears the list, walks the 120-entry table pushing name then value - nine names take a computed
// value instead - then appends the A2M pair, the two shader-model forcing flags, the filter and mip
// block from VideoOptions, and the terrain pair.

void updateShaderDefines(void* shaderManager, void* base)
{
    ShaderManager* sm = static_cast<ShaderManager*>(shaderManager);
    RendDX9Base*   b  = static_cast<RendDX9Base*>(base);
    const VideoOptions* vo = &b->videoOptions;                     // base + 1016

    clearDefines(sm);

    char buf[32];

    for (int i = 0; i < 120; ++i)
    {
        const char* name  = kShaderDefines[i].name;
        const char* value = kShaderDefines[i].value;

        pushDefine(sm, name);

        // The nine special cases, in the body's own strcmp order.
        if (::strcmp(name, "RAPATH") == 0)
        {
            // PE 0x10023793..0x100237BF: _itoa(g_Renderer +0x6CC, 10) over a buffer that already
            // holds "0", so with no Renderer the define stays "0".
            const int raPath = g_Renderer ? static_cast<int>(rendererRaPath()) : 0;
            ::_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%d", raPath);
            pushDefine(sm, buf);
        }
        else if (::strcmp(name, "USEPARTIALPRECISIONTYPES") == 0)
        {
            pushDefine(sm, sm->flag7A ? "1" : "0");
        }
        else if (::strcmp(name, "PS2_EXT") == 0)
        {
            // This define expands to a shader profile name.
            pushDefine(sm, sm->flag78 ? "ps_2_a" : "ps_2_0");
        }
        else if (::strcmp(name, "NVIDIA") == 0)
        {
            pushDefine(sm, isNvidia() ? "1" : "0");
        }
        else if (::strcmp(name, "ATI") == 0)
        {
            pushDefine(sm, isAti() ? "1" : "0");
        }
        else if (::strcmp(name, "SM30") == 0)
        {
            pushDefine(sm, (deviceCapsPixelShaderMajor() < 3 || deviceCapsVertexShaderMajor() < 3)
                               ? "0" : "1");
        }
        else if (::strcmp(name, "IS_NV4X") == 0)
        {
            pushDefine(sm, gpuFamily() == 6 ? "1" : "0");
        }
        else if (::strcmp(name, "HASALPHA2MASK") == 0)
        {
            // 0x10023AB0..0x10023AF7, read: sub_1000A6B0(dword_1023B970) and
            // then the byte at dword_1023B970 + 0x516 (both through the global, not the `base`
            // argument) -> "1", else "0". The inputs are no longer constant: VideoOptions'
            // resolution change puts the selected AA descriptor into the multisample list and its
            // two probes set +0x514 / +0x515 / +0x516. (Was a hard-coded "0" while nothing wrote
            // them.)
            const bool alpha2Mask =
                isMultisampleActive() &&
                static_cast<const RendDX9Base*>(g_rendererBase)->hasAlphaToCoverage != 0;
            pushDefine(sm, alpha2Mask ? "1" : "0");
        }
        else if (::strcmp(name, "Alpha2Mask") == 0)
        {
            // Select the vendor-specific render state used to enable alpha-to-coverage.
            pushDefine(sm, isNvidia() ? "AdaptiveTess_Y" : value);
        }
        else
        {
            pushDefine(sm, value);
        }
    }

    // Format both vendor transparency-AA codes on each update before adding the defines.
    char a2mEnable[32];
    char a2mDisable[32];
    ::_snprintf_s(a2mEnable, sizeof(a2mEnable), _TRUNCATE, "%X",
                  transparencyAACode(g_transparencyAAOn));
    ::_snprintf_s(a2mDisable, sizeof(a2mDisable), _TRUNCATE, "%X",
                  transparencyAACode(g_transparencyAAOff));
    pushDefine(sm, "A2MEnable");
    pushDefine(sm, a2mEnable);
    pushDefine(sm, "A2MDisable");
    pushDefine(sm, a2mDisable);

    // _FORCE_1_4_SHADERS_ indexes a two-entry table of "0" / "1" at off_101E19A8. The guard reads
    //     if (psMajor < 2 || effectManager[64] != 0) -> 1 else 0
    const bool force14 =
        (deviceCapsPixelShaderMajor() < 2)
        || (Ra::g_effectManager != 0 && Ra::effectManagerFlag40() != 0);
    pushDefine(sm, "_FORCE_1_4_SHADERS_");
    pushDefine(sm, force14 ? "1" : "0");
    pushDefine(sm, "_FORCE_1_3_SHADERS_");
    pushDefine(sm, "0");

    // the texture filter block, from VideoOptions::getQuality(7)
    const int filterQuality = vo->quality[7];
    const int aniso         = static_cast<int>(maxAniso());

    if (filterQuality == 1 || filterQuality == 2)
    {
        // Both arms leave all four diff filters linear; they differ only in which mip filter goes
        // point, which the mip block below picks up.
        pushDefine(sm, "FILTER_TRN_DIFF_MIN");  pushDefine(sm, "LINEAR");
        pushDefine(sm, "FILTER_TRN_DIFF_MAG");  pushDefine(sm, "LINEAR");
        pushDefine(sm, "FILTER_ROAD_DIFF_MIN"); pushDefine(sm, "LINEAR");
        pushDefine(sm, "FILTER_ROAD_DIFF_MAG"); pushDefine(sm, "LINEAR");
    }
    else if (filterQuality == 3)
    {
        // THE TWO clamps ARE different AND that IS NOT A slip: terrain caps at 2, road at 4.
        const int trnAniso  = aniso >= 2 ? 2 : aniso;
        const int roadAniso = aniso >= 4 ? 4 : aniso;

        pushDefine(sm, "FILTER_TRN_DIFF_MIN");
        pushDefine(sm, minAnisoOk() ? "ANISOTROPIC" : "LINEAR");
        pushDefine(sm, "FILTER_TRN_DIFF_MAG");
        pushDefine(sm, magAnisoOk() ? "ANISOTROPIC" : "LINEAR");
        pushDefine(sm, "FILTER_TRN_DIFF_MAX_ANISOTROPY");
        ::_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%i", trnAniso);
        pushDefine(sm, buf);

        pushDefine(sm, "FILTER_ROAD_DIFF_MIN");
        pushDefine(sm, minAnisoOk() ? "ANISOTROPIC" : "LINEAR");
        pushDefine(sm, "FILTER_ROAD_DIFF_MAG");
        pushDefine(sm, magAnisoOk() ? "ANISOTROPIC" : "LINEAR");
        pushDefine(sm, "FILTER_ROAD_DIFF_MAX_ANISOTROPY");
        ::_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%i", roadAniso);
        pushDefine(sm, buf);
    }
    else
    {
        ::renddx9abi::logf("ShaderManager.cpp:950 Undefined texture filter quality. (got %d)",
                           filterQuality);
    }

    // The three mip filters. All linear unless quality 1 or 2 forced one to point - quality 1
    // affects both the terrain and the bundled-mesh filter, quality 2 only the latter.
    const char* trnMip  = (filterQuality == 1) ? "POINT" : "LINEAR";
    const char* roadMip = (filterQuality == 1 || filterQuality == 2) ? "POINT" : "LINEAR";
    pushDefine(sm, "FILTER_TRN_MIP");      pushDefine(sm, trnMip);
    pushDefine(sm, "FILTER_ROAD_MIP");     pushDefine(sm, roadMip);
    pushDefine(sm, "FILTER_PARTICLE_MIP"); pushDefine(sm, roadMip);

    // terrain quality, from getQuality(0) - 1
    // 0x10023730 computes `result = getQuality(0) - 1`
    // and then tests `result - 1`: quality 1 (result 0) defines nothing, quality 2 (result 1) is
    // midterrain, quality 3 (result 2) is highterrain, anything else is the :980 diagnostic. The
    // earlier transcription made quality 2 high and quality 3 "undefined", which compiled the LOW
    // include for a High-terrain profile and left the terrain effect without `Hi_Terrain` (run gr10).
    const int terrain = vo->quality[0] - 1;
    if (terrain == 1)
    {
        pushDefine(sm, "HIGHTERRAIN"); pushDefine(sm, "0");
        pushDefine(sm, "MIDTERRAIN");  pushDefine(sm, "1");
    }
    else if (terrain == 2)
    {
        pushDefine(sm, "HIGHTERRAIN"); pushDefine(sm, "1");
        pushDefine(sm, "MIDTERRAIN");  pushDefine(sm, "0");
    }
    else if (terrain != 0)
    {
        // Preserve the diagnostic's original texture/terrain wording mismatch.
        ::renddx9abi::logf("ShaderManager.cpp:980 Undefined texture filter quality. (terrain %d)",
                           terrain);
    }

    if (sm->flag7F)
    {
        pushDefine(sm, "DEBUGTERRAIN");
        pushDefine(sm, "1");
    }

    const unsigned int count = static_cast<unsigned int>(sm->defines.size());
    ::renddx9abi::logf("ShaderManager::updateShaderDefines: %u entries (%u pairs) - NVIDIA=%d "
                       "ATI=%d SM30=%d IS_NV4X=%d PS2_EXT=%s filterQuality=%d terrain=%d",
                       count, count / 2,
                       isNvidia() ? 1 : 0, isAti() ? 1 : 0,
                       (deviceCapsPixelShaderMajor() >= 3
                        && deviceCapsVertexShaderMajor() >= 3) ? 1 : 0,
                       gpuFamily() == 6 ? 1 : 0,
                       sm->flag78 ? "ps_2_a" : "ps_2_0",
                       filterQuality, terrain);
}

// The static D3DXMACRO table at 0x101DEBD0, kShaderDefines with its {0, 0} terminator. Retail's
// updateShaderDefines walks it to that terminator; DiceEffect::compile hands it to D3DXCreateEffect as
// pDefines (PE 0x1001B3BA).
const void* shaderManagerDefineTable()
{
    return kShaderDefines;
}


// ID3DXInclude occupies the ShaderManager face at +0x04; it has Open/Close only.
// Open resolves the mod path, reads into engine-owned storage, and optionally deobfuscates.
// A short read fails. Close frees the returned buffer.

namespace {

// PE 0x10022610.
long __stdcall smIncludeOpen(void* /*self*/, int /*includeType*/, const char* fileName,
                             const void* /*parentData*/, const void** outData,
                             unsigned int* outBytes)
{
    if (!fileName || !outData || !outBytes)
        return (long)-2005529767;

    // FileManager +0x90 - stripModPath, string returned by value through the hidden slot.
    std::string path = stripShaderModPath(fileName);

    // sub_10029D00 - already under "mods/" stays; anything else gets the active mod's prefix.
    if (g_Renderer)
        rendererPatchModPath(path);

    const renddx9abi::HostString hostPath = renddx9abi::makeHostString(path);
    renddx9abi::HostStream* const stream = ::dice::hfe::fileManager->open(
        hostPath, static_cast<renddx9abi::HostFileMode>(2),
        static_cast<renddx9abi::HostFileAccess>(0), 0);
    if (!stream)
        return (long)-2005530522;

    *outBytes = static_cast<unsigned int>(
        static_cast<renddx9abi::HostNonlinearStream*>(stream)->getSize());
    unsigned char* const buf = static_cast<unsigned char*>(::renddx9abi::engineAlloc(*outBytes));
    *outData = buf;
    if (!buf)
    {
        // shipped defect: the opened stream is leaked on allocation failure.
        return (long)-2147024882;       // E_OUTOFMEMORY
    }
    const unsigned int got = stream->read(buf, *outBytes);
    stream->release();

    if (settings->boolGet(std::string("GSLoadObfuscated"), true))
        deobfuscateShader(const_cast<unsigned char*>(static_cast<const unsigned char*>(*outData)),
                         *outBytes);

    return got == *outBytes ? 0 : (long)-2005529767;
}

// PE 0x10022770.
long __stdcall smIncludeClose(void* /*self*/, const void* data)
{
    if (!data)
        return (long)-2005529767;
    ::renddx9abi::engineFree(const_cast<void*>(data));
    return 0;
}

void* const kShaderManagerIncludeVtable[2] =
{
    reinterpret_cast<void*>(&smIncludeOpen),    // +0x00
    reinterpret_cast<void*>(&smIncludeClose),   // +0x04
};


// Device notifications visit cached effects and nested generated effects without null guards.
// Priority 5 releases device state first and restores it last.

// ID3DXEffect::OnLostDevice or ID3DXEffect::OnResetDevice.
typedef HRESULT (STDMETHODCALLTYPE ID3DXEffect::*EffectDeviceNotifyFn)();

inline ShaderManager* smFromListener(void* listener)
{
    // The DeviceLostResetListener face is the +0x08 subobject.
    return reinterpret_cast<ShaderManager*>(
        static_cast<unsigned char*>(listener) - offsetof(ShaderManager, vptrDeviceLost));
}

// PE 0x10022B90 / 0x10022CE0: each generated effect owns the map at retail +0x9C.
void forEachSubEffect(DiceEffect* object, EffectDeviceNotifyFn notify)
{
    for (const auto& node : object->effects)
        (node.second.first->*notify)();
}

void forEachShaderManagerEffect(ShaderManager* sm, EffectDeviceNotifyFn notify)
{
    for (std::map<std::string, Effect*>::iterator node = sm->effectCache.begin();
         node != sm->effectCache.end(); ++node)
    {
        ID3DXEffect* const d3dx = static_cast<ID3DXEffect*>(
            effectD3dxHandle(node->second));                                    // wrapper+0x08
        (d3dx->*notify)();
    }
    for (std::map<std::string, DiceEffect*>::iterator node = sm->map48.begin();
         node != sm->map48.end(); ++node)
        forEachSubEffect(node->second, notify);
}

void RD9_THUNK sm_onDeviceLost(void* listener RD9_THUNK_EDX)
{
    forEachShaderManagerEffect(smFromListener(listener), &ID3DXEffect::OnLostDevice);
}

// PE 0x10022CE0.
void RD9_THUNK sm_onDeviceReset(void* listener RD9_THUNK_EDX)
{
    forEachShaderManagerEffect(smFromListener(listener), &ID3DXEffect::OnResetDevice);
}

// shutdown only (the destroy broadcast, 0x10002600). The mapped objects' IBase
// release (`mov ecx, [node+28h]; mov eax, [ecx]; call [eax+8]`) over both maps; the nodes are left
// in place for the destructor that follows.
// PE 0x10022E30.
void RD9_THUNK sm_onDeviceDestroy(void* listener RD9_THUNK_EDX)
{
    ShaderManager* const sm = smFromListener(listener);
    typedef int (__thiscall* ReleaseFn)(void*);
    for (std::map<std::string, Effect*>::iterator node = sm->effectCache.begin();
         node != sm->effectCache.end(); ++node)
    {
        void* const object = node->second;
        reinterpret_cast<ReleaseFn>((*reinterpret_cast<void***>(object))[2])(object);
    }
    for (std::map<std::string, DiceEffect*>::iterator node = sm->map48.begin();
         node != sm->map48.end(); ++node)
    {
        void* const object = node->second;
        reinterpret_cast<ReleaseFn>((*reinterpret_cast<void***>(object))[2])(object);
    }
}

void* const kShaderManagerListenerVtable[3] =
{
    reinterpret_cast<void*>(&sm_onDeviceLost),      // +0x00
    reinterpret_cast<void*>(&sm_onDeviceReset),     // +0x04
    reinterpret_cast<void*>(&sm_onDeviceDestroy),   // +0x08
};

} // anonymous namespace


// PE 0x10024A40: the constructor continues after native member initialization above.
// The allocation and global publication belong to its caller at PE 0x1000B6D5..0x1000B73F.
bool createShaderManager(void* base, IDirect3DDevice9* device)
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(ShaderManager));
    if (!storage)
    {
        g_shaderManager = 0;
        return false;
    }

    // The six containers, in the constructor's own order (0x10024A66..0x10024B50): sub_10024D40 for
    // the 0x30-node maps, sub_10024E70 for the 0x14-node one, sub_1001BA10 for the 0x48-node one,
    // with the std::string at +0x2C default-constructed between the second and third (0x10024AB9).
    // The member declaration order is that order. The effect cache used to be built by clearDefines,
    // on every define refresh - see the correction there.
    ShaderManager* sm = new (storage) ShaderManager;

    device->AddRef();
    sm->device = device;

    // The three flag-to-bit transfers, in the body's order. All three sources are zero on a fresh
    // object, so none fires - reproduced anyway because they are what the constructor does.
    if (sm->flag7A)  sm->flags84 |= 0x20u;
    if (!sm->flag7B) reinterpret_cast<unsigned char*>(&sm->flags84)[1] |= 1u;
    if (sm->flag7C)  sm->flags84 |= 4u;

    sm->flag78 = isNvidia() ? 1 : 0;

    updateShaderDefines(sm, base);

    g_shaderManager = sm;

    ::renddx9abi::logf("ShaderManager: built at 0x%08X, device AddRef'd, isNvidia=%d",
                       ::renddx9abi::addr(sm), sm->flag78);
    return true;
}

// the expression initializeD3DSubsystems registers at 0x1000B78D..0x1000B7A2:
// `dword_1023B97C ? dword_1023B97C + 8 : 0`, the DeviceLostResetListener subobject. The null arm
// is shipped (it registers a null listener).
void* shaderManagerDeviceLostListener()
{
    return g_shaderManager ? &static_cast<ShaderManager*>(g_shaderManager)->vptrDeviceLost : 0;
}

// PE 0x10022380: release the device reference the constructor took and answer true. Requires
// the live manager; the cached effects are released by the destroy broadcast before this.
bool shaderManagerShutdown()
{
    static_cast<IUnknown*>(static_cast<ShaderManager*>(g_shaderManager)->device)->Release();
    return true;
}

// PE 0x1000AF31..0x1000AF49: the destructor (0x100248E0) and the free, past the reference count.
// dword_1023B97C keeps its value, as shipped.
void destroyShaderManager()
{
    if (ShaderManager* const sm = static_cast<ShaderManager*>(g_shaderManager))
    {
        destructShaderManager(sm);
        ::renddx9abi::engineFree(sm);
    }
}

// sub_10024A10 - init. On anything but nvidia it does nothing and returns 1. Reproduced whole.
bool shaderManagerInit(void* base)
{
    if (!g_shaderManager)
        return true;

    ShaderManager* sm = static_cast<ShaderManager*>(g_shaderManager);

    if (g_deviceCaps && isNvidia())
    {
        sm->flags84 |= 0x20u;
        sm->flag7A   = 1;
        sm->flag78   = 1;
        updateShaderDefines(sm, base);
    }
    return true;
}


// Returns a borrowed view of the macro strings used by the effect compiler.
ShaderManagerDefines shaderManagerDefines(void* shaderManager)
{
    ShaderManagerDefines out;
    out.first = 0;
    out.count = 0;
    ShaderManager* sm = static_cast<ShaderManager*>(shaderManager);
    if (sm && !sm->defines.empty())
    {
        out.first = &sm->defines[0];
        out.count = static_cast<unsigned int>(sm->defines.size());
    }
    return out;
}

const char* ShaderManagerDefines::at(unsigned int i) const
{
    return first ? first[i].c_str() : "";
}

// Returns the embedded ID3DXInclude interface that resolves files through mounted archives.
void* shaderManagerInclude(void* shaderManager)
{
    return shaderManager ? &static_cast<ShaderManager*>(shaderManager)->vptr2 : 0;   // +4
}

#if !defined(_WIN32)
// [CHOSEN, not recovered] The native include handler already takes an int; avoid
// converting PE 0x1001A132's 0x7FFFFFFF through the subset's truncated enum range.
void shaderManagerOpenDiceInclude(const char* name, const void* parent,
                                  const void** data, unsigned int* size)
{
    smIncludeOpen(shaderManagerInclude(g_shaderManager), 0x7FFFFFFF, name, parent, data, size);
}
#endif

// +0xA4: a nonzero byte keeps freshly bound effects off the shared StateManager (Effect.cpp's
// bind and begin read it).
unsigned char shaderManagerStateManagerDisabled(void* shaderManager)
{
    return static_cast<ShaderManager*>(shaderManager)->disableStateManagerA4;
}

// +132 == +0x84, the D3DXSHADER_* flag word handed to D3DXCreateEffect.
unsigned long shaderManagerEffectFlags(void* shaderManager)
{
    ShaderManager* sm = static_cast<ShaderManager*>(shaderManager);
    return sm ? sm->flags84 : 0u;
}

// The device the compiler passes as pDevice: g_Renderer's base object at +0x0C.
void* renderDevice()
{
    if (!g_Renderer)
        return 0;
    return reinterpret_cast<RendDX9Base*>(g_Renderer)->device;
}

// Effect-cache path helpers. Keys are lowercased with the mod prefix stripped; loads restore
// that prefix and append .fx. Returned effects carry a caller reference.
namespace {

void toLowerAscii(char* text)
{
    for (char* c = text; *c; ++c)
        if (*c >= 'A' && *c <= 'Z')
            *c = static_cast<char>(*c - 'A' + 'a');
}

} // anonymous namespace

// sub_1001F8F0's caller-facing shape: a find in the effect cache with the same key normalisation
// getEffect uses, and NO compile on a miss. The QuadDrawer's frame bracket (its +0x14/+0x18)
// probes for "Menu" through this; on the recovered paths nothing ever caches that name, so the
// miss arm is the live one.
void* shaderManagerFindCachedEffect(const char* rawPath)
{
    ShaderManager* sm = static_cast<ShaderManager*>(g_shaderManager);
    if (!sm || !rawPath)
        return 0;

    char key[512];
    ::_snprintf_s(key, sizeof(key), _TRUNCATE, "%s", rawPath);
    toLowerAscii(key);

    std::map<std::string, Effect*>& cache = sm->effectCache;      // +0x14
    const std::map<std::string, Effect*>::iterator hit = cache.find(key);
    return hit != cache.end() ? hit->second : 0;
}

void* shaderManagerGetEffect(void* shaderManager, const char* rawPath)
{
    // PE 0x10022EF0
    RD9_TRACE_AT("dice::hfe::dx9rend::ShaderManager::getEffect", shaderManager, 0x00);

    ShaderManager* sm = static_cast<ShaderManager*>(shaderManager);
    loadStat->m_unknown08 = true;
    std::string key = stripShaderModPath(rawPath);
    pathToLower(key);

    std::map<std::string, Effect*>& cache = sm->effectCache;      // +0x14

    const std::map<std::string, Effect*>::iterator hit = cache.find(key);
    if (hit != cache.end())
    {
        void* const cached = hit->second;
        void** v = *reinterpret_cast<void***>(cached);
        typedef void (__thiscall* AddRefFn)(void*);
        reinterpret_cast<AddRefFn>(v[0x00 / 4])(cached);
        loadStat->m_unknown08 = false;
        return hit->second;
    }

    std::string path(key);
    if (g_Renderer)
        rendererPatchModPath(path);
    path += ".fx";

    Effect* const effect = createEffectObject();
    const bool loaded = effectReload(effect, path);
    bool retried = false;
    if (g_Renderer && !loaded &&
        !static_cast<IRenderer*>(g_Renderer)->waitWhileAppInactive())
    {
        retried = true;
        effectReload(effect, path);
    }

    {
        void** v = *reinterpret_cast<void***>(effect);
        typedef void (__thiscall* AddRefFn)(void*);
        reinterpret_cast<AddRefFn>(v[0x00 / 4])(effect);
    }
    // PE 0x10075300: inlined string copy, separator replacement and returned copy.
    std::string normalized(key);
    pathReplaceSeparators(normalized, '/');
    cache[normalized] = effect;
    normalized = key;
    pathReplaceSeparators(normalized, '/');
    pathToLower(normalized);
    sm->cache20[normalized] = effect;

    loadStat->m_unknown08 = false;
    if (retried)
        shaderLoadingEvent();
    return effect;
}

// PE 0x100230E0. Generated effects have their own pool and three-slot reference table.
DiceEffect* shaderManagerGetDiceEffect(void* shaderManager, const std::string& name)
{
    ShaderManager* const sm = static_cast<ShaderManager*>(shaderManager);
    loadStat->m_unknown08 = true;
    const auto hit = sm->map48.find(name);
    if (hit != sm->map48.end())
    {
        reinterpret_cast<DiceEffectReferences*>(hit->second)->addRef();
        loadStat->m_unknown08 = false;
        return hit->second;
    }

    static const char* const extensions[] = { ".dfx", ".dcfx", "", 0 }; // PE 0x1023818C
    unsigned int extension;
    std::string path;
    for (;;)
    {
        for (extension = 0; extensions[extension]; ++extension)
        {
            path = name + extensions[extension];
            if (::renddx9abi::engineFileExists(path))
                break;
        }
        if (extensions[extension])
            break;
        const std::string message = name
            + " not found!!! _DO_ check your working directory, _AND_ sync your shaders folder "
              "before calling upon your local rendering programmer/GP. (really!)";
#if defined(_WIN32)
        if (::MessageBoxA(0, message.c_str(), "Error!", 0x50015) == 2)
            std::exit(99);
#else
        // [CHOSEN, not recovered] No Win32 modal UI: select Cancel, which exits with 99.
        // PE 0x1002320C..0x1002322B; do not turn a missing effect into a cache miss.
        std::fprintf(stderr, "[CHOSEN, not recovered] dice::hfe::dx9rend::ShaderManager::getDiceEffect "
                             "missing-file dialog: Cancel\n%s\n", message.c_str());
        std::fflush(stderr);
        std::exit(99);
#endif
    }

    // PE 0x10023230..0x10023251: the result is deliberately unchecked.
    ID3DXEffectPool* created = 0;
    D3DXCreateEffectPool(&created);
    IUnknown* const pool = created;
    sm->map60.insert(pool);
    void* const storage = ::renddx9abi::engineAlloc(sizeof(DiceEffect));
    DiceEffect* const effect = storage
        ? new (storage) DiceEffect(name, std::string(extensions[extension]), pool) : 0;
    reinterpret_cast<DiceEffectReferences*>(effect)->addRef();

    std::string normalized(name);
    pathReplaceSeparators(normalized, '/');
    sm->map48[normalized] = effect;
    normalized = name;
    pathReplaceSeparators(normalized, '/');
    pathToLower(normalized);
    sm->map54[normalized] = effect;
    loadStat->m_unknown08 = false;
    return effect;
}


// sub_10023330 - reload one effect file. `path` is "<cache key>.fx"; when the Renderer exists it
// gets the active mod's prefix first (sub_10029D00). The LoadStat byte +8 is raised for the
// duration. A missing file, an uncached key or a failed reload returns false. The effect is also
// registered in the +0x20 map under its lowered, '/'-separated path when that name is new.
namespace {

// PE 0x10023330.
bool shaderManagerReloadEffect(ShaderManager* sm, const std::string& rawPath)
{
    std::string path(rawPath);
    if (g_Renderer)
        rendererPatchModPath(path);

    loadStat->m_unknown08 = true;
    if (!::renddx9abi::engineFileExists(path))                          // FileManager +0x1C
    {
        loadStat->m_unknown08 = false;
        return false;
    }

    // The cache key: extension cut, separators normalised (sub_100753D0), mod prefix stripped
    // (FileManager +0x90).
    std::string key;
    pathStripExtension(key, path);
    renddx9abi::assignFromHostString(key, stripShaderModPath(key.c_str()));

    const std::map<std::string, Effect*>::iterator hit = sm->effectCache.find(key);
    Effect* const effect = hit != sm->effectCache.end() ? hit->second : 0;
    if (!effect || !effectReload(effect, path))                         // sub_1001EE90
    {
        loadStat->m_unknown08 = false;
        return false;
    }

    std::string name(path);                                             // sub_10075300(path, '/')
    pathReplaceSeparators(name, '/');
    pathToLower(name);                                                  // sub_10074E80
    if (sm->cache20.find(name) == sm->cache20.end())
        sm->cache20[name] = effect;

    loadStat->m_unknown08 = false;
    return true;
}

// PE 0x100229C0. The generated-effect loader remains the actual-use frontier.
bool shaderManagerReloadDiceEffect(ShaderManager* sm, const std::string& path)
{
    loadStat->m_unknown08 = true;
    if (!::renddx9abi::engineFileExists(path))
    {
        loadStat->m_unknown08 = false;
        return false;
    }
    std::string key;
    pathStripExtension(key, path);
    pathToLower(key);
    const auto hit = sm->map54.find(key);
    if (hit == sm->map54.end())
    {
        loadStat->m_unknown08 = false;
        return false;
    }
    const unsigned char saved = sm->flag7E;
    sm->flag7E = 0;
    hit->second->reload();
    sm->flag7E = saved;
    loadStat->m_unknown08 = false;
    return true;
}

} // anonymous namespace

// sub_10023540 - the cached-effect reload sweep the Renderer runs after the shader defines change:
// every effect cache entry is reloaded from "<key>.fx" (sub_10023330), then every +0x54 entry from
// "<key>.dfx" (sub_100229C0). Results are not checked.
void shaderManagerReloadAllEffects(void* shaderManager)
{
    ShaderManager* const sm = static_cast<ShaderManager*>(shaderManager);
    for (std::map<std::string, Effect*>::iterator node = sm->effectCache.begin();
         node != sm->effectCache.end(); ++node)
        shaderManagerReloadEffect(sm, node->first + ".fx");             // off_10238184
    for (const auto& node : sm->map54)
        shaderManagerReloadDiceEffect(sm, node.first + ".dfx");
}

// The shadermanager console object's arms (RendererConsole.cpp; ctors 0x1005D860..0x1005E760),
// all on g_shaderManager, which the bindings' activity check (0x1005E850) requires.

// reloadEffect's executor (0x1005DA10) - sub_10023690 on a copy of the name; result unused.
void shaderManagerConsoleReloadEffect(const std::string& name)
{
    shaderManagerReloadPath(static_cast<ShaderManager*>(g_shaderManager), std::string(name));
}

// listEffects (sub_10022A80): walks the +0x20 alias map and then the +0x54 map. The shipped walk
// does nothing per entry (its report is compiled out of the retail image).
void shaderManagerListEffects()
{
    ShaderManager* const sm = static_cast<ShaderManager*>(g_shaderManager);
    for (std::map<std::string, Effect*>::iterator node = sm->cache20.begin();
         node != sm->cache20.end(); ++node)
    {
    }
    for (std::map<std::string, DiceEffect*>::iterator node = sm->map54.begin();
         node != sm->map54.end(); ++node)
    {
    }
}

// The compile switches. which: 0 compileUsePS2A (+0x78, get 0x100224E0, set 0x10024880),
// 1 compilePartialPrecisionTypes (+0x79, 0x100224F0 / 0x100248A0), 2 compileUsePartialPrecision
// (+0x7A, 0x10022500 / 0x10022510), 3 compileUsePreShaders (+0x7B, 0x10022540 / 0x10022550),
// 4 compileSkipOptimizations (+0x7C, 0x10022580 / 0x10022590), 5 compileWithDebug (+0x7D,
// 0x100225F0 / 0x10022600), 6 compileDebugTerrain (+0x7F, 0x100225E0 / 0x100248C0),
// 7 byPassDiceStates (+0xA4, read and written by its executor 0x1005E8C0 itself).
// PE 0x100224F0, 0x10022500, 0x10022540, 0x10022580, 0x100225E0, 0x100225F0: the out-of-line getters, one byte read each.
bool shaderManagerGetConsoleSwitch(int which)
{
    const ShaderManager* const sm = static_cast<const ShaderManager*>(g_shaderManager);
    switch (which)
    {
    case 0:  return sm->flag78 != 0;
    case 1:  return sm->flag79 != 0;
    case 2:  return sm->flag7A != 0;
    case 3:  return sm->flag7B != 0;
    case 4:  return sm->flag7C != 0;
    case 5:  return sm->flag7D != 0;
    case 6:  return sm->flag7F != 0;
    default: return sm->disableStateManagerA4 != 0;
    }
}

// ShaderManager flag7E / flag80 accessors. 0x100225C0 and 0x100224C0 are unreachable in 1.50: no code
// or data references; 0x100225D0's only caller is DiceEffect::compile (0x1001ADBA), itself unreachable.
// The retail members take the manager as `this`; the manager is passed explicitly here. [INFERRED] names.
// PE 0x100225D0: returns byte +0x7E.
bool shaderManagerGetFlag7E(const ShaderManager* sm) { return sm->flag7E != 0; }
// PE 0x100225C0: stores the argument byte at +0x7E.
void shaderManagerSetFlag7E(ShaderManager* sm, bool on) { sm->flag7E = on; }
// PE 0x100224C0: stores the argument byte at +0x80.
void shaderManagerSetFlag80(ShaderManager* sm, bool on) { sm->flag80 = on; }

// PE 0x10022510 / 0x10022550 / 0x10022590 / 0x10024880 / 0x100248A0 / 0x100248C0 / 0x10022600.
void shaderManagerSetConsoleSwitch(int which, bool on)
{
    ShaderManager* const sm = static_cast<ShaderManager*>(g_shaderManager);
    switch (which)
    {
    case 0:                                         // the defines are rebuilt (sub_10023730)
        sm->flag78 = on;
        updateShaderDefines(sm, g_rendererBase);
        break;
    case 1:
        sm->flag79 = on;
        updateShaderDefines(sm, g_rendererBase);
        break;
    case 2:                                         // D3DXSHADER_PARTIALPRECISION (0x20)
        sm->flag7A = on;
        if (on)
            sm->flags84 |= 0x20u;
        else
            sm->flags84 &= ~0x20u;
        break;
    case 3:                                         // byte +0x85 bit 0: D3DXSHADER_NO_PRESHADER
        sm->flag7B = on;
        if (on)
            sm->flags84 &= ~0x100u;
        else
            sm->flags84 |= 0x100u;
        break;
    case 4:                                         // D3DXSHADER_SKIPOPTIMIZATION (0x4)
        sm->flag7C = on;
        if (on)
            sm->flags84 |= 0x4u;
        else
            sm->flags84 &= ~0x4u;
        break;
    case 5:                                         // the shipped setter is an empty `ret 4`
        break;
    case 6:
        sm->flag7F = on;
        updateShaderDefines(sm, g_rendererBase);
        break;
    default:                                        // a set flag reloads every effect
        sm->disableStateManagerA4 = on;
        if (on)
            shaderManagerReloadAllEffects(sm);
        break;
    }
}

// Clears texture bindings on every cached effect without changing the cache.
// Requires a live manager and valid cached effect pointers.

// PE 0x10022B30.
void shaderManagerClearEffectTextureBindings(void* shaderManager)
{
    ShaderManager* const sm = static_cast<ShaderManager*>(shaderManager);
    for (std::map<std::string, Effect*>::iterator node = sm->effectCache.begin();
         node != sm->effectCache.end(); ++node)
    {
        effectClearTextureParameters(node->second);
    }
}


// PE 0x10022390 (listing read). Unreachable in 1.50: no code or data references (the _r build's copy,
// RendDX9_r 0x10068CA0, is reached only through an unreferenced incremental-link thunk). The retail
// member takes the manager as `this`; it is passed explicitly here. [INFERRED] name. The Error
// diagnostic is emitted before the name at +0x2C is assigned.
void shaderManagerSetName(ShaderManager* sm, const std::string& name)
{
    {
        ::renddx9abi::Debug diagnostic(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\ShaderManager.cpp",
            102, "");
        diagnostic << "Not fully implemented";
    }
    sm->name = name;
}

// PE 0x10022960 (listing read). Unreachable in 1.50: no code or data references. The _r build's copy
// (RendDX9_r 0x1006A7C0, likewise reached only through an unreferenced incremental-link thunk) brackets
// the same LoadStat byte and logs "SHADERS: loading D3DX effect before MOD Hack: " and "... after  MOD
// Hack: " (ShaderManager.cpp lines 322 and 323) with Debug-level diagnostics that 1.50 compiles out.
// The path is normalised as getEffect does (FileManager +0x90, then the mod prefix when a Renderer
// exists) and the result is discarded; eax is 0.
// [CHOSEN, not recovered] Unobservable-type exception: retail is __thiscall with ecx never read, takes
// a string reference and a second stack argument nothing reads (`ret 8`), and no image reads its
// result. The owner, the name, the second argument's type and the int return are chosen.
int shaderManagerLoadD3DXEffect(const std::string& path, unsigned int)
{
    loadStat->m_unknown08 = true;
    std::string stripped = stripShaderModPath(path.c_str());
    if (g_Renderer)
        rendererPatchModPath(stripped);
    loadStat->m_unknown08 = false;
    return 0;
}

// setCurrentEffect - sub_100227A0, the call Effect::begin makes with the effect and Effect::end
// makes with null. Replacing a still-current effect emits the filtered Error diagnostic;
// if filtered, the replacement still becomes current.
// PE 0x100227A0.
void shaderManagerSetCurrentEffect(void* effect)
{
    ShaderManager* sm = static_cast<ShaderManager*>(g_shaderManager);
    if (sm->currentEffect98 && effect)
    {
        ::renddx9abi::Debug diagnostic(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\ShaderManager.cpp",
            110, "");
        const std::string& previousName = effectName(sm->currentEffect98);
        diagnostic << "Previous effect: " << previousName << " not ended.";
    }
    sm->currentEffect98 = effect;
}

// PE 0x10022880.
void shaderManagerSetCurrentDiceEffect(void* effect)
{
    ShaderManager* const sm = static_cast<ShaderManager*>(g_shaderManager);
    if (sm->statesEffect9C && effect)
        ::renddx9abi::logf("ShaderManager.cpp:130 Previous effect: %s not ended.",
                           sm->statesEffect9C->name.c_str());
    sm->statesEffect9C = static_cast<DiceEffect*>(effect);
}


// The pre-draw CommitChanges walk - the first three calls of sub_10026880, in the shipped order:
// the raw ID3DXEffect at +0xA0, the current Effect wrapper at +0x98 (through its ID3DXEffect at
// wrapper+0x08), then the states-effect arm at +0x9C.
// The states-effect getter deliberately returns null when no technique is selected;
// the shipped commit dereferences that result without another guard.

void shaderManagerCommitCurrentEffects()
{
    ShaderManager* sm = static_cast<ShaderManager*>(g_shaderManager);
    if (sm->rawEffectA0)
    {
        ID3DXEffect* fx = static_cast<ID3DXEffect*>(sm->rawEffectA0);
        fx->CommitChanges();
    }
    sm = static_cast<ShaderManager*>(g_shaderManager);
    if (sm->currentEffect98)
    {
        ID3DXEffect* fx = static_cast<ID3DXEffect*>(
            effectD3dxHandle(sm->currentEffect98));                       // wrapper+0x08
        fx->CommitChanges();
    }
    sm = static_cast<ShaderManager*>(g_shaderManager);
    if (sm->statesEffect9C)
    {
        sm->statesEffect9C->currentEffect()->CommitChanges();
    }
}

// sub_10022420, the active RA effect, kept at +0xA0 (`rawEffectA0` above): what
// RaShader::begin installs and RaShader::end clears, and the first of the three effects the
// pre-draw apply (sub_10026880) commits before every device draw - which is how a Ra shader's
// per-entry parameter sets reach the device.  Installing over a live one logs the attested
// ShaderManager.cpp:119 line and proceeds.  No null test on the manager,
// as shipped.
void shaderManagerSetActiveRaEffect(void* effect)
{
    ShaderManager* const sm = static_cast<ShaderManager*>(g_shaderManager);
    if (sm->rawEffectA0 && effect)
        ::renddx9abi::logf("ShaderManager.cpp:119 Previous effect not ended in RaShader.");
    sm->rawEffectA0 = effect;
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
