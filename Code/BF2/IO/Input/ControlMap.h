// Maps device axes and triggers to game controls and stores their tuning values.
// Header path [INFERRED]. Axis and trigger remap structures have different PE/ELF layouts;
// use the declared fields and native container layout for the selected image.
// The concrete virtual destructor follows the interface methods; preserve their declaration order.

#ifndef IO_INPUT_CONTROLMAP_H           // [INFERRED] guard style, per Core/IBase.h
#define IO_INPUT_CONTROLMAP_H

#include <map>
#include <string>
#include <vector>
#include <iosfwd>

#include "Input.h"
#include "../../Core/IBase.h"

namespace dice {
namespace hfe {
class ControlSettings;   // Game/ControlSettings.cpp - a friend of ControlMap, see the class
namespace io {

// Primary and alternate axis/trigger bindings. These records have no vtable.
// Double alignment differs between compilers; retain [UNRESOLVED] fields until their roles are identified.
struct AxisMapping {
    AxisMapping();                                  // five stores, then reset()
    void reset();                                   // every field zeroed
    AxisMapping& operator=(const AxisMapping&);     // every field
    // Input mapping fields shared by binding creation and runtime resolution.
    int           device;         // +00  InputDeviceFlags the resolver asks the device manager for
    int           type;           // +04  1 axis, 2 buttons, 3 keys, 4 POV, 5 mixed (resolveAxisMapping
                                  //      0x8493030's switch; the add* bodies write the same values)
    int           axisOptions;    // +08  AxisToAxisOptions, written by 0x84916A0, read by axisToAxis
    int           axis;           // +0C  InputDeviceAxes, same pair
    int           positiveButton; // +10  written by 0x8491F80 (first argument), read by buttonsToAxis
    int           negativeButton; // +14    as the +1.0 / -1.0 side respectively
    int           positiveKey;    // +18  written by 0x84920F0 (first argument), read by buttonsToAxis
    int           negativeKey;    // +1C    first argument is the positive one - `addKeysToAxisMapping
                                  //      c_PIThrottle IDFKeyboard IDKey_W IDKey_S` makes W forward
    int           pov;            // +20  the POV id (addAxisMapping's POV form)          [INFERRED]
    unsigned char flag24;         // +24  the POV form's first bool                        [UNRESOLVED]
    int           field28;        // +28  device flags of one half of a buttons/keys pair  [UNRESOLVED] which
    int           field2C;        // +2C  device flags of the other half
    int           field30;        // +30  button / key ids of the pair, four slots         [UNRESOLVED] assignment
    int           field34;        // +34
    int           field38;        // +38
    int           field3C;        // +3C
    float         value;          // +40  the current sampled value. Every resolver writes it and
                                  //      ControlMap::update copies it to AxisRemap::value.
    double        value48;        // +44 ELF / +48 PE  [UNRESOLVED] - no body read so far touches it
    unsigned char invert;         // +4C ELF / +50 PE  resolveAxisMapping negates the result when set
};
struct TriggerMapping {
    TriggerMapping();                               // four stores, then reset()
    void reset();                                   // zeroed, 10000.0f at the last float
    TriggerMapping& operator=(const TriggerMapping&); // every field
    int           device;         // +00  InputDeviceFlags
    int           type;           // +04  1 axis, 2 button, 3 key (resolveTriggerMapping)
    int           button;         // +08  the button form's InputDeviceButtons
    int           key;            // +0C  the key form's InputDeviceKeys
    int           axisOptions;    // +10  the axis form's AxisToAxisOptions (0x84925E0 writes,
                                  //      axisToTrigger reads) - now attested, was [INFERRED]
    int           axis;           // +14  InputDeviceAxes, same pair. Was `field14` / "the axis
                                  //      form's bool", which was wrong: it is the axis ID.
    unsigned char doubleTapLatched; // +18  buttonsToTrigger's latch: set while the double-tap arm
                                  //      is holding the trigger down. Was `flag18`.
    double        timer;          // +1C ELF / +20 PE  buttonsToTrigger's auto-repeat / double-tap
                                  //      accumulator, in seconds. A double - `fld qword ptr
                                  //      [esi+1Ch]` at 0x849344C. Was `value20`.
    float         threshold;      // +24 ELF / +28 PE  reset 10000.0f. buttonsToTrigger's mode
                                  //      selector and the auto-repeat period: <= 0 is level, >=
                                  //      10000 is edge, < 1000 is auto-repeat, else double-tap.
                                  //      Was `float28`.
};
struct AxisRemap {                // ELF 168 / PE 184 bytes; new'd with both mappings constructed, flag and value unwritten (retail)
    AxisRemap& operator=(const AxisRemap&);          // memberwise, as ControlMap::operator= inlines it
    AxisMapping primary;          // +00
    AxisMapping alternate;        // +50 ELF / +58 PE
    int         isAnalogue;       // +A0 ELF / +B0 PE  isAxisValueAnalogue
    float       value;            // +A4 ELF / +B4 PE  getAxisValue
};
struct TriggerRemap {             // ELF 84 / PE 104 bytes; the selector byte is not copied
    TriggerRemap& operator=(const TriggerRemap&);    // both mappings and the state byte only
    TriggerMapping primary;       // +00
    TriggerMapping alternate;     // +28 ELF / +30 PE  (the sub-record getTriggerSampleLatency selects)
    unsigned char  state;         // +50 ELF / +60 PE  getTriggerState
    unsigned char  selector;      // +51 ELF / +61 PE  getTriggerSampleLatency's test [UNRESOLVED] role
};

// A named, individually created control map; ControlMapManager owns the registry, not a singleton.
// Construction starts with one reference but leaves m_id uninitialized until setID.
// Reset owns/removes mapping records, including unassigned PINone trigger mappings.
// Use native map/vector storage and preserve the declared tuning-field order.
class ControlMap : public IControlMap
{
public:
    ControlMap();

