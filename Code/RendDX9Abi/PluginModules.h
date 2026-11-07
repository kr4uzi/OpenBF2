// Renderer plugin module declarations, collected here as [CHOSEN, not recovered] file organization.
// Construct DX9RendModule, RendModule, ModuleGeom and ModuleGeomMisc in that order.
// Each starts with one reference for the host registration/deinit lifecycle.
// ModuleGeom also inherits the event-listener interface and therefore has a second vptr.
// ModuleGeomMisc's geom namespace is [INFERRED]; registered class namespaces remain unchanged.

#ifndef RENDDX9ABI_PLUGINMODULES_H
#define RENDDX9ABI_PLUGINMODULES_H

#include "HfeAbi.h"
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Native callbacks use the host's pointer-sized event arguments.
#include "../BF2/Game/IEventListener.h"
#endif

namespace dice {
namespace hfe {

// The eight-slot shape, identical for three of the four. Written out as a macro for the same reason
// Core/Modules.h does it: a census of the vtables returns the same eight members every time, and
// three identical blocks would only invite one of them to drift.
#define RD9_MODULE(name, vtableAddr)                                     \
    class name : public IModule                                          \
    {                                                                    \
    public:                                                              \
        name() : m_refCount(1) {}   /* born with one reference */        \
        virtual void  addRef();                                          \
        virtual int   getRef();                                          \
        virtual int   release();                                         \
        virtual void* queryInterface(unsigned int iid) const;            \
        virtual void  getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector));       \
        virtual void  getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, hostVector)); \
        virtual bool  init();                                            \
        virtual bool  close();                                           \
    private:                                                             \
        int m_refCount;             /* +0x04 */                          \
    }

namespace dx9rend {
// vtable 0x101E2E80, object 8 bytes. Registers the class dice.hfe.dx9rend.Renderer and the two
// singletons "Renderer" and "PostProductionFrameWork".
RD9_MODULE(DX9RendModule, 0x101E2E80);
} // namespace dx9rend

namespace Rend {
// RendModule uses the case-sensitive Rend namespace of its registered singleton names.
RD9_MODULE(RendModule, 0x101E2EA0);
} // namespace Rend

namespace geom {

// ModuleGeom has a secondary event-handler vptr before its refcount, so it needs an
// explicit declaration. [INFERRED] Handler interface name; preserve four integer arguments
// and its single virtual slot, with no added destructor entry.
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

class ModuleGeom : public IModule, public ECCoreEventHandler
{
public:
    ModuleGeom() : m_refCount(1) {}
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;
    virtual void  getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector));
    virtual void  getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, hostVector));
    virtual bool  init();
    virtual bool  close();

    virtual void  handleCoreModuleEvent(int category, int id, int a3, int a4);

private:
    int m_refCount;                 // +0x08 for this one - two vptrs precede it
};

// vtable 0x101E2EE4, object 8 bytes.
RD9_MODULE(ModuleGeomMisc, 0x101E2EE4);

// Shared ObjectDrawer class/interface ids. Keep extern linkage so every module refers
// to the same definitions.
extern const unsigned int CID_ObjectDrawer;
extern const unsigned int IID_IObjectDrawer;

// ModuleGeom's singleton slot. Renderer::initialize dispatches on it directly - it does not look it
// up - so the pointer has to be visible outside the module that owns it.
extern IBase* g_GeometryTemplateManager;

} // namespace geom

} // namespace hfe
} // namespace dice

#endif // RENDDX9ABI_PLUGINMODULES_H
