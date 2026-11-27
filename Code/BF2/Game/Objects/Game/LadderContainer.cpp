// Ladder-container occupancy and animation.

#include "LadderContainer.h"

#include "../../../Core/Ids.h"
#include "../../../Animation/BoneAnimation/AnimationSystem.h"
#include "../../../Game/IGameLogic.h"
#include "../../../Math/Mat4.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Physics/IResponsePhysics.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../ICameraObject.h"
#include "../IPlayer.h"
#include "../ISoldier.h"
#include "../ObjectLayoutAccess.h"
#include "../TemplateLeaves.h"                    // LadderContainerTemplate lives there
#include "RotationalBundle.h"
#include "Soldier.h"
#if !DEDICATED
#include "../../Main/Game.h"
#endif

#include <cmath>

namespace dice {
namespace hfe {
namespace world {

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

void normalizeLikeEngine(Vec3& v)
{
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    const float epsilon = 1.1920929e-7f;
    if (std::fabs(lenSq - 1.0f) >= epsilon)
    {
        if (std::fabs(lenSq) >= epsilon)
        {
            const float inv = 1.0f / std::sqrt(lenSq);
            v.x *= inv; v.y *= inv; v.z *= inv;
        }
        else
        {
            v.x = v.y = v.z = 0.0f;
        }
    }
}

} // anonymous namespace

// The constructor - 0x818AB50 (C2), 225 bytes, read in full. PCO ctor, the four vptr writes,
// then the class's own sheet: refcount 1, everything else zero, Object::updateFlags(0x1008, 0).
LadderContainer::LadderContainer(const LadderContainerTemplate* objectTemplate)
    : PlayerControlObject(objectTemplate)
{
    m_refCount2 = 1;                             // +0x3BC
    m_ladder = 0;                                // +0x3C0
    m_action = 0.0f;
    m_throttle = 0.0f;
    _dword3CC = _dword3D0 = _dword3D4 = 0;
    m_exitType = LadderExitNone;
    _dword3DC = _dword3E0 = _dword3E4 = 0;
    m_exitRotation.x = m_exitRotation.y = m_exitRotation.z = 0.0f;
    m_force = 0.0f;                              // +0x3F4
    m_skidding = m_climbingUp = m_climbingDown = m_justExited = false;
    Object::updateFlags(0x1008, 0);
}

// vptr restores + the base chain.
LadderContainer::~LadderContainer() {}

void LadderContainer::addRef()  { ++m_refCount2; }
int  LadderContainer::getRef()  { return m_refCount2; }
int  LadderContainer::release()
{
    const int r = --m_refCount2;
    if (r == 0) delete this;
    return r;
}

// byte-identical dispatch to the base's table, re-emitted by GCC; forwarding reproduces it.
void* LadderContainer::queryInterface(unsigned int iid) const
{
    return PlayerControlObject::queryInterface(iid);
}

// a ladder ignores collisions outright.
bool LadderContainer::handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&,
                                      int, int, CollisionFaceInfo*, CollisionFaceInfo*)
{
    return false;
}

// the ladder replicates through its occupant: the occupying soldier's networkable, or 0.
// The read walks m_occupyingObject's +0x04 target and uses IObject::getNetworkable.
io::INetworkable* LadderContainer::getNetworkable() const
{
    IObject* occupant = weakTarget<IObject>(m_occupyingObject);
    return occupant ? occupant->getNetworkable() : 0;
}

// calls the supplied player's free-camera predicate and discards the answer.
void LadderContainer::handleMessage(TemplateMessage, IPlayer* player, bool)
{
    if (player)
        player->getFreeCameraEnabled();
}

// empty, as shipped: a ladder never builds a networkable of its own.
void LadderContainer::initNetworkableComp() {}

// . The ladder mount is deliberately a second entry point: the ordinary
// enter() virtual above refuses the request, while Ladder::attachSoldier calls this slot directly.
bool LadderContainer::_enter(IPlayer* player)
{
    IObject* vehicle = player->getVehicle();
    IPlayerControlObject* vehiclePco = static_cast<IPlayerControlObject*>(
        vehicle->queryInterface(IID_IPlayerControlObject));

    const Vec3& vehiclePosition = vehicle->getAbsolutePosition();
    const Vec3& ladderPosition = m_ladder->getAbsolutePosition();
    m_ladder->getLadderHeight();                         // shipped dead call

    const float lower = m_ladder->getAbsoluteBottom() + *ladder_detail::ladderBottomMarginal;
    const float dropped = vehiclePosition.y - 1.0f;
    const float top = m_ladder->getAbsoluteTop();
    const float upper = dropped > top ? top : dropped;
    const float snappedY = upper > lower ? upper : lower;
    const float relativeY = snappedY - ladderPosition.y;

    ICameraObject* activeCamera = vehiclePco->getActiveCamera();
    IObject* activeCameraObject = static_cast<IObject*>(
        activeCamera->queryInterface(IID_IObject));
    Vec3 savedRotation = { 0.0f, 0.0f, 0.0f };
    if (activeCameraObject)
        savedRotation = getObjectRotation(activeCameraObject);
    const CameraViewMode savedViewMode = activeCamera->getViewMode();

    vehiclePco->onEnter(this);
    const bool entered = PlayerControlObject::enter(player);

    Vec3 position = { ladderPosition.x, ladderPosition.y + relativeY, ladderPosition.z };
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
        const Vec3 stop = { 0.0f, 0.0f, 0.0f };
        response->setSurfacePositionalSpeed(stop);
    }

