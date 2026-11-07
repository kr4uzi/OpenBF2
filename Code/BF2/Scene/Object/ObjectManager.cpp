// Live-object registration, spatial queries, culling and deferred destruction.
// Path attested by __FILE__; recovery uses the dedicated ELF and Win32 PE.
// Simulation updates and frame updates use separate lists. Keep message and teardown order.
// Unrecovered query paths remain marked at their definitions.


// Per-frame grid reconciliation precedes object frame updates.
// gameLogic must exist before updateObjectsInGrid: the optimization check dereferences it even
// when the pending list is empty. [UNRESOLVED] whether initialization can violate that precondition;
// preserve the existing caller ordering rather than adding a silent null fallback.

#include "ObjectManager.h"
#include "../../Game/Objects/GeometryPartAccess.h"   // named geometry operations (non-MSVC)
#if defined(BF2_STATIC_PLUGINS) && !defined(_MSC_VER)
// [CHOSEN, not recovered] One native scale-interface declaration in every image.
#include "../../Geom/IScaleable.h"
#endif
#if defined(BF2_STATIC_PLUGINS) && !defined(_MSC_VER)
// [CHOSEN, not recovered] Native static geometry tables retain PE slot order.
#include "../../Geom/NativeGeometrySlots.h"
#endif
#include "../../Geom/GeometryInstanceData.h"
#include "../../Geom/IGeometryTemplate.h"
#include "ICuller.h"
#include "ObjectPredicator.h"
#include "../../Core/ClassManager.h"  // classManager - the ctor builds its cullers through it
#include "../../Core/Ids.h"           // CID_BinTreeCuller / CID_QuadTreeCuller / IID_ICuller
#include "../../AI/AIInterface/BFKnowledge/IAIObject.h"   // the per-frame positionChanged
#include <cfloat>
#include <cstdio>
#include <cstdlib>              // abort - deleteAllRegisteredObjects' integrity checks
#include <cmath>
#include "../../Core/Debug.h"         // the five culler-failure aborts
#include "../../Core/Random.h"
#include <algorithm>
#if !defined(_MSC_VER) || !defined(_M_IX86)
#include "../../Platform/X87Compat.h"
#endif

#include "../../Game/IGameLogic.h"   // hfe::gameLogic - the ds:0x8ACBFA0 global
#include "../../Core/System.h"         // WorldPref::getTickCount - the client update path
#include "../../Game/Utility/Vars.h"   // objectManager-cullHandleUpdateOnClientFactor
#include "../../IO/Network/INetworkable.h"   // m_networkableId - destroyObjects hands it to the cache
#include "ObjectCache.h"               // push/pop/mark/deleteAll all real
#include "../../Game/Objects/ObjectLayoutAccess.h"   // the flag word (+0x04)
#include "../../Game/Objects/IObjectTemplate.h"       // getClassID - the client path's always-updated classes
#include "../../RendMisc/IRenderer.h"     // getRenderView (+0x48) - the cull-settings reads
#include "../../RendMisc/IRenderView.h"   // getZoom (+0x28) / getFarPlane (+0x3C)
#include "../../Physics/IPhysicsNode.h"
#include "../../Physics/LatencyCompensation.h"
#include "../../Game/Objects/ObjectTemplate.h"        // getIsPreCaching - createObject's cache gate
#include "../../Core/SettingsRepostitory.h"           // settings->boolGet("GSUseObjectCache")
#include "../../Core/EventManager.h"                  // destroyObject: ECNetwork 17 for a networkable going away
#include "../../Game/IEventListener.h"                // ECNetwork
#include "../../Game/Objects/ISoldier.h"              // destroyObject: the soldier-with-a-kit assert
#include "../../Game/Objects/Game/Soldier.h"          // intersectLineWithObject: soldier skeleton ray
#include "../../Animation/BoneAnimation/Skeleton.h"   // Skeleton::checkRayVsSkeleton
#include "../../Physics/ICollisionMesh.h"              // object collision-mesh line test
#include "../../Physics/HeightmapCluster.h"            // optional terrain branch of intersectLine
#include "../../Math/Frustum.h"                        // render-grid Frustum query and six planes
#include "../../Math/LocalBoundingBox.h"                // render-query aggregate bounds
#include "ObjectTypes/Game/IPlayerControlObject.h"    // destroyObject: removeChildVehicle on the soldier's PCO

namespace dice {
namespace hfe {

// ds:0x8AC85A0 = _ZN4dice3hfe12classManagerE, defined in Core/Globals.cpp. Declared at dice::hfe
// scope - not inside world - because that is where Core/Globals.cpp defines it, and under MSVC the
// namespace is part of the symbol. Same spelling as BF2Engine.cpp:417 and five other call sites.
class ClassManager;
extern ClassManager* classManager;

// Game/Main/BF2Engine.cpp - the renderer BF2Render::init publishes at its line 223.  Declared here
// rather than pulled through Game/Main/BF2ClientSeams.h, because that header is client-side and
// this translation unit is shared; the same convention Scene/Object already uses for classManager
// and heightmapCluster.
// The dedicated images use a literal null receiver at the cull-settings seam below; only the
// client uses this published renderer. Keep the dedicated link independent of the client global.
#if !DEDICATED
extern IBase* g_renderer;
#endif

namespace world {

void setBoundRadiusIsDirty(IObject* object); // 0x831B870, implemented in Scene/Object/Object.cpp
extern IBase* heightmapCluster;              // ds:0x8ABAF24, defined in Core/Globals.cpp

namespace {

#if defined(BF2_STATIC_PLUGINS) && !defined(_MSC_VER)
using IScaleable = geom::IScaleable;
#else
class IScaleable : public IBase
{
public:
    virtual void setScale(const Vec3&) = 0;       // +0x10
    virtual const Vec3& getScale() const = 0;     // +0x14
};
#endif

// IGeometry remains opaque. Its destructor adds one table entry on Itanium.
inline geom::IGeometryTemplate* geometryTemplate(IBase* geometry)
{
#if defined(_MSC_VER)
    // The MSVC destructor occupies one slot on both x86 and native x64.
    enum { GetTemplateIndex = 14 }; // retail PE +0x38 / four-byte slot
    typedef geom::IGeometryTemplate* (__thiscall *GetTemplateFn)(void*);
    void** const vtable = *reinterpret_cast<void***>(geometry);
    return reinterpret_cast<GetTemplateFn>(vtable[GetTemplateIndex])(geometry);
#else
    // Non-MSVC: the named geometry_part operation (ELF +0x3C).
    return static_cast<geom::IGeometryTemplate*>(world::geometry_part::getTemplate(geometry));
#endif
}

// IGeometryTemplate +0x94 in both shipped ports.  The semantic name is [UNRESOLVED]; the two
// ObjectManager render bodies prove only its role: true admits a mode-2 wholly-visible candidate
// without a collision box, and selects visual-sphere rather than collision-box bounds.
inline bool geometryTemplateRendFlag(IBase* geometry)
{
    return geometryTemplate(geometry)->slot_0x94();
}

// BoundingBox is a TU-local helper in the retail ObjectManager source.  Its only state is eight
// world-space corners.  The corner order below is the full 0x83AF780 instruction stream:
// min/min/min, min/min/max, min/max/min, min/max/max, then the four max-x twins.
struct RendBoundingBox
{
    Vec3 corners[8];
};

inline RendBoundingBox makeRendBoundingBox(const LocalBoundingBox& local,
                                           const Mat4& transform)
{
    RendBoundingBox result;
    for (unsigned int i = 0; i != 8; ++i)
    {
        const float x = (i & 4) ? local.m_max.x : local.m_min.x;
        const float y = (i & 2) ? local.m_max.y : local.m_min.y;
        const float z = (i & 1) ? local.m_max.z : local.m_min.z;
        Vec3& corner = result.corners[i];
        corner.x = transform.m[0] * x + transform.m[4] * y
                 + transform.m[8] * z + transform.m[12];
        corner.y = transform.m[1] * x + transform.m[5] * y
                 + transform.m[9] * z + transform.m[13];
        corner.z = transform.m[2] * x + transform.m[6] * y
                 + transform.m[10] * z + transform.m[14];
    }
    return result;
}

inline bool rendBoundingBoxInside(const RendBoundingBox& box, const Frustum& frustum)
{
    for (unsigned int planeIndex = 0; planeIndex != 6; ++planeIndex)
    {
        const Plane3& plane = frustum.m_planes[planeIndex];
        unsigned int outsideCount = 0;
        for (unsigned int cornerIndex = 0; cornerIndex != 8; ++cornerIndex)
        {
            const Vec3& corner = box.corners[cornerIndex];
            const float distance = plane.normal.x * corner.x
                                 + plane.normal.y * corner.y
                                 + plane.normal.z * corner.z + plane.d;
            if (distance > 0.0f)
                ++outsideCount;
        }
        if (outsideCount == 8)
            return false;
    }
    return true;
}

inline void expandByRendBoundingBox(LocalBoundingBox& destination,
                                    const RendBoundingBox& source)
{
    Vec3 minimum = source.corners[0];
    Vec3 maximum = source.corners[0];
    for (unsigned int i = 1; i != 8; ++i)
    {
        const Vec3& p = source.corners[i];
        if (p.x < minimum.x) minimum.x = p.x;
        if (p.y < minimum.y) minimum.y = p.y;
        if (p.z < minimum.z) minimum.z = p.z;
        if (p.x > maximum.x) maximum.x = p.x;
        if (p.y > maximum.y) maximum.y = p.y;
        if (p.z > maximum.z) maximum.z = p.z;
    }
    destination.expand(minimum);
    destination.expand(maximum);
}

inline void expandByCollisionBox(IObject* object, LocalBoundingBox& destination)
{
    ICollisionMesh* const collisionMesh =
        static_cast<ICollisionMesh*>(object_detail::LayoutAccess::collisionMesh(object));
    const LocalBoundingBox* const local = collisionMesh->getBoundingBox(0);
    const RendBoundingBox world =
        makeRendBoundingBox(*local, object->getAbsoluteTransformation());
    expandByRendBoundingBox(destination, world);
}

inline void initialiseRendBounds(LocalBoundingBox& bounds, IObject* firstObject)
{
    const Vec3& position = firstObject->getAbsolutePosition();
    bounds.m_min.x = position.x - 0.1f;
    bounds.m_min.y = position.y - 0.1f;
    bounds.m_min.z = position.z - 0.1f;
    bounds.m_max.x = position.x + 0.1f;
    bounds.m_max.y = position.y + 0.1f;
    bounds.m_max.z = position.z + 0.1f;
    bounds.m_derivedDataDirty = true;
    bounds.calcRadius();
}

inline void expandByVisualSphere(IObject* object, LocalBoundingBox& bounds)
{
    const Vec3& position = object->getAbsolutePosition();
    // The ELF emits a guarded function-local initialisation for this exact sqrt(2.0) value.
    static const float diagonalFactor = static_cast<float>(std::sqrt(2.0));
    const float radius = object->getVisualBoundingRadius() * diagonalFactor;
    Vec3 point = { position.x - radius, position.y - radius, position.z - radius };
    bounds.expand(point);
    point.x = position.x + radius;
    point.y = position.y + radius;
    point.z = position.z + radius;
    bounds.expand(point);
}

// ds:0x8AB0A64 in bf2.i64 / dword_A0F740 in BF2.exe.  The name is [INFERRED].  All four
// render-query internals add the wholly-visible-list count; notably the sphere overload still
// adds zero because its culler only receives the ordinary visible list.  drawStats is its retail
// consumer; that larger body remains outside this recovery.
unsigned int g_rendWhollyVisibleCount = 0;

inline Vec3 objectPosition(IObject* object)
{
    const Mat4& transform = object->getAbsoluteTransformation();
    const Vec3 result = { transform.m[12], transform.m[13], transform.m[14] };
    return result;
}

inline float objectSpeed(IObject* object)
{
    IPhysicsNode* node = static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(object));
    if (!node)
        return 0.0f;
    const Vec3& speed = node->getPositionalSpeed();
    return static_cast<float>(std::sqrt(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z));
}

inline bool hasNonIdentityScale(IObject* object)
{
    IBase* geometry = static_cast<IBase*>(object_detail::LayoutAccess::geometry(object));
    if (!geometry || !geometryTemplate(geometry)->isLoaded())
        return false;
    IScaleable* scaleable = static_cast<IScaleable*>(geometry->queryInterface(geom::IID_IScaleable));
    if (!scaleable)
        return false;
    const Vec3& scale = scaleable->getScale();
    return std::fabs(scale.x - 1.0f) > 0.00000011920929f
        || std::fabs(scale.y - 1.0f) > 0.00000011920929f
        || std::fabs(scale.z - 1.0f) > 0.00000011920929f;
}

// LayoutAccess preserves direct-field meaning when native pointers widen. The pending
// grid-update latch is set on queueing and cleared after the object leaves the update list.
inline bool& objectPendingGridUpdate(IObject* o)
{
    return object_detail::LayoutAccess::gridRefreshQueued(o);
}

// +0x34, the object's AI counterpart. The type is established rather than guessed: the dispatch at
// 0x831379C is vtable +0x104, and that slot is __cxa_pure_virtual on ai::IAIObject's table while
// ai::AIObject's table has AIObject::positionChanged there. So the field holds an ai::IAIObject*.
// [INFERRED] name.
inline hfe::ai::IAIObject* objectAIObject(IObject* o)
{
    return static_cast<hfe::ai::IAIObject*>(object_detail::LayoutAccess::aiObject(o));
}

} // anonymous namespace

void addChildObjects(IObject* object, std::vector<IObject*>& out,
                     ObjectPredicator* predicator)
{
    while (object)
    {
        if ((object->getFlags() & 1) == 0)
        {
            if (!predicator || predicator->predicate(object))
                out.push_back(object);

            IObject* child = object->getChild();
            if (child)
                addChildObjects(child, out, predicator);
        }
        object = object_detail::LayoutAccess::sibling(object);
    }
}

// Reconciles queued weak object handles with the spatial cullers, then clears the queue.
// Expired handles are skipped. Admission requires flags 0x400 and 0x1000000 with disabled clear;
// otherwise the object is removed. Clears acknowledged dirty flags through updateFlags.
// Re-read the queue end each iteration: callbacks can enqueue more work for the same pass.
void ObjectManager::updateObjectsInGrid()
{
    for (std::vector<WeakPtr<IObject> >::iterator it = m_objectsToUpdateInGrid.begin();
         it != m_objectsToUpdateInGrid.end();
         ++it)
    {
        IObject* object = it->get();
        if (!object)
            continue;                                   // a dead weak handle - 0x831362D

        if (object->isObjectDestroyed())                // PE +0x38 / ELF +0x3C, typed call
            continue;

        objectPendingGridUpdate(object) = 0;

        const unsigned int flags = object->getFlags();  // direct +0x04 inline in the retail source

        if ((flags & 0x400) != 0 && (flags & 0x1) == 0 && (flags & 0x1000000) != 0)
        {
            CullerObject* cullerObject = object->getGridCullerObject(); // PE +0x48 / ELF +0x4C

            // Reconcile only if the object is not in the culler yet, or if one of the two dirty
            // bits says its cached bounds are stale. Otherwise there is nothing to do.
            if (!cullerObject || (flags & 0x300000) != 0)
            {
                object->updateFlags(0, 0x300000);       // PE +0x5C / ELF +0x60. Result discarded.

                if (cullerObject)
                    updateObjectInCuller(object);
                else
                    addObjectToCuller(object);

                // The AI counterpart is told the object moved. Null on objects with no AI
                // representation, which is most of them.
                if (hfe::ai::IAIObject* aiObject = objectAIObject(object))
                    aiObject->positionChanged();        // PE +0x100 / ELF +0x104
            }
        }
        else
        {
            removeObjectFromCuller(object);
        }
    }

    // The one-shot culler optimisation, 0x8313651-0x831370B
    // The first frame on which the level reports itself loaded, the culler is optimised once and the
    // latch is set so it never happens again. Before that this test runs every single frame - which
    // is the empty-world path described at the top of this section, and the reason this function
    // dereferences the gameLogic global on a server that has not loaded anything yet.
    if (!m_gridCullerOptimized)
    {
        // no null check on gameLogic. Faithful - see the empty-world note above.
        if (gameLogic->isLevelLoaded())
        {
            m_gridCullerOptimized = true;
            m_gridCuller->optimize();                                        // +0x28
        }
    }

    // Drain with resize(0): preserve the empty-vector path's WeakPtr temporary lifetime.
    m_objectsToUpdateInGrid.resize(0);
}

// Ticks enabled frame-update objects in ascending map-key order; key meaning is [INFERRED] object ID.
// Changes during the walk are deferred. Drain all additions before removals through virtual calls;
// an object in both queues ends absent regardless of enqueue order.
void ObjectManager::handleFrameUpdates(float deltaTime)
{
    if (m_drawStats > 0)                                // 0x8312702, signed >0
        drawStats();

    m_inFrameUpdate = true;

    for (std::map<unsigned int, IObject*>::iterator it = m_frameUpdateObjects.begin();
         it != m_frameUpdateObjects.end();
         ++it)
    {
        IObject* object = it->second;                   // 0x8312740, the node's mapped value
        if ((object->getFlags() & 1) == 0)              // 0x8312743, the disable bit
            object->handleFrameUpdate(deltaTime);       // PE +0xA8 / ELF +0xAC, typed call
    }

    m_inFrameUpdate = false;

    // Drain the deferred adds, 0x8312770-0x83128A9
    // The end pointer is re-read from the vector each time round (0x83128A4) - `it != v.end()`.
    // Safe against re-entry here only because the latch is down, so these calls go to the map.
    for (std::vector<IObject*>::iterator it = m_frameUpdateAddList.begin();
         it != m_frameUpdateAddList.end();
         ++it)
    {
        addObjectToFrameUpdate(*it);                    // virtual in the binary, on this
    }
    m_frameUpdateAddList.resize(0);                     // resize(0), see above

    // Then the deferred removes, 0x83127BE-0x8312857
    for (std::vector<IObject*>::iterator it = m_frameUpdateRemoveList.begin();
         it != m_frameUpdateRemoveList.end();
         ++it)
    {
        removeObjectFromFrameUpdate(*it);               // virtual in the binary, on this
    }
    m_frameUpdateRemoveList.resize(0);
}

// Creates an ObjectManager with one reference; rejects aggregation and releases on IID mismatch.
// The constructor performs setup, so the factory neither calls init nor adds a reference.
// Factory placement here differs from the original world-module translation unit.
IBase* ObjectManager::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)
        return 0;

