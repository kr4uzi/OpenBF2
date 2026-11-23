// [TEMP] macOS input review harness. Runs the Cocoa host headless
// (CocoaHostTestHooks::enableHeadlessForTest: no window, no NSApplication, the real cursor untouched),
// routes NSEvents built from CoreGraphics events through it exactly as pumpEvents routes them,
// drives NativeInputDeviceManager with InputManager::update's protocol (update(-1) polls, then one
// positive pass per input tick, 10.0 on the last) and checks what the engine-facing device
// interfaces report against the DirectInput semantics of the recovered DX8 devices
// (KeyboardDevice.cpp, MouseDevice.cpp). The host's focus repair and its launch activation run
// against a simulated AppKit state (CocoaHostTestHooks::setSystemFocusForTest): the activation's
// requests and Dock resets are counted, none is made. The restart marker is set and removed in
// this process's own environment. [RETIRE-WHEN: macos-client-window]

#import <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

#include "../../BF2/IO/Input/NativeInputDeviceManager.h"
#include "../../BF2/Platform/Host/ClientHost.h"
#include "../../BF2/Platform/macOS/CocoaHostTestHooks.h"
#include "../../BF2/Platform/macOS/CursorScale.h"
#include "../../BF2/Platform/macOS/MacKeyMap.h"
#include "../../BF2/Platform/macOS/MacRawMouse.h"
#include "../../BF2/Platform/macOS/NativeFullscreen.h"
#include "../../BF2/Platform/macOS/ProcessAncestry.h"
#include "../../BF2/Platform/macOS/RestartInFront.h"
#include "../native_host/WindowMessageRecorder.h"
#include "../../BF2/Core/Ids.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <atomic>
#include <string>
#include <chrono>
#include <thread>
#include <vector>

namespace dice { namespace hfe { namespace io { void installControlMapFixture(); extern IBase* inputDeviceManager; } } }

using namespace dice::hfe;
using host::ClientHost;
using macos::CocoaHostTestHooks;

namespace {

int g_failures = 0;
int g_checks = 0;
std::string g_test;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("  FAIL [%s] %s\n", g_test.c_str(), what);
    }
}

void begin(const char* name) { g_test = name; std::printf("[test] %s\n", name); }

// --- event construction ----------------------------------------------------------------------

// Event times on the host clock (system uptime), as hardware events carry them.
CGEventTimestamp nowNs(int offsetMs = 0)
{
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + static_cast<long long>(offsetMs) * 1000000LL;
}

const SInt16 kKeyboardANSI = 40;    // KBGetLayoutType(40) == 'ANSI' (Apple Extended/MacBook ANSI)
const SInt16 kKeyboardISO  = 41;    // KBGetLayoutType(41) == 'ISO '

void inject(CGEventRef cg)
{
    NSEvent* e = [NSEvent eventWithCGEvent:cg];
    CFRelease(cg);
    CocoaHostTestHooks::injectEventForTest(ClientHost::instance(), (__bridge void*)e);
}

void key(unsigned short keyCode, bool down, bool repeat = false, SInt16 kbType = kKeyboardANSI,
         CGEventFlags flags = 0, int offsetMs = 0)
{
    CGEventRef cg = CGEventCreateKeyboardEvent(0, keyCode, down);
    CGEventSetFlags(cg, flags);
    CGEventSetIntegerValueField(cg, kCGKeyboardEventKeyboardType, kbType);
    if (repeat) CGEventSetIntegerValueField(cg, kCGKeyboardEventAutorepeat, 1);
    CGEventSetTimestamp(cg, nowNs(offsetMs));
    inject(cg);
}

// flagsChanged: the modifier key's code with the flag set after the change.
void flags(unsigned short keyCode, CGEventFlags after)
{
    CGEventRef cg = CGEventCreateKeyboardEvent(0, keyCode, true);
    CGEventSetType(cg, kCGEventFlagsChanged);
    CGEventSetFlags(cg, after);
    CGEventSetIntegerValueField(cg, kCGKeyboardEventKeyboardType, kKeyboardANSI);
    CGEventSetTimestamp(cg, nowNs());
    inject(cg);
}

void move(int dx, int dy)
{
    CGEventRef cg = CGEventCreateMouseEvent(0, kCGEventMouseMoved, CGPointMake(100, 100), kCGMouseButtonLeft);
    CGEventSetIntegerValueField(cg, kCGMouseEventDeltaX, dx);
    CGEventSetIntegerValueField(cg, kCGMouseEventDeltaY, dy);
    CGEventSetTimestamp(cg, nowNs());
    inject(cg);
}

void button(int number, bool down, int offsetMs = 0)
{
    CGEventType type;
    if (number == 0)      type = down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
    else if (number == 1) type = down ? kCGEventRightMouseDown : kCGEventRightMouseUp;
    else                  type = down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
    CGEventRef cg = CGEventCreateMouseEvent(0, type, CGPointMake(100, 100), static_cast<CGMouseButton>(number));
    CGEventSetIntegerValueField(cg, kCGMouseEventButtonNumber, number);
    CGEventSetTimestamp(cg, nowNs(offsetMs));
    inject(cg);
}

// A wheel event; returns the sign AppKit gives the device direction (natural scrolling undone).
void wheelLines(int lines)
{
    CGEventRef cg = CGEventCreateScrollWheelEvent2(0, kCGScrollEventUnitLine, 1, lines, 0, 0);
    CGEventSetTimestamp(cg, nowNs());
    inject(cg);
}

void wheelPixels(int pixels, CGMomentumScrollPhase momentum = kCGMomentumScrollPhaseNone)
{
    CGEventRef cg = CGEventCreateScrollWheelEvent2(0, kCGScrollEventUnitPixel, 1, pixels, 0, 0);
    if (momentum != kCGMomentumScrollPhaseNone)
        CGEventSetIntegerValueField(cg, kCGScrollWheelEventMomentumPhase, momentum);
    CGEventSetTimestamp(cg, nowNs());
    inject(cg);
}

// --- engine side -----------------------------------------------------------------------------

io::IInputDeviceManager* g_manager = 0;

io::IKeyboardDevice* keyboard()
{ return static_cast<io::IKeyboardDevice*>(g_manager->getInputDevice(io::IDFKeyboard)); }
io::IAdvancedInputDevice* mouse()
{ return static_cast<io::IAdvancedInputDevice*>(g_manager->getInputDevice(io::IDFMouse)); }

// One frame: the raw-mouse drain at the end of pumpEvents, then one InputManager::update with
// one input tick: the poll, then the final pass (10.0).
void frame()
{
    CocoaHostTestHooks::pumpRawMouseForTest(ClientHost::instance());
    g_manager->update(-1.0f);
    g_manager->update(10.0f);
}

// A GCMouse report as MacRawMouse's mouseMovedHandler publishes it (y already down positive).
void raw(float dx, float dy, int offsetMs = 0)
{
    macos::rawMousePublish(dx, dy, ClientHost::tickCountMs() + offsetMs);
}

// The focus part of BF2::handleInGameWinMessages: setHasFocus(window focus, mask), the mouse bit
// cleared while a menu logic exists (always, on the client).
void engineFocus(bool focus)
{
    const unsigned int mask = static_cast<unsigned int>(-2);
    if (focus) g_manager->activateDevices(mask);
    else       g_manager->deActivateDevices(mask);
}

void setFocus(bool focus)
{
    CocoaHostTestHooks::overrideFocusForTest(ClientHost::instance(), focus, focus);
    engineFocus(focus);
}

// Clears every device: releases all keys and buttons and runs two frames.
void settle()
{
    frame(); frame();
}

// Records the mouse events every host listener sees, as BF2Window_native's WindowMessageSource does.
struct MouseLog final : host::IHostEventListener
{
    int moves = 0, buttons = 0, wheels = 0;
    int capturedMoves = 0, capturedButtons = 0, capturedWheels = 0;
    void onMouseMove(const host::HostMouseMoveEvent& e) override { ++moves; capturedMoves += e.captured; }
    void onMouseButton(const host::HostMouseButtonEvent& e) override { ++buttons; capturedButtons += e.captured; }
    void onMouseWheel(const host::HostMouseWheelEvent& e) override { ++wheels; capturedWheels += e.captured; }
    double rawX = 0.0, rawY = 0.0;
    void onRawMouseMove(const host::HostRawMouseEvent& e) override { rawX += e.dx; rawY += e.dy; }
    void reset() { *this = MouseLog(); }
};

// Records the focus notifications every host listener sees, and whether a button event arrived
// after the notification that opened the focus gate.
struct FocusLog final : host::IHostEventListener
{
    int activations = 0, deactivations = 0, focusChanges = 0, buttons = 0, buttonsWithFocus = 0;
    bool key = false, active = false;
    void onAppActivationChanged(bool appActive) override { ++(appActive ? activations : deactivations); }
    void onFocusChanged(bool windowKey, bool appActive) override { ++focusChanges; key = windowKey; active = appActive; }
    void onMouseButton(const host::HostMouseButtonEvent&) override { ++buttons; buttonsWithFocus += key && active; }
    void reset() { activations = deactivations = focusChanges = buttons = buttonsWithFocus = 0; }
};

// --- tests -----------------------------------------------------------------------------------

// Every key the shipped Controls.con files bind (mods/bf2 and mods/xpack, Settings and Default)
// is reachable from a macOS key code, and a press and release arrive as DirectInput reports them.
void testControlsConKeys()
{
    begin("Controls.con keys reach their DIK_ codes");
    struct K { const char* name; unsigned char dik; };
    static const K keys[] = {
        {"0",11},{"1",2},{"2",3},{"3",4},{"4",5},{"5",6},{"6",7},{"7",8},{"8",9},{"9",10},
        {"A",30},{"B",48},{"C",46},{"D",32},{"E",18},{"F",33},{"G",34},{"J",36},{"K",37},{"L",38},
        {"M",50},{"N",49},{"P",25},{"Q",16},{"R",19},{"S",31},{"T",20},{"V",47},{"W",17},{"X",45},
        {"Z",44},{"Add",78},{"ArrowDown",208},{"ArrowLeft",203},{"ArrowRight",205},{"ArrowUp",200},
        {"Backspace",14},{"Delete",211},{"End",207},{"Enter",28},{"Escape",1},{"F1",59},{"F2",60},
        {"F3",61},{"F4",62},{"F5",63},{"F6",64},{"F7",65},{"F8",66},{"F9",67},{"F10",68},{"F11",87},
        {"F12",88},{"Grave",41},{"Home",199},{"Insert",210},{"PageDown",209},{"PageUp",201},
        {"PrintScreen",183},{"Space",57},{"Tab",15},
    };
    for (const K& k : keys)
    {
        const unsigned short code = macos::macKeyCodeFromDik(k.dik, false);
        char what[96];
        std::snprintf(what, sizeof what, "IDKey_%s (DIK %u) has a macOS key code", k.name, k.dik);
        check(code != 0xFFFF, what);
        if (code == 0xFFFF) continue;
        key(code, true);
        frame();
        std::snprintf(what, sizeof what, "IDKey_%s pressed after its key down", k.name);
        check(keyboard()->keyPressed(static_cast<io::InputDeviceKeys>(k.dik)), what);
        key(code, false);
        frame();
        std::snprintf(what, sizeof what, "IDKey_%s released after its key up", k.name);
        check(keyboard()->keyReleased(static_cast<io::InputDeviceKeys>(k.dik)) &&
              !keyboard()->keyDown(static_cast<io::InputDeviceKeys>(k.dik)), what);
    }
    settle();

    // Modifiers come from flagsChanged with the device-dependent bits.
    struct M { const char* name; unsigned short code; CGEventFlags down; unsigned char dik; };
    static const M mods[] = {
        {"LeftShift", 0x38, kCGEventFlagMaskShift | 0x02, 42},
        {"RightShift", 0x3C, kCGEventFlagMaskShift | 0x04, 54},
        {"LeftCtrl", 0x3B, kCGEventFlagMaskControl | 0x01, 29},
        {"RightCtrl", 0x3E, kCGEventFlagMaskControl | 0x2000, 157},
        {"LeftAlt", 0x3A, kCGEventFlagMaskAlternate | 0x20, 56},
        {"RightAlt", 0x3D, kCGEventFlagMaskAlternate | 0x40, 184},
    };
    for (const M& m : mods)
    {
        char what[96];
        flags(m.code, m.down);
        frame();
        std::snprintf(what, sizeof what, "IDKey_%s pressed by flagsChanged", m.name);
        check(keyboard()->keyPressed(static_cast<io::InputDeviceKeys>(m.dik)), what);
        flags(m.code, 0);
        frame();
        std::snprintf(what, sizeof what, "IDKey_%s released by flagsChanged", m.name);
        check(keyboard()->keyReleased(static_cast<io::InputDeviceKeys>(m.dik)), what);
    }
    // Caps Lock (IDKey_Capital): a toggle; each change is a press, the release follows next pump.
    flags(0x39, kCGEventFlagMaskAlphaShift);
    frame();
    check(keyboard()->keyPressed(io::IDKey_Capital), "IDKey_Capital pressed when Caps Lock locks");
    inject(CGEventCreate(0));                   // any event: the pump flushes the pending release first
    frame();
    check(keyboard()->keyReleased(io::IDKey_Capital), "IDKey_Capital released on the next pump");
    flags(0x39, 0);
    frame();
    check(keyboard()->keyPressed(io::IDKey_Capital), "IDKey_Capital pressed again when Caps Lock unlocks");
    settle();
}

