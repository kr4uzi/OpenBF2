// Ground-material-aware rotor/jet downwash template, combining SimpleObjectTemplate and EffectProperties.
// Dedicated createObject returns null; templateInitialize is empty and construction sets no flags.
// Original ELF size is 0x2D4. Remaining own-field roles are [UNRESOLVED]; placement is [INFERRED].

#ifndef GAME_OBJECTS_FX_AIRDRAFTEFFECTBUNDLETEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_FX_AIRDRAFTEFFECTBUNDLETEMPLATE_H

#include <string>
#include "EffectProperties.h"
#include "../Base/SimpleObjectTemplate.h"

namespace dice {
namespace hfe {
namespace world {

class AirDraftEffectBundleTemplate : public SimpleObjectTemplate, public EffectProperties
{
public:
    AirDraftEffectBundleTemplate();                           // read in full

    virtual unsigned int getClassID() const;                  // reads the CID global
    virtual ~AirDraftEffectBundleTemplate();                  // the two bases' teardown
    virtual void templateInitialize();                        // empty, as shipped
    virtual void* queryImplementation(unsigned int classId);  // three CIDs, flat
    virtual IObject* createObject() const;                    // dedicated null; client BF2.exe 0x646880
    virtual bool makeScript(io::IStream*);                    // ELF/PE recovered

    std::string   m_string2C0;                                // +0x2C0 [UNRESOLVED name]
    float         m_field2C4;                                 // +0x2C4 engineInfluence (PE float writer)
    float         m_field2C8;                                 // +0x2C8 startDistance
    float         m_field2CC;                                 // +0x2CC maxDistance
    unsigned char m_byte2D0;                                  // +0x2D0
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif
