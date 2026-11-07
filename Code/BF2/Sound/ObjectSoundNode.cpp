// Object-backed sound nodes expose object transforms and physics velocity.

#include "ObjectSoundNode.h"

#include "../Game/Objects/IObject.h"
#include "../Game/Objects/ObjectLayoutAccess.h"
#include "../Math/Mat4.h"
#include "../Physics/IPhysicsNode.h"

namespace dice {
namespace hfe {

const Mat4& ObjectSoundNode::getTransform()
{
    if (m_object)
        return m_object->getAbsoluteTransformation();

    // [CHOSEN, not recovered] Retail forms a reference through null on this impossible owner-less
    // path. Preserve a stable reference without manufacturing motion or another engine object.
    static const Mat4 s_zero{};     // value-initialized: Clang rejects const default-init here
    return s_zero;
}

const Math::Vec3* ObjectSoundNode::getVelocity()
{
    if (!m_object)
        return 0;

    world::IPhysicsNode* const node = static_cast<world::IPhysicsNode*>(
        world::object_detail::LayoutAccess::physicsNode(m_object));
    return node ? &node->getPositionalSpeed() : 0;
}

} // namespace hfe
} // namespace dice
