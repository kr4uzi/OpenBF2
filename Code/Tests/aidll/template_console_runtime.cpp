// [TEMP]
// Tests the BFConsole binding block with real registries, templates, parsers, and math.
// Observers supply singleton publication, current filenames, and typed AI interfaces; unexpected slots abort.
// Does not run the script interpreter, module lifecycle, or other BFConsole behavior.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>
#include <crtdbg.h>
#include "../../BF2/Core/ClassManager.h"
#include "../../BF2/Core/ConsoleObjects.h"
#include "../../BF2/Core/ConsoleObjectBase.h"
#include "../../BF2/Core/Debug.h"
#include "../../BF2/IO/Console/Console.h"
#include "../../BF2/IO/Input/Input.h"
#include "../../BF2/Math/Mat4.h"
#include "../../BF2/Math/Vec3.h"
#include "../../BF2/AI/AIInterface/Module/BFConsole.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/AITemplate.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/AITemplateArmament.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/AITemplateControlInfo.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/AITemplateCover.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/AITemplateManager.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/AITemplateMobile.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/AITemplatePhysical.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/AITemplatePlugInManager.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/AITemplateUnit.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/InformationType.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/KitTemplateManager.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/SoldierPose.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/WeaponTemplate.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/WeaponTemplateManager.h"
#include "../../BF2/AI/AIInterface/SingletonServer.h"
#include "../../BF2/AI/AIDLL/AI/Main/AISettingsInterfaces.h"
#include "../../BF2/AI/AIDLL/AI/Main/BFPathfindingCourier.h"
#include "../../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"

using namespace dice::hfe;
using namespace dice::hfe::ai;

static unsigned checks;
static void check(bool pass, const char* message)
{
    ++checks;
    if (!pass) { std::printf("FAIL %s\n", message); std::fflush(stdout); std::exit(2); }
}
[[noreturn]] static void unexpected(const char* method)
{
    std::printf("UNEXPECTED %s\n", method); std::fflush(stdout); std::abort();
}

// [TEMP]
// AI object runtime is omitted; unexpected calls reach the shared aborting link seam.
#include "ai_runtime_link_seam.inl"

// chosen collaborator state
static std::string workingFile = "Objects/fixture/ai/Objects.ai";
static int settingsCalls;
static std::vector<std::string> vehicles;          // the observer's IAISettings "vehicle" ids
static std::map<std::string, int> pathMaps;        // the observer's pathfinding map ids
static void* registeredKit;
static int vehicleId(const std::string& name)
{
    if (name == "NoSuchVehicle") return -1;
    for (unsigned i = 0; i < vehicles.size(); ++i)
        if (vehicles[i] == name) return static_cast<int>(i);
    vehicles.push_back(name);
    return static_cast<int>(vehicles.size()) - 1;
}
static const std::string& vehicleName(int id) { return vehicles.at(static_cast<unsigned>(id)); }
static int mapId(const std::string& name)
{
    if (name == "NoSuchMap") return -1;
    const std::map<std::string, int>::const_iterator it = pathMaps.find(name);
    if (it != pathMaps.end()) return it->second;
    const int id = static_cast<int>(pathMaps.size()) + 10;
    pathMaps[name] = id;
    return id;
}

namespace dice { namespace hfe {
ClassManager* classManager;
ClassManager::ClassManager() {}
ClassManager::~ClassManager() {}
void ClassManager::registerSimpleSingleton(const std::string& name, void* value)
{
    check(name == "IKitTemplateManager", "kit manager publication name"); registeredKit = value;
}
void ClassManager::unregisterSimpleSingleton(const std::string& name)
{
    check(name == "IKitTemplateManager", "kit manager unpublication name"); registeredKit = nullptr;
}
namespace io {
OldConsole* mainConsole;
OldConsole::Level::Level() {}
OldConsole::OldConsole() {}
OldConsole::~OldConsole() {}
const std::string& OldConsole::getWorkingFile() const { return workingFile; }
}
namespace ai {
void* CommonSingletonServer::m_iKitTemplateManager;
void* CommonSingletonServer::m_iaiSettings;
void* CommonSingletonServer::m_iaiPathfindingCourier;
void* CommonSingletonServer::m_iaiGame;
void BFSingletonServer::updateStaticMethods()
{
    CommonSingletonServer::m_iKitTemplateManager = registeredKit;
}
// [CHOSEN] The DLL interfaces' own constructor/destructor pairs register with the DLL's roster; the
// observers below are handed to the engine directly instead.
IAISettings::IAISettings() {}
IAISettings::~IAISettings() {}
IAIPathfindingCourier::IAIPathfindingCourier() {}
IAIPathfindingCourier::~IAIPathfindingCourier() {}
} } }
#include "kit_collaborators.inl"
#include "template_observers.inl"
#include "stock_template_commands.inl"

// helpers
static ConsoleObject* row(const char* object, const char* name)
{
    for (ConsoleObject* o : kitTemplateConsoleObjects())
        if (std::strcmp(o->getObjectName(), object) == 0 && std::strcmp(o->getFunctionName(), name) == 0)
            return o;
    unexpected(name);
}
static std::string exec(const std::string& object, const std::string& method, std::vector<std::string> args)
{
    ConsoleObject* o = mainConsoleObjects->getConsoleObject(object, method);
    if (!o) { std::printf("unresolved %s.%s\n", object.c_str(), method.c_str()); }
    check(o != nullptr, "command resolves through the real registry");
    args.push_back(std::string());          // never read: keeps data() valid for argc 0
    return o->execute(args.data(), static_cast<int>(args.size()) - 1);
}
static std::string get(const std::string& object, const std::string& method)
{
    return exec(object, method, std::vector<std::string>());
}
static bool closeTo(float a, float b) { return std::fabs(a - b) <= 1e-6f * (1.0f + std::fabs(b)); }

// The retail metadata, read from the ELF's static initialiser 0x88521A0 (class ids 93..102, 139..216).
struct Row { const char* object; const char* name; int type, minArgs, maxArgs; const char* ret; bool dyn;
             const char* a0; const char* a1; int kind; };
