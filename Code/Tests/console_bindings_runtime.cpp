// [TEMP] [CHOSEN, not recovered] Real console/receiver checks without engine startup.
// Models input availability and authored descriptor state; it does not construct the GPU-owning
// particle manager or exercise particle collection. Unused observer methods abort. Services live until exit.
#include "../BF2/Core/ConsoleObjectMacro.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/Modules.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/IO/File/FileManager.h"
#include "../BF2/IO/File/UnixFileSystem.h"
#include "../BF2/IO/Input/NullInputDeviceManager.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Game/Objects/Base/SimpleObjectTemplate.h"
#include "../BF2/Game/Objects/PlayerControlObjectTemplate.h"
#include "../BF2/Game/Objects/ToggleCameraCompTemplate.h"
#include "../BF2/Game/Objects/Fx/ParticleSystemTemplates.h"
#include "../BF2/Game/Objects/Base/BundleTemplate.h"
#include "../BF2/Game/Objects/EngineTemplate.h"
#include "../BF2/Game/Objects/TemplateLeaves.h"
#include "../BF2/Game/Objects/Game/AreaObject.h"
#include "../BF2/Game/Objects/Visual/LightSource.h"
#include "../BF2/Game/Objects/Visual/LightSourceTemplate.h"
#include "../BF2/Geom/ParticleSystem/ParticleSystemManager.h"
#include <template_save_descriptors.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <sys/stat.h>

namespace dice { namespace hfe { extern ClassManager* classManager;
namespace io { extern IBase* inputDeviceManager; }
} }
using namespace dice::hfe;
using namespace dice::hfe::world;
static int checks, failures;
static void check(bool value, const char* text)
{ ++checks; if (!value) { ++failures; std::fprintf(stderr, "FAIL %s\n", text); } }
static ConsoleObject* row(const char* object, const char* method)
{
    auto* result = mainConsoleObjects->getConsoleObject(object, method);
    check(result != 0, method);
    if (!result) std::abort();
    return result;
}
static ConsoleObject* declared(const char* tag, const char* name)
{
    for (auto* binding : consoleBindingList(tag))
        if (std::string(binding->getFunctionName()) == name) return binding;
    check(false, name); std::abort();
}
static std::string call(const char* object, const char* method, std::initializer_list<std::string> values = {})
{
    std::string args[10]; unsigned int i = 0;
    for (const auto& value : values) args[i++] = value;
    return row(object, method)->execute(args, static_cast<int>(i));
}
struct Simple : SimpleObjectTemplate { unsigned int grip() const { return m_grip; } };
struct Sprite : SpriteParticleSystemTemplate
{ void attach(geom::IParticleSystemTemplate* value) { m_impl = value; } };
struct SkyLight : LightSource
{
    explicit SkyLight(const LightSourceTemplate* value) : LightSource(value) {}
    int selections = 0;
    void setAsSkyLight() override { ++selections; }
};
static std::vector<int> arrowCalls;
struct Arrow : Bundle
{
    Arrow(const BundleTemplate* value, AreaObject* owner) : Bundle(value), area(owner) {}
    AreaObject* area;
    Vec3 position;
    void init() override { arrowCalls.push_back(1); area->setAbsolutePosition(Vec3{10, 11, 12}); }
    void setAbsolutePosition(const Vec3& value) override { arrowCalls.push_back(2); position = value; }
};
struct ArrowTemplate : BundleTemplate
{
    AreaObject* area = 0;
    mutable Arrow* arrow = 0;
    IObject* createObject() const override
    { arrowCalls.push_back(0); arrow = new Arrow(this, area); return arrow; }
};
struct Templates : ObjectTemplateManager
{
    using ObjectTemplateManager::getTemplate;
    IObjectTemplate* directionArrow = 0;
    IObjectTemplate* getTemplate(const std::string& name) const override
    { return name == "DirectionArrow" ? directionArrow : ObjectTemplateManager::getTemplate(name); }
};
struct Device : io::IAdvancedInputDevice
{
    io::InputDeviceAxes axis = io::IDAxis_None; bool inverted = false; unsigned int writes = 0;
    void setInvertAxis(io::InputDeviceAxes a, bool b) override { axis = a; inverted = b; ++writes; }
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    io::InputDeviceTypes getType() const override { std::abort(); }
    bool setCooperativeMode(io::InputDeviceMode) override { std::abort(); }
    io::InputDeviceMode getCooperativeMode() const override { std::abort(); }
    bool activate() override { std::abort(); }
    bool deActivate() override { std::abort(); }
    void update(float) override { std::abort(); }
    bool isActive() const override { std::abort(); }
    bool getNextInput(io::InputDeviceAxes&, io::InputDeviceButtons&, io::InputDeviceKeys&, bool&) override { std::abort(); }
    int getNumAxes() const override { std::abort(); }
    float readAxis(io::InputDeviceAxes, bool) const override { std::abort(); }
    bool getInvertAxis(io::InputDeviceAxes) const override { std::abort(); }
    void setAxisScale(io::InputDeviceAxes, float) override { std::abort(); }
    float getAxisScale(io::InputDeviceAxes) const override { std::abort(); }
    void setAxisAdd(io::InputDeviceAxes, float) override { std::abort(); }
    float getAxisAdd(io::InputDeviceAxes) const override { std::abort(); }
    int getNumButtons() const override { std::abort(); }
    bool buttonDown(io::InputDeviceButtons) const override { std::abort(); }
    bool buttonPressed(io::InputDeviceButtons) const override { std::abort(); }
    bool buttonReleased(io::InputDeviceButtons) const override { std::abort(); }
    float buttonPressedSampleLatency(io::InputDeviceButtons) const override { std::abort(); }
};
struct Devices : io::NullInputDeviceManager
{
    Device* device = 0;
    io::IInputDevice* getInputDevice(io::InputDeviceFlags flags) const override
    { check(flags == io::IDFGameController_0, "device flags forwarded"); return device; }
};