// DirectInput reports transitions only: auto-repeat key-downs are not device data.
void testKeyRepeat()
{
    begin("auto-repeat is not device data");
    key(kVK_ANSI_W, true);
    frame();
    check(keyboard()->keyPressed(io::IDKey_W), "W pressed");
    for (int i = 0; i < 5; ++i) { key(kVK_ANSI_W, true, true); frame(); }
    check(keyboard()->keyDown(io::IDKey_W) && !keyboard()->keyPressed(io::IDKey_W),
          "W held, not re-pressed, through repeats");
    key(kVK_ANSI_W, false);
    frame();
    check(keyboard()->keyReleased(io::IDKey_W), "W released");
    settle();
}

// The key left of 1 is DIK_GRAVE (the console key) on ANSI and ISO keyboards; the ISO key beside
// left Shift is DIK_OEM_102. macOS swaps the two key codes on ISO keyboards.
void testIsoSwap()
{
    begin("ISO keyboards: console key and OEM_102 by position");
    key(kVK_ANSI_Grave, true, false, kKeyboardANSI);
    frame();
    check(keyboard()->keyPressed(io::IDKey_Grave), "ANSI key left of 1 is DIK_GRAVE");
    key(kVK_ANSI_Grave, false, false, kKeyboardANSI);
    frame();
    key(kVK_ISO_Section, true, false, kKeyboardISO);
    frame();
    check(keyboard()->keyPressed(io::IDKey_Grave), "ISO key left of 1 (kVK_ISO_Section) is DIK_GRAVE");
    key(kVK_ISO_Section, false, false, kKeyboardISO);
    frame();
    key(kVK_ANSI_Grave, true, false, kKeyboardISO);
    frame();
    check(keyboard()->keyPressed(io::IDKey_OEM_102), "ISO key beside left Shift is DIK_OEM_102");
    key(kVK_ANSI_Grave, false, false, kKeyboardISO);
    settle();
}

// Events are consumed by timestamp within a frame's passes (KeyboardDevice::updateCurrentState).
void testSubFrameSampling()
{
    begin("buffered key events are split across input ticks by timestamp");
    key(kVK_ANSI_A, true, false, kKeyboardANSI, 0, -60);
    key(kVK_ANSI_A, false, false, kKeyboardANSI, 0, -10);
    g_manager->update(-1.0f);
    g_manager->update(0.03f);
    check(keyboard()->keyDown(io::IDKey_A), "first pass sees only the press");
    g_manager->update(10.0f);
    check(!keyboard()->keyDown(io::IDKey_A) && keyboard()->keyReleased(io::IDKey_A),
          "last pass sees the release");
    settle();
}

// Focus loss: the window loses focus, the engine deactivates the keyboard and every held key is
// released (KeyboardDevice::deActivate); keys held across a loss/regain inside one pump are dropped
// on the next poll (DIERR_INPUTLOST).
void testFocusLoss()
{
    begin("focus loss clears held keys and buttons");
    key(kVK_ANSI_W, true);
    button(0, true);
    frame();
    check(keyboard()->keyDown(io::IDKey_W) && mouse()->buttonDown(io::IDButton_0), "W and left button held");
    setFocus(false);
    frame();
    check(!keyboard()->keyDown(io::IDKey_W), "W released by deactivation");
    check(!mouse()->buttonDown(io::IDButton_0), "left button released (input lost)");
    key(kVK_ANSI_W, false);                     // the release happens elsewhere
    button(0, false);
    setFocus(true);
    frame();
    check(!keyboard()->keyDown(io::IDKey_W) && !keyboard()->keyPressed(io::IDKey_W), "no stale W after regain");

    key(kVK_ANSI_S, true);
    CocoaHostTestHooks::overrideFocusForTest(ClientHost::instance(), false, false);   // lost and regained within one pump
    CocoaHostTestHooks::overrideFocusForTest(ClientHost::instance(), true, true);
    frame();
    check(!keyboard()->keyDown(io::IDKey_S), "a press before a loss inside one pump is dropped");
    key(kVK_ANSI_S, false);
    settle();
}

// Mouse: raw relative counts, Y down, buttons 0..7 as DIMOFS_BUTTON0..7, sample latency on the
// event clock.
void testMouseBasics()
{
    begin("mouse deltas, buttons and latency");
    move(5, -3);
    move(2, 4);
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 7.0f, "X sums deltas (7)");
    check(mouse()->readAxis(io::IDAxis_1, true) == 1.0f, "Y sums deltas, down positive (1)");
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 0.0f, "axes are per sample");
    const io::InputDeviceButtons bits[5] = { io::IDButton_0, io::IDButton_1, io::IDButton_2,
                                             io::IDButton_3, io::IDButton_4 };
    for (int b = 0; b < 5; ++b)
    {
        char what[64];
        button(b, true);
        frame();
        std::snprintf(what, sizeof what, "button %d is IDButton_%d", b, b);
        check(mouse()->buttonPressed(bits[b]), what);
        button(b, false);
        frame();
        check(mouse()->buttonReleased(bits[b]), "and releases");
    }
    button(0, true, -15);
    frame();
    const float latency = mouse()->buttonPressedSampleLatency(io::IDButton_0);
    check(latency > 0.005f && latency < 0.03f, "press latency measured on the event clock (~15 ms)");
    button(0, false);
    settle();
}

// A wheel notch is +120 away from the user, -120 towards (DirectInput lZ, WHEEL_DELTA).
void testWheelNotches()
{
    begin("wheel notches are +-120");
    wheelLines(1);
    frame();
    check(mouse()->readAxis(io::IDAxis_2, true) == 120.0f, "one notch away is +120");
    wheelLines(-1);
    frame();
    check(mouse()->readAxis(io::IDAxis_2, true) == -120.0f, "one notch towards is -120");
    settle();
}

// Trackpad scrolling: DirectInput's wheel reports whole notches, and Controls.con turns any
// positive or negative wheel sample into c_PINextItem / c_PIPrevItem (addAxisToTriggerMapping,
// ControlMap::axisToTrigger fires on value > 0). A 40-point swipe is four notches; it must not
// select the next item on every sample it spans.
void testPreciseScroll()
{
    begin("trackpad scrolling reports whole notches");
    int samples = 0, nonNotch = 0;
    float total = 0.0f;
    for (int i = 0; i < 20; ++i)
    {
        wheelPixels(2);
        frame();
        const float z = mouse()->readAxis(io::IDAxis_2, true);
        if (z != 0.0f) ++samples;
        if (z != 0.0f && std::fabs(z) != 120.0f) ++nonNotch;
        total += z;
    }
    std::printf("  40 points over 20 samples: %d wheel samples, total %.0f\n", samples, total);
    check(samples == 4, "a 40-point swipe fires 4 wheel samples (one per 10 points)");
    check(nonNotch == 0, "each wheel sample is a whole notch");
    check(std::fabs(total) == 480.0f, "the swipe totals 4 notches");
    settle();
}

// The momentum (inertia) phase AppKit synthesizes after the fingers lift is not device motion.
void testMomentumScroll()
{
    begin("momentum scrolling is not wheel input");
    int samples = 0;
    wheelPixels(5, kCGMomentumScrollPhaseBegin);
    frame();
    if (mouse()->readAxis(io::IDAxis_2, true) != 0.0f) ++samples;
    for (int i = 0; i < 30; ++i)
    {
        wheelPixels(10, kCGMomentumScrollPhaseContinue);
        frame();
        if (mouse()->readAxis(io::IDAxis_2, true) != 0.0f) ++samples;
    }
    wheelPixels(0, kCGMomentumScrollPhaseEnd);
    frame();
    std::printf("  31 momentum events: %d wheel samples\n", samples);
    check(samples == 0, "no wheel samples from momentum events");
    settle();
}

// An exclusively acquired DirectInput mouse (MouseDevice's default cooperative level) hides and
// holds the system cursor, so Windows generates no window mouse messages in-game. Host mouse
// events in that state are marked captured for the window-message source; in menus (mouse
// deactivated by MainMenuLogic::activate) they are not.
void testCapturedMouse(MouseLog& log)
{
    begin("in-game mouse events are captured, menu mouse events are not");
    bool hidden = false, disassociated = false;
    log.reset();
    move(3, 3);
    button(0, true);
    button(0, false);
    wheelLines(1);
    frame();
    CocoaHostTestHooks::cursorStateForTest(ClientHost::instance(), hidden, disassociated);
    check(hidden && disassociated, "in-game: cursor hidden and held (relative mode)");
    check(log.moves == 1 && log.buttons == 2 && log.wheels == 1, "in-game: the host reports the events");
    check(log.capturedMoves == 1 && log.capturedButtons == 2 && log.capturedWheels == 1,
          "in-game: every mouse event is captured");

    mouse()->deActivate();                      // MainMenuLogic::activate
    log.reset();
    move(3, 3);
    button(0, true);
    button(0, false);
    wheelLines(1);
    frame();
    CocoaHostTestHooks::cursorStateForTest(ClientHost::instance(), hidden, disassociated);
    check(!hidden && !disassociated, "menu: cursor visible and free");
    check(log.capturedMoves == 0 && log.capturedButtons == 0 && log.capturedWheels == 0,
          "menu: no mouse event is captured");
    check(log.moves == 1 && log.buttons == 2, "menu: the window still gets moves and clicks");

    mouse()->activate();                        // MainMenuLogic::deactivate
    settle();
}

// With a GCMouse connected the in-game counts are its unaccelerated deltas, summed over every
// mouse; the NSEvent deltas (accelerated) are ignored. Raw motion never reaches window listeners.
void testRawMouse(MouseLog& log)
{
    begin("GCMouse deltas drive the in-game mouse");
    macos::rawMouseSetCountForTest(2);          // two mice
    check(ClientHost::instance().rawMouseAvailable(), "raw mouse available with a GCMouse connected");
    log.reset();
    move(100, 100);                             // the NSEvent of the same motion, accelerated
    raw(5.0f, 3.0f);                            // mouse A
    raw(-2.0f, 1.0f);                           // mouse B
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 3.0f, "X is the sum of the raw deltas (3)");
    check(mouse()->readAxis(io::IDAxis_1, true) == 4.0f, "Y is the sum of the raw deltas, down positive (4)");
    check(log.moves == 1, "the window still sees the NSEvent move (captured), never raw motion");
    for (int i = 0; i < 4; ++i) raw(0.5f, -0.5f);
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 2.0f && mouse()->readAxis(io::IDAxis_1, true) == -2.0f,
          "sub-count raw motion is carried (4 x 0.5 = 2)");
    raw(0.0f, 0.0f);
    frame();

    // Timestamps split raw motion across input ticks like any buffered event: MouseDevice's first
    // tick takes what is older than the previous poll plus the tick (30 ms).
    frame();
    raw(7.0f, 0.0f, 5);
    raw(1.0f, 0.0f, 60);
    CocoaHostTestHooks::pumpRawMouseForTest(ClientHost::instance());
    g_manager->update(-1.0f);
    g_manager->update(0.03f);
    check(mouse()->readAxis(io::IDAxis_0, true) == 7.0f, "first tick sees the older raw report");
    g_manager->update(10.0f);
    check(mouse()->readAxis(io::IDAxis_0, true) == 1.0f, "last tick sees the newer one");

    // Focus: raw reports arriving without focus are discarded.
    CocoaHostTestHooks::overrideFocusForTest(ClientHost::instance(), false, false);
    raw(50.0f, 50.0f);
    frame();
    CocoaHostTestHooks::overrideFocusForTest(ClientHost::instance(), true, true);
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 0.0f, "raw motion without focus is dropped");

    // Disconnect at run time: back to the NSEvent deltas; pending raw reports are ignored.
    macos::rawMouseSetCountForTest(0);
    check(!ClientHost::instance().rawMouseAvailable(), "no raw mouse after the last disconnect");
    raw(9.0f, 9.0f);
    move(4, 2);
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 4.0f && mouse()->readAxis(io::IDAxis_1, true) == 2.0f,
          "fallback: NSEvent deltas drive the mouse");
    macos::rawMouseSetCountForTest(1);          // reconnect
    move(4, 2);
    raw(1.0f, 1.0f);
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 1.0f, "after a reconnect the raw deltas are used again");
    macos::rawMouseSetCountForTest(0);
    settle();
}

