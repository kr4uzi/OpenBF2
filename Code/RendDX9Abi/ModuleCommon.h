// Shared module reference-count/interface bodies. File organization is [CHOSEN, not recovered];
// the four module implementations share this observed behavior.

#ifndef RENDDX9ABI_MODULECOMMON_H
#define RENDDX9ABI_MODULECOMMON_H

#include "HfeAbi.h"
#include "BfMemory.h"

// Interface ids accepted by renderer-module queryInterface.
#define RD9_IID_IBASE         1u        // dice::hfe::IID_IBase
#define RD9_IID_IMODULE       40000u    // dice::hfe::IID_IModule
#define RD9_IID_ICLASSSERVER  40003u    // dice::hfe::IID_IClassServer

#define RD9_MODULE_IBASE_BODIES(Class)                                              \
    void Class::addRef()                                                            \
    {                                                                               \
        /* 0x1000CB20 - "return ++*(this+1)". The value is materialised in eax even \
           though IModule declares this void; nothing reads it. */                  \
        ++m_refCount;                                                               \
    }                                                                               \
                                                                                    \
    int Class::getRef()                                                             \
    {                                                                               \
        /* 0x101A9A10 */                                                            \
        return m_refCount;                                                          \
    }                                                                               \
                                                                                    \
    int Class::release()                                                            \
    {                                                                               \
        /* 0x10051820. The decremented value is CACHED BEFORE the free, and that is \
           not tidiness - reading m_refCount back after bf_free(this) would be a    \
           use-after-free. The original caches it in esi for the same reason. */    \
        const int remaining = --m_refCount;                                         \
        if (remaining == 0)                                                         \
            ::renddx9abi::engineFree(this);                                         \
        return remaining;                                                           \
    }                                                                               \
                                                                                    \
    void* Class::queryInterface(unsigned int iid) const                             \
    {                                                                               \
        /* PE 0x10051840. Returns the RAW this with no adjustment for all three ids -  \
           reproduced with const_cast rather than a static_cast, because a          \
           static_cast to a base would be free to insert a this-adjustment under    \
           multiple inheritance and silently diverge from the binary on ModuleGeom. */ \
        if (iid == RD9_IID_IBASE || iid == RD9_IID_ICLASSSERVER || iid == RD9_IID_IMODULE) \
            return const_cast<Class*>(this);                                        \
        return 0;                                                                   \
    }

#endif // RENDDX9ABI_MODULECOMMON_H
