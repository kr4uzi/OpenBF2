// [TEMP] [CHOSEN, not recovered] Actual AirDraftEffectBundle bodies with typed observers.
// The world, base lifecycle and effect/sound sinks are fixtures. Unused engine calls abort
// through link_stubs.py. No game, device, content, window or network is initialized.
#include "../BF2/Game/Objects/Fx/AirDraftEffectBundle.h"
#include "../BF2/Game/Objects/Fx/AirDraftEffectBundleTemplate.h"
#include "../BF2/Game/Objects/Fx/EffectBundle.h"
#include "../BF2/Game/Objects/Fx/EffectBundleTemplate.h"
#include "../BF2/Game/Objects/Game/EngineBase.h"
#include "../BF2/Game/Objects/ObjectLayoutAccess.h"
#include "../BF2/Game/Objects/IObjectTemplateComponent.h"
#include "../BF2/Game/GameLogic/GameLogicCollision.h"
#include "../BF2/Physics/HeightmapCluster.h"
#include "../BF2/Physics/MaterialManager.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/ObjectPredicator.h"
#include "../BF2/Sound/NullSound.h"
#include "../BF2/Sound/ObjectSoundNode.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

using namespace dice::hfe;
using namespace dice::hfe::world;
static unsigned checks;
static void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
}
static std::vector<std::string> calls;
static unsigned frame = 100;
static bool dedicated, objectHit, terrainHit;
static float groundY;
static int objectMaterial = 7;
static unsigned targetMaterial;
static IObject* hitOwner;
static Vec3 lastRay;
static float properties[5];

namespace dice { namespace hfe {
class IBfSound;
IBfSound* g_bfSound = 0;
bool isDedicated() { return dedicated; }
namespace world {
IBase* materialManager = 0;
IBase* heightmapCluster = 0;
ObjectManager* objectManager = 0;
IObject::IObject() : m_flags(0), m_template(0), m_parent(0), m_physicsNode(0) {}
IObject::~IObject() {}
Object::Object(const IObjectTemplate* t) { m_template = t; m_absoluteTransformation = Mat4::getIdentity(); }
Object::~Object() {}
SimpleObject::SimpleObject(const SimpleObjectTemplate* t) : Object(t) {}
SimpleObject::~SimpleObject() {}
void SimpleObject::init() { calls.push_back("base-init"); }
void SimpleObject::disableCachedObject() { calls.push_back("base-disable"); }
void SimpleObject::destroy(bool force) { calls.push_back(force ? "base-destroy-1" : "base-destroy-0"); }
void Object::updateFlags(unsigned int set, unsigned int clear)
{ m_flags = (m_flags | set) & ~clear; }
const Vec3& Object::getAbsolutePosition() const
{ return *reinterpret_cast<const Vec3*>(&m_absoluteTransformation.m[12]); }
const Mat4& Object::getAbsoluteTransformation() const { return m_absoluteTransformation; }
void Object::setAbsolutePosition(const Vec3& p)
{ m_absoluteTransformation.m[12] = p.x; m_absoluteTransformation.m[13] = p.y; m_absoluteTransformation.m[14] = p.z; }
void Object::setAbsoluteTransformation(const Mat4& m) { m_absoluteTransformation = m; }
IObjectTemplate::IObjectTemplate() {}
IObjectTemplate::~IObjectTemplate() {}
ObjectTemplate::ObjectTemplate() {}
ObjectTemplate::~ObjectTemplate() {}
SimpleObjectTemplate::SimpleObjectTemplate() {}
SimpleObjectTemplate::~SimpleObjectTemplate() {}
Bundle::Bundle(const BundleTemplate* t) : SimpleObject(t) {}
Bundle::~Bundle() {}
EffectBundle::EffectBundle(const EffectBundleTemplate* t) : Bundle(t) {}
EffectBundle::~EffectBundle() {}
void EffectBundle::setAlphaIntensityForAllEffects(float v) { properties[0] = v; }
void EffectBundle::setEmitScaleFactorForAllEffects(float v) { properties[1] = v; }
void EffectBundle::setEmitSpeedScaleFactorForAllEffects(float v) { properties[2] = v; }
void EffectBundle::setAgeScaleFactorForAllEffects(float v) { properties[3] = v; }
void EffectBundle::setStartAgeForAllEffects(float v) { properties[4] = v; }
void EffectBundle::setCollisionPlane(const Plane3&) { calls.push_back("plane"); }
IObject* getRootParent(const IObject* o)
{ IObject* p = const_cast<IObject*>(o); while (p && p->getParent()) p = p->getParent(); return p; }
unsigned int getCurrentFrame() { return frame; }
MaterialManager::MaterialManager() {}
MaterialManager::~MaterialManager() {}
void MaterialManager::addRef() { std::abort(); }
void* MaterialManager::getEffectTemplate(unsigned int a, unsigned int b, int index)
{ check(a == 62 && index == 0, "effect material cell"); targetMaterial = b; return 0; }
IObjectTemplate* MaterialManager::getSoundTemplate(unsigned int a, unsigned int b, int index)
{ check(a == 62 && index == 0, "sound material cell"); targetMaterial = b; return 0; }
ObjectManager::ObjectManager() {}
ObjectManager::~ObjectManager() {}
void ObjectManager::addRef() { std::abort(); }
bool ObjectFlagPredicator::predicate(IObject*) { std::abort(); }
bool ObjectPredicator::predicateSurface(int) { std::abort(); }
bool ObjectManager::intersectLine(IObject*& object, Vec3& point, Vec3& normal, int& material,
    const Vec3&, const Vec3& direction, ObjectPredicator* pred, bool a, bool b, bool c, bool d, unsigned u)
{
    calls.push_back("objects"); lastRay = direction;
    check(pred->getFlags() == 0x100 && !a && !b && !c && d && !u, "object ray flags");
    object = objectHit ? hitOwner : 0; material = objectMaterial;
    point = {0, 2, 0}; normal = {0, 1, 0}; return objectHit;
}
HeightmapCluster::HeightmapCluster() {}
HeightmapCluster::~HeightmapCluster() {}
bool HeightmapCluster::intersectRayInWorldCoords(const Vec3& from, const Vec3& to, Vec3& point,
    float& fraction, Vec3& normal, unsigned char& material, bool twoSided) const
{
    calls.push_back("terrain");
    check(twoSided && to.y == from.y + lastRay.y, "terrain endpoint and sidedness");
    material = 9; fraction = 0.5f;
    if (terrainHit) { point = {0, groundY, 0}; normal = {0, 1, 0}; }
    return terrainHit;
}
const Mat4& GameLogicCollision::getCollisionEffectTransformation(const Vec3& p, const Vec3&, const Vec3&)
{ static Mat4 m; m = Mat4::getIdentity(); m.m[13] = p.y; calls.push_back("align"); return m; }
} } }

