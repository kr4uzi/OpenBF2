// Path: [INFERRED] in Fx; registration TU attested by ModuleWorldObjTemplFx.cpp's ELF symbol.
// Particle-system authoring templates that forward to renderer descriptors.
// These factories also preserve dedicated-server template IDs when no renderer exists.
// Accessor null guards differ by method; getters must not acquire blanket fallback values.
// Slot order, by-value Vec3/string returns and setter dirty notifications are part of the ABI.

#ifndef GAME_OBJECTS_PARTICLESYSTEMTEMPLATES_H // [INFERRED] Guard style.
#define GAME_OBJECTS_PARTICLESYSTEMTEMPLATES_H

#include <iosfwd>
#include <string>

#include "../ObjectTemplate.h"
#include "../../../Core/Random.h"

namespace dice {
namespace hfe {

namespace geom { class IParticleSystemTemplate; enum GasCloudType : int; }

namespace world {

// The five .rodata name tables - see the header block. world-scope symbols, as the ELF's.
extern const char* const SpriteParticleEmitterTypeNames[4];
extern const char* const ParticleBlendModeNames[2];
extern const char* const ParticleDrawOrderNames[3];
extern const char* const ParticleEmitPrioNames[3];
extern const char* const SpriteParticleSystemTypeNames[3];

// The console spelling of geom::GasCloudType: ELF 0x8211340 / 0x8211380 (world scope, as the
// symbols). 0 = "None", 1 = "TearGas".
std::ostream& operator<<(std::ostream& stream, geom::GasCloudType type);
std::istream& operator>>(std::istream& stream, geom::GasCloudType& type);

// ParticleSystemTemplateBase - the family root. +15 virtuals over ObjectTemplate (slot order
// below); no CID of its own (queryImplementation in every leaf answers only the leaf CID and
// CID_ObjectTemplate); no quartet (the leaves own theirs).
// m_impl is absent without ParticleSystemManager. Keep each accessor's own null policy.
class ParticleSystemTemplateBase : public ObjectTemplate
{
public:
    ParticleSystemTemplateBase();
    virtual ~ParticleSystemTemplateBase();
    virtual bool makeScript(io::IStream* stream) override;

    // Base particle-system virtual methods.
    virtual void  setIsLocalSystem(bool);                            // +0xE8 impl-forward
    virtual bool  getIsLocalSystem() const;                          // +0xEC impl-pull
    virtual void  setRadialDirection(float);                         // +0xF0 float (Ef)
    virtual float getRadialDirection() const;                        // +0xF4
    virtual void  setRandomRadialDirection(float);                   // +0xF8 float (Ef)
    virtual float getRandomRadialDirection() const;                  // +0xFC
    virtual void  setEmitterType(const std::string& name);           // +0x100 .cpp
    virtual const std::string& getEmitterType() const;               // +0x104 .cpp
    virtual int   getEmitterTypeEnum() const;                        // +0x108 .cpp
    virtual void  setEmitRangeX(const RandomDistribution&);          // +0x10C RandomDistribution
    virtual const RandomDistribution& getEmitRangeX() const;         // +0x110
    virtual void  setEmitRangeY(const RandomDistribution&);          // +0x114
    virtual const RandomDistribution& getEmitRangeY() const;         // +0x118
    virtual void  setEmitRangeZ(const RandomDistribution&);          // +0x11C
    virtual const RandomDistribution& getEmitRangeZ() const;         // +0x120

    // NON-virtual name parsers with read bodies (the slot view proves they are not in the
    // vtable; the recovered authoring forwarding surface follows below).
    void setEmitPrio(const std::string& name);        // .cpp
    void setLodDistance(const std::string& name);     // .cpp - parses the
    void setQualityLevel(const std::string& name);    // .cpp -   EmitPrio names
    void setEmitRadius(Vec3 radius);                  // by value, impl-forward