    ObjectManager* self = new ObjectManager();          // 0x87FA4A7, operator new(0x128u)
    void* itf = self->queryInterface(iid);              // 0x87FA4C2, virtual, vtable +0x0C
    if (itf == 0)
        self->release();                                // 0x87FA4D4, virtual, vtable +0x08
    return static_cast<IBase*>(itf);
}

// Constructs two independent BinTree cullers and three independent QuadTree cullers.
// Each factory failure is fatal; the cullers are separate roles, not fallback implementations.
// Initializes registries, update queues and per-frame state before objects can be published.
ObjectManager::ObjectManager()
    : m_refCount(1)                 // +0x04, 0x830DDFE - the one-reference rule; create() does not addRef
    , m_quadTreeCuller(0)           // +0x08
    , m_gridCullerOptimized(false)  // +0x0C, 0x830DE0C (byte)
    , m_gridCuller(0)               // +0x10, 0x830DE10 - overwritten by the culler block below
    , m_noCollisionGridCuller(0)    // +0x14
    , m_noCollisionQuadTreeCuller(0)// +0x18
    , m_latencyCompensationCuller(0)// +0x1C
    // The name index default-constructs with 3011 empty buckets.
    , m_deletingAllObjects(false)   // +0x90, 0x830DE13-ish (byte) [INFERRED name]
    , m_inFrameUpdate(false)        // +0x68, 0x830DF34 (byte) - the deferred-mutation latch
    , m_drawStats(0)                // +0x11C
    , m_activeObject(0)             // +0x20
    , m_distanceCullConst(4.0f)     // +0x110
    , m_distanceCullConstPCO(5.0f)  // +0x114
    , m_minCullDistance(100.0f)     // +0x118, 0x830E157 (0x42C80000) - 100, not 4; see the header
    , m_overrideLineTestLod(-1)     // +0x124
    , m_newId(0)                    // +0x0E4
    // Initialize deferred updates, destruction buffers, message timing, and cache state.
    , m_isUpdatingObjects(false)    // +0x030
    , m_currentDestroyList(&m_destroyLists[0])   // +0x0AC -> +0x0B0
    , m_destroyListIndex(0)         // +0x0C8
    , m_lastMessageCheckTime(0.0)   // +0x0E8
    , m_clientUpdateTick(0)         // +0x10C
    // Constructs the owned object cache during member initialization.
    , m_objectCache(new ObjectCache)   // +0x120 - 0x830E2DE..0x830E327, the ctor's last act
{

    // the five cullers. See the correction above - these are five slots, not a fallback chain.
    // All five go through ClassManager::createInstance(cid, iid, outer) at vtable +0x34.
    m_gridCuller = (ICuller*)classManager->createInstance(CID_BinTreeCuller, IID_ICuller, 0);
    if (!m_gridCuller)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 287, __FUNCTION__)
            << "Could not create culler.";

    m_quadTreeCuller = (ICuller*)classManager->createInstance(CID_QuadTreeCuller, IID_ICuller, 0);
    if (!m_quadTreeCuller)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 293, __FUNCTION__)
            << "Could not create culler.";

    m_noCollisionGridCuller =
        (ICuller*)classManager->createInstance(CID_BinTreeCuller, IID_ICuller, 0);
    if (!m_noCollisionGridCuller)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 299, __FUNCTION__)
            << "Could not create culler.";

    m_noCollisionQuadTreeCuller =
        (ICuller*)classManager->createInstance(CID_QuadTreeCuller, IID_ICuller, 0);
    if (!m_noCollisionQuadTreeCuller)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 305, __FUNCTION__)
            << "Could not create culler.";

    m_latencyCompensationCuller =
        (ICuller*)classManager->createInstance(CID_QuadTreeCuller, IID_ICuller, 0);
    if (!m_latencyCompensationCuller)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 311, __FUNCTION__)
            << "Could not create culler.";

    // both deferred-mutation lists are sized up front for 8,192 objects, and
    // 0x830E299 reserves 0x200 (512) for a third vector this class does not model. Preserved because
    // a reserve is observable: it fixes when reallocation happens during a frame walk.
    m_frameUpdateAddList.reserve(0x2000);
    m_frameUpdateRemoveList.reserve(0x2000);
}

// Object registration, spatial queries and update lifecycle. Unfinished query paths are marked at their bodies.

// IBase quartet, +0x00..+0x0C. All four
void ObjectManager::addRef()  { ++m_refCount; }
int  ObjectManager::getRef()  { return m_refCount; }

// Dispatches the deleting destructor through the vtable on reaching zero; the
// `&& this` null test is in the binary.
int ObjectManager::release()
{
    const int n = --m_refCount;
    if (n == 0 && this)
        delete this;
    return n;
}

// Two ids only. IID_IObjectManager is attested by this function - it is the one
// place in the binary that names it.
void* ObjectManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectManager)
        return const_cast<ObjectManager*>(this);
    return 0;
}

// The plain accessors. All from their own disassembly.
void  ObjectManager::setDrawStats(int level)      { m_drawStats = level; }
int   ObjectManager::getDrawStats() const         { return m_drawStats; }
IObject* ObjectManager::getActiveObject() const   { return m_activeObject; }
void  ObjectManager::setActiveObject(IObject* o)  { m_activeObject = o; }

// the offsets behind these six are not in ascending name order - see the header. minCullDistance
// is +0x118 and defaults to 100.0f; distanceCullConst is +0x110 and defaults to 4.0f.
void  ObjectManager::setMinCullDistance(float v)      { m_minCullDistance = v; }
float ObjectManager::getMinCullDistance()             { return m_minCullDistance; }
void  ObjectManager::setDistanceCullConst(float v)    { m_distanceCullConst = v; }
float ObjectManager::getDistanceCullConst()           { return m_distanceCullConst; }
void  ObjectManager::setDistanceCullConstPCO(float v) { m_distanceCullConstPCO = v; }
float ObjectManager::getDistanceCullConstPCO()        { return m_distanceCullConstPCO; }

void ObjectManager::setOverrideLineTestLod(int lod) { m_overrideLineTestLod = lod; }
int  ObjectManager::getOverrideLineTestLod()        { return m_overrideLineTestLod; }

// PRE-increment: the first id handed out is 1, not 0.
unsigned int ObjectManager::getNewID() { return ++m_newId; }

const std::map<unsigned int, IObject*>& ObjectManager::getAllRegisteredObjects() const
{ return m_allRegisteredObjects; }

const std::map<unsigned int, IObject*>& ObjectManager::getAllRegisteredGamePlayObjects() const
{ return m_allRegisteredGamePlayObjects; }

// Registers by the object's unsigned ID, replacing an existing pointer without ownership changes.
// Unregistration removes culler membership, frame updates, registry and the [UNRESOLVED] auxiliary map in order.
void ObjectManager::registerObject(IObject* object)
{
    m_allRegisteredObjects[object_detail::LayoutAccess::objectId(object)] = object;
}

void ObjectManager::unregisterObject(IObject* object)
{
    removeObjectFromCuller(object);
    m_frameUpdateObjects.erase(object_detail::LayoutAccess::objectId(object)); // +0x50
    m_allRegisteredObjects.erase(object_detail::LayoutAccess::objectId(object)); // +0x94
    m_updateObjects.erase(object_detail::LayoutAccess::objectId(object)); // +0x44
}

// Three that look like gaps and are not
// `xor eax,eax; ret`. There is no ObjectGrid in this build - the grid is the
// culler pair. Reproduced as the shipped null body.
ObjectGrid* ObjectManager::getObjectGrid() { return 0; }

// empty in this build.
void ObjectManager::refreshTerrainSampling() {}

// empty. Worth pausing on - a function named updateObjectInGrid that does
// nothing. The real work happens in updateObjectsInGrid (plural, 0x83135F0, written above), which
// drains a deferred list. So per-object grid updates are batched, and this per-object entry point
// was hollowed out; calling it is not how an object's grid position gets refreshed.
void ObjectManager::updateObjectInGrid(IObject*) {}

// a straight tail-call. The grid and the culler are the same thing here.
void ObjectManager::removeObjectFromGrid(IObject* object) { removeObjectFromCuller(object); }

// Forwards to the dynamic QuadTree (+0x08) clearGetObjects slot. ICuller now pins
// the PE/ELF overload ordering, so this is a typed virtual call rather than an ABI-specific raw slot.
void ObjectManager::clearGetObjectsFromGrid()
{
    if (m_quadTreeCuller)
        m_quadTreeCuller->clearGetObjects();
}

