// Maintains response lists and runs collision checks, impulses and friction in frame order.
// The original source path is Physics/ResponseManager.cpp.
// reset() advances collisionCheckedCounter; it does not clear object state. Matching stamps suppress
// duplicate collision pairs until the next generation.

#include "ResponsePhysicsManager.h"

#include <string.h>

#include "IResponsePhysics.h"          // The header this file's stubs waited on since
#include "CollisionStats.h"            // world::collisionStatsData - the shared 0x3C block
#include "../Core/Ids.h"
#include "../Core/Debug.h"                    // removeObject's "no physics node" warning
#include "../Game/Objects/IObject.h"                    // getRootParent
#include "../Game/Objects/Game/Soldier.h"               // the soldier-only pose calls
#include "../Game/Objects/ObjectLayoutAccess.h"
#include <math.h>                     // sqrtf - getCollisionObjects / checkMeshObjectVsObjects
#include <cstdio>      // [TEMP] the soldier/turret trace
#include <stdlib.h>                   // abort - checkPointObjectVsObjects (shipped) and the stop
#include <vector>

#include "IPhysicsNode.h"             // the per-object node: speed, sleep, separate-update, lods
#include "ICollisionMesh.h"           // getBoundingRadius / getBoundingBox / getVertexCount
#include "HeightmapCluster.h"         // isInsideHeightmapCluster - the out-of-world test
#include "LatencyCompensation.h"      // the rewound-grid arm of getCollisionObjects
#include "../Game/IGameLogic.h"       // gameLogic->isServer() - the culler switch
#include "../Game/Objects/IObjectTemplate.h"           // CollisionTestCuller: the projectile exemption
#include "../Math/LocalBoundingBox.h" // isInside / testIntersectWithOBB - checkMeshObjectVsObjects
#include "../Math/Mat4.h"
#include "../Scene/Object/ObjectManager.h"             // the grid queries, postMessage, getCameraInfo
#include "../Scene/Object/ObjectPredicator.h"          // ObjectFlagPredicator(0x1000100)

namespace dice {
namespace hfe {
namespace world {

// Counts collision checks for the shared response-physics statistics.
int collisionCheckedCounter = 0;                       // ds:0x898BC44

// ds:0x8ABAF24 world::heightmapCluster - Core/Globals.cpp, declared IBase* there and cast at the
// point of use, the way every Physics/ TU reaches it (checkPointObjectVsObjects' out-of-world test).
extern IBase* heightmapCluster;
class IPlayer;

// [INFERRED names] - the two Vec3 debug vectors immediately following CollisionMeshTemplate::
// m_temporaryFaces. Their static initialisation/destruction belongs to CollisionMeshTemplate.cpp
// in both ports; this walker clears them and CollisionManager::drawDebugPrimitives consumes them.
extern std::vector<Vec3> collisionTestedFaces;
extern std::vector<Vec3> collisionIntersectFaces;


// Shared broad-phase settings, candidate caches and OBB counters; not reentrant.
// [CHOSEN] names describe use, and values replace pointer pairs that are never rebound.
// Unused settings remain [UNRESOLVED].
namespace {
float s_cullRadiusScale     = 0.06f;
float s_unresolvedFloatB4   = 3.0f;                     // 0x8ABABB4  [UNRESOLVED]
float s_cullMinRadius       = 25.0f;
int   s_unresolvedIntC4     = 0;                        // 0x8ABABC4  [UNRESOLVED]
int   s_useBoxPrefilter     = 1;
int   s_obbTestMode         = 1;
float s_obbDistanceFactor   = 0.57499999f;
int   s_obbMinVertexCount   = 10;
float s_maxSweepSpeed       = 1500.0f;
std::vector<IObject*> s_collisionObjects;
std::vector<IObject*> s_soldierCandidates;
int   s_obbStageEntered     = 0;                        // 0x8ABAC0C  [CHOSEN] name
int   s_obbStagePassed      = 0;                        // 0x8ABAC10  [CHOSEN] name

// Client collision culler; true means skip. Servers and locally created objects always remain.
// Uses camera distance, exempting projectiles when the radius is nonpositive.
struct CollisionTestCuller
{
    bool  m_enabled;                                    // +0x00  = !isServer
    Vec3  m_cameraPosition;                             // +0x04
    float m_radiusSq;                                   // +0x10  = (s_cullRadiusScale * fov)^2

