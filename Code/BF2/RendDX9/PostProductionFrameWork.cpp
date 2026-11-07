// Post-processing settings and device/event-listener owner. [INFERRED] source placement.
// The published singleton is the owner's third interface (owner+8 on x86); preserve that adjustment.
// [CHOSEN] named storage wraps the fixed owner layout. Also here: the mode selector (dword_1023F3D8)
// and its night-vision / TV presets.
//
// BF2Render.cpp declares and uses these (defined at the foot of this file):
//   unsigned int postProductionFrameWorkOwnerSize();      // the owner allocation size
//   void* postProductionFrameWorkOwnerCtor(void* mem);
//   void* postProductionFrameWorkInterface(void* owner);   // the published subobject
//   void  postProductionFrameWorkOwnerDetach(void* owner);
//   bool  postProductionFrameWorkIsActive();               // sub_1004F1C0 on the published owner
// and the frame passes and console accessors declared in PostProductionFrameWork.h.

#ifdef BF2_IMAGE_RENDDX9
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Signatures of the existing native post-production callers.
#include "../RendMisc/NativePostProductionSlots.h"
#endif
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Compiler-typed native table entries.
#include "../../RendDX9Abi/NativeVtable.h"
#include "../Game/IEventListener.h"
#endif

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"   // classManager, for the mode selector's HudManager
#include "../../RendDX9Abi/D3dx9Sdk.h"     // D3DXCreateTexture / D3DXSaveTextureToFileA, PE 0x1004F3A0
#include <string>
#include "RendDX9Base.h"
#include "Texture.h"          // textureReleaseRef for the listener's lost slot
#include "PostProductionFrameWork.h"
#include "ShaderParameter.h"
#include "../Geom/Terrain.h"
#include "../Core/EventManager.h"
#include "../GeomMisc/FSQuadDrawer.h"
#include "../RendMisc/IRenderView.h"
#include "../Menu/BF2HudManager.h"          // the mode selector's vehicle-HUD query
#include "../Menu/HudVehicleInformation.h"
#if !defined(_M_IX86)
#include "../Platform/X87Compat.h"
#endif

#include <new>
#include <deque>
#include <cmath>
#include <cstdlib>
#include <stddef.h>
#include <string.h>

#if defined(_MSC_VER)
extern "C" int __cdecl _purecall();
#else
extern "C" void __cxa_pure_virtual();
#endif