// The handler queue publishes from another thread while the engine thread pumps and samples.
void testRawMouseThreads(MouseLog& log)
{
    log.reset();
    begin("raw mouse publication across threads");
    macos::rawMouseSetCountForTest(1);
    // A burst of 200000 reports (ring wrap-around and the full-ring path), then 20000 in lockstep
    // with the engine thread (each waits for the previous one to reach the device), which the
    // device must accept completely: a burst can overflow its 128-record buffer, as in DX8.
    const int burst = 200000, paced = 20000, total = burst + paced;
    std::atomic<bool> done(false), burstDone(false);
    std::atomic<int> accepted(0);
    std::thread producer([&]() {
        for (int i = 0; i < burst; ++i)
            while (!macos::rawMousePublish(1.0f, -1.0f, ClientHost::tickCountMs()))
                std::this_thread::yield();      // full ring: the handler would drop; the test waits
        while (!burstDone.load()) std::this_thread::yield();
        for (int i = 0; i < paced; ++i)
        {
            macos::rawMousePublish(1.0f, -1.0f, ClientHost::tickCountMs());
            while (accepted.load() <= i) std::this_thread::yield();
        }
        done.store(true);
    });
    double sumX = 0.0, sumY = 0.0;
    int frames = 0;
    while (log.rawX < burst) frame();           // drain the burst; the device may overflow here
    frame(); frame();
    burstDone.store(true);
    for (;;)
    {
        const bool finished = done.load();
        frame();
        sumX += mouse()->readAxis(io::IDAxis_0, true);
        sumY += mouse()->readAxis(io::IDAxis_1, true);
        accepted.store(static_cast<int>(sumX));
        ++frames;
        if (finished)
            break;
    }
    producer.join();
    frame();
    sumX += mouse()->readAxis(io::IDAxis_0, true);
    sumY += mouse()->readAxis(io::IDAxis_1, true);
    std::printf("  host delivered %.0f raw; %d samples over %d frames: sum (%.0f, %.0f), %u full-ring retries\n", log.rawX, total, frames,
                sumX, sumY, macos::rawMouseDropped());
    // The device itself clears on a DirectInput buffer overflow (128 records per poll), as DX8
    // does, and this producer runs far faster than any mouse, so the device keeps only part.
    check(log.rawX == total && log.rawY == -total, "every published report reaches the engine thread once, in pairs");
    check(sumX == -sumY, "X and Y stay paired in the device");
    check(sumX == paced && sumY == -paced, "the paced reports all reach the device's axes");
    macos::rawMouseSetCountForTest(0);
    settle();
}

static void testAsyncEnter()
{
    begin("asynchronous Enter held/consuming press bits");
    ClientHost& host = ClientHost::instance();
    (void)host.consumeAsyncEnterState();
    key(kVK_Return, true);
    check(host.consumeAsyncEnterState() == 0x8001, "first Enter query sees held and press");
    check(host.consumeAsyncEnterState() == 0x8000, "second Enter query consumes the press bit");
    key(kVK_Return, true, true);
    check(host.consumeAsyncEnterState() == 0x8000, "auto-repeat creates no second press");
    CocoaHostTestHooks::overrideFocusForTest(host, false, false); CocoaHostTestHooks::overrideFocusForTest(host, true, true);
    key(kVK_Return, true);
    check(host.consumeAsyncEnterState() == 0x8001, "fresh down after an undelivered off-focus release creates a press");
    key(kVK_Return, false);
    check(host.consumeAsyncEnterState() == 0, "release clears held state");
    key(kVK_Return, true); key(kVK_Return, false);
    check(host.consumeAsyncEnterState() == 1, "release before query retains only the press bit");
    check(host.consumeAsyncEnterState() == 0, "released press bit is consumed too");
    key(kVK_ANSI_KeypadEnter, true);
    check(host.consumeAsyncEnterState() == 0x8001, "keypad Enter contributes to VK_RETURN");
    key(kVK_Return, true); key(kVK_ANSI_KeypadEnter, false);
    check(host.consumeAsyncEnterState() == 0x8001, "either Enter keeps the aggregate held");
    key(kVK_Return, false);
    key(kVK_ANSI_A, true);
    check(host.consumeAsyncEnterState() == 0, "ordinary character does not affect Enter");
    key(kVK_ANSI_A, false);
}

void testHostBoundary()
{
    begin("Cocoa labels and actual window-message text delivery");
    ClientHost& host = ClientHost::instance();
    struct Label { unsigned char dik; const char* text; };
    const Label labels[] = {{56,"Option"}, {184,"Right Option"}, {219,"Command"},
                            {220,"Right Command"}, {69,"Clear"}};
    for (const auto& label : labels)
        check(host.keyDisplayName(label.dik) && std::string(host.keyDisplayName(label.dik)) == label.text,
              "Cocoa retains native key names");
    // controlShortcutDown polls AppKit's global flags; injected events cannot validate that query.
    WindowMessageRecorder messages;
    host.addListener(&messages);
    struct TextKey { unsigned short key, character; CGEventFlags modifiers; };
    const TextKey keys[] = {{kVK_ANSI_C,3,kCGEventFlagMaskControl}, {kVK_ANSI_KeypadEnter,3,0},
                            {kVK_Delete,0x7F,0}, {kVK_ANSI_V,'v',kCGEventFlagMaskCommand}};
    for (const auto& value : keys) {
        CGEventRef event = CGEventCreateKeyboardEvent(0, value.key, true);
        CGEventSetFlags(event, value.modifiers);
        CGEventSetIntegerValueField(event, kCGKeyboardEventKeyboardType, kKeyboardANSI);
        CGEventKeyboardSetUnicodeString(event, 1, &value.character);
        inject(event);
        key(value.key, false);
    }
    check(messages.values(hostmsg::WM_CHAR) == std::vector<unsigned int>({3,13,8,22}),
          "real Cocoa event routing preserves Ctrl-C, keypad Enter, Backspace and Command-V");
    host.removeListener(&messages);
    settle();
}

// The focus gate reads a cache of AppKit's state. With the state AppKit and the system would
// report simulated (CocoaHostTestHooks::setSystemFocusForTest), a stale cache is brought in line
// by the next pump, an active application's window that is not key is made key, a press into the
// window repairs before it is routed, and what the gate drops is counted and reported.
void testFocusRepair()
{
    begin("focus follows AppKit; a press repairs a closed gate");
    ClientHost& host = ClientHost::instance();
    typedef CocoaHostTestHooks Hooks;
    FocusLog focus;
    host.addListener(&focus);
    const int self = static_cast<int>(getpid()), otherApp = 1;
    Hooks::SystemFocus sys = {};
    sys.appActive = sys.windowKey = sys.windowOnScreen = sys.makeKeyWorks = true;
    sys.frontPid = self;
    sys.now = 1000.0;
    const auto stats = [&]() { return Hooks::focusRepairStatsForTest(host); };
    // Hands the simulated state to the host `seconds` later; the window's key status is the host's.
    const auto apply = [&](double seconds) { sys.now += seconds; Hooks::setSystemFocusForTest(host, &sys); };
    const auto pump = [&]() { Hooks::pumpFocusForTest(host); sys.windowKey = stats().windowKey; };
    const auto click = [&]() { button(0, true); button(0, false); sys.windowKey = stats().windowKey; };

    // A callback that never came: the cache says inactive and not key, AppKit says active and key.
    Hooks::overrideFocusForTest(host, false, false);
    apply(0.0);
    focus.reset();
    check(!host.hasFocus(), "stale cache: the gate is closed");
    pump();
    check(host.hasFocus() && host.isAppActive(), "a pump brings the cache in line with AppKit");
    check(focus.activations == 1 && focus.focusChanges == 1 && focus.key && focus.active,
          "the listeners hear the activation and the focus once");
    pump(); pump();
    check(focus.activations == 1 && focus.focusChanges == 1 && focus.deactivations == 0,
          "unchanged state notifies nobody");
    check(stats().makeKeyCalls == 0, "a window that is key is left alone");

    // Application active, window on screen but never made key, cache still "key".
    sys.windowKey = false;
    apply(1.0);
    focus.reset();
    pump();
    check(stats().makeKeyCalls == 1 && stats().windowKey, "a pump makes the game window key");
    check(host.hasFocus() && focus.focusChanges == 0, "the gate stays open, nobody is notified twice");
    pump();
    check(stats().makeKeyCalls == 1, "once it is key nothing more is done");

    // Making it key fails: the cache follows AppKit, the attempt repeats every half second.
    sys.windowKey = false;
    sys.makeKeyWorks = false;
    apply(1.0);
    focus.reset();
    pump();
    check(stats().makeKeyCalls == 2, "another attempt for a window that lost the key status unnoticed");
    check(!host.hasFocus() && host.isAppActive() && focus.focusChanges == 1 && !focus.key && focus.active
              && focus.deactivations == 0,
          "the cache follows AppKit: not key, application still active");
    pump(); pump();
    check(stats().makeKeyCalls == 2, "not on every pump");
    apply(0.6);
    pump();
    check(stats().makeKeyCalls == 3, "again after half a second");

    // Another window of this process is due the key status: it keeps it.
    sys.makeKeyWorks = true;
    sys.otherKeyWindow = true;
    apply(1.0);
    pump();
    check(stats().makeKeyCalls == 3 && !host.hasFocus(), "an alert of this process keeps the key status");
    sys.otherKeyWindow = false;
    apply(1.0);
    focus.reset();
    pump();
    check(stats().makeKeyCalls == 4 && host.hasFocus() && focus.focusChanges == 1 && focus.key,
          "without it the window is made key and the gate opens");
    pump();

    // The window gives the key status away while the application stays active (Spotlight): pumps
    // leave it there, a press into the window takes it back and is delivered.
    sys.windowKey = false;
    apply(1.0);
    Hooks::focusCallbackForTest(host, Hooks::WindowResignedKey);
    check(!host.hasFocus() && host.isAppActive(), "resigned key: the gate closes");
    for (int i = 0; i < 3; ++i) { apply(1.0); pump(); }
    check(stats().makeKeyCalls == 4 && !host.hasFocus(),
          "a pump does not take the keyboard back from a panel of another process");
    focus.reset();
    button(0, true);
    sys.windowKey = stats().windowKey;
    check(stats().makeKeyCalls == 5 && host.hasFocus(), "a press into the game window makes it key");
    check(focus.buttons == 1 && focus.buttonsWithFocus == 1 && focus.focusChanges == 1,
          "and is delivered, after the focus change");
    button(0, false);
    check(focus.buttons == 2, "its release pairs up");

    // A modal window of this process: the press is not the game's. It is dropped and reported,
    // one report a second at most.
    sys.windowKey = false;
    sys.modalWindow = true;
    apply(2.0);
    Hooks::focusCallbackForTest(host, Hooks::WindowResignedKey);
    const Hooks::FocusRepairStats before = stats();
    focus.reset();
    click();
    check(focus.buttons == 0 && !host.hasFocus() && stats().makeKeyCalls == before.makeKeyCalls,
          "a press behind a modal window is not delivered and takes nothing");
    check(stats().droppedInputs == before.droppedInputs + 1 && stats().dropReports == before.dropReports + 1,
          "the dropped press is counted and reported");
    for (int i = 0; i < 4; ++i) click();
    key(kVK_ANSI_W, true);
    key(kVK_ANSI_W, false);
    move(1, 1);
    check(stats().droppedInputs == before.droppedInputs + 7 && stats().dropReports == before.dropReports + 1,
          "presses, key presses and moves are counted; one report a second at most");
    apply(1.5);
    click();
    check(stats().dropReports == before.dropReports + 2, "the next second reports again");
    sys.modalWindow = false;

    // The application resigns active and no callback says so: the next pump notices.
    sys.windowKey = true;
    apply(2.0);
    Hooks::focusCallbackForTest(host, Hooks::WindowBecameKey);
    check(host.hasFocus(), "key again");
    sys.appActive = false;
    sys.windowKey = false;
    sys.frontPid = otherApp;
    apply(1.0);
    focus.reset();
    pump();
    check(!host.hasFocus() && !host.isAppActive() && focus.deactivations == 1 && focus.focusChanges == 1,
          "a deactivation nobody reported is noticed by the next pump, once");
    click();
    check(focus.buttons == 0 && !stats().systemActive && stats().makeKeyCalls == before.makeKeyCalls,
          "the click that brings the application forward from another one is not game input");

    // The system calls this process frontmost, AppKit never activates it: a press uses the
    // system's state once that has lasted a quarter of a second.
    sys.frontPid = self;
    apply(1.0);
    pump();
    focus.reset();
    click();
    check(focus.buttons == 0 && !stats().systemActive, "not at once: the application may just be coming forward");
    apply(0.3);
    pump();
    button(0, true);
    check(stats().systemActive && host.hasFocus() && host.isAppActive(), "then the press uses the system's state");
    check(focus.buttons == 1 && focus.buttonsWithFocus == 1 && focus.activations == 1 && focus.focusChanges == 1,
          "and is delivered, after the activation and the focus change");
    button(0, false);
    sys.frontPid = 0;
    apply(1.0);
    pump();
    check(stats().systemActive && host.hasFocus(), "a moment without a frontmost application is not the user leaving");
    sys.frontPid = otherApp;
    apply(1.0);
    focus.reset();
    pump();
    check(!stats().systemActive && !host.hasFocus() && focus.deactivations == 1,
          "another application in front ends that at the next pump");

    // Focus set by a test stands, as before: no simulated system, no repair.
    Hooks::setSystemFocusForTest(host, 0);
    Hooks::overrideFocusForTest(host, false, false);
    focus.reset();
    Hooks::pumpFocusForTest(host);
    button(0, true);
    button(0, false);
    check(!host.hasFocus() && focus.focusChanges == 0 && focus.buttons == 0,
          "overrideFocusForTest alone: pumps and presses leave the forced focus alone");
    host.removeListener(&focus);
    setFocus(true);
    settle();
}

