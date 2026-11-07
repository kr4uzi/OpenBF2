// Path: [INFERRED] beside AnimatedBundleTemplate; neither MaskObject class has an attested
// __FILE__ or anonymous-namespace path in the dedicated images.
// Recovery: ELF 0x818D790..0x818DADD, vtable 0x889B7A0; PE 0x590EE0..0x590FF1,
// factory 0x482210. Own fields follow AnimatedBundleTemplate (ELF 0x194 / PE 0x244).
#ifndef GAME_OBJECTS_MASKOBJECTTEMPLATE_H
#define GAME_OBJECTS_MASKOBJECTTEMPLATE_H

#include "AnimatedBundleTemplate.h"
#include <iosfwd>

namespace dice {
namespace hfe {

// Names and values attested by both MaskObjectType stream operators.
enum MaskObjectType
{
    MOTNightVisionGoggles = 0,
    MOTGasMask = 1,
    MOTNoMask = 2
};

namespace world {

std::ostream& operator<<(std::ostream&, MaskObjectType);
std::istream& operator>>(std::istream&, MaskObjectType&);

struct GameTemplateConsoleAccess;

class MaskObjectTemplate : public AnimatedBundleTemplate
{
public:
    MaskObjectTemplate();
    virtual ~MaskObjectTemplate();
    virtual unsigned int getClassID() const;
    virtual void* queryImplementation(unsigned int);
    virtual IObject* createObject() const;
    virtual bool makeScript(io::IStream*);

    // Nonvirtual in the template vtable; console methods 0x818D8B0 / 0x818D8D0.
    void setMaskType(MaskObjectType);
    MaskObjectType getMaskType() const;

protected:
    friend struct GameTemplateConsoleAccess;
    MaskObjectType m_maskType; // ELF +0x194 / PE +0x244; ctor MOTNoMask
    Vec3 m_maskOffset;         // ELF +0x198 / PE +0x248; ctor (0, -1.55, 0.08)
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif
