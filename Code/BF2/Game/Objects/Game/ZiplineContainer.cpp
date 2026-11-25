// Zipline rider control, collision and animation. [INFERRED] Source placement.

#include "ZiplineContainer.h"

#include "Zipline.h"
#include "RotationalBundle.h"
#include "Soldier.h"
#include "../ICameraObject.h"
#include "../IPlayer.h"
#include "../ISoldier.h"
#include "../ObjectLayoutAccess.h"
#include "../ObjectTemplate.h"
#include "../TemplateLeaves.h"
#include "../../../Animation/BoneAnimation/AnimationSystem.h"
#include "../../../Core/ClassManager.h"
#include "../../../Core/Ids.h"
#include "../../../Game/IGameLogic.h"
#include "../../../Math/Mat4.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Physics/ICollisionMesh.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Physics/IResponsePhysics.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectPredicator.h"
#include "../../Utility/Vars.h"

#include <cfloat>
#include <cmath>
#include <string>

namespace dice {
namespace hfe {

extern ClassManager* classManager;                 // ds:0x8AC85A0, Core/Globals.cpp

namespace world {

extern IBase* heightmapCluster;                    // ds:0x8ABAF24, Core/Globals.cpp

// TU static initialization in both ports registers this one Vars value. The ELF stores the
// returned pointer at 0x8AADFAC; the PE stores it at 0x88723C.
extern float* const ziplineSoldierHeight = Vars::getFloat("zipline-soldier-height", 1.49f);

namespace {

template <class T>
T* weakTarget(int* control)
{
    return control
        ? reinterpret_cast<typename WeakPtr<T>::Ctrl*>(control)->target
        : 0;
}

float mappedInput(const io::PlayerInput& input, io::PlayerInputMap index)
{
    const unsigned int bit = static_cast<unsigned int>(index);
    return (input.mappingFlags & (1ULL << bit)) ? input.inputs[bit] : 0.0f;
}

float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float lengthSq(const Vec3& value)
{
    return dot(value, value);
}

float length(const Vec3& value)
{
    return std::sqrt(lengthSq(value));
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs)
{
    Vec3 out = { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    return out;
}

void normalizeLikeEngine(Vec3& value)
{
    const float squared = lengthSq(value);
    if (std::fabs(squared - 1.0f) >= FLT_EPSILON)
    {
        if (std::fabs(squared) >= FLT_EPSILON)
        {
            const float inverse = 1.0f / std::sqrt(squared);
            value.x *= inverse;
            value.y *= inverse;
            value.z *= inverse;
        }
        else
        {
            value.x = value.y = value.z = 0.0f;
        }
    }
}

Vec3 getObjectRotation(IObject* object)
{
    const bool hasParent = object_detail::LayoutAccess::parent(object) != 0;
    const Mat4& transform = hasParent
        ? object->getRelativeTransformation()
        : object->getAbsoluteTransformation();
    float z, x, y;
    getRotationZXY(transform, z, x, y);
    Vec3 out = { -y, -x, -z };
    return out;
}

void setObjectRotation(IObject* object, const Vec3& rotation)
{
    const bool hasParent = object_detail::LayoutAccess::parent(object) != 0;
    Mat4 transform = hasParent
        ? object->getRelativeTransformation()
        : object->getAbsoluteTransformation();
    setRotationZXY(transform, -rotation.z, -rotation.y, -rotation.x);
    if (hasParent)
        object->setRelativeTransformation(transform);
    else
        object->setAbsoluteTransformation(transform);
}

// IDebugRenderer and IDebugCylinderMesh remain unrecovered interfaces. Isolate the three retail
// dispatches here. The renderer's draw/get slots are +0x14/+0x18 in both ports; the cylinder's
// setColour slot is +0xF8 in PE and +0xFC in ELF because of the destructor-slot ABI difference.
#if defined(_MSC_VER)
#  define BF2_ZIPLINE_VCALL __thiscall
#else
#  define BF2_ZIPLINE_VCALL
#endif

template <class Fn>
Fn ziplineVSlot(void* object, unsigned int byteOffset)
{
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[byteOffset / 4]);
}

typedef void (BF2_ZIPLINE_VCALL* DebugDrawObjectFn)(void*, unsigned int,
                                                     const Mat4&, const std::string&);
typedef IBase* (BF2_ZIPLINE_VCALL* DebugGetObjectFn)(void*, unsigned int);
typedef void (BF2_ZIPLINE_VCALL* DebugSetColourFn)(void*, const Vec4&);

#undef BF2_ZIPLINE_VCALL

} // anonymous namespace

ZiplineContainer::ZiplineContainer(const ZiplineContainerTemplate* objectTemplate)
    : PlayerControlObject(objectTemplate)
    , m_refCount2(1)
    , m_zipline(0)
    , m_action(0.0f)
    , m_use(-1.0f)
    , m_throttle(0.0f)
    , _dword3D0(0)
    , m_positionRefreshTimer(0.0f)
    , m_motion()
    , _vec3E4()
    , _vec3F0()
    , m_exitRotation()
    , m_exitPosition()
    , m_traversalStart()
    , m_traversalEnd()
    , m_lastSoldierPosition()
    , m_accumulatedSlopeMotion()
    , m_slopeSpeed(0.0f)
    , m_force(0.0f)
    , _byte44C(false)
{
    m_motion.x = m_motion.y = m_motion.z = 0.0f;
    _vec3E4.x = _vec3E4.y = _vec3E4.z = 0.0f;
    _vec3F0.x = _vec3F0.y = _vec3F0.z = 0.0f;
    m_exitRotation.x = m_exitRotation.y = m_exitRotation.z = 0.0f;
    m_exitPosition.x = m_exitPosition.y = m_exitPosition.z = 0.0f;
    m_traversalStart.x = m_traversalStart.y = m_traversalStart.z = 0.0f;
    m_traversalEnd.x = m_traversalEnd.y = m_traversalEnd.z = 0.0f;
    m_lastSoldierPosition.x = m_lastSoldierPosition.y = m_lastSoldierPosition.z = 0.0f;
    m_accumulatedSlopeMotion.x = m_accumulatedSlopeMotion.y = m_accumulatedSlopeMotion.z = 0.0f;
}

ZiplineContainer::~ZiplineContainer()
{
    if (!getParent())
        gameLogic->removePco(this);
}

void ZiplineContainer::addRef() { ++m_refCount2; }
int ZiplineContainer::getRef() { return m_refCount2; }
int ZiplineContainer::release()
{
    const int left = --m_refCount2;
    if (left == 0)
        delete this;
    return left;
}

void* ZiplineContainer::queryInterface(unsigned int iid) const
{
    return PlayerControlObject::queryInterface(iid);
}

bool ZiplineContainer::enter(IPlayer*)
{
    return false;
}

void ZiplineContainer::exit()
{
    IObject* occupant = weakTarget<IObject>(m_occupyingObject);
    if (occupant && m_zipline)
        m_zipline->detachSoldier(static_cast<ISoldier*>(static_cast<Soldier*>(occupant)));
}

bool ZiplineContainer::_enter(IPlayer* player)
{
    IObject* vehicle = player->getVehicle();
    IPlayerControlObject* vehiclePco = static_cast<IPlayerControlObject*>(
        vehicle->queryInterface(IID_IPlayerControlObject));

    m_lastSoldierPosition = vehicle->getAbsolutePosition();
    const Vec3& ziplinePosition = m_zipline->getAbsolutePosition();
    m_zipline->getZiplineHeight();                         // shipped dead call

    ICameraObject* activeCamera = vehiclePco->getActiveCamera();
    IObject* activeCameraObject = static_cast<IObject*>(
        activeCamera->queryInterface(IID_IObject));
    Vec3 savedRotation = { 0.0f, 0.0f, 0.0f };
    if (activeCameraObject)
        savedRotation = getObjectRotation(activeCameraObject);
    const CameraViewMode savedViewMode = activeCamera->getViewMode();

    vehiclePco->onEnter(this);
    const bool entered = PlayerControlObject::enter(player);

    const Vec3 vehicleLocalPosition = { 0.0f, 1.1f, 0.0f };
    vehicle->setRelativePosition(vehicleLocalPosition);
    const Vec3 position = { ziplinePosition.x, ziplinePosition.y + 1.0f, ziplinePosition.z };
    setAbsolutePosition(position);

    IObject* occupant = weakTarget<IObject>(m_occupyingObject);
    if (occupant)
    {
        occupant->updateFlags(0, 1);
        updateFlags(0x800, 0);

        IPhysicsNode* physicsNode = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(occupant));
        physicsNode->reset(true);

        IResponsePhysics* response = static_cast<IResponsePhysics*>(
            object_detail::LayoutAccess::responsePhysics(occupant));
        response->reset();
        const Vec3 stopped = { 0.0f, 0.0f, 0.0f };
        response->setSurfacePositionalSpeed(stopped);
    }

