// [TEMP] [CHOSEN, not recovered] Win32 client test tool: the scripted input host and its DIK_ key
// table (WinInputScript.h, Platform/InputScript.h, docs/macos-c-input-script.md).
// [RETIRE-WHEN: client-test-automation-replaced]
#include "WinInputScript.h"
#if !DEDICATED && defined(_WIN32)
#include "../InputScript.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace dice {
namespace hfe {
namespace inputscript {

namespace keys {
namespace {

struct KeyName { const char* name; unsigned short code; };

// Physical key positions (DIK_ codes: scan codes, 0x80 marks the E0-extended keys). Names follow
// Controls.con's IDKey_ names without the prefix, as in the macOS table.
const KeyName kKeys[] = {
    {"a", DIK_A}, {"b", DIK_B}, {"c", DIK_C}, {"d", DIK_D}, {"e", DIK_E}, {"f", DIK_F},
    {"g", DIK_G}, {"h", DIK_H}, {"i", DIK_I}, {"j", DIK_J}, {"k", DIK_K}, {"l", DIK_L},
    {"m", DIK_M}, {"n", DIK_N}, {"o", DIK_O}, {"p", DIK_P}, {"q", DIK_Q}, {"r", DIK_R},
    {"s", DIK_S}, {"t", DIK_T}, {"u", DIK_U}, {"v", DIK_V}, {"w", DIK_W}, {"x", DIK_X},
    {"y", DIK_Y}, {"z", DIK_Z},
    {"0", DIK_0}, {"1", DIK_1}, {"2", DIK_2}, {"3", DIK_3}, {"4", DIK_4},
    {"5", DIK_5}, {"6", DIK_6}, {"7", DIK_7}, {"8", DIK_8}, {"9", DIK_9},
    {"escape", DIK_ESCAPE}, {"esc", DIK_ESCAPE}, {"enter", DIK_RETURN}, {"return", DIK_RETURN},
    {"space", DIK_SPACE}, {"tab", DIK_TAB}, {"backspace", DIK_BACK}, {"grave", DIK_GRAVE},
    {"minus", DIK_MINUS}, {"equals", DIK_EQUALS}, {"leftbracket", DIK_LBRACKET},
    {"rightbracket", DIK_RBRACKET}, {"semicolon", DIK_SEMICOLON},
    {"apostrophe", DIK_APOSTROPHE}, {"backslash", DIK_BACKSLASH}, {"comma", DIK_COMMA},
    {"period", DIK_PERIOD}, {"slash", DIK_SLASH},
    {"f1", DIK_F1}, {"f2", DIK_F2}, {"f3", DIK_F3}, {"f4", DIK_F4}, {"f5", DIK_F5}, {"f6", DIK_F6},
    {"f7", DIK_F7}, {"f8", DIK_F8}, {"f9", DIK_F9}, {"f10", DIK_F10}, {"f11", DIK_F11}, {"f12", DIK_F12},
    {"up", DIK_UP}, {"down", DIK_DOWN}, {"left", DIK_LEFT}, {"right", DIK_RIGHT},
    {"arrowup", DIK_UP}, {"arrowdown", DIK_DOWN}, {"arrowleft", DIK_LEFT},
    {"arrowright", DIK_RIGHT},
    {"home", DIK_HOME}, {"end", DIK_END}, {"pageup", DIK_PRIOR}, {"pagedown", DIK_NEXT},
    {"delete", DIK_DELETE}, {"insert", DIK_INSERT}, {"numpadenter", DIK_NUMPADENTER},
    {"leftshift", DIK_LSHIFT}, {"shift", DIK_LSHIFT}, {"rightshift", DIK_RSHIFT},
    {"leftctrl", DIK_LCONTROL}, {"ctrl", DIK_LCONTROL}, {"rightctrl", DIK_RCONTROL},
    {"leftalt", DIK_LMENU}, {"alt", DIK_LMENU}, {"rightalt", DIK_RMENU},
    {"capital", DIK_CAPITAL},
};

} // namespace

bool lookup(const std::string& name, unsigned short& code)
{
    for (const KeyName& k : kKeys)
        if (name == k.name) { code = k.code; return true; }
    return false;
}

// The WM_CHAR TranslateMessage would produce for keys ToUnicode answers with a control character.
std::u16string controlCharacters(unsigned short code)
{
    switch (code)
    {
    case DIK_RETURN:
    case DIK_NUMPADENTER: return std::u16string(1, u'\r');
    case DIK_TAB:         return std::u16string(1, u'\t');
    case DIK_ESCAPE:      return std::u16string(1, u'\x1B');
    case DIK_BACK:        return std::u16string(1, u'\x08');
    default:              return std::u16string();
    }
}

bool isModifier(unsigned short code)
{
    return code == DIK_LSHIFT || code == DIK_RSHIFT || code == DIK_LCONTROL || code == DIK_RCONTROL ||
           code == DIK_LMENU || code == DIK_RMENU || code == DIK_LWIN || code == DIK_RWIN ||
           code == DIK_CAPITAL;
}

bool isShift(unsigned short code) { return code == DIK_LSHIFT || code == DIK_RSHIFT; }
unsigned short leftShift() { return DIK_LSHIFT; }
unsigned short grave()     { return DIK_GRAVE; }
unsigned short enter()     { return DIK_RETURN; }
unsigned short w()         { return DIK_W; }

} // namespace keys

namespace {

const std::size_t kQueueLimit = 256;        // a real buffer would overflow long before this

// The window-message key code for a DIK_ code, as a real keyboard's WM_KEYDOWN carries it
// (generic VK_SHIFT / VK_CONTROL / VK_MENU for the modifiers).
UINT virtualKey(unsigned short code, bool generic)
{
    const UINT scan = code & 0x7Fu;
    const UINT vk = MapVirtualKeyW((code & 0x80u) ? (0xE000u | scan) : scan, MAPVK_VSC_TO_VK_EX);
    if (!generic)
        return vk;
    switch (vk)
    {
    case VK_LSHIFT: case VK_RSHIFT:     return VK_SHIFT;
    case VK_LCONTROL: case VK_RCONTROL: return VK_CONTROL;
    case VK_LMENU: case VK_RMENU:       return VK_MENU;
    default:                            return vk;
    }
}

class WinSink : public IInputScriptSink
{
public:
    HWND window = 0;
    bool menuActive = false;
    unsigned long pumps = 0;
    std::deque<DIDEVICEOBJECTDATA> keyboard, mouse;

