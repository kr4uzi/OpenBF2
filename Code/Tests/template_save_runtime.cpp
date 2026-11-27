// [TEMP] [CHOSEN, not recovered] Content-free fixture using real engine templates and
// typed descriptor inputs. No engine startup, renderer/device initialization, files or sockets.
// Test-owned services/templates remain alive until process exit.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/ConsoleObjectMacro.h"
#include "../BF2/Core/Debug.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Game/Objects/CameraTemplate.h"
#include "../BF2/Game/Objects/FreeCameraTemplate.h"
#include "../BF2/Game/Objects/GenericFireArmTemplate.h"
#include "../BF2/Game/Objects/Game/SoldierTemplate.h"
#include "../BF2/Game/Objects/PlayerControlObjectTemplate.h"
#include "../BF2/Game/Objects/Fx/ParticleSystemTemplates.h"
#include "../BF2/Animation/BoneAnimation/AnimationSystem.h"
#include "../BF2/Animation/BoneAnimation/Skeleton.h"
#include "../BF2/Geom/ParticleSystem/ParticleSystemManager.h"
#include <template_save_descriptors.h>
#include "../BF2/IO/Stream/IStream.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

namespace dice { namespace hfe { extern ClassManager* classManager; } }
using namespace dice::hfe;
using namespace dice::hfe::world;

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    std::fflush(stdout);
}
struct Sink : io::IStream
{
    std::string text;
    unsigned writes = 0, failAt = 0;
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned) const override { return 0; }
    unsigned read(void*, unsigned) override { std::abort(); }
    unsigned write(const void* data, unsigned size) override
    {
        ++writes;
        const unsigned count = writes == failAt && size ? size - 1 : size;
        text.append(static_cast<const char*>(data), count);
        return count;
    }
    int skip(unsigned) override { std::abort(); }
    int getAvailableBytes() override { return 0; }
    io::IStream* clone() override { return 0; }
    bool canRead() override { return false; }
    bool canWrite() override { return true; }
};
void exact(io::IConsoleSaveable& object, const std::string& expected, const char* label)
{
    Sink out;
    check(object.makeScript(&out) && out.text == expected, label);
    if (out.text != expected)
        std::fprintf(stderr, "Expected:\n%sActual:\n%s", expected.c_str(), out.text.c_str());
    Sink shortFirst; shortFirst.failAt = 1;
    check(!object.makeScript(&shortFirst) && shortFirst.writes == 1, "base/first short write stops save");
    Sink shortLast; shortLast.failAt = out.writes;
    check(!object.makeScript(&shortLast), "class-specific short write is rejected");
}
struct Camera : CameraTemplate
{
    void configure()
    {
        m_hasTarget = m_toggleMouseLook = true; m_cameraId = -7;
        m_CVMInside = m_CVMChase = m_CVMFrontChase = m_CVMFlyBy = false;
        m_CVMTrace = m_CVMExternTrace = m_CVMNose = true;
        m_cameraShakeSensitivity = 0.25f; m_cameraShakeStartSpeed = 2; m_cameraShakeMaxSpeed = 30;
        m_bounceLength = 1; m_bounceSensitivity = 2; m_bounceStrength = 3; m_bounceDamping = 4;
        m_nosePos = Vec3{0.0001f, 0, -2}; m_followStiffness = 0.5f;
        m_chaseDistance = 10; m_chaseAngle = 0.25f; m_chaseOffset = Vec3{1, 2, 3};
        m_worldFOV = 1.2345678f; m_insideFOV = 0.75f;
        m_cockpitLod = 2; m_cockpitSubGeom = 3;
    }
    void omitGroups() { m_cameraShakeStartSpeed = m_bounceLength = 0; m_nosePos = Vec3{0,0,0}; }
    void nose(Vec3 value) { m_nosePos = value; }
};
struct Animation : dice::anim::IAnimationSystemTemplate
{
    std::string path;
    mutable int calls = 0;
    explicit Animation(const char* p) : path(p) {}
    void addRef() override {}
    int release() override { return 1; }
    bool loadScript(const std::string&) override { std::abort(); }
    dice::anim::AnimationSystem* makeInstance() override { std::abort(); }
    const std::string& getPath() const override { ++calls; return path; }
    dice::anim::CameraSpring* getCameraSpring() const override { return 0; }
};
struct Animated : AnimatedBundleTemplate
{
    void attach(Animation* first, Animation* third)
    {
        m_skeleton = new dice::anim::Skeleton();
        m_animationSystem1P = first; m_animationSystem3P = third;
    }
};
template<class T> struct Particle : T
{
    void attach(geom::IParticleSystemTemplate* descriptor) { this->m_impl = descriptor; }
};
struct RootWriter : Particle<ParticleSystemEmitterTemplate>
{
    bool makeScript(io::IStream* out) override { return ParticleSystemTemplateBase::makeScript(out); }
};
struct BranchWriter : Particle<SpriteParticleSystemTemplate>
{
    bool makeScript(io::IStream* out) override { return SpriteParticleSystemTemplateBase::makeScript(out); }
};
struct Projectile : FreeCameraTemplate
{
    int saves = 0;
    bool makeScript(io::IStream* stream) override
    {
        ++saves;
        stream->write("projectile\n", 11);
        return false;
    }
};
std::string baseScript(ObjectTemplate& object)
{
    Sink out;
    check(object.ObjectTemplate::makeScript(&out), "real ObjectTemplate base save");
    return out.text;
}
std::string bundleScript(BundleTemplate& object)
{
    Sink out;
    check(object.BundleTemplate::makeScript(&out), "real BundleTemplate base save");
    return out.text;
}
const char* common =
    "ObjectTemplate.showInFirstPerson 1\nObjectTemplate.showInThirdPerson 1\n"
    "ObjectTemplate.emitPrio High\nObjectTemplate.lodDistance Medium\nObjectTemplate.qualityLevel Low\n"
    "ObjectTemplate.emitterType Standard\nObjectTemplate.isLocalSystem 1\nObjectTemplate.emitRadius 1/2/3\n"
    "ObjectTemplate.emitRangeX CRD_NONE/1/0/0\nObjectTemplate.emitRangeY CRD_NONE/1/0/0\n"
    "ObjectTemplate.emitRangeZ CRD_NONE/1/0/0\nObjectTemplate.randomRadialDirection 0.25\n"
    "ObjectTemplate.radialDirection 0.5\nObjectTemplate.emitFrequency 4\nObjectTemplate.preWarmTime 5\n"
    "ObjectTemplate.emitTime 6\nObjectTemplate.emitDelay 7\nObjectTemplate.isLooping 1\n"
    "ObjectTemplate.emitSpeed 8\nObjectTemplate.emitDirection 9/10/11\nObjectTemplate.timeToLive 12\n"
    "ObjectTemplate.randomTimeToLive 13\nObjectTemplate.windAffectionFactor 14\n"
    "ObjectTemplate.forceToWaterSurface 1\nObjectTemplate.waterSurfaceOffset 15\n"
    "ObjectTemplate.rotationGraph 1/2/3/4\nObjectTemplate.alignRotationToSpeed 1\n"
    "ObjectTemplate.transparencyGraph 1/2/3/4\nObjectTemplate.gravity 16\n"
    "ObjectTemplate.gravityGraph 1/2/3/4\nObjectTemplate.airResistanceGraph 1/2/3/4\n"
    "ObjectTemplate.emitSpeedGraph 1/2/3/4\nObjectTemplate.randomSpeed 17\n"
    "ObjectTemplate.randomDirectionAngle 18/19/20\nObjectTemplate.airResistance 21\n"
    "ObjectTemplate.collisionEnable 1\nObjectTemplate.collisionSpeedAlongNormal 22\n"
    "ObjectTemplate.collisionSpeedAlongPlane 23\n";
