// Simulation clock and platform timing operations.
// Exact time is scaled ticks divided by platform frequency; fractional carries avoid losing time
// between frames. The fixed-step overload accepts microseconds and does not sample the clock.
// [PLATFORM] ELF uses microsecond ticks; Windows uses QueryPerformanceFrequency.
// Globals.cpp owns g_system; do not define another instance here.

#include "System.h"
#include "SystemInfo.h"
#include "StringUtil.h"
#include "Ids.h"        // IID_IBase - the binary reads a dword global (0x88E8DC8); the tree's
                        // canonical constant (value 1) is the same value, internals are free.

#if defined(_WIN32)
#include <windows.h>    // QueryPerformanceCounter/Frequency, Sleep
#include <intrin.h>     // __rdtsc
#include <dbghelp.h>
#include <process.h>
#include <algorithm>
#include <deque>
#else
#include <sys/time.h>   // gettimeofday, timeval
#include <time.h>       // nanosleep, timespec
#include <errno.h>
#include <string.h>     // strerror_r
#endif

namespace dice {
namespace hfe {

namespace {

// Shared clock counters and frequency. [CHOSEN] Unsigned 64-bit representation.
unsigned long long g_startTime;
unsigned long long g_frequency;
unsigned long long g_oldTime;
unsigned long long g_simTimeTicks;
unsigned long long g_simTimeFraction;
unsigned long long g_deltaFraction;

// [CHOSEN mechanism] the binary inlines the raw 0F 31 opcode (e.g. 0x83D2101); any equivalent
// 64-bit TSC read reproduces the behaviour.
inline unsigned long long rdtsc()
{
#if defined(_WIN32)
    return __rdtsc();
#elif defined(__aarch64__)
    // [CHOSEN, not recovered] arm64 macOS port (untested): the generic timer's virtual count is
    // the constant-rate counter that stands in for the TSC; callers calibrate its frequency.
    unsigned long long count;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(count));
    return count;
#else
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
#endif
}

// Samples the platform clock. [PLATFORM] Windows uses performance-counter ticks; POSIX uses epoch microseconds.
// [CHOSEN] Signed 64-bit return type.
long long getCurrentTime()
{
#if defined(_WIN32)
    // [PLATFORM] PE uses the monotonic performance counter, paired with the frequency captured by
    // resetExactTime. The bool result is ignored in retail.
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
#else
    static long long old;       // 0x898BFB8 [ATTESTED name] - written, never read. Kept verbatim.
    static timeval tv;          // 0x898BFC0 [ATTESTED name]

    if (gettimeofday(&tv, 0) == -1)                     // 0x83D2649; -1 test at 0x83D264E (inc/jz)
    {
        // the error text is fetched into a 256-byte stack buffer and then simply
        // discarded (no print, no early-out) before falling through to the normal computation.
        // A stripped debug print, preserved verbatim.
        char buffer[256];
        strerror_r(errno, buffer, 256);
    }
    old = tv.tv_sec * 1000000LL + tv.tv_usec;
    return old;
#endif
}

} // namespace

// C1 0x83D1FF0 / C2 0x83D1FA0 (byte-identical), 65 B each.
System::System()
    : m_refCount(1)             // [this+0x04] = 1        0x83D1FAE
    , m_simulationRate(100)     // [this+0x08] = 100      0x83D1FB5 (percent; 100 = realtime)
    , m_exactTime(0.0)          // [this+0x0C] fstp qword 0x83D1FBC
    , m_cpuTicksPerSecond(0)    // [this+0x14] qword      0x83D1FBF/0x83D1FC6
    , m_useFpuExceptions(0)            // [this+0x1C] byte       0x83D1FCD  useFpuExceptions, see System.h
    , m_fpuErrorCount(0)        // [this+0x20] dword      0x83D1FD1
    , m_unknown24(0)            // [this+0x24] dword      0x83D1FD8  [UNRESOLVED purpose]
{
    // vptr <- VMT 0x88D29E8 at 0x83D1FA8. No base-ctor call: IBase carries no data.
}

// D2 0x83D2040 / D1 0x83D2050 reset the vptr only -> empty user body.
// D0 0x83D2060 is the compiler-generated deleting variant (D1 + operator delete).
System::~System()
{
}

#if defined(_WIN32)
namespace {
// [PLATFORM] Windows crash-report state. [INFERRED] Names; containers use native STL storage.
CRITICAL_SECTION* g_crashDumpLock;
std::deque<std::string> g_crashDumpAsserts;
std::string g_crashDumpInformation;

struct CrashDumpArguments {
    const wchar_t* fileName;
    DWORD threadId;
    EXCEPTION_POINTERS* exception;
    MINIDUMP_TYPE type;
};

// Writes a crash dump on a worker thread. The callback signature must match _beginthreadex.
unsigned __stdcall writeCrashDumpThread(void* opaque)
{
    if (!g_crashDumpLock) {
        g_crashDumpLock = new CRITICAL_SECTION;
        InitializeCriticalSection(g_crashDumpLock);
    }
    const CrashDumpArguments& args = *static_cast<CrashDumpArguments*>(opaque);
    const std::wstring path = std::wstring(L"\\\\?\\") + args.fileName;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file != INVALID_HANDLE_VALUE) {
        EnterCriticalSection(g_crashDumpLock);
        MINIDUMP_EXCEPTION_INFORMATION exception = {};
        if (args.exception) {
            exception.ThreadId = args.threadId;
            exception.ExceptionPointers = args.exception;
            exception.ClientPointers = TRUE;
        }
        // Retail deliberately has no guard here after the optional assignment above.
        std::string text = formatString(
            "Battlefield2 MiniDump. Address: %p (In Windbg type: .ecxr)",
            exception.ExceptionPointers->ExceptionRecord->ExceptionAddress);
        text += '\n';
        text += g_crashDumpInformation;
        text += "ASSERT INFORMATION:\n";
        text += "[...]\n";
        // PE 40F3A0 / 4336F0 reverses the owning deque itself, including on repeated dumps.
        std::reverse(g_crashDumpAsserts.begin(), g_crashDumpAsserts.end());
        for (const std::string& entry : g_crashDumpAsserts)
            text += entry;
        text += "[EOF]";
        MINIDUMP_USER_STREAM stream = {};
        stream.Type = CommentStreamA;
        stream.BufferSize = static_cast<ULONG>(text.size() + 1);
        stream.Buffer = const_cast<char*>(text.c_str());
        MINIDUMP_USER_STREAM_INFORMATION streams = { 1, &stream };
        const DWORD processId = GetCurrentProcessId();
        const HANDLE process = GetCurrentProcess();
        MiniDumpWriteDump(process, processId, file, args.type, &exception, &streams, 0);
        LeaveCriticalSection(g_crashDumpLock);
        DeleteCriticalSection(g_crashDumpLock);
        // Shipped defect: DeleteCriticalSection does not free the new allocation.
        // Both PEs clear the pointer without operator delete; preserve that leak.
        g_crashDumpLock = 0;
        CloseHandle(file);
    }
    return 1;
}
} // namespace

