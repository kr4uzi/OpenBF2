// [TEMP] [CHOSEN, not recovered] Constructor/teardown and fatal-callback fixtures.
// Real ZiplineContainer, DestroyableWindowsBundle and BF2Log bodies; base object
// initialization and external services model only the dependencies checked here.
// Unused engine dependencies abort through tools/macos/link_stubs.py. No game,
// renderer, socket, content or sound device is initialized.
#include <array>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#define private public
#define protected public
#include "../BF2/Game/Objects/Game/ZiplineContainer.h"
#include "../BF2/Game/Objects/Game/Zipline.h"
#include "../BF2/Game/Objects/Game/DestroyableWindowsBundle.h"
#include "../BF2/Game/Objects/IPlayer.h"
#include "../BF2/Game/Objects/ICameraObject.h"
#include "../BF2/Game/Objects/TemplateLeaves.h"
#include "../BF2/Game/Objects/IObjectTemplateComponent.h"
#undef private
#undef protected
#include "../BF2/Game/Main/BF2Log.h"
#include "../BF2/Game/Utility/Vars.h"
#include "../BF2/Game/Utility/BuildNrUtil.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/Sound/BfSound.h"
#include "../BF2/Geom/GeometryTemplateManager.h"
#include "../BF2/Geom/IGeometryTemplate.h"
#include "../BF2/Geom/NativeGeometrySlots.h"
#include "../BF2/Core/Ids.h"

using namespace dice::hfe;
using namespace dice::hfe::world;
using namespace dice::hfe::geom;
static int checks;
static void check(bool ok, const char* name)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", name); std::exit(1); }
}
[[noreturn]] static void unexpected(const char* name)
{
    std::fprintf(stderr, "[TEMP] unexpected dependency: %s\n", name);
    std::abort();
}
#include "client_effects_fakes.h"

static void* primaryGeometry;
static void* secondaryGeometry;
static std::vector<std::string> calls;
static int geometryIndex;
static void* owner;
static void* nextGeometry;
static int geometryReleases;
static int soundReleases;
static ICameraObject* activeCamera;
static bool entryResult;

// Named base-class fixture bodies, intentionally independent of production setup.
namespace dice { namespace hfe {
void setDebugCallback(DebugType, DebugCallback, void*) {}
const char* BuildNrUtil::getBuildNrString() { return "fixture"; }
int* Vars::getInt(const char*, int) { static int value = 0; return &value; }
float* Vars::getFloat(const char*, float) { static float value = 1.49f; return &value; }
SprintState::SprintState() {}
namespace io {
OldConsole::OldConsole() {}
OldConsole::~OldConsole() {}
OldConsole::Level::Level() {}
OldConsole* mainConsole = 0;
}
namespace world {
IObject::IObject() { m_parent = this; m_child = 0; m_geometry = primaryGeometry; m_physicsNode = 0; }
IObject::~IObject() {}
IPlayer::IPlayer() {}
IPlayer::~IPlayer() {}
Object::Object(const IObjectTemplate* value) { m_template = value; }
Object::~Object() {}
const Vec3& Object::getAbsolutePosition() const { static Vec3 position = {}; return position; }
const Mat4& Object::getAbsoluteTransformation() const { return m_absoluteTransformation; }
IObject* Object::getChild() const { return m_child; }
void Object::setRelativePosition(const Vec3&) {}
void Object::setAbsolutePosition(const Vec3&) {}
SimpleObject::SimpleObject(const SimpleObjectTemplate* value) : Object(value) {}
SimpleObject::~SimpleObject() {}
Bundle::Bundle(const BundleTemplate* value) : SimpleObject(value) {}
Bundle::~Bundle() {}
PlayerControlObject::PlayerControlObject(const PlayerControlObjectTemplate* value) : Bundle(value)
{ m_occupyingObject = 0; }
PlayerControlObject::~PlayerControlObject() {}
void* PlayerControlObject::queryInterface(unsigned int iid) const
{
    check(iid == IID_IPlayerControlObject, "vehicle interface lookup");
    return static_cast<IPlayerControlObject*>(const_cast<PlayerControlObject*>(this));
}
ICameraObject* PlayerControlObject::getActiveCamera() { return activeCamera; }
void PlayerControlObject::onEnter(IObject*) {}
bool PlayerControlObject::enter(IPlayer*) { return entryResult; }
Zipline::Zipline(const ZiplineTemplate* value) : SimpleObject(value) {}
Zipline::~Zipline() {}
float Zipline::getZiplineHeight() { return 0; }
IObjectTemplate::IObjectTemplate() {}
IObjectTemplate::~IObjectTemplate() {}
ObjectTemplate::ObjectTemplate() {}
ObjectTemplate::~ObjectTemplate() {}
SimpleObjectTemplate::SimpleObjectTemplate() {}
SimpleObjectTemplate::~SimpleObjectTemplate() {}
BundleTemplate::BundleTemplate() {}
BundleTemplate::~BundleTemplate() {}
DestroyableWindowsBundleTemplate::DestroyableWindowsBundleTemplate() {}
DestroyableWindowsBundleTemplate::~DestroyableWindowsBundleTemplate() {}
} } }