    bool cull(IObject* object, float radius) const
    {
        typedef object_detail::LayoutAccess L;

        if (!m_enabled || object->isCreatedByLocalPlayer())          // IObject slot 84
            return false;

        if (radius > 0.0f)
        {
            const Vec3& p = object->getAbsolutePosition();           // slot 27
            const float dx = p.x - m_cameraPosition.x;
            const float dy = p.y - m_cameraPosition.y;
            const float dz = p.z - m_cameraPosition.z;
            return dz * dz + dy * dy + dx * dx > radius * radius;
        }

        const IObjectTemplate* t = L::objectTemplate(object);        // +0x14, read raw
        if (t && t->getClassID() == CID_GenericProjectileTemplate)
            return false;

        const Vec3& p = object->getAbsolutePosition();
        const float dx = p.x - m_cameraPosition.x;
        const float dy = p.y - m_cameraPosition.y;
        const float dz = p.z - m_cameraPosition.z;
        float r2 = s_cullMinRadius * s_cullMinRadius;
        if (r2 <= m_radiusSq)
            r2 = m_radiusSq;
        return dz * dz + dy * dy + dx * dx > r2;
    }
};
} // namespace

// Returns the first object with response physics and flag 0x100, checking self then
// descendants depth-first. Returns null when none qualifies.
IObject* getFirstObjectWithResponse(IObject* object)
{
    typedef object_detail::LayoutAccess L;

    while (object)
    {
        if (L::responsePhysics(object) && (L::flags(object) & 0x100) != 0)
            return object;
        IObject* const found = getFirstObjectWithResponse(object->getChild());
        if (found)
            return found;
        object = L::sibling(object);
    }
    return 0;
}

// world::getNextChild(IResponsePhysics*) - 0x8370560, 46 B, read in full. The walkers' step along
// a response's next-to-check chain: the next response's object, or 0 at the chain's end.
IObject* getNextChild(IResponsePhysics* response)
{
    if (!response)
        return 0;
    IResponsePhysics* const next = response->getNextToCheck();      // +0x60
    return next ? next->getObject() : 0;                            // +0x40
}

// IProjectileObject subtable slot +0x54 = GenericProjectile::getPlayer
// (the owning player's weak block, +0x174); the shared definition lives in
// Game/Objects/Projectiles/GenericProjectile.cpp. Camera.cpp declares the same seam.
IPlayer* projectileOwner(void* projectile);                         // defined in GenericProjectile.cpp

// Starts with one reference, five empty response lists, and a dirty object cache so
// the first object-collision pass rebuilds it.
ResponsePhysicsManager::ResponsePhysicsManager()
    : m_refCount(1)
    , m_cachedCollisionObjectsDirty(true)
    , m_startFrameMarker(0)
{
    m_responsePhysicsList.head                 = 0;
    m_responsePhysicsList.tail                 = 0;
    m_pointResponsePhysicsList.head            = 0;
    m_pointResponsePhysicsList.tail            = 0;
    m_rotationalPointResponsePhysicsList.head  = 0;
    m_rotationalPointResponsePhysicsList.tail  = 0;
    m_soldierResponsePhysicsList.head          = 0;
    m_soldierResponsePhysicsList.tail          = 0;
    m_childResponsePhysicsList.head            = 0;
    m_childResponsePhysicsList.tail            = 0;
}

// Deletes only the frame marker. Response lists borrow object-owned responses and must
// not delete or release them during manager destruction.
ResponsePhysicsManager::~ResponsePhysicsManager()
{
    if (m_startFrameMarker)
        delete m_startFrameMarker;      // ELF: operator delete(void*), 0x836FE2x / 0x836FE6x
}

// IBase quartet - identical in shape to PhysicsNodeManager's, as Core/IBase.h predicts.

// The ELF leaves the new count in eax; IBase::addRef is void and frozen.
void ResponsePhysicsManager::addRef()
{
    ++m_refCount;
}


int ResponsePhysicsManager::getRef()
{
    return m_refCount;
}

// Same shipped `this` null test as every sibling release() in this
// subsystem - dead in defined C++, preserved to match retail. The dispatch is
// `call [vptr+0x14]`, the deleting-destructor slot, i.e. `delete this`.
int ResponsePhysicsManager::release()
{
    int count = --m_refCount;
    if (count == 0 && this != 0)
        delete this;
    return count;
}

// Branchless leaf form.
void* ResponsePhysicsManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<ResponsePhysicsManager*>(this);
    if (iid == IID_IResponsePhysicsManager)
        return const_cast<ResponsePhysicsManager*>(this);
    return 0;
}

// Resets and queues an object's response when its flags and root membership permit it.
// Child responses use the child list unless their root is already listed. Parentless static responses
// are reset but not queued. Preserve the repeated getClassID calls and their order.
void ResponsePhysicsManager::addObject(IObject* obj)
{
    typedef object_detail::LayoutAccess L;

    if (!(L::flags(obj) & 0x100))                        // IObject +0x04 bit 8: "has response"
        return;
    IResponsePhysics* rp = static_cast<IResponsePhysics*>(L::responsePhysics(obj));
    if (!rp)
        return;

    if (L::parent(obj))                                  // a child object -> the tag-5 list,
    {                                                    //   unless its root already speaks
        IResponsePhysics* rootRp =
            static_cast<IResponsePhysics*>(L::responsePhysics(getRootParent(obj)));
        if (rootRp && rootRp->isInList())
            return;
        rp->reset();
        rp->pushBackOnList(m_childResponsePhysicsList, 5);
        return;
    }

    rp->reset();
    if (rp->getClassID() == CID_ResponsePhysics)
        rp->pushBackOnList(m_responsePhysicsList, 1);
    else if (rp->getClassID() == CID_PointResponsePhysics)
        rp->pushBackOnList(m_pointResponsePhysicsList, 2);
    else if (rp->getClassID() == CID_RotationalPointResponsePhysics)
        rp->pushBackOnList(m_rotationalPointResponsePhysicsList, 3);
    else if (rp->getClassID() == CID_SoldierResponsePhysics)
        rp->pushBackOnList(m_soldierResponsePhysicsList, 4);
    // else: CID_StaticResponsePhysics and anything unknown - dropped, as shipped
}

// Unlinks the object's response from its tagged list, if present.
// The missing-physics-node diagnostic does not stop removal. Child-list tags bypass class dispatch;
// other responses use the same repeated class-ID ladder as registration.
void ResponsePhysicsManager::removeObject(IObject* obj)
{
    typedef object_detail::LayoutAccess L;

    IResponsePhysics* rp = static_cast<IResponsePhysics*>(L::responsePhysics(obj));
    if (!rp)
        return;
    const unsigned char tag = rp->isInList();        // vtable +0xA4 - the 1..5 pushBackOnList stored
    if (!tag)
        return;

    if (!L::physicsNode(obj))                        // IObject +0x44 - a warning, not a guard
        Debug(DEBUGTYPE_6, "Physics/ResponseManager.cpp", 305)
            << "Object does not have a physics node.";

    ResponsePhysicsList* list;
    if (tag == 5)
        list = &m_childResponsePhysicsList;
    else if (rp->getClassID() == CID_ResponsePhysics)
        list = &m_responsePhysicsList;
    else if (rp->getClassID() == CID_PointResponsePhysics)
        list = &m_pointResponsePhysicsList;
    else if (rp->getClassID() == CID_RotationalPointResponsePhysics)
        list = &m_rotationalPointResponsePhysicsList;
    else if (rp->getClassID() == CID_SoldierResponsePhysics)
        list = &m_soldierResponsePhysicsList;
    else
    {
        rp->getClassID();                            // the ladder's last, discarded re-read, as shipped
        return;
    }
    rp->removeFromList(*list);
}


// A pure dispatcher, and both arms are tail calls in the binary.
void ResponsePhysicsManager::update(float deltaTime, IObject* obj)
{
    if (obj)
        updateSingleObject(deltaTime, obj);
    else
        updateAllObjects(deltaTime);            // 0x8370220 (call), body at 0x83708D0
}


// See the header block: this is a generation-counter bump, not a state clear.
void ResponsePhysicsManager::reset()
{
    ++collisionCheckedCounter;      // 0x83705A3 `inc dword ptr ds:898BC44h`
}

// ResponsePhysicsManager::resetCachedCollisionObjects()  -
// Raises the dirty flag; the four check*VsObjects routines test it against 1 and
// clear it once they have rebuilt (0x8370FFB/0x8371129, 0x83712DB/0x8371409, 0x8371532/0x8371CA0).
void ResponsePhysicsManager::resetCachedCollisionObjects()
{
    m_cachedCollisionObjectsDirty = true;   // 0x8370596 `mov byte ptr [eax+30h], 1`
}

// Allocates the one-byte frame marker once, then clears all fifteen collision-statistics dwords.
void ResponsePhysicsManager::startFrame()
{
    if (!m_startFrameMarker)
        m_startFrameMarker = new char;      // 0x836FF00 `operator new(1)` - no constructor call

    memset(collisionStatsData, 0, 0x3C);
}

// ResponsePhysicsManager::stopFrame()  -  : it is empty.

//     push ebp / mov ebp, esp / pop ebp / retn

// Five bytes with a frame pointer and nothing between - that is a function whose source body is
// empty, not one that failed to decompile. Game/GameServer/GameServer.cpp:586 identifies this exact
// slot as the per-sub-step call sitting right after GameServer::simulateFrame, so the shipped
// dedicated server dispatches an empty function once per physics sub-step. Keep the slot and keep
// it empty: removing it renumbers postFrameUpdate.
void ResponsePhysicsManager::stopFrame()
{
}


// three of the five lists, in this order, and the omission is the interesting part. Soldier
// responses, child responses, and plain ResponsePhysics get their previous transformation stamped;
// the point and rotational-point lists do not. Point responses carry no orientation and nothing
// rewinds them, so there is nothing to record - the lag-compensation design that
// Physics/CollisionManager.cpp documents (PhysicsNode::getLatencyCompensatedTransformation +
// ObjectManager::getObjectsFromGridWithLatency + this) only covers bodies with a full transform.
// A rebuild that "completes" this by adding the other two lists changes what the server rewinds.
void ResponsePhysicsManager::postFrameUpdate()
{
    setPrevTransformationForResponseList(m_soldierResponsePhysicsList);
    setPrevTransformationForResponseList(m_childResponsePhysicsList);
    setPrevTransformationForResponseList(m_responsePhysicsList);          // 0x836FEC7 (tail call)
}

// Stores previous transforms for each response and its next-to-check chain. Re-tests
// enabled state on every chain member; disabled links do not terminate traversal.
void ResponsePhysicsManager::setPrevTransformationForResponseList(ResponsePhysicsList& list)
{
    typedef object_detail::LayoutAccess L;

    IResponsePhysics* const end = list.tail ? list.tail->m_listNext : 0;
    for (IResponsePhysics* rp = list.head; rp != end; rp = rp->m_listNext)
    {
        IObject* const o = rp->getObject();                          // +0x40
        if ((L::flags(o) & 1) != 0)     continue;                    // destroyed
        if ((L::flags(o) & 0x100) == 0) continue;                    // no response

        for (IResponsePhysics* c = rp; c; c = c->getNextToCheck())   // +0x60
        {
            IObject* const co = c->getObject();
            if ((L::flags(co) & 1) != 0)     continue;
            if ((L::flags(co) & 0x100) == 0) continue;
            if (c->getEnabled() != true)     continue;               // +0x1C
            static_cast<IPhysicsNode*>(L::physicsNode(co))           // +0x44, not null-tested
                ->setPrevTransformation(co->getAbsoluteTransformation());   // node +0x54 <- obj slot 33
        }
    }
}

// Fills the shared candidate cache using the root physics node's velocity.
// Point responses use a swept query, optionally rewound for projectile-owner latency; other responses
// use a sphere centered halfway along the sweep. A missing root node warns and leaves the cache alone.
// Candidates must carry both response and root flags; the walkers descend into their children.
// [RETIRE-WHEN: spatial-queries-real] RW-4: radius and swept-radius queries are implemented;
// the remaining direction, box and frustum query families must be completed separately.
void ResponsePhysicsManager::getCollisionObjects(IObject* obj, float radius, float dt)
{
    typedef object_detail::LayoutAccess L;

    ObjectFlagPredicator predicator(0x1000100u);

    IPhysicsNode* const rootNode =
        static_cast<IPhysicsNode*>(L::physicsNode(getRootParent(obj)));     // root +0x44
    if (!rootNode)
    {
        Debug(DEBUGTYPE_6, "Physics/ResponseManager.cpp", 425) << "No physicsNodeComp!";
        return;
    }

    const Vec3& speed = rootNode->getPositionalSpeed();                    // +0x60
    Vec3 sweep;
    sweep.x = dt * speed.x;
    sweep.y = dt * speed.y;
    sweep.z = dt * speed.z;

    IResponsePhysics* const rp = static_cast<IResponsePhysics*>(L::responsePhysics(obj));
    if (rp && rp->getClassID() != CID_PointResponsePhysics)
    {
        // The sphere query - hulls and tumbling points.
        const Vec3& pos = obj->getAbsolutePosition();                      // slot 27
        Vec3 centre;
        centre.x = pos.x - sweep.x * 0.5f;
        centre.y = pos.y - sweep.y * 0.5f;
        centre.z = pos.z - 0.5f * sweep.z;
        float r = sqrtf(sweep.z * sweep.z + sweep.y * sweep.y + sweep.x * sweep.x);
        const float cap = s_maxSweepSpeed / 30.0f;
        if (r > cap)
            r = cap;
        objectManager->getObjectsFromGrid(centre, r + radius, s_collisionObjects, &predicator);   // +0x40
        return;
    }

    // The swept query - wheels and projectiles.
    if (LatencyCompensation::m_isEnabled)
    {
        void* projectile = obj ? obj->queryInterface(IID_IProjectileObject) : 0;
        if (projectile && LatencyCompensation::getLatency(projectileOwner(projectile)) > 0.0f)
        {
            const Vec3& pos = obj->getAbsolutePosition();
            Vec3 from;
            from.x = pos.x - sweep.x;
            from.y = pos.y - sweep.y;
            from.z = pos.z - sweep.z;
            objectManager->getObjectsFromGridWithLatency(from, sweep, s_collisionObjects,
                                                         &predicator);     // +0x60
            return;
        }
    }
    const Vec3& pos = obj->getAbsolutePosition();
    Vec3 from;
    from.x = pos.x - sweep.x;
    from.y = pos.y - sweep.y;
    from.z = pos.z - sweep.z;
    objectManager->getObjectsFromGrid(from, sweep, s_collisionObjects, &predicator,
                                      false, 0, false);                    // +0x5C
}

// Tests a point object against nearby mesh, static and soldier responses; missing response aborts.
// A root outside the heightmap posts message 29 and skips collision. Rebuilds candidates for roots
// or a dirty cache. Stops each chain at the same root or a missing response, skipping destroyed
// objects and shared groups. Soldier partners solve with reversed (soldier, point) arguments.
void ResponsePhysicsManager::checkPointObjectVsObjects(float dt, IObject* obj)
{
    typedef object_detail::LayoutAccess L;

    IResponsePhysics* const rp = static_cast<IResponsePhysics*>(L::responsePhysics(obj));
    if (!rp)
        abort();

    if (!L::parent(obj))                                                  // a root point object
    {
        const Vec3& pos = obj->getAbsolutePosition();
        static_cast<IPhysicsNode*>(L::physicsNode(obj))->getPositionalSpeed();   // the dead sweep, see above
        if (!static_cast<IHeightmapCluster*>(heightmapCluster)->isInsideHeightmapCluster(pos.x, pos.z))
        {
            objectManager->postMessage(obj, static_cast<TemplateMessage>(29), 0.0f);   // +0xD0
            return;
        }
    }

    const unsigned int groups = rp->getCollisionGroups();                 // +0x54
    rp->setCollisionChecked(static_cast<unsigned int>(collisionCheckedCounter));   // +0x44

    if (!obj || !L::parent(obj) || m_cachedCollisionObjectsDirty == true)
    {
        m_cachedCollisionObjectsDirty = false;
        s_collisionObjects.resize(0);
        getCollisionObjects(obj, 0.0f, dt);
    }

    IObject* const root = getRootParent(obj);
    for (std::vector<IObject*>::iterator it = s_collisionObjects.begin();
         it != s_collisionObjects.end(); ++it)
    {
        IResponsePhysics* orp = 0;
        for (IObject* other = getFirstObjectWithResponse(*it); other; other = getNextChild(orp))
        {
            if (root == getRootParent(other))
                break;
            orp = static_cast<IResponsePhysics*>(L::responsePhysics(other));
            if (!orp)
                break;

            if ((orp->getCollisionGroups() & groups) != 0)
                continue;
            if ((L::flags(other) & 1) != 0)
                continue;
            if (orp->getClassID() != CID_ResponsePhysics &&                // the repeated-dispatch
                orp->getClassID() != CID_StaticResponsePhysics &&          //   ladder, as shipped
                orp->getClassID() != CID_SoldierResponsePhysics)
                continue;

            if (orp->getClassID() == CID_SoldierResponsePhysics)
                static_cast<IResponsePhysics*>(L::responsePhysics(other))
                    ->checkObjectVsObject(other, obj, dt, false);         // the soldier tests
            else
                rp->checkObjectVsObject(obj, other, dt, false);           // +0x28
        }
    }
}

// Checks rotational-point contacts against other objects, including soldier reversal.
// Requires a response pointer; unlike point checks, does not test heightmap bounds first.
void ResponsePhysicsManager::checkRotationalPointObjectVsObjects(float dt, IObject* obj)
{
    typedef object_detail::LayoutAccess L;

    IResponsePhysics* const rp = static_cast<IResponsePhysics*>(L::responsePhysics(obj));
    const unsigned int groups = rp->getCollisionGroups();
    rp->setCollisionChecked(static_cast<unsigned int>(collisionCheckedCounter));

    if (!obj || !L::parent(obj) || m_cachedCollisionObjectsDirty == true)
    {
        m_cachedCollisionObjectsDirty = false;
        s_collisionObjects.resize(0);
        getCollisionObjects(obj, 0.0f, dt);
    }

    IObject* const root = getRootParent(obj);
    for (std::vector<IObject*>::iterator it = s_collisionObjects.begin();
         it != s_collisionObjects.end(); ++it)
    {
        IResponsePhysics* orp = 0;
        for (IObject* other = getFirstObjectWithResponse(*it); other; other = getNextChild(orp))
        {
            if (root == getRootParent(other))
                break;
            orp = static_cast<IResponsePhysics*>(L::responsePhysics(other));
            if (!orp)
                break;

            if ((orp->getCollisionGroups() & groups) != 0)
                continue;
            if ((L::flags(other) & 1) != 0)
                continue;
            if (orp->getClassID() != CID_ResponsePhysics &&
                orp->getClassID() != CID_StaticResponsePhysics &&
                orp->getClassID() != CID_SoldierResponsePhysics)
                continue;

            if (orp->getClassID() == CID_SoldierResponsePhysics)
                static_cast<IResponsePhysics*>(L::responsePhysics(other))
                    ->checkObjectVsObject(other, obj, dt, false);
            else
                rp->checkObjectVsObject(obj, other, dt, false);
        }
    }
}

// Checks mesh contacts against cached broad-phase candidates with disjoint collision groups.
// Requires collision LOD 1, stamps the current generation, then applies sphere, optional box and
// optional OBB rejection before narrow-phase dispatch. Preserve squared-speed reach terms.
// Radius ratios select pass order and optional LOD-4 tests; the small-body arm checks our root
// identity and the large-body arm checks the partner's. Repeated virtual calls remain ordered.
void ResponsePhysicsManager::checkMeshObjectVsObjects(float dt, IObject* obj)
{
    typedef object_detail::LayoutAccess L;

    IResponsePhysics* const rp = static_cast<IResponsePhysics*>(L::responsePhysics(obj));
    const unsigned int groups = rp->getCollisionGroups();
    rp->setCollisionChecked(static_cast<unsigned int>(collisionCheckedCounter));

    if (!obj->hasCollisionLod(1))                                         // slot 51
        return;

    ICollisionMesh* const mesh = static_cast<ICollisionMesh*>(L::collisionMesh(obj));   // +0x40
    const float myRadius = mesh->getBoundingRadius(1);                    // slot 21

    if (!L::parent(obj) || m_cachedCollisionObjectsDirty == true)
    {
        m_cachedCollisionObjectsDirty = false;
        s_collisionObjects.resize(0);
        getCollisionObjects(obj, obj->getPhysicBoundingRadius(), dt);     // slot 38
    }

    const Mat4& myTransform = obj->getAbsoluteTransformation();           // slot 33
    IPhysicsNode* const rootNode =
        static_cast<IPhysicsNode*>(L::physicsNode(getRootParent(obj)));
    IObject* const root = getRootParent(obj);

    for (std::vector<IObject*>::iterator it = s_collisionObjects.begin();
         it != s_collisionObjects.end(); ++it)
    {
        IObject* other = getFirstObjectWithResponse(*it);
        if (!other)
            continue;
        if ((L::flags(other) & 0x100) == 0 || (L::flags(other) & 1) != 0)
            continue;

        IResponsePhysics* orp = 0;
        for (; other; other = getNextChild(orp))
        {
            IObject* const otherRoot = getRootParent(other);
            if (root == otherRoot)
                break;
            orp = static_cast<IResponsePhysics*>(L::responsePhysics(other));
            if (!orp)
                break;

            if ((L::flags(other) & 1) != 0)
                continue;
            if (orp->getClassID() != CID_ResponsePhysics &&
                orp->getClassID() != CID_StaticResponsePhysics)
                continue;
            if (orp->getCollisionChecked() == static_cast<unsigned int>(collisionCheckedCounter))
                continue;                                                 // +0x48 - done this pass
            if ((orp->getCollisionGroups() & groups) != 0)
                continue;

            int lod = 1;
            if (rootNode->getOtherCollisionLod() >= 0)                    // +0x19C
                lod = rootNode->getOtherCollisionLod();
            if (!other->hasCollisionLod(lod))
                continue;

            ICollisionMesh* const otherMesh =
                static_cast<ICollisionMesh*>(L::collisionMesh(other));
            const float otherRadius = otherMesh->getBoundingRadius(lod);
            const Mat4& otherTransform = other->getAbsoluteTransformation();

            // 5. the sphere test
            const float dx = otherTransform.m[12] - myTransform.m[12];
            const float dy = otherTransform.m[13] - myTransform.m[13];
            const float dz = otherTransform.m[14] - myTransform.m[14];
            const float dist2 = dx * dx + dy * dy + dz * dz;

            IPhysicsNode* const otherRootNode =
                static_cast<IPhysicsNode*>(L::physicsNode(otherRoot));
            const Vec3& v = rootNode->getPositionalSpeed();               // +0x60
            const float mySweep = (v.x * v.x + v.y * v.y + v.z * v.z) * dt * 0.25f;
            float otherSweep = 0.0f;
            if (otherRootNode)
            {
                const Vec3& ov = otherRootNode->getPositionalSpeed();
                otherSweep = (ov.x * ov.x + ov.y * ov.y + ov.z * ov.z) * dt * 0.25f;
            }
            const float reach = otherSweep + myRadius + otherRadius + mySweep;
            if (dist2 > reach * reach)
                continue;

            // 6. the box prefilter - the smaller sphere inside the larger box
            if (s_useBoxPrefilter == 1)
            {
                bool inside;
                if (otherRadius > myRadius)
                {
                    const Mat4& otherInv = other->getInvAbsoluteTransformation();   // slot 34
                    const Vec3& otherPos = other->getAbsolutePosition();
                    const Vec3& myPos    = obj->getAbsolutePosition();
                    inside = static_cast<const LocalBoundingBox*>(otherMesh->getBoundingBox(lod))
                                 ->isInside(myPos, myRadius, otherPos, otherInv);
                }
                else
                {
                    const Mat4& myInv    = obj->getInvAbsoluteTransformation();
                    const Vec3& myPos    = obj->getAbsolutePosition();
                    const Vec3& otherPos = other->getAbsolutePosition();
                    inside = static_cast<const LocalBoundingBox*>(mesh->getBoundingBox(1))
                                 ->isInside(otherPos, otherRadius, myPos, myInv);
                }
                if (!inside)
                    continue;
            }

            // 7. the OBB stage
            if (s_obbTestMode > 0)
            {
                ++s_obbStageEntered;
                // ELF 0x0837185B / 0x08371AB6: both counts compare signed (jg, jle) against the limit,
                // which a console write can make negative.
                if (static_cast<int>(mesh->getVertexCount(1)) > s_obbMinVertexCount &&          // slot 16
                    static_cast<int>(otherMesh->getVertexCount(lod)) > s_obbMinVertexCount)
                {
                    bool testObb = false;
                    if (s_obbTestMode == 1)
                        testObb = true;
                    else if (s_obbTestMode == 2)
                        testObb = sqrtf(dist2) > (myRadius + otherRadius) * s_obbDistanceFactor;
                    if (testObb)
                    {
                        const Vec3& zero = Vec3::getZero();
                        const Mat4& ot = other->getAbsoluteTransformation();
                        const LocalBoundingBox* const obox =
                            static_cast<const LocalBoundingBox*>(otherMesh->getBoundingBox(lod));
                        const Mat4& mt = obj->getAbsoluteTransformation();
                        const LocalBoundingBox* const mbox =
                            static_cast<const LocalBoundingBox*>(mesh->getBoundingBox(1));
                        if (!mbox->testIntersectWithOBB(mt, *obox, ot, zero))
                            continue;
                    }
                }
                ++s_obbStagePassed;
            }

            // 8. the narrow phase
            if (mesh->getVertexCount(1) <= 3 && otherMesh->getVertexCount(lod) <= 3)
                continue;

            IResponsePhysics* const myRp =
                static_cast<IResponsePhysics*>(L::responsePhysics(obj));  // re-read, as shipped
            const float ratio = myRadius / otherRadius;
            if (0.25f > ratio)
            {
                myRp->checkObjectVsObject(obj, other, dt, false);
                if (obj == root && other->hasCollisionLod(4) == true)
                    rp->checkObjectVsObject(other, obj, dt, true);
            }
            else if (ratio <= 4.0f)
            {
                myRp->checkObjectVsObject(other, obj, dt, false);
                myRp->checkObjectVsObject(obj, other, dt, false);
            }
            else
            {
                IPhysicsNode* const otherNode = static_cast<IPhysicsNode*>(L::physicsNode(other));
                IPhysicsNode* const myNode    = static_cast<IPhysicsNode*>(L::physicsNode(obj));
                bool bothMobile = false;
                if (myNode && myNode->getIsMobile() == true && otherNode)  // +0x134
                    bothMobile = otherNode->getIsMobile() != false;
                myRp->checkObjectVsObject(other, obj, dt, false);
                if (bothMobile)
                    myRp->checkObjectVsObject(obj, other, dt, false);
                else if (other == otherRoot && obj->hasCollisionLod(4) == true)
                    rp->checkObjectVsObject(obj, other, dt, true);
            }
        }
    }
}

// Builds soldier collision candidates and tests a pending pose change before contact response.
// Stamps the generation before the LOD check so the later impulse pass can consume terrain contacts.
// The relative-speed term is subtracted before squaring without a clamp. Preserve repeated class,
// pose and transform queries. Candidates require partner LOD 2; projectiles skip pose-change tests.
void ResponsePhysicsManager::checkSoldierObjectVsObjects(float dt, IObject* obj)
{
    typedef object_detail::LayoutAccess L;

    collisionTestedFaces.resize(0);
    collisionIntersectFaces.resize(0);

    Soldier* const soldier = static_cast<Soldier*>(obj);
    ISoldier* const soldierInterface = static_cast<ISoldier*>(soldier);
    IResponsePhysics* const rp = static_cast<IResponsePhysics*>(L::responsePhysics(soldier));
    rp->setCollisionChecked(static_cast<unsigned int>(collisionCheckedCounter));
    const unsigned int groups = rp->getCollisionGroups();

    if (!soldier->hasCollisionLod(1))
        return;

    ICollisionMesh* const mesh = static_cast<ICollisionMesh*>(L::collisionMesh(soldier));
    const float radius = mesh->getBoundingRadius(1);
    s_collisionObjects.resize(0);
    float queryRadius = radius;
    if (soldierInterface->getIsNewPoseRequested() == true)
        queryRadius = radius + 1.0f;
    getCollisionObjects(soldier, queryRadius, dt);

    const Vec3& position = soldier->getAbsolutePosition();
    IObject* const root = getRootParent(soldier);
    IPhysicsNode* const rootNode = static_cast<IPhysicsNode*>(L::physicsNode(root)); // unchecked
    s_soldierCandidates.resize(0);

    for (std::vector<IObject*>::iterator it = s_collisionObjects.begin();
         it != s_collisionObjects.end(); ++it)
    {
        IObject* other = getFirstObjectWithResponse(*it);
        if (!other)
            continue;

        const unsigned int firstFlags = L::flags(other);
        if ((firstFlags & 0x100) == 0 || (firstFlags & 1) != 0)
            continue;

        IResponsePhysics* otherRp = 0;
        for (; other; other = getNextChild(otherRp))
        {
            IObject* const otherRoot = getRootParent(other);
            if (root == otherRoot)
                break;

            otherRp = static_cast<IResponsePhysics*>(L::responsePhysics(other));
            if (!otherRp)
                break;

            if ((L::flags(other) & 1) != 0)
                continue;
            if ((L::flags(other) & 0x100) == 0)
                continue;
            if (otherRp->getClassID() == CID_PointResponsePhysics)
                continue;
            if (otherRp->getClassID() == CID_SoldierResponsePhysics &&
                otherRp->getCollisionChecked() ==
                    static_cast<unsigned int>(collisionCheckedCounter))
                continue;
            if ((otherRp->getCollisionGroups() & groups) != 0 &&
                otherRp->getClassID() != CID_SoldierResponsePhysics)
                continue;
            if (root != soldier && otherRoot != other)
                continue;
            if (!other->hasCollisionLod(2))
                continue;

            ICollisionMesh* const otherMesh =
                static_cast<ICollisionMesh*>(L::collisionMesh(other));
            const float otherRadius = otherMesh->getBoundingRadius(2);
            IPhysicsNode* const otherRootNode =
                static_cast<IPhysicsNode*>(L::physicsNode(otherRoot));

            const Vec3& myVelocity = rootNode->getLinearIntegrationVelocity();
            const Vec3& otherVelocity = otherRootNode
                ? otherRootNode->getLinearIntegrationVelocity()
                : Vec3::getZero();
            const float vx = myVelocity.x - otherVelocity.x;
            const float vy = myVelocity.y - otherVelocity.y;
            const float vz = myVelocity.z - otherVelocity.z;
            const float reach = radius + otherRadius
                              - sqrtf(vz * vz + vy * vy + vx * vx) * dt;
            const float reachSq = reach * reach;

            const Vec3& otherPosition = other->getAbsolutePosition();
            const float dx = position.x - otherPosition.x;
            const float dy = position.y - otherPosition.y;
            const float dz = position.z - otherPosition.z;
            if (!(dz * dz + dy * dy + dx * dx <= reachSq))
                continue;

            bool inside = true;
            if (s_useBoxPrefilter == 1 && !(otherRadius <= radius))
            {
                // This virtual-call order is visible in both ports.
                const Mat4& otherInv = other->getInvAbsoluteTransformation();
                const Vec3& otherPosition2 = other->getAbsolutePosition();
                const Vec3& soldierPosition2 = soldier->getAbsolutePosition();
                const LocalBoundingBox* const box =
                    static_cast<const LocalBoundingBox*>(otherMesh->getBoundingBox(2));
                inside = box->isInside(soldierPosition2, radius, otherPosition2, otherInv);
            }
            if (inside)
                s_soldierCandidates.push_back(other);
        }
    }

    if (soldierInterface->getIsNewPoseRequested() == true)
    {
        std::vector<IObject*>::iterator it = s_soldierCandidates.begin();
        for (; it != s_soldierCandidates.end(); ++it)
        {
            IObject* const other = *it;
            if (other->getTemplate()->getClassID() != CID_GenericProjectileTemplate &&
                !rp->poseChangeTest(soldier, other, dt))
                break;
        }
        if (it == s_soldierCandidates.end())
            soldierInterface->executePoseChange();
    }

    // [TEMP] [RETIRE-WHEN: turret-collision-diagnosed] armed by BF2_TRACE_TURRET.
    {
        static int armed = -1;
        if (armed < 0) armed = std::getenv("BF2_TRACE_TURRET") ? 1 : 0;
        if (armed == 1)
        {
            bool interesting = false;
            for (std::vector<IObject*>::iterator it = s_soldierCandidates.begin();
                 it != s_soldierCandidates.end(); ++it)
                if (L::parent(*it)) interesting = true;      // a vehicle part is a candidate
            static int throttle = 0;
            if ((throttle++ % 30) == 0)
            {
                std::fprintf(stderr, "[SOLDIER] cand=%u interestingPart=%d :",
                             static_cast<unsigned int>(s_soldierCandidates.size()),
                             interesting ? 1 : 0);
                for (std::vector<IObject*>::iterator it = s_soldierCandidates.begin();
                     it != s_soldierCandidates.end(); ++it)
                    std::fprintf(stderr, " %s%s", (*it)->getTemplate()->getName().c_str(),
                                 L::parent(*it) ? "(child)" : "");
                std::fprintf(stderr, "\n");
                std::fflush(stderr);
            }
        }
    }

    for (std::vector<IObject*>::iterator it = s_soldierCandidates.begin();
         it != s_soldierCandidates.end(); ++it)
        rp->checkObjectVsObject(soldier, *it, dt, false);
}

// Runs one object's response step, stamping the previous generation so the next full pass retests it.
// Skips destroyed, absent/disabled response, sleeping and static cases. Missing nodes warn and stop;
// unknown response classes warn but still run terrain, impulse and friction tail calls.
void ResponsePhysicsManager::updateSingleObject(float dt, IObject* obj)
{
    typedef object_detail::LayoutAccess L;

    if ((L::flags(obj) & 1) != 0)
        return;
    if ((L::flags(obj) & 0x100) == 0)
        return;
    IResponsePhysics* const rp = static_cast<IResponsePhysics*>(L::responsePhysics(obj));
    if (!rp->getEnabled())
        return;
    IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(obj));
    if (!node)
    {
        Debug(DEBUGTYPE_6, "Physics/ResponseManager.cpp", 558)
            << "Making a seprarate collisionDetection test for not physicsNode object; only resetting.";
        return;
    }

