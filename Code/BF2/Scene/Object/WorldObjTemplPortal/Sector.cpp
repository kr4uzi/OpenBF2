// ELF FILE symbol: Sector.cpp; directory follows the portal interface units.
#include "Sector.h"
#include "PortalEvent.h"
#include "../../../Core/Ids.h"
#include <new>

namespace dice { namespace hfe { namespace world {

// PE 0x00735180 / ELF 0x08328EC0.
Sector::Sector() : m_refCount(1)
{
    for (int type = 0; type < NUM_PORTAL_EVENT_TYPES; ++type)
#if defined(_WIN32)
        // [CHOSEN, not recovered] Modern new spelling for the PE's nullable allocation.
        m_callbacks.push_back(new (std::nothrow) std::set<IPortalEventCallback*>());
#else
        // [PLATFORM] ELF uses throwing new without the PE's allocation-null branch.
        m_callbacks.push_back(new std::set<IPortalEventCallback*>());
#endif
}

// PE 0x007352A0 / ELF 0x083290D0.
Sector::~Sector()
{
    for (std::vector<std::set<IPortalEventCallback*>*>::iterator it = m_callbacks.begin();
         it != m_callbacks.end(); ++it)
        delete *it;
}

// PE 0x006D0970 / ELF 0x083294A0.
void Sector::addRef() { ++m_refCount; }

// PE 0x0068AED0 / ELF 0x083294B0.
int Sector::getRef() { return m_refCount; }

// PE 0x006FF5A0 / ELF 0x083294C0.
int Sector::release()
{
    int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

// PE 0x00735270 / ELF 0x08329500.
void* Sector::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_ISector)
        return const_cast<Sector*>(this);
    return 0;
}

// PE 0x00734FC0 / ELF 0x083292F0. Append without clearing the caller's list.
void Sector::getObjects(std::list<IObject*>& objects) const
{
    for (std::set<IObject*>::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it)
        objects.push_back(*it);
}

// PE 0x007350B0 / ELF 0x08329350.
void Sector::addObject(IObject* object) { m_objects.insert(object); }

// PE 0x00735100 / ELF 0x08329380.
void Sector::removeObject(IObject* object) { m_objects.erase(object); }

// PE 0x007350D0 / ELF 0x083293A0.
void Sector::subscribeEvents(PortalEventType type, IPortalEventCallback* callback)
{
    m_callbacks[type]->insert(callback);
}

// PE 0x00735140 / ELF 0x083293E0.
void Sector::unSubscribeEvents(PortalEventType type, IPortalEventCallback* callback)
{
    m_callbacks[type]->erase(callback);
}

// PE 0x00734F40 / ELF 0x08329410.
void Sector::broadcastEvent(PortalEvent* event) const
{
    std::set<IPortalEventCallback*>* callbacks = m_callbacks[event->getType()];
    for (std::set<IPortalEventCallback*>::iterator it = callbacks->begin();
         it != callbacks->end(); ++it)
        (*it)->handleEvent(event);
}

} } }