namespace dice {
namespace hfe {
namespace geom {
// XpLightManager.cpp - the render-target selector with level 0 and face 0x7FFFFFFF.
void lightManagerSetRenderTargetTexture(unsigned int index, dx9rend::EngineTexture* texture);
}
namespace dx9rend {

// Defined in BF2Render.cpp.
extern IBase* g_PostProductionFrameWorkOwner;
extern IBase* g_Renderer;                      // dword_1023D098, the device at +0x0C
void rendererPostReselectTarget();             // sub_10028070
extern void* g_shaderManager;                  // ShaderManager.cpp, dword_1023B97C
extern void* g_renderTargetSystem;             // RenderQueues.cpp, dword_1023F3D8
bool renderQueueHasEntries(unsigned int slotByteOffset);
void renderQueueFlush(unsigned int slotByteOffset, int flushArg);
void postProductionFrameWorkOwnerDetach(void* owner);

namespace {

// [CHOSEN, not recovered] The owner as a struct (issue #7). On x86 every member sits at its retail
// offset (asserted below, 548 bytes in all); on x64 the interface pointers, resource pointers and
// the std::string and deque take their native sizes. The scalar fields live in one pointer-free window that
// is addressed by retail owner offset (U/F/Bt below); on x86 that is the retail address itself.
const unsigned int kScalarsBegin          = 0x00C;
const unsigned int kScalarsEnd            = 0x150;
const unsigned int kRetailFrameWorkOffset = 0x008;      // the published interface, retail owner+8
// [CHOSEN, not recovered] (issue #7) Keep bounded retail slots on x86 and native STL widths elsewhere.
#if defined(_M_IX86)
const unsigned int kNameStorageBytes      = 28;         // the retail VC7.1 std::string width
const unsigned int kHistoryStorageBytes   = 20;         // the retail VC7.1 std::deque width
#else
const unsigned int kNameStorageBytes      = sizeof(std::string);
const unsigned int kHistoryStorageBytes   = sizeof(std::deque<unsigned int>);
#endif

// ECGameLogic 13's payload, BfSound::TinnitusSetup (Code/BF2/Sound/BfSound.h). Pointer-free, so the
// same five dwords on x86 and x64.
struct TinnitusRecord
{
    int   state;      // +0x00 1 while the effect is started
    float attack;     // +0x04
    float hold;       // +0x08
    float release;    // +0x0C
    float volume;     // +0x10
};

class PostProductionPreset;

struct PostProductionOwner
{
    const void* const* listenerVtable;      // +0x000 EventListener + IBase spine (kOwnerVtable)
    const void* const* deviceLostVtable;    // +0x004 DeviceLostResetListener (kDeviceLostVtable)
    const void* const* frameWorkVtable;     // +0x008 the published interface (kPostProdVtable)
    unsigned char      scalars[kScalarsEnd - kScalarsBegin];   // +0x00C..+0x14F by retail offset:
                                            //        refcount, timer, flags, the fog record
                                            //        (+0x28..+0x78), frame-work floats, matrices
    // PE 0x10050B99..0x10050BB7 / 0x10050C80..0x10050CCA push target IDs 17..20,
    // 0x10050D20 reads them as renderer-array indices; 0x10051740 stores four-byte elements.
    // [INFERRED] Source integer signedness; only the target-ID bit patterns are observable here.
    alignas(std::deque<unsigned int>)
        unsigned char targetHistoryStorage[kHistoryStorageBytes]; // +0x150
    TinnitusRecord     tinnitus;            // +0x164 the last ECGameLogic 13 payload
    void*              effect;              // +0x178 the PostProduction effect (an Effect)
    void*              effectParameters[29];// +0x17C ShaderParameter handles, TEXLAYER0..DISPLACEMENT
    unsigned int       field1F0;            // +0x1F0 [UNRESOLVED] identity; zeroed by both default arms
    EngineTexture*     helperTextures[3];   // +0x1F4 common/textures/noise, random, sine
    EngineTexture*     noiseTexture;        // +0x200 the setNoiseTextureName texture (listener +0x1FC)
    unsigned char      noiseTextureNameStorage[kNameStorageBytes];  // +0x204 std::string (listener +0x200)
    PostProductionPreset* preset;           // +0x220 the selected preset (sub_1004F2B0), or null
#if !defined(_M_IX86)
    // [CHOSEN, not recovered] (issue #7) The +0xD0 gradient texture pointer lives in the retail
    // scalar window; native pointers do not fit there, so x64 keeps it here.
    EngineTexture*     gradientTextureNative;
#endif
};

static_assert(sizeof(TinnitusRecord) == 20, "BfSound::TinnitusSetup is five dwords");
static_assert(sizeof(std::string) <= kNameStorageBytes, "the noise-texture name does not fit");
static_assert(sizeof(std::deque<unsigned int>) <= kHistoryStorageBytes,
              "the target-ID history does not fit the owner storage");
static_assert(offsetof(PostProductionOwner, noiseTextureNameStorage) % alignof(std::string) == 0,
              "the noise-texture name is misaligned");
#if defined(_M_IX86)
static_assert(offsetof(PostProductionOwner, listenerVtable)          == 0x000, "retail owner layout");
static_assert(offsetof(PostProductionOwner, deviceLostVtable)        == 0x004, "retail owner layout");
static_assert(offsetof(PostProductionOwner, frameWorkVtable)         == kRetailFrameWorkOffset,
              "retail owner layout");
static_assert(offsetof(PostProductionOwner, scalars)                 == kScalarsBegin, "retail owner layout");
static_assert(offsetof(PostProductionOwner, targetHistoryStorage)    == 0x150, "retail owner layout");
static_assert(offsetof(PostProductionOwner, tinnitus)                == 0x164, "retail owner layout");
static_assert(offsetof(PostProductionOwner, effect)                  == 0x178, "retail owner layout");
static_assert(offsetof(PostProductionOwner, effectParameters)        == 0x17C, "retail owner layout");
static_assert(offsetof(PostProductionOwner, field1F0)                == 0x1F0, "retail owner layout");
static_assert(offsetof(PostProductionOwner, helperTextures)          == 0x1F4, "retail owner layout");
static_assert(offsetof(PostProductionOwner, noiseTexture)            == 0x200, "retail owner layout");
static_assert(offsetof(PostProductionOwner, noiseTextureNameStorage) == 0x204, "retail owner layout");
static_assert(offsetof(PostProductionOwner, preset)                  == 0x220, "retail owner layout");
static_assert(sizeof(PostProductionOwner) == 548, "DX9RendModule::init allocates 548 bytes");
#endif

// Scalar fields by retail owner offset; the window check is at compile time.
template<class T, unsigned int Off> T& scalarAt(PostProductionOwner* o)
{
    static_assert(Off >= kScalarsBegin && Off + sizeof(T) <= kScalarsEnd, "outside the scalar window");
    return *reinterpret_cast<T*>(o->scalars + (Off - kScalarsBegin));
}
template<unsigned int Off, unsigned int Len> unsigned char* scalarBytes(PostProductionOwner* o)
{
    static_assert(Off >= kScalarsBegin && Off + Len <= kScalarsEnd, "outside the scalar window");
    return o->scalars + (Off - kScalarsBegin);
}
template<unsigned int Off> unsigned int&  U (PostProductionOwner* o) { return scalarAt<unsigned int, Off>(o); }
template<unsigned int Off> float&         F (PostProductionOwner* o) { return scalarAt<float, Off>(o); }
template<unsigned int Off> unsigned char& Bt(PostProductionOwner* o) { return scalarAt<unsigned char, Off>(o); }

// Each thunk receives its own subobject; these undo that adjustment (retail 0 / 4 / 8 on x86).
inline PostProductionOwner* ownerAt(void* subobject, size_t offset)
{
    return reinterpret_cast<PostProductionOwner*>(static_cast<char*>(subobject) - offset);
}
inline PostProductionOwner* ownerOfEventListener(void* self)
{
    return ownerAt(self, offsetof(PostProductionOwner, listenerVtable));
}
inline PostProductionOwner* ownerOfDeviceLost(void* self)
{
    return ownerAt(self, offsetof(PostProductionOwner, deviceLostVtable));
}
inline PostProductionOwner* ownerOfFrameWork(void* self)
{
    return ownerAt(self, offsetof(PostProductionOwner, frameWorkVtable));
}
inline void* frameWorkOf(PostProductionOwner* o) { return &o->frameWorkVtable; }
inline PostProductionOwner* publishedOwner()
{
    return static_cast<PostProductionOwner*>(static_cast<void*>(g_PostProductionFrameWorkOwner));
}
inline std::string& noiseTextureName(PostProductionOwner* o)
{
    return *reinterpret_cast<std::string*>(o->noiseTextureNameStorage);
}
inline std::deque<unsigned int>& targetHistory(PostProductionOwner* o)
{
    return *reinterpret_cast<std::deque<unsigned int>*>(o->targetHistoryStorage);
}
inline Effect* effectOf(PostProductionOwner* o) { return static_cast<Effect*>(o->effect); }
// +0xD0, the TV gradient texture (TEXLAYER3 of "TVEffect_Gradient_Tex").
inline EngineTexture*& gradientTexture(PostProductionOwner* o)
{
#if defined(_M_IX86)
    return *reinterpret_cast<EngineTexture**>(scalarBytes<0xD0, 4>(o));
#else
    return o->gradientTextureNative;
#endif
}
// The handle at retail owner offset `Off` (+0x17C..+0x1EC).
template<unsigned int Off> ShaderParameter* parameterAt(PostProductionOwner* o)
{
    static_assert(Off >= 0x17C && Off <= 0x1EC && (Off - 0x17C) % 4 == 0, "not a parameter slot");
    return static_cast<ShaderParameter*>(o->effectParameters[(Off - 0x17C) / 4]);
}

// The frame-work bodies' fields by the shipped `this`-relative offset (retail this = owner+8).
template<unsigned int Off> unsigned int&  PU (void* self) { return U <kRetailFrameWorkOffset + Off>(ownerOfFrameWork(self)); }
template<unsigned int Off> float&         PF (void* self) { return F <kRetailFrameWorkOffset + Off>(ownerOfFrameWork(self)); }
template<unsigned int Off> unsigned char& PBt(void* self) { return Bt<kRetailFrameWorkOffset + Off>(ownerOfFrameWork(self)); }

// A float argument of a published setter. On x86 the caller's stack dword is taken as unsigned and
// stored as a bit pattern, as the shipped `mov` stores do. [CHOSEN, not recovered] x64 passes a
// float in an XMM register, so the parameter is float there and its bits are stored unchanged
// (issue #7).
#if defined(_M_IX86)
typedef unsigned int FloatArg;
inline unsigned int floatArgBits(FloatArg v) { return v; }
#else
typedef float FloatArg;
inline unsigned int floatArgBits(FloatArg v)
{
    unsigned int bits;
    memcpy(&bits, &v, sizeof bits);
    return bits;
}
#endif

inline void copyWord(void* dst, const void* src) { memcpy(dst, src, 4); }


// The published singleton's vtable (owner+8). Every slot `this` is owner+8.


// PE 0x10051700 adjusts the interface to the owner before deleting at 0x100517C0.
void* RD9_THUNK pp_dtor(void* self RD9_THUNK_EDX, unsigned int flags)
{
    PostProductionOwner* const owner = ownerOfFrameWork(self);
    postProductionFrameWorkOwnerDetach(owner);
    if (flags & 1)
        ::renddx9abi::engineFree(owner);
    return owner;
}

// +0x04 / +0x08 / +0x0C / +0x10 / +0x14 / +0x18 / +0x1C / +0x20 - eight paired float accessors.
// (this+40..43 <=> byte 160/164/168/172.)
void  RD9_THUNK pp_set160(void* self RD9_THUNK_EDX, FloatArg v) { PU<160>(self) = floatArgBits(v); }  // +0x04
float RD9_THUNK pp_get160(void* self RD9_THUNK_EDX)             { return PF<160>(self); }            // +0x08
void  RD9_THUNK pp_set164(void* self RD9_THUNK_EDX, FloatArg v) { PU<164>(self) = floatArgBits(v); }  // +0x0C
float RD9_THUNK pp_get164(void* self RD9_THUNK_EDX)             { return PF<164>(self); }            // +0x10
// PE 0x10051670
void  RD9_THUNK pp_set168(void* self RD9_THUNK_EDX, FloatArg v) { PU<168>(self) = floatArgBits(v); }  // +0x14
float RD9_THUNK pp_get168(void* self RD9_THUNK_EDX)             { return PF<168>(self); }            // +0x18
void  RD9_THUNK pp_set172(void* self RD9_THUNK_EDX, FloatArg v) { PU<172>(self) = floatArgBits(v); }  // +0x1C
float RD9_THUNK pp_get172(void* self RD9_THUNK_EDX)             { return PF<172>(self); }            // +0x20

// PE 0x1004F7A0: store the state and invalidate the published terrain's lighting.
void RD9_THUNK pp_setStateVec4(void* self RD9_THUNK_EDX, const float* state)
{
    const float w = state[3], z = state[2], y = state[1];
    const unsigned int x = *reinterpret_cast<const unsigned*>(&state[0]);
    PF<48>(self) = y;
    PU<44>(self) = x;
    PF<52>(self) = z;
    PF<56>(self) = w;
    if (geom::terrain())
        geom::terrain()->m_lightingDirty = 1;
}

// +0x28 getStateVec4 (0x1004F7E0).
void* RD9_THUNK pp_getStateVec4(void* self RD9_THUNK_EDX, float* out)
{
    out[0] = PF<44>(self);
    out[1] = PF<48>(self);
    out[2] = PF<52>(self);
    out[3] = PF<56>(self);
    return out;
}

// The shader constant block's vec4 accessors, reached through the native constant-block accessor.
// PE 0x100380B0: stores the four words of `value` at constant offset 0xD0 (the fog range).
void raShaderConstantsSetRange(const float* value)
{
    memcpy(raShaderConstantsAt(0xD0), value, 4 * sizeof(float));
}

// PE 0x10038080: copies the four words at constant offset 0xE4 (the ambient colour) to `value`.
void raShaderConstantsGetColour(float* value)
{
    memcpy(value, raShaderConstantsAt(0xE4), 4 * sizeof(float));
}

// PE 0x10038050: stores the four words of `value` at constant offset 0xE4.
void raShaderConstantsSetColour(const float* value)
{
    memcpy(raShaderConstantsAt(0xE4), value, 4 * sizeof(float));
}

// PE 0x1004F810: update the range coefficients and ambient alpha, then invalidate terrain lighting.
void RD9_THUNK pp_setRange(void* self RD9_THUNK_EDX, const float* a2)
{
    const float w = a2[3], z = a2[2], y = a2[1];
    PU<144>(self) = *reinterpret_cast<const unsigned*>(&a2[0]);
    PF<148>(self) = y;
    PF<152>(self) = z;
    PF<156>(self) = w;

    const float farRange = a2[1], nearRange = a2[0];
    float range[4];
    // MSVC x64 takes the float arm too: its long double is binary64, so bf2x87::wide stops there.
#if defined(_M_IX86) || defined(_MSC_VER)
    const float inverse = 1.0f / (farRange - nearRange);
    range[0] = inverse;
    range[1] = -(inverse * a2[2]);
    range[2] = -(inverse * nearRange);
    range[3] = inverse * farRange;
#else
    // [CHOSEN, not recovered] Preserve the x87 intermediate precision and float-store boundaries.
    const float inverse = bf2x87::store(bf2x87::div(bf2x87::wide(1.0f),
        bf2x87::sub(bf2x87::wide(farRange), bf2x87::wide(nearRange))));
    range[0] = inverse;
    range[1] = bf2x87::store(-bf2x87::mul(bf2x87::wide(inverse), bf2x87::wide(a2[2])));
    range[2] = bf2x87::store(-bf2x87::mul(bf2x87::wide(inverse), bf2x87::wide(nearRange)));
    range[3] = bf2x87::store(bf2x87::mul(bf2x87::wide(inverse), bf2x87::wide(farRange)));
#endif
    raShaderConstantsSetRange(range);
    float color[4];
    raShaderConstantsGetColour(color);
    copyWord(&color[3], &a2[3]);
    raShaderConstantsSetColour(color);
    if (geom::terrain())
        geom::terrain()->m_lightingDirty = 1;
}

// +0x30 get the range vec4 (0x1004F8C0).
void* RD9_THUNK pp_getRange(void* self RD9_THUNK_EDX, float* a2)
{
    a2[0] = PF<144>(self);
    a2[1] = PF<148>(self);
    a2[2] = PF<152>(self);
    a2[3] = PF<156>(self);
    return a2;
}

// +0x34 setGasMaskBlend (0x1004F330) - THE per-frame caller. Store the blend at this+216; if the
// previous blend was exactly 0, latch the "just became active" byte at this+220.
void RD9_THUNK pp_setBlend(void* self RD9_THUNK_EDX, FloatArg a2)
{
    if (PF<216>(self) == 0.0f)
        PBt<220>(self) = 1;
    PU<216>(self) = floatArgBits(a2);
}

// +0x38 getGasMaskBlend (return this[54] == byte 216).
float RD9_THUNK pp_getBlend(void* self RD9_THUNK_EDX) { return PF<216>(self); }

// +0x3C / +0x40 two 0x24-byte (3x3 matrix) copies into this+256 / this+292 (0x10051680 / 0x100516A0).
void RD9_THUNK pp_setMat0(void* self RD9_THUNK_EDX, const void* a2)
{
    memcpy(scalarBytes<kRetailFrameWorkOffset + 256, 0x24u>(ownerOfFrameWork(self)), a2, 0x24u);
}
// PE 0x100516A0 (listing read: nine dwords to +0x124).
void RD9_THUNK pp_setMat1(void* self RD9_THUNK_EDX, const void* a2)
{
    memcpy(scalarBytes<kRetailFrameWorkOffset + 292, 0x24u>(ownerOfFrameWork(self)), a2, 0x24u);
}

// +0x44 / +0x48 two dword setters (this+62 / this+63 == byte 248/252).
// PE 0x100516C0
void RD9_THUNK pp_set248(void* self RD9_THUNK_EDX, FloatArg v) { PU<248>(self) = floatArgBits(v); }
// PE 0x100516D0
void RD9_THUNK pp_set252(void* self RD9_THUNK_EDX, FloatArg v) { PU<252>(self) = floatArgBits(v); }

// +0x4C / +0x50 two byte flag setters (this+242 / this+241) (0x100516E0 / 0x100516F0).
void RD9_THUNK pp_setByte242(void* self RD9_THUNK_EDX, unsigned char v) { PBt<242>(self) = v; }
// PE 0x100516F0
void RD9_THUNK pp_setByte241(void* self RD9_THUNK_EDX, unsigned char v) { PBt<241>(self) = v; }

// +0x54 / +0x58 two getters that read the live Renderer (renderer+1852/1856).
float RD9_THUNK pp_getRendererFloat1852(void* RD9_THUNK_EDX) { return rendererTailFloat(0x73C); }
float RD9_THUNK pp_getRendererFloat1856(void* RD9_THUNK_EDX) { return rendererTailFloat(0x740); }

// +0x5C setNoiseTextureName (0x1004FC30): a name different from the stored one (listener +0x1FC
// string) releases the noise texture (+0x200), is stored, and - when non-empty - is loaded with
// the TextureManager's managed flag forced to 0 (sub_10017E20 / sub_10019260).
void RD9_THUNK pp_setNoiseTextureName(void* self RD9_THUNK_EDX, const std::string& name)
{
    PostProductionOwner* const o = ownerOfFrameWork(self);
    std::string& stored = noiseTextureName(o);
    if (stored == name)
        return;
    textureReleaseRef(o->noiseTexture);
    stored = name;
    if (name.size() == 0)
        return;
    const int oldManaged = textureManagerSwapManaged(0);
    o->noiseTexture = static_cast<EngineTexture*>(textureManagerGetTexture(name));
    textureManagerSwapManaged(oldManaged);
}

#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Match the native callers of this explicit function table.
const void* const kPostProdVtable[24] =
{
    reinterpret_cast<void*>(&pp_dtor),
    BF2_NATIVE_SLOT(native_post::Calls, setFrequency, pp_set160),
    reinterpret_cast<void*>(&pp_get160),
    BF2_NATIVE_SLOT(native_post::Calls, setRoll, pp_set164),
    reinterpret_cast<void*>(&pp_get164),
    BF2_NATIVE_SLOT(native_post::Calls, setNoise, pp_set168),
    reinterpret_cast<void*>(&pp_get168),
    BF2_NATIVE_SLOT(native_post::Calls, setFilter, pp_set172),
    reinterpret_cast<void*>(&pp_get172),
    BF2_NATIVE_SLOT(native_post::Calls, setFog, pp_setStateVec4),
    reinterpret_cast<void*>(&pp_getStateVec4),
    reinterpret_cast<void*>(&pp_setRange),
    reinterpret_cast<void*>(&pp_getRange),
    BF2_NATIVE_SLOT(native_post::Calls, setBlend, pp_setBlend),
    reinterpret_cast<void*>(&pp_getBlend),
    reinterpret_cast<void*>(&pp_setMat0),
    reinterpret_cast<void*>(&pp_setMat1),
    BF2_NATIVE_SLOT(native_post::Calls, setGlow, pp_set248),
    BF2_NATIVE_SLOT(native_post::Calls, setBlur, pp_set252),
    BF2_NATIVE_SLOT(native_post::Calls, setEnabled, pp_setByte242),
    BF2_NATIVE_SLOT(native_post::Calls, setCapture, pp_setByte241),
    reinterpret_cast<void*>(&pp_getRendererFloat1852),
    reinterpret_cast<void*>(&pp_getRendererFloat1856),
    reinterpret_cast<void*>(&pp_setNoiseTextureName),
};
#else
const void* const kPostProdVtable[24] =
{
    reinterpret_cast<void*>(&pp_dtor),                    // +0x00
    reinterpret_cast<void*>(&pp_set160),                  // +0x04
    reinterpret_cast<void*>(&pp_get160),                  // +0x08
    reinterpret_cast<void*>(&pp_set164),                  // +0x0C
    reinterpret_cast<void*>(&pp_get164),                  // +0x10
    reinterpret_cast<void*>(&pp_set168),                  // +0x14
    reinterpret_cast<void*>(&pp_get168),                  // +0x18
    reinterpret_cast<void*>(&pp_set172),                  // +0x1C
    reinterpret_cast<void*>(&pp_get172),                  // +0x20
    reinterpret_cast<void*>(&pp_setStateVec4),            // +0x24
    reinterpret_cast<void*>(&pp_getStateVec4),            // +0x28
    reinterpret_cast<void*>(&pp_setRange),                // +0x2C
    reinterpret_cast<void*>(&pp_getRange),                // +0x30
    reinterpret_cast<void*>(&pp_setBlend),                // +0x34  gas-mask blend  <- per-frame caller
    reinterpret_cast<void*>(&pp_getBlend),                // +0x38
    reinterpret_cast<void*>(&pp_setMat0),                 // +0x3C
    reinterpret_cast<void*>(&pp_setMat1),                 // +0x40
    reinterpret_cast<void*>(&pp_set248),                  // +0x44
    reinterpret_cast<void*>(&pp_set252),                  // +0x48
    reinterpret_cast<void*>(&pp_setByte242),              // +0x4C
    reinterpret_cast<void*>(&pp_setByte241),              // +0x50
    reinterpret_cast<void*>(&pp_getRendererFloat1852),    // +0x54
    reinterpret_cast<void*>(&pp_getRendererFloat1856),    // +0x58
    reinterpret_cast<void*>(&pp_setNoiseTextureName),     // +0x5C
};
#endif

extern const void* const kPostProdBaseVtable[24];

struct PostProductionBase
{
    const void* const* vptr;
};

// PE 0x1004E830. The base has no members to unwind.
void* RD9_THUNK pp_baseDtor(void* self RD9_THUNK_EDX, unsigned int flags)
{
    static_cast<PostProductionBase*>(self)->vptr = kPostProdBaseVtable;
    if (flags & 1)
        ::renddx9abi::engineFree(self);
    return self;
}

// PE 0x101E2AA0: deleting entry followed by 23 runtime pure-call entries.
#if defined(_MSC_VER)
#define PP_PURE_ENTRY reinterpret_cast<const void*>(&_purecall)
#else
#define PP_PURE_ENTRY reinterpret_cast<const void*>(&__cxa_pure_virtual)
#endif
extern const void* const kPostProdBaseVtable[24] =
{
    reinterpret_cast<const void*>(&pp_baseDtor),
    PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY,
    PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY,
    PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY,
    PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY,
    PP_PURE_ENTRY, PP_PURE_ENTRY, PP_PURE_ENTRY
};
#undef PP_PURE_ENTRY


// Releases and clears the owned distortion-noise texture on device loss.
// PE 0x1004F4D0
void RD9_THUNK dl_onLost(void* self RD9_THUNK_EDX)
{
    textureReleaseRef(ownerOfDeviceLost(self)->noiseTexture);        // listener +0x1FC
}

// slot 1 onDeviceReset, only while the name is non-empty (its _Mysize at
// listener+0x214 = owner+0x218; `jbe` = unsigned, so "non-zero"): force the TextureManager's managed
// flag to 0 (sub_10017E20), reload the texture BY name (sub_10019260, the string copied by value)
// into owner+0x200, and restore the flag. The old pointer is not released first - the lost slot
// already nulled it.
// PE 0x1004E910
void RD9_THUNK dl_onReset(void* self RD9_THUNK_EDX)
{
    PostProductionOwner* const o = ownerOfDeviceLost(self);
    const std::string& name = noiseTextureName(o);                    // listener +0x200
    if (name.size() == 0)
        return;
    const int oldManaged = textureManagerSwapManaged(0);
    EngineTexture* texture;
    {
        const std::string textureName(name);
        texture = static_cast<EngineTexture*>(textureManagerGetTexture(textureName));
    }
    o->noiseTexture = texture;
    textureManagerSwapManaged(oldManaged);
}

// slot 2 (0x1004E960): nullsub in the shipped image.
void RD9_THUNK dl_slot2(void* RD9_THUNK_EDX) { }

const void* const kDeviceLostVtable[3] =
{
    reinterpret_cast<void*>(&dl_onLost),
    reinterpret_cast<void*>(&dl_onReset),
    reinterpret_cast<void*>(&dl_slot2),
};


// EventListener / IBase spine vtable (owner+0), registered for ECGameLogic by the constructor; the
// reference spine at +4/+8/+C/+10 is the recovered IBase implementation. Refcount is the dword at
// this+0x0C.


// slot 0 handleEvent (0x1004F0F0): ECGameLogic 13 copies the tinnitus setup and, for a started
// effect, arms the frame counters; (class 5, id 1) compares the setting name with GSViewDistance.
// The IEventListener::handleEvent arguments: category, id, node, userData.
int RD9_THUNK el_handleEvent(void* self RD9_THUNK_EDX, int category, unsigned int id, void* node,
                             void* userData)
{
    PostProductionOwner* const o = ownerOfEventListener(self);
    int result = category;
    if (category)
    {
        if (category == 5 && id == 1)
        {
            const std::string* s = static_cast<const std::string*>(userData);
            return strcmp(s->c_str(), "GSViewDistance") == 0 ? 1 : 0;
        }
    }
    else if (id == 13)
    {
        // `mov eax, [edx+2Ch]`: the payload pointer of the retail node. BF2 posts the setup with
        // EventManager::postEvent(ECGameLogic, 13, setup), so the node is a NotificationEventNode
        // and the pointer is its data member. [INFERRED] that +0x2C is that member in the PE layout.
        const TinnitusRecord* const payload = static_cast<const TinnitusRecord*>(
            static_cast<const NotificationEventNode*>(node)->m_data);
        if (payload)
        {
            TinnitusRecord& t = o->tinnitus;                  // +0x164..+0x174, dword copies
            copyWord(&t.state,   &payload->state);
            copyWord(&t.attack,  &payload->attack);
            copyWord(&t.hold,    &payload->hold);
            copyWord(&t.release, &payload->release);
            copyWord(&result,    &payload->volume);
            copyWord(&t.volume,  &result);
            if (t.state == 1)
            {
                int v6 = static_cast<int>((t.release + t.hold + t.attack) * 30.0f);
                if (v6 < 20) v6 = 20;
                U<28>(o) = static_cast<unsigned>(v6) + U<16>(o);
                F<32>(o) = static_cast<float>(v6);
                result = v6;
            }
        }
    }
    return result;
}

int  RD9_THUNK el_addRef (void* self RD9_THUNK_EDX) { return ++U<12>(ownerOfEventListener(self)); }
int  RD9_THUNK el_getRc  (void* self RD9_THUNK_EDX) { return   U<12>(ownerOfEventListener(self)); }

// decrement; at zero invoke the PostProductionFrameWork subobject's deleting dtor
// (owner+8 vtable slot 0), i.e. `(*(*(this+8)))(this+8, 1)`.
int RD9_THUNK el_release(void* self RD9_THUNK_EDX)
{
    PostProductionOwner* const o = ownerOfEventListener(self);
    int v = --U<12>(o);
    if (!v)
    {
        void* sub = frameWorkOf(o);
        void** vt = *reinterpret_cast<void***>(sub);
        typedef void* (__thiscall* DtorFn)(void*, int);
        reinterpret_cast<DtorFn>(vt[0])(sub, 1);
    }
    return v;
}

// queryInterface - accepts iid 1 (IBase) only.
void* RD9_THUNK el_qi(void* self RD9_THUNK_EDX, unsigned iid) { return iid != 1u ? 0 : self; }

#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Match the shared consumer interface under the native ABI.
const void* const kOwnerVtable[] =
{
    BF2_NATIVE_SLOT(::dice::hfe::IEventListener, handleEvent, el_handleEvent),
    reinterpret_cast<void*>(&el_addRef),
    reinterpret_cast<void*>(&el_getRc),
    reinterpret_cast<void*>(&el_release),
    reinterpret_cast<void*>(&el_qi),
};
#else
const void* const kOwnerVtable[5] =
{
    reinterpret_cast<void*>(&el_handleEvent),
    reinterpret_cast<void*>(&el_addRef),
    reinterpret_cast<void*>(&el_getRc),
    reinterpret_cast<void*>(&el_release),
    reinterpret_cast<void*>(&el_qi),
};
#endif

// sub_1004E860 - the effect defaults. The constructor runs it after the acquisition block and the
// preset switch (sub_1004F2B0) before every new preset, so the zero stores matter: TV, distortion,
// glow-queued, gradient, gas-mask blend and flashbang state are all cleared. Shipped store order.
void applySecondaryDefaults(PostProductionOwner* owner)
{
    U <0x0C0>(owner) = 0x3F000000u;         // 0.5
    U <0x0B0>(owner) = 0x3F000000u;         // DISTORTIONFREQ 0.5
    U <0x0B4>(owner) = 0x3F000000u;         // DISTORTIONSCALE 0.5
    Bt<0x025>(owner) = 0;                   // glow queued
    Bt<0x026>(owner) = 1;
    owner->field1F0  = 0;
    Bt<0x027>(owner) = 0;                   // TV
    U <0x0AC>(owner) = 0x3DCCCCCDu;         // DISTORTIONROLL 0.1
    gradientTexture(owner) = 0;             // +0xD0 TV gradient texture
    U <0x0B8>(owner) = 0x40600000u;         // TVGRANULARITY 3.5
    U <0x0BC>(owner) = 0;                   // TVAMBIENT
    Bt<0x0D4>(owner) = 0;                   // distortion
    U <0x0D8>(owner) = 0x3EE66666u;         // distortion blur strength 0.45
    U <0x0E0>(owner) = 0;                   // gas-mask blend
    Bt<0x0E5>(owner) = 0;
    U <0x0E8>(owner) = 0x3F333333u;         // 0.7
    U <0x0EC>(owner) = 0x40200000u;         // 2.5
    U <0x0F0>(owner) = 0x40400000u;         // 3.0
    U <0x0F4>(owner) = 0x40200000u;         // 2.5
    U <0x100>(owner) = 0;                   // flashbang glow
    U <0x104>(owner) = 0;                   // flashbang blur
    Bt<0x0FA>(owner) = 0;                   // flashbang
    Bt<0x0F9>(owner) = 0;                   // flashbang capture
    U <0x0FC>(owner) = 0;
}

// The published frame work's float setter at table byte offset `Slot` (retail this = owner+8).
template<unsigned int Slot> void callFrameWorkFloatSetter(PostProductionOwner* o, float value)
{
    typedef void (RD9_THUNK* SetFn)(void* RD9_THUNK_EDX, FloatArg);
    void* const self = frameWorkOf(o);
    const SetFn fn = reinterpret_cast<SetFn>(const_cast<void*>(o->frameWorkVtable[Slot / 4]));
#if defined(_M_IX86)
    FloatArg bits;
    memcpy(&bits, &value, sizeof bits);
    fn(self RD9_THUNK_ARG, bits);
#else
    fn(self RD9_THUNK_ARG, value);
#endif
}

// A post-production preset the mode selector hands to the owner (sub_1004F2B0). Base table
// 0x101E2998: the deleting destructor 0x1004DD10, a pure apply slot and the shared empty slot
// 0x10037D40. [INFERRED] class names.
class PostProductionPreset
{
public:
    virtual ~PostProductionPreset() {}
    // Called with the owner (not the published +8 interface) when the preset becomes current.
    virtual void apply(PostProductionOwner* owner) = 0;
    virtual void unusedSlot2() {}
};

// Table 0x101E29A4, 0x24 bytes in retail: the night-vision preset. Its gradient texture (+4) and
// name (+8) are initialized by 0x1004DD80 and updated by 0x1004DDE0.
class NightVisionPreset : public PostProductionPreset
{
public:
    // PE 0x1004DD80
    NightVisionPreset() : m_gradient(0) {}
    // PE 0x1004DE80 (also 0x1004DDA0): release the gradient texture reference, end the name.
    // 0x1004DE80 is the scalar deleting destructor table 0x101E29A4 holds; 0x1004DDA0 is the same
    // body as the complete-object destructor, unreachable in 1.50: no code or data references.
    ~NightVisionPreset()
    {
        EngineTexture* gradient = m_gradient;
        if (gradient)
            textureReleaseRef(gradient);
    }
    // 0x1004DD40: TV on with the gradient, distortion on, and roll, frequency and scale cleared
    // through the published interface's +0x0C / +0x14 / +0x1C setters.
    void apply(PostProductionOwner* o)
    {
        Bt<0x27>(o) = 1;
        gradientTexture(o) = m_gradient;
        Bt<0xD4>(o) = 1;
        callFrameWorkFloatSetter<0x0C>(o, 0.0f);
        callFrameWorkFloatSetter<0x14>(o, 0.0f);
        callFrameWorkFloatSetter<0x1C>(o, 0.0f);
    }

private:
    EngineTexture* m_gradient;              // +0x04
    std::string    m_name;                  // +0x08

public:
    // PE 0x1004DDE0. Reuse an existing texture when reupload succeeds, otherwise load the name.
    void setGradient(const std::string& name)
    {
        if (name != m_name)
        {
            // shipped defect: the comparison uses the stored prefixed name against the raw input.
            m_name = std::string("common/textures/") + name;
            if (!textureManagerReupload(m_name, m_gradient))
                m_gradient = static_cast<EngineTexture*>(textureManagerGetTexture(m_name));
        }
    }
};

// Table 0x101E2A68, 4 bytes: the vehicle-camera TV preset.
class TvPreset : public PostProductionPreset
{
public:
    // 0x100517F0: TV on, white TV colour, no gradient.
    void apply(PostProductionOwner* o)
    {
        Bt<0x27>(o) = 1;
        U<0xC4>(o) = 0x3F800000u;
        U<0xC8>(o) = 0x3F800000u;
        U<0xCC>(o) = 0x3F800000u;
        gradientTexture(o) = 0;
    }
};

template<class T> T* newPreset()
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(T));
    return storage ? new (storage) T() : 0;
}

// The deleting destructor through table slot 0, as 0x1004DEE0 calls it.
void deletePreset(PostProductionPreset* preset)
{
    preset->~PostProductionPreset();
    ::renddx9abi::engineFree(preset);
}

// sub_1004F2B0 - make `preset` current: on a change, restore the effect defaults, store it at
// +0x220 and let it apply itself to the owner.
void setPreset(PostProductionOwner* o, PostProductionPreset* preset)
{
    if (o->preset == preset)
        return;
    applySecondaryDefaults(o);
    o->preset = preset;
    if (preset)
        preset->apply(o);
}

// dword_1023F3D8, 20 bytes in retail (DX9RendModule::init 0x100519EF): picks the preset each frame.
// [INFERRED] name; RenderQueues.cpp declares the global as g_renderTargetSystem.
struct PostProductionModeSelector
{
    PostProductionPreset* nightVisionPreset;    // +0x00
    unsigned char         nightVision;          // +0x04 renderer.nightVisionEnabled (0x1004DF90)
    PostProductionPreset* tvPreset;             // +0x08
    void*                 hudManager;           // +0x0C the "HudManager" singleton, looked up once
    unsigned char         firstFrame;           // +0x10
};
#if defined(_M_IX86)
static_assert(sizeof(PostProductionModeSelector) == 0x14, "DX9RendModule::init allocates 20 bytes");
#endif

} // anonymous namespace

// 0x1004E100 after DX9RendModule::init's 20-byte allocation: both presets, night vision off, the
// HudManager singleton (null when the HUD is not registered yet) and the first-frame byte.
void* postProductionModeSelectorCreate()
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(PostProductionModeSelector));
    if (!storage)
        return 0;
    PostProductionModeSelector* const s = static_cast<PostProductionModeSelector*>(storage);
    s->nightVisionPreset = newPreset<NightVisionPreset>();
    s->nightVision = 0;
    s->tvPreset = newPreset<TvPreset>();
    s->hudManager = classManager->getSingleton(renddx9abi::HostString("HudManager"));
    s->firstFrame = 1;
    ::renddx9abi::logf("PostProductionModeSelector created, HudManager 0x%08X",
                       ::renddx9abi::addr(s->hudManager));
    return s;
}

// 0x1004DEE0 then the free in DX9RendModule::close (0x10051A50..0x10051A6B).
void postProductionModeSelectorDestroy(void* selector)
{
    PostProductionModeSelector* const s = static_cast<PostProductionModeSelector*>(selector);
    if (s->nightVisionPreset)
    {
        deletePreset(s->nightVisionPreset);
        s->nightVisionPreset = 0;
    }
    if (s->tvPreset)
    {
        deletePreset(s->tvPreset);
        s->tvPreset = 0;
    }
    ::renddx9abi::engineFree(s);
}

// sub_1004DF30, early in every renderFrame: the vehicle HUD's post-processing (a TV-guided weapon
// view) selects the TV preset, else night vision selects its preset, else none. Neither the
// vehicle information object nor the owner is null-tested.
void postProductionModeSelectorUpdate(void* selector)
{
    PostProductionModeSelector* const s = static_cast<PostProductionModeSelector*>(selector);
    PostProductionOwner* const o = publishedOwner();
    if (s->hudManager)
    {
        hud::HudVehicleInformation* const vehicle =
            static_cast<hud::BF2HudManager*>(s->hudManager)->getVehicleInformation();    // +0x24C
        if (vehicle->updatePostProcessing())                                              // +0x20
        {
            setPreset(o, s->tvPreset);
            return;
        }
    }
    setPreset(o, s->nightVision ? s->nightVisionPreset : 0);
}

