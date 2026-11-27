#include "NewsManager.h"
#include "../AutoPatch/AutoPatch.h"
#include "../../Core/ClassManager.h"
#include "../../Core/Ids.h"
#if !DEDICATED
namespace dice { namespace hfe {
extern ClassManager* classManager;
NewsManager::NewsManager() : m_refCount(1) {} // BF2.exe 0x4A7390
NewsManager::~NewsManager() {} // BF2.exe deleting destructor 0x4A7360
IBase* NewsManager::create(unsigned int iid, IBase* outer) // BF2.exe 0x41043F
{
    if (outer) return 0;
    NewsManager* object = new NewsManager;
    IBase* result = static_cast<IBase*>(object->queryInterface(iid));
    if (!result) object->release();
    return result;
}
void NewsManager::addRef() { ++m_refCount; } // BF2.exe 0x6D0970
int NewsManager::getRef() { return m_refCount; } // BF2.exe 0x68AED0
int NewsManager::release() // BF2.exe 0x4A73D0; virtual destructor at +14
{
    const int remaining = --m_refCount;
    if (!remaining) delete this;
    return remaining;
}
void* NewsManager::queryInterface(unsigned int iid) const // BF2.exe 0x4A72E0
{
    return iid == IID_IBase || iid == IID_INewsManager ? const_cast<NewsManager*>(this) : 0;
}
void NewsManager::checkForPatch(PatchDesc* desc) // BF2.exe 0x4A7310
{
    IAutoPatch* autoPatch = static_cast<IAutoPatch*>(classManager->getSingleton("AutoPatch"));
    autoPatch->checkForPatch(desc);
}
} }
#endif
