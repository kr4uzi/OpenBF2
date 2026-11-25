// [CHOSEN, not recovered] macOS port: Cocoa implementation of ClientHost. Compile with ARC
// (-fobjc-arc). See ClientHost.h for the contract and docs/macos-s2-4.md for platform behavior.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <unistd.h>                         // getpid
#include <sys/wait.h>

#include "../Host/ClientHost.h"
#include "../Host/KeyMap.h"
#include "CocoaHostTestHooks.h"
#include "CocoaText.h"
#include "MacKeyMap.h"
#include "MacRawMouse.h"
#include "MacSystem.h"                      // handOverActivation
#include "NativeFullscreen.h"               // [CHOSEN] the game window's own full-screen Space
#include "../InputScript.h"         // [TEMP] BF2_INPUT_SCRIPT (docs/macos-c-input-script.md)
#include "../Host/CursorFile.h"
#include "CursorScale.h"                 // [CHOSEN] the cursor's size on screen
#include "DisplayPolicy.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include <Carbon/Carbon.h>                  // LMGetKbdType, TIS input sources, UCKeyTranslate
#include <IOKit/pwr_mgt/IOPMLib.h>          // display-sleep assertion


#if !__has_feature(objc_arc)
#error "CocoaHost.mm must be compiled with -fobjc-arc"
#endif

// HIToolbox exports KBGetLayoutType but no current SDK header declares it. The layout-type
// constants are four-character codes ('ANSI', 'ISO ', 'JIS ').
extern "C" UInt32 KBGetLayoutType(SInt16 keyboardType);

namespace {
const UInt32 kLayoutTypeISO = 0x49534F20u;  // 'ISO '

bool currentKeyboardIsISO()
{
    return KBGetLayoutType(static_cast<SInt16>(LMGetKbdType())) == kLayoutTypeISO;
}

unsigned int msFromSeconds(NSTimeInterval seconds)
{
    return static_cast<unsigned int>(static_cast<unsigned long long>(seconds * 1000.0));
}
} // namespace

using dice::hfe::host::ClientHost;

namespace {
// [CHOSEN, not recovered] A cursor token (the counterpart of an HCURSOR). The two shape tokens
// show the system arrow; a token from loadCursorFromFile carries the decoded file and the
// NSCursors built from it for the current view scale.
struct HostCursor
{
    bool custom = false;
    dice::hfe::host::CursorAnimation anim;
    int totalJiffies = 0;
    CGFloat builtScale = 0.0;           // points per frame pixel the cursors were built for
    CGFloat builtBacking = 0.0;
    NSArray<NSCursor*>* cursors = nil;  // one per frame; NSNull-free, empty when building failed
};

// The view's cursor rect asks the host for the cursor it shows (defined after ClientHost::Impl).
NSCursor* currentViewCursor();
// The view asks the host to re-choose the layer's magnification filter (defined with it).
void refreshViewMagnification();
} // namespace

@interface OpenBF2HostWindow : NSWindow
@end

// [CHOSEN, not recovered] The game window keeps its title bar in full screen too (native full
// screen, NativeFullscreen.h), so it becomes key and main as any titled window does. The borderless
// full-screen window it replaces needed overrides for that.
@implementation OpenBF2HostWindow
@end

@interface OpenBF2HostView : NSView
@end

@implementation OpenBF2HostView
- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]))
    {
        self.wantsLayer = YES;
        self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawNever;
    }
    return self;
}
- (CALayer*)makeBackingLayer
{
    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    layer.opaque = YES;
    layer.contentsScale = 1.0;
    layer.contentsGravity = kCAGravityResize;
    // [CHOSEN, not recovered] The internal-resolution drawable is enlarged by Core Animation
    // on Retina. A whole-number enlargement keeps its pixel-snapped bitmap text with nearest-
    // neighbour magnification; other factors use linear (ClientHost::Impl::updateMagnification).
    // Keep linear minification for windows scaled to fit.
    layer.magnificationFilter = kCAFilterNearest;
    CGColorRef black = CGColorCreateGenericGray(0.0, 1.0);
    layer.backgroundColor = black;
    CGColorRelease(black);
    return layer;
}
- (BOOL)wantsUpdateLayer       { return YES; }
// [CHOSEN, not recovered] A loaded game cursor is the view's cursor rect, so AppKit shows it when
// the pointer enters the view and restores the arrow when it leaves (title bar, other windows).
- (void)resetCursorRects
{
    if (NSCursor* cursor = currentViewCursor())
        [self addCursorRect:self.bounds cursor:cursor];
}
- (BOOL)acceptsFirstResponder  { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event { (void)event; return YES; }
// Keys are consumed by the host pump before dispatch; swallowing them here prevents the
// "unhandled key" alert sound. Key equivalents (Cmd-Q, Cmd-H) still reach the menu.
- (void)keyDown:(NSEvent*)event       { (void)event; }
- (void)keyUp:(NSEvent*)event         { (void)event; }
- (void)flagsChanged:(NSEvent*)event  { (void)event; }
// AppKit resets the layer scale when the window moves between displays; the drawable keeps
// its internal resolution and Core Animation scales it to the view.
- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    self.layer.contentsScale = 1.0;
    refreshViewMagnification();
}
@end

@interface OpenBF2HostDelegate : NSObject <NSWindowDelegate, NSApplicationDelegate>
- (void)toggleGameWindowFullScreen;
- (void)launchTimedOut;
- (void)activeSpaceDidChange:(NSNotification*)note;
@end

namespace dice {
namespace hfe {
namespace host {

struct ClientHost::Impl : inputscript::IInputScriptSink
{
    OpenBF2HostDelegate* delegate = nil;
    OpenBF2HostWindow*   window   = nil;
    OpenBF2HostView*     view     = nil;
    CAMetalLayer*        layer    = nil;

    bool appInitialized = false;
    bool quit           = false;
    macos::NativeFullscreen fullscreen;  // [CHOSEN] what the engine asked for and where the window is
    // [CHOSEN, not recovered] The window is in its full-screen Space and that Space is not the
    // active one. Read again on every focus sync and when the active Space changes.
    bool spaceHidden = false;
    bool visible        = false;
    bool minimized      = false;
    bool windowKey      = false;
    bool appActive      = false;

    int  backWidth  = 0;            // drawableSize
    int  backHeight = 0;

    int  cursorCounter    = 0;      // ShowCursor display counter
    bool cursorHiddenInView = false;
    bool mouseInView      = false;
    bool relativeMouse    = false;
    bool mouseDisassociated = false;
    bool cursorHidden     = false;  // the [NSCursor hide] balance this host owns
    HostCursor* activeCursor = nullptr;     // the last SetCursor token (null: none or hidden)
    double cursorAnimStart = 0.0;           // systemUptime when activeCursor was selected
    NSCursor* rectCursor = nil;             // the cursor the view's cursor rect holds
    NSCursor* shownCursor = nil;            // the cursor last set while the pointer was inside

    unsigned int modifiers = 0;
    // [CHOSEN, not recovered] Native GetAsyncKeyState(VK_RETURN) adapter.
    bool enterHeld[2] = { false, false };
    bool enterPressed = false;
    bool capsReleasePending = false;
    HostKeyEvent capsRelease;
    unsigned int buttonsDown = 0;   // buttons whose press was reported
    double wheelRemainder = 0.0;

    IOPMAssertionID sleepAssertion = 0;
    bool sleepAssertionHeld = false;

    std::vector<IHostEventListener*> listeners;

    // [TEMP] BF2_INPUT_SCRIPT (MacInputScript.h): scripted input replaces the real devices, focus
    // is reported as held, and the real cursor is left alone. [RETIRE-WHEN: client-test-automation-replaced]
    bool scripted = false;
    // [TEMP] BF2_INPUT_SCRIPT_REAL_FOCUS=1: a scripted run still follows the real window and
    // application focus (the app is activated at launch and focus changes from outside,
    // e.g. activating another app, reach the engine), so deactivate/reactivate paths can be tested
    // unattended. Scripted input and the cursor rules are unchanged. [RETIRE-WHEN: client-test-automation-replaced]
    bool scriptedFocus = false;     // scripted && !BF2_INPUT_SCRIPT_REAL_FOCUS: focus is forced held
    unsigned int deactivations = 0; // deactivationCount(): active -> inactive transitions

    // [CHOSEN, not recovered] The start. The engine owns the main loop and does not service
    // AppKit while it initialises, for seconds before and after the window is created. So the
    // host completes AppKit's launch itself (initApplication runs -run, endLaunchRun ends it),
    // asks to become the active application at launch and runs the event loop until
    // AppKit has answered (activateAtLaunch). Cooperative requests continue briefly after the
    // window is shown; createWindow also waits for a full-screen start's way into its Space
    // (waitForFullscreen). From then on the engine's focus is what
    // AppKit reports (syncFocus), and what the focus gate drops is reported (inputDropped).
    bool   launching = false;        // initApplication's -run for the launch is under way
    bool   launchTimedOut = false;   // AppKit did not report the end of the launch within kLaunchLimit
    double activationFirstRequestAt = 0.0;  // the first request, for the startup log
    double activationRequestAt = 0.0;       // requests outstanding since; 0 after activation was reported
    double activationLastRequestAt = 0.0;   // spaces cooperative requests by one second
    double activationWindowAt = 0.0;        // window shown; 0 when no more requests are due
    unsigned activationRequests = 0;        // request numbers in the startup log
    double fullscreenEnterAt = 0.0;  // uptime() of the last Space entry started
    // The engine's window adapter has no window handle while createWindow runs and drops what
    // the host reports then (BF2Window_native.cpp). It sends the focus again itself; the pointer
    // position that goes with gaining focus (reportPointer) is owed to the first frame's pump.
    bool   pointerOwed = false;
    static constexpr double kLaunchLimit = 5.0;            // seconds: the launch pass
    static constexpr double kLaunchActivationWait = 1.0;   // the answer to the launch request
    static constexpr double kActivationRepeatLimit = 5.0;  // seconds after ordering the window in
    static constexpr double kFullscreenRetryDelay = 0.3;   // between two attempts to enter the Space
    enum ServiceMode { DeliverInput, AppKitOnly, HoldEvents }; // see service()

    bool   dropReported = false;     // inputDropped: one report a second at most
    double dropReportAt = 0.0;
    unsigned int dropsSinceReport = 0;
    unsigned int droppedInputs = 0;  // totals (CocoaHostTestHooks::droppedInputStatsForTest)
    unsigned int dropReports = 0;
    float scriptX = 0.0f, scriptY = 0.0f;
    bool scriptPointerSet = false;

    // [CHOSEN, not recovered] The busy service (ClientHost::serviceWhileBusy). macOS shows the
    // spinning wait cursor over an application that leaves the window server's events unread for
    // a few seconds, and only the host animates the game's own .ani cursor (updateCursorImage) -
    // Windows animates a cursor by itself and queues a busy application's messages without
    // comment. A level load runs for tens of seconds without a pump. During it the service takes
    // the queued events from the window server and holds them here, in order, and moves the
    // cursor on; the next pump delivers them before any newer event, exactly as it would have
    // found them in the queue. No event is dispatched to AppKit or the listeners in between;
    // run-loop callbacks defer focus and quit, so neither reaches the engine early. AppKit-only
    // waits consume held events without routing game input, just as they consume newer events.
    NSMutableArray<NSEvent*>* heldEvents = nil;
    double lastPumpAt = 0.0;          // uptime() of the last pump or busy service
    unsigned int heldDropped = 0;     // moves dropped for the cap since the last delivery
    // The run loop that the service runs can still deliver an Apple Event (the Dock's Quit) to the
    // delegate. A quit request made while the service runs is held, as its events are, and the
    // next pump makes it (requestQuit).
    bool servicing = false;
    bool quitHeld = false;
    static const NSUInteger kHeldEventCap = 4096;

    // [TEMP] setSystemFocusForTest: the answers AppKit and the system would give, so the focus
    // rules can be checked without an application or a window.
    struct TestSystem
    {
        bool   set = false;
        bool   appActive = false;        // [NSApp isActive]
        bool   windowKey = false;        // window.isKeyWindow
        bool   systemFrontmost = false;  // the system names this process the frontmost application
        bool   cooperativeActivation = true; // macOS 14+: requests may be repeated
        bool   windowVisible = false;   // an ordered-in, non-minimized game window
        double now = 0.0;                // focusNow()
    };
#if defined(OPENBF2_HOST_TEST_SEAMS)
    bool testHeadless = false;      // enableHeadlessForTest: no window, no real cursor changes
    bool testFocusOverride = false; // overrideFocusForTest was used: the focus it set stands
    mutable TestSystem testSystem;
    TestSystem* testSystemState() const { return testSystem.set ? &testSystem : nullptr; }
#else
    static constexpr bool testHeadless = false;
    static constexpr bool testFocusOverride = false;
    static constexpr TestSystem* testSystemState() { return nullptr; }
#endif

