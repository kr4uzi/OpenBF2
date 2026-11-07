// [INFERRED] path. Client-only class read from BF2.exe (table 8A3BF8, 38 slots, factory 41063D);
// the dedicated ELF carries only the IID/CID symbols.
#include "ControlSettings.h"
#if !DEDICATED
#include "Main/BF2ClientSeams.h"          // the client globals
#include "../IO/Input/ControlMap.h"       // ControlMap::operator= - the backup/restore deep copy (PE 6B0F80)
#include "../Core/EventManager.h"         // frameEventManager - mapNextInput's delayed (10,12) event
#include "../Core/Ids.h"
#include "../IO/Console/Console.h"        // io::mainConsole - setDefaults runs Settings/Controls.con
#include "../IO/Input/Input.h"            // IControlMap, IControlMapManager, IInputDeviceManager, IInputDevice
#include <cstdio>
#include <cstdlib>
#include <new>
#include <sstream>

namespace dice { namespace hfe {
namespace io {
extern IBase* inputDeviceManager;         //  (Core/Globals.cpp defines it)
extern IBase* controlMapManager;
}
namespace {
// : the static std::string array at 9A6B80 (28-byte stride) every body walks, in table
// order. Index 5, defaultGameControlMap, is the one save() is told is not a player map.
// [INFERRED] name.
const std::string controlMapNames[7] = {
    std::string("InfantryPlayerInputControlMap"),
    std::string("LandPlayerInputControlMap"),
    std::string("AirPlayerInputControlMap"),
    std::string("HelicopterPlayerInputControlMap"),
    std::string("SeaPlayerInputControlMap"),
    std::string("defaultGameControlMap"),
    std::string("defaultPlayerInputControlMap")
};
// : the player inputs that are axes (PIYaw..PICameraY), terminated by 64, the
// PlayerInputMap count. mapNextInput and getBindingDescription test the input against it.
// [INFERRED] name.
const int axisPlayerInputs[] = { 0, 1, 2, 3, 4, 5, 6, 7, 64 };

inline io::IControlMapManager* mapManager()
{
    return static_cast<io::IControlMapManager*>(io::controlMapManager);
}
inline io::IInputDeviceManager* deviceManager()
{
    return static_cast<io::IInputDeviceManager*>(io::inputDeviceManager);
}
// The VC7.1 operator+(const char*, const std::string&) every backup name is built with:
// string("_") += name, copied out.
inline std::string backupNameOf(const std::string& name)
{
    return std::string(std::string("_") += name);
}
//  = io::ControlMap::operator=(const ControlMap&), called with the manager's IControlMap
// pointers (retail casts them; the manager only ever creates ControlMaps). A null pointer on either
// side faults here as it does in retail.
inline void assignControlMap(io::IControlMap* target, const io::IControlMap* source)
{
    *static_cast<io::ControlMap*>(target) = *static_cast<const io::ControlMap*>(source);
}
}

// (37 B): table, refcount 1, flag 0, no map, empty name.
ControlSettings::ControlSettings() : m_refCount(1), m_mappingInput(false), m_map(nullptr) {}

//: the one string, then the free.
ControlSettings::~ControlSettings() = default;

// (44 B): the settings-factory shape - aggregation refused, 44 retail bytes, construction
// only on a non-null allocation, unconditional queryInterface (the shipped null defect is kept).
IBase* ControlSettings::create(unsigned int iid, IBase* outer)
{
    if (outer) return nullptr;
    void* allocation = ::operator new(sizeof(ControlSettings));
    ControlSettings* owner = allocation ? new (allocation) ControlSettings : nullptr;
    IBase* result = static_cast<IBase*>(owner->queryInterface(iid));
    if (!result) owner->release();
    return result;
}
void ControlSettings::addRef() { ++m_refCount; }
int ControlSettings::getRef() { return m_refCount; }
// (26 B): decrement, destroy through the (withheld) +94 deleting destructor at zero.
int ControlSettings::release()
{
    const int references = --m_refCount;
    if (!references) delete this;
    return references;
}
// (33 B): IBase (dword_90D358 = 1), then IControlSettings (dword_892C88 = 121043).
void* ControlSettings::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IControlSettings)
        return const_cast<ControlSettings*>(this);
    return nullptr;
}

std::string ControlSettings::getFileName() { return std::string("Controls.con"); }