    const unsigned int classId = rp->getClassID();
    rp->setCollisionChecked(static_cast<unsigned int>(collisionCheckedCounter - 1));
    if (node->getIsSleeping())                                            // +0x11C
        return;

    if (classId == CID_PointResponsePhysics)
        checkPointObjectVsObjects(dt, obj);
    else if (classId == CID_RotationalPointResponsePhysics)
        checkRotationalPointObjectVsObjects(dt, obj);
    else if (classId == CID_ResponsePhysics)
        checkMeshObjectVsObjects(dt, obj);
    else if (classId == CID_SoldierResponsePhysics)
        checkSoldierObjectVsObjects(dt, obj);
    else if (classId == CID_StaticResponsePhysics)
        return;
    else
        Debug(DEBUGTYPE_6, "Physics/ResponseManager.cpp", 504) << "Unknown response physics class";

    rp->checkVsTerrain(dt);                                               // +0x2C
    rp->solveImpulse(node);                                               // +0x34
    rp->addFriction(node);                                                // +0x38
}

// Runs response checks in soldier, point, rotational-point and mesh order, with client-only culling.
// Mesh responses solve impulses and friction inline; later passes solve stamped soldiers, points
// and rotational points. Only soldiers receive friction in those later passes.
// Preserve each pass's guard order: only the soldier check null-tests its node, and mesh chains use
// the head entry's enabled flag. Check passes read next after callbacks; impulse passes save it first.

// [TEMP] [RETIRE-WHEN: turret-collision-diagnosed] the stationary-path audit: what a soldier
// standing on a part gets, i.e. getDistanceToSphere with a zero sweep. Armed by BF2_TRACE_TURRET.
namespace {
void auditStationary(IResponsePhysics* rp)
{
    typedef object_detail::LayoutAccess L;
    IObject* const rootObj = rp->getObject();
    if (!rootObj) return;
    std::fprintf(stderr, "[STAND] ---- %s ----\n", rootObj->getTemplate()->getName().c_str());
    std::fflush(stderr);

    int guard = 0;
    for (IResponsePhysics* c = rp; c && guard < 200; c = c->getNextToCheck(), ++guard)
    {
        IObject* const o = c->getObject();
        if (!o || !o->hasCollisionLod(2)) continue;
        ICollisionMesh* const mesh = static_cast<ICollisionMesh*>(L::collisionMesh(o));
        if (!mesh) continue;
        const unsigned int vcount = mesh->getVertexCount(2);
        const float* const verts = static_cast<const float*>(mesh->getVertexVector(2));
        if (!vcount || !verts) continue;
        unsigned int best = 0;
        for (unsigned int vi = 1; vi < vcount; ++vi)
            if (verts[vi*3+1] > verts[best*3+1]) best = vi;

        const Mat4& o2w = o->getAbsoluteTransformation();
        const Mat4& w2o = o->getInvAbsoluteTransformation();
        const float r = 0.4f;
        Vec3 lp;  lp.x = verts[best*3+0]; lp.y = verts[best*3+1] + r * 0.85f; lp.z = verts[best*3+2];
        Vec3 wp;
        wp.x = o2w.m[0]*lp.x + o2w.m[4]*lp.y + o2w.m[8]*lp.z + o2w.m[12];
        wp.y = o2w.m[1]*lp.x + o2w.m[5]*lp.y + o2w.m[9]*lp.z + o2w.m[13];
        wp.z = o2w.m[2]*lp.x + o2w.m[6]*lp.y + o2w.m[10]*lp.z + o2w.m[14];

        Vec3 zero;  zero.x = zero.y = zero.z = 0.0f;
        std::vector<Vec3> n, pos;  std::vector<float> dA, dB;  std::vector<int> m;
        mesh->getDistanceToSphere(2, false, o2w, w2o, wp, zero, r, n, pos, dA, dB, &m);
        float deepest = 0.0f;
        for (unsigned int i = 0; i < dA.size(); ++i) if (dA[i] < deepest) deepest = dA[i];
        std::fprintf(stderr, "[STAND]   %-34s child=%d v2=%u STATIONARY hits=%u deepest=%.4f\n",
                     o->getTemplate()->getName().c_str(), L::parent(o) ? 1 : 0,
                     vcount, static_cast<unsigned int>(dA.size()), deepest);
        std::fflush(stderr);
    }
}
} // namespace

void ResponsePhysicsManager::updateAllObjects(float dt)
{
    {
        static int armed2 = -1;
        if (armed2 < 0) armed2 = std::getenv("BF2_TRACE_TURRET") ? 1 : 0;
        static bool done2 = false;
        if (armed2 == 1 && !done2)
        {
            done2 = true;
            for (IResponsePhysics* r = m_responsePhysicsList.head;
                 r != (m_responsePhysicsList.tail ? m_responsePhysicsList.tail->m_listNext : 0);
                 r = r->m_listNext)
                auditStationary(r);
        }
    }

    typedef object_detail::LayoutAccess L;

    CollisionTestCuller culler;
    culler.m_cameraPosition.x = 0.0f;
    culler.m_cameraPosition.y = 0.0f;
    culler.m_cameraPosition.z = 0.0f;
    culler.m_enabled = !gameLogic->isServer();
    if (culler.m_enabled)
    {
        Mat4 cameraTransform = Mat4::getIdentity();
        float fov;
        objectManager->getCameraInfo(&cameraTransform, 0, &fov);          // +0x10C
        culler.m_cameraPosition.x = cameraTransform.m[12];
        culler.m_cameraPosition.y = cameraTransform.m[13];
        culler.m_cameraPosition.z = cameraTransform.m[14];
        const float r = s_cullRadiusScale * fov;
        culler.m_radiusSq = r * r;
    }

    // 1. soldiers: check
    for (IResponsePhysics* rp = m_soldierResponsePhysicsList.head;
         rp != (m_soldierResponsePhysicsList.tail ? m_soldierResponsePhysicsList.tail->m_listNext : 0);
         rp = rp->m_listNext)
    {
        IObject* const o = rp->getObject();
        if ((L::flags(o) & 1) != 0 || (L::flags(o) & 0x100) == 0)
            continue;
        if (!rp->getEnabled())
            continue;
        IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(o));
        if (!node)
            continue;
        if (node->getHasSeparatePhysicsUpdate())                          // +0x104
            continue;
        if (culler.cull(rp->getObject(), 0.0f))
            continue;
        checkSoldierObjectVsObjects(dt, o);
        rp->checkVsTerrain(dt);                                           // +0x2C
    }

