// Path:     [INFERRED] - beside its header. Contents: recovered from bf2.i64.

#include "AirDraftEffectBundleTemplate.h"
#if !DEDICATED
#include "AirDraftEffectBundle.h"
#include "../../Main/Game.h"
#endif
#include "../../../IO/Stream/IStream.h"
#include <sstream>

#include "../../../Core/Ids.h"

namespace dice {
namespace hfe {
namespace world {

// SimpleObjectTemplate base, EffectProperties base at +0x164, then the five own
// fields. NO updateFlags call - unusual for this family, and as read.
AirDraftEffectBundleTemplate::AirDraftEffectBundleTemplate()
{
    m_byte2D0 = 0;
    // m_string2C0: default-constructed empty (the ELF's _S_empty store).
    m_field2CC = 0;
    m_field2C8 = 0;
    m_field2C4 = 0;
}

// (D1): the two bases' teardown - the compiler's job in our build.
AirDraftEffectBundleTemplate::~AirDraftEffectBundleTemplate()
{
}

unsigned int AirDraftEffectBundleTemplate::getClassID() const
{
    return CID_AirDraftEffectBundleTemplate;
}

// 5 bytes - its own empty override. Preserve the no-op (its EffectBundleTemplate cousin adds a
// "Start" sound template here; the air draft does not).
void AirDraftEffectBundleTemplate::templateInitialize()
{
}

// three CIDs, flat: it does NOT chain to SimpleObjectTemplate's implementation.
void* AirDraftEffectBundleTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_AirDraftEffectBundleTemplate ||
        classId == CID_SimpleObjectTemplate ||
        classId == CID_ObjectTemplate)
        return this;
    return 0;
}

// BF2.exe 0x646880 allocates/constructs the client object after the runtime dedicated
// check. Dedicated PE 0x5FE490 / ELF 0x81F5180 return null.
IObject* AirDraftEffectBundleTemplate::createObject() const
{
#if !DEDICATED
    if (!isDedicated())
        return new AirDraftEffectBundle(this);
#endif
    return 0;
}

// ELF 81F51A0 / dedicated PE 5FD9A0: both bases precede the five unconditional fields.
bool AirDraftEffectBundleTemplate::makeScript(io::IStream* stream)
{
    if (!SimpleObjectTemplate::makeScript(stream) || !EffectProperties::makeScript(stream))
        return false;
    std::ostringstream out;
    out << "ObjectTemplate.maxDistance " << m_field2CC << std::endl;
    out << "ObjectTemplate.startDistance " << m_field2C8 << std::endl;
    out << "ObjectTemplate.engineInfluence " << m_field2C4 << std::endl;
    out << "ObjectTemplate.alignEffectTransformation " << (m_byte2D0 != 0) << std::endl;
    out << "ObjectTemplate.engineName " << m_string2C0 << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

} // namespace world
} // namespace hfe
} // namespace dice