    std::vector<IHostEventListener*> snapshot() const { return listeners; }

    bool hasFocus() const { return (window != nil || testHeadless) && windowKey && appActive; }

    static double uptime() { return [[NSProcessInfo processInfo] systemUptime]; }

    // What AppKit and the system report right now; a headless test supplies the answers.
    double focusNow() const
    {
        if (const TestSystem* t = testSystemState())
            return t->now;
        return uptime();
    }
    bool nsAppActive() const
    {
        if (const TestSystem* t = testSystemState())
            return t->appActive;
        return [NSApp isActive] ? true : false;
    }
    bool windowIsKey() const
    {
        if (const TestSystem* t = testSystemState())
            return t->windowKey;
        return window != nil && window.isKeyWindow;
    }
    // The system names this process the active, frontmost application, whatever AppKit says.
    bool systemFrontmost() const
    {
        if (const TestSystem* t = testSystemState())
            return t->systemFrontmost;
        return [NSRunningApplication currentApplication].active &&
               [[NSWorkspace sharedWorkspace] frontmostApplication].processIdentifier == getpid();
    }

    // [CHOSEN, not recovered] Ends the -run that initApplication makes for the launch. -stop: takes
    // effect once the loop has handled another event, so one is posted. A timeout lets startup
    // continue if AppKit never reports the end of its launch.
    void endLaunchRun(bool timedOut)
    {
        if (!launching)
            return;
        launching = false;
        launchTimedOut = timedOut;
        [NSObject cancelPreviousPerformRequestsWithTarget:delegate selector:@selector(launchTimedOut) object:nil];
        [NSApp stop:nil];
        [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint
                                       modifierFlags:0 timestamp:0 windowNumber:0 context:nil
                                             subtype:0 data1:0 data2:0]
                 atStart:YES];
    }

    bool cooperativeActivation() const
    {
        if (const TestSystem* t = testSystemState())
            return t->cooperativeActivation;
        if (@available(macOS 14.0, *))
            return true;
        return false;
    }

    // [CHOSEN, not recovered] macOS 14+ activation is cooperative: repeating the request lets
    // AppKit accept it after startup or a permission prompt, without forcing another application
    // out of the front. On 11-13 the non-cooperative request is made only once, at launch.
    void requestActivation(const char* when)
    {
        if (scriptedFocus || nsAppActive() || (!cooperativeActivation() && activationRequests != 0))
            return;
        const double now = focusNow();
        if (activationRequests == 0)
            activationFirstRequestAt = now;
        if (activationRequestAt == 0.0)
            activationRequestAt = now;
        activationLastRequestAt = now;
        ++activationRequests;
        std::fprintf(stderr, "[host] activation requested at %s (#%u, %d ms since the first request)\n",
                     when, activationRequests, static_cast<int>((now - activationFirstRequestAt) * 1000.0));
        if (testSystemState() || testHeadless)
            return;                              // [TEMP] count requests without activating a real application
        if (@available(macOS 14.0, *))
            [NSApp activate];
        else
        {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
        }
    }

    // [CHOSEN, not recovered] A cooperative request when the window is ordered in while the
    // application is inactive, then one per second for five seconds while it remains visible and
    // inactive. That includes an application that was active at launch and lost the activation
    // before it had a window: only the window gives the system something to activate for the
    // player, and the system still decides (a request made after the user went to another
    // application is not granted). The engine's pumps service these requests; no extra wait delays
    // startup. Activation after the window was shown ends the retries for good.
    void windowOrderedIn()
    {
        if (scriptedFocus || nsAppActive() || !cooperativeActivation())
            return;
        if (activationRequestAt == 0.0 && !testSystemState())
        {
            NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
            std::fprintf(stderr, "[host] window ordered in while the application is inactive (frontmost: %s pid %d)\n",
                         front.localizedName.UTF8String ?: "none", static_cast<int>(front.processIdentifier));
        }
        activationWindowAt = focusNow();
        requestActivation("window order-in");
    }

    void repeatCooperativeActivation()
    {
        if (activationWindowAt == 0.0)
            return;
        const double now = focusNow();
        if (nsAppActive() || now - activationWindowAt > kActivationRepeatLimit)
        {
            activationWindowAt = 0.0;
            return;
        }
        const TestSystem* t = testSystemState();
        const bool onScreen = t ? t->windowVisible : window && window.isVisible && !minimized;
        if (onScreen && now - activationLastRequestAt >= 1.0)
            requestActivation("visible window");
    }

    // [CHOSEN, not recovered] Called once, at the end of initApplication: the game asks to be the
    // active application when its launch has completed, and the host runs the event loop until
    // AppKit says so, for kLaunchActivationWait at most. The engine starts afterwards and does
    // not service AppKit for seconds; asked later, or left unserviced, the activation would be
    // noticed with the first frame and the game window would sit inactive until then.
    void activateAtLaunch()
    {
        if (scriptedFocus)                       // [TEMP] a scripted run does not take the user's focus
        {
            syncFocus();
            return;
        }
        @autoreleasepool
        {
            service(0.0, AppKitOnly);            // what the change of the activation policy queued
            NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
            std::fprintf(stderr, "[host] launch frontmost: %s pid %d\n",
                         front.localizedName.UTF8String ?: "none", static_cast<int>(front.processIdentifier));
            const double start = uptime();
            if (![NSApp isActive])
                requestActivation("launch");
            while (![NSApp isActive] && uptime() - start < kLaunchActivationWait)
                service(0.02, AppKitOnly);
            const int ms = static_cast<int>((uptime() - start) * 1000.0);
            if ([NSApp isActive])
                std::fprintf(stderr, "[host] application active after %d ms\n", ms);
            else
            {
                front = [[NSWorkspace sharedWorkspace] frontmostApplication];
                std::fprintf(stderr, "[host] application not activated within %d ms (frontmost: %s pid %d, the "
                             "system calls this process %s); %s\n",
                             ms, front.localizedName.UTF8String ?: "none", static_cast<int>(front.processIdentifier),
                             [NSRunningApplication currentApplication].active ? "active" : "inactive",
                             cooperativeActivation() ? "cooperative requests resume when the game window is shown"
                                                     : "the game window waits for a click or a switch to it");
            }
        }
        syncFocus();
    }

    // [CHOSEN, not recovered] The engine's focus is what AppKit reports: the game window is the key
    // window, the application is active, and the window's full-screen Space is the active one.
    // windowKey and appActive cache that for hasFocus(); the listeners hear of a change once
    // (setFocusState). Called from the delegate's focus callbacks, from the active-Space
    // notification and once per run of the event loop (followFocus), so a change that no
    // callback reported lasts one frame. Busy-service callbacks leave it for the next pump.
    // Nothing here asks for activation or key status.
    void syncFocus()
    {
        if (servicing)
            return;                              // a busy run-loop callback waits for the next pump
        updateSpaceHidden();
        const bool active = nsAppActive();
        const bool key = (window != nil || testHeadless) && !spaceHidden && windowIsKey();
        const bool becameActive = active && !appActive && !scriptedFocus;
        setFocusState(key, active);
        if (becameActive)
            applicationBecameActive();
    }

    // The application became active, by AppKit's callback or found by a run of the event loop.
    void applicationBecameActive()
    {
        if (activationRequestAt != 0.0)
        {
            std::fprintf(stderr, "[host] application active %d ms after the first request (last request #%u, %d ms ago)\n",
                         static_cast<int>((focusNow() - activationFirstRequestAt) * 1000.0), activationRequests,
                         static_cast<int>((focusNow() - activationLastRequestAt) * 1000.0));
            activationRequestAt = 0.0;
        }
        activationWindowAt = 0.0;
        // The activated application's window is the key window, as AppKit makes it when the
        // window was key before; one that was ordered in while the application was inactive
        // becomes key here.
        if (window && visible && !minimized && !spaceHidden && !windowIsKey())
            [window makeKeyAndOrderFront:nil];
        startFullscreenTransition(fullscreen.activated());   // a full-screen request that waited
    }

    // The focus part of one run of the event loop, followed by any cooperative startup request
    // due. Forced focus stays as it was set (focusFollowsSystem).
    void followFocus()
    {
        if (focusFollowsSystem())
            syncFocus();
        repeatCooperativeActivation();
    }

    // Whether the focus state follows AppKit. Forced focus stays as it was set: scripted runs
    // ([TEMP] scriptedFocus) and tests that use overrideFocusForTest without a simulated system.
    bool focusFollowsSystem() const
    {
        if (scriptedFocus)
            return false;
        return testSystemState() || (!testHeadless && !testFocusOverride);
    }

    // [CHOSEN, not recovered] The focus gate dropped an input event (a key press, a press or move
    // inside the game window, the wheel). One line a second at most says so, with the cache and
    // what AppKit and the system report, so a log shows whether "the game ignores my clicks" was
    // the gate and which side was wrong.
    void inputDropped(const char* what)
    {
        // Input for an application in the background was not the game's: nothing was lost. The
        // system's answer counts too, so drops are reported in the state in which the system
        // has this process in front and AppKit does not.
        if (!nsAppActive() && !systemFrontmost())
            return;
        ++droppedInputs;
        ++dropsSinceReport;
        const double now = focusNow();
        if (dropReported && now - dropReportAt < 1.0)
            return;
        dropReported = true;
        dropReportAt = now;
        ++dropReports;
        NSRunningApplication* front = testSystemState() ? nil : [[NSWorkspace sharedWorkspace] frontmostApplication];
        std::fprintf(stderr, "[host] input dropped: %s (%u event(s) since the last report); appActive=%d "
                     "windowKey=%d [NSApp isActive]=%d window.isKeyWindow=%d window.isVisible=%d "
                     "spaceHidden=%d system frontmost=%d frontmost pid=%d (%s), own pid=%d\n",
                     what, dropsSinceReport, appActive ? 1 : 0, windowKey ? 1 : 0, nsAppActive() ? 1 : 0,
                     windowIsKey() ? 1 : 0,
                     window != nil && window.isVisible ? 1 : 0,
                     spaceHidden ? 1 : 0, systemFrontmost() ? 1 : 0, static_cast<int>(front.processIdentifier),
                     front.localizedName.UTF8String ?: "?", static_cast<int>(getpid()));
        dropsSinceReport = 0;
    }

    // [CHOSEN, not recovered] Win32 sends WM_SETCURSOR and WM_MOUSEMOVE when a window under the
    // pointer is activated; the menu learns the pointer position before the first click instead of
    // after the first move. A view-relative move without motion, only when the pointer is inside.
    void reportPointer()
    {
        if (!window || !view || scripted || testHeadless)
            return;
        const NSPoint inWindow = [window convertPointFromScreen:[NSEvent mouseLocation]];
        const NSPoint p = [view convertPoint:inWindow fromView:nil];
        const NSRect b = view.bounds;
        if (b.size.width <= 0 || b.size.height <= 0 || !NSPointInRect(p, b))
            return;
        HostMouseMoveEvent move;
        move.dx = move.dy = 0.0f;
        move.x = static_cast<float>(p.x * backWidth / b.size.width);
        move.y = static_cast<float>((b.size.height - p.y) * backHeight / b.size.height);
        move.insideView = true;
        move.captured = relativeMouse && hasFocus();
        move.timeMs = ClientHost::tickCountMs();
        if (!mouseInView)
        {
            mouseInView = true;
            applyCursor();
        }
        for (IHostEventListener* l : snapshot())
            l->onMouseMove(move);
    }

    void updateAsyncEnter(unsigned short keyCode, bool down, bool repeat)
    {
        const int index = keyCode == kVK_Return ? 0 : keyCode == kVK_ANSI_KeypadEnter ? 1 : -1;
        if (index < 0) return;
        // A non-repeat Cocoa down is a fresh press even when its preceding
        // release happened outside this application and was not delivered.
        if (down && !repeat) enterPressed = true;
        enterHeld[index] = down;
    }

    // [CHOSEN, not recovered] Windowed content size in points for a back buffer of
    // width x height pixels. A back buffer that fits the screen's visible area keeps one point
    // per pixel. A larger one is scaled down only as far as the visible area requires, keeping
    // its aspect, independently of the Retina backing scale. The drawable stays width x height.
    static NSSize windowedContentSize(int width, int height, NSScreen* screen, NSWindowStyleMask style)
    {
        NSSize size = NSMakeSize(width, height);
        if (!screen)
            return size;
        const NSRect visible = screen.visibleFrame;
        const NSRect probe = [NSWindow frameRectForContentRect:NSMakeRect(0, 0, 100, 100) styleMask:style];
        const CGFloat maxW = NSWidth(visible) - (NSWidth(probe) - 100);
        const CGFloat maxH = NSHeight(visible) - (NSHeight(probe) - 100);
        const auto fitted = macos::fitWindowContent(width, height, maxW, maxH);
        return NSMakeSize(fitted.width, fitted.height);
    }