// (222 B): each named map that exists writes itself into a fresh ostringstream, told it is
// a player map except for index 5, and the pieces are concatenated. Without a manager the result
// is constructed from a null char pointer (retail 4B2B17: `push 0` into basic_string(const char*)).
std::string ControlSettings::getSettingsScript()
{
    if (!io::controlMapManager)
        return std::string(static_cast<const char*>(nullptr));
    std::string script;
    for (int index = 0; index < 7; ++index) {
        std::ostringstream stream;
        io::IControlMap* map = mapManager()->getControlMap(controlMapNames[index]);
        if (map) {
            map->save(stream, index != 5);
            script += stream.str();
        }
    }
    return script;
}

// (137 B): every live map is reset and then assigned from its "_" backup. Neither the
// manager nor the two map pointers are tested before the assignment (retail).
bool ControlSettings::restoreControlMaps()
{
    for (int index = 0; index < 7; ++index) {
        const std::string backupName = backupNameOf(controlMapNames[index]);
        io::IControlMap* map = mapManager()->getControlMap(controlMapNames[index]);
        if (map)
            map->reset();
        io::IControlMap* backup = mapManager()->getControlMap(backupName);
        assignControlMap(map, backup);
    }
    return true;
}

// (213 B): the active map is read before the manager is tested (retail 4B30E0..4B30F9);
// with a manager every "_" backup is deleted, created afresh and, when both exist, assigned from
// the live map; the active map is then restored by pointer.
void ControlSettings::backupControlMaps()
{
    io::IControlMap* active = mapManager()->getActive();
    if (io::controlMapManager) {
        for (int index = 0; index < 7; ++index) {
            const std::string backupName = backupNameOf(controlMapNames[index]);
            mapManager()->deleteControlMap(backupName);
            mapManager()->create(backupName);
            io::IControlMap* map = mapManager()->getControlMap(controlMapNames[index]);
            io::IControlMap* backup = mapManager()->getControlMap(backupName);
            if (backup && map)
                assignControlMap(backup, map);
        }
    }
    mapManager()->setActive(active);
}

// (273 B): the active map's name is kept, the backups are rebuilt with no null tests on
// either map, and the active map is re-selected by that name when there was one.
void ControlSettings::backupControlMapsByName()
{
    std::string activeName;
    if (mapManager()->getActive())
        activeName = mapManager()->getActive()->getName();
    for (int index = 0; index < 7; ++index) {
        const std::string backupName = backupNameOf(controlMapNames[index]);
        mapManager()->deleteControlMap(backupName);
        mapManager()->create(backupName);
        io::IControlMap* backup = mapManager()->getControlMap(backupName);
        io::IControlMap* map = mapManager()->getControlMap(controlMapNames[index]);
        assignControlMap(backup, map);
    }
    if (activeName != "")
        mapManager()->setActive(mapManager()->getControlMap(activeName));
}

// (591 B): the active map is read before the manager is tested; with a manager every live
// map is reset, Settings/Controls.con is run through the main console (eight empty arguments;
// retail's shared output string A02FF0 is modelled by a local), and the backups are rebuilt as
// backupControlMaps does; the active map is restored by pointer.
bool ControlSettings::setDefaults()
{
    io::IControlMap* active = mapManager()->getActive();
    if (io::controlMapManager) {
        for (int index = 0; index < 7; ++index) {
            io::IControlMap* map = mapManager()->getControlMap(controlMapNames[index]);
            if (map)
                map->reset();
        }
        std::string out;
        io::mainConsole->run(std::string("Settings/Controls.con"), "", "", "", "", "", "", "", "", out);
        for (int index = 0; index < 7; ++index) {
            const std::string backupName = backupNameOf(controlMapNames[index]);
            mapManager()->deleteControlMap(backupName);
            mapManager()->create(backupName);
            io::IControlMap* map = mapManager()->getControlMap(controlMapNames[index]);
            io::IControlMap* backup = mapManager()->getControlMap(backupName);
            if (backup && map)
                assignControlMap(backup, map);
        }
    }
    mapManager()->setActive(active);
    return true;
}

// (50 B)
void ControlSettings::resetControlMaps()
{
    for (int index = 0; index < 7; ++index) {
        io::IControlMap* map = mapManager()->getControlMap(controlMapNames[index]);
        if (map)
            map->reset();
    }
}