// PE 0x1004F3A0. Unreachable in 1.50 (and in RendDX9_r, PostProductionFrameWork.cpp): no code or
// data references. A content tool: a 256x256 dynamic A8R8G8B8 texture in the default pool whose
// texel (x, y) is 0xFF00GGBB with BB = sin(y * 2pi/256) * 127 + 128 and GG = cos(x * 2pi/256) *
// 127 + 128 (truncated), locked with D3DLOCK_DISCARD and written row by row at Pitch / 4 dwords,
// saved as DDS to "mods/xpack/common/textures/Sine.dds", released, and then
// "common/textures/sine" loaded through the TextureManager. No HRESULT is tested (RendDX9_r
// asserts each one). [INFERRED] name. [CHOSEN, not recovered] Return type: the listing leaves the
// TextureManager's answer in eax at the `ret`, which a void body would leave too; no caller in any
// image exposes which, so the answer is returned.
void* postProductionCreateSineTexture()
{
    IDirect3DDevice9* const device = reinterpret_cast<RendDX9Base*>(g_Renderer)->device;
    IDirect3DTexture9* texture = 0;
    D3DXCreateTexture(device, 0x100, 0x100, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                      &texture);
    D3DLOCKED_RECT locked;
    texture->LockRect(0, &locked, 0, D3DLOCK_DISCARD);
    const float kStep = 0.0245436933f;                       // 0x3CC90FDB, 2pi / 256
    for (int y = 0; y < 0x100; ++y)
    {
#if !defined(_MSC_VER)
        // [CHOSEN, not recovered] fsin and the scale stay in the x87 register before _ftol2.
        const unsigned int blue = static_cast<unsigned int>(bf2x87::toIntTruncate(bf2x87::add(
            bf2x87::mul(bf2x87::sinValue(bf2x87::mul(bf2x87::wide(y), bf2x87::wide(kStep))),
                        bf2x87::wide(127.0f)),
            bf2x87::wide(128.0f)))) & 0xFFu;
#else
        const unsigned int blue = static_cast<unsigned int>(static_cast<int>(
            std::sin(static_cast<double>(y) * kStep) * 127.0 + 128.0)) & 0xFFu;
#endif
        for (int x = 0; x < 0x100; ++x)
        {
#if !defined(_MSC_VER)
            const unsigned int green = static_cast<unsigned int>(bf2x87::toIntTruncate(bf2x87::add(
                bf2x87::mul(bf2x87::cosValue(bf2x87::mul(bf2x87::wide(x), bf2x87::wide(kStep))),
                            bf2x87::wide(127.0f)),
                bf2x87::wide(128.0f)))) & 0xFFu;
#else
            const unsigned int green = static_cast<unsigned int>(static_cast<int>(
                std::cos(static_cast<double>(x) * kStep) * 127.0 + 128.0)) & 0xFFu;
#endif
            static_cast<unsigned int*>(locked.pBits)[locked.Pitch / 4 * y + x] =
                ((green | 0xFFFF0000u) << 8) | blue;
        }
    }
    texture->UnlockRect(0);
    D3DXSaveTextureToFileA("mods/xpack/common/textures/Sine.dds", D3DXIFF_DDS, texture, 0);
    texture->Release();
    return textureManagerGetTexture(std::string("common/textures/sine"));
}

// sub_1004DF10, after beginScene: on the first frame only, clear the byte and tail-call 0x1004F390
// on the owner, whose body is a bare `ret`.
void postProductionModeSelectorFrameStart(void* selector)
{
    PostProductionModeSelector* const s = static_cast<PostProductionModeSelector*>(selector);
    if (s->firstFrame)
        s->firstFrame = 0;
}


// Initializes the preallocated owner (postProductionFrameWorkOwnerSize bytes) and returns it: the
// scalar defaults, the PostProduction effect and its 29 parameter handles, the ECGameLogic
// registration, the three helper textures and the device-lost registration. Requires the
// ShaderManager, the TextureManager and the base renderer (its device-lost list).

void* postProductionFrameWorkOwnerCtor(void* mem)
{
    // The shipped ctor sets every field explicitly; we memset first and then write only the non-zero
    // finals, which is the identical resulting state.
    memset(mem, 0, sizeof(PostProductionOwner));
    PostProductionOwner* const o = static_cast<PostProductionOwner*>(mem);

    // The three interface vtables (owner+0 / +4 / +8).
    o->listenerVtable   = kOwnerVtable;
    o->deviceLostVtable = kDeviceLostVtable;
    o->frameWorkVtable  = kPostProdVtable;

    U<12>(o) = 1u;                        // this+3  refcount

    // this+10..16 (byte 40..64): the ctor sets these twice; the final values are written here.
    U<40>(o) = 1065353216u;               // 1.0f
    U<44>(o) = 1065353216u;
    U<48>(o) = 1065353216u;
    U<52>(o) = 1133903872u;
    U<56>(o) = 1140457472u;
    U<60>(o) = 1065353216u;
    // owner+64 (this+16): set 0.5f then 0 -> final 0 (memset).

    // this+17..22 (byte 68..88): the "final" second-pass values.
    U<68>(o) = 1050589266u;
    U<72>(o) = 1047233823u;
    U<76>(o) = 1034952901u;
    U<80>(o) = 1050589266u;
    U<84>(o) = 1047233823u;
    U<88>(o) = 1034952901u;

    // this+23..30 (byte 92..120): v2 block + trailing scalars (finals).
    U< 96>(o) = 1077936128u;              // v2[1]
    U<100>(o) = 1065353216u;              // v2[2]
    U<112>(o) = 1106247680u;              // this+28
    U<116>(o) = 1065353216u;              // this+29

    // this+31..51 (byte 124..204).
    U<124>(o) = 1045352145u;
    U<128>(o) = 1045352145u;
    U<132>(o) = 1056997505u;
    U<136>(o) = 1092616192u;
    U<140>(o) = 1132068864u;
    U<144>(o) = 1065353216u;
    U<152>(o) = 1092616192u;
    U<156>(o) = 1148846080u;
    U<168>(o) = 1028443341u;              // this+42
    U<172>(o) = 1036831949u;              // this+43
    U<176>(o) = 1056964608u;              // this+44
    U<180>(o) = 1056964608u;              // this+45
    U<192>(o) = 1056964608u;              // this+48

    // Byte flags set by the ctor (this+220 and this+228 both == 1).
    Bt<220>(o) = 1;
    Bt<228>(o) = 1;

    // PE 0x1005010D..0x1005011F constructs the empty target-ID history.
    new (o->targetHistoryStorage) std::deque<unsigned int>();

    // The embedded std::string at this+129 (byte 516; a 28-byte VC7.1 string in retail) - the
    // noise-texture name, empty. postProductionFrameWorkOwnerDetach destroys it.
    new (o->noiseTextureNameStorage) std::string();

    // PE 0x1005016D..0x1005068F: the effect (sub_10022EF0), then one counted handle per semantic
    // (sub_1001E420) in this order.
    o->effect = shaderManagerGetEffect(g_shaderManager, "mods/xpack/Shaders/PostProduction_nv3x");
    static const struct { unsigned int offset; const char* semantic; } kParameters[29] =
    {
        { 0x17C, "TEXLAYER0" }, { 0x180, "TEXLAYER1" }, { 0x184, "TEXLAYER2" },
        { 0x188, "TEXLAYER3" }, { 0x18C, "TEXLAYER4" }, { 0x190, "TEXLAYER5" },
        { 0x194, "TEXLAYER6" }, { 0x198, "BACKBUFFERLERPBIAS" }, { 0x19C, "SAMPLEOFFSET" },
        { 0x1A8, "FRACTIME" }, { 0x1AC, "FRACTIME256" }, { 0x1B0, "FRACSINE" },
        { 0x1B4, "INTERFERENCE" }, { 0x1B8, "DISTORTIONROLL" }, { 0x1BC, "DISTORTIONFREQ" },
        { 0x1C0, "DISTORTIONSCALE" }, { 0x1C4, "TVGRANULARITY" }, { 0x1C8, "TVAMBIENT" },
        { 0x1CC, "GLOWSTRENGTH" }, { 0x1A0, "FOGCOLOR" }, { 0x1A4, "FOGSTARTANDEND" },
        { 0x1D0, "TVCOLOR" }, { 0x1D4, "NIGHTFILTER_NOISE_STRENGTH" },
        { 0x1D8, "NIGHTFILTER_NOISE" }, { 0x1DC, "NIGHTFILTER_BLUR" },
        { 0x1E0, "NIGHTFILTER_MONO" }, { 0x1E4, "DELTAU" }, { 0x1E8, "DELTAV" },
        { 0x1EC, "DISPLACEMENT" },
    };
    for (unsigned int i = 0; i != 29; ++i)
        o->effectParameters[(kParameters[i].offset - 0x17C) / 4] =
            effectGetShaderParameterTyped(effectOf(o), std::string(kParameters[i].semantic));

    // PE 0x10050695..0x100506AD: EventManager +0x2C (ECGameLogic, owner, priority 0), then the
    // tinnitus state is cleared.
    ::dice::hfe::eventManager->registerEventHandler(
        ECGameLogic, reinterpret_cast<IEventListener*>(&o->listenerVtable), 0);
    o->tinnitus.state = 0;

    // PE 0x100506A3..0x10050708: the three helper textures by name (sub_10019260).
    o->helperTextures[0] = static_cast<EngineTexture*>(
        textureManagerGetTexture(std::string("common/textures/noise")));
    o->helperTextures[1] = static_cast<EngineTexture*>(
        textureManagerGetTexture(std::string("common/textures/random")));
    o->helperTextures[2] = static_cast<EngineTexture*>(
        textureManagerGetTexture(std::string("common/textures/sine")));

    // this+66..83 (byte 264..332): the two 3x3 matrices, set once near the end of the ctor.
    U<264>(o) = 1028443341u; U<268>(o) = 1045220557u; U<272>(o) = 1056964608u;
    U<276>(o) = 1063675494u; U<280>(o) = 1065353216u; U<284>(o) = 1063675494u;
    U<288>(o) = 1056964608u; U<292>(o) = 1045220557u; U<296>(o) = 1028443341u;
    U<300>(o) = 1028443341u; U<304>(o) = 1045220557u; U<308>(o) = 1056964608u;
    U<312>(o) = 1063675494u; U<316>(o) = 1065353216u; U<320>(o) = 1063675494u;
    U<324>(o) = 1056964608u; U<328>(o) = 1045220557u; U<332>(o) = 1028443341u;

    // The constructor's second default arm.
    applySecondaryDefaults(o);

    // the constructor's last act, 0x100507A9 `push 3` / 0x100507AB
    // `lea eax, [esi+4]` ... 0x1005080E `mov ecx, dword_1023B970; push eax; call sub_10009D60`:
    // RendDX9_registerDeviceLostResetListener(owner + 4, 3). DX9RendModule::init constructs this
    // after the Renderer (pass two of initDll), so the list at dword_1023B970+0x5C exists.
    registerDeviceLostResetListener(static_cast<void*>(&o->deviceLostVtable), 3);

    ::renddx9abi::logf("PostProductionFrameWork constructed at owner 0x%08X (singleton owner+8), "
                       "effect 0x%08X", ::renddx9abi::addr(mem), ::renddx9abi::addr(o->effect));
    return mem;
}