// Dedicated PE 40F0D0 / client 4342D0: no lock, push_front then resize(4) if over limit.
void addCrashDumpAssertInfo(const std::string& entry)
{
    g_crashDumpAsserts.push_front(entry);
    if (g_crashDumpAsserts.size() > 4)
        g_crashDumpAsserts.resize(4);
}

// Dedicated PE 40EFD0 / client 433CD0. Delete the old file before launching the
// worker and join it while the argument block and exception pointers remain alive.
void writeCrashDump(EXCEPTION_POINTERS* exception, const wchar_t* fileName)
{
    DeleteFileW(fileName);
    CrashDumpArguments args;
    args.type = MiniDumpWithProcessThreadData; // 0x100 in both retail callers
    args.exception = exception;
    args.threadId = GetCurrentThreadId();
    args.fileName = fileName;
    unsigned threadId;
    const uintptr_t thread = _beginthreadex(0, 0, &writeCrashDumpThread, &args, 0, &threadId);
    // Retail compares against -1 rather than the CRT's documented zero failure.
    if (thread != static_cast<uintptr_t>(-1)) {
        WaitForSingleObject(reinterpret_cast<HANDLE>(thread), INFINITE);
        CloseHandle(reinterpret_cast<HANDLE>(thread));
    }
}

// Builds a timestamped crash-dump filename in static storage. [INFERRED] Name.
// The first call chooses the base; later calls append timestamps to that same string.
const wchar_t* crashDumpTargetName(const wchar_t* baseFileName)
{
    std::wstring dateTime = toWideString(systemInfo()->getDateTimeString());
    for (std::wstring::size_type i = dateTime.find(L":", 1);
         i != std::wstring::npos; i = dateTime.find(L":", i + 1))
        dateTime[i] = L'.';
    for (std::wstring::size_type i = dateTime.find(L" ", 1);
         i != std::wstring::npos; i = dateTime.find(L" ", i + 1))
        dateTime[i] = L'_';

    static std::wstring fileName(baseFileName);
    const std::wstring::size_type dot = fileName.rfind(L".");
    fileName = fileName.substr(0, dot) + L"_" + dateTime + L"." + fileName.substr(dot + 1);
    return fileName.c_str();
}
#endif

