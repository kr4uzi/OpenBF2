// Object lifetime, identity, component routing, hierarchy, and dirty-state propagation.

#include "../../Game/Objects/Object.h"
#include "../../Game/Objects/GeometryPartAccess.h"   // named geometry operations (non-MSVC)
#include "../../Game/Objects/ObjectLayoutAccess.h"
#include "../../Game/Objects/IObjectTemplate.h"
#include "../../Game/IGameLogic.h"
#include "../../Geom/IGeometryTemplate.h"
#include "../../AI/AIInterface/AIInstance.h"
#include "../../Core/SettingsRepostitory.h"
#include "../../Core/StringUtil.h"
#include "ObjectManager.h"
#include "../../Core/EventManager.h"
#include "../../Core/Debug.h"
#include "../../Core/Ids.h"
#include "../../Physics/PhysicsNodeManager.h"
#include "../../Physics/ResponsePhysicsManager.h"
#include "../../Physics/ICollisionMesh.h"          // checkIfObjectIntersects
#include "../../Physics/IPhysicsNode.h"
#include "../../Physics/IResponsePhysics.h"        // checkIfObjectIntersectsSoldier
#include "../../Physics/HeightmapCluster.h"        // isObjectCloseToGround
#include "ObjectPredicator.h"                    // getChilds' two predicators
#include "../../IO/Stream/IStream.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>
#if !defined(_MSC_VER)
#include "../../Platform/X87Compat.h"
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
#  define BF2_OBJECT_VCALL __thiscall
#else
#  define BF2_OBJECT_VCALL
#endif