    void scriptKey(unsigned short keyCode, bool down, const std::u16string& text) override
    {
        push(keyboard, keyCode, down ? 0x80u : 0u);
        if (!window)
            return;
        const UINT vk = virtualKey(keyCode, true);
        LPARAM lParam = 1 | (static_cast<LPARAM>(keyCode & 0x7Fu) << 16);
        if (keyCode & 0x80u)
            lParam |= 1 << 24;
        if (!down)
            lParam |= static_cast<LPARAM>(3u << 30);
        if (vk)
            SendMessageA(window, down ? WM_KEYDOWN : WM_KEYUP, vk, lParam);
        if (!down)
            return;
        for (char16_t c : text)
        {
            const wchar_t wide = static_cast<wchar_t>(c);
            char ansi[4] = {};
            if (WideCharToMultiByte(CP_ACP, 0, &wide, 1, ansi, sizeof(ansi), 0, 0) == 1)
                SendMessageA(window, WM_CHAR, static_cast<unsigned char>(ansi[0]), lParam);
        }
    }

    void scriptMouseTo(float x, float y) override
    {
        pointerInit();
        moveDevice(x - m_x, y - m_y);
        m_x = x;
        m_y = y;
        clampPointer();
        if (menuActive)
            sendPointer(WM_MOUSEMOVE);
    }

    void scriptMouseBy(float dx, float dy) override
    {
        pointerInit();
        moveDevice(dx, dy);
        if (!menuActive)
            return;                             // the exclusive in-game mouse posts no messages
        m_x += dx;
        m_y += dy;
        clampPointer();
        sendPointer(WM_MOUSEMOVE);
    }

    void scriptButton(int button, bool down) override
    {
        pointerInit();
        if (button < 0 || button > 7)
            return;
        push(mouse, DIMOFS_BUTTON0 + button, down ? 0x80u : 0u);
        const unsigned int bit = 1u << button;
        if (down) m_buttons |= bit; else m_buttons &= ~bit;
        if (!menuActive || button > 2)
            return;
        static const UINT messages[3][2] = {
            {WM_LBUTTONUP, WM_LBUTTONDOWN}, {WM_RBUTTONUP, WM_RBUTTONDOWN}, {WM_MBUTTONUP, WM_MBUTTONDOWN}};
        sendPointer(messages[button][down ? 1 : 0]);
    }