struct TestSound : NullSound {
    ISoundNode* parent = 0;
    const float* volume = 0;
    const float* pitch = 0;
    bool playing = false;
    void play() override { calls.push_back("play"); playing = true; }
    int release() override { ++soundReleases; return 1; }
    void setParent(ISoundNode* value) override { parent = value; calls.push_back("parent"); }
    ISoundNode* getParent() const override { return parent; }
    void setVolumeEvaluator(ISoundEvaluator* evaluator, const float* value) override
    { check(!evaluator, "template volume evaluator"); volume = value; calls.push_back("volume"); }
    void setPitchEvaluator(ISoundEvaluator* evaluator, const float* value) override
    { check(!evaluator, "template pitch evaluator"); pitch = value; calls.push_back("pitch"); }
    bool isPlaying() override { calls.push_back("playing?"); return playing; }
    void forceStop() override { playing = false; calls.push_back("forceStop"); }
};
struct TestSoundOwner : BfSoundNull {
    ISound* sound = 0;
    ISound* getSound(const std::string& name) const override
    { check(name == "S_ZiplineUsed", "ride sound lookup"); return sound; }
};
struct TestTemplate : UnusedTemplate {
    void* instance = 0;
    void* createInstance() override { calls.push_back("create"); return instance; }
};
struct TestManager : UnusedManager {
    IGeometryTemplate* result = 0;
    IGeometryTemplate* getTemplate(const std::string& name) override
    { check(name == "window_mesh", "authored geometry lookup"); calls.push_back("lookup"); return result; }
};
struct TestPlayer : UnusedPlayer {
    IObject* vehicle;
    IObject* getVehicle() const override { return vehicle; }
};
struct TestCamera : UnusedCamera {
    void* queryInterface(unsigned int iid) const override
    { check(iid == IID_IObject, "camera object lookup"); return 0; }
    CameraViewMode getViewMode() const override { return static_cast<CameraViewMode>(0); }
};
struct TestConsole : io::OldConsole {
    const std::string& getWorkingFile() const override { static std::string path("fixture.con"); return path; }
    void output(const std::string&) override {}
};
namespace dice { namespace hfe {
IBase* g_geometryTemplateManager = 0;
IBfSound* g_bfSound = 0;
} }

// The renderer publishes explicit tables. Only the attested operations used by
// these constructors/destructors are supplied, with native pointer-sized entries.
static int geomCount(void* receiver)
{ check(receiver == secondaryGeometry, "query secondary instance"); calls.push_back("count"); return 3; }
static void* geomNext(void* receiver)
{ check(receiver == primaryGeometry, "read primary chain"); return nextGeometry; }
static void* geomSetNext(void* receiver, void* value)
{ check(receiver == primaryGeometry, "attach/clear primary chain"); calls.push_back(value ? "attach" : "clear"); return nextGeometry = value; }
static void* geomOwner(void* receiver, void* value)
{ check(receiver == secondaryGeometry, "set secondary owner"); calls.push_back("owner"); return owner = value; }
static void geomIndex(void* receiver, int value)
{ check(receiver == secondaryGeometry, "select secondary geometry"); calls.push_back("index"); geometryIndex = value; }
static int geomRelease(void* receiver)
{ check(receiver == secondaryGeometry, "release secondary instance"); calls.push_back("release"); return ++geometryReleases; }