// Grid and culler queries
// The init and radius-query chain used by response physics is recovered. Unimplemented overloads
// remain marked at their individual definitions so they are not confused with this production path.
// PE 0x00721330; ELF 0x083101C0. A nonzero release result is a shipped invariant failure.
void ObjectManager::initGrid(const Vec2& minimum2, const Vec2& maximum2,
                             int, const Vec3&)
{
    // The ctor's five provisional cullers are all one-reference objects.  Retail asserts that
    // releasing any of them leaves no outstanding reference before constructing the map-sized set.
    if (m_gridCuller && m_gridCuller->release() != 0)
        std::abort();
    if (m_quadTreeCuller && m_quadTreeCuller->release() != 0)
        std::abort();
    if (m_noCollisionGridCuller && m_noCollisionGridCuller->release() != 0)
        std::abort();
    if (m_noCollisionQuadTreeCuller && m_noCollisionQuadTreeCuller->release() != 0)
        std::abort();
    if (m_latencyCompensationCuller && m_latencyCompensationCuller->release() != 0)
        std::abort();

    m_gridCuller = (ICuller*)classManager->createInstance(CID_BinTreeCuller, IID_ICuller, 0);
    if (!m_gridCuller)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 409, __FUNCTION__)
            << "can't create static culler";

    m_quadTreeCuller =
        (ICuller*)classManager->createInstance(CID_QuadTreeCuller, IID_ICuller, 0);
    if (!m_quadTreeCuller)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 415, __FUNCTION__)
            << "can't create dynamic culler";

    m_noCollisionGridCuller =
        (ICuller*)classManager->createInstance(CID_BinTreeCuller, IID_ICuller, 0);
    if (!m_noCollisionGridCuller)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 421, __FUNCTION__)
            << "can't create noCollisionCuller culler";

    m_noCollisionQuadTreeCuller =
        (ICuller*)classManager->createInstance(CID_QuadTreeCuller, IID_ICuller, 0);
    if (!m_noCollisionQuadTreeCuller)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 427, __FUNCTION__)
            << "can't create dynamic culler";

    m_latencyCompensationCuller =
        (ICuller*)classManager->createInstance(CID_QuadTreeCuller, IID_ICuller, 0);
    if (!m_latencyCompensationCuller)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 433, __FUNCTION__)
            << "can't create latencyCompensationCuller";

    const Vec3 minimum = { minimum2.x, 0.0f, minimum2.y };
    const Vec3 maximum = { maximum2.x, 0.0f, maximum2.y };
    const Vec3 extent = {
        maximum.x - minimum.x,
        maximum.y - minimum.y,
        maximum.z - minimum.z
    };
    if (extent.x != extent.z)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 445, __FUNCTION__)
            << "grid not sqr";

    const Vec3 centre = {
        minimum.x + extent.x * 0.5f,
        minimum.y + extent.y * 0.5f,
        minimum.z + extent.z * 0.5f
    };
    const float halfDiagonal = static_cast<float>(
        std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z) * 0.5f);
    const float halfWidth = extent.x * 0.5f < 1000.0f ? extent.x * 0.5f : 1000.0f;
    const float rootWidth = halfWidth + halfDiagonal;
    const float firstChildOffset = extent.x * 0.25f;

    m_quadTreeCuller->initQuadTree(centre, firstChildOffset, 32.0f, rootWidth);
    m_noCollisionQuadTreeCuller->initQuadTree(centre, firstChildOffset, 32.0f, rootWidth);
    m_latencyCompensationCuller->initQuadTree(centre, firstChildOffset, 32.0f, rootWidth);

    LatencyCompensation::m_isEnabled = gameLogic && gameLogic->isServer();
}
// Returns the registered object for id, or null; this lookup uses the ID registry, not spatial cullers.
IObject* ObjectManager::getObjectFromGrid(unsigned int id)
{
    std::map<unsigned int, IObject*>::iterator it = m_allRegisteredObjects.find(id);
    if (it == m_allRegisteredObjects.end())
        return 0;
    return it->second;
}
// Rebuilds and returns the ID-sorted view of registered parentless objects with flag 0x400.
// Callers observe this ordering when assigning network IDs; the result is overwritten on every call.
std::map<unsigned int, IObject*>& ObjectManager::getAllObjectsFromGrid()
{
    if (!m_objectsFromGrid.empty())
        m_objectsFromGrid.clear();

    for (std::map<unsigned int, IObject*>::iterator it = m_allRegisteredObjects.begin();
         it != m_allRegisteredObjects.end(); ++it)
    {
        IObject* object = it->second;
        if (object_detail::LayoutAccess::parent(object) == 0
            && (object_detail::LayoutAccess::flags(object) & 0x400u) != 0)
            m_objectsFromGrid.insert(std::make_pair(
                object_detail::LayoutAccess::objectId(object), object));
    }
    return m_objectsFromGrid;
}
void ObjectManager::getObjectsFromGrid(std::vector<IObject*>&, ObjectPredicator*) {}
void ObjectManager::getObjectsFromGrid(const Vec3& centre, float radius,
                                       std::vector<IObject*>& out,
                                       ObjectPredicator* predicator)
{
    getObjectsFromGridByRadiusInternal(true, false, centre, radius, out, predicator);
}

void ObjectManager::getNoCollisionObjectsFromGrid(const Vec3& centre, float radius,
                                                  std::vector<IObject*>& out,
                                                  ObjectPredicator* predicator)
{
    getObjectsFromGridByRadiusInternal(false, false, centre, radius, out, predicator);
}

void ObjectManager::getObjectsFromGridByRadiusInternal(bool collisionObjects,
                                                        bool latencyObjects,
                                                        const Vec3& centre, float radius,
                                                        std::vector<IObject*>& out,
                                                        ObjectPredicator* predicator)
{
    out.resize(0);
    if (collisionObjects)
    {
        m_gridCuller->getObjects(centre, radius, out, predicator);
        if (latencyObjects)
            m_latencyCompensationCuller->getObjects(centre, radius, out, predicator);
        else
            m_quadTreeCuller->getObjects(centre, radius, out, predicator);
    }
    else
    {
        m_noCollisionGridCuller->getObjects(centre, radius, out, predicator);
        m_noCollisionQuadTreeCuller->getObjects(centre, radius, out, predicator);
    }

    if (!predicator || (predicator->getFlags() & 0x01000000) == 0)
    {
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            IObject* object = out[i];
            if ((object->getFlags() & 0x00800000) != 0)
            {
                IObject* child = object->getChild();
                if (child)
                    addChildObjects(child, out, predicator);
            }
        }
    }
}
// Sets distance-cull records, using the PCO constant for soldiers/control objects only in the main record.
// The subrecord always uses the ordinary constant. Query visual radius before template cull scale.
void ObjectManager::setObjectCullConsts(IObject* object, CullConsts& consts, CullConsts& subConsts)
{
    // the three scales are function-local statics and their values are read, not chosen.
    // They sit at ELF 0x898A7C8/0x898A7CC/0x898A7D0 as plain initialised .data - no guard variable,
    // so they are constants rather than lazily-constructed locals, and that is how they are spelled
    // here.  0x3F000000, 0x3E800000, 0x3F4CCCCD.
    static const float subCullConstScale       = 0.5f;
    static const float subMinCullDistanceScale = 0.25f;
    static const float subRadiusThreshold      = 0.8f;

    const unsigned int classId = object->getTemplate()->getClassID();     // template +0x10

    // 0x83141C3..0x83141E2 - both distances are squared and scaled before either is chosen.
    const float cullConst    = m_distanceCullConst    * m_distanceCullConst    * consts.cullConstScale;
    const float cullConstPCO = m_distanceCullConstPCO * m_distanceCullConstPCO * consts.cullConstScale;

    // 0x83141E4 tests PlayerControlObject first, then Soldier.
    consts.distanceCullConst =
        (classId == CID_PlayerControlObjectTemplate || classId == CID_SoldierTemplate)
            ? cullConstPCO
            : cullConst;

    subConsts.distanceCullConst = cullConst * subCullConstScale;
    subConsts.minCullDistance   =
        subMinCullDistanceScale * consts.minCullDistance;

    const float visualRadius   = object->getVisualBoundingRadius();            // slot 37, +0x94
    const float cullRadiusScale = object->getTemplate()->getCullRadiusScale(); // template +0x74

    const float threshold =
        cullRadiusScale * visualRadius * subRadiusThreshold;

    subConsts.radiusThreshold = threshold;
    consts.radiusThreshold    = threshold;
}

// Updates an object's distance fade with per-frame rate limiting and applies its base opacity.
// Already fully visible objects avoid redundant stores/calls. Squared distance excludes radius squared.
// [PLATFORM] preserve Windows' repeated getter, late geometry capture and comparison senses.
// [INFERRED] ELF fade comparisons follow decompiler interpretation and still require x87 confirmation.
namespace {

// [PLATFORM] Geometry fade state uses the mesh's own ABI and shared data prefix.
// Keep its two factors independent; legacy PE/ELF string and virtual-table layouts differ.
typedef geom::GeometryInstanceData ObjectGeometryFadeView;

} // anonymous namespace

void ObjectManager::setCullFade(IObject* object, float radius, const CullConsts& consts)
{
    static const float fadeDist = 0.2f;
    static const float fadeRate = 4.0f;

#if defined(_MSC_VER)
    // [PLATFORM] PE 0x6DFE40 / client 0x721A50 query the position again unless the
    // first distance is strictly negative (unordered also repeats). They capture
    // geometry only afterward, at 0x6DFF2E. ELF 0x8314470 makes one query below.
    const Vec3& position = object->getAbsolutePosition();
    const float dx = consts.cameraX - position.x;
    const float dy = consts.cameraY - position.y;
    const float dz = consts.cameraZ - position.z;
    const float radiusSq = radius * radius;
    float distanceSq = dz * dz + dy * dy + dx * dx - radiusSq;
    if (distanceSq < 0.0f)
        distanceSq = 0.0f;
    else
    {
        const Vec3& repeatedPosition = object->getAbsolutePosition();
        const float repeatedDx = consts.cameraX - repeatedPosition.x;
        const float repeatedDy = consts.cameraY - repeatedPosition.y;
        const float repeatedDz = consts.cameraZ - repeatedPosition.z;
        // Retail does not clamp the second result or reuse the first reference.
        distanceSq = repeatedDz * repeatedDz + repeatedDy * repeatedDy +
                     repeatedDx * repeatedDx - radiusSq;
    }

    const float areaTerm = radiusSq * 3.1415927f * consts.distanceCullConst;
    const float threshold = areaTerm > consts.minCullDistance
                          ? areaTerm : consts.minCullDistance;
    ObjectGeometryFadeView* const geometry =
        static_cast<ObjectGeometryFadeView*>(object_detail::LayoutAccess::geometry(object));
    const float outer = radius + consts.radiusBias;
    float fade = 0.0f;
    if (!(distanceSq > outer * outer || distanceSq > (fadeDist + 1.0f) * threshold))
    {
        const float inner = (1.0f - fadeDist) * threshold;
        // PE x87 test-AH branches include unordered in these two <= arms.
        if (!(distanceSq > inner))
        {
            const float opacity = geometry->m_float28;
            geometry->m_float2C = 1.0f;
            geometry->setOpacity(opacity);
            return;
        }
        if (!(distanceSq > threshold))
        {
            const float current = geometry->m_float2C;
            if (!(current < 1.0f))
                return;
            const float target = 1.0f - (distanceSq - inner) / (fadeDist * distanceSq);
            const float rateLimited = current + fadeRate * consts.fadeStep;
            const float selected = rateLimited > target ? rateLimited : target;
            fade = selected > 1.0f ? 1.0f : selected;
        }
        else
        {
            const float current = geometry->m_float2C;
            if (!(current > 0.0f))
                return;
            const float target = 1.0f - (distanceSq - threshold) / (fadeDist * distanceSq);
            const float rateLimited = current - fadeRate * consts.fadeStep;
            const float selected = rateLimited < target ? rateLimited : target;
            fade = selected < 0.0f ? 0.0f : selected;
        }
    }
    // PE 0x6DF3F0 and its inlined far arm read/multiply base opacity before storing
    // the new fade; the callback observes that store on the captured geometry.
    const float opacity = fade * geometry->m_float28;
    geometry->m_float2C = fade;
    geometry->setOpacity(opacity);
#else
    const Vec3& position = object->getAbsolutePosition();          // slot 27, +0x6C

    ObjectGeometryFadeView* const geometry =
        static_cast<ObjectGeometryFadeView*>(object_detail::LayoutAccess::geometry(object));

    const float dx = consts.cameraX - position.x;
    const float dy = consts.cameraY - position.y;
    const float dz = consts.cameraZ - position.z;

    const float radiusSq = radius * radius;

    // 0x83144E1..0x8314504 then the fcmov clamp.
    float distanceSq = dx * dx + dy * dy + dz * dz - radiusSq;
    if (distanceSq <= 0.0f)
        distanceSq = 0.0f;

    // the pi at flt_88A2CC4, then the max against the record's own minimum.
    const float areaTerm = radiusSq * 3.1415927f * consts.distanceCullConst;
    float threshold = consts.minCullDistance;
    if (threshold <= areaTerm)
        threshold = areaTerm;

    const float outer   = radius + consts.radiusBias;
    const float outerSq = outer * outer;

    float fade = 0.0f;

    if (distanceSq > outerSq || distanceSq > (fadeDist + 1.0f) * threshold)
    {
        fade = 0.0f;                        // past both gates - fully faded out
    }
    else
    {
        const float inner = (1.0f - fadeDist) * threshold;

        if (distanceSq <= inner)
        {
            // fully visible.  The setter gets the base opacity unchanged.
            geometry->m_float2C = 1.0f;
            geometry->setOpacity(geometry->m_float28);
            return;
        }

        if (distanceSq <= threshold)
        {
            // Fading in.  An object already at full fade is left entirely alone (0x83145AE).
            const float current = geometry->m_float2C;
            if (current >= 1.0f)
                return;

            const float rateLimited = current + fadeRate * consts.fadeStep;
            float target = 1.0f - (distanceSq - inner) / (fadeDist * distanceSq);
            if (target <= rateLimited)
                target = rateLimited;
            fade = target <= 1.0f ? target : 1.0f;
        }
        else
        {
            // Fading out.  Nothing to do once it has reached zero.
            const float current = geometry->m_float2C;
            if (current <= 0.0f)
                return;

            const float rateLimited = current - fadeRate * consts.fadeStep;
            float target = 1.0f - (distanceSq - threshold) / (fadeDist * distanceSq);
            if (target > rateLimited)
                target = rateLimited;
            fade = target <= 0.0f ? 0.0f : target;
        }
    }

    // store, then push fade * baseOpacity through the geometry's +0xF8 setter.
    geometry->m_float2C = fade;
    geometry->setOpacity(fade * geometry->m_float28);
#endif
}

// Culls, fades and adds an object hierarchy using main/sub distance records selected by visual radius.
// Disabled or predicate-rejected nodes prune the subtree. Nodes without geometry skip fade and remain.
// Soldier/player-control objects derive local records inherited by descendants; preserve repeated class queries.
void ObjectManager::cullAndAddObjectHierarchy(IObject* object, std::vector<IObject*>& out,
                                              ObjectPredicator* predicator,
                                              const CullConsts& consts,
                                              const CullConsts& subConsts)
{
    // 0x8314262 - `(m_flags ^ 1) & 1`, i.e. proceed only while bit 0 (OFIsDisabled) is clear.
    if ((object->getFlags() & 1u) != 0)
        return;

    if (predicator && !predicator->predicate(object))       // predicator slot +0x00
        return;

    const CullConsts* selectedConsts    = &consts;
    const CullConsts* selectedSubConsts = &subConsts;

    CullConsts localConsts;
    CullConsts localSubConsts;

    const IObjectTemplate* const objectTemplate = object->getTemplate();
    // ELF 0x83142AF/0x83142C9; PE 0x6E1091/0x6E10A1: retain both calls.
    if (objectTemplate->getClassID() == CID_SoldierTemplate ||
        objectTemplate->getClassID() == CID_PlayerControlObjectTemplate)
    {
        localConsts    = consts;
        localSubConsts = subConsts;
        setObjectCullConsts(object, localConsts, localSubConsts);
        selectedConsts    = &localConsts;
        selectedSubConsts = &localSubConsts;
    }

    void* const geometry = object_detail::LayoutAccess::geometry(object);   // IObject +0x38

    if (geometry)
    {
        // the order of these two virtual calls is the shipped one - the object's radius is
        // fetched first (slot 37), then the template's scale (+0x74), and only then multiplied.
        const float visualRadius    = object->getVisualBoundingRadius();
        const float cullRadiusScale = objectTemplate->getCullRadiusScale();
        const float scaledRadius    = cullRadiusScale * visualRadius;

        const CullConsts* fadeConsts = selectedSubConsts;
        // ELF 0x8314316 cmovbe and PE 0x6E1163 test-AH also select main when unordered.
        if (!(selectedConsts->radiusThreshold > scaledRadius))
            fadeConsts = selectedConsts;

        setCullFade(object, scaledRadius, *fadeConsts);

        // 0x83142F?: the fade this just wrote, read straight back off the geometry.
        if (static_cast<const ObjectGeometryFadeView*>(geometry)->m_float2C <= 0.0f)
            return;
    }

    out.push_back(object);

    // getChild is slot 60 (+0xF0); the walk then follows the +0x2C sibling link, which is the same
    // chain getChilds() in this file already uses.
    for (IObject* child = object->getChild(); child; child = child->getSibling())
    {
        cullAndAddObjectHierarchy(child, out, predicator, *selectedConsts, *selectedSubConsts);
    }
}