// The size DX9RendModule::init allocates for the owner: the retail 548 bytes on x86.
unsigned int postProductionFrameWorkOwnerSize()
{
    return static_cast<unsigned int>(sizeof(PostProductionOwner));
}

// The published PostProductionFrameWork interface inside an owner (owner + 8 on x86).
void* postProductionFrameWorkInterface(void* owner)
{
    return frameWorkOf(static_cast<PostProductionOwner*>(owner));
}

// sub_1004F1C0 - whether post-production is active, on the published owner (false while none is
// published): a noise texture, glow, a started tinnitus, TV, +0x220, flashbang or a gas-mask blend.
bool postProductionFrameWorkIsActive()
{
    PostProductionOwner* const o = publishedOwner();
    if (!o)
        return false;
    if (o->noiseTexture || Bt<36>(o))
        return true;
    return o->tinnitus.state == 1
        || Bt<39>(o)
        || o->preset
        || Bt<250>(o)
        || F<224>(o) > 0.0f;
}


// PE 0x10050830: detach listeners and release owned resources before ending member lifetimes.
void postProductionFrameWorkOwnerDetach(void* owner)
{
    PostProductionOwner* const o = static_cast<PostProductionOwner*>(owner);
    o->listenerVtable   = kOwnerVtable;
    o->deviceLostVtable = kDeviceLostVtable;
    o->frameWorkVtable  = kPostProdVtable;
    unregisterDeviceLostResetListener(static_cast<void*>(&o->deviceLostVtable));
    eventManager->unregisterEventHandler(ECGameLogic, reinterpret_cast<IEventListener*>(o));

    EngineTexture* texture = o->noiseTexture;
    textureReleaseRef(texture);
    for (unsigned int i = 0; i != 3; ++i)
    {
        texture = o->helperTextures[i];
        textureReleaseRef(texture);
    }

    // The handle order is deliberately neither ascending nor reverse construction order.
    static const unsigned char parameterOrder[29] =
    {
        6, 5, 4, 3, 2, 1, 0, 7, 8, 20, 9, 10, 11, 12, 13,
        14, 15, 17, 16, 18, 19, 21, 22, 23, 24, 25, 26, 27, 28
    };
    for (unsigned int i = 0; i != 29; ++i)
    {
        ParameterOwner* parameter = static_cast<ParameterOwner*>(
            o->effectParameters[parameterOrder[i]]);
        if (parameter)
            parameter->release();
    }
    if (o->effect)
        static_cast<IBase*>(o->effect)->release();
    noiseTextureName(o).~basic_string();
    // PE 0x100514B0, retaining source-level deque destruction rather than VC7.1 internals.
    targetHistory(o).~deque();
    o->frameWorkVtable = kPostProdBaseVtable;
}

// THE FOG RECORD: owner +0x28..+0x40 the fog colour (normalised RGB) and start/end/base/w, +0x44 /
// +0x50 the two water-fog colours (A / B), +0x5C / +0x6C the two water-fog ranges (A / B).
// Stores 0..255 RGB as normalized fog colour. Requires the post-production owner.
// Helpers use owner-relative offsets; the public singleton is its +8 subobject.
// PE 0x1004F570
void postProductionSetFogColor255(const float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    const float k = 1.0f / 255.0f;
    const float x = rgb[0] * k, y = rgb[1] * k, z = rgb[2] * k;
    F<0x30>(o) = z;
    F<0x28>(o) = x;
    F<0x2C>(o) = y;
}

// the read arm: the stored colour times 255 (0x437F0000).
// PE 0x1004F8F0
void postProductionGetFogColor255(float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    const float z = F<0x30>(o) * 255.0f, y = F<0x2C>(o) * 255.0f,
                x = F<0x28>(o) * 255.0f;
    rgb[0] = x;
    rgb[1] = y;
    rgb[2] = z;
}

// renderer.waterFogColor's write arm: the one colour, divided by 255, into both
// water-fog colour slots (B at +0x50 first, then A at +0x44).
// PE 0x1004F690
void postProductionSetWaterFogColor255(const float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    const float k = 1.0f / 255.0f;
    {
        const float x = rgb[0] * k, y = rgb[1] * k, z = rgb[2] * k;
        F<0x58>(o) = z;
        F<0x50>(o) = x;
        F<0x54>(o) = y;
    }
    {
        const float x = rgb[0] * k, y = rgb[1] * k, z = rgb[2] * k;
        F<0x4C>(o) = z;
        F<0x44>(o) = x;
        F<0x48>(o) = y;
    }
}

// its read arm reads slot B (+0x50) times 255.
// PE 0x1004F700 (also 0x1004F660). The 0x1004F660 copy is the same body as a member of the owner (receiver
// in ecx, ret 4), called from Renderer::setNightMode, which reaches it through this free function.
void postProductionGetWaterFogColor255(float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    const float z = F<0x58>(o) * 255.0f, y = F<0x54>(o) * 255.0f,
                x = F<0x50>(o) * 255.0f;
    rgb[0] = x;
    rgb[1] = y;
    rgb[2] = z;
}

// renderer.waterFogStartEndAndBase's write arm (0x1004FD50): the four floats into range B
// (+0x6C) and then range A (+0x5C); a present terrain gets its lighting-dirty byte raised.
void postProductionSetWaterFogStartEndAndBase(const float range[4])
{
    PostProductionOwner* const o = publishedOwner();
    F<0x6C>(o) = range[0];
    F<0x70>(o) = range[1];
    F<0x74>(o) = range[2];
    F<0x78>(o) = range[3];
    F<0x5C>(o) = range[0];
    F<0x60>(o) = range[1];
    F<0x64>(o) = range[2];
    F<0x68>(o) = range[3];
    if (geom::terrain())
        geom::terrain()->m_lightingDirty = 1;
}

// PE 0x1004EF70. [INFERRED] Native helper name; the returned range is borrowed.
static const float* waterFogRangeB(PostProductionOwner* owner)
{
    return &F<0x6C>(owner);
}

// its read arm (0x1004F730): range B (+0x6C) as stored.
void postProductionGetWaterFogStartEndAndBase(float range[4])
{
    const float* const source = waterFogRangeB(publishedOwner());
    range[0] = source[0];
    range[1] = source[1];
    range[2] = source[2];
    range[3] = source[3];
}

// PE 0x10051430. [INFERRED] Native helper name; inputs are copied before output stores.
static float* interpolateFogRange(float result[4], Vec4 a, Vec4 b, float t)
{
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] Preserve the x87 register values and explicit float spills.
    const bf2x87::Real bx = bf2x87::mul(bf2x87::wide(b.x), bf2x87::wide(t));
    const bf2x87::Real by = bf2x87::mul(bf2x87::wide(b.y), bf2x87::wide(t));
    const float bz = bf2x87::store(bf2x87::mul(bf2x87::wide(b.z), bf2x87::wide(t)));
    const float bw = bf2x87::store(bf2x87::mul(bf2x87::wide(b.w), bf2x87::wide(t)));
    const bf2x87::Real inverse = bf2x87::sub(bf2x87::wide(1.0f), bf2x87::wide(t));
    const bf2x87::Real ax = bf2x87::mul(bf2x87::wide(a.x), inverse);
    const float ay = bf2x87::store(bf2x87::mul(bf2x87::wide(a.y), inverse));
    const float az = bf2x87::store(bf2x87::mul(bf2x87::wide(a.z), inverse));
    const float aw = bf2x87::store(bf2x87::mul(bf2x87::wide(a.w), inverse));
    result[0] = bf2x87::store(bf2x87::add(ax, bx));
    result[1] = bf2x87::store(bf2x87::add(bf2x87::wide(ay), by));
    result[2] = bf2x87::store(bf2x87::add(bf2x87::wide(az), bf2x87::wide(bz)));
    result[3] = bf2x87::store(bf2x87::add(bf2x87::wide(aw), bf2x87::wide(bw)));
#else
    const float inverse = 1.0f - t;
    result[0] = a.x * inverse + b.x * t;
    result[1] = a.y * inverse + b.y * t;
    result[2] = a.z * inverse + b.z * t;
    result[3] = a.w * inverse + b.w * t;
#endif
    return result;
}

// PE 0x1004F760. Unreachable in 1.50: no code or data references. The calcFog coefficients of a
// {start, end, base, w} range into `result` (ecx), as postProductionUpdateFog computes them inline:
// s = 1 / (end - start), then s, -(base * s), -(start * s), end * s with s kept unspilled; w is not
// read. Answers `result`. [INFERRED] name.
float* postProductionFogRangeCoefficients(float result[4], Vec4 range)
{
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] The reciprocal stays in the x87 register for all three products.
    const bf2x87::Real s = bf2x87::div(bf2x87::wide(1.0f),
        bf2x87::sub(bf2x87::wide(range.y), bf2x87::wide(range.x)));
    result[0] = bf2x87::store(s);
    result[1] = bf2x87::store(-bf2x87::mul(bf2x87::wide(range.z), s));
    result[2] = bf2x87::store(-bf2x87::mul(bf2x87::wide(range.x), s));
    result[3] = bf2x87::store(bf2x87::mul(s, bf2x87::wide(range.y)));
#else
    const float s = 1.0f / (range.y - range.x);
    result[0] = s;
    result[1] = -(range.z * s);
    result[2] = -(range.x * s);
    result[3] = s * range.y;
#endif
    return result;
}