// Callers: Profiler ctor (0x83C686A/0x83C6CAF), io::ModuleIO::init (0x84FF7DB) -
// not the frame loop. The calibration below busy-spins a full wall-clock second on first call.
void System::resetExactTime()
{
    g_startTime = getCurrentTime();     // one call (0x83D2093); both cursors get its result
#if defined(_WIN32)
    // [PLATFORM] PE 0x40E75A calls QueryPerformanceFrequency immediately after the initial counter
    // sample and stores its qword result. As in retail, failure is not given a fallback value.
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    g_frequency = static_cast<unsigned long long>(frequency.QuadPart);
#else
    g_frequency = 1000000;              // 0x83D208B/0x83D20AE/0x83D20B6 (1 tick = 1 us)
#endif
    g_oldTime   = g_startTime;          // stores interleaved at 0x83D2098-0x83D20BE
    g_simTimeTicks    = 0;
    g_simTimeFraction = 0;
    g_deltaFraction   = 0;
    m_exactTime = 0.0;                  // 0x83D209F fldz -> 0x83D20ED fstp qword [this+0xC]

    if (m_cpuTicksPerSecond == 0)       // 0x83D20EA-0x83D20F7 (qword or-test)
    {
        // Calibrate the TSC against one wall-clock second, then subtract the cost of a single
        // getWallClockTime() call. getWallClockTime is dispatched virtually ([vptr+0x1C]).
        unsigned long long start = rdtsc();
        double endTime = getWallClockTime() + 1.0;          // 0x83D2114 call; +dbl_8884C58 (1.0)
        while (endTime > getWallClockTime())                // busy-spin, 0x83D2120-0x83D212F
        {
        }
        m_cpuTicksPerSecond = rdtsc() - start;
        unsigned long long callStart = rdtsc();
        getWallClockTime();                                 // 0x83D215C, result dropped (fstp st)
        m_cpuTicksPerSecond -= rdtsc() - callStart;         // 0x83D2161-0x83D2176 sub/sbb in place
    }
}

// Called once per frame by BF2Engine::mainLoop (0x81379EF), non-virtually.
void System::advanceExactTime()
{
    long long currentTime = getCurrentTime();
    unsigned long long timeDelta =
        (unsigned long long)currentTime - g_oldTime;        // 0x83D2198 sub / 0x83D21A3 sbb
    g_oldTime = (unsigned long long)currentTime;

    // m_simulationRate is sign-extended (cdq, 0x83D21B8) into a 64x64 multiply
    // (0x83D21C2/0x83D21C4/0x83D21DA); int -> unsigned long long has that exact bit pattern.
    unsigned long long acc =
        g_simTimeFraction + m_simulationRate * timeDelta;   // + at 0x83D21E3/0x83D21EF

    g_simTimeTicks += acc / 100;        // __udivdi3 0x83D2208; += 0x83D2216/0x83D2226
    g_simTimeFraction = acc % 100;      // __umoddi3 0x83D2245; div emitted before mod here

    // Both conversions are unsigned u64 -> double (fild + js -> fadd 2^64, flt_88CC398).
    m_exactTime = (double)g_simTimeTicks / (double)g_frequency;   // fdivp 0x83D2287, fstp 0x83D2289
}

// Sole caller: Demo::overrideAdvanceTime (0x81431DD), which passes
// 1000000/fps - the parameter is microseconds of wall time to advance. Never reads the clock.
void System::advanceExactTime(unsigned int deltaMicroSeconds)   // [INFERRED] parameter name
{
    // g_frequency is cached in a stack qword at entry (0x83D22C4-0x83D22D8) and that cached copy
    // feeds the final division - harmless compiler caching, nothing changes it in between.
    unsigned long long delta =
        g_deltaFraction + g_frequency * deltaMicroSeconds;  // zext arg (0x83D22B5), mul 0x83D22DB

    g_deltaFraction = delta % 1000000;                      // __umoddi3 0x83D231E - MOD first here
    unsigned long long deltaTicks = delta / 1000000;        // __udivdi3 0x83D2344, computed once
    g_oldTime += deltaTicks;                                // 0x83D2349 add / 0x83D235B adc

    unsigned long long acc =
        g_simTimeFraction + m_simulationRate * deltaTicks;  // mul 0x83D2367, sar 31 0x83D236B

    g_simTimeTicks += acc / 100;        // __udivdi3 0x83D23B4; stores 0x83D23E5/0x83D23EB
    g_simTimeFraction = acc % 100;      // __umoddi3 0x83D23F1; stores 0x83D2400/0x83D2405

    m_exactTime = (double)g_simTimeTicks / (double)g_frequency;
}

// Returns the published simulation time as a double.
double System::getExactTime()
{
    return m_exactTime;
}

// Real seconds since resetExactTime, unscaled. double return attested
// (StatusMonitorImpl_posix::shouldUpdate stores it with fstp qword).
double System::getWallClockTime()
{
    return (double)((unsigned long long)getCurrentTime() - g_startTime)   // sub/sbb 0x83D246B/71
           / (double)g_frequency;                                          // fdivp 0x83D24A9
}