namespace dice {
namespace hfe {
namespace world {

extern PhysicsNodeManager* physicsNodeManager;
extern ResponsePhysicsManager* responsePhysicsManager;
extern IBase* heightmapCluster;                 // ds:0x8ABAF24 - Core/Globals.cpp

// world::getNextChild(IResponsePhysics*) - 0x8370560, defined in Physics/ResponsePhysicsManager.cpp
// beside the four check*VsObjects walkers that share it. No header declares it yet;
// checkIfObjectIntersectsSoldier below is its first caller outside that TU.
IObject* getNextChild(IResponsePhysics* response);

// Shared direct-field access is defined once in Game/Objects/ObjectLayoutAccess.h.

ObjectManager* Object::mObjectManager = 0;

namespace {

#if defined(_MSC_VER)
template <typename Fn>
Fn objectRawSlot(void* object, unsigned int byteOffset)
{
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[byteOffset / 4]);
}
#endif

void setResponseCacheToDirty(IObject* object)
{
    if (!object_detail::LayoutAccess::responsePhysics(object))
        return;

    IObject* root = getRootParent(object);
    IResponsePhysics* response = static_cast<IResponsePhysics*>(
        object_detail::LayoutAccess::responsePhysics(root));
    if (!response)
        return;

    response->setDirtyNextToCheck();
}

void setTransformationIsDirty(IObject* object)
{
    if (!object)
        return;

    const unsigned int oldFlags = object_detail::LayoutAccess::flags(object);
    object_detail::LayoutAccess::flags(object) = oldFlags | 0x40200000u;
    if ((oldFlags & 0x00080000u) == 0)
    {
        object_detail::LayoutAccess::flags(object) = oldFlags | 0x50280000u;
        for (IObject* child = object->getChild(); child;
             child = object_detail::LayoutAccess::sibling(child))
            setTransformationIsDirty(child);
    }
}

// PE 0x0071E4E0 (inlined); ELF 0x083079C0. Roots require a live global manager.
void transformationWasUpdated(IObject* object)
{
    setTransformationIsDirty(object);
    if (!object_detail::LayoutAccess::parent(object))
        objectManager->refreshGridObject(object);
}

float translationDistanceSquared(const Mat4& lhs, const Mat4& rhs)
{
    const float x = lhs.m[12] - rhs.m[12];
    const float y = lhs.m[13] - rhs.m[13];
    const float z = lhs.m[14] - rhs.m[14];
    return x * x + y * y + z * z;
}

float vectorLength(const Vec3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

void objectLifecycleInvariantFailure()
{
#if defined(_MSC_VER)
    __debugbreak();
#else
    std::abort();
#endif
}

void resetCachedPhysicsNode(void* physicsNode)
{
    IPhysicsNode* node = static_cast<IPhysicsNode*>(physicsNode);
    node->reset(true);
    node->setEnabled(true);
}

void resetCachedResponsePhysics(void* responsePhysics)
{
    IResponsePhysics* response = static_cast<IResponsePhysics*>(responsePhysics);
    response->reset();
    response->setEnabled(true);
}

Vec3 transformBoundingPoint(const Mat4& transform, const Vec3& point)
{
    Vec3 result;
    result.x = transform.m[0] * point.x + transform.m[4] * point.y +
               transform.m[8] * point.z + transform.m[12];
    result.y = transform.m[1] * point.x + transform.m[5] * point.y +
               transform.m[9] * point.z + transform.m[13];
    result.z = transform.m[2] * point.x + transform.m[6] * point.y +
               transform.m[10] * point.z + transform.m[14];
    return result;
}

void setBoundingEndpoints(LocalBoundingBox*& box, const Vec3& minimum, const Vec3& maximum)
{
    if (!box)
        box = new LocalBoundingBox;
    box->m_min = minimum;
    box->m_derivedDataDirty = true;
    box->m_max = maximum;
    box->m_derivedDataDirty = true;
}

void mergeBoundingEndpoints(LocalBoundingBox*& box, const Vec3& minimum, const Vec3& maximum,
                            bool alreadyHasBounds)
{
    if (!alreadyHasBounds)
    {
        setBoundingEndpoints(box, minimum, maximum);
        return;
    }

    Vec3 mergedMinimum = box->m_min;
    Vec3 mergedMaximum = box->m_max;
    mergedMinimum.x = std::min(mergedMinimum.x, minimum.x);
    mergedMinimum.y = std::min(mergedMinimum.y, minimum.y);
    mergedMinimum.z = std::min(mergedMinimum.z, minimum.z);
    mergedMaximum.x = std::max(mergedMaximum.x, maximum.x);
    mergedMaximum.y = std::max(mergedMaximum.y, maximum.y);
    mergedMaximum.z = std::max(mergedMaximum.z, maximum.z);
    setBoundingEndpoints(box, mergedMinimum, mergedMaximum);
}

void updateDescendantFlags(IObject* object, unsigned int set, unsigned int clear)
{
    for (IObject* child = object->getChild(); child; child = child->getSibling())
    {
        child->updateFlags(set, clear);
        updateDescendantFlags(child, set, clear);
    }
}

void updateDirectChildrenFlags(IObject* object, unsigned int set, unsigned int clear)
{
    for (IObject* child = object->getChild(); child;
         child = object_detail::LayoutAccess::sibling(child))
        child->updateFlags(set, clear);
}

void findAllChildren(IObject* object, std::vector<IObject*>& children)
{
    if (!object)
        return;
    children.push_back(object);
    findAllChildren(object->getChild(), children);
    findAllChildren(object_detail::LayoutAccess::sibling(object), children);
}

void setDisableFlagHasChanged(IObject* object, bool disabled)
{
    if ((object_detail::LayoutAccess::flags(object) & 0x01000000u) != 0)
        objectManager->refreshGridObject(object);
    setBoundRadiusIsDirty(object);
    setResponseCacheToDirty(object);

    IPhysicsNode* physicsNode = reinterpret_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(object));
    if (disabled)
    {
        if (physicsNode)
            physicsNodeManager->removePhysicsNode(physicsNode);
        responsePhysicsManager->removeObject(object);
    }
    else
    {
        if (physicsNode)
            physicsNodeManager->addPhysicsNode(physicsNode);
        responsePhysicsManager->addObject(object);
    }
}

} // unnamed namespace

IObject* getRootParent(const IObject* object)
{
    if (!object)
        return 0;

    IObject* current = const_cast<IObject*>(object);
    IObject*& cached = object_detail::LayoutAccess::cachedRootParent(current);
    if ((object_detail::LayoutAccess::flags(current) & 0x01000000u) != 0)
    {
        cached = current;
        return current;
    }
    if (!cached)
        cached = getRootParent(object_detail::LayoutAccess::parent(current));
    return cached;
}

// Collects self and descendants depth-first. stop prunes an entire branch before
// include decides whether to append that node; both predicates propagate recursively.
void getChilds(IObject* object, std::vector<IObject*>& out, ObjectPredicator* include,
               ObjectPredicator* stop)
{
    if (!object)
        return;
    if (stop && stop->predicate(object))                                       // slot 0
        return;
    if (!include || include->predicate(object))
        out.push_back(object);
    for (IObject* child = object->getChild(); child;                           // slot 60 (+0xF0)
         child = object_detail::LayoutAccess::sibling(child))                  // +0x2C
        getChilds(child, out, include, stop);
}

// Returns the first descendant with the requested object class id, or null.
// Searches children before siblings and does not test the root itself.
IObject* findFirstMatchingChild(IObject* object, unsigned int classId)
{
    if (!object)
        return 0;
    for (IObject* child = object->getChild(); child;                           // +0xF0
         child = object_detail::LayoutAccess::sibling(child))                  // +0x2C
    {
        if (child->getClassID() == classId)                                    // slot 4 (+0x10)
            return child;
        if (IObject* found = findFirstMatchingChild(child, classId))
            return found;
    }
    return 0;
}

// Tests transformed LOD-1 mesh vertex rays against the object's LOD-2 front faces.
// Requires a nonnull mesh; absent response physics, LODs, or vertices return false.
// Rays use transformed directions, and the first accepted vertex hit returns true.
// PE 0x0071B430; ELF 0x0831C100.
bool checkIfObjectIntersects(ICollisionMesh* mesh, const Mat4& transform, IObject* object)
{
    if (!object_detail::LayoutAccess::responsePhysics(object))          // +0x48
        return false;
    if (!mesh)
        objectLifecycleInvariantFailure();
    if (!mesh->hasLod(1))                                                // slot 11
        return false;
    if (!object->hasCollisionLod(2))                                     // slot 51
        return false;
    ICollisionMesh* objectMesh = static_cast<ICollisionMesh*>(object_detail::LayoutAccess::collisionMesh(object));   // +0x40
    getRootParent(object);                                               // called, result unused - as shipped
    const Mat4& objectToWorld = object->getAbsoluteTransformation();     // slot 33
    const Mat4& worldToObject = object->getInvAbsoluteTransformation();  // slot 34
    const int vertexCount = static_cast<int>(mesh->getVertexCount(1));    // signed loop bound in both images
    if (vertexCount == 0)
        return false;
    const Vec3& rayStart = *reinterpret_cast<const Vec3*>(&transform.m[12]);
    Vec3 hitNormal = Vec3::getZero();
    Vec3 hitPosition = Vec3::getZero();
    float planeDist, along;
    int material = 1;
    // Both callers zero one four-byte output record (PE +0x08 on the stack;
    // ELF -0x118). CollisionMesh writes LOD/geometry bytes 0/1 and its template
    // writes the face index at +2. [UNRESOLVED] source-level field names; retain
    // the attested carrier until Physics publishes CollisionFaceInfo's declaration.
    // [RETIRE-WHEN: scene-collision-face-info] Use that verified type, then rerun
    // client/server builds and the spawn-collision checks with the coordinator.
    unsigned int faceInfoStorage = 0;
    for (int i = 0; i < vertexCount; ++i)
    {
        const float* v = static_cast<const float*>(mesh->getVertexVector(1)) + 3 * i;   // slot 17, re-fetched per vertex as shipped
        Vec3 rayDelta;
        rayDelta.x = transform.m[4] * v[1] + transform.m[0] * v[0] + transform.m[8] * v[2];
        rayDelta.y = transform.m[5] * v[1] + transform.m[1] * v[0] + transform.m[9] * v[2];
        rayDelta.z = transform.m[6] * v[1] + transform.m[2] * v[0] + transform.m[10] * v[2];
        if (objectMesh->getDistance(2, false, objectToWorld, worldToObject, rayStart, rayDelta,
                                    hitNormal, hitPosition, planeDist, along, &material,
                                    reinterpret_cast<CollisionFaceInfo*>(&faceInfoStorage), true, false))
            return true;
    }
    return false;
}

// Radius for the swept-sphere soldier query; placement [INFERRED].
// This writable global is distinct from Soldier.cpp's local console-variable alias.
float g_soldierRadius = 0.5f;

// Tests a response-physics chain against the soldier's vertical swept sphere at collision LOD 2.
// Stops on the first hit, null object or missing response; soldier must be valid.
// extendToObjectHeight (chosen name) stretches the upward sweep to the object's origin height;
// false uses a downward sweep from +1 to -1. Preserve min/max argument order for NaNs.
// Transforms and output vectors are prepared before the LOD gate; only the collision bool is used.
bool _checkIfObjectIntersectsSoldier(IObject* object, IObject* soldier, bool extendToObjectHeight);

bool checkIfObjectIntersectsSoldier(IObject* object, IObject* soldier, bool extendToObjectHeight)
{
    if (!object)
        return false;

    for (;;)
    {
        IResponsePhysics* const response = static_cast<IResponsePhysics*>(
            object_detail::LayoutAccess::responsePhysics(object));              // +0x48
        if (!response)
            return false;
        if (_checkIfObjectIntersectsSoldier(object, soldier, extendToObjectHeight))
            return true;
        object = getNextChild(response);
        if (!object)
            return false;
    }
}

bool _checkIfObjectIntersectsSoldier(IObject* object, IObject* soldier, bool extendToObjectHeight)
{
    const Mat4& objectToWorld = object->getAbsoluteTransformation();            // slot 33
    Mat4 worldToObject = object->getInvAbsoluteTransformation();               // slot 34 - copied
    const Mat4& soldierToWorld = soldier->getAbsoluteTransformation();          // slot 33

    std::vector<Vec3> hitNormals;
    std::vector<Vec3> hitPositions;
    std::vector<float> hitPlaneDists;
    std::vector<float> hitAlongs;

    if (!object->hasCollisionLod(2))                                            // slot 51
        return false;

    ICollisionMesh* const objectMesh = static_cast<ICollisionMesh*>(
        object_detail::LayoutAccess::collisionMesh(object));                    // +0x40, unchecked

    const float bottom = extendToObjectHeight
                       ? std::min(-1.0f, objectToWorld.m[13] - soldierToWorld.m[13])
                       : 1.0f;
    const float top = extendToObjectHeight
                    ? std::max(objectToWorld.m[13] - soldierToWorld.m[13], 1.0f)
                    : -1.0f;

    Vec3 offset;
    offset.x = 0.0f; offset.y = bottom; offset.z = 0.0f;
    Vec3 sweep;
    sweep.x = 0.0f; sweep.y = top - bottom; sweep.z = 0.0f;
    Vec3 center;
    center.x = soldierToWorld.m[12] + offset.x;
    center.y = soldierToWorld.m[13] + offset.y;
    center.z = soldierToWorld.m[14] + offset.z;

    return objectMesh->getDistanceToSphere(2, true, objectToWorld, worldToObject, center, sweep,
                                           g_soldierRadius, hitNormals, hitPositions,
                                           hitPlaneDists, hitAlongs, 0);        // slot 25
}

// Tests the segment from from to to for an object blocker, excluding the caller's root hierarchy.
// The predicate also filters optional soldiers and SimpleObject special-material bit 1.
// Terrain is excluded. The result comes from the hit pointer, not intersectLine's bool, and the
// soldier filter is repeated afterward. Preserve both root checks and both soldier checks.
class LineTestPredicator : public ObjectFlagPredicator
{
public:
    LineTestPredicator(unsigned int flags, IObject* root, bool ignoreSoldiers)
        : ObjectFlagPredicator(flags), m_root(root), m_ignoreSoldiers(ignoreSoldiers) {}

    virtual bool predicate(IObject* object);

    IObject* m_root;                                // +0x08
    bool     m_ignoreSoldiers;                      // +0x0C
};

bool LineTestPredicator::predicate(IObject* object)
{
    typedef object_detail::LayoutAccess L;

    if (m_ignoreSoldiers &&
        L::objectTemplate(object)->getClassID() == CID_SoldierTemplate)      // raw +0x14
        return false;

    // the root test is made twice against the same stored root, as compiled.
    if (getRootParent(object) == m_root)
        return false;
    if (object == m_root)
        return false;

    if (L::objectTemplate(object)->getClassID() == CID_SimpleObjectTemplate)
    {
        // bit 1 of the special materials = transparent to the ray. See the header block.
        if ((L::objectTemplate(object)->getSpecialMaterials() & 2) != 0)     // +0xD0
            return false;
    }

    return ObjectFlagPredicator::predicate(object);
}

IObject* checkIfRayHitAnyObject(Vec3 from, Vec3 to, IObject* object, bool ignoreSoldiers)
{
    LineTestPredicator predicator(0x100, getRootParent(object), ignoreSoldiers);

    Vec3 delta;
    delta.x = to.x - from.x;
    delta.y = to.y - from.y;
    delta.z = to.z - from.z;

    IObject* hitObject = 0;
    Vec3 hitPoint;   hitPoint.x  = 0.0f; hitPoint.y  = 0.0f; hitPoint.z  = 0.0f;
    Vec3 hitNormal;  hitNormal.x = 0.0f; hitNormal.y = 0.0f; hitNormal.z = 0.0f;
    int  hitPart = 0;

    // the second Vec3 is the delta, not the end point - the callee takes a direction here.
    // The object-line chain is live; this call deliberately does not request terrain.
    objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, from, delta,
                                 &predicator, true, false, false, true, 0);   // +0x68

