// DecalShadowManager, recovered from RendDX9.dll 1.50 (0x10127160..0x101274E0 and its factory
// 0x100B0793). Attested source path: RendDX9_r's __FILE__ for
// Geom/DecalSystem/DecalShadowManager.cpp (docs/tracks/rend/attribution.md). 0x10127490 is the
// std::vector<DecalObject*>::push_back instantiation (with its insert 0x10126F30).

#ifdef BF2_IMAGE_RENDDX9

#include "DecalShadowManager.h"
#include "DecalObject.h"
#include "IDecalManager.h"
#include "IDecalTemplate.h"
#include "../../Game/Objects/GeometryPartAccess.h"   // the geometry instance slots +0x28/+0x44/+0x48
#include "../../Game/Objects/IObject.h"
#include "../../Game/Objects/ObjectLayoutAccess.h"
#include "../../Math/Mat4.h"
#include "../../Math/Vec3.h"
#include "../../Physics/ICollisionMesh.h"
#include "../../Scene/Object/ObjectManager.h"
#include "../../Scene/Object/ObjectPredicator.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/PluginAbi.h"

#include <new>

namespace dice {
namespace hfe {

// PluginEntry.cpp - the host ObjectManager singleton (dword_10244AD8).
extern IBase* objectManager;

namespace geom {

extern IBase* g_DecalManager;               // ModuleGeom.cpp, dword_1024B908

namespace {

const unsigned int kIID_IBase               = 1u;
const unsigned int kIID_IDecalShadowManager = 820021u;    // dword_1021AE50

// g_DecalManager is the manager's primary interface pointer, re-read at every use as shipped.
inline IDecalManager* decalManager()
{
    return static_cast<IDecalManager*>(g_DecalManager);
}

// The ray-test predicator addShadow builds on its stack: sub_101C7E90 with mask 0x102
// (OFIsVisible | OFHasCollision) and 0x101C7EB0's rule that every bit of the mask must be present.
// The same shape as XpLightManager.cpp's LightOcclusionPredicator.
class ShadowGroundPredicator : public world::ObjectPredicator
{
public:
    ShadowGroundPredicator() : m_mask(0x102u) {}
    virtual bool predicate(world::IObject* object)
    {
        return (m_mask & object->getFlags()) == m_mask;
    }
    virtual unsigned int getFlags() const { return m_mask; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int) { return true; }

private:
    unsigned int m_mask;
};

} // anonymous namespace


// PE 0x10127160. The template is fetched (created on first use) from the decal manager, which
// ModuleGeom publishes first; neither the manager nor the template is tested for null, as shipped.
DecalShadowManager::DecalShadowManager()
    : m_refCount(1), m_shadows(), m_template(0)
{
    m_template = decalManager()->addTemplate(DecalTemplateString("DecalShadowTemplate"));
    m_template->setSize(1.0f);                                            // +0x08
}

// PE 0x101271D0. The decals are not handed back here; clearShadows does that.
DecalShadowManager::~DecalShadowManager()
{
    m_template = 0;
}


// The IBase quartet.

void DecalShadowManager::addRef()
{
    ++m_refCount;
}

int DecalShadowManager::getRef()
{
    return m_refCount;
}

// PE 0x101274E0. The interface has no destructor slot: at zero the destructor runs directly and
// the engine heap takes the block back.
int DecalShadowManager::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~DecalShadowManager();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// IBase (1) and IID_IDecalShadowManager answer `this`.
// PE 0x10127460.
void* DecalShadowManager::queryInterface(unsigned int iid) const
{
    if (iid == kIID_IBase)
        return const_cast<DecalShadowManager*>(this);
    return iid != kIID_IDecalShadowManager ? 0 : const_cast<DecalShadowManager*>(this);
}


// PE 0x10127200. VC7.1's clear() frees the storage (the listing's free plus three zeroed
// pointers).
void DecalShadowManager::clearShadows()
{
    for (unsigned int i = 0; i < m_shadows.size(); ++i)
        decalManager()->enqueue(m_shadows[i]);                           // +0x18
    m_shadows.clear();
}

// PE 0x10127260. The ray starts at the geometry's position raised by its bounding radius and
// runs 10 units down (ObjectManager +0x68 takes a delta). On an object hit, the hit object's
// collision mesh refines the point (ICollisionMesh +0x58, lod 1, front faces) and receives the
// decal with the hit face; a ground hit places it without a mesh. The refined normal is stored
// and never read, as shipped.
bool DecalShadowManager::addShadow(void* geometry)
{
    Vec3 direction;
    direction.x = 0.0f;
    direction.y = -10.0f;
    direction.z = 0.0f;
    const Mat4* const transform = world::geometry_part::getGeometryTransform(geometry);
    Vec3 from;
    from.x = transform->m[12];
    from.y = transform->m[13];
    from.z = transform->m[14];
    from.y = world::geometry_part::getBoundingRadius(geometry) + from.y;

    world::IObject* hitObject = 0;
    Vec3 hitPoint;
    hitPoint.x = hitPoint.y = hitPoint.z = 0.0f;
    Vec3 hitNormal;
    hitNormal.x = hitNormal.y = hitNormal.z = 0.0f;
    int hitPart;
    ShadowGroundPredicator predicator;                                    // sub_101C7E90(0x102)
    if (!reinterpret_cast<world::ObjectManager*>(objectManager)->intersectLine(
            hitObject, hitPoint, hitNormal, hitPart, from, direction, &predicator,
            false, false, true, false, 0))
        return false;

    world::ICollisionMesh* collisionMesh = 0;
    unsigned int faceInfo = 0;                  // CollisionFaceInfo: lod, geometry, u16 face
    if (hitObject)
    {
        collisionMesh = static_cast<world::ICollisionMesh*>(
            world::object_detail::LayoutAccess::collisionMesh(hitObject));   // IObject +0x40
        if (!collisionMesh)
            return false;
        Vec3 normal;
        normal.x = normal.y = normal.z = 0.0f;
        Vec3 position;
        position.x = position.y = position.z = 0.0f;
        float planeDistance;
        float along;
        // The inverse is fetched first (PE +0x84), then the transform (PE +0x80).
        const Mat4& worldToObject = hitObject->getInvAbsoluteTransformation();
        const Mat4& objectToWorld = hitObject->getAbsoluteTransformation();
        if (!collisionMesh->getDistance(1, false, objectToWorld, worldToObject, hitPoint,
                                        direction, normal, position, planeDistance, along, 0,
                                        reinterpret_cast<world::CollisionFaceInfo*>(&faceInfo),
                                        true, false))
            return false;
        hitPoint = position;
        hitNormal = normal;
    }

    DecalObject* const decal = static_cast<DecalObject*>(
        decalManager()->createClient(hitObject));                         // +0x14
    if (!decal)
        return false;
    decalManager()->addInstance(decal);                                   // +0x24
    m_shadows.push_back(decal);

    Mat4 placement = Mat4::getIdentity();
    placement.m[12] = placement.m[12] + hitPoint.x;
    placement.m[13] = placement.m[13] + hitPoint.y;
    placement.m[14] = placement.m[14] + hitPoint.z;
    decal->add(&placement, m_template, world::geometry_part::getBoundingRadius(geometry),
               collisionMesh,
               collisionMesh ? reinterpret_cast<world::CollisionFaceInfo*>(&faceInfo) : 0);
    decal->setShadowRecord(world::geometry_part::getShadowRecord(geometry));   // decal +0x04
    return true;
}


// 0x100B0793: an aggregate outer is refused; a failed query releases the new object.
IBase* __fastcall createDecalShadowManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(DecalShadowManager));
    DecalShadowManager* const self = storage ? new (storage) DecalShadowManager() : 0;
    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