// PE 0x1004FDA0.
// Updates shader and device fog from camera depth below the water plane.
// Interpolates water fog over ten units; NaN depth selects the low bound.
// Fog coefficient division is unchecked when start and end coincide.
void postProductionUpdateFog(float underwaterDepth)
{
    PostProductionOwner* const o = publishedOwner();

    float t = underwaterDepth * 0.1f;
    if (t >= 1.0f)
        t = 1.0f;
    if (!(t >= 0.0f))
        t = 0.0f;
    float c[3];
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] PE 0x1004FDEA..0x1004FE42 spills all six products.
    const float bz = bf2x87::store(bf2x87::mul(bf2x87::wide(F<0x4C>(o)), bf2x87::wide(t)));
    const float by = bf2x87::store(bf2x87::mul(bf2x87::wide(F<0x48>(o)), bf2x87::wide(t)));
    const float bx = bf2x87::store(bf2x87::mul(bf2x87::wide(F<0x44>(o)), bf2x87::wide(t)));
    const bf2x87::Real inverse = bf2x87::sub(bf2x87::wide(1.0f), bf2x87::wide(t));
    const float az = bf2x87::store(bf2x87::mul(bf2x87::wide(F<0x58>(o)), inverse));
    const float ay = bf2x87::store(bf2x87::mul(bf2x87::wide(F<0x54>(o)), inverse));
    const float ax = bf2x87::store(bf2x87::mul(bf2x87::wide(F<0x50>(o)), inverse));
    c[0] = bf2x87::store(bf2x87::add(bf2x87::wide(ax), bf2x87::wide(bx)));
    c[1] = bf2x87::store(bf2x87::add(bf2x87::wide(ay), bf2x87::wide(by)));
    c[2] = bf2x87::store(bf2x87::add(bf2x87::wide(az), bf2x87::wide(bz)));
#else
    const float u = 1.0f - t;
    c[0] = F<0x50>(o) * u + F<0x44>(o) * t;
    c[1] = F<0x54>(o) * u + F<0x48>(o) * t;
    c[2] = F<0x58>(o) * u + F<0x4C>(o) * t;
#endif

    float r[4];
    const Vec4 a = {F<0x6C>(o), F<0x70>(o), F<0x74>(o), F<0x78>(o)};
    const Vec4 b = {F<0x5C>(o), F<0x60>(o), F<0x64>(o), F<0x68>(o)};
    interpolateFogRange(r, a, b, t);

    float waterRange[4];
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] PE 0x1004FE8E retains the unspilled reciprocal for all products.
    const bf2x87::Real ws = bf2x87::div(bf2x87::wide(1.0f),
        bf2x87::sub(bf2x87::wide(r[1]), bf2x87::wide(r[0])));
    waterRange[0] = bf2x87::store(ws);
    waterRange[1] = bf2x87::store(-bf2x87::mul(bf2x87::wide(r[2]), ws));
    waterRange[2] = bf2x87::store(-bf2x87::mul(bf2x87::wide(r[0]), ws));
    waterRange[3] = bf2x87::store(bf2x87::mul(bf2x87::wide(r[1]), ws));
#else
    const float ws = 1.0f / (r[1] - r[0]);
    waterRange[0] = ws;
    waterRange[1] = -(r[2] * ws);
    waterRange[2] = -(r[0] * ws);
    waterRange[3] = r[1] * ws;
#endif
    raShaderConstantsSetWaterFogRange(waterRange);
    raShaderConstantsSetWaterFogColor(c);

    if (!cameraIsUnderWater())
    {
        float rgbw[4];
        rgbw[0] = F<0x28>(o);
        rgbw[1] = F<0x2C>(o);
        rgbw[2] = F<0x30>(o);
        rgbw[3] = F<0x40>(o);
        raShaderConstantsSetFogColor(rgbw);

        float range[4];
#if !defined(_MSC_VER)
        // [CHOSEN, not recovered] PE 0x1004FF15 spills the reciprocal before these products.
        const float s = bf2x87::store(bf2x87::div(bf2x87::wide(1.0f),
            bf2x87::sub(bf2x87::wide(F<0x38>(o)), bf2x87::wide(F<0x34>(o)))));
        range[0] = s;
        range[1] = bf2x87::store(-bf2x87::mul(bf2x87::wide(s), bf2x87::wide(F<0x3C>(o))));
        range[2] = bf2x87::store(-bf2x87::mul(bf2x87::wide(s), bf2x87::wide(F<0x34>(o))));
        range[3] = bf2x87::store(bf2x87::mul(bf2x87::wide(s), bf2x87::wide(F<0x38>(o))));
#else
        const float s = 1.0f / (F<0x38>(o) - F<0x34>(o));      // 1 / (end - start)
        range[0] = s;
        range[1] = -(F<0x3C>(o) * s);
        range[2] = -(F<0x34>(o) * s);
        range[3] = F<0x38>(o) * s;
#endif
        raShaderConstantsSetFogRange(range);
    }
    else
    {
        float rgbw[4];
        rgbw[0] = c[0];
        rgbw[1] = c[1];
        rgbw[2] = c[2];
        rgbw[3] = r[3];
        raShaderConstantsSetFogColor(rgbw);
        raShaderConstantsSetFogRange(waterRange);
    }
}

// The console accessors renderer.tvInterference / tvGranularity / tvAmbient / tvColor / tvEnabled
// (owner +0xA8, +0xB8, +0xBC, +0xC4..+0xCC and the byte +0x27). tvColor copies the three floats
// unscaled (0x100581A0). Each requires the published owner.
float postProductionGetTVInterference()          { return F<0xA8>(publishedOwner()); }
void  postProductionSetTVInterference(float v)   { F<0xA8>(publishedOwner()) = v; }
float postProductionGetTVGranularity()           { return F<0xB8>(publishedOwner()); }
void  postProductionSetTVGranularity(float v)    { F<0xB8>(publishedOwner()) = v; }
float postProductionGetTVAmbient()               { return F<0xBC>(publishedOwner()); }
void  postProductionSetTVAmbient(float v)        { F<0xBC>(publishedOwner()) = v; }
void  postProductionGetTVColor(float rgb[3])
{
    memcpy(rgb, scalarBytes<0xC4, 12>(publishedOwner()), 12);
}
void  postProductionSetTVColor(const float rgb[3])
{
    memcpy(scalarBytes<0xC4, 12>(publishedOwner()), rgb, 12);
}
bool  postProductionGetTVEnabled()               { return Bt<0x27>(publishedOwner()) != 0; }
void  postProductionSetTVEnabled(bool v)         { Bt<0x27>(publishedOwner()) = v ? 1 : 0; }


// sub_1004F210 - before the scene: clear the glow-queued (+0x25) and +0xF8 bytes, draw the scene
// into the scene target (the back buffer when multisampled; the composite resolves it) and clear
// its colour to black.
void postProductionPrepare()
{
    PostProductionOwner* const o = publishedOwner();
    Bt<0xF8>(o) = 0;
    Bt<0x25>(o) = 0;
    rendererPostSetTarget(rendererPostMultisampleActive() ? 0 : rendererPostSceneTexture());
    stateManagerClear(D3DCLEAR_TARGET, 0, 1.0f, 0);                          // sub_100057E0
}

// PE 0x1004FCD0 - renderFrame's glow pass after the sky: with glow on (+0x24) and an enabled,
// non-empty RSMTGlow queue (render-queue table +0x80; queue +0x288, entries +0x27C..+0x280), mark
// the glow queued (+0x25), clear +0x7A4 through FSQuadDrawer pass 32 (sub_10085CF0(0x20, +0x7A4,
// 0, 1, 0, 0)) and flush the queue (slot +0x08 with 1). Only renderScene sets +0x24.
void postProductionPass()
{
    PostProductionOwner* const o = publishedOwner();
    if (Bt<0x24>(o) && renderQueueHasEntries(0x80))
    {
        Bt<0x25>(o) = 1;
        geom::fsQuadDrawerSourcePass(0x20, rendererPostHistoryTexture((0x7A4 - 0x778) / 4),
                                     false, true, 0, 0);
        renderQueueFlush(0x80, 1);
    }
}

// renderScene (PE 0x1002BF42..0x1002BF52): the owner's +0x24 takes the flags' post-production byte.
void postProductionSetGlowEnabled(unsigned char enabled)
{
    Bt<0x24>(publishedOwner()) = enabled;
}

