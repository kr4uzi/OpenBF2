// [TEMP] [CHOSEN, not recovered] Win32 client test tool: the host of the scripted input engine
// (Platform/InputScript.h, docs/macos-c-input-script.md). Off unless BF2_INPUT_SCRIPT=<file> is
// set; without it none of these functions changes what the game sees.
//
// While a script is active:
//   - BF2::handleInGameWinMessages drops the real keyboard and mouse messages it pumps and runs one
//     script tick per pump (winInputScriptPump).
//   - Scripted keys reach the game window as WM_KEYDOWN / WM_KEYUP plus the WM_CHAR sequence the
//     press carries, sent straight to its window procedure (no TranslateMessage, so the real
//     keyboard state never adds characters). Pointer moves, buttons and the wheel reach it as
//     WM_MOUSEMOVE / WM_xBUTTONDOWN/UP / WM_MOUSEWHEEL only while a menu is up, as for a real mouse
//     the in-game exclusive DirectInput mouse holds.
//   - KeyboardDevice and MouseDevice take their buffered device data from this feed instead of
//     IDirectInputDevice8::GetDeviceData, and count as acquired even when Acquire failed (the
//     window need not be in front), so the in-game path sees scripted input where it reads real
//     DirectInput data.
//   - Window::getHasFocus reports focus.
// Client images only (!DEDICATED). [RETIRE-WHEN: client-test-automation-replaced]
#ifndef PLATFORM_WINDOWS_WININPUTSCRIPT_H
#define PLATFORM_WINDOWS_WININPUTSCRIPT_H

#include "../../Core/BuildConfig.h"
#if !DEDICATED && defined(_WIN32)
#include <windows.h>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>

namespace dice {
namespace hfe {
namespace inputscript {

// True when BF2_INPUT_SCRIPT named a readable script. The script is loaded on the first call.
bool winInputScriptActive();

// True for a queued message the real keyboard or mouse produced; dropped while a script runs.
bool winInputScriptIsRealInput(UINT message);

// One script tick. window is the game window; menuActive says whether a menu (menuLogic) is up.
void winInputScriptPump(HWND window, bool menuActive);

// Scripted buffered device data, oldest first; returns the number of entries written to out.
DWORD winInputScriptTakeKeyboardData(DIDEVICEOBJECTDATA* out, DWORD capacity);
DWORD winInputScriptTakeMouseData(DIDEVICEOBJECTDATA* out, DWORD capacity);
// Called when a device becomes acquired: like a real DirectInput buffer, the device sees only
// input from then on.
void winInputScriptClearKeyboardData();
void winInputScriptClearMouseData();

} // namespace inputscript
} // namespace hfe
} // namespace dice

#endif // !DEDICATED && _WIN32
#endif // PLATFORM_WINDOWS_WININPUTSCRIPT_H