// kind: -2 aiTemplate manager row, -1 aiTemplate field, -3 plug-in manager row, 0..5 the plug-in id.
static const Row rows[] = {
    {"aITemplate", "create", 0, 1, 1, "std::string", true, "std::string", 0, -2},
    {"aITemplate", "active", 1, 0, 1, "std::string", true, "std::string", 0, -2},
    {"aITemplate", "types", 1, 0, 1, "int", true, "int", 0, -1},
    {"aITemplate", "degeneration", 1, 0, 1, "float", true, "float", 0, -1},
    {"aITemplate", "allowedTimeDiff", 1, 0, 1, "float", true, "float", 0, -1},
    {"aITemplate", "basicTemp", 1, 0, 1, "float", true, "float", 0, -1},
    {"aITemplate", "commonKnowledge", 1, 0, 1, "bool", true, "bool", 0, -1},
    {"aITemplate", "secondary", 1, 0, 1, "bool", true, "bool", 0, -1},
    {"aITemplate", "addType", 0, 1, 1, "void", false, "ai::InformationType", 0, -1},
    {"aITemplate", "addPlugIn", 0, 1, 1, "std::string", true, "std::string", 0, -2},
    {"aITemplatePlugIn", "create", 0, 2, 2, "std::string", true, "std::string", "std::string", -3},
    {"aITemplatePlugIn", "active", 1, 0, 1, "std::string", true, "std::string", 0, -3},
    {"aITemplatePlugIn", "coverValue", 1, 0, 1, "float", true, "float", 0, 2},
    {"aITemplatePlugIn", "transparency", 1, 0, 1, "float", true, "float", 0, 2},
    {"aITemplatePlugIn", "vehicleNumber", 1, 0, 1, "int", true, "int", 0, 3},
    {"aITemplatePlugIn", "setPathfindingMap", 0, 1, 1, "void", false, "std::string", 0, 3},
    {"aITemplatePlugIn", "maxSpeed", 1, 0, 1, "float", true, "float", 0, 3},
    {"aITemplatePlugIn", "lodHeight", 1, 0, 1, "float", true, "float", 0, 3},
    {"aITemplatePlugIn", "turnRadius", 1, 0, 1, "float", true, "float", 0, 3},
    {"aITemplatePlugIn", "coverSearchRadius", 1, 0, 1, "float", true, "float", 0, 3},
    {"aITemplatePlugIn", "avoidCollisionLookAhead", 1, 0, 1, "float", true, "float", 0, 3},
    {"aITemplatePlugIn", "isTurnable", 1, 0, 1, "bool", true, "bool", 0, 3},
    {"aITemplatePlugIn", "setSoundSphereRadius", 0, 2, 2, "void", false, "float", "float", 3},
    {"aITemplatePlugIn", "setHearingProbability", 0, 2, 2, "void", false, "float", "float", 3},
    {"aITemplatePlugIn", "getStrType", 0, 0, 0, "int", true, 0, 0, 4},
    {"aITemplatePlugIn", "setStrType", 0, 1, 1, "void", false, "CombatStrengthType", 0, 4},
    {"aITemplatePlugIn", "setIsAntiAircraft", 0, 1, 1, "void", false, "int", 0, 0},
    {"aITemplatePlugIn", "cullDistance", 1, 0, 1, "float", true, "float", 0, 5},
    {"aITemplatePlugIn", "equipmentTypeName", 1, 0, 1, "std::string", true, "std::string", 0, 5},
    {"aITemplatePlugIn", "setBattleStrength", 0, 2, 2, "void", false, "int", "int", 5},
    {"aITemplatePlugIn", "setStrategicStrength", 0, 2, 2, "void", false, "int", "int", 5},
    {"aITemplatePlugIn", "selectKey", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 5},
    {"aITemplatePlugIn", "setUseNoPathfindingToGetToObject", 0, 1, 1, "void", false, "bool", 0, 5},
    {"aITemplatePlugIn", "setHasExposedSoldier", 0, 1, 1, "void", false, "bool", 0, 5},
    {"aITemplatePlugIn", "setAlwaysTargetSoldier", 0, 1, 1, "void", false, "bool", 0, 5},
    {"aITemplatePlugIn", "driveControls", 2, 0, 0, "u64", true, 0, 0, 1},
    {"aITemplatePlugIn", "aimControls", 2, 0, 0, "u64", true, 0, 0, 1},
    {"aITemplatePlugIn", "lookControls", 2, 0, 0, "u64", true, 0, 0, 1},
    {"aITemplatePlugIn", "throttleSensitivity", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "pitchSensitivity", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "rollSensitivity", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "yawSensitivity", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "proportionalConstant", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "derivativeConstant", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "angleSpeedConstant", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "throttleLookAhead", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "pitchLookAhead", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "rollLookAhead", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "yawLookAhead", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "throttleScale", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "pitchScale", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "rollScale", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "yawScale", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "lookVerticalSensitivity", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "lookHorizontalSensitivity", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "lookVerticalLookAhead", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "lookHorizontalLookAhead", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "lookVerticalScale", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "lookHorizontalScale", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "maxSpeedBumpinessFallOff", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "driveThrottleControl", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 1},
    {"aITemplatePlugIn", "driveTurnControl", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 1},
    {"aITemplatePlugIn", "aimVerticalControl", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 1},
    {"aITemplatePlugIn", "aimHorizontalControl", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 1},
    {"aITemplatePlugIn", "lookVerticalControl", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 1},
    {"aITemplatePlugIn", "lookHorizontalControl", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 1},
    {"aITemplatePlugIn", "setCameraRelativeDofRotationOffsetDeg", 0, 1, 1, "void", false, "float", 0, 1},
    {"aITemplatePlugIn", "setCameraRelativeMinRotationDeg", 0, 1, 1, "void", false, "hfe::Vec3", 0, 1},
    {"aITemplatePlugIn", "setCameraRelativeMaxRotationDeg", 0, 1, 1, "void", false, "hfe::Vec3", 0, 1},
    {"aITemplatePlugIn", "maxClimbAngle", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "maxRollAngle", 1, 0, 1, "float", true, "float", 0, 1},
    {"aITemplatePlugIn", "driveRollControl", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 1},
    {"aITemplatePlugIn", "drivePitchControl", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 1},
    {"aITemplatePlugIn", "aimThrottleControl", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 1},
    {"aITemplatePlugIn", "aimRollControl", 1, 0, 1, "io::PlayerInputMap", true, "io::PlayerInputMap", 0, 1},
    // weaponTemplate, ConsoleClass223..245 (phase D). kind -4: manager row, -5: weapon field.
    {"weaponTemplate", "create", 0, 1, 1, "std::string", true, "std::string", 0, -4},
    {"weaponTemplate", "active", 1, 0, 1, "std::string", true, "std::string", 0, -4},
    {"weaponTemplate", "deviation", 1, 0, 1, "float", true, "float", 0, -5},
    {"weaponTemplate", "deviationCorrectionTime", 1, 0, 1, "float", true, "float", 0, -5},
    {"weaponTemplate", "indirect", 1, 0, 1, "bool", true, "bool", 0, -5},
    {"weaponTemplate", "useAimerOnly", 1, 0, 1, "bool", true, "bool", 0, -5},
    {"weaponTemplate", "drag", 1, 0, 1, "float", true, "float", 0, -5},
    {"weaponTemplate", "minRange", 1, 0, 1, "float", true, "float", 0, -5},
    {"weaponTemplate", "maxRange", 1, 0, 1, "float", true, "float", 0, -5},
    {"weaponTemplate", "weaponActivate", 1, 0, 1, "hfe::io::PlayerInputMap", true, "hfe::io::PlayerInputMap", 0, -5},
    {"weaponTemplate", "weaponFire", 1, 0, 1, "hfe::io::PlayerInputMap", true, "hfe::io::PlayerInputMap", 0, -5},
    {"weaponTemplate", "isThrown", 1, 0, 1, "bool", true, "bool", 0, -5},
    {"weaponTemplate", "soundSphereRadius", 1, 0, 1, "float", true, "float", 0, -5},
    {"weaponTemplate", "exitVelocity", 1, 0, 1, "float", true, "float", 0, -5},
    {"weaponTemplate", "getStrength", 0, 1, 1, "int", false, "CombatStrengthType", 0, -5},
    {"weaponTemplate", "setStrength", 0, 2, 2, "void", false, "CombatStrengthType", "int", -5},
    {"weaponTemplate", "fireRate", 1, 0, 1, "int", true, "int", 0, -5},
    {"weaponTemplate", "firingPose", 1, 0, 1, "SoldierPose", true, "SoldierPose", 0, -5},
    {"weaponTemplate", "optimalRangePercentage", 1, 0, 1, "float", true, "float", 0, -5},
    {"weaponTemplate", "explosionRadius", 1, 0, 1, "float", true, "float", 0, -5},
    {"weaponTemplate", "allowedDeviation", 1, 0, 1, "float", true, "float", 0, -5},
    {"weaponTemplate", "firesThroughTransparent", 1, 0, 1, "bool", true, "bool", 0, -5},
};
static bool managerRow(int kind) { return kind == -2 || kind == -3 || kind == -4; }
static const unsigned rowCount = sizeof(rows) / sizeof(rows[0]);

