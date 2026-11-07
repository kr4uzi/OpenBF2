// Antenna-object lifecycle and visual simulation.

#include "AntennaObject.h"
#include "../GeometryPartAccess.h"   // named geometry operations (non-MSVC)

#include <cfloat>
#include <cmath>

#include "../../../Animation/BoneAnimation/Skeleton.h"   // the declared-only anim::Skeleton
#include "../../../Core/Ids.h"
#include "../../../Math/Mat4.h"                          // Mat4, setRotateX/Y/ZDeg
#include "../AntennaObjectTemplate.h"

namespace dice {
namespace hfe {
namespace world {

namespace {

// Passes the bone-matrix pointer and count to the renderer.
// [PLATFORM] Verified slots are ELF 0x58 and PE 0x54; retain the two-argument calling convention.
#if defined(_MSC_VER)
#define BF2_ANTENNA_GEOM_CALL __thiscall
const unsigned int setBoneTransformsSlot = 0x54;
#endif
void geometrySetBoneTransforms(void* geometry, const Mat4* transforms, int boneCount)
{
#if defined(_MSC_VER)
    typedef void (BF2_ANTENNA_GEOM_CALL* F)(void*, const Mat4*, int);
    reinterpret_cast<F>(
        (*static_cast<void***>(geometry))[setBoneTransformsSlot / 4])(geometry, transforms, boneCount);
#else
    geometry_part::setBoneTransforms(geometry, transforms, boneCount);           // ELF +0x58
#endif
}
#undef BF2_ANTENNA_GEOM_CALL

// The 3x3 rotation of a vector, IN place, row-vector convention. Inlined by GCC into the ELF body;
// MSVC keeps it out of line at PE 0x5A1770. Written as the tree writes it everywhere else
// (RagDoll.cpp:490, GameLogicExplosion.cpp:495) - this engine has no shared helper for it.
void rotateByUpper3x3(Vec3& v, const Mat4& m)
{
    const float x = m.m[0] * v.x + m.m[4] * v.y + m.m[8]  * v.z;
    const float y = m.m[1] * v.x + m.m[5] * v.y + m.m[9]  * v.z;
    const float z = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z;
    v.x = x; v.y = y; v.z = z;
}

// The 2-D normalise, same "only if it is not already unit" shape as Vec3's (Math/Vec3.cpp's
// normaliseIfNeeded) and the same degenerate-zeroing behaviour. Inlined in the ELF; PE 0x5F3230,
// where it returns 0 or -20001 - the Vec2 sibling of Vec3's -20003. Both callers here discard it.
// The two thresholds are different and that is as read: the "already unit" test uses
// FLT_EPSILON, the degenerate test FLT_EPSILON squared.
void normaliseIfNeeded2(float& x, float& y)
{
    const float lenSq = x * x + y * y;
    if (std::fabs(lenSq - 1.0f) < FLT_EPSILON)
        return;                                          // already unit - untouched
    if (std::fabs(lenSq) < 1.4210855e-14f)               // FLT_EPSILON squared
    {
        x = 0.0f;
        y = 0.0f;
        return;
    }
    const float inv = 1.0f / std::sqrt(lenSq);
    x *= inv;
    y *= inv;
}

// Inlined in the ELF as a min/max pair; PE 0x402EE0 keeps it out of line.
float clampRange(float value, float low, float high)
{
    if (value < low)
        return low;
    return value > high ? high : value;
}

// sign(0) IS zero here, which is what stops the wrap loop below from spinning on an exact zero.
float signOf(float value)
{
    if (value > 0.0f)
        return 1.0f;
    return value < 0.0f ? -1.0f : 0.0f;
}

// The bone-override write, lazily allocating the bone's owned Mat4. Inlined twice in the ELF; the
// PE keeps it out of line at 0x5F32A0 and calls it with the literals 0 and 1 - which is what
// proves the ELF's `bone + 0x40` and `bone + 0x88` are bones[0] and bones[1] of a 72-byte Bone,
// rather than two fields of one bone.
// The fresh matrix is initialised to the identity and then immediately overwritten by the copy.
// That is dead in the shipped code too, and is kept for that reason.
void setBoneOverride(anim::Skeleton* skeleton, int boneIndex, const Mat4& transform)
{
    anim::Skeleton::Bone& bone = skeleton->m_bones[boneIndex];
    if (!bone.m_override)
        bone.m_override = new Mat4(Mat4::getIdentity());          // operator new(0x40)
    *bone.m_override = transform;
}

}  // namespace

AntennaObject::AntennaObject(const AntennaObjectTemplate* objectTemplate)
    : SimpleObject(objectTemplate)
    , m_refCount(1)                                                                  // +0x138
    , m_template(objectTemplate)                                                     // +0x13C
    , m_skeleton(0)                                                                  // +0x140
    , m_swayAngleX(0.0f)                                                             // +0x144
    , m_swayAngleZ(0.0f)
    , m_swayVelocityX(0.0f)
    , m_swayVelocityZ(0.0f)
    , m_heading(0.0f)                                                                // +0x16C
    , m_wobblePhase(0.0f)                                                            // +0x170
{
    m_previousPosition.x = 0.0f;                                                     // +0x154
    m_previousPosition.y = 0.0f;
    m_previousPosition.z = 0.0f;
    m_previousVelocity.x = 0.0f;                                                     // +0x160
    m_previousVelocity.y = 0.0f;
    m_previousVelocity.z = 0.0f;
    if (objectTemplate->m_skeleton)                                                  // template +0x168
    {
        m_skeleton = new anim::Skeleton();                                           // operator new(0x40)
        *m_skeleton = *objectTemplate->m_skeleton;                                   // Skeleton::operator=
        updateFlags(0x1000u, 0u);
    }
}


AntennaObject::~AntennaObject()
{
    if (m_skeleton)
        delete m_skeleton;                                                           // ~Skeleton + operator delete
}

// The quartet.
void AntennaObject::addRef()  { ++m_refCount; }
int  AntennaObject::getRef()  { return m_refCount; }
int  AntennaObject::release()
{
    const int left = m_refCount - 1;
    const bool last = (m_refCount == 1);
    m_refCount = left;
    if (last)
        delete this;                                                                 // the D0 slot
    return left;
}

void* AntennaObject::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObject)
        return const_cast<AntennaObject*>(this);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<AntennaObject*>(this));   // this + 0x80
    return 0;
}