    // Nonvirtual authoring forwarders, read in full at ELF820F410..8210640.
    std::string getEmitPrio() const;
    std::string getLodDistance() const;
    std::string getQualityLevel() const;
    Vec3 getEmitRadius() const;
    void setTimeToLive(float value);
    float getTimeToLive();
    void setRandomTimeToLive(float value);
    float getRandomTimeToLive();
    void setWindAffectionFactor(float value);
    float getWindAffectionFactor() const;
    void setRotationGraph(const Vec4& value);
    const Vec4& getRotationGraph() const;
    void setGravityGraph(const Vec4& value);
    const Vec4& getGravityGraph() const;
    void setAirResistanceGraph(const Vec4& value);
    const Vec4& getAirResistanceGraph() const;
    void setEmitSpeedGraph(const Vec4& value);
    const Vec4& getEmitSpeedGraph() const;
    void setGravity(float value);
    float getGravity() const;
    void setAirResistance(float value);
    float getAirResistance() const;
    void setRandomSpeed(float value);
    float getRandomSpeed() const;
    void setRandomDirectionAngle(const Vec3& value);
    Vec3 getRandomDirectionAngle() const;
    void setCollisionEnable(bool value);
    bool getCollisionEnable() const;
    void setForceToWaterSurface(bool value);
    bool getForceToWaterSurface() const;
    void setWaterSurfaceOffset(float value);
    float getWaterSurfaceOffset() const;
    void setCollisionSpeedAlongNormal(float value);
    float getCollisionSpeedAlongNormal() const;
    void setCollisionSpeedAlongPlane(float value);
    float getCollisionSpeedAlongPlane() const;
    void setEmitFrequency(float value);
    float getEmitFrequency() const;
    void setEmitSpeed(float value);
    float getEmitSpeed() const;
    void setEmitTime(float value);
    float getEmitTime() const;
    void setIsLooping(bool value);
    bool getIsLooping() const;
    void setEmitDelay(float value);
    float getEmitDelay() const;
    void setAlignRotationToSpeed(bool value);
    bool getAlignRotationToSpeed() const;
    void setEmitDirection(const Vec3& value);
    Vec3 getEmitDirection() const;
    void setPreWarmTime(float value);
    float getPreWarmTime() const;
    void setTransparencyGraph(const Vec4& value);
    const Vec4& getTransparencyGraph() const;

    // Gas-cloud authoring forwarders (xpack tear gas): ELF 0x8210270..0x8210560 /
    // client PE 0x64F300..0x64F4E0, read in full. Setters keep the repeated impl test before the
    // refresh; getters dereference the impl unguarded, as shipped.
    void setGasCloudType(geom::GasCloudType value);
    geom::GasCloudType getGasCloudType() const;
    void setBlurryVisionTime(float value);
    float getBlurryVisionTime() const;
    void setBlurryVisionHoldTime(float value);
    float getBlurryVisionHoldTime() const;
    void setMinGasCloudRadius(float value);
    float getMinGasCloudRadius() const;
    void setMaxGasCloudRadius(float value);
    float getMaxGasCloudRadius() const;
    void setGasCloudRadiusTime(float value);
    float getGasCloudRadiusTime() const;
    void setGasCloudDamage(float value);
    float getGasCloudDamage() const;

protected:
    // layout, +0x30..+0x43. ObjectTemplate occupies 0x00..0x2F.
    // m_emitterType is NOT written by the constructor's stores - the tail call
    // setEmitterType("Standard") is what deterministically zeroes it. Left uninitialised here for
    // the same reason, with the same guarantee.
    /* +0x30 */ int         m_emitterType;            // index into SpriteParticleEmitterTypeNames
    /* +0x34 */ std::string m_emitterTypeName;        // setEmitterType's copy of the parsed token
    /* +0x38 */ std::string m_string38;               // [UNRESOLVED role] ctor "", dtor-only
    friend class EffectBundleTemplate; // instance creation reads impl and view flags
    /* +0x3C */ geom::IParticleSystemTemplate* m_impl; // render-side impl; null on dedicated
public:
    // Direct console fields: ELF ConsoleClass301/302; client flags at +A0/+A1.
    /* +0x40 */ bool        m_byte40;                 // showInFirstPerson, ctor true
    /* +0x41 */ bool        m_byte41;                 // showInThirdPerson, ctor true
};

// SpriteParticleSystemTemplateBase - the sprite-branch intermediate, +2 virtuals. Its 40-byte
// constructor is base + the two vptr stores and nothing else, and the branch's leaves read their
// refcount at +0x48 - so this class carries ONE dword nothing recovered ever writes.
class SpriteParticleSystemTemplateBase : public ParticleSystemTemplateBase
{
public:
    SpriteParticleSystemTemplateBase();
    virtual ~SpriteParticleSystemTemplateBase();
    virtual bool makeScript(io::IStream* stream) override;

