// System-information implementation; combined source placement is [INFERRED].
// Platform seams cover hostname and page faults. Date formatting uses the shared C runtime path,
// and getIsDebuggerInstalled retains PosixSystemInfo's unconditional false on both platforms.

#include "SystemInfo.h"

#include <string.h>     // strlen
#include <time.h>       // time, localtime, strftime

#if defined(_WIN32)
#  include <windows.h>  // GetComputerNameA - see the [CHOSEN, not recovered] note at its site
#else
#  include <unistd.h>       // gethostname
#  include <sys/time.h>     // struct timeval, for <sys/resource.h>
#  include <sys/resource.h> // getrusage, RUSAGE_SELF, struct rusage
#endif

namespace dice {
namespace hfe {

// Constructs the system-information interface.
ISystemInfo::ISystemInfo()
{
}


// Base destructor definition used by concrete implementations.
ISystemInfo::~ISystemInfo()
{
}

// Starts with an empty hostname cache.
PosixSystemInfo::PosixSystemInfo()
{
}


// Releases the cached hostname storage.
PosixSystemInfo::~PosixSystemInfo()
{
}

// Returns a cached hostname, querying the OS on first use. Failure caches "unknown" for the remaining lifetime.
const std::string& PosixSystemInfo::getComputerName() const
{
    if (m_computerName.length() == 0)
    {
        // Reserve space for the explicit trailing terminator.
        char name[255];

#if defined(_WIN32)
        // [CHOSEN, not recovered] Retail fix "Windows computer names keep all 15 characters".
        // Retail passes capacity 15 and caches "unknown" for a 15-character name; use the full buffer.
        DWORD size = sizeof(name);
        if (GetComputerNameA(name, &size) != 0)
        {
            name[sizeof(name) - 1] = '\0';
            m_computerName.assign(name, strlen(name));
        }
        else
        {
            m_computerName.assign("unknown", strlen("unknown"));
        }
#else
        if (gethostname(name, sizeof(name)) == 0)
        {
            name[sizeof(name) - 1] = '\0';      // 0x83D2BCB, success path only
            m_computerName.assign(name, strlen(name));  // 0x83D2BE2, after an explicit strlen
        }
        else
        {
            m_computerName.assign("unknown", strlen("unknown"));     // 0x83D2BF0, "unknown"
        }
#endif
    }

    return m_computerName;      // `mov eax, ebx` where ebx == this+4 - a reference, not a copy
}

// Returns local date and time as yyyy-MM-DD HH:MM:SS. Formatting errors are not checked.
std::string PosixSystemInfo::getDateTimeString() const
{
    time_t timer = time(0);                     // 0x83D2C19 passes a literal 0, not a pointer
    struct tm* tp = localtime(&timer);

    char s[79];
    strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", tp);

    return std::string(s);
}

// Returns the process's major page-fault count, or zero if the platform query fails.
unsigned int PosixSystemInfo::getNumberOfPageFaults() const
{
#if defined(_WIN32)
    // [CHOSEN, not recovered] Windows fallback returns zero; no equivalent major-fault query is implemented.
    return 0;
#else
    struct rusage usage;

    // The cmov means the source was branchless - a conditional expression, not an if/return pair.
    return (getrusage(RUSAGE_SELF, &usage) == 0) ? (unsigned int)usage.ru_majflt : 0u;
#endif
}

// Returns zero, matching the disabled memory-usage query.
unsigned int PosixSystemInfo::getMemoryUsage() const
{
    return 0;
}

// Sets both memory totals to zero, matching the disabled query.
void PosixSystemInfo::getMemoryInfo(unsigned int& totalPhysical,
                                    unsigned int& availablePhysical) const
{
    availablePhysical = 0;
    totalPhysical     = 0;
}

// Returns false; this implementation does not detect a debugger.
// [CHOSEN, not recovered] Windows keeps the POSIX answer. Retail checks forceNoDebugger,
// Visual Studio 7.0/7.1 and AeDebug (excluding drwtsn32/drwatson). Even with a registered
// debugger, non-dedicated clients clear hack-ignore-asserts; Windows keeps SEH unless +seh 0 is given.
// See docs/architecture.md, "Debugger detection at startup".
bool PosixSystemInfo::getIsDebuggerInstalled() const
{
    return false;
}

// The accessor

// Returns the process-local system-information instance; original startup initializes it before workers.
ISystemInfo* systemInfo()
{
    static PosixSystemInfo obj;
    return &obj;
}

} // namespace hfe
} // namespace dice
