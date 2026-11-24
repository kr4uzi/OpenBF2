// [INFERRED] Interface header placement beside the object-template interfaces.
#ifndef GAME_OBJECTS_ISOLDIERTEMPLATE_H
#define GAME_OBJECTS_ISOLDIERTEMPLATE_H

#include "../../Core/IBase.h"

namespace dice { namespace hfe { namespace world {

// PE secondary table 0x008F1738 / ELF 0x088A0AD4 has the IBase quartet then inventory size.
// Offsets are authored through SoldierTemplate itself, not this secondary face.
class ISoldierTemplate : public IBase
{
public:
    virtual int getInventorySize() = 0;

protected:
    ~ISoldierTemplate() {}
};

} } }

#endif
