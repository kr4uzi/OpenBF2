// [CHOSEN, not recovered] macOS port: the game window's native full screen.
//
// Retail is Windows-only: a full-screen start creates a WS_POPUP window and the renderer switches
// the display mode. The Cocoa host instead puts the game window into its own Space with AppKit's
// native full screen (-[NSWindow toggleFullScreen:]), the way macOS applications go full screen:
// the menu bar and the Dock hide and reappear at the screen edges, the Space can be left and
// re-entered with a swipe or Mission Control, and Cmd-Tab returns to it. The display mode is never
// changed; the drawable keeps the game resolution and Core Animation scales it to the screen
// (CocoaHost.mm, updateDrawable / updateMagnification).
//
// Native full screen is asynchronous. The window animates into its Space after the request, and
// AppKit reports the steps to the window delegate (windowWillEnterFullScreen, ...DidEnter...,
// ...WillExit..., ...DidExit..., ...DidFailToEnter/Exit...). A request that arrives during a
// transition cannot start another one. This class holds what the engine asked for and where the
// window is, and answers which transition to start, if any, at each step. It is plain C++ without
// AppKit: CocoaHost.mm starts the transitions it answers, and a headless check pins the rules
// (Code/Tests/macos_input, testNativeFullscreen).
//
// The rules:
// - What the engine asked for (ClientHost::setFullscreen) is what isFullscreen() reports, at once,
//   as the borderless window it replaces did. The window follows when it can.
// - A full-screen request made while the application is inactive waits for its activation. Going
//   into a new Space switches the user to it; for a client started in the background (the user
//   works in another application) that would take the screen from what the user works in. The
//   application is made active at launch, before the window exists (CocoaHost.mm, activateAtLaunch),
//   so a player's start never waits here; the wait remains for a client started behind another app.
// - One transition at a time. A request made during a transition is applied when it ends.
// - A transition the host did not start is the user's: macOS lets the user leave a full-screen
//   Space through the window's title bar (revealed at the top of the screen) or the system's
//   full-screen shortcut. The host then stays windowed and reports not full screen until the
//   engine asks again; it does not take the user back into the Space.
// - An entry AppKit refused (it does during another animation or a gesture) is tried again while
//   the application is active: three attempts in all, a pause between them (CocoaHost.mm). When
//   the application is inactive at the refusal, or has become inactive before the host makes a
//   queued entry, the entry waits for the next activation instead: nobody is taken into the
//   Space from another application. AppKit reports a refused entry as
//   windowDidFailToEnterFullScreen, as windowDidExitFullScreen while the entry runs, or as both
//   in that order; the two are one refusal. A refused exit is not tried again before the next
//   request or activation.
#ifndef PLATFORM_MACOS_NATIVEFULLSCREEN_H
#define PLATFORM_MACOS_NATIVEFULLSCREEN_H

namespace dice { namespace hfe { namespace macos {

class NativeFullscreen
{
public:
    enum Action { None, Enter, Exit };  // the transition to start now: toggleFullScreen, or nothing

    // What became of an entry AppKit refused (enterRefused).
    enum Refusal
    {
        Duplicate,  // the second report of a refusal handled already
        Retry,      // the entry is under way again: start it
        Waits,      // the entry follows the inactive application's next activation
        GivenUp     // not tried again before the next request or activation
    };
    static const int kEnterAttempts = 3;

    // ClientHost::setFullscreen. `appActive`: the application is active now.
    Action request(bool fullscreen, bool appActive)
    {
        m_wanted = fullscreen;
        m_userLeft = false;
        m_failed = false;
        m_attempts = 0;
        m_waiting = fullscreen && !appActive && !m_inSpace;
        // A request during a transition the user started overrides the user's choice: its end
        // is then corrected towards the request (next).
        if (m_transition != None)
            m_own = true;
        return next();
    }

    // The application became active.
    Action activated()
    {
        m_waiting = false;
        m_failed = false;
        m_attempts = 0;
        return next();
    }

