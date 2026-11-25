#include "RankManager.h"
#include "../../Core/Ids.h"
#if !DEDICATED
namespace dice { namespace hfe {
RankManager::RankManager() : m_refCount(1) {} // BF2.exe 0x4A7260
RankManager::~RankManager() {} // BF2.exe deleting destructor 0x4A7230
IBase* RankManager::create(unsigned int iid, IBase* outer) // BF2.exe 0x4103C1
{
    if (outer) return 0;
    RankManager* object = new RankManager;
    IBase* result = static_cast<IBase*>(object->queryInterface(iid));
    if (!result) object->release();
    return result;
}
void RankManager::addRef() { ++m_refCount; } // BF2.exe 0x6D0970
int RankManager::getRef() { return m_refCount; } // BF2.exe 0x68AED0
int RankManager::release() // BF2.exe 0x6FF5A0; virtual destructor at +10
{
    const int remaining = --m_refCount;
    if (!remaining) delete this;
    return remaining;
}
void* RankManager::queryInterface(unsigned int iid) const // BF2.exe 0x4A7200
{
    return iid == IID_IBase || iid == IID_IRankManager ? const_cast<RankManager*>(this) : 0;
}
} }
#endif