    // IBase quartet, +0x00..+0x0C
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;       // 0x8496FA0  - _ZNK..., const

    // IControlMap, +0x10..+0xBC. order is frozen - see IO/Input/Input.h.
    virtual void  setName(const std::string& name);             // 0x8496FC0,   18   recovered
    virtual const std::string& getName() const;                 // 0x8496FE0,   11   recovered
    virtual int   getID() const;                                // 0x8497000,   11   recovered
    virtual void  setID(int id);                                // 0x8496FF0,   14   recovered
    virtual bool  axisExists(int axisId) const;                 // 0x8492D80,   59   recovered
    virtual bool  triggerExists(int triggerId) const;           // 0x8492DC0,   59   recovered

    // all ten return bool - see the note on IControlMap in Input.h. Sizes corrected against the
    // ELF function boundaries at the same time; the old ones were off for six of them.
    virtual bool  addAxisMapping(int axisId, int secondaryId, InputDeviceFlags device,
                                 InputDeviceAxes axis, bool invert, bool alternate); // 0x8491BF0, 149  recovered
    virtual bool  addAxisMapping(int axisId, InputDeviceFlags device,
                                 InputDeviceAxes axis, bool invert, bool alternate); // 0x8491C90,  67  recovered
    virtual bool  addAxisMapping(int axisId, InputDeviceFlags device,
                                 InputDeviceButtons positive,
                                 InputDeviceButtons negative, bool alternate);       // 0x8491F80, 359  recovered
    virtual bool  addAxisMapping(int axisId, InputDeviceFlags device,
                                 InputDeviceKeys positive,
                                 InputDeviceKeys negative, bool alternate);          // 0x84920F0, 388  recovered
    virtual bool  addAxisMapping(int axisId, int secondaryId, InputDeviceFlags device,
                                 InputDevicePOVs pov, bool a, bool b, bool c);       // 0x84919F0, 164  recovered
    virtual bool  addAxisMapping(int axisId, InputDeviceFlags device,
                                 InputDeviceButtons button, InputDeviceFlags device2,
                                 InputDeviceKeys key, bool alternate);               // 0x8491CE0,  73  recovered
    virtual bool  addAxisMapping(int axisId, InputDeviceFlags device,
                                 InputDeviceKeys key, InputDeviceFlags device2,
                                 InputDeviceButtons button, bool alternate);         // 0x8491D30,  76  recovered

    virtual bool  addTriggerMapping(int triggerId, InputDeviceFlags device,
                                    InputDeviceButtons button, float threshold,
                                    bool alternate);                                 // 0x8492280, 356  recovered
    virtual bool  addTriggerMapping(int triggerId, InputDeviceFlags device,
                                    InputDeviceKeys key, float threshold,
                                    bool alternate);                                 // 0x84923F0, 484  recovered
    virtual bool  addTriggerMapping(int triggerId, int secondaryId, InputDeviceFlags device,
                                    InputDeviceAxes axis, bool alternate,
                                    float f, float g);                               // 0x8492750, 173  recovered

    virtual void  removeMapping(unsigned int id);
    virtual void  reset();
    virtual void  reset(int id, bool positive, bool negative, bool alternate); // 0x8492AC0, 256  recovered

    virtual void  setButtonRiseTime(float t);                   // 0x8492BE0,   14   recovered
    virtual void  setButtonFallTime(float t);                   // 0x8492C00,   14   recovered
    virtual void  setDoubleTapTime(float t);                    // 0x8492BF0,   14   recovered

    virtual float getAxisValue(int axisId) const;               // 0x8492C10,   68   recovered
    virtual bool  isAxisValueAnalogue(int axisId) const;        // 0x8492C60,   69   recovered
    virtual int   getTriggerState(int triggerId) const;         // 0x8492CB0,   68   recovered
    virtual float getTriggerSampleLatency(int triggerId) const;

