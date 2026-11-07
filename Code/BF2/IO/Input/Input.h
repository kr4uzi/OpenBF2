// Shared input vocabulary and interfaces. Enum names and case-sensitive spellings are part of .con
// serialization; unknown spellings select defaults rather than reporting errors.
// IDA_COUNT is a [CHOSEN] four-element ControlMap tuning-array extent, not the device-axis count.
// Preserve declared enum values and virtual order.

#ifndef IO_INPUT_H              // [INFERRED] guard style, per Core/IBase.h
#define IO_INPUT_H

#include <string>
#include <vector>
#include <iosfwd>

#include "../../Core/IBase.h"

namespace dice {
namespace hfe {
namespace io {

class IInputDevice;
class IControlMap;
class IInputFilter;                 // IO/IInputFilter.h - forward-declared rather than included so
                                    // this header does not pull in another owner's file.
class GameInput;
class PlayerInput;

// The parameter enums. type names attested (see the header block); enumerators [CHOSEN].

// Device flags used as a mask for activation and as individual values for device lookup.
enum InputDeviceFlags
{
    IDFNone             = 0,
    IDFMouse            = 1,
    IDFKeyboard         = 2,
    IDFGameController_0 = 4,        // was [CHOSEN] IDF_GAME_CONTROLLER
    IDFGameController_1 = 8,        // was [CHOSEN] IDF_GAME_CONTROLLER_2 - note the index shift
    IDFGameController_2 = 16,
    IDFGameController_3 = 32,
    IDFGameController_4 = 64,
    IDFGameController_5 = 128,
    IDFGameController_6 = 256,
    IDFGameController_7 = 512,
    IDFFalcon           = 1024,
    IDFAll              = -1        // 0xFFFFFFFF; the reader stores it as such
};

// Device kinds. IDTFalcon's name and value are [INFERRED]; the stream codec handles only the first four kinds.
enum InputDeviceTypes { IDTUndefined = 0, IDTMouse = 1, IDTGameController = 2,
                        IDTKeyboard = 3, IDTFalcon = 4 }; // IDTFalcon spelling [INFERRED]

// Selects a whole axis, its positive half or its mirrored negative half. Enumerator names are [CHOSEN].
enum AxisToAxisOptions { ATAWholeAxis = 0, ATAPositiveHalf = 1, ATANegativeHalf = 2 };
// Type attested in FalconDevice::setCooperativeMode's ELF symbol. Names [CHOSEN].
enum InputDeviceMode { IDM_EXCLUSIVE = 0, IDM_NONEXCLUSIVE = 1 };

// Numbered input axes; value 15 denotes no axis.
enum InputDeviceAxes
{
    IDAxis_0 = 0,  IDAxis_1 = 1,  IDAxis_2 = 2,  IDAxis_3 = 3,
    IDAxis_4 = 4,  IDAxis_5 = 5,  IDAxis_6 = 6,  IDAxis_7 = 7,
    IDAxis_8 = 8,  IDAxis_9 = 9,  IDAxis_10 = 10, IDAxis_11 = 11,
    IDAxis_12 = 12, IDAxis_13 = 13, IDAxis_14 = 14,
    IDAxis_None = 15
};

// [CHOSEN] Extent of the control-map axis-scale array; this is not a device-axis enumerator.
const int IDA_COUNT = 4;

// Button masks use one bit per button. Zero prints as None, while an unknown input token reads as zero.
enum InputDeviceButtons
{
    IDButton_0  = 1u <<  0, IDButton_1  = 1u <<  1, IDButton_2  = 1u <<  2, IDButton_3  = 1u <<  3,
    IDButton_4  = 1u <<  4, IDButton_5  = 1u <<  5, IDButton_6  = 1u <<  6, IDButton_7  = 1u <<  7,
    IDButton_8  = 1u <<  8, IDButton_9  = 1u <<  9, IDButton_10 = 1u << 10, IDButton_11 = 1u << 11,
    IDButton_12 = 1u << 12, IDButton_13 = 1u << 13, IDButton_14 = 1u << 14, IDButton_15 = 1u << 15,
    IDButton_16 = 1u << 16, IDButton_17 = 1u << 17, IDButton_18 = 1u << 18, IDButton_19 = 1u << 19,
    IDButton_20 = 1u << 20, IDButton_21 = 1u << 21, IDButton_22 = 1u << 22, IDButton_23 = 1u << 23,
    IDButton_24 = 1u << 24, IDButton_25 = 1u << 25, IDButton_26 = 1u << 26, IDButton_27 = 1u << 27,
    IDButton_28 = 1u << 28, IDButton_29 = 1u << 29, IDButton_30 = 1u << 30,
    IDButton_31 = static_cast<int>(1u << 31)
};

// DirectInput scan-code values. InputEnums.cpp provides text names and accepts lowercase function-key aliases.
enum InputDeviceKeys
{
    IDKey_Escape = 1, IDKey_1 = 2, IDKey_2 = 3, IDKey_3 = 4,
    IDKey_4 = 5, IDKey_5 = 6, IDKey_6 = 7, IDKey_7 = 8,
    IDKey_8 = 9, IDKey_9 = 10, IDKey_0 = 11, IDKey_Minus = 12,
    IDKey_Equals = 13, IDKey_Backspace = 14, IDKey_Tab = 15, IDKey_Q = 16,
    IDKey_W = 17, IDKey_E = 18, IDKey_R = 19, IDKey_T = 20,
    IDKey_Y = 21, IDKey_U = 22, IDKey_I = 23, IDKey_O = 24,
    IDKey_P = 25, IDKey_LeftBracket = 26, IDKey_RightBracket = 27, IDKey_Enter = 28,
    IDKey_LeftCtrl = 29, IDKey_A = 30, IDKey_S = 31, IDKey_D = 32,
    IDKey_F = 33, IDKey_G = 34, IDKey_H = 35, IDKey_J = 36,
    IDKey_K = 37, IDKey_L = 38, IDKey_Semicolon = 39, IDKey_Apostrophe = 40,
    IDKey_Grave = 41, IDKey_LeftShift = 42, IDKey_Backslash = 43, IDKey_Z = 44,
    IDKey_X = 45, IDKey_C = 46, IDKey_V = 47, IDKey_B = 48,
    IDKey_N = 49, IDKey_M = 50, IDKey_Comma = 51, IDKey_Period = 52,
    IDKey_Slash = 53, IDKey_RightShift = 54, IDKey_Multiply = 55, IDKey_LeftAlt = 56,
    IDKey_Space = 57, IDKey_Capital = 58, IDKey_F1 = 59, IDKey_F2 = 60,
    IDKey_F3 = 61, IDKey_F4 = 62, IDKey_F5 = 63, IDKey_F6 = 64,
    IDKey_F7 = 65, IDKey_F8 = 66, IDKey_F9 = 67, IDKey_F10 = 68,
    IDKey_Numlock = 69, IDKey_ScrollLock = 70, IDKey_Numpad7 = 71, IDKey_Numpad8 = 72,
    IDKey_Numpad9 = 73, IDKey_Subtract = 74, IDKey_Numpad4 = 75, IDKey_Numpad5 = 76,
    IDKey_Numpad6 = 77, IDKey_Add = 78, IDKey_Numpad1 = 79, IDKey_Numpad2 = 80,
    IDKey_Numpad3 = 81, IDKey_Numpad0 = 82, IDKey_Decimal = 83, IDKey_OEM_102 = 86,
    IDKey_F11 = 87, IDKey_F12 = 88, IDKey_F13 = 100, IDKey_F14 = 101,
    IDKey_F15 = 102, IDKey_Kana = 112, IDKey_Convert = 121, IDKey_NoConvert = 123,
    IDKey_Yen = 125, IDKey_NumpadEquals = 141, IDKey_PrevTrack = 144, IDKey_At = 145,
    IDKey_Colon = 146, IDKey_Underline = 147, IDKey_Kanji = 148, IDKey_Stop = 149,
    IDKey_Ax = 150, IDKey_Unlabeled = 151, IDKey_NextTrack = 153, IDKey_NumpadEnter = 156,
    IDKey_RightCtrl = 157, IDKey_Mute = 160, IDKey_Calculator = 161, IDKey_PlayPause = 162,
    IDKey_MediaStop = 164, IDKey_VolumeDown = 174, IDKey_VolumeUp = 176, IDKey_WebHome = 178,
    IDKey_NumpadComma = 179, IDKey_Divide = 181, IDKey_PrintScreen = 183, IDKey_RightAlt = 184,
    IDKey_Pause = 197, IDKey_Home = 199, IDKey_ArrowUp = 200, IDKey_PageUp = 201,
    IDKey_ArrowLeft = 203, IDKey_ArrowRight = 205, IDKey_End = 207, IDKey_ArrowDown = 208,
    IDKey_PageDown = 209, IDKey_Insert = 210, IDKey_Delete = 211, IDKey_LeftWin = 219,
    IDKey_RightWin = 220, IDKey_AppMenu = 221, IDKey_Power = 222, IDKey_Sleep = 223,
    IDKey_Wake = 227, IDKey_WebSearch = 229, IDKey_WebFavorites = 230, IDKey_WebRefresh = 231,
    IDKey_WebStop = 232, IDKey_WebForward = 233, IDKey_WebBack = 234, IDKey_MyComputer = 235,
    IDKey_Mail = 236, IDKey_MediaSelect = 237,
    // [CHOSEN] Name for the unmatched-key sentinel; the printer emits the bare IDKey_ prefix for this value.
    IDKey_None = 255
};

// POV selectors. Unknown values print None, which parses back as IDPOV_0.
enum InputDevicePOVs
{
    IDPOV_0 = 0,
    IDPOV_1 = 1,
    IDPOV_2 = 2
};

// The six stream-operator pairs the console's argument marshalling rides. Bodies and the complete
// name tables: IO/Input/InputEnums.cpp.
std::ostream& operator<<(std::ostream& stream, InputDeviceTypes value);
std::istream& operator>>(std::istream& stream, InputDeviceTypes& value);
std::ostream& operator<<(std::ostream& stream, InputDeviceFlags value);
std::istream& operator>>(std::istream& stream, InputDeviceFlags& value);
std::ostream& operator<<(std::ostream& stream, InputDeviceAxes value);
std::istream& operator>>(std::istream& stream, InputDeviceAxes& value);
std::ostream& operator<<(std::ostream& stream, InputDeviceButtons value);
std::istream& operator>>(std::istream& stream, InputDeviceButtons& value);
std::ostream& operator<<(std::ostream& stream, InputDeviceKeys value);
std::istream& operator>>(std::istream& stream, InputDeviceKeys& value);
std::ostream& operator<<(std::ostream& stream, InputDevicePOVs value);
std::istream& operator>>(std::istream& stream, InputDevicePOVs& value);

// Shared input-device ABI. Preserve virtual order and omit a destructor slot; concrete release owns destruction.
class IInputDevice : public IBase
{
public:
    virtual InputDeviceTypes getType() const = 0;                         // +0x10
    virtual bool setCooperativeMode(InputDeviceMode mode) = 0;            // +0x14
    virtual InputDeviceMode getCooperativeMode() const = 0;               // +0x18
    virtual bool activate() = 0;                                         // +0x1C
    virtual bool deActivate() = 0;                                       // +0x20
    virtual void update(float dt) = 0;                                   // +0x24
    virtual bool isActive() const = 0;                                   // +0x28
    virtual bool getNextInput(InputDeviceAxes&, InputDeviceButtons&,
                              InputDeviceKeys&, bool&) = 0;             // +0x2C
};

class IKeyboardDevice : public IInputDevice
{
public:
    virtual bool keyDown(InputDeviceKeys key) const = 0;                  // +0x30
    virtual bool keyPressed(InputDeviceKeys key) const = 0;               // +0x34
    virtual bool keyReleased(InputDeviceKeys key) const = 0;              // +0x38
    virtual std::wstring getKeyName(InputDeviceKeys key) const = 0;        // +0x3C
};

class IAdvancedInputDevice : public IInputDevice
{
public:
    virtual int getNumAxes() const = 0;                                  // +0x30
    virtual float readAxis(InputDeviceAxes axis, bool scaled) const = 0;  // +0x34
    // ELF 0x8499DF0 / PE 0x6A4ED0: the real inherited default writes Y first.
    virtual void peakXYAxis(float& x, float& y) { y = 0.0f; x = 0.0f; }    // +0x38
    virtual void setInvertAxis(InputDeviceAxes axis, bool invert) = 0;    // +0x3C
    virtual bool getInvertAxis(InputDeviceAxes axis) const = 0;           // +0x40
    virtual void setAxisScale(InputDeviceAxes axis, float scale) = 0;     // +0x44
    virtual float getAxisScale(InputDeviceAxes axis) const = 0;           // +0x48
    virtual void setAxisAdd(InputDeviceAxes axis, float add) = 0;         // +0x4C
    virtual float getAxisAdd(InputDeviceAxes axis) const = 0;             // +0x50
    virtual int getNumButtons() const = 0;                               // +0x54
    virtual bool buttonDown(InputDeviceButtons button) const = 0;        // +0x58
    virtual bool buttonPressed(InputDeviceButtons button) const = 0;     // +0x5C
    virtual bool buttonReleased(InputDeviceButtons button) const = 0;    // +0x60
    virtual float buttonPressedSampleLatency(InputDeviceButtons button) const = 0; // +0x64
};

// The bindings ControlMap::findDuplicate found for one device input: the axis or trigger id, whether
// the clash is on the negative half of an axis pair, and whether it is the alternate binding. The
// ELF mangles it as a class (_ZN...18DuplicateKeyVectorE); in ia-32 findDuplicate and mapNextInput
// it is three parallel vectors at +0x00 (int), +0x0C and +0x20 (bool). Member names [INFERRED].
struct DuplicateKeyVector
{
    std::vector<int>  ids;          // +0x00
    std::vector<bool> negative;     // +0x0C
    std::vector<bool> alternate;    // +0x20
};

// Input identifiers. None is the one-past-end sentinel and has no mapping bit.
enum PlayerInputMap
{
    PIYaw = 0,                PIPitch = 1,              PIRoll = 2,               PIThrottle = 3,
    PIMouseLookX = 4,         PIMouseLookY = 5,         PICameraX = 6,            PICameraY = 7,
    PIFire = 8,               PIAction = 9,             PIUse = 10,               PIMouseLook = 11,
    PIAltSprint = 12,         PISprint = 13,            PIWeaponSelect1 = 14,     PIWeaponSelect2 = 15,
    PIWeaponSelect3 = 16,     PIWeaponSelect4 = 17,     PIWeaponSelect5 = 18,     PIWeaponSelect6 = 19,
    PIWeaponSelect7 = 20,     PIWeaponSelect8 = 21,     PIWeaponSelect9 = 22,     PIPositionSelect1 = 23,
    PIPositionSelect2 = 24,   PIPositionSelect3 = 25,   PIPositionSelect4 = 26,   PIPositionSelect5 = 27,
    PIPositionSelect6 = 28,   PIPositionSelect7 = 29,   PIPositionSelect8 = 30,   PIAltFire = 31,
    PIReload = 32,            PISelectFunc = 33,        PIDrop = 34,              PIToggleCameraMode = 35,
    PIToggleCamera = 36,      PILie = 37,               PICrouch = 38,            PICameraMode1 = 39,
    PICameraMode2 = 40,       PICameraMode3 = 41,       PICameraMode4 = 42,       PISelectPrimWeapon = 43,
    PISelectSecWeapon = 44,   PIToggleWeapon = 45,      PIToggleFireRate = 46,    PIFlareFire = 47,
    PIRadio1 = 48,            PIRadio2 = 49,            PIRadio3 = 50,            PIRadio4 = 51,
    PIRadio5 = 52,            PIRadio6 = 53,            PIRadio7 = 54,            PIRadio8 = 55,
    PIScreenShot = 56,        PIToolTip = 57,           PISayAll = 58,            PISayTeam = 59,
    PINextItem = 60,          PIPrevItem = 61,          PICommunication = 62,     PIShowScoreBoard = 63,
    PINone = 64,
};

// Converts player-input names to and from console tokens.
std::ostream& operator<<(std::ostream& stream, PlayerInputMap value);
std::istream& operator>>(std::istream& stream, PlayerInputMap& value);
std::wistream& operator>>(std::wistream& stream, PlayerInputMap& value);

enum GameInputMap
{
    GIMenu = 0,               GIToggleConsole = 1,      GIEscape = 2,             GIUp = 3,
    GIDown = 4,               GILeft = 5,               GIRight = 6,              GIPageUp = 7,
    GIPageDown = 8,           GIRightShift = 9,         GILeftShift = 10,         GILeftCtrl = 11,
    GIRightCtrl = 12,         GILeftAlt = 13,           GIRightAlt = 14,          GIOk = 15,
    GIAltOk = 16,             GICancel = 17,            GIScreenShot = 18,        GITogglePause = 19,
    GIMouseLookX = 20,        GIMouseLookY = 21,        GISayAll = 22,            GISayTeam = 23,
    GISaySquad = 24,          GIMouseWheelUp = 25,      GIMouseWheelDown = 26,    GIQuit = 27,
    GITab = 28,               GIEnter = 29,             GIDelete = 30,            GIBack = 31,
    GIAltEnter = 32,          GITacticalComm = 33,      GIRadioComm = 34,         GIMapSize = 35,
    GIMapZoom = 36,           GIMenuSelect0 = 37,       GIMenuSelect1 = 38,       GIMenuSelect2 = 39,
    GIMenuSelect3 = 40,       GIMenuSelect4 = 41,       GIMenuSelect5 = 42,       GIMenuSelect6 = 43,
    GIMenuSelect7 = 44,       GIMenuSelect8 = 45,       GIMenuSelect9 = 46,       GIYes = 47,
    GINo = 48,                GIAutoAccept = 49,        GICreateSquad = 50,       GIJoinSquad = 51,
    GISelectSquad = 52,       GIToggleFreeCamera = 53,  GIHotRankUp = 54,         GIShowScoreboard = 55,
    GIVoipUseLeaderChannel = 56,GI3dMap = 57,             GISelectAll = 58,         GIDeselectAll = 59,
    GILeader = 60,            GIVoipPushToTalk = 61,    GIScript1 = 62,           GIScript2 = 63,
    GIScript3 = 64,           GIScript4 = 65,           GIScript5 = 66,           GIScript6 = 67,
    GIScript7 = 68,           GIScript8 = 69,           GIScript9 = 70,           GIScript10 = 71,
    GIScript11 = 72,          GIScript12 = 73,          GINone = 74,
};

std::ostream& operator<<(std::ostream& stream, GameInputMap value);
std::istream& operator>>(std::istream& stream, GameInputMap& value);
std::wistream& operator>>(std::wistream& stream, GameInputMap& value);

// Device interfaces return GameInput and PlayerInput samples through their shared declarations.
class IInputDevice;

// Device initialization, lookup and sampling interface. Preserve virtual order; there is no virtual destructor.
class IInputDeviceManager : public IBase
{
public:
    virtual bool          initDevices(unsigned int* deviceMask)            = 0;   // +0x10
    virtual IInputDevice* getInputDevice(InputDeviceFlags flags) const     = 0;   // +0x14
    virtual unsigned int  getInputDevices() const                          = 0;   // +0x18
    virtual bool          activateDevices(unsigned int deviceMask)         = 0;   // +0x1C
    virtual bool          deActivateDevices(unsigned int deviceMask)       = 0;   // +0x20
    virtual void          update(float deltaTime)                          = 0;   // +0x24
    virtual unsigned int  getActiveInputDevices() const                    = 0;   // +0x28

