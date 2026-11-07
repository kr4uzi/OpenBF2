// ELF FILE symbol attests PortalEvent.cpp; directory placement is [INFERRED].
#include "PortalEvent.h"

namespace dice { namespace hfe { namespace world {

// PE 0x00735380 / ELF 0x08325CC0.
PortalEvent::PortalEvent(PortalEventType type, IObject* passingObject, IObject* portalObject,
                         unsigned int portalId)
    : m_type(type), m_passingObject(passingObject), m_portalObject(portalObject), m_portalId(portalId)
{
}

// PE 0x007353B0 / ELF 0x08325D00.
PortalEvent::~PortalEvent()
{
}

// PE 0x0068AED0 / ELF 0x08325D30.
PortalEventType PortalEvent::getType() const
{
    return m_type;
}

// ELF 0x08325D60.
unsigned int PortalEvent::getPortalId() const
{
    return m_portalId;
}

} } }
