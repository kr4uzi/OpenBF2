// Skinned-mesh render-path state. Registration and queryInterface both use IID_IBase.
// The primary and secondary vtables, with the listener at +0x04, are ABI boundaries.

#ifdef BF2_IMAGE_RENDDX9

#if !defined(_MSC_VER)
#include "../../RendDX9Abi/NativeVtable.h"
#endif
#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../Core/StringUtil.h"
#include <string>
#include "SkinnedMesh.h"                       // the submission seams this file defines
#include "../GeomMisc/Debug.h"
#include "../RendDX9/RendDX9Base.h"

#include <map>
#include <new>
#include <string.h>

namespace dice {
namespace hfe {
namespace geom {

extern IBase* g_SkinnedMeshRenderer;   // ModuleGeom.cpp, dword_1024B8F8

// getClassList: sub_100518A0(821015, "dice.hfe.geom.SkinnedMeshRenderer", factory). getSingletonList
// { "SkinnedMeshRenderer", &g_SkinnedMeshRenderer, iid 1, classId 821015, flags 2 }.
extern const unsigned int CID_SkinnedMeshRenderer  = 821015u;
extern const unsigned int IID_ISkinnedMeshRenderer = 1u;        // IID_IBase - the record really uses 1

// The retail object is 0x44 bytes (factory alloc, 0x100B06D0); the native one holds std
// containers and is allocated at sizeof.

// A distance-ordered submission queue: camera distance -> mesh, equal distances after the
// existing ones (multimap insert at the upper bound). The shipped nodes are 0x18 bytes.
typedef std::multimap<float, SkinnedMesh*> SkinnedMeshQueue;

// [INFERRED] enumerator spellings from the printed strings; metadata and moves prove 32 bits.
enum ShaderQuality : int { SQHigh = 0, SQMedium = 1, SQLow = 2 };

// Native containers replace the retail STL layout; offsets below describe the x86 image.
struct SkinnedMeshRenderer
{
    SkinnedMeshRenderer();
    ~SkinnedMeshRenderer();
    void setShaderQuality(ShaderQuality value);
    ShaderQuality getShaderQuality() const;