// ds:0x8AB0A60 in bf2.i64, the sibling of g_rendWhollyVisibleCount above.  [INFERRED] name: the
// only writer adds the number of objects the grid query finally handed back.
unsigned int g_rendCulledObjectCount = 0;

// Collects frustum-visible objects from the collision or no-collision culler pair.
// Predicate flag 0x01000000 returns visible objects before distance/fade settings are read.
// Otherwise applies per-object frustum and distance/fade tests; the first float parameter is unused.
// Preserve repeated renderer queries and initialization order of the copied CullConsts record.
// [PLATFORM] the client requires a published renderer; dedicated images dereference zero on that
// settings path. The predicate bypass is valid there; do not fabricate a dedicated renderer.


void ObjectManager::getObjectsFromGridInternal(bool collisionObjects, const Frustum& frustum,
                                               std::vector<IObject*>& out,
                                               ObjectPredicator* predicator,
                                               bool frustumTestEachObject,
                                               float unused, float fadeStep)
{
    (void)unused;                       // see the note above - the shipped body never reads it

    m_rendAllVisibleObjects.resize(0);  // +0x100, cleared first
    m_rendVisibleObjects.resize(0);     // +0x0F4

    if (collisionObjects)
    {
        m_gridCuller->getObjects(frustum, m_rendVisibleObjects,
                                 m_rendAllVisibleObjects, predicator);      // +0x10
        m_quadTreeCuller->getObjects(frustum, m_rendVisibleObjects,
                                     m_rendAllVisibleObjects, predicator);  // +0x08
    }
    else
    {
        m_noCollisionGridCuller->getObjects(frustum, m_rendVisibleObjects,
                                            m_rendAllVisibleObjects, predicator);      // +0x14
        m_noCollisionQuadTreeCuller->getObjects(frustum, m_rendVisibleObjects,
                                                m_rendAllVisibleObjects, predicator);  // +0x18
    }

    g_rendWhollyVisibleCount += static_cast<unsigned int>(m_rendAllVisibleObjects.size());

    // The wholly-visible set is appended to the visible one, optionally frustum-tested per object.
    for (std::vector<IObject*>::const_iterator it = m_rendAllVisibleObjects.begin();
         it != m_rendAllVisibleObjects.end(); ++it)
    {
        IObject* const object = *it;

        if (frustumTestEachObject)
        {
            const float radius = object->getVisualBoundingRadius();          // slot 37
            if (!frustum.inside(object->getAbsolutePosition(), radius))      // slot 27
                continue;
        }

        m_rendVisibleObjects.push_back(object);
    }

    if (predicator && (predicator->getFlags() & 0x01000000u) != 0)
    {
        // the no-distance-cull arm: hand the set back untouched.
        out.resize(0);
        for (std::vector<IObject*>::const_iterator it = m_rendVisibleObjects.begin();
             it != m_rendVisibleObjects.end(); ++it)
        {
            out.push_back(*it);
        }
        return;
    }

    CullConsts consts;
    CullConsts subConsts;

    // the manager's own minimum, squared, before the settings scale it.
    const float minCullDistanceSq = m_minCullDistance * m_minCullDistance;

    // Preserve three separate render-view calls in order. [INFERRED] radiusBias stores the
    // far plane and participates in radius addition; its name does not establish another meaning.
#if DEDICATED
    consts.minCullDistance =
        static_cast<dx9rend::IRenderer*>(0)->getRenderView()->getZoom()
        * minCullDistanceSq;

    consts.radiusBias =
        static_cast<dx9rend::IRenderer*>(0)->getRenderView()->getFarPlane();

    consts.cullConstScale =
        static_cast<dx9rend::IRenderer*>(0)->getRenderView()->getZoom()
        * 10.0f;
#else
    consts.minCullDistance =
        static_cast<dx9rend::IRenderer*>(::dice::hfe::g_renderer)->getRenderView()->getZoom()
        * minCullDistanceSq;

    consts.radiusBias =
        static_cast<dx9rend::IRenderer*>(::dice::hfe::g_renderer)->getRenderView()->getFarPlane();

    consts.cullConstScale =
        static_cast<dx9rend::IRenderer*>(::dice::hfe::g_renderer)->getRenderView()->getZoom()
        * 10.0f;                                                // ds:flt_888B588
#endif

    // the camera position is the frustum's own transformation matrix translation row
    // (+0x1A4/+0x1A8/+0x1AC is m_transformation.m[12..14]).
    const Mat4& frustumTransform = frustum.m_transformation;
    consts.cameraX = frustumTransform.m[12];
    consts.cameraY = frustumTransform.m[13];
    consts.cameraZ = frustumTransform.m[14];

    consts.fadeStep = fadeStep;

    subConsts = consts;     // all nine floats - see the uninitialised-copy note above

    out.resize(0);

    for (std::vector<IObject*>::const_iterator it = m_rendVisibleObjects.begin();
         it != m_rendVisibleObjects.end(); ++it)
    {
        IObject* const object = *it;
        setObjectCullConsts(object, consts, subConsts);
        cullAndAddObjectHierarchy(object, out, predicator, consts, subConsts);
    }

    g_rendCulledObjectCount += static_cast<unsigned int>(out.size());
}

void ObjectManager::appendGetObjectsFromGrid(const Vec3&, float, std::vector<IObject*>&, ObjectPredicator*) {}
// Forwards frustum queries with the collision selector inserted before the frustum argument.
void ObjectManager::getObjectsFromGrid(const Frustum& frustum, std::vector<IObject*>& out,
                                       ObjectPredicator* predicator, bool frustumTestEachObject,
                                       float unused, float fadeStep)
{
    getObjectsFromGridInternal(true, frustum, out, predicator, frustumTestEachObject,
                               unused, fadeStep);
}

void ObjectManager::getNoCollisionObjectsFromGrid(const Frustum& frustum,
                                                  std::vector<IObject*>& out,
                                                  ObjectPredicator* predicator,
                                                  bool frustumTestEachObject,
                                                  float unused, float fadeStep)
{
    getObjectsFromGridInternal(false, frustum, out, predicator, frustumTestEachObject,
                               unused, fadeStep);
}

// PE 0x00727F20; ELF 0x08314630.
void ObjectManager::getObjectsFromGridNoCullConst(const Frustum& frustum,
                                                 std::vector<IObject*>& out,
                                                 ObjectPredicator* predicator,
                                                 bool frustumTestEachObject,
                                                 float unused, float unusedFadeStep)
{
    getObjectsFromGridNoCullConstInternal(true, frustum, out, predicator,
                                          frustumTestEachObject, unused, unusedFadeStep);
}

// Collects root culler hits without distance fading or child expansion.
// PE 0x00726840; ELF 0x08314680. Neither trailing float is read in either image.
void ObjectManager::getObjectsFromGridNoCullConstInternal(bool collisionObjects,
                                                         const Frustum& frustum,
                                                         std::vector<IObject*>& out,
                                                         ObjectPredicator* predicator,
                                                         bool frustumTestEachObject,
                                                         float, float)
{
    m_rendAllVisibleObjects.resize(0);
    m_rendVisibleObjects.resize(0);

    if (collisionObjects)
    {
        m_gridCuller->getObjects(frustum, m_rendVisibleObjects,
                                 m_rendAllVisibleObjects, predicator);
        m_quadTreeCuller->getObjects(frustum, m_rendVisibleObjects,
                                     m_rendAllVisibleObjects, predicator);
    }
    else
    {
        m_noCollisionGridCuller->getObjects(frustum, m_rendVisibleObjects,
                                            m_rendAllVisibleObjects, predicator);
        m_noCollisionQuadTreeCuller->getObjects(frustum, m_rendVisibleObjects,
                                                m_rendAllVisibleObjects, predicator);
    }

    g_rendWhollyVisibleCount += static_cast<unsigned int>(m_rendAllVisibleObjects.size());

    for (std::vector<IObject*>::const_iterator it = m_rendAllVisibleObjects.begin();
         it != m_rendAllVisibleObjects.end(); ++it)
    {
        IObject* const object = *it;
        if (frustumTestEachObject)
        {
            const float radius = object->getVisualBoundingRadius();
            if (!frustum.inside(object->getAbsolutePosition(), radius))
                continue;
        }
        m_rendVisibleObjects.push_back(object);
    }

    if (predicator && (predicator->getFlags() & 0x01000000u) != 0)
    {
        out.resize(0);
        for (std::vector<IObject*>::const_iterator it = m_rendVisibleObjects.begin();
             it != m_rendVisibleObjects.end(); ++it)
        {
            out.push_back(*it);
        }
    }
    else
    {
        out.assign(m_rendVisibleObjects.begin(), m_rendVisibleObjects.end());
    }
}
// Appends swept-line candidates in culler order, without sorting or deduplication.
// testCollisionMesh enables exact intersection; firstHit stops after the first accepted result.
// [PLATFORM] Use typed dispatch because the logical method has different caller-facing slots across ports.
void ObjectManager::getObjectsFromGrid(const Vec3& from, const Vec3& direction,
                                       std::vector<IObject*>& out,
                                       ObjectPredicator* predicator,
                                       bool testCollisionMesh,
                                       int collisionLod,
                                       bool firstHit)
{
    m_lineCandidates.clear();
    m_gridCuller->getObjects(from, direction, m_lineCandidates);
    m_quadTreeCuller->getObjects(from, direction, m_lineCandidates);

    out.clear();
    out.reserve(m_lineCandidates.size());

    for (std::vector<std::pair<float, IObject*> >::const_iterator it = m_lineCandidates.begin();
         it != m_lineCandidates.end(); ++it)
    {
        IObject* const object = it->second;
        if (predicator && !predicator->predicate(object))
            continue;

        if (!testCollisionMesh)
        {
            out.push_back(object);
            continue;
        }

        if (!object->hasCollisionLod(collisionLod))
            continue;

        ICollisionMesh* const collisionMesh = static_cast<ICollisionMesh*>(
            object_detail::LayoutAccess::collisionMesh(object));
        Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
        Vec3 hitPosition = { 0.0f, 0.0f, 0.0f };
        float planeDistance;
        float alongRayDistance;
        if (!collisionMesh->getDistance(collisionLod, false,
                                        object->getAbsoluteTransformation(),
                                        object->getInvAbsoluteTransformation(),
                                        from, direction,
                                        hitNormal, hitPosition,
                                        planeDistance, alongRayDistance,
                                        0, 0, true, firstHit))
        {
            continue;
        }

        out.push_back(object);
        if (firstHit)
            break;
    }

    m_lineCandidates.clear();
}
// Collects positive-latency projectile candidates from the static culler, then the
// expanded dynamic culler, retaining duplicates. Applies the predicate afterward,
// without child expansion or mesh intersection.
// PE 0x00726AA0; ELF 0x08314F10. Disabled compensation is a shipped invariant failure.
void ObjectManager::getObjectsFromGridWithLatency(const Vec3& from, const Vec3& direction,
                                                  std::vector<IObject*>& out,
                                                  ObjectPredicator* predicator)
{
    if (!LatencyCompensation::m_isEnabled)
    {
        // [PLATFORM] PE traps with int 3; ELF calls abort. This is the shipped precondition.
#if defined(_WIN32)
        __debugbreak();
#else
        std::abort();
#endif
    }

    m_lineCandidates.resize(0);
    m_gridCuller->getObjects(from, direction, m_lineCandidates);
    m_latencyCompensationCuller->getObjects(from, direction, m_lineCandidates);

    out.resize(0);
    for (std::vector<std::pair<float, IObject*> >::const_iterator it = m_lineCandidates.begin();
         it != m_lineCandidates.end(); ++it)
    {
        IObject* const object = it->second;
        if (!predicator || predicator->predicate(object))
            out.push_back(object);
    }
    m_lineCandidates.resize(0);
}
// PE 0x00724CF0; ELF 0x08315170.
void ObjectManager::getObjectsFromGridWithLatency(const Vec3& centre, float radius,
                                                  std::vector<IObject*>& out,
                                                  ObjectPredicator* predicator)
{
    getObjectsFromGridByRadiusInternal(true, true, centre, radius, out, predicator);
}
// PE 0x00721070; ELF 0x08315540. Appends all four categories without clearing the output.
void ObjectManager::getObjectsInBoxFromGrid(const Vec3& minimum, const Vec3& maximum,
                                            std::vector<IObject*>& out,
                                            ObjectPredicator* predicator)
{
    m_gridCuller->getObjects(minimum, maximum, out, predicator);
    m_quadTreeCuller->getObjects(minimum, maximum, out, predicator);
    m_noCollisionGridCuller->getObjects(minimum, maximum, out, predicator);
    m_noCollisionQuadTreeCuller->getObjects(minimum, maximum, out, predicator);
}

// Render queries keep culler output categories distinct; mode 0 appends wholly visible objects directly.
// The final bool selects collision boxes with bit-23 child recursion or visual spheres without recursion.
// The concrete frustum walkers live in the culler files.
void ObjectManager::getStaticObjectsFromGridRend(const Frustum& frustum,
                                                 std::vector<IObject*>& out,
                                                 LocalBoundingBox* bounds,
                                                 ObjectPredicator* predicator,
                                                 unsigned int mode,
                                                 bool useCollisionBoundsAndChildren)
{
    getObjectsFromGridRendInternal(frustum, out, bounds, predicator, mode, true,
                                   useCollisionBoundsAndChildren);
}

void ObjectManager::getDynamicObjectsFromGridRend(const Frustum& frustum,
                                                  std::vector<IObject*>& out,
                                                  LocalBoundingBox* bounds,
                                                  ObjectPredicator* predicator,
                                                  unsigned int mode,
                                                  bool useCollisionBoundsAndChildren)
{
    getObjectsFromGridRendInternal(frustum, out, bounds, predicator, mode, false,
                                   useCollisionBoundsAndChildren);
}

void ObjectManager::getStaticObjectsFromGridRend(const Vec3& centre, float radius,
                                                 std::vector<IObject*>& out,
                                                 LocalBoundingBox* bounds,
                                                 ObjectPredicator* predicator,
                                                 unsigned int mode,
                                                 bool useCollisionBoundsAndChildren)
{
    getObjectsFromGridRendInternal(centre, radius, out, bounds, predicator, mode, true,
                                   useCollisionBoundsAndChildren);
}

