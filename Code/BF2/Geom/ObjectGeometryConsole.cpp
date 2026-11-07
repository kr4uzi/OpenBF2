
// Native Object.geometry bindings: GenerateLightmapForThisInstance (the active geometry's
// lightmap-generation flag) and loadMesh for compiled roads.
// Compatibility builds cannot share host ConsoleObject layouts and omit these bindings.

#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/PluginAbi.h"

#include "MeshTemplate.h"                  // IGeometryTemplate (getClassId, +0x10)
#include "RoadCompiled.h"
#include "RoadCompiledTemplate.h"          // CID_RoadCompiledTemplate
#include "ObjectDrawer.h"                  // the ObjectDrawer console's fields
#include <string.h>

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <string>
#include <sstream>
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/GeometryPartAccess.h"
#include "../Scene/Object/ObjectManager.h"

#include <string>

namespace dice {
namespace hfe {

extern IBase* objectManager;               // PluginEntry.cpp
namespace dx9rend { extern IBase* g_ObjectDrawer; }   // BF2Render.cpp - dword_1025A39C

namespace geom {

namespace {

// Stands in for the original's list (ModuleGeom's unk_102543C4, which also holds this module's other
// rows) - the tree keeps one keyed list per binding TU.
const char kObjectGeometryConsoleTag[] = "geom.object";

// IID_IGeometry - dword_1021AE58, 0x492FE0FE, read as data by the lookup below. A file constant like
// GeometryTemplateManager.cpp's and MeshTemplate.cpp's: Core/Ids.h carries the same value but also
// defines CID_RoadCompiledTemplate with internal linkage, which RoadCompiledTemplate.h declares extern.
const unsigned int kIID_IGeometry = 0x492FE0FEu;

// the active object's IGeometry component, shared by all six rows: the host
// ObjectManager (tested), its active object (+0x90, tested), then queryComponent(IID_IGeometry,
// IID_IGeometry) (+0x58).
// PE 0x100B1149
IBase* activeObjectGeometry()
{
    world::ObjectManager* const manager =
        reinterpret_cast<world::ObjectManager*>(::dice::hfe::objectManager);
    if (!manager)
        return 0;
    world::IObject* const object = manager->getActiveObject();              // +0x90
    if (!object)
        return 0;
    return object->queryComponent(kIID_IGeometry, kIID_IGeometry);          // +0x58
}

// PE 0x100B4362 / 0x100B469A: these slots return a borrowed template and an integer.
IGeometryTemplate* getObjectGeometryTemplate()
{
    IBase* const geometry = activeObjectGeometry();
    return static_cast<IGeometryTemplate*>(world::geometry_part::getTemplate(geometry));
}

int getObjectSubGeometryCount()
{
    IBase* const geometry = activeObjectGeometry();
    return world::geometry_part::getSubgeomCount(geometry);
}

// PE 0x100B45F8: the geometry is reacquired separately for each execution.
int getObjectSubGeometry()
{
    IBase* const geometry = activeObjectGeometry();
    return world::geometry_part::getSubgeom(geometry);
}

void setObjectSubGeometry(int value)
{
    IBase* const geometry = activeObjectGeometry();
    world::geometry_part::setSubgeom(geometry, value);
}

// PE 0x100B131F: template values print their name, not their address.
std::string objectGeometryValueText(IGeometryTemplate* value)
{
    std::ostringstream out;
    if (value)
        out << value->getName();
    else
        out << "No geometry template";
    return out.str();
}

std::string objectGeometryValueText(int value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

// that geometry as a RoadCompiled: only when its template (IGeometry +0x38) is a
// GeometryTemplate.RoadCompiled (getClassId +0x10 against the reflection id, read as data). The
// component is the IGeometry subobject, so the object is that member's offset lower (retail 4
// bytes, `lea eax, [esi-4]`).
// IGeometry has no typed interface in this tree; +0x38 is dispatched through the component's own
// table, as XpLightManager.cpp and ObjectDrawer.cpp do.
// PE 0x100B12F0
RoadCompiled* activeRoadCompiled()
{
    IBase* const geometry = activeObjectGeometry();
    if (!geometry)
        return 0;

    typedef IGeometryTemplate* (__thiscall* GetTemplateFn)(void*);
    IGeometryTemplate* const tmpl = reinterpret_cast<GetTemplateFn>(
        (*reinterpret_cast<void***>(geometry))[0x38 / 4])(geometry);
    if (!tmpl)
        return 0;
    if (tmpl->getClassId() != CID_RoadCompiledTemplate)
        return 0;
    return reinterpret_cast<RoadCompiled*>(reinterpret_cast<unsigned char*>(geometry)
                                           - offsetof(RoadCompiled, m_geometry));
}

// geometry.GenerateLightmapForThisInstance: the active object's geometry flag behind IGeometry
// +0xBC (get) / +0xB8 (set) - on a StaticMesh, bit 1 of its flags (StaticMesh::getFlagBit1 /
// setFlagBit1). The executor fetches the geometry again without a null check.
bool getGenerateLightmapForThisInstance()
{
    IBase* const geometry = activeObjectGeometry();
    typedef bool (__thiscall* GetFn)(void*);
    return reinterpret_cast<GetFn>(
        (*reinterpret_cast<void***>(geometry))[0xBC / 4])(geometry);
}

void setGenerateLightmapForThisInstance(bool value)
{
    IBase* const geometry = activeObjectGeometry();
    typedef void (__thiscall* SetFn)(void*, bool);
    reinterpret_cast<SetFn>(
        (*reinterpret_cast<void***>(geometry))[0xB8 / 4])(geometry, value);
}

} // anonymous namespace

// `SetConsoleClassDefaults("Object", 2, 0)` - stored as "object", which is how the .con files spell it.
BF2_CONSOLE_OBJECT("Object", 2, 0)

// Read-only rows carry type 2 and accept exactly zero arguments.
#define BF2_OBJECT_GEOMETRY_READONLY_ROW(nameLiteral, T, getter)                    \
namespace {                                                                       \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                    \
{                                                                                 \
    BF2_CC_HEAD_NAMED(nameLiteral, 2, 0, 0, #T, true)                               \
    BF2_CC_TAIL(0, kObjectGeometryConsoleTag)                                       \
    virtual bool isObjectActive() const { return activeObjectGeometry() != 0; }     \
    virtual void executeObjectMethod()                                            \
    {                                                                             \
        if (m_argCount == 0)                                                      \
        {                                                                         \
            m_hasReturnValue = true;                                              \
            m_returnValue = getter();                                             \
        }                                                                         \
    }                                                                             \
    virtual void setArgFromString(int, const std::string&) {}                      \
    virtual std::string getReturnValueAsString() const                             \
    {                                                                             \
        return m_hasReturnValue ? objectGeometryValueText(m_returnValue)           \
                                : std::string();                                  \
    }                                                                             \
    BF2_CC_COMMON_HOOKS                                                           \
private:                                                                          \
    T m_returnValue;                                                               \
};                                                                                \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                \
}

// PE 0x100B4301 / 0x100B4362, first in the shipped Object.geometry group.
BF2_OBJECT_GEOMETRY_READONLY_ROW("geometry.template", IGeometryTemplate*, getObjectGeometryTemplate)

namespace {

// PE 0x100B43E6 / 0x100B4466: setter-only type 3, with string return metadata.
class ObjectGeometryTechniqueBinding : public ConsoleObjectBaseImpl
{
public:
    // PE 0x100B43E6
    ObjectGeometryTechniqueBinding()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "geometry.technique";
        m_type = 3;
        m_minNoArgs = m_maxNoArgs = 1;
        m_returnType = "std::string";
        m_hasReturnValue = false;
        m_isDynamic = true;
        m_argTypes[0] = "std::string";
        m_argNeedsConversion[0] = 0;
        consoleBindingList(kObjectGeometryConsoleTag).push_back(this);
    }
    virtual bool isObjectActive() const { return activeObjectGeometry() != 0; }
    // PE 0x100B4466
    virtual void executeObjectMethod()
    {
        if (m_argCount != 1)
            return;
        const std::string value(BF2_CC_ARG(std::string, 0));
        m_hasReturnValue = false;
        IBase* const geometry = activeObjectGeometry();
        typedef void (__thiscall* Fn)(void*, const std::string&);
#if defined(_MSC_VER)
        reinterpret_cast<Fn>((*reinterpret_cast<void***>(geometry))[0x68 / 4])(geometry, value);
#elif defined(BF2_STATIC_PLUGINS)
        // [CHOSEN, not recovered] Native explicit tables keep the proven PE ordinal.
        // The ELF selector is unused in this guarded arm; no ELF +0x68 contract is claimed.
        world::geometry_part::slot<Fn>(geometry, 0x68, 0)(geometry, value);
#else
#error Object.geometry.technique requires an attested geometry table provider
#endif
    }
    // PE 0x100B44C5
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(std::string, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};

ObjectGeometryTechniqueBinding s_objectGeometryTechniqueBinding;

} // anonymous namespace

// PE 0x100B453A / 0x100B45F8, then PE 0x100B4645 / 0x100B469A.
// PE 0x100B45AB
BF2_CONSOLE_PROPERTY_FN_NAMED("geometry.subGeometry", int, activeObjectGeometry() != 0,
                              getObjectSubGeometry(), setObjectSubGeometry,
                              kObjectGeometryConsoleTag)
// PE 0x100B4645
BF2_OBJECT_GEOMETRY_READONLY_ROW("geometry.subGeometryCount", int, getObjectSubGeometryCount)
#undef BF2_OBJECT_GEOMETRY_READONLY_ROW

// Object.geometry.GenerateLightmapForThisInstance - a bool property, active while the active object
// has a geometry. Registered ahead of geometry.loadMesh, its neighbour in the shipped list.
// PE 0x100B46C6, 0x100B4784, 0x100B4737
BF2_CONSOLE_PROPERTY_FN_NAMED("geometry.GenerateLightmapForThisInstance", bool,
                              activeObjectGeometry() != 0, getGenerateLightmapForThisInstance(),
                              setGenerateLightmapForThisInstance, kObjectGeometryConsoleTag)

// Binds geometry.loadMesh to the active compiled road; requires one string argument.
// The console checks activity before execution; the executor repeats the lookup without
// a null guard. Keep class and constructor in one macro invocation for matching __LINE__.
#define BF2_OBJECT_GEOMETRY_LOADMESH_ROW(fnNameLiteral, listTag)                                     \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD_NAMED(fnNameLiteral, 0, 1, 1, "void", false)                                       \
            m_argTypes[0] = "std::string";                                                         \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return activeRoadCompiled() != 0; }                      \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 1) return;                                                               \
        const std::string copy(BF2_CC_ARG(std::string, 0).c_str());                               \
        m_hasReturnValue = false;                                                                  \
        activeRoadCompiled()->loadMesh(copy);                                                      \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(std::string, 1)                                                             \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

// PE 0x100B47DF, 0x100B489C, 0x100B485C
BF2_OBJECT_GEOMETRY_LOADMESH_ROW("geometry.loadMesh", kObjectGeometryConsoleTag)
#undef BF2_OBJECT_GEOMETRY_LOADMESH_ROW

namespace {

// The ObjectDrawer console object (ctors 0x100B4911 .. 0x100B4E0A), in the shipped order after
// the Object rows. Every row is active while the renderer's drawer exists (0x100B4982,
// dword_1025A39C) and its executor reads or stores one drawer field directly.
// [INFERRED] placement in this list: retail registers both objects through ModuleGeom.
ObjectDrawer* consoleObjectDrawer()
{
    return reinterpret_cast<ObjectDrawer*>(static_cast<void*>(dx9rend::g_ObjectDrawer));
}

// useOcclusion +0x60 (0x100B49DB), collectPlanesDistance +0x64 (0x100B4AD6), useExactTest +0x68
// (0x100B4BD4), faceForwardThreshold +0x6C as a float (0x100B4CCF), showCullStats +0x70
// (0x100B4DCD), drawDebugPlanes +0x88 (0x100B4EC8).
bool  getUseOcclusion() { return consoleObjectDrawer()->m_byte60 != 0; }
void  setUseOcclusion(bool v) { consoleObjectDrawer()->m_byte60 = v; }
float getCollectPlanesDistance() { return consoleObjectDrawer()->m_float64; }
void  setCollectPlanesDistance(float v) { consoleObjectDrawer()->m_float64 = v; }
bool  getUseExactTest() { return consoleObjectDrawer()->m_byte68 != 0; }
void  setUseExactTest(bool v) { consoleObjectDrawer()->m_byte68 = v; }
float getFaceForwardThreshold()
{
    float v;
    ::memcpy(&v, &consoleObjectDrawer()->m_dword6C, sizeof(v));
    return v;
}
void  setFaceForwardThreshold(float v) { ::memcpy(&consoleObjectDrawer()->m_dword6C, &v, sizeof(v)); }
bool  getShowCullStats() { return consoleObjectDrawer()->m_byte70 != 0; }
void  setShowCullStats(bool v) { consoleObjectDrawer()->m_byte70 = v; }
bool  getDrawDebugPlanes() { return consoleObjectDrawer()->m_byte88 != 0; }
void  setDrawDebugPlanes(bool v) { consoleObjectDrawer()->m_byte88 = v; }

BF2_CONSOLE_OBJECT("ObjectDrawer", 2, 0)
// PE 0x100B498E
BF2_CONSOLE_PROPERTY_FN(useOcclusion, bool, dx9rend::g_ObjectDrawer != 0,
                        getUseOcclusion(), setUseOcclusion, kObjectGeometryConsoleTag)
// PE 0x100B4A18, 0x100B4A89
BF2_CONSOLE_PROPERTY_FN(collectPlanesDistance, float, dx9rend::g_ObjectDrawer != 0,
                        getCollectPlanesDistance(), setCollectPlanesDistance, kObjectGeometryConsoleTag)
// PE 0x100B4B16, 0x100B4B87
BF2_CONSOLE_PROPERTY_FN(useExactTest, bool, dx9rend::g_ObjectDrawer != 0,
                        getUseExactTest(), setUseExactTest, kObjectGeometryConsoleTag)
// PE 0x100B4C11, 0x100B4C82
BF2_CONSOLE_PROPERTY_FN(faceForwardThreshold, float, dx9rend::g_ObjectDrawer != 0,
                        getFaceForwardThreshold(), setFaceForwardThreshold, kObjectGeometryConsoleTag)
// PE 0x100B4D0F, 0x100B4D80
BF2_CONSOLE_PROPERTY_FN(showCullStats, bool, dx9rend::g_ObjectDrawer != 0,
                        getShowCullStats(), setShowCullStats, kObjectGeometryConsoleTag)
// PE 0x100B4E0A, 0x100B4E7B
BF2_CONSOLE_PROPERTY_FN(drawDebugPlanes, bool, dx9rend::g_ObjectDrawer != 0,
                        getDrawDebugPlanes(), setDrawDebugPlanes, kObjectGeometryConsoleTag)

} // anonymous namespace

bool registerObjectGeometryConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kObjectGeometryConsoleTag));
}

bool unregisterObjectGeometryConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kObjectGeometryConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
