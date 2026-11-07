// Portal-passing component interface. Directory attested by the ELF global initializer.
#ifndef SCENE_OBJECT_WORLDOBJTEMPLPORTAL_IPORTALPASSING_H
#define SCENE_OBJECT_WORLDOBJTEMPLPORTAL_IPORTALPASSING_H

#include "../../../Core/IBase.h"
#include "../../../Math/Vec3.h"

namespace dice { namespace hfe { namespace world {
class IObject;

// The interface has no virtual destructor. PortalPassing introduces its destructor
// after these methods (PE table 0x00920C18, ELF table 0x088CADA0).
class IPortalPassing : public IBase
{
public:
    virtual IObject* getSector() const = 0;
    virtual void setSector(IObject* sector) = 0; // [INFERRED] void; callers discard EAX
    // [INFERRED] Reference spelling; the result is the embedded coordinate's address in EAX.
    virtual const Vec3& getPortalPassingCoordinate() const = 0;
};

} } }
#endif
