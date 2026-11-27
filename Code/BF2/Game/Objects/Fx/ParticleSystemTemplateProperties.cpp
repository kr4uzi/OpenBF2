// [INFERRED] split beside ParticleSystemTemplates.cpp; full ELF and client PE instructions read.
// Bounded authoring surface used by the stock soldier swimming descriptors. Setters preserve
// the repeated implementation lookup before refresh; getters retain retail null behavior.
#include "ParticleSystemTemplates.h"
#include "../../../Geom/ParticleSystem/ParticleSystemManager.h"
#include "../../../Core/StringUtil.h"

namespace dice { namespace hfe { namespace world {
using geom::IParticleSpriteSystemTemplate;
using geom::IParticleSystemTemplateType0;
using geom::IParticleSystemTemplateType4;


std::string ParticleSystemTemplateBase::getEmitPrio() const
{
    return ParticleEmitPrioNames[m_impl->getEmitPrio()];
}


std::string ParticleSystemTemplateBase::getLodDistance() const
{
    return ParticleEmitPrioNames[m_impl->getLodDistance()];
}


std::string ParticleSystemTemplateBase::getQualityLevel() const
{
    return ParticleEmitPrioNames[m_impl->getQualityLevel()];
}


Vec3 ParticleSystemTemplateBase::getEmitRadius() const
{
    return m_impl->getEmitRadius();
}


void ParticleSystemTemplateBase::setTimeToLive(float value)
{
    if (m_impl)
    {
        m_impl->setTimeToLive(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getTimeToLive()
{
    return m_impl->getTimeToLive();
}


void ParticleSystemTemplateBase::setRandomTimeToLive(float value)
{
    if (m_impl)
    {
        m_impl->setRandomTimeToLive(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getRandomTimeToLive()
{
    return m_impl->getRandomTimeToLive();
}


void ParticleSystemTemplateBase::setWindAffectionFactor(float value)
{
    if (m_impl)
    {
        m_impl->setWindAffectionFactor(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getWindAffectionFactor() const
{
    return m_impl->getWindAffectionFactor();
}


void ParticleSystemTemplateBase::setRotationGraph(const Vec4& value)
{
    if (m_impl)
    {
        m_impl->setRotationGraph(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
const Vec4& ParticleSystemTemplateBase::getRotationGraph() const
{
    return m_impl->getRotationGraph();
}


void ParticleSystemTemplateBase::setGravityGraph(const Vec4& value)
{
    if (m_impl)
    {
        m_impl->setGravityGraph(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
const Vec4& ParticleSystemTemplateBase::getGravityGraph() const
{
    return m_impl->getGravityGraph();
}


void ParticleSystemTemplateBase::setAirResistanceGraph(const Vec4& value)
{
    if (m_impl)
    {
        m_impl->setAirResistanceGraph(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
const Vec4& ParticleSystemTemplateBase::getAirResistanceGraph() const
{
    return m_impl->getAirResistanceGraph();
}


void ParticleSystemTemplateBase::setEmitSpeedGraph(const Vec4& value)
{
    if (m_impl)
    {
        m_impl->setEmitSpeedGraph(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
const Vec4& ParticleSystemTemplateBase::getEmitSpeedGraph() const
{
    return m_impl->getEmitSpeedGraph();
}


void ParticleSystemTemplateBase::setGravity(float value)
{
    if (m_impl)
    {
        m_impl->setGravity(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getGravity() const
{
    return m_impl->getGravity();
}


void ParticleSystemTemplateBase::setAirResistance(float value)
{
    if (m_impl)
    {
        m_impl->setAirResistance(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getAirResistance() const
{
    return m_impl->getAirResistance();
}


void ParticleSystemTemplateBase::setRandomSpeed(float value)
{
    if (m_impl)
    {
        m_impl->setRandomSpeed(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getRandomSpeed() const
{
    return m_impl->getRandomSpeed();
}


void ParticleSystemTemplateBase::setRandomDirectionAngle(const Vec3& value)
{
    if (m_impl)
    {
        m_impl->setRandomDirectionAngle(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
Vec3 ParticleSystemTemplateBase::getRandomDirectionAngle() const
{
    return m_impl->getRandomDirectionAngle();
}


void ParticleSystemTemplateBase::setCollisionEnable(bool value)
{
    if (m_impl)
    {
        m_impl->setCollisionEnable(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
bool ParticleSystemTemplateBase::getCollisionEnable() const
{
    return m_impl->getCollisionEnable();
}


void ParticleSystemTemplateBase::setForceToWaterSurface(bool value)
{
    if (m_impl)
    {
        m_impl->setForceToWaterSurface(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
bool ParticleSystemTemplateBase::getForceToWaterSurface() const
{
    return m_impl->getForceToWaterSurface();
}


void ParticleSystemTemplateBase::setWaterSurfaceOffset(float value)
{
    if (m_impl)
    {
        m_impl->setWaterSurfaceOffset(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getWaterSurfaceOffset() const
{
    return m_impl->getWaterSurfaceOffset();
}


void ParticleSystemTemplateBase::setCollisionSpeedAlongNormal(float value)
{
    if (m_impl)
    {
        m_impl->setCollisionSpeedAlongNormal(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getCollisionSpeedAlongNormal() const
{
    return m_impl->getCollisionSpeedAlongNormal();
}


void ParticleSystemTemplateBase::setCollisionSpeedAlongPlane(float value)
{
    if (m_impl)
    {
        m_impl->setCollisionSpeedAlongPlane(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getCollisionSpeedAlongPlane() const
{
    return m_impl->getCollisionSpeedAlongPlane();
}


void ParticleSystemTemplateBase::setEmitFrequency(float value)
{
    if (m_impl)
    {
        m_impl->setEmitFrequency(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getEmitFrequency() const
{
    return m_impl->getEmitFrequency();
}


void ParticleSystemTemplateBase::setEmitSpeed(float value)
{
    if (m_impl)
    {
        m_impl->setEmitSpeed(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getEmitSpeed() const
{
    return m_impl->getEmitSpeed();
}


void ParticleSystemTemplateBase::setEmitTime(float value)
{
    if (m_impl)
    {
        m_impl->setEmitTime(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getEmitTime() const
{
    return m_impl->getEmitTime();
}


void ParticleSystemTemplateBase::setIsLooping(bool value)
{
    if (m_impl)
    {
        m_impl->setIsLooping(value);
    }
}
bool ParticleSystemTemplateBase::getIsLooping() const
{
    return m_impl->getIsLooping();
}


void ParticleSystemTemplateBase::setEmitDelay(float value)
{
    if (m_impl)
    {
        m_impl->setEmitDelay(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getEmitDelay() const
{
    return m_impl->getEmitDelay();
}


void ParticleSystemTemplateBase::setAlignRotationToSpeed(bool value)
{
    if (m_impl)
    {
        m_impl->setAlignRotationToSpeed(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
bool ParticleSystemTemplateBase::getAlignRotationToSpeed() const
{
    return m_impl->getAlignRotationToSpeed();
}


void ParticleSystemTemplateBase::setEmitDirection(const Vec3& value)
{
    if (m_impl)
    {
        m_impl->setEmitDirection(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
Vec3 ParticleSystemTemplateBase::getEmitDirection() const
{
    return m_impl->getEmitDirection();
}


void ParticleSystemTemplateBase::setPreWarmTime(float value)
{
    if (m_impl)
    {
        m_impl->setPreWarmTime(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getPreWarmTime() const
{
    return m_impl->getPreWarmTime();
}


void ParticleSystemTemplateBase::setTransparencyGraph(const Vec4& value)
{
    if (m_impl)
    {
        m_impl->setTransparencyGraph(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
const Vec4& ParticleSystemTemplateBase::getTransparencyGraph() const
{
    return m_impl->getTransparencyGraph();
}


void SpriteParticleSystemTemplateBase::setLightMapIntensityOffset(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSpriteSystemTemplate*>(m_impl)->setLightMapIntensityOffset(value);
    }
}
float SpriteParticleSystemTemplateBase::getLightMapIntensityOffset() const
{
    return static_cast<IParticleSpriteSystemTemplate*>(m_impl)->getLightMapIntensityOffset();
}


void SpriteParticleSystemTemplateBase::setParticleMaxSize(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSpriteSystemTemplate*>(m_impl)->setParticleMaxSize(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float SpriteParticleSystemTemplateBase::getParticleMaxSize()
{
    return static_cast<IParticleSpriteSystemTemplate*>(m_impl)->getParticleMaxSize();
}


void SpriteParticleSystemTemplateBase::setColor1(const Vec3& value)
{
    if (m_impl)
    {
        static_cast<IParticleSpriteSystemTemplate*>(m_impl)->setColor1(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
Vec3 SpriteParticleSystemTemplateBase::getColor1() const
{
    return static_cast<IParticleSpriteSystemTemplate*>(m_impl)->getColor1();
}


void SpriteParticleSystemTemplateBase::setColor2(const Vec3& value)
{
    if (m_impl)
    {
        static_cast<IParticleSpriteSystemTemplate*>(m_impl)->setColor2(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
Vec3 SpriteParticleSystemTemplateBase::getColor2() const
{
    return static_cast<IParticleSpriteSystemTemplate*>(m_impl)->getColor2();
}


void SpriteParticleSystemTemplateBase::setColorBlendGraph(const Vec4& value)
{
    if (m_impl)
    {
        static_cast<IParticleSpriteSystemTemplate*>(m_impl)->setColorBlendGraph(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
const Vec4& SpriteParticleSystemTemplateBase::getColorBlendGraph() const
{
    return static_cast<IParticleSpriteSystemTemplate*>(m_impl)->getColorBlendGraph();
}


void SpriteParticleSystemTemplateBase::setSizeGraph(const Vec4& value)
{
    if (m_impl)
    {
        static_cast<IParticleSpriteSystemTemplate*>(m_impl)->setSizeGraph(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
const Vec4& SpriteParticleSystemTemplateBase::getSizeGraph() const
{
    return static_cast<IParticleSpriteSystemTemplate*>(m_impl)->getSizeGraph();
}


void SpriteParticleSystemTemplateBase::setRandomSize(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSpriteSystemTemplate*>(m_impl)->setRandomSize(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float SpriteParticleSystemTemplateBase::getRandomSize() const
{
    return static_cast<IParticleSpriteSystemTemplate*>(m_impl)->getRandomSize();
}


void SpriteParticleSystemTemplateBase::setRandomIntensity(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSpriteSystemTemplate*>(m_impl)->setRandomIntensity(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float SpriteParticleSystemTemplateBase::getRandomIntensity() const
{
    return static_cast<IParticleSpriteSystemTemplate*>(m_impl)->getRandomIntensity();
}


void SpriteParticleSystemTemplateBase::setAlphaCull(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSpriteSystemTemplate*>(m_impl)->setAlphaCull(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float SpriteParticleSystemTemplateBase::getAlphaCull() const
{
    return static_cast<IParticleSpriteSystemTemplate*>(m_impl)->getAlphaCull();
}


void SpriteParticleSystemTemplate::setRotationSpeed(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setRotationSpeed(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float SpriteParticleSystemTemplate::getRotationSpeed() const
{
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getRotationSpeed();
}


void SpriteParticleSystemTemplate::setRandomRotationSpeed(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setRandomRotationSpeed(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float SpriteParticleSystemTemplate::getRandomRotationSpeed() const
{
    if (!m_impl)
        return 0.0f;
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getRandomRotationSpeed();
}


void SpriteParticleSystemTemplate::setRandomRotation(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setRandomRotation(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float SpriteParticleSystemTemplate::getRandomRotation() const
{
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getRandomRotation();
}


void SpriteParticleSystemTemplate::setPivotOffset(const Vec2& value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setPivotOffset(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
Vec2 SpriteParticleSystemTemplate::getPivotOffset() const
{
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getPivotOffset();
}


void SpriteParticleSystemTemplate::setSelfShadowIntensity(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setSelfShadowIntensity(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float SpriteParticleSystemTemplate::getSelfShadowIntensity() const
{
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getSelfShadowIntensity();
}


void SpriteParticleSystemTemplate::setAnimationEnable(bool value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setAnimationEnable(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
bool SpriteParticleSystemTemplate::getAnimationEnable() const
{
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getAnimationEnable();
}


void SpriteParticleSystemTemplate::setAnimationPlayOnce(bool value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setAnimationPlayOnce(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
bool SpriteParticleSystemTemplate::getAnimationPlayOnce() const
{
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getAnimationPlayOnce();
}


void SpriteParticleSystemTemplate::setAnimationRandomizedStartFrame(bool value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setAnimationRandomizedStartFrame(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
bool SpriteParticleSystemTemplate::getAnimationRandomizedStartFrame() const
{
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getAnimationRandomizedStartFrame();
}


void SpriteParticleSystemTemplate::setAnimationSpeed(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setAnimationSpeed(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float SpriteParticleSystemTemplate::getAnimationSpeed() const
{
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getAnimationSpeed();
}


void SpriteParticleSystemTemplate::setAnimationFrameCount(unsigned int value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setAnimationFrameCount(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
unsigned int SpriteParticleSystemTemplate::getAnimationFrameCount() const
{
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getAnimationFrameCount();
}


void SpriteParticleSystemTemplate::setAnimationFrameCountX(unsigned int value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setAnimationFrameCountX(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
unsigned int SpriteParticleSystemTemplate::getAnimationFrameCountX() const
{
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getAnimationFrameCountX();
}


void SpriteParticleSystemTemplate::setAnimationFrameWidthRelative(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setAnimationFrameWidthRelative(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}


void SpriteParticleSystemTemplate::setAnimationFrameHeightRelative(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setAnimationFrameHeightRelative(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}


void SpriteParticleSystemTemplate::setTextureName(const std::string& value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType0*>(m_impl)->setTextureName(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}


const std::string& SpriteParticleSystemTemplate::getTextureName() const
{
    if (!m_impl)
        return getEmptyString();
    return static_cast<IParticleSystemTemplateType0*>(m_impl)->getTextureName();
}


void NonScreenAlignedParticleSystemTemplate::setRotationSpeed(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setRotationSpeed(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float NonScreenAlignedParticleSystemTemplate::getRotationSpeed() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getRotationSpeed();
}


void NonScreenAlignedParticleSystemTemplate::setRandomRotationSpeed(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setRandomRotationSpeed(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float NonScreenAlignedParticleSystemTemplate::getRandomRotationSpeed() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getRandomRotationSpeed();
}


void NonScreenAlignedParticleSystemTemplate::setRandomRotation(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setRandomRotation(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float NonScreenAlignedParticleSystemTemplate::getRandomRotation() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getRandomRotation();
}


void NonScreenAlignedParticleSystemTemplate::setPivotOffset(const Vec2& value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setPivotOffset(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
Vec2 NonScreenAlignedParticleSystemTemplate::getPivotOffset() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getPivotOffset();
}


void NonScreenAlignedParticleSystemTemplate::setSelfShadowIntensity(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setSelfShadowIntensity(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float NonScreenAlignedParticleSystemTemplate::getSelfShadowIntensity() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getSelfShadowIntensity();
}


void NonScreenAlignedParticleSystemTemplate::setAnimationEnable(bool value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setAnimationEnable(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
bool NonScreenAlignedParticleSystemTemplate::getAnimationEnable() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getAnimationEnable();
}


void NonScreenAlignedParticleSystemTemplate::setAnimationPlayOnce(bool value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setAnimationPlayOnce(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
bool NonScreenAlignedParticleSystemTemplate::getAnimationPlayOnce() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getAnimationPlayOnce();
}


void NonScreenAlignedParticleSystemTemplate::setAnimationRandomizedStartFrame(bool value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setAnimationRandomizedStartFrame(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
bool NonScreenAlignedParticleSystemTemplate::getAnimationRandomizedStartFrame() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getAnimationRandomizedStartFrame();
}


void NonScreenAlignedParticleSystemTemplate::setAnimationSpeed(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setAnimationSpeed(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float NonScreenAlignedParticleSystemTemplate::getAnimationSpeed() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getAnimationSpeed();
}


void NonScreenAlignedParticleSystemTemplate::setAnimationFrameCount(unsigned int value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setAnimationFrameCount(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
unsigned int NonScreenAlignedParticleSystemTemplate::getAnimationFrameCount() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getAnimationFrameCount();
}


void NonScreenAlignedParticleSystemTemplate::setAnimationFrameCountX(unsigned int value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setAnimationFrameCountX(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
unsigned int NonScreenAlignedParticleSystemTemplate::getAnimationFrameCountX() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getAnimationFrameCountX();
}


void NonScreenAlignedParticleSystemTemplate::setAnimationFrameWidthRelative(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setAnimationFrameWidthRelative(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}


void NonScreenAlignedParticleSystemTemplate::setAnimationFrameHeightRelative(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setAnimationFrameHeightRelative(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}


void NonScreenAlignedParticleSystemTemplate::setTextureName(const std::string& value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType4*>(m_impl)->setTextureName(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}


const std::string& NonScreenAlignedParticleSystemTemplate::getTextureName() const
{
    return static_cast<IParticleSystemTemplateType4*>(m_impl)->getTextureName();
}


using geom::IParticleSystemTemplateType2;

// Trail tail: ELF 0x8214190..0x82147C0; renderer PE vtable 0x10220308 slots 114..145.
void TrailSystemTemplate::setTextureName(const std::string& value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setTextureName(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
const std::string& TrailSystemTemplate::getTextureName() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getTextureName();
}

void TrailSystemTemplate::setAnimationEnable(bool value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setAnimationEnable(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
bool TrailSystemTemplate::getAnimationEnable() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getAnimationEnable();
}

void TrailSystemTemplate::setAnimationSpeed(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setAnimationSpeed(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float TrailSystemTemplate::getAnimationSpeed() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getAnimationSpeed();
}


void TrailSystemTemplate::setAnimationFrameWidth(unsigned int value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setAnimationFrameWidth(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
unsigned int TrailSystemTemplate::getAnimationFrameWidth() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getAnimationFrameWidth();
}


void TrailSystemTemplate::setAnimationFrameHeight(unsigned int value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setAnimationFrameHeight(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
unsigned int TrailSystemTemplate::getAnimationFrameHeight() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getAnimationFrameHeight();
}

void TrailSystemTemplate::setAnimationFrameWidthRelative(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setAnimationFrameWidthRelative(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

void TrailSystemTemplate::setAnimationFrameHeightRelative(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setAnimationFrameHeightRelative(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}


void TrailSystemTemplate::setAnimationFrameCount(unsigned int value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setAnimationFrameCount(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
unsigned int TrailSystemTemplate::getAnimationFrameCount() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getAnimationFrameCount();
}

void TrailSystemTemplate::setAnimationFrameCountX(unsigned int value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setAnimationFrameCountX(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
unsigned int TrailSystemTemplate::getAnimationFrameCountX() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getAnimationFrameCountX();
}

void TrailSystemTemplate::setFadeInFactor(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setFadeInFactor(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float TrailSystemTemplate::getFadeInFactor() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getFadeInFactor();
}

void TrailSystemTemplate::setFadeOutFactor(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setFadeOutFactor(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float TrailSystemTemplate::getFadeOutFactor() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getFadeOutFactor();
}

void TrailSystemTemplate::setTextureTileFactor(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setTextureTileFactor(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float TrailSystemTemplate::getTextureTileFactor() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getTextureTileFactor();
}

void TrailSystemTemplate::setTextureOffsetVelocity(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setTextureOffsetVelocity(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float TrailSystemTemplate::getTextureOffsetVelocity() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getTextureOffsetVelocity();
}

void TrailSystemTemplate::setNumPlanes(unsigned int value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setNumPlanes(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
unsigned int TrailSystemTemplate::getNumPlanes() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getNumPlanes();
}

void TrailSystemTemplate::setMinTrailVelocity(float value)
{
    if (m_impl)
    {
        static_cast<IParticleSystemTemplateType2*>(m_impl)->setMinTrailVelocity(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float TrailSystemTemplate::getMinTrailVelocity() const
{
    return static_cast<IParticleSystemTemplateType2*>(m_impl)->getMinTrailVelocity();
}

// Gas-cloud forwarders: ELF 0x8210270..0x8210560 / client PE 0x64F300.. (impl ELF +0x130..+0x164,
// PE +0x12C..+0x160; refresh ELF +0xC / PE +0x8). Every instruction read in both images.

// ELF 0x8210270 / 0x82102C0, client PE 0x64F300 / 0x64F340.
void ParticleSystemTemplateBase::setGasCloudType(geom::GasCloudType value)
{
    if (m_impl)
    {
        m_impl->setGasCloudType(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
geom::GasCloudType ParticleSystemTemplateBase::getGasCloudType() const
{
    return m_impl->getGasCloudType();
}

// ELF 0x82102E0 / 0x8210330, client PE 0x64F350 / 0x64F390.
void ParticleSystemTemplateBase::setBlurryVisionTime(float value)
{
    if (m_impl)
    {
        m_impl->setBlurryVisionTime(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getBlurryVisionTime() const
{
    return m_impl->getBlurryVisionTime();
}

// ELF 0x8210350 / 0x82103A0, client PE 0x64F3A0 / 0x64F3E0.
void ParticleSystemTemplateBase::setBlurryVisionHoldTime(float value)
{
    if (m_impl)
    {
        m_impl->setBlurryVisionHoldTime(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getBlurryVisionHoldTime() const
{
    return m_impl->getBlurryVisionHoldTime();
}

// ELF 0x82103C0 / 0x8210410, client PE 0x64F3F0 / 0x64F430.
void ParticleSystemTemplateBase::setMinGasCloudRadius(float value)
{
    if (m_impl)
    {
        m_impl->setMinGasCloudRadius(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getMinGasCloudRadius() const
{
    return m_impl->getMinGasCloudRadius();
}

// ELF 0x8210430 / 0x8210480, client PE 0x64F440 / 0x64F480.
void ParticleSystemTemplateBase::setMaxGasCloudRadius(float value)
{
    if (m_impl)
    {
        m_impl->setMaxGasCloudRadius(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getMaxGasCloudRadius() const
{
    return m_impl->getMaxGasCloudRadius();
}

// ELF 0x82104A0 / 0x82104F0, client PE 0x64F490 / 0x64F4D0.
void ParticleSystemTemplateBase::setGasCloudRadiusTime(float value)
{
    if (m_impl)
    {
        m_impl->setGasCloudRadiusTime(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getGasCloudRadiusTime() const
{
    return m_impl->getGasCloudRadiusTime();
}

// ELF 0x8210510 / 0x8210560, client PE 0x64F4E0 / 0x64F520.
void ParticleSystemTemplateBase::setGasCloudDamage(float value)
{
    if (m_impl)
    {
        m_impl->setGasCloudDamage(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}
float ParticleSystemTemplateBase::getGasCloudDamage() const
{
    return m_impl->getGasCloudDamage();
}

} } } // namespace dice::hfe::world
