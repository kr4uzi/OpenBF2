// Sector membership and callbacks; source filename attested by the ELF FILE symbol.
#ifndef SCENE_OBJECT_WORLDOBJTEMPLPORTAL_SECTOR_H
#define SCENE_OBJECT_WORLDOBJTEMPLPORTAL_SECTOR_H

#include "ISector.h"
#include "PortalManager.h"

namespace dice { namespace hfe { namespace world {

// Membership and callbacks are borrowed; destruction only frees the containers.
class Sector : public ISector
{
public:
    Sector();
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    ~Sector() override;
    void getObjects(std::list<IObject*>& objects) const override;
    void addObject(IObject* object) override;
    void removeObject(IObject* object) override;

    void subscribeEvents(PortalEventType type, IPortalEventCallback* callback);
    void unSubscribeEvents(PortalEventType type, IPortalEventCallback* callback);
    void broadcastEvent(PortalEvent* event) const;

private:
    int m_refCount;                 // x86 +0x04
    std::set<IObject*> m_objects;   // x86 +0x08
    std::vector<std::set<IPortalEventCallback*>*> m_callbacks; // x86 +0x14
};

} } }
#endif