    if (!hitObject)
        return 0;
    // the soldier filter again, on the same flag the predicator already applied.
    if (ignoreSoldiers &&
        object_detail::LayoutAccess::objectTemplate(hitObject)->getClassID()
            == CID_SoldierTemplate)
        return 0;
    return hitObject;
}

// Tests objects and optionally ground, returning true for terrain even when hitObject
// is null. Uses caller-supplied hit-point storage or a zero-initialized local.
bool checkIfRayHitAnyObjectOrGround(const Vec3& from, const Vec3& to, const IObject* object,
                                    IObject*& hitObject, bool includeTerrain,
                                    bool terrainTwoSided, bool ignoreSoldiers,
                                    Vec3* hitPoint)
{
    IObject* root = object ? getRootParent(object) : 0;
    LineTestPredicator predicator(0x100, root, ignoreSoldiers);

    Vec3 delta;
    delta.x = to.x - from.x;
    delta.y = to.y - from.y;
    delta.z = to.z - from.z;

    IObject* found = 0;
    Vec3 localHitPoint;
    localHitPoint.x = 0.0f;
    localHitPoint.y = 0.0f;
    localHitPoint.z = 0.0f;
    Vec3 hitNormal;
    hitNormal.x = 0.0f;
    hitNormal.y = 0.0f;
    hitNormal.z = 0.0f;
    int hitPart = 0;

    Vec3& point = hitPoint ? *hitPoint : localHitPoint;
    const bool hit = objectManager->intersectLine(found, point, hitNormal, hitPart, from, delta,
                                                   &predicator, true, false, includeTerrain,
                                                   terrainTwoSided, 0);

    if (found && ignoreSoldiers
        && object_detail::LayoutAccess::objectTemplate(found)->getClassID()
               == CID_SoldierTemplate)
    {
        hitObject = 0;
        return false;
    }

    hitObject = found;
    return hit;
}

// ELF 0x87FA5A0 / PE 0x6D0FC0. Aggregation is rejected; QI failure releases the new object.
IBase* Object::create(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    Object* object = new Object(0);
    IBase* result = static_cast<IBase*>(object->queryInterface(iid));
    if (!result) object->release();
    return result;
}

Object::Object(const IObjectTemplate* objectTemplate)
    : m_refCount(1),
      m_absoluteTransformation(Mat4::getIdentity()),
      m_invertedAbsoluteTransformation(0),
      m_relativeTransformation(const_cast<Mat4*>(&Mat4::getIdentity())),
      m_absolutePositionSecondary(Vec3::getIdentity()),
      m_rotationSecondary(Vec3::getIdentity()),
      m_visualBoundingRadius(0.0f),
      m_physicBoundingRadius(0.0f),
      m_updateFrequency(0),
      m_updateFrequencyType(UpdateFreqencyType_UNRECOVERED),
      m_cacheFlags(0),
      m_fullBoundingBox(0),
      m_mineBoundingBox(0),
      m_inputId(-1),
      m_childId(-1),
      m_layerId(-1),
      m_layer(0),
      m_group(0),
      m_isObjectDestroyed(false),
      m_isOvergrowth(false),
      m_notInAI(false),
      m_disableChildren(false),
      m_objectInDestroyList(false)
{
    m_cachedRootParent = 0;
    m_objectId = 0;
    m_template = objectTemplate;
    m_parent = 0;
    m_sibling = 0;
    m_child = 0;
    m_aiObject = 0;
    m_geometry = 0;
    m_armorComponent = 0;
    m_collisionMesh = 0;
    m_physicsNode = 0;
    m_responsePhysics = 0;
    m_networkable = 0;
    m_postProcessComponent = 0;
    m_vehicleHudComponent = 0;
    m_helpHudComponent = 0;
    m_weaponHudComponent = 0;
    m_abilityHudComponent = 0;
    m_abilityGLComponent = 0;
    m_radioGLComponent = 0;
    m_warningHudComponent = 0;
    m_windAffector = 0;
    m_strategicObjectComponent = 0;
    m_createdOnLoad = false;
    m_flags = 0x01008200u;

    setTransformationIsDirty(this);
    setBoundRadiusIsDirty(this);

    if (objectManager)
    {
        if (!mObjectManager)
            mObjectManager = objectManager;
        m_objectId = mObjectManager->getNewID();
        mObjectManager->registerObject(this);
    }
    if (m_template)
        const_cast<IObjectTemplate*>(m_template)->objectCreated(this);
}

// PE 0x007200D0; ELF 0x083084C0. Cached or undestroyed objects fail the shipped invariants.
Object::~Object()
{
    if ((m_flags & 0x20u) != 0 || m_objectCacheId != 0 || !m_isObjectDestroyed)
        objectLifecycleInvariantFailure();
    updateFlags(0, 0x17C18u);
    mObjectManager->unregisterObject(this);
    const_cast<IObjectTemplate*>(m_template)->objectDestroyed(this);
    delete m_mineBoundingBox;
    delete m_fullBoundingBox;
    ::operator delete(m_invertedAbsoluteTransformation);
}

void Object::addRef() { ++m_refCount; }
int Object::getRef() { return m_refCount; }
int Object::release()
{
    const int refs = --m_refCount;
    if (refs == 0)
        delete this;
    return refs;
}

void* Object::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObject)
        return const_cast<Object*>(this);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<Object*>(this));
    return 0;
}

unsigned int Object::getClassID() const { return CID_Object; }

void Object::init()
{
    m_isObjectDestroyed = false;
    if (!m_template)
        return;

    if ((m_template->getFlags() & 4u) == 0)
        updateFlags(2u, 0);
    if ((m_template->getFlags() & 2u) == 0)
        updateFlags(0x400u, 0);
    const_cast<IObjectTemplate*>(m_template)->updateFlags(0x20u, 0);
    initNetworkableComp();

    IGameLogic* logic = gameLogic;
    if (logic && logic->isServer())
    {
        IPhysicsNode* physicsNode = static_cast<IPhysicsNode*>(m_physicsNode);
        if (getNetworkable() && physicsNode && physicsNode->getIsMobile() && !m_parent)
            physicsNode->initLatencyCompensation();
    }

    bool hasComponentUpdate = false;
    for (ComponentMap::iterator it = m_components.begin(); it != m_components.end(); ++it)
    {
        IObjectComponent* component = it->second.get();
        component->init(this);
        if (component->hasUpdate())
            hasComponentUpdate = true;
    }
    if (hasComponentUpdate)
        updateFlags(0x400800u, 0);
}

void Object::disableCachedObject()
{
    if ((m_flags & 0x20u) != 0 || getObjectCacheID() == 0)
        objectLifecycleInvariantFailure();

    updateFlags(0x20u, 0);

    IObject* child = getChild();
    while (child)
    {
        if (getObjectCacheID() == child->getObjectCacheID())
        {
            child->disableCachedObject();
            child = object_detail::LayoutAccess::sibling(child);
        }
        else
        {
            IObject* next = object_detail::LayoutAccess::sibling(child);
            objectManager->destroyObject(child);
            child = next;
        }
    }

    while (!m_components.empty())
    {
        ComponentMap::iterator it = m_components.begin();
        IObjectComponent* component = it->second.get();
        if (component)
        {
            component->destroy();
            setComponent(it->first, 0);
        }
        else
            m_components.erase(it);
    }

    SetActiveLod(ObjectLodType_UNRECOVERED);
    m_interpolationTransformation = 0;
    if (!m_parent)
        setAbsoluteTransformation(Mat4::getIdentity());
    if (m_physicsNode)
        resetCachedPhysicsNode(m_physicsNode);
    if (m_responsePhysics)
        resetCachedResponsePhysics(m_responsePhysics);
    if (m_collisionMesh)
        static_cast<ICollisionMesh*>(m_collisionMesh)->reset();
    updateFlags(0x40000001u, 0);
    updateGeometry();
}

