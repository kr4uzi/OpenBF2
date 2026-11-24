// [TEMP] [CHOSEN, not recovered] Headless input/console scaffolding.
// Device samples and factory allocation are supplied by the fixture; mapping, registry removal,
// serialization and console argument conversion execute production bodies. No host is started.
#include "../../BF2/IO/Input/ControlMap.h"
#include "../../BF2/IO/Input/ControlMapManager.h"
#include "../../BF2/Core/ConsoleObjectMacro.h"
#include "../../BF2/Core/ClassManager.h"
#include "../../BF2/Core/System.h"
#include "../../BF2/Platform/Host/ClientHost.h"
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace dice::hfe;
using namespace dice::hfe::io;
static unsigned checks, failures;
static void check(bool value, const char* label)
{ ++checks; if (!value) { ++failures; std::fprintf(stderr, "FAIL %s\n", label); } }
static void unused() { std::abort(); }

namespace dice { namespace hfe {
ClassManager* classManager = nullptr;
System* g_system = nullptr;
namespace host {
ClientHost& ClientHost::instance() { unused(); std::abort(); }
bool ClientHost::pumpEvents() { unused(); return false; }
}
namespace io {
IBase* inputDeviceManager = nullptr;
IBase* controlMapManager = nullptr;
struct ControlMapFixture {
    static void insert(ControlMapManager& manager, ControlMap& map) {
        manager.m_byId[map.getID()] = &map;
        manager.m_byName.insert(map.getName(), &map, manager.m_byName.bucketOf(map.getName()));
    }
    static AxisMapping& primary(ControlMap& map, int id) { return map.m_axisMappings.at(id)->primary; }
    static void selectAlternate(ControlMap& map, int id) { map.m_triggerMappings.at(id)->selector = 0; }
};
} } }

struct SampleDevice : IPOVInputDevice {
    int angle = -1;
    unsigned buttons = 0;
    mutable int latencyButton = 0;
    void addRef() override { unused(); }
    int getRef() override { unused(); return 0; }
    int release() override { unused(); return 0; }
    void* queryInterface(unsigned) const override { unused(); return nullptr; }
    InputDeviceTypes getType() const override { return IDTGameController; }
    bool setCooperativeMode(InputDeviceMode) override { unused(); return false; }
    InputDeviceMode getCooperativeMode() const override { unused(); return static_cast<InputDeviceMode>(0); }
    bool activate() override { unused(); return false; }
    bool deActivate() override { unused(); return false; }
    void update(float) override { unused(); }
    bool isActive() const override { return true; }
    bool getNextInput(InputDeviceAxes&, InputDeviceButtons&, InputDeviceKeys&, bool&) override { unused(); return false; }
    int getNumAxes() const override { unused(); return 0; }
    float readAxis(InputDeviceAxes, bool) const override { unused(); return 0; }
    void setInvertAxis(InputDeviceAxes, bool) override { unused(); }
    bool getInvertAxis(InputDeviceAxes) const override { unused(); return false; }
    void setAxisScale(InputDeviceAxes, float) override { unused(); }
    float getAxisScale(InputDeviceAxes) const override { unused(); return 0; }
    void setAxisAdd(InputDeviceAxes, float) override { unused(); }
    float getAxisAdd(InputDeviceAxes) const override { unused(); return 0; }
    int getNumButtons() const override { unused(); return 0; }
    bool buttonDown(InputDeviceButtons button) const override { return (buttons & static_cast<unsigned>(button)) != 0; }
    bool buttonPressed(InputDeviceButtons) const override { unused(); return false; }
    bool buttonReleased(InputDeviceButtons) const override { unused(); return false; }
    float buttonPressedSampleLatency(InputDeviceButtons button) const override {
        latencyButton = button;
        return button == 1 ? 0.02f : 0.04f;
    }
    int getNumPOVs() const override { return 1; }
    int readPOV(InputDevicePOVs pov) const override { check(pov == IDPOV_0, "POV selector forwarded"); return angle; }
};
struct SampleManager : IInputDeviceManager {
    SampleDevice device;
    void addRef() override { unused(); }
    int getRef() override { unused(); return 0; }
    int release() override { unused(); return 0; }
    void* queryInterface(unsigned) const override { unused(); return nullptr; }
    bool initDevices(unsigned*) override { unused(); return false; }
    IInputDevice* getInputDevice(InputDeviceFlags flag) const override {
        check(flag == IDFMouse || flag == IDFGameController_0, "device flag forwarded");
        return const_cast<SampleDevice*>(&device);
    }
    unsigned getInputDevices() const override { unused(); return 0; }
    bool activateDevices(unsigned) override { unused(); return false; }
    bool deActivateDevices(unsigned) override { unused(); return false; }
    void update(float) override { unused(); }
    unsigned getActiveInputDevices() const override { unused(); return 0; }
    void* getAllDevices() override { unused(); return nullptr; }
};
struct FixtureManager : ControlMapManager {
    std::vector<ControlMap*> owned;
    ~FixtureManager() { for (ControlMap* map : owned) map->release(); }
    // Allocation seam only; the production class factory is not initialized in this fixture.
    IControlMap* create(const std::string& name) override {
        if (IControlMap* existing = getControlMap(name)) { setActive(existing); return existing; }
        ControlMap* map = new ControlMap;
        map->setName(name); map->setID(static_cast<int>(owned.size()));
        owned.push_back(map);
        ControlMapFixture::insert(*this, *map);
        setActive(map);
        return map;
    }
};