int main()
{
    eventManager = new EventManager;
    classManager = new ClassManager;
    classManager->registerServer(new ObjectModule);
    auto* templates = new Templates;
    objectTemplateManager = templates;
    objectManager = new ObjectManager;
    playerManager = new PlayerManager;
    auto* console = new io::OldConsole;
    io::mainConsole = console;
    io::fileManager = new io::FileManager;
    io::fileManager->setBaseSystem(new io::UnixFileSystem);
    for (const char* tag : {"io.FileManager", "io.ControlMap", "world.objTemplBase", "world.objTemplGame", "world.objTemplVehicle",
                            "world.objTemplFx", "world.objectManager", "tool.moduleToolObject",
                            "world.physics", "world.physics.collisionManager", "world.physics.heightmap"})
        mainConsoleObjects->registerConsoleObjects(consoleBindingList(tag));

    for (const char* name : {"workingPath", "echo", "echoErrors", "maxLineSize", "maxHistorySize",
                             "maxCommandHistorySize", "useRelativePaths", "listVariables", "listConstants",
                             "run", "bindKeyToConsoleScript"})
    {
        auto* binding = row("Console", name);
        check(binding->getAccess() == 2 && binding->getCategory() == 0 && !binding->isDynamic(), "Console metadata");
    }
    for (const char* name : {"echo", "echoErrors", "useRelativePaths"})
    {
        call("Console", name, {"0"}); check(call("Console", name) == "0", "bool set/get false");
        call("Console", name, {"1"}); check(call("Console", name) == "1", "bool set/get true");
    }
    call("Console", "workingPath", {"subdir/"});
    check(call("Console", "workingPath") == "subdir/", "working path round trip");
    call("Console", "workingPath", {""}); call("Console", "useRelativePaths", {"0"});
    for (const char* name : {"maxLineSize", "maxHistorySize", "maxCommandHistorySize"})
    { call("Console", name, {"321"}); check(call("Console", name) == "321", "integer state round trip"); }
    console->addVariable("v_fixture", "value"); console->addVariable("other", "second");
    console->addConstant("c_fixture", "constant");
    check(call("Console", "listVariables") == "v_fixture value\n", "default variable prefix");
    check(call("Console", "listVariables", {"oth"}) == "other second\n", "explicit variable prefix");
    check(call("Console", "listConstants") == "c_fixture constant\n", "default constant prefix");
    check(call("Console", "listConstants", {"missing"}).empty(), "empty constant selection");
    check(call("Console", "bindKeyToConsoleScript", {"1", "script.con"}) == "1", "bind first key");
    check(call("Console", "bindKeyToConsoleScript", {"12", ""}) == "1", "clear last key");
    check(call("Console", "bindKeyToConsoleScript", {"0", "script.con"}) == "0", "reject key below range");
    check(call("Console", "bindKeyToConsoleScript", {"13", "script.con"}) == "0", "reject key above range");

    mkdir("files", 0700);
    call("FileManager", "addPath", {"files/"});
    std::ofstream("files/source.txt") << "binding data";
    check(io::fileManager->fileExists("source.txt"), "added path reaches loose file");
    check(call("FileManager", "copyFile", {"source.txt", "copy.txt"}) == "1", "real file copy");
    check(call("FileManager", "copyFile", {"source.txt", "copy.txt"}) == "0", "existing destination refused");
    check(call("FileManager", "moveFile", {"copy.txt", "moved.txt"}) == "1", "real file move");
    check(!io::fileManager->fileExists("copy.txt") && io::fileManager->fileExists("moved.txt"), "move updates paths");
    check(call("FileManager", "deleteFile", {"moved.txt"}) == "1", "real file delete");
    check(call("FileManager", "deleteFile", {"moved.txt"}) == "0", "missing delete refused");
    std::ofstream("files/script.con") << "Console.maxLineSize v_arg1\n";
    check(call("Console", "run", {"script.con", "87"}) == "1" && console->getMaxLineSize() == 87, "run forwards script arguments");
    check(call("Console", "run", {"missing.con"}) == "0", "run reports missing script");

    io::inputDeviceManager = 0;
    check(call("InputDevices", "setInvertAxis", {"IDFGameController_0", "IDAxis_7", "1"}) == "0", "missing device manager");
    auto* devices = new Devices; io::inputDeviceManager = devices;
    check(call("InputDevices", "setInvertAxis", {"IDFGameController_0", "IDAxis_7", "1"}) == "0", "unavailable device");
    devices->device = new Device;
    check(call("InputDevices", "setInvertAxis", {"IDFGameController_0", "IDAxis_7", "1"}) == "1", "available device");
    check(devices->device->writes == 1 && devices->device->axis == io::IDAxis_7 && devices->device->inverted, "axis and inversion forwarded");

    auto* simple = new Simple; simple->setName("simple"); objectTemplateManager->setActiveObjectTemplate(simple);
    const char* grips[] = {"PGFNoGrip", "PGFContactGrip", "PGFRollGrip", "PGFEngineGrip",
        "PGFRollGripWhenOccupied", "PGFDummyGrip", "PGFStaticFriction", "PGFEngineDummyGrip"};
    for (unsigned int i = 0; i < 8; ++i)
    {
        call("ObjectTemplate", "grip", {grips[i]});
        check(simple->grip() == (1u << i) && call("ObjectTemplate", "grip") == grips[i], "named grip round trip");
        call("ObjectTemplate", "grip", {std::string("c_") + grips[i]});
        check(simple->grip() == (1u << i), "constant grip spelling");
        call("ObjectTemplate", "grip", {std::to_string(1u << i)});
        check(simple->grip() == (1u << i), "numeric grip normalization");
    }
    call("ObjectTemplate", "grip", {"3"}); check(simple->grip() == 1, "unnamed numeric grip selects NoGrip");
    call("ObjectTemplate", "grip", {"pgfEngineGrip"}); check(simple->grip() == 1, "grip is case-sensitive");
    auto* grip = row("ObjectTemplate", "grip");
    check(grip->hasRange() && call("Console", "getRange", {"ObjectTemplate", "grip"}) == "0 7", "numeric grip reflection range");
    check(grip->getDeltaAsString() == "PGFNoGrip", "grip delta text");
    const auto range = grip->getRange();
    check(*static_cast<const unsigned int*>(range.first) == 0 && *static_cast<const unsigned int*>(range.second) == 7,
          "grip range pointers carry dword bounds");
    call("ObjectTemplate", "grip", {"128"});
    static_cast<ConsoleObjectBaseImpl*>(grip)->checkObjectRange(true);
    static_cast<ConsoleObjectBaseImpl*>(grip)->executeObjectMethod();
    check(simple->grip() == 128, "grip range metadata does not clamp valid flags");
    check(call("Console", "getEnumName", {"ObjectTemplate", "grip", "8"}) == "PGFEngineGrip"
          && simple->grip() == 128, "enum reflection restores the original grip");
    call("ObjectTemplate", "grip", {""}); check(simple->grip() == 1, "empty grip uses the reader fallback");

    auto* pco = new PlayerControlObjectTemplate; pco->setName("pco"); objectTemplateManager->setActiveObjectTemplate(pco);
    call("ObjectTemplate", "setToolTipType", {"TTHeavyTank"}); check(pco->getToolTipType() == 0, "tooltip full name");
    call("ObjectTemplate", "setToolTipType", {"M3GMC"}); check(pco->getToolTipType() == 21, "tooltip short name");
    call("ObjectTemplate", "setToolTipType", {""}); check(pco->getToolTipType() == 22, "empty tooltip uses the reader fallback");
    call("ObjectTemplate", "setToolTipType", {"0"}); check(pco->getToolTipType() == 22, "tooltip has no numeric arm");
    check(row("ObjectTemplate", "setToolTipType")->getCategory() == 9, "tooltip Hud category");
    for (int i = -1; i < 4; ++i)
    {
        call("ObjectTemplate", "vehicleCameraShake", {std::to_string(i)});
        check(pco->getVehicleCameraShake() == i, "numeric camera shake");
    }
    call("ObjectTemplate", "vehicleCameraShake", {"cstexplosion"});
    check(pco->getVehicleCameraShake() == 0 && call("ObjectTemplate", "vehicleCameraShake") == "CstExplosion", "camera shake name and display");
    call("ObjectTemplate", "vehicleCameraShake", {"unknown"}); check(pco->getVehicleCameraShake() == 0, "unknown shake retains parsed value");
    check(row("ObjectTemplate", "vehicleCameraShake")->getCategory() == 15, "shake PlayerControlObject category");

    auto* zoom = new ToggleCameraCompTemplate;
    simple->setComponent(zoom->getInterfaceID(), static_cast<IObjectTemplateComponent*>(zoom));
    objectTemplateManager->setActiveObjectTemplate(simple);
    const char* views[] = {"CVMFirstPerson", "CVMInside", "CVMNose", "CVMChase", "CVMFrontChase", "CVMFlyBy", "CVMTrace", "CVMExternTrace"};
    for (const char* name : {"zoom.startCameraViewMode", "zoom.changeCameraViewMode"})
        for (int i = 0; i < 8; ++i)
        {
            call("ObjectTemplate", name, {views[i]}); check(call("ObjectTemplate", name) == views[i], "camera view name round trip");
            call("ObjectTemplate", name, {std::to_string(i)}); check(call("ObjectTemplate", name) == views[i], "camera view numeric round trip");
        }

    auto* sprite = new Sprite; sprite->setName("sprite");
    auto* descriptor = new geom::SaveDescriptor0;
    sprite->attach(descriptor); objectTemplateManager->setActiveObjectTemplate(sprite);
    for (const char* name : {"nearCullingEnable", "isStaticSystem"})
    {
        auto* binding = row("ObjectTemplate", name);
        check(binding->getAccess() == 2 && binding->getCategory() == 21 && binding->isDynamic(), "particle metadata");
        call("ObjectTemplate", name, {"1"}); check(call("ObjectTemplate", name) == "1", "particle flag true");
        call("ObjectTemplate", name, {"0"}); check(call("ObjectTemplate", name) == "0", "particle flag false");
        call("ObjectTemplate", name, {"1"});
    }
    check(descriptor->getNearCullingEnable(), "near flag reaches typed renderer descriptor slot");
    check(descriptor->getIsStaticSystem(), "static flag reaches typed renderer descriptor slot");
    objectTemplateManager->setActiveObjectTemplate(simple);
    check(!mainConsoleObjects->getConsoleObject("ObjectTemplate", "nearCullingEnable"), "near culling rejects non-sprites");

    objectTemplateManager->registerTemplateClass(10, "First"); objectTemplateManager->registerTemplateClass(20, "Second");
    check(call("ObjectTemplate", "listTemplateClasses") == "First\nSecond\n", "template class listing");
    playerManager->registerPlayerClass(10, "PlayerFirst");
    check(call("Player", "listPlayerClasses") == "PlayerFirst\n", "player class listing");
    call("Terrain", "refreshTerrainSampling");
    check(call("Editor", "getActiveObjectTemplate") == "simple", "active template proxy");
    check(call("Editor", "getActiveObject") == "NoObject", "null active object proxy");
    for (const char* name : {"getNameOfObjectInFocus", "identityRotateObjectInFocus", "buildDebugCollisionMeshes"})
    {
        auto* binding = declared("world.physics.collisionManager", name);
        check(!binding->isDynamic() && binding->getMinNoArgs() == 0 && binding->getMaxNoArgs() == 0
              && std::string(binding->getReturnType()) == "void", "collision command metadata");
        check(!binding->isActive(), "collision command rejects missing manager");
    }
    auto* physics = declared("world.physics", "listProperties");
    check(physics->isDynamic() && std::string(physics->getReturnType()) == "std::string"
          && !physics->isActive(), "physics listing metadata and singleton guard");
    check(!mainConsoleObjects->getConsoleObject("Object", "setAsSkyLight")
          && !mainConsoleObjects->getConsoleObject("Object", "getLightSourceMask"), "object inspection requires an active object");
    check(!mainConsoleObjects->getConsoleObject("Object", "addAbsolutePoint"), "area editing needs an active object");
    auto* bundle = new BundleTemplate;
    bundle->setName("bundle"); objectTemplateManager->setActiveObjectTemplate(bundle);
    auto* childQuery = row("ObjectTemplate", "getTemplate");
    check(childQuery->getAccess() == 2 && childQuery->getCategory() == 1 && childQuery->isDynamic()
          && childQuery->getMinNoArgs() == 0 && childQuery->getMaxNoArgs() == 1
          && std::string(childQuery->getReturnType()) == "std::string", "child query metadata");
    check(call("ObjectTemplate", "getTemplate").empty(), "empty bundle child query");
    bundle->addTemplate("first"); bundle->addTemplate("second");
    check(call("ObjectTemplate", "getTemplate") == "second", "child query follows addTemplate selection");
    check(call("ObjectTemplate", "getTemplate", {"0"}) == "first", "explicit child index");
    check(call("ObjectTemplate", "getTemplate") == "second", "explicit query does not change selection");
    bundle->setActiveTemplate(0);
    check(call("ObjectTemplate", "getTemplate") == "first", "child query follows setActiveTemplate selection");
    check(call("ObjectTemplate", "getTemplate", {"-1"}).empty()
          && call("ObjectTemplate", "getTemplate", {"2"}).empty(), "invalid child indices return empty text");
    objectTemplateManager->setActiveObjectTemplate(simple);
    check(!mainConsoleObjects->getConsoleObject("ObjectTemplate", "getTemplate"), "child query rejects simple template");
    auto* engine = new EngineTemplate;
    engine->setName("engine"); objectTemplateManager->setActiveObjectTemplate(engine);
    auto* heat = row("ObjectTemplate", "getHeatBarType");
    check(heat->getAccess() == 2 && heat->getCategory() == 1 && !heat->isDynamic()
          && heat->getMinNoArgs() == 0 && heat->getMaxNoArgs() == 0
          && std::string(heat->getReturnType()) == "int", "heat command metadata");
    check(call("ObjectTemplate", "getHeatBarType").empty() && !heat->hasReturnValue(), "retail heat command publishes no result");
    objectTemplateManager->setActiveObjectTemplate(simple);
    check(!mainConsoleObjects->getConsoleObject("ObjectTemplate", "getHeatBarType"), "heat command rejects non-engine template");

    auto* lightTemplate = new LightSourceTemplate;
    auto* light = new SkyLight(lightTemplate);
    objectManager->setActiveObject(light);
    auto* sky = row("Object", "setAsSkyLight");
    check(sky->getAccess() == 2 && sky->getCategory() == 0 && !sky->isDynamic()
          && sky->getMinNoArgs() == 0 && sky->getMaxNoArgs() == 0
          && std::string(sky->getReturnType()) == "void", "sky command metadata");
    check(call("Object", "setAsSkyLight").empty() && light->selections == 1, "sky command dispatches typed light virtual");
    check(!mainConsoleObjects->getConsoleObject("Object", "addAbsolutePoint"), "area command rejects light template");
    auto* areaTemplate = new AreaObjectTemplate;
    areaTemplate->setTriggerRadius(2);
    auto* area = new AreaObject(areaTemplate);
    area->setAbsolutePosition(Vec3{4, 7, -9});
    objectManager->setActiveObject(area);
    check(!mainConsoleObjects->getConsoleObject("Object", "setAsSkyLight"), "sky command rejects non-light runtime class");
    auto* areaCommand = row("Object", "addAbsolutePoint");
    check(areaCommand->getAccess() == 2 && areaCommand->getCategory() == 0 && !areaCommand->isDynamic()
          && areaCommand->getMinNoArgs() == 0 && areaCommand->getMaxNoArgs() == 0
          && std::string(areaCommand->getReturnType()) == "void", "area command metadata");
    call("Object", "addAbsolutePoint");
    check(areaTemplate->m_linePoints.size() == 1 && areaTemplate->m_linePoints[0].x == -4
          && areaTemplate->m_linePoints[0].y == 9, "area command appends negative absolute x/z");
    auto* arrowTemplate = new ArrowTemplate;
    arrowTemplate->area = area; templates->directionArrow = arrowTemplate;
    call("Object", "addAbsolutePoint");
    check(areaTemplate->m_linePoints.size() == 2 && arrowCalls == std::vector<int>({0, 1, 2}),
          "area marker creation, initialization and position order");
    check(arrowTemplate->arrow->position.x == 6 && arrowTemplate->arrow->position.y == 11
          && arrowTemplate->arrow->position.z == 21, "area marker resamples position after init");
    objectManager->setActiveObject(0);
    check(!mainConsoleObjects->getConsoleObject("HeightmapCluster", "compileWaterMeshesIntoHeightmap"), "unfinished water bake stays unpublished");

    std::remove("files/script.con"); std::remove("files/source.txt");
    std::printf("console bindings: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