// Result of the OS sleep ignored; no eintr retry loop - a signal shortens the
// sleep on the recovered (POSIX) side. Kept verbatim.
void System::sleep(int milliSeconds)
{
#if defined(_WIN32)
    // [PLATFORM] [CHOSEN, not recovered] equivalent.
    Sleep(milliSeconds);
#else
    timespec requestedTime;
    requestedTime.tv_sec  = milliSeconds / 1000;              // magic-div 0x83D24C1-0x83D24DA
    requestedTime.tv_nsec = (milliSeconds % 1000) * 1000000;
    nanosleep(&requestedTime, 0);
#endif
}

// no clamping.
void System::setSimulationRate(int rate)
{
    m_simulationRate = rate;
}


int System::getSimulationRate()
{
    return m_simulationRate;
}

// getSimulationRate goes through the vtable ([edx+0x24], 0x83D260E) - a plain
// member call in source; int -> float via cvtsi2ss (0x83D2611), then fdiv by 100.0f (flt_88844B8).
float System::getSimulationRatio()
{
    return getSimulationRate() / 100.0f;
}

// all on [this+0x20].
void System::increaseFpuErrorCount() { ++m_fpuErrorCount; }
int  System::getFpuErrorCount()      { return m_fpuErrorCount; }
void System::resetFpuErrorCount()    { m_fpuErrorCount = 0; }

// 0x83D2560 / 0x83D2570 - 5-byte empty stubs on the Linux server (the PE versions do real FPU
// control-word work; that is unread - do not invent it here).
void System::storeAndTurnOffFPUExceptions() {}
void System::restoreFPUExceptions()         {}

// Non-atomic load/inc/store. The incremented value is left in eax, so an int
// return would compile identically; the header's void is kept.
void System::addRef()
{
    ++m_refCount;
}


int System::getRef()
{
    return m_refCount;
}

// Releases one non-atomic reference, deleting the object at zero. Returns the remaining count.
int System::release()
{
    int refs = --m_refCount;
    if (refs == 0)
        delete this;
    return refs;
}

// Compares against the dword value of dice::hfe::IID_IBase (0x88E8DC8); cmovz.
void* System::queryInterface(unsigned int iid) const
{
    return iid == IID_IBase ? (void*)const_cast<System*>(this) : 0;
}

// [CHOSEN, not recovered] Stores the SEH option in the selected field. Verify the original PE layout before relying on it.
void System::setUseSeh(bool useSeh)
{
    m_unknown24 = useSeh;
}

// World time derived from the tick count by tick updates and reset operations. [CHOSEN] Storage name.
static double s_worldTime = 0.0;       // ds:0x8AC0D60

namespace WorldPref
{
double getWorldTime()
{
    // Client PE4C4400 reads9A7420. Its reset/tick writers4410/4440/4470 match
    // ELF83D2740/2790/27E0.
    return s_worldTime;
}

// Shared world tick count, advanced and reset by the WorldPref helpers.
unsigned int mTickCount = 0;           // ds:0x8AC0D68

// the counter, returned through the `int` form the tree committed to.
int getTickCount()
{
    return static_cast<int>(mTickCount);
}

// Fixed simulation interval in seconds, initialized to 1/30.
double mTickTime = 1.0 / 30.0;         // ds:0x898BFD8

// `fld qword ptr [mTickTime]; ret` - returns the double in st(0). Every caller
// read so far stores it straight into a float (Trigger::updateOnTime/updateOnEvent/handleTrigger).
double getTickTime()
{
    return mTickTime;
}

// the counter forward, then the clock re-derived from it - the same x87 sequence
// resetTickCount uses, on the NEW count. See the header.
void incTickCount(unsigned int count)
{
    mTickCount += count;
    resetWorldTime(getTickTime() * static_cast<double>(mTickCount));
}

// `fld qword [esp+4]; fstp qword ds:8AC0D60h` - the world clock's reset writer.
void resetWorldTime(double time)
{
    s_worldTime = time;
}

// the count into mTickCount, then the clock re-derived from it - the x87 sequence
// is getTickTime() * (double)count (an unsigned load via fild after a zero-extend), handed to
// resetWorldTime.
void resetTickCount(unsigned int count)
{
    mTickCount = count;
    resetWorldTime(getTickTime() * static_cast<double>(count));
}

// Advances the unsigned tick count and derives world time from count * tick duration.
// Recomputing avoids drift from repeatedly accumulating a floating-point interval.
void advanceTickCount(unsigned int delta)
{
    mTickCount += delta;
    resetWorldTime(static_cast<double>(mTickCount) * mTickTime);
}
} // namespace WorldPref

} // namespace hfe
} // namespace dice
