// [TEMP] [CHOSEN, not recovered] Cocoa-only host fixture access; never part of the shared host.
// [RETIRE-WHEN: macos-client-window] Replace with equivalent native host/input coverage before removal.
#ifndef PLATFORM_MACOS_COCOAHOSTTESTHOOKS_H
#define PLATFORM_MACOS_COCOAHOSTTESTHOOKS_H

#if defined(__APPLE__) && defined(OPENBF2_HOST_TEST_SEAMS)
#include "../Host/ClientHost.h"

namespace dice { namespace hfe { namespace macos {

class CocoaHostTestHooks
{
public:
    // Borrowed Cocoa objects for the window fixture; null when the host has no window.
    static void* nativeWindow(host::ClientHost& host);
    static void* nativeView(host::ClientHost& host);
    static void* metalLayer(host::ClientHost& host);
    static bool isISOKeyboard(const host::ClientHost& host);

    // Simulates focus when AppKit activation is unavailable in the test session. The focus set
    // here stands: the host stops following AppKit unless setSystemFocusForTest simulates it.
    static void overrideFocusForTest(host::ClientHost& host, bool windowKey, bool appActive);

    // What AppKit and the system would report about the application and the game window.
    struct SystemFocus
    {
        bool   appActive;        // [NSApp isActive]
        bool   windowKey;        // the game window is the key window
        double now;              // the clock of activation and dropped-input reports, in seconds
        bool   systemFrontmost;  // the system names this process the frontmost application
        bool   cooperativeActivation = true; // macOS 14+; false simulates 11-13
        bool   windowVisible = false; // the game window is on screen and not minimized
    };
    // Simulates that state, so the host's focus rules run without an application or a window;
    // null ends the simulation.
    static void setSystemFocusForTest(host::ClientHost& host, const SystemFocus* state);
    // A focus callback of the delegates (the window's key status, the application's activation,
    // the active Space), after the simulated state was changed to match: they all end in the
    // member this calls.
    // `whileBusy` models a callback delivered by the busy service's run loop.
    static void focusCallbackForTest(host::ClientHost& host, bool whileBusy = false);
    // The focus part of one run of the event loop, as a frame's pump makes it.
    static void pumpFocusForTest(host::ClientHost& host);
    // Starts an independent activation fixture without NSApplication or its launch wait.
    static void launchActivationForTest(host::ClientHost& host, bool forcedFocus = false);
    // Runs the order-in activation path and seeds the requested full-screen state without a window.
    static void windowOrderedInForTest(host::ClientHost& host, bool fullscreen);
    struct StartupState
    {
        unsigned requests;
        bool fullscreenWaiting;
        bool fullscreenEntering;
    };
    static StartupState startupStateForTest(const host::ClientHost& host);
    struct DroppedInputStats
    {
        unsigned droppedInputs;  // input events the focus gate dropped while the application was in front
        unsigned dropReports;    // "[host] input dropped" lines written
    };
    static DroppedInputStats droppedInputStatsForTest(const host::ClientHost& host);
    // Routes injected NSEvents without creating a window or touching the hardware cursor.
    static void enableHeadlessForTest(host::ClientHost& host);
    static void injectEventForTest(host::ClientHost& host, void* nsEvent);
    // The busy service (ClientHost::serviceWhileBusy): holds an event as the service holds what it
    // takes from the window server, reports how many are held, and delivers them as the next pump
    // does first.
    static void holdEventForTest(host::ClientHost& host, void* nsEvent);
    static unsigned heldEventsForTest(const host::ClientHost& host);
    static void deliverHeldEventsForTest(host::ClientHost& host);
    // Runs the host's bounded-wait service without creating NSApplication or a window.
    static void serviceAppKitOnlyForTest(host::ClientHost& host);
    static void cursorStateForTest(const host::ClientHost& host, bool& hidden, bool& disassociated);
    static void pumpRawMouseForTest(host::ClientHost& host);
};

} } }
#endif
#endif
