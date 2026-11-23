// [CHOSEN, not recovered] macOS port: Cocoa implementation of ClientHost. Compile with ARC
// (-fobjc-arc). See ClientHost.h for the contract and docs/macos-s2-4.md for the design.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <unistd.h>                         // getpid, getppid: who started this process

#include "../Host/ClientHost.h"
#include "../Host/KeyMap.h"
#include "CocoaHostTestHooks.h"
#include "CocoaText.h"
#include "MacKeyMap.h"
#include "MacRawMouse.h"
#include "ProcessAncestry.h"                // [CHOSEN] the launcher of this process (activation)
#include "RestartInFront.h"                 // [CHOSEN] a game that restarted itself (activation)
#include "NativeFullscreen.h"               // [CHOSEN] the game window's own full-screen Space
#include "../InputScript.h"         // [TEMP] BF2_INPUT_SCRIPT (docs/macos-c-input-script.md)
#include "../Host/CursorFile.h"
#include "CursorScale.h"                 // [CHOSEN] the cursor's size on screen

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
    // [CHOSEN, not recovered] The game window is in its full-screen Space and that Space is not on
    // screen (the user swiped or switched to another Space). The window keeps the key status and
    // the application can stay active - nothing else may be in front on an empty desktop Space -
    // yet the game is out of sight: the focus gate treats it as unfocused, so the mouse is released
    // and input goes nowhere until the Space is back (occlusionChanged).
    bool spaceHidden = false;
    bool visible        = false;
    bool minimized      = false;
    bool windowKey      = false;
    bool appActive      = false;

    int  backWidth  = 0;            // drawableSize
    int  backHeight = 0;
    NSRect windowedFrame = NSZeroRect;

    int  cursorCounter    = 0;      // ShowCursor display counter
    bool cursorHiddenInView = false;
    bool mouseInView      = false;
    bool relativeMouse    = false;
    bool mouseDisassociated = false;
    bool cursorHidden     = false;  // the [NSCursor hide] balance this host owns
    HostCursorShape cursorShape = HostCursorArrow;
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
    // application focus (the app is activated at window creation and focus changes from outside,
    // e.g. activating another app, reach the engine), so deactivate/reactivate paths can be tested
    // unattended. Scripted input and the cursor rules are unchanged. [RETIRE-WHEN: client-test-automation-replaced]
    bool scriptedFocus = false;     // scripted && !BF2_INPUT_SCRIPT_REAL_FOCUS: focus is forced held
    unsigned int deactivations = 0; // deactivationCount(): active -> inactive transitions

    // [CHOSEN, not recovered] Activation after a start from another application (the setup app
    // runs the client through NSTask; a player or a script starts it from Terminal). Since
    // macOS 14 activation is cooperative: a request made while another application is active may
    // be refused, and a refused game window stays in front of the launcher without key status,
    // so its clicks never reach the menu. The first request is made when the game window is
    // shown (beginActivation) and opens an activation period of ten seconds, in which the
    // request is repeated every half second (updateActivation) - but only at the expense of the
    // launcher, of nobody, or of this process itself. An unrelated application that is in front
    // at the first request (the user works elsewhere while a client starts in the background)
    // gets that one request, made on every macOS version in the form that yields to an active
    // application (requestActivation). It keeps the front unless the system itself grants that
    // request, and an application the user goes to afterwards ends the period. The ten seconds
    // count from the host's first look at the period after window creation, not from the first
    // request (activationCountFrom).
    bool   activationPending = false;  // an activation period is running
    // [CHOSEN, not recovered] The system reports this process active (another app activated it
    // during launch) while NSApp never received the activation; AppKit then keeps isActive NO
    // and its windows non-key, so the engine's focus gate drops every click. When that state
    // lasts, the host trusts the system's answer until NSApp catches up or the user leaves.
    bool   systemActive = false;
    double activationStarted = 0.0;  // focusNow() of the period's first request; the report counts from it
    double activationLastTry = 0.0;  // of its last request, Dock reset or look at a bystander
    // [CHOSEN, not recovered] From when the period's ten seconds count (focusNow()). A look is one
    // run of the event pump, which calls updateActivation. The host is not pumped while the
    // engine initialises: the look inside createWindow follows the first request at once, and the
    // next one comes with the first frame, six to seven seconds later on the Intel test machine
    // and longer on a slower one, with a cold disk cache or in a debug build. Counted from the
    // first request, most of the ten seconds would be gone at that look, or all of them, and the
    // period would end before its Dock reset and its repeated requests could run. So they count
    // - from the first look after the one inside createWindow, for every period that begins at
    //   window creation and whoever is in front at that look (activationLookCounted);
    // - once more from the first look that finds the front free after a wait behind a bystander
    //   (activationRecounted);
    // - from the click that starts a period or ends its wait behind a bystander, in place of the
    //   first request (clickedWhileInactive).
    // The two look-based counts happen once per period each, so a front that alternates between
    // the bystander and this process cannot prolong the requests without end, and every period
    // ends: ten seconds after the last of these counts, or with the request its Dock reset owes
    // (dockAskedAt).
    double activationCountFrom = 0.0;
    bool   activationLookCounted = false;
    bool   activationRecounted = false;
    bool   creationLook = false;       // createWindow is running: a look made now is the one inside
                                       // it (InsideCreateWindow sets and clears this, nothing else)
    // Totals for the tests (CocoaHostTestHooks::activationStatsForTest): every look made (look),
    // and those of them made while createWindow ran.
    unsigned int looks = 0;
    unsigned int looksInsideCreateWindow = 0;
    // [CHOSEN, not recovered] Whose front the repeated requests may take, as pids; 0: nobody's.
    // Launcher: the application in front at the first request when it is this process's parent
    // or another ancestor (launcherAncestry): Terminal above the shells and scripts that started
    // the client, the OpenBF2 app. Bystander: the application in front at the first request when
    // it is no ancestor, or when the process lookup failed. The host takes nothing from it: its
    // one request yields to it (requestActivation), nothing is repeated and the Dock is left
    // alone while it stays in front. Clicks: every application the user left with a click into
    // the inactive game window (clickedWhileInactive), which is the user choosing the game over
    // it; the click makes a bystander one of these. They stay until the period ends: a later
    // click adds the application in front then and replaces none. With a single place for the
    // latest one, the application of the click that started the period was dropped by the next
    // click made with another application in front, and the requests against it stopped early.
    pid_t  activationLauncherPid = 0;
    pid_t  activationBystanderPid = 0;
    std::vector<pid_t> activationClickPids;
    // [CHOSEN, not recovered] A game that restarted itself (RestartInFront.h). The instance that
    // started this process was the active application at that moment and says so in the
    // environment. This process is then entitled to the front: the application that takes the
    // front over from the first instance counts as the launcher, whatever it is. That is the
    // application in front at the first request. When the first instance itself is still in
    // front then, or nobody is, it is the application found in front once the first instance has
    // exited (activationHeirOpen: not found yet). The entitlement belongs to this process's first
    // showing of its game window: the environment is read once, by the first beginActivation
    // (restartMarkerRead), so a window created again later starts like any other.
    bool   restartMarkerRead = false;
    bool   activationHeirOpen = false;
    // Since when the requests have been free to take the front (focusNow()); 0 while the
    // bystander holds it. The system-state fallback and the Dock reset wait one second from
    // here, so a period that waited behind a bystander still gives AppKit that second once the
    // game comes forward.
    double activationFreeSince = 0.0;
    // [CHOSEN, not recovered] An unbundled executable started from Terminal can be left frontmost
    // for the window server while AppKit never completes its activation: NSApp stays inactive,
    // GCMouse delivers nothing, and only switching applications recovers. Activating the Dock
    // first resets that state, then this process asks again (the workaround SDL uses for
    // Terminal launches). Once per activation period, and never against a bystander. The reset
    // leaves the Dock in front until this process asks again, so the period owes that request:
    // its ten seconds do not end it before the request was made and had half a second for its
    // answer (updateActivation).
    double dockResetAt = 0.0;        // focusNow() of the period's Dock activation; 0: none yet,
                                     // -1: there is no Dock process
    double dockAskedAt = 0.0;        // focusNow() of the first request after it; 0: still owed
    // [CHOSEN, not recovered] Focus self-repair. windowKey and appActive cache AppKit's state as
    // the delegate callbacks report it, and the engine's input gate (hasFocus) reads only the
    // cache. A callback that never came, or a game window left without key status, would close
    // the gate for good: every key and click dropped while the game is visibly in front. So each
    // pump re-reads AppKit and brings the cache in line (reconcileFocus), an active application's
    // game window is made key again when nothing else is due the key status (repairKeyWindow), a
    // press into the window repairs before it is routed (repairFocusForPress), and what the gate
    // still drops is reported (inputDropped). None of this asks for activation: taking the front
    // stays with the launch requests above and the user's own click (clickedWhileInactive).
    //
    // keyLostWhileActive: the game window resigned key while the application stayed active.
    // Something else took the keyboard, usually a panel of another process (Spotlight,
    // Notification Centre) that returns it when it closes. Taking it back from a pump would close
    // that panel, so only a press into the window does.
    bool   keyLostWhileActive = false;
    bool   keyRepairing = false;     // a pump-time key repair is under way (logged once)
    double keyRepairAt = 0.0;        // focusNow() of its last attempt
    // Since when the system has called this process frontmost while NSApp is inactive (0: it does
    // not). A press repairs through the system-state fallback only once that has lasted, so the
    // click that brings the application forward from another one is still not game input.
    double systemFrontSince = 0.0;
    bool   dropReported = false;     // inputDropped: one report a second at most
    double dropReportAt = 0.0;
    unsigned int dropsSinceReport = 0;
    unsigned int droppedInputs = 0;  // totals (CocoaHostTestHooks::focusRepairStatsForTest)
    unsigned int dropReports = 0;
    float scriptX = 0.0f, scriptY = 0.0f;
    bool scriptPointerSet = false;

    // [TEMP] setSystemFocusForTest: the answers AppKit and the system would give, so the focus
    // repair and the launch activation can be checked without an application or a window.
    struct TestSystem
    {
        bool   set = false;
        bool   appActive = false;       // [NSApp isActive]
        bool   windowKey = false;       // window.isKeyWindow
        bool   windowOnScreen = false;  // window.isVisible, not miniaturized
        bool   modalWindow = false;     // a modal window of this process, or a sheet on the window
        bool   otherKeyWindow = false;  // another key or key-capable visible window of this process
        bool   makeKeyWorks = true;     // making the game window key succeeds
        pid_t  frontPid = 0;            // the system's frontmost application; 0: none
        double now = 0.0;               // focusNow()
        int    makeKeyCalls = 0;
        // The launch activation. Its requests and Dock resets are counted, none is made.
        bool   activation = false;      // the simulation covers it (activationRuns)
        pid_t  parentPid = 0;           // getppid()
        pid_t  ancestors[6] = {};       // the parent's own ancestors, nearest first; 0 ends them
        bool   ancestryDenied = false;  // the process lookup fails for every process
        bool   realProcessTable = false; // the three above are not used: getppid() and the kernel's
                                        // process table answer, as in the client
        pid_t  dockPid = 0;             // the Dock's pid; 0: there is no Dock process
        int    activationRequests = 0;  // requestActivation: a period's first request
        int    activationIgnoringOthers = 0; // those of them made ignoring other applications
        int    activationRetries = 0;   // requestActivationNow: its repeated requests
        int    dockResets = 0;          // activateDock
        // The simulated process table (macos::parentPidOf): the chain ends at launchd, pid 1.
        pid_t parentOf(pid_t pid) const
        {
            if (ancestryDenied || pid <= 0)
                return -1;
            const size_t count = sizeof ancestors / sizeof ancestors[0];
            if (pid == parentPid)
                return ancestors[0] ? ancestors[0] : 1;
            for (size_t i = 0; i < count && ancestors[i]; ++i)
                if (ancestors[i] == pid)
                    return i + 1 < count && ancestors[i + 1] ? ancestors[i + 1] : 1;
            return -1;                           // no such process
        }
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
    // On screen: ordered in, not hidden with the application, not miniaturized.
    bool windowOnScreen() const
    {
        if (const TestSystem* t = testSystemState())
            return t->windowOnScreen;
        return window != nil && window.isVisible && !window.isMiniaturized;
    }
    // The pid of the application the system calls frontmost; 0 when it names none.
    pid_t systemFrontPid() const
    {
        if (const TestSystem* t = testSystemState())
            return t->frontPid;
        NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
        return front ? front.processIdentifier : 0;
    }
    // The system reports this process active and frontmost, whatever NSApp says.
    bool systemFrontmost() const
    {
        if (const TestSystem* t = testSystemState())
            return t->frontPid == getpid();
        return [NSRunningApplication currentApplication].active && systemFrontPid() == getpid();
    }
    // Another window of this process holds the key status or is due it: a modal window or a sheet
    // on the game window; for a repair nobody asked for (unprompted) also the current key window
    // and any other visible window that can become key, such as an alert. A press into the game
    // window takes the key status from an ordinary window, as AppKit does itself.
    bool keyStatusTaken(bool unprompted) const
    {
        if (const TestSystem* t = testSystemState())
            return t->modalWindow || (unprompted && t->otherKeyWindow);
        NSWindow* modal = [NSApp modalWindow];
        if ((modal && modal != window) || window.attachedSheet)
            return true;
        if (!unprompted)
            return false;
        NSWindow* key = [NSApp keyWindow];
        if (key && key != window)
            return true;
        for (NSWindow* other in [NSApp windows])
            if (other != window && other.isVisible && other.canBecomeKeyWindow)
                return true;
        return false;
    }

    // The application the system calls frontmost, read once for a decision; pid 0 when it names
    // none. The name is for the log: nil when there is none and in a simulated system.
    struct FrontApp
    {
        pid_t pid = 0;
        bool  isDock = false;
        NSString* name = nil;
    };
    FrontApp frontApplication() const
    {
        FrontApp app;
        if (const TestSystem* t = testSystemState())
        {
            app.pid = t->frontPid;
            app.isDock = t->dockPid != 0 && t->frontPid == t->dockPid;
            return app;
        }
        NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (front)
        {
            app.pid = front.processIdentifier;
            app.isDock = [front.bundleIdentifier isEqualToString:@"com.apple.dock"] ? true : false;
            app.name = front.localizedName;
        }
        return app;
    }
    // This process's parent: the launcher itself when an application started the client directly.
    pid_t parentPid() const
    {
        const TestSystem* t = testSystemState();
        if (t && !t->realProcessTable)
            return t->parentPid;
        return getppid();
    }
    // [CHOSEN, not recovered] Whether the application `pid` started this process: it is the
    // parent or a further ancestor (Terminal -> login -> shell -> script -> client). A lookup that
    // fails part of the way answers "unknown", which the caller treats as "no": the direct parent
    // needs no lookup and is still recognised, while an application further up gets a
    // bystander's one cooperative request (requestActivation) and no repeated ones. That costs a
    // click into the game window (or a switch to it) when that request is not granted: before
    // macOS 14 whenever the application is active, from macOS 14 on when the system refuses it.
    // Counting an unknown application as a launcher would let a client started in the
    // background take the front from whatever the user works in.
    macos::ProcessAncestry launcherAncestry(pid_t pid) const
    {
        const TestSystem* t = testSystemState();
        if (t && !t->realProcessTable)
            return macos::processAncestry(pid, t->parentPid, [t](pid_t p) { return t->parentOf(p); });
        return macos::processAncestry(pid, getppid(), macos::parentPidOf);
    }

    // Whether the host asks for activation at all: never in scripted runs, which do not take the
    // user's focus ([TEMP] scriptedFocus), and in a test only against a simulated system that
    // covers the activation.
    bool activationRuns() const
    {
        if (scriptedFocus)
            return false;
        if (const TestSystem* t = testSystemState())
            return t->activation;
        return !testHeadless;
    }

    // [CHOSEN, not recovered] One activation request. `ignoringOtherApps` says whether it may take
    // the front from an application that is active, which only a system before macOS 14 leaves
    // to the caller:
    // - macOS 14 and later: -[NSApplication activate]. Activation is cooperative there: the
    //   system may refuse any request made while another application is active, so the flag
    //   changes nothing.
    // - macOS 11 to 13: activateIgnoringOtherApps:, which AppKit has marked for deprecation since.
    //   With YES this process is activated whatever application is active; with NO "the app is
    //   activated only if no other app is currently active" (NSApplication.h).
    // A request that yields to an active application on all of these systems is called
    // cooperative here. A bystander's one request is cooperative (beginActivation): made with
    // YES, that request alone would take the front from the application the user works in on
    // macOS 11 to 13, whatever the host withholds afterwards. Every other request ignores other
    // applications; the host makes those only while this process may take the front (the first
    // request without a bystander, updateActivation, clickedWhileInactive). The macOS 11 to 13
    // branch follows AppKit's documentation; no such system was at hand to run it on.
    void requestActivation(bool ignoringOtherApps)
    {
        if (TestSystem* t = testSystemState())
        {
            ++t->activationRequests;
            if (ignoringOtherApps)
                ++t->activationIgnoringOthers;
            return;
        }
        if (@available(macOS 14.0, *))
        {
            [NSApp activate];
        }
        else
        {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [NSApp activateIgnoringOtherApps:ignoringOtherApps ? YES : NO];
#pragma clang diagnostic pop
        }
    }

    // [CHOSEN, not recovered] Reads the restart marker (RestartInFront.h), once per process: the
    // first call answers whether the instance that started this process was the active
    // application then, every later call answers no.
    bool takeRestartMarker()
    {
        if (restartMarkerRead)
            return false;
        restartMarkerRead = true;
        return macos::restartedInFront(std::getenv(macos::kRestartedInFrontVariable));
    }

    // The first request, made when the game window is shown, and the decision whose front the
    // repeated requests may take: the application in front now is the launcher when it is this
    // process's parent or another ancestor, and a bystander otherwise. A bystander's request is
    // the cooperative one; a launcher, nobody and this process itself get the request that
    // ignores other applications (requestActivation). A game that restarted itself while it was
    // the active application has no bystander: the application in front counts as its launcher.
    void beginActivation()
    {
        // The marker is for the first showing of the game window, whether or not that showing
        // has to ask for activation.
        const bool restarted = takeRestartMarker();
        if (!activationRuns() || nsAppActive())
            return;
        const double now = focusNow();
        const FrontApp front = frontApplication();
        activationPending = true;
        activationStarted = activationCountFrom = activationLastTry = now;
        activationLookCounted = activationRecounted = false;
        activationLauncherPid = activationBystanderPid = 0;
        activationClickPids.clear();
        activationHeirOpen = false;
        dockResetAt = dockAskedAt = 0.0;
        const char* relation = "nobody is in front";
        if (front.pid == getpid())
            relation = "this process";
        else if (front.pid != 0)
        {
            switch (launcherAncestry(front.pid))
            {
            case macos::AncestryYes:
                activationLauncherPid = front.pid;
                relation = front.pid == parentPid() ? "the parent of this process"
                                                    : "an ancestor of this process";
                break;
            case macos::AncestryNo:
                activationBystanderPid = front.pid;
                relation = "not an ancestor of this process";
                break;
            case macos::AncestryUnknown:
                activationBystanderPid = front.pid;
                relation = "ancestry unknown as the process lookup failed, taken as not an ancestor";
                break;
            }
            if (restarted && activationBystanderPid)
            {
                // Whatever application is in front where the first instance was counts as the
                // launcher: its ancestry does not matter.
                activationLauncherPid = front.pid;
                activationBystanderPid = 0;
                relation = "not an ancestor, in front in place of the instance that restarted the game";
            }
        }
        // The first instance is still in front, or nobody is yet: which application follows it
        // shows at a later look (updateActivation).
        if (restarted && (front.pid == 0 || (front.pid != getpid() && front.pid == parentPid())))
            activationHeirOpen = true;
        activationFreeSince = activationBystanderPid ? 0.0 : now;
        std::fprintf(stderr, "[host] activation requested (frontmost: %s pid %d, %s; parent pid %d)%s%s\n",
                     front.name.UTF8String ?: (front.pid ? "?" : "none"), static_cast<int>(front.pid),
                     relation, static_cast<int>(parentPid()),
                     activationBystanderPid ? " - one cooperative request, no retries or Dock reset while it stays in front"
                                            : "",
                     restarted ? " - restarted by an instance of the game that was the active application" : "");
        requestActivation(activationBystanderPid == 0);
    }

    // Asks for activation the strongest way available while this process may take the front.
    void requestActivationNow()
    {
        if (TestSystem* t = testSystemState())
        {
            ++t->activationRetries;
            return;
        }
        [[NSRunningApplication currentApplication] activateWithOptions:NSApplicationActivateAllWindows];
        requestActivation(true);
        if (window && visible && !minimized && !window.isKeyWindow)
        {
            [window makeKeyAndOrderFront:nil];
            [window makeFirstResponder:view];
        }
    }

    // A repeated request, counted for the half-second cadence. The first one after the period's
    // Dock reset is the request that reset owes (dockAskedAt).
    void repeatRequest(double now)
    {
        activationLastTry = now;
        if (dockResetAt > 0.0 && dockAskedAt == 0.0)
            dockAskedAt = now;
        requestActivationNow();
    }

    // Activates the Dock, which takes the front from whoever holds it. False without a Dock.
    bool activateDock()
    {
        if (TestSystem* t = testSystemState())
        {
            if (t->dockPid == 0)
                return false;
            ++t->dockResets;
            return true;
        }
        NSArray<NSRunningApplication*>* docks =
            [NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.apple.dock"];
        if (docks.count == 0)
            return false;
        [docks.firstObject activateWithOptions:0];
        return true;
    }

    // [CHOSEN, not recovered] A click into the game window while AppKit still reports the
    // application inactive is the user choosing the game: ask for activation again (a request
    // made in response to the user's own input is not refused) and keep retrying from here, so
    // the menu reacts without switching applications. The application in front at the click may
    // lose the front to those requests, whatever it is, and so may the bystander a period waited
    // behind: with the click it stops being one. That holds until the period ends, also when a
    // later click is made with another application in front (clickedAwayFrom). A click that
    // finds no period running, or one that only waited behind the bystander, starts the ten
    // seconds; only a click that finds none running allows another Dock reset, so clicks within
    // a period never repeat it.
    void clickedWhileInactive()
    {
        if (!activationRuns() || nsAppActive())
            return;
        std::fprintf(stderr, "[host] click into the inactive game window - requesting activation\n");
        const double now = focusNow();
        const FrontApp front = frontApplication();
        if (!activationPending)
        {
            activationLauncherPid = activationBystanderPid = 0;
            activationClickPids.clear();
            activationHeirOpen = false;
            activationRecounted = false;
            dockResetAt = dockAskedAt = 0.0;
        }
        if (!activationPending || activationFreeSince == 0.0)
        {
            activationPending = true;
            activationStarted = activationCountFrom = now;
            activationFreeSince = now;
            // The ten seconds count from this click, and no later look counts them again from
            // window creation: clicks arrive in a pump, with the engine's initialisation over.
            // The one exception is a click that the look inside createWindow itself delivers.
            activationLookCounted = !creationLook;
        }
        // The Dock that this period's reset put in front is not what the user clicked away from.
        const bool ownDock = front.isDock && dockResetAt > 0.0;
        if (front.pid != 0 && front.pid != getpid() && !ownDock)
            clickedAwayFrom(front.pid);
        else if (activationBystanderPid)
            clickedAwayFrom(activationBystanderPid);
        activationBystanderPid = 0;
        repeatRequest(now);
    }

    // Adds an application to those the user clicked away from in this period; none is removed
    // before the period ends.
    void clickedAwayFrom(pid_t pid)
    {
        if (!clickedAway(pid))
            activationClickPids.push_back(pid);
    }
    bool clickedAway(pid_t pid) const
    {
        return std::find(activationClickPids.begin(), activationClickPids.end(), pid) != activationClickPids.end();
    }

    // [CHOSEN, not recovered] Marks the time createWindow runs (creationLook). createWindow holds
    // one of these for as long as it runs, so the pump it runs is the look inside createWindow
    // wherever in the function that pump is made, and however often. The headless test never
    // calls createWindow, which needs a window; it holds one of these around the look it makes
    // in createWindow's place (CocoaHostTestHooks::lookInsideCreateWindowForTest). The two lines
    // that set and clear the mark are therefore the ones its checks run. What no headless check
    // covers is that createWindow holds one: the test that opens a real window checks that
    // (Code/Tests/macos_host).
    struct InsideCreateWindow
    {
        explicit InsideCreateWindow(Impl& host) : impl(host) { impl.creationLook = true; }
        ~InsideCreateWindow() { impl.creationLook = false; }
        InsideCreateWindow(const InsideCreateWindow&) = delete;
        InsideCreateWindow& operator=(const InsideCreateWindow&) = delete;
        Impl& impl;
    };

    // [CHOSEN, not recovered] The first look at a period after the one inside createWindow: its
    // ten seconds count from here, whoever is in front (activationCountFrom). Once per period.
    void countFromFirstLook(double now)
    {
        if (activationLookCounted || creationLook)
            return;
        activationLookCounted = true;
        activationCountFrom = now;
        std::fprintf(stderr, "[host] first look %d ms after the first request; the ten seconds count from here\n",
                     static_cast<int>((now - activationStarted) * 1000.0));
    }

    // Called on every pump and on applicationDidBecomeActive: reports the outcome once, and for
    // ten seconds repeats the request every half second while this process may take the front.
    // The host is not pumped while the engine initialises and so first looks at a start seconds
    // after its first request, however long the initialisation takes. The ten seconds therefore
    // count from that look, the first after the one inside createWindow (countFromFirstLook),
    // and a period that waited behind a bystander counts them once more from the first look that
    // finds the front free (activationCountFrom). A period that is first looked at later than
    // ten seconds after its request thus still gets the system-state fallback, the Dock reset and
    // the repeated requests, and ends ten seconds after that look. No period ends between its
    // Dock reset and the request that reset owes (dockAskedAt).
    void updateActivation()
    {
        if (!activationPending)
            return;
        if (!activationRuns())
        {
            activationPending = false;           // a test ended its simulation: nothing real is asked
            return;
        }
        const double now = focusNow();
        if (nsAppActive())
        {
            activationPending = false;
            std::fprintf(stderr, "[host] application active after %d ms\n",
                         static_cast<int>((now - activationStarted) * 1000.0));
            return;
        }
        // This process may take the front from nobody, from itself, from its parent, from the
        // launcher and from the applications the user clicked away from. A child started by a
        // launcher can already be reported frontmost (its window ordered front) while NSApp is
        // still inactive; that state needs the retry most, and the window must be made key again
        // with it.
        const FrontApp front = frontApplication();
        const pid_t self = getpid();
        // The Dock is in front because this period's reset put it there.
        const bool dockInFront = dockResetAt > 0.0 && front.isDock;
        // [CHOSEN, not recovered] A restarted game whose first instance, or nobody, was in front
        // at the first request: once that instance has exited (launchd is this process's parent
        // then), the application the system names in front took its place and counts as the
        // launcher. Decided once per period. This process in front then means that no other
        // application followed the first instance.
        if (activationHeirOpen && parentPid() <= 1 && front.pid != 0 && !dockInFront &&
            front.pid != activationLauncherPid)
        {
            activationHeirOpen = false;
            if (front.pid != self)
            {
                activationLauncherPid = front.pid;
                std::fprintf(stderr, "[host] %s (pid %d) is in front in place of the instance that restarted the game; "
                             "it counts as the launcher\n",
                             front.name.UTF8String ?: "?", static_cast<int>(front.pid));
            }
        }
        const bool mayTakeFront = front.pid == 0 || front.pid == self || front.pid == parentPid() ||
                                  (activationLauncherPid && front.pid == activationLauncherPid) ||
                                  clickedAway(front.pid);
        if (!mayTakeFront && !dockInFront)
        {
            if (!activationBystanderPid || front.pid != activationBystanderPid)
            {
                activationPending = false;
                std::fprintf(stderr, "[host] another application (%s) was activated; activation left to the user\n",
                             front.name.UTF8String ?: "?");
                return;
            }
            countFromFirstLook(now);
            // The bystander of the first request is in front: it had that one request, and
            // nothing is aimed at it. The period only waits for this process, its parent or
            // nobody to be in front, or for the user's click. The half-second cadence runs on, so
            // the next request follows within half a second of the front becoming free.
            activationFreeSince = 0.0;
            if (now - activationCountFrom > 10.0)
            {
                activationPending = false;
                std::fprintf(stderr, "[host] application not activated; click the game window or switch to it\n");
                return;
            }
            if (now - activationLastTry >= 0.5)
                activationLastTry = now;
            return;
        }
        countFromFirstLook(now);
        if (mayTakeFront && activationFreeSince == 0.0)
        {
            // The front is free after a wait behind the bystander: this process, its parent or
            // nobody is in front. The first time in a period, the ten seconds count from here
            // once more. Without that a front that comes free late in the wait would leave a
            // second or two for the requests, the system-state fallback and the Dock reset, or
            // none at all.
            activationFreeSince = now;
            if (!activationRecounted)
            {
                activationRecounted = true;
                activationCountFrom = now;
                std::fprintf(stderr, "[host] %s in front %d ms after the first request; the request is repeated "
                             "for up to ten seconds from here\n",
                             front.pid == self ? "this process is" : front.pid == 0 ? "nobody is"
                                                                                    : "the parent of this process is",
                             static_cast<int>((now - activationStarted) * 1000.0));
            }
        }
        const bool freeForASecond = activationFreeSince != 0.0 && now - activationFreeSince > 1.0;
        if (!systemActive && freeForASecond && front.pid == self && systemFrontmost())
        {
            enterSystemFallback();               // its becameActive ran this function for this pump
            return;
        }
        // A Dock reset is finished half a second after the request that follows it. Until then
        // the ten seconds do not end the period: the reset put the Dock in front, and a period
        // that ended there would leave it in front and the game without focus, by the host's own
        // doing. The cadence below makes that request, past the ten seconds if it has to, and
        // the half second lets its answer arrive while the period still owns the focus state.
        const bool dockResetUnfinished = dockResetAt > 0.0 && (dockAskedAt == 0.0 || now - dockAskedAt < 0.5);
        if (now - activationCountFrom > 10.0 && !dockResetUnfinished)
        {
            activationPending = false;
            std::fprintf(stderr, "[host] application not activated; click the game window or switch to it\n");
            return;
        }
        if (dockResetAt == 0.0 && freeForASecond && mayTakeFront)
        {
            if (activateDock())
            {
                dockResetAt = now;
                std::fprintf(stderr, "[host] activation still pending; resetting it through the Dock\n");
                activationLastTry = now;     // ask again once the Dock has taken the front
                return;
            }
            dockResetAt = -1.0;              // no Dock process: retries only
        }
        if (now - activationLastTry < (dockResetAt > 0.0 && now - dockResetAt < 0.5 ? 0.3 : 0.5))
            return;
        repeatRequest(now);
    }

    // The application became active: the game window takes key status if it lost it (a window
    // ordered front while the application was inactive), so input reaches the engine at once.
    void becameActive()
    {
        startFullscreenTransition(fullscreen.activated());   // a full-screen request that waited
        if (window && visible && !minimized && !window.isKeyWindow)
        {
            [window makeKeyAndOrderFront:nil];
            [window makeFirstResponder:view];
        }
        updateActivation();
    }

    // Enters the system-state fallback (systemActive): the system's answer counts from here on.
    void enterSystemFallback()
    {
        systemActive = true;
        std::fprintf(stderr, "[host] system reports the application active; NSApp does not - using the system's state\n");
        becameActive();
        syncFocus();
    }

    // Brings windowKey and appActive in line with AppKit and the system-state fallback; the
    // listeners hear of a change once (setFocusState). The delegate callbacks end here, and so
    // does every pump (reconcileFocus). A pump leaves the fallback only for an application the
    // system names: a moment without any frontmost application is not the user leaving.
    void syncFocus(bool fromPump = false)
    {
        updateSpaceHidden();
        if (systemActive)
        {
            // Leave the fallback once AppKit has caught up, or once another application is in front.
            const pid_t front = systemFrontPid();
            if (nsAppActive() || (front != getpid() && (front != 0 || !fromPump)))
                systemActive = false;
        }
        const bool active = nsAppActive() || systemActive;
        const bool key = (window != nil || testHeadless) && !spaceHidden &&
                         (windowIsKey() || (systemActive && windowOnScreen()));
        setFocusState(key, active);
    }

    // windowDidBecomeKey / windowDidResignKey.
    void windowKeyChanged(bool key)
    {
        keyLostWhileActive = !key && nsAppActive();
        syncFocus();
    }

    // applicationDidBecomeActive / applicationDidResignActive.
    void applicationActiveChanged(bool active)
    {
        keyLostWhileActive = false;              // the key status went or returns with the application
        if (active)
            becameActive();
        syncFocus();
    }

    // Makes the game window key and its view the first responder.
    void makeWindowKey()
    {
        if (TestSystem* t = testSystemState())
        {
            ++t->makeKeyCalls;
            if (t->makeKeyWorks && !t->windowKey)
            {
                t->windowKey = true;
                windowKeyChanged(true);          // as AppKit's windowDidBecomeKey
            }
            return;
        }
        [window makeKeyAndOrderFront:nil];
        [window makeFirstResponder:view];
    }

    // Whether the focus state follows AppKit. Forced focus stays as it was set: scripted runs
    // ([TEMP] scriptedFocus) and tests that use overrideFocusForTest without a simulated system.
    bool focusFollowsSystem() const
    {
        if (scriptedFocus)
            return false;
        return testSystemState() || (!testHeadless && !testFocusOverride);
    }

    // [CHOSEN, not recovered] Every pump: the application is active and the game window is on
    // screen, yet the window is not key and nothing else of this process is due the key status.
    // Make it key and first responder, as becameActive does at activation; again every half
    // second while that state lasts. Not after the window gave the key status away while the
    // application stayed active (keyLostWhileActive).
    void repairKeyWindow(double now)
    {
        if (!nsAppActive() || windowIsKey() || !windowOnScreen())
        {
            keyRepairing = false;
            return;
        }
        if (keyLostWhileActive || keyStatusTaken(true))
            return;
        if (keyRepairing && now - keyRepairAt < 0.5)
            return;
        if (!keyRepairing)
            std::fprintf(stderr, "[host] game window not key in the active application - making it key\n");
        keyRepairing = true;
        keyRepairAt = now;
        makeWindowKey();
    }

    // [CHOSEN, not recovered] Called on every pump: re-reads AppKit and passes through the
    // transitions the delegate callbacks would have made, so a cache they left stale lasts one
    // frame instead of until the next application switch. Unchanged state notifies nobody. While
    // an activation period makes its requests updateActivation owns the state (its Dock reset
    // puts the Dock in front for a moment); a period that only waits behind a bystander does not.
    void reconcileFocus()
    {
        if (!focusFollowsSystem())
            return;
        const double now = focusNow();
        const bool active = nsAppActive();
        if (!active && systemFrontmost())
        {
            if (systemFrontSince == 0.0)
                systemFrontSince = now;
        }
        else
            systemFrontSince = 0.0;
        if (activationPending && activationFreeSince != 0.0)
            return;
        if (active && !appActive)
            becameActive();                      // an activation the delegate never reported
        repairKeyWindow(now);
        syncFocus(true);
    }

    // [CHOSEN, not recovered] A press inside the game window found the focus gate closed. When
    // this application is active for AppKit, or the system has called it frontmost for a quarter
    // of a second while AppKit never activated it, the press is meant for the game: bring the key
    // status and the cache in line first, so the listeners get the focus change, the pointer
    // position (reportPointer) and then the press. A press that brings the application forward
    // from another one stays window management and is not delivered.
    void repairFocusForPress()
    {
        if (!focusFollowsSystem())
            return;
        if (nsAppActive())
        {
            if (!appActive)
                becameActive();
            if (!windowIsKey() && windowOnScreen() && !keyStatusTaken(false))
                makeWindowKey();
        }
        else if (!systemActive && systemFrontSince != 0.0 && focusNow() - systemFrontSince >= 0.25 &&
                 systemFrontmost())
            enterSystemFallback();
        syncFocus();
    }

    // [CHOSEN, not recovered] The focus gate dropped an input event (a key press, a press or move
    // inside the game window, the wheel). One line a second at most says so, with the cache and
    // what AppKit and the system report, so a log shows whether "the game ignores my clicks" was
    // the gate and which side was wrong.
    void inputDropped(const char* what)
    {
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
                     "systemActive=%d frontmost pid=%d (%s), own pid=%d\n",
                     what, dropsSinceReport, appActive ? 1 : 0, windowKey ? 1 : 0, nsAppActive() ? 1 : 0,
                     windowIsKey() ? 1 : 0,
                     testSystemState() ? (windowOnScreen() ? 1 : 0) : (window != nil && window.isVisible ? 1 : 0),
                     systemActive ? 1 : 0, static_cast<int>(systemFrontPid()),
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
    // per pixel (the size a 96-dpi Windows desktop shows). A larger one is shown at the screen's
    // backing scale (one physical pixel per back-buffer pixel on Retina), and if it still does
    // not fit it is scaled down to fit, keeping its aspect. The drawable stays width x height.
    static NSSize windowedContentSize(int width, int height, NSScreen* screen, NSWindowStyleMask style)
    {
        NSSize size = NSMakeSize(width, height);
        if (!screen)
            return size;
        const NSRect visible = screen.visibleFrame;
        const NSRect probe = [NSWindow frameRectForContentRect:NSMakeRect(0, 0, 100, 100) styleMask:style];
        const CGFloat maxW = NSWidth(visible) - (NSWidth(probe) - 100);
        const CGFloat maxH = NSHeight(visible) - (NSHeight(probe) - 100);
        if (size.width <= maxW && size.height <= maxH)
            return size;
        const CGFloat scale = screen.backingScaleFactor > 1.0 ? screen.backingScaleFactor : 1.0;
        size = NSMakeSize(size.width / scale, size.height / scale);
        if (size.width > maxW || size.height > maxH)
        {
            const CGFloat k = std::min(maxW / size.width, maxH / size.height);
            size = NSMakeSize(std::floor(size.width * k), std::floor(size.height * k));
        }
        return size;
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

    // [CHOSEN, not recovered] Native full screen (NativeFullscreen.h). Starts the transition the
    // state answered. A windowed game window is NSWindowCollectionBehaviorFullScreenNone, so the
    // green button and the system's full-screen shortcut do not take it into a Space behind the
    // engine's back; entering makes it a full-screen primary window and resizable, as AppKit
    // expects of a window it fills the screen with, and both are undone once it is windowed again
    // (fullscreenExited). The toggle is made on the next pass of the run loop - the next pump -
    // rather than inside the delegate callback or engine call that asked for it.
    void startFullscreenTransition(macos::NativeFullscreen::Action action)
    {
        if (action == macos::NativeFullscreen::None || !window)
            return;
        if (action == macos::NativeFullscreen::Enter)
        {
            windowedFrame = window.frame;
            window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
            window.styleMask |= NSWindowStyleMaskResizable;
            std::fprintf(stderr, "[host] full screen: entering the game window's own Space\n");
        }
        else
            std::fprintf(stderr, "[host] full screen: leaving the game window's Space\n");
        [window performSelector:@selector(toggleFullScreen:) withObject:nil afterDelay:0.0];
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
        windowedFrame = window.frame;
    }

    // windowDidEnterFullScreen / windowDidExitFullScreen / windowDidFailTo{Enter,Exit}FullScreen.
    void fullscreenEntered()
    {
        const macos::NativeFullscreen::Action next = fullscreen.didEnter();
        std::fprintf(stderr, "[host] full screen: in the game window's own Space\n");
        occlusionChanged();
        afterFullscreenTransition();
        startFullscreenTransition(next);
    }
    // windowDidChangeOcclusionState: the full-screen Space went off or came back on screen.
    void occlusionChanged() { syncFocus(); }
    // Re-read on every focus sync, so every pump (reconcileFocus) repairs a notification that
    // never came. Not during a transition, whose animation can hide the window for a moment.
    void updateSpaceHidden()
    {
        const bool hidden = window && !testHeadless && fullscreen.inSpace() && !fullscreen.transitioning() &&
                            !(window.occlusionState & NSWindowOcclusionStateVisible);
        if (hidden == spaceHidden)
            return;
        spaceHidden = hidden;
        std::fprintf(stderr, "[host] full screen: the game's Space is %s\n", hidden ? "off screen" : "on screen again");
        applyCursor();                           // the cursor rule reads it too
    }
    void fullscreenExited()
    {
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
    void fullscreenFailed(bool entering)
    {
        fullscreen.didFail();
        if (fullscreen.windowed())
            restoreWindowed();
        std::fprintf(stderr, "[host] full screen: AppKit refused to %s the Space; not retried before the next switch "
                     "or activation\n", entering ? "enter" : "leave");
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
        bool focus = hasFocus();
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
        {
            if (!nsAppActive() && event.window == window)
                clickedWhileInactive();          // also in the system-active fallback (focus held)
            const bool inside = !focus && insideGameView(event);
            if (inside)
            {
                repairFocusForPress();
                focus = hasFocus();
            }
            if (focus) handleMouseButton(event, true);
            else if (inside) inputDropped("mouse down");
            break;
        }
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

    // One look: the focus part of one run of the pump, the launch activation first and then the
    // reconciliation. The pump makes it once per run, after it has routed the queued events, and
    // the headless test makes it in the pump's place (CocoaHostTestHooks::pumpFocusForTest), so
    // both run these lines. The two totals are for the tests.
    void look()
    {
        ++looks;
        if (creationLook)
            ++looksInsideCreateWindow;
        updateActivation();
        reconcileFocus();
    }

    bool pump(double timeout)
    {
        if (!appInitialized)
            return quit;
        flushCapsRelease();
        @autoreleasepool
        {
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
                route(event);
                [NSApp sendEvent:event];
                until = [NSDate distantPast];
            }
            [NSApp updateWindows];
            look();
            updateCursorImage();
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
- (void)windowDidBecomeKey:(NSNotification*)note     { (void)note; if (impl) impl->windowKeyChanged(true); }
- (void)windowDidResignKey:(NSNotification*)note     { (void)note; if (impl) impl->windowKeyChanged(false); }
- (void)windowDidMiniaturize:(NSNotification*)note   { (void)note; if (impl) { impl->minimized = true;  impl->notifyResize(); } }
- (void)windowDidDeminiaturize:(NSNotification*)note
{
    (void)note;
    if (!impl)
        return;
    impl->minimized = false;
    impl->keyLostWhileActive = false;            // the key status went with the miniaturized window
    impl->notifyResize();
}
// [CHOSEN, not recovered] Native full screen (NativeFullscreen.h).
- (void)windowWillEnterFullScreen:(NSNotification*)note { (void)note; if (impl) impl->fullscreen.willEnter(); }
- (void)windowDidEnterFullScreen:(NSNotification*)note  { (void)note; if (impl) impl->fullscreenEntered(); }
- (void)windowWillExitFullScreen:(NSNotification*)note  { (void)note; if (impl) impl->fullscreen.willExit(); }
- (void)windowDidExitFullScreen:(NSNotification*)note   { (void)note; if (impl) impl->fullscreenExited(); }
- (void)windowDidFailToEnterFullScreen:(NSWindow*)window { (void)window; if (impl) impl->fullscreenFailed(true); }
- (void)windowDidFailToExitFullScreen:(NSWindow*)window  { (void)window; if (impl) impl->fullscreenFailed(false); }
- (void)windowDidResize:(NSNotification*)note           { (void)note; if (impl) impl->updateMagnification(); }
- (void)windowDidChangeOcclusionState:(NSNotification*)note { (void)note; if (impl) impl->occlusionChanged(); }
- (void)applicationDidBecomeActive:(NSNotification*)note { (void)note; if (impl) impl->applicationActiveChanged(true); }
- (void)applicationDidResignActive:(NSNotification*)note { (void)note; if (impl) impl->applicationActiveChanged(false); }
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
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
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
        m_impl->delegate = [[OpenBF2HostDelegate alloc] init];
        m_impl->delegate->impl = m_impl;
        [NSApp setDelegate:m_impl->delegate];

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

        [NSApp finishLaunching];
        m_impl->scripted = inputscript::inputScript().loadFromEnvironment();   // [TEMP] BF2_INPUT_SCRIPT
        {
            const char* realFocus = std::getenv("BF2_INPUT_SCRIPT_REAL_FOCUS");
            m_impl->scriptedFocus = m_impl->scripted && !(realFocus && realFocus[0] == '1');
        }
        macos::rawMouseStart();                         // GCMouse connections, observed for the run
    }
    m_impl->appInitialized = true;
    m_impl->appActive = (m_impl->scriptedFocus || [NSApp isActive]) ? true : false;
    return true;
}

bool ClientHost::createWindow(const HostWindowDesc& desc)
{
    if (!initApplication() || m_impl->window)
        return false;
    // [CHOSEN, not recovered] From here to the return the host is inside createWindow. The pump
    // at the end of this function is therefore the look inside createWindow: it follows the
    // first activation request at once and is the last one before the engine has initialised,
    // so the activation period's ten seconds count from the look after it, not from this one
    // (Impl::countFromFirstLook). Keep this ahead of everything that can pump.
    const Impl::InsideCreateWindow insideCreateWindow(*m_impl);
    @autoreleasepool
    {
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
        // Win32 positions are the frame's top left from the screen's top left.
        [window setFrameTopLeftPoint:NSMakePoint(NSMinX(screenFrame) + desc.x,
                                                 NSMaxY(screenFrame) - desc.y)];

        OpenBF2HostView* view = [[OpenBF2HostView alloc] initWithFrame:content];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        window.contentView = view;

        m_impl->window = window;
        m_impl->view = view;
        m_impl->layer = static_cast<CAMetalLayer*>(view.layer);
        m_impl->windowedFrame = window.frame;
        m_impl->backWidth = width;
        m_impl->backHeight = height;
        m_impl->fullscreen = macos::NativeFullscreen();
        m_impl->quit = false;
        m_impl->cursorHiddenInView = desc.hideCursor;
        m_impl->updateDrawable();

        m_impl->visible = true;
        m_impl->beginActivation();               // [TEMP] not in scripted runs (scriptedFocus)
        [window makeKeyAndOrderFront:nil];
        [window makeFirstResponder:view];

        // [CHOSEN, not recovered] A full-screen start shows the window, then asks for its own
        // Space (NativeFullscreen.h). An inactive application's request waits for the activation
        // (becameActive), which the launch activation above asks for.
        if (desc.fullscreen)
            setFullscreen(true);
    }
    pumpEvents();                                // the look inside createWindow (insideCreateWindow
                                                 // above): deliver the activation before returning
    m_impl->syncFocus();
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
        [NSObject cancelPreviousPerformRequestsWithTarget:m_impl->window];
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
    m_impl->windowKey = false;
    m_impl->keyLostWhileActive = false;
    m_impl->keyRepairing = false;
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
        m_impl->windowedFrame = m_impl->window.frame;
    }
    m_impl->updateDrawable();
    m_impl->notifyResize();
}

// [CHOSEN, not recovered] Native full screen (NativeFullscreen.h): the request is what
// isFullscreen() reports from now on; the window follows asynchronously, at once when the
// application is active and otherwise with its activation. The drawable keeps the back-buffer size
// in both, and the engine hears of the switch as before (WM_SIZE with the unchanged size).
void ClientHost::setFullscreen(bool fullscreen)
{
    if (!m_impl->window || fullscreen == m_impl->fullscreen.wanted())
        return;
    OpenBF2HostWindow* window = m_impl->window;
    const bool active = m_impl->scriptedFocus || [NSApp isActive];
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
bool ClientHost::quitRequested() const            { return m_impl->quit; }
void ClientHost::postQuitMessage()                { m_impl->quit = true; }

void ClientHost::requestQuit()
{
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
        m_impl->cursorShape = cursor == cursorHandle(HostCursorBusy) ? HostCursorBusy : HostCursorArrow;
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

std::vector<HostDisplayModeInfo> ClientHost::displayModes() const
{
    std::vector<HostDisplayModeInfo> modes;
    const void* keys[] = { kCGDisplayShowDuplicateLowResolutionModes };
    const void* values[] = { kCFBooleanTrue };
    CFDictionaryRef options = CFDictionaryCreate(0, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef list = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), options);
    CFRelease(options);
    if (!list)
        return modes;
    for (CFIndex i = 0; i < CFArrayGetCount(list); ++i)
    {
        CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(list, i);
        if (!CGDisplayModeIsUsableForDesktopGUI(mode))
            continue;
        HostDisplayModeInfo info;
        info.width = static_cast<int>(CGDisplayModeGetPixelWidth(mode));
        info.height = static_cast<int>(CGDisplayModeGetPixelHeight(mode));
        info.refreshHz = static_cast<int>(std::lround(CGDisplayModeGetRefreshRate(mode)));
        if (info.refreshHz == 0)
            info.refreshHz = builtInPanelRefreshHz();
        info.bitsPerPixel = 32;
        bool duplicate = false;
        for (const HostDisplayModeInfo& m : modes)
            duplicate = duplicate || (m.width == info.width && m.height == info.height &&
                                      m.refreshHz == info.refreshHz);
        if (!duplicate)
            modes.push_back(info);
    }
    CFRelease(list);
    std::sort(modes.begin(), modes.end(), [](const HostDisplayModeInfo& a, const HostDisplayModeInfo& b) {
        if (a.width != b.width) return a.width < b.width;
        if (a.height != b.height) return a.height < b.height;
        return a.refreshHz < b.refreshHz;
    });
    return modes;
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

// RestartInFront.h. The host's own focus state as the engine sees it, minus forced focus.
bool clientHoldsFront()
{
    const host::ClientHost::Impl* impl = s_hostImpl;
    return impl && impl->appActive && !impl->scriptedFocus;
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
    system.windowOnScreen = state->windowOnScreen;
    system.modalWindow    = state->modalWindow;
    system.otherKeyWindow = state->otherKeyWindow;
    system.makeKeyWorks   = state->makeKeyWorks;
    system.frontPid       = static_cast<pid_t>(state->frontPid);
    system.now            = state->now;
    system.activation     = state->activation;
    system.parentPid      = static_cast<pid_t>(state->parentPid);
    const size_t ancestors = sizeof system.ancestors / sizeof system.ancestors[0];
    for (size_t i = 0; i < ancestors; ++i)
        system.ancestors[i] = static_cast<pid_t>(state->ancestors[i]);
    system.ancestryDenied = state->ancestryDenied;
    system.realProcessTable = state->realProcessTable;
    system.dockPid        = static_cast<pid_t>(state->dockPid);
}

void CocoaHostTestHooks::focusCallbackForTest(host::ClientHost& host, FocusCallback callback)
{
    switch (callback)
    {
    case WindowBecameKey:   host.m_impl->windowKeyChanged(true); break;
    case WindowResignedKey: host.m_impl->windowKeyChanged(false); break;
    case AppBecameActive:   host.m_impl->applicationActiveChanged(true); break;
    case AppResignedActive: host.m_impl->applicationActiveChanged(false); break;
    }
}

void CocoaHostTestHooks::pumpFocusForTest(host::ClientHost& host)
{
    host.m_impl->look();                        // the lines the pump runs
}

void CocoaHostTestHooks::lookInsideCreateWindowForTest(host::ClientHost& host, const std::function<void()>& events)
{
    // What createWindow holds while it runs; nothing here writes the mark itself.
    const host::ClientHost::Impl::InsideCreateWindow insideCreateWindow(*host.m_impl);
    if (events)
        events();                               // pump's order: the queued events are routed first
    host.m_impl->look();
}

CocoaHostTestHooks::FocusRepairStats CocoaHostTestHooks::focusRepairStatsForTest(const host::ClientHost& host)
{
    FocusRepairStats stats;
    stats.windowKey     = host.m_impl->testSystem.windowKey;
    stats.systemActive  = host.m_impl->systemActive;
    stats.makeKeyCalls  = host.m_impl->testSystem.makeKeyCalls;
    stats.droppedInputs = host.m_impl->droppedInputs;
    stats.dropReports   = host.m_impl->dropReports;
    return stats;
}

void CocoaHostTestHooks::beginActivationForTest(host::ClientHost& host)
{
    host.m_impl->beginActivation();
}

void CocoaHostTestHooks::forgetRestartMarkerForTest(host::ClientHost& host)
{
    host.m_impl->restartMarkerRead = false;
}

CocoaHostTestHooks::ActivationStats CocoaHostTestHooks::activationStatsForTest(const host::ClientHost& host)
{
    ActivationStats stats;
    stats.pending    = host.m_impl->activationPending;
    stats.requests   = host.m_impl->testSystem.activationRequests;
    stats.ignoringOthers = host.m_impl->testSystem.activationIgnoringOthers;
    stats.retries    = host.m_impl->testSystem.activationRetries;
    stats.dockResets = host.m_impl->testSystem.dockResets;
    stats.lookCounted = host.m_impl->activationLookCounted;
    stats.looks      = host.m_impl->looks;
    stats.looksInsideCreateWindow = host.m_impl->looksInsideCreateWindow;
    return stats;
}

void CocoaHostTestHooks::enableHeadlessForTest(host::ClientHost& host)
{
    host.m_impl->testHeadless = true;
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
