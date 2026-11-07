// [CHOSEN, not recovered] macOS CPU identity for the renderer's quality probe.
#ifndef BF2_PLATFORM_MACOS_CPUINFO_H
#define BF2_PLATFORM_MACOS_CPUINFO_H

#include <cstdint>
#include <cstddef>
#include <sys/sysctl.h>

namespace bf2macos {
struct CpuInfo {
    char name[128] = {};
    std::uint64_t frequencyHz = 0;
};

// Frequency is optional: Apple silicon may not publish hw.cpufrequency. A
// missing value stays zero; it must never be interpreted as a measured score.
inline bool readCpuInfo(CpuInfo& info)
{
    info = CpuInfo{};
    std::size_t length = sizeof(info.name);
    if (sysctlbyname("machdep.cpu.brand_string", info.name, &length, nullptr, 0) != 0) {
        length = sizeof(info.name);
        if (sysctlbyname("hw.model", info.name, &length, nullptr, 0) != 0)
            return false;
    }
    info.name[sizeof(info.name) - 1] = '\0';
    length = sizeof(info.frequencyHz);
    if (sysctlbyname("hw.cpufrequency", &info.frequencyHz, &length, nullptr, 0) != 0 ||
        length != sizeof(info.frequencyHz))
        info.frequencyHz = 0;
    return true;
}
} // namespace bf2macos
#endif
