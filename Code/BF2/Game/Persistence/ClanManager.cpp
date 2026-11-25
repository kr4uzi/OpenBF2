#include "ClanManager.h"
#include "../../Core/Ids.h"
#if !DEDICATED
namespace dice { namespace hfe {
ClanManager::ClanManager() : m_refCount(1) {} // BF2.exe 0x4A72D0
ClanManager::~ClanManager() {} // BF2.exe deleting destructor 0x4A72A0
IBase* ClanManager::create(unsigned int iid, IBase* outer) // BF2.exe 0x410400
{
    if (outer) return 0;
    ClanManager* object = new ClanManager;
    IBase* result = static_cast<IBase*>(object->queryInterface(iid));
    if (!result) object->release();
    return result;
}
void ClanManager::addRef() { ++m_refCount; } // BF2.exe 0x6D0970
int ClanManager::getRef() { return m_refCount; } // BF2.exe 0x68AED0
int ClanManager::release() // BF2.exe 0x6FF5A0; virtual destructor at +10
{
    const int remaining = --m_refCount;
    if (!remaining) delete this;
    return remaining;
}
void* ClanManager::queryInterface(unsigned int iid) const // BF2.exe 0x4A7270
{
    return iid == IID_IBase || iid == IID_IClanManager ? const_cast<ClanManager*>(this) : 0;
}
} }
#endif