// ProcessAncestry.h: the walk up the parent chain that decides whether the application in front
// started this process, against a process table and against this process's real parent.
void testProcessAncestry()
{
    begin("process ancestry: the parent chain up to launchd");
    using namespace macos;
    // launchd (1) -> 10 -> 20 -> 30 -> 40 -> this process: 40 is the parent.
    const auto table = [](pid_t pid) -> pid_t
    { return pid == 40 ? 30 : pid == 30 ? 20 : pid == 20 ? 10 : pid == 10 ? 1 : -1; };
    check(processAncestry(40, 40, table) == AncestryYes, "the parent is an ancestor");
    check(processAncestry(10, 40, table) == AncestryYes, "so is the process four levels up");
    check(processAncestry(50, 40, table) == AncestryNo, "a process outside the chain is not");
    check(processAncestry(1, 40, table) == AncestryNo && processAncestry(0, 40, table) == AncestryNo,
          "launchd and the kernel are nobody's launcher");
    check(processAncestry(10, 1, table) == AncestryNo, "a process adopted by launchd has no launcher left");
    // The lookup fails above the parent.
    const auto denied = [](pid_t) -> pid_t { return -1; };
    check(processAncestry(30, 40, denied) == AncestryUnknown, "a failed lookup leaves the answer unknown");
    check(processAncestry(40, 40, denied) == AncestryYes, "the parent is recognised without any lookup");
    const auto loop = [](pid_t pid) -> pid_t { return pid == 40 ? 30 : 40; };
    check(processAncestry(10, 40, loop) == AncestryUnknown, "a chain that never reaches launchd is given up");
    // The kernel's process table, as the client reads it. launchd (pid 1) adopts a process whose
    // parent has exited, as when this test is started detached from a shell that is gone, and
    // launchd is nobody's launcher. So the answer for the parent follows who the parent is.
    const pid_t parent = getppid();
    std::printf("  this process's parent is pid %d%s\n", static_cast<int>(parent),
                parent > 1 ? "" : ": launchd, the test was started detached");
    check(parentPidOf(getpid()) == parent, "sysctl answers this process's parent");
    check(processAncestry(parent, parent, parentPidOf) == (parent > 1 ? AncestryYes : AncestryNo) &&
          processAncestry(getpid(), parent, parentPidOf) == AncestryNo,
          "the real chain: the parent is an ancestor unless it is launchd, this process is not its own");
}