    void scriptWheel(int delta) override
    {
        pointerInit();
        push(mouse, DIMOFS_Z, static_cast<DWORD>(static_cast<LONG>(delta)));
        if (!menuActive || !window)
            return;
        POINT p = {static_cast<LONG>(m_x), static_cast<LONG>(m_y)};
        ClientToScreen(window, &p);
        SendMessageA(window, WM_MOUSEWHEEL, MAKEWPARAM(buttonFlags(), static_cast<short>(delta)),
                     MAKELPARAM(p.x, p.y));
    }

    std::u16string scriptKeyCharacter(unsigned short keyCode, bool shift) override
    {
        const UINT vk = virtualKey(keyCode, false);
        if (!vk)
            return std::u16string();
        BYTE state[256] = {};
        if (shift)
            state[VK_SHIFT] = state[VK_LSHIFT] = 0x80;
        wchar_t chars[8] = {};
        // Flag 4: leave the keyboard (dead-key) state alone.
        const int n = ToUnicode(vk, keyCode & 0x7Fu, state, chars, 8, 4);
        if (n == 1 && chars[0] >= 0x20 && chars[0] != 0x7F)
            return std::u16string(1, static_cast<char16_t>(chars[0]));
        return std::u16string();
    }

    void scriptQuit() override
    {
        if (window)
            PostMessageA(window, WM_CLOSE, 0, 0);   // as the close button
    }

    void push(std::deque<DIDEVICEOBJECTDATA>& queue, DWORD offset, DWORD data)
    {
        DIDEVICEOBJECTDATA event;
        std::memset(&event, 0, sizeof(event));
        event.dwOfs = offset;
        event.dwData = data;
        event.dwTimeStamp = GetTickCount();
        event.dwSequence = ++m_sequence;
        queue.push_back(event);
        if (queue.size() > kQueueLimit)
            queue.pop_front();
    }

private:
    void pointerInit()
    {
        if (m_pointerSet || !window)
            return;
        RECT r;
        if (!GetClientRect(window, &r))
            return;
        m_pointerSet = true;
        m_x = (r.right - r.left) * 0.5f;
        m_y = (r.bottom - r.top) * 0.5f;
    }

    void clampPointer()
    {
        RECT r;
        if (!window || !GetClientRect(window, &r))
            return;
        const float w = static_cast<float>(r.right - r.left - 1), h = static_cast<float>(r.bottom - r.top - 1);
        m_x = m_x < 0 ? 0 : m_x > w ? w : m_x;
        m_y = m_y < 0 ? 0 : m_y > h ? h : m_y;
    }

    // Relative device motion; fractions carry over to the next move.
    void moveDevice(float dx, float dy)
    {
        m_remX += dx;
        m_remY += dy;
        const LONG ix = static_cast<LONG>(m_remX), iy = static_cast<LONG>(m_remY);
        m_remX -= ix;
        m_remY -= iy;
        if (ix) push(mouse, DIMOFS_X, static_cast<DWORD>(ix));
        if (iy) push(mouse, DIMOFS_Y, static_cast<DWORD>(iy));
    }

    WPARAM buttonFlags() const
    {
        WPARAM flags = 0;
        if (m_buttons & 1) flags |= MK_LBUTTON;
        if (m_buttons & 2) flags |= MK_RBUTTON;
        if (m_buttons & 4) flags |= MK_MBUTTON;
        if (GetKeyState(VK_SHIFT) < 0) flags |= MK_SHIFT;
        return flags;
    }

    void sendPointer(UINT message)
    {
        if (!window)
            return;
        SendMessageA(window, message, buttonFlags(),
                     MAKELPARAM(static_cast<int>(m_x), static_cast<int>(m_y)));
    }

