// attested: BF2.exe 0x912388 names IO/Input/KeyboardDevice.cpp.
// Complete 18-slot client device, primary table 0x912408.
#include "KeyboardDevice.h"
#if !DEDICATED
#include "../../Core/Debug.h"
#include "../../Core/Ids.h"
#include "../../Game/Main/BF2ClientSeams.h"
#include "../../Game/Main/BF2Window.h"
#include "../../Platform/Windows/WinInputScript.h"   // [TEMP] BF2_INPUT_SCRIPT feed
#include <cstring>

namespace dice { namespace hfe { namespace io {
extern IBase* inputDeviceManager;
namespace {
const char* const sourcePath = "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\IO\\Input\\KeyboardDevice.cpp";
DIDEVICEOBJECTDATA bufferedEvents[100]; // PE 0xA03E90, shared by all KeyboardDevice instances
}

KeyboardDevice::KeyboardDevice() : m_refCount(1), m_device(0), m_active(false) {}

KeyboardDevice::~KeyboardDevice()
{
    if (m_device) {
        m_device->Unacquire();
        m_device->Release();
        m_device = 0;
    }
    // Retail reads this even when init has never run; retain the constructor's
    // untouched pointer rather than silently repairing its lifetime defect.
    if (m_directInput) {
        m_directInput->Release();
        m_directInput = 0;
    }
}

void KeyboardDevice::addRef() { ++m_refCount; }
int KeyboardDevice::getRef() { return m_refCount; }
int KeyboardDevice::release()
{
    const int count = --m_refCount;
    if (count == 0) delete this;
    return count;
}
void* KeyboardDevice::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IInputDevice || iid == IID_IKeyboardDevice)
        return const_cast<KeyboardDevice*>(this);
    return 0;
}
InputDeviceTypes KeyboardDevice::getType() const { return IDTKeyboard; }
InputDeviceMode KeyboardDevice::getCooperativeMode() const { return m_mode; }

bool KeyboardDevice::setCooperativeMode(InputDeviceMode mode)
{
    const DWORD flags = mode ? 22 : 21;
    if (m_device->SetCooperativeLevel(GetFocus(), flags) != DI_OK)
        return setCooperativeModeForWindow(g_bf2Render->getWindow()->getHwnd(), mode);
    m_mode = mode;
    return true;
}
bool KeyboardDevice::setCooperativeModeForWindow(HWND window, InputDeviceMode mode)
{
    if (m_device->SetCooperativeLevel(window, mode ? 22 : 21) != DI_OK)
        return false;
    m_mode = mode;
    return true;
}
bool KeyboardDevice::activate()
{
    if (!m_device) return false;
    if (!m_active) {
        m_active = m_device->Acquire() == DI_OK;
        // [TEMP] [CHOSEN, not recovered] BF2_INPUT_SCRIPT (Platform/Windows/WinInputScript.h): the
        // scripted feed counts as acquired without focus. [RETIRE-WHEN: client-test-automation-replaced]
        if (inputscript::winInputScriptActive()) {
            m_active = true;
            inputscript::winInputScriptClearKeyboardData();
        }
    }
    return true; // Retail reports success even if Acquire failed.
}
bool KeyboardDevice::deActivate()
{
    if (!m_device) return false;
    if (m_active) {
        m_device->Unacquire();
        m_active = false;
        m_events.clear();
        std::memset(m_keys, 0, sizeof(m_keys));
    }
    return true;
}
bool KeyboardDevice::isActive() const { return m_device && m_active; }

bool KeyboardDevice::getNextInput(InputDeviceAxes& axis, InputDeviceButtons& button,
                                  InputDeviceKeys& key, bool&)
{
    for (int i = 0; i < 255; ++i) {
        if (keyPressed(static_cast<InputDeviceKeys>(i))) {
            axis = static_cast<InputDeviceAxes>(15);
            button = static_cast<InputDeviceButtons>(0);
            key = static_cast<InputDeviceKeys>(i);
            return true;
        }
    }
    return false;
}
bool KeyboardDevice::keyDown(InputDeviceKeys key) const
{ return m_keys[static_cast<unsigned char>(key)] != 0; }
bool KeyboardDevice::keyPressed(InputDeviceKeys key) const
{
    const unsigned char k = static_cast<unsigned char>(key);
    return !m_previousKeys[k] && m_keys[k];
}
bool KeyboardDevice::keyReleased(InputDeviceKeys key) const
{
    const unsigned char k = static_cast<unsigned char>(key);
    return m_previousKeys[k] && !m_keys[k];
}
std::wstring KeyboardDevice::getKeyName(InputDeviceKeys key) const
{
    DIPROPSTRING property; // Retail leaves its output string uninitialized on error.
    property.diph.dwSize = sizeof(property);
    property.diph.dwHeaderSize = sizeof(property.diph);
    property.diph.dwObj = static_cast<DWORD>(key);
    property.diph.dwHow = DIPH_BYOFFSET;
    m_device->GetProperty(DIPROP_KEYNAME, &property.diph);
    return std::wstring(property.wsz);
}

bool KeyboardDevice::initDirectInput()
{
    const HRESULT hr = DirectInput8Create(GetModuleHandleA(0), 0x800,
        IID_IDirectInput8A, reinterpret_cast<void**>(&m_directInput), 0);
    if (hr == DI_OK) return true;
    switch (hr) {
    case DIERR_BETADIRECTINPUTVERSION:
        Debug(DEBUGTYPE_6, sourcePath, 56, "") << "InputDeviceBase error betaversion"; break;
    case DIERR_INVALIDPARAM:
        Debug(DEBUGTYPE_6, sourcePath, 59, "") << "InputDeviceBase error invalid param"; break;
    case DIERR_OLDDIRECTINPUTVERSION:
        Debug(DEBUGTYPE_6, sourcePath, 62, "") << "InputDeviceBase error oldversion"; break;
    case DIERR_OUTOFMEMORY:
        Debug(DEBUGTYPE_6, sourcePath, 65, "") << "InputDeviceBase error out of memory"; break;
    default:
        Debug(DEBUGTYPE_6, sourcePath, 69, "") << "InputDeviceBase error other error " << hr; break;
    }
    return false;
}

bool KeyboardDevice::init()
{
    if (!inputDeviceManager) return false;
    if (!initDirectInput()) {
        Debug(DEBUGTYPE_6, sourcePath, 122, "") << "init failed";
        return false;
    }
    if (m_directInput->CreateDevice(GUID_SysKeyboard, &m_device, 0) != DI_OK)
        return false;
    if (m_device->SetDataFormat(&c_dfDIKeyboard) != DI_OK) return false;
    DIPROPDWORD property = { { sizeof(property), sizeof(property.diph), 0, 0 }, 100 };
    if (m_device->SetProperty(DIPROP_BUFFERSIZE, &property.diph) < 0)
        Debug(DEBUGTYPE_6, sourcePath, 156, "") << "Failed to init buffered mode";
    if (!setCooperativeMode(IDM_NONEXCLUSIVE)) return false;
    std::memset(m_keys, 0, sizeof(m_keys));
    return true;
}

void KeyboardDevice::updateCurrentState(float dt)
{
    std::memcpy(m_previousKeys, m_keys, sizeof(m_keys));
    DWORD firstTimestamp = 0, boundary = 0;
    while (!m_events.empty()) {
        const DIDEVICEOBJECTDATA& event = m_events.front();
        if (!firstTimestamp) {
            firstTimestamp = event.dwTimeStamp;
            // PE multiplies by negative 1000, truncates to signed int, then
            // subtracts with unsigned dword arithmetic (including wraparound).
            boundary = firstTimestamp - static_cast<int>(static_cast<double>(dt) * -1000.0);
        }
        if (event.dwTimeStamp >= boundary) break;
        m_keys[event.dwOfs] = static_cast<unsigned char>((event.dwData >> 7) & 1);
        m_events.pop_front();
    }
}

void KeyboardDevice::update(float dt)
{
    if (!m_active || !m_device) return;
    if (dt > 0.0f) { // unordered NaN takes the device-data arm, as in PE fnstsw/test.
        updateCurrentState(dt);
        return;
    }
    DWORD count;
    HRESULT hr;
    // [TEMP] [CHOSEN, not recovered] BF2_INPUT_SCRIPT: scripted device data replaces the real
    // device's. [RETIRE-WHEN: client-test-automation-replaced]
    if (inputscript::winInputScriptActive()) {
        count = inputscript::winInputScriptTakeKeyboardData(bufferedEvents, 100);
        hr = DI_OK;
    } else
    for (;;) {
        count = 100;
        hr = m_device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), bufferedEvents, &count, 0);
        if (hr != DIERR_INPUTLOST) break;
        m_events.clear();
        std::memset(m_keys, 0, sizeof(m_keys));
        if (m_device->Acquire() != DI_OK) return;
    }
    if (hr == DI_BUFFEROVERFLOW) {
        m_events.clear();
        std::memset(m_keys, 0, sizeof(m_keys));
        return;
    }
    if (hr == DI_OK)
        for (DWORD i = 0; i < count; ++i)
            if (bufferedEvents[i].dwOfs < 255)
                m_events.push_back(bufferedEvents[i]);
}
} } }
#endif
