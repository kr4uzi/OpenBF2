// Recovered SoldierTemplate bodies whose shipped source placement is attested independently from
// Soldier.cpp. The ELF's function grouping places this setter before the translation unit's own
// static-initialization tail; both source-tree manifests name Game/Objects/Game/SoldierTemplate.cpp.

#include "SoldierTemplate.h"

#include "../../../Animation/BoneAnimation/Skeleton.h"
#include "../../../Sound/SoldierSound.h"

namespace dice {
namespace hfe {
namespace world {

// ELF D1 0x81B8A00 (D0 0x81B8B10) / dedicated PE 0x592670 (deleting wrapper
// ). Both call releaseAnimation first, then destroy soundFilter, geometry3P,
// geometry1P, another string, and BundleTemplate. The existing typed C++ members
// supply their own teardown after this body. [UNRESOLVED] The additional string at
// ELF +0x204 / PE +0x2B4 is still within the header's unmodeled _pad1F4 region;
// its construction/layout recovery is separate, not an invented raw destructor call.
SoldierTemplate::~SoldierTemplate()
{
    releaseAnimation();
}

// Full paired primary-slot bodies: the count is this leaf's m_refCount3, rather
// than BundleTemplate::m_refCount2. addRef's materialized register result is discarded
// by the existing void IBase slot. getRef/release return the signed count.
void SoldierTemplate::addRef()
{
    ++m_refCount3;
}

int SoldierTemplate::getRef()
{
    return m_refCount3;
}

int SoldierTemplate::release()
{
    const int remaining = --m_refCount3;
    if (remaining == 0)
        delete this;
    return remaining;
}

// BF2.exe 0x5F4830 / ELF 0x081B8C20: the manager calls this after assigning the template name.
void SoldierTemplate::templateInitialize()
{
    SoldierSound::initTemplates(this);
}

// , 23 B. The null guard and the exclusive use of the third-person
// skeleton are identical on both ports; authoring before skeleton3P is therefore silently ignored.
void SoldierTemplate::setSkeletonCollisionBone(const std::string& name, int material,
                                                const Vec3& offset, float radius, float length)
{
    if (m_skeleton3P)
        m_skeleton3P->setSkeletonCollisionBone(name, material, offset, radius, length);
}

} // namespace world
} // namespace hfe
} // namespace dice