void Object::enableCachedObject()
{
    if ((m_flags & 0x20u) == 0 || getObjectCacheID() == 0)
        objectLifecycleInvariantFailure();

    const unsigned int clear = 0x20u | ((m_cacheFlags & 1u) ? 0u : 1u);
    updateFlags(m_cacheFlags, clear);
    const_cast<IObjectTemplate*>(m_template)->objectCreated(this);

    for (ComponentMap::iterator it = m_components.begin(); it != m_components.end(); ++it)
    {
        IObjectComponent* component = it->second.get();
        if (component)
            component->init(this);
    }

    if ((m_flags & 1u) != (m_cacheFlags & 1u))
        objectLifecycleInvariantFailure();

    bool skipChild = false;
    bool skipSibling = false;
    IObject* root = getRootParent(this);
    if (root)
    {
        const std::string rootName(root->getTemplate()->getName());
        bool teamDropVehicle =
            compareIgnoreCase(rootName, gameLogic->getTeamDropVehicle(1)) == 0;
        if (!teamDropVehicle)
            teamDropVehicle =
                compareIgnoreCase(rootName, gameLogic->getTeamDropVehicle(2)) == 0;

        if (teamDropVehicle)
        {
            if (getChild() && getChild()->getTemplate() &&
                getChild()->getTemplate()->getName() == "supply_parachute")
                skipChild = true;
            if (m_sibling && m_sibling->getTemplate() &&
                m_sibling->getTemplate()->getName() == "supply_parachute")
                skipSibling = true;
        }
    }

    if (getChild() && !skipChild)
        getChild()->enableCachedObject();
    if (m_sibling && !skipSibling)
        m_sibling->enableCachedObject();
}

// PE 0x0071D4E0; ELF 0x083092D0. A second nonzero cache assignment is fatal.
void Object::setObjectCacheID(unsigned int id)
{
    m_objectCacheId = id;
    if (!id)
    {
        m_cacheFlags = 0;
        return;
    }
    if (m_cacheFlags != 0) objectLifecycleInvariantFailure();
    m_cacheFlags = (m_flags & 0x8FC3FFDFu) | 0x40000000u;
}

// PE 0x0071FB80; ELF 0x08309FE0. Cached/parked objects cannot enter this destruction path.
void Object::destroy(bool force)
{
    if (getObjectCacheID() != 0 || (m_flags & 0x20u) != 0)
        objectLifecycleInvariantFailure();
    if (m_isObjectDestroyed)
        return;

    m_isObjectDestroyed = true;
    for (ComponentMap::iterator it = m_components.begin(); it != m_components.end(); ++it)
    {
        IObjectComponent* component = it->second.get();
        if (component)
            component->destroy();
    }
    mObjectManager->removeObjectFromGrid(this);
    if (getNetworkable())
        eventManager->postEvent(static_cast<EventCategory>(6), 17, this, 0.0f);

    if (!force)
        return;
    if (m_parent)
        m_parent->removeChild(this);

    std::vector<IObject*> children;
    findAllChildren(getChild(), children);
    for (std::vector<IObject*>::iterator it = children.begin(); it != children.end(); ++it)
    {
        if (!(*it)->isObjectDestroyed())
            (*it)->destroy(false);
    }
    mObjectManager->addObjectToDestroyList(this);
}

// PE 0x0071D680; ELF 0x0830A270. The reserved dirty bits cannot be set through this API.
void Object::updateFlags(unsigned int set, unsigned int clear)
{
    if ((set & 0x303C0000u) != 0)
        objectLifecycleInvariantFailure();

    const unsigned int oldFlags = m_flags;
    unsigned int newlySet = ~oldFlags & set;
    unsigned int newlyCleared = oldFlags & clear;
    if (!newlySet && !newlyCleared)
        return;

    if ((newlySet & 1u) != 0)
    {
        if ((m_flags & 0x800u) != 0)
            mObjectManager->removeObjectFromUpdate(this);
        if ((m_flags & 0x2000u) != 0)
            mObjectManager->removeObjectFromFrameUpdate(this);
    }
    else if ((newlyCleared & 1u) != 0)
    {
        if (((m_flags | newlySet) & 0x800u) != 0 &&
            (newlyCleared & 0x800u) == 0)
            mObjectManager->addObjectToUpdate(this);
        if (((m_flags | newlySet) & 0x2000u) != 0 &&
            (newlyCleared & 0x2000u) == 0)
            mObjectManager->addObjectToFrameUpdate(this);
    }
    else if ((m_flags & 1u) == 0)
    {
        if ((newlySet & 0x2000u) != 0)
            mObjectManager->addObjectToFrameUpdate(this);
        else if ((newlyCleared & 0x2000u) != 0)
            mObjectManager->removeObjectFromFrameUpdate(this);

        if ((newlySet & 0x800u) != 0)
            mObjectManager->addObjectToUpdate(this);
        else if ((newlyCleared & 0x800u) != 0)
            mObjectManager->removeObjectFromUpdate(this);
    }

    if (((newlySet | newlyCleared) & 1u) != 0)
        setDisableFlagHasChanged(this, (newlySet & 1u) != 0);

    if ((newlySet & 0x4000u) != 0)
        mObjectManager->addObjectToNameHash(this);
    else if ((newlyCleared & 0x4000u) != 0)
        mObjectManager->removeObjectFromNameHash(this);

    if ((newlyCleared & 0x8000u) != 0)
    {
        setTransformationIsDirty(this);
        if (!m_parent)
            newlyCleared &= ~0x8000u;
    }
    else if ((newlySet & 0x8000u) != 0)
        setTransformationIsDirty(this);

    if ((newlySet & 8u) != 0)
        updateDirectChildrenFlags(this, 0x10u, 0);
    else if ((newlyCleared & 8u) != 0)
        updateDirectChildrenFlags(this, 0, 0x10u);

    if ((newlySet & 0x10u) != 0)
        updateDirectChildrenFlags(this, 0x10u, 0);
    else if ((newlyCleared & 0x10u) != 0 && (m_flags & 8u) == 0)
        updateDirectChildrenFlags(this, 0, 0x10u);

    if (!m_parent)
    {
        if ((newlySet & 0x400u) != 0)
            mObjectManager->addObjectToGrid(this);
        else if ((newlyCleared & 0x400u) != 0)
            mObjectManager->removeObjectFromGrid(this);
    }

    m_flags = (m_flags | newlySet) & ~newlyCleared;
    if ((newlySet & 0x100u) != 0)
    {
        responsePhysicsManager->addObject(this);
        setResponseCacheToDirty(this);
    }
    else if ((newlyCleared & 0x100u) != 0)
    {
        responsePhysicsManager->removeObject(this);
        setResponseCacheToDirty(this);
    }
}