// The launch activation with the system simulated (SystemFocus::activation). The application in
// front at the first request decides what follows. A launcher (the parent or another ancestor of
// this process), nobody, and this process itself lose the front to a request every half second
// and, after a second, to a reset through the Dock. An unrelated application gets the one
// cooperative request and keeps the front. Cooperative means that the request yields to an active
// application on every macOS version: before macOS 14 the host makes it with
// activateIgnoringOtherApps:NO, and every other request with YES, which activates whatever
// application is active. A look is one run of the host's event pump. createWindow looks once,
// right after the first request; the next look comes with the first frame, when the engine has
// initialised. The requests run for ten seconds, counted from that look, the first after the one
// inside createWindow, and once more from the first look that finds the front free after a wait
// behind an unrelated application; and no period ends between its Dock reset and the request
// after it. Requests and Dock resets are counted, none is made.
void testLaunchActivation()
{
    begin("launch activation: only a launcher loses the front to repeated requests");
    ClientHost& host = ClientHost::instance();
    typedef CocoaHostTestHooks Hooks;
    // A client started from Terminal through a script: Terminal -> login -> shell -> script ->
    // this process. The editor and the browser are applications outside that chain.
    const int self = static_cast<int>(getpid());
    const int terminal = 484, login = 663, shell = 665, script = 59564;
    const int editor = 439, browser = 600, dock = 511;
    Hooks::SystemFocus sys = {};
    sys.windowOnScreen = sys.makeKeyWorks = true;
    sys.activation = true;
    sys.parentPid = script;
    sys.ancestors[0] = shell;
    sys.ancestors[1] = login;
    sys.ancestors[2] = terminal;
    sys.dockPid = dock;
    sys.now = 5000.0;
    const auto stats = [&]() { return Hooks::activationStatsForTest(host); };
    const auto fallback = [&]() { return Hooks::focusRepairStatsForTest(host).systemActive; };
    // Hands the simulated state to the host `seconds` later.
    const auto apply = [&](double seconds) { sys.now += seconds; Hooks::setSystemFocusForTest(host, &sys); };
    const auto pump = [&]() { Hooks::pumpFocusForTest(host); sys.windowKey = Hooks::focusRepairStatsForTest(host).windowKey; };
    const auto after = [&](double seconds) { apply(seconds); pump(); };
    const auto click = [&]() { button(0, true); button(0, false); };
    const auto appKitActive = [&]() { sys.appActive = sys.windowKey = true; sys.frontPid = self; };
    // What the scenario under way added to the counters.
    Hooks::ActivationStats base = stats();
    const auto requests = [&]() { return stats().requests - base.requests; };
    // Those of the first requests that ignore other applications; the rest are cooperative.
    const auto ignoringOthers = [&]() { return stats().ignoringOthers - base.ignoringOthers; };
    const auto retries = [&]() { return stats().retries - base.retries; };
    const auto dockResets = [&]() { return stats().dockResets - base.dockResets; };
    // Ends whatever a scenario left: another application in front, twenty seconds later.
    const auto finish = [&]()
    {
        sys.appActive = sys.windowKey = false;
        sys.frontPid = browser;
        after(20.0);
    };
    // The first request: the game window is shown inactive with `front` in front.
    const auto request = [&](int front)
    {
        finish();
        sys.frontPid = front;
        after(1.0);
        base = stats();
        Hooks::beginActivationForTest(host);
    };
    // The look inside createWindow, which follows the first request at once. The host is told
    // that createWindow runs by the object createWindow itself holds, not by this test.
    const auto lookInsideCreateWindow = [&]()
    {
        Hooks::lookInsideCreateWindowForTest(host);
        sys.windowKey = Hooks::focusRepairStatsForTest(host).windowKey;
    };
    // A start, as createWindow makes it: the first request and its own look. Every `after` that
    // follows is a later look, and the first of them stands for the first frame's.
    const auto start = [&](int front)
    {
        request(front);
        lookInsideCreateWindow();
    };
    Hooks::overrideFocusForTest(host, false, false);

    // An unrelated application in front: the user works there while the client starts.
    start(editor);
    check(stats().pending && requests() == 1 && retries() == 0 && dockResets() == 0,
          "an unrelated application in front: the one cooperative request is made");
    check(ignoringOthers() == 0,
          "it does not ignore other applications: activateIgnoringOtherApps:NO before macOS 14");
    for (double step : { 0.1, 0.5, 0.6, 1.0, 2.0, 3.0 })
        after(step);                            // 7.2 s after the request
    check(stats().pending && requests() == 1 && retries() == 0 && dockResets() == 0,
          "while it stays in front nothing is repeated and the Dock is left alone");
    check(!host.hasFocus() && !host.isAppActive() && !fallback(), "the game stays without focus");
    after(3.0);                                 // 10.1 s after the first look
    check(!stats().pending && requests() == 1 && retries() == 0 && dockResets() == 0,
          "ten seconds after its first look the host stops waiting, with that one request made");
    check(ignoringOthers() == 0, "and no request of the whole wait ignored other applications");

    // The first request by who is in front. It ignores other applications when the launcher,
    // nobody or this process is in front. For an application that did not start the client, and
    // for one that a denied lookup cannot place, it is cooperative. Before macOS 14 that is the
    // flag of activateIgnoringOtherApps:, and with YES this one request alone would take the
    // front from the application the user works in.
    {
        struct First { int front; bool lookupDenied; int ignoring; const char* what; };
        const First firsts[] = {
            { editor,   false, 0, "an unrelated application in front: the first request is cooperative" },
            { dock,     false, 0, "the Dock in front: cooperative, the Dock did not start the client" },
            { terminal, false, 1, "an ancestor in front: the first request ignores other applications" },
            { script,   false, 1, "the parent in front: the first request ignores other applications" },
            { 0,        false, 1, "nobody in front: the first request ignores other applications" },
            { self,     false, 1, "this process in front for the system: the first request ignores other applications" },
            { terminal, true,  0, "lookup denied, an ancestor above the parent in front: cooperative, it may be a stranger" },
            { editor,   true,  0, "lookup denied, an unrelated application in front: cooperative" },
            { script,   true,  1, "lookup denied, the parent in front: the first request ignores other applications" },
        };
        for (const First& first : firsts)
        {
            sys.ancestryDenied = first.lookupDenied;
            start(first.front);
            check(stats().pending && requests() == 1 && retries() == 0 &&
                  ignoringOthers() == first.ignoring, first.what);
        }
        sys.ancestryDenied = false;
    }

    // The same start, and the system grants the request.
    start(editor);
    appKitActive();
    after(0.4);
    check(!stats().pending && retries() == 0 && dockResets() == 0 && host.hasFocus(),
          "the request granted: the game is active and nothing was repeated");

    // The same start; then the system calls this process frontmost and AppKit never activates
    // it. That state is repaired as after a launcher's start, a second after it began.
    start(editor);
    after(3.0);
    sys.frontPid = self;
    after(0.1);
    check(retries() == 0 && dockResets() == 0 && !fallback(),
          "this process frontmost for the system after the wait: no Dock reset at once");
    after(0.5);
    check(retries() == 1 && dockResets() == 0, "the request is repeated within half a second");
    after(0.6);
    check(dockResets() == 1 && fallback() && host.hasFocus(),
          "a second later the system's state is used and the activation reset through the Dock");
    sys.frontPid = dock;
    after(0.1);
    check(stats().pending && retries() == 1 && host.hasFocus(), "the Dock in front does not end the period");
    after(0.25);
    check(retries() == 2, "the request follows three tenths of a second after the reset");
    appKitActive();
    after(0.05);
    check(!stats().pending && !fallback() && host.hasFocus() && dockResets() == 1,
          "AppKit active: the period ends and the system-state fallback is left");

    // The same again, but the requests are refused and the unrelated application is in front
    // once more: it keeps the front, and the host stops trusting the system's state.
    start(editor);
    after(1.0);
    sys.frontPid = self;
    after(0.6);
    after(0.6);
    after(0.6);
    check(fallback() && host.hasFocus() && retries() == 2 && dockResets() == 1,
          "frontmost for the system again: two requests, then the fallback and the Dock reset");
    sys.frontPid = editor;
    after(0.2);
    check(stats().pending && !fallback() && !host.hasFocus(),
          "the unrelated application in front again: the fallback ends, the period waits");
    after(0.6);
    after(0.6);
    check(retries() == 2 && dockResets() == 1, "and nothing more is requested while it stays there");
    after(9.0);                                 // 11.6 s after the front first came free
    check(!stats().pending && retries() == 2 && dockResets() == 1,
          "not even the request owed after the Dock reset: the wait ends on its ten seconds");

    // The unrelated application quits and nobody is in front: the front is free.
    start(editor);
    after(2.0);
    sys.frontPid = 0;
    after(0.6);
    check(retries() == 1 && dockResets() == 0, "nobody in front after the wait: the request is repeated");

    // The user goes from the unrelated application to the parent: the parent may lose the front
    // whenever it holds it, as before.
    start(editor);
    after(2.0);
    sys.frontPid = script;
    after(0.6);
    after(0.6);
    after(0.6);
    check(stats().pending && retries() == 2 && dockResets() == 1,
          "the parent in front after the wait: repeated requests and the Dock reset");

    // An ancestor in front that is not the parent: Terminal above the shells and the script.
    start(terminal);
    check(stats().pending && requests() == 1 && retries() == 0, "an ancestor in front: the first request");
    after(0.2);
    check(retries() == 0, "not repeated before half a second has passed");
    after(0.4);
    check(retries() == 1 && dockResets() == 0, "repeated after half a second");
    after(0.5);
    check(retries() == 1 && dockResets() == 1, "after a second the activation is reset through the Dock");
    sys.frontPid = dock;
    after(0.1);
    after(0.25);
    check(retries() == 2 && stats().pending, "and requested again with the Dock in front");
    sys.frontPid = terminal;                    // refused: the launcher is in front again
    after(0.3);
    after(0.3);
    after(0.6);
    check(retries() == 4 && dockResets() == 1,
          "refused: the requests go on every half second, the Dock reset is not repeated");
    appKitActive();
    after(0.1);
    check(!stats().pending && host.hasFocus(), "until AppKit reports the application active");

    // The parent in front: an application that started the client directly.
    start(script);
    after(0.6);
    after(0.6);
    check(requests() == 1 && retries() == 1 && dockResets() == 1,
          "the parent in front: repeated requests and the Dock reset");

    // Nobody in front.
    start(0);
    after(0.6);
    after(0.6);
    check(requests() == 1 && retries() == 1 && dockResets() == 1,
          "nobody in front: repeated requests and the Dock reset");

    // This process in front for the system while AppKit reports it inactive.
    start(self);
    after(0.6);
    check(retries() == 1 && !fallback() && !host.hasFocus(),
          "frontmost for the system, inactive for AppKit: the request is repeated");
    after(0.6);
    check(fallback() && host.hasFocus() && dockResets() == 1,
          "after a second the system's state is used and the activation reset through the Dock");
    sys.frontPid = dock;
    after(0.35);
    appKitActive();
    after(0.1);
    check(!stats().pending && !fallback() && host.hasFocus() && retries() == 2,
          "the request after the reset is granted: active without a switch of applications");

    // Without a Dock process the requests simply go on.
    sys.dockPid = 0;
    start(terminal);
    after(0.6);
    after(0.6);
    check(retries() == 2 && dockResets() == 0 && stats().pending, "no Dock process: requests only");
    sys.dockPid = dock;

    // The user goes to another application after the first request: that ends the period, also
    // when the launcher is in front again afterwards.
    start(terminal);
    after(0.6);
    sys.frontPid = browser;
    after(0.2);
    check(!stats().pending && retries() == 1 && dockResets() == 0,
          "the user switches to another application: the requests end");
    sys.frontPid = terminal;
    after(0.6);
    after(0.6);
    check(!stats().pending && retries() == 1 && dockResets() == 0, "and do not resume behind the user's back");
    start(editor);
    sys.frontPid = browser;
    after(0.3);
    check(!stats().pending, "a switch away from the unrelated application ends the wait too");
    sys.frontPid = 0;
    after(0.6);
    check(retries() == 0 && dockResets() == 0, "nothing is requested after that");

    // The process lookup is denied: an ancestor above the parent cannot be told from an
    // unrelated application and is treated as one. The parent needs no lookup.
    sys.ancestryDenied = true;
    start(terminal);
    after(0.6);
    after(0.6);
    after(1.0);
    check(stats().pending && requests() == 1 && retries() == 0 && dockResets() == 0,
          "lookup denied: an ancestor above the parent gets the one request, like a stranger");
    start(script);
    after(0.6);
    after(0.6);
    check(retries() == 1 && dockResets() == 1, "lookup denied: the parent is still the launcher");
    sys.ancestryDenied = false;

    // createWindow looks once, right after the first request, and the host does not take that
    // look for the start of a period's ten seconds: it marks the time createWindow runs. No
    // window is created here, so the look is made in createWindow's place
    // (lookInsideCreateWindowForTest), marked by the object that createWindow holds for this.
    // The checks therefore run the client's own lines for the mark. What they cannot see is
    // that createWindow holds that object; macos_host_test, which opens a real window, checks it.
    {
        request(terminal);
        const Hooks::ActivationStats atRequest = stats();
        lookInsideCreateWindow();
        check(stats().looks == atRequest.looks + 1 &&
              stats().looksInsideCreateWindow == atRequest.looksInsideCreateWindow + 1,
              "the look made in createWindow's place is one look, counted as made inside createWindow");
        check(stats().pending && !stats().lookCounted,
              "the look inside createWindow does not start the period's ten seconds");
        after(6.5);
        check(stats().looks == atRequest.looks + 2 &&
              stats().looksInsideCreateWindow == atRequest.looksInsideCreateWindow + 1,
              "the look after it is one more look, and not one made inside createWindow");
        check(stats().pending && stats().lookCounted, "the first look after createWindow starts the ten seconds");
    }

    // The host is not pumped while the engine initialises: its first look at a start after the
    // one inside createWindow comes six to seven seconds after the first request on the Intel
    // test machine and later on a slower one. A period's ten seconds count from that look, so the
    // repair is the same however long the initialisation took. Here a launcher's period is first
    // looked at 10.5 s and 30 s after the request (a cold disk cache, a debug build): it still
    // gets its Dock reset and its repeated requests, and ends ten seconds after that look.
    for (double firstLook : { 10.5, 30.0 })
    {
        char what[240];
        const auto say = [&](const char* text)
        {
            std::snprintf(what, sizeof what, "a launcher's period first looked at %.1f s after the request, %s",
                          firstLook, text);
            return what;
        };
        // The launcher is still in front: the system refused the first request.
        start(terminal);
        after(firstLook);
        check(stats().pending && requests() == 1 && dockResets() == 1 && retries() == 0 && !fallback(),
              say("the launcher in front: the period goes on and the activation is reset through the Dock at once"));
        sys.frontPid = dock;
        after(0.35);
        check(stats().pending && retries() == 1, say("the launcher in front: the request after the reset is made"));
        sys.frontPid = terminal;                // refused: the launcher is in front again
        after(0.6);
        after(0.6);
        check(stats().pending && retries() == 3 && dockResets() == 1,
              say("the launcher in front: the requests go on every half second, with the one Dock reset"));
        after(8.3);                             // 9.85 s after the first look
        check(stats().pending && retries() == 4,
              say("the launcher in front: the period still runs 9.85 s after that look"));
        after(0.2);                             // 10.05 s after the first look
        check(!stats().pending && retries() == 4 && dockResets() == 1 && !host.hasFocus(),
              say("the launcher in front: the period ends ten seconds after that look"));
        // This process is in front for the system while AppKit reports it inactive, the state
        // seen after a start from Terminal on Apple silicon.
        start(terminal);
        sys.frontPid = self;
        after(firstLook);
        check(stats().pending && fallback() && host.hasFocus() && dockResets() == 1 && retries() == 0,
              say("this process in front for the system: its state is used and the activation reset through the Dock at once"));
        sys.frontPid = dock;
        after(0.35);
        check(stats().pending && retries() == 1 && host.hasFocus(),
              say("this process in front for the system: the request after the reset is made"));
        sys.frontPid = self;                    // refused: in front for the system again, AppKit inactive
        after(0.6);
        after(0.6);
        check(stats().pending && retries() == 3 && dockResets() == 1 && fallback(),
              say("this process in front for the system: the requests go on every half second, with the one Dock reset"));
        after(8.3);                             // 9.85 s after the first look
        check(stats().pending && retries() == 4,
              say("this process in front for the system: the period still runs 9.85 s after that look"));
        after(0.2);                             // 10.05 s after the first look
        check(!stats().pending && retries() == 4 && fallback() && host.hasFocus(),
              say("this process in front for the system: the period ends ten seconds after that look, "
                  "the system's state still holding the focus"));
    }
    // The request after that late Dock reset is granted: active without a click.
    start(terminal);
    sys.frontPid = self;
    after(30.0);
    sys.frontPid = dock;
    after(0.35);
    appKitActive();
    after(0.05);
    check(!stats().pending && !fallback() && host.hasFocus() && retries() == 1 && dockResets() == 1,
          "a launcher's period first looked at 30.0 s after the request, the request after the reset granted: active");
    // First looked at within ten seconds of the request (6.5 s, the Intel test machine), a period
    // makes the requests it made when its ten seconds counted from the request, and goes on
    // making them until ten seconds after that look.
    start(terminal);
    after(6.5);
    check(stats().pending && dockResets() == 1 && retries() == 0,
          "first look 6.5 s after the request: the activation is reset through the Dock at once");
    sys.frontPid = dock;
    after(0.35);
    sys.frontPid = terminal;
    after(0.6);
    after(0.6);                                 // 8.05 s after the request
    check(retries() == 3 && dockResets() == 1, "first look 6.5 s after the request: three requests in the next 1.55 s");
    after(2.0);                                 // 10.05 s after the request, 3.55 s after the first look
    check(stats().pending && retries() == 4,
          "first look 6.5 s after the request: ten seconds after the request the period goes on");
    after(6.3);                                 // 9.85 s after the first look
    check(stats().pending && retries() == 5,
          "first look 6.5 s after the request: the period still runs 9.85 s after that look");
    after(0.2);                                 // 10.05 s after the first look
    check(!stats().pending && retries() == 5 && dockResets() == 1,
          "first look 6.5 s after the request: the period ends ten seconds after that look");
    // The pump inside createWindow can itself deliver a click into the new window. A click that
    // ends a wait behind an unrelated application counts the ten seconds from the click, and
    // when it arrives in a frame's pump no look counts them again. This one arrives inside
    // createWindow, with the engine's initialisation still ahead: the first look after
    // createWindow counts them from itself, or a first frame 10.5 s later would end the period
    // that the click began, before its Dock reset.
    request(editor);
    Hooks::lookInsideCreateWindowForTest(host, [&] { click(); });
    sys.windowKey = Hooks::focusRepairStatsForTest(host).windowKey;
    check(stats().pending && retries() == 1 && dockResets() == 0 && !stats().lookCounted,
          "a click delivered by the look inside createWindow: it requests at once, and the ten seconds are "
          "still to be counted from the first look after createWindow");
    after(10.5);
    check(stats().pending && stats().lookCounted && retries() == 1 && dockResets() == 1,
          "the first look 10.5 s after that click: the period goes on and the activation is reset through the Dock");
    sys.frontPid = dock;
    after(0.35);
    sys.frontPid = editor;                      // refused: the application clicked away from is in front again
    after(9.5);                                 // 9.85 s after the first look
    check(stats().pending && retries() == 3,
          "the period that the click inside createWindow began still runs 9.85 s after the first look");
    after(0.2);                                 // 10.05 s after the first look
    check(!stats().pending && retries() == 3 && dockResets() == 1, "and ends ten seconds after that look");
    // A click can also arrive in the first frame's pump, which routes it before it looks. In a
    // launcher's period the requests were free to run from the start and the click starts no
    // count of its own, so it does not stand in for the first look either: the look of that same
    // pump starts the ten seconds. Were the click taken for it, a first frame 10.5 s after the
    // request would end the period right after the user's click, before its Dock reset.
    start(terminal);
    apply(10.5);
    click();
    check(stats().pending && retries() == 1 && !stats().lookCounted,
          "a click in the first frame's pump of a launcher's period: it requests at once, and the ten seconds "
          "are still to be counted from that pump's look");
    pump();
    check(stats().pending && stats().lookCounted && retries() == 1 && dockResets() == 1,
          "the look of that pump, 10.5 s after the request: the period goes on and the activation is reset "
          "through the Dock");

    // A start behind a bystander (here Terminal, the lookup denied) whose one request left this
    // process frontmost for the system and inactive for AppKit is repaired in full however late
    // the first look comes: the ten seconds count from that look, which is also the first to find
    // the front free.
    sys.ancestryDenied = true;
    for (double firstLook : { 8.8, 9.5, 10.5 })
    {
        char what[160];
        const auto say = [&](const char* text)
        {
            std::snprintf(what, sizeof what, "first look %.1f s after the request: %s", firstLook, text);
            return what;
        };
        start(terminal);
        sys.frontPid = self;
        after(firstLook);
        check(stats().pending && requests() == 1 && retries() == 1 && dockResets() == 0 && !fallback(),
              say("the period goes on and the request is repeated"));
        after(0.6);
        after(0.6);
        check(retries() == 2 && dockResets() == 1 && fallback() && host.hasFocus(),
              say("a second later the system's state is used and the activation reset through the Dock"));
        sys.frontPid = dock;
        after(0.35);
        check(stats().pending && retries() == 3, say("the request after the reset is made"));
        appKitActive();
        after(0.05);
        check(!stats().pending && !fallback() && host.hasFocus(), say("and granted: active without a click"));
    }
    // The same late look with the bystander still in front: the ten seconds count from that look
    // as for every period, so the host goes on waiting. Nothing is asked while the bystander stays.
    start(terminal);
    after(10.5);
    check(stats().pending && retries() == 0 && dockResets() == 0 && !host.hasFocus(),
          "first look 10.5 s after the request, the bystander still in front: the host goes on waiting and asks nothing");
    after(9.9);
    check(stats().pending && retries() == 0 && dockResets() == 0,
          "the wait behind the bystander lasts ten seconds from that look");
    after(0.2);                                 // 10.1 s after the first look
    check(!stats().pending && requests() == 1 && retries() == 0 && dockResets() == 0 && !host.hasFocus(),
          "then the host stops waiting, with the one request made");
    // This process comes to the front during that wait: repaired as after an earlier look.
    start(terminal);
    after(10.5);
    sys.frontPid = self;
    after(4.0);                                 // 14.5 s after the request
    check(stats().pending && retries() == 1 && dockResets() == 0 && !fallback(),
          "this process in front 4 s after a first look at 10.5 s: the request is repeated");
    after(0.6);
    after(0.6);
    check(retries() == 2 && dockResets() == 1 && fallback() && host.hasFocus(),
          "and a second later the system's state is used and the activation reset through the Dock");
    sys.ancestryDenied = false;
    // The one cooperative request can be granted for the system at once. The look inside
    // createWindow then already finds this process in front and counts the ten seconds for a
    // front that came free. The next look counts them from itself all the same, so this start is
    // repaired too when that look comes late.
    request(editor);
    sys.frontPid = self;
    apply(0.0);
    lookInsideCreateWindow();
    after(10.5);
    check(stats().pending && fallback() && host.hasFocus() && dockResets() == 1 && retries() == 0,
          "an unrelated application at the request, this process in front at the look inside createWindow, the next "
          "look 10.5 s later: the system's state is used and the activation reset through the Dock");
    // That look inside createWindow made the period's one count for a front that came free. When
    // the unrelated application is in front again at the first look and the front comes free once
    // more later, the request is repeated but the ten seconds are not counted a third time: the
    // period ends ten seconds after the first look.
    request(editor);
    sys.frontPid = self;
    apply(0.0);
    lookInsideCreateWindow();
    sys.frontPid = editor;
    after(1.0);                                 // the first look: ten seconds from here
    sys.frontPid = self;
    after(9.5);                                 // 10.5 s after the request: the front is free again
    check(stats().pending && retries() == 1 && dockResets() == 0,
          "the front free at the look inside createWindow and again 9.5 s after the first look: the request is repeated");
    after(0.6);                                 // 10.1 s after the first look
    check(!stats().pending && retries() == 1 && dockResets() == 0,
          "but the count for a front that came free was made inside createWindow: the period ends ten seconds "
          "after the first look");

    // A period that waited behind an unrelated application counts its ten seconds once more from
    // the first look that finds the front free. In the starts above that look is the first look
    // itself, or follows it so closely that the ten seconds of the first look cover everything
    // checked; the second count decides nothing there. It decides when the front comes free late
    // in those ten seconds: the requests, the system-state fallback and the Dock reset would
    // otherwise get what is left of them, here 2 s. The first look comes 1.0 s after the request
    // and finds the editor in front; this process is in front for the system, inactive for
    // AppKit, from the look 9.0 s after the request.
    start(editor);
    after(1.0);                                 // the first look: ten seconds from here
    sys.frontPid = self;
    after(8.0);                                 // 9.0 s after the request: the front is free
    check(stats().pending && retries() == 1 && dockResets() == 0 && !fallback(),
          "this process in front 8.0 s after the first look: the request is repeated");
    after(0.6);
    after(0.6);                                 // 10.2 s after the request
    check(retries() == 2 && dockResets() == 1 && fallback() && host.hasFocus(),
          "front free 8.0 s after the first look: a second later the system's state is used and the "
          "activation reset through the Dock");
    sys.frontPid = dock;
    after(0.35);
    check(stats().pending && retries() == 3, "front free 8.0 s after the first look: the request after the reset is made");
    sys.frontPid = self;                        // refused: in front for the system again, AppKit inactive
    after(0.6);                                 // 11.15 s after the request, 10.15 s after the first look
    check(stats().pending && retries() == 4,
          "ten seconds after the first look the period goes on: they count once more from the look that "
          "found the front free");
    after(7.7);                                 // 18.85 s after the request, 9.85 s after the front came free
    check(stats().pending && retries() == 5 && dockResets() == 1,
          "the period still runs 9.85 s after the look that found the front free");
    after(0.2);                                 // 10.05 s after the front came free
    check(!stats().pending && retries() == 5 && fallback() && host.hasFocus(),
          "and ends ten seconds after that look, the system's state still holding the focus");
    // The front also comes free when the unrelated application quits, which leaves nobody in
    // front, and when the user goes to the client's parent: both count alike. Each of these
    // starts follows one that has used its second count, so they also show that this count is
    // once per period and not once per process: a new first request brings it back.
    {
        struct Free { int front; const char* goesOn; const char* reset; };
        const Free frees[] = {
            { 0, "nobody in front 9.5 s after the first look: the requests go on past the ten seconds of the "
                 "first look, in a start that follows one which used its second count",
                 "nobody in front: the Dock reset follows a second after the front came free" },
            { script, "the parent in front 9.5 s after the first look: the requests go on past the ten seconds "
                      "of the first look, again after a start that used its second count",
                      "the parent in front: the Dock reset follows a second after the front came free" },
        };
        for (const Free& free : frees)
        {
            start(editor);
            after(1.0);                         // the first look: ten seconds from here
            sys.frontPid = free.front;
            after(9.5);                         // 10.5 s after the request: the front is free
            after(0.6);                         // 10.1 s after the first look
            check(stats().pending && retries() == 2 && dockResets() == 0, free.goesOn);
            after(0.6);                         // 1.2 s after the front came free
            check(stats().pending && retries() == 2 && dockResets() == 1, free.reset);
        }
    }

    // The count for a front that comes free is made once per period. A front that goes back to
    // the unrelated application and returns to this process does not start the ten seconds a
    // third time. While the unrelated application is in front again, the wait lasts until ten
    // seconds after the front first came free, here 3 s longer than the ten seconds of the first
    // look.
    start(editor);
    after(1.0);                                 // the first look: ten seconds from here
    sys.frontPid = self;
    after(3.0);                                 // free 4.0 s after the request: ten seconds once more from here
    sys.frontPid = editor;
    after(0.1);
    after(7.4);                                 // 11.5 s after the request: 10.5 s after the first look,
                                                // 7.5 s after the front came free
    check(stats().pending && retries() == 1 && dockResets() == 0,
          "the unrelated application in front again: the wait goes on past the ten seconds of the first look, "
          "until ten seconds after the front first came free");
    sys.frontPid = self;
    after(2.1);                                 // 9.6 s after the front first came free
    check(stats().pending && retries() == 2, "this process in front a second time: the request is repeated");
    after(0.5);                                 // 10.1 s after the front first came free
    check(!stats().pending && retries() == 2 && dockResets() == 0,
          "but the ten seconds are not counted a third time: the period ends");

    // The Dock reset puts the Dock in front until this process asks again, so no period ends
    // between the two. Here a launcher's period is first looked at 0.2 s after the request and
    // then not for 9.8 s (a pump that stalls): the reset comes 9.8 s into the ten seconds.
    start(terminal);
    sys.frontPid = self;
    after(0.2);
    after(9.8);
    check(fallback() && dockResets() == 1 && retries() == 0,
          "a look 9.8 s after the first one: the system's state is used and the activation reset at once");
    sys.frontPid = dock;
    after(0.25);                                // 10.05 s after the first look
    check(stats().pending && retries() == 0 && host.hasFocus(),
          "ten seconds over, the request after the reset still owed: the period goes on");
    after(0.1);
    check(stats().pending && retries() == 1, "the owed request is made");
    sys.frontPid = self;                        // refused: this process in front again, AppKit inactive
    after(0.3);
    check(stats().pending && retries() == 1, "it has half a second for its answer, and no request follows it");
    after(0.3);
    check(!stats().pending && retries() == 1 && fallback() && host.hasFocus(),
          "then the period ends, with the system's state still holding the focus");
    // A pump that comes late after the reset still makes that request, and sees it granted.
    start(terminal);
    sys.frontPid = self;
    after(0.2);
    after(9.8);
    sys.frontPid = dock;
    after(2.5);                                 // 12.3 s after the first look
    check(stats().pending && retries() == 1 && dockResets() == 1,
          "no pump for 2.5 s after the Dock reset: the owed request is made by the next one");
    appKitActive();
    after(0.05);
    check(!stats().pending && !fallback() && host.hasFocus(), "and granted: active without a click");
    // The first request after the reset settles that debt, also when a click makes it.
    start(terminal);
    sys.frontPid = self;
    after(0.2);
    after(9.8);
    sys.frontPid = dock;
    apply(0.1);
    click();
    check(stats().pending && retries() == 1, "a click with the Dock in front after the reset: its request is the owed one");
    after(0.55);                                // 10.45 s after the first look
    check(!stats().pending && retries() == 1, "half a second later the period ends without another request");

    // A click into the inactive game window is the user choosing the game: the requests start,
    // also against the unrelated application, with one Dock reset for the period.
    start(editor);
    after(2.0);
    click();
    check(stats().pending && retries() == 1, "a click into the inactive game window requests at once");
    after(0.6);
    check(retries() == 2 && dockResets() == 0, "and the requests go on against the application clicked away from");
    after(0.6);
    check(dockResets() == 1, "with the Dock reset a second after the click");
    sys.frontPid = dock;
    apply(0.4);
    click();                                    // while the reset has the Dock in front
    sys.frontPid = editor;
    after(0.6);
    apply(1.5);
    click();
    pump();
    check(retries() == 5 && dockResets() == 1 && stats().pending,
          "further clicks request again and do not repeat the Dock reset");
    after(7.0);
    check(!stats().pending, "ten seconds after the first click the period ends");
    click();
    after(1.2);
    check(stats().pending && dockResets() == 2,
          "a click that finds no period running starts one, with its own Dock reset");
    sys.frontPid = dock;
    after(10.0);
    check(stats().pending && retries() == 7,
          "no pump for ten seconds after that reset: the request it owes is still made");
    // A period that a click starts counts its ten seconds from the click. Clicks arrive in a pump,
    // after the engine's initialisation, so the look that follows does not count them again.
    finish();
    sys.frontPid = editor;
    after(1.0);
    base = stats();
    click();
    after(0.3);
    after(0.9);                                 // 1.2 s after the click: the Dock reset
    sys.frontPid = dock;
    after(0.35);
    sys.frontPid = editor;
    after(8.35);                                // 9.9 s after the click
    check(stats().pending && retries() == 3 && dockResets() == 1,
          "a period that a click starts still runs 9.9 s after the click");
    after(0.15);                                // 10.05 s after the click, 9.75 s after the look that followed it
    check(!stats().pending && retries() == 3,
          "and ends ten seconds after the click, not ten seconds after the look that followed it");

    // A click that ends a wait behind the bystander starts the ten seconds as well.
    start(editor);
    after(8.0);
    click();
    after(4.0);                                 // 12.0 s after the first request, 4.0 s after the click
    check(stats().pending && retries() == 1 && dockResets() == 1,
          "a click after eight seconds of waiting: the period runs for ten seconds from the click");

    // The click arrives when the system already calls this process frontmost, and the unrelated
    // application is in front again afterwards: the click chose the game over it all the same.
    start(editor);
    after(2.0);
    sys.frontPid = self;
    apply(0.1);
    click();
    sys.frontPid = editor;
    after(0.6);
    check(stats().pending && retries() == 2,
          "a click with this process frontmost: the application left behind may lose the front too");
    // The same when the click arrives while the period's own Dock reset has the Dock in front:
    // the Dock is not what the user clicked away from, the unrelated application that the period
    // waited behind is.
    start(editor);
    after(1.0);
    sys.frontPid = self;
    after(0.6);
    after(0.6);
    after(0.6);                                 // in front for 1.2 s: the fallback and the Dock reset
    sys.frontPid = dock;
    apply(0.1);
    click();
    sys.frontPid = editor;
    after(0.6);
    check(stats().pending && retries() == 4 && dockResets() == 1,
          "a click while the reset has the Dock in front: the unrelated application of the first request is "
          "the one clicked away from, and may lose the front");

    // Every application the user leaves with a click into the inactive game window may lose the
    // front until the period ends. A later click made with another application in front adds
    // that one. It does not replace the application of the click that started the period or
    // ended its wait, against which the requests would otherwise stop early.
    start(editor);
    after(2.0);
    click();                                    // ends the wait: the editor may lose the front
    sys.frontPid = browser;
    apply(0.3);
    click();                                    // the user went to the browser and clicks again
    pump();
    check(stats().pending && retries() == 2,
          "a later click with another application in front: the period goes on against that one");
    sys.frontPid = editor;
    after(0.6);
    check(stats().pending && retries() == 3,
          "the application of the first click may still lose the front after a later click with another one in front");
    sys.frontPid = browser;
    after(0.6);                                 // 1.5 s after the first click: the Dock reset is due
    check(stats().pending && retries() == 3 && dockResets() == 1,
          "and so may the application of the later click: the Dock reset is made with it in front");
    sys.frontPid = terminal;                    // no ancestor's turn: an unrelated application was in front at the start
    after(0.2);
    check(!stats().pending, "an application the user went to without a click still ends the period");
    // The applications clicked away from belong to one period. A click that finds no period
    // running starts a new one against the application in front at that click alone: the editor
    // and the browser, clicked away from in the period before, are again applications the user
    // can go to, and that ends the requests.
    apply(1.0);
    click();                                    // no period running; Terminal is in front
    pump();
    check(stats().pending && retries() == 4,
          "a click that finds no period running starts one against the application in front at the click");
    sys.frontPid = editor;
    after(0.6);
    check(!stats().pending && retries() == 4,
          "an application clicked away from in the period before is not this period's: the user going to it "
          "ends the requests");

    // The same decision with the lookup the client makes: getppid() and the kernel's process
    // table instead of the simulated chain that every scenario above uses. The application in
    // front is this test's real parent, then that parent's parent, then a process outside the
    // chain. launchd (pid 1) is no application and nobody's launcher: where the test's parent, or
    // its parent's parent, is launchd (the test was started detached from a shell that has
    // exited), a process outside the chain stands in, and the host must take it for a bystander.
    {
        sys.realProcessTable = true;
        const int outside = 0x7FFFFFF0;         // no process has this pid
        const int realParent = static_cast<int>(getppid());
        const int realGrandparent = realParent > 1 ? static_cast<int>(macos::parentPidOf(realParent)) : -1;
        const int launcher = realParent > 1 ? 1 : 0;      // expected first requests that ignore other applications
        const int further = realGrandparent > 1 ? 1 : 0;
        start(realParent > 1 ? realParent : outside);
        check(stats().pending && requests() == 1 && ignoringOthers() == launcher,
              "the real process table, this test's parent in front: the launcher (a bystander stands in when "
              "launchd is the parent)");
        after(0.6);
        after(0.6);
        check(retries() == launcher && dockResets() == launcher,
              "the real process table: repeated requests and the Dock reset against the real parent, none against "
              "a bystander");
        start(realGrandparent > 1 ? realGrandparent : outside);
        check(stats().pending && requests() == 1 && ignoringOthers() == further,
              "the real process table, the parent's parent in front: an ancestor found through sysctl is the "
              "launcher (a bystander stands in when the chain has ended at launchd)");
        start(outside);
        check(stats().pending && requests() == 1 && ignoringOthers() == 0,
              "the real process table, an application outside the chain in front: a bystander");
        sys.realProcessTable = false;
    }

    // A game that restarts itself (the player activates a mod): the first instance starts the new
    // one and quits. When it was the active application at that moment it says so in the new
    // instance's environment (RestartInFront.h), and the new instance is entitled to the front:
    // the application that the system put in front in the first instance's place counts as the
    // launcher. Here the first instance has exited: launchd (pid 1) is this process's parent.
    const char* const marker = macos::kRestartedInFrontVariable;
    const int startParent = sys.parentPid, startAncestor = sys.ancestors[0];
    sys.parentPid = 1;
    sys.ancestors[0] = 0;
    // The host read the environment at this process's first activation request, at the top of
    // this test: a marker that appears afterwards changes nothing.
    setenv(marker, "1", 1);
    start(editor);
    check(stats().pending && requests() == 1 && ignoringOthers() == 0,
          "the restart marker set after this process's first request is not read: the unrelated application is a bystander");
    // As a process that has just started: the marker is read at the first request.
    Hooks::forgetRestartMarkerForTest(host);
    start(editor);
    check(stats().pending && requests() == 1 && ignoringOthers() == 1 && retries() == 0,
          "restarted in front, an unrelated application in front: the first request ignores other applications, "
          "as for a launcher");
    after(0.6);
    after(0.6);
    check(retries() == 1 && dockResets() == 1,
          "restarted in front: the request is repeated and the activation reset through the Dock, as for a launcher");
    sys.frontPid = dock;
    after(0.35);
    sys.frontPid = editor;                      // refused: the unrelated application is in front again
    after(0.6);
    check(stats().pending && retries() == 3, "restarted in front: the requests go on against that application");
    sys.frontPid = browser;
    after(0.2);
    check(!stats().pending, "restarted in front: an application the user goes to still ends the requests");
    // Read once: a game window that the same process shows again starts like any other.
    start(editor);
    after(0.6);
    after(0.6);
    after(1.0);
    check(stats().pending && requests() == 1 && ignoringOthers() == 0 && retries() == 0 && dockResets() == 0,
          "the marker is read once: at the next first request of the same process the unrelated application is a bystander");
    // Only the value 1 is the marker.
    setenv(marker, "0", 1);
    Hooks::forgetRestartMarkerForTest(host);
    start(editor);
    check(stats().pending && requests() == 1 && ignoringOthers() == 0,
          "the variable with the value 0: the unrelated application is a bystander");
    // Without the marker: a game that restarted in the background, and every other start whose
    // parent has exited.
    unsetenv(marker);
    Hooks::forgetRestartMarkerForTest(host);
    start(editor);
    after(0.6);
    after(0.6);
    after(1.0);
    check(stats().pending && requests() == 1 && ignoringOthers() == 0 && retries() == 0 && dockResets() == 0,
          "restarted without the marker, an unrelated application in front: a bystander with its one cooperative request");
    // The marker belongs to this process's first showing of its game window, also when that
    // showing finds the application active already and has nothing to ask: a window shown again
    // later starts like any other.
    setenv(marker, "1", 1);
    Hooks::forgetRestartMarkerForTest(host);
    finish();
    appKitActive();
    after(1.0);
    base = stats();
    Hooks::beginActivationForTest(host);
    check(!stats().pending && requests() == 0, "restarted in front and active at the first showing: nothing is asked");
    start(editor);
    check(stats().pending && requests() == 1 && ignoringOthers() == 0,
          "that showing used the marker up: at the next one the unrelated application is a bystander");

    // The new instance can make its first request while the first one is still running and in
    // front. The first instance is then the parent, a launcher like any other. Once it has
    // exited, the application found in front took its place, and for a game restarted in front
    // that application counts as the launcher.
    const int firstInstance = 4242;
    const auto restartedBy = [&](bool inFront)
    {
        if (inFront)
            setenv(marker, "1", 1);
        else
            unsetenv(marker);
        Hooks::forgetRestartMarkerForTest(host);
        sys.parentPid = firstInstance;
        start(firstInstance);
    };
    restartedBy(true);
    check(stats().pending && ignoringOthers() == 1,
          "restarted in front, the first instance still in front: it is the parent, a launcher");
    sys.parentPid = 1;                          // the first instance has exited
    sys.frontPid = browser;                     // and the system put the browser in front
    after(6.5);
    check(stats().pending && dockResets() == 1,
          "restarted in front: the application in front once the first instance has exited counts as the launcher");
    sys.frontPid = dock;
    after(0.35);
    sys.frontPid = browser;
    after(0.6);
    check(stats().pending && retries() == 2, "and loses the front to the repeated requests");
    sys.frontPid = editor;
    after(0.2);
    check(!stats().pending,
          "one application follows the first instance: another one the user goes to ends the requests");
    // Between the first instance's exit and the system naming its successor nobody may be in
    // front for a moment, and the system may go on naming the first instance for a moment after
    // it has exited. Neither look settles who follows: the application that comes to the front
    // next does, and it counts as the launcher.
    {
        struct Between { int front; const char* what; };
        const Between betweens[] = {
            { 0, "restarted in front, nobody in front right after the first instance has exited: the application "
                 "that follows counts as the launcher" },
            { firstInstance, "restarted in front, the first instance still named in front after it has exited: the "
                             "application that follows counts as the launcher" },
        };
        for (const Between& between : betweens)
        {
            restartedBy(true);
            sys.parentPid = 1;                  // the first instance has exited
            sys.frontPid = between.front;
            after(0.2);
            sys.frontPid = browser;
            after(6.5);
            check(stats().pending && retries() == 0 && dockResets() == 1, between.what);
        }
    }
    // The same restart from the background: no marker, and the period ends.
    restartedBy(false);
    sys.parentPid = 1;
    sys.frontPid = browser;
    after(6.5);
    check(!stats().pending && retries() == 0 && dockResets() == 0,
          "restarted in the background: the application in front after the first instance ends the requests");
    // The first instance still runs while another application is in front: the user went there.
    restartedBy(true);
    sys.frontPid = browser;
    after(6.5);
    check(!stats().pending && retries() == 0 && dockResets() == 0,
          "restarted in front, the first instance still running: an application the user went to ends the requests");
    // That period ended with the question of who follows the first instance still open, since
    // the first instance never exited in it. The question belongs to that period. A later first
    // request of the same process, and a click that starts a period, begin without it: an
    // application that comes to the front then is one the user went to.
    sys.parentPid = 1;
    start(0);
    sys.frontPid = browser;
    after(0.6);
    check(!stats().pending && retries() == 0 && dockResets() == 0,
          "a later first request without the marker: the application that comes to the front is not taken for "
          "the first instance's successor");
    restartedBy(true);
    sys.frontPid = browser;
    after(6.5);                                 // ends again with that question open
    sys.parentPid = 1;
    sys.frontPid = editor;
    apply(1.0);
    click();                                    // no period running: a new one, against the editor
    sys.frontPid = browser;
    after(0.6);
    check(!stats().pending && retries() == 1 && dockResets() == 0,
          "a click that starts a period after it: the application the user then goes to is not taken for the "
          "first instance's successor either");
    // The system puts this process itself in front: no other application follows the first one.
    restartedBy(true);
    sys.parentPid = 1;
    sys.frontPid = self;
    after(6.5);
    check(stats().pending && fallback() && dockResets() == 1,
          "restarted in front, this process in front after the first instance: repaired like any start");
    check(macos::clientHoldsFront(),
          "while the host uses the system's state the application counts as active: a restart from here is in front");
    sys.frontPid = browser;
    after(0.2);
    check(!stats().pending, "and an application the user goes to afterwards ends the requests");
    // Nobody is in front at the first request: the system has not named the first instance's
    // successor yet.
    setenv(marker, "1", 1);
    Hooks::forgetRestartMarkerForTest(host);
    sys.parentPid = 1;
    start(0);
    sys.frontPid = browser;
    after(6.5);
    check(stats().pending && dockResets() == 1,
          "restarted in front, nobody in front at the first request: the application in front at the next look "
          "counts as the launcher");
    sys.frontPid = editor;
    after(0.2);
    check(!stats().pending, "and another application after that one ends the requests");
    // The Dock that the period's own reset put in front is not the first instance's successor
    // either: the application in front after it is.
    setenv(marker, "1", 1);
    Hooks::forgetRestartMarkerForTest(host);
    sys.parentPid = 1;
    start(0);
    after(0.6);
    after(0.6);                                 // nobody in front for 1.2 s: the Dock reset
    sys.frontPid = dock;
    after(0.35);                                // the request that reset owes, with the Dock in front
    sys.frontPid = browser;
    after(0.6);
    check(stats().pending && retries() == 3 && dockResets() == 1,
          "restarted in front, the Dock in front by the period's own reset: it is not the first instance's "
          "successor, the application in front after it is");
    sys.frontPid = editor;
    after(0.2);
    check(!stats().pending, "and an application after that one ends the requests");
    unsetenv(marker);
    Hooks::forgetRestartMarkerForTest(host);
    sys.parentPid = startParent;
    sys.ancestors[0] = startAncestor;

    // End the simulation with no period running, and focus as the other tests force it.
    finish();
    check(!stats().pending && !fallback() && !host.hasFocus(), "the last period is over");
    Hooks::setSystemFocusForTest(host, 0);
    setFocus(true);
    settle();
}