    void updateDrawable()
    {
        if (!layer)
            return;
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        layer.contentsScale = 1.0;
        layer.drawableSize = CGSizeMake(backWidth, backHeight);
        updateMagnification();
        [CATransaction commit];
    }

    // [CHOSEN, not recovered] Core Animation enlarges the drawable to the view's backing pixels.
    // Nearest-neighbour keeps a whole-number enlargement (1440x900 on a 2880x1800 backing) pixel-
    // exact. At any other factor (800x600 or 2048x1330 full screen on a Retina panel) it repeats
    // some rows and columns and the image looks jagged ("oversharp"), so those use linear.
    void updateMagnification()
    {
        if (!layer || !view || backWidth <= 0 || backHeight <= 0)
            return;
        const NSRect pixels = [view convertRectToBacking:view.bounds];
        const double fx = NSWidth(pixels) / backWidth, fy = NSHeight(pixels) / backHeight;
        const auto whole = [](double f) { return f >= 1.0 && std::fabs(f - std::round(f)) < 0.01; };
        NSString* const filter = whole(fx) && whole(fy) ? kCAFilterNearest : kCAFilterLinear;
        if (![layer.magnificationFilter isEqualToString:filter])
            layer.magnificationFilter = filter;
    }

    // [CHOSEN, not recovered] Builds a token's NSCursors for the current view scale. Retail shows
    // the 32x32 cursor at one back-buffer pixel per cursor pixel (a full-screen mode change on
    // Windows), so its size follows the game image: points per back-buffer pixel, times the frame
    // size - and, on high-resolution back buffers, times MenuViewport::bitmapScale. `scale` is
    // that product within its bounds, in points per cursor pixel (macos::cursorPointsPerPixel in
    // CursorScale.h, which customCursor calls).
    // Pixels are enlarged nearest-neighbour to the next whole multiple and, when the Retina
    // target is not a whole multiple, reduced to it with high-quality filtering.
    static NSCursor* buildCursorFrame(const CursorFrame& f, CGFloat scale, CGFloat backing)
    {
        const NSSize points = NSMakeSize(f.width * scale, f.height * scale);
        const NSPoint hot = NSMakePoint(f.hotX * scale, f.hotY * scale);
        if (!f.png.empty())
        {
            NSData* data = [NSData dataWithBytes:f.png.data() length:f.png.size()];
            NSImage* image = [[NSImage alloc] initWithData:data];
            if (!image)
                return nil;
            image.size = points;
            return [[NSCursor alloc] initWithImage:image hotSpot:hot];
        }
        const int w = f.width, h = f.height;
        const int pw = std::max(1, static_cast<int>(std::lround(points.width * backing)));
        const int ph = std::max(1, static_cast<int>(std::lround(points.height * backing)));
        const int m = std::max(1, (pw + w - 1) / w);
        const int uw = w * m, uh = h * m;
        // The reps own their pixel buffers (no provider referring to a temporary).
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:nullptr pixelsWide:uw pixelsHigh:uh bitsPerSample:8
                     samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
                        bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:uw * 4 bitsPerPixel:32];
        if (!rep || !rep.bitmapData)
            return nil;
        std::uint8_t* up = rep.bitmapData;
        for (int y = 0; y < uh; ++y)
            for (int x = 0; x < uw; ++x)
                std::memcpy(&up[(static_cast<size_t>(y) * uw + x) * 4],
                            &f.rgba[(static_cast<size_t>(y / m) * w + x / m) * 4], 4);
        if (uw != pw || uh != ph)
        {
            NSBitmapImageRep* scaled = [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:nullptr pixelsWide:pw pixelsHigh:ph bitsPerSample:8
                         samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
                            bitmapFormat:0 bytesPerRow:pw * 4 bitsPerPixel:32];
            NSGraphicsContext* gc = scaled ? [NSGraphicsContext graphicsContextWithBitmapImageRep:scaled] : nil;
            if (!gc)
                return nil;
            std::memset(scaled.bitmapData, 0, static_cast<size_t>(pw) * ph * 4);
            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext setCurrentContext:gc];
            gc.imageInterpolation = NSImageInterpolationHigh;
            [rep drawInRect:NSMakeRect(0, 0, pw, ph) fromRect:NSZeroRect
                  operation:NSCompositingOperationCopy fraction:1.0 respectFlipped:NO hints:nil];
            [gc flushGraphics];
            [NSGraphicsContext restoreGraphicsState];
            rep = scaled;
        }
        rep.size = points;
        NSImage* nsImage = [[NSImage alloc] initWithSize:points];
        [nsImage addRepresentation:rep];
        return [[NSCursor alloc] initWithImage:nsImage hotSpot:hot];
    }

    // The NSCursor for the active token's current animation step, or nil for the system arrow.
    NSCursor* customCursor()
    {
        HostCursor* c = activeCursor;
        if (!c || !c->custom || !view || backHeight <= 0)
            return nil;
        const NSRect b = view.bounds;
        if (b.size.height <= 0)
            return nil;
        // [CHOSEN, not recovered] The menu's .ani cursors grow with the HUD on high-resolution back
        // buffers (2560x1664: 2 back-buffer pixels per cursor pixel) instead of shrinking with it,
        // never past retail's size against the UI (MenuViewport::bitmapScale rounds down). On
        // screen a cursor pixel is that many back-buffer pixels, kept between 0.5 and 4 points.
        // The rule is macos::cursorPointsPerPixel (CursorScale.h), which a headless check pins;
        // this call only passes on the view's height in points and the back buffer's size.
        const CGFloat scale = static_cast<CGFloat>(macos::cursorPointsPerPixel(b.size.height, backWidth, backHeight));
        const CGFloat backing = window && window.backingScaleFactor > 0 ? window.backingScaleFactor : 1.0;
        if (!c->cursors || std::fabs(c->builtScale - scale) > 0.001 || c->builtBacking != backing)
        {
            NSMutableArray<NSCursor*>* built = [NSMutableArray arrayWithCapacity:c->anim.frames.size()];
            for (const CursorFrame& f : c->anim.frames)
            {
                NSCursor* cursor = buildCursorFrame(f, scale, backing);
                if (!cursor)
                {
                    built = [NSMutableArray array];
                    break;
                }
                [built addObject:cursor];
            }
            c->cursors = built;
            c->builtScale = scale;
            c->builtBacking = backing;
            // [TEMP] [CHOSEN, not recovered] BF2_HOST_TRACE=1: the size each cursor build shows.
            // [RETIRE-WHEN: macos-options-validated]
            static const bool trace = [] { const char* e = std::getenv("BF2_HOST_TRACE"); return e && e[0] == '1'; }();
            if (trace && !c->anim.frames.empty())
            {
                const CursorFrame& f = c->anim.frames[0];
                std::fprintf(stderr, "[TEMP] [HOST] cursor %zu frame(s) %dx%d at %.3f pt per cursor pixel "
                             "(bitmapScale %u, back=%dx%d, backing %.1f): %.1fx%.1f pt\n",
                             c->anim.frames.size(), f.width, f.height, scale,
                             macos::cursorBitmapScale(backWidth, backHeight), backWidth, backHeight,
                             backing, f.width * scale, f.height * scale);
                std::fflush(stderr);
                // BF2_HOST_CURSOR_DUMP=DIR also writes the first frame's bitmap as built, since
                // back-buffer captures do not contain the system cursor.
                const char* dump = std::getenv("BF2_HOST_CURSOR_DUMP");
                NSImage* image = built.count ? built[0].image : nil;
                NSBitmapImageRep* rep = image.representations.count
                    ? (NSBitmapImageRep*)image.representations[0] : nil;
                if (dump && dump[0] && [rep isKindOfClass:[NSBitmapImageRep class]])
                {
                    NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
                    NSString* path = [NSString stringWithFormat:@"%s/cursor_%dx%d_scale%.3f.png", dump,
                                      backWidth, backHeight, scale];
                    [png writeToFile:path atomically:YES];
                }
            }
        }
        if (c->cursors.count == 0 || c->anim.steps.empty())
            return nil;
        // Windows plays an animated cursor's steps for their jiffies (1/60 s) and loops.
        size_t step = 0;
        if (c->anim.steps.size() > 1 && c->totalJiffies > 0)
        {
            const double elapsed = ([NSProcessInfo processInfo].systemUptime - cursorAnimStart) * 60.0;
            long t = static_cast<long>(std::fmod(std::max(0.0, elapsed), static_cast<double>(c->totalJiffies)));
            while (step + 1 < c->anim.steps.size() && t >= c->anim.jiffies[step])
                t -= c->anim.jiffies[step++];
        }
        const int frame = c->anim.steps[step];
        return frame >= 0 && static_cast<NSUInteger>(frame) < c->cursors.count ? c->cursors[frame] : nil;
    }

    bool pointerOverView() const
    {
        if (!window || !view || !window.visible)
            return false;
        const NSPoint screen = [NSEvent mouseLocation];
        if ([NSWindow windowNumberAtPoint:screen belowWindowWithWindowNumber:0] != window.windowNumber)
            return false;
        const NSPoint p = [view convertPoint:[window convertPointFromScreen:screen] fromView:nil];
        return NSPointInRect(p, view.bounds);
    }

    // [CHOSEN, not recovered] Keeps the cursor image in step with SetCursor's token: the view's
    // cursor rect holds the current frame, and while the active application's pointer is over the
    // view the frame is set directly so animation steps and token changes show at once. Outside
    // the view and in other applications AppKit's own cursors apply. Visibility is applyCursor's.
    void updateCursorImage()
    {
        if (testHeadless || !window || !view)
        {
            rectCursor = shownCursor = nil;
            return;
        }
        NSCursor* cursor = customCursor();
        if (cursor != rectCursor)
        {
            rectCursor = cursor;
            [window invalidateCursorRectsForView:view];
        }
        if (!appActive || !windowKey || !pointerOverView())
        {
            shownCursor = nil;
            return;
        }
        NSCursor* target = cursor ? cursor : [NSCursor arrowCursor];
        if (target != shownCursor)
        {
            [target set];
            shownCursor = target;
        }
    }

    // Applies the combined cursor-visibility rule. The cursor is only ever hidden while the
    // application is active and its full-screen Space, if it has one, is on screen, so other
    // applications and other Spaces never inherit a hidden cursor.
    void applyCursor()
    {
        const bool focus = hasFocus();
        const bool wantHidden = appActive && !spaceHidden && (window != nil || testHeadless) &&
            (cursorCounter < 0 || (relativeMouse && focus) || (cursorHiddenInView && mouseInView));
        const bool realCursor = !testHeadless && !scripted;
        if (wantHidden != cursorHidden && realCursor)
        {
            if (wantHidden)
                [NSCursor hide];
            else
                [NSCursor unhide];
        }
        cursorHidden = wantHidden;
        const bool wantDisassociated = relativeMouse && focus;
        if (wantDisassociated != mouseDisassociated)
        {
            if (realCursor)
            {
                if (wantDisassociated)
                    warpToCentre();
                CGAssociateMouseAndMouseCursorPosition(wantDisassociated ? false : true);
            }
            mouseDisassociated = wantDisassociated;
        }
    }

    void warpToCentre()
    {
        if (!window || !view || scripted)
            return;
        const NSRect inWindow = [view convertRect:view.bounds toView:nil];
        const NSRect onScreen = [window convertRectToScreen:inWindow];
        NSArray<NSScreen*>* screens = [NSScreen screens];
        if (screens.count == 0)
            return;
        // CoreGraphics global coordinates have their origin at the primary display's top left.
        const CGFloat primaryHeight = screens[0].frame.size.height;
        const CGPoint centre = CGPointMake(NSMidX(onScreen), primaryHeight - NSMidY(onScreen));
        CGWarpMouseCursorPosition(centre);
    }

    // View-relative location in back-buffer pixels, origin top left.
    void locate(NSEvent* event, float& x, float& y, bool& inside) const
    {
        x = y = 0.0f;
        inside = false;
        if (testHeadless && !view)
        {
            x = static_cast<float>(event.locationInWindow.x);
            y = static_cast<float>(event.locationInWindow.y);
            inside = true;
            return;
        }
        if (!view || event.window != window)
            return;
        const NSPoint p = [view convertPoint:event.locationInWindow fromView:nil];
        const NSRect b = view.bounds;
        if (b.size.width <= 0 || b.size.height <= 0)
            return;
        inside = NSPointInRect(p, b);
        x = static_cast<float>(p.x * backWidth / b.size.width);
        y = static_cast<float>((b.size.height - p.y) * backHeight / b.size.height);
    }