    // 2. points: check
    for (IResponsePhysics* rp = m_pointResponsePhysicsList.head;
         rp != (m_pointResponsePhysicsList.tail ? m_pointResponsePhysicsList.tail->m_listNext : 0);
         rp = rp->m_listNext)
    {
        IObject* const o = rp->getObject();
        if ((L::flags(o) & 1) != 0 || (L::flags(o) & 0x100) == 0)
            continue;
        if (!rp->getEnabled())
            continue;
        if (culler.cull(rp->getObject(), 0.0f))
            continue;
        if (static_cast<IPhysicsNode*>(L::physicsNode(rp->getObject()))->getHasSeparatePhysicsUpdate())
            continue;
        checkPointObjectVsObjects(dt, rp->getObject());
        rp->checkVsTerrain(dt);
    }

    // 3. rotational points: check
    for (IResponsePhysics* rp = m_rotationalPointResponsePhysicsList.head;
         rp != (m_rotationalPointResponsePhysicsList.tail
                    ? m_rotationalPointResponsePhysicsList.tail->m_listNext : 0);
         rp = rp->m_listNext)
    {
        IObject* const o = rp->getObject();
        if ((L::flags(o) & 1) != 0 || (L::flags(o) & 0x100) == 0)
            continue;
        if (culler.cull(rp->getObject(), 7.0f))
            continue;
        if (static_cast<IPhysicsNode*>(L::physicsNode(rp->getObject()))->getHasSeparatePhysicsUpdate())
            continue;
        if (rp->getEnabled() != true)
            continue;
        checkRotationalPointObjectVsObjects(dt, rp->getObject());
        rp->checkVsTerrain(dt);
    }

