// Portal-passing component; source filename attested by the ELF FILE symbol.
#ifndef SCENE_OBJECT_WORLDOBJTEMPLPORTAL_PORTALPASSING_H
#define SCENE_OBJECT_WORLDOBJTEMPLPORTAL_PORTALPASSING_H

#include "IPortalPassing.h"
#include "PortalManager.h"

namespace dice { namespace hfe { namespace world {

// Borrows its sector and callback pointers; owns two callback sets.
class PortalPassing : public IPortalPassing
{
public:
    PortalPassing(const Vec3& coordinate, IObject* sector);
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    IObject* getSector() const override;
    void setSector(IObject* sector) override;
    const Vec3& getPortalPassingCoordinate() const override;
    virtual ~PortalPassing();

    void subscribeEvents(PortalEventType type, IPortalEventCallback* callback);
    void unSubscribeEvents(PortalEventType type, IPortalEventCallback* callback);
    void broadcastEvent(PortalEvent* event) const;

private:
    int m_refCount;                  // x86 +0x04
    Vec3 m_coordinate;               // x86 +0x08
    IObject* m_sector;               // x86 +0x14, borrowed
    std::vector<std::set<IPortalEventCallback*>*> m_callbacks; // x86 +0x18
};

} } }
#endif