namespace {

// The Renderer's render-path target at retail offset `Off` (+0x788 the scene, +0x7A0..+0x7AC the
// four post-production work targets).
template<unsigned int Off> EngineTexture* target()
{
    static_assert(Off >= 0x778 && (Off - 0x778) % 4 == 0, "not a render-path target slot");
    return rendererPostHistoryTexture((Off - 0x778) / 4);
}

// sub_1004EC30 - the glow source: unless the distortion pass already built it this frame (+0xF8),
// scale the scene down twice (passes 43 and 42, +0x788 -> +0x7A0 -> +0x7A4) and mark it built;
// then pass 30 into +0x7AC.
void buildGlowSource(PostProductionOwner* o)
{
    stateManagerPush();
    if (!Bt<0xF8>(o))
    {
        geom::fsQuadDrawerTexturePass(0x2B, target<0x788>(), target<0x7A0>(), false, 0);
        geom::fsQuadDrawerTexturePass(0x2A, target<0x7A0>(), target<0x7A4>(), false, 0);
        Bt<0xF8>(o) = 1;
    }
    geom::fsQuadDrawerTexturePass(0x1E, target<0x7A4>(), target<0x7AC>(), false, 0);
    stateManagerPop();
}

// sub_1004ECD0 - blur the glow horizontally and vertically (passes 25 and 26 through +0x7A0) and
// add it onto the scene target (pass 19).
void compositeGlow()
{
    stateManagerPush();
    geom::fsQuadDrawerTexturePass(0x19, target<0x7AC>(), target<0x7A0>(), false, 0);
    geom::fsQuadDrawerTexturePass(0x1A, target<0x7A0>(), target<0x7AC>(), false, 0);
    geom::fsQuadDrawerTexturePass(0x13, target<0x7AC>(), target<0x788>(), false, 0);
    stateManagerPop();
}

// sub_1004F510 - the glow composite, with multisampling off for its duration.
void drawGlowComposite(PostProductionOwner* o)
{
    if (rendererPostMultisampleActive())
        stateManagerSetRenderStatePersistent(D3DRS_MULTISAMPLEANTIALIAS, 0);
    buildGlowSource(o);
    compositeGlow();
    if (rendererPostMultisampleActive())
        stateManagerSetRenderStatePersistent(D3DRS_MULTISAMPLEANTIALIAS, 1);
}

// sub_1004ED60 - the night-vision distortion: on its first frame (+0xDC) seed the work target
// +0x7A8 from the scene (pass 44); hand the blur strength (+0xD8) to the FSQuadDrawer settings;
// scale the scene down (passes 43, 42; marks the glow source built, +0xF8), run pass 29 into
// +0x7A8, with glow queued (+0x25) passes 30 and 33, then 25, 26, 35 and 37 through +0x7AC /
// +0x7A4, and pass 34 back over the scene target. The pass semantics are the shader's.
void drawDistortion(PostProductionOwner* o)
{
    stateManagerPush();
    if (Bt<0xDC>(o))
    {
        geom::fsQuadDrawerTexturePass(0x2C, target<0x788>(), target<0x7A8>(), false, 0);
        Bt<0xDC>(o) = 0;
    }
    geom::fsQuadDrawerSetBlurStrength(F<0xD8>(o));
    geom::fsQuadDrawerTexturePass(0x2B, target<0x788>(), target<0x7A0>(), false, 0);
    geom::fsQuadDrawerTexturePass(0x2A, target<0x7A0>(), target<0x7A4>(), false, 0);
    Bt<0xF8>(o) = 1;
    geom::fsQuadDrawerTexturePass(0x1D, target<0x7A4>(), target<0x7A8>(), false, 0);
    if (Bt<0x25>(o))
    {
        geom::fsQuadDrawerTexturePass(0x1E, target<0x7A4>(), target<0x7AC>(), false, 0);
        geom::fsQuadDrawerTexturePass(0x21, target<0x7AC>(), target<0x7A8>(), false, 0);
    }
    geom::fsQuadDrawerTexturePass(0x19, target<0x7A8>(), target<0x7AC>(), false, 0);
    geom::fsQuadDrawerTexturePass(0x1A, target<0x7AC>(), target<0x7A4>(), false, 0);
    geom::fsQuadDrawerTexturePass(0x23, target<0x7A4>(), target<0x7AC>(), false, 0);
    geom::fsQuadDrawerTexturePass(0x25, target<0x7AC>(), target<0x7A4>(), false, 0);
    geom::fsQuadDrawerTexturePass(0x22, target<0x7A4>(), target<0x788>(), false, 0);
    stateManagerPop();
}

// sub_1004E970 - the gas mask ("WaveDistortion"), while the blend (+0xE0) is above zero: on its
// first frame (+0xE4) seed +0x7B8 from the scene; blur that (pass 18 on RAPATH above 1, else the
// 15x15 pair 11 / 12 through +0x7B0) into +0x7B4; capture the scene into +0x7B8 again; then draw
// the wave distortion over the scene target, its texel step scaled by the blend, and restore the
// caller's render target. Neither the saved target nor the view is null-tested.
void drawGasMask(PostProductionOwner* o)
{
    IDirect3DSurface9* const saved = stateManagerGetRenderTarget(0);   // sub_10025A70(0)
    saved->AddRef();
    stateManagerPush();
    if (Bt<0xE4>(o))
    {
        geom::fsQuadDrawerTexturePass(9, target<0x788>(), target<0x7B8>(), false, 0);
        Bt<0xE4>(o) = 0;
    }
    const int raPath = static_cast<int>(rendererRaPath());           // renderer +0x6CC
    if (raPath > 1)
    {
        geom::fsQuadDrawerTexturePass(0x12, target<0x7B8>(), target<0x7B4>(), false, 0);
    }
    else
    {
        geom::fsQuadDrawerTexturePass(0x0B, target<0x7B8>(), target<0x7B0>(), false, 0);
        geom::fsQuadDrawerTexturePass(0x0C, target<0x7B0>(), target<0x7B4>(), false, 0);
    }
    geom::fsQuadDrawerTexturePass(9, target<0x788>(), target<0x7B8>(), false, 0);
    geom::lightManagerSetRenderTargetTexture(0, target<0x788>());     // sub_10005920

    effectSetTechniqueByName(o->effect, std::string("WaveDistortion"));
    effectBegin(o->effect);
    const float time = static_cast<float>(rendererPostTime());
    const float* const viewport = rendererGetRenderView()->getViewport();   // +0x48, +0x14
    const float fraction = static_cast<float>(
        std::fmod(static_cast<double>(time) * 0.3f, 1.0));
    if (raPath > 1)
        parameterAt<0x180>(o)->setValue(o->helperTextures[2], 0);      // TEXLAYER1: sine
    parameterAt<0x17C>(o)->setValue(target<0x7B4>(), 0);               // TEXLAYER0
    const double strength = static_cast<double>(F<0xE0>(o)) * 10.0f;
    parameterAt<0x1E4>(o)->setFloat(static_cast<float>(
        1.0f / (static_cast<double>(viewport[2]) * 0.25f) * strength));     // DELTAU
    parameterAt<0x1E8>(o)->setFloat(static_cast<float>(
        1.0f / (static_cast<double>(viewport[3]) * 0.25f) * strength));     // DELTAV
    parameterAt<0x198>(o)->setValue(&F<0xE0>(o), 0);                   // BACKBUFFERLERPBIAS
    parameterAt<0x1A8>(o)->setFloat(fraction);                         // FRACTIME
    effectBeginPass(o->effect, 0);
    geom::fsQuadDrawerDrawCurrentEffect(raPath > 1 ? 1u : 0u);
    effectEnd(o->effect);
    stateManagerPop();
    stateManagerSetRenderTarget(0, saved);                            // StateManager +0x60
    saved->Release();
}

// dword_10238668 (.data, initially 17): the next history target the flashbang captures into,
// cycling through 17..20. [INFERRED] name.
unsigned int g_flashbangCaptureTarget = 17;

// sub_10050AE0 - the flashbang, while +0xFA is set. The FSQuadDrawer settings are swapped for the
// flashbang's own (glow weights from the two 3x3 matrices +0x108 / +0x12C, high-pass gate 0.2,
// blur strength +0x100) and restored at the end. A new flash (+0xF9) restarts the history with
// four copies of target 17. Every 0.05 (+0xFC accumulates renderer +0x5E8) one more frame is
// captured into the history through passes 9, 41, 25 and 26. The "Flashbang" technique then blends
// the four history targets over the scene with BACKBUFFERLERPBIAS = +0x104.
void drawFlashbang(PostProductionOwner* o)
{
    stateManagerPush();
    IDirect3DSurface9* const saved = stateManagerGetRenderTarget(0);   // sub_10025A70(0)
    saved->AddRef();

    const geom::FSQuadDrawerSettings original = geom::fsQuadDrawerGetSettings();   // sub_100845F0
    geom::FSQuadDrawerSettings flash;
    memcpy(&flash.weights[0], scalarBytes<0x108, 0x24>(o), 0x24);
    memcpy(&flash.weights[9], scalarBytes<0x12C, 0x24>(o), 0x24);
    flash.highPassGate = 0.2f;
    copyWord(&flash.blurStrength, &U<0x100>(o));
    geom::fsQuadDrawerSetSettings(flash);                             // sub_10084E60

    std::deque<unsigned int>& history = targetHistory(o);
    if (Bt<0xF9>(o))
    {
        g_flashbangCaptureTarget = 17;
        Bt<0xF9>(o) = 0;
        U<0xFC>(o) = 0x3D4CCCCDu;                                     // 0.05
        history.clear();                                              // sub_100514B0
        for (int i = 4; i != 0; --i)
            history.push_front(g_flashbangCaptureTarget);             // sub_10051740
    }
    // `fst` keeps the unrounded sum for the compare; NaN does not capture.
    const double elapsed = rendererTailDouble(0x5E8) + F<0xFC>(o);
    F<0xFC>(o) = static_cast<float>(elapsed);
    if (elapsed >= 0.05f)
    {
        EngineTexture* const capture = rendererPostHistoryTexture(g_flashbangCaptureTarget);
        geom::fsQuadDrawerTexturePass(9, target<0x788>(), capture, false, 0);
        geom::fsQuadDrawerTexturePass(0x29, rendererPostHistoryTexture(g_flashbangCaptureTarget),
                                      target<0x7A4>(), false, 0);
        geom::fsQuadDrawerTexturePass(0x19, target<0x7A4>(), target<0x7AC>(), false, 0);
        geom::fsQuadDrawerTexturePass(0x1A, target<0x7AC>(),
                                      rendererPostHistoryTexture(g_flashbangCaptureTarget), false, 0);
        const unsigned int id = g_flashbangCaptureTarget++;
        history.push_front(id);
        if (static_cast<int>(g_flashbangCaptureTarget) > 20)
            g_flashbangCaptureTarget = 17;
        if (!history.empty())
            history.pop_back();
        U<0xFC>(o) = 0;
    }

    geom::lightManagerSetRenderTargetTexture(0, target<0x788>());     // sub_10005920
    effectSetTechniqueByName(o->effect, std::string("Flashbang"));
    effectBegin(o->effect);
    parameterAt<0x17C>(o)->setValue(rendererPostHistoryTexture(history[0]), 0);
    parameterAt<0x180>(o)->setValue(rendererPostHistoryTexture(history[1]), 0);
    parameterAt<0x184>(o)->setValue(rendererPostHistoryTexture(history[2]), 0);
    parameterAt<0x188>(o)->setValue(rendererPostHistoryTexture(history[3]), 0);
    parameterAt<0x198>(o)->setValue(&F<0x104>(o), 0);                 // BACKBUFFERLERPBIAS
    effectBeginPass(o->effect, 0);
    geom::fsQuadDrawerDrawCurrentEffect();
    effectEnd(o->effect);
    geom::fsQuadDrawerSetSettings(original);
    stateManagerPop();
    stateManagerSetRenderTarget(0, saved);                            // StateManager +0x60
    saved->Release();
}

// sub_10050EE0 - capture the scene into the next quarter-size history target (17 + the +0x14
// cursor), push its id to the front of the four-entry history and advance the counters; +0x18
// becomes 1 once four frames are held. Without an FSQuadDrawer only the frame count (+0x10) moves.
void captureTinnitusFrame(PostProductionOwner* o)
{
    if (!geom::g_FSQuadDrawer)
    {
        ++U<0x10>(o);
        return;
    }
    const unsigned int id = U<0x14>(o) + 17;
    geom::fsQuadDrawerCaptureTinnitus(rendererPostSceneTexture(), rendererPostHistoryTexture(id));
    std::deque<unsigned int>& history = targetHistory(o);
    history.push_front(id);                                                   // sub_10051740
    if (history.size() > 4)
        history.pop_back();
    ++U<0x14>(o);
    ++U<0x10>(o);
    if (U<0x14>(o) >= 4)
    {
        Bt<0x18>(o) = 1;
        U<0x14>(o) = 0;
    }
}

// sub_10050F80 - the tinnitus pass. Without the effect, during a flashbang (+0xFA) or once the
// frame count reaches its end (+0x10 >= +0x1C) the effect stops (state 2). Otherwise the frame is
// captured and, once four are held, blended from the history targets with the remaining-time bias
// and the half-texel offset difference between the full and quarter sizes.
bool drawTinnitus(PostProductionOwner* o)
{
    if (!o->effect || Bt<0xFA>(o) || U<0x10>(o) >= U<0x1C>(o))
    {
        o->tinnitus.state = 2;
        return false;
    }
    captureTinnitusFrame(o);
    if (!Bt<0x18>(o))
        return false;

    // FSQuadDrawer sub_10083D60: 0.5 / (unsigned short)size.
    const float* const viewport = rendererGetRenderView()->getViewport();
    const unsigned short width  = static_cast<unsigned short>(static_cast<int>(viewport[2]));
    const unsigned short height = static_cast<unsigned short>(static_cast<int>(viewport[3]));
    float offset[2];
    const float halfWidth = static_cast<float>(0.5 / width);
    offset[0] = static_cast<float>(halfWidth - 0.5 / static_cast<unsigned short>(width >> 2));
    const float halfHeight = static_cast<float>(0.5 / height);
    offset[1] = static_cast<float>(halfHeight - 0.5 / static_cast<unsigned short>(height >> 2));
    const float bias = static_cast<float>(
        static_cast<double>(U<0x1C>(o) - U<0x10>(o)) / F<0x20>(o));

    stateManagerPush();
    effectSetTechniqueByName(o->effect, std::string("Tinnitus"));
    effectBegin(o->effect);
    const std::deque<unsigned int>& history = targetHistory(o);
    parameterAt<0x17C>(o)->setValue(rendererPostHistoryTexture(history[0]), 0);
    parameterAt<0x180>(o)->setValue(rendererPostHistoryTexture(history[1]), 0);
    parameterAt<0x184>(o)->setValue(rendererPostHistoryTexture(history[2]), 0);
    parameterAt<0x188>(o)->setValue(rendererPostHistoryTexture(history[3]), 0);
    parameterAt<0x198>(o)->setValue(&bias, 0);                        // BACKBUFFERLERPBIAS
    parameterAt<0x19C>(o)->setValue(offset, 0);                       // SAMPLEOFFSET
    effectBeginPass(o->effect, 0);
    geom::fsQuadDrawerDrawCurrentEffect();                            // sub_10085CB0(0)
    effectEnd(o->effect);
    stateManagerPop();
    return true;
}

// sub_1004F980 - the TV pass straight into the back buffer: the scene, noise and random textures,
// the TV colour, the time fractions, the six TV scalars (+0xA8..+0xBC) and a random texel
// displacement in the random texture.
void drawTV(PostProductionOwner* o)
{
    stateManagerPush();
    rendererPostSelectBackBuffer();                                   // sub_10005920(0, 0, ...)
    // +0xD0 is the night-vision preset's gradient texture (sub_1004DD40 copies it from the preset's
    // +4, which nothing in the image ever sets, so in practice it stays null).
    EngineTexture* const gradient = gradientTexture(o);
    if (gradient)
    {
        effectSetTechniqueByName(o->effect, std::string("TVEffect_Gradient_Tex"));
        parameterAt<0x188>(o)->setValue(gradient, 0);                // TEXLAYER3
    }
    else
    {
        effectSetTechniqueByName(o->effect, std::string("TVEffect"));
    }
    effectBegin(o->effect);
    parameterAt<0x17C>(o)->setValue(rendererPostSceneTexture(), 0);
    parameterAt<0x180>(o)->setValue(o->helperTextures[0], 0);         // noise
    EngineTexture* const random = o->helperTextures[1];
    parameterAt<0x184>(o)->setValue(random, 0);
    parameterAt<0x1D0>(o)->setValue(scalarBytes<0xC4, 12>(o), 0);     // TVCOLOR

    // fmod(time * 0.1f, 1.0) as a float; then its 256 multiple's fraction and sin(2 pi t).
    const float fraction = static_cast<float>(std::fmod(rendererPostTime() * 0.1f, 1.0));
    parameterAt<0x1A8>(o)->setFloat(fraction);
    const double scaled = static_cast<double>(fraction) * 256.0f;
    parameterAt<0x1AC>(o)->setFloat(static_cast<float>(scaled - static_cast<int>(scaled)));
    parameterAt<0x1B0>(o)->setFloat(
        static_cast<float>(std::sin(static_cast<double>(fraction) * 6.28318548f)));
    parameterAt<0x1B4>(o)->setFloat(F<0xA8>(o));                      // INTERFERENCE
    parameterAt<0x1B8>(o)->setFloat(F<0xAC>(o));                      // DISTORTIONROLL
    parameterAt<0x1BC>(o)->setFloat(F<0xB0>(o));                      // DISTORTIONFREQ
    parameterAt<0x1C0>(o)->setFloat(F<0xB4>(o));                      // DISTORTIONSCALE
    parameterAt<0x1C4>(o)->setFloat(F<0xB8>(o));                      // TVGRANULARITY
    parameterAt<0x1C8>(o)->setFloat(F<0xBC>(o));                      // TVAMBIENT

    const unsigned int* const dims = textureDims(random);             // sub_10014940
    float displacement[2];
    displacement[0] = static_cast<float>(static_cast<double>(static_cast<unsigned int>(std::rand())
                                                             % dims[0]) / dims[0]);
    displacement[1] = static_cast<float>(static_cast<double>(static_cast<unsigned int>(std::rand())
                                                             % dims[1]) / dims[1]);
    parameterAt<0x1EC>(o)->setValue(displacement, 0);                 // DISPLACEMENT
    effectBeginPass(o->effect, 0);
    geom::fsQuadDrawerDrawCurrentEffect();
    effectEnd(o->effect);
    stateManagerPop();
}

// sub_100511E0 - the passes of the composite, in order; true while the scene still has to be
// copied to the back buffer.
bool drawPostProduction(PostProductionOwner* o)
{
    bool copyScene = true;
    if (Bt<0xD4>(o))
        drawDistortion(o);                                            // sub_1004ED60
    else if (Bt<0x25>(o))
        drawGlowComposite(o);                                         // sub_1004F510
    if (Bt<0xFA>(o))
        drawFlashbang(o);                                             // sub_10050AE0
    if (F<0xE0>(o) > 0.0f)
        drawGasMask(o);                                               // sub_1004E970
    if (o->tinnitus.state == 1)
    {
        if (rendererPostMultisampleActive())
        {
            copyScene = false;
            rendererPostSetTarget(0);
        }
        drawTinnitus(o);
    }
    if (Bt<0x27>(o))
    {
        copyScene = false;
        drawTV(o);
    }
    // The noise texture (+0x200) over the scene target while the scene is still to be copied
    // (sub_100859F0), else straight over the back buffer (sub_10085CF0).
    EngineTexture* const noise = o->noiseTexture;
    if (noise)
    {
        if (copyScene)
            geom::fsQuadDrawerTexturePass(1, noise, target<0x788>(), true, 0);
        else
            geom::fsQuadDrawerSourcePass(1, noise, true, true, 0, 0);
    }
    return copyScene;
}

} // anonymous namespace