    // Retail repeats getCamera rather than caching it. Keep that observable walk and the exact
    // pitch-only restoration: yaw and roll become zero on the ladder camera.
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

// empty gate, as shipped. Ladder::attachSoldier calls the separate _enter slot directly.
bool LadderContainer::enter(IPlayer*) { return false; }

// detach the occupying soldier from the ladder, if both exist.
void LadderContainer::exit()
{
    IObject* occupant = weakTarget<IObject>(m_occupyingObject);
    if (occupant && m_ladder)
        m_ladder->detachSoldier(static_cast<ISoldier*>(static_cast<Soldier*>(occupant)));
}

// . This assumes a live occupant and both physics components, just as the
// shipped body does; callers only reach it while the soldier is attached.
void LadderContainer::_exit()
{
    IObject* occupant = weakTarget<IObject>(m_occupyingObject);
    IPlayerControlObject* occupantPco = static_cast<IPlayerControlObject*>(
        occupant->queryInterface(IID_IPlayerControlObject));

    PlayerControlObject::exit();
    occupantPco->onExit();
    m_skidding = false;

    IPhysicsNode* physicsNode = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(occupant));
    physicsNode->reset(true);
    IResponsePhysics* response = static_cast<IResponsePhysics*>(
        object_detail::LayoutAccess::responsePhysics(occupant));
    response->reset();
    m_justExited = true;
}

bool LadderContainer::isAllowedToExit() { return false; }            // , as shipped

// the occupying soldier's team (vcall +0xE8 on the soldier), else 0.
int LadderContainer::getTeam()
{
    IPlayer* occupant = weakTarget<IPlayer>(m_occupyingPlayer);
    return occupant ? occupant->getTeam() : 0;
}

Vec3* LadderContainer::getExitRotation()
{
    return &m_exitRotation;
}

// . The interface contract always succeeds, even with a null ladder.
bool LadderContainer::getExitPosition(Vec3& out)
{
    out = _getExitPosition(m_ladder, getAbsolutePosition(), m_exitType);
    return true;
}

// . Exit type values and the unusual unconditional +1.01 Y lift (for every
// non-null ladder, including an unknown type) are common to both shipped ports.
Vec3 LadderContainer::_getExitPosition(Ladder* ladder, const Vec3& input, LadderExitType type)
{
    if (!ladder)
        return input;

    Vec3 out = { 0.0f, 0.0f, 0.0f };
    const Vec3& ladderPosition = ladder->getAbsolutePosition();
    const Mat4& transform = ladder->getAbsoluteTransformation();
    Vec3 forward = { transform.m[8], transform.m[9], transform.m[10] };
    normalizeLikeEngine(forward);

    switch (type)
    {
    case LadderExitNone:
        out = input;
        break;
    case LadderExitTop:
        out.x = ladderPosition.x + forward.x * *ladder_detail::ladderTopExitOffset;
        out.y = ladder->getAbsoluteTop() + forward.y * *ladder_detail::ladderTopExitOffset;
        out.z = ladderPosition.z + forward.z * *ladder_detail::ladderTopExitOffset;
        break;
    case LadderExitBottom:
        out.x = ladderPosition.x - forward.x * *ladder_detail::ladderExitOffset;
        out.y = ladder->getAbsoluteBottom() - forward.y * *ladder_detail::ladderExitOffset;
        out.z = ladderPosition.z - forward.z * *ladder_detail::ladderExitOffset;
        break;
    case LadderExitSide:
        out.x = input.x - forward.x * *ladder_detail::ladderExitOffset;
        out.y = input.y - forward.y * *ladder_detail::ladderExitOffset;
        out.z = input.z - forward.z * *ladder_detail::ladderExitOffset;
        break;
    }

    out.y += 1.01f;
    return out;
}