    float m_x = 0.0f, m_y = 0.0f, m_remX = 0.0f, m_remY = 0.0f;
    bool m_pointerSet = false;
    unsigned int m_buttons = 0;
    DWORD m_sequence = 0;
};

WinSink& sink()
{
    static WinSink s;
    return s;
}

// "capture LABEL": the game window's client area (a screen copy) as a 24-bit BMP in BF2_PRESENT_PNG (a directory,
// as for the macOS present capture), named present_<pump>_<label>.bmp.
void captureWindow(HWND window, unsigned long pump, const std::string& label)
{
    const char* dir = std::getenv("BF2_PRESENT_PNG");
    RECT r;
    if (!dir || !*dir || !window || !GetClientRect(window, &r))
        return;
    const int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0)
        return;
    HDC windowDc = GetDC(window);
    HDC memory = CreateCompatibleDC(windowDc);
    BITMAPINFO info;
    std::memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = h;                // bottom-up, as a BMP file stores it
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = 0;
    HBITMAP bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, &bits, 0, 0);
    bool ok = false;
    if (bitmap)
    {
        HGDIOBJ old = SelectObject(memory, bitmap);
        // The screen copy of the client area (the window is in front in the test session);
        // PrintWindow is the fallback for an occluded window. PrintWindow alone returns black for
        // the Direct3D swap chain.
        POINT origin = {0, 0};
        ClientToScreen(window, &origin);
        HDC screen = GetDC(0);
        ok = BitBlt(memory, 0, 0, w, h, screen, origin.x, origin.y, SRCCOPY | CAPTUREBLT) != FALSE;
        ReleaseDC(0, screen);
        if (!ok)
            ok = PrintWindow(window, memory, 3) != FALSE;   // PW_CLIENTONLY | PW_RENDERFULLCONTENT
        GdiFlush();
        SelectObject(memory, old);
    }
    if (ok)
    {
        const DWORD stride = (static_cast<DWORD>(w) * 3 + 3) & ~3u;
        const DWORD imageSize = stride * static_cast<DWORD>(h);
        BITMAPFILEHEADER file;
        std::memset(&file, 0, sizeof(file));
        file.bfType = 0x4D42;
        file.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        file.bfSize = file.bfOffBits + imageSize;
        char path[MAX_PATH];
        std::snprintf(path, sizeof(path), "%s\\present_%05lu_%s.bmp", dir, pump, label.c_str());
        if (FILE* f = std::fopen(path, "wb"))
        {
            std::fwrite(&file, sizeof(file), 1, f);
            std::fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, f);
            std::fwrite(bits, 1, imageSize, f);
            std::fclose(f);
            std::fprintf(stderr, "[TEMP] [INPUT-SCRIPT] captured %s\n", path);
        }
    }
    if (bitmap)
        DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(window, windowDc);
}

DWORD take(std::deque<DIDEVICEOBJECTDATA>& queue, DIDEVICEOBJECTDATA* out, DWORD capacity)
{
    DWORD n = 0;
    while (n < capacity && !queue.empty())
    {
        out[n++] = queue.front();
        queue.pop_front();
    }
    return n;
}

} // namespace

bool winInputScriptActive()
{
    static int state = -1;
    if (state < 0)
        state = inputScript().loadFromEnvironment() ? 1 : 0;
    return state == 1;
}

bool winInputScriptIsRealInput(UINT message)
{
    return (message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
           (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) ||
           message == WM_INPUT;
}

void winInputScriptPump(HWND window, bool menuActive)
{
    if (!winInputScriptActive())
        return;
    WinSink& s = sink();
    s.window = window;
    s.menuActive = menuActive;
    ++s.pumps;
    inputScript().tick(s);
    std::string label;
    while (inputScript().takeCaptureRequest(label))
    {
        for (char& c : label)                   // a file-name component
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
                c = '_';
        captureWindow(window, s.pumps, label);
    }
}

DWORD winInputScriptTakeKeyboardData(DIDEVICEOBJECTDATA* out, DWORD capacity)
{
    return take(sink().keyboard, out, capacity);
}

DWORD winInputScriptTakeMouseData(DIDEVICEOBJECTDATA* out, DWORD capacity)
{
    return take(sink().mouse, out, capacity);
}

void winInputScriptClearKeyboardData() { sink().keyboard.clear(); }
void winInputScriptClearMouseData()    { sink().mouse.clear(); }

} // namespace inputscript
} // namespace hfe
} // namespace dice

#endif // !DEDICATED && _WIN32