    // Retail repeats getCamera rather than caching it and restores pitch only.
    if (getCamera())
    {
        IObject* cameraObject = getCamera();
        ICameraObject* camera = static_cast<ICameraObject*>(
            cameraObject->queryInterface(IID_ICameraObject));
        camera->setViewMode(savedViewMode, 0, false);

        Vec3 rotation = { 0.0f, savedRotation.y, 0.0f };
        setObjectRotation(getCamera(), rotation);
        static_cast<RotationalBundle*>(getCamera())->setCurrentRotation(rotation);
    }

    return entered;
}

void ZiplineContainer::_exit()
{
    IObject* occupant = weakTarget<IObject>(m_occupyingObject);
    IPlayerControlObject* occupantPco = static_cast<IPlayerControlObject*>(
        occupant->queryInterface(IID_IPlayerControlObject));

    PlayerControlObject::exit();
    occupantPco->onExit();
    _byte44C = false;
    _dword3D0 = 0;
    m_motion.x = m_motion.y = m_motion.z = 0.0f;
    m_accumulatedSlopeMotion.x = m_accumulatedSlopeMotion.y = m_accumulatedSlopeMotion.z = 0.0f;

    IPhysicsNode* physicsNode = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(occupant));
    physicsNode->reset(true);
    IResponsePhysics* response = static_cast<IResponsePhysics*>(
        object_detail::LayoutAccess::responsePhysics(occupant));
    response->reset();
}

void ZiplineContainer::handlePlayerInput(IPlayer* player, const io::PlayerInput& input,
                                         float deltaTime, unsigned int flags)
{
    IObject* occupant = weakTarget<IObject>(m_occupyingObject);
    if (occupant)
        occupant->handlePlayerInput(player, input, deltaTime, 0);

    for (std::list<IObject*>::iterator it = m_playerInputObjects.begin();
         it != m_playerInputObjects.end(); ++it)
        (*it)->handlePlayerInput(player, input, deltaTime, flags);

    m_action = mappedInput(input, io::PIAction);

    if (m_use != -1.0f)
    {
        m_use = mappedInput(input, io::PIUse);
    }
    else if (mappedInput(input, io::PIUse) == 0.0f)
    {
        // The -1 sentinel suppresses a use button already held when the soldier grabs the line.
        m_use = 0.0f;
    }

    const float throttle = mappedInput(input, io::PIThrottle);
    float clampedThrottle = -1.0f;
    if (throttle >= -1.0f)
    {
        clampedThrottle = throttle;
        if (throttle > 1.0f)
            clampedThrottle = 1.0f;
    }
    m_throttle = clampedThrottle;
}

void ZiplineContainer::handleUpdate(float deltaTime, unsigned int flags)
{
    if (!m_zipline)
        return;

    PlayerControlObject::handleUpdate(deltaTime, flags);
    IObject* occupant = weakTarget<IObject>(m_occupyingObject);

    // Shipped code performs this query and discards its result, without guarding the camera.
    getActiveCamera()->queryInterface(IID_IObject);

    m_positionRefreshTimer += deltaTime;
    if (occupant && m_positionRefreshTimer > 1.0f)
    {
        m_positionRefreshTimer -= 1.0f;             // exactly once, not a catch-up loop

        // Shipped defect, common to ELF and PE: both endpoints are read from Zipline::m_startPoint.
        // Keep the full zero-length projection instead of simplifying it to m_startPoint.
        const Vec3 segmentStart = m_zipline->m_startPoint;
        const Vec3 segmentEnd = m_zipline->m_startPoint;
        Vec3 segment = subtract(segmentEnd, segmentStart);
        const float segmentLength = length(segment);
        normalizeLikeEngine(segment);
        const Vec3 fromStart = subtract(occupant->getAbsolutePosition(), segmentStart);
        float along = dot(segment, fromStart);
        if (along > segmentLength)
            along = segmentLength;                 // no lower clamp in retail
        Vec3 candidate = {
            segmentStart.x + segment.x * along,
            segmentStart.y + segment.y * along,
            segmentStart.z + segment.z * along
        };
        m_lastSoldierPosition = m_zipline->getNewSoldierPosition(
            candidate, m_lastSoldierPosition);
    }

    Vec3 direction = subtract(m_traversalEnd, m_traversalStart);
    normalizeLikeEngine(direction);
    const Vec3 up = { 0.0f, 0.98000002f, 0.0f };
    const float slope = std::fabs(dot(up, direction));
    const Vec3 acceleration = {
        direction.x * slope, direction.y * slope, direction.z * slope
    };
    m_accumulatedSlopeMotion.x += acceleration.x * deltaTime;
    m_accumulatedSlopeMotion.y += acceleration.y * deltaTime;
    m_accumulatedSlopeMotion.z += acceleration.z * deltaTime;
    m_slopeSpeed = length(m_accumulatedSlopeMotion);

    if (gameLogic->isServer() && occupant)
    {
        const float distanceFromStart = length(
            subtract(m_traversalStart, occupant->getAbsolutePosition()));
        const float collisionStartRadius = m_zipline->getCollisionStartRadius();
        ISoldier* soldier = static_cast<ISoldier*>(static_cast<Soldier*>(occupant));

        // Both tests run even when the terrain test detached the soldier; retail does not
        // short-circuit these calls.
        const bool terrainCollision = CheckTerrainCollision(soldier);
        const bool objectCollision = CheckObjectCollision(soldier);
        if (!terrainCollision && !objectCollision)
        {
            updateZiplineAnimation(deltaTime);
            if (m_use > 0.0f && distanceFromStart > collisionStartRadius)
            {
                m_zipline->detachSoldier(soldier);
                static_cast<Soldier*>(occupant)->m_byte399 = true;
            }
        }
    }
}

bool ZiplineContainer::CheckTerrainCollision(ISoldier* soldier)
{
    ICollisionMesh* mesh = static_cast<ICollisionMesh*>(
        object_detail::LayoutAccess::collisionMesh(this));
    if (mesh && mesh->hasLod(0))
        mesh->getBoundingBox(0);                   // shipped dead prelude

    const Vec3 from = getAbsolutePosition();
    const Mat4& transform = getAbsoluteTransformation();
    const Vec3 down = { 0.0f, -*ziplineSoldierHeight, 0.0f };
    Vec3 thirdRow = { transform.m[8], transform.m[9], transform.m[10] };
    normalizeLikeEngine(thirdRow);
    const Vec3 to = {
        from.x + down.x - thirdRow.x,
        from.y + down.y - thirdRow.y,
        from.z + down.z - thirdRow.z
    };

    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    float hitFraction;
    unsigned char material;
    if (static_cast<IHeightmapCluster*>(heightmapCluster)->intersectRayInWorldCoords(
            from, to, hitPoint, hitFraction, hitNormal, material, true))
    {
        m_zipline->detachSoldier(soldier);
        return true;
    }
    return false;
}

bool ZiplineContainer::CheckObjectCollision(ISoldier* soldier)
{
    ICollisionMesh* mesh = static_cast<ICollisionMesh*>(
        object_detail::LayoutAccess::collisionMesh(this));
    if (mesh && mesh->hasLod(0))
        mesh->getBoundingBox(0);                   // shipped dead prelude

    const Vec3 position = getAbsolutePosition();
    const Mat4& transform = getAbsoluteTransformation();
    const Vec3 down = { 0.0f, -*ziplineSoldierHeight, 0.0f };
    Vec3 thirdRow = { transform.m[8], transform.m[9], transform.m[10] };
    normalizeLikeEngine(thirdRow);

    ObjectFlagPredicator predicator(0x100);
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart;

    const Vec3 sidePositive = {
        transform.m[0] * 0.40000001f,
        transform.m[1] * 0.40000001f,
        transform.m[2] * 0.40000001f
    };
    const Vec3 sideNegative = {
        -transform.m[0] * 0.40000001f,
        -transform.m[1] * 0.40000001f,
        -transform.m[2] * 0.40000001f
    };
    if (objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                     position, sidePositive, &predicator,
                                     true, false, false, true, 0)
        || objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                        position, sideNegative, &predicator,
                                        true, false, false, true, 0))
    {
        m_zipline->detachSoldier(soldier);
        return true;
    }

    const Vec3 lowerPosition = {
        position.x + down.x, position.y + down.y, position.z + down.z
    };
    const Vec3 rawThirdRow = { transform.m[8], transform.m[9], transform.m[10] };
    if (!objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                      lowerPosition, rawThirdRow, &predicator,
                                      true, false, false, true, 0))
    {
        const Vec3 fallbackFrom = {
            position.x - thirdRow.x * 0.1f,
            position.y - thirdRow.y * 0.1f,
            position.z - thirdRow.z * 0.1f
        };
        const Vec3 fallbackDirection = {
            fallbackFrom.x - position.x + down.x,
            fallbackFrom.y - position.y + down.y,
            fallbackFrom.z - position.z + down.z
        };
        // Retail ignores this bool and decides from the output object pointer.
        objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                     fallbackFrom, fallbackDirection, &predicator,
                                     true, false, false, true, 0);
    }

    if (!hitObject)
        return false;

    const unsigned int classId = hitObject->getTemplate()->getClassID();
    if (classId == CID_ZiplineRopeTemplate
        || classId == CID_ZiplineTemplate
        || classId == CID_ZiplineContainerTemplate)
        return false;

    if (classId == CID_SoldierTemplate
        && getTeam() == static_cast<Soldier*>(hitObject)->getTeam())
        return false;

    Soldier* concreteSoldier = static_cast<Soldier*>(soldier);
    const float distanceFromStart = length(
        subtract(m_traversalStart, concreteSoldier->getAbsolutePosition()));
    const float collisionStartRadius = m_zipline->getCollisionStartRadius();
    const float entryRadius = m_zipline->getEntryRadius();
    const Vec3 traversal = subtract(m_traversalEnd, m_traversalStart);
    if (distanceFromStart > collisionStartRadius
        || lengthSq(traversal) < entryRadius * entryRadius)
    {
        m_zipline->detachSoldier(soldier);
        return true;
    }
    return false;
}

