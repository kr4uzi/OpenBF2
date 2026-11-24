// Path:     [INFERRED] - see Module.h header block.
// Contents: recovered from bf2.i64 (ELF, symbolised).

#include "Module.h"
#include "Ids.h"                          // [INFERRED] header name; the constants are attested
#include "Profiler.h"                     // [INFERRED] header name; the factories are attested
#include "Debug.h"
#include "DebugCounterManager.h"
#include "EventManager.h"                  // registerEvent (vtable +0x34) and the eventManager global
#include "ConsoleObjects.h"
#include <list>

// Uses the shared typed console registry; all declarations must agree across translation units.

namespace dice {
namespace hfe {

// Register addresses of the typed singleton globals; keep declaration types consistent across translation units.
class System;
extern System* g_system;              // ds:0x8AC0D20 - attested as _ZN4dice3hfe8g_systemE,

extern IBase* profiler;               // ds:0x8ABBCAC
extern IBase* debugCounterManager;    // ds:0x8AC86EC

// Local factory declaration for the registry; its signature must match MemoryPool's definition.
class MemoryPool   { public: static IBase* create(unsigned int, IBase*); };
// Singleton registration stores addresses without dereferencing their declared type.
// Keep the globals' concrete types and cast their addresses only at the registry boundary.

// Use Core/Debug.h's declarations for all four debug globals. Registering local IBase* shadows
// would publish permanently null slots to side DLLs. mainConsoleObjects is likewise typed by its header.

// Match LoadStat.h's global type; the singleton table takes its address.
class LoadStat;
extern LoadStat* loadStat;            // ds:0x8ABBC98 — see Core/LoadStat.h

// Accesses the typed registry that owns the core binding list.
inline ConsoleObjects* consoleObjects() { return mainConsoleObjects; }
// Returns this module's console-binding list, creating native storage on first use.
inline std::list<ConsoleObject*>& coreConsoleObjectList()
{
    static std::list<ConsoleObject*> coreList;   // [CHOSEN] a real object; the name is attested
    return coreList;
}

void Module::getClassList(std::vector<ClassRegInfo>& out)
{
    out.push_back(ClassRegInfo{CID_MemoryPool,          "dice.hfe.MemoryPool.Standard", &MemoryPool::create});
    out.push_back(ClassRegInfo{CID_EventManager,        "dice.hfe.EventManager",        &EventManager::create});
    out.push_back(ClassRegInfo{CID_Profiler,            "dice.hfe.Profiler",            &Profiler::create});
    out.push_back(ClassRegInfo{CID_DebugCounterManager, "dice.hfe.DebugCounterManager", &DebugCounterManager::create});
}

// getSingletonList - ELF 0x84F9130. The eleven foundation entries retain their recovered order.
// Flags: 1 creates during initSingletons; 2 creates on registration; 4 registers an existing slot.
void Module::getSingletonList(std::vector<SingletonRegInfo>& out)
{
    //                       name                     instance                 iid                        classId                   flags
    out.push_back(SingletonRegInfo{"System",   reinterpret_cast<IBase**>(&g_system), IID_IBase,          CID_System,               4});
    // Same reinterpret_cast as the System row above, and for the same reason: the row records an
    // address for a flags-bit-2 entry that is never constructed through, so the pointee type is
    // never used. ConsoleObjects is not an IBase at all (it is non-polymorphic), which is precisely
    // why this has to be a reinterpret_cast rather than an implicit conversion.
    out.push_back(SingletonRegInfo{"MainConsoleObjects", reinterpret_cast<IBase**>(&mainConsoleObjects), 0,      0,                        4});
    out.push_back(SingletonRegInfo{"BfGlobal_callback",     reinterpret_cast<IBase**>(&bfGlobal_callback),      0,                         0,                        4});
    out.push_back(SingletonRegInfo{"BfGlobal_callbackData", reinterpret_cast<IBase**>(&bfGlobal_callbackData),  0,                         0,                        4});
    out.push_back(SingletonRegInfo{"DebugOn",               reinterpret_cast<IBase**>(&debugOn),                0,                         0,                        4});
    out.push_back(SingletonRegInfo{"DebugModules",          reinterpret_cast<IBase**>(&debugModules),           0,                         0,                        4});
    out.push_back(SingletonRegInfo{"EventManager",     reinterpret_cast<IBase**>(&eventManager),      IID_IBase, CID_EventManager,         2});
    out.push_back(SingletonRegInfo{"FrameEventManager", reinterpret_cast<IBase**>(&frameEventManager), IID_IBase, CID_EventManager,         2});
    out.push_back(SingletonRegInfo{"Profiler",              &profiler,               IID_IProfilerClient,       CID_Profiler,             2});
    out.push_back(SingletonRegInfo{"DebugCounterManager",   &debugCounterManager,    IID_IDebugCounterManager,  CID_DebugCounterManager,  1});
    // The cast is a consequence of `loadStat`'s real type being `LoadStat*` (Core/LoadStat.h)
    // while every other row here is an `IBase*`. The registry stores an address and never
    // dereferences it through the declared type, so the cast is safe and the table is unchanged.
    // The alternative — declaring the global as IBase* to match the table — is what produced the
    // duplicate symbol the link step reported.
    out.push_back(SingletonRegInfo{"LoadStat",              reinterpret_cast<IBase**>(&loadStat), 0,            0,                        4});
}

// Core event ids 1..16 are recovered from ECCore registration calls; registration order differs from id order.
// [UNRESOLVED] No registration proves an id-zero enumerator.
// [CHOSEN, not recovered] The enum type name and file-local placement; enumerator names are attested.
enum CoreEvent
{
    CEChangedSetting        = 1,
    CEPreLoad               = 2,
    CEPostLoad              = 3,
    CEUpdateLoadStats       = 4,
    CEDrawDebugCounters     = 5,
    CEShutdownGame          = 6,
    CEResourcesLoaded       = 7,
    CEDebugTextHook         = 8,
    CECollectSystemInfo     = 9,
    CEStartPerformanceTest  = 10,
    CEStopPerformanceTest   = 11,
    CEReportPerformanceInfo = 12,
    CEUnloadLevel           = 13,
    CEAbortLoadLevel        = 14,
    CEActivateApp           = 15,
    CEPeakWinMessages       = 16
};

// [CHOSEN, not recovered] Macro spelling. Event names concatenate the category and event identifiers.
#define REGISTER_CORE_EVENT(ev) eventManager->registerEvent(ECCore, (ev), "ECCore" #ev)

// Registers core console bindings and named events in the observed order.
// close unregisters the bindings and reports success.
bool Module::init()
{
    // Publish the profiler as its client interface before any profiler-client calls can run.
    profilerClient = reinterpret_cast<IProfilerClient*>(profiler);

    consoleObjects()->registerConsoleObjects(coreConsoleObjectList());   // ds:0x8AC86F4

    REGISTER_CORE_EVENT(CEPreLoad);
    REGISTER_CORE_EVENT(CEPostLoad);
    REGISTER_CORE_EVENT(CEUpdateLoadStats);
    REGISTER_CORE_EVENT(CEShutdownGame);
    REGISTER_CORE_EVENT(CEResourcesLoaded);
    REGISTER_CORE_EVENT(CEDebugTextHook);
    REGISTER_CORE_EVENT(CECollectSystemInfo);
    REGISTER_CORE_EVENT(CEUnloadLevel);
    REGISTER_CORE_EVENT(CEAbortLoadLevel);
    REGISTER_CORE_EVENT(CEActivateApp);
    REGISTER_CORE_EVENT(CEPeakWinMessages);
    REGISTER_CORE_EVENT(CEStartPerformanceTest);
    REGISTER_CORE_EVENT(CEStopPerformanceTest);
    REGISTER_CORE_EVENT(CEReportPerformanceInfo);
#if !DEDICATED
    // BF2.exe 0x68CEF5..0x68CF1B: this name belongs to the frame manager.
    // DED PE Module::init returns at 0x63EC97 and ELF at 0x84F8FD0 without this registration.
    frameEventManager->registerEvent(ECCore, 0xDEADBEEF, "MemoryInfoDrawerEvent");
#endif
    return true;
}

#undef REGISTER_CORE_EVENT

bool Module::close()
{
    consoleObjects()->unregisterConsoleObjects(coreConsoleObjectList());
    return true;
}

// Module reference counting and interface dispatch; the interface has no virtual destructor slot.

void Module::addRef()
{
    ++m_refCount;
}

int Module::getRef()
{
    return m_refCount;
}

// Decrements ownership, deletes at zero and returns the saved new count without reading freed memory.
int Module::release()
{
    int n = --m_refCount;                                   // ebx
    if (n == 0)
        delete this;                                        // operator delete(this), 0x84FA343
    return n;
}

// Returns the same object address for IBase, IClassServer and IModule through single inheritance.
void* Module::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IClassServer)
        return const_cast<Module*>(this);
    return (iid == IID_IModule) ? const_cast<Module*>(this) : 0;
}


} // namespace hfe
} // namespace dice