    // Returns a borrowed view of the manager's device map. [CHOSEN] Opaque return type.
    virtual void*         getAllDevices()                                  = 0;   // +0x2C
};

// Control-map interface. [CHOSEN, not recovered] Interface/concrete boundary; virtual order is fixed.
class IControlMap : public IBase
{
public:
    virtual void  setName(const std::string& name)                          = 0;  // +0x10
    virtual const std::string& getName() const                              = 0;  // +0x14
    virtual int   getID() const                                             = 0;  // +0x18
    virtual void  setID(int id)                                             = 0;  // +0x1C
    virtual bool  axisExists(int axisId) const                              = 0;  // +0x20
    virtual bool  triggerExists(int triggerId) const                        = 0;  // +0x24

    // Adds axis and trigger mappings. Preserve overload order; digital-source forms take the positive id first.
    virtual bool  addAxisMapping(int axisId, int secondaryId, InputDeviceFlags device,
                                 InputDeviceAxes axis, bool invert, bool alternate) = 0;  // +0x28
    virtual bool  addAxisMapping(int axisId, InputDeviceFlags device,
                                 InputDeviceAxes axis, bool invert, bool alternate) = 0;  // +0x2C
    virtual bool  addAxisMapping(int axisId, InputDeviceFlags device,
                                 InputDeviceButtons positive,
                                 InputDeviceButtons negative, bool alternate) = 0;  // +0x30
    virtual bool  addAxisMapping(int axisId, InputDeviceFlags device,
                                 InputDeviceKeys positive,
                                 InputDeviceKeys negative, bool alternate)   = 0;  // +0x34
    virtual bool  addAxisMapping(int axisId, int secondaryId, InputDeviceFlags device,
                                 InputDevicePOVs pov, bool a, bool b, bool c) = 0; // +0x38
    // Maps an axis from a pair of device inputs, with each half carrying its own device flags.
    virtual bool  addAxisMapping(int axisId, InputDeviceFlags device,
                                 InputDeviceButtons button, InputDeviceFlags device2,
                                 InputDeviceKeys key, bool alternate)        = 0;  // +0x3C
    virtual bool  addAxisMapping(int axisId, InputDeviceFlags device,
                                 InputDeviceKeys key, InputDeviceFlags device2,
                                 InputDeviceButtons button, bool alternate)  = 0;  // +0x40