void ZiplineContainer::updateZiplineAnimation(float deltaTime)
{
    if (m_zipline)
    {
        m_zipline->getAbsolutePosition();              // shipped dead calls
        m_zipline->getAbsoluteTransformation();

        Vec3 direction = subtract(m_traversalEnd, m_traversalStart);
        normalizeLikeEngine(direction);

        if (length(m_motion) == 0.0f)
        {
            Vec3 start = m_traversalStart;
            start.y -= 0.07f;
            setAbsolutePosition(start);
        }

        const Vec3 up = { 0.0f, 0.98000002f, 0.0f };
        const float slope = std::fabs(dot(up, direction));
        m_motion.x += direction.x * slope * deltaTime;
        m_motion.y += direction.y * slope * deltaTime;
        m_motion.z += direction.z * slope * deltaTime;

        const Vec3 current = getAbsolutePosition();
        IObject* occupant = weakTarget<IObject>(m_occupyingObject);
        const float distanceToEnd = length(
            subtract(m_traversalEnd, getAbsolutePosition()));
        if (!(distanceToEnd >= 1.0f))
        {
            m_zipline->detachSoldier(occupant
                ? static_cast<ISoldier*>(static_cast<Soldier*>(occupant)) : 0);
            return;                                    // skips the animation triggers in retail
        }

        ObjectFlagPredicator predicator(0x100);
        IObject* hitObject = 0;
        Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
        Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
        int hitPart;
        objectManager->setOverrideLineTestLod(2);
        if (objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                         current, m_motion, &predicator,
                                         true, false, false, true, 0)
            && hitPart == 666 && gameLogic->isServer())
        {
            const Vec3 zero = { 0.0f, 0.0f, 0.0f };
            const Vec3 upright = { 0.0f, 1.0f, 0.0f };
            gameLogic->giveDamage(occupant, 1.0e10f, 0, 0, 0, -1, 0, 0,
                                  zero, upright, -1, false, true, false);
        }
        objectManager->setOverrideLineTestLod(-1);

        Mat4 next = getAbsoluteTransformation();
        next.m[12] = current.x + m_motion.x;
        next.m[13] = current.y + m_motion.y;
        next.m[14] = current.z + m_motion.z;
        setAbsoluteTransformation(next);
    }

    if (m_activeAnimationSystem)
    {
        m_activeAnimationSystem->clearTriggerMessage(2);
        m_activeAnimationSystem->setTriggerMessage(1);
    }
}

