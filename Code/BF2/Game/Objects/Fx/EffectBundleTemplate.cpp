// Path:     [INFERRED] - beside its header. Contents: recovered from bf2.i64.

#include "EffectBundleTemplate.h"
#include "../../../IO/Stream/IStream.h"

#include "../../../Core/Ids.h"
#include "../../../Sound/SoundObjectTemplate.h"    // templateInitialize configures the impl
#include <sstream>

namespace dice {
namespace hfe {
namespace world {

// TemplateInfo(name) base, own vtable, two zeroed dwords.
EffectBundleTemplate::EffectInfo::EffectInfo(const std::string& templateName)
    : TemplateInfo(templateName)
{
    m_field54 = 0;
    m_field58 = 0;
}

// ctor - 0x81FBA20, 340 B, read in full: Bundle base, EffectProperties base (at +0x17C via the
// compiler), then every own field verbatim. Tail: updateFlags(0x40, 0).
EffectBundleTemplate::EffectBundleTemplate()
{
    m_field2D8 = -1.0f;
    m_field2DC = -1.0f;
    // The ten stores that used to sit here are two default-constructed RandomDistributions and
    // nothing more - CRD_NONE / 1.0f / 0.0f / false / seed 0x759DC150, twice. The members now carry
    // that themselves, so the ctor writes nothing; the emitted stores are identical.
    // (The 0x759DC150 that was read as a float sentinel is Random's default Lehmer seed.)
    m_field308 = 500.0f;
    m_emitFrequencyScaleFactorLowerBound = 0.05f;
    m_field310 = -1.0f;
    m_byte314 = 0;
    // m_vec318 / m_vec324: default-constructed empty.
    m_field330 = 0;
    m_field334 = 0;
    m_byte338 = 0; m_byte339 = 0; m_byte33A = 0;
    m_field33C = 0;

    updateFlags(0x40, 0);
}

// (D1), 180 B: the two vectors freed (compiler's job here), then ~EffectProperties and
// ~BundleTemplate. Nothing manual survives the translation.
EffectBundleTemplate::~EffectBundleTemplate()
{
}

unsigned int EffectBundleTemplate::getClassID() const
{
    return CID_EffectBundleTemplate;
}

// templateInitialize - 0x81FBD20, 158 B: add the "Start" sound template through the Bundle layer
// (the same addSoundTemplate chain the PCO family uses) and configure its engine impl 3D with
// loop count 1 - the opposite of PCO's Ambient pair. On the server both settings land in the
// shared null template's 5-byte no-ops, exactly as shipped.
void EffectBundleTemplate::templateInitialize()
{
    hfe::SoundObjectTemplate* start =
        static_cast<hfe::SoundObjectTemplate*>(addSoundTemplate("Start"));   // vtable +0x110
    start->impl()->setIs3dSound(true);           // impl() +0x150; setIs3dSound +0x68
    start->impl()->setLoopCount(1);              // setLoopCount +0x18
}

// the standard one-CID hop into Bundle's chain (a literal tail-jump in the ELF).
void* EffectBundleTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_EffectBundleTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}

#if DEDICATED
// Dedicated ELF 0x81FBDB0: xor eax. The client factory is in attested EffectBundle.cpp.
IObject* EffectBundleTemplate::createObject() const
{
    return 0;
}
#endif

// the already-precached bit gates an explicit ObjectTemplate::preCache call - the
// grandparent's, not Bundle's. As shipped.
void EffectBundleTemplate::preCache() const
{
    if ((getFlags() & 0x20) == 0)
        ObjectTemplate::preCache();
}

// Dedicated ELF81FC410 / PE5F9250 and client BF2.exe 0x6416E0 (table 0x8FF850 +0xF8), read in
// full and identical. The local stream is genuinely empty; its zero-byte write still occurs after a
// successful base writer and its result is checked.
bool EffectBundleTemplate::makeMemberScript(const TemplateInfo* info, io::IStream* stream)
{
    std::ostringstream out;
    if (!BundleTemplate::makeMemberScript(info, stream))
        return false;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

// substitute the effect-aware child info for Bundle's plain TemplateInfo.
BundleTemplate::TemplateInfo* EffectBundleTemplate::createTemplateInfo(const std::string& name)
{
    return new EffectInfo(name);
}

void EffectBundleTemplate::setEmitFrequencyScaleFactorLowerBound(float v)
{
    m_emitFrequencyScaleFactorLowerBound = v;
}

float EffectBundleTemplate::getEmitFrequencyScaleFactorLowerBound() const
{
    return m_emitFrequencyScaleFactorLowerBound;
}

// ELF 81FC820 / dedicated PE 5FA020: both base writers must succeed before this block.
bool EffectBundleTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream) || !EffectProperties::makeScript(stream)) return false;
    std::ostringstream out;
    if (m_field2D8 != -1.0f) out << "ObjectTemplate.minDistanceUnderwaterSurface " << m_field2D8 << std::endl;
    if (m_field2DC != -1.0f) out << "ObjectTemplate.maxDistanceUnderwaterSurface " << m_field2DC << std::endl;
    for (unsigned int i = 0; i < m_vec318.size(); ++i)
        out << "ObjectTemplate.addWorkOnMaterial " << static_cast<int>(m_vec318[i]) << std::endl;
    out << "ObjectTemplate.setStartOnEffects " << static_cast<unsigned int>(m_field330) << std::endl;
    out << "ObjectTemplate.setStopOnEffects " << static_cast<unsigned int>(m_field334) << std::endl;
    out << "ObjectTemplate.timeToLive " << m_dist2E0 << std::endl;
    out << "ObjectTemplate.randomAgeFactor " << m_dist2F4 << std::endl;
    out << "ObjectTemplate.startAtCreation " << (m_byte314 != 0) << std::endl;
    out << "ObjectTemplate.speedFromPhysics " << (m_byte338 != 0) << std::endl;
    if (m_emitFrequencyScaleFactorLowerBound != 0.05f)
        out << "ObjectTemplate.emitFrequencyScaleFactorLowerBound " << m_emitFrequencyScaleFactorLowerBound << std::endl;
    if (m_field310 >= 0.0f)
        out << "ObjectTemplate.startLodPercentageOfLodDistance " << m_field310 << std::endl;
    // [PLATFORM] PE compares bits 0x43FA0000 (500); ELF constant 88A9868 is 501.
#if defined(_WIN32)
    const float omittedLodDistance = 500.0f;
#else
    const float omittedLodDistance = 501.0f;
#endif
    if (m_field308 != omittedLodDistance) out << "ObjectTemplate.lodDistance " << m_field308 << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
 }

} // namespace world
} // namespace hfe
} // namespace dice