static float value;
static float getValue() { return value; }
static void setValue(float v) { value = v; }
BF2_CONSOLE_OBJECT("TestRange", 2, 0)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(half, float, true, getValue(), setValue, 0.0f, 1.0f, 0.5f, "test.range")
BF2_CONSOLE_PROPERTY_FN_RANGED_T(hundredth, float, true, getValue(), setValue, 0.0f, 1.0f, 0.01f, "test.range")
BF2_CONSOLE_PROPERTY_FN_RANGED_T(one, float, true, getValue(), setValue, 0.0f, 10.0f, 1.0f, "test.range")

static std::string save(ControlMap& map) { std::ostringstream out; map.save(out, true); return out.str(); }
static bool close(float a, float b) { return std::fabs(a-b) < 0.00001f; }
struct DumpMap : ControlMap {
    bool called = false;
    bool player = false;
    void save(std::ostream& out, bool playerMap) override {
        called = true; player = playerMap; out << "fixture";
    }
};

int main(int argc, char** argv)
{
    check(argc == 2, "output Controls.con path supplied");
    if (argc != 2) return 2;
    SampleManager devices;
    FixtureManager manager;
    inputDeviceManager = &devices;
    controlMapManager = &manager;
    const auto first = static_cast<InputDeviceButtons>(1), second = static_cast<InputDeviceButtons>(2);
    {
        DumpMap map;
        map.dump(true);
        check(map.called && map.player, "dump invokes virtual save with player-map argument");
        map.dump(false);
        check(!map.player, "dump forwards game-map argument");
    }
    {
        ControlMap map;
        map.setName("controls"); map.setID(10);
        map.addTriggerMapping(PIFire, IDFMouse, first, 0.0f, false);
        map.addTriggerMapping(PIFire, IDFMouse, second, 0.0f, true);
        devices.device.buttons = 1; map.update(0.01f);
        check(close(map.getTriggerSampleLatency(PIFire), 0.02f), "primary mouse fire latency");
        devices.device.buttons = 2; map.update(0.01f);
        // Retail update keeps selector=1 even when the alternate is down (PE 0x006AFC60).
        check(close(map.getTriggerSampleLatency(PIFire), 0.02f), "retail update retains primary latency selector");
        ControlMapFixture::selectAlternate(map, PIFire);
        check(close(map.getTriggerSampleLatency(PIFire), 0.04f), "alternate mouse fire latency");
        check(devices.device.latencyButton == 2, "sampled button is selected binding");
        check(map.getTriggerSampleLatency(99) == 0, "unmapped latency zero");
        map.addTriggerMapping(PIUse, IDFGameController_0, first, 0, false);
        map.update(0.01f);
        check(map.getTriggerSampleLatency(PIUse) == 0, "non-mouse latency zero");
        map.removeMapping(PIUse);
        PlayerInputMap input = PINone;
        check(map.getInputMappedToMouseButton(true, input) && input == PIFire, "primary mouse lookup");
        check(map.getInputMappedToMouseButton(false, input) && input == PIFire, "alternate mouse lookup");
        map.addAxisMapping(PIYaw, IDFMouse, first, static_cast<InputDeviceButtons>(0), false);
        check(map.getInputMappedToMouseButton(true, input) && input == PIYaw, "axis lookup precedes trigger");
        map.removeMapping(PIYaw);
        check(!map.axisExists(PIYaw), "axis removed");
        map.addTriggerMapping(PIReload, IDFFalcon, static_cast<InputDeviceButtons>(8), 0, false);
        map.removeMapping(PIFire);
        check(!map.triggerExists(PIFire), "trigger removed");
        check(map.getInputMappedToMouseButton(false, input) && input == PIReload, "Falcon eighth button lookup");
        input = PINone;
        check(!map.getInputMappedToMouseButton(true, input) && input == PINone, "miss retains output");
        map.addAxisMapping(PIReload, IDFMouse, first, first, false);
        map.removeMapping(PIReload);
        check(map.axisExists(PIReload) && !map.triggerExists(PIReload), "trigger removal precedes same-id axis");
        map.removeMapping(PIReload); map.removeMapping(999);
    }
    {
        ControlMap map;
        map.setButtonRiseTime(0.1f); map.setButtonFallTime(0.2f);
        check(map.addAxisMapping(PIYaw, PIPitch, IDFGameController_0, IDPOV_0, true, true, false), "POV pair created");
        devices.device.angle = 9000; map.update(0.025f);
        check(close(map.getAxisValue(PIYaw), 0.25f) && close(map.getAxisValue(PIPitch), 0), "right hat ramps horizontal only; ignored booleans");
        map.update(0.2f); check(map.getAxisValue(PIYaw) == 1, "POV positive clamp");
        devices.device.angle = -1; map.update(0.05f);
        check(close(map.getAxisValue(PIYaw), 0.75f), "centered hat decays with fall time");
        map.reset(); map.setButtonRiseTime(0.1f);
        map.addAxisMapping(PIYaw, PIPitch, IDFGameController_0, IDPOV_0, false, false, true);
        devices.device.angle = 0; map.update(0.025f);
        check(close(map.getAxisValue(PIPitch), 0.25f), "last POV boolean inverts vertical");
        check(!map.isAxisValueAnalogue(PIYaw), "POV axis is digital");
        check(ControlMapFixture::primary(map, PIYaw).invert == 1, "inversion stored");
        check(save(map).find("addPOV") == std::string::npos, "retail save omits POV mappings");
        map.reset(); map.setButtonRiseTime(0.1f);
        map.addAxisMapping(PIYaw, PIPitch, IDFGameController_0, IDPOV_0, false, false, false);
        devices.device.angle = 31500; map.update(0.025f);
        check(close(map.getAxisValue(PIYaw), -0.25f) && close(map.getAxisValue(PIPitch), -0.25f), "diagonal hat drives both negative components");
        map.update(0.2f);
        check(map.getAxisValue(PIYaw) == -1 && map.getAxisValue(PIPitch) == -1, "POV negative clamp");
        devices.device.angle = -1; map.update(0.05f);
        check(close(map.getAxisValue(PIYaw), -0.75f), "negative POV decays toward zero");
    }
    {
        ControlMap& original = *static_cast<ControlMap*>(manager.create("profile"));
        original.addTriggerMapping(PIFire, IDFMouse, first, 0, false);
        original.addTriggerMapping(PIReload, IDFKeyboard, IDKey_R, 10000, false);
        original.removeMapping(PIReload);
        original.addAxisMapping(PIThrottle, IDFKeyboard, IDKey_W, IDKey_S, false);
        original.addTriggerMapping(PIFire, IDFMouse, second, 0, false);
        const std::string text = save(original);
        check(text.find("c_PIReload") == std::string::npos, "removed mapping absent from Controls.con");
        check(text.find("IDButton_1") != std::string::npos, "changed fire button serialized");
        { std::ofstream file(argv[1]); file << text; check(file.good(), "Controls.con written"); }
        original.reset();
        ConsoleObjects registry;
        registry.registerConsoleObjects(consoleBindingList("io.ControlMap"));
        std::ifstream file(argv[1]); std::string line;
        while (std::getline(file, line)) {
            std::istringstream row(line); std::string command; if (!(row >> command)) continue;
            const auto dot = command.find('.');
            std::string object = command.substr(0,dot);
            object[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(object[0])));
            ConsoleObject* binding = registry.getConsoleObject(object, command.substr(dot+1));
            check(binding != nullptr, "saved command registered"); if (!binding) continue;
            std::vector<std::string> args; std::string arg;
            while (row >> arg) {
                // The console's constant expansion precedes generated argument conversion.
                if (arg.compare(0,4,"c_PI") == 0) {
                    PlayerInputMap id; std::istringstream name(arg.substr(2)); name >> id;
                    arg = std::to_string(static_cast<int>(id));
                }
                args.push_back(arg);
            }
            binding->execute(args.data(), static_cast<int>(args.size()));
        }
        ControlMap& loaded = *static_cast<ControlMap*>(manager.getActive());
        check(save(loaded) == text, "Controls.con save/load/save stable through console bindings");
        check(!loaded.triggerExists(PIReload), "removed mapping remains absent after load");
        PlayerInputMap input = PINone;
        check(loaded.getInputMappedToMouseButton(false,input) && input == PIFire, "changed fire mapping survives load");
        const int ref = loaded.getRef(); const int id = loaded.getID();
        manager.deleteControlMap("profile");
        check(manager.getControlMap(id) == nullptr && manager.getControlMap("profile") == nullptr, "map unlinked by id and name");
        check(manager.getActive() == &loaded && loaded.getRef() == ref, "unlink preserves active pointer and ownership");
        manager.deleteControlMap(static_cast<const IControlMap*>(nullptr));
        manager.deleteControlMap(999);
    }
    const char* deltas[] = {"0.5", "0.01", "1"}; unsigned index = 0;
    for (ConsoleObject* property : consoleBindingList("test.range")) {
        check(property->getDeltaAsString() == deltas[index++], "ranged delta dispatch through console base");
        check(property->hasRange(), "range metadata retained");
    }
    std::printf("Control maps and console: %u/%u checks passed\n", checks-failures, checks);
    controlMapManager = nullptr; inputDeviceManager = nullptr;
    return failures ? 1 : 0;
}