// ELF 0x830B850 / PE 0x6DDF20.  Runtime components and ordinary IBase attachments share this
// entry point.  The queried IObjectComponent subobject is stored, while the original incoming
// object supplies the ownership reference; these addresses can differ under multiple inheritance.
bool Object::setComponent(unsigned int classId, IBase* component)
{
    IObjectComponent* objectComponent = 0;
    if (component)
    {
        objectComponent = static_cast<IObjectComponent*>(
            component->queryInterface(IID_IObjectComponent));
        if (objectComponent)
            objectComponent->setObject(this);
    }

    if (classId == IID_IPhysicsNode)
        m_physicsNode = component ? component->queryInterface(classId) : 0;
    else if (classId == ai::IID_IAIObject)
        m_aiObject = component ? component->queryInterface(classId) : 0;
    else if (classId == io::IID_INetworkable)
        m_networkable = component
            ? static_cast<io::INetworkable*>(component->queryInterface(classId)) : 0;
    else if (classId == IID_IResponsePhysics)
    {
        if (component)
        {
            m_responsePhysics = component->queryInterface(classId);
            setResponseCacheToDirty(this);
        }
        else
        {
            setResponseCacheToDirty(this);
            m_responsePhysics = 0;
        }
    }
    else if (classId == IID_ICollisionMesh)
    {
        m_collisionMesh = component ? component->queryInterface(classId) : 0;
        setBoundRadiusIsDirty(this);
    }
    else if (classId == IID_IArmorGLComp)
        m_armorComponent = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;
    else if (classId == IID_IPostProcessComp)
        m_postProcessComponent = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;
    else if (classId == IID_IVehicleHudComp)
        m_vehicleHudComponent = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;
    else if (classId == IID_IHelpHudComp)
        m_helpHudComponent = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;
    else if (classId == IID_IWeaponHudComp)
        m_weaponHudComponent = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;
    else if (classId == IID_IAbilityHudComp)
        m_abilityHudComponent = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;
    else if (classId == IID_IAbilityGLComp)
        m_abilityGLComponent = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;
    else if (classId == IID_IRadioGLComp)
        m_radioGLComponent = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;
    else if (classId == IID_IWarningHudComp)
        m_warningHudComponent = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;
    else if (classId == geom::IID_IGeometry)
    {
        m_geometry = component ? component->queryInterface(classId) : 0;
        setBoundRadiusIsDirty(this);
    }
    else if (classId == IID_IWindAffector)
        m_windAffector = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;
    else if (classId == IID_IStrategicObjectComp)
        m_strategicObjectComponent = component
            ? static_cast<IObjectComponent*>(component->queryInterface(classId)) : 0;

    if (component)
    {
        if (objectComponent)
        {
            ComponentMap::iterator slot = m_components.lower_bound(classId);
            if (slot == m_components.end() || classId < slot->first)
                slot = m_components.insert(
                    slot, std::make_pair(classId, SmartPtr<IObjectComponent>()));
            slot->second.assignQueriedInterface(objectComponent, component);
        }
        else
        {
            BaseMap::iterator slot = m_bases.lower_bound(classId);
            if (slot == m_bases.end() || classId < slot->first)
                slot = m_bases.insert(slot, std::make_pair(classId, SmartPtr<IBase>()));
            slot->second.assignQueriedInterface(component, component);
        }
    }
    else
    {
        m_bases.erase(classId);
        m_components.erase(classId);
    }
    return true;
}

// ELF 0x830B5F0 / PE 0x6DD7E0.  The seventeen hot interfaces bypass both maps.  General IBase
// attachments precede IObjectComponent attachments, and a hit that cannot supply interfaceId ends
// the search rather than falling through to the other map.
IBase* Object::queryComponent(unsigned int classId, unsigned int interfaceId)
{
    IBase* fast = 0;
    if (classId == IID_IPhysicsNode) fast = reinterpret_cast<IBase*>(m_physicsNode);
    else if (classId == IID_IResponsePhysics) fast = reinterpret_cast<IBase*>(m_responsePhysics);
    else if (classId == geom::IID_IGeometry) fast = reinterpret_cast<IBase*>(m_geometry);
    else if (classId == IID_ICollisionMesh) fast = reinterpret_cast<IBase*>(m_collisionMesh);
    else if (classId == io::IID_INetworkable) fast = reinterpret_cast<IBase*>(m_networkable);
    else if (classId == ai::IID_IAIObject) fast = reinterpret_cast<IBase*>(m_aiObject);
    else if (classId == IID_IArmorGLComp) fast = m_armorComponent;
    else if (classId == IID_IPostProcessComp) fast = m_postProcessComponent;
    else if (classId == IID_IVehicleHudComp) fast = m_vehicleHudComponent;
    else if (classId == IID_IHelpHudComp) fast = m_helpHudComponent;
    else if (classId == IID_IWeaponHudComp) fast = m_weaponHudComponent;
    else if (classId == IID_IAbilityHudComp) fast = m_abilityHudComponent;
    else if (classId == IID_IAbilityGLComp) fast = m_abilityGLComponent;
    else if (classId == IID_IRadioGLComp) fast = m_radioGLComponent;
    else if (classId == IID_IWarningHudComp) fast = m_warningHudComponent;
    else if (classId == IID_IWindAffector) fast = m_windAffector;
    else if (classId == IID_IStrategicObjectComp) fast = m_strategicObjectComponent;

    if (fast)
        return interfaceId == classId
            ? fast
            : static_cast<IBase*>(fast->queryInterface(interfaceId));

    BaseMap::iterator base = m_bases.find(classId);
    if (base != m_bases.end())
        return static_cast<IBase*>(base->second->queryInterface(interfaceId));

    ComponentMap::iterator objectComp = m_components.find(classId);
    if (objectComp != m_components.end())
        return static_cast<IBase*>(objectComp->second->queryInterface(interfaceId));
    return 0;
}

// Finds the first object with the given template name, requiring non-null templates.
// Searches self, then siblings, then children. Preserve the two virtual getChild calls.
IObject* findObjectWithName(IObject* object, const std::string& name)
{
    if (object->getTemplate()->getName() == name)
        return object;

    IObject* found = 0;
    IObject* sibling = object_detail::LayoutAccess::sibling(object);
    if (sibling)
        found = findObjectWithName(sibling, name);

    if (!found)
    {
        if (!object->getChild())
            return 0;

        found = findObjectWithName(object->getChild(), name);
        if (!found)
            return 0;
    }

    return found;
}

// Returns whether terrain or an object lies within physicBoundingRadius minus margin below object.
// Terrain hits return the terrain normal; object hits return up. Water is not tested.
// outNormal is written by the terrain query even when the final result is false.
// The bool return type is [INFERRED] from the materialized value and caller's conditional use.
bool isObjectCloseToGround(IObject* object, Vec3& outNormal, float margin)
{
    const Vec3  pos    = object->getAbsolutePosition();          // slot +0x6C
    const float radius = object->getPhysicBoundingRadius();      // slot +0x98

    unsigned char material = 0;
    const float height = static_cast<IHeightmapCluster*>(heightmapCluster)
                             ->getHeightAndNormalInWorldCoords(pos.x, pos.z, outNormal, material);
    if (height > pos.y - radius + margin)
        return true;                    // outNormal keeps the terrain normal - not overwritten

    // The image materialises `Vec3(0, radius - margin, 0)` and subtracts it from pos, which is why
    // x and z carry an explicit `- 0.0f`. This tree's Vec3 has no arithmetic operators, so the
    // subtraction is spelled out rather than dropped.
    Vec3 offset;
    offset.x = 0.0f;
    offset.y = radius - margin;
    offset.z = 0.0f;

    Vec3 to;
    to.x = pos.x - offset.x;
    to.y = pos.y - offset.y;
    to.z = pos.z - offset.z;

    if (!checkIfRayHitAnyObject(pos, to, object, true))
        return false;

    // A Vec3 temporary in the image, copied out over the terrain normal the heightmap wrote.
    outNormal.x = 0.0f;
    outNormal.y = 1.0f;
    outNormal.z = 0.0f;
    return true;
}

bool Object::isObjectDestroyed() { return m_isObjectDestroyed; }
void Object::setObjectInDestroyList(bool value) { m_objectInDestroyList = value; }
bool Object::getObjectInDestroyList() const { return m_objectInDestroyList; }

const std::string& Object::getName() const { return m_name; }
void Object::setName(const std::string& name)
{
    if (!m_name.empty()) updateFlags(0, 0x4000u);
    m_name = name;
    updateFlags(0x4000u, 0);
}

const Vec3& Object::getAbsolutePosition() const
{
    const Mat4& transform = Object::getAbsoluteTransformation();
    return *reinterpret_cast<const Vec3*>(&transform.m[12]);
}
void Object::setAbsolutePosition(const Vec3& p)
{
    if (m_parent)
    {
        const Mat4& inverse = m_parent->getInvAbsoluteTransformation();
        m_relativeTransformation->m[12] =
            p.x * inverse.m[0] + p.y * inverse.m[4] + p.z * inverse.m[8] + inverse.m[12];
        m_relativeTransformation->m[13] =
            p.x * inverse.m[1] + p.y * inverse.m[5] + p.z * inverse.m[9] + inverse.m[13];
        m_relativeTransformation->m[14] =
            p.x * inverse.m[2] + p.y * inverse.m[6] + p.z * inverse.m[10] + inverse.m[14];
        setBoundRadiusIsDirty(this);
    }
    else
    {
        m_absoluteTransformation.m[12] = p.x;
        m_absoluteTransformation.m[13] = p.y;
        m_absoluteTransformation.m[14] = p.z;
    }
    transformationWasUpdated(this);
}
const Vec3& Object::getAbsolutePositionSecondary() const { return m_absolutePositionSecondary; }
void Object::setAbsolutePositionSecondary(const Vec3& p) { m_absolutePositionSecondary = p; }
const Vec3& Object::getRotationSecondary() { return m_rotationSecondary; }
void Object::setRotationSecondary(const Vec3& p) { m_rotationSecondary = p; }
const Mat4& Object::getAbsoluteTransformation() const
{
    Object* self = const_cast<Object*>(this);
    unsigned int flags = self->m_flags;
    if ((flags & 0x00088000u) == 0x00080000u)
    {
        self->m_absoluteTransformation.mult(
            *self->m_relativeTransformation,
            self->m_parent->getAbsoluteTransformation());
        flags = self->m_flags;
    }
    self->m_flags = flags & ~0x00080000u;
    return self->m_absoluteTransformation;
}
const Mat4& Object::getInvAbsoluteTransformation() const
{
    Object* self = const_cast<Object*>(this);
    if (!self->m_invertedAbsoluteTransformation)
        self->m_invertedAbsoluteTransformation = new Mat4(Mat4::getIdentity());
    if ((self->m_flags & 0x10000000u) != 0)
    {
        self->m_invertedAbsoluteTransformation->setInverse(getAbsoluteTransformation());
        self->m_flags &= ~0x10000000u;
    }
    return *self->m_invertedAbsoluteTransformation;
}
void Object::setAbsoluteTransformation(const Mat4& transform)
{
    if (m_parent)
    {
        const Mat4 oldRelative = *m_relativeTransformation;
        m_relativeTransformation->mult(transform, m_parent->getInvAbsoluteTransformation());
        if (translationDistanceSquared(oldRelative, *m_relativeTransformation) > 0.0001f)
            setBoundRadiusIsDirty(this);
    }
    else
        m_absoluteTransformation = transform;
    transformationWasUpdated(this);
}
const Mat4& Object::getVisualAbsoluteTransformation() const
{
    return getAbsoluteTransformation();
}

