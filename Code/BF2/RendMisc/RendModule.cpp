// Registers render views, environment maps and their console bindings.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginModules.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/Frontier.h"
#include "EnvMapSystem/EnvMapManager.h"
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../../RendDX9Abi/PluginEntry.h"

namespace dice {
namespace hfe {
namespace Rend {

// Declared here rather than through a header: RenderView.cpp is the only other file in this image
// that defines it, and a one-line extern is smaller than a header for one symbol.
IBase* __fastcall createRenderView(unsigned int iid, IBase* outer);

extern const unsigned int IID_IRenderView;
extern const unsigned int CID_RenderView;

// The two globals RendModule's registrations point at - 0x10267A30 and 0x10267A34 in the shipped
// image, adjacent, which is what a pair of file-scope pointers looks like.
IBase* g_RenderView    = 0;
IBase* g_EnvMapManager = 0;

// PE 0x10051840 is the queryInterface this macro expands: the raw this for the three interface ids.
RD9_MODULE_IBASE_BODIES(RendModule)


namespace {

// PE 0x100518A0: the shipped inline constructor of a class record (id, name, factory).
ClassRegInfo makeClassRegInfo(unsigned int classId, const std::string& name,
                              decltype(ClassRegInfo::create) create)
{
    ClassRegInfo info;
    info.classId = classId;
    info.name    = name;
    info.create  = create;
    return info;
}

// PE 0x100518D0: the shipped inline constructor of a singleton record (name, instance slot, interface
// id, class id, flags).
SingletonRegInfo makeSingletonRegInfo(const std::string& name, IBase** instance, unsigned int iid,
                                      unsigned int classId, int flags)
{
    SingletonRegInfo info;
    info.name     = name;
    info.instance = instance;
    info.iid      = iid;
    info.classId  = classId;
    info.flags    = flags;
    return info;
}

} // anonymous namespace

// Publishes RenderView followed by EnvMapManager.

// PE 0x101C2408
void RendModule::getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::Rend::RendModule::getClassList", 0x10);

    ::renddx9abi::HostVectorWriter<ClassRegInfo> out(hostVector);

    // RenderView now has a body - see RendMisc/RenderView.cpp. Registering it is what turns the
    // "RenderView" singleton below from null into a real object, and it is also what
    // Renderer::initialize's createInstance(820301, 820201, 0) resolves against.
    out.push_back(makeClassRegInfo(CID_RenderView, "dice.hfe.Rend.RenderView",
                                   RD9_CLASS_FACTORY(&createRenderView)));
    out.push_back(makeClassRegInfo(820302u, "dice.hfe.Rend.EnvMapManager",
                                   RD9_CLASS_FACTORY(&createEnvMapManager)));
}


// getSingletonList, 0x101C236E. Two entries, both flags == 2, read from the
// sub_100518D0(name, instance, iid, classId, flags) call sites.
// The shipped lists build each entry with the inline record constructors - SingletonRegInfo(name,
// instance, iid, classId, flags) PE 0x100518D0 and ClassRegInfo(classId, name, create)
// PE 0x100518A0 - before the push; the lists here store the same members in place.

// The RenderView pair is corroborated from the other end: Renderer::initialize (0x1002C8C0) asks
// the host for createInstance(820301, 820201, 0) - the same classId and iid, in the argument order
// createInstance takes.

// PE 0x101C236E
void RendModule::getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::Rend::RendModule::getSingletonList", 0x14);

    ::renddx9abi::HostVectorWriter<SingletonRegInfo> out(hostVector);

    out.push_back(makeSingletonRegInfo("RenderView", &g_RenderView, IID_IRenderView, CID_RenderView, 2));
    out.push_back(makeSingletonRegInfo("EnvMapManager", &g_EnvMapManager, 820202u, 820302u, 2));
}


// Native environment-map console bindings, available while the manager exists.
// Method results are discarded; ConsoleObject layouts must match the host toolchain.

namespace {

const char kEnvMapManagerConsoleTag[] = "rend.envmapManager";

inline EnvMapManager* envMapManager()
{
    return static_cast<EnvMapManager*>(g_EnvMapManager);
}

// the argument copied, then the manager's +0x34 (loadInfo) - the renderer's own string at the
// call, converted here, once.
void consoleLoadEMIFile(const std::string& path)
{
    envMapManager()->loadInfo(std::string(path.c_str(), static_cast<unsigned int>(path.size())));
}

void consoleGenerateEnvMaps()
{
    envMapManager()->generateEnvMaps();
}

void consoleSaveEnvMaps()
{
    envMapManager()->save();
}

bool consoleGetPerPixelEnvironmentMapping()
{
    return envMapManager()->getPerPixelEnabled();
}

void consoleSetPerPixelEnvironmentMapping(bool enabled)
{
    envMapManager()->setPerPixelEnabled(enabled);
}

BF2_CONSOLE_OBJECT("envmapManager", 2, 0)

// PE 0x101C2054, 0x101C2110, 0x101C20D1 (also 0x101C2039): constructor, argument conversion, executor;
// 0x101C2039 is the shared push of the new binding onto this TU console list (0x10267A38).
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(loadEMIFile, consoleLoadEMIFile, g_EnvMapManager != 0,
                                   std::string, kEnvMapManagerConsoleTag)
// PE 0x101C2185: constructor (executor body 0x101C21D7 is part of the same Ghidra function).
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(generateEnvMaps, consoleGenerateEnvMaps, g_EnvMapManager != 0,
                                   kEnvMapManagerConsoleTag)
// PE 0x101C21F2: constructor (executor body 0x101C2244 follows it).
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(save, consoleSaveEnvMaps, g_EnvMapManager != 0,
                                   kEnvMapManagerConsoleTag)
// PE 0x101C225F, 0x101C22D0, 0x101C231D: constructor, argument conversion, executor.
BF2_CONSOLE_PROPERTY_FN(perPixelEnvironmentMappingEnabled, bool, g_EnvMapManager != 0,
                        consoleGetPerPixelEnvironmentMapping(),
                        consoleSetPerPixelEnvironmentMapping, kEnvMapManagerConsoleTag)

} // anonymous namespace

// PE 0x101C2013 (19 B), `mov ecx, dword_10244A98; push offset 0x10267A38; call
// 0x1007B570` - ConsoleObjects::registerConsoleObjects(this TU's list) - then `mov al, 1`. The native
// flavour publishes the list above; the compatibility flavour publishes no console list at all (the
// registry carries the host toolchain's STL - the same split ModuleGeom::init and DX9RendModule::init
// make), so there it answers true with nothing registered.
bool RendModule::init()
{
    RD9_TRACE("dice::hfe::Rend::RendModule::init", 0x18);
    mainConsoleObjects->registerConsoleObjects(consoleBindingList(kEnvMapManagerConsoleTag));
    return true;
}

// PE 0x101C2026 (19 B), the twin - ConsoleObjects::unregisterConsoleObjects
// (0x1007B6D0) on the same list, then true.
bool RendModule::close()
{
    RD9_TRACE("dice::hfe::Rend::RendModule::close", 0x1C);
    mainConsoleObjects->unregisterConsoleObjects(consoleBindingList(kEnvMapManagerConsoleTag));
    return true;
}

} // namespace Rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
