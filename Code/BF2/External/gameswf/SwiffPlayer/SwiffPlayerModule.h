// Path:     attested - Code\BF2\External\gameswf\SwiffPlayer\SwiffPlayerModule.cpp (a BP2 __FILE__
//           string; SwiffPlayerModule.h is [INFERRED] its header).
// Contents: the 16-byte plugin module - the IModule the host registers and the one-slot
//           ECCoreEventHandler it registers with the EventManager to publish the "SwiffPlayer"
//           singleton on core event (10,1). Layout and vtable shapes read from SwiffPlayer.dll
//           (SwiffModule_vftable, SwiffModule_ECCoreEventHandler_vftable, constructSwiffPluginModule).

#ifndef BF2_SWIFFPLAYER_SWIFFPLAYERMODULE_H
#define BF2_SWIFFPLAYER_SWIFFPLAYERMODULE_H

#ifdef BF2_IMAGE_SWIFFPLAYER

#include "../../../../RendDX9Abi/HfeAbi.h"
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Native callbacks use the host's pointer-sized event arguments.
#include "../../../Game/IEventListener.h"
#endif

namespace dice {
namespace hfe {
namespace ui {

// The one-slot event interface, the same shape RendDX9's ModuleGeom carries (PluginModules.h): the
// EventManager pushes four dwords, the handler tests the first two - (category, id). A local copy
// rather than a shared one so the SwiffPlayer image does not depend on the RendDX9 module headers.
#if !defined(_MSC_VER)
class ECCoreEventHandler : public ::dice::hfe::IEventListener
#else
class ECCoreEventHandler
#endif
{
public:
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] Both module bodies inspect only category and id.
    // Preserve their recovered helper while publishing the actual host callback.
    void handleEvent(EventCategory category, unsigned int id, EventNode*, void*) override
    {
        handleCoreModuleEvent(static_cast<int>(category), static_cast<int>(id), 0, 0);
    }
#endif
    virtual void handleCoreModuleEvent(int category, int id, int a3, int a4) = 0;   // +0x00

protected:
    ~ECCoreEventHandler() {}
};


// SwiffModule - 16 bytes: IModule vtbl +0, ECCoreEventHandler vtbl +4, refcount +8. constructed with
// bf_malloc(16) by the shipped static initialiser constructSwiffPluginModule (0x100D1730); born with
// refcount 1 (ClassManager::addServer -> 2, unregisterServer -> 1, deinitDll's release -> 0, free).

// Declared explicitly (not via a macro) because the second base moves the refcount to +0x08, and
// because SwiffModule's queryInterface answers a different id set than RendDX9's modules do - 1 and
// 40000 only, NOT 40003 (read from sub_10008680). So RD9_MODULE_IBASE_BODIES does not apply.

class SwiffModule : public IModule, public ECCoreEventHandler
{
public:
    SwiffModule() : m_refCount(1) {}

    // IModule / IBase
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;     // (answers 1 and 40000)
    virtual void  getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector));
    virtual void  getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, hostVector)); // (empty)
    virtual bool  init();
    virtual bool  close();

    // ECCoreEventHandler
    virtual void  handleCoreModuleEvent(int category, int id, int a3, int a4);

private:
    int m_refCount;   // +0x08 - two vptrs precede it
};

} // namespace ui
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_SWIFFPLAYER
#endif // BF2_SWIFFPLAYER_SWIFFPLAYERMODULE_H
