// Aircraft downwash selected by the ground material and driven by the owning engine.
// Retail offsets below describe BF2.exe x86, not native pointer/STL layouts.
#ifndef GAME_OBJECTS_FX_AIRDRAFTEFFECTBUNDLE_H
#define GAME_OBJECTS_FX_AIRDRAFTEFFECTBUNDLE_H

#if !DEDICATED
#include "../Base/SimpleObject.h"
#include <map>
#include <memory>

namespace dice { namespace hfe {
class ISound;
class ObjectSoundNodeFixed;
namespace world {
class AirDraftEffectBundleTemplate;
class EffectBundle;
class EngineBase;

class AirDraftEffectBundle : public SimpleObject
{
public:
    explicit AirDraftEffectBundle(const AirDraftEffectBundleTemplate* objectTemplate);
    virtual ~AirDraftEffectBundle();
    virtual void init();
    virtual void disableCachedObject();
    virtual void destroy(bool force);
    virtual void handleVisualUpdate(float deltaTime, float interpolation);
    virtual void handleUpdate(float deltaTime, unsigned int flags);
    virtual void handleMessage(TemplateMessage message, IPlayer* player, bool recursive);
    virtual void initNetworkableComp();

    void getEngine();
    void calculateCurrentDistanceAndEffectPosition();
    void updateEffectProperties(float distance, float engineIntensity);
    void setCurrentMaterial(int material, bool recursive);
    void stopEffect();

protected:
    // Member names are [INFERRED] from their uses; the method names are ELF-attested.
    std::map<int, EffectBundle*> m_effects; // +0x150; lifecycle methods destroy values, dtor frees nodes.
    EngineBase* m_engine;                  // +0x15C borrowed, resolved once after parenting.
    bool m_needsEngine;                     // +0x160
    int m_material;                         // +0x164, initially -1.
    unsigned char m_unaccessed168[4];        // +0x168 [UNRESOLVED] untouched extent, no source type claimed.
    Vec3 m_effectPosition;                  // +0x16C
    Mat4 m_effectTransformation;            // +0x178
    EffectBundle* m_currentEffect;          // +0x1B8 alias into m_effects.
    bool m_playing;                         // +0x1BC
    ISound* m_sound;                        // +0x1C0 owned reference.
    std::unique_ptr<ObjectSoundNodeFixed> m_soundNode; // +0x1C4 owned, outlives the sound reference.
    float m_soundIntensity;                 // +0x1C8 shared volume/pitch parameter.
    unsigned int m_expiryFrame;             // +0x1CC, unsigned comparison.
};
} } }
#endif
#endif
