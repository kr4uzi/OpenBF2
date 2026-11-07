// Path:     attested by __FILE__ in both PE and ELF server binaries.
// Contents: recovered Bundle construction and composite child propagation.

#include "Bundle.h"
#include "../GeometryPartAccess.h"   // named geometry operations (non-MSVC)
#include "BundleTemplate.h"
#include "../../../Core/Debug.h"
#include "../../../Core/Ids.h"
#include "../../../Physics/CollisionManager.h"
#include "../../../Physics/ICollisionMesh.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../GameLogicComp/IArmorGLComp.h"

#include <cfloat>
#include <cmath>

namespace dice {
namespace hfe {
namespace world {

namespace {

#if defined(_MSC_VER)
#  define BF2_BUNDLE_VCALL __thiscall
#else
#  define BF2_BUNDLE_VCALL
#endif

#if defined(_MSC_VER)
template <class Fn>
inline Fn bundleVslot(void* object, unsigned int byteOffset)
{
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[byteOffset / 4]);
}
#endif

// The renderer geometry interface is not otherwise present in the server source tree.  Both ports
// pin this one operation: PE +0x4C / ELF +0x54, taking an absolute transform and a part number.
void setGeometryPartTransformation(geom::IGeometry* geometry,
                                   const Mat4& transformation, int part)
{
#if defined(_MSC_VER)
    const unsigned int slot = 0x4C;
    typedef void (BF2_BUNDLE_VCALL* Fn)(void*, const Mat4&, int);
    bundleVslot<Fn>(geometry, slot)(geometry, transformation, part);
#else
    geometry_part::setTransform(geometry, &transformation, part);                // ELF +0x54
#endif
}

// Leaves nearly unit vectors unchanged and collapses nearly zero vectors to zero.
// [UNRESOLVED] original helper placement.
void normalizeInPlace(Vec3& value)
{
    const float squaredLength = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(squaredLength - 1.0f) < FLT_EPSILON)
        return;