    virtual bool  addTriggerMapping(int triggerId, InputDeviceFlags device,
                                    InputDeviceButtons button, float threshold,
                                    bool alternate)                          = 0;  // +0x44
    virtual bool  addTriggerMapping(int triggerId, InputDeviceFlags device,
                                    InputDeviceKeys key, float threshold,
                                    bool alternate)                          = 0;  // +0x48
    virtual bool  addTriggerMapping(int triggerId, int secondaryId, InputDeviceFlags device,
                                    InputDeviceAxes axis, bool alternate,
                                    float f, float g)                        = 0;  // +0x4C

    virtual void  removeMapping(unsigned int id)                            = 0;  // +0x50
    virtual void  reset()                                                   = 0;  // +0x54
    virtual void  reset(int id, bool a, bool b, bool c)                     = 0;  // +0x58

    virtual void  setButtonRiseTime(float t)                                = 0;  // +0x5C
    virtual void  setButtonFallTime(float t)                                = 0;  // +0x60
    virtual void  setDoubleTapTime(float t)                                 = 0;  // +0x64

    virtual float getAxisValue(int axisId) const                            = 0;  // +0x68
    virtual bool  isAxisValueAnalogue(int axisId) const                     = 0;  // +0x6C
    virtual int   getTriggerState(int triggerId) const                      = 0;  // +0x70
    virtual float getTriggerSampleLatency(int triggerId) const              = 0;  // +0x74

