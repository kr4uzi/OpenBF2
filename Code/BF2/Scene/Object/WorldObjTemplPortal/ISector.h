// Sector interface; filename attested by the ELF FILE symbol.
#ifndef SCENE_OBJECT_WORLDOBJTEMPLPORTAL_ISECTOR_H
#define SCENE_OBJECT_WORLDOBJTEMPLPORTAL_ISECTOR_H

#include <list>
#include "../../../Core/IBase.h"

namespace dice { namespace hfe { namespace world {
class IObject;

// PE table 0x00920BB0 / ELF table 0x088CAEA0: destructor, then the three methods.
class ISector : public IBase
{
public:
    // PE 0x00734F10 / ELF 0x08329470.
    virtual ~ISector() {}
    virtual void getObjects(std::list<IObject*>& objects) const = 0;
    virtual void addObject(IObject* object) = 0;
    virtual void removeObject(IObject* object) = 0;
};

} } }
#endif