void ZiplineContainer::drawDebugline(Vec3& start, Vec3& end, float width, Vec4 colour)
{
    IBase* renderer = classManager->getSingleton(std::string("DebugRenderer"));

    const Vec3 direction = subtract(end, start);
    Mat4 scale = Mat4::getIdentity();
    scale.m[0] = width;
    scale.m[5] = width;
    scale.m[10] = length(direction);

    Mat4 orientation = Mat4::getIdentity();
    orientation.m[8] = direction.x;
    orientation.m[9] = direction.y;
    orientation.m[10] = direction.z;
    Vec3& up = *reinterpret_cast<Vec3*>(&orientation.m[0]);
    Vec3& right = *reinterpret_cast<Vec3*>(&orientation.m[4]);
    Vec3& forward = *reinterpret_cast<Vec3*>(&orientation.m[8]);
    right.makeOrthonormalBasis(forward, up);          // return deliberately ignored
    orientation.m[12] = start.x + direction.x * 0.5f;
    orientation.m[13] = start.y + direction.y * 0.5f;
    orientation.m[14] = start.z + direction.z * 0.5f;

    Mat4 transform;
    transform.mult(scale, orientation);
    ziplineVSlot<DebugDrawObjectFn>(renderer, 0x14)(
        renderer, geom::CID_DebugCylinderMeshTemplate, transform, std::string());
    IBase* cylinder = ziplineVSlot<DebugGetObjectFn>(renderer, 0x18)(
        renderer, geom::CID_DebugCylinderMeshTemplate);
    void* debugCylinder = cylinder->queryInterface(geom::IID_IDebugCylinderMesh);
    if (debugCylinder)
    {
#if defined(_MSC_VER)
        const unsigned int setColourOffset = 0xF8;
#else
        const unsigned int setColourOffset = 0xFC;
#endif
        ziplineVSlot<DebugSetColourFn>(debugCylinder, setColourOffset)(debugCylinder, colour);
    }
}