// RestartInFront.h: what a game that restarts itself hands to the new instance. The first instance
// builds the new one's environment (MacSystem.cpp, relaunchExecutable) and decides from the
// host's focus state whether it holds the front. What the host makes of the marker is checked
// with the launch activation above.
void testRestartInFront()
{
    begin("restart in front: the marker in the new instance's environment");
    using namespace macos;
    typedef std::vector<std::string> Env;
    check(std::string(kRestartedInFrontVariable) == "BF2_RESTARTED_IN_FRONT", "the variable is BF2_RESTARTED_IN_FRONT");
    check(restartedInFront("1"), "the value 1 marks a restart in front");
    check(!restartedInFront(nullptr) && !restartedInFront("") && !restartedInFront("0") &&
          !restartedInFront("11") && !restartedInFront("true"),
          "no variable and any other value do not");
    // A first instance that a person started: no marker in its environment.
    const char* const plain[] = { "HOME=/Users/player", "BF2_HOST_TRACE=1", nullptr };
    check(restartEnvironment(plain, true) == Env({ "HOME=/Users/player", "BF2_HOST_TRACE=1", "BF2_RESTARTED_IN_FRONT=1" }),
          "restarted in front: the environment is passed on with the marker added");
    check(restartEnvironment(plain, false) == Env({ "HOME=/Users/player", "BF2_HOST_TRACE=1" }),
          "restarted in the background: the environment is passed on as it is");
    // A second generation: this instance was itself restarted in front and carries the marker.
    const char* const marked[] = { "BF2_RESTARTED_IN_FRONT=1", "HOME=/Users/player", "BF2_RESTARTED_IN_FRONT_X=1",
                                   nullptr };
    check(restartEnvironment(marked, false) == Env({ "HOME=/Users/player", "BF2_RESTARTED_IN_FRONT_X=1" }),
          "a marked instance that restarts in the background: its marker is not passed on, other variables are");
    check(restartEnvironment(marked, true) ==
              Env({ "HOME=/Users/player", "BF2_RESTARTED_IN_FRONT_X=1", "BF2_RESTARTED_IN_FRONT=1" }),
          "a marked instance that restarts in front: the marker is passed on once");
    check(restartEnvironment(nullptr, true) == Env({ "BF2_RESTARTED_IN_FRONT=1" }) &&
          restartEnvironment(nullptr, false).empty(),
          "no environment at all: the marker alone, or nothing");
    // Whether this process holds the front follows the application's state as the host reports
    // it to the engine.
    ClientHost& host = ClientHost::instance();
    CocoaHostTestHooks::overrideFocusForTest(host, true, true);
    check(clientHoldsFront(), "the application active: this process holds the front");
    CocoaHostTestHooks::overrideFocusForTest(host, false, true);
    check(clientHoldsFront(), "the application active while another window has the keyboard: it still does");
    CocoaHostTestHooks::overrideFocusForTest(host, false, false);
    check(!clientHoldsFront(), "the application inactive: it does not");
    setFocus(true);
    settle();
}