    void setFocusState(bool key, bool active)
    {
        if (scriptedFocus)
            key = active = true;                 // [TEMP] scripted runs keep focus
        const bool activeChanged = active != appActive;
        const bool changed = key != windowKey || activeChanged;
        const bool hadFocus = hasFocus();
        windowKey = key;
        appActive = active;
        if (!changed)
            return;
        // [TEMP] [CHOSEN, not recovered] BF2_HOST_TRACE=1: each focus transition, for unattended
        // deactivate/reactivate runs. [RETIRE-WHEN: macos-options-validated]
        static const bool trace = [] { const char* e = std::getenv("BF2_HOST_TRACE"); return e && e[0] == '1'; }();
        if (trace)
            std::fprintf(stderr, "[host] focus key=%d active=%d fullscreen=%d\n",
                         key ? 1 : 0, active ? 1 : 0, fullscreen.wanted() ? 1 : 0);
        if (activeChanged && !active)
            ++deactivations;
        if (!hasFocus())
            wheelRemainder = 0.0;
        applyCursor();
        for (IHostEventListener* l : snapshot())
        {
            if (activeChanged)
                l->onAppActivationChanged(active);
            l->onFocusChanged(key, active);
        }
        if (!hadFocus && hasFocus())
            reportPointer();
    }

    void requestClose()
    {
        for (IHostEventListener* l : snapshot())
            l->onCloseRequested();
        quit = true;
    }

    void notifyResize()
    {
        geometryChanged("resize");
        for (IHostEventListener* l : snapshot())
            l->onResize(backWidth, backHeight, minimized);
    }

    // The view's frame may have changed (a resize, a full-screen transition): re-choose the
    // magnification filter, and trace the geometry.
    void geometryChanged(const char* why)
    {
        updateMagnification();
        // [TEMP] [CHOSEN, not recovered] BF2_HOST_TRACE=1: window geometry after each resize or
        // full-screen switch, for unattended option runs. [RETIRE-WHEN: macos-options-validated]
        static const bool trace = [] { const char* e = std::getenv("BF2_HOST_TRACE"); return e && e[0] == '1'; }();
        if (trace && window)
        {
            NSScreen* s = window.screen ?: [NSScreen mainScreen];
            const NSRect c = [window contentRectForFrameRect:window.frame];
            const NSRect backing = [view convertRectToBacking:view.bounds];
            std::fprintf(stderr, "[TEMP] [HOST] %s back=%dx%d fullscreen=%d inSpace=%d content=%.0fx%.0f@(%.0f,%.0f) "
                         "screen=%.0fx%.0f scale=%.1f drawable=%.0fx%.0f backing=%.0fx%.0f "
                         "magnification=%s window=%ld\n",
                         why, backWidth, backHeight, fullscreen.wanted() ? 1 : 0, fullscreen.inSpace() ? 1 : 0,
                         NSWidth(c), NSHeight(c), NSMinX(c), NSMinY(c),
                         NSWidth(s.frame), NSHeight(s.frame), s.backingScaleFactor,
                         layer ? layer.drawableSize.width : 0.0, layer ? layer.drawableSize.height : 0.0,
                         NSWidth(backing), NSHeight(backing),
                         layer ? layer.magnificationFilter.UTF8String : "none",
                         static_cast<long>(window.windowNumber));
            std::fflush(stderr);
        }
    }

    // Whether the application counts as active for a full-screen request. [TEMP] A scripted run
    // with forced focus never asks for activation and still goes full screen.
    bool activeForFullscreen() const { return scriptedFocus || nsAppActive(); }

    // [CHOSEN, not recovered] Native full screen (NativeFullscreen.h). Starts the transition the
    // state answered. A windowed game window is NSWindowCollectionBehaviorFullScreenNone, so the
    // green button and the system's full-screen shortcut do not take it into a Space behind the
    // engine's back; entering makes it a full-screen primary window and resizable, as AppKit
    // expects of a window it fills the screen with, and both are undone once it is windowed again
    // (restoreWindowed). The toggle is queued for the run loop - the next pump, or the host's own
    // wait in createWindow - rather than made inside the delegate callback or engine call that
    // asked for it; `delay` is the pause before a refused entry is tried again.
    void startFullscreenTransition(macos::NativeFullscreen::Action action, double delay = 0.0)
    {
        if (action == macos::NativeFullscreen::None || !window)
            return;
        if (action == macos::NativeFullscreen::Enter)
        {
            window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
            window.styleMask |= NSWindowStyleMaskResizable;
            fullscreenEnterAt = uptime();
            std::fprintf(stderr, "[host] full screen: entering the game window's own Space\n");
        }
        else
            std::fprintf(stderr, "[host] full screen: leaving the game window's Space\n");
        cancelQueuedToggle();
        [delegate performSelector:@selector(toggleGameWindowFullScreen) withObject:nil afterDelay:delay];
    }

    void cancelQueuedToggle()
    {
        [NSObject cancelPreviousPerformRequestsWithTarget:delegate
                                                 selector:@selector(toggleGameWindowFullScreen) object:nil];
    }