void ZiplineContainer::handleMessage(TemplateMessage, IPlayer*, bool) {}

bool ZiplineContainer::handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&,
                                       int, int, CollisionFaceInfo*, CollisionFaceInfo*)
{
    return false;
}

io::INetworkable* ZiplineContainer::getNetworkable() const
{
    if (!weakTarget<IObject>(m_occupyingObject))
        return 0;
    return weakTarget<IObject>(m_occupyingObject)->getNetworkable();
}

bool ZiplineContainer::getExitPosition(Vec3& out)
{
    out = _getExitPosition();
    return true;
}

bool ZiplineContainer::applyCameraAnimation(const Mat4&, Mat4&)
{
    return false;
}

Vec3* ZiplineContainer::getExitRotation()
{
    return &m_exitRotation;
}

Vec3 ZiplineContainer::_getExitPosition()
{
    return m_exitPosition;
}

IObject* ZiplineContainer::getCamera()
{
    for (IObject* object = getChild(); object; object = object->getSibling())
    {
        if (object->queryInterface(IID_ICameraObject))
            return object;
    }
    return 0;
}

bool ZiplineContainer::isAllowedToExit()
{
    return false;
}

void ZiplineContainer::addForce(float force)
{
    float total = m_force + force;
    if (total >= -30.0f)
    {
        if (total > 30.0f)
            total = 30.0f;
    }
    else
    {
        total = -30.0f;
    }
    m_force = total;
}

void ZiplineContainer::setExitPosition(Vec3 position)
{
    m_exitPosition = position;
}

bool ZiplineContainer::allowEnterOtherVehicle()
{
    return false;
}

} // namespace world
} // namespace hfe
} // namespace dice
