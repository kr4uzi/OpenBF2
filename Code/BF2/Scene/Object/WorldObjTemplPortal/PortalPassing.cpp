// ELF FILE symbol: PortalPassing.cpp; directory attested by IPortalPassing's initializer.
#include "PortalPassing.h"
#include "PortalEvent.h"
#include "../../../Core/Ids.h"
#include <new>

namespace dice { namespace hfe { namespace world {

// PE 0x007354D0 / ELF 0x08328810.
PortalPassing::PortalPassing(const Vec3& coordinate, IObject* sector)
    : m_refCount(1), m_coordinate(coordinate), m_sector(sector)
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

// PE 0x00735600 / ELF 0x083289A0.
PortalPassing::~PortalPassing()
{
    for (std::vector<std::set<IPortalEventCallback*>*>::iterator it = m_callbacks.begin();
         it != m_callbacks.end(); ++it)
        delete *it;
}

// PE 0x006D0970 / ELF 0x08328C90.
void PortalPassing::addRef() { ++m_refCount; }

// PE 0x0068AED0 / ELF 0x08328CA0.
int PortalPassing::getRef() { return m_refCount; }

// PE 0x007355E0 / ELF 0x08328CB0.
int PortalPassing::release()
{
    int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

// PE 0x007355B0 / ELF 0x08328CF0.
void* PortalPassing::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IPortalPassing)
        return const_cast<PortalPassing*>(this);
    return 0;
}

// PE 0x0067D080 / ELF 0x08328B10.
IObject* PortalPassing::getSector() const { return m_sector; }

// PE 0x005BB600 / ELF 0x08328B20.
void PortalPassing::setSector(IObject* sector) { m_sector = sector; }

// PE 0x00659EC0 / ELF 0x08328B30.
const Vec3& PortalPassing::getPortalPassingCoordinate() const { return m_coordinate; }

// PE 0x00735460 / ELF 0x08328B40.
void PortalPassing::subscribeEvents(PortalEventType type, IPortalEventCallback* callback)
{
    m_callbacks[type]->insert(callback);
}

// PE 0x00735490 / ELF 0x08328B80.
void PortalPassing::unSubscribeEvents(PortalEventType type, IPortalEventCallback* callback)
{
    m_callbacks[type]->erase(callback);
}

// PE 0x007353E0 / ELF 0x08328BB0.
void PortalPassing::broadcastEvent(PortalEvent* event) const
{
    std::set<IPortalEventCallback*>* callbacks = m_callbacks[event->getType()];
    for (std::set<IPortalEventCallback*>::iterator it = callbacks->begin();
         it != callbacks->end(); ++it)
        (*it)->handleEvent(event);
}

} } }