static void logging()
{
    TestConsole console;
    io::mainConsole = &console;
    BF2Log log;
    check(log.getExitInsteadOfErrorWindow() == !DEDICATED, "image-specific fatal default");
    check(!log.getDoNotDisplayErrorWindow(), "normal execution does not suppress the error window");
    for (DebugType type : {DebugTypeAssert, DebugTypeError, DebugTypeDxAssert})
    for (bool suppressed : {false, true})
    {
        const pid_t child = fork();
        check(child >= 0, "fork fatal callback");
        if (!child)
        {
            log.setDoNotDisplayErrorWindow(suppressed);
            BF2Log::debugCallback(type, "fixture", "", 1, "", "fixture", "fixture", &log);
            _exit(23);
        }
        int status = 0;
        check(waitpid(child, &status, 0) == child && WIFEXITED(status), "fatal child exited normally");
        check(WEXITSTATUS(status) == (suppressed ? (DEDICATED ? 23 : 0) : 1), "fatal callback exit status");
    }
    log.setDoNotDisplayErrorWindow(true);
    for (DebugType type : {DebugTypeInfo, DebugTypeWarning, DebugTypeLog})
        BF2Log::debugCallback(type, "fixture", "", 1, "", "fixture", "fixture", &log);
    check(true, "nonfatal diagnostics continue with error windows suppressed");
    log.setExitInsteadOfErrorWindow(true);
    check(!log.getExitInsteadOfErrorWindow(), "explicit setter still clears the exit flag");
    io::mainConsole = 0;
}