// (33 B): -1.0f (BF800000) then 0.1f (3DCCCCCD) into the device manager's update.
void ControlSettings::updateInputDevices()
{
    deviceManager()->update(-1.0f);
    deviceManager()->update(0.1f);
}

// (155 B, retn 14h): the capture flag is raised, the input is classified as an axis unless
// it is a game input, the output name is emptied, the manager captures into it, and the frame
// event manager is asked to post event (10, 12) - the engine's EnableEscapeKeyMapping handler,
// which clears the flag - two seconds later (40000000h). The third argument is never read.
int ControlSettings::mapNextInput(int input, bool alternate, int /*unused*/, bool second, bool gameInput)
{
    m_mappingInput = true;
    bool isAxis = false;
    if (!gameInput) {
        for (const int* id = axisPlayerInputs; *id != 64; ++id) {
            if (input == *id)
                isAxis = true;
        }
    }
    m_mappedInputName = "";
    const int result = mapManager()->mapNextInput(m_map, input, alternate, isAxis, second, !gameInput, m_mappedInputName);
    frameEventManager->postEvent(static_cast<EventCategory>(10), 12u, nullptr, 2.0f);
    return result;
}

// (28 B): no null test on the manager.
bool ControlSettings::selectControlMap(const std::string& name)
{
    m_map = mapManager()->getControlMap(name);
    return true;
}

namespace {
// The device prefix both walks stream first (4B35DA..4B3641 / 4B3729..4B3790): a two-level switch -
// `cmp eax, 40h; jg / je`, then the byte table 4B3850 (4B387C) over 1..0x20 into three targets.
// Mouse and Falcon have their own names; every game-controller flag shares one; the keyboard (2)
// and anything else stream nothing.
void streamDevicePrefix(std::ostream& stream, int device)
{
    switch (device)
    {
    case io::IDFMouse:
        stream << "CONTROLLER_IDFMouse";
        break;
    case io::IDFGameController_0: case io::IDFGameController_1: case io::IDFGameController_2:
    case io::IDFGameController_3: case io::IDFGameController_4: case io::IDFGameController_5:
    case io::IDFGameController_6: case io::IDFGameController_7:
        stream << "CONTROLLER_IDFGameController";
        break;
    case io::IDFFalcon:
        stream << "CONTROLLER_IDFFalcon";
        break;
    default:
        break;
    }
}
}

// Builds a binding's localization key; leaves text unchanged. Uses the selected map
// when map is null. Returns false for a missing map/binding, but true with an empty
// key for an unbound button or axis. alternate selects the second binding; axis
// direction uses invert XOR second.
bool ControlSettings::getBindingDescription(io::IControlMap* map, int input, bool alternate, bool second,
                                            bool gameInput, std::string& key, std::wstring& /*text*/)
{
    io::IControlMap* target = map ? map : m_map;
    if (!target)
        return false;

    bool isAxis = false;
    if (!gameInput)
    {
        for (const int* id = axisPlayerInputs; *id != 64; ++id)
        {
            if (input == *id)
            {
                isAxis = true;
                break;
            }
        }
    }
    bool found = false;
    deviceManager()->getInputDevice(io::IDFKeyboard);                     // +0x14, answer unused
    std::ostringstream stream;

    io::ControlMap* const controlMap = static_cast<io::ControlMap*>(target);
    if (isAxis)
    {
        const std::map<int, io::AxisRemap*>::iterator it = controlMap->m_axisMappings.find(input);
        if (it != controlMap->m_axisMappings.end())
        {
            io::AxisMapping* mapping = &it->second->primary;
            if (alternate)
                mapping = &it->second->alternate;                          // +0x58 PE
            streamDevicePrefix(stream, mapping->device);
            // `mov al, [esi+50h]; xor al, second` - a byte xor, so a non-0/1 invert byte counts.
            const bool negative =
                (static_cast<unsigned char>(mapping->invert ^ static_cast<unsigned char>(second))) != 0;
            if (mapping->type == 3)
            {
                const int k = negative ? mapping->negativeKey : mapping->positiveKey;   // +0x1C / +0x18
                stream << "CONTROLLER_" << static_cast<io::InputDeviceKeys>(k);
            }
            else if (mapping->type == 2)
            {
                const int button = negative ? mapping->negativeButton : mapping->positiveButton;
                if (!button)
                {
                    key = "";
                    return true;
                }
                stream << "_";
                stream << static_cast<io::InputDeviceButtons>(button);
            }
            else if (mapping->type == 1)
            {
                if (mapping->axis == io::IDAxis_None)                      // 0xF
                {
                    key = "";
                    return true;
                }
                stream << "_";
                stream << static_cast<io::InputDeviceAxes>(mapping->axis);
            }
            found = true;
        }
    }
    else
    {
        const std::map<int, io::TriggerRemap*>::iterator it = controlMap->m_triggerMappings.find(input);
        if (it != controlMap->m_triggerMappings.end())
        {
            io::TriggerMapping* mapping = &it->second->primary;
            if (alternate)
                mapping = &it->second->alternate;                          // +0x30 PE
            streamDevicePrefix(stream, mapping->device);
            if (mapping->type == 3)
                stream << "CONTROLLER_" << static_cast<io::InputDeviceKeys>(mapping->key);
            else if (mapping->type == 2)
            {
                stream << "_";
                stream << static_cast<io::InputDeviceButtons>(mapping->button);
            }
            else if (mapping->type == 1)
            {
                stream << "_";
                stream << static_cast<io::InputDeviceAxes>(mapping->axis);
            }
            found = true;
        }
    }
    key = stream.str();
    return found;
}