void ObjectManager::getDynamicObjectsFromGridRend(const Vec3& centre, float radius,
                                                  std::vector<IObject*>& out,
                                                  LocalBoundingBox* bounds,
                                                  ObjectPredicator* predicator,
                                                  unsigned int mode,
                                                  bool useCollisionBoundsAndChildren)
{
    getObjectsFromGridRendInternal(centre, radius, out, bounds, predicator, mode, false,
                                   useCollisionBoundsAndChildren);
}

void ObjectManager::getObjectsFromGridRendInternal(const Frustum& frustum,
                                                   std::vector<IObject*>& out,
                                                   LocalBoundingBox* bounds,
                                                   ObjectPredicator* predicator,
                                                   unsigned int mode,
                                                   bool staticObjects,
                                                   bool useCollisionBoundsAndChildren)
{
    m_rendAllVisibleObjects.resize(0);
    m_rendVisibleObjects.resize(0);

    if (staticObjects)
    {
        m_gridCuller->getObjects(frustum, m_rendVisibleObjects,
                                 m_rendAllVisibleObjects, predicator);
        m_noCollisionGridCuller->getObjects(frustum, m_rendVisibleObjects,
                                            m_rendAllVisibleObjects, predicator);
    }
    else
    {
        m_quadTreeCuller->getObjects(frustum, m_rendVisibleObjects,
                                     m_rendAllVisibleObjects, predicator);
        m_noCollisionQuadTreeCuller->getObjects(frustum, m_rendVisibleObjects,
                                                m_rendAllVisibleObjects, predicator);
    }

    g_rendWhollyVisibleCount += static_cast<unsigned int>(m_rendAllVisibleObjects.size());

    if (mode == 0)
    {
        for (std::vector<IObject*>::const_iterator it = m_rendAllVisibleObjects.begin();
             it != m_rendAllVisibleObjects.end(); ++it)
            m_rendVisibleObjects.push_back(*it);
    }
    else
    {
        for (std::vector<IObject*>::const_iterator it = m_rendAllVisibleObjects.begin();
             it != m_rendAllVisibleObjects.end(); ++it)
        {
            IObject* const object = *it;
            // Instruction order is radius first, position second.
            const float visualRadius = object->getVisualBoundingRadius();
            const Vec3& position = object->getAbsolutePosition();
            if (!frustum.inside(position, visualRadius))
                continue;

            bool append = mode != 2;
            if (!append)
            {
                IBase* const geometry =
                    static_cast<IBase*>(object_detail::LayoutAccess::geometry(object));
                if (geometry && geometryTemplateRendFlag(geometry))
                {
                    append = true;
                }
                else if (object->hasCollisionLod(0))
                {
                    ICollisionMesh* const collisionMesh = static_cast<ICollisionMesh*>(
                        object_detail::LayoutAccess::collisionMesh(object));
                    const LocalBoundingBox* const local = collisionMesh->getBoundingBox(0);
                    const RendBoundingBox world =
                        makeRendBoundingBox(*local, object->getAbsoluteTransformation());
                    append = rendBoundingBoxInside(world, frustum);
                }
            }

            if (append)
                m_rendVisibleObjects.push_back(object);
        }
    }

    getObjectsFromGridRendInternal(out, bounds, predicator, mode, staticObjects,
                                   useCollisionBoundsAndChildren);
}

void ObjectManager::getObjectsFromGridRendInternal(const Vec3& centre, float radius,
                                                   std::vector<IObject*>& out,
                                                   LocalBoundingBox* bounds,
                                                   ObjectPredicator* predicator,
                                                   unsigned int mode,
                                                   bool staticObjects,
                                                   bool useCollisionBoundsAndChildren)
{
    m_rendAllVisibleObjects.resize(0);
    m_rendVisibleObjects.resize(0);

    if (staticObjects)
    {
        m_gridCuller->getObjects(centre, radius, m_rendVisibleObjects, predicator);
        m_noCollisionGridCuller->getObjects(centre, radius, m_rendVisibleObjects, predicator);
    }
    else
    {
        m_quadTreeCuller->getObjects(centre, radius, m_rendVisibleObjects, predicator);
        m_noCollisionQuadTreeCuller->getObjects(centre, radius, m_rendVisibleObjects, predicator);
    }

    // This is deliberately the other (empty) list.  Both shipped bodies perform the add even
    // though the sphere culler receives only m_rendVisibleObjects.
    g_rendWhollyVisibleCount += static_cast<unsigned int>(m_rendAllVisibleObjects.size());
    getObjectsFromGridRendInternal(out, bounds, predicator, mode, staticObjects,
                                   useCollisionBoundsAndChildren);
}

void ObjectManager::getObjectsFromGridRendInternal(std::vector<IObject*>& out,
                                                   LocalBoundingBox* bounds,
                                                   ObjectPredicator* predicator,
                                                   unsigned int mode,
                                                   bool staticObjects,
                                                   bool useCollisionBoundsAndChildren)
{
    // These two source parameters survive in both manglings but the shared retail body never
    // reads them.  Selection/filtering has already happened in the overload above.
    (void)mode;
    (void)staticObjects;

    if (bounds && bounds->m_radiusFromOrigin == 0.0f && !m_rendVisibleObjects.empty())
        initialiseRendBounds(*bounds, m_rendVisibleObjects.front());

    if (useCollisionBoundsAndChildren)
    {
        for (std::vector<IObject*>::const_iterator it = m_rendVisibleObjects.begin();
             it != m_rendVisibleObjects.end(); ++it)
        {
            IObject* const object = *it;
            out.push_back(object);

            if (bounds && object->hasCollisionLod(0) == true)
                expandByCollisionBox(object, *bounds);

            if ((object->getFlags() & 0x00800000u) != 0)
            {
                IObject* const child = object->getChild();
                if (child)
                    addChildrenRend(child, out, bounds, predicator);
            }
        }
    }
    else
    {
        for (std::vector<IObject*>::const_iterator it = m_rendVisibleObjects.begin();
             it != m_rendVisibleObjects.end(); ++it)
        {
            IObject* const object = *it;
            out.push_back(object);

            if (!bounds)
                continue;

            IBase* const geometry =
                static_cast<IBase*>(object_detail::LayoutAccess::geometry(object));
            if ((geometry && geometryTemplateRendFlag(geometry))
                || (object->getFlags() & 0x04000000u) != 0)
            {
                expandByVisualSphere(object, *bounds);
            }
            else if (object->hasCollisionLod(0) == true)
            {
                expandByCollisionBox(object, *bounds);
            }
        }
    }
}

void ObjectManager::addChildrenRend(const std::vector<IObject*>& in,
                                    std::vector<IObject*>& out,
                                    LocalBoundingBox* bounds,
                                    ObjectPredicator* predicator)
{
    if (bounds && bounds->m_radiusFromOrigin == 0.0f && !in.empty())
        initialiseRendBounds(*bounds, in.front());

    // Re-read in.end() after every iteration, as the ELF does.  The recursive call is virtual and
    // passes the predicate onward; roots in this vector are never themselves predicate-filtered.
    for (std::vector<IObject*>::const_iterator it = in.begin(); it != in.end(); ++it)
    {
        IObject* const object = *it;
        out.push_back(object);

        if (bounds && object->hasCollisionLod(0) == true)
            expandByCollisionBox(object, *bounds);

        if ((object->getFlags() & 0x00800000u) != 0)
        {
            IObject* const child = object->getChild();
            if (child)
                addChildrenRend(child, out, bounds, predicator);
        }
    }
}

void ObjectManager::addChildrenRend(IObject* object,
                                    std::vector<IObject*>& out,
                                    LocalBoundingBox* bounds,
                                    ObjectPredicator* predicator)
{
    while (object)
    {
        const unsigned int flags = object->getFlags();
        if ((flags & 1u) == 0 && (flags & 2u) != 0)
        {
            IBase* const geometry =
                static_cast<IBase*>(object_detail::LayoutAccess::geometry(object));
            if (geometry && (!predicator || predicator->predicate(object)))
                out.push_back(object);

            // Unlike the vector/common overloads, this body tests truth rather than exactly 1.
            // Bounds and descendants are processed even when geometry/predicate excluded `object`.
            if (bounds && object->hasCollisionLod(0))
                expandByCollisionBox(object, *bounds);

            IObject* const child = object->getChild();
            if (child)
                addChildrenRend(child, out, bounds, predicator);
        }

        // Disabled/invisible objects suppress their entire child subtree but not later siblings.
        object = object->getSibling();
    }
}

// world::_getChildren - / The line query already adds
// the candidate itself; this helper adds its sibling subtree first, then its child subtree.  The
// raw flag test is intentional: predicate() is applied later by intersectLineWithObject.
void _getChildren(IObject* object, unsigned int flags, std::vector<IObject*>& out)
{
    IObject* sibling = object->getSibling();
    if (sibling)
    {
        if ((flags & sibling->getFlags()) == flags)
            out.push_back(sibling);
        _getChildren(sibling, flags, out);
    }

    IObject* child = object->getChild();
    if (child)
    {
        if ((flags & child->getFlags()) == flags)
            out.push_back(child);
        _getChildren(child, flags, out);
    }
}

// world::intersectLineWithObject - / Both ports were
// read end to end.  The culler supplies only a candidate; this is the exact narrow phase:
// predicate -> bounding-sphere interval -> collision mesh (or Soldier skeleton) -> distance and
// surface predicate.  `direction` is normalised and `length` is the original delta magnitude.

// Two retail quirks are preserved.  A sphere tangent (discriminant == 0) is rejected, and the
// Soldier skeleton's local part index is not copied to `hitPart`; predicateSurface therefore sees
// the caller's existing part value on that arm.
bool intersectLineWithObject(float* hitDistance, Vec3* hitPoint, Vec3* hitNormal,
                             int& hitPart, IObject* object, Vec3 rayStart,
                             Vec3 direction, float length, ObjectPredicator* predicator,
                             float nearestDistance, bool firstHit, int overrideLod)
{
    const Vec3& objectPosition = object->getAbsolutePosition();
    const Vec3 startFromObject = {
        rayStart.x - objectPosition.x,
        rayStart.y - objectPosition.y,
        rayStart.z - objectPosition.z
    };
    const float projectedStart = startFromObject.x * direction.x
                               + startFromObject.y * direction.y
                               + startFromObject.z * direction.z;
    const float radius = object->getPhysicBoundingRadius();
    if (projectedStart - radius > length)
        return false;
    if (!predicator->predicate(object))
        return false;

    if (overrideLod == -1)
        overrideLod = 0;
    const bool hasCollisionLod = object->hasCollisionLod(overrideLod);
    const bool isSoldier = object->getTemplate()->getClassID() == CID_SoldierTemplate;
    if (!hasCollisionLod && !isSoldier)
        return false;

    const float radiusAgain = object->getPhysicBoundingRadius();
    const float doubledProjection = projectedStart + projectedStart;
    const float discriminant = doubledProjection * doubledProjection
        - 4.0f * (startFromObject.x * startFromObject.x
                + startFromObject.y * startFromObject.y
                + startFromObject.z * startFromObject.z
                - radiusAgain * radiusAgain);
    if (discriminant <= 0.0f)
        return false;

    const float root = std::sqrt(discriminant);
    float farDistance = (root - doubledProjection) * 0.5f;
    if (farDistance <= 0.0f)
        return false;
    float nearDistance = (-doubledProjection - root) * 0.5f;
    if (nearDistance >= length || farDistance <= 0.0f)
        return false;
    if (nearDistance < 0.0f)
        nearDistance = 0.0f;
    if (farDistance > length)
        farDistance = length;
    if (nearDistance == farDistance)
        return false;

    Vec3 clippedStart;
    if (nearDistance <= 0.0f)
    {
        clippedStart = rayStart;
    }
    else
    {
        clippedStart.x = rayStart.x + direction.x * nearDistance;
        clippedStart.y = rayStart.y + direction.y * nearDistance;
        clippedStart.z = rayStart.z + direction.z * nearDistance;
    }
    const Vec3 clippedEnd = {
        rayStart.x + direction.x * farDistance,
        rayStart.y + direction.y * farDistance,
        rayStart.z + direction.z * farDistance
    };

    bool intersects = false;
    if (isSoldier)
    {
        anim::Skeleton* skeleton = static_cast<Soldier*>(object)->getCollisionSkeleton(-1);
        if (!skeleton)
            return false;
        float fraction = 0.0f;
        int bone = -1;
        int localPart = 0;
        intersects = skeleton->checkRayVsSkeleton(hitPoint, hitNormal, &localPart, &bone,
                                                  &fraction, clippedStart, clippedEnd,
                                                  object->getAbsoluteTransformation());
    }
    else
    {
        const Vec3 clippedDelta = {
            clippedEnd.x - clippedStart.x,
            clippedEnd.y - clippedStart.y,
            clippedEnd.z - clippedStart.z
        };
        ICollisionMesh* collisionMesh = static_cast<ICollisionMesh*>(
            object_detail::LayoutAccess::collisionMesh(object));
        float planeDistance = 0.0f;
        float along = 0.0f;
        intersects = collisionMesh->getDistance(
            overrideLod, false, object->getAbsoluteTransformation(),
            object->getInvAbsoluteTransformation(), clippedStart, clippedDelta,
            *hitNormal, *hitPoint, planeDistance, along, &hitPart, 0, true, firstHit);
    }

    if (!intersects)
        return false;

    const float hitX = hitPoint->x - rayStart.x;
    const float hitY = hitPoint->y - rayStart.y;
    const float hitZ = hitPoint->z - rayStart.z;
    const float distance = std::sqrt(hitX * hitX + hitY * hitY + hitZ * hitZ);
    if (distance < nearestDistance && predicator->predicateSurface(hitPart))
    {
        *hitDistance = distance;
        return true;
    }
    return false;
}