    // The queued toggle. -toggleFullScreen: has no direction, so it is made only while the window
    // is not yet where the running transition leads: a toggle that found it there would take it
    // out again.
    void toggleFullscreenNow()
    {
        if (!window || !fullscreen.transitioning())
            return;                              // given up meanwhile
        const bool entering = fullscreen.entering();
        const bool inSpace = (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
        if (inSpace == entering)
        {
            // Already there without this toggle. When AppKit has announced a transition its end
            // is still to come; otherwise nothing is under way and the state is brought in line.
            if (!fullscreen.announced())
            {
                if (entering) fullscreenEntered();
                else          fullscreenExited();
            }
            return;
        }
        if (entering && !activeForFullscreen())
        {
            // The user went to another application since the entry was queued: no entry behind
            // their back, it follows the activation as a request made while inactive does.
            fullscreen.postpone();
            restoreWindowed();
            std::fprintf(stderr, "[host] full screen: the application is inactive; the entry follows the activation\n");
            return;
        }
        fullscreen.toggled();
        [window toggleFullScreen:nil];
    }

    // The window is windowed again: no full-screen behaviour, not resizable, and sized for the
    // back buffer (a resolution change made in full screen applies to the restored window too).
    void restoreWindowed()
    {
        if (!window)
            return;
        window.styleMask &= ~NSWindowStyleMaskResizable;
        window.collectionBehavior = NSWindowCollectionBehaviorFullScreenNone;
        const NSPoint topLeft = NSMakePoint(NSMinX(window.frame), NSMaxY(window.frame));
        [window setContentSize:windowedContentSize(backWidth, backHeight, window.screen ?: [NSScreen mainScreen],
                                                   window.styleMask)];
        [window setFrameTopLeftPoint:topLeft];
    }

    // windowDidEnterFullScreen / windowDidExitFullScreen / windowDidFailTo{Enter,Exit}FullScreen.
    void fullscreenEntered()
    {
        const macos::NativeFullscreen::Action next = fullscreen.didEnter();
        std::fprintf(stderr, "[host] full screen: in the game window's own Space after %d ms\n",
                     static_cast<int>((uptime() - fullscreenEnterAt) * 1000.0));
        syncFocus();
        afterFullscreenTransition();
        startFullscreenTransition(next);
    }
    // Whether the game window's full-screen Space is the active one (spaceHidden). Read on every
    // focus sync, so on every run of the event loop and when the active Space changes. Not
    // during a transition, in which the window is on its way between two Spaces.
    void updateSpaceHidden()
    {
        const bool hidden = window && !testHeadless && fullscreen.inSpace() && !fullscreen.transitioning() &&
                            !window.isOnActiveSpace;
        if (hidden == spaceHidden)
            return;
        spaceHidden = hidden;
        std::fprintf(stderr, "[host] full screen: the game's Space is %s\n", hidden ? "off screen" : "on screen again");
        applyCursor();                           // the cursor rule reads it too
    }
    void fullscreenExited()
    {
        if (fullscreen.entering())               // AppKit reports an entry it refused as an exit
        {
            enterRefused(true);
            return;
        }
        const macos::NativeFullscreen::Action next = fullscreen.didExit();
        syncFocus();                             // out of its Space: no longer hidden with it
        if (next != macos::NativeFullscreen::Enter)
            restoreWindowed();
        if (fullscreen.userLeft())
            std::fprintf(stderr, "[host] full screen: left through the system; the game stays windowed until it is "
                         "switched to full screen again\n");
        else
            std::fprintf(stderr, "[host] full screen: windowed\n");
        afterFullscreenTransition();
        startFullscreenTransition(next);
    }
    // windowDidFailToEnterFullScreen, or windowDidExitFullScreen during an entry (asExit).
    void enterRefused(bool asExit)
    {
        switch (fullscreen.enterRefused(activeForFullscreen(), asExit))
        {
        case macos::NativeFullscreen::Duplicate:
            return;
        case macos::NativeFullscreen::Retry:
            std::fprintf(stderr, "[host] full screen: AppKit refused to enter the Space; trying again\n");
            startFullscreenTransition(macos::NativeFullscreen::Enter, kFullscreenRetryDelay);
            return;
        case macos::NativeFullscreen::Waits:
            restoreWindowed();
            std::fprintf(stderr, "[host] full screen: AppKit refused to enter the Space while the application is "
                         "inactive; it follows the activation\n");
            return;
        case macos::NativeFullscreen::GivenUp:
            restoreWindowed();
            if (fullscreen.wanted())
                std::fprintf(stderr, "[host] full screen: AppKit refused to enter the Space; not retried before "
                             "the next switch or activation\n");
            else
                std::fprintf(stderr, "[host] full screen: windowed\n");
            return;
        }
    }

    void exitRefused()
    {
        fullscreen.exitRefused();
        std::fprintf(stderr, "[host] full screen: AppKit refused to leave the Space; not retried before the next "
                     "switch or activation\n");
    }

    // [CHOSEN, not recovered] createWindow's wait for the window's way into its Space. The engine
    // does not service AppKit for seconds after createWindow returns, so the host runs the event
    // loop itself and the transition takes AppKit's own half second instead of ending with the
    // first frames. It waits until the transition has ended and the Space it led into is the
    // active one; retries of a refused entry fall inside the wait. At the limit a transition
    // AppKit has announced is left to end with the engine's first frames; one it never announced
    // will bring no callback and is given up.
    void waitForFullscreen(double seconds)
    {
        if (!window || !fullscreen.transitioning())
            return;
        const double start = uptime();
        while ((fullscreen.transitioning() || (fullscreen.inSpace() && spaceHidden)) && uptime() - start < seconds)
            service(0.02, AppKitOnly);
        const int ms = static_cast<int>((uptime() - start) * 1000.0);
        if (!fullscreen.transitioning())
        {
            service(0.0, AppKitOnly);            // the key status that follows the transition
            return;
        }
        if (fullscreen.announced())
        {
            std::fprintf(stderr, "[host] full screen: the transition has not ended after %d ms; it ends while the "
                         "engine starts\n", ms);
            return;
        }
        cancelQueuedToggle();
        fullscreen.abandon();
        restoreWindowed();
        std::fprintf(stderr, "[host] full screen: AppKit did not start the transition within %d ms; the game stays "
                     "windowed until the next switch or activation\n", ms);
    }

    void afterFullscreenTransition()
    {
        if (!window)
            return;
        if (visible && !minimized && appActive)
            [window makeFirstResponder:view];
        [window invalidateCursorRectsForView:view];
        if (mouseDisassociated)
            warpToCentre();
        geometryChanged("full-screen transition");
    }

    // The keyboard type travels with each key event (kCGKeyboardEventKeyboardType); LMGetKbdType
    // only reflects the last event, which need not be this keyboard's.
    static bool eventIsISO(NSEvent* event)
    {
        CGEventRef cg = event.CGEvent;
        const SInt16 type = cg ? static_cast<SInt16>(CGEventGetIntegerValueField(cg, kCGKeyboardEventKeyboardType))
                               : static_cast<SInt16>(LMGetKbdType());
        return KBGetLayoutType(type) == kLayoutTypeISO;
    }

    void flushCapsRelease()
    {
        if (!capsReleasePending)
            return;
        capsReleasePending = false;
        capsRelease.timeMs = ClientHost::tickCountMs();
        for (IHostEventListener* l : snapshot())
            l->onKey(capsRelease);
    }

    void handleKey(NSEvent* event, bool down)
    {
        HostKeyEvent key;
        key.dik       = macos::dikFromMacKeyCode(event.keyCode, eventIsISO(event));
        key.down      = down;
        key.repeat    = down && event.isARepeat;
        key.modifiers = modifiers = static_cast<unsigned int>(event.modifierFlags);
        key.timeMs    = msFromSeconds(event.timestamp);
        updateAsyncEnter(event.keyCode, key.down, key.repeat);
        for (IHostEventListener* l : snapshot())
            l->onKey(key);
        if (!down)
            return;
        HostTextEvent text;
        text.dik       = key.dik;
        text.modifiers = key.modifiers;
        text.repeat    = key.repeat;
        NSString* chars = event.characters;
        for (NSUInteger i = 0; i < chars.length; ++i)
            macos::appendNormalizedCocoaText(text, [chars characterAtIndex:i]);
        if (text.characters.empty())
            return;
        for (IHostEventListener* l : snapshot())
            l->onText(text);
    }

    // Modifier keys: flagsChanged reports the new flag set; the device-dependent bit of the key
    // that changed tells press from release.
    void handleFlags(NSEvent* event)
    {
        const unsigned int flags = static_cast<unsigned int>(event.modifierFlags);
        const unsigned short keyCode = event.keyCode;
        const unsigned int previous = modifiers;
        modifiers = flags;
        HostKeyEvent key;
        key.dik       = macos::dikFromMacKeyCode(keyCode, eventIsISO(event));
        key.repeat    = false;
        key.modifiers = flags;
        key.timeMs    = msFromSeconds(event.timestamp);

        unsigned int deviceMask = 0;
        unsigned int genericMask = 0;
        switch (keyCode)
        {
        case 0x38: deviceMask = HostModLeftShift;    genericMask = HostModShift;   break;
        case 0x3C: deviceMask = HostModRightShift;   genericMask = HostModShift;   break;
        case 0x3B: deviceMask = HostModLeftControl;  genericMask = HostModControl; break;
        case 0x3E: deviceMask = HostModRightControl; genericMask = HostModControl; break;
        case 0x3A: deviceMask = HostModLeftAlt;      genericMask = HostModAlt;     break;
        case 0x3D: deviceMask = HostModRightAlt;     genericMask = HostModAlt;     break;
        case 0x37: deviceMask = HostModLeftSuper;    genericMask = HostModSuper;   break;
        case 0x36: deviceMask = HostModRightSuper;   genericMask = HostModSuper;   break;
        case 0x3F: genericMask = HostModFunction; break;
        case 0x39:                                   // Caps Lock: a lock-state toggle
            if ((flags ^ previous) & HostModCapsLock)
            {
                flushCapsRelease();
                key.down = true;
                for (IHostEventListener* l : snapshot())
                    l->onKey(key);
                capsRelease = key;
                capsRelease.down = false;
                capsReleasePending = true;
            }
            return;
        default:
            return;
        }
        bool down;
        if (deviceMask != 0 && ((flags | previous) & 0x207Fu) != 0)
            down = (flags & deviceMask) != 0;
        else                                         // no device bits: fall back to the generic flag
            down = (flags & genericMask) != 0;
        key.down = down;
        for (IHostEventListener* l : snapshot())
            l->onKey(key);
    }

    void handleMouseMove(NSEvent* event)
    {
        HostMouseMoveEvent move;
        move.dx = static_cast<float>(event.deltaX);
        move.dy = static_cast<float>(event.deltaY);
        move.timeMs = msFromSeconds(event.timestamp);
        move.captured = relativeMouse && hasFocus();
        locate(event, move.x, move.y, move.insideView);
        if (move.insideView != mouseInView)
        {
            mouseInView = move.insideView;
            applyCursor();
        }
        for (IHostEventListener* l : snapshot())
            l->onMouseMove(move);
    }

    void handleMouseButton(NSEvent* event, bool down)
    {
        HostMouseButtonEvent button;
        button.button = static_cast<int>(event.buttonNumber);
        button.down = down;
        button.clickCount = static_cast<int>(event.clickCount);
        button.timeMs = msFromSeconds(event.timestamp);
        button.captured = relativeMouse && hasFocus();
        bool inside = false;
        locate(event, button.x, button.y, inside);
        if (button.button < 0 || button.button > 31)
            return;
        const unsigned int bit = 1u << button.button;
        if (down)
        {
            // A press on the title bar or outside the view is window management, not game input.
            if (!inside && !relativeMouse)
                return;
            buttonsDown |= bit;
        }
        else
        {
            if (!(buttonsDown & bit))
                return;
            buttonsDown &= ~bit;
        }
        for (IHostEventListener* l : snapshot())
            l->onMouseButton(button);
    }

    // DirectInput's wheel axis reports whole notches of WHEEL_DELTA (120). Line-based wheels give
    // deltaY in lines (accelerated); trackpads and continuous wheels give scrollingDeltaY in points,
    // counted as a notch per 10 points. Fractions of a notch are carried until a whole notch has
    // accumulated, so a trackpad swipe reads as the notches it amounts to rather than as a small
    // wheel value on every input sample. Momentum-phase events are the inertia AppKit synthesizes
    // after the fingers lift, not device motion, and are dropped; a new gesture starts from zero.
    void handleWheel(NSEvent* event)
    {
        if (event.momentumPhase != NSEventPhaseNone)
            return;
        if (event.phase & NSEventPhaseBegan)
            wheelRemainder = 0.0;
        double delta = event.hasPreciseScrollingDeltas ? event.scrollingDeltaY * 12.0
                                                       : event.deltaY * 120.0;
        if (event.isDirectionInvertedFromDevice)
            delta = -delta;
        delta += wheelRemainder;
        const double whole = std::trunc(delta / 120.0) * 120.0;
        wheelRemainder = delta - whole;
        if (whole == 0.0)
            return;
        HostMouseWheelEvent wheel;
        wheel.delta = static_cast<int>(whole);
        wheel.captured = relativeMouse && hasFocus();
        wheel.timeMs = msFromSeconds(event.timestamp);
        for (IHostEventListener* l : snapshot())
            l->onMouseWheel(wheel);
    }

    // Routes one event to the listeners before AppKit dispatches it. Only events for the focused
    // game window are game input; everything else goes to AppKit alone.
    void route(NSEvent* event)
    {
        if (!window && !testHeadless)
            return;
        if (scripted)
            return;                              // [TEMP] scripted runs ignore the real devices
        const bool focus = hasFocus();
        switch (event.type)
        {
        case NSEventTypeKeyDown:
            if (focus) handleKey(event, true);
            else inputDropped("key down");
            break;
        case NSEventTypeKeyUp:
            if (focus) handleKey(event, false);
            break;
        case NSEventTypeFlagsChanged:
            if (focus) handleFlags(event);
            else modifiers = static_cast<unsigned int>(event.modifierFlags);
            break;
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
            if (focus) handleMouseMove(event);
            else if (insideGameView(event)) inputDropped("mouse move");
            break;
        case NSEventTypeLeftMouseDown:
        case NSEventTypeRightMouseDown:
        case NSEventTypeOtherMouseDown:
            if (focus) handleMouseButton(event, true);
            else if (insideGameView(event)) inputDropped("mouse down");
            break;
        case NSEventTypeLeftMouseUp:
        case NSEventTypeRightMouseUp:
        case NSEventTypeOtherMouseUp:
            handleMouseButton(event, false);       // releases after a focus change still pair up
            break;
        case NSEventTypeScrollWheel:
            if (focus) handleWheel(event);
            else if (event.window == window) inputDropped("scroll wheel");
            break;
        default:
            break;
        }
    }

    // Whether a mouse event lies inside the game view; the title bar is window management.
    bool insideGameView(NSEvent* event) const
    {
        float x = 0.0f, y = 0.0f;
        bool inside = false;
        locate(event, x, y, inside);
        return inside;
    }

    // Hands the GCMouse samples published since the last pump to the listeners, on this (the
    // engine) thread. Without focus they are discarded, as the routed NSEvents are.
    void drainRawMouse()
    {
        macos::RawMouseSample samples[256];
        for (;;)
        {
            const std::size_t n = macos::rawMouseDrain(samples, 256);
            if (n == 0)
                break;
            if (!hasFocus() || scripted)
                continue;
            for (std::size_t i = 0; i < n; ++i)
            {
                HostRawMouseEvent raw;
                raw.dx = samples[i].dx;
                raw.dy = samples[i].dy;
                raw.timeMs = samples[i].timeMs;
                for (IHostEventListener* l : snapshot())
                    l->onRawMouseMove(raw);
            }
        }
    }

    // The busy service: hold what the window server queued, animate the cursor. Throttled to one
    // run per 50 ms of busy time, counted from the last pump or service. A queued or running
    // full-screen transition belongs to the real pump; do not run its selectors or callbacks here.
    void serviceWhileBusy()
    {
        if (!appInitialized || testHeadless || scripted || servicing || fullscreen.transitioning())
            return;
        const double now = uptime();
        if (lastPumpAt != 0.0 && now - lastPumpAt < 0.05)
            return;
        lastPumpAt = now;
        servicing = true;
        service(0.0, HoldEvents);
        servicing = false;
    }

    // Keeps one event for the next pump. A pointer move that follows another one replaces it (the
    // engine reads positions, and relative motion during a load is stale): a long load with the
    // mouse in motion would otherwise hold thousands. Past the cap only moves are dropped.
    void holdEvent(NSEvent* event)
    {
        if (!heldEvents)
            heldEvents = [NSMutableArray array];
        const bool isMove = event.type == NSEventTypeMouseMoved;
        if (isMove && heldEvents.count && heldEvents.lastObject.type == NSEventTypeMouseMoved)
            [heldEvents removeLastObject];
        else if (isMove && heldEvents.count >= kHeldEventCap)
        {
            ++heldDropped;
            return;
        }
        [heldEvents addObject:event];
    }

    // The pump's first step: the held events, in order, as the queue would have delivered them.
    // A press outside the game view (the title bar, the menu bar, another window of this process)
    // is not handed to AppKit late: its tracking would wait for a release that has already come.
    // Such a press is window management, which the engine never sees either (route).
    void deliverHeldEvents(ServiceMode mode = DeliverInput)
    {
        if (!heldEvents.count)
            return;
        NSArray<NSEvent*>* held = heldEvents;
        heldEvents = nil;
        if (heldDropped)
        {
            std::fprintf(stderr, "[host] %u pointer move(s) dropped while the application was busy\n", heldDropped);
            heldDropped = 0;
        }
        for (NSEvent* event in held)
        {
            if (mode == DeliverInput)
                route(event);
            switch (event.type)
            {
            case NSEventTypeLeftMouseDown:
            case NSEventTypeRightMouseDown:
            case NSEventTypeOtherMouseDown:
            case NSEventTypeLeftMouseDragged:
            case NSEventTypeRightMouseDragged:
            case NSEventTypeOtherMouseDragged:
                if (!insideGameView(event))
                    continue;
                break;
            default:
                break;
            }
            [NSApp sendEvent:event];
        }
    }

    // [CHOSEN, not recovered] One run of AppKit's event loop. DeliverInput routes events to
    // the engine before AppKit (the frame's pump). AppKitOnly serves the host's bounded waits;
    // held events go to AppKit first, preserving queue order without delivering game input.
    // HoldEvents only dequeues and holds events and animates the cursor: no dispatch or focus sync.
    void service(double timeout, ServiceMode mode)
    {
        @autoreleasepool
        {
            if (mode == AppKitOnly)
                deliverHeldEvents(AppKitOnly);
            NSDate* until = timeout > 0.0 ? [NSDate dateWithTimeIntervalSinceNow:timeout]
                                          : [NSDate distantPast];
            for (;;)
            {
                NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:until
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if (!event)
                    break;
                if (mode == HoldEvents)
                    holdEvent(event);
                else
                {
                    if (mode == DeliverInput)
                        route(event);
                    [NSApp sendEvent:event];
                }
                until = [NSDate distantPast];
            }
            if (mode != HoldEvents)
            {
                [NSApp updateWindows];
                followFocus();
            }
            updateCursorImage();
        }
    }

    bool pump(double timeout)
    {
        if (!appInitialized)
            return quit;
        flushCapsRelease();
        lastPumpAt = uptime();
        @autoreleasepool
        {
            deliverHeldEvents();
            if (quitHeld)
            {
                quitHeld = false;
                if (!quit)
                    requestClose();
            }
        }
        service(timeout, DeliverInput);
        if (pointerOwed)                         // the first pump after createWindow
        {
            pointerOwed = false;
            if (hasFocus())
                reportPointer();
        }
        drainRawMouse();
        if (scripted)
            inputscript::inputScript().tick(*this);
        return quit;
    }

    // [TEMP] IInputScriptSink: the scripted counterparts of handleKey/handleFlags/handleMouse*/
    // handleWheel/drainRawMouse, built with the same fields those fill from an NSEvent.
    void scriptKey(unsigned short keyCode, bool down, const std::u16string& text) override
    {
        unsigned int deviceMask = 0, genericMask = 0;
        switch (keyCode)
        {
        case 0x38: deviceMask = HostModLeftShift;    genericMask = HostModShift;   break;
        case 0x3C: deviceMask = HostModRightShift;   genericMask = HostModShift;   break;
        case 0x3B: deviceMask = HostModLeftControl;  genericMask = HostModControl; break;
        case 0x3E: deviceMask = HostModRightControl; genericMask = HostModControl; break;
        case 0x3A: deviceMask = HostModLeftAlt;      genericMask = HostModAlt;     break;
        case 0x3D: deviceMask = HostModRightAlt;     genericMask = HostModAlt;     break;
        case 0x37: deviceMask = HostModLeftSuper;    genericMask = HostModSuper;   break;
        case 0x36: deviceMask = HostModRightSuper;   genericMask = HostModSuper;   break;
        default: break;
        }
        if (deviceMask)
        {
            if (down) modifiers |= deviceMask | genericMask;
            else
            {
                modifiers &= ~deviceMask;
                // The generic flag stays while the other side's key is held.
                const unsigned int sides = genericMask == HostModShift   ? (HostModLeftShift | HostModRightShift)
                                         : genericMask == HostModControl ? (HostModLeftControl | HostModRightControl)
                                         : genericMask == HostModAlt     ? (HostModLeftAlt | HostModRightAlt)
                                                                         : (HostModLeftSuper | HostModRightSuper);
                if (!(modifiers & sides)) modifiers &= ~genericMask;
            }
        }
        HostKeyEvent key;
        key.dik       = macos::dikFromMacKeyCode(keyCode, false); // scripts name ANSI positions
        key.down      = down;
        key.repeat    = false;
        key.modifiers = modifiers;
        key.timeMs    = ClientHost::tickCountMs();
        updateAsyncEnter(keyCode, key.down, key.repeat);
        for (IHostEventListener* l : snapshot())
            l->onKey(key);
        if (!down || deviceMask)
            return;
        HostTextEvent chars;
        chars.dik       = key.dik;
        chars.modifiers = modifiers;
        chars.repeat    = false;
        for (char16_t c : text)
            macos::appendNormalizedCocoaText(chars, static_cast<unsigned short>(c));
        if (chars.characters.empty())
            return;
        for (IHostEventListener* l : snapshot())
            l->onText(chars);
    }

    void scriptPointerInit()
    {
        if (scriptPointerSet)
            return;
        scriptPointerSet = true;
        scriptX = backWidth * 0.5f;
        scriptY = backHeight * 0.5f;
    }

    void scriptMove(float x, float y, float dx, float dy)
    {
        HostMouseMoveEvent move;
        move.dx = dx;
        move.dy = dy;
        move.x = x;
        move.y = y;
        move.insideView = x >= 0 && y >= 0 && x < backWidth && y < backHeight;
        move.captured = relativeMouse && hasFocus();
        move.timeMs = ClientHost::tickCountMs();
        mouseInView = move.insideView;
        for (IHostEventListener* l : snapshot())
            l->onMouseMove(move);
    }

    void scriptMouseTo(float x, float y) override
    {
        scriptPointerInit();
        const float dx = x - scriptX, dy = y - scriptY;
        scriptX = x;
        scriptY = y;
        scriptMove(x, y, dx, dy);
    }

    void scriptMouseBy(float dx, float dy) override
    {
        scriptPointerInit();
        if (!relativeMouse)
        {
            scriptX += dx;
            scriptY += dy;
        }
        scriptMove(scriptX, scriptY, dx, dy);    // the parked cursor stays put in relative mode
        if (hasFocus() && rawMouseAvailable())   // a GCMouse reports the same motion raw
        {
            HostRawMouseEvent raw;
            raw.dx = dx;
            raw.dy = dy;
            raw.timeMs = ClientHost::tickCountMs();
            for (IHostEventListener* l : snapshot())
                l->onRawMouseMove(raw);
        }
    }

    void scriptButton(int number, bool down) override
    {
        scriptPointerInit();
        if (number < 0 || number > 31)
            return;
        const unsigned int bit = 1u << number;
        if (down) buttonsDown |= bit;
        else
        {
            if (!(buttonsDown & bit))
                return;
            buttonsDown &= ~bit;
        }
        HostMouseButtonEvent button;
        button.button = number;
        button.down = down;
        button.clickCount = 1;
        button.x = scriptX;
        button.y = scriptY;
        button.captured = relativeMouse && hasFocus();
        button.timeMs = ClientHost::tickCountMs();
        for (IHostEventListener* l : snapshot())
            l->onMouseButton(button);
    }

    void scriptWheel(int delta) override
    {
        HostMouseWheelEvent wheel;
        wheel.delta = delta;
        wheel.captured = relativeMouse && hasFocus();
        wheel.timeMs = ClientHost::tickCountMs();
        for (IHostEventListener* l : snapshot())
            l->onMouseWheel(wheel);
    }

    std::u16string scriptKeyCharacter(unsigned short keyCode, bool shift) override
    {
        // [TEMP] Scripts name ANSI positions. DIK identifies that physical key before the
        // current layout maps it back to Cocoa, including ISO grave/OEM_102 swapping.
        return ClientHost::instance().keyCharacter(macos::dikFromMacKeyCode(keyCode, false),
                                                   shift ? HostModShift : 0u);
    }

    void scriptQuit() override
    {
        if (!quit)
            requestClose();
    }

    static bool rawMouseAvailable() { return macos::rawMouseCount() > 0; }
};

} // namespace host
} // namespace hfe
} // namespace dice