const char* spriteBase =
    "ObjectTemplate.randomSize 24\nObjectTemplate.particleMaxSize 25\nObjectTemplate.sizeGraph 1/2/3/4\n"
    "ObjectTemplate.randomIntensity 26\nObjectTemplate.lightMapIntensityOffset 27\n"
    "ObjectTemplate.lightAffectionFactor 0\nObjectTemplate.color1 28/29/30\n"
    "ObjectTemplate.color2 31/32/33\nObjectTemplate.colorBlendGraph 1/2/3/4\n"
    "ObjectTemplate.alphaCull 34\nObjectTemplate.isStaticSystem 1\n";
geom::IParticleSystemTemplate* descriptor(int type)
{
    geom::IParticleSystemTemplate* p = type == 0 ? static_cast<geom::IParticleSystemTemplate*>(new geom::SaveDescriptor0())
        : type == 1 ? static_cast<geom::IParticleSystemTemplate*>(new geom::SaveDescriptor1())
        : type == 2 ? static_cast<geom::IParticleSystemTemplate*>(new geom::SaveDescriptor2())
        : type == 3 ? static_cast<geom::IParticleSystemTemplate*>(new geom::SaveDescriptor3())
                    : static_cast<geom::IParticleSystemTemplate*>(new geom::SaveDescriptor4());
    p->setEmitPrio(2); p->setLodDistance(1); p->setQualityLevel(0); p->setIsLocalSystem(true);
    p->setEmitRadius(Vec3{1,2,3});
    RandomDistribution range;
    p->setEmitRangeX(range); p->setEmitRangeY(range); p->setEmitRangeZ(range);
    p->setRandomRadialDirection(0.25f); p->setRadialDirection(0.5f);
    p->setEmitFrequency(4); p->setPreWarmTime(5); p->setEmitTime(6); p->setEmitDelay(7);
    p->setIsLooping(true); p->setEmitSpeed(8); p->setEmitDirection(Vec3{9,10,11});
    p->setTimeToLive(12); p->setRandomTimeToLive(13); p->setWindAffectionFactor(14);
    p->setForceToWaterSurface(true); p->setWaterSurfaceOffset(15);
    const Vec4 graph{1,2,3,4};
    p->setRotationGraph(graph); p->setAlignRotationToSpeed(true); p->setTransparencyGraph(graph);
    p->setGravity(16); p->setGravityGraph(graph); p->setAirResistanceGraph(graph);
    p->setEmitSpeedGraph(graph); p->setRandomSpeed(17); p->setRandomDirectionAngle(Vec3{18,19,20});
    p->setAirResistance(21); p->setCollisionEnable(true);
    p->setCollisionSpeedAlongNormal(22); p->setCollisionSpeedAlongPlane(23);
    if (type != 3)
    {
        auto* s = static_cast<geom::IParticleSpriteSystemTemplate*>(p);
        s->setRandomSize(24); s->setParticleMaxSize(25); s->setSizeGraph(graph);
        s->setRandomIntensity(26); s->setLightMapIntensityOffset(27); s->setLightAffectionFactor(0);
        s->setColor1(Vec3{28,29,30}); s->setColor2(Vec3{31,32,33}); s->setColorBlendGraph(graph);
        s->setAlphaCull(34); s->setIsStaticSystem(true);
    }
    return p;
}
template<class T> std::string attach(Particle<T>& object, geom::IParticleSystemTemplate* p)
{
    object.attach(p); object.setName("fixture");
    return baseScript(object) + common;
}
template<class D> void spriteSettings(D* p)
{
    p->setRandomRotation(35); p->setRandomRotationSpeed(36); p->setRotationSpeed(37);
    p->setPivotOffset(Vec2{38,39}); p->setSelfShadowIntensity(40);
    p->setAnimationEnable(true); p->setAnimationSpeed(41); p->setAnimationPlayOnce(true);
    p->setAnimationRandomizedStartFrame(true); p->setAnimationFrameCount(16); p->setAnimationFrameCountX(4);
}
const char* spriteSettingsText =
    "ObjectTemplate.randomRotation 35\nObjectTemplate.randomRotationSpeed 36\nObjectTemplate.rotationSpeed 37\n"
    "ObjectTemplate.pivotOffset 38/39\nObjectTemplate.selfShadowIntensity 40\nObjectTemplate.textureName \n"
    "ObjectTemplate.animationEnable 1\nObjectTemplate.animationSpeed 41\nObjectTemplate.animationPlayOnce 1\n"
    "ObjectTemplate.setAnimationFrameWidthRelative 0\nObjectTemplate.setAnimationFrameHeightRelative 0\n"
    "ObjectTemplate.animationRandomizedStartFrame 1\nObjectTemplate.animationFrameCount 16\nObjectTemplate.animationFrameCountX 4\n";
}

