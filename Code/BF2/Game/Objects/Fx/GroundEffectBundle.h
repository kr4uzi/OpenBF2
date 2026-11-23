// SimpleObject that plays a ground effect below itself when it starts. Only the client factory
// creates it (GroundEffectBundleTemplate::createObject). The source file name
// GroundEffectBundle.cpp is attested; its directory and this header are [INFERRED].
#ifndef GAME_OBJECTS_FX_GROUNDEFFECTBUNDLE_H
#define GAME_OBJECTS_FX_GROUNDEFFECTBUNDLE_H
#include "../Base/SimpleObject.h"

namespace dice { namespace hfe { namespace world {
class GroundEffectBundleTemplate;

// No fields of its own: the retail allocation is SimpleObject's size (BF2.exe 0x646A1D
// `push 0x150`), and the ELF constructor stores only the two vtable pointers.
class GroundEffectBundle : public SimpleObject
{
public:
    explicit GroundEffectBundle(const GroundEffectBundleTemplate* objectTemplate);
    virtual ~GroundEffectBundle();
    virtual void init();
    virtual void handleMessage(TemplateMessage message, IPlayer* player, bool recursive);

    void calculateCurrentDistanceAndEffectPosition();
};
} } }
#endif
