// [CHOSEN, not recovered] macOS port: process and dialog services the client's Win32 call sites
// used (ShellExecuteA on its own executable, MessageBoxA). Plain C++; the implementation is
// MacSystem.cpp. See docs/macos-port.md, "Win32 and CRT items (S2.5/S2.8)".

#ifndef PLATFORM_MACOS_MACSYSTEM_H
#define PLATFORM_MACOS_MACSYSTEM_H

#include <string>
#include <vector>

namespace dice {
namespace hfe {
namespace macos {

// Splits a Win32-style command line into arguments as CommandLineToArgvW does for arguments
// after the program name: whitespace separates, double quotes group, 2n backslashes before a
// quote become n backslashes, 2n+1 become n and a literal quote.
std::vector<std::string> splitCommandLine(const std::string& commandLine);

// The ShellExecuteA(0, "open", <this executable>, commandLine, ...) counterpart: starts a new
// instance of the running executable with the split command line, in the current working
// directory and environment. Returns true when the new process was started (ShellExecute's
// result above 32), false otherwise.
bool relaunchExecutable(const std::string& commandLine);

// MessageBoxA(0, text, caption, MB_OK): a modal alert with one OK button that blocks the calling
// thread until it is dismissed. Text and caption are Windows-1252, as the ANSI call takes them.
void showMessageBox(const char* text, const char* caption);

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // PLATFORM_MACOS_MACSYSTEM_H
