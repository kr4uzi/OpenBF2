// Input control maps and binding resolution. [INFERRED] Source placement.
// [CHOSEN-STUB] Remaining incomplete operations are marked at their definitions.

#include "ControlMap.h"
#include "../../Core/Ids.h"                // [INFERRED] header name; the constants are attested
#include <cmath>                          // fabsf - update's magnitude test and buttonsToAxis' decay
#include <ostream>                        // save writes .con source
#include <sstream>                        //   through an ostringstream per input id

namespace dice {
namespace hfe {
namespace io {

// Engine-owned device manager used by input resolvers.
extern IBase* inputDeviceManager;
// ds:0x8AC8C8C - save()'s per-input threshold comes from the manager through this global, not
// through anything ControlMap owns. Same shape, and the same reason, as the device manager above.
extern IBase* controlMapManager;
namespace {
IInputDeviceManager* deviceManager()
{
    return static_cast<IInputDeviceManager*>(static_cast<void*>(inputDeviceManager));
}
}

// Mapping records initialize and reset their runtime input state.

AxisMapping::AxisMapping()
{
    device = 0;
    type = 0;
    value = 0.0f;
    value48 = 0.0;
    invert = 0;
    reset();
}
void AxisMapping::reset()
{
    device = 0; type = 0; axisOptions = 0; axis = 0; positiveButton = 0; negativeButton = 0;
    positiveKey = 0; negativeKey = 0; pov = 0;
    flag24 = 0;
    field28 = 0; field2C = 0; field30 = 0; field34 = 0; field38 = 0; field3C = 0; value = 0.0f;
    value48 = 0.0;
    invert = 0;
}
AxisMapping& AxisMapping::operator=(const AxisMapping& o)
{
    device = o.device; type = o.type; axisOptions = o.axisOptions; axis = o.axis;
    positiveButton = o.positiveButton; negativeButton = o.negativeButton;
    positiveKey = o.positiveKey; negativeKey = o.negativeKey; pov = o.pov; flag24 = o.flag24;
    field28 = o.field28; field2C = o.field2C;
    field30 = o.field30; field34 = o.field34; field38 = o.field38; field3C = o.field3C; value = o.value;
    value48 = o.value48; invert = o.invert;
    return *this;
}
TriggerMapping::TriggerMapping()
{
    device = 0;
    type = 0;
    doubleTapLatched = 0;
    timer = 0.0;
    reset();
}
void TriggerMapping::reset()
{
    device = 0; type = 0; button = 0; key = 0; axisOptions = 0; axis = 0;
    doubleTapLatched = 0;
    timer = 0.0;
    threshold = 10000.0f;
}
TriggerMapping& TriggerMapping::operator=(const TriggerMapping& o)
{
    device = o.device; timer = o.timer; type = o.type; button = o.button; key = o.key; axisOptions = o.axisOptions;
    axis = o.axis; doubleTapLatched = o.doubleTapLatched; threshold = o.threshold;
    return *this;
}
// The remap copies as ControlMap::operator= inlines them: both mappings, then the extras.
AxisRemap& AxisRemap::operator=(const AxisRemap& o)
{
    primary = o.primary;
    alternate = o.alternate;
    isAnalogue = o.isAnalogue;
    value = o.value;
    return *this;
}
TriggerRemap& TriggerRemap::operator=(const TriggerRemap& o)
{
    primary = o.primary;
    alternate = o.alternate;
    state = o.state;
    return *this;
}

// constructor / destructor

// Initializes control tuning and empty mappings. The id remains uninitialized until explicitly assigned.
ControlMap::ControlMap()
    : m_refCount(1)
    , m_name()                          // the shared empty rep
    // m_id - not initialised. This omission is dice's and is deliberate here. Do not add it.
    , m_axisMappings()                  // 0x84907AB + header node alloc at 0x84907CE
    , m_triggerMappings()               // 0x8490801 + header node alloc at 0x8490808
    , m_buttonRiseTime(0.001f)
    , m_buttonFallTime(0.001f)
    , m_doubleTapTime(0.2f)
    , m_invertMouse(false)              // 0x849085D  (byte store)
    , m_mouseSensitivity(1.0f)
    , m_keyboardSensitivity(0.1f)
    , m_yawFactor(1.0f)
    , m_pitchFactor(1.0f)
    , m_unassignedTriggerMappings()
{
    // Initialize each axis scale.
    for (int i = 0; i < IDA_COUNT; ++i)
        m_axisScale[i] = 0.03f;
}

// Deletes owned axis and trigger remaps and unassigned mappings.
ControlMap::~ControlMap()
{
    for (std::map<int, AxisRemap*>::iterator it = m_axisMappings.begin(); it != m_axisMappings.end(); ++it)
        delete it->second;
    for (std::map<int, TriggerRemap*>::iterator it = m_triggerMappings.begin(); it != m_triggerMappings.end(); ++it)
        delete it->second;
    m_axisMappings.clear();
    m_triggerMappings.clear();
    for (std::vector<TriggerMapping*>::size_type i = 0; i < m_unassignedTriggerMappings.size(); ++i)
        delete m_unassignedTriggerMappings[i];
    m_unassignedTriggerMappings.resize(0);
}

// Clones mappings and input settings after reset, preserving this map's name and id.
ControlMap& ControlMap::operator=(const ControlMap& other)
{
    reset();
    for (std::map<int, AxisRemap*>::const_iterator it = other.m_axisMappings.begin(); it != other.m_axisMappings.end(); ++it) {
        AxisRemap* clone = new AxisRemap;
        *clone = *it->second;
        m_axisMappings.insert(std::make_pair(it->first, clone));
    }
    for (std::map<int, TriggerRemap*>::const_iterator it = other.m_triggerMappings.begin(); it != other.m_triggerMappings.end(); ++it) {
        TriggerRemap* clone = new TriggerRemap;
        *clone = *it->second;
        m_triggerMappings.insert(std::make_pair(it->first, clone));
    }
    m_buttonRiseTime = other.m_buttonRiseTime;
    m_buttonFallTime = other.m_buttonFallTime;
    m_doubleTapTime = other.m_doubleTapTime;
    m_invertMouse = other.m_invertMouse;
    m_mouseSensitivity = other.m_mouseSensitivity;
    m_keyboardSensitivity = other.m_keyboardSensitivity;
    m_yawFactor = other.m_yawFactor;
    m_pitchFactor = other.m_pitchFactor;
    for (int i = 0; i < IDA_COUNT; ++i)
        m_axisScale[i] = other.m_axisScale[i];
    for (std::vector<TriggerMapping*>::size_type i = 0; i < other.m_unassignedTriggerMappings.size(); ++i) {
        TriggerMapping* clone = new TriggerMapping;
        *clone = *other.m_unassignedTriggerMappings[i];
        m_unassignedTriggerMappings.push_back(clone);
    }
    return *this;
}

// IBase quartet - all four

// Core/IBase.h declares addRef() void.
void ControlMap::addRef()
{
    ++m_refCount;
}


int ControlMap::getRef()
{
    return m_refCount;
}

// Dispatches `(*(*this + 196))(this)` - vtable slot +0xC4, the deleting
// destructor - so this class does have a virtual destructor, unlike the Null* family.
int ControlMap::release()
{
    const int count = m_refCount - 1;
    const bool dead = (m_refCount == 1);
    m_refCount = count;

    if (dead)
        delete this;                    // virtual, slot +0xC4

    return count;
}

// The uniform two-id shape.
void* ControlMap::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<ControlMap*>(this);
    if (iid == IID_IControlMap)
        return const_cast<ControlMap*>(this);
    return 0;
}

// identity - all four

// - a tail call to std::string::assign, i.e. plain assignment.
void ControlMap::setName(const std::string& name)
{
    m_name = name;
}

// - `lea eax, [this+8] / ret`. Returns the member by reference.
const std::string& ControlMap::getName() const
{
    return m_name;
}

// Reads a member the constructor never wrote - see ControlMap.h.
int ControlMap::getID() const
{
    return m_id;
}


void ControlMap::setID(int id)
{
    m_id = id;
}

// Tests whether the requested axis or trigger mapping exists.


bool ControlMap::axisExists(int axisId) const
{
    return m_axisMappings.find(axisId) != m_axisMappings.end();
}


bool ControlMap::triggerExists(int triggerId) const
{
    return m_triggerMappings.find(triggerId) != m_triggerMappings.end();
}

// Adds a primary or alternate input binding. Returns false if an alternate has no primary mapping.

// The private axis form. Stores an axis source (type 1).
bool ControlMap::addAxisMapping(AxisToAxisOptions options, int axisId, InputDeviceFlags device,
                                InputDeviceAxes axis, bool invert, bool alternate)
{
    AxisRemap* remap;
    std::map<int, AxisRemap*>::iterator it = m_axisMappings.find(axisId);
    if (it != m_axisMappings.end())
    {
        remap = it->second;
    }
    else
    {
        if (alternate)
            return false;
        remap = new AxisRemap;                  // operator new(0xA8) then both AxisMapping ctors;
        if (!remap)                             //   isAnalogue and value stay unwritten, as shipped
            return false;                       // retail tests the allocation and answers false
        m_axisMappings[axisId] = remap;         // lower_bound + insert_unique
    }

    AxisMapping& mapping = alternate ? remap->alternate : remap->primary;
    mapping.type        = 1;
    mapping.device      = device;
    mapping.axis        = axis;
    mapping.axisOptions = options;
    mapping.positiveKey = 0;                    // the two KEY ids are cleared here, not the buttons
    mapping.negativeKey = 0;
    mapping.invert      = invert ? 1 : 0;
    return true;
}

// The public splitting form: the first id gets the positive half of the
// axis and the second the negative half. Both halves are attempted; the answer is their and, and
// the second call runs even when the first failed.
bool ControlMap::addAxisMapping(int axisId, int secondaryId, InputDeviceFlags device,
                                InputDeviceAxes axis, bool invert, bool alternate)
{
    const bool first  = addAxisMapping(ATAPositiveHalf, axisId, device, axis, invert, alternate);
    const bool second = addAxisMapping(ATANegativeHalf, secondaryId, device, axis, invert, alternate);
    return first && second;
}

// One id takes the whole axis - `ControlMap.addAxisToAxisMapping
// c_GIMouseLookX IDFMouse IDAxis_0 0 1`, the row the HUD pointer rides on.
bool ControlMap::addAxisMapping(int axisId, InputDeviceFlags device, InputDeviceAxes axis,
                                bool invert, bool alternate)
{
    return addAxisMapping(ATAWholeAxis, axisId, device, axis, invert, alternate);
}

// Buttons -> axis (the analog-from-digital path), type 2.
// the only overload that resets the record first, and only when it is changing the source type.
bool ControlMap::addAxisMapping(int axisId, InputDeviceFlags device,
                                InputDeviceButtons positive, InputDeviceButtons negative,
                                bool alternate)
{
    AxisRemap* remap;
    std::map<int, AxisRemap*>::iterator it = m_axisMappings.find(axisId);
    if (it != m_axisMappings.end())
    {
        remap = it->second;
    }
    else
    {
        if (alternate)
            return false;
        remap = new AxisRemap;
        if (!remap)
            return false;
        m_axisMappings[axisId] = remap;
    }

    AxisMapping& mapping = alternate ? remap->alternate : remap->primary;
    if (mapping.type != 2)
        mapping.reset();
    mapping.type   = 2;
    mapping.device = device;
    if (positive)                               // a zero id leaves the existing binding standing
        mapping.positiveButton = positive;
    if (negative)
        mapping.negativeButton = negative;
    mapping.invert = 0;
    return true;
}

// Keys -> axis (W/S becoming forward/back), type 3.
// first KEY is the positive one: `addKeysToAxisMapping c_PIThrottle IDFKeyboard IDKey_W IDKey_S`
// and buttonsToAxis drives +1.0 from the +0x18 slot this writes first.
bool ControlMap::addAxisMapping(int axisId, InputDeviceFlags device,
                                InputDeviceKeys positive, InputDeviceKeys negative, bool alternate)
{
    AxisRemap* remap;
    std::map<int, AxisRemap*>::iterator it = m_axisMappings.find(axisId);
    if (it != m_axisMappings.end())
    {
        remap = it->second;
    }
    else
    {
        if (alternate)
            return false;
        remap = new AxisRemap;
        if (!remap)
            return false;
        m_axisMappings[axisId] = remap;
    }

    AxisMapping& mapping = alternate ? remap->alternate : remap->primary;
    mapping.type   = 3;                         // no reset() here, unlike the buttons overload
    mapping.device = device;
    if (positive)
        mapping.positiveKey = positive;
    if (negative)
        mapping.negativeKey = negative;
    mapping.invert = 0;
    return true;
}

// POV hat -> axis, the splitting form. The two private calls differ only
// in the fourth argument (1 then 0) and both pass a literal false last.
bool ControlMap::addAxisMapping(int axisId, int secondaryId, InputDeviceFlags device,
                                InputDevicePOVs pov, bool /*a*/, bool b, bool /*c*/)
{
    const bool first  = addAxisMapping(axisId, device, pov, true, b, false);
    const bool second = addAxisMapping(secondaryId, device, pov, false, b, false);
    return first && second;
}

// / 0x8491D30, 76 bytes. The two mixed forms; both forward to the private
// _addAxisMapping with the pair swapped one way or the other.
bool ControlMap::addAxisMapping(int axisId, InputDeviceFlags device, InputDeviceButtons button,
                                InputDeviceFlags device2, InputDeviceKeys key, bool alternate)
{
    return _addAxisMapping(axisId, device, button, device2, key, alternate, false);
}

// The mirror form: the second device is passed first, so the private body's swapped arm stores the
// pair crossed over. 0x8491D30 pushes axisId, device2, button, device, key, alternate, true.
bool ControlMap::addAxisMapping(int axisId, InputDeviceFlags device, InputDeviceKeys key,
                                InputDeviceFlags device2, InputDeviceButtons button, bool alternate)
{
    return _addAxisMapping(axisId, device2, button, device, key, alternate, true);
}

// Button -> trigger, type 2. no PINone arm, unlike the key form below.
bool ControlMap::addTriggerMapping(int triggerId, InputDeviceFlags device,
                                   InputDeviceButtons button, float threshold, bool alternate)
{
    TriggerRemap* remap;
    std::map<int, TriggerRemap*>::iterator it = m_triggerMappings.find(triggerId);
    if (it != m_triggerMappings.end())
    {
        remap = it->second;
    }
    else
    {
        if (alternate)
            return false;
        remap = new TriggerRemap;               // operator new(0x54) then both TriggerMapping ctors
        if (!remap)
            return false;
        m_triggerMappings[triggerId] = remap;
    }

    TriggerMapping& mapping = alternate ? remap->alternate : remap->primary;
    mapping.type      = 2;
    mapping.device    = device;
    mapping.button    = button;
    mapping.threshold = threshold;
    return true;
}

// Key -> trigger, type 3 - 181 of `Controls.con`'s 242 mapping rows.
// the PINone ARM: a row bound to id 64 is not a binding at all, it is a lone TriggerMapping
// parked in m_unassignedTriggerMappings. That vector is what ControlMap::reset frees and
// operator= clones, and it is why its [INFERRED] name is what it is.
bool ControlMap::addTriggerMapping(int triggerId, InputDeviceFlags device, InputDeviceKeys key,
                                   float threshold, bool alternate)
{
    if (triggerId == PINone)
    {
        TriggerMapping* unassigned = new TriggerMapping;
        unassigned->device = device;
        unassigned->key    = key;               // neither the type nor the threshold is set
        m_unassignedTriggerMappings.push_back(unassigned);
        return true;
    }

    TriggerRemap* remap;
    std::map<int, TriggerRemap*>::iterator it = m_triggerMappings.find(triggerId);
    if (it != m_triggerMappings.end())
    {
        remap = it->second;
    }
    else
    {
        if (alternate)
            return false;
        remap = new TriggerRemap;
        if (!remap)
            return false;
        m_triggerMappings[triggerId] = remap;
    }

    TriggerMapping& mapping = alternate ? remap->alternate : remap->primary;
    mapping.type      = 3;
    mapping.device    = device;
    mapping.key       = key;
    mapping.threshold = threshold;
    return true;
}

// The private trigger form: an axis source, type 1.
bool ControlMap::addTriggerMapping(AxisToAxisOptions options, int triggerId,
                                   InputDeviceFlags device, InputDeviceAxes axis, bool alternate,
                                   float /*f*/)
{
    TriggerRemap* remap;
    std::map<int, TriggerRemap*>::iterator it = m_triggerMappings.find(triggerId);
    if (it != m_triggerMappings.end())
    {
        remap = it->second;
    }
    else
    {
        if (alternate)
            return false;
        remap = new TriggerRemap;
        if (!remap)
            return false;
        m_triggerMappings[triggerId] = remap;
    }

    TriggerMapping& mapping = alternate ? remap->alternate : remap->primary;
    mapping.type        = 1;
    mapping.device      = device;
    mapping.axis        = axis;
    mapping.threshold   = 0.0f;                 // the float argument is not what lands here
    mapping.axisOptions = options;
    return true;
}

// Axis -> trigger, the splitting form - the mouse wheel. -1 means
// "this half is not wanted" and counts as success; `addAxisToTriggerMapping c_GIMouseWheelUp -1
// IDFFalcon IDAxis_2 0` binds only the positive half.
bool ControlMap::addTriggerMapping(int triggerId, int secondaryId, InputDeviceFlags device,
                                   InputDeviceAxes axis, bool alternate, float f, float /*g*/)
{
    bool first = true;
    if (triggerId != -1)
        first = addTriggerMapping(ATAPositiveHalf, triggerId, device, axis, alternate, f);
    bool second = true;
    if (secondaryId != -1)
        second = addTriggerMapping(ATANegativeHalf, secondaryId, device, axis, alternate, f);
    return first && second;
}

// The private mixed form: one half of the pair comes from one device and
// the other half from another, so the record carries two device-flags values (+0x28 / +0x2C) and
// four ids (+0x30..+0x3C). Type 5. `swapped` decides which pair of id slots is written, and the
// swapped arm writes the two device slots crossed over.
bool ControlMap::_addAxisMapping(int axisId, InputDeviceFlags device, InputDeviceButtons button,
                                 InputDeviceFlags device2, InputDeviceKeys key,
                                 bool alternate, bool swapped)
{
    AxisRemap* remap;
    std::map<int, AxisRemap*>::iterator it = m_axisMappings.find(axisId);
    if (it != m_axisMappings.end())
    {
        remap = it->second;
    }
    else
    {
        if (alternate)
            return false;
        remap = new AxisRemap;
        if (!remap)
            return false;
        m_axisMappings[axisId] = remap;
    }

    AxisMapping& mapping = alternate ? remap->alternate : remap->primary;
    mapping.type   = 5;
    mapping.device = device;                    // stored before the swap test, from the first device
    if (swapped)
    {
        mapping.field2C = device;
        mapping.field28 = device2;
        mapping.field3C = button;
        mapping.field38 = key;
    }
    else
    {
        mapping.field28 = device;
        mapping.field2C = device2;
        mapping.field30 = button;
        mapping.field34 = key;
    }
    if (alternate)
    {
        mapping.positiveKey = 0;                // the alternate arm also clears the two key ids
        mapping.negativeKey = 0;                //   (+0x68 / +0x6C of the record); the primary
    }                                           //   arm does not. Shipped asymmetry.
    mapping.invert = 0;
    return true;
}

// [CHOSEN-STUB] POV mapping requires the undeclared advanced-device tail interface.
// [RETIRE-WHEN: controlmap-pov] Implement after the IAdvancedInputDevice POV slots are established.
bool ControlMap::addAxisMapping(int /*axisId*/, InputDeviceFlags /*device*/, InputDevicePOVs /*pov*/,
                                bool /*a*/, bool /*b*/, bool /*alternate*/)
{
    return false;
}

float ControlMap::povToAxis(IInputDevice* /*device*/, AxisMapping& /*mapping*/, float /*deltaTime*/)
{
    return 0.0f;
}

// Samples devices and updates mapped axis values and trigger states for the input tick.


void ControlMap::update(float deltaTime)
{
    if (!m_axisMappings.empty())
    {
        for (std::map<int, AxisRemap*>::iterator it = m_axisMappings.begin();
             it != m_axisMappings.end(); ++it)
        {
            AxisRemap* const remap = it->second;
            if (!remap->primary.device)                     // an unbound primary skips the record
                continue;

            remap->value      = resolveAxisMapping(remap->primary, deltaTime);
            remap->isAnalogue = (remap->primary.type == 1);

            if (!remap->alternate.device)
                continue;

            const float alternate = resolveAxisMapping(remap->alternate, deltaTime);
            // The magnitude test is an andnps against 0x7FFFFFFF - fabs on both sides.
            bool takeAlternate = (fabsf(remap->value) < 0.0001f && fabsf(alternate) > 0.0001f);
            if (!takeAlternate)
            {
                // Retail asks the manager twice and never guards the first answer.
                IInputDevice* const device = deviceManager()->getInputDevice(static_cast<InputDeviceFlags>(remap->primary.device));
                takeAlternate = !device
                             || !deviceManager()->getInputDevice(static_cast<InputDeviceFlags>(remap->primary.device))->isActive();
            }
            if (takeAlternate)
            {
                remap->isAnalogue = (remap->alternate.type == 1);
                remap->value      = alternate;
            }
        }
    }

    if (!m_triggerMappings.empty())
    {
        for (std::map<int, TriggerRemap*>::iterator it = m_triggerMappings.begin();
             it != m_triggerMappings.end(); ++it)
        {
            TriggerRemap* const remap = it->second;
            if (!remap->primary.device)
                continue;

            remap->state    = resolveTriggerMapping(remap->primary, deltaTime) ? 1 : 0;
            remap->selector = 1;                            // written unconditionally, then unread here

            if (!remap->alternate.device)
                continue;

            const bool alternate = resolveTriggerMapping(remap->alternate, deltaTime);
            if (remap->state || alternate)                  // the two bindings or together
                remap->state = 1;
        }
    }
}

// The type-5 arm answers zero when either half's device is missing,
// and it reaches the invert test anyway - harmlessly, since the value is zero.
float ControlMap::resolveAxisMapping(AxisMapping& mapping, float deltaTime)
{
    float value = 0.0f;
    IInputDevice* const device = deviceManager()->getInputDevice(static_cast<InputDeviceFlags>(mapping.device));
    if (device)
    {
        switch (mapping.type)
        {
        case 1:
            value = axisToAxis(static_cast<IAdvancedInputDevice*>(device), mapping);
            break;
        case 2:
        case 3:
            value = buttonsToAxis(device, mapping, deltaTime);
            break;
        case 4:
            value = povToAxis(device, mapping, deltaTime);
            break;
        case 5:
        {
            IInputDevice* const half = deviceManager()->getInputDevice(static_cast<InputDeviceFlags>(mapping.field28));
            if (deviceManager()->getInputDevice(static_cast<InputDeviceFlags>(mapping.field2C)) && half)
                value = buttonsToAxis(device, mapping, deltaTime);
            break;
        }
        default:
            break;
        }
    }
    return mapping.invert ? -value : value;
}

// The type range test is `>= 1 && <= 3` after the ==1 arm has left, so
// only 2 and 3 reach buttonsToTrigger; anything else answers false.
bool ControlMap::resolveTriggerMapping(TriggerMapping& mapping, float deltaTime)
{
    IInputDevice* const device = deviceManager()->getInputDevice(static_cast<InputDeviceFlags>(mapping.device));
    if (!device)
        return false;

    if (mapping.type == 1)
    {
        IAdvancedInputDevice* const advanced =
            static_cast<IAdvancedInputDevice*>(device->queryInterface(IID_IAdvancedInputDevice));
        return axisToTrigger(advanced, mapping);
    }
    if (mapping.type >= 1 && mapping.type <= 3)
        return buttonsToTrigger(device, mapping, deltaTime);
    return false;
}

// The mouse-look path: read the axis, apply invert, mouse sensitivity and
// the yaw/pitch ratio, then clamp to whichever half of the axis this binding wanted.
// The invert is done by asking the device to invert around the read, not by negating here.
float ControlMap::axisToAxis(IAdvancedInputDevice* device, AxisMapping& mapping)
{
    float sign = 1.0f;
    if (m_invertMouse && mapping.device == IDFMouse)
        sign = (mapping.axis == 1) ? -1.0f : 1.0f;

    if (sign < 0.0f)
        device->setInvertAxis(static_cast<InputDeviceAxes>(1), true);
    mapping.value = device->readAxis(static_cast<InputDeviceAxes>(mapping.axis), true);
    if (sign < 0.0f)
        device->setInvertAxis(static_cast<InputDeviceAxes>(1), false);

    if (mapping.device == IDFMouse)
    {
        mapping.value = m_mouseSensitivity * mapping.value;
        // Both ratio arms re-read the two factors; neither is symmetric with the other.
        if (m_yawFactor > m_pitchFactor && !mapping.axis)
            mapping.value = mapping.value * (m_yawFactor / m_pitchFactor);
        if (m_pitchFactor > m_yawFactor && mapping.axis == 1)
            mapping.value = (m_pitchFactor / m_yawFactor) * mapping.value;
    }

    if (mapping.axisOptions == ATAPositiveHalf)
    {
        mapping.value = (mapping.value >= 0.0f) ? mapping.value : 0.0f;
    }
    else if (mapping.axisOptions == ATANegativeHalf)
    {
        mapping.value = (mapping.value <= 0.0f) ? -mapping.value : 0.0f;
    }
    // ATAWholeAxis returns what the device gave, without the re-store the two halves make.
    return mapping.value;
}

// Digital source -> analogue axis: ramp toward +/-1 at the rise rate
// while a side is held, and decay toward zero at the fall rate when neither is.
float ControlMap::buttonsToAxis(IInputDevice* device, AxisMapping& mapping, float deltaTime)
{
    bool positive;
    bool negative;
    if (mapping.type == 5)
    {
        // The mixed form asks the manager for each half's own device, and dereferences the answers
        // with no null test - reproduced.
        IInputDevice* const first  = deviceManager()->getInputDevice(static_cast<InputDeviceFlags>(mapping.field28));
        IInputDevice* const second = deviceManager()->getInputDevice(static_cast<InputDeviceFlags>(mapping.field2C));
        if (mapping.field28 == IDFKeyboard)
            positive = static_cast<IKeyboardDevice*>(first)->keyDown(static_cast<InputDeviceKeys>(mapping.field38));
        else
            positive = static_cast<IAdvancedInputDevice*>(device)->buttonDown(static_cast<InputDeviceButtons>(mapping.field30));
        if (mapping.field2C == IDFKeyboard)
            negative = static_cast<IKeyboardDevice*>(second)->keyDown(static_cast<InputDeviceKeys>(mapping.field34));
        else
            negative = static_cast<IAdvancedInputDevice*>(device)->buttonDown(static_cast<InputDeviceButtons>(mapping.field3C));
    }
    else if (device->getType() != IDTKeyboard)
    {
        positive = static_cast<IAdvancedInputDevice*>(device)->buttonDown(static_cast<InputDeviceButtons>(mapping.positiveButton));
        negative = static_cast<IAdvancedInputDevice*>(device)->buttonDown(static_cast<InputDeviceButtons>(mapping.negativeButton));
    }
    else
    {
        positive = static_cast<IKeyboardDevice*>(device)->keyDown(static_cast<InputDeviceKeys>(mapping.positiveKey));
        negative = static_cast<IKeyboardDevice*>(device)->keyDown(static_cast<InputDeviceKeys>(mapping.negativeKey));
    }

    if (positive)
    {
        mapping.value = mapping.value + deltaTime / m_buttonRiseTime;
        if (mapping.value > 1.0f)
            mapping.value = 1.0f;
        return mapping.value;
    }
    if (negative)
    {
        // The falling branch also uses rise time; retain this asymmetric timing.
        mapping.value = mapping.value - deltaTime / m_buttonRiseTime;
        if (mapping.value < -1.0f)
            mapping.value = -1.0f;
        return mapping.value;
    }

    float sign = 1.0f;
    if (mapping.value <= 0.0f)
        sign = (mapping.value >= 0.0f) ? 0.0f : -1.0f;
    float magnitude = fabsf(mapping.value) - deltaTime / m_buttonFallTime;
    if (magnitude < 0.0f)
        magnitude = 0.0f;
    mapping.value = sign * magnitude;
    return mapping.value;
}

// An axis becomes a trigger when it leaves zero in the wanted direction.
bool ControlMap::axisToTrigger(IAdvancedInputDevice* device, TriggerMapping& mapping)
{
    if (mapping.threshold > 0.0f)
        return false;

    const int type = device->getType();
    if (type == IDTGameController)
        return static_cast<IAdvancedInputDevice*>(device)->buttonDown(static_cast<InputDeviceButtons>(mapping.button));
    if (type > IDTGameController)
    {
        if (type != IDTKeyboard)
            return false;
        return static_cast<IKeyboardDevice*>(static_cast<IInputDevice*>(device))
                   ->keyDown(static_cast<InputDeviceKeys>(mapping.key));
    }
    if (type != IDTMouse)
        return false;

    float value = device->readAxis(static_cast<InputDeviceAxes>(mapping.axis), true);
    if (m_invertMouse && mapping.axis == 1)
        value = -value;
    // Only 2 is special. 0, 1 and anything above 2 all mean "fires on the positive side" - the
    // three jumps the compiler emits before that test all land on the same block (0x849365F).
    if (mapping.axisOptions == ATANegativeHalf)
        return 0.0f > value;
    return value > 0.0f;
}

// Resolves a button trigger according to its threshold: held (<=0), edge (>=10000),
// auto-repeat (<1000), or double-tap (remaining values).
bool ControlMap::buttonsToTrigger(IInputDevice* device, TriggerMapping& mapping, float deltaTime)
{
    const bool keyboard = (device->getType() == IDTKeyboard);

    if (mapping.threshold <= 0.0f)
    {
        if (keyboard)
            return static_cast<IKeyboardDevice*>(device)->keyDown(static_cast<InputDeviceKeys>(mapping.key));
        return static_cast<IAdvancedInputDevice*>(device)->buttonDown(static_cast<InputDeviceButtons>(mapping.button));
    }

    if (mapping.threshold >= 10000.0f)
    {
        if (keyboard)
            return static_cast<IKeyboardDevice*>(device)->keyPressed(static_cast<InputDeviceKeys>(mapping.key));
        return static_cast<IAdvancedInputDevice*>(device)->buttonPressed(static_cast<InputDeviceButtons>(mapping.button));
    }

    bool state;
    if (mapping.threshold < 1000.0f)
    {
        const bool held = keyboard
            ? static_cast<IKeyboardDevice*>(device)->keyDown(static_cast<InputDeviceKeys>(mapping.key))
            : static_cast<IAdvancedInputDevice*>(device)->buttonDown(static_cast<InputDeviceButtons>(mapping.button));
        state = false;
        if (held)
        {
            bool fire = (mapping.timer <= 0.0);
            if (!fire)
            {
                fire = keyboard
                    ? static_cast<IKeyboardDevice*>(device)->keyPressed(static_cast<InputDeviceKeys>(mapping.key))
                    : static_cast<IAdvancedInputDevice*>(device)->buttonPressed(static_cast<InputDeviceButtons>(mapping.button));
            }
            if (fire)
            {
                state = true;
                mapping.timer = mapping.threshold;
            }
        }
        mapping.timer = mapping.timer - deltaTime;   // every path of this arm counts down
        return state;
    }

    if (mapping.doubleTapLatched)
    {
        state = keyboard
            ? static_cast<IKeyboardDevice*>(device)->keyDown(static_cast<InputDeviceKeys>(mapping.key))
            : static_cast<IAdvancedInputDevice*>(device)->buttonDown(static_cast<InputDeviceButtons>(mapping.button));
    }
    else if (keyboard)
    {
        state = false;
        if (static_cast<IKeyboardDevice*>(device)->keyPressed(static_cast<InputDeviceKeys>(mapping.key)))
        {
            const double elapsed = mapping.timer;
            mapping.timer = 0.0;
            state = (m_doubleTapTime >= elapsed);
        }
    }
    else
    {
        state = false;
        if (static_cast<IAdvancedInputDevice*>(device)->buttonPressed(static_cast<InputDeviceButtons>(mapping.button)))
        {
            const double elapsed = mapping.timer;
            mapping.timer = 0.0;
            // A literal 0.1 here, not m_doubleTapTime - the keyboard arm two lines up uses the
            // member. Shipped asymmetry, reproduced.
            state = (0.1 >= elapsed);
        }
    }
    mapping.doubleTapLatched = state ? 1 : 0;
    mapping.timer = mapping.timer + deltaTime;       // this arm counts UP
    return state;
}

// [CHOSEN-STUB]
void ControlMap::removeMapping(unsigned int /*id*/)
{
}

// the trigger remaps are freed and their tree
// cleared, then the axis remaps, then the unassigned mappings (the vector emptied through the
// erase / fill-insert pair, i.e. resize(0)); the four axis scales, the invert flag, yaw, pitch and
// the two sensitivities return to their constructor values. The three times are left alone.
void ControlMap::reset()
{
    for (std::map<int, TriggerRemap*>::iterator it = m_triggerMappings.begin(); it != m_triggerMappings.end(); ++it)
        delete it->second;
    m_triggerMappings.clear();
    for (std::map<int, AxisRemap*>::iterator it = m_axisMappings.begin(); it != m_axisMappings.end(); ++it)
        delete it->second;
    m_axisMappings.clear();
    for (std::vector<TriggerMapping*>::size_type i = 0; i < m_unassignedTriggerMappings.size(); ++i)
        delete m_unassignedTriggerMappings[i];
    m_unassignedTriggerMappings.resize(0);
    for (int i = 0; i < IDA_COUNT; ++i)
        m_axisScale[i] = 0.03f;
    m_invertMouse = false;
    m_yawFactor = 1.0f;
    m_pitchFactor = 1.0f;
    m_mouseSensitivity = 1.0f;
    m_keyboardSensitivity = 0.1f;
}

// ELF 0x8492AC0. Clears one binding of input id (mapNextInput's Escape). A trigger loses its
// primary or alternate mapping whole. An axis loses its positive half, its negative half, or (both
// set) everything; once neither half holds a key or button the mapping also drops its device and
// axis (15 = none).
void ControlMap::reset(int id, bool positive, bool negative, bool alternate)
{
    std::map<int, TriggerRemap*>::iterator t = m_triggerMappings.find(id);
    if (t != m_triggerMappings.end())
    {
        (alternate ? t->second->alternate : t->second->primary).reset();
        return;
    }
    std::map<int, AxisRemap*>::iterator a = m_axisMappings.find(id);
    if (a == m_axisMappings.end())
        return;
    AxisMapping& m = alternate ? a->second->alternate : a->second->primary;
    if (positive)
    {
        if (negative)
        {
            m.reset();
            return;
        }
        m.positiveKey = 0;
        m.positiveButton = 0;
    }
    else if (negative)
    {
        m.negativeKey = 0;
        m.negativeButton = 0;
    }
    if (m.positiveKey == 0 && m.negativeKey == 0 && m.positiveButton == 0 && m.negativeButton == 0)
    {
        m.axis = 15;
        m.device = 0;
    }
}


void ControlMap::setButtonRiseTime(float t)
{
    m_buttonRiseTime = t;
}


void ControlMap::setButtonFallTime(float t)
{
    m_buttonFallTime = t;
}


void ControlMap::setDoubleTapTime(float t)
{
    m_doubleTapTime = t;
}

// Returns sampled input state, using neutral values for unbound ids.


// The hit path is `return *(remap + 164)` - the value lives inside the AxisRemap, at +0xA4.
float ControlMap::getAxisValue(int axisId) const
{
    std::map<int, AxisRemap*>::const_iterator it = m_axisMappings.find(axisId);
    if (it == m_axisMappings.end())
        return 0.0f;

    return it->second->value;           // +0xA4
}


// The hit path is `return *(remap + 160)` - AxisRemap's +0xA0. Same stub caveat as getAxisValue.
bool ControlMap::isAxisValueAnalogue(int axisId) const
{
    std::map<int, AxisRemap*>::const_iterator it = m_axisMappings.find(axisId);
    if (it == m_axisMappings.end())
        return false;

    return it->second->isAnalogue != 0; // +0xA0
}


// The hit path is `return *(remap + 80)` - TriggerRemap's +0x50.
int ControlMap::getTriggerState(int triggerId) const
{
    std::map<int, TriggerRemap*>::const_iterator it = m_triggerMappings.find(triggerId);
    if (it == m_triggerMappings.end())
        return 0;

    return it->second->state;           // +0x50
}

// [CHOSEN-STUB] Returns zero latency. The original live-trigger path has an unresolved floating-point result.
float ControlMap::getTriggerSampleLatency(int /*triggerId*/) const
{
    return 0.0f;                        // [CHOSEN-STUB] - see the shipped defect note above
}

// Returns true without validating mappings.
bool ControlMap::validate()
{
    return true;
}

void ControlMap::setInvertMouse(bool invert)
{
    m_invertMouse = invert;
}

// (byte load).
bool ControlMap::getInvertMouse()
{
    return m_invertMouse;
}


void ControlMap::setMouseSensitivity(float s)
{
    m_mouseSensitivity = s;
}


float ControlMap::getMouseSensitivity()
{
    return m_mouseSensitivity;
}


void ControlMap::setKeyboardSensitivity(float s)
{
    m_keyboardSensitivity = s;
}


float ControlMap::getKeyboardSensitivity()
{
    return m_keyboardSensitivity;
}

// Sets the scale for an axis. Requires an index in the four-element axis range.
void ControlMap::setAxisScale(InputDeviceAxes axis, float scale)
{
    m_axisScale[axis] = scale;
}

float ControlMap::getAxisScale(InputDeviceAxes axis) const
{
    return m_axisScale[axis];
}


void ControlMap::setYawFactor(float f)
{
    m_yawFactor = f;
}


void ControlMap::setPitchFactor(float f)
{
    m_pitchFactor = f;
}


float ControlMap::getYawFactor()
{
    return m_yawFactor;
}


float ControlMap::getPitchFactor()
{
    return m_pitchFactor;
}

// Writes the control map as .con commands. A map without axis or trigger remaps writes nothing.
void ControlMap::save(std::ostream& out, bool playerMap)
{
    if (m_axisMappings.empty() && m_triggerMappings.empty())
        return;

    out << "ControlMap.create " << getName() << std::endl;   // getName is the virtual, slot +0x14

    // The axis remaps, in ascending input id (std::map order)
    for (std::map<int, AxisRemap*>::const_iterator it = m_axisMappings.begin();
         it != m_axisMappings.end(); ++it)
    {
        const AxisRemap* const remap = it->second;

        // The id is spelled through its own enum printer - `c_` plus the name is a console
        // constant, which is exactly what the reader on the other side expects.
        std::ostringstream name;
        if (playerMap)
            name << static_cast<PlayerInputMap>(it->first);
        else
            name << static_cast<GameInputMap>(it->first);

        for (int i = 0; i <= 1; ++i)
        {
            const AxisMapping& mapping = i ? remap->alternate : remap->primary;
            if (!mapping.device)
                continue;

            // the first test is the device, not the type. A keyboard source is written as the
            // keys form whatever its type says.
            if (mapping.device == IDFKeyboard)
            {
                out << "ControlMap.addKeysToAxisMapping " << "c_" << name.str() << " "
                    << static_cast<InputDeviceFlags>(mapping.device) << " "
                    << static_cast<InputDeviceKeys>(mapping.positiveKey) << " "
                    << static_cast<InputDeviceKeys>(mapping.negativeKey) << " "
                    << i << std::endl;
                continue;
            }

            if (mapping.type == 1)
            {
                out << "ControlMap.addAxisToAxisMapping " << "c_" << name.str() << " "
                    << static_cast<InputDeviceFlags>(mapping.device) << " "
                    << static_cast<InputDeviceAxes>(mapping.axis) << " "
                    << static_cast<int>(mapping.invert);
            }
            else if (mapping.type == 2)
            {
                out << "ControlMap.addButtonsToAxisMapping " << "c_" << name.str() << " "
                    << static_cast<InputDeviceFlags>(mapping.device) << " "
                    << static_cast<InputDeviceButtons>(mapping.positiveButton) << " "
                    << static_cast<InputDeviceButtons>(mapping.negativeButton);
            }
            else
            {
                continue;   // types 4 and 5 have no writer
            }
            out << " " << i << std::endl;   // the shared tail of the two non-keyboard forms
        }
    }

    // The trigger remaps
    for (std::map<int, TriggerRemap*>::const_iterator it = m_triggerMappings.begin();
         it != m_triggerMappings.end(); ++it)
    {
        const TriggerRemap* const remap = it->second;

        // Serializes the manager's per-input repeat default rather than the mapping's stored threshold.
        double repeat = static_cast<IControlMapManager*>(
            static_cast<void*>(controlMapManager))->getRepeatRate(it->first, playerMap);

        std::ostringstream name;
        if (playerMap)
            name << static_cast<PlayerInputMap>(it->first);
        else
            name << static_cast<GameInputMap>(it->first);

        for (int j = 0; j <= 1; ++j)
        {
            const TriggerMapping& mapping = j ? remap->alternate : remap->primary;
            if (!mapping.device)
                continue;

            // The exception: a double-tap binding keeps its own 1000. `repeat` is not reset
            // between the primary and the alternate, so a primary that is a double-tap drags its
            // 1000 onto the alternate row as well. Shipped, reproduced.
            if (mapping.threshold == 1000.0f)
                repeat = 1000.0;

            if (mapping.device == IDFKeyboard)
            {
                out << "ControlMap.addKeyToTriggerMapping " << "c_" << name.str() << " "
                    << static_cast<InputDeviceFlags>(mapping.device) << " "
                    << static_cast<InputDeviceKeys>(mapping.key) << " "
                    << repeat << " " << j << std::endl;
                continue;
            }

            if (mapping.type == 1)
            {
                // The unused axis half is written as -1 before or after the mapped name.
                out << "ControlMap.addAxisToTriggerMapping "
                    << (mapping.axisOptions == ATANegativeHalf ? "-1 " : "")
                    << "c_" << name.str() << " "
                    << (mapping.axisOptions == ATAPositiveHalf ? "-1 " : "")
                    << static_cast<InputDeviceFlags>(mapping.device) << " "
                    << static_cast<InputDeviceAxes>(mapping.axis) << " "
                    << j << std::endl;       // this form writes no threshold
                continue;
            }

            if (mapping.type != 2)
                continue;

            out << "ControlMap.addButtonToTriggerMapping " << "c_" << name.str() << " "
                << static_cast<InputDeviceFlags>(mapping.device) << " "
                << static_cast<InputDeviceButtons>(mapping.button) << " "
                << repeat << " " << j << std::endl;
        }
    }

    // The unassigned mappings, all written as c_PINone with two literal zeros
    for (std::vector<TriggerMapping*>::size_type k = 0; k < m_unassignedTriggerMappings.size(); ++k)
    {
        const TriggerMapping* const mapping = m_unassignedTriggerMappings[k];
        out << "ControlMap.addKeyToTriggerMapping " << "c_PINone "
            << static_cast<InputDeviceFlags>(mapping->device) << " "
            << static_cast<InputDeviceKeys>(mapping->key) << " "
            << 0 << " " << 0 << std::endl;
    }

    // The feel scalars, each written only when it differs from its constructor default
    if (m_invertMouse)
        out << "ControlMap.invertMouse " << static_cast<int>(m_invertMouse) << std::endl;
    if (m_mouseSensitivity != 1.0f)
        out << "ControlMap.mouseSensitivity " << static_cast<double>(m_mouseSensitivity) << std::endl;
    if (m_keyboardSensitivity != 0.1f)
        out << "ControlMap.keyboardSensitivity " << static_cast<double>(m_keyboardSensitivity) << std::endl;
    if (m_yawFactor != 1.0f)
        out << "ControlMap.setYawFactor " << static_cast<double>(m_yawFactor) << std::endl;
    if (m_pitchFactor != 1.0f)
        out << "ControlMap.setPitchFactor " << static_cast<double>(m_pitchFactor) << std::endl;
    for (int axis = 0; axis < IDA_COUNT; ++axis)
    {
        if (m_axisScale[axis] != 0.03f)
            out << "ControlMap.setAxisScale " << axis << " "
                << static_cast<double>(m_axisScale[axis]) << std::endl;
    }

    out << std::endl;   // the blank line that separates one map's block from the next
}

// [CHOSEN-STUB] - a debug print, distinct from save().
void ControlMap::dump(bool /*b*/)
{
}

// ELF 0x8494960 (4595 bytes). Collects every binding of this map that already uses the device
// input (device, axis, button, key) into out and answers whether there was any. A keyboard key
// held by an unassigned trigger answers true at once with nothing collected. Keyboard bindings
// are matched on the key alone (the device argument is not consulted for them); other bindings
// need the same device and the axis (15 = none) or button (0 = none). Triggers whose threshold
// is 1000 (the double-tap companions, e.g. c_PIAltSprint) never count as duplicates.
bool ControlMap::findDuplicate(InputDeviceFlags& device, InputDeviceAxes& axis,
                               InputDeviceButtons& button, InputDeviceKeys& key,
                               DuplicateKeyVector& out)
{
    if (device == 2)                                    // IDFKeyboard
    {
        for (unsigned int i = 0; i < m_unassignedTriggerMappings.size(); ++i)
            if (m_unassignedTriggerMappings[i]->key == key)
                return true;
    }

    for (std::map<int, AxisRemap*>::iterator it = m_axisMappings.begin(); it != m_axisMappings.end(); ++it)
    {
        for (int slot = 0; slot < 2; ++slot)
        {
            const AxisMapping& m = slot != 0 ? it->second->alternate : it->second->primary;
            if (m.device == 0)
                continue;
            if (m.device == 2)
            {
                if (key == 0)
                    continue;
                if (key == m.positiveKey)
                {
                    out.ids.push_back(it->first);
                    out.negative.push_back(false);
                    out.alternate.push_back(slot > 0);
                }
                else if (key == m.negativeKey)
                {
                    out.ids.push_back(it->first);
                    out.negative.push_back(true);
                    out.alternate.push_back(slot > 0);
                }
            }
            else if (device == m.device)
            {
                if (axis != 15 && axis == m.axis)
                {
                    out.ids.push_back(it->first);
                    out.negative.push_back(false);
                    out.alternate.push_back(slot > 0);
                }
                else if (button != 0)
                {
                    if (button == m.positiveButton)
                    {
                        out.ids.push_back(it->first);
                        out.negative.push_back(false);
                        out.alternate.push_back(slot > 0);
                    }
                    else if (button == m.negativeButton)
                    {
                        out.ids.push_back(it->first);
                        out.negative.push_back(true);
                        out.alternate.push_back(slot > 0);
                    }
                }
            }
        }
    }

    for (std::map<int, TriggerRemap*>::iterator it = m_triggerMappings.begin(); it != m_triggerMappings.end(); ++it)
    {
        for (int slot = 0; slot < 2; ++slot)
        {
            const TriggerMapping& m = slot != 0 ? it->second->alternate : it->second->primary;
            if (m.device == 0)
                continue;
            if (m.device == 2)
            {
                if (key != 0 && key == m.key && m.threshold != 1000.0f)
                {
                    out.ids.push_back(it->first);
                    out.negative.push_back(false);
                    out.alternate.push_back(slot > 0);
                }
            }
            else if (m.type == 1)
            {
                if (device == m.device && axis != 15 && axis == m.axis)
                {
                    out.ids.push_back(it->first);
                    out.negative.push_back(false);
                    out.alternate.push_back(slot > 0);
                }
            }
            else if (device == m.device && button != 0 && button == m.button)
            {
                out.ids.push_back(it->first);
                out.negative.push_back(false);
                out.alternate.push_back(slot > 0);
            }
        }
    }
    return out.ids.size() != 0;
}

// ELF 0x8495B60. [CHOSEN-STUB]
bool ControlMap::getInputMappedToMouseButton(bool /*b*/, PlayerInputMap& /*out*/)
{
    return false;
}

// Creates the requested control-map interface with one reference. Rejects aggregation and unsupported IDs.
IBase* ControlMap::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)
        return 0;

    ControlMap* self = new ControlMap();            // 0x850BCB0, operator new(0x64u)
    void* itf = self->queryInterface(iid);          // virtual, slot +0x0C
    if (itf == 0)
        self->release();                            // virtual, slot +0x08
    return static_cast<IBase*>(itf);
}

} // namespace io
} // namespace hfe
} // namespace dice