@implementation OpenBF2HostDelegate
{
@public
    dice::hfe::host::ClientHost::Impl* impl;
}
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    (void)sender;
    ClientHost::instance().requestQuit();
    return NO;                                   // the engine tears the window down itself
}
- (void)windowDidBecomeKey:(NSNotification*)note     { (void)note; if (impl) impl->syncFocus(); }
- (void)windowDidResignKey:(NSNotification*)note     { (void)note; if (impl) impl->syncFocus(); }
- (void)windowDidMiniaturize:(NSNotification*)note   { (void)note; if (impl) { impl->minimized = true;  impl->notifyResize(); } }
- (void)windowDidDeminiaturize:(NSNotification*)note
{
    (void)note;
    if (!impl)
        return;
    impl->minimized = false;
    impl->notifyResize();
}
// [CHOSEN, not recovered] Native full screen (NativeFullscreen.h).
- (void)windowWillEnterFullScreen:(NSNotification*)note { (void)note; if (impl) impl->fullscreen.willEnter(); }
- (void)windowDidEnterFullScreen:(NSNotification*)note  { (void)note; if (impl) impl->fullscreenEntered(); }
- (void)windowWillExitFullScreen:(NSNotification*)note  { (void)note; if (impl) impl->fullscreen.willExit(); }
- (void)windowDidExitFullScreen:(NSNotification*)note   { (void)note; if (impl) impl->fullscreenExited(); }
- (void)windowDidFailToEnterFullScreen:(NSWindow*)window { (void)window; if (impl) impl->enterRefused(false); }
- (void)windowDidFailToExitFullScreen:(NSWindow*)window  { (void)window; if (impl) impl->exitRefused(); }
- (void)windowDidResize:(NSNotification*)note           { (void)note; if (impl) impl->updateMagnification(); }
- (void)applicationDidBecomeActive:(NSNotification*)note { (void)note; if (impl) impl->syncFocus(); }
- (void)applicationDidResignActive:(NSNotification*)note { (void)note; if (impl) impl->syncFocus(); }
- (void)activeSpaceDidChange:(NSNotification*)note { (void)note; if (impl) impl->syncFocus(); }
- (void)applicationDidFinishLaunching:(NSNotification*)note { (void)note; if (impl) impl->endLaunchRun(false); }
- (void)launchTimedOut { if (impl) impl->endLaunchRun(true); }
- (void)toggleGameWindowFullScreen { if (impl) impl->toggleFullscreenNow(); }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    (void)sender;
    ClientHost::instance().requestQuit();
    return NSTerminateCancel;                    // shutdown runs through the engine, not exit()
}
- (void)quitRequested:(id)sender
{
    (void)sender;
    ClientHost::instance().requestQuit();
}
@end

namespace dice {
namespace hfe {
namespace host {

} // namespace host
} // namespace hfe
} // namespace dice

namespace {
ClientHost::Impl* s_hostImpl = nullptr;   // the singleton's state, for the view's cursor rect
NSCursor* currentViewCursor()
{
    return s_hostImpl ? s_hostImpl->rectCursor : nil;
}
void refreshViewMagnification()
{
    if (s_hostImpl)
        s_hostImpl->updateMagnification();
}
} // namespace

