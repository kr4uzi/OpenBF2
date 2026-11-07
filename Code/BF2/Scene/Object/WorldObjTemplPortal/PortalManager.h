// Portal visibility and object traversal.
// Directory attested by the ELF translation-unit symbol; filename [INFERRED].
// Keep virtual order and unresolved members as declared.

#ifndef SCENE_OBJECT_WORLDOBJTEMPLPORTAL_PORTALMANAGER_H     // [INFERRED] guard style
#define SCENE_OBJECT_WORLDOBJTEMPLPORTAL_PORTALMANAGER_H

#include <list>
#include <map>
#include <set>
#include <vector>

#include "../../../Core/IBase.h"
#include "../../../Math/Vec3.h"            // Vec3 is a typedef (Math::Vec3), not a class
#include "IPortalPassing.h"

namespace dice {
namespace hfe {
namespace world {

class IObject;
class PortalEvent;

// Portal callback interface with one virtual method and no virtual destructor.
// [INFERRED] handleEvent has no return value.
class IPortalEventCallback
{
public:
    virtual void handleEvent(PortalEvent* event) = 0;                           // +0x00
};
class PortalPassing;
class Sector;

// Portal transition type in world namespace; preserve enum type for MSVC signature linkage.
// Names are [CHOSEN, not recovered], with observed ordinals 0 = enter and 1 = leave.
// Subscribers store two callback sets indexed by these values.
enum PortalEventType
{
    PORTAL_EVENT_ENTER_SECTOR = 0,      // [CHOSEN] name; ordinal 0 is read
    PORTAL_EVENT_LEAVE_SECTOR = 1,      // [CHOSEN] name; ordinal 1 is read

    NUM_PORTAL_EVENT_TYPES    = 2       // read - the constructor allocates exactly this many
                                        // callback sets. The identifier is [CHOSEN].
};

// Portal-manager interface derived from IBase. Keep virtual method order.
// [UNRESOLVED] Original parameter names and return types marked at declarations remain unknown.
class IPortalManager : public IBase
{
public:
    // first. Slots +0x10 / +0x14. Moving this declaration down moves twelve slots.
    virtual ~IPortalManager() {}

    // Portal-passing objects: things that move through portals (soldiers, vehicles)
    // [UNRESOLVED] return type of the first four; `void` is [CHOSEN]. Each ends by returning
    // whatever its container operation returned (an iterator, a pair, a size_type), which is the
    // classic signature of a `void` function whose last statement is a call - not of a designed
    // return value. No caller anywhere in the binary uses one.
    virtual void registerPortalPassingObject(IObject* object,
                                             const Vec3& portalPassingCoordinate)   = 0;  // +0x18
    virtual void unRegisterPortalPassingObject(IObject* object)                     = 0;  // +0x1C

    // Sector objects: the volumes portals connect
    virtual void registerSectorObject(IObject* object)                              = 0;  // +0x20
    virtual void unRegisterSectorObject(IObject* object)                            = 0;  // +0x24

    // Event subscription, three scopes: global, per passing object, per sector
    virtual void subscribeGlobalEvents(PortalEventType type,
                                       IPortalEventCallback* callback)              = 0;  // +0x28
    virtual void unSubscribeGlobalEvents(PortalEventType type,
                                         IPortalEventCallback* callback)            = 0;  // +0x2C

    // these four return bool - read, not chosen. See the note above the class.
    virtual bool subscribePortalPassingObjectEvents(PortalEventType type,
                                                    IObject* object,
                                                    IPortalEventCallback* callback) = 0;  // +0x30
    virtual bool unSubscribePortalPassingObjectEvents(PortalEventType type,
                                                      IObject* object,
                                                      IPortalEventCallback* callback) = 0;// +0x34
    virtual bool subscribeSectorEvents(PortalEventType type,
                                       IObject* object,
                                       IPortalEventCallback* callback)              = 0;  // +0x38
    virtual bool unSubscribeSectorEvents(PortalEventType type,
                                         IObject* object,
                                         IPortalEventCallback* callback)            = 0;  // +0x3C

    virtual void update(float deltaTime)                                            = 0;  // +0x40
    virtual void renderStats()                                                      = 0;  // +0x44
};

// The one implementation. `dice.hfe.world.PortalManager.Standard`, CID 283933184.
class PortalManager : public IPortalManager
{
public:
    // nested types. Both are named in the mangled symbols as members of PortalManager
    // (`NS2_13PortalManager17PassingObjectDataE`, `NS2_13PortalManager15PortalCollisionE`, where
    // S2_ is dice::hfe::world) - so the nesting is read, not chosen. The field names are chosen
    // except where noted; the offsets, sizes and types are read.

    // Previous position and portal component, 16 bytes on x86.
    // The record does not release its pointer; unregister releases it after detaching the object.
    struct PassingObjectData
    {
        /* +0x00 */ Vec3           lastPosition = {0.0f, 0.0f, 0.0f}; // PE 0x0072E7DB
        /* +0x0C */ PortalPassing* portalPassing;
    };