    virtual bool  validate()                                                = 0;  // +0x78
    virtual void  update(float deltaTime)                                   = 0;  // +0x7C

    virtual void  setInvertMouse(bool invert)                               = 0;  // +0x80
    virtual bool  getInvertMouse()                                          = 0;  // +0x84
    virtual void  setMouseSensitivity(float s)                              = 0;  // +0x88
    virtual float getMouseSensitivity()                                     = 0;  // +0x8C
    virtual void  setKeyboardSensitivity(float s)                           = 0;  // +0x90
    virtual float getKeyboardSensitivity()                                  = 0;  // +0x94
    virtual void  setAxisScale(InputDeviceAxes axis, float scale)           = 0;  // +0x98
    virtual float getAxisScale(InputDeviceAxes axis) const                  = 0;  // +0x9C
    virtual void  setYawFactor(float f)                                     = 0;  // +0xA0
    virtual void  setPitchFactor(float f)                                   = 0;  // +0xA4
    virtual float getYawFactor()                                            = 0;  // +0xA8
    virtual float getPitchFactor()                                          = 0;  // +0xAC

    virtual void  save(std::ostream& out, bool b)                           = 0;  // +0xB0
    virtual void  dump(bool b)                                              = 0;  // +0xB4
    virtual bool  findDuplicate(InputDeviceFlags& device, InputDeviceAxes& axis,
                                InputDeviceButtons& button, InputDeviceKeys& key,
                                DuplicateKeyVector& out)                    = 0;  // +0xB8
    virtual bool  getInputMappedToMouseButton(bool b, PlayerInputMap& out)  = 0;  // +0xBC
};

// Manages control maps. The virtual create(name) is distinct from the static registry factory.
class IControlMapManager : public IBase
{
public:
    virtual void         update(float deltaTime)                            = 0;  // +0x10
    virtual IControlMap* create(const std::string& name)                    = 0;  // +0x14  see above
    virtual IControlMap* getControlMap(const std::string& name) const       = 0;  // +0x18
    virtual IControlMap* getControlMap(int id) const                        = 0;  // +0x1C
    virtual void         deleteControlMap(const std::string& name)          = 0;  // +0x20
    virtual void         deleteControlMap(int id)                           = 0;  // +0x24
    virtual void         deleteControlMap(const IControlMap* map)           = 0;  // +0x28
    virtual void         setActive(const std::string& name)                 = 0;  // +0x2C
    virtual void         setActive(const IControlMap* map)                  = 0;  // +0x30
    virtual IControlMap* getActive() const                                  = 0;  // +0x34