namespace dice {
namespace hfe {
namespace host {

ClientHost& ClientHost::instance()
{
    static ClientHost* host = new ClientHost();    // lives for the process, like the Win32 class
    return *host;
}

ClientHost::ClientHost() : m_impl(new Impl()) { s_hostImpl = m_impl; }
ClientHost::~ClientHost() { delete m_impl; }

bool ClientHost::initApplication()
{
    if (m_impl->appInitialized)
        return true;
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        m_impl->delegate = [[OpenBF2HostDelegate alloc] init];
        m_impl->delegate->impl = m_impl;
        [NSApp setDelegate:m_impl->delegate];
        [[[NSWorkspace sharedWorkspace] notificationCenter]
            addObserver:m_impl->delegate selector:@selector(activeSpaceDidChange:)
                   name:NSWorkspaceActiveSpaceDidChangeNotification object:nil];

        NSMenu* menuBar = [[NSMenu alloc] init];
        NSMenuItem* appItem = [[NSMenuItem alloc] init];
        [menuBar addItem:appItem];
        NSMenu* appMenu = [[NSMenu alloc] init];
        NSString* name = [[NSProcessInfo processInfo] processName];
        [appMenu addItemWithTitle:[@"Hide " stringByAppendingString:name]
                           action:@selector(hide:) keyEquivalent:@"h"];
        [appMenu addItem:[NSMenuItem separatorItem]];
        NSMenuItem* quitItem = [appMenu addItemWithTitle:[@"Quit " stringByAppendingString:name]
                                                  action:@selector(quitRequested:)
                                           keyEquivalent:@"q"];
        quitItem.target = m_impl->delegate;
        appItem.submenu = appMenu;
        [NSApp setMainMenu:menuBar];

        m_impl->scripted = inputscript::inputScript().loadFromEnvironment();   // [TEMP] BF2_INPUT_SCRIPT
        {
            const char* realFocus = std::getenv("BF2_INPUT_SCRIPT_REAL_FOCUS");
            m_impl->scriptedFocus = m_impl->scripted && !(realFocus && realFocus[0] == '1');
        }
        const NSApplicationActivationPolicy initialPolicy = [NSApp activationPolicy];
        const double start = Impl::uptime();
        if (![NSRunningApplication currentApplication].finishedLaunching)
        {
            m_impl->launching = true;
            [m_impl->delegate performSelector:@selector(launchTimedOut) withObject:nil afterDelay:Impl::kLaunchLimit];
            [NSApp run];
        }
        const char* policy = initialPolicy == NSApplicationActivationPolicyRegular ? "regular" :
                             initialPolicy == NSApplicationActivationPolicyAccessory ? "accessory" : "prohibited";
        std::fprintf(stderr, "[host] launch: finished after %d ms (activation policy at the start: %s)%s\n",
                     static_cast<int>((Impl::uptime() - start) * 1000.0), policy,
                     m_impl->launchTimedOut ? "; AppKit did not report its end" : "");
        // [CHOSEN, not recovered] The client is a helper executable inside the launcher's bundle
        // (Contents/MacOS/bf2_client), not the bundle's CFBundleExecutable, so the Dock would show
        // a generic icon for it. Use the enclosing bundle's icon when there is one.
        {
            NSBundle* bundle = [NSBundle mainBundle];
            NSString* iconName = [bundle objectForInfoDictionaryKey:@"CFBundleIconFile"];
            if ([bundle.bundlePath hasSuffix:@".app"] && iconName.length > 0)
            {
                NSImage* icon = [bundle imageForResource:iconName];
                if (icon)
                    [NSApp setApplicationIconImage:icon];
            }
        }
        // [CHOSEN, not recovered] Set the policy after launch: set earlier, an unbundled
        // program's menu bar stays dead until the next application switch.
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        macos::rawMouseStart();                         // GCMouse connections, observed for the run
    }
    m_impl->appInitialized = true;
    m_impl->activateAtLaunch();
    return true;
}

bool ClientHost::createWindow(const HostWindowDesc& desc)
{
    if (!initApplication() || m_impl->window)
        return false;
    @autoreleasepool
    {
        // [CHOSEN, not recovered] The host has not read AppKit's queue since the launch. An
        // activation that arrived while the engine started (the launcher's hand-over on a slow
        // start, a click on the Dock icon), or its loss (the user went to another application),
        // counts from here.
        m_impl->service(0.0, Impl::AppKitOnly);
        NSScreen* screen = [NSScreen mainScreen];
        if (!screen)
            return false;
        const NSRect screenFrame = screen.frame;
        const int width  = desc.width  > 0 ? desc.width  : 800;
        const int height = desc.height > 0 ? desc.height : 600;

        const NSWindowStyleMask windowedStyle =
            NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
        const NSSize points = Impl::windowedContentSize(width, height, screen, windowedStyle);
        const NSRect content = NSMakeRect(0, 0, points.width, points.height);
        OpenBF2HostWindow* window =
            [[OpenBF2HostWindow alloc] initWithContentRect:content
                                                 styleMask:windowedStyle
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
        if (!window)
            return false;
        window.releasedWhenClosed = NO;
        window.title = [NSString stringWithUTF8String:desc.title.c_str()] ?: @"BF2";
        window.backgroundColor = [NSColor blackColor];
        window.acceptsMouseMovedEvents = YES;
        window.collectionBehavior = NSWindowCollectionBehaviorFullScreenNone;
        window.delegate = m_impl->delegate;
        window.animationBehavior = NSWindowAnimationBehaviorNone;   // shown at once, no order-in animation
        // Win32 positions are the frame's top left from the screen's top left.
        [window setFrameTopLeftPoint:NSMakePoint(NSMinX(screenFrame) + desc.x,
                                                 NSMaxY(screenFrame) - desc.y)];

        OpenBF2HostView* view = [[OpenBF2HostView alloc] initWithFrame:content];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        window.contentView = view;

        m_impl->window = window;
        m_impl->view = view;
        m_impl->layer = static_cast<CAMetalLayer*>(view.layer);
        m_impl->backWidth = width;
        m_impl->backHeight = height;
        m_impl->fullscreen = macos::NativeFullscreen();
        m_impl->cursorHiddenInView = desc.hideCursor;
        m_impl->updateDrawable();

        m_impl->visible = true;
        [window makeKeyAndOrderFront:nil];
        [window makeFirstResponder:view];
        m_impl->windowOrderedIn();

        // [CHOSEN, not recovered] A full-screen start goes into the window's own Space now
        // (NativeFullscreen.h) when the application is active; otherwise it follows the
        // activation, including one received after the host's bounded startup waits.
        if (desc.fullscreen)
            setFullscreen(true);
    }
    m_impl->service(0.0, Impl::AppKitOnly);              // the new window's key status; starts a queued toggle
    m_impl->waitForFullscreen(3.0);
    m_impl->syncFocus();
    m_impl->pointerOwed = true;
    return true;
}

void ClientHost::destroyWindow()
{
    if (!m_impl->window)
        return;
    m_impl->relativeMouse = false;
    m_impl->cursorCounter = 0;
    m_impl->cursorHiddenInView = false;
    @autoreleasepool
    {
        // A window closed in full screen takes its Space with it; a toggle still queued for the
        // next run-loop pass is dropped.
        m_impl->cancelQueuedToggle();
        m_impl->window.delegate = nil;
        [m_impl->window orderOut:nil];
        [m_impl->window close];
        m_impl->window = nil;
        m_impl->view = nil;
        m_impl->layer = nil;
    }
    m_impl->fullscreen = macos::NativeFullscreen();
    m_impl->spaceHidden = false;
    m_impl->visible = false;
    m_impl->activationWindowAt = 0.0;
    m_impl->windowKey = false;
    // A quit posted for the destroyed window (Window::destroy posts one, as DestroyWindow
    // does) does not carry over to a later window. A quit before the first window stands, so
    // Cmd-Q during startup ends the game at its first frame.
    m_impl->quit = false;
    m_impl->pointerOwed = false;
    m_impl->buttonsDown = 0;
    m_impl->applyCursor();                       // window gone: unhide and re-associate
}

bool  ClientHost::hasWindow() const    { return m_impl->window != nil; }
void* ClientHost::rendererWindowHandle() const { return (__bridge void*)m_impl->view; }
int   ClientHost::clientWidth() const  { return m_impl->backWidth; }
int   ClientHost::clientHeight() const { return m_impl->backHeight; }

void ClientHost::show(bool visible)
{
    if (!m_impl->window)
        return;
    if (visible)
    {
        [m_impl->window makeKeyAndOrderFront:nil];
        [m_impl->window makeFirstResponder:m_impl->view];
    }
    else
    {
        [m_impl->window orderOut:nil];
    }
    m_impl->visible = visible;
}

bool ClientHost::isVisible() const { return m_impl->window != nil && m_impl->visible; }

void ClientHost::minimize()
{
    if (m_impl->window)
        [m_impl->window miniaturize:nil];
}

void ClientHost::setTitle(const std::string& title)
{
    if (m_impl->window)
        m_impl->window.title = [NSString stringWithUTF8String:title.c_str()] ?: @"";
}

void ClientHost::resizeClient(int width, int height)
{
    if (!m_impl->window || width <= 0 || height <= 0)
        return;
    m_impl->backWidth = width;
    m_impl->backHeight = height;
    if (m_impl->fullscreen.windowed())           // in or on the way to its Space it fills the screen
    {
        const NSRect frame = m_impl->window.frame;
        const NSPoint topLeft = NSMakePoint(NSMinX(frame), NSMaxY(frame));
        NSScreen* screen = m_impl->window.screen ?: [NSScreen mainScreen];
        [m_impl->window setContentSize:Impl::windowedContentSize(width, height, screen,
                                                                  m_impl->window.styleMask)];
        [m_impl->window setFrameTopLeftPoint:topLeft];
    }
    m_impl->updateDrawable();
    m_impl->notifyResize();
}

// [CHOSEN, not recovered] The requested mode is reported at once; the Space transition is
// asynchronous (createWindow waits for a full-screen start). Inactive applications wait for
// activation. The drawable size and the engine's WM_SIZE notification stay unchanged.
void ClientHost::setFullscreen(bool fullscreen)
{
    if (!m_impl->window || fullscreen == m_impl->fullscreen.wanted())
        return;
    OpenBF2HostWindow* window = m_impl->window;
    const bool active = m_impl->activeForFullscreen();
    m_impl->startFullscreenTransition(m_impl->fullscreen.request(fullscreen, active));
    if (m_impl->fullscreen.waiting())
        std::fprintf(stderr, "[host] full screen: requested while the application is inactive; it follows the activation\n");
    [window makeKeyAndOrderFront:nil];
    [window makeFirstResponder:m_impl->view];
    m_impl->updateDrawable();
    m_impl->notifyResize();
}

bool ClientHost::isFullscreen() const { return m_impl->fullscreen.wanted(); }
bool ClientHost::hasFocus() const     { return m_impl->hasFocus(); }
bool ClientHost::isAppActive() const  { return m_impl->appActive; }
unsigned int ClientHost::deactivationCount() const { return m_impl->deactivations; }

bool ClientHost::pumpEvents()                     { return m_impl->pump(0.0); }
bool ClientHost::waitEvents(double timeoutSeconds){ return m_impl->pump(timeoutSeconds); }
void ClientHost::serviceWhileBusy()               { m_impl->serviceWhileBusy(); }
bool ClientHost::quitRequested() const            { return m_impl->quit; }
void ClientHost::postQuitMessage()                { m_impl->quit = true; }

void ClientHost::requestQuit()
{
    if (m_impl->servicing)
    {
        m_impl->quitHeld = true;                 // made by the next pump (Impl::serviceWhileBusy)
        return;
    }
    if (!m_impl->quit)
        m_impl->requestClose();
}

void ClientHost::addListener(IHostEventListener* listener)
{
    if (listener && std::find(m_impl->listeners.begin(), m_impl->listeners.end(), listener)
                    == m_impl->listeners.end())
        m_impl->listeners.push_back(listener);
}

void ClientHost::removeListener(IHostEventListener* listener)
{
    m_impl->listeners.erase(std::remove(m_impl->listeners.begin(), m_impl->listeners.end(), listener),
                            m_impl->listeners.end());
}

int ClientHost::showCursor(bool show)
{
    m_impl->cursorCounter += show ? 1 : -1;
    m_impl->applyCursor();
    return m_impl->cursorCounter;
}

void ClientHost::setCursorHiddenInView(bool hidden)
{
    m_impl->cursorHiddenInView = hidden;
    m_impl->applyCursor();
}

void ClientHost::setCursorShape(HostCursorShape shape)
{
    // AppKit has no public busy cursor; both shapes show the arrow (docs/macos-s2-4.md).
    setCursor(cursorHandle(shape));
}

void* ClientHost::cursorHandle(HostCursorShape shape)
{
    static HostCursor shapes[2];
    return &shapes[shape == HostCursorBusy ? 1 : 0];
}

void* ClientHost::loadCursorFromFile(const char* path)
{
    if (!path || !*path)
        return nullptr;
    FILE* file = std::fopen(path, "rb");
    if (!file)
        return nullptr;                          // LoadCursorFromFileA answers null
    std::vector<std::uint8_t> bytes;
    std::uint8_t chunk[16384];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof chunk, file)) > 0 && bytes.size() < (16u << 20))
        bytes.insert(bytes.end(), chunk, chunk + got);
    std::fclose(file);
    std::unique_ptr<HostCursor> cursor(new HostCursor());
    if (!decodeCursorFile(bytes.data(), bytes.size(), cursor->anim))
    {
        std::fprintf(stderr, "[host] cursor %s: not a decodable .ani/.cur, the arrow is shown\n", path);
        return cursorHandle(HostCursorArrow);
    }
    cursor->custom = true;
    for (int j : cursor->anim.jiffies)
        cursor->totalJiffies += j;
    static std::vector<std::unique_ptr<HostCursor>> loaded;   // tokens live for the process
    loaded.push_back(std::move(cursor));
    return loaded.back().get();
}

void ClientHost::setCursor(void* handle)
{
    if (!handle)
    {
        m_impl->activeCursor = nullptr;
        setCursorHiddenInView(true);
        m_impl->updateCursorImage();
        return;
    }
    HostCursor* cursor = static_cast<HostCursor*>(handle);
    if (cursor != m_impl->activeCursor)
    {
        // SetCursor with the current handle keeps an animation running; a new one starts it.
        m_impl->activeCursor = cursor;
        m_impl->cursorAnimStart = [NSProcessInfo processInfo].systemUptime;
    }
    setCursorHiddenInView(false);
    m_impl->updateCursorImage();
}

void ClientHost::setRelativeMouseMode(bool enabled)
{
    m_impl->relativeMouse = enabled;
    m_impl->applyCursor();
}

bool ClientHost::relativeMouseMode() const { return m_impl->relativeMouse; }

unsigned short ClientHost::consumeAsyncEnterState()
{
    // [CHOSEN, not recovered] Cocoa supplies the consuming press bit. Quartz
    // supplies hardware state independently of gameswf delivery/focus. Scripted
    // and headless tests read their injected keys instead of the real keyboard.
    const bool held = (m_impl->scripted || m_impl->testHeadless)
        ? (m_impl->enterHeld[0] || m_impl->enterHeld[1])
        : (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Return)
            || CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_ANSI_KeypadEnter));
    const unsigned short state = (held ? 0x8000 : 0) | (m_impl->enterPressed ? 1 : 0);
    m_impl->enterPressed = false;
    return state;
}
bool ClientHost::rawMouseAvailable() const { return macos::rawMouseCount() > 0; }

unsigned int ClientHost::modifierFlags() const
{
    return static_cast<unsigned int>([NSEvent modifierFlags]);
}

bool ClientHost::controlShortcutDown() const
{
    return (modifierFlags() & (HostModControl | HostModSuper)) != 0;
}

const char* ClientHost::keyDisplayName(unsigned char dik) const
{
    switch (dik) {
    case 56:  return "Option";
    case 184: return "Right Option";
    case 219: return "Command";
    case 220: return "Right Command";
    case 69:  return "Clear";
    default:  return dikDisplayName(dik);
    }
}