// first child (or self) answering ICameraObject, walking the sibling chain.
IObject* LadderContainer::getCamera()
{
    IObject* walk = getChild();
    while (walk)
    {
        if (walk->queryInterface(world::IID_ICameraObject))
            return walk;
        walk = object_detail::LayoutAccess::sibling(walk);
    }
    return 0;
}

// . The soldier receives a forced flags=0 update first, then every child
// input object receives the caller's flags. Sprint while descending latches the four-times skid.
void LadderContainer::handlePlayerInput(IPlayer* player, const io::PlayerInput& input,
                                        float deltaTime, unsigned int flags)
{
    IObject* occupant = weakTarget<IObject>(m_occupyingObject);
    if (occupant)
        occupant->handlePlayerInput(player, input, deltaTime, 0);

    for (std::list<IObject*>::iterator it = m_playerInputObjects.begin();
         it != m_playerInputObjects.end(); ++it)
        (*it)->handlePlayerInput(player, input, deltaTime, flags);

    m_action = mappedInput(input, io::PIAction);

    const bool sprint = mappedInput(input, io::PISprint) > 0.5f;
    const bool alternateSprint = mappedInput(input, io::PIAltSprint) != 0.0f;
    if ((sprint || alternateSprint) && !isGettingOff() && m_climbingDown)
        m_skidding = true;

    float throttle = mappedInput(input, io::PIThrottle);
    if (throttle < -1.0f)
        throttle = -1.0f;
    else if (throttle > 1.0f)
        throttle = 1.0f;
    m_throttle = m_skidding ? *ladder_detail::ladderSkidSpeedMod : throttle;
}

// . The movement axis is the transform's second row (m[4..6]); the camera
// animation below deliberately uses the third. The limit-detach path continues into the collision
// test after destroy(true), matching retail's queued-destruction lifetime.
void LadderContainer::handleUpdate(float deltaTime, unsigned int flags)
{
    if (!m_ladder)
        return;

    PlayerControlObject::handleUpdate(deltaTime, flags);
#if !DEDICATED
    // BF2.exe 0x5C3514..0x5C3525: client animation belongs to the visual phase.
    if (isDedicated())
#endif
    updateClimbAnimation(deltaTime);

    IObject* occupant = weakTarget<IObject>(m_occupyingObject);
    m_justExited = false;

    ICameraObject* activeCamera = getActiveCamera();
    IObject* cameraObject = static_cast<IObject*>(activeCamera->queryInterface(IID_IObject));

    IPlayer* occupantPlayer = weakTarget<IPlayer>(m_occupyingPlayer);
    if (!gameLogic->isServer() && occupantPlayer != playerManager->getLocalHumanPlayer())
        return;

    if (m_action != 0.0f)
    {
        m_exitType = LadderExitSide;
        if (canSoldierExit())
        {
            m_ladder->detachSoldier(occupant
                ? static_cast<ISoldier*>(static_cast<Soldier*>(occupant)) : 0);
            return;
        }
    }

    const Vec3 cameraRotation = getObjectRotation(cameraObject);
    const float climbDirection = cameraRotation.y >= *ladder_detail::ladderDownAngleLimit
        ? -1.0f : 1.0f;
    const float directedThrottle = climbDirection * m_throttle;
    m_climbingUp = directedThrottle > 0.0f;
    m_climbingDown = directedThrottle < 0.0f;

    m_ladder->getAbsolutePosition();                  // two shipped dead getter calls
    getAbsolutePosition();

    const float speedModifier = isGettingOff()
        ? *ladder_detail::ladderGetoffSpeedMod : 1.0f;
    Mat4 next = getAbsoluteTransformation();
    const float motionDirection = m_skidding ? -1.0f : climbDirection;

    if (m_throttle != 0.0f)
    {
        const float distance = m_throttle * motionDirection
            * *ladder_detail::ladderClimbSpeedMod * deltaTime * 30.0f * speedModifier;
        next.m[12] += next.m[4] * distance;
        next.m[13] += next.m[5] * distance;
        next.m[14] += next.m[6] * distance;
    }

    if (m_force != 0.0f)
    {
        const float forceScale = m_force * 0.00050000002f;
        next.m[12] += next.m[4] * forceScale;
        next.m[13] += next.m[5] * forceScale;
        next.m[14] += next.m[6] * forceScale;

        if (m_force > 0.0f)
        {
            m_force -= 5.0f;
            if (m_force < 1.0f)
                m_force = 0.0f;
        }
        else
        {
            m_force += 5.0f;
            if (m_force > -1.0f)
                m_force = 0.0f;
        }
    }

    const LadderExitType limit = m_ladder->checkLimits(
        next, m_climbingUp, m_climbingDown || m_skidding);
    if (limit != LadderExitNone)
    {
        m_exitType = limit;
        if (!canSoldierExit())
            return;
        m_ladder->detachSoldier(occupant
            ? static_cast<ISoldier*>(static_cast<Soldier*>(occupant)) : 0);
    }

    const int collision = m_ladder->checkCollision(next, this);
    if (collision == 0)
    {
        setAbsoluteTransformation(next);
        return;
    }

    if (collision == 1)
    {
        m_exitType = LadderExitSide;
        if (canSoldierExit())
            m_ladder->detachSoldier(occupant
                ? static_cast<ISoldier*>(static_cast<Soldier*>(occupant)) : 0);
    }
}