// PE 0x0071EBB0; ELF 0x0830A610. Nonpositive loaded geometry bounds fail the shipped invariant.
float Object::getVisualBoundingRadius() const
{
    Object* self = const_cast<Object*>(this);
    if ((self->m_flags & 1u) != 0)
    {
        self->m_visualBoundingRadius = 0.0f;
        self->m_flags |= 0x00040000u;
        return 0.0f;
    }
    if ((self->m_flags & 0x00040000u) == 0)
        return self->m_visualBoundingRadius;

    float candidate = 0.0f;
    if (self->m_collisionMesh)
        candidate = static_cast<ICollisionMesh*>(self->m_collisionMesh)->getMaxBoundingRadius();
    else if (self->m_geometry)
    {
#if defined(_MSC_VER)
        const unsigned int getTemplateSlot = 0x38;
        const unsigned int getRadiusSlot = 0x44;
        typedef geom::IGeometryTemplate* (BF2_OBJECT_VCALL *GetGeometryTemplateFn)(void*);
        typedef float (BF2_OBJECT_VCALL *GetGeometryRadiusFn)(void*);
        // An unloaded geometry template has no queryable bounds and returns zero here.
        geom::IGeometryTemplate* geometryTemplate = objectRawSlot<GetGeometryTemplateFn>(
            self->m_geometry, getTemplateSlot)(self->m_geometry);
        if (!geometryTemplate->isLoaded())
            return 0.0f;
        candidate = objectRawSlot<GetGeometryRadiusFn>(
            self->m_geometry, getRadiusSlot)(self->m_geometry);
#else
        // Non-MSVC: the named geometry_part operations (ELF +0x3C / +0x48).
        geom::IGeometryTemplate* geometryTemplate =
            static_cast<geom::IGeometryTemplate*>(world::geometry_part::getTemplate(self->m_geometry));
        if (!geometryTemplate->isLoaded())
            return 0.0f;
        candidate = world::geometry_part::getBoundingRadius(self->m_geometry);
#endif
        if (!(candidate > 0.0f))
        {
            // Shipped invariant, preserved: BF2.exe 0x71EBB0 reaches INT3 at 0x71EC2F;
            // ELF 0x830A610 aborts here. RW-204 fixes the missing SkinnedMesh constructor
            // selection that supplied zero for flag_us, without weakening this check.
#if defined(_MSC_VER)
            __debugbreak();
#else
            std::abort();
#endif
        }
    }

    candidate += getBoundingRadiusModifier();
    for (IObject* child = getChild(); child;
         child = object_detail::LayoutAccess::sibling(child))
    {
        if (child->getClassID() == CID_LightSource)
            continue;
        const float childRadius = child->getVisualBoundingRadius();
        if (childRadius > 0.0f)
        {
#if defined(_MSC_VER)
            candidate = std::max(candidate,
                                 vectorLength(child->getRelativePosition()) + childRadius);
#else
            // [CHOSEN, not recovered] Preserve the native x87 length through the radius
            // addition and comparison; only the selected candidate is stored as a float.
            const Vec3& position = child->getRelativePosition();
            const bf2x87::Real x = bf2x87::wide(position.x);
            const bf2x87::Real y = bf2x87::wide(position.y);
            const bf2x87::Real z = bf2x87::wide(position.z);
            const bf2x87::Real radius = bf2x87::squareRoot((x * x + y * y) + z * z)
                + bf2x87::wide(childRadius);
            if (radius > bf2x87::wide(candidate))
                candidate = bf2x87::store(radius);
#endif
        }
    }

    if (std::fabs(candidate - self->m_visualBoundingRadius) > 0.0001f)
    {
        self->m_flags |= 0x00100000u;
        self->m_visualBoundingRadius = candidate;
    }
    self->m_flags &= ~0x00040000u;
    return self->m_visualBoundingRadius;
}

float Object::getPhysicBoundingRadius() const
{
    Object* self = const_cast<Object*>(this);
    if ((self->m_flags & 1u) != 0)
    {
        self->m_physicBoundingRadius = 0.0f;
        self->m_flags |= 0x20000000u;
        return 0.0f;
    }
    if ((self->m_flags & 0x20000000u) == 0)
        return self->m_physicBoundingRadius;

    float candidate = self->m_collisionMesh
        ? static_cast<ICollisionMesh*>(self->m_collisionMesh)->getMaxBoundingRadius() : 0.0f;
    candidate += getBoundingRadiusModifier();
    for (IObject* child = getChild(); child;
         child = object_detail::LayoutAccess::sibling(child))
    {
        const float childRadius = child->getPhysicBoundingRadius();
        if (childRadius > 0.0f)
            candidate = std::max(candidate,
                                 vectorLength(child->getRelativePosition()) + childRadius);
    }

    if (std::fabs(candidate - self->m_physicBoundingRadius) > 0.0001f)
    {
        self->m_flags |= 0x00100000u;
        self->m_physicBoundingRadius = candidate;
    }
    self->m_flags &= ~0x20000000u;
    return self->m_physicBoundingRadius;
}

float Object::getBoundingRadiusModifier() const { return 0.0f; }

LocalBoundingBox* Object::getFullBoundingBox()
{
    if (!m_fullBoundingBox)
    {
        m_fullBoundingBox = new LocalBoundingBox;
        buildFullBoundingBox();
    }
    return m_fullBoundingBox;
}

LocalBoundingBox* Object::getMineBoundingBox()
{
    if (!m_mineBoundingBox)
    {
        m_mineBoundingBox = new LocalBoundingBox;
        buildMineBoundingBox();
    }
    return m_mineBoundingBox;
}

bool Object::buildFullBoundingBox()
{
    bool found = false;
    if (m_collisionMesh)
    {
        Vec3 minimum = Vec3::getZero();
        Vec3 maximum = Vec3::getZero();
        if (static_cast<ICollisionMesh*>(m_collisionMesh)->getBoundingBoxMinMax(minimum, maximum))
        {
            setBoundingEndpoints(m_fullBoundingBox, minimum, maximum);
            found = true;
        }
    }

    for (IObject* child = getChild(); child;
         child = object_detail::LayoutAccess::sibling(child))
    {
        Object* childObject = static_cast<Object*>(child);
        if (!childObject->buildFullBoundingBox())
            continue;

        const Mat4& relative = child->getRelativeTransformation();
        const Vec3 minimum = transformBoundingPoint(
            relative, child->getFullBoundingBox()->m_min);
        const Vec3 maximum = transformBoundingPoint(
            relative, child->getFullBoundingBox()->m_max);
        mergeBoundingEndpoints(m_fullBoundingBox, minimum, maximum, found);
        found = true;
    }
    return found;
}