static const char* const plugInTypes[6] = {"Armament", "ControlInfo", "Cover", "Mobile", "Physical", "Unit"};

// Print the Error text, then return: ~Debug then aborts, as it does after any shipped callback.
static void errorCallback(DebugType, const std::string&, const std::string&, int line, const std::string&,
                          const std::string& text, const std::string& fullFileName, void*)
{
    std::printf("DEBUG-ERROR %s(%d): %s\n", fullFileName.c_str(), line, text.c_str());
    std::fflush(stdout);
}

struct Managers
{
    ObservedSettings settings;
    ObservedCourier courier;
    io::OldConsole console;
    AITemplateManager templates;
    AITemplatePlugInManager plugIns;
    WeaponTemplateManager weapons;
    Managers() : weapons(6)          // BFConsole::createSingletons' literal: the strength count
    {
        io::mainConsole = &console;
        CommonSingletonServer::m_iaiSettings = static_cast<IAISettings*>(&settings);
        CommonSingletonServer::m_iaiPathfindingCourier = static_cast<IAIPathfindingCourier*>(&courier);
    }
};

// A Physical plug-in that reports its destruction, for AITemplate::addPlugIn's replacement defect.
struct CountingPhysical : AITemplatePhysical
{
    static int destroyed;
    ~CountingPhysical() { ++destroyed; }
};
int CountingPhysical::destroyed;