    // 5,146 bytes in the ELF (0x8497AC0) - the "press a key to bind it" capture mode, and the
    // single largest routine in the whole input subsystem.
    // Returns an int status, not a Boolean: ControlSettings +0x30 (BF2.exe 0x4B2CA0) passes this
    // full EAX on, and SwiffPlayer mapInput (sub_1000EDA0) compares it with 1 (duplicate) and 2.
    virtual int          mapNextInput(IControlMap* map, int id, bool a, bool b, bool c, bool d,
                                      std::string& out)                     = 0;  // +0x38
    virtual float        getRepeatRate(int id, bool b)                      = 0;  // +0x3C
    virtual void         applyActive()                                      = 0;  // +0x40
};

// Input sampling and filtering interface. Preserve declaration order for virtual dispatch.
class IInputManager : public IBase
{
public:
    virtual bool  init(unsigned int deviceMask)                             = 0;  // +0x10
    virtual bool  shutdown()                                                = 0;  // +0x14
    virtual bool  setHasFocus(bool hasFocus, unsigned int deviceMask)       = 0;  // +0x18
    virtual void  update(int tick, float deltaTime)                         = 0;  // +0x1C
    virtual bool  registerInputFilter(IInputFilter* filter)                 = 0;  // +0x20
    virtual bool  unregisterInputFilter(IInputFilter* filter)               = 0;  // +0x24
    virtual bool  setGameControlMap(std::string name)                       = 0;  // +0x28  by value
    virtual bool  setPlayerControlMap(std::string name)                     = 0;  // +0x2C  by value
    virtual std::string getPlayerControlMap()                               = 0;  // +0x30
    virtual PlayerInput* getNewPlayerInputSample()                          = 0;  // +0x34
    virtual GameInput*   getNewGameInputSample()                            = 0;  // +0x38
    virtual bool  advanceNextUnread()                                       = 0;  // +0x3C
    virtual void  catchUp()                                                 = 0;  // +0x40
    virtual void  setPlayerInputDelay(int delay)                            = 0;  // +0x44
    virtual int   getNumEntries()                                           = 0;  // +0x48
};

// Shared sample builders called by InputManager::update. Parameter types and names are attested by
// the ELF symbols; BF2.exe 0x6C90C0/0x6C92A0 explicitly materialise their bool return values.
bool getPlayerInput(PlayerInput& input, IControlMap* controlMap);
bool getGameInput(GameInput& input, IControlMap* controlMap);


// Stores N input values and their mapping state. Unmapped inputs ignore value writes.
// Callers must provide valid indices: mapping updates are unchecked and value setters only check the upper bound.
template <class T, unsigned int N>
class Input
{
public:
    /* +0x000 */ float              inputs[N];
    /* +N*4   */ unsigned long long mappingFlags;   // bit i = "input i is bound by the control map"
    /* +N*4+8 */ unsigned int       dword0;         // [UNRESOLVED]
                 unsigned int       dword1;         // [UNRESOLVED]
                 unsigned char      flag;           // [UNRESOLVED] - the last byte the memset covers
                 unsigned char      byte1;          // [UNRESOLVED] - outside the memset
                 unsigned char      byte2;
                 unsigned char      byte3;

    void setInputIsMapped(T index, bool mapped)
    {
        // no range check - see defect 1 above. The explicit mask reproduces x86's variable-shift
        // count handling: GameInput ids 64..73 alias mask bits 0..9 in the shipped code.
        const unsigned int shift = static_cast<unsigned int>(index) & 0x3Fu;
        if (mapped)
            mappingFlags |= (1ULL << shift);
        else
            mappingFlags &= ~(1ULL << shift);
    }

    void setInput(T index, float value);            // Input.h:191
    void setInput(T index, bool value);             // Input.h:203
};

} // namespace io
} // namespace hfe
} // namespace dice

#endif // IO_INPUT_H
