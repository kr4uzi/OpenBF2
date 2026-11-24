// Path: [INFERRED] header for the attested Game/Objects/Fx/EffectBundle.cpp.
// BF2.exe constructor 0x6424B0 (0x1B0 bytes); ELF 0x81F9000 (0x190).
// STL/base layout differs between ports. Native members preserve ownership and ordering.
#ifndef GAME_OBJECTS_FX_EFFECTBUNDLE_H
#define GAME_OBJECTS_FX_EFFECTBUNDLE_H
#include "../Base/Bundle.h"
#include "../../../Sound/ISoundNode.h"
#include "IEffectController.h"
#include <vector>

namespace dice { namespace hfe {
class ISound;
struct Plane3;
namespace geom { class IParticleSystemInstance; }
namespace world {
class EffectBundleTemplate;
extern bool g_effectsDisabled; // ; enableParticles is its inverse.

class EffectBundle : public Bundle, public ISoundNode, public IEffectController
{
    friend class WheelEffectBundle; // ELF 82159CE / PE 5FC6AA: direct speed/has-speed stores.
#if !DEDICATED
    friend class AirDraftEffectBundle; // BF2.exe 0x645B24..0x645B46: direct speed/has-speed stores.
#endif
    friend class Emitter;           // PE 648603: handleUpdate reads the parent's m_fovOverride (+1AC).
public:
    explicit EffectBundle(const EffectBundleTemplate* objectTemplate);
    virtual ~EffectBundle();
    virtual void init();
    virtual void disableCachedObject();
    virtual void enableCachedObject();
    virtual void handleUpdate(float deltaTime, unsigned int flags);
    virtual void handleMessage(TemplateMessage message, IPlayer* player, bool recursive);
    virtual void overRideTimeToLive(bool enabled);
    virtual void togglePause();
    virtual const Mat4& getTransform();
    virtual const Math::Vec3* getVelocity();
    virtual void addNumEmittersPlaying();
    virtual void decNumEmittersPlaying();
    virtual IObject* getObject() const;

    void addParticleSystemInstance(geom::IParticleSystemInstance*, unsigned char viewMask);
    void removeParticleSystemInstance(geom::IParticleSystemInstance*);
    void removeInstances();
    void startEffects();
    void stopEffects();
    bool getIsFirstPerson(bool* fovOverride);
    void updateFirstPerson(bool forceThirdPerson);
    void updateEffectProperties();
    void setSpeedForAllEffects();
    void setAlphaIntensityForAllEffects(float value);
    void setEmitScaleFactorForAllEffects(float value);
    void setEmitSpeedScaleFactorForAllEffects(float value);
    void setEmitDirectionScaleFactorForAllEffects(const Vec3& value);
    void setAgeScaleFactorForAllEffects(float value);
    void setStartAgeForAllEffects(float value);
    void setCollisionPlane(const Plane3& value);
    void setPlayerForGasCloudType(IPlayer* player, int material, int gasCloudType);

protected:
    // Own fields, PE +0x15C / ELF +0x144 onward.
    bool m_startOnNextUpdate;
    bool m_keepUpdating;
    bool m_effectsPlaying;
    int m_numEmittersPlaying;
    float m_timeToLive;
    ISound* m_startSound;
    bool m_paused;
    std::vector<geom::IParticleSystemInstance*> m_instances;
    std::vector<unsigned char> m_viewMasks;
    Vec3 m_speed;
    bool m_hasSpeed;
    float m_maxAcceleration;
    bool m_overrideTimeToLive;
    float m_soundIntensity;
    bool m_fovOverride;
    bool m_firstPerson;
};
} } }
#endif