    virtual void setIsStaticSystem(bool);             // +0x124 impl-forward
    virtual bool getIsStaticSystem() const;           // +0x128 impl-pull

    // Nonvirtual sprite-branch authoring, ELF8210E00..8211240.
    void setLightMapIntensityOffset(float value);
    float getLightMapIntensityOffset() const;
    void setParticleMaxSize(float value);
    float getParticleMaxSize();
    void setColor1(const Vec3& value);
    Vec3 getColor1() const;
    void setColor2(const Vec3& value);
    Vec3 getColor2() const;
    void setColorBlendGraph(const Vec4& value);
    const Vec4& getColorBlendGraph() const;
    void setSizeGraph(const Vec4& value);
    const Vec4& getSizeGraph() const;
    void setRandomSize(float value);
    float getRandomSize() const;
    void setRandomIntensity(float value);
    float getRandomIntensity() const;
    void setAlphaCull(float value);
    float getAlphaCull() const;
    // / 0x82112B0, 67 B / 27 B - Nonvirtual like the twelve
    // above; the mesh particle's `directionalScale` console binding (ConsoleClass387, a mesh-only
    // selector) is their only caller. Impl ELF +0x1BC / +0x1C0 = IParticleSpriteSystemTemplate
    // PE +0x1B8 / +0x1BC.
    void setDirectionalScale(float value);
    float getDirectionalScale() const;

protected:
    /* +0x44 */ int _dword44;                         // [UNRESOLVED] - no recovered body writes it
};

// The shared concrete surface every leaf owns: quartet + identity + lifecycle. All bodies read.
#define BF2_PARTICLE_TEMPLATE_LEAF(cls)                                                     \
    cls();                                                                                  \
    virtual ~cls();                                                                         \
    virtual void  addRef();                                                                 \
    virtual int   getRef();                                                                 \
    virtual int   release();                                                                \
    virtual void* queryInterface(unsigned int iid) const;                                   \
    virtual unsigned int getClassID() const;                                                \
    virtual void* queryImplementation(unsigned int classId);                                \
    virtual void  templateInitialize();                                                     \
    virtual bool  makeScript(io::IStream* stream) override;                                  \
    virtual IObject* createObject() const

// Concrete particle-system templates.
class SpriteParticleSystemTemplate : public SpriteParticleSystemTemplateBase      // sizeof 0x50
{
public:
    BF2_PARTICLE_TEMPLATE_LEAF(SpriteParticleSystemTemplate);      // ctor 0x82122F0
    virtual void setParticleType(const std::string& name);         // +0x12C .cpp
    virtual std::string getParticleType() const;                   // +0x130 BY value
    virtual int  getParticleTypeEnum() const;                      // +0x134 impl-pull
    virtual void setDrawOrder(const std::string& name);            // +0x138 .cpp
    virtual std::string getDrawOrder() const;                      // +0x13C BY value
    virtual int  getParticleDrawOrderEnum() const;                 // +0x140 impl-pull
    void setNearCullingEnable(bool value);
    bool getNearCullingEnable() const;
    void setTextureName(const std::string& name);
    const std::string& getTextureName() const;
    void setRotationSpeed(float value);
    float getRotationSpeed() const;
    void setRandomRotationSpeed(float value);
    float getRandomRotationSpeed() const;
    void setRandomRotation(float value);
    float getRandomRotation() const;
    void setPivotOffset(const Vec2& value);
    Vec2 getPivotOffset() const;
    void setSelfShadowIntensity(float value);
    float getSelfShadowIntensity() const;
    void setAnimationEnable(bool value);
    bool getAnimationEnable() const;
    void setAnimationPlayOnce(bool value);
    bool getAnimationPlayOnce() const;
    void setAnimationRandomizedStartFrame(bool value);
    bool getAnimationRandomizedStartFrame() const;
    void setAnimationSpeed(float value);
    float getAnimationSpeed() const;
    void setAnimationFrameCount(unsigned int value);
    unsigned int getAnimationFrameCount() const;
    void setAnimationFrameCountX(unsigned int value);
    unsigned int getAnimationFrameCountX() const;
    void setAnimationFrameWidthRelative(float value);
    void setAnimationFrameHeightRelative(float value);
    void setWindSpeedAgeDistance(float value);
    float getWindSpeedAgeDistance() const;
    void setWindAgingFactor(float value);
    float getWindAgingFactor() const;
protected:
    /* +0x48 */ int         m_refCount;
    /* +0x4C */ std::string m_particleTypeName;       // setParticleType's copy of the parsed token
};

class MeshParticleSystemTemplate : public SpriteParticleSystemTemplateBase        // sizeof 0x50
{
public:
    BF2_PARTICLE_TEMPLATE_LEAF(MeshParticleSystemTemplate);        // ctor 0x82038E0
    virtual void preCache() const;                                 // .cpp
    virtual void setBlendMode(const std::string& name);            // +0x12C .cpp
    virtual std::string getBlendMode() const;                      // +0x130 BY value
    virtual int  getParticleBlendModeEnum() const;                 // +0x134 impl-pull
    virtual void setDrawOrder(const std::string& name);            // +0x138 .cpp
    virtual std::string getDrawOrder() const;                      // +0x13C BY value
    virtual int  getParticleDrawOrderEnum() const;                 // +0x140 impl-pull
    virtual void setGeometry(const std::string&);                  // +0x144 impl-forward
    virtual const std::string& getGeometry() const;                // +0x148 impl-pull
    // Also present, identically sized, in ParticleSystemEmitterTemplate - different slots, no
    // shared base. Vec3 BY value per the manglings.
    // return type the six Vec3 getters on this class and on
    // ParticleSystemEmitterTemplate return Vec3 BY value, not by const reference. Their 47-byte
    // ELF bodies are the sret shape - hidden buffer in, `mov eax, <buffer>` out, `retn 4` - and the
    // impl slot they forward to is PT_GETVEC, an out-parameter writer. A by-reference return would
    // have been ~24 B, which is exactly what the RandomDistribution getters on the base measure.
    virtual void setRandomRotation(Vec3);                          // +0x14C impl-forward
    virtual Vec3 getRandomRotation() const;                        // +0x150 impl-pull
    virtual void setRandomRotationSpeed(Vec3);                     // +0x154 impl-forward
    virtual Vec3 getRandomRotationSpeed() const;                   // +0x158 impl-pull
    virtual void setRotationSpeed(Vec3);                           // +0x15C impl-forward
    virtual Vec3 getRotationSpeed() const;                         // +0x160 impl-pull
    // Nonvirtual, ELF 0x82048A0 / 0x82048F0, 67 B / 30 B - for the mesh-only
    // `writeZ` console binding (ConsoleClass311). Impl ELF +0x1DC / +0x1E0 =
    // IParticleSystemTemplateType1 PE +0x1D8 / +0x1DC.
    void setWriteZ(bool value);
    bool getWriteZ() const;
protected:
    /* +0x48 */ int         m_refCount;
    /* +0x4C */ std::string m_blendModeName;          // setBlendMode's copy of the parsed token
};

// Adds no virtual slots; specializes class identity and object creation.
class NonScreenAlignedParticleSystemTemplate : public SpriteParticleSystemTemplateBase   // sizeof 0x4C
{
public:
    BF2_PARTICLE_TEMPLATE_LEAF(NonScreenAlignedParticleSystemTemplate);   // ctor 0x8204D30
    void setTextureName(const std::string& name);
    const std::string& getTextureName() const;
    void setRotationSpeed(float value);
    float getRotationSpeed() const;
    void setRandomRotationSpeed(float value);
    float getRandomRotationSpeed() const;
    void setRandomRotation(float value);
    float getRandomRotation() const;
    void setPivotOffset(const Vec2& value);
    Vec2 getPivotOffset() const;
    void setSelfShadowIntensity(float value);
    float getSelfShadowIntensity() const;
    void setAnimationEnable(bool value);
    bool getAnimationEnable() const;
    void setAnimationPlayOnce(bool value);
    bool getAnimationPlayOnce() const;
    void setAnimationRandomizedStartFrame(bool value);
    bool getAnimationRandomizedStartFrame() const;
    void setAnimationSpeed(float value);
    float getAnimationSpeed() const;
    void setAnimationFrameCount(unsigned int value);
    unsigned int getAnimationFrameCount() const;
    void setAnimationFrameCountX(unsigned int value);
    unsigned int getAnimationFrameCountX() const;
    void setAnimationFrameWidthRelative(float value);
    void setAnimationFrameHeightRelative(float value);
protected:
    /* +0x48 */ int m_refCount;
};

// adds nothing - same as above.
class TrailSystemTemplate : public SpriteParticleSystemTemplateBase               // sizeof 0x4C
{
public:
    BF2_PARTICLE_TEMPLATE_LEAF(TrailSystemTemplate);               // ctor 0x8213E10
    void setTextureName(const std::string& value);
    const std::string& getTextureName() const;
    void setAnimationEnable(bool value);
    bool getAnimationEnable() const;
    void setAnimationSpeed(float value);
    float getAnimationSpeed() const;
    void setAnimationFrameWidth(unsigned int value);
    unsigned int getAnimationFrameWidth() const;
    void setAnimationFrameHeight(unsigned int value);
    unsigned int getAnimationFrameHeight() const;
    void setAnimationFrameWidthRelative(float value);
    void setAnimationFrameHeightRelative(float value);
    void setAnimationFrameCount(unsigned int value);
    unsigned int getAnimationFrameCount() const;
    void setAnimationFrameCountX(unsigned int value);
    unsigned int getAnimationFrameCountX() const;
    void setFadeInFactor(float value);
    float getFadeInFactor() const;
    void setFadeOutFactor(float value);
    float getFadeOutFactor() const;
    void setTextureTileFactor(float value);
    float getTextureTileFactor() const;
    void setTextureOffsetVelocity(float value);
    float getTextureOffsetVelocity() const;
    void setNumPlanes(unsigned int value);
    unsigned int getNumPlanes() const;
    void setMinTrailVelocity(float value);
    float getMinTrailVelocity() const;
protected:
    /* +0x48 */ int m_refCount;
};

// Derives from the lower base, so it has NO isStaticSystem - see the header block.
class ParticleSystemEmitterTemplate : public ParticleSystemTemplateBase           // sizeof 0x48
{
public:
    BF2_PARTICLE_TEMPLATE_LEAF(ParticleSystemEmitterTemplate);     // ctor 0x820D190
    virtual void setParticleSystemTemplate(const std::string&);    // +0x124 impl-forward
    virtual const std::string& getParticleSystemTemplate();        // +0x128 impl-pull
    virtual void setRandomRotation(Vec3);                          // +0x12C impl-forward
    virtual Vec3 getRandomRotation() const;                        // +0x130 impl-pull
    virtual void setRandomRotationSpeed(Vec3);                     // +0x134 impl-forward
    virtual Vec3 getRandomRotationSpeed() const;                   // +0x138 impl-pull
    virtual void setRotationSpeed(Vec3);                           // +0x13C impl-forward
    virtual Vec3 getRotationSpeed() const;                         // +0x140 impl-pull
    virtual void setSpeedFromParticle(float);                      // +0x144 float (Ef)
    virtual float getSpeedFromParticle() const;                    // +0x148 impl-pull
protected:
    /* +0x44 */ int m_refCount;
};

#undef BF2_PARTICLE_TEMPLATE_LEAF

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_PARTICLESYSTEMTEMPLATES_H