struct Template final : AirDraftEffectBundleTemplate
{
    int getMaterial() const override { return 62; }
};
struct Effect final : EffectBundle
{
    Effect() : EffectBundle(0) {}
    void handleMessage(TemplateMessage m, IPlayer*, bool recursive) override
    { calls.push_back(m == 0 ? "start" : (recursive ? "stop-recursive" : "stop")); }
    void destroy(bool force) override { check(force, "cached effects force destroyed"); calls.push_back("effect-destroy"); }
    bool hasZeroSpeed() const { return m_hasSpeed && m_speed.x == 0 && m_speed.y == 0 && m_speed.z == 0; }
};
struct Sound final : NullSound
{
    int release() override { calls.push_back("release"); return 1; }
    void stop() override { calls.push_back("sound-stop"); }
    void play() override { calls.push_back("sound-play"); }
};
struct Draft final : AirDraftEffectBundle
{
    explicit Draft(const Template* t) : AirDraftEffectBundle(t) {}
    void parent(IObject* p) { m_parent = p; }
    void cache(int material, Effect* effect) { m_effects[material] = effect; }
    void sound(ISound* p) { m_sound = p; }
    void current(int material, Effect* effect)
    { m_material = material; m_currentEffect = effect; targetMaterial = material; }
    void playing(bool b) { m_playing = b; }
    bool playing() const { return m_playing; }
    float intensity() const { return m_soundIntensity; }
    bool empty() const { return m_effects.empty() && !m_currentEffect && !m_sound; }
    unsigned expiry() const { return m_expiryFrame; }
    bool initial() const
    { return !m_engine && m_needsEngine && m_material == -1 && !m_playing && !m_sound && !m_expiryFrame
        && m_effectPosition.y == 0 && m_effectTransformation.m[0] == 1 && m_effectTransformation.m[15] == 1; }
};