bool Object::buildMineBoundingBox()
{
    bool found = false;
    if (m_collisionMesh &&
        (m_template->getClassID() == CID_SpringTemplate || (m_flags & 0x01000000u) != 0))
    {
        Vec3 minimum = Vec3::getZero();
        Vec3 maximum = Vec3::getZero();
        if (static_cast<ICollisionMesh*>(m_collisionMesh)->getBoundingBoxMinMax(minimum, maximum))
        {
            setBoundingEndpoints(m_mineBoundingBox, minimum, maximum);
            found = true;
        }
    }

    for (IObject* child = getChild(); child;
         child = object_detail::LayoutAccess::sibling(child))
    {
        Object* childObject = static_cast<Object*>(child);
        if (!childObject->buildMineBoundingBox())
            continue;

        const Mat4& relative = child->getRelativeTransformation();
        const Vec3 minimum = transformBoundingPoint(
            relative, child->getMineBoundingBox()->m_min);
        const Vec3 maximum = transformBoundingPoint(
            relative, child->getMineBoundingBox()->m_max);
        mergeBoundingEndpoints(m_mineBoundingBox, minimum, maximum, found);
        found = true;
    }
    return found;
}

void Object::handleVisualUpdate(float, float) {}
void Object::handleFrameUpdate(float) {}
void Object::handleUpdate(float, unsigned int) {}
bool Object::handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&, int, int,
                             CollisionFaceInfo*, CollisionFaceInfo*) { return true; }
void Object::handlePlayerInput(IPlayer*, const io::PlayerInput&, float, unsigned int) {}
void Object::handleMessage(TemplateMessage, IPlayer*, bool) {}

int Object::getUpdateFrequency() const { return m_updateFrequency; }
UpdateFreqencyType Object::getUpdateFrequencyType() const { return m_updateFrequencyType; }
void Object::setUpdateFrequencyType(UpdateFreqencyType type) { m_updateFrequencyType = type; }
int Object::getMaterial() const { return 0; }

void Object::SetActiveLod(ObjectLodType lod)
{
    void* geometry = m_geometry;
    if (geometry)
    {
#if defined(_MSC_VER)
        const unsigned int setSlot = 0x6C;
        const unsigned int countSlot = 0x74;
        typedef int (BF2_OBJECT_VCALL *GetLodCountFn)(void*);
        typedef void (BF2_OBJECT_VCALL *SetGeometryLodFn)(void*, int);
        const int count = objectRawSlot<GetLodCountFn>(geometry, countSlot)(geometry);
        if (count <= 1)
            updateFlags(lod == static_cast<ObjectLodType>(1) ? 1u : 0u,
                        lod == static_cast<ObjectLodType>(1) ? 0u : 1u);
        else
        {
            const int refreshedCount =
                objectRawSlot<GetLodCountFn>(geometry, countSlot)(geometry);
            objectRawSlot<SetGeometryLodFn>(geometry, setSlot)(
                geometry, refreshedCount -
                    (lod == static_cast<ObjectLodType>(1) ? 1 : 2));
        }
#else
        // Non-MSVC: the named geometry_part operations (ELF +0x70 / +0x78).
        const int count = world::geometry_part::getSubgeomCount(geometry);
        if (count <= 1)
            updateFlags(lod == static_cast<ObjectLodType>(1) ? 1u : 0u,
                        lod == static_cast<ObjectLodType>(1) ? 0u : 1u);
        else
        {
            const int refreshedCount = world::geometry_part::getSubgeomCount(geometry);
            world::geometry_part::setSubgeom(geometry, refreshedCount -
                    (lod == static_cast<ObjectLodType>(1) ? 1 : 2));
        }
#endif
    }
    setBoundRadiusIsDirty(this);
}

void Object::updateGeometry()
{
    if (!m_geometry || (m_flags & 0x40000000u) == 0)
        return;

#if defined(_MSC_VER)
    typedef void (BF2_OBJECT_VCALL *SetGeometryTransformationFn)(void*, const Mat4&);
    objectRawSlot<SetGeometryTransformationFn>(m_geometry, 0x50)(
        m_geometry, getVisualAbsoluteTransformation());
    m_flags &= ~0x40000000u;

    const unsigned int nextSlot = 0xCC;
    typedef void* (BF2_OBJECT_VCALL *GetNextGeometryFn)(void*);
    for (void* geometry = objectRawSlot<GetNextGeometryFn>(m_geometry, nextSlot)(m_geometry);
         geometry;
         geometry = objectRawSlot<GetNextGeometryFn>(geometry, nextSlot)(geometry))
    {
        objectRawSlot<SetGeometryTransformationFn>(geometry, 0x50)(
            geometry, getVisualAbsoluteTransformation());
    }
#else
    // Non-MSVC: the named geometry_part operations (both ports +0x50; next ELF +0xD0).
    world::geometry_part::setGeometryTransform(m_geometry, &getVisualAbsoluteTransformation());
    m_flags &= ~0x40000000u;
    for (void* geometry = world::geometry_part::getNextGeometry(m_geometry); geometry;
         geometry = world::geometry_part::getNextGeometry(geometry))
        world::geometry_part::setGeometryTransform(geometry, &getVisualAbsoluteTransformation());
#endif
}

const Vec3& Object::getRelativePosition() const
{
    const Mat4& transform = m_parent ? *m_relativeTransformation : m_absoluteTransformation;
    return *reinterpret_cast<const Vec3*>(&transform.m[12]);
}
void Object::setRelativePosition(const Vec3& position)
{
    if (!m_parent)
        return;
    Mat4 replacement = *m_relativeTransformation;
    replacement.m[12] = position.x;
    replacement.m[13] = position.y;
    replacement.m[14] = position.z;
    if (translationDistanceSquared(*m_relativeTransformation, replacement) > 0.0001f)
        setBoundRadiusIsDirty(this);
    *m_relativeTransformation = replacement;
    transformationWasUpdated(this);
}
const Mat4& Object::getRelativeTransformation() const
{
    return m_parent ? *m_relativeTransformation : m_absoluteTransformation;
}
void Object::setRelativeTransformation(const Mat4& transform)
{
    if (!m_parent)
        return;
    if (translationDistanceSquared(*m_relativeTransformation, transform) > 0.0001f)
        setBoundRadiusIsDirty(this);
    *m_relativeTransformation = transform;
    transformationWasUpdated(this);
}

IObject* Object::getChild() const { return m_child; }
void Object::addChild(IObject* object)
{
    for (IObject* ancestor = this; ancestor; ancestor = ancestor->getParent())
    {
        if (ancestor->getTemplate() == object->getTemplate())
            objectLifecycleInvariantFailure();
    }
    if (isObjectDestroyed())
        objectLifecycleInvariantFailure();

    void* parentPhysicsNode = m_physicsNode;
    void* childPhysicsNode = object_detail::LayoutAccess::physicsNode(object);
    if (parentPhysicsNode && childPhysicsNode)
        static_cast<IPhysicsNode*>(childPhysicsNode)->setParent(
            static_cast<IPhysicsNode*>(parentPhysicsNode));

    responsePhysicsManager->removeObject(object);
    object->setParent(this);
    IObject* firstChild = m_child;
    if (firstChild)
        firstChild->addSibling(object);
    else
    {
        m_flags |= 0x00800000u;
        m_child = object;
    }

    if ((m_flags & 8u) != 0)
        object->updateFlags(0x10u, 0);
    if ((object_detail::LayoutAccess::flags(object) & 0x400u) != 0)
        objectManager->removeObjectFromGrid(object);
    object->updateFlags(0, 0x8000u);
    object->setRelativeTransformation(Mat4::getIdentity());
    setBoundRadiusIsDirty(this);
    setResponseCacheToDirty(this);

    childPhysicsNode = object_detail::LayoutAccess::physicsNode(object);
    if (childPhysicsNode)
        static_cast<IPhysicsNode*>(childPhysicsNode)->reset(true);
    void* childResponsePhysics = object_detail::LayoutAccess::responsePhysics(object);
    if (childResponsePhysics)
        static_cast<IResponsePhysics*>(childResponsePhysics)->reset();
}

void Object::removeChild(IObject* object)
{
    if (!object || !object_detail::LayoutAccess::parent(object))
        return;

    setResponseCacheToDirty(object);
    void* childPhysicsNode = object_detail::LayoutAccess::physicsNode(object);
    if (childPhysicsNode)
        static_cast<IPhysicsNode*>(childPhysicsNode)->setParent(0);

    IObject* firstChild = m_child;
    if (firstChild == object)
    {
        m_child = object_detail::LayoutAccess::sibling(object);
        object->setParent(0);
        if (!m_child)
            m_flags &= ~0x00800000u;
    }
    else if (firstChild)
        firstChild->removeSibling(object);

    responsePhysicsManager->addObject(object);
    childPhysicsNode = object_detail::LayoutAccess::physicsNode(object);
    if (childPhysicsNode)
        static_cast<IPhysicsNode*>(childPhysicsNode)->reset(true);
    void* childResponsePhysics = object_detail::LayoutAccess::responsePhysics(object);
    if (childResponsePhysics)
        static_cast<IResponsePhysics*>(childResponsePhysics)->reset();
}

