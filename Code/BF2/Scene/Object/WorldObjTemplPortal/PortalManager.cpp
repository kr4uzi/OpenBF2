// Portal registration and sector transitions; ELF FILE symbol attests PortalManager.cpp.

#include "PortalManager.h"
#include "PortalPassing.h"
#include "Sector.h"
#include "PortalEvent.h"
#include "../ObjectUtil.h"
#include "../ObjectManager.h"
#include "../ObjectPredicator.h"
#include "../../../Platform/X87Compat.h"
#include <cfloat>
#include <cmath>

#include "../../../Core/Ids.h"       // IID_IBase, world::IID_IPortalManager - the two ids
                                     // queryInterface compares against, both loaded from globals in
                                     // the ELF (0x8328616 / 0x8328625), not from immediates.
#include "../../../Game/Objects/IObject.h"

#include <new>

namespace dice {
namespace hfe {
namespace world {

// Starts with one reference and an empty callback set for each of the two portal event types.
PortalManager::PortalManager()
    : m_refCount(1)                                 // +0x04
    , m_passingObjects()                            // +0x08  allocate(0x18) at 0x8325F8D
    , m_sectorObjects()                             // +0x14  allocate(0x14) at 0x8325FC9
    , m_globalCallbacks()                           // +0x20  0x8325FF4/FB/0x8326002
    , m_showStats(false)                            // +0x2C
    , m_debugLevel(0)                               // +0x30
{
    // One callback set per event type. `operator new(0xCu)` at 0x8326022 is the 12-byte
    // std::set<IPortalEventCallback*>; its own header node is the allocate(0x14) at 0x8326036.
    for (int type = 0; type < NUM_PORTAL_EVENT_TYPES; ++type)
        m_globalCallbacks.push_back(new std::set<IPortalEventCallback*>());
}

// Deletes PassingObjectData records and callback sets, but not PortalPassing objects or callbacks.
// Callers must unregister live passing objects before manager destruction to release them.
PortalManager::~PortalManager()
{
    // The per-object data blocks. Loop 0x8326280-0x832629E; the delete itself is the hoisted block
    // at 0x83263F0. The map is emptied by its own destructor afterwards (_M_erase at 0x8326350,
    // deallocate(header, 24) at 0x8326330).
    for (std::map<IObject*, PassingObjectData*>::iterator it = m_passingObjects.begin();
         it != m_passingObjects.end(); ++it)
    {
        delete it->second;                          // 0x83263F3, bare operator delete - POD
    }

    // The two callback sets. Loop 0x83262B0-0x83262E6.
    for (std::vector<std::set<IPortalEventCallback*>*>::iterator it = m_globalCallbacks.begin();
         it != m_globalCallbacks.end(); ++it)
    {
        delete *it;                                 // 0x83262D7, set dtor + operator delete
    }

    // m_sectorObjects is emptied by its own destructor: _M_erase at 0x8326381, deallocate(header,
    // 20) at 0x8326315. The vector's buffer goes at 0x83263B2. All three are compiler output.
}

// The IBase quartet. 0x83285B0 / 0x83285C0 / 0x83285D0 / 0x8328610.

// Byte for byte the same four functions as world::BasicPhysicsSystem's - same sizes (15/11/57/32),
// same shapes. release() calls vtable +0x14, the deleting destructor (`call dword ptr [eax+14h]` at
// 0x83285F0), and returns the new count (`mov eax, ebx` at 0x8328600).
void PortalManager::addRef()
{
    ++m_refCount;
}

int PortalManager::getRef()
{
    return m_refCount;
}

int PortalManager::release()
{
    int remaining = --m_refCount;
    if (remaining == 0)
        delete this;                                // 0x83285F0, call [vtable+0x14] = D0
    return remaining;
}

// Returns IBase or IPortalManager for the matching global id, otherwise null.
// Factory registration must request IID_IPortalManager, not zero.
void* PortalManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<PortalManager*>(this);
    if (iid == IID_IPortalManager)
        return const_cast<PortalManager*>(this);
    return 0;
}

// Adds a global callback for event type 0 or 1. The caller must keep type within
// the two-entry vector; registration does not check bounds.
void PortalManager::subscribeGlobalEvents(PortalEventType type, IPortalEventCallback* callback)
{
    m_globalCallbacks[type]->insert(callback);      // 0x83271AC index, 0x83271C7 insert_unique
}

// unSubscribeGlobalEvents - 0x83271E0, 41 bytes. The mirror image; `erase(const key&)`, whose return
// (a size_type) is discarded.
void PortalManager::unSubscribeGlobalEvents(PortalEventType type, IPortalEventCallback* callback)
{
    m_globalCallbacks[type]->erase(callback);       // 0x83271EC index, 0x8327200 erase
}

// Intentionally empty statistics rendering in the dedicated image.
void PortalManager::renderStats()
{
}

// Reads or writes the statistics flag and integer debug level.
void PortalManager::setShowStats(bool on)
{
    m_showStats = on;
}

bool PortalManager::getShowStats() const
{
    return m_showStats;                             // 0x8327497, movzx
}

void PortalManager::setDebugLevel(int level)
{
    m_debugLevel = level;
}

int PortalManager::getDebugLevel() const
{
    return m_debugLevel;
}

// PE 0x0072E790 / ELF 0x083265C0.
void PortalManager::registerPortalPassingObject(IObject* object, const Vec3& portalPassingCoordinate)
{
#if defined(_WIN32)
    // [CHOSEN, not recovered] Modern new spelling for the PE's nullable allocations.
    PortalPassing* passing = new (std::nothrow) PortalPassing(portalPassingCoordinate, 0);
#else
    PortalPassing* passing = new PortalPassing(portalPassingCoordinate, 0);
#endif
    object->setComponent(IID_IPortalPassing, passing);
#if defined(_WIN32)
    PassingObjectData* data = new (std::nothrow) PassingObjectData;
#else
    PassingObjectData* data = new PassingObjectData;
#endif
    data->lastPosition = object->getAbsolutePosition();
    data->portalPassing = passing;
    // shipped defect: replacement leaks the previous record and its component reference.
    m_passingObjects[object] = data;
}

// PE 0x0072DD60 / ELF 0x083266C0.
// Detaches the component before releasing it, then deletes data and erases the entry.
void PortalManager::unRegisterPortalPassingObject(IObject* object)
{
    std::map<IObject*, PassingObjectData*>::iterator it =
        m_passingObjects.find(object);
    if (it == m_passingObjects.end())
        return;

    object->setComponent(IID_IPortalPassing, 0);
    PassingObjectData* data = it->second;
    data->portalPassing->release();
    ::operator delete(data);
    m_passingObjects.erase(it);
}

// PE 0x0072E070 / ELF 0x08326790.
void PortalManager::registerSectorObject(IObject* object)
{
#if defined(_WIN32)
    // [CHOSEN, not recovered] Modern new spelling for the PE's nullable allocation.
    Sector* sector = new (std::nothrow) Sector;
#else
    Sector* sector = new Sector;
#endif
    object->setComponent(IID_ISector, sector);
    m_sectorObjects.insert(object);
}

// Queries the sector with IID_ISector in both arguments, detaches it, releases it and erases membership.
// The doubled-ID interpretation remains [UNRESOLVED]; preserve the observed dispatch.
void PortalManager::unRegisterSectorObject(IObject* object)
{
    IBase* sector = object->queryComponent(IID_ISector, IID_ISector);
    if (!sector)
        return;

    object->setComponent(IID_ISector, 0);
    sector->release();
    m_sectorObjects.erase(object);
}

// PE 0x0072E820 / ELF 0x08327270.
bool PortalManager::subscribePortalPassingObjectEvents(PortalEventType type,
                                                       IObject* object,
                                                       IPortalEventCallback* callback)
{
    PassingObjectData* data = m_passingObjects[object];
    if (!data)
        return false;
    data->portalPassing->subscribeEvents(type, callback);
    return true;
}

// PE 0x0072E860 / ELF 0x08327320.
bool PortalManager::unSubscribePortalPassingObjectEvents(PortalEventType type,
                                                         IObject* object,
                                                         IPortalEventCallback* callback)
{
    PassingObjectData* data = m_passingObjects[object];
    if (!data)
        return false;
    data->portalPassing->unSubscribeEvents(type, callback);
    return true;
}

// PE 0x0072D130 / ELF 0x083273D0.
bool PortalManager::subscribeSectorEvents(PortalEventType type,
                                          IObject* object,
                                          IPortalEventCallback* callback)
{
    Sector* sector = static_cast<Sector*>(object->queryComponent(IID_ISector, IID_ISector));
    if (!sector)
        return false;
    sector->subscribeEvents(type, callback);
    return true;
}

// PE 0x0072D170 / ELF 0x08327420.
bool PortalManager::unSubscribeSectorEvents(PortalEventType type,
                                            IObject* object,
                                            IPortalEventCallback* callback)
{
    Sector* sector = static_cast<Sector*>(object->queryComponent(IID_ISector, IID_ISector));
    if (!sector)
        return false;
    sector->unSubscribeEvents(type, callback);
    return true;
}

// PE 0x0072D5B0 / ELF 0x08327210.
// The callback interface has no IBase reference-count quartet.
void PortalManager::broadcastEvent(PortalEvent* event) const
{
    std::set<IPortalEventCallback*>* callbacks = m_globalCallbacks[event->getType()];
    for (std::set<IPortalEventCallback*>::iterator it = callbacks->begin();
         it != callbacks->end(); ++it)
        (*it)->handleEvent(event);
}

namespace {

// PE 0x0072D110 / ELF 0x08328630; table PE 0x0092066C / ELF 0x088CABD0.
class PortalObjectsPred : public ObjectPredicator
{
public:
    bool predicate(IObject* object) override
    {
        return object->queryComponent(IID_ISector, IID_ISector) != 0;
    }
    // PE 0x00818360 / ELF 0x08328660.
    unsigned int getFlags() const override { return 0x01000000u; }
    // PE 0x005A1860 / ELF 0x08328670.
    bool performCulling(IObject*) override { return true; }
};

// Inlined comparison in list::merge: PE 0x0072E4B4 / ELF 0x08328210.
struct PortalCollisionPred
{
    bool operator()(const PortalManager::PortalCollision* left,
                     const PortalManager::PortalCollision* right) const
    {
        return left->distance > right->distance;
    }
};

// [INFERRED] Packaging of the inlined vector arithmetic in update.
#if defined(_MSC_VER) && defined(_M_IX86)
typedef float PortalReal;
inline PortalReal portalWide(float value) { return value; }
inline float portalStore(PortalReal value) { return value; }
inline PortalReal portalAbs(PortalReal value) { return std::fabs(value); }
inline PortalReal portalSqrt(PortalReal value) { return std::sqrt(value); }
#elif defined(_MSC_VER)
// [CHOSEN, not recovered] Follow the existing native MSVC binary64 numeric policy.
typedef double PortalReal;
inline PortalReal portalWide(float value) { return value; }
inline float portalStore(PortalReal value) { return static_cast<float>(value); }
inline PortalReal portalAbs(PortalReal value) { return std::fabs(value); }
inline PortalReal portalSqrt(PortalReal value) { return std::sqrt(value); }
#else
// [CHOSEN, not recovered] Preserve ELF x87 intermediates through the native numeric policy.
typedef bf2x87::Real PortalReal;
inline PortalReal portalWide(float value) { return bf2x87::wide(value); }
inline float portalStore(PortalReal value) { return bf2x87::store(value); }
inline PortalReal portalAbs(PortalReal value) { return bf2x87::abs(value); }
inline PortalReal portalSqrt(PortalReal value) { return bf2x87::squareRoot(value); }
#endif

// PE 0x004639B0 / inlined ELF 0x08326BBF..0x08326C49, 0x0832715A..0x08327197.
void normalisePortalDirection(Vec3& direction, PortalReal squaredLength)
{
#if defined(_MSC_VER)
    squaredLength = portalWide(portalStore(squaredLength));
    const PortalReal deviation = squaredLength - portalWide(1.0f);
#else
    const PortalReal deviation = portalWide(portalStore(squaredLength - portalWide(1.0f)));
#endif
    if (!(portalAbs(deviation) < portalWide(FLT_EPSILON)))
    {
        if (!(portalAbs(squaredLength) < portalWide(FLT_EPSILON)))
        {
            PortalReal root = portalSqrt(squaredLength);
#if !defined(_MSC_VER)
            // [PLATFORM] ELF falls back to sqrtf when the x87 square root is unordered.
            if (!(root == root))
                root = portalWide(std::sqrt(portalStore(squaredLength)));
#endif
            const PortalReal inverseLength = portalWide(1.0f) / root;
            direction.x = portalStore(portalWide(direction.x) * inverseLength);
            direction.y = portalStore(portalWide(direction.y) * inverseLength);
            direction.z = portalStore(portalWide(direction.z) * inverseLength);
        }
        else
            direction.x = direction.y = direction.z = 0.0f;
    }
}

// PE 0x0072F18C..0x0072F226 / ELF 0x08326B03..0x08326C49.
void portalMotion(IObject* object, PortalManager::PassingObjectData* data,
                   Vec3& position, Vec3& from, Vec3& direction)
{
#if defined(_MSC_VER)
    const Vec3& coordinate = data->portalPassing->getPortalPassingCoordinate();
    const Mat4& transform = object->getAbsoluteTransformation();
    const float* m = transform.m;
    position.x = m[4] * coordinate.y + m[8] * coordinate.z + m[0] * coordinate.x + m[12];
    position.y = m[1] * coordinate.x + m[5] * coordinate.y + m[9] * coordinate.z + m[13];
    position.z = m[2] * coordinate.x + m[6] * coordinate.y + m[10] * coordinate.z + m[14];
    from = data->lastPosition;
    direction = Vec3{position.x - from.x, position.y - from.y, position.z - from.z};
    normalisePortalDirection(direction,
        (direction.x * direction.x + direction.y * direction.y) + direction.z * direction.z);
#else
    // [PLATFORM] ELF obtains the matrix first and retains all three transformed coordinates
    // through the difference and squared length; PE stores x and normalises reloaded floats.
    const Mat4& transform = object->getAbsoluteTransformation();
    const Vec3& coordinate = data->portalPassing->getPortalPassingCoordinate();
    const float* m = transform.m;
    const PortalReal x = portalWide(coordinate.x), y = portalWide(coordinate.y),
                     z = portalWide(coordinate.z);
    const PortalReal px = ((portalWide(m[0]) * x + portalWide(m[4]) * y)
                         + portalWide(m[8]) * z) + portalWide(m[12]);
    const PortalReal py = ((portalWide(m[1]) * x + portalWide(m[5]) * y)
                         + portalWide(m[9]) * z) + portalWide(m[13]);
    const PortalReal pz = ((portalWide(m[2]) * x + portalWide(m[6]) * y)
                         + portalWide(m[10]) * z) + portalWide(m[14]);
    position = Vec3{portalStore(px), portalStore(py), portalStore(pz)};
    from = data->lastPosition;
    const PortalReal dx = px - portalWide(from.x), dy = py - portalWide(from.y),
                     dz = pz - portalWide(from.z);
    direction = Vec3{portalStore(dx), portalStore(dy), portalStore(dz)};
    normalisePortalDirection(direction, (dy * dy + dx * dx) + dz * dz);
#endif
}

#if !defined(_MSC_VER)
// PE 0x0072F250 / 0x0072F325; ELF 0x08326C9C / 0x08326E40 keeps the products extended.
Vec3 advancePortalPoint(const Vec3& point, const Vec3& direction)
{
    return Vec3{portalStore(portalWide(direction.x) * portalWide(0.01f) + portalWide(point.x)),
                portalStore(portalWide(direction.y) * portalWide(0.01f) + portalWide(point.y)),
                portalStore(portalWide(direction.z) * portalWide(0.01f) + portalWide(point.z))};
}
#endif

// [UNRESOLVED] IPortalCollider's source method name and full interface. Both call sites prove
// the first slot after IBase, eight addresses/Boolean arguments, and an AL Boolean result:
// PE 0x0072E15E..0x0072E188, ELF 0x08326932..0x08326978. No destructor precedes this slot.
// [RETIRE-WHEN: scene-portal-collider-interface] Replace this view/adapter when Geom's shared
// interface and implementing vtables are recovered, then validate crossings on both native ABIs.
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Native ABI view of the proven prefix, not the original interface's
// source declaration. Pointer arguments express the observed stack addresses without guessing
// source-level reference types or constness. No implementation or extra slots are supplied.
class PortalColliderView : public IBase
{
public:
    virtual bool intersectPortal(int* portalId, Vec3* normal, Vec3* position, float* distance,
                                  const Mat4* transform, const Vec3* from, const Vec3* to,
                                  bool twoSided) = 0;
};
#else
bool intersectPortal(IBase* collider, void** table, int& portalId, Vec3& normal, Vec3& position, float& distance,
                     const Mat4& transform, const Vec3& from, const Vec3& to, bool twoSided)
{
    typedef bool (__thiscall* Intersect)(IBase*, int*, Vec3*, Vec3*, float*, const Mat4*,
                                        const Vec3*, const Vec3*, bool);
    return reinterpret_cast<Intersect>(table[4])(collider, &portalId, &normal, &position, &distance,
                                                &transform, &from, &to, twoSided);
}
#endif

} // namespace

// PE 0x0072E0C0 / ELF 0x08326870. Static intersection outputs make this non-reentrant.
PortalManager::PortalCollision* PortalManager::checkAddPortalCollision(
        std::list<PortalCollision*>& collisions,
        IObject* object,
        const Vec3& from,
        const Vec3& to,
        bool twoSided) const
{
    static int portalId;
    static Vec3 intersectionNormal = {0.0f, 0.0f, 0.0f};
    static Vec3 intersectionPosition = {0.0f, 0.0f, 0.0f};
    static float distance;

    IBase* mesh = getPortalMesh(object, 0);
    if (!mesh)
        return 0;
    IBase* collider = static_cast<IBase*>(mesh->queryInterface(geom::IID_IPortalCollider));
    Sector* sector = static_cast<Sector*>(object->queryComponent(IID_ISector, IID_ISector));
#if defined(_MSC_VER)
    // [PLATFORM] PE captures the collider table before obtaining the transformation.
    void** table = *reinterpret_cast<void***>(collider);
    const Mat4& transform = object->getAbsoluteTransformation();
    const bool hit = intersectPortal(collider, table, portalId, intersectionNormal, intersectionPosition,
                                     distance, transform, from, to, twoSided);
#else
    const Mat4& transform = object->getAbsoluteTransformation();
    PortalColliderView* view = reinterpret_cast<PortalColliderView*>(collider);
    const bool hit = view->intersectPortal(&portalId, &intersectionNormal, &intersectionPosition,
                                          &distance, &transform, &from, &to, twoSided);
#endif
    if (!hit)
        return 0;
#if defined(_WIN32)
    // [CHOSEN, not recovered] Modern new spelling; the following stores stay unguarded.
    PortalCollision* collision = new (std::nothrow) PortalCollision;
#else
    PortalCollision* collision = new PortalCollision;
#endif
    collision->portalId = portalId;
    collision->intersectionNormal = intersectionNormal;
    collision->intersectionPosition = intersectionPosition;
    collision->distance = distance;
    collision->portalObject = object;
    collision->sector = sector;
    collisions.push_front(collision);
    return collision;
}

// PE 0x0072F120 / ELF 0x08326A70. deltaTime is unused.
void PortalManager::update(float deltaTime)
{
    (void)deltaTime;
    std::vector<IObject*> objects;
    PortalObjectsPred predicator;
    std::list<PortalCollision*> collisions;
    for (std::map<IObject*, PassingObjectData*>::iterator it = m_passingObjects.begin();
         it != m_passingObjects.end(); ++it)
    {
        IObject* object = it->first;
        PassingObjectData* data = it->second;
        if (!data)
            continue;

        Vec3 position = {0.0f, 0.0f, 0.0f};
        Vec3 from, direction;
        portalMotion(object, data, position, from, direction);
        IObject* sectorObject = data->portalPassing->getSector();
        bool queryGrid = true;
        if (sectorObject)
        {
            PortalCollision* collision = checkAddPortalCollision(collisions, sectorObject,
                                                                from, position, false);
            if (!collision)
                queryGrid = false;
            else
            {
#if defined(_MSC_VER)
                const Vec3 offset = {direction.x * 0.01f, direction.y * 0.01f, direction.z * 0.01f};
                from = Vec3{offset.x + collision->intersectionPosition.x,
                            offset.y + collision->intersectionPosition.y,
                            offset.z + collision->intersectionPosition.z};
#else
                from = advancePortalPoint(collision->intersectionPosition, direction);
#endif
            }
        }

        if (queryGrid)
        {
            const Vec3 delta = {position.x - from.x, position.y - from.y, position.z - from.z};
            objects.resize(0, 0);
            // shipped defect: the query starts at the saved position even when from was advanced.
            objectManager->getObjectsFromGrid(data->lastPosition, delta, objects, &predicator,
                                              false, 0, false);
            for (std::vector<IObject*>::iterator objectIt = objects.begin();
                 objectIt != objects.end(); ++objectIt)
            {
                IObject* candidate = *objectIt;
                PortalCollision* collision = checkAddPortalCollision(collisions, candidate,
                                                                    from, position, true);
                Vec3 nextFrom = {0.0f, 0.0f, 0.0f};
                if (!collision)
                    continue;
#if defined(_MSC_VER)
                const Vec3 offset = {direction.x * 0.01f, direction.y * 0.01f, direction.z * 0.01f};
#endif
                while (collision)
                {
#if defined(_MSC_VER)
                    nextFrom = Vec3{offset.x + collision->intersectionPosition.x,
                                    offset.y + collision->intersectionPosition.y,
                                    offset.z + collision->intersectionPosition.z};
#else
                    nextFrom = advancePortalPoint(collision->intersectionPosition, direction);
#endif
                    collision = checkAddPortalCollision(collisions, candidate, nextFrom, position, true);
                }
            }
        }

        collisions.sort(PortalCollisionPred());
        for (std::list<PortalCollision*>::iterator collisionIt = collisions.begin();
             collisionIt != collisions.end(); ++collisionIt)
        {
            PortalCollision* collision = *collisionIt;
            const Vec3& normal = collision->intersectionNormal;
#if defined(_MSC_VER)
            const PortalReal dot = (direction.z * normal.z + direction.y * normal.y)
                                 + direction.x * normal.x;
#else
            const PortalReal dot = (portalWide(direction.y) * portalWide(normal.y)
                                  + portalWide(direction.x) * portalWide(normal.x))
                                  + portalWide(direction.z) * portalWide(normal.z);
#endif
            const bool entering = !(dot >= portalWide(0.0f)); // unordered also enters
            const PortalEventType type = entering ? PORTAL_EVENT_ENTER_SECTOR : PORTAL_EVENT_LEAVE_SECTOR;
#if defined(_WIN32)
            // [CHOSEN, not recovered] Preserve PE's nullable event allocation.
            PortalEvent* event = new (std::nothrow) PortalEvent(type, object, collision->portalObject,
                                                              collision->portalId);
#else
            PortalEvent* event = new PortalEvent(type, object, collision->portalObject, collision->portalId);
#endif
            if (entering)
            {
                collision->sector->addObject(object);
                data->portalPassing->setSector(collision->portalObject);
            }
            else
            {
                collision->sector->removeObject(object);
                data->portalPassing->setSector(0);
            }
            broadcastEvent(event);
            collision->sector->broadcastEvent(event);
            data->portalPassing->broadcastEvent(event);
            // shipped defect: PortalEvent is not deleted after notification.
            delete collision;
        }
        collisions.resize(0, 0);
        data->lastPosition = position;
    }
}

} // namespace world
} // namespace hfe
} // namespace dice
