// [PLATFORM] The plugins' DllModule handshake with the rebuilt host.
// Name the renderer's heap category before the null-check assertion, then acquire host services.
// [INFERRED] source placement of the heap-name registration (registerMemoryName).
// [RETIRE-WHEN: RW-49] Consolidate with Core/DllModule once it can use the required per-image types.

#ifndef RENDDX9ABI_PLUGINENTRY_H
#define RENDDX9ABI_PLUGINENTRY_H

#include "HfeAbi.h"
#include "HostFileAbi.h"

namespace dice {
namespace hfe {

class ConsoleObjects;
class EventManager;

// The host's ClassManager, stored by initDll and never cleared. Defined in PluginEntry.cpp
// alongside the nineteen singleton globals; declared here because RendDX9/BF2Render.cpp needs it to
// reach hWndBf2TempSlt during construction.
extern IClassManager* classManager;

// The host's SettingsRepostitory, pulled in by initDll. RendDX9Init.cpp reads GSDeviceType,
// GSDeviceBehavior and GSRenderPath through it by raw vtable offset. Consumers may use the
// recovered class directly.
class SettingsRepostitory;
extern SettingsRepostitory* settings;

// The host's ConsoleObjects registry; its containers are the shared std types.
extern ConsoleObjects* mainConsoleObjects;

// The host's MainConsole singleton.  Kept as IBase at the cross-image boundary; consumers that
// recover a specific Console slot use PluginAbi's HostString (std::string).
extern IBase* mainConsole;

// The host's EventManager.  The singleton boundary stores the unadjusted pointer exactly as
// retail does; consumers call the recovered EventManager methods rather than indexing its table.
extern EventManager* eventManager;

// Typed host virtual filesystem, including mounted archives and mod layering.
extern ::renddx9abi::HostFileManager* fileManager;

// the module table, null-terminated. Declared here, defined by the image - see PluginEntry.cpp for
// RendDX9's four.
extern IModule* dllmodules[];

// 0x48A6E0 in the ELF. Counts to the terminator and caches, and the cache is never invalidated.
int getNoModules();

extern "C" {

// The two exports that are the entire ABI of a BF2 side-DLL. Undecorated in the shipped image -
// `dumpbin /exports RendDX9.dll` returns exactly "deinitDll" (RVA 0x74240) and "initDll"
// (RVA 0x73DA0) and nothing else.
bool initDll(IClassManager* pClsMgr);
void deinitDll();

} // extern "C"

} // namespace hfe
} // namespace dice

namespace renddx9abi {

// The image's Memory.dll name tag (PE dword_10244A8C); set by registerMemoryName.
extern int memoryNameTag;

// Stores the host's tag for the image's heap name while the tag is still zero. initDll calls it
// first, with "renderer".
void registerMemoryName(const char* name);

} // namespace renddx9abi

#endif // RENDDX9ABI_PLUGINENTRY_H
