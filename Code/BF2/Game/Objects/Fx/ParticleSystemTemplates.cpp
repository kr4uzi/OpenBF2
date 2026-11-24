// Particle-system template creation and configuration.

#include "ParticleSystemTemplates.h"

#include "../../../Core/ClassManager.h"            // classManager->getSingleton("ParticleSystemManager")
#include "../../../Core/Ids.h"
#include "../../../Core/StringUtil.h"              // compareIgnoreCase - the name-table parsers
#include "../../../Geom/ParticleSystem/ParticleSystemManager.h"
#include "../IObject.h"                            // createObject's (always null) product type
#if !DEDICATED
#include "../../../Geom/GeometryTemplateManager.h"
#include "../../../Geom/IGeometryTemplate.h"
#endif

#include <cstdio>                                  // sscanf - the GasCloudType reader
#include <istream>
#include <ostream>
#include <sstream>

namespace dice {
namespace hfe {

// hfe scope, NOT world - an extern declared inside world would mint a wrong-scope twin of the
// global (the SimpleObject.cpp lesson). Core/Globals.cpp:137 defines it.
extern ClassManager* classManager;                   // ds:0x8AC85A0
#if !DEDICATED
extern IBase* g_geometryTemplateManager;              // BF2.exe ds:0x009A5578
#endif

namespace world {

// THE NAME tables - .rodata at 0x8988844..0x8988870 and 0x8988924, adjacent in the shipped image.
// ParticleDrawOrderNames carries Mesh's adjacency overrun as an explicit third entry: that
// leaf's shipped setDrawOrder loop probes indices 0..2 over what the linker laid out as a two-entry
// table followed immediately by ParticleEmitPrioNames, so Mesh accepts "Low" as draw order 2.
// Sprite's distinct body probes only indices 0..1. Making the adjacent entry explicit reproduces
// the Mesh behavior without retaining an out-of-bounds read.
const char* const SpriteParticleEmitterTypeNames[4] = { "Standard", "RadialDirection", "Radial", "Box" };
const char* const ParticleBlendModeNames[2]         = { "AlphaBlend", "Additive" };
const char* const ParticleDrawOrderNames[3]         = { "Front", "Back", "Low" };
const char* const ParticleEmitPrioNames[3]          = { "Low", "Medium", "High" };
const char* const SpriteParticleSystemTypeNames[3]  = { "Sorted", "PointSprite", "Additive" };

// operator<< (geom::GasCloudType) - ELF 0x8211340, 58 B. Every value other than 1 writes "None".
// The literals are the tails of two merged .rodata strings ("ABNone"+2, "S_TearGas"+2).
std::ostream& operator<<(std::ostream& stream, geom::GasCloudType type)
{
    if (static_cast<int>(type) == 0 || static_cast<int>(type) != 1)
        stream << "None";
    else
        stream << "TearGas";
    return stream;
}

// operator>> (geom::GasCloudType) - ELF 0x8211380, 883 B. A token of at most three characters that
// scans as an integer is first spelled through operator<<; the raw token and that spelling are then
// each compared (case-sensitive) with "None" before "TearGas". Anything else selects 0 ("None").
std::istream& operator>>(std::istream& stream, geom::GasCloudType& type)
{
    std::string token;
    stream >> token;
    std::ostringstream spelled;
    if (token.length() <= 3)
    {
        int numeric;
        if (std::sscanf(token.c_str(), "%d", &numeric) == 1)
            spelled << static_cast<geom::GasCloudType>(numeric);
    }
    if (token.compare("None") != 0 && spelled.str().compare("None") != 0
        && (token.compare("TearGas") == 0 || spelled.str().compare("TearGas") == 0))
        type = static_cast<geom::GasCloudType>(1);
    else
        type = static_cast<geom::GasCloudType>(0);
    return stream;
}

// ParticleSystemTemplateBase - the root. Constructor 0x820DF10 (C2) / 0x820E000 (C1), 239 B, read
// at the instruction level: ObjectTemplate(); both vptrs; the two strings default-constructed
// (+0x34/+0x38); m_impl = 0; the two bytes at +0x40/+0x41 = true; then the two self-calls that
// seed the defaults - setEmitPrio("High") (which parses and, with no impl, drops the result) and
// setEmitterType("Standard") (which stores enum 0 and the name).
// m_emitterType (+0x30) is deliberately not in the init list - the ELF's constructor never
// stores it directly either; setEmitterType's tail is its one deterministic writer.
ParticleSystemTemplateBase::ParticleSystemTemplateBase()
    : m_impl(0)
    , m_byte40(true)
    , m_byte41(true)
{
    setEmitPrio(std::string("High"));
    setEmitterType(std::string("Standard"));
}

// D2/D1 0x820E0F0/0x820E170, 126 B: revector, the two strings destroy (their own destructors
// here), ~ObjectTemplate. Nothing manual survives the translation.
ParticleSystemTemplateBase::~ParticleSystemTemplateBase()
{
}

// setEmitterType - 0x820F300, 131 B. Parse the token against SpriteParticleEmitterTypeNames[0..3]
// (case-insensitive); ON match store the index (+0x30) and copy the token (+0x34); NO match skips
// both stores. THE impl push runs either WAY and pushes the stored enum - so an unknown token
// re-pushes the previous value (render-template ELF +0x44 / PE +0x40, no derived-state refresh).
void ParticleSystemTemplateBase::setEmitterType(const std::string& name)
{
    for (int i = 0; i <= 3; ++i)
    {
        if (compareIgnoreCase(name.c_str(), SpriteParticleEmitterTypeNames[i]) == 0)
        {
            m_emitterType = i;                       // +0x30
            m_emitterTypeName = name;                // +0x34
            break;
        }
    }
    if (m_impl)
        m_impl->setEmitterType(m_emitterType);       // ELF +0x44 / PE +0x40, no derived-state call
}

const std::string& ParticleSystemTemplateBase::getEmitterType() const
{
    return m_emitterTypeName;
}

int ParticleSystemTemplateBase::getEmitterTypeEnum() const
{
    return m_emitterType;
}

// setEmitPrio / setLodDistance / setQualityLevel - 0x820F3B0 / 0x820F460 / 0x820F510, 91 B each.
// All three parse against ParticleEmitPrioNames[0..2] (including LodDistance - the "distance"
// is spelled Low/Medium/High, as shipped), store nothing locally, and on a match push the loop
// index to the impl (vslots +0x14 / +0x1C / +0x24, no dirty poke). An unknown token returns
// without touching the impl. Dedicated has no render manager; a client forwards through the
// evidence-scoped render-template view.
void ParticleSystemTemplateBase::setEmitPrio(const std::string& name)
{
    for (int i = 0; i <= 2; ++i)
    {
        if (compareIgnoreCase(name.c_str(), ParticleEmitPrioNames[i]) == 0)
        {
            if (m_impl)
                m_impl->setEmitPrio(i);                     // ELF +0x14 / PE +0x10
            return;
        }
    }
}

void ParticleSystemTemplateBase::setLodDistance(const std::string& name)
{
    for (int i = 0; i <= 2; ++i)
    {
        if (compareIgnoreCase(name.c_str(), ParticleEmitPrioNames[i]) == 0)
        {
            if (m_impl)
                m_impl->setLodDistance(i);                  // ELF +0x1C / PE +0x18
            return;
        }
    }
}

void ParticleSystemTemplateBase::setQualityLevel(const std::string& name)
{
    for (int i = 0; i <= 2; ++i)
    {
        if (compareIgnoreCase(name.c_str(), ParticleEmitPrioNames[i]) == 0)
        {
            if (m_impl)
                m_impl->setQualityLevel(i);                 // ELF +0x24 / PE +0x20
            return;
        }
    }
}

// . The Vec3 is passed by value into this world wrapper, copied to a local,
// forwarded by value to the render template (ELF +0x2C / PE +0x28), then the render template's
// derived state is recomputed through ELF +0x0C / PE +0x08.
void ParticleSystemTemplateBase::setEmitRadius(Vec3 radius)
{
    if (m_impl)
    {
        m_impl->setEmitRadius(radius);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

// Forwards particle template properties to the renderer implementation.
// Getters require m_impl; dedicated templates normally have no renderer handle. Keep each setter's
// specific refresh behavior and repeated member reads.

// / 0x82105D0, 67 B / 30 B. Impl ELF +0x118 / +0x11C, PE +0x114 / +0x118.
void ParticleSystemTemplateBase::setIsLocalSystem(bool value)
{
    if (m_impl)
    {
        m_impl->setIsLocalSystem(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

bool ParticleSystemTemplateBase::getIsLocalSystem() const
{
    return m_impl->getIsLocalSystem();
}

// / 0x820F5F0, 37 B / 24 B. Impl ELF +0x34 / +0x38, PE +0x30 / +0x34.
// NO derived-state poke - one of the four the header names, confirmed by the read: the shipped
// body issues a single call and returns.
void ParticleSystemTemplateBase::setRadialDirection(float value)
{
    if (m_impl)
        m_impl->setRadialDirection(value);
}

float ParticleSystemTemplateBase::getRadialDirection() const
{
    return m_impl->getRadialDirection();
}

// / 0x820F640, 37 B / 24 B. Impl ELF +0x3C / +0x40, PE +0x38 / +0x3C.
// NO derived-state poke either. And note the impl slot +0x3C is numerically the same as this
// class's OWN m_impl member offset - a coincidence of two unrelated layouts, not a relationship.
void ParticleSystemTemplateBase::setRandomRadialDirection(float value)
{
    if (m_impl)
        m_impl->setRandomRadialDirection(value);
}

float ParticleSystemTemplateBase::getRandomRadialDirection() const
{
    return m_impl->getRandomRadialDirection();
}

// / 0x8210130, 64 B / 24 B. Impl ELF +0x48 / +0x4C, PE +0x44 / +0x48.
void ParticleSystemTemplateBase::setEmitRangeX(const RandomDistribution& value)
{
    if (m_impl)
    {
        m_impl->setEmitRangeX(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

const RandomDistribution& ParticleSystemTemplateBase::getEmitRangeX() const
{
    return m_impl->getEmitRangeX();
}

// / 0x8210190, 64 B / 24 B. Impl ELF +0x50 / +0x54, PE +0x4C / +0x50.
void ParticleSystemTemplateBase::setEmitRangeY(const RandomDistribution& value)
{
    if (m_impl)
    {
        m_impl->setEmitRangeY(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

const RandomDistribution& ParticleSystemTemplateBase::getEmitRangeY() const
{
    return m_impl->getEmitRangeY();
}

// / 0x82101F0, 64 B / 24 B. Impl ELF +0x58 / +0x5C, PE +0x54 / +0x58.
void ParticleSystemTemplateBase::setEmitRangeZ(const RandomDistribution& value)
{
    if (m_impl)
    {
        m_impl->setEmitRangeZ(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

const RandomDistribution& ParticleSystemTemplateBase::getEmitRangeZ() const
{
    return m_impl->getEmitRangeZ();
}

// SpriteParticleSystemTemplateBase - 40-byte constructor: base + the two vptr stores, nothing
// else. The +0x44 dword is left exactly as the ELF leaves it: unwritten.
SpriteParticleSystemTemplateBase::SpriteParticleSystemTemplateBase()
{
}

// Sprite-branch properties narrow m_impl to IParticleSpriteSystemTemplate.
// Getters require a valid implementation; setters recheck it before refreshing derived state.
void SpriteParticleSystemTemplateBase::setDirectionalScale(float value)
{
    if (m_impl)
    {
        static_cast<geom::IParticleSpriteSystemTemplate*>(m_impl)->setDirectionalScale(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

float SpriteParticleSystemTemplateBase::getDirectionalScale() const
{
    return static_cast<geom::IParticleSpriteSystemTemplate*>(m_impl)->getDirectionalScale();
}

void SpriteParticleSystemTemplateBase::setIsStaticSystem(bool value)
{
    if (m_impl)
    {
        static_cast<geom::IParticleSpriteSystemTemplate*>(m_impl)->setIsStaticSystem(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

bool SpriteParticleSystemTemplateBase::getIsStaticSystem() const
{
    return static_cast<geom::IParticleSpriteSystemTemplate*>(m_impl)->getIsStaticSystem();
}

SpriteParticleSystemTemplateBase::~SpriteParticleSystemTemplateBase()
{
}

// Particle template lifecycle: initialization creates the renderer descriptor and pushes virtual defaults.
// Destruction re-fetches the manager and returns the descriptor only when both exist.
// Dedicated creation marks precached and returns null; mesh creation also guards its one-time precache.

#define BF2_PARTICLE_LEAF_COMMON(cls, typeCode, ctorAddr)                                          \
cls::~cls()                                                                                        \
{                                                                                                  \
    geom::IParticleSystemManager* const manager = geom::asParticleSystemManager(                    \
        classManager->getSingleton(std::string("ParticleSystemManager")));                         \
    if (manager && m_impl)                                                                         \
        manager->removeParticleSystem(m_impl);                                                   \
}                                                                                                  \
void cls::addRef()  { ++m_refCount; }                                                              \
int  cls::getRef()  { return m_refCount; }                                                         \
int  cls::release()                                                                                \
{                                                                                                  \
    const int r = --m_refCount;                                                                    \
    if (r == 0)                                                                                    \
        delete this;                                                                               \
    return r;                                                                                      \
}                                                                                                  \
void* cls::queryInterface(unsigned int iid) const                                                  \
{                                                                                                  \
    cls* self = const_cast<cls*>(this);                                                            \
    if (iid == IID_IBase || iid == IID_IObjectTemplate)                                            \
        return static_cast<IObjectTemplate*>(self);                                                \
    if (iid == io::IID_IConsoleSaveable)                                                           \
        return static_cast<io::IConsoleSaveable*>(self);                                           \
    return 0;                                                                                      \
}                                                                                                  \
unsigned int cls::getClassID() const { return CID_##cls; }                                         \
void* cls::queryImplementation(unsigned int classId)                                               \
{                                                                                                  \
    if (classId == CID_##cls)                                                                      \
        return this;                                                                               \
    if (classId == CID_ObjectTemplate)                                                             \
        return this;                                                                               \
    return 0;                                                                                      \
}

// SpriteParticleSystemTemplate (type code 0) - ctor 0x82122F0
SpriteParticleSystemTemplate::SpriteParticleSystemTemplate()
    : m_refCount(1)                                  // +0x48
{
}
BF2_PARTICLE_LEAF_COMMON(SpriteParticleSystemTemplate, 0, 0x82122F0)

// . The manager lookup/create is followed by a separately guarded default replay;
// keep that split, including the final second derived-state refresh.
void SpriteParticleSystemTemplate::templateInitialize()
{
    geom::IParticleSystemManager* const manager = geom::asParticleSystemManager(
        classManager->getSingleton(std::string("ParticleSystemManager")));
    if (manager)
        m_impl = manager->createParticleSystem(0, getName());

    if (m_impl)
    {
        setParticleType(std::string("Sorted"));
        setEmitterType(std::string("Standard"));
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

IObject* SpriteParticleSystemTemplate::createObject() const
{
    const_cast<SpriteParticleSystemTemplate*>(this)->updateFlags(0x20, 0);
    return 0;
}

// setParticleType - 0x8212FE0, 151 B: parse against SpriteParticleSystemTypeNames[0..2]; on a
// match keep the token (+0x4C); the impl push (ELF +0x1CC / PE +0x1C8, followed by derived-state
// refresh at ELF +0x0C / PE +0x08) runs either WAY and pushes 0 for an unknown token, as shipped.
void SpriteParticleSystemTemplate::setParticleType(const std::string& name)
{
    int particleType = 0;
    for (int i = 0; i <= 2; ++i)
    {
        if (compareIgnoreCase(name.c_str(), SpriteParticleSystemTypeNames[i]) == 0)
        {
            particleType = i;
            m_particleTypeName = name;               // +0x4C
            break;
        }
    }
    if (m_impl)
    {
        static_cast<geom::IParticleSystemTemplateType0*>(m_impl)->setParticleType(particleType);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

// setDrawOrder - ELF 0x8213100, 126 B (vslot +0x138). Sprite probes only the two authored
// ParticleDrawOrderNames entries; unlike Mesh it does not reach the adjacent "Low". No local
// store; the impl arm (ELF +0x1D4 / PE +0x1D0 plus refresh) runs either way with zero on a miss.
void SpriteParticleSystemTemplate::setDrawOrder(const std::string& name)
{
    int drawOrder = 0;
    for (int i = 0; i <= 1; ++i)
    {
        if (compareIgnoreCase(name.c_str(), ParticleDrawOrderNames[i]) == 0)
        {
            drawOrder = i;
            break;
        }
    }
    if (m_impl)
    {
        static_cast<geom::IParticleSystemTemplateType0*>(m_impl)->setDrawOrder(drawOrder);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

// MeshParticleSystemTemplate (type code 1) - ctor 0x82038E0
MeshParticleSystemTemplate::MeshParticleSystemTemplate()
    : m_refCount(1)                                  // +0x48
{
}
BF2_PARTICLE_LEAF_COMMON(MeshParticleSystemTemplate, 1, 0x82038E0)

// . Unlike Sprite, every operation remains inside the manager-present arm and the
// final refresh is unguarded after create/default replay.
void MeshParticleSystemTemplate::templateInitialize()
{
    geom::IParticleSystemManager* const manager = geom::asParticleSystemManager(
        classManager->getSingleton(std::string("ParticleSystemManager")));
    if (manager)
    {
        m_impl = manager->createParticleSystem(1, getName());
        setEmitterType(std::string("Standard"));
        setBlendMode(std::string("AlphaBlend"));
        m_impl->refreshDerivedState();
    }
}

IObject* MeshParticleSystemTemplate::createObject() const
{
    if ((getFlags() & 0x20) == 0)
    {
        MeshParticleSystemTemplate* self = const_cast<MeshParticleSystemTemplate*>(this);
        self->updateFlags(0x20, 0);
        preCache();                                  // through the virtual, as shipped
    }
    return 0;
}

void MeshParticleSystemTemplate::preCache() const
{
    if ((getFlags() & 0x20) == 0)
    {
        ObjectTemplate::preCache();
#if !DEDICATED
        // BF2.exe 0x0064CDB3..0x0064CDFD warms geometry with a temporary instance.
        // Preserve both virtual name reads and all three resource null guards.
        if (g_geometryTemplateManager && !getGeometry().empty())
        {
            geom::IGeometryTemplate* const geometryTemplate =
                static_cast<geom::IGeometryTemplateManager*>(g_geometryTemplateManager)
                    ->getTemplate(getGeometry());
            if (geometryTemplate)
            {
                IBase* const instance = static_cast<IBase*>(geometryTemplate->createInstance());
                if (instance)
                    instance->release();
            }
        }
#endif
    }
}

// setBlendMode - 0x8203D50, 151 B: ParticleBlendModeNames[0..1]; a match keeps the token (+0x4C);
// the impl arm (ELF +0x1E4 / PE +0x1E0 plus derived-state refresh) runs either way with 0 for an
// unknown token.
void MeshParticleSystemTemplate::setBlendMode(const std::string& name)
{
    int blendMode = 0;
    for (int i = 0; i <= 1; ++i)
    {
        if (compareIgnoreCase(name.c_str(), ParticleBlendModeNames[i]) == 0)
        {
            blendMode = i;
            m_blendModeName = name;                  // +0x4C
            break;
        }
    }
    if (m_impl)
    {
        static_cast<geom::IParticleSystemTemplateType1*>(m_impl)->setBlendMode(blendMode);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

// setDrawOrder - 0x8204560, 126 B: ParticleDrawOrderNames[0..2] (index 2 is the adjacency entry
// "Low" - see the table note), NO local store; the impl arm (vslot +0x1D4 + poke) runs either way.
void MeshParticleSystemTemplate::setDrawOrder(const std::string& name)
{
    int drawOrder = 0;
    for (int i = 0; i <= 2; ++i)
    {
        if (compareIgnoreCase(name.c_str(), ParticleDrawOrderNames[i]) == 0)
        {
            drawOrder = i;
            break;
        }
    }
    if (m_impl)
    {
        static_cast<geom::IParticleSystemTemplateType1*>(m_impl)->setDrawOrder(drawOrder);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

// NonScreenAlignedParticleSystemTemplate (type code 4) - ctor 0x8204D30
NonScreenAlignedParticleSystemTemplate::NonScreenAlignedParticleSystemTemplate()
    : m_refCount(1)                                  // +0x48
{
}
BF2_PARTICLE_LEAF_COMMON(NonScreenAlignedParticleSystemTemplate, 4, 0x8204D30)

// manager lookup and create only; this leaf replays no defaults here.
void NonScreenAlignedParticleSystemTemplate::templateInitialize()
{
    geom::IParticleSystemManager* const manager = geom::asParticleSystemManager(
        classManager->getSingleton(std::string("ParticleSystemManager")));
    if (manager)
        m_impl = manager->createParticleSystem(4, getName());
}

IObject* NonScreenAlignedParticleSystemTemplate::createObject() const
{
    const_cast<NonScreenAlignedParticleSystemTemplate*>(this)->updateFlags(0x20, 0);
    return 0;
}

// TrailSystemTemplate (type code 2) - ctor 0x8213E10
TrailSystemTemplate::TrailSystemTemplate()
    : m_refCount(1)                                  // +0x48
{
}
BF2_PARTICLE_LEAF_COMMON(TrailSystemTemplate, 2, 0x8213E10)

// . setEmitRadius itself refreshes the render template, and the body performs one
// additional guarded refresh afterwards; the duplicate call is shipped and preserved.
void TrailSystemTemplate::templateInitialize()
{
    geom::IParticleSystemManager* const manager = geom::asParticleSystemManager(
        classManager->getSingleton(std::string("ParticleSystemManager")));
    if (manager)
    {
        m_impl = manager->createParticleSystem(2, getName());
        setEmitterType(std::string("Standard"));
        Vec3 radius = { 1.0f, 1.0f, 0.0f };
        setEmitRadius(radius);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

IObject* TrailSystemTemplate::createObject() const
{
    const_cast<TrailSystemTemplate*>(this)->updateFlags(0x20, 0);
    return 0;
}

// ParticleSystemEmitterTemplate (type code 3) - ctor 0x820D190
ParticleSystemEmitterTemplate::ParticleSystemEmitterTemplate()
    : m_refCount(1)                                  // +0x44 the direct-branch offset
{
}
BF2_PARTICLE_LEAF_COMMON(ParticleSystemEmitterTemplate, 3, 0x820D190)

// create, replay Standard, then unconditionally refresh within manager-present.
void ParticleSystemEmitterTemplate::templateInitialize()
{
    geom::IParticleSystemManager* const manager = geom::asParticleSystemManager(
        classManager->getSingleton(std::string("ParticleSystemManager")));
    if (manager)
    {
        m_impl = manager->createParticleSystem(3, getName());
        setEmitterType(std::string("Standard"));
        m_impl->refreshDerivedState();
    }
}

IObject* ParticleSystemEmitterTemplate::createObject() const
{
    const_cast<ParticleSystemEmitterTemplate*>(this)->updateFlags(0x20, 0);
    return 0;
}

#undef BF2_PARTICLE_LEAF_COMMON

// Renderer-forwarding leaf accessors retain their individual null policies.
// Enum getters default to zero; sprite particle type returns through a const-int reference.
// Mesh geometry defaults to the empty string, while emitter template-name access is unguarded.
// Preserve setter-specific refresh calls; geometry/template-name setters skip them.

// MeshParticleSystemTemplate
// Impl is the type-1 descriptor. ELF 0x8204660 / 0x8204690, 40 B / 39 B; impl ELF +0x1CC / +0x1D0.
void MeshParticleSystemTemplate::setGeometry(const std::string& name)
{
    if (m_impl)
        static_cast<geom::IParticleSystemTemplateType1*>(m_impl)->setGeometry(name);
}

const std::string& MeshParticleSystemTemplate::getGeometry() const
{
    if (!m_impl)
        return getEmptyString();                  // the family's only null fallback - shipped
    return static_cast<geom::IParticleSystemTemplateType1*>(m_impl)->getGeometry();
}

// setWriteZ / getWriteZ - ELF 0x82048A0 / 0x82048F0, 67 B / 30 B, every instruction
// (G1). The impl-forward / impl-pull pair of this family: impl ELF +0x1DC (the byte, `movzx`) then
// the derived-state refresh (+0x0C) behind a RE-read of the impl, and ELF +0x1E0 for the getter.
// The getter is unguarded, as shipped. The mesh-only `writeZ` console binding (ConsoleClass311)
// is their one caller.
void MeshParticleSystemTemplate::setWriteZ(bool value)
{
    if (m_impl)
    {
        static_cast<geom::IParticleSystemTemplateType1*>(m_impl)->setWriteZ(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

bool MeshParticleSystemTemplate::getWriteZ() const
{
    return static_cast<geom::IParticleSystemTemplateType1*>(m_impl)->getWriteZ();
}

// / 0x8204630, 35 B each; impl ELF +0x1E8 / +0x1D8. Guarded, zero on a null impl.
int MeshParticleSystemTemplate::getParticleBlendModeEnum() const
{
    if (!m_impl)
        return 0;
    return static_cast<geom::IParticleSystemTemplateType1*>(m_impl)->getBlendMode();
}

int MeshParticleSystemTemplate::getParticleDrawOrderEnum() const
{
    if (!m_impl)
        return 0;
    return static_cast<geom::IParticleSystemTemplateType1*>(m_impl)->getDrawOrder();
}

// The rotation trio - ELF 0x82046C0/0x8204730, 0x8204760/0x82047D0, 0x8204800/0x8204870.
// setters (guarded + poke), 47 B getters (unguarded, sret). Impl ELF +0x1EC..+0x200.
#define BF2_PARTICLE_ROT_PAIR(CLS, IFACE, NAME)                                                   \
    void CLS::set##NAME(Vec3 value)                                                                \
    {                                                                                              \
        if (m_impl)                                                                                \
        {                                                                                          \
            static_cast<geom::IFACE*>(m_impl)->set##NAME(value);                                   \
            if (m_impl)                                                                            \
                m_impl->refreshDerivedState();                                                     \
        }                                                                                          \
    }                                                                                              \
    Vec3 CLS::get##NAME() const                                                                    \
    {                                                                                              \
        return static_cast<geom::IFACE*>(m_impl)->get##NAME();                                     \
    }

BF2_PARTICLE_ROT_PAIR(MeshParticleSystemTemplate, IParticleSystemTemplateType1, RandomRotation)
BF2_PARTICLE_ROT_PAIR(MeshParticleSystemTemplate, IParticleSystemTemplateType1, RandomRotationSpeed)
BF2_PARTICLE_ROT_PAIR(MeshParticleSystemTemplate, IParticleSystemTemplateType1, RotationSpeed)

// SpriteParticleSystemTemplate
// (37 B) / 0x82131D0 (35 B); impl ELF +0x1D0 / +0x1D8. Both guarded, zero on null.
// The two extra bytes in the first are its `mov eax, [eax]`: its slot returns `const int&`.
int SpriteParticleSystemTemplate::getParticleTypeEnum() const
{
    if (!m_impl)
        return 0;
    return static_cast<geom::IParticleSystemTemplateType0*>(m_impl)->getParticleTypeRef();
}

int SpriteParticleSystemTemplate::getParticleDrawOrderEnum() const
{
    if (!m_impl)
        return 0;
    return static_cast<geom::IParticleSystemTemplateType0*>(m_impl)->getDrawOrder();
}

// BF2.exe 0x64C200/0x64C220; ELF 0x82138F0/0x8213920.
// Setters tolerate an absent renderer descriptor; getters retain the shipped unguarded dispatch.
void SpriteParticleSystemTemplate::setWindSpeedAgeDistance(float value)
{
    if (m_impl)
        static_cast<geom::IParticleSystemTemplateType0*>(m_impl)->setWindSpeedAgeDistance(value);
}
float SpriteParticleSystemTemplate::getWindSpeedAgeDistance() const
{
    return static_cast<geom::IParticleSystemTemplateType0*>(m_impl)->getWindSpeedAgeDistance();
}

// BF2.exe 0x64C230/0x64C250; ELF 0x8213940/0x8213970.
void SpriteParticleSystemTemplate::setWindAgingFactor(float value)
{
    if (m_impl)
        static_cast<geom::IParticleSystemTemplateType0*>(m_impl)->setWindAgingFactor(value);
}
float SpriteParticleSystemTemplate::getWindAgingFactor() const
{
    return static_cast<geom::IParticleSystemTemplateType0*>(m_impl)->getWindAgingFactor();
}

// ParticleSystemEmitterTemplate
// Impl is the type-3 descriptor - which derives from the base interface, not the sprite one, the
// same way this class does. ELF 0x820DAE0 (40 B) / 0x820DB10 (27 B); impl ELF +0x168 / +0x16C.
// The setter has no poke; the getter has no guard.
void ParticleSystemEmitterTemplate::setParticleSystemTemplate(const std::string& name)
{
    if (m_impl)
        static_cast<geom::IParticleSystemEmitterTemplate*>(m_impl)->setParticleSystemTemplate(name);
}

const std::string& ParticleSystemEmitterTemplate::getParticleSystemTemplate()
{
    return static_cast<geom::IParticleSystemEmitterTemplate*>(m_impl)->getParticleSystemTemplate();
}

BF2_PARTICLE_ROT_PAIR(ParticleSystemEmitterTemplate, IParticleSystemEmitterTemplate, RandomRotation)
BF2_PARTICLE_ROT_PAIR(ParticleSystemEmitterTemplate, IParticleSystemEmitterTemplate, RandomRotationSpeed)
BF2_PARTICLE_ROT_PAIR(ParticleSystemEmitterTemplate, IParticleSystemEmitterTemplate, RotationSpeed)

#undef BF2_PARTICLE_ROT_PAIR

// / 0x820DC20, 67 B / 27 B; impl ELF +0x188 / +0x18C. Guarded setter with the poke,
// unguarded getter - the plain shape the family note describes.
void ParticleSystemEmitterTemplate::setSpeedFromParticle(float value)
{
    if (m_impl)
    {
        static_cast<geom::IParticleSystemEmitterTemplate*>(m_impl)->setSpeedFromParticle(value);
        if (m_impl)
            m_impl->refreshDerivedState();
    }
}

float ParticleSystemEmitterTemplate::getSpeedFromParticle() const
{
    return static_cast<geom::IParticleSystemEmitterTemplate*>(m_impl)->getSpeedFromParticle();
}

// Returns an owned string from the name table indexed by this object's virtual enum getter.
// Null implementations inherit enum zero. Keep by-value returns and the shared three-entry draw-order table;
// mesh setter probing retains its original adjacency overrun.
std::string MeshParticleSystemTemplate::getBlendMode() const
{
    return ParticleBlendModeNames[getParticleBlendModeEnum()];
}

std::string MeshParticleSystemTemplate::getDrawOrder() const
{
    return ParticleDrawOrderNames[getParticleDrawOrderEnum()];
}

std::string SpriteParticleSystemTemplate::getParticleType() const
{
    return SpriteParticleSystemTypeNames[getParticleTypeEnum()];
}

std::string SpriteParticleSystemTemplate::getDrawOrder() const
{
    return ParticleDrawOrderNames[getParticleDrawOrderEnum()];
}

} // namespace world
} // namespace hfe
} // namespace dice