    if (std::fabs(squaredLength) >= FLT_EPSILON)
    {
        const float inverse = 1.0f / std::sqrt(squaredLength);
        value.x *= inverse;
        value.y *= inverse;
        value.z *= inverse;
    }
    else
    {
        value.x = 0.0f;
        value.y = 0.0f;
        value.z = 0.0f;
    }
}

#undef BF2_BUNDLE_VCALL

} // unnamed namespace

void setChildPartGeometries(IObject* object, geom::IGeometry* geometry)
{
    while (object && !object->m_geometry)
    {
        SimpleObjectTemplate* objectTemplate = static_cast<SimpleObjectTemplate*>(
            const_cast<IObjectTemplate*>(object->m_template)->queryImplementation(
                CID_SimpleObjectTemplate));
        if (objectTemplate && objectTemplate->m_geometryPart > 0)
            setGeometryPartTransformation(geometry,
                                          object->getAbsoluteTransformation(),
                                          objectTemplate->m_geometryPart);

        setChildPartGeometries(object->getChild(), geometry);
        object = object->m_sibling;
    }
}

void setChildPartCollisionMeshes(const IObjectTemplate* objectTemplate,
                                 IObject* object, const std::string& collisionMesh)
{
    if (!object || object->m_collisionMesh)
        return;

    SimpleObjectTemplate* childTemplate = static_cast<SimpleObjectTemplate*>(
        const_cast<IObjectTemplate*>(object->m_template)->queryImplementation(
            CID_SimpleObjectTemplate));
    if (childTemplate && childTemplate->m_collisionPart > 0)
    {
        ICollisionMesh* mesh = collisionManager->getCollisionMeshPart(
            collisionMesh, childTemplate->m_collisionPart);
        if (mesh)
        {
            mesh->createMaterialMapping(objectTemplate);
            object->setComponent(IID_ICollisionMesh, mesh);
            mesh->release();
        }
    }

    setChildPartCollisionMeshes(objectTemplate, object->getChild(), collisionMesh);
    setChildPartCollisionMeshes(objectTemplate, object->m_sibling, collisionMesh);
}

void turnOffChildFloaters(IObject* object)
{
    if (!object)
        return;

    const IObjectTemplate* objectTemplate = object->getTemplate();
    SimpleObjectTemplate* simpleTemplate = static_cast<SimpleObjectTemplate*>(
        const_cast<IObjectTemplate*>(objectTemplate)->queryImplementation(
            CID_SimpleObjectTemplate));
    if (simpleTemplate)
        simpleTemplate->m_floaterMod = 0.0f;

    turnOffChildFloaters(object->getChild());
    turnOffChildFloaters(object->getSibling());
}

Bundle::Bundle(const BundleTemplate* objectTemplate)
    : SimpleObject(objectTemplate),
      m_bundleTransformation(&Mat4::getIdentity())
{
}

Bundle::~Bundle() {}

void Bundle::init()
{
    SimpleObject::init();
    const BundleTemplate* objectTemplate = static_cast<const BundleTemplate*>(m_template);
    objectTemplate->addBundleChilds(this);

    if (m_geometry)
        setChildPartGeometries(getChild(),
                               static_cast<geom::IGeometry*>(m_geometry));

    IObjectTemplate* simpleTemplate = static_cast<IObjectTemplate*>(
        const_cast<BundleTemplate*>(objectTemplate)->queryImplementation(
            CID_SimpleObjectTemplate));
    if (m_collisionMesh)
        setChildPartCollisionMeshes(simpleTemplate, getChild(),
                                    objectTemplate->getCollisionMesh());

    turnOffChildFloaters(getChild());
    if (m_parent)
    {
        IBundleTemplate* parentTemplate = static_cast<IBundleTemplate*>(
            const_cast<IObjectTemplate*>(m_parent->getTemplate())->queryInterface(
                IID_IBundleTemplate));
        if (parentTemplate)
            m_bundleTransformation = &parentTemplate->getTransformation(getChildId());
    }
}

void Bundle::enableCachedObject()
{
    if (m_geometry)
        setChildPartGeometries(getChild(), static_cast<geom::IGeometry*>(m_geometry));
    SimpleObject::enableCachedObject();
}

bool Bundle::handleCollision(IObject* object, const Vec3& position,
                             const Vec3& normal, const Vec3& velocity,
                             int material, int collisionLod,
                             CollisionFaceInfo*, CollisionFaceInfo*)
{
    return SimpleObject::handleCollision(object, position, normal, velocity,
                                         material, collisionLod, 0, 0);
}

// Broadcasts the message to children before handling this bundle. Message 24 may detach and launch an armored part.
// Launch requires a physics node; missing physics reaches a fatal diagnostic before the speed updates.
void Bundle::handleMessage(TemplateMessage message, IPlayer* player, bool flag)
{
    const BundleTemplate* objectTemplate = static_cast<const BundleTemplate*>(m_template);
    for (IObject* child = getChild(); child; child = child->m_sibling)
        child->handleMessage(message, player, flag);

    if (objectTemplate->m_explodePartAtDestroy && message == static_cast<TemplateMessage>(24))
    {
        IObjectComponent* armor = 0;
        for (IObject* object = this; object; object = object->m_parent)
        {
            armor = object->m_armorComponent;
            if (armor)
                break;
        }

        if (armor && static_cast<IArmorGLComp*>(armor)->isSendingMessage())
        {
            updateFlags(0x800u, 0);

            IObject* parent = m_parent;
            if (parent)
            {
                parent->removeChild(this);

                const float radius =
                    getRootParent(parent)->getPhysicBoundingRadius();

                const Vec3& position = getAbsolutePosition();
                Vec3 launch = {position.x, radius + radius + position.y, position.z};

                const Vec3& rootPosition = getRootParent(parent)->getAbsolutePosition();
                launch.x -= rootPosition.x;
                launch.y -= rootPosition.y;
                launch.z -= rootPosition.z;

                normalizeInPlace(launch);
                launch.x *= 10.0f;
                launch.y *= 10.0f;
                launch.z *= 10.0f;

                if (m_physicsNode)
                    static_cast<IPhysicsNode*>(m_physicsNode)->getIsMobile();
                else
                    Debug(DEBUGTYPE_6, "Game/Objects/Base/Bundle.cpp", 277)
                        << "No physicsNodeComp!";

                static_cast<IPhysicsNode*>(m_physicsNode)->setPositionalSpeed(launch);
                const Vec3 stop = {0.0f, 0.0f, 0.0f};
                static_cast<IPhysicsNode*>(m_physicsNode)->setRotationalSpeed(stop);
            }
        }
    }

    SimpleObject::handleMessage(message, player, flag);
}

void Bundle::handlePlayerInput(IPlayer*, const io::PlayerInput&, float, unsigned int) {}

void Bundle::setInputId(char inputId)
{
    Object::setInputId(inputId);
    const BundleTemplate* objectTemplate = static_cast<const BundleTemplate*>(m_template);
    for (IObject* child = getChild(); child; child = child->getSibling())
    {
        if (child->getChildId() >= 0 &&
            static_cast<std::size_t>(child->getChildId()) <
                objectTemplate->m_templateInfos.size())
        {
            const int mapped = objectTemplate
                ->m_templateInfos[child->getChildId()]->m_inputId;
            child->setInputId(static_cast<char>(mapped < 0 ? inputId : mapped));
        }
    }
}

const Mat4* Bundle::getBundleTransformation() const { return m_bundleTransformation; }

} // namespace world
} // namespace hfe
} // namespace dice