// NativeFullscreen.h: the game window's own full-screen Space. The engine's request is what the
// host reports at once; the window follows with one AppKit transition at a time, waits for the
// application's activation, and stays windowed after the user left the Space.
void testNativeFullscreen()
{
    begin("native full screen: one transition at a time, after the activation, the user's exit kept");
    typedef macos::NativeFullscreen F;
    {
        F f;
        check(!f.wanted() && f.windowed() && !f.inSpace() && !f.transitioning(), "a new window is windowed");
        check(f.request(true, true) == F::Enter, "active: a full-screen request enters the Space at once");
        check(f.wanted() && !f.windowed() && f.transitioning() && f.ownTransition(),
              "reported full screen at once, the window on its way");
        f.willEnter();
        check(f.ownTransition(), "AppKit's will-enter for the host's own toggle stays the host's");
        check(f.didEnter() == F::None && f.inSpace() && !f.transitioning(), "in the Space, nothing more to do");
        check(f.request(false, true) == F::Exit && !f.wanted(), "windowed request: leave the Space");
        f.willExit();
        check(f.didExit() == F::None && f.windowed() && !f.userLeft(), "windowed again, by the host's own exit");
    }
    {
        F f;
        check(f.request(true, false) == F::None && f.waiting() && f.wanted() && f.windowed(),
              "inactive: the request waits, the window stays windowed, full screen is reported");
        check(f.activated() == F::Enter && !f.waiting(), "the activation enters the Space");
        check(f.activated() == F::None, "a second activation during the transition starts nothing");
        check(f.didEnter() == F::None && f.inSpace(), "in the Space");
        check(f.activated() == F::None, "activations in the Space start nothing");
    }
    {
        F f;
        f.request(true, false);
        check(f.request(false, false) == F::None && !f.waiting() && f.windowed(),
              "a windowed request cancels a waiting one without a transition");
        check(f.activated() == F::None, "and the activation then starts nothing");
    }
    {
        F f;
        f.request(true, true);
        check(f.request(false, true) == F::None && !f.wanted(), "windowed during the enter: nothing starts now");
        check(f.didEnter() == F::Exit, "the enter's end leaves the Space again");
        check(f.request(true, true) == F::None && f.wanted(), "full screen during that exit: nothing starts now");
        check(f.didExit() == F::Enter && !f.userLeft(), "the exit's end enters again; the host's own exit, not the user's");
        check(f.didEnter() == F::None && f.inSpace() && f.wanted(), "in the Space as last requested");
    }
    {
        F f;
        f.request(true, true);
        f.didEnter();
        f.willExit();                           // the title bar's button or the system shortcut
        check(!f.ownTransition() && f.transitioning(), "an exit the host did not start is the user's");
        check(f.didExit() == F::None && f.userLeft() && !f.wanted() && f.windowed(),
              "after the user's exit the host stays windowed and reports it");
        check(f.activated() == F::None, "an activation does not take the user back into the Space");
        check(f.request(true, true) == F::Enter && !f.userLeft(), "the engine's next request enters again");
    }
    {
        F f;
        f.request(true, true);
        f.didEnter();
        f.willExit();
        check(f.request(true, true) == F::None, "a full-screen request during the user's exit waits for its end");
        check(f.didExit() == F::Enter && !f.userLeft() && f.wanted(), "and then overrides it: back into the Space");
    }
    {
        F f;
        f.request(true, true);
        f.didFail();
        check(f.windowed() && f.wanted(), "a refused enter leaves the window windowed");
        F g;
        g.request(true, true);
        g.didFail();
        check(g.activated() == F::Enter, "the next activation tries again");
        F h;
        h.request(true, true);
        h.didFail();
        check(h.request(false, true) == F::None && h.request(true, true) == F::Enter,
              "a new request tries again");
    }
}