    const void**  m_vptr;          // +0x00  primary vtable (ABI)
    const void**  m_listenerVptr;  // +0x04  RenderPathListener subobject vtable (ABI)
    int           m_renderPath;    // +0x08  listener state; constructor leaves it unwritten
    int           m_refCount;      // +0x0C
    SkinnedMeshQueue m_map1;       // +0x10  the draw's sub_10123A50 queue
    SkinnedMeshQueue m_map2;       // +0x1C  [UNRESOLVED] use; the same 0x18-byte node
    SkinnedMeshQueue m_map3;       // +0x28  the draw's sub_10123A90 queue
    unsigned char m_bool;          // +0x34  primary slots 4/5
    unsigned char m_pad35[3];
    int           m_dword38;       // +0x38
    int           m_dword3C;       // +0x3C  [UNRESOLVED] - ctor does not write it
    int           m_dword40;       // +0x40  shaderQuality
};

// PE 0x10123A30 (the getter, 0x10123A40, is skinnedMeshRendererMode's read). [INFERRED] member
// names; the console supplies the type.
void SkinnedMeshRenderer::setShaderQuality(ShaderQuality value)
{
    m_dword40 = static_cast<int>(value);
}

ShaderQuality SkinnedMeshRenderer::getShaderQuality() const
{
    return static_cast<ShaderQuality>(m_dword40);
}

namespace {

// primary vtable (0x1021C928, 6 slots) - refcount at +0x0C
int RD9_THUNK skm_addRef(void* self RD9_THUNK_EDX) { return ++static_cast<SkinnedMeshRenderer*>(self)->m_refCount; }
int RD9_THUNK skm_getRc (void* self RD9_THUNK_EDX) { return   static_cast<SkinnedMeshRenderer*>(self)->m_refCount; }
int RD9_THUNK skm_release(void* self RD9_THUNK_EDX);
// queryInterface (0x10025930): iid 1 only.
void* RD9_THUNK skm_qi(void* self RD9_THUNK_EDX, unsigned int iid) { return (iid == RD9_IID_IBASE) ? self : 0; }
// PE 0x101240A0: +0x10 copies the argument's low byte to +0x34 (retn 4).
void RD9_THUNK skm_setBool(void* self RD9_THUNK_EDX, unsigned char v) { static_cast<SkinnedMeshRenderer*>(self)->m_bool = v; }
// PE 0x101240B0: +0x14 returns the stored byte in al.
unsigned char RD9_THUNK skm_getBool(void* self RD9_THUNK_EDX) { return static_cast<SkinnedMeshRenderer*>(self)->m_bool; }

// secondary vtable (0x1021C910, RenderPathListener, 6 slots). `self` is the subobject pointer at
// object+0x04, so the object is `self - 4` and the integer render path is object+0x08.
inline SkinnedMeshRenderer* objOfListener(void* self)
{
    return reinterpret_cast<SkinnedMeshRenderer*>(
        reinterpret_cast<char*>(self) - offsetof(SkinnedMeshRenderer, m_listenerVptr));
}
// PE 0x101C43C0 / 0x101A9A10: store/read the listener's +4 dword.
void RD9_THUNK skm_setRenderPath(void* self RD9_THUNK_EDX, int path) { objOfListener(self)->m_renderPath = path; }
int  RD9_THUNK skm_getRenderPath(void* self RD9_THUNK_EDX)           { return objOfListener(self)->m_renderPath; }
void  RD9_THUNK skm_lisNull1(void* RD9_THUNK_EDX)      { }   // +0x04 nullsub_15
void  RD9_THUNK skm_lisNull2(void* RD9_THUNK_EDX, int) { }   // +0x08 nullsub_53
// PE 0x101239B0. The path word is unused; type 7 retains the error-alias policy.
void RD9_THUNK skm_lisPathInvalid(void* RD9_THUNK_EDX, int)
{
    ::renddx9abi::Debug(DebugTypeError2,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\SkinnedMeshRenderer.cpp", 29, "")
        << "This path is invalid";
}
// PE 0x10124080. Returns the listener name by value through the compatibility output pointer.
void* RD9_THUNK skm_lisGetName(void* RD9_THUNK_EDX, void* out)
{
    new (out) std::string("SkinnedMeshRenderer");
    return out;
}

const void* const kPrimaryVtable[6] =
{
    reinterpret_cast<void*>(&skm_addRef),         // +0x00
    reinterpret_cast<void*>(&skm_getRc),          // +0x04
    reinterpret_cast<void*>(&skm_release),        // +0x08
    reinterpret_cast<void*>(&skm_qi),             // +0x0C
    reinterpret_cast<void*>(&skm_setBool),        // +0x10
    reinterpret_cast<void*>(&skm_getBool),        // +0x14
};

const void* const kListenerVtable[6] =
{
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] Let the compiler supply native integer and string returns.
    BF2_NATIVE_SLOT(dx9rend::RenderPathListener, onRenderPathActivated, skm_setRenderPath),
    BF2_NATIVE_SLOT(dx9rend::RenderPathListener, onRenderPathDeactivated, skm_lisNull1),
    BF2_NATIVE_SLOT(dx9rend::RenderPathListener, onRenderPathChanged, skm_lisNull2),
    BF2_NATIVE_SLOT(dx9rend::RenderPathListener, onRenderPathNotification, skm_lisPathInvalid),
    BF2_NATIVE_SLOT(dx9rend::RenderPathListener, getCurrentRenderPath, skm_getRenderPath),
    BF2_NATIVE_SLOT(dx9rend::RenderPathListener, getRenderPathListenerName, skm_lisGetName),
#else
    reinterpret_cast<void*>(&skm_setRenderPath),  // +0x00
    reinterpret_cast<void*>(&skm_lisNull1),       // +0x04  nullsub
    reinterpret_cast<void*>(&skm_lisNull2),       // +0x08  nullsub
    reinterpret_cast<void*>(&skm_lisPathInvalid), // +0x0C  "This path is invalid"
    reinterpret_cast<void*>(&skm_getRenderPath),  // +0x10  getRenderPath (inherited shape)
    reinterpret_cast<void*>(&skm_lisGetName),     // +0x14  getName
#endif
};

// PE 0x101240C0. Preserve the decremented result across destruction and freeing.
int RD9_THUNK skm_release(void* self RD9_THUNK_EDX)
{
    SkinnedMeshRenderer* p = static_cast<SkinnedMeshRenderer*>(self);
    const int remaining = --p->m_refCount;
    if (remaining == 0)
    {
        p->~SkinnedMeshRenderer();
        ::renddx9abi::engineFree(self);
    }
    return remaining;
}

} // anonymous namespace

// PE 0x10123B80. Tables/refcount precede the three queues; registration follows the tail stores.
SkinnedMeshRenderer::SkinnedMeshRenderer()
    : m_vptr(const_cast<const void**>(kPrimaryVtable)),
      m_listenerVptr(const_cast<const void**>(kListenerVtable)),
      m_refCount(1), m_bool(1), m_dword38(0), m_dword40(0)
{
    dx9rend::registerRenderPathListener(static_cast<void*>(&m_listenerVptr));
}

// PE 0x10123AD0. Restore both tables, unregister, then destroy queues 3, 2 and 1.
SkinnedMeshRenderer::~SkinnedMeshRenderer()
{
    m_vptr = const_cast<const void**>(kPrimaryVtable);
    m_listenerVptr = const_cast<const void**>(kListenerVtable);
    dx9rend::unregisterRenderPathListener(static_cast<void*>(&m_listenerVptr));
}

// RefFactory_geom_SkinnedMeshRenderer, 0x100B06D0.
IBase* __fastcall createSkinnedMeshRenderer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(SkinnedMeshRenderer));
    SkinnedMeshRenderer* const self = storage ? ::new (storage) SkinnedMeshRenderer : 0;

