// Owns the named server-browser instances used by the menu.
#include "ServerBrowserManager.h"
#if !DEDICATED
#include "IServerBrowser.h"
#include "../Core/ClassManager.h"
#include "../Core/Ids.h"
#include <new>

namespace dice { namespace hfe {

extern ClassManager* classManager;

// Construction (0x486790), destruction (0x4867D0), the factory (0x410205) and the quartet
ServerBrowserManager::ServerBrowserManager()
    : m_refCount(1)
{
}

// clearLists, then the map's nodes are erased and its header freed.
ServerBrowserManager::~ServerBrowserManager()
{
    clearLists();
}

IBase* ServerBrowserManager::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    ServerBrowserManager* manager = new ServerBrowserManager();
    IBase* result = static_cast<IBase*>(manager->queryInterface(iid));
    if (!result)
        manager->release();
    return result;
}

void  ServerBrowserManager::addRef() { ++m_refCount; }
int   ServerBrowserManager::getRef() { return m_refCount; }
int   ServerBrowserManager::release()
{
    const int left = --m_refCount;
    if (!left)
        delete this;
    return left;
}
void* ServerBrowserManager::queryInterface(unsigned int iid) const
{
    if (iid == 1 || iid == IID_IServerBrowserManager)
        return const_cast<ServerBrowserManager*>(this);
    return 0;
}

// +0x10..+0x24
unsigned int ServerBrowserManager::getListCount()
{
    return static_cast<unsigned int>(m_lists.size());
}

// find by name; the node's value or null.
ui::IServerBrowser* ServerBrowserManager::getList(const std::string& name)
{
    std::map<std::string, ui::IServerBrowser*>::iterator found = m_lists.find(name);
    if (found == m_lists.end())
        return 0;
    return found->second;
}

// a name already present refuses; otherwise a dice.hfe.ServerBrowser is created for
// IID_IServerBrowser, initialised for the internet or the LAN (the result is not read) and stored
// under the name.
bool ServerBrowserManager::createList(const std::string& name, bool internet)
{
    if (m_lists.find(name) != m_lists.end())
        return false;
    IBase* object = classManager->createInstance(std::string("dice.hfe.ServerBrowser"), IID_IServerBrowser, 0);
    if (!object)
        return false;
    ui::IServerBrowser* list = static_cast<ui::IServerBrowser*>(object);
    list->init(internet);
    m_lists[name] = list;
    return true;
}

// PE 0x4865B0: free each non-null interface pointer (0x4865C0..0x4865C8 calls
// 0x402D10, bf_free), without release or destruction, then empty the map.
void ServerBrowserManager::clearLists()
{
    for (std::map<std::string, ui::IServerBrowser*>::iterator it = m_lists.begin(); it != m_lists.end(); ++it)
    {
        if (it->second)
            ::operator delete(it->second);   // Preserve the retail leak of the browser's owned resources.
    }
    m_lists.clear();
}

// every list that is browsing gets its per-frame update.
void ServerBrowserManager::update()
{
    for (std::map<std::string, ui::IServerBrowser*>::iterator it = m_lists.begin(); it != m_lists.end(); ++it)
    {
        ui::IServerBrowser* list = it->second;
        if (list->isBrowsing())
            list->update();
    }
}

bool ServerBrowserManager::isAnyBrowsing()
{
    for (std::map<std::string, ui::IServerBrowser*>::iterator it = m_lists.begin(); it != m_lists.end(); ++it)
    {
        if (it->second->isBrowsing())
            return true;
    }
    return false;
}

} }
#endif // !dedicated