int main()
{
    eventManager = new EventManager();
    classManager = new ClassManager();
    objectTemplateManager = new ObjectTemplateManager();
    dice::anim::animationSystemTemplateManager = new dice::anim::AnimationSystemTemplateManager();
    auto* camera = new Camera(); camera->setName("camera"); camera->configure();
    Sink cameraBase; camera->RotationalBundleTemplate::makeScript(&cameraBase);
    const std::string cameraTail =
        "ObjectTemplate.setHasTarget 1\nObjectTemplate.toggleMouseLook 1\nObjectTemplate.CameraId -7\n"
        "ObjectTemplate.CVMInside 0\nObjectTemplate.CVMChase 0\nObjectTemplate.CVMFrontChase 0\n"
        "ObjectTemplate.CVMFlyBy 0\nObjectTemplate.CVMTrace 1\nObjectTemplate.CVMExternTrace 1\nObjectTemplate.CVMNose 1\n"
        "ObjectTemplate.cameraShakeSensitivity 0.25\nObjectTemplate.cameraShakeStartSpeed 2\nObjectTemplate.cameraShakeMaxSpeed 30\n"
        "ObjectTemplate.bounceLength 1\nObjectTemplate.bounceSensitivity 2\nObjectTemplate.bounceStrength 3\nObjectTemplate.bounceDamping 4\n"
        "ObjectTemplate.nosePos 0.0001/0/-2\nObjectTemplate.followStiffness 0.5\nObjectTemplate.chaseDistance 10\n"
        "ObjectTemplate.chaseAngle 0.25\nObjectTemplate.chaseOffset 1/2/3\n"
        "ObjectTemplate.worldFOV 1.23457\nObjectTemplate.insideFOV 0.75\nObjectTemplate.worldFOV 1.23457\nObjectTemplate.insideFOV 0.75\n"
        "ObjectTemplate.allowInsideDynamicLights 1\nObjectTemplate.allowInsideStaticSun 0\n"
        "ObjectTemplate.insideStaticSunDirection -0.25/-0.5/0.75\nObjectTemplate.insideStaticSunColor 1/1/1\n"
        "ObjectTemplate.insideStaticSunDamageColor 1/0/0\nObjectTemplate.insideDamageFrequency 3\n"
        "ObjectTemplate.insideDamageThreshold 0.15\nObjectTemplate.insideStaticSunAmbientColor 0.7/0.7/0.7\n"
        "ObjectTemplate.cockpitLod 2\nObjectTemplate.cockpitSubGeom 3\n";
    exact(*camera, cameraBase.text + cameraTail, "camera exact ordering, precision and duplicate FOVs");
    camera->omitGroups(); Sink omitted; camera->makeScript(&omitted);
    check(omitted.text.find("cameraShakeMaxSpeed") == std::string::npos && omitted.text.find("bounceStrength") == std::string::npos
          && omitted.text.find("nosePos") == std::string::npos, "camera grouped default omissions");
    camera->nose(Vec3{0.0001f,-0.0001f,std::numeric_limits<float>::quiet_NaN()}); Sink tolerance;
    camera->makeScript(&tolerance);
    check(tolerance.text.find("nosePos") == std::string::npos, "camera nose tolerance includes endpoints and unordered components");
    camera->nose(Vec3{std::nextafter(0.0001f, 1.0f),0,0}); Sink beyond;
    camera->makeScript(&beyond);
    check(beyond.text.find("nosePos") != std::string::npos, "camera nose outside tolerance is saved");
    auto* free = new FreeCameraTemplate();
    exact(*free, "ObjectTemplate.worldFOV 1.2\nObjectTemplate.insideFOV 1.2\n", "free camera omits base script");
    auto* animated = new Animated(); animated->setName("animated");
    auto* first = new Animation("first.inc"); auto* third = new Animation("third.inc");
    animated->attach(first, third);
    exact(*animated, bundleScript(*animated) + "ObjectTemplate.createSkeleton \nObjectTemplate.animationSystem1P first.inc\n"
          "ObjectTemplate.animationSystem3P third.inc\n", "animated blank skeleton and 1P/3P order");
    check(first->calls == 4 && third->calls == 4, "animation paths tested and fetched separately; base failure skips paths");
    first->path.clear(); third->path.clear(); Sink emptyPaths; animated->makeScript(&emptyPaths);
    check(emptyPaths.text == bundleScript(*animated) + "ObjectTemplate.createSkeleton \n", "empty animation paths omitted");
    auto* firearm = new GenericFireArmTemplate(); firearm->setName("firearm");
    firearm->m_secondaryProjectileTemplateName = "secondary"; firearm->m_keepProjectiles = 0.125f;
    firearm->m_velocity = 123.45678f; firearm->setItemIndex(2); firearm->setItemType(3);
    firearm->setDelayToUse(0.5f); firearm->m_isSelectable = false; firearm->m_canInternalHandleMessage = true;
    firearm->m_inheritHeatFromPrimary = true; firearm->m_heatAddWhenFire = 1; firearm->m_coolDownPerSec = 2;
    firearm->m_overheatPenalty = 3; firearm->m_setHideFirstPerson = true; firearm->m_blockInputOnRotBundle = "turret";
    exact(*firearm, bundleScript(*firearm) + "ObjectTemplate.secondaryProjectileTemplate secondary\n"
          "ObjectTemplate.keepProjectiles 0.125\nObjectTemplate.velocity 123.457\nObjectTemplate.itemIndex 2\nObjectTemplate.itemType 3\n"
          "ObjectTemplate.delayToUse 0.5\n ObjectTemplate.isSelectable 0\n ObjectTemplate.forceSustainedFire 1\n"
          "ObjectTemplate.inheritHeatFromPrimary 1\nObjectTemplate.heatAddWhenFire 1\nObjectTemplate.coolDownPerSec 2\n"
          "ObjectTemplate.overheatPenalty 3\nObjectTemplate.setHideFirstPerson 1\nObjectTemplate.blockInputOnRotBundle turret\n",
          "firearm exact formatting, including leading spaces");
    auto* projectile = new Projectile();
    firearm->m_projectileTemplateName = "primary"; firearm->m_projectileTemplate = projectile;
    Sink nested;
    check(firearm->makeScript(&nested) && projectile->saves == 1
          && nested.text.find("\n\nprojectile\n") != std::string::npos,
          "firearm nests primary projectile and ignores its failed save");
    Sink nestedShort; nestedShort.failAt = nested.writes - 2;
    check(!firearm->makeScript(&nestedShort) && projectile->saves == 2,
          "firearm still saves nested projectile after its own short write");
    projectile->updateFlags(1,0); Sink separate;
    check(firearm->makeScript(&separate) && projectile->saves == 2,
          "separately saved projectile is not nested");
    firearm->m_heatAddWhenFire = 0; Sink cold;
    firearm->makeScript(&cold);
    check(cold.text.find("heatAddWhenFire") == std::string::npos && cold.text.find("coolDownPerSec") == std::string::npos
          && cold.text.find("overheatPenalty") == std::string::npos, "firearm zero heat omits the entire heat group");
    auto* soldier = new SoldierTemplate(); soldier->setNameTagOffset(Vec3{1,2,3}); soldier->setRidingWithOffset(Vec3{4,5,6});
    auto* face = static_cast<ISoldierTemplate*>(soldier->queryInterface(IID_ISoldierTemplate));
    check(face && face == static_cast<ISoldierTemplate*>(soldier) && face->getInventorySize() == 6,
          "adjusted soldier interface dispatches inventory getter");
    check(soldier->getNameTagOffset().x == 1 && soldier->getNameTagOffset().y == 2 && soldier->getNameTagOffset().z == 3
          && soldier->getRidingWithOffset().x == 4 && soldier->getRidingWithOffset().y == 5 && soldier->getRidingWithOffset().z == 6,
          "soldier offset setters preserve all components");
    const int refs = face->getRef(); face->addRef(); check(face->getRef() == refs+1 && face->release() == refs, "secondary refcount forwards to soldier leaf");
    debugOn[DebugTypeError] = false; Sink refused;
    check(!static_cast<io::IConsoleSaveable*>(soldier->queryInterface(io::IID_IConsoleSaveable))->makeScript(&refused)
          && refused.writes == 0, "soldier refuses save without base output when diagnostic filtered");
    io::mainConsole = new io::OldConsole();
    mainConsoleObjects->registerConsoleObjects(consoleBindingList("world.objTemplGame"));
    auto* vehicle = new PlayerControlObjectTemplate();
    objectTemplateManager->setActiveObjectTemplate(vehicle);
    std::string result;
    io::mainConsole->handleCommand("ObjectTemplate.nameTagOffset 1/2/3", result, static_cast<AccessType>(7));
    check(result.empty() && vehicle->getNameTagOffset().x == 1 && vehicle->getNameTagOffset().y == 2
          && vehicle->getNameTagOffset().z == 3, "registered nameTagOffset writes the active player-control template");
    io::mainConsole->handleCommand("ObjectTemplate.nameTagOffset", result, static_cast<AccessType>(7));
    check(result == "1/2/3", "registered nameTagOffset reads the active player-control template");
    objectTemplateManager->setActiveObjectTemplate(soldier);
    io::mainConsole->handleCommand("ObjectTemplate.nameTagOffset 9/9/9", result, static_cast<AccessType>(7));
    check(result == "Unknown object or method!" && soldier->getNameTagOffset().x == 1,
          "nameTagOffset retains the shipped player-control-only activity check");
    auto* root = new RootWriter();
    const std::string rootText = attach(*root, descriptor(3)); exact(*root, rootText, "particle base exact lines");
    auto* branch = new BranchWriter();
    auto* branchDescriptor = static_cast<geom::SaveDescriptor0*>(descriptor(0));
    const std::string branchText = attach(*branch, branchDescriptor) + spriteBase; exact(*branch, branchText, "sprite base exact lines");
    check(branchDescriptor->readsgetLightAffectionFactor == 4 && branchDescriptor->readsgetIsStaticSystem == 4,
          "sprite base repeats conditional getters for output");
    branchDescriptor->setLightAffectionFactor(-1); branchDescriptor->setIsStaticSystem(false); Sink noLight;
    branch->makeScript(&noLight);
    check(noLight.text.find("lightAffectionFactor") == std::string::npos && noLight.text.find("isStaticSystem") == std::string::npos,
          "sprite base omits negative light affection and false static flag");
    branchDescriptor->setLightAffectionFactor(std::numeric_limits<float>::quiet_NaN()); Sink unorderedLight;
    branch->makeScript(&unorderedLight);
    check(unorderedLight.text.find("lightAffectionFactor") == std::string::npos, "sprite base omits unordered light affection");
    auto* sprite = new Particle<SpriteParticleSystemTemplate>(); auto* s = static_cast<geom::IParticleSystemTemplateType0*>(descriptor(0));
    const std::string spritePrefix = attach(*sprite, s) + spriteBase; spriteSettings(s); s->setParticleType(2); s->setDrawOrder(1);
    s->setNearCullingEnable(true); s->setWindAgingFactor(0.25f); s->setWindSpeedAgeDistance(10);
    exact(*sprite, spritePrefix + "ObjectTemplate.particleType Additive\nObjectTemplate.drawOrder Back\n" + spriteSettingsText
          + "ObjectTemplate.nearCullingEnable 1\nObjectTemplate.windAgingFactor 0.25\nObjectTemplate.windSpeedAgeDistance 10\n", "sprite exact lines and typed wind/culling dispatch");
    const auto* spriteInputs = static_cast<geom::SaveDescriptor0*>(s);
    check(spriteInputs->readsgetNearCullingEnable == 4 && spriteInputs->readsgetWindAgingFactor == 4
          && spriteInputs->readsgetWindSpeedAgeDistance == 2, "sprite repeats the culling and aging tests before output");
    s->setNearCullingEnable(false); s->setWindAgingFactor(std::numeric_limits<float>::quiet_NaN()); Sink noWind;
    sprite->makeScript(&noWind);
    check(noWind.text.find("nearCullingEnable") == std::string::npos && noWind.text.find("windAgingFactor") == std::string::npos
          && noWind.text.find("windSpeedAgeDistance") == std::string::npos, "sprite omits false culling and unordered wind aging group");
    auto* nsa = new Particle<NonScreenAlignedParticleSystemTemplate>(); auto* n = static_cast<geom::IParticleSystemTemplateType4*>(descriptor(4));
    const std::string nsaPrefix = attach(*nsa, n) + spriteBase; spriteSettings(n);
    exact(*nsa, nsaPrefix + spriteSettingsText, "non-screen-aligned exact lines");
    auto* mesh = new Particle<MeshParticleSystemTemplate>(); auto* m = static_cast<geom::IParticleSystemTemplateType1*>(descriptor(1));
    const std::string meshPrefix = attach(*mesh, m) + spriteBase;
    m->setRandomRotation(Vec3{1,2,3}); m->setRandomRotationSpeed(Vec3{4,5,6}); m->setRotationSpeed(Vec3{7,8,9});
    m->setDirectionalScale(0.5f); m->setBlendMode(1); m->setDrawOrder(1); m->setWriteZ(true);
    exact(*mesh, meshPrefix + "ObjectTemplate.randomRotation 1/2/3\nObjectTemplate.randomRotationSpeed 4/5/6\n"
          "ObjectTemplate.rotationSpeed 7/8/9\nObjectTemplate.directionalScale 0.5\nObjectTemplate.blendMode Additive\n"
          "ObjectTemplate.drawOrder Back\nObjectTemplate.writeZ 1\nObjectTemplate.geometry \n", "mesh exact lines");
    auto* emitter = new Particle<ParticleSystemEmitterTemplate>(); auto* e = static_cast<geom::IParticleSystemEmitterTemplate*>(descriptor(3));
    const std::string emitterPrefix = attach(*emitter, e);
    e->setRandomRotation(Vec3{1,2,3}); e->setRandomRotationSpeed(Vec3{4,5,6}); e->setRotationSpeed(Vec3{7,8,9}); e->setSpeedFromParticle(0.75f);
    exact(*emitter, emitterPrefix + "ObjectTemplate.randomRotation 1/2/3\nObjectTemplate.randomRotationSpeed 4/5/6\n"
          "ObjectTemplate.rotationSpeed 7/8/9\nObjectTemplate.speedFromParticle 0.75\nObjectTemplate.particleSystemTemplate \n", "particle emitter exact lines");
    auto* trail = new Particle<TrailSystemTemplate>(); auto* t = static_cast<geom::IParticleSystemTemplateType2*>(descriptor(2));
    const std::string trailPrefix = attach(*trail, t) + spriteBase;
    t->setAnimationEnable(true); t->setAnimationSpeed(35); t->setAnimationFrameCount(16); t->setAnimationFrameCountX(4);
    t->setFadeInFactor(36); t->setFadeOutFactor(37); t->setTextureTileFactor(38); t->setTextureOffsetVelocity(39);
    t->setNumPlanes(0xffffffffu); t->setMinTrailVelocity(40);
    exact(*trail, trailPrefix + "ObjectTemplate.textureName \nObjectTemplate.animationEnable 1\nObjectTemplate.animationSpeed 35\n"
          "ObjectTemplate.setAnimationFrameWidthRelative 0\nObjectTemplate.setAnimationFrameHeightRelative 0\n"
          "ObjectTemplate.animationFrameCount 16\nObjectTemplate.animationFrameCountX 4\nObjectTemplate.fadeInFactor 36\n"
          "ObjectTemplate.fadeOutFactor 37\nObjectTemplate.textureTileFactor 38\nObjectTemplate.textureOffsetVelocity 39\n"
          "ObjectTemplate.numPlanes 4294967295\nObjectTemplate.minTrailVelocity 40\n", "trail exact lines and unsigned formatting");
    std::printf("template-save fixture: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