    // Portal intersection result, 40 bytes on x86. Intersection field names are attested;
    // portalObject and sector are [CHOSEN] names.
    struct PortalCollision
    {
        /* +0x00 */ int      portalId;              // attested name
        /* +0x04 */ Vec3     intersectionNormal = {0.0f, 0.0f, 0.0f};
        /* +0x10 */ Vec3     intersectionPosition = {0.0f, 0.0f, 0.0f};
        /* +0x1C */ IObject* portalObject;          // [CHOSEN] name; the IObject argument, stored raw
        /* +0x20 */ Sector*  sector;                // [CHOSEN] name; that object's ISector
                                                    //   implementation, from queryImplementation
        /* +0x24 */ float    distance;              // attested name; PortalCollisionPred sorts on it
    };

    PortalManager();

    // Vtable, in slot order. Do not reorder.
    virtual void  addRef();                                             // +0x00
    virtual int   getRef();                                             // +0x04
    virtual int   release();                                            // +0x08
    virtual void* queryInterface(unsigned int iid) const;               // +0x0C

    virtual ~PortalManager();                                           // +0x10 / +0x14


    virtual void registerPortalPassingObject(IObject* object,
                                             const Vec3& portalPassingCoordinate);  // +0x18
    virtual void unRegisterPortalPassingObject(IObject* object);                    // +0x1C
    virtual void registerSectorObject(IObject* object);                             // +0x20
    virtual void unRegisterSectorObject(IObject* object);                           // +0x24
    virtual void subscribeGlobalEvents(PortalEventType type,
                                       IPortalEventCallback* callback);             // +0x28
    virtual void unSubscribeGlobalEvents(PortalEventType type,
                                         IPortalEventCallback* callback);           // +0x2C
    virtual bool subscribePortalPassingObjectEvents(PortalEventType type,
                                                    IObject* object,
                                                    IPortalEventCallback* callback);// +0x30
    virtual bool unSubscribePortalPassingObjectEvents(PortalEventType type,
                                                      IObject* object,
                                                      IPortalEventCallback* callback);// +0x34
    virtual bool subscribeSectorEvents(PortalEventType type,
                                       IObject* object,
                                       IPortalEventCallback* callback);             // +0x38
    virtual bool unSubscribeSectorEvents(PortalEventType type,
                                         IObject* object,
                                         IPortalEventCallback* callback);           // +0x3C
    virtual void update(float deltaTime);                               // +0x40
    virtual void renderStats();                                         // +0x44

    // NON-virtual. Absent from both vtables; reached only by name.

    // const (_ZNK) and not a vtable slot, though PortalPassing and Sector each have a
    // broadcastEvent of their own and update() calls all three in a row - this one at 0x8326FE6,
    // Sector::broadcastEvent at 0x8326FF5, PortalPassing::broadcastEvent at 0x832700A.
    void broadcastEvent(PortalEvent* event) const;

    // const, and the signature is read off the mangled name in full:
    //   RSt4listIPNS2_15PortalCollisionESaIS5_EE  std::list<PortalManager::PortalCollision*>&
    //   PNS1_7IObjectE                            IObject*
    //   RKNS0_4Vec3E SD_                          const Vec3&, const Vec3&   (SD_ repeats it)
    //   b                                         bool
    // The return type is read: the body yields either the new PortalCollision* or 0.
    PortalCollision* checkAddPortalCollision(std::list<PortalCollision*>& collisions,
                                            IObject* object,
                                            const Vec3& from,
                                            const Vec3& to,
                                            bool twoSided) const;

    // The four debug switches. No caller anywhere in the binary; see the header block.
    void setShowStats(bool on);
    bool getShowStats() const;                                          // 0x8327490     12  movzx
    void setDebugLevel(int level);
    int  getDebugLevel() const;

private:
    // Offsets describe the original x86 layout. [CHOSEN] Field names are descriptive;
    // container template arguments are established by the original symbols.
    /* +0x04 */ int m_refCount;                     // = 1 at construction (0x8325F78). The
                                                    // one-reference rule holds; ClassFactory<T>
                                                    // ::create does not addRef.

    // +0x08 .. +0x13. From _ZNSt8_Rb_treeIPN4dice3hfe5world7IObjectESt4pairIKS4_PNS2_13PortalManager
    // 17PassingObjectDataEESt10_Select1stISA_ESt4lessIS4_ESaISA_EE... - std::less, so ordered by
    // pointer value, which makes iteration order address order and therefore allocator-dependent.
    // 24-byte nodes = 16 header + 8 value.
    /* +0x08 */ std::map<IObject*, PassingObjectData*> m_passingObjects;

    // Tracks registered sector objects for insertion and removal. update queries the
    // ObjectManager grid instead of iterating this set.
    /* +0x14 */ std::set<IObject*> m_sectorObjects;

    // Owns two separately allocated callback sets. Event types directly index this vector;
    // callers must use type 0 or 1 because no bounds check or growth exists.
    /* +0x20 */ std::vector<std::set<IPortalEventCallback*>*> m_globalCallbacks;

    /* +0x2C */ bool m_showStats;                   // = false. A byte - setShowStats writes `dl`.
    /* +0x2D */ // three bytes of padding
    /* +0x30 */ int  m_debugLevel;                  // = 0
};

// ELF 0x8B1ED80 = _ZN4dice3hfe5world13portalManagerE. already defined in Core/Globals.cpp:215 as an
// IBase* - do not add another definition. Declared here the way that file defines it, so this header
// and that TU agree on the type; a second declaration at a different type is a second MSVC symbol.
extern IBase* portalManager;

} // namespace world
} // namespace hfe
} // namespace dice

#endif // SCENE_OBJECT_WORLDOBJTEMPLPORTAL_PORTALMANAGER_H