int main()
{
    logging();
    TestSound sound;
    TestSoundOwner soundOwner;
    g_bfSound = &soundOwner;
    for (int state = 0; state != 3; ++state)
    {
        calls.clear();
        soundOwner.sound = state ? &sound : 0;
        sound.playing = state == 2;
        ZiplineContainer* zip = new ZiplineContainer(0);
#if !DEDICATED
        if (state)
        {
            check(calls == std::vector<std::string>({"parent", "volume", "pitch"}), "sound setup order");
            check(static_cast<ObjectSoundNode*>(sound.parent)->m_object == zip, "sound follows the container");
            check(sound.volume == &zip->m_slopeSpeed && sound.pitch == sound.volume, "live slide-speed bindings");
            zip->m_slopeSpeed = 4.0f;
            check(*sound.volume == 4.0f && *sound.pitch == 4.0f, "speed changes reach both evaluators");
        }
#endif
        calls.clear();
        delete zip;
        std::vector<std::string> expected;
        if (!DEDICATED && state) expected.push_back("playing?");
        if (!DEDICATED && state == 2) expected.push_back("forceStop");
        if (!DEDICATED && state)
        {
            expected.insert(expected.end(), {"parent", "volume", "pitch"});
            check(!sound.parent && !sound.volume && !sound.pitch, "destroyed parent leaves no sound bindings");
        }
        check(calls == expected, "absent/inactive/active sound teardown");
        check(soundReleases == 0, "borrowed sound not released by container");
    }
    TestPlayer player;
    TestCamera camera;
    activeCamera = &camera;
    Zipline line(0);
    for (bool available : {false, true})
    {
        soundOwner.sound = available ? &sound : 0;
        ZiplineContainer zip(0);
        zip.m_zipline = &line;
        player.vehicle = &zip;
        for (bool success : {false, true})
        {
            entryResult = success;
            calls.clear();
            check(zip._enter(&player) == success, "base entry result preserved");
            check(calls == ((!DEDICATED && available && success)
                  ? std::vector<std::string>({"parent", "volume", "pitch", "play"}) : std::vector<std::string>()),
                  "only successful entry with a sound starts playback");
        }
    }

#if !DEDICATED
    // BfSound lends one instance per name. Destroy the last constructor's parent,
    // then ride the surviving container through its real _enter body.
    soundOwner.sound = &sound;
    ZiplineContainer* survivor = new ZiplineContainer(0);
    survivor->m_zipline = &line;
    ZiplineContainer* lastParent = new ZiplineContainer(0);
    check(survivor->m_usedSound == lastParent->m_usedSound, "containers borrow the same ride sound");
    check(sound.parent == &lastParent->m_soundNode && sound.volume == &lastParent->m_slopeSpeed,
          "last constructor owns the shared bindings");
    delete lastParent;
    check(!sound.parent && !sound.volume && !sound.pitch, "shared sound detached after parent destruction");
    player.vehicle = survivor;
    entryResult = true;
    calls.clear();
    check(survivor->_enter(&player), "surviving container can be ridden");
    check(calls == std::vector<std::string>({"parent", "volume", "pitch", "play"}),
          "survivor binds shared sound before playback");
    survivor->m_slopeSpeed = 7.0f;
    check(sound.playing && sound.parent == &survivor->m_soundNode &&
          sound.volume == &survivor->m_slopeSpeed && sound.pitch == sound.volume &&
          *sound.volume == 7.0f && *sound.pitch == 7.0f,
          "next ride uses the surviving node and speed");

    // Another constructor takes the bindings; a failed entry must not steal them.
    ZiplineContainer* other = new ZiplineContainer(0);
    entryResult = false;
    calls.clear();
    check(!survivor->_enter(&player) && calls.empty(), "failed entry leaves shared sound untouched");
    check(sound.parent == &other->m_soundNode && sound.volume == &other->m_slopeSpeed,
          "failed entry preserves the other container's bindings");
    entryResult = true;
    check(survivor->_enter(&player), "successful entry reclaims bindings from another live container");
    calls.clear();
    delete other;
    check(calls == std::vector<std::string>({"playing?", "forceStop"}),
          "nonparent destruction preserves retail stop conditions without detaching");
    check(sound.parent == &survivor->m_soundNode && sound.volume == &survivor->m_slopeSpeed &&
          sound.pitch == sound.volume, "nonparent destruction preserves live bindings");
    check(survivor->_enter(&player) && sound.playing, "ride restarts after nonparent destruction");
    delete survivor;
    check(!sound.parent && !sound.volume && !sound.pitch, "final parent clears all shared bindings");
    check(soundReleases == 0, "shared sound remains owned by BfSound");
#endif

    TestManager manager;
    TestTemplate mesh;
    std::array<void*, 60> table = {};
    table[2] = reinterpret_cast<void*>(&geomRelease);
    table[native_geometry::GetSubgeomCount] = reinterpret_cast<void*>(&geomCount);
    table[native_geometry::GetNextGeometry] = reinterpret_cast<void*>(&geomNext);
    table[native_geometry::SetNextGeometry] = reinterpret_cast<void*>(&geomSetNext);
    table[native_geometry::SetOwnerObject] = reinterpret_cast<void*>(&geomOwner);
    table[native_geometry::SetSubgeom] = reinterpret_cast<void*>(&geomIndex);
    void** geometry = table.data();
    void** instanceGeometry = table.data();
    secondaryGeometry = &instanceGeometry;
    DestroyableWindowsBundleTemplate authored;
    authored.clientHitDetection = false; // The PE +0x184 gate is string length, not this ELF-offset flag.
    for (int state = 0; state != 6; ++state)
    {
        primaryGeometry = state == 0 ? 0 : &geometry;
        authored.m_geometry = state == 1 ? "" : "window_mesh";
        g_geometryTemplateManager = state == 2 ? 0 : &manager;
        manager.result = state == 3 ? 0 : &mesh;
        mesh.instance = state == 4 ? 0 : secondaryGeometry;
        nextGeometry = 0;
        geometryReleases = 0;
        calls.clear();
        DestroyableWindowsBundle* windows = new DestroyableWindowsBundle(&authored);
        if (DEDICATED || state < 3) check(calls.empty(), "geometry prerequisites skip lookup");
        else if (state == 3) check(calls == std::vector<std::string>({"lookup"}), "missing template skips create");
        else if (state == 4) check(calls == std::vector<std::string>({"lookup", "create"}), "missing instance skips attach");
        else
        {
            check(calls == std::vector<std::string>({"lookup", "create", "count", "attach", "owner", "index"}), "secondary geometry call order");
            check(nextGeometry == secondaryGeometry && owner == windows && geometryIndex == 2, "broken-pane geometry selection");
        }
        calls.clear();
        delete windows;
        if (!DEDICATED && state == 5)
            check(calls == std::vector<std::string>({"release", "clear"}) && geometryReleases == 1 && !nextGeometry,
                  "secondary geometry released and detached once");
        else check(calls.empty(), "absent secondary geometry not released");
    }
    std::printf("client effects DEDICATED=%d: %d checks passed\n", DEDICATED, checks);
}