// CursorScale.h: points on screen per cursor pixel. The host hands over the game view's height in
// points and the back buffer's size in pixels (cursorPointsPerPixel). The function works out the
// points one back-buffer pixel covers (the view's height over the back buffer's height) and
// MenuViewport::bitmapScale, and keeps their product between 0.5 and 4 points
// (cursorPointsWithinBounds): the cursor is bitmapScale back-buffer pixels per cursor pixel
// wherever that is within those bounds, and both bounds apply to the product.
void testCursorScale()
{
    begin("cursor size: bitmapScale back-buffer pixels per cursor pixel, within 0.5 to 4 points");
    using macos::cursorBitmapScale;
    using macos::cursorPointsPerPixel;
    using macos::cursorPointsWithinBounds;
    const auto about = [](double a, double b) { return std::fabs(a - b) < 1e-9; };
    // The rule from the values the host holds, as customCursor calls it.
    check(cursorPointsPerPixel(900.0, 2560, 1600) == 1.125,
          "a 2560x1600 back buffer shown 900 points high: 1.125 points per cursor pixel (the approved size)");
    check(about(cursorPointsPerPixel(776.0, 2560, 1600), 0.97),
          "a 2560x1600 back buffer shown 776 points high (a window shrunk to fit the screen): 0.97 points, not 1.0");
    check(cursorPointsPerPixel(600.0, 800, 600) == 1.0 && cursorPointsPerPixel(900.0, 1440, 900) == 1.0,
          "800x600 and 1440x900 at one point per pixel: 1 point, retail's size");
    check(cursorPointsPerPixel(832.0, 2560, 1664) == 1.0,
          "a 2560x1664 back buffer shown 832 points high: 2 back-buffer pixels, 1 point");
    check(cursorPointsPerPixel(956.0, 2940, 1912) == 1.5,
          "a 2940x1912 back buffer shown 956 points high: 3 back-buffer pixels, 1.5 points");
    check(cursorBitmapScale(800, 600) == 1 && cursorBitmapScale(1440, 900) == 1 && cursorBitmapScale(2560, 1600) == 2 &&
          cursorBitmapScale(2940, 1912) == 3,
          "the factor is MenuViewport::bitmapScale: 1 at 600 and 900 lines, 2 at 1600, 3 at 1912");
    check(cursorBitmapScale(1200, 1600) == 1 && cursorPointsPerPixel(1600.0, 1200, 1600) == 1.0 &&
          cursorBitmapScale(1600, 1200) == 2 && cursorPointsPerPixel(1200.0, 1600, 1200) == 2.0,
          "the factor follows the fitted 4:3 viewport, so width and height are not interchangeable "
          "(1200x1600: factor 1; 1600x1200: factor 2)");
    check(cursorPointsPerPixel(200.0, 2560, 1600) == 0.5 && cursorPointsPerPixel(2880.0, 800, 600) == 4.0,
          "the bounds: 0.25 points is raised to 0.5 (2560x1600 shown 200 points high), 4.8 is cut to 4 "
          "(800x600 shown 2880 points high)");
    check(cursorPointsPerPixel(600.0, 0, 600) == 1.0, "a back buffer without width has the factor 1");
    check(cursorPointsPerPixel(0.0, 2560, 1600) == 0.0 && cursorPointsPerPixel(-1.0, 2560, 1600) == 0.0 &&
          cursorPointsPerPixel(900.0, 2560, 0) == 0.0,
          "a view or a back buffer without height has no cursor size: 0, for which the host shows the arrow");
    // The same rule in its two derived values: points per back-buffer pixel and the factor.
    check(cursorPointsWithinBounds(0.5625, 2) == 1.125,
          "2560x1600 full screen on a 1440x900-point desktop, factor 2: 1.125 points (the approved size)");
    check(about(cursorPointsWithinBounds(0.43, 3), 1.29),
          "a full-screen mode above the desktop's backing size (0.43, factor 3): 1.29 points, not 1.5");
    check(about(cursorPointsWithinBounds(0.485, 2), 0.97),
          "a 2560x1600 window shrunk to fit the screen (0.485, factor 2): 0.97 points, not 1.0");
    check(cursorPointsWithinBounds(0.2, 1) == 0.5, "0.2 points per back-buffer pixel, factor 1: the 0.5-point floor");
    check(cursorPointsWithinBounds(0.2, 2) == 0.5 && about(cursorPointsWithinBounds(0.3, 2), 0.6),
          "the floor applies after the factor (0.2 x 2 is raised to 0.5, 0.3 x 2 stays 0.6)");
    check(cursorPointsWithinBounds(1.0, 1) == 1.0 && cursorPointsWithinBounds(1.5, 1) == 1.5,
          "between the bounds the product stands (1.0 and 1.5 points at factor 1)");
    check(cursorPointsWithinBounds(0.5, 3) == 1.5, "2940x1912 full screen on a 1470x956-point desktop, factor 3: 1.5 points");
    check(cursorPointsWithinBounds(5.0, 1) == 4.0 && cursorPointsWithinBounds(3.0, 2) == 4.0 &&
          cursorPointsWithinBounds(4.0, 1) == 4.0,
          "never more than 4 points per cursor pixel");
    check(cursorPointsWithinBounds(1.0, 0) == 1.0, "a factor of 0 counts as 1");
}

} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, 0, _IONBF, 0);
    @autoreleasepool
    {
        ClientHost& host = ClientHost::instance();
        CocoaHostTestHooks::enableHeadlessForTest(host);
        CocoaHostTestHooks::overrideFocusForTest(host, true, true);
        if (argc == 2 && std::string(argv[1]) == "--async-enter-only")
        {
            testAsyncEnter();
            std::printf("[result] %d checks, %d failures\n", g_checks, g_failures);
            return g_failures ? 1 : 0;
        }
        MouseLog log;
        host.addListener(&log);

        io::installControlMapFixture();
        g_manager = static_cast<io::IInputDeviceManager*>(
            io::NativeInputDeviceManager::create(io::IID_IInputDeviceManager, 0));
        io::inputDeviceManager = g_manager;
        unsigned int mask = 1087;               // BF2Engine::init: inputManager->init(1087)
        const bool all = g_manager->initDevices(&mask);
        std::printf("[input] initDevices(1087) all=%d opened=0x%X\n", all, mask);
        check(!all && (mask & 3u) == 3u, "keyboard and mouse open; the Falcon never does");
        check(!(mask & io::IDFFalcon), "no Falcon on macOS");
        engineFocus(true);
        mouse()->activate();                    // in-game: MainMenuLogic::deactivate activates the mouse
        check(keyboard()->isActive() && mouse()->isActive(), "keyboard and mouse acquired with focus");

        testControlsConKeys();
        testKeyRepeat();
        testIsoSwap();
        testSubFrameSampling();
        testFocusLoss();
        testMouseBasics();
        testWheelNotches();
        testPreciseScroll();
        testMomentumScroll();
        testCapturedMouse(log);
        testRawMouse(log);
        testRawMouseThreads(log);
        testAsyncEnter();

        testHostBoundary();
        testFocusRepair();
        testProcessAncestry();
        testLaunchActivation();
        testRestartInFront();
        testCursorScale();
        testNativeFullscreen();

        host.removeListener(&log);
        g_manager->release();
    }
    std::printf("[result] %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