int main(int argc, char** argv)
{
    static_assert(sizeof(void*) == 4, "x86 only");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    if (argc != 2) return 2;
    const std::string mode(argv[1]);
    mainConsoleObjects->registerConsoleObjects(kitTemplateConsoleObjects());
    check(kitTemplateConsoleObjects().size() == rowCount + 4, "the TU list plus four kitTemplate commands");

    if (mode == "metadata")
    {
        unsigned index = 0;
        for (ConsoleObject* o : kitTemplateConsoleObjects())
        {
            if (index < rowCount)
            {
                const Row& r = rows[index];
                check(std::strcmp(o->getObjectName(), r.object) == 0, "object name, first letter lowered");
                check(std::strcmp(o->getFunctionName(), r.name) == 0, "retail class order");
                check(o->getType() == r.type, "property/method/read-only type");
                check(o->getMinNoArgs() == r.minArgs && o->getMaxNoArgs() == r.maxArgs, "argc range");
                check(std::strcmp(o->getReturnType(), r.ret) == 0, "return type string");
                check(o->isDynamic() == r.dyn, "dynamic flag");
                check(o->getAccess() == 2 && o->getCategory() == 0, "access 2, category 0");
                check(!o->hasRange() && !o->hasReturnValue(), "unranged, no return value yet");
                if (r.a0) check(std::strcmp(o->getArgType(0), r.a0) == 0, "first argument type string");
                if (r.a1) check(std::strcmp(o->getArgType(1), r.a1) == 0, "second argument type string");
            }
            else
                check(std::strcmp(o->getObjectName(), "kitTemplate") == 0, "kit rows follow");
            ++index;
        }
    }
    else if (mode == "gates")
    {
        for (unsigned i = 0; i < rowCount; ++i)
            check(!row(rows[i].object, rows[i].name)->isActive(), "every row inactive with no managers");
        Managers m;
        for (unsigned i = 0; i < rowCount; ++i)
            check(row(rows[i].object, rows[i].name)->isActive() == managerRow(rows[i].kind),
                  "only the manager rows with empty managers");
        check(exec("weaponTemplate", "create", {"gatedGun"}) == "WeaponTemplate 'gatedGun' successfully created",
              "weapon create");
        for (unsigned i = 0; i < rowCount; ++i)
            if (rows[i].kind == -5) check(row(rows[i].object, rows[i].name)->isActive(), "weapon fields live");
        exec("weaponTemplate", "active", {"missing"});
        for (unsigned i = 0; i < rowCount; ++i)
            if (rows[i].kind == -5) check(!row(rows[i].object, rows[i].name)->isActive(), "a weapon setActive miss clears");
        check(exec("aiTemplate", "create", {"gated"}) == "AITemplate 'gated' successfully created", "template create");
        for (unsigned i = 0; i < rowCount; ++i)
            if (rows[i].kind == -1) check(row(rows[i].object, rows[i].name)->isActive(), "template fields live");
        exec("aiTemplate", "active", {"missing"});
        for (unsigned i = 0; i < rowCount; ++i)
            if (rows[i].kind == -1) check(!row(rows[i].object, rows[i].name)->isActive(), "a setActive miss clears");
        for (int kind = 0; kind < 6; ++kind)
        {
            const std::string name = std::string("gate") + plugInTypes[kind];
            check(exec("aiTemplatePlugIn", "create", {plugInTypes[kind], name}) ==
                  "AITemplatePlugIn '" + name + "' successfully created", "plug-in create");
            for (unsigned i = 0; i < rowCount; ++i)
                if (rows[i].kind >= 0)
                    check(row(rows[i].object, rows[i].name)->isActive() == (rows[i].kind == kind),
                          "exactly the active plug-in kind's rows are live (getId match)");
        }
        exec("aiTemplatePlugIn", "active", {"nothing"});
        for (unsigned i = 0; i < rowCount; ++i)
            if (rows[i].kind >= 0) check(!row(rows[i].object, rows[i].name)->isActive(), "plug-in miss clears");
    }
    else if (mode == "templates")
    {
        Managers m;
        check(get("aiTemplate", "active") == "", "no active template reads empty");
        check(exec("aiTemplate", "addPlugIn", {"anything"}) == "Error: No active template", "addPlugIn needs an active one");
        check(exec("aiTemplate", "create", {"Tank"}) == "AITemplate 'Tank' successfully created", "create message");
        check(get("aiTemplate", "active") == "Tank", "create makes it active");
        AITemplate* const tank = m.templates.getActiveTemplate();
        check(tank && m.templates.getTemplate("tank") == tank && m.templates.getTemplate("TANK") == tank,
              "the name map is case-insensitive");
        check(!tank->m_plugIns && tank->m_plugInMask == 0 && tank->m_types == 0 && !tank->m_wreckTemplate &&
              tank->m_name == "Tank" && !tank->m_initialized, "constructor defaults");
        exec("aiTemplate", "degeneration", {"10"}); exec("aiTemplate", "allowedTimeDiff", {"2"});
        exec("aiTemplate", "basicTemp", {"7"}); exec("aiTemplate", "commonKnowledge", {"0"});
        exec("aiTemplate", "secondary", {"1"}); exec("aiTemplate", "types", {"1"});
        check(closeTo(tank->m_degeneration, 10.0f) && closeTo(tank->m_allowedTimeDiff, 2.0f) &&
              closeTo(tank->m_basicTemp, 7.0f) && !tank->m_commonKnowledge && tank->m_secondary && tank->m_types == 1,
              "raw field stores");
        check(get("aiTemplate", "degeneration") == "10" && get("aiTemplate", "secondary") == "1", "get arms");
        exec("aiTemplate", "addType", {"ITHasWreck"}); exec("aiTemplate", "addType", {"ITGround"});
        check(tank->m_types == (1 | (1 << 24) | (1 << 6)), "addType ORs 1 << type");
        exec("aiTemplate", "addType", {"ithaswreck"}); exec("aiTemplate", "addType", {"Bogus"});
        exec("aiTemplate", "addType", {""});
        check(tank->m_types == (1 | (1 << 24) | (1 << 6)), "an unknown name parses to -1 and is skipped");
        check(exec("aiTemplate", "addPlugIn", {"TankPhysical"}) == "AITemplatePlugIn 'TankPhysical' doesn't exist",
              "a missing plug-in is only a message");
        exec("aiTemplatePlugIn", "create", {"Physical", "TankPhysical"});
        exec("aiTemplatePlugIn", "setStrType", {"HeavyArmour"});
        check(exec("aiTemplate", "addPlugIn", {"TankPhysical"}) == "Successfully added plugin TankPhysical", "added");
        check(tank->m_plugIns && tank->m_plugIns[4] == m.plugIns.getTemplatePlugIn("TankPhysical") &&
              tank->m_plugInMask == 0x10 && tank->isPhysical() && !tank->isUnit() && !tank->isArmament() &&
              !tank->isCover() && !tank->isMobile() && !tank->m_plugIns[1], "slot 4, bit 4, array zeroed");
        exec("aiTemplate", "create", {"Crate"});
        exec("aiTemplatePlugIn", "create", {"Physical", "CratePhysical"});
        exec("aiTemplate", "addPlugIn", {"CratePhysical"});
        AITemplate* const crate = m.templates.getTemplate("Crate");
        m.templates.createWreckTemplates();
        check(!crate->m_wreckTemplate, "no ITHasWreck, no wreck");
        AITemplate* const wreck = tank->m_wreckTemplate;
        check(wreck && wreck->m_name == "Tank_wreck" && wreck->m_types == (1 << 25) &&
              closeTo(wreck->m_degeneration, 20.0f) && closeTo(wreck->m_basicTemp, 1.0f) && wreck->m_commonKnowledge &&
              closeTo(wreck->m_allowedTimeDiff, 10.0f) && !wreck->m_secondary && wreck->m_plugInMask == 0x10,
              "the wreck template's fields");
        check(wreck->m_plugIns[4] && wreck->m_plugIns[4] != tank->m_plugIns[4] &&
              static_cast<AITemplatePhysical*>(wreck->m_plugIns[4])->m_strType == 2, "a fresh Physical, strType copied");
        check(!m.templates.getTemplate("Tank_wreck"), "wrecks are not entered in the map");
        // addPlugIn's replacement defect, on a template of the fixture's own (a manager-owned plug-in
        // replaced this way is freed while the plug-in map still holds it - retail's double free).
        {
            AITemplate standalone("standalone");
            CountingPhysical* const first = new CountingPhysical;
            CountingPhysical* const second = new CountingPhysical;
            standalone.addPlugIn(first);
            check(CountingPhysical::destroyed == 0 && standalone.m_plugIns[4] == first, "an empty slot takes it");
            standalone.addPlugIn(second);
            check(CountingPhysical::destroyed == 1 && standalone.m_plugIns[4] == second &&
                  standalone.m_plugInMask == 0x10, "a second plug-in of the kind DELETES the first");
            delete second;                 // ~AITemplate frees only its slot array, never the plug-ins
        }
        // reset leaves both maps pointing at freed memory, so a manager that was reset must never be
        // destroyed (its destructor resets again - a double free). Probed on a second manager, which is
        // then leaked on purpose; the Managers member is last-wins-replaced and restored afterwards.
        {
            AITemplateManager* const doomed = new AITemplateManager;
            check(AITemplateManager::m_instance == doomed, "last wins");
            exec("aiTemplate", "create", {"Doomed"});
            check(get("aiTemplate", "active") == "Doomed", "the new manager is the console's");
            check(doomed->reset() == 1 && get("aiTemplate", "active") == "", "reset returns 1 and clears active");
            AITemplateManager::m_instance = &m.templates;    // [TEMP] hand the console back; `doomed` leaks
        }
    }
    else if (mode == "plugins")
    {
        Managers m;
        const int ids[6] = {0, 1, 2, 3, 4, 5};
        for (int kind = 0; kind < 6; ++kind)
        {
            const std::string name = std::string("p") + plugInTypes[kind];
            check(exec("aiTemplatePlugIn", "create", {plugInTypes[kind], name}) ==
                  "AITemplatePlugIn '" + name + "' successfully created", "create message");
            AITemplatePlugIn* const p = m.plugIns.getActiveTemplatePlugIn();
            check(p && p->getId() == ids[kind] && p->getTemplateKind() == 0 && m.plugIns.getTemplatePlugIn(name) == p,
                  "the kind's class, by getId");
            check(get("aiTemplatePlugIn", "active") == name, "active name");
            check(!m.plugIns.getTemplatePlugIn(std::string("P") + plugInTypes[kind]), "the plug-in map is case-SENSITIVE");
        }
        check(exec("aiTemplatePlugIn", "create", {"cover", "lower"}) == "TemplatePlugIn type 'cover' doesn't exist" &&
              !m.plugIns.getActiveTemplatePlugIn() && !m.plugIns.getTemplatePlugIn("lower"),
              "an unknown or wrongly-cased type: message, active cleared, nothing entered");
        AITemplateArmament* const arms = static_cast<AITemplateArmament*>(m.plugIns.getTemplatePlugIn("pArmament"));
        AITemplateCover* const cover = static_cast<AITemplateCover*>(m.plugIns.getTemplatePlugIn("pCover"));
        AITemplateMobile* const mobile = static_cast<AITemplateMobile*>(m.plugIns.getTemplatePlugIn("pMobile"));
        AITemplatePhysical* const physical = static_cast<AITemplatePhysical*>(m.plugIns.getTemplatePlugIn("pPhysical"));
        AITemplateUnit* const unit = static_cast<AITemplateUnit*>(m.plugIns.getTemplatePlugIn("pUnit"));
        check(!arms->m_initialized && arms->m_isAntiAircraft == 0, "Armament defaults");
        check(!cover->m_polygon && closeTo(cover->m_coverValue, 1.0f) && cover->m_height == 0.0f && !cover->m_initialized &&
              cover->m_transparency == 0.0f && !cover->m_portals && cover->m_pathfindingPlaneCount == 0 &&
              !cover->m_pathfindingPlaneCorners, "Cover defaults");
        check(mobile->m_vehicleNumber == -1 && mobile->m_maxSpeed == 0.0f && mobile->m_turnRadius == 0.0f &&
              mobile->m_coverSearchRadius == 0.0f && closeTo(mobile->m_lodHeight, 0.75f) &&
              closeTo(mobile->m_soundSphereRadius2, 30.0f) && mobile->m_soundSphereRadius1 == 0.0f &&
              closeTo(mobile->m_hearingProbability2, 0.1f) && closeTo(mobile->m_hearingProbability1, 0.01f) &&
              closeTo(mobile->m_avoidCollisionLookAhead, 2.0f) && !mobile->m_isTurnable, "Mobile defaults");
        check(physical->m_strType == 0, "Physical default");
        check(unit->m_maxAmmo == -1 && unit->m_equipmentType == 777 && !unit->m_initialized &&
              !unit->m_useNoPathfindingToGetToObject && !unit->m_hasExposedSoldier && !unit->m_alwaysTargetSoldier &&
              closeTo(unit->m_cullDistance, 1000.0f) && closeTo(unit->m_cullDistanceSquared, 1000000.0f) &&
              unit->m_selectKey == io::PIPositionSelect1, "Unit defaults");
        for (int i = 0; i < 6; ++i) check(unit->m_battleStrengths[i] == 0, "six zeroed battle strengths");
        for (int i = 0; i < 2; ++i) check(unit->m_strategicStrengths[i] == 0, "two zeroed strategic strengths");
        // Two sizing calls, then the loops' bound re-asked per test: 2 + 1 strategic, 6 + 1 battle.
        check(settingsCalls == 2 + 3 + 7, "the zeroing loops re-ask the bound every iteration");

        exec("aiTemplatePlugIn", "active", {"pArmament"});
        exec("aiTemplatePlugIn", "setIsAntiAircraft", {"2"});
        check(arms->m_isAntiAircraft == 2, "setIsAntiAircraft keeps a 2");
        exec("aiTemplatePlugIn", "active", {"pCover"});
        exec("aiTemplatePlugIn", "coverValue", {"2.5"}); exec("aiTemplatePlugIn", "transparency", {"0.25"});
        check(closeTo(cover->m_coverValue, 2.5f) && closeTo(cover->m_transparency, 0.25f), "Cover stores");
        exec("aiTemplatePlugIn", "active", {"pMobile"});
        exec("aiTemplatePlugIn", "setPathfindingMap", {"Vehicle"});
        check(mobile->m_vehicleNumber == mapId("Vehicle") && get("aiTemplatePlugIn", "vehicleNumber") == "10",
              "setPathfindingMap asks the courier and stores the number");
        exec("aiTemplatePlugIn", "setPathfindingMap", {"NoSuchMap"});
        check(mobile->m_vehicleNumber == -1, "an unknown map is the courier's -1");
        exec("aiTemplatePlugIn", "vehicleNumber", {"3"});
        check(mobile->m_vehicleNumber == 3, "vehicleNumber writes the same field");
        exec("aiTemplatePlugIn", "setSoundSphereRadius", {"0.0", "15.0"});
        exec("aiTemplatePlugIn", "setHearingProbability", {"0.01", "0.1"});
        check(mobile->m_soundSphereRadius1 == 0.0f && closeTo(mobile->m_soundSphereRadius2, 15.0f) &&
              closeTo(mobile->m_hearingProbability1, 0.01f) && closeTo(mobile->m_hearingProbability2, 0.1f),
              "the two-float setters store first and second where the image does");
        exec("aiTemplatePlugIn", "coverSearchRadius", {"50"}); exec("aiTemplatePlugIn", "maxSpeed", {"20"});
        exec("aiTemplatePlugIn", "turnRadius", {"5"}); exec("aiTemplatePlugIn", "lodHeight", {"1"});
        exec("aiTemplatePlugIn", "avoidCollisionLookAhead", {"4"}); exec("aiTemplatePlugIn", "isTurnable", {"1"});
        check(closeTo(mobile->m_coverSearchRadius, 50.0f) && get("aiTemplatePlugIn", "coverSearchRadius") == "50" &&
              closeTo(mobile->m_maxSpeed, 20.0f) && closeTo(mobile->m_turnRadius, 5.0f) && closeTo(mobile->m_lodHeight, 1.0f) &&
              closeTo(mobile->m_avoidCollisionLookAhead, 4.0f) && mobile->m_isTurnable, "Mobile stores");
        exec("aiTemplatePlugIn", "active", {"pPhysical"});
        exec("aiTemplatePlugIn", "setStrType", {"HeavyArmour"});
        check(physical->m_strType == 2 && get("aiTemplatePlugIn", "getStrType") == "2", "strType through the parser");
        exec("aiTemplatePlugIn", "setStrType", {"Nonsense"});
        check(physical->m_strType == 6, "an unknown strength type is CSTNumberOfTypes (6), as parsed");
        exec("aiTemplatePlugIn", "active", {"pUnit"});
        exec("aiTemplatePlugIn", "equipmentTypeName", {"ArmedCar"});
        check(unit->m_equipmentType == vehicleId("ArmedCar") && get("aiTemplatePlugIn", "equipmentTypeName") == "ArmedCar",
              "equipmentTypeName round-trips through IAISettings slots 21 and 20");
        exec("aiTemplatePlugIn", "equipmentTypeName", {"NoSuchVehicle"});
        check(unit->m_equipmentType == -1, "an unknown equipment type is -1");
        exec("aiTemplatePlugIn", "cullDistance", {"250"});
        check(closeTo(unit->m_cullDistance, 250.0f) && closeTo(unit->m_cullDistanceSquared, 62500.0f), "distance and its square");
        exec("aiTemplatePlugIn", "setStrategicStrength", {"1", "5"});
        exec("aiTemplatePlugIn", "setBattleStrength", {"3", "9"});
        check(unit->m_strategicStrengths[1] == 5 && unit->m_battleStrengths[3] == 9, "strength stores");
        // Stock soldiers write `setBattleStrength Infantry 4.0`: the int parse fails and keeps the
        // shared static's previous index (3 here), then "4.0" reads as 4. Retail's content bug, kept.
        exec("aiTemplatePlugIn", "setBattleStrength", {"Infantry", "4.0"});
        check(unit->m_battleStrengths[3] == 4 && unit->m_battleStrengths[0] == 0, "a failed index reuses the last one");
        exec("aiTemplatePlugIn", "setSelectKey", {"PIPositionSelect2"});
        check(unit->m_selectKey == io::PIPositionSelect2 && get("aiTemplatePlugIn", "selectKey") == "PIPositionSelect2",
              "setSelectKey resolves to the selectKey property through the registry's set-prefix rule");
        exec("aiTemplatePlugIn", "SelectKey", {"PIBogus"});
        check(unit->m_selectKey == io::PINone, "an unknown input name is PINone");
        exec("aiTemplatePlugIn", "setUseNoPathfindingToGetToObject", {"1"});
        exec("aiTemplatePlugIn", "setHasExposedSoldier", {"1"}); exec("aiTemplatePlugIn", "setAlwaysTargetSoldier", {"1"});
        check(unit->m_useNoPathfindingToGetToObject && unit->m_hasExposedSoldier && unit->m_alwaysTargetSoldier, "flags");
    }
    else if (mode == "controls")
    {
        Managers m;
        exec("aiTemplatePlugIn", "create", {"ControlInfo", "ctrl"});
        AITemplateControlInfo* const c = static_cast<AITemplateControlInfo*>(m.plugIns.getTemplatePlugIn("ctrl"));
        check(c->m_throttleSensitivity == 0.0f && c->m_proportionalConstant == -7.0f && c->m_derivativeConstant == -1.0f &&
              closeTo(c->m_angleSpeedConstant, 1.5f) && closeTo(c->m_maxSpeedBumpinessFallOff, 0.002f) &&
              c->m_driveThrottleControl == io::PINone && c->m_driveTurnControl == io::PINone &&
              c->m_aimVerticalControl == io::PINone && c->m_aimHorizontalControl == io::PINone &&
              c->m_lookVerticalControl == io::PINone && c->m_lookHorizontalControl == io::PINone &&
              c->m_driveControls == 0 && c->m_aimControls == 0 && c->m_lookControls == 0 &&
              c->m_cameraRelativeDofRotationOffset == 0.0f && !c->m_initialized && !c->m_hasCameraRelativeRotationLimits,
              "ControlInfo defaults");
        const float pi = 3.1415927f;
        check(c->m_cameraRelativeMinRotation.x == -pi && c->m_cameraRelativeMinRotation.z == -pi &&
              c->m_cameraRelativeMaxRotation.y == pi, "camera limits default to +-pi");
        check(std::memcmp(&c->m_cameraBaseTransformation, &Mat4::getIdentity(), sizeof(Mat4)) == 0 &&
              std::memcmp(&c->m_unknown100, &Mat4::getIdentity(), sizeof(Mat4)) == 0, "both matrices identity");
        // The 22 float properties, each set through the console and read back raw and through GET.
        const char* floats[] = {"throttleSensitivity", "pitchSensitivity", "rollSensitivity", "yawSensitivity",
            "proportionalConstant", "derivativeConstant", "angleSpeedConstant", "throttleLookAhead", "pitchLookAhead",
            "rollLookAhead", "yawLookAhead", "throttleScale", "pitchScale", "rollScale", "yawScale",
            "lookVerticalSensitivity", "lookHorizontalSensitivity", "lookVerticalLookAhead", "lookHorizontalLookAhead",
            "lookVerticalScale", "lookHorizontalScale", "maxSpeedBumpinessFallOff"};
        float* const fields[] = {&c->m_throttleSensitivity, &c->m_pitchSensitivity, &c->m_rollSensitivity,
            &c->m_yawSensitivity, &c->m_proportionalConstant, &c->m_derivativeConstant, &c->m_angleSpeedConstant,
            &c->m_throttleLookAhead, &c->m_pitchLookAhead, &c->m_rollLookAhead, &c->m_yawLookAhead,
            &c->m_throttleScale, &c->m_pitchScale, &c->m_rollScale, &c->m_yawScale, &c->m_lookVerticalSensitivity,
            &c->m_lookHorizontalSensitivity, &c->m_lookVerticalLookAhead, &c->m_lookHorizontalLookAhead,
            &c->m_lookVerticalScale, &c->m_lookHorizontalScale, &c->m_maxSpeedBumpinessFallOff};
        for (int i = 0; i < 22; ++i)
        {
            std::ostringstream text; text << (i + 1) * 0.5f;
            exec("aiTemplatePlugIn", floats[i], {text.str()});
            check(closeTo(*fields[i], (i + 1) * 0.5f) && get("aiTemplatePlugIn", floats[i]) == text.str(),
                  "each float property writes its own field");
        }
        exec("aiTemplatePlugIn", "maxClimbAngle", {"0.3333"}); exec("aiTemplatePlugIn", "maxRollAngle", {"0.99"});
        check(closeTo(c->m_maxClimbAngle, 0.3333f) && closeTo(c->m_maxRollAngle, 0.99f), "the two angles");
        exec("aiTemplatePlugIn", "driveThrottleControl", {"PIThrottle"});
        exec("aiTemplatePlugIn", "driveTurnControl", {"PIYaw"});
        check(c->m_driveThrottleControl == io::PIThrottle && c->m_driveTurnControl == io::PIYaw &&
              c->m_driveControls == ((1ull << 3) | 1ull), "drive inputs and their bits");
        check(get("aiTemplatePlugIn", "driveControls") == "9" && get("aiTemplatePlugIn", "driveThrottleControl") == "PIThrottle",
              "the read-only mask through the virtual getter, and an input by name");
        exec("aiTemplatePlugIn", "drivePitchControl", {"PIPitch"}); exec("aiTemplatePlugIn", "driveRollControl", {"PIRoll"});
        check(c->m_driveControls == ((1ull << 3) | 1ull | 2ull | 4ull), "pitch and roll share the drive mask");
        exec("aiTemplatePlugIn", "aimVerticalControl", {"PIAltFire"});
        check(c->m_aimVerticalControl == io::PIAltFire && c->m_aimControls == 0xFFFFFFFF80000000ull,
              "input 31: the 32-bit shift sign-extends into the upper half");
        exec("aiTemplatePlugIn", "aimThrottleControl", {"PIReload"});
        check(c->m_aimThrottleControl == io::PIReload && c->m_aimControls == 0xFFFFFFFF80000001ull,
              "input 32 aliases bit 0 (x86 masks the shift count)");
        exec("aiTemplatePlugIn", "lookHorizontalControl", {"PIBogus"});
        check(c->m_lookHorizontalControl == io::PINone && c->m_lookControls == 1ull, "PINone (64) also aliases bit 0");
        exec("aiTemplatePlugIn", "aimHorizontalControl", {"PIMouseLookX"}); exec("aiTemplatePlugIn", "aimRollControl", {"PIRoll"});
        exec("aiTemplatePlugIn", "lookVerticalControl", {"PIMouseLookY"});
        check(c->m_aimHorizontalControl == io::PIMouseLookX && c->m_aimRollControl == io::PIRoll &&
              c->m_lookVerticalControl == io::PIMouseLookY && c->m_lookControls == (1ull | (1ull << 5)), "remaining inputs");
        exec("aiTemplatePlugIn", "setCameraRelativeDofRotationOffsetDeg", {"270"});
        check(c->m_cameraRelativeDofRotationOffset == 270.0f * 0.017453292f, "degrees to radians, single precision");
        exec("aiTemplatePlugIn", "setCameraRelativeMinRotationDeg", {"-400/-10/0"});
        check(c->m_hasCameraRelativeRotationLimits && c->m_cameraRelativeMinRotation.x == -pi &&
              c->m_cameraRelativeMinRotation.y == -10.0f * 0.017453292f && c->m_cameraRelativeMinRotation.z == 0.0f,
              "min: clamped to -pi, converted");
        exec("aiTemplatePlugIn", "setCameraRelativeMaxRotationDeg", {"100/60/720"});
        check(c->m_cameraRelativeMaxRotation.x == 100.0f * 0.017453292f && c->m_cameraRelativeMaxRotation.y ==
              60.0f * 0.017453292f && c->m_cameraRelativeMaxRotation.z == pi, "max: converted, z clamped to pi");
        Vec3 nan; nan.x = nan.y = nan.z = std::numeric_limits<float>::quiet_NaN();
        c->setCameraRelativeMaxRotationDeg(nan);
        check(c->m_cameraRelativeMaxRotation.x == pi && c->m_cameraRelativeMaxRotation.y == pi &&
              c->m_cameraRelativeMaxRotation.z != c->m_cameraRelativeMaxRotation.z,
              "a NaN clamps to pi in X and Y and stays NaN in Z");
        check(c->getDriveControls() == c->m_driveControls && c->getAimControls() == c->m_aimControls &&
              c->getLookControls() == c->m_lookControls, "the three virtual getters");
    }
    else if (mode == "parsers")
    {
        const char* names[27] = {"ITProduction", "ITAmmunition", "ITRepair", "ITUnit", "ITAir", "ITHelicopter",
            "ITGround", "ITNaval", "ITControlpoint", "ITRadar", "ITAirfield", "ITTransportation", "ITArtillery",
            "ITFixed", "ITCover", "ITMobile", "ITUnmanned", "ITLowPriority", "ITStructure", "ITBiological",
            "ITVegetation", "ITSoldier", "ITObstructedView", "ITNoTemperature", "ITHasWreck", "ITObstacle",
            "ITHasCarHorn"};
        for (int i = 0; i < 27; ++i)
        {
            InformationType value = static_cast<InformationType>(99);
            std::istringstream in(names[i]);
            check(&(in >> value) == &in && value == i, "every name parses to its value");
            std::ostringstream out; out << static_cast<InformationType>(i);
            check(out.str() == names[i], "every value prints its name");
        }
        for (const char* bad : {"", "itsoldier", "ITSOLDIER", "5", "IT Soldier", "ITHelicopters"})
        {
            InformationType value = static_cast<InformationType>(3);
            std::istringstream in(bad);
            in >> value;
            check(value == -1, "a miss always stores -1, case-sensitive, no numbers");
        }
        std::ostringstream none; none << static_cast<InformationType>(-1) << static_cast<InformationType>(27);
        check(none.str().empty(), "an out-of-range value prints nothing");
    }
    else if (mode == "weapons")
    {
        Managers m;
        check(get("weaponTemplate", "active") == "", "no active weapon reads empty");
        check(exec("weaponTemplate", "create", {"Gun"}) == "WeaponTemplate 'Gun' successfully created", "create message");
        WeaponTemplate* const gun = m.weapons.getActiveTemplate();
        check(gun && m.weapons.getTemplate("gun") == gun && get("weaponTemplate", "active") == "Gun",
              "created, active, case-insensitive");
        check(!gun->m_indirect && !gun->m_initialized && !gun->m_isThrown && !gun->m_useAimerOnly &&
              gun->m_projectileMaterial == -1 && gun->m_maxAmmo == 0 && gun->m_maxAmmoInMag == 0 &&
              gun->m_fireRate == -1 &&
              gun->m_drag == 0.0f && gun->m_minRange == 0.0f && gun->m_maxRange == 0.0f &&
              gun->m_explosionRadius == 0.0f && gun->m_soundSphereRadius == 50.0f && gun->m_exitVelocity == 0.0f &&
              gun->m_gravityModifier == 0.0f && gun->m_deviation == 14.0f && gun->m_deviationCorrectionTime == 15.0f &&
              gun->m_deviationCorrectionRate == 0.06666667f && gun->m_firingPose == SPStanding &&
              gun->m_allowedDeviation == 0.5f && gun->m_optimalRangePercentage == 0.35f &&
              gun->m_firesThroughTransparent && gun->m_weaponGuidingSystem == WGSNone, "constructor defaults");
        for (int i = 0; i < 6; ++i) check(gun->m_strengths[i] == 0, "six zeroed strengths (the manager's 6)");
        check(exec("weaponTemplate", "create", {"GUN"}) == "Error: Template 'GUN' already exists" &&
              !m.weapons.getActiveTemplate(), "a duplicate is ONLY a message here (no Debug), and clears active");
        exec("weaponTemplate", "active", {"Gun"});
        exec("weaponTemplate", "deviationCorrectionTime", {"4"});
        check(gun->m_deviationCorrectionTime == 4.0f && gun->m_deviationCorrectionRate == 0.25f, "time and reciprocal");
        exec("weaponTemplate", "deviationCorrectionTime", {"0"});
        check(gun->m_deviationCorrectionTime == 0.0f && gun->m_deviationCorrectionRate == 0.0f, "zero time, zero rate");
        exec("weaponTemplate", "isThrown", {"1"});
        check(gun->getIsThrown() && get("weaponTemplate", "isThrown") == "1", "isThrown through its accessors");
        exec("weaponTemplate", "setStrength", {"HeavyArmour", "7"}); exec("weaponTemplate", "setStrength", {"Infantry", "4.0"});
        check(gun->m_strengths[2] == 7 && gun->m_strengths[0] == 4, "setStrength by parsed type, int prefix");
        ConsoleObject* const getter = mainConsoleObjects->getConsoleObject("weaponTemplate", "getStrength");
        std::string one[] = {"HeavyArmour", ""};
        check(getter && getter->execute(one, 1).empty() && !getter->hasReturnValue(),
              "retail's getStrength returns nothing");
        exec("weaponTemplate", "setFiringPose", {"Crouching"});
        check(gun->m_firingPose == SPCrouching && get("weaponTemplate", "firingPose") == "SPCrouching",
              "setFiringPose resolves to firingPose; the pose prints with its SP prefix");
        exec("weaponTemplate", "firingPose", {"SPLying"});
        check(gun->m_firingPose == SPLying, "prefixed spelling");
        exec("weaponTemplate", "firingPose", {"crouching"});
        check(gun->m_firingPose == SPNotAvailable, "case-sensitive: a miss is SPNotAvailable");
        exec("weaponTemplate", "weaponActivate", {"PINone"}); exec("weaponTemplate", "weaponFire", {"PIAltFire"});
        check(gun->m_weaponActivate == io::PINone && gun->m_weaponFire == io::PIAltFire &&
              get("weaponTemplate", "weaponFire") == "PIAltFire", "the two inputs");
        const char* floats[] = {"deviation", "drag", "minRange", "maxRange", "soundSphereRadius", "exitVelocity",
                                "optimalRangePercentage", "explosionRadius", "allowedDeviation"};
        float* const fields[] = {&gun->m_deviation, &gun->m_drag, &gun->m_minRange, &gun->m_maxRange,
                                 &gun->m_soundSphereRadius, &gun->m_exitVelocity, &gun->m_optimalRangePercentage,
                                 &gun->m_explosionRadius, &gun->m_allowedDeviation};
        for (int i = 0; i < 9; ++i)
        {
            std::ostringstream text; text << (i + 1) * 1.5f;
            exec("weaponTemplate", floats[i], {text.str()});
            check(closeTo(*fields[i], (i + 1) * 1.5f) && get("weaponTemplate", floats[i]) == text.str(),
                  "each float property writes its own field");
        }
        exec("weaponTemplate", "fireRate", {"100"}); exec("weaponTemplate", "indirect", {"1"});
        exec("weaponTemplate", "useAimerOnly", {"1"}); exec("weaponTemplate", "setFiresThroughTransparent", {"0"});
        check(gun->m_fireRate == 100 && gun->m_indirect && gun->m_useAimerOnly && !gun->m_firesThroughTransparent,
              "int and bool fields");
        exec("weaponTemplate", "create", {"Second"});
        check(m.weapons.reset() == 1 && !m.weapons.getTemplate("Gun") && !m.weapons.getTemplate("Second") &&
              get("weaponTemplate", "active") == "", "reset REALLY empties this manager");
        check(exec("weaponTemplate", "create", {"Gun"}) == "WeaponTemplate 'Gun' successfully created",
              "and the name is free again - the destructor then resets a live map safely");
    }
    else if (mode == "stock")
    {
        Managers m;
        const unsigned expected = expectedCounts[10];
        check(sizeof(stockCommands) / sizeof(stockCommands[0]) == expected, "generated command table");
        unsigned created = 0, plugIns = 0, added = 0, errors = 0, weapons = 0, weaponDuplicates = 0;
        std::vector<std::string> templateNames, plugInNames, weaponNames;
        unsigned kinds[6] = {0, 0, 0, 0, 0, 0};
        for (unsigned i = 0; i < expected; ++i)
        {
            std::istringstream parse(stockCommands[i]);
            std::string command; parse >> command;
            std::vector<std::string> args; std::string token;
            while (parse >> token) args.push_back(token);
            const std::string object = command.substr(0, command.find('.'));
            const std::string method = command.substr(command.find('.') + 1);
            const std::string result = exec(object, method, args);
            if (object == "weaponTemplate" && result.find("Error: Template '") == 0)
            { ++weaponDuplicates; std::printf("stock weapon duplicate: %s\n", stockCommands[i]); continue; }
            if (result.find("Error") != std::string::npos || result.find("doesn't exist") != std::string::npos)
            { ++errors; std::printf("stock error: %s -> %s\n", stockCommands[i], result.c_str()); }
            if (result.find("AITemplate '") == 0) { ++created; templateNames.push_back(args[0]); }
            if (result.find("AITemplatePlugIn '") == 0 && result.find("successfully") != std::string::npos)
            { ++plugIns; plugInNames.push_back(args[1]); ++kinds[m.plugIns.getActiveTemplatePlugIn()->getId()]; }
            if (result.find("Successfully added plugin ") == 0) ++added;
            if (result.find("WeaponTemplate '") == 0) { ++weapons; weaponNames.push_back(args[0]); }
        }
        check(errors == 0, "every live stock command succeeds");
        check(weapons == expectedCounts[11] && weaponDuplicates == expectedCounts[12],
              "weapon templates (a duplicate weapon name is only a message)");
        for (const std::string& name : weaponNames) check(m.weapons.getTemplate(name) != 0, "each weapon found");
        check(created == expectedCounts[0] && plugIns == expectedCounts[1] && added == expectedCounts[2],
              "templates, plug-ins and additions as the content defines them");
        check(kinds[0] == expectedCounts[3] && kinds[1] == expectedCounts[4] && kinds[2] == expectedCounts[5] &&
              kinds[3] == expectedCounts[6] && kinds[4] == expectedCounts[7] && kinds[5] == expectedCounts[8],
              "the six plug-in kinds");
        for (const std::string& name : templateNames) check(m.templates.getTemplate(name) != 0, "each template found");
        for (const std::string& name : plugInNames) check(m.plugIns.getTemplatePlugIn(name) != 0, "each plug-in found");
        m.templates.createWreckTemplates();
        unsigned wrecks = 0;
        for (const std::string& name : templateNames)
        {
            AITemplate* const t = m.templates.getTemplate(name);
            if (t->m_wreckTemplate)
            {
                ++wrecks;
                check(t->m_wreckTemplate->m_name == t->m_name + "_wreck" && t->isPhysical(), "wreck of a physical template");
            }
            check(!!t->m_wreckTemplate == !!(t->m_types & (1 << 24)), "a wreck exactly for ITHasWreck");
        }
        check(wrecks == expectedCounts[9], "wreck count");
        std::printf("stock: %u commands, %u templates, %u plug-ins (arm %u ctrl %u cover %u mobile %u phys %u unit %u),"
                    " %u additions, %u wrecks, %u weapons (%u duplicate names), %u path maps, %u vehicle names\n",
                    expected, created, plugIns, kinds[0], kinds[1], kinds[2], kinds[3], kinds[4], kinds[5], added,
                    wrecks, weapons, weaponDuplicates, static_cast<unsigned>(pathMaps.size()),
                    static_cast<unsigned>(vehicles.size()));
    }
    else if (mode == "duplicate_template" || mode == "duplicate_plugin")
    {
        setDebugCallback(DebugTypeError, &errorCallback, 0);
        Managers m;
        if (mode == "duplicate_template")
        {
            exec("aiTemplate", "create", {"twice"});
            exec("aiTemplate", "create", {"TWICE"});      // the name map is case-insensitive
        }
        else
        {
            exec("aiTemplatePlugIn", "create", {"Cover", "twice"});
            exec("aiTemplatePlugIn", "create", {"Unit", "twice"});
        }
        std::printf("REACHED-AFTER the duplicate\n");
        return 0;
    }
    else
        unexpected(mode.c_str());

    mainConsoleObjects->unregisterConsoleObjects(kitTemplateConsoleObjects());
    check(mainConsoleObjects->getConsoleObjects().empty(), "list unregistered");
    std::printf("PASS %s %u checks\n", mode.c_str(), checks);
    return 0;
}