    // Shipped defect: allocation failure still enters virtual queryInterface through null.
    IBase* const instance = reinterpret_cast<IBase*>(self);
    void* itf = instance->queryInterface(iid);
    if (!itf)
        instance->release();
    return static_cast<IBase*>(itf);
}

// Native draw facade for the published renderer's shader-quality getter.
int skinnedMeshRendererMode()
{
    return static_cast<int>(reinterpret_cast<const SkinnedMeshRenderer*>(
        g_SkinnedMeshRenderer)->getShaderQuality());
}

// PE 0x101240A0, primary +0x10. Renderer checks the singleton before this call.
void skinnedMeshRendererSetEnabled(unsigned char enabled)
{
    skm_setBool(g_SkinnedMeshRenderer RD9_THUNK_ARG, enabled);
}

// sub_10123A50 / sub_10123A90 - insert the mesh by its camera distance into the renderer's +0x10 /
// +0x28 queue.
void skinnedMeshRendererSubmitA(SkinnedMesh* mesh)
{
    if (!g_SkinnedMeshRenderer || !mesh)
        return;
    reinterpret_cast<SkinnedMeshRenderer*>(g_SkinnedMeshRenderer)->m_map1.insert(
        SkinnedMeshQueue::value_type(mesh->m_distanceToCamera, mesh));
}

// PE 0x10123A90
void skinnedMeshRendererSubmitB(SkinnedMesh* mesh)
{
    // PE 0x101A6A10, camera-distance accessor expanded here.
    const float distance = mesh->m_distanceToCamera;
    reinterpret_cast<SkinnedMeshRenderer*>(g_SkinnedMeshRenderer)->m_map3.insert(
        SkinnedMeshQueue::value_type(distance, mesh));
}

namespace {

const char kSkinnedMeshRendererConsoleTag[] = "geom.skinnedMeshRenderer";

// PE 0x100B0D77: do not substitute the published global for the named singleton lookup.
SkinnedMeshRenderer* consoleSkinnedMeshRenderer()
{
    IBase* const found = classManager->getSingleton(::renddx9abi::HostString("SkinnedMeshRenderer"));
    return static_cast<SkinnedMeshRenderer*>(static_cast<void*>(found));
}

// PE 0x100B0F53. Unrecognised stored values write no text.
std::ostream& __fastcall operator<<(std::ostream& out, ShaderQuality value)
{
    switch (value)
    {
    case SQHigh: out << "SQHigh"; break;
    case SQMedium: out << "SQMedium"; break;
    case SQLow: out << "SQLow"; break;
    }
    return out;
}

// PE 0x100B1494. Substring matches precede numeric aliases; unknown tokens select High.
std::istream& __fastcall operator>>(std::istream& in, ShaderQuality& value)
{
    std::string token;
    in >> token;
    if (lowerCaseFind(token, "High") != std::string::npos || token == "0")
        value = SQHigh;
    else if (lowerCaseFind(token, "Medium") != std::string::npos || token == "1")
        value = SQMedium;
    else if (lowerCaseFind(token, "Low") != std::string::npos || token == "2")
        value = SQLow;
    else
        value = SQHigh;
    return in;
}

BF2_CONSOLE_OBJECT("SkinnedMeshRenderer", 2, 0)

// getReturnValueAsString is empty unless a result is present, otherwise the enum streamed through
// the ShaderQuality inserter 0x100B0F53.
class SkinnedMeshShaderQualityBinding : public ConsoleObjectBaseImpl
{
public:
    // PE 0x100BB3B8: binding constructor.
    SkinnedMeshShaderQualityBinding()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "shaderQuality";
        m_type = 1;
        m_minNoArgs = 0;
        m_maxNoArgs = 1;
        m_returnType = "ShaderQuality";
        m_hasReturnValue = false;
        m_isDynamic = true;
        m_argTypes[0] = "ShaderQuality";
        m_argNeedsConversion[0] = -1;
        consoleBindingList(kSkinnedMeshRendererConsoleTag).push_back(this);
    }
    virtual bool isObjectActive() const { return consoleSkinnedMeshRenderer() != 0; }
    // PE 0x100BB435
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = consoleSkinnedMeshRenderer()->getShaderQuality();
        }
        else if (m_argCount == 1)
        {
            const ShaderQuality value = BF2_CC_ARG(ShaderQuality, 0);
            m_hasReturnValue = false;
            consoleSkinnedMeshRenderer()->setShaderQuality(value);
        }
    }
    // PE 0x100BB4D5
    virtual void setArgFromString(int index, const std::string& value)
    {
        if (index == 1)
        {
            static ShaderQuality parsed = SQHigh;
            std::istringstream in(value);
            // The retail operator assigns High even when token extraction fails.
            in >> parsed;
            m_args[0] = reinterpret_cast<std::string*>(&parsed);
        }
    }
    // PE 0x100BB47D
    virtual std::string getReturnValueAsString() const
    {
        if (!m_hasReturnValue)
            return std::string();
        std::ostringstream out;
        out << m_returnValue;
        return out.str();
    }
    BF2_CC_COMMON_HOOKS
private:
    ShaderQuality m_returnValue;
};

SkinnedMeshShaderQualityBinding s_skinnedMeshShaderQualityBinding;

} // anonymous namespace

bool registerSkinnedMeshRendererConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(consoleBindingList(kSkinnedMeshRendererConsoleTag));
}

bool unregisterSkinnedMeshRendererConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(consoleBindingList(kSkinnedMeshRendererConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