// (80 B)
std::string ControlSettings::getActiveControlMapName()
{
    if (io::controlMapManager && mapManager()->getActive())
        return std::string(mapManager()->getActive()->getName());
    return std::string("");
}

// +40..+74: forwards to the selected map; the getters fall back to 1.0f / false without one, the
// setters do nothing - except setInvertMouse, which retail forwards with no null test.
float ControlSettings::getMouseSensitivity() { return m_map ? m_map->getMouseSensitivity() : 1.0f; }
void ControlSettings::setMouseSensitivity(float value) { if (m_map) m_map->setMouseSensitivity(value); }
float ControlSettings::getSlot48() { return 1.0f; }
void ControlSettings::setSlot4C(float /*value*/) {}
float ControlSettings::getSlot50() { return 0.0f; }
void ControlSettings::setSlot54(float /*value*/) {}
float ControlSettings::getYawFactor() { return m_map ? m_map->getYawFactor() : 1.0f; }
void ControlSettings::setYawFactor(float value) { if (m_map) m_map->setYawFactor(value); }
float ControlSettings::getPitchFactor() { return m_map ? m_map->getPitchFactor() : 1.0f; }
void ControlSettings::setPitchFactor(float value) { if (m_map) m_map->setPitchFactor(value); }
bool ControlSettings::getInvertMouse() { return m_map ? m_map->getInvertMouse() : false; }
void ControlSettings::setInvertMouse(bool value) { m_map->setInvertMouse(value); }
float ControlSettings::getKeyboardSensitivity() { return m_map ? m_map->getKeyboardSensitivity() : 1.0f; }
void ControlSettings::setKeyboardSensitivity(float value) { if (m_map) m_map->setKeyboardSensitivity(value); }

// (8 B): the backup through the table, then true.
bool ControlSettings::apply()
{
    backupControlMaps();
    return true;
}

// (112 B): every "_" backup is looked up and handed to deleteControlMap by pointer, null or not.
void ControlSettings::deleteControlMapBackups()
{
    for (int index = 0; index < 7; ++index) {
        const std::string backupName = backupNameOf(controlMapNames[index]);
        io::IControlMap* backup = mapManager()->getControlMap(backupName);
        mapManager()->deleteControlMap(backup);
    }
}

// / 4B2EC0 (20 B each): the device manager's mouse (flag 1), activated or deactivated;
// no null tests.
void ControlSettings::activateMouse() { deviceManager()->getInputDevice(io::IDFMouse)->activate(); }
void ControlSettings::deactivateMouse() { deviceManager()->getInputDevice(io::IDFMouse)->deActivate(); }

// / 8247D0 (shared bodies)
bool ControlSettings::isMappingInput() { return m_mappingInput; }
void ControlSettings::setMappingInput(bool value) { m_mappingInput = value; }

// (23 B)
std::string ControlSettings::getMappedInputName() { return m_mappedInputName; }

} }
#endif