// Updates a two-bone antenna spring from vehicle motion and submits the resulting skeleton pose.
// Requires skeleton, geometry and positive deltaTime. Velocity change is not divided by time.
// Zero previous position reseeds motion; the original behavior at the world origin is retained.
void AntennaObject::handleVisualUpdate(float deltaTime, float)
{
    void* geometry = m_geometry;                                                     // +0x38
    if (!m_skeleton || !geometry || deltaTime <= FLT_EPSILON)
        return;

    // getAbsoluteTransformation, ELF slot +0x84 / PE +0x80; +0x30 into the matrix is the
    // translation row, i.e. the world position.
    const Mat4& absolute = getAbsoluteTransformation();
    Vec3 position;
    position.x = absolute.m[12];
    position.y = absolute.m[13];
    position.z = absolute.m[14];

    if (m_previousPosition.x == 0.0f && m_previousPosition.y == 0.0f
        && m_previousPosition.z == 0.0f)
    {
        m_previousPosition = position;
    }

    const float invDeltaTime = 1.0f / deltaTime;
    Vec3 velocity;
    velocity.x = (position.x - m_previousPosition.x) * invDeltaTime;
    velocity.y = (position.y - m_previousPosition.y) * invDeltaTime;
    velocity.z = (position.z - m_previousPosition.z) * invDeltaTime;

    Vec3 acceleration;                                                               // see note 1
    acceleration.x = velocity.x - m_previousVelocity.x;
    acceleration.y = velocity.y - m_previousVelocity.y;
    acceleration.z = velocity.z - m_previousVelocity.z;

    m_previousVelocity = velocity;
    m_previousPosition = position;

    const Mat4& inverse = getInvAbsoluteTransformation();                            // +0x88 PE +0x84
    rotateByUpper3x3(acceleration, inverse);
    rotateByUpper3x3(velocity, inverse);

    const AntennaObjectTemplate* const t = m_template;

    // the X spring
    m_swayVelocityX -= acceleration.z * t->accelerationFactor + velocity.z * t->speedFactor;
    m_swayVelocityX -= t->stiffness * m_swayAngleX;
    m_swayVelocityX -= t->damping * m_swayVelocityX;
    if (std::fabs(m_swayVelocityX) < 0.001f)
        m_swayVelocityX = 0.0f;
    m_swayAngleX += deltaTime * m_swayVelocityX;
    if (std::fabs(m_swayAngleX) < 0.001f)
        m_swayAngleX = 0.0f;
    m_swayAngleX = clampRange(m_swayAngleX, -t->maxSwayAngle[0], t->maxSwayAngle[0]);

    // the Z spring - note the minus between its two drive terms
    m_swayVelocityZ -= acceleration.x * t->accelerationFactor - velocity.x * t->speedFactor;
    m_swayVelocityZ -= t->stiffness * m_swayAngleZ;
    m_swayVelocityZ -= t->damping * m_swayVelocityZ;
    if (std::fabs(m_swayVelocityZ) < 0.001f)
        m_swayVelocityZ = 0.0f;
    m_swayAngleZ += deltaTime * m_swayVelocityZ;
    if (std::fabs(m_swayAngleZ) < 0.001f)
        m_swayAngleZ = 0.0f;
    m_swayAngleZ = clampRange(m_swayAngleZ, -t->maxSwayAngle[1], t->maxSwayAngle[1]);

    // which way is it leaning
    float target = 0.0f;
    if (std::fabs(m_swayAngleX) > 0.5f || std::fabs(m_swayAngleZ) > 0.5f)
    {
        float leanX = -m_swayAngleZ;
        float leanY = m_swayAngleX;
        normaliseIfNeeded2(leanX, leanY);

        // The reference direction is the literal (0, -1), built on the stack in both ports.
        float dot = leanY * -1.0f + leanX * 0.0f;
        // clamped twice, and that is as shipped - the ELF emits two min/max pairs and the PE two
        // calls to the same clamp helper. The second is a no-op; it is kept because it is there.
        dot = clampRange(dot, -1.0f, 1.0f);
        dot = clampRange(dot, -1.0f, 1.0f);

        float degrees = std::acos(dot) * 57.295776f;
        if (leanX < 0.0f)
            degrees = -degrees;
        target = clampRange(degrees, -360.0f, 360.0f);
    }

    // the speed wobble
    float wobble = 0.0f;
    const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y
                                  + velocity.z * velocity.z);
    if (speed > 0.5f)
    {
        m_wobblePhase += speed * deltaTime;
        while (std::fabs(m_wobblePhase) > 360.0f)
            m_wobblePhase -= signOf(m_wobblePhase) * 360.0f;
        wobble = std::sin(0.017453292f * m_wobblePhase) * 10.0f;
    }

    // approach the heading
    if (target < 0.0f)
        target += 360.0f;
    if (std::fabs(m_heading - target) > 0.0099999998f)
    {
        const float distance = std::fabs(target - m_heading);
        float direction = distance > 180.0f ? -1.0f : 1.0f;      // take the short way round
        if (m_heading > target)
            direction = -direction;
        const float step = (direction * distance + direction * distance) * deltaTime;
        if (std::fabs(step) < distance)
        {
            m_heading += step;
            if (m_heading < 0.0f)
                m_heading += 360.0f;
            else if (m_heading > 360.0f)
                m_heading -= 360.0f;
        }
        else
        {
            m_heading = target;                                  // the step would overshoot: snap
        }
    }

    // pose the two bones
    // The shipped code inlines dice's zeroing Mat4 constructor and then calls makeIdentity over it
    // (PE 0x440B20 then 0x45AFE0, one after the other on every one of these four). This tree's Mat4
    // is a plain aggregate with neither - see Math/Mat4.h - so the identity comes from getIdentity,
    // which is the same value. It is load-bearing: setRotate*Deg and rotMult both write only the
    // nine rotation elements and leave the translation row and m[15] untouched.
    Mat4 headingRotation = Mat4::getIdentity();
    setRotateYDeg(headingRotation, wobble + m_heading);
    setBoneOverride(m_skeleton, 0, headingRotation);

    Mat4 bendX = Mat4::getIdentity();
    Mat4 bendZ = Mat4::getIdentity();
    Mat4 bend  = Mat4::getIdentity();
    setRotateXDeg(bendX, m_swayAngleX);
    setRotateZDeg(bendZ, m_swayAngleZ);
    bend.rotMult(bendX, bendZ);
    setBoneOverride(m_skeleton, 1, bend);

    m_skeleton->transform(0, -1);
    geometrySetBoneTransforms(geometry, m_skeleton->m_transforms, m_skeleton->m_boneCount);
}

} // namespace world
} // namespace hfe
} // namespace dice