// Intersects a direction delta against static and optionally dynamic candidates and
// terrain. Expands children unless predicate flag 0x01000000 disables it; both walks
// cache end() before traversal. RW-4 still requires runtime terrain fixtures.
bool ObjectManager::intersectLine(IObject*& hitObject, Vec3& hitPoint, Vec3& hitNormal,
                                  int& hitPart, const Vec3& from, const Vec3& direction,
                                  ObjectPredicator* predicator, bool includeDynamic,
                                  bool firstHit, bool includeTerrain, bool terrainTwoSided,
                                  unsigned int excludedTemplateClass)
{
    if (!predicator)
        return false;

    const float length = std::sqrt(direction.x * direction.x
                                 + direction.y * direction.y
                                 + direction.z * direction.z);
    if (length == 0.0f)
        return false;

    const float inverseLength = 1.0f / length;
    const Vec3 normalizedDirection = {
        direction.x * inverseLength,
        direction.y * inverseLength,
        direction.z * inverseLength
    };

    m_lineCandidates.resize(0);
    m_gridCuller->getObjects(from, direction, m_lineCandidates);
    if (includeDynamic)
        m_quadTreeCuller->getObjects(from, direction, m_lineCandidates);

    if (m_lineCandidates.empty() && !includeTerrain && !terrainTwoSided)
        return false;

    IObject* nearestObject = 0;
    Vec3 nearestPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 nearestNormal = { 0.0f, 0.0f, 0.0f };
    float nearestDistance = FLT_MAX;

    m_objectTempVector.resize(0);
    const bool expandChildren = (predicator->getFlags() & 0x01000000u) == 0;
    const std::vector<std::pair<float, IObject*> >::const_iterator candidatesEnd =
        m_lineCandidates.end();                                    // read once - as shipped
    for (std::vector<std::pair<float, IObject*> >::const_iterator it = m_lineCandidates.begin();
         it != candidatesEnd; ++it)
    {
        IObject* object = it->second;
        if (object)
        {
            m_objectTempVector.push_back(object);
            if (expandChildren)
                _getChildren(object, predicator->getFlags(), m_objectTempVector);
        }
    }

    // Capture end once (RW-263): candidate callbacks can reenter this query and replace shared scratch results.
    // Preserve the original residual hazard: an inner reallocation still invalidates the outer iterators.
    const std::vector<IObject*>::const_iterator objectsEnd = m_objectTempVector.end();
    for (std::vector<IObject*>::const_iterator it = m_objectTempVector.begin();
         it != objectsEnd; ++it)
    {
        IObject* object = *it;
        bool testObject = true;
        if (object)
        {
            if (!getRootParent(object) || !getRootParent(object)->getTemplate())
                return false;
            if (getRootParent(object)->getTemplate()->getClassID() == excludedTemplateClass)
            {
                testObject = false;
            }
            else
            {
                if (!object->getTemplate())
                    return false;
                if (object->getTemplate()->getClassID() == excludedTemplateClass)
                    testObject = false;
            }
        }

        if (!testObject)
            continue;

        float objectDistance = 0.0f;
        Vec3 objectPoint = { 0.0f, 0.0f, 0.0f };
        Vec3 objectNormal = { 0.0f, 0.0f, 0.0f };
        if (intersectLineWithObject(&objectDistance, &objectPoint, &objectNormal, hitPart,
                                    object, from, normalizedDirection, length, predicator,
                                    nearestDistance, firstHit, m_overrideLineTestLod))
        {
            nearestObject = object;
            nearestDistance = objectDistance;
            nearestPoint = objectPoint;
            nearestNormal = objectNormal;
        }
    }

    hitObject = nearestObject;
    if (nearestObject)
    {
        hitPoint = nearestPoint;
        hitNormal = nearestNormal;
    }

    if (includeTerrain)
    {
        const Vec3 end = {
            from.x + direction.x,
            from.y + direction.y,
            from.z + direction.z
        };
        Vec3 terrainPoint;
        Vec3 terrainNormal;
        float terrainFraction;
        unsigned char terrainMaterial;
        IHeightmapCluster* cluster = static_cast<IHeightmapCluster*>(heightmapCluster);
        if (cluster->intersectRayInWorldCoords(from, end, terrainPoint, terrainFraction,
                                               terrainNormal, terrainMaterial, terrainTwoSided))
        {
            const float terrainX = terrainPoint.x - from.x;
            const float terrainY = terrainPoint.y - from.y;
            const float terrainZ = terrainPoint.z - from.z;
            const float terrainDistanceSquared = terrainX * terrainX
                                               + terrainY * terrainY
                                               + terrainZ * terrainZ;
            bool terrainIsNearer = !hitObject;
            if (hitObject)
            {
                const float objectX = hitPoint.x - from.x;
                const float objectY = hitPoint.y - from.y;
                const float objectZ = hitPoint.z - from.z;
                terrainIsNearer = objectX * objectX + objectY * objectY + objectZ * objectZ
                                > terrainDistanceSquared;
            }
            if (terrainIsNearer)
            {
                hitObject = 0;
                hitPoint = terrainPoint;
                hitNormal = terrainNormal;
                m_lineCandidates.resize(0);
                return true;
            }
        }
    }

    m_lineCandidates.resize(0);
    return nearestObject != 0;
}

void getChilds(IObject* object, std::vector<IObject*>& out, ObjectPredicator* include,
               ObjectPredicator* stop);

namespace {
// [INFERRED] Packaging for the scalar/vector expressions in the queries below.
#if defined(_MSC_VER) && defined(_M_IX86)
typedef float QueryReal;
inline QueryReal queryWide(float value) { return value; }
inline float queryStore(QueryReal value) { return value; }
inline QueryReal querySqrt(QueryReal value) { return std::sqrt(value); }
inline QueryReal queryAbs(QueryReal value) { return std::fabs(value); }
#elif defined(_MSC_VER)
// [CHOSEN, not recovered] Native MSVC x64 has binary64 long double. Follow the
// existing native numeric policy rather than entering the extended-precision-only arm.
typedef double QueryReal;
inline QueryReal queryWide(float value) { return static_cast<double>(value); }
inline float queryStore(QueryReal value) { return static_cast<float>(value); }
inline QueryReal querySqrt(QueryReal value) { return std::sqrt(value); }
inline QueryReal queryAbs(QueryReal value) { return std::fabs(value); }
#else
// [CHOSEN, not recovered] Native ports retain the inspected x87 intermediates until
// an explicit float store. Apple silicon uses the existing ExtendedFloat policy.
typedef bf2x87::Real QueryReal;
inline QueryReal queryWide(float value) { return bf2x87::wide(value); }
inline float queryStore(QueryReal value) { return bf2x87::store(value); }
inline QueryReal querySqrt(QueryReal value) { return bf2x87::squareRoot(value); }
inline QueryReal queryAbs(QueryReal value) { return bf2x87::abs(value); }
#endif

inline QueryReal queryLengthSquared(const Vec3& value)
{
    const QueryReal x = queryWide(value.x), y = queryWide(value.y), z = queryWide(value.z);
    return (x * x + y * y) + z * z;
}

// PE 0x004639B0; inlined in ELF 0x08318070. Both leave unit vectors unchanged.
void normaliseQueryVector(Vec3& value, bool roundedDeviation = true)
{
    QueryReal lengthSquared = queryLengthSquared(value);
#if defined(_MSC_VER)
    lengthSquared = queryWide(queryStore(lengthSquared));
    (void)roundedDeviation;
    const QueryReal deviation = lengthSquared - queryWide(1.0f);
#else
    // [PLATFORM] ELF retains the squared length; its two input normalisations store
    // the deviation as a float, whereas the output-normal normalisation does not.
    const QueryReal deviation = roundedDeviation
        ? queryWide(queryStore(lengthSquared - queryWide(1.0f)))
        : lengthSquared - queryWide(1.0f);
#endif
    // Both images send unordered comparisons through the normalisation arm.
    if (!(queryAbs(deviation) < queryWide(FLT_EPSILON)))
    {
        if (!(queryAbs(lengthSquared) < queryWide(FLT_EPSILON)))
        {
            const QueryReal inverseLength = queryWide(1.0f) / querySqrt(lengthSquared);
            value.x = queryStore(queryWide(value.x) * inverseLength);
            value.y = queryStore(queryWide(value.y) * inverseLength);
            value.z = queryStore(queryWide(value.z) * inverseLength);
        }
        else
            value.x = value.y = value.z = 0.0f;
    }
}

// PE 0x007210E0; inlined in ELF 0x08316EC0 and 0x08318940.
// Tangents and spheres wholly behind the origin are rejected.
bool querySphereInterval(const Vec3& offset, const Vec3& direction, float radius,
                          QueryReal& nearDistance, QueryReal& farDistance)
{
    const QueryReal x = queryWide(offset.x), y = queryWide(offset.y), z = queryWide(offset.z);
#if defined(_MSC_VER)
    // [PLATFORM] PE's out-of-line helper stores the doubled dot product; ELF retains it.
    const float doubled = queryStore(((z * queryWide(direction.z)
        + y * queryWide(direction.y)) + x * queryWide(direction.x)) * queryWide(2.0f));
    const QueryReal projection = queryWide(doubled);
#else
    const QueryReal dot = (y * queryWide(direction.y) + x * queryWide(direction.x))
        + z * queryWide(direction.z);
    const QueryReal projection = dot + dot;
#endif
    const QueryReal r = queryWide(radius);
    const QueryReal discriminant = projection * projection
        - queryWide(4.0f) * (((x * x + y * y) + z * z) - r * r);
    if (!(discriminant > queryWide(0.0f)))
        return false;
    const QueryReal root = querySqrt(discriminant);
    const QueryReal farValue = (root - projection) * queryWide(0.5f);
    farDistance = queryWide(queryStore(farValue));
    if (!(farValue > queryWide(0.0f)))
        return false;
    nearDistance = (-projection - root) * queryWide(0.5f);
#if defined(_MSC_VER)
    nearDistance = queryWide(queryStore(nearDistance));
#endif
    return true;
}

inline Vec3 queryPoint(const Vec3& from, const Vec3& direction, QueryReal distance)
{
    const Vec3 result = {
        queryStore(queryWide(from.x) + queryWide(direction.x) * distance),
        queryStore(queryWide(from.y) + queryWide(direction.y) * distance),
        queryStore(queryWide(from.z) + queryWide(direction.z) * distance)
    };
    return result;
}

inline Vec3 queryClippedDelta(const Vec3& from, const Vec3& direction,
                              QueryReal distance, const Vec3& clippedStart)
{
#if defined(_MSC_VER)
    const Vec3 end = queryPoint(from, direction, distance);
    const Vec3 result = { end.x - clippedStart.x, end.y - clippedStart.y, end.z - clippedStart.z };
#else
    // [PLATFORM] ELF retains each end coordinate across the vector subtraction.
    const Vec3 result = {
        queryStore((queryWide(from.x) + queryWide(direction.x) * distance) - queryWide(clippedStart.x)),
        queryStore((queryWide(from.y) + queryWide(direction.y) * distance) - queryWide(clippedStart.y)),
        queryStore((queryWide(from.z) + queryWide(direction.z) * distance) - queryWide(clippedStart.z))
    };
#endif
    return result;
}
} // anonymous namespace

// PE 0x00723BE0; ELF 0x08316EC0. The first radius call is deliberately discarded.
bool intersectPillWithObject(std::vector<Vec3>& hitPoints, std::vector<Vec3>& hitNormals,
                             IObject* object, Vec3 from, Vec3 direction, float radius,
                             float length, ObjectPredicator* predicator, int collisionLod)
{
    const Vec3& position = object->getAbsolutePosition();
    const Vec3 offset = { from.x - position.x, from.y - position.y, from.z - position.z };
    object->getPhysicBoundingRadius();
    if (!predicator->predicate(object))
        return false;
    if (collisionLod == -1)
        collisionLod = 0;
    const bool hasLod = object->hasCollisionLod(collisionLod);
    const bool soldier = object->getTemplate()->getClassID() == CID_SoldierTemplate;
    if (!hasLod && !soldier)
        return false;

    QueryReal nearDistance, farDistance;
    const float objectRadius = object->getPhysicBoundingRadius();
    if (!querySphereInterval(offset, direction, objectRadius, nearDistance, farDistance))
        return false;
    if (!(nearDistance < queryWide(length)) || !(farDistance > queryWide(0.0f)))
        return false;
    if (nearDistance < queryWide(0.0f)) nearDistance = queryWide(0.0f);
    if (farDistance > queryWide(length)) farDistance = queryWide(length);
    if (nearDistance == farDistance)
        return false;
    const Vec3 clippedStart = nearDistance > queryWide(0.0f)
        ? queryPoint(from, direction, nearDistance) : from;
    const Vec3 clippedDelta = queryClippedDelta(from, direction, farDistance, clippedStart);
    std::vector<float> planeDistances;
    std::vector<float> alongDistances;
#if defined(_MSC_VER)
    // [PLATFORM] PE captures the mesh before either transform getter; ELF reads it after both.
    ICollisionMesh* const mesh = static_cast<ICollisionMesh*>(
        object_detail::LayoutAccess::collisionMesh(object));
#endif
    const Mat4& inverse = object->getInvAbsoluteTransformation();
    const Mat4& absolute = object->getAbsoluteTransformation();
#if !defined(_MSC_VER)
    ICollisionMesh* const mesh = static_cast<ICollisionMesh*>(
        object_detail::LayoutAccess::collisionMesh(object));
#endif
    return mesh->getDistanceToSphere(collisionLod, true, absolute, inverse, clippedStart,
        clippedDelta, radius, hitNormals, hitPoints, planeDistances, alongDistances, 0);
}

// PE 0x00727390; ELF 0x08317C20. Success means broad-phase candidates existed,
// even if no narrow-phase test appends an intersection.
bool ObjectManager::intersectPill(std::vector<Vec3>& hitPoints, std::vector<Vec3>& hitNormals,
                                  const Vec3& from, const Vec3& direction, float radius,
                                  ObjectPredicator* predicator, bool includeDynamic)
{
    if (!predicator)
        return false;
    const float length = queryStore(querySqrt(queryLengthSquared(direction)));
    if (length == 0.0f)
        return false;
    const QueryReal inverseLength = queryWide(1.0f) / queryWide(length);
    const Vec3 unit = { queryStore(queryWide(direction.x) * inverseLength),
                        queryStore(queryWide(direction.y) * inverseLength),
                        queryStore(queryWide(direction.z) * inverseLength) };
    m_lineCandidates.resize(0);
    m_gridCuller->getObjects(from, direction, m_lineCandidates);
    if (includeDynamic)
        m_quadTreeCuller->getObjects(from, direction, m_lineCandidates);
    if (m_lineCandidates.empty())
        return false;

    const bool includeChildren = (predicator->getFlags() & 0x01000000u) == 0;
    m_objectTempVector.resize(0);
    const auto end = m_lineCandidates.end();
    for (auto it = m_lineCandidates.begin(); it != end; ++it)
    {
        IObject* const object = it->second;
        if (object)
        {
            m_objectTempVector.push_back(object);
            if (includeChildren)
                _getChildren(object, predicator->getFlags(), m_objectTempVector);
        }
    }
    const auto objectsEnd = m_objectTempVector.end();
    for (auto it = m_objectTempVector.begin(); it != objectsEnd; ++it)
        intersectPillWithObject(hitPoints, hitNormals, *it, from, unit, radius, length,
                                predicator, m_overrideLineTestLod);
    m_lineCandidates.resize(0);
    return true;
}

