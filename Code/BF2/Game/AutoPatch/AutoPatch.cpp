#include "AutoPatch.h"
#include "../../Core/Ids.h"
#if !DEDICATED
namespace dice { namespace hfe {
AutoPatch::AutoPatch() : m_refCount(1) {} // BF2.exe 0x4A7420
AutoPatch::~AutoPatch() {} // BF2.exe deleting destructor 0x4A73F0
IBase* AutoPatch::create(unsigned int iid, IBase* outer) // BF2.exe 0x41047E
{
    if (outer) return 0;
    AutoPatch* object = new AutoPatch;
    IBase* result = static_cast<IBase*>(object->queryInterface(iid));
    if (!result) object->release();
    return result;
}
void AutoPatch::addRef() { ++m_refCount; } // BF2.exe 0x6D0970
int AutoPatch::getRef() { return m_refCount; } // BF2.exe 0x68AED0
int AutoPatch::release() // BF2.exe 0x4A73D0; virtual destructor at +14
{
    const int remaining = --m_refCount;
    if (!remaining) delete this;
    return remaining;
}
void* AutoPatch::queryInterface(unsigned int iid) const // BF2.exe 0x4A73A0
{
    return iid == IID_IBase || iid == IID_IAutoPatch ? const_cast<AutoPatch*>(this) : 0;
}
bool AutoPatch::checkForPatch(PatchDesc*) // BF2.exe 0x455400: xor al,al; ret 4
{
    return false;
}
} }
#endif
