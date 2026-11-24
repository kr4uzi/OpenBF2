// Attested source path: Game/Objects/Fx/AirDraftEffectBundle.cpp (BF2.exe 0x900200).
#include "AirDraftEffectBundle.h"

#if !DEDICATED
#include "AirDraftEffectBundleTemplate.h"
#include "EffectBundle.h"
#include "EffectBundleTemplate.h"
#include "../Game/EngineBase.h"
#include "../Game/Rotor.h"
#include "../ObjectLayoutAccess.h"
#include "../../GameLogic/GameLogicCollision.h"
#include "../../../Core/Debug.h"
#include "../../../Core/Ids.h"
#include "../../../Math/Plane3.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Physics/MaterialManager.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectPredicator.h"
#include "../../../Scene/Object/ObjectUtil.h"
#include "../../../Sound/BfSound.h"
#include "../../../Sound/ObjectSoundNode.h"
#include "../../../Sound/SoundObjectTemplate.h"
#include <cmath>

namespace dice { namespace hfe {
extern IBfSound* g_bfSound;
namespace world {
extern IBase* materialManager;
extern IBase* heightmapCluster;
// Shared with WheelEffectBundle.cpp: BF2.exe 0x6447B0, renderer frame ordinal + 100.
unsigned int getCurrentFrame();
namespace { typedef object_detail::LayoutAccess L; }

// BF2.exe 0x646400: SimpleObject, empty map, engine-search latch, material -1,
// zero position, identity matrix, then null effect/sound/node and zero timers.
AirDraftEffectBundle::AirDraftEffectBundle(const AirDraftEffectBundleTemplate* objectTemplate)
    : SimpleObject(objectTemplate), m_engine(0), m_needsEngine(true), m_material(-1),
      m_effectPosition{0, 0, 0}, m_currentEffect(0), m_playing(false), m_sound(0),
      m_soundIntensity(0), m_expiryFrame(0)
{
    m_effectTransformation = Mat4::getIdentity();
}

// BF2.exe 0x646120 (deleting wrapper 0x646510): release sound, delete node,
// free map nodes (not values), then SimpleObject. unique_ptr supplies the node teardown.
AirDraftEffectBundle::~AirDraftEffectBundle()
{
    if (m_sound) m_sound->release();
}

// BF2.exe 0x5D3DB0: folded forwarding jump to SimpleObject::init (0x628090).
void AirDraftEffectBundle::init() { SimpleObject::init(); }

// BF2.exe table 0x900060 +0x174 = 0x83D270, a bare ret, shared with the client
// SimpleObject table. Supply that client body here; the shared base has the ELF's
// networkable-creation body instead. No additional virtual slot is introduced.
void AirDraftEffectBundle::initNetworkableComp() {}

// BF2.exe 0x645F10: stop before releasing sound; clear alias before destroying
// cached effects, clear map, then base. Engine/material/node state is retained.
void AirDraftEffectBundle::disableCachedObject()
{
    stopEffect();
    if (m_sound)
    {
        m_sound->release();
        m_sound = 0;
    }
    m_currentEffect = 0;
    for (std::map<int, EffectBundle*>::iterator i = m_effects.begin(); i != m_effects.end(); ++i)
        i->second->destroy(true);
    m_effects.clear();
    SimpleObject::disableCachedObject();
}

// BF2.exe 0x646030: unlike disableCachedObject, does not stop or reset the playing
// latch/intensity. Both lifecycle methods clear the map before forwarding.
void AirDraftEffectBundle::destroy(bool force)
{
    if (m_sound)
    {
        m_sound->release();
        m_sound = 0;
    }
    m_currentEffect = 0;
    for (std::map<int, EffectBundle*>::iterator i = m_effects.begin(); i != m_effects.end(); ++i)
        i->second->destroy(true);
    m_effects.clear();
    SimpleObject::destroy(force);
}

// BF2.exe 0x645A00: only non-recursive enter/exit messages; no base forwarding.
void AirDraftEffectBundle::handleMessage(TemplateMessage message, IPlayer*, bool recursive)
{
    const int messageId = static_cast<int>(message);
    if (!recursive)
    {
        if (messageId == 4) updateFlags(0x1000, 0);
        if (messageId == 5)
        {
            updateFlags(0, 0x1000);
            stopEffect();
        }
    }
}

// BF2.exe 0x6468B0, primary +0xA4. The visual pass renews simulation updates.
void AirDraftEffectBundle::handleVisualUpdate(float, float)
{
    calculateCurrentDistanceAndEffectPosition();
    m_expiryFrame = getCurrentFrame() + 30;
    updateFlags(0x800, 0);
}

// BF2.exe 0x645A50: unsigned jbe retains the effect through the expiry frame.
void AirDraftEffectBundle::handleUpdate(float, unsigned int)
{
    if (getCurrentFrame() > m_expiryFrame)
    {
        stopEffect();
        updateFlags(0, 0x800);
    }
}

// BF2.exe 0x645920: defer lookup until parented, then try direct Engine, Rotor's
// engine, or the configured engine name at the root. Lookup is not retried on failure.
void AirDraftEffectBundle::getEngine()
{
    IObject* const parent = getParent();
    if (parent && m_needsEngine)
    {
        const AirDraftEffectBundleTemplate* const t =
            static_cast<const AirDraftEffectBundleTemplate*>(L::objectTemplate(this));
        if (L::objectTemplate(parent)->getClassID() == CID_EngineTemplate)
            m_engine = static_cast<EngineBase*>(parent);
        else if (L::objectTemplate(parent)->getClassID() == CID_RotorTemplate)
            m_engine = static_cast<EngineBase*>(static_cast<Rotor*>(parent)->getEngine());
        else if (!t->m_string2C0.empty())
            m_engine = static_cast<EngineBase*>(findEngine(getRootParent(this), t->m_string2C0));
        m_needsEngine = false;
    }
}

// BF2.exe 0x6459C0: sound stop follows the particle stop; intensity clears last.
void AirDraftEffectBundle::stopEffect()
{
    if (m_currentEffect) m_currentEffect->handleMessage(static_cast<TemplateMessage>(1), 0, false);
    if (m_sound) m_sound->stop();
    m_playing = false;
    m_soundIntensity = 0;
}

// BF2.exe 0x6461A0: cache by target material. recursive is forwarded to the old
// effect's stop message, not a guard on changing material. No sound is created without an effect.
void AirDraftEffectBundle::setCurrentMaterial(int material, bool recursive)
{
    if (material == m_material) return;
    if (m_currentEffect)
    {
        m_playing = false;
        m_currentEffect->handleMessage(static_cast<TemplateMessage>(1), 0, recursive);
        m_currentEffect->updateFlags(1, 0);
        m_currentEffect = 0;
    }
    m_material = material;
    if (material == -1) return;
    const int sourceMaterial = L::objectTemplate(this)->getMaterial();
    EffectBundle* effect = 0;
    std::map<int, EffectBundle*>::iterator i = m_effects.find(m_material);
    if (i != m_effects.end())
    {
        effect = i->second;
        effect->updateFlags(0, 1);
        m_currentEffect = effect;
    }
    else
    {
        const EffectBundleTemplate* const t = static_cast<const EffectBundleTemplate*>(
            static_cast<MaterialManager*>(materialManager)->getEffectTemplate(sourceMaterial, m_material, 0));
        if (t)
        {
            effect = static_cast<EffectBundle*>(t->createObject());
            if (effect)
            {
                m_effects[m_material] = effect;
                // PE +0x394 is timeToLive.value, not the sampled runtime lifetime.
                if (t->m_dist2E0.value > 0.0f)
                    Debug(DebugTypeError,
                        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Game\\Objects\\Fx\\AirDraftEffectBundle.cpp", 470, "")
                        << "TimeToLive Should be set to -1.f";
                m_currentEffect = effect;
                effect->overRideTimeToLive(true);
                effect->init();
            }
        }
    }
    if (!m_currentEffect) return;
    SoundObjectTemplate* const soundTemplate = static_cast<SoundObjectTemplate*>(
        static_cast<MaterialManager*>(materialManager)->getSoundTemplate(sourceMaterial, m_material, 0));
    if (!soundTemplate) return;
    if (m_sound) m_sound->release();
    m_sound = g_bfSound->createSound(static_cast<ISoundObjectTemplate*>(soundTemplate)->impl(), this);
    if (!m_sound) return;
    m_soundNode.reset(new ObjectSoundNodeFixed(effect));
    m_sound->setParent(m_soundNode.get());
    m_sound->setVolumeEvaluator(0, &m_soundIntensity);
    m_sound->setPitchEvaluator(0, &m_soundIntensity);
}

// BF2.exe 0x645AB0: only Speed, Altitude and Engine affectors participate.
// Products retain the x87 expression order; the sixth property controls sound.
void AirDraftEffectBundle::updateEffectProperties(float distance, float engineIntensity)
{
    Vec3 speed = {0, 0, 0};
    IObject* const root = getRootParent(this);
    if (root && root != this && L::physicsNode(root))
        speed = static_cast<IPhysicsNode*>(L::physicsNode(root))->getTangentSpeed(getAbsolutePosition());
    const float magnitude = std::sqrt(speed.z * speed.z + speed.y * speed.y + speed.x * speed.x);
    m_currentEffect->m_hasSpeed = true;
    m_currentEffect->m_speed = speed;
    EffectProperties* const t = const_cast<AirDraftEffectBundleTemplate*>(
        static_cast<const AirDraftEffectBundleTemplate*>(L::objectTemplate(this)));
    for (unsigned int i = 0; i != 6; ++i)
    {
        float speedFactor = 1, altitudeFactor = 1, engineFactor = 1;
        bool used = false;
        if (t->m_property[i][EA_Speed].x != 0 || t->m_property[i][EA_Speed].y != 0)
        {
            speedFactor = t->updateVar(magnitude, t->getProperty(static_cast<EffectProperty>(i), EA_Speed));
            used = true;
        }
        if (t->m_property[i][EA_Altitude].x != 0 || t->m_property[i][EA_Altitude].y != 0)
        {
            altitudeFactor = t->updateVar(distance, t->getProperty(static_cast<EffectProperty>(i), EA_Altitude));
            used = true;
        }
        if (t->m_property[i][EA_Engine].x != 0 || t->m_property[i][EA_Engine].y != 0)
        {
            engineFactor = t->updateVar(engineIntensity, t->getProperty(static_cast<EffectProperty>(i), EA_Engine));
            used = true;
        }
        const float value = ((t->m_range[i].y - t->m_range[i].x) * engineFactor) * altitudeFactor
            * speedFactor + t->m_range[i].x;
        if (used)
            switch (i)
            {
            case 0: m_currentEffect->setAlphaIntensityForAllEffects(value); break;
            case 1: m_currentEffect->setEmitScaleFactorForAllEffects(value); break;
            case 2: m_currentEffect->setEmitSpeedScaleFactorForAllEffects(value); break;
            case 3: m_currentEffect->setAgeScaleFactorForAllEffects(value); break;
            case 4: m_currentEffect->setStartAgeForAllEffects(value); break;
            case 5:
                // PE 0x402EE0 selects the lower bound for unordered input too.
                m_soundIntensity = value >= 0 ? (value <= 1 ? value : 1) : 0;
                break;
            }
    }
}

// BF2.exe 0x646540: query objects, then terrain unconditionally with the SAME hit
// position/normal outputs. A terrain hit may overwrite an object hit. The distance
// field is startDistance (PE +0x38C / ELF +0x2C8); maxDistance and engineInfluence
// are not read. The ray extends five units beyond the startDistance limit.
void AirDraftEffectBundle::calculateCurrentDistanceAndEffectPosition()
{
    IObject* const parent = getParent();
    getEngine();
    if (m_engine && !m_engine->m_engineRunning)
    {
        stopEffect();
        return;
    }
    const AirDraftEffectBundleTemplate* const t =
        static_cast<const AirDraftEffectBundleTemplate*>(L::objectTemplate(this));
    ObjectFlagPredicator predicator(0x100u);
    Vec3 hitPoint = {0, 0, 0}, hitNormal = {0, 0, 0};
    int hitMaterial = -1;
    const float rayLength = t->m_field2C8 + 5.0f;
    const Vec3 down = {0, -1, 0};
    const Mat4& transform = getAbsoluteTransformation();
    const Vec3 direction = {
        (down.z * transform.m[8] + down.y * transform.m[4] + down.x * transform.m[0]) * rayLength,
        (down.z * transform.m[9] + down.y * transform.m[5] + down.x * transform.m[1]) * rayLength,
        (down.z * transform.m[10] + down.y * transform.m[6] + down.x * transform.m[2]) * rayLength};
    IObject* hitObject = 0;
    objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitMaterial,
        getAbsolutePosition(), direction, &predicator, false, false, false, true, 0);
    const Vec3& from = getAbsolutePosition();
    const Vec3 to = {direction.x + from.x, direction.y + from.y, direction.z + from.z};
    float hitFraction;
    unsigned char terrainMaterial;
    const bool hitTerrain = static_cast<IHeightmapCluster*>(heightmapCluster)->intersectRayInWorldCoords(
        getAbsolutePosition(), to, hitPoint, hitFraction, hitNormal, terrainMaterial, true);
    if (hitObject || hitTerrain)
    {
        const Vec3& position = getAbsolutePosition();
        const float dx = position.x - hitPoint.x, dy = position.y - hitPoint.y, dz = position.z - hitPoint.z;
        const float distance = std::sqrt(dz * dz + dy * dy + dx * dx);
        // PE 0x64670B..0x646716: ordered <=, so NaN takes the stop path.
        if (distance <= t->m_field2C8)
        {
            setCurrentMaterial(hitObject && hitMaterial != -1 ? hitMaterial : terrainMaterial, false);
            if (m_material == -1) setCurrentMaterial(0, false);
            m_effectPosition = hitPoint;
            if (!parent) return;
            const float engineIntensity = m_engine ? m_engine->getEngineIntensity() : 1.0f;
            if (m_currentEffect)
            {
                if (t->m_byte2D0)
                {
                    const Mat4& rootTransform = getRootParent(this)->getAbsoluteTransformation();
                    const Vec3 forward = {-rootTransform.m[8], -rootTransform.m[9], -rootTransform.m[10]};
                    m_effectTransformation = GameLogicCollision::getCollisionEffectTransformation(
                        m_effectPosition, hitNormal, forward);
                    m_currentEffect->setAbsoluteTransformation(m_effectTransformation);
                }
                else
                    m_currentEffect->setAbsolutePosition(m_effectPosition);
                updateEffectProperties(distance, engineIntensity);
                const Plane3 plane = {hitNormal,
                    -(hitPoint.z * hitNormal.z + hitPoint.y * hitNormal.y + hitPoint.x * hitNormal.x)};
                m_currentEffect->setCollisionPlane(plane);
            }
            if (!m_playing)
            {
                if (m_currentEffect) m_currentEffect->handleMessage(static_cast<TemplateMessage>(0), 0, false);
                if (m_sound) m_sound->play();
                m_playing = true;
            }
            return;
        }
    }
    if (m_playing) stopEffect();
}
} } }
#endif