void Object::addSibling(IObject* object)
{
    if (m_sibling) m_sibling->addSibling(object);
    else m_sibling = object;
}
// PE 0x0071EE00; ELF 0x0830AC10. A missing sibling emits the shipped type-7 diagnostic.
void Object::removeSibling(IObject* object)
{
    if (m_sibling == object)
    {
        m_sibling = object_detail::LayoutAccess::sibling(object);
        object->setParent(0);
    }
    else if (m_sibling)
        m_sibling->removeSibling(object);
    else
    {
#if defined(_MSC_VER)
        // [PLATFORM] PE fetches the template name after constructing the diagnostic.
        const IObjectTemplate* const objectTemplate = object->getTemplate();
        Debug diagnostic(DEBUGTYPE_7, "Scene/Object/Object.cpp", 1136);
        diagnostic << "RemoveChild/Sibling: The child or sibling can not be find:"
                   << objectTemplate->getName();
#else
        const std::string& name = object->getTemplate()->getName();
        Debug(DEBUGTYPE_7, "Scene/Object/Object.cpp", 1136)
            << "RemoveChild/Sibling: The child or sibling can not be find:" << name;
#endif
    }
}
// PE 0x0071E3D0; ELF 0x0830AD70. Detaching republishes through the object's static manager.
void Object::setParent(IObject* parent)
{
    if (m_parent == parent)
        return;
    if (m_parent && parent)
        m_parent->removeChild(this);

    m_parent = parent;
    m_sibling = 0;
    if (parent)
    {
        m_relativeTransformation = new Mat4(Mat4::getIdentity());
        m_flags &= ~0x01000000u;
    }
    else
    {
        if (m_relativeTransformation == &Mat4::getIdentity())
            Debug(DEBUGTYPE_7, "Scene/Object/Object.cpp", 1158) << "Should not free &identyTrans";
        delete m_relativeTransformation;
        m_relativeTransformation = const_cast<Mat4*>(&Mat4::getIdentity());
        m_flags |= 0x01000000u;
    }

    resetCachedRootParents();
    m_interpolationTransformation = 0;
    if (!parent)
        updateFlags(0x8000u, 0x10u);
    transformationWasUpdated(this);
    if (!parent && (m_flags & 0x400u) != 0)
        mObjectManager->addObjectToGrid(this);
    setBoundRadiusIsDirty(this);
}
void Object::resetCachedRootParents()
{
    if (m_sibling) m_sibling->resetCachedRootParents();
    if (m_cachedRootParent)
    {
        m_cachedRootParent = 0;
        if (m_child) m_child->resetCachedRootParents();
    }
}

char Object::getInputId() const { return m_inputId; }
void Object::setInputId(char id) { m_inputId = id; }
char Object::getChildId() const { return m_childId; }
void Object::setChildId(char id) { m_childId = id; }
unsigned char Object::getAlpha() { return 0xFFu; }
unsigned char Object::getLayer() { return m_layer; }
void Object::setLayer(unsigned char value) { m_layer = value; }
unsigned char Object::getGroup() { return m_group; }
void Object::setGroup(unsigned char value) { m_group = value; }
void Object::setIsOvergrowth(bool value) { m_isOvergrowth = value; }
bool Object::getIsOvergrowth() { return m_isOvergrowth; }
char Object::getLayerId() const { return m_layerId; }
void Object::setLayerId(char value) { m_layerId = value; }
bool Object::getNotInAI() const { return m_notInAI; }

// Performs the game-mode lookup before storing notInAI, even when false.
// Preserve virtual destroy(true) dispatch and its platform-specific slot mapping.
void Object::setNotInAI(bool notInAI)
{
    std::string gameMode("gpm_cq");
    if (settings)
        gameMode = settings->stringGet(std::string("GSGameMode"), std::string("gpm_cq"));

    const bool coop = compareIgnoreCase(gameMode, std::string("gpm_coop")) == 0;
    m_notInAI = notInAI;
    if (notInAI && (ai::g_aiInstance || coop))
        destroy(true);
}

void Object::setLightSourceMask(int mask)
{
    if (!m_geometry)
        return;
#if defined(_MSC_VER)
    const unsigned int slot = 0xE0;
    typedef void (BF2_OBJECT_VCALL *SetLightSourceMaskFn)(void*, int);
    objectRawSlot<SetLightSourceMaskFn>(m_geometry, slot)(m_geometry, mask);
#else
    world::geometry_part::setLightSourceMask(m_geometry, mask);                  // ELF +0xE4
#endif
}

int Object::getLightSourceMask() const
{
    if (!m_geometry)
        return 7;
#if defined(_MSC_VER)
    const unsigned int slot = 0xE4;
    typedef int (BF2_OBJECT_VCALL *GetLightSourceMaskFn)(void*);
    return objectRawSlot<GetLightSourceMaskFn>(m_geometry, slot)(m_geometry);
#else
    return world::geometry_part::getLightSourceMask(m_geometry);                 // ELF +0xE8
#endif
}

bool Object::getDisableChildren() { return m_disableChildren; }

void Object::setDisableChildren(bool disable)
{
    m_disableChildren = disable;
    for (IObject* child = getChild(); child; child = child->getSibling())
        child->updateFlags(disable ? 1u : 0u, disable ? 0u : 1u);
}

// Writes this object as .con commands; stream and template must be valid.
// Preserve the trailing space on Object.create, repeated name query and negated/reversed ZXY rotation.
// NaN rotation components still emit. Returns true without checking writes or geometry serialization;
// the queried geometry interface is borrowed and is not released.
bool Object::makeScript(io::IStream* stream)
{
    std::ostringstream script;

    script << "Object.create " << m_template->getName() << " " << std::endl;

    if (!getName().empty())
        script << "Object.name " << getName() << std::endl;

    const Vec3& position = getAbsolutePosition();
    script << "Object.absolutePosition " << position.x << '/' << position.y << '/' << position.z
           << std::endl;

    float zDegrees = 0.0f;
    float xDegrees = 0.0f;
    float yDegrees = 0.0f;
    getRotationZXY(getAbsoluteTransformation(), zDegrees, xDegrees, yDegrees);

    const float rotationFirst = -yDegrees;
    const float rotationSecond = -xDegrees;
    const float rotationThird = -zDegrees;
    if (!(std::fabs(rotationFirst) <= FLT_EPSILON && std::fabs(rotationSecond) <= FLT_EPSILON &&
          std::fabs(rotationThird) <= FLT_EPSILON))
    {
        script << "Object.rotation " << rotationFirst << '/' << rotationSecond << '/'
               << rotationThird << std::endl;
    }

    // One write of the whole text, NUL excluded, count ignored.
    const std::string text = script.str();
    stream->write(text.c_str(), static_cast<unsigned int>(text.length()));

    // Only after the write does the geometry get its turn, and only if it has one.
    if (m_geometry)
    {
        void* saveable =
            reinterpret_cast<IBase*>(m_geometry)->queryInterface(io::IID_IConsoleSaveable);
        if (saveable)
            static_cast<io::IConsoleSaveable*>(saveable)->makeScript(stream);
    }

    return true;
}

void Object::updateFlagsRecursive(unsigned int set, unsigned int clear)
{
    updateDescendantFlags(this, set, clear);
}

// PE 0x0071EB50; ELF 0x0830A1D0. Null is not accepted; flag 0x200 is required before release.
void Object::internalDestroyObject(IObject* object)
{
    if (object_detail::LayoutAccess::parent(object))
        object_detail::LayoutAccess::parent(object)->removeChild(object);
    while (object->getChild()) internalDestroyObject(object->getChild());
    if ((object->getFlags() & 0x200u) == 0) objectLifecycleInvariantFailure();
    object->updateFlags(1u, 0x17E18u);
    object->release();
}

void setBoundRadiusIsDirty(IObject* object)
{
    IObject* last = object;
    for (IObject* current = object; current;
         current = object_detail::LayoutAccess::parent(current))
    {
        object_detail::LayoutAccess::flags(current) |= 0x20140000u;
        last = current;
    }
    if (objectManager && last) objectManager->refreshGridObject(last);
}

void Object::initNetworkableComp() {}
void Object::analyseNetworkableObject() {}

} // namespace world
} // namespace hfe
} // namespace dice
