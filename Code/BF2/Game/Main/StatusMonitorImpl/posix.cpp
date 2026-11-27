// ncurses status monitor for POSIX images. The Windows monitor lives in its own
// translation unit. [INFERRED] source path; no binary filename attests this placement.

#include "../StatusMonitorImpl.h"

#if defined(__unix__) || defined(__linux__) || defined(BF2_PLATFORM_POSIX)

#include <curses.h>
#include <cstdlib>

#include "../../../Core/System.h"           // System::getWallClockTime, g_system (ds:0x8AC0D20)
#include "../../../IO/Console/Console.h"    // io::OldConsole::updateAsciiKey, io::mainConsole

namespace dice {
namespace hfe {

class StatusMonitorImpl_posix;              // defined below; the instance pointer names it first

// a file-scope pointer to the live instance, written by the constructor and read by the
// atexit hook. [INFERRED] name; the ELF has no symbol for it.
namespace {

StatusMonitorImpl_posix* g_instance = 0;

// [CHOSEN, not recovered] Exit hook restores terminal mode with endwin.
// Registration is attested; the original hook body remains unread.
void atexit_cleanup_statusmonitor()
{
    if (g_instance)
        endwin();
}

} // anonymous namespace

// ncurses status display with refresh counter, last update time, and window handle.
class StatusMonitorImpl_posix : public StatusMonitorImpl
{
public:
    StatusMonitorImpl_posix();
    virtual ~StatusMonitorImpl_posix();

    virtual bool shouldUpdate(bool duringLoad);
    virtual void clearScreen();
    virtual void getDimensions(int& width, int& height);
    virtual void outputString(int x, int y, const std::string& text);
    virtual void flushToScreen();

private:
    int     m_refreshCount;
    double  m_lastUpdateTime;
    WINDOW* m_window;
};

StatusMonitorImpl_posix::StatusMonitorImpl_posix()
    : m_refreshCount(0)
    , m_lastUpdateTime(-1000.0)     //  C08F4000. A sentinel: any real wall-clock reading
                                    // is more than 0.5 past it, so the first update always draws.
    , m_window(0)
{
    if (!initscr())
        return;                     // . No terminal -> every method below no-ops.

    cbreak();
    noecho();
    nonl();
    intrflush(stdscr, FALSE);

    m_window = newwin(0, 0, 0, 0);  // 0,0,0,0 = full screen
    if (m_window)
    {
        nodelay(m_window, TRUE);    // wgetch must not block the main loop
        keypad(m_window, TRUE);     // decode arrows / PgUp / PgDn into KEY_*
    }

    // Both of these run even when newwin failed - 0x830415B jumps into the nodelay/keypad block
    // and that block falls back here rather than skipping past. Registering the cleanup regardless
    // is the right call anyway: initscr() already changed the terminal.
    atexit(atexit_cleanup_statusmonitor);
    g_instance = this;
}

//  / 0x83042F0, 63 bytes each. [CHOSEN] body - the destructors are 63 bytes, which is more
// than a bare vptr store, but they were not disassembled. Releasing the window and leaving the
// terminal restored is the only behaviour consistent with the atexit hook.
StatusMonitorImpl_posix::~StatusMonitorImpl_posix()
{
    if (m_window)
    {
        delwin(m_window);
        m_window = 0;
    }
    if (g_instance == this)
        g_instance = 0;
    endwin();
}

// . See StatusMonitorImpl.h for why `duringLoad` reads the way it does.
bool StatusMonitorImpl_posix::shouldUpdate(bool duringLoad)
{
    if (!m_window)
        return false;

    // System vptr+0x1C.
    const double now = g_system->getWallClockTime();

    if (!duringLoad)
    {
        const int c = wgetch(m_window);
        if (c != ERR)
        {
            int key = c;
            switch (c)
            {
            case KEY_DOWN:      key = 14; break;   // ^N
            case KEY_UP:        key = 16; break;   // ^P   yes, this way round
            case KEY_PPAGE:     key =  2; break;   // ^B
            case KEY_NPAGE:     key =  6; break;   // ^F
            case KEY_BACKSPACE:
            case 127:
            case 8:             key =  8; break;
            case 10:
            case 13:
            case KEY_ENTER:     key = 13; break;
            case KEY_RESIZE:    key =  0; break;   // swallowed - see the guard below
            default:                      break;
            }

            if (key)
                io::mainConsole->updateAsciiKey((char)key);      // vptr+0x18

            // A keystroke resets the timer AND returns true, so typing repaints immediately
            // instead of waiting out the half second.
            m_lastUpdateTime = now;
            return true;
        }
    }

    if (now - m_lastUpdateTime <= 0.5)      // flt_888B170
        return false;

    m_lastUpdateTime = now;
    return true;
}

void StatusMonitorImpl_posix::clearScreen()
{
    if (m_window)
        werase(m_window);
}

// 0x8304470. width first, height second; ncurses' _maxx/_maxy are inclusive maxima so
// both get +1. -1 when there is no window.
void StatusMonitorImpl_posix::getDimensions(int& width, int& height)
{
    // The ELF loads window._maxy/_maxx (0x8304482 / 0x830449A) and adds 1. Modern ncurses builds
    // keep WINDOW opaque (NCURSES_OPAQUE), so the members are read through getmaxy/getmaxx, which
    // already answer _maxy + 1 / _maxx + 1 (curses.h). Adding 1 again made the height one row too
    // large: StatusMonitor::update then put the console's last line, the "> " input line, one row
    // below the screen, where wmove refuses it, so typed characters never showed (B2BF #15).
    height = m_window ? getmaxy(m_window) : -1;
    width  = m_window ? getmaxx(m_window) : -1;
}

// . x first, y second - wmove takes them the other way round.
void StatusMonitorImpl_posix::outputString(int x, int y, const std::string& text)
{
    if (!m_window)
        return;
    if (wmove(m_window, y, x) == ERR)       // off-screen coordinates are dropped, not clamped
        return;

    // The original inlines GCC 3's std::string::c_str(), which writes the terminator at data[size()]
    // before returning the pointer. Calling c_str() is the same operation.
    waddnstr(m_window, text.c_str(), -1);   // 1 = the whole string
}

// 0x8304530. Nine incremental refreshes, then one that scrubs the whole screen.
void StatusMonitorImpl_posix::flushToScreen()
{
    if (!m_window)
        return;

    if (++m_refreshCount > 9)
    {
        m_refreshCount = 0;
        wtouchln(m_window, 0, getmaxy(m_window), 1);     // every line dirty: the ELF's _maxy + 1
                                                         // (0x830455C), which getmaxy already is
        wrefresh(curscr);                                // repaint from the current screen
    }
    else
    {
        wrefresh(m_window);
    }
}

// . `operator new(0x14)` - 20 bytes, which is what the members above add up to.
StatusMonitorImpl* StatusMonitorImpl::createInstance()
{
    return new StatusMonitorImpl_posix();
}

} // namespace hfe
} // namespace dice

#endif // posix