// . A detached container performs neither animation nor base visual work.
void LadderContainer::handleVisualUpdate(float deltaTime, float interpolation)
{
    if (!m_ladder)
        return;
#if !DEDICATED
    // BF2.exe 0x5C3920..0x5C3937: keep both consecutive calls on non-dedicated clients.
    if (!isDedicated())
        updateClimbAnimation(deltaTime);
#endif
    updateClimbAnimation(deltaTime);
    SimpleObject::handleVisualUpdate(deltaTime, interpolation);
}

// . Trigger-message bits 1/2 select climbing/get-off. The climb phase wraps
// only when strictly greater than one; a value exactly one is preserved.
void LadderContainer::updateClimbAnimation(float)
{
    if (!m_activeAnimationSystem)
        return;

    float phase;
    if (isGettingOff())
    {
        m_activeAnimationSystem->clearTriggerMessage(1);
        m_activeAnimationSystem->setTriggerMessage(2);
        phase = 1.0f - (m_ladder->getAbsoluteTop() - getAbsolutePosition().y)
            / *ladder_detail::ladderSoldierStartGetoff;
    }
    else
    {
        m_activeAnimationSystem->clearTriggerMessage(2);
        m_activeAnimationSystem->setTriggerMessage(1);
        phase = (getAbsolutePosition().y - m_ladder->getAbsoluteBottomReal())
            / (*ladder_detail::ladderStepSize * *ladder_detail::ladderStepSize);
        if (phase > 1.0f)
            phase -= static_cast<float>(std::floor(phase));
    }
    m_activeAnimationSystem->setForceTime(phase);
}

// . Near the top, add an ease-in displacement along the ladder's third
// transform row. Below that band, return false without touching the caller's output matrix.
bool LadderContainer::applyCameraAnimation(const Mat4& input, Mat4& output)
{
    const Mat4& ladderTransform = getAbsoluteTransformation();
    const float y = ladderTransform.m[13];
    const float start = m_ladder->getAbsoluteTop()
        - *ladder_detail::ladderSoldierStartGetoff;
    if (y <= start)
        return false;

    output = input;
    const float distance = y - start;
    const float factor = (1.0f - std::cos(distance))
        * *ladder_detail::ladderEndCameraOffset;
    output.m[12] += ladderTransform.m[8] * factor;
    output.m[13] += ladderTransform.m[9] * factor;
    output.m[14] += ladderTransform.m[10] * factor;
    return true;
}

// . A non-positive configured band disables the transition entirely.
bool LadderContainer::isGettingOff()
{
    if (*ladder_detail::ladderSoldierStartGetoff <= 0.0f)
        return false;
    return getAbsoluteTransformation().m[13]
        > m_ladder->getAbsoluteTop() - *ladder_detail::ladderSoldierStartGetoff;
}

// . Build the exact prospective standing transform and ask the occupant's
// ISoldier interface. No other pose is attempted.
bool LadderContainer::canSoldierExit()
{
    IObject* occupant = weakTarget<IObject>(m_occupyingObject);
    if (!occupant)
        return false;

    Mat4 transform = Mat4::getIdentity();
    Vec3* rotation = getExitRotation();
    setRotationZXY(transform, -rotation->z, -rotation->y, -rotation->x);
    const Vec3 exitPosition = _getExitPosition(
        m_ladder, getAbsolutePosition(), m_exitType);
    transform.m[12] = exitPosition.x;
    transform.m[13] = exitPosition.y;
    transform.m[14] = exitPosition.z;

    ISoldier* soldier = static_cast<ISoldier*>(occupant->queryInterface(IID_ISoldier));
    if (!soldier)
        return false;
    return soldier->testIfValidTransformation(SoldierPose_Stand, transform, true);
}

// the accumulator, clamped to [-30, +30] exactly as the fp sequence lands.
void LadderContainer::addForce(float force)
{
    float f = m_force + force;
    if (f >= -30.0f)
    {
        if (f > 30.0f)
            f = 30.0f;
    }
    else
    {
        f = -30.0f;
    }
    m_force = f;
}

bool LadderContainer::allowEnterOtherVehicle() { return true; }

} // namespace world
} // namespace hfe
} // namespace dice
