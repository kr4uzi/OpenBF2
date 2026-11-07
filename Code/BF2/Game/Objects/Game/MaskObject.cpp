// Path: [INFERRED] - see MaskObject.h. Every own body read in both dedicated images.
#include "MaskObject.h"
#include "../GeometryPartAccess.h"   // named geometry operations (non-MSVC)
#if defined(BF2_STATIC_PLUGINS) && !defined(_MSC_VER)
// [CHOSEN, not recovered] Native static geometry tables retain PE slot order.
#include "../../../Geom/NativeGeometrySlots.h"
#endif
#include "Soldier.h"
#include "../IPlayer.h"
#include "../../../Animation/BoneAnimation/Skeleton.h"
#include "../../../Core/Ids.h"

namespace dice {
namespace hfe {
namespace world {

// /0x818D500; PE constructor 0x590C40 and createObject 0x590D40.
MaskObject::MaskObject(const MaskObjectTemplate* objectTemplate)
    : AnimatedBundle(objectTemplate), m_maskTemplate(objectTemplate), m_geometryPositionInitialized(false)
{
}

// / 0x818D9D0; PE shares AnimatedBundle's destructor (0x592610).
MaskObject::~MaskObject()
{
}

// . Bundle's init is the direct call, with no intermediate override.
void MaskObject::init()
{
    Bundle::init();
    updateFlags(0x1800, 2);
}


MaskObjectType MaskObject::getType() const
{
    return m_maskTemplate->getMaskType();
}

// . The initial geometry update happens before the player/ability
// tests, and a visible mask updates it again. Repeated getOccupingPlayer/getType calls are retail.
void MaskObject::handleUpdate(float, unsigned int)
{
    Soldier* soldier = getSoldier();
    if (!m_geometryPositionInitialized && soldier)
    {
        m_geometryPositionInitialized = true;
        updateGeometryPosition(soldier);
    }
    if (!soldier || !soldier->getOccupingPlayer())
    {
        updateFlags(0, 2);
        return;
    }
    if ((soldier->getOccupingPlayer()->getIsGasMaskOn() && getType() == MOTGasMask)
        || (soldier->getOccupingPlayer()->getNightVision() && getType() == MOTNightVisionGoggles))
    {
        updateFlags(2, 0);
        updateGeometryPosition(soldier);
    }
    else
        updateFlags(0, 2);
}

// the direct base tail-call preserves both float arguments.
void MaskObject::handleVisualUpdate(float deltaTime, float interpolation)
{
    AnimatedBundle::handleVisualUpdate(deltaTime, interpolation);
}

// . There is no soldier/skeleton null guard in either original.
void MaskObject::updateGeometryPosition(Soldier* soldier)
{
    if (m_geometry)
    {
        anim::Skeleton* skeleton = soldier->getCollisionSkeleton(soldier->getPose());
        // IGeometry remains undeclared. Keep the proven pose seam isolated and express the
        // genuine ABI difference: PE dispatches +0x54, ELF +0x58 (one versus two destructor slots).
#if defined(_MSC_VER)
        typedef void (__thiscall* GeometryPoseFn)(void*, void*, int);
        const unsigned int slot = 0x54 / 4;
        reinterpret_cast<GeometryPoseFn>((*reinterpret_cast<void***>(m_geometry))[slot])(
            m_geometry, skeleton->m_transforms, skeleton->m_boneCount);
#else
        geometry_part::setBoneTransforms(m_geometry, skeleton->m_transforms, skeleton->m_boneCount);   // ELF +0x58
#endif
    }
}

// ; PE inlined at 0x590DD4..0x590E19. Only parent and grandparent are inspected.
Soldier* MaskObject::getSoldier() const
{
    if (m_parent)
    {
        if (m_parent->getTemplate()->getClassID() == CID_SoldierTemplate)
            return static_cast<Soldier*>(m_parent);
        if (m_parent->getParent()
            && m_parent->getParent()->getTemplate()->getClassID() == CID_SoldierTemplate)
            return static_cast<Soldier*>(m_parent->getParent());
    }
    return 0;
}

} // namespace world
} // namespace hfe
} // namespace dice