// PE 0x00727560; ELF 0x08318070. hitPart is never written; candidates remain cached.
bool ObjectManager::intersectLineWithSpheres(IObject*& hitObject, Vec3& hitPoint, Vec3& hitNormal,
                                             int& hitPart, const Vec3& from, const Vec3& direction,
                                             ObjectPredicator* include, ObjectPredicator* stop,
                                             float probabilityScale)
{
    (void)hitPart;
    m_objectTempVector.resize(0);
    const Vec3 origin = from;
#if !defined(_MSC_VER)
    // [PLATFORM] ELF also normalises an unused copy before querying the cullers.
    Vec3 unusedUnit = direction;
    normaliseQueryVector(unusedUnit);
#endif
    m_lineCandidates.resize(0);
    m_gridCuller->getObjects(origin, direction, m_lineCandidates);
    m_quadTreeCuller->getObjects(origin, direction, m_lineCandidates);
    std::sort(m_lineCandidates.begin(), m_lineCandidates.end(),
        [](const std::pair<float, IObject*>& a, const std::pair<float, IObject*>& b) {
            return a.first < b.first;
        });
    if (m_lineCandidates.empty())
        return false;
    hitObject = 0;
    Vec3 unit = direction;
    normaliseQueryVector(unit);
    std::vector<IObject*> objects;
    for (auto it = m_lineCandidates.begin(); it != m_lineCandidates.end(); ++it)
    {
        objects.resize(0);
        IObject* const object = it->second;
        if (include && (include->getFlags() & 0x01000000u) != 0)
        {
            if (include->predicate(object)) objects.push_back(object);
        }
        else
            getChilds(object, objects, include, stop);
        for (auto child = objects.begin(); child != objects.end(); ++child)
        {
            IObject* const candidate = *child;
            const float radius = candidate->getPhysicBoundingRadius();
            if (radius <= 0.0f)
                continue;
            const Vec3& position = candidate->getAbsolutePosition();
            // shipped defect: direction, rather than origin, is subtracted here and
            // used as the unnormalised projection axis.
            QueryReal offsetX = queryWide(position.x) - queryWide(direction.x);
            QueryReal offsetY = queryWide(position.y) - queryWide(direction.y);
            QueryReal offsetZ = queryWide(position.z) - queryWide(direction.z);
            const Vec3 offset = { queryStore(offsetX), queryStore(offsetY), queryStore(offsetZ) };
#if defined(_MSC_VER)
            // [PLATFORM] PE stores all offset coordinates before taking the dot product.
            offsetX = queryWide(offset.x);
            offsetY = queryWide(offset.y);
            offsetZ = queryWide(offset.z);
#endif
            const QueryReal projection = (offsetY * queryWide(direction.y)
                + offsetZ * queryWide(direction.z)) + offsetX * queryWide(direction.x);
            const QueryReal x = queryWide(unit.x) * projection;
            const QueryReal y = queryWide(unit.y) * projection;
            const QueryReal z = queryWide(unit.z) * projection;
            const QueryReal ratio = ((z * z + y * y) + x * x)
                / (queryWide(radius) * queryWide(radius));
            if (!(ratio < queryWide(1.0f)))
                continue;
            QueryReal threshold = ((queryWide(1.0f) - ratio) * queryWide(0.75f)
                + queryWide(0.25f)) * queryWide(probabilityScale);
#if defined(_MSC_VER)
            // [PLATFORM] PE stores the threshold before calling Random::getRaw.
            threshold = queryWide(queryStore(threshold));
#endif
            const unsigned int random = Rand.getRaw();
            const float sample = static_cast<float>(((random >> 7) | 1u) + 1u);
            if (!(queryWide(sample) * queryWide(5.9604644775390625e-8f) > threshold))
                continue;
            hitObject = candidate;
            hitPoint = candidate->getAbsolutePosition();
            hitNormal = { -offset.x, -offset.y, -offset.z };
            normaliseQueryVector(hitNormal, false);
            const float normalX = hitNormal.x;
            hitNormal.x = -hitNormal.z;
            hitNormal.z = normalX;
            return true;
        }
    }
    return false;
}

// PE 0x00727850; ELF 0x08318940. Returns the first mesh hit in culler/hierarchy order.
// The optional part output is unused in both images.
bool ObjectManager::raycast(const Vec3& from, const Vec3& direction, ObjectPredicator* include,
                            ObjectPredicator* stop, unsigned int* outPart)
{
    (void)outPart;
    const Vec3 origin = from;
    const float length = queryStore(querySqrt(queryLengthSquared(direction)));
    if (length == 0.0f)
        return false;
    const QueryReal inverseLength = queryWide(1.0f) / queryWide(length);
    const Vec3 unit = { queryStore(queryWide(direction.x) * inverseLength),
                        queryStore(queryWide(direction.y) * inverseLength),
                        queryStore(queryWide(direction.z) * inverseLength) };
    m_lineCandidates.resize(0);
    m_gridCuller->getObjects(origin, direction, m_lineCandidates);
    m_quadTreeCuller->getObjects(origin, direction, m_lineCandidates);
    if (m_lineCandidates.empty())
        return false;
    Vec3 clippedStart = { 0.0f, 0.0f, 0.0f };
    Vec3 normal = { 0.0f, 0.0f, 0.0f };
    Vec3 point = { 0.0f, 0.0f, 0.0f };
    float planeDistance, alongDistance;
    std::vector<IObject*> objects;
    for (auto it = m_lineCandidates.begin(); it != m_lineCandidates.end(); ++it)
    {
        objects.resize(0);
        IObject* const object = it->second;
        if (include && (include->getFlags() & 0x01000000u) != 0)
        {
            if (include->predicate(object)) objects.push_back(object);
        }
        else
            getChilds(object, objects, include, stop);
        for (auto child = objects.begin(); child != objects.end(); ++child)
        {
            IObject* const candidate = *child;
            if (!candidate->hasCollisionLod(0))
                continue;
            ICollisionMesh* const mesh = static_cast<ICollisionMesh*>(
                object_detail::LayoutAccess::collisionMesh(candidate));
            const Vec3& position = candidate->getAbsolutePosition();
            const Vec3 offset = { from.x - position.x, from.y - position.y, from.z - position.z };
            QueryReal nearDistance, farDistance;
            const float radius = candidate->getVisualBoundingRadius();
            if (!querySphereInterval(offset, unit, radius, nearDistance, farDistance)
                || !(nearDistance < queryWide(length)) || !(farDistance > queryWide(0.0f)))
                continue;
            if (nearDistance < queryWide(0.0f)) nearDistance = queryWide(0.0f);
            if (farDistance > queryWide(length)) farDistance = queryWide(length);
            if (nearDistance == farDistance)
                continue;
            clippedStart = nearDistance > queryWide(0.0f) ? queryPoint(from, unit, nearDistance) : from;
            const Vec3 delta = queryClippedDelta(from, unit, farDistance, clippedStart);
            const Mat4& inverse = candidate->getInvAbsoluteTransformation();
            const Mat4& absolute = candidate->getAbsoluteTransformation();
            if (mesh->getDistance(0, false, absolute, inverse, clippedStart, delta,
                                  normal, point, planeDistance, alongDistance, 0, 0, true, false))
            {
                m_lineCandidates.resize(0);
                return true;
            }
        }
    }
    m_lineCandidates.resize(0);
    return false;
}

std::vector<IObject*>& ObjectManager::getObjectTempVector() { return m_objectTempVector; }

// Ignores null; destroys uncached objects immediately or queues cached objects for deferred destruction.
// PE 0x00724590; ELF 0x08311280. Cached soldiers must have no kit before queueing.
void ObjectManager::destroyObject(IObject* object)
{
    typedef object_detail::LayoutAccess L;

    if (!object)
        return;

    if (object->queryInterface(IID_ISoldier))
    {
        IPlayerControlObject* pco = 0;
        for (IObject* o = L::parent(object); o; o = L::parent(o))
        {
            pco = static_cast<IPlayerControlObject*>(o->queryInterface(IID_IPlayerControlObject));
            if (pco)
                break;
        }
        if (pco)
            pco->removeChildVehicle(object);                                // PCO +0x148
    }

    if (!object->getObjectCacheID())
    {
        object->destroy(true);                                              // slot 13 - immediate
        return;
    }

    IObject* parent = L::parent(object);
    if (parent)
    {
        if (parent->getObjectCacheID() == object->getObjectCacheID())
            return;
        parent->removeChild(object);                                        // slot 62
    }

    if ((L::flags(object) & 0x20u) == 0)
    {
        ISoldier* soldier = static_cast<ISoldier*>(object->queryInterface(IID_ISoldier));
        if (soldier && soldier->getKit())
            std::abort();
        removeObjectFromGrid(object);                                       // +0x108, virtual
        if (object->getNetworkable())
            eventManager->postEvent(ECNetwork, 17, object, 0.0f);           // EventManager +0x14
        object->setObjectInDestroyList(true);                               // slot 16
        m_currentDestroyList->push_back(object);
    }
}
// Returns the object matching a case-insensitive name hash lookup, or null.
IObject* ObjectManager::getObjectFromNameHash(const std::string& name)
{
    IObject* object = 0;
    m_nameHashes.get(name, object);                          // +0x24
    return object;
}
void ObjectManager::refreshGridObject(IObject* object)
{
    if (!objectPendingGridUpdate(object))
    {
        objectPendingGridUpdate(object) = 1;
        m_objectsToUpdateInGrid.push_back(object->getWeakPtr());
    }
}
// ELF 0x8310B90 / PE 0x6E0680: unsigned-key find, without inserting a missing entry.
IObject* ObjectManager::getRegisteredObject(int id)
{
    const auto found = m_allRegisteredObjects.find(static_cast<unsigned int>(id));
    return found == m_allRegisteredObjects.end() ? 0 : found->second;
}
// Destroys registered objects in ordered passes: cached objects, detached parents, then remaining roots.
// Drain queued destruction between passes; registry iterators stay live during the walks.
// A surviving parent or nonempty registry/update set/culler is fatal before reset completes.
// Clears the primary static culler, verifies the other four are empty, and resets IDs and active state.
// PE 0x00728400; ELF 0x08310C00. The remaining registry/update/culler checks are shipped invariants.
void ObjectManager::deleteAllRegisteredObjects()
{
    m_clientUpdateTick    = 0;                                           // +0x10C
    m_deletingAllObjects  = true;                                        // +0x90

    for (std::map<unsigned int, IObject*>::iterator it = m_allRegisteredObjects.begin();
         it != m_allRegisteredObjects.end(); ++it)
    {
        IObject* const object = it->second;
        if (object && object->getObjectCacheID())                        // vt +0x2C
            destroyObject(object);                                       // vt +0x14
    }
    destroyObjects();                                                    // vt +0x98
    m_objectCache->deleteAll();                                          // unguarded, as shipped
    destroyObjects();

    for (std::map<unsigned int, IObject*>::iterator it = m_allRegisteredObjects.begin();
         it != m_allRegisteredObjects.end(); ++it)
    {
        IObject* const object = it->second;
        if (object)
        {
            IObject* const parent = object_detail::LayoutAccess::parent(object);   // +0x28, raw
            if (parent)
                parent->removeChild(object);                             // vt +0xF8
        }
    }

    for (std::map<unsigned int, IObject*>::iterator it = m_allRegisteredObjects.begin();
         it != m_allRegisteredObjects.end(); ++it)
    {
        IObject* const object = it->second;
        if (!object->isObjectDestroyed())                                // vt +0x3C  no null test here
        {
            if (object_detail::LayoutAccess::parent(object))
                Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 574)
                    << "hierarchy error: object still has a parent while deleting all registered objects";
            object->forceDestroy();                                      // vt +0x38
        }
    }
    destroyObjects();

    if (!m_allRegisteredObjects.empty())                                 // +0x98 - the binary aborts
        std::abort();

    m_nameHashes.clear();                                                // +0x24

    if (!m_updateObjects.empty() || !m_frameUpdateObjects.empty())       // +0x48 / +0x54
        std::abort();

    m_objectsToUpdateInGrid.resize(0);                                   // +0x84

    if (m_gridCuller)                                                    // +0x10
        m_gridCuller->clear();                                           // vt +0x2C
    if ((m_quadTreeCuller && m_quadTreeCuller->getNumberOfObjects())
        || (m_noCollisionGridCuller && m_noCollisionGridCuller->getNumberOfObjects())
        || (m_noCollisionQuadTreeCuller
            && m_noCollisionQuadTreeCuller->getNumberOfObjects())
        || (m_latencyCompensationCuller
            && m_latencyCompensationCuller->getNumberOfObjects()))
        std::abort();

    m_deletingAllObjects  = false;                                       // +0x90
    m_newId               = 0;                                           // +0xE4
    m_activeObject        = 0;                                           // +0x20
    m_gridCullerOptimized = false;                                       // +0x0C
}
// Indexes object names case-insensitively. The first duplicate name wins; removal
// unlinks and frees the first match, doing nothing when absent.
void ObjectManager::addObjectToNameHash(IObject* object)
{
    const std::string& name = object->getName();                         // vt +0x64
    const unsigned int bucket = m_nameHashes.bucketOf(name);
    if (!m_nameHashes.exists(name, bucket))
        m_nameHashes.insert(name, object, bucket);
}

void ObjectManager::removeObjectFromNameHash(IObject* object)
{
    m_nameHashes.remove(object->getName());                              // vt +0x64
}
// During frame-update traversal, queues changes without deduplication and applies all
// adds before removes afterward. Outside traversal, updates the object-id map immediately.
void ObjectManager::addObjectToFrameUpdate(IObject* object)
{
    if (m_inFrameUpdate)
    {
        m_frameUpdateAddList.push_back(object);
        return;
    }
    m_frameUpdateObjects[object->getObjectId()] = object;
}

void ObjectManager::removeObjectFromFrameUpdate(IObject* object)
{
    if (m_inFrameUpdate)
    {
        m_frameUpdateRemoveList.push_back(object);
        return;
    }
    m_frameUpdateObjects.erase(object->getObjectId());
}
void ObjectManager::addObjectToGrid(IObject* object)
{
    if ((object->getFlags() & 0x01000000) == 0)
    {
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 1285, __FUNCTION__)
            << "Tried to add a non root parent object to the grid: "
            << object->getTemplate()->getName();
    }
    refreshGridObject(object);
}
// Writes requested camera outputs; null output pointers are ignored.
// [PLATFORM] Dedicated returns identity/1/1. Client reads render-view transform, far plane and far/FOV,
// querying the render view separately for each output.
void ObjectManager::getCameraInfo(Mat4* transform, float* farPlane, float* viewDistance)
{
#if DEDICATED
    if (transform)
        *transform = Mat4::getIdentity();
    if (farPlane)
        *farPlane = 1.0f;
    if (viewDistance)
        *viewDistance = 1.0f;
#else
    if (transform)
        *transform = *static_cast<dx9rend::IRenderer*>(::dice::hfe::g_renderer)
                          ->getRenderView()->getViewMatrixPtr();                 // +0x48, +0x64
    if (farPlane)
        *farPlane = static_cast<dx9rend::IRenderer*>(::dice::hfe::g_renderer)
                        ->getRenderView()->getFarPlane();                         // +0x3C
    if (viewDistance)
    {
        const float fov = static_cast<dx9rend::IRenderer*>(::dice::hfe::g_renderer)
                              ->getRenderView()->getFov();                        // +0x20
        *viewDistance = static_cast<dx9rend::IRenderer*>(::dice::hfe::g_renderer)
                            ->getRenderView()->getFarPlane() / fov;              // +0x3C
    }
#endif
}

