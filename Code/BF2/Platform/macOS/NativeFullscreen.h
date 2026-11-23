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
//   works in another application) that would take the screen from what the user works in, which
//   the launch activation never does either (CocoaHost.mm, beginActivation). The launch activation
//   brings the game forward when it may, and the window goes full screen with that activation.
// - One transition at a time. A request made during a transition is applied when it ends.
// - A transition the host did not start is the user's: macOS lets the user leave a full-screen
//   Space through the window's title bar (revealed at the top of the screen) or the system's
//   full-screen shortcut. The host then stays windowed and reports not full screen until the
//   engine asks again; it does not take the user back into the Space.
// - A transition AppKit refused is not retried until the next request or the next activation.
#ifndef PLATFORM_MACOS_NATIVEFULLSCREEN_H
#define PLATFORM_MACOS_NATIVEFULLSCREEN_H

namespace dice { namespace hfe { namespace macos {

class NativeFullscreen
{
public:
    enum Action { None, Enter, Exit };  // the transition to start now: toggleFullScreen, or nothing

    // ClientHost::setFullscreen. `appActive`: the application is active now.
    Action request(bool fullscreen, bool appActive)
    {
        m_wanted = fullscreen;
        m_userLeft = false;
        m_failed = false;
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
        return next();
    }

    // windowWillEnterFullScreen / windowWillExitFullScreen. A transition that no answer of this
    // class started is the user's (or the system's).
    void willEnter() { if (m_transition == None) { m_transition = Enter; m_own = false; } }
    void willExit()  { if (m_transition == None) { m_transition = Exit;  m_own = false; } }

    // windowDidEnterFullScreen.
    Action didEnter()
    {
        m_transition = None;
        m_inSpace = true;
        return next();
    }

    // windowDidExitFullScreen. The user left the Space while the engine wants full screen: the
    // host stays windowed (wanted() false) until the engine asks again.
    Action didExit()
    {
        const bool own = m_own;
        m_transition = None;
        m_inSpace = false;
        if (!own && m_wanted)
        {
            m_wanted = false;
            m_userLeft = true;
        }
        return next();
    }

    // windowDidFailToEnterFullScreen / windowDidFailToExitFullScreen: the window stays where it
    // was, and nothing is retried before the next request or activation.
    void didFail()
    {
        m_transition = None;
        m_failed = true;
    }

    bool wanted() const       { return m_wanted; }      // isFullscreen()
    bool inSpace() const      { return m_inSpace; }     // the window is in its full-screen Space
    bool transitioning() const { return m_transition != None; }
    bool windowed() const     { return !m_inSpace && m_transition == None; }  // shown as a window
    bool waiting() const      { return m_waiting; }     // a full-screen request waits for activation
    bool userLeft() const     { return m_userLeft; }    // the last exit was the user's
    bool ownTransition() const { return m_transition != None && m_own; }

private:
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
