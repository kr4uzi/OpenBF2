// Animated-bundle lifecycle and skeleton/geometry synchronization.

#include "AnimatedBundle.h"
#include "../GeometryPartAccess.h"   // named geometry operations (non-MSVC)

#include "../../../Animation/BoneAnimation/AnimationSystem.h"
#include "../../../Animation/BoneAnimation/Skeleton.h"

#include "../../../Core/Ids.h"
#include "../AnimatedBundleTemplate.h"

namespace dice {


namespace hfe {
namespace world {

namespace {
// IGeometry is still unrecovered, so its one pose call remains isolated here. Animation-system
// calls are typed: PE has one destructor slot where ELF has two, making fixed raw offsets invalid
// as soon as the formerly-null template pointers become live.
#if defined(_MSC_VER)
#  define BF2_AB_VCALL __thiscall
const unsigned int kGeometryPoseSlot = 0x54;
// Geometry pose upload takes one matrix per bone and a count; the slot differs by platform.
typedef void  (BF2_AB_VCALL* GeometryPoseFn)(void*, void*, int);
inline void* rawSlot(void* object, unsigned int byteOffset)
{
    return (*reinterpret_cast<void***>(object))[byteOffset / 4];
}
#endif
} // namespace

AnimatedBundle::AnimatedBundle(const AnimatedBundleTemplate* objectTemplate)
    : Bundle(objectTemplate)
{
    // the three vptrs (primary, +0x13C IAnimatedBundle, +0x80 IConsoleSaveable): the compiler's
    m_refCount = 1;                                              // +0x140
    m_activeAnimationSystemIndex = 0;                            // +0x14C
    m_skeleton = 0;                                              // +0x150
    m_animationSystems[1] = 0;                                   // +0x148
    m_animationSystems[0] = 0;                                   // +0x144
    if (objectTemplate->m_skeleton)                              // template +0x180
    {
        m_skeleton = new anim::Skeleton();                       // operator new(0x40)
        *m_skeleton = *objectTemplate->m_skeleton;               // Skeleton::operator=
        if (m_geometry)                                          // +0x38
            updateFlags(0x1000u, 0);
    }
    if (objectTemplate->m_animationSystem1P)                     // template +0x184
        m_animationSystems[0] = objectTemplate->m_animationSystem1P->makeInstance();
    if (objectTemplate->m_animationSystem3P)                     // template +0x188
        m_animationSystems[1] = objectTemplate->m_animationSystem3P->makeInstance();
}

AnimatedBundle::~AnimatedBundle()
{
    if (m_skeleton)
        delete m_skeleton;                                       // ~Skeleton + operator delete
    delete m_animationSystems[0];
    delete m_animationSystems[1];
    // ~Bundle
}

// The quartet - 0x81682E0 / 0x8168320 / 0x8168350 / 0x81683B0.
void AnimatedBundle::addRef()  { ++m_refCount; }
int  AnimatedBundle::getRef()  { return m_refCount; }
int  AnimatedBundle::release()
{
    const int left = m_refCount - 1;
    const bool last = (m_refCount == 1);
    m_refCount = left;
    if (last)
        delete this;                                             // the D0 slot (+0x18)
    return left;
}

void* AnimatedBundle::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObject)
        return const_cast<AnimatedBundle*>(this);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<AnimatedBundle*>(this));   // this + 0x80
    if (iid == IID_IAnimatedBundle)
        return static_cast<IAnimatedBundle*>(const_cast<AnimatedBundle*>(this));        // this + 0x13C
    return 0;
}

// the client's per-frame skeleton pose - only with a skeleton AND a geometry.
void AnimatedBundle::handleVisualUpdate(float deltaTime, float)
{
    if (!m_skeleton)
        return;
    // Both ports retain the geometry across the animation callbacks (PE 0x5F410D -> 0x5F4163).
    void* const geometry = m_geometry;
    if (!geometry)
        return;
    if (m_animationSystems[0])
    {
        m_animationSystems[0]->update(this, deltaTime);
        m_animationSystems[0]->applyOnSkeleton(m_skeleton, false);
    }
    m_skeleton->transform(0, -1);
    // Upload one transformation matrix per bone.
#if defined(_MSC_VER)
    reinterpret_cast<GeometryPoseFn>(rawSlot(geometry, kGeometryPoseSlot))(
        geometry, m_skeleton->m_transforms, m_skeleton->m_boneCount);
#else
    geometry_part::setBoneTransforms(geometry, reinterpret_cast<const Mat4*>(m_skeleton->m_transforms),
                                     m_skeleton->m_boneCount);                   // ELF +0x58
#endif
}

// / 0x8167940, 74 B each: both systems, slots +0x30 / +0x38.
void AnimatedBundle::setTriggerMessage(int message)
{
    for (int i = 0; i < 2; ++i)
        if (m_animationSystems[i])
            m_animationSystems[i]->setTriggerMessage(static_cast<unsigned int>(message));
}

void AnimatedBundle::clearTriggerMessage(int message)
{
    for (int i = 0; i < 2; ++i)
        if (m_animationSystems[i])
            m_animationSystems[i]->clearTriggerMessage(static_cast<unsigned int>(message));
}


void  AnimatedBundle::setActiveAnimationSystem(int index) { m_activeAnimationSystemIndex = index; }
void* AnimatedBundle::getActiveAnimationSystem()          { return m_animationSystems[m_activeAnimationSystemIndex]; }

} // namespace world
} // namespace hfe
} // namespace dice