// Simulation updates walk enabled objects in key order and filter by update-frequency type.
// The client additionally distance-culls most objects to every fifteenth tick. Deferred removals
// run through the virtual entry point after the latch clears, then destruction and due messages run.
namespace {
float* const g_cullHandleUpdateOnClientFactor =
    Vars::getFloat("objectManager-cullHandleUpdateOnClientFactor", 0.5f);   // ds:0x8AB0A7C, static-init _342 of this TU
} // namespace

void ObjectManager::addObjectToUpdate(IObject* object)
{
    m_updateObjects[object->getObjectId()] = object;             // IObject +0x10; lower_bound + insert_unique, then the store
}

void ObjectManager::removeObjectFromUpdate(IObject* object)
{
    if (m_isUpdatingObjects)
    {
        m_removeFromUpdateList.push_back(object);
        return;
    }
    m_updateObjects.erase(object->getObjectId());
}

void ObjectManager::updateObjects(float deltaTime, UpdateFreqencyType type, unsigned int flags)
{
    typedef object_detail::LayoutAccess L;
    m_isUpdatingObjects = true;
    if (gameLogic && gameLogic->isServer())
    {
        for (std::map<unsigned int, IObject*>::iterator it = m_updateObjects.begin();
             it != m_updateObjects.end(); )
        {
            std::map<unsigned int, IObject*>::iterator next = it;
            ++next;                                             // taken before the call, as shipped
            IObject* object = it->second;
            if (!(L::flags(object) & 1u) && object->getUpdateFrequencyType() == type)
                object->handleUpdate(deltaTime, flags);         // slot 44
            it = next;
        }
    }
    else
    {
        const unsigned int tick = WorldPref::getTickCount();
        Mat4 camera = Mat4::getIdentity();
        float viewDistance = 0.0f;
        getCameraInfo(&camera, 0, &viewDistance);               // the client's camera
        float radius = *g_cullHandleUpdateOnClientFactor * viewDistance;
        if (radius >= 200.0f)
        {
            if (radius > 2000.0f)
                radius = 2000.0f;
        }
        else
            radius = 200.0f;
        const float radiusSq = radius * radius;
        for (std::map<unsigned int, IObject*>::iterator it = m_updateObjects.begin();
             it != m_updateObjects.end(); )
        {
            std::map<unsigned int, IObject*>::iterator next = it;
            ++next;
            IObject* object = it->second;
            if (!(L::flags(object) & 1u) && object->getUpdateFrequencyType() == type)
            {
                const unsigned int cid = object->getTemplate()->getClassID();
                const Vec3& p = object->getAbsolutePosition();
                const float dx = camera.m[12] - p.x;
                const float dy = camera.m[13] - p.y;
                const float dz = camera.m[14] - p.z;
                if (radiusSq > dy * dy + dx * dx + dz * dz
                    || cid == CID_EngineTemplate
                    || cid == CID_GenericFireArmTemplate
                    || cid == CID_EffectBundleTemplate
                    || cid == CID_AirDraftEffectBundleTemplate
                    || cid == CID_WheelEffectBundleTemplate
                    || tick >= m_clientUpdateTick)
                {
                    object->handleUpdate(deltaTime, flags);
                }
            }
            it = next;
        }
        if (tick >= m_clientUpdateTick)
            m_clientUpdateTick = tick + 15;
    }
    m_isUpdatingObjects = false;
    for (unsigned int i = 0; i < m_removeFromUpdateList.size(); ++i)
        removeObjectFromUpdate(m_removeFromUpdateList[i]);      // virtual, as shipped
    m_removeFromUpdateList.resize(0);
    destroyObjects();                                           // virtual +0x98
    checkMessages(deltaTime);
}

// the three shipped asserts (a cached object, a flag-0x20 object, or one that has
// not been destroy()ed cannot be queued - abort), mark, append to the current buffer.
// PE 0x00724690; ELF 0x08311470.
void ObjectManager::addObjectToDestroyList(IObject* object)
{
    if ((object_detail::LayoutAccess::flags(object) & 0x20u) != 0
        || object->getObjectCacheID() != 0
        || !object->isObjectDestroyed())
        std::abort();
    object->setObjectInDestroyList(true);
    m_currentDestroyList->push_back(object);
}

// nothing to do while the current buffer is empty; else flip the index, point
// the current pointer at the other buffer (so anything queued during the drain lands there),
// drain: every object with flag 0x200 either goes to the object cache (a cache id: pushObject with
// its networkable's id, or 0) or is destroyed for real (destroyObject -> Object::
// internalDestroyObject: parent unlink, children, the flags, release), and the active object is
// forgotten if it was one of them; resize(0) the drained buffer; then recurse - the drain may have
// queued more, and the recursion ends when a drain finds the current buffer empty.
void ObjectManager::destroyObjects()
{
    std::vector<IObject*>* list = m_currentDestroyList;
    if (list->empty())
        return;
    m_destroyListIndex = (m_destroyListIndex + 1) % 2;
    m_currentDestroyList = &m_destroyLists[m_destroyListIndex];
    for (unsigned int i = 0; i < list->size(); ++i)
    {
        IObject* object = (*list)[i];
        if ((object_detail::LayoutAccess::flags(object) & 0x200u) != 0)
        {
            if (object->getObjectCacheID())
            {
                unsigned int networkableId = 0;
                if (object->getNetworkable())
                    networkableId = object->getNetworkable()->m_networkableId;   // INetworkable +0x04
                m_objectCache->pushObject(object, networkableId);   // Real since
            }
            else
            {
                object->destroyObject();                        // slot 86 -> internalDestroyObject
            }
            if (m_activeObject == object)
                m_activeObject = 0;
        }
    }
    list->resize(0);
    destroyObjects();                                           // virtual, as shipped
}

// a null target is a type-6 Debug (process-fatal, and the ELF then dereferences
// it anyway); a destroyed target is silently dropped; else insert {clock + delay, weak target,
// message} before the first entry due later (stable among equals).
void ObjectManager::postMessage(IObject* to, TemplateMessage message, float delay)
{
    if (!to)
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectManager.cpp", 3137, "")
            << "trying to postMessage to NULL object";
    if (to->isObjectDestroyed())
        return;
    const double due = m_lastMessageCheckTime + delay;
    std::list<Message>::iterator pos = m_messages.begin();
    while (pos != m_messages.end() && !(pos->time > due))
        ++pos;
    Message entry;
    entry.time = due;
    entry.target = to->getWeakPtr();
    entry.message = message;
    m_messages.insert(pos, entry);
}

// Removes all messages to the target and expired weak targets, preserving other entries.
void ObjectManager::removeAllMessagesTo(IObject* to)
{
    for (std::list<Message>::iterator it = m_messages.begin(); it != m_messages.end(); )
    {
        IObject* const target = it->target.get();
        if (!target || target == to)
            m_messages.erase(it++);
        else
            ++it;
    }
}

// advance the clock, deliver from the front while due; a dead target is dropped.
void ObjectManager::checkMessages(float deltaTime)
{
    m_lastMessageCheckTime += deltaTime;
    while (!m_messages.empty())
    {
        Message& front = m_messages.front();
        IObject* target = front.target.get();
        if (!target)
        {
            m_messages.erase(m_messages.begin());
            continue;
        }
        if (front.time > m_lastMessageCheckTime)
            return;
        const TemplateMessage message = front.message;
        m_messages.erase(m_messages.begin());
        target->handleMessage(message, 0, false);               // slot 65
    }
}

// Creates from a nonnull template, using eligible cached objects outside precaching.
// On a miss, creates, optionally transforms, and initializes the object. Cache-created
// objects are marked; id selects cached objects only and is ignored by fresh creation.
namespace {
bool g_checkObjectCacheDisabledVariable = true;   // the function-static "read the setting once" latch
bool g_objectCacheDisabled = false;               // ds:0x8AB0A80 - the file static the setting sets
} // namespace

// PE 0x00723220; ELF 0x083110B0. Null templates fail the shipped invariant.
IObject* ObjectManager::createObject(const IObjectTemplate* objectTemplate, unsigned int id, const Mat4* transform)
{
    if (!objectTemplate)
        std::abort();
    bool useCache = ObjectTemplate::getIsPreCaching() == false;
    if (g_checkObjectCacheDisabledVariable)
    {
        g_checkObjectCacheDisabledVariable = false;
        const bool defaultTrue = true;
        if (!settings->boolGet(std::string("GSUseObjectCache"), defaultTrue))   // +0x50
            g_objectCacheDisabled = true;
    }
    if (g_objectCacheDisabled)
        useCache = false;
    if (objectTemplate->getClassID() == CID_OverheadCameraTemplate
        || objectTemplate->getClassID() == CID_ZiplineTemplate
        || objectTemplate->getClassID() == CID_GrapplingHookRopeTemplate)
        useCache = false;

    // The cache is constructed with the manager and must exist before this lookup.
    IObject* object = 0;
    if (useCache)
        object = m_objectCache->popObject(objectTemplate, id, transform);
    if (!object)
    {
        object = objectTemplate->createObject();                               // +0x54 - the per-class factory
        if (object)
        {
            if (transform)
                object->setAbsoluteTransformation(*transform);                 // slot 35
            object->init();                                                    // slot 8
            if (useCache)
                m_objectCache->markObjectAsCreatedInCache(object);
        }
    }
    return object;
}

// The four non-virtual helpers the recovered bodies call.
void ObjectManager::drawStats() {}
// PE 0x00721790; ELF 0x08312990. Existing membership is a shipped invariant failure.
void ObjectManager::addObjectToCuller(IObject* object)
{
    if (object->getGridCullerObject())
        std::abort();
    if (object->isObjectDestroyed())
        return;

    const unsigned int flags = object->getFlags();
    ICuller* culler = 0;
    if ((flags & 0x100u) != 0)
    {
        if (!m_gridCullerOptimized && (flags & 8u) == 0)
        {
            culler = m_gridCuller;
            if (hasNonIdentityScale(object))
                setBoundRadiusIsDirty(object);
        }
        else
        {
            culler = m_quadTreeCuller;
        }
    }
    else
    {
        if (!m_gridCullerOptimized && (flags & 8u) == 0)
        {
            culler = m_noCollisionGridCuller;
            if (hasNonIdentityScale(object))
                object->updateFlags(0x40000u, 0);
        }
        else
        {
            culler = m_noCollisionQuadTreeCuller;
        }
    }

    const float visualRadius = object->getVisualBoundingRadius();
    const Vec3 position = objectPosition(object);
    object->setGridCullerObject(culler->createObject(position, visualRadius, object));

    if (LatencyCompensation::m_isEnabled
        && culler == m_quadTreeCuller
        && (object->getNetworkable()
            || object->queryInterface(IID_IPlayerControlObject)))
    {
        const float speed = objectSpeed(object);
        const float latencyDistance = LatencyCompensation::getMaxLatency() * speed;
        const float radius = object->getPhysicBoundingRadius() + latencyDistance + 0.5f;
        const Vec3 latencyPosition = objectPosition(object);
        object->setLatencyCullerObject(
            m_latencyCompensationCuller->createObject(latencyPosition, radius, object));
    }
}

// PE 0x00720F20; ELF 0x08312D70. Missing membership is a shipped invariant failure.
void ObjectManager::updateObjectInCuller(IObject* object)
{
    if (object->isObjectDestroyed())
        return;

    CullerObject* cullerObject = object->getGridCullerObject();
    if (!cullerObject)
        std::abort();

#if defined(_MSC_VER)
    // [PLATFORM] PE obtains the transformation before querying the radius; ELF reverses them.
    const Mat4& transform = object->getAbsoluteTransformation();
    const float visualRadius = object->getVisualBoundingRadius();
    const Vec3 position = { transform.m[12], transform.m[13], transform.m[14] };
#else
    const float visualRadius = object->getVisualBoundingRadius();
    const Vec3 position = objectPosition(object);
#endif
    object->setGridCullerObject(cullerObject->updateObject(position, visualRadius));

    CullerObject* latencyObject = object->getLatencyCullerObject();
    if (latencyObject)
    {
        const float speed = objectSpeed(object);
        const float latencyDistance = LatencyCompensation::getMaxLatency() * speed;
        const float radius = object->getPhysicBoundingRadius() + latencyDistance + 0.5f;
        const Vec3 latencyPosition = objectPosition(object);
        object->setLatencyCullerObject(latencyObject->updateObject(latencyPosition, radius));
    }
}

void ObjectManager::removeObjectFromCuller(IObject* object)
{
    objectPendingGridUpdate(object) = 0;

    CullerObject* cullerObject = object->getLatencyCullerObject();
    if (cullerObject)
    {
        cullerObject->destroyObject();
        object->setLatencyCullerObject(0);
    }

    cullerObject = object->getGridCullerObject();
    if (cullerObject)
    {
        cullerObject->destroyObject();
        object->setGridCullerObject(0);
    }
}

// Requires live objects to be drained, then destroys the cache and releases five cullers
// in order. Member containers release their own storage.
// PE 0x00729190; ELF 0x0830F650. Nonempty cullers and outstanding references remain fatal.
ObjectManager::~ObjectManager()
{
    if (!m_allRegisteredObjects.empty()
        || m_quadTreeCuller->getNumberOfObjects()
        || m_noCollisionGridCuller->getNumberOfObjects()
        || m_noCollisionQuadTreeCuller->getNumberOfObjects()
        || m_latencyCompensationCuller->getNumberOfObjects())
        std::abort();

    if (m_objectCache)
    {
        m_objectCache->deleteAll();                    // the attested dtor arm
        delete m_objectCache;
        m_objectCache = 0;
    }

    if ((m_gridCuller && m_gridCuller->release() != 0)
        || (m_quadTreeCuller && m_quadTreeCuller->release() != 0)
        || (m_noCollisionGridCuller && m_noCollisionGridCuller->release() != 0)
        || (m_noCollisionQuadTreeCuller && m_noCollisionQuadTreeCuller->release() != 0)
        || (m_latencyCompensationCuller && m_latencyCompensationCuller->release() != 0))
        std::abort();
}

} // namespace world
} // namespace hfe
} // namespace dice