    // windowWillEnterFullScreen / windowWillExitFullScreen. A transition that no answer of this
    // class started is the user's (or the system's).
    void willEnter() { m_reportedAsExit = false; m_announced = true;
                       if (m_transition == None) { m_transition = Enter; m_own = false; } }
    void willExit()  { m_announced = true;
                       if (m_transition == None) { m_transition = Exit;  m_own = false; } }

    // windowDidEnterFullScreen.
    Action didEnter()
    {
        endTransition();
        m_inSpace = true;
        m_attempts = 0;
        m_reportedAsExit = false;
        return next();
    }

    // windowDidExitFullScreen when no entry runs (an exit during entry is enterRefused instead).
    // The user left the Space while the engine wants full screen: the
    // host stays windowed (wanted() false) until the engine asks again.
    Action didExit()
    {
        const bool own = m_own;
        endTransition();
        m_inSpace = false;
        m_attempts = 0;
        if (!own && m_wanted)
        {
            m_wanted = false;
            m_userLeft = true;
        }
        return next();
    }

    // AppKit reports a refusal as a failure callback, an exit during entry (asExit), or both in
    // that order. A failure following an exit, before the next toggle, is the same refusal.
    Refusal enterRefused(bool appActive, bool asExit)
    {
        if (!asExit && m_reportedAsExit)
        {
            m_reportedAsExit = false;
            return Duplicate;
        }
        m_reportedAsExit = asExit;
        endTransition();
        m_inSpace = false;
        if (!m_wanted)
            return GivenUp;
        if (!appActive || m_waiting)
        {
            m_waiting = true;
            return Waits;
        }
        if (!m_failed && ++m_attempts < kEnterAttempts)
        {
            m_transition = Enter;
            m_own = true;
            return Retry;
        }
        m_failed = true;
        return GivenUp;
    }
    // A refused exit leaves the window in its Space until the next request or activation.
    void exitRefused() { endTransition(); m_failed = true; }
    // The host makes its queued toggle: any refusal from here is a new one.
    void toggled() { m_reportedAsExit = false; }
    // The application became inactive before the host made the entry it had queued: the entry
    // waits for the next activation, as a request made while inactive does.
    void postpone() { if (m_transition == Enter) { endTransition(); m_waiting = true; } }
    // The host stopped waiting for a transition AppKit never announced.
    void abandon() { endTransition(); m_failed = true; }

    bool entering() const { return m_transition == Enter; }
    bool announced() const { return m_transition != None && m_announced; }

    bool wanted() const       { return m_wanted; }      // isFullscreen()
    bool inSpace() const      { return m_inSpace; }     // the window is in its full-screen Space
    bool transitioning() const { return m_transition != None; }
    bool windowed() const     { return !m_inSpace && m_transition == None; }  // shown as a window
    bool waiting() const      { return m_waiting; }     // a full-screen request waits for activation
    bool userLeft() const     { return m_userLeft; }    // the last exit was the user's
    bool ownTransition() const { return m_transition != None && m_own; }

private:
    void endTransition() { m_transition = None; m_announced = false; }
    bool m_announced = false;
    bool m_reportedAsExit = false;
    int m_attempts = 0;  // entries refused since the last request, activation or success

    Action next()
    {
        if (m_transition != None || m_failed)
            return None;
        if (m_wanted && !m_inSpace && !m_waiting)
        {
            m_transition = Enter;
            m_own = true;
            return Enter;
        }
        if (!m_wanted && m_inSpace)
        {
            m_transition = Exit;
            m_own = true;
            return Exit;
        }
        return None;
    }

    bool   m_wanted = false;
    bool   m_inSpace = false;
    bool   m_waiting = false;
    bool   m_failed = false;
    bool   m_userLeft = false;
    bool   m_own = false;           // the running transition was started by an answer of this class
    Action m_transition = None;     // Enter or Exit while one runs
};

} } }
#endif