    virtual bool  validate();                                   // 0x8492BC0,   18   recovered
    virtual void  update(float deltaTime);

    virtual void  setInvertMouse(bool invert);                  // 0x8497010,   14   recovered
    virtual bool  getInvertMouse();                             // 0x8497020,   12   recovered
    virtual void  setMouseSensitivity(float s);                 // 0x8497030,   14   recovered
    virtual float getMouseSensitivity();                        // 0x8497040,   11   recovered
    virtual void  setKeyboardSensitivity(float s);              // 0x8497050,   14   recovered
    virtual float getKeyboardSensitivity();                     // 0x8497060,   11   recovered
    virtual void  setAxisScale(InputDeviceAxes axis, float scale);   // 0x8494770, 26  recovered
    virtual float getAxisScale(InputDeviceAxes axis) const;          // 0x8494790, 23  recovered
    virtual void  setYawFactor(float f);                        // 0x8497070,   14   recovered
    virtual void  setPitchFactor(float f);                      // 0x8497080,   14   recovered
    virtual float getYawFactor();                               // 0x8497090,   11   recovered
    virtual float getPitchFactor();                             // 0x84970A0,   11   recovered

    virtual void  save(std::ostream& out, bool b);
    virtual void  dump(bool b);
    virtual bool  findDuplicate(InputDeviceFlags& device, InputDeviceAxes& axis,
                                InputDeviceButtons& button, InputDeviceKeys& key,
                                DuplicateKeyVector& out);
    virtual bool  getInputMappedToMouseButton(bool b, PlayerInputMap& out);

    // +0xC0 / +0xC4
    virtual ~ControlMap();

    // Deep-copies remaps and tuning after reset. Name and ID are retained by the destination.
    ControlMap& operator=(const ControlMap& other);

    // The factory ClassManager::createInstance calls.
    // _ZN4dice3hfe2io10ControlMap6createEjPNS0_5IBaseE.
    static IBase* create(unsigned int iid, IBase* outer);

private:
    friend struct ControlMapFixture;
    // ControlSettings reads mappings to build binding descriptions.
    friend class ::dice::hfe::ControlSettings;

    // Non-virtual binding helpers and per-frame input resolvers.
    bool  addAxisMapping(AxisToAxisOptions options, int axisId, InputDeviceFlags device,
                         InputDeviceAxes axis, bool invert, bool alternate);
    bool  addAxisMapping(int axisId, InputDeviceFlags device, InputDevicePOVs pov,
                         bool a, bool b, bool alternate);
    // device2 identifies the second input device, paired with key.
    bool  _addAxisMapping(int axisId, InputDeviceFlags device, InputDeviceButtons button,
                          InputDeviceFlags device2, InputDeviceKeys key,
                          bool alternate, bool swapped);
    bool  addTriggerMapping(AxisToAxisOptions options, int triggerId, InputDeviceFlags device,
                            InputDeviceAxes axis, bool alternate, float f);

    float resolveAxisMapping(AxisMapping& mapping, float deltaTime);
    bool  resolveTriggerMapping(TriggerMapping& mapping, float deltaTime);
    float axisToAxis(IAdvancedInputDevice* device, AxisMapping& mapping);
    float buttonsToAxis(IInputDevice* device, AxisMapping& mapping, float deltaTime);
    float povToAxis(IInputDevice* device, AxisMapping& mapping, float deltaTime);
    bool  axisToTrigger(IAdvancedInputDevice* device, TriggerMapping& mapping);
    bool  buttonsToTrigger(IInputDevice* device, TriggerMapping& mapping, float deltaTime);

    /* 0x04 */ int         m_refCount;
    /* 0x08 */ std::string m_name;
    /* 0x0C */ int         m_id;                    // not initialised by the ctor - see above
    /* 0x10 */ std::map<int, AxisRemap*>    m_axisMappings;
    /* 0x1C */ std::map<int, TriggerRemap*> m_triggerMappings;
    /* 0x28 */ float       m_buttonRiseTime;
    /* 0x2C */ float       m_buttonFallTime;
    /* 0x30 */ float       m_doubleTapTime;
    /* 0x34 */ bool        m_invertMouse;
    /* 0x38 */ float       m_mouseSensitivity;
    /* 0x3C */ float       m_keyboardSensitivity;
    /* 0x40 */ float       m_axisScale[IDA_COUNT];  // four entries - the count is attested
    /* 0x50 */ float       m_yawFactor;
    /* 0x54 */ float       m_pitchFactor;
    /* 0x58 */ std::vector<TriggerMapping*> m_unassignedTriggerMappings;   // [INFERRED] name; see above
};

} // namespace io
} // namespace hfe
} // namespace dice

#endif // IO_INPUT_CONTROLMAP_H