// sub_100512C0 - the composite after the first-person queues: with multisampling off for its
// duration, resolve the back buffer into the scene target (sub_1004F260), run the passes, copy
// the scene to the back buffer unless a pass already drew there (FSQuadDrawer sub_10085CF0(0,
// renderer +0x788, 1, 1, 0, 0)), and release the retained target.
void postProductionFinish()
{
    PostProductionOwner* const o = publishedOwner();
    if (rendererPostMultisampleActive())
    {
        stateManagerSetRenderStatePersistent(D3DRS_MULTISAMPLEANTIALIAS, 0);
        rendererPostResolveScene();
    }
    if (drawPostProduction(o))
        geom::fsQuadDrawerBlitToBackBuffer(rendererPostSceneTexture());
    if (rendererPostMultisampleActive())
        stateManagerSetRenderStatePersistent(D3DRS_MULTISAMPLEANTIALIAS, 1);
    rendererPostSetTarget(0);
}

// sub_1004EF80 - renderScene's glow pass after the sky, with glow on (+0x24): the scene's glow
// material ("GlowMaterial", GLOWSTRENGTH +0xC0) into +0x7A4, two rounds of the 15x15 blur
// (passes 11 and 12 through +0x7A8), the retained target selected again (sub_10028070), and the
// glow queued for the composite (+0x25).
void postProductionSceneGlowPass()
{
    PostProductionOwner* const o = publishedOwner();
    if (!Bt<0x24>(o))
        return;
    stateManagerPush();
    geom::lightManagerSetRenderTargetTexture(0, target<0x7A4>());     // sub_10005920
    effectSetTechniqueByName(o->effect, std::string("GlowMaterial"));
    effectBegin(o->effect);
    parameterAt<0x1CC>(o)->setFloat(F<0xC0>(o));                    // GLOWSTRENGTH
    parameterAt<0x17C>(o)->setValue(target<0x788>(), 0);             // TEXLAYER0
    effectBeginPass(o->effect, 0);
    geom::fsQuadDrawerDrawCurrentEffect();
    effectEnd(o->effect);
    stateManagerPop();
    geom::fsQuadDrawerTexturePass(0x0B, target<0x7A4>(), target<0x7A8>(), true, 0);
    geom::fsQuadDrawerTexturePass(0x0C, target<0x7A8>(), target<0x7A4>(), true, 0);
    geom::fsQuadDrawerTexturePass(0x0B, target<0x7A4>(), target<0x7A8>(), true, 0);
    geom::fsQuadDrawerTexturePass(0x0C, target<0x7A8>(), target<0x7A4>(), true, 0);
    rendererPostReselectTarget();
    Bt<0x25>(o) = 1;
}

void postProductionGetHemiState(float outState[4])
{
    pp_getStateVec4(frameWorkOf(publishedOwner()) RD9_THUNK_ARG, outState);
}

void postProductionSetHemiState(const float state[4])
{
    pp_setStateVec4(frameWorkOf(publishedOwner()) RD9_THUNK_ARG, state);
}

// PE 0x100573E0, owner byte +0x24; renderScene also sets this byte each frame.
bool postProductionGetGlowEnabled() { return Bt<0x24>(publishedOwner()) != 0; }
void postProductionSetConsoleGlowEnabled(bool value) { Bt<0x24>(publishedOwner()) = value; }
// PE 0x10057570, consumed by the GLOWSTRENGTH shader parameter.
float postProductionGetGlowStrength() { return F<0xC0>(publishedOwner()); }
void postProductionSetGlowStrength(float value) { F<0xC0>(publishedOwner()) = value; }
// PE 0x10057710, also enabled by the night-vision preset.
bool postProductionGetFakeHDREnabled() { return Bt<0xD4>(publishedOwner()) != 0; }
void postProductionSetFakeHDREnabled(bool value) { Bt<0xD4>(publishedOwner()) = value; }
// PE 0x100578B0 -> 0x1004F2F0: latch the first-frame byte when the old strength is zero.
float postProductionGetLightBlurStrength() { return F<0xD8>(publishedOwner()); }
// PE 0x1004F2F0
void postProductionSetLightBlurStrength(float value)
{
    PostProductionOwner* const o = publishedOwner();
    if (F<0xD8>(o) == 0.0f)
        Bt<0xDC>(o) = 1;
    F<0xD8>(o) = value;
}
// PE 0x10057A60: the published interface's +0x38 / +0x34 pair.
float postProductionGetTearGasStrength()
{
    PostProductionOwner* const o = publishedOwner();
    typedef float (RD9_THUNK* Fn)(void* RD9_THUNK_EDX);
    return reinterpret_cast<Fn>(const_cast<void*>(o->frameWorkVtable[0x38 / 4]))(
        frameWorkOf(o) RD9_THUNK_ARG);
}
void postProductionSetTearGasStrength(float value)
{
    callFrameWorkFloatSetter<0x34>(publishedOwner(), value);
}
// PE 0x10058630: preserve virtual dispatch and the range setter's shader/terrain side effects.
void postProductionGetAmbientIlluminationRange(float range[4])
{
    PostProductionOwner* const o = publishedOwner();
    float temporary[4];
    typedef void* (RD9_THUNK* Fn)(void* RD9_THUNK_EDX, float*);
    const void* const result = reinterpret_cast<Fn>(const_cast<void*>(o->frameWorkVtable[0x30 / 4]))(
        frameWorkOf(o) RD9_THUNK_ARG, temporary);
    memcpy(range, result, sizeof temporary);
}
void postProductionSetAmbientIlluminationRange(const float range[4])
{
    float temporary[4];
    memcpy(temporary, range, sizeof temporary);
    PostProductionOwner* const o = publishedOwner();
    typedef void (RD9_THUNK* Fn)(void* RD9_THUNK_EDX, const float*);
    reinterpret_cast<Fn>(const_cast<void*>(o->frameWorkVtable[0x2C / 4]))(
        frameWorkOf(o) RD9_THUNK_ARG, temporary);
}

// PE 0x1004E020, the mode selector's guarded forward to the night-vision preset.
void postProductionSetNightVisionGradient(const std::string& name)
{
    PostProductionModeSelector* const selector = static_cast<PostProductionModeSelector*>(g_renderTargetSystem);
    if (selector->nightVisionPreset)
        static_cast<NightVisionPreset*>(selector->nightVisionPreset)->setGradient(name);
}

// PE 0x1004F7E0 through the framework's +0x28 slot.
void postProductionGetFogState(float state[4])
{
    PostProductionOwner* const o = publishedOwner();
    float temporary[4];
    typedef void* (RD9_THUNK* Fn)(void* RD9_THUNK_EDX, float*);
    const void* const result = reinterpret_cast<Fn>(const_cast<void*>(o->frameWorkVtable[0x28 / 4]))(
        frameWorkOf(o) RD9_THUNK_ARG, temporary);
    memcpy(state, result, sizeof temporary);
}

// PE 0x1004F7A0 through the framework's +0x24 slot, including terrain invalidation.
void postProductionSetFogState(const float state[4])
{
#if defined(_MSC_VER)
    float temporary[4];
    memcpy(temporary, state, sizeof temporary);
    PostProductionOwner* const o = publishedOwner();
    typedef void (RD9_THUNK* Fn)(void* RD9_THUNK_EDX, const float*);
    reinterpret_cast<Fn>(const_cast<void*>(o->frameWorkVtable[0x24 / 4]))(
        frameWorkOf(o) RD9_THUNK_ARG, temporary);
#else
    // [CHOSEN, not recovered] Match the native table's typed setFog adapter.
    Vec4 temporary;
    memcpy(&temporary, state, sizeof temporary);
    PostProductionOwner* const o = publishedOwner();
    typedef void (*Fn)(void*, const Vec4*);
    reinterpret_cast<Fn>(const_cast<void*>(o->frameWorkVtable[0x24 / 4]))(
        frameWorkOf(o), &temporary);
#endif
}

// PE 0x1004F5F0
void postProductionGetWaterFogColorA255(float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    rgb[0] = F<0x44>(o) * 255.0f;
    rgb[1] = F<0x48>(o) * 255.0f;
    rgb[2] = F<0x4C>(o) * 255.0f;
}

// PE 0x1004F620
void postProductionSetWaterFogColorB255(const float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    const float k = 1.0f / 255.0f;
    F<0x58>(o) = rgb[2] * k;
    F<0x50>(o) = rgb[0] * k;
    F<0x54>(o) = rgb[1] * k;
}

// PE 0x1004F5B0
void postProductionSetWaterFogColorA255(const float rgb[3])
{
    PostProductionOwner* const o = publishedOwner();
    const float k = 1.0f / 255.0f;
    F<0x4C>(o) = rgb[2] * k;
    F<0x44>(o) = rgb[0] * k;
    F<0x48>(o) = rgb[1] * k;
}

// PE 0x1004EF60, copied by PE 0x1002B409..0x1002B430.
void postProductionGetWaterFogRangeA(float range[4])
{
    memcpy(range, &F<0x5C>(publishedOwner()), sizeof(float) * 4);
}

// PE 0x1004F950
void postProductionSetWaterFogRangeB(const float range[4])
{
    PostProductionOwner* const o = publishedOwner();
    F<0x70>(o) = range[1];
    F<0x6C>(o) = range[0];
    F<0x74>(o) = range[2];
    F<0x78>(o) = range[3];
}

// PE 0x1004F920
void postProductionSetWaterFogRangeA(const float range[4])
{
    PostProductionOwner* const o = publishedOwner();
    F<0x60>(o) = range[1];
    F<0x5C>(o) = range[0];
    F<0x64>(o) = range[2];
    F<0x68>(o) = range[3];
}

// PE 0x1004DFC0
bool postProductionGetNightVisionEnabled()
{
    return static_cast<PostProductionModeSelector*>(g_renderTargetSystem)->nightVision != 0;
}

// PE 0x1004DF90; the environment setter runs even when the flag did not change.
void postProductionSetNightVisionEnabled(bool enabled)
{
    static_cast<PostProductionModeSelector*>(g_renderTargetSystem)->nightVision = enabled;
    rendererSetNightMode(enabled ? 1u : 0u);
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