    // 4. meshes: check + impulse, along each entry's next-to-check chain
    for (IResponsePhysics* rp = m_responsePhysicsList.head;
         rp != (m_responsePhysicsList.tail ? m_responsePhysicsList.tail->m_listNext : 0);
         rp = rp->m_listNext)
    {
        IObject* const o = rp->getObject();
        if ((L::flags(o) & 1) != 0 || (L::flags(o) & 0x100) == 0)
            continue;
        if (culler.cull(rp->getObject(), 0.0f))
            continue;

        for (IResponsePhysics* c = rp; c; c = c->getNextToCheck())       // +0x60
        {
            IObject* const co = c->getObject();
            if ((L::flags(co) & 1) != 0 || (L::flags(co) & 0x100) == 0)
                continue;
            IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(co));   // not null-tested
            if (node->getHasSeparatePhysicsUpdate())
                continue;
            if (node->getIsSleeping())                                    // +0x11C
                continue;
            if (rp->getEnabled() != true)                                 // the list entry's
                continue;
            checkMeshObjectVsObjects(dt, c->getObject());
            c->checkVsTerrain(dt);
            IPhysicsNode* const node2 = static_cast<IPhysicsNode*>(L::physicsNode(c->getObject()));
            c->solveImpulse(node2);                                       // +0x34
            c->addFriction(node2);                                        // +0x38
        }
    }

    // 5. soldiers: impulse
    for (IResponsePhysics* rp = m_soldierResponsePhysicsList.head;
         rp != (m_soldierResponsePhysicsList.tail ? m_soldierResponsePhysicsList.tail->m_listNext : 0);)
    {
        IResponsePhysics* const next = rp->m_listNext;
        if (rp->getCollisionChecked() == static_cast<unsigned int>(collisionCheckedCounter))   // +0x48
        {
            IObject* const o = rp->getObject();
            if ((L::flags(o) & 1) == 0 && (L::flags(o) & 0x100) != 0)
            {
                IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(rp->getObject()));
                if (!node->getHasSeparatePhysicsUpdate() && rp->getEnabled() == true)
                {
                    rp->solveImpulse(node);
                    rp->addFriction(node);
                }
            }
        }
        rp = next;
    }

    // 6. points: impulse
    for (IResponsePhysics* rp = m_pointResponsePhysicsList.head;
         rp != (m_pointResponsePhysicsList.tail ? m_pointResponsePhysicsList.tail->m_listNext : 0);)
    {
        IResponsePhysics* const next = rp->m_listNext;
        if (rp->getCollisionChecked() == static_cast<unsigned int>(collisionCheckedCounter))
        {
            IObject* const o = rp->getObject();
            if ((L::flags(o) & 1) == 0 && (L::flags(o) & 0x100) != 0 && rp->getEnabled())
                rp->solveImpulse(static_cast<IPhysicsNode*>(L::physicsNode(rp->getObject())));
        }
        rp = next;
    }

    // 7. rotational points: impulse
    for (IResponsePhysics* rp = m_rotationalPointResponsePhysicsList.head;
         rp != (m_rotationalPointResponsePhysicsList.tail
                    ? m_rotationalPointResponsePhysicsList.tail->m_listNext : 0);)
    {
        IResponsePhysics* const next = rp->m_listNext;
        if (rp->getCollisionChecked() == static_cast<unsigned int>(collisionCheckedCounter))
        {
            IObject* const o = rp->getObject();
            if ((L::flags(o) & 1) == 0 && (L::flags(o) & 0x100) != 0 && rp->getEnabled())
                rp->solveImpulse(static_cast<IPhysicsNode*>(L::physicsNode(rp->getObject())));
        }
        rp = next;
    }
}

} // namespace world
} // namespace hfe
} // namespace dice

// Interface calls require the complete physics headers. Local one-slot substitutes change dispatch.
// collisionCheckedCounter belongs to this translation unit; do not define a second copy.