int main()
{
    MaterialManager materials; materialManager = &materials;
    ObjectManager objects; objectManager = &objects;
    HeightmapCluster terrain; heightmapCluster = static_cast<IHeightmapCluster*>(&terrain);
    Template t; t.m_field2C8 = 10; t.m_field2CC = 1;
    Effect effect, second;
    Sound sound;
    Draft root(&t);
    Draft draft(&t);
    check(draft.initial(), "constructor defaults");
    dedicated = true; check(t.createObject() == 0, "runtime dedicated factory remains null");
    dedicated = false; delete t.createObject();
    calls.clear(); draft.init(); draft.initNetworkableComp();
    check(calls == std::vector<std::string>{"base-init"}, "init forwards; client networkable slot is empty");
    draft.handleMessage(static_cast<TemplateMessage>(4), 0, true);
    check(!(draft.getFlags() & 0x1000), "recursive enter ignored");
    draft.handleMessage(static_cast<TemplateMessage>(4), 0, false);
    check(draft.getFlags() & 0x1000, "enter enables visual updates");
    draft.parent(&root); draft.setAbsolutePosition(Vec3{0, 5, 0}); hitOwner = &root;
    objectHit = terrainHit = true; groundY = 0;
    draft.cache(7, &effect); draft.current(7, &effect); draft.sound(&sound);
    calls.clear(); draft.handleVisualUpdate(0, 0);
    check(calls == std::vector<std::string>{"objects", "terrain", "plane", "start", "sound-play"},
        "object and terrain order, collision plane before start and sound");
    check(effect.getAbsolutePosition().y == 0 && lastRay.y == -15, "terrain overwrites object point; startDistance plus five ray");
    check(draft.playing() && draft.expiry() == 130 && (draft.getFlags() & 0x800), "visual update arms expiry");
    frame = 130; calls.clear(); draft.handleUpdate(0, 0); check(calls.empty(), "inclusive expiry frame");
    frame = 131; draft.handleUpdate(0, 0);
    check(calls == std::vector<std::string>{"stop", "sound-stop"} && !draft.playing() && !(draft.getFlags() & 0x800), "expiry stops both sinks");
    terrainHit = false; calls.clear(); draft.calculateCurrentDistanceAndEffectPosition();
    check(effect.getAbsolutePosition().y == 2, "object point survives terrain miss");
    objectHit = false; terrainHit = true; draft.cache(9, &second);
    draft.calculateCurrentDistanceAndEffectPosition();
    check(targetMaterial == 9, "terrain-only material selects cache");
    t.m_byte2D0 = 1; calls.clear(); draft.calculateCurrentDistanceAndEffectPosition();
    check(calls == std::vector<std::string>{"objects", "terrain", "align", "plane"}, "optional terrain-aligned frame");
    t.m_byte2D0 = 0; groundY = -5; draft.calculateCurrentDistanceAndEffectPosition();
    check(draft.playing(), "startDistance boundary accepted");
    groundY = -5.01f; draft.calculateCurrentDistanceAndEffectPosition(); check(!draft.playing(), "beyond startDistance stops");
    draft.playing(true); groundY = std::numeric_limits<float>::quiet_NaN();
    draft.calculateCurrentDistanceAndEffectPosition(); check(!draft.playing(), "unordered distance stops");
    draft.current(9, &second);
    t.m_property[0][EA_Altitude] = Vec2{10, 0}; t.m_property[0][EA_Engine] = Vec2{0, 1};
    t.m_range[0] = Vec2{0.5f, 1};
    t.m_property[5][EA_Engine] = Vec2{0, 1}; t.m_range[5] = Vec2{0, 2};
    draft.updateEffectProperties(5, 0.5f);
    check(properties[0] == 0.625f && draft.intensity() == 1 && second.hasZeroSpeed(), "altitude/engine factors, sound clamp and zero speed publication");
    t.m_range[5] = Vec2{std::numeric_limits<float>::quiet_NaN(), 1}; draft.updateEffectProperties(5, 1);
    check(draft.intensity() == 0, "unordered sound intensity clamps to zero");
    calls.clear(); draft.setCurrentMaterial(7, true);
    check(calls[0] == "stop-recursive", "material switch forwards recursive flag");
    calls.clear(); draft.disableCachedObject();
    check(calls == std::vector<std::string>{"stop", "sound-stop", "release", "effect-destroy", "effect-destroy", "base-disable"}
        && draft.empty(), "cache disable ownership/order");
    draft.cache(7, &effect); draft.current(7, &effect); draft.sound(&sound); draft.playing(true);
    calls.clear(); draft.destroy(false);
    check(calls == std::vector<std::string>{"release", "effect-destroy", "base-destroy-0"} && draft.empty() && draft.playing(),
        "destroy clears cache, preserves latch and forwards force");
    calls.clear(); draft.handleMessage(static_cast<TemplateMessage>(5), 0, false);
    check(!(draft.getFlags() & 0x1000) && !draft.playing(), "exit clears visual flag and playing latch");
    calls.clear();
    {
        Draft retiring(&t); retiring.cache(7, &effect); retiring.current(7, &effect); retiring.sound(&sound);
    }
    check(calls == std::vector<std::string>{"release"}, "destructor releases sound without destroying cached values");
    std::printf("air-draft: %u checks passed (typed fixture, no game runtime)\n", checks);
}