std::u16string ClientHost::keyCharacter(unsigned char dik, unsigned int modifiers) const
{
    const unsigned short keyCode = macos::macKeyCodeFromDik(dik, currentKeyboardIsISO());
    if (keyCode == 0xFFFF)
        return {};
    // UCKeyTranslate takes the Carbon modifier byte (shiftKey >> 8, alphaLock >> 8).
    UInt32 modifierState = 0;
    if (modifiers & HostModShift)    modifierState |= (shiftKey >> 8) & 0xFF;
    if (modifiers & HostModCapsLock) modifierState |= (alphaLock >> 8) & 0xFF;
    std::u16string result;
    TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
    if (!source)
        return result;
    CFDataRef data = static_cast<CFDataRef>(
        TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
    if (data)
    {
        const UCKeyboardLayout* layout =
            reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(data));
        UInt32 deadKeyState = 0;
        UniChar chars[8];
        UniCharCount length = 0;
        if (UCKeyTranslate(layout, keyCode, kUCKeyActionDisplay, modifierState, LMGetKbdType(),
                           kUCKeyTranslateNoDeadKeysBit, &deadKeyState, 8, &length, chars) == noErr)
        {
            for (UniCharCount i = 0; i < length; ++i)
                if (chars[i] >= 0x20 && chars[i] != 0x7F)
                    result.push_back(static_cast<char16_t>(chars[i]));
        }
    }
    CFRelease(source);
    return result;
}

bool ClientHost::setClipboardText(const std::string& text)
{
    @autoreleasepool
    {
        NSString* string = [[NSString alloc] initWithBytes:text.data()
                                                    length:text.size()
                                                  encoding:NSWindowsCP1252StringEncoding];
        if (!string)
            return false;
        NSPasteboard* board = [NSPasteboard generalPasteboard];
        [board clearContents];
        return [board setString:string forType:NSPasteboardTypeString] ? true : false;
    }
}

bool ClientHost::getClipboardText(std::string& text) const
{
    @autoreleasepool
    {
        NSString* string = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        if (!string)
            return false;
        NSData* bytes = [string dataUsingEncoding:NSWindowsCP1252StringEncoding
                             allowLossyConversion:YES];
        if (!bytes)
            return false;
        text.assign(static_cast<const char*>(bytes.bytes), bytes.length);
        return true;
    }
}

// [CHOSEN, not recovered] Core Graphics reports 0 Hz for the modes of built-in panels; their
// real rate is the screen's maximum frame rate (60 on non-ProMotion panels).
static int builtInPanelRefreshHz()
{
    if (@available(macOS 12.0, *))
    {
        NSScreen* screen = [NSScreen mainScreen];
        if (screen && screen.maximumFramesPerSecond > 0)
            return static_cast<int>(screen.maximumFramesPerSecond);
    }
    return 0;
}

// [CHOSEN, not recovered] Offer macOS's logical "looks like" sizes, omitting oversized 1x
// modes on HiDPI displays. These are internal render sizes, never display-mode switches.
std::vector<HostDisplayModeInfo> ClientHost::displayModes() const
{
    std::vector<macos::DisplayMode> modes;
    const void* keys[] = { kCGDisplayShowDuplicateLowResolutionModes };
    const void* values[] = { kCFBooleanTrue };
    CFDictionaryRef options = CFDictionaryCreate(0, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef list = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), options);
    CFRelease(options);
    if (!list)
        return {};
    for (CFIndex i = 0; i < CFArrayGetCount(list); ++i)
    {
        CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(list, i);
        modes.push_back({static_cast<int>(CGDisplayModeGetWidth(mode)),
                         static_cast<int>(CGDisplayModeGetHeight(mode)),
                         static_cast<int>(CGDisplayModeGetPixelWidth(mode)),
                         static_cast<int>(CGDisplayModeGetPixelHeight(mode)),
                         static_cast<int>(std::lround(CGDisplayModeGetRefreshRate(mode))),
                         CGDisplayModeIsUsableForDesktopGUI(mode)});
    }
    CFRelease(list);
    return macos::selectDisplayModes(modes, builtInPanelRefreshHz());
}

HostDisplayModeInfo ClientHost::desktopMode() const
{
    HostDisplayModeInfo info = { 0, 0, 0, 32 };
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(CGMainDisplayID());
    if (mode)
    {
        // [PLATFORM] Points, not pixels (see the declaration).
        info.width = static_cast<int>(CGDisplayModeGetWidth(mode));
        info.height = static_cast<int>(CGDisplayModeGetHeight(mode));
        info.refreshHz = static_cast<int>(std::lround(CGDisplayModeGetRefreshRate(mode)));
        if (info.refreshHz == 0)
            info.refreshHz = builtInPanelRefreshHz();
        CGDisplayModeRelease(mode);
    }
    return info;
}

void ClientHost::preventDisplaySleep(bool prevent)
{
    if (prevent == m_impl->sleepAssertionHeld)
        return;
    if (prevent)
    {
        m_impl->sleepAssertionHeld = IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
            CFSTR("OpenBF2 game window"), &m_impl->sleepAssertion) == kIOReturnSuccess;
    }
    else
    {
        IOPMAssertionRelease(m_impl->sleepAssertion);
        m_impl->sleepAssertionHeld = false;
    }
}

bool ClientHost::openURL(const std::string& url)
{
    @autoreleasepool
    {
        NSString* string = [NSString stringWithUTF8String:url.c_str()];
        if (!string)
            return false;
        NSURL* target = [NSURL URLWithString:string];
        if (!target || !target.scheme)
            target = [NSURL fileURLWithPath:string];
        return [[NSWorkspace sharedWorkspace] openURL:target] ? true : false;
    }
}

unsigned int ClientHost::tickCountMs()
{
    return msFromSeconds([[NSProcessInfo processInfo] systemUptime]);
}

} // namespace host

namespace macos {

// MacSystem.h. [CHOSEN, not recovered] After its last frame a restarting foreground game waits
// for its new instance to become a regular application, then yields and activates it. macOS 14
// requires the application in front to allow activation. Background and forced-focus runs
// hand nothing over; no old frame runs as an inactive full-screen application.
void handOverActivation(int processId)
{
    host::ClientHost::Impl* impl = s_hostImpl;
    const pid_t pid = static_cast<pid_t>(processId);
    if (!impl || !impl->appInitialized || impl->scriptedFocus || pid <= 0)
        return;
    @autoreleasepool
    {
        if (![NSApp isActive])
        {
            std::fprintf(stderr, "[host] restart: this instance is not the active application; pid %d starts "
                         "without the front\n", static_cast<int>(pid));
            return;
        }
        const double start = host::ClientHost::Impl::uptime();
        NSRunningApplication* next = nil;
        bool ready = false;
        for (;;)
        {
            int status = 0;
            if (waitpid(pid, &status, WNOHANG) == pid)
            {
                std::fprintf(stderr, "[host] restart: pid %d has exited; nothing handed over\n", static_cast<int>(pid));
                return;
            }
            next = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
            ready = next && next.finishedLaunching && next.activationPolicy == NSApplicationActivationPolicyRegular;
            if (ready || host::ClientHost::Impl::uptime() - start >= 2.0)
                break;
            impl->service(0.05, host::ClientHost::Impl::AppKitOnly);
        }
        const int ms = static_cast<int>((host::ClientHost::Impl::uptime() - start) * 1000.0);
        if (!ready)
        {
            std::fprintf(stderr, "[host] restart: pid %d was not a regular application after %d ms; nothing handed "
                         "over\n", static_cast<int>(pid), ms);
            return;
        }
        if (![NSApp isActive])
        {
            std::fprintf(stderr, "[host] restart: the user left this instance meanwhile; pid %d starts without the "
                         "front\n", static_cast<int>(pid));
            return;
        }
        BOOL allowed = NO;
        if (@available(macOS 14.0, *))
        {
            [NSApp yieldActivationToApplication:next];
            allowed = [next activateFromApplication:[NSRunningApplication currentApplication] options:0];
        }
        else
        {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            allowed = [next activateWithOptions:NSApplicationActivateIgnoringOtherApps];
#pragma clang diagnostic pop
        }
        std::fprintf(stderr, "[host] restart: the front handed to pid %d after %d ms (the system %s the request)\n",
                     static_cast<int>(pid), ms, allowed ? "accepted" : "refused");
    }
}

} // namespace macos

#if defined(OPENBF2_HOST_TEST_SEAMS)
namespace macos {

void* CocoaHostTestHooks::nativeWindow(host::ClientHost& host)
{
    return (__bridge void*)host.m_impl->window;
}

void* CocoaHostTestHooks::nativeView(host::ClientHost& host)
{
    return (__bridge void*)host.m_impl->view;
}

void* CocoaHostTestHooks::metalLayer(host::ClientHost& host)
{
    return (__bridge void*)host.m_impl->layer;
}

bool CocoaHostTestHooks::isISOKeyboard(const host::ClientHost&)
{
    return currentKeyboardIsISO();
}

void CocoaHostTestHooks::overrideFocusForTest(host::ClientHost& host, bool windowKey, bool appActive)
{
    host.m_impl->testFocusOverride = true;
    host.m_impl->setFocusState(windowKey, appActive);
}

void CocoaHostTestHooks::setSystemFocusForTest(host::ClientHost& host, const SystemFocus* state)
{
    host::ClientHost::Impl::TestSystem& system = host.m_impl->testSystem;
    system.set = state != nullptr;
    if (!state)
        return;
    system.appActive      = state->appActive;
    system.windowKey      = state->windowKey;
    system.now = state->now;
    system.systemFrontmost = state->systemFrontmost;
    system.cooperativeActivation = state->cooperativeActivation;
    system.windowVisible = state->windowVisible;
}

void CocoaHostTestHooks::focusCallbackForTest(host::ClientHost& host, bool whileBusy)
{
    const bool servicing = host.m_impl->servicing;
    host.m_impl->servicing = whileBusy;
    host.m_impl->syncFocus();
    host.m_impl->servicing = servicing;
}

void CocoaHostTestHooks::pumpFocusForTest(host::ClientHost& host)
{
    host.m_impl->followFocus();
}

void CocoaHostTestHooks::launchActivationForTest(host::ClientHost& host, bool forcedFocus)
{
    host.m_impl->activationRequests = 0;
    host.m_impl->activationFirstRequestAt = 0.0;
    host.m_impl->activationRequestAt = 0.0;
    host.m_impl->activationLastRequestAt = 0.0;
    host.m_impl->activationWindowAt = 0.0;
    host.m_impl->fullscreen = NativeFullscreen();
    host.m_impl->scriptedFocus = forcedFocus;
    host.m_impl->requestActivation("launch");
}

void CocoaHostTestHooks::windowOrderedInForTest(host::ClientHost& host, bool fullscreen)
{
    host.m_impl->windowOrderedIn();
    host.m_impl->fullscreen.request(fullscreen, host.m_impl->activeForFullscreen());
}

CocoaHostTestHooks::StartupState CocoaHostTestHooks::startupStateForTest(const host::ClientHost& host)
{
    return { host.m_impl->activationRequests, host.m_impl->fullscreen.waiting(), host.m_impl->fullscreen.entering() };
}

CocoaHostTestHooks::DroppedInputStats CocoaHostTestHooks::droppedInputStatsForTest(const host::ClientHost& host)
{
    DroppedInputStats stats;
    stats.droppedInputs = host.m_impl->droppedInputs;
    stats.dropReports = host.m_impl->dropReports;
    return stats;
}

void CocoaHostTestHooks::enableHeadlessForTest(host::ClientHost& host)
{
    host.m_impl->testHeadless = true;
}

void CocoaHostTestHooks::holdEventForTest(host::ClientHost& host, void* nsEvent)
{
    host.m_impl->holdEvent((__bridge NSEvent*)nsEvent);
}

unsigned CocoaHostTestHooks::heldEventsForTest(const host::ClientHost& host)
{
    return static_cast<unsigned>(host.m_impl->heldEvents.count);
}

void CocoaHostTestHooks::deliverHeldEventsForTest(host::ClientHost& host)
{
    @autoreleasepool
    {
        host.m_impl->deliverHeldEvents();
    }
}

void CocoaHostTestHooks::serviceAppKitOnlyForTest(host::ClientHost& host)
{
    host.m_impl->service(0.0, host::ClientHost::Impl::AppKitOnly);
}

void CocoaHostTestHooks::injectEventForTest(host::ClientHost& host, void* nsEvent)
{
    @autoreleasepool
    {
        host.m_impl->flushCapsRelease();        // pumpEvents' order: pending Caps Lock release first
        host.m_impl->route((__bridge NSEvent*)nsEvent);
    }
}

void CocoaHostTestHooks::pumpRawMouseForTest(host::ClientHost& host)
{
    host.m_impl->drainRawMouse();
}

void CocoaHostTestHooks::cursorStateForTest(const host::ClientHost& host, bool& hidden, bool& disassociated)
{
    hidden = host.m_impl->cursorHidden;
    disassociated = host.m_impl->mouseDisassociated;
}

} // namespace macos
#endif

} // namespace hfe
} // namespace dice
