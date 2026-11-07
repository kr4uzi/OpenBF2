// [CHOSEN, not recovered] macOS port: see MacSystem.h.
#include "MacSystem.h"

#if defined(__APPLE__) && !(defined(DEDICATED) && DEDICATED)

#include <CoreFoundation/CoreFoundation.h>
#include <crt_externs.h>     // _NSGetEnviron
#include <mach-o/dyld.h>     // _NSGetExecutablePath
#include <spawn.h>
#include <sys/types.h>
#include <cstdint>
#include <vector>

namespace dice {
namespace hfe {
namespace macos {

std::vector<std::string> splitCommandLine(const std::string& commandLine)
{
    std::vector<std::string> args;
    const char* p = commandLine.c_str();
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            ++p;
        if (!*p)
            break;
        std::string arg;
        bool quoted = false;
        while (*p && (quoted || !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))) {
            if (*p == '\\') {
                std::size_t slashes = 0;
                while (*p == '\\') { ++slashes; ++p; }
                if (*p == '"') {
                    arg.append(slashes / 2, '\\');
                    if (slashes % 2) { arg += '"'; ++p; }
                } else {
                    arg.append(slashes, '\\');
                }
                continue;
            }
            if (*p == '"') {
                if (quoted && p[1] == '"') { arg += '"'; p += 2; continue; }
                quoted = !quoted;
                ++p;
                continue;
            }
            arg += *p++;
        }
        args.push_back(arg);
    }
    return args;
}

bool relaunchExecutable(const std::string& commandLine)
{
    std::uint32_t size = 0;
    _NSGetExecutablePath(0, &size);
    std::vector<char> path(size + 1, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0)
        return false;
    std::vector<std::string> args = splitCommandLine(commandLine);
    std::vector<char*> argv;
    argv.push_back(path.data());
    for (std::size_t i = 0; i < args.size(); ++i)
        argv.push_back(&args[i][0]);
    argv.push_back(0);
    pid_t child = 0;
    return posix_spawn(&child, path.data(), 0, 0, argv.data(), *_NSGetEnviron()) == 0;
}

void showMessageBox(const char* text, const char* caption)
{
    CFStringRef title = CFStringCreateWithCString(kCFAllocatorDefault, caption ? caption : "",
                                                  kCFStringEncodingWindowsLatin1);
    CFStringRef message = CFStringCreateWithCString(kCFAllocatorDefault, text ? text : "",
                                                    kCFStringEncodingWindowsLatin1);
    CFOptionFlags response = 0;
    CFUserNotificationDisplayAlert(0, kCFUserNotificationPlainAlertLevel, 0, 0, 0,
                                   title, message, CFSTR("OK"), 0, 0, &response);
    if (message) CFRelease(message);
    if (title) CFRelease(title);
}

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // __APPLE__ && !DEDICATED
