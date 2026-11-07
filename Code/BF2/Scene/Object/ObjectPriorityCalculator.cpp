// Path: [INFERRED], see header. Five-slot tables ELF 0x88CA1B0 / PE 0x7FEE30.
#include "ObjectPriorityCalculator.h"
#include "ObjectManager.h"
#include "../../Core/Ids.h"
#include "../../Game/Objects/IObject.h"
#include "../../RendMisc/IRenderView.h"
#if !DEDICATED
#include "../../RendMisc/IRenderer.h"
#include "../../Game/Main/BF2ClientSeams.h"   // g_renderer, BF2.exe ds:0x9A557C
#endif
#include <cmath>
#include <vector>

namespace dice { namespace hfe { namespace world {
// Constructors are inlined in ELF 0x87FA540 / PE 0x6D1330.
ObjectPriorityCalculator::ObjectPriorityCalculator() : m_refCount(1) {}
void ObjectPriorityCalculator::addRef() { ++m_refCount; }
int ObjectPriorityCalculator::getRef() { return m_refCount; }
int ObjectPriorityCalculator::release() // ELF 0x831B6E0 / PE 0x424BB0: no virtual destructor
{
    const int result = --m_refCount;
    if (!result) delete this;
    return result;
}
void* ObjectPriorityCalculator::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectPriorityCalculator)
        return const_cast<ObjectPriorityCalculator*>(this);
    return 0;
}
IBase* ObjectPriorityCalculator::create(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    ObjectPriorityCalculator* calculator = new ObjectPriorityCalculator;
    IBase* result = static_cast<IBase*>(calculator->queryInterface(iid));
    if (!result) calculator->release();
    return result;
}

#if DEDICATED
// Updates priorities from render-camera state. Dedicated images retain the original
// null-renderer fault; preserve the volatile load and keep raw ABI calls within this seam.
void ObjectPriorityCalculator::update()
{
    std::vector<IObject*> objects;
#if defined(_MSC_VER)
    typedef dx9rend::IRenderView* (__thiscall* GetCamera)(void*);
    typedef void (__thiscall* SetPriority)(void*, float);
#else
    typedef dx9rend::IRenderView* (*GetCamera)(void*);
    typedef void (*SetPriority)(void*, float);
#endif
    void** renderVtable = *reinterpret_cast<void** volatile*>(0);
    dx9rend::IRenderView* camera = reinterpret_cast<GetCamera>(renderVtable[0x48 / 4])(0);
    const float radius = 2.0f * camera->getFarPlane();
    renderVtable = *reinterpret_cast<void** volatile*>(0);
    camera = reinterpret_cast<GetCamera>(renderVtable[0x48 / 4])(0);
    const Mat4& transform = *camera->getViewMatrixPtr();
    const float* position = &transform.m[12];
    const Vec3 center = {position[0], position[1], position[2]};
    objectManager->getObjectsFromGrid(center, radius, objects, 0);
    for (std::vector<IObject*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
    {
        IBase* priority = (*i)->queryComponent(geom::IID_IGeometry, io::IID_IResourcePriority);
        if (!priority) continue;
        const Vec3& absolute = (*i)->getAbsolutePosition();
        const Vec3 delta = {absolute.x - position[0], absolute.y - position[1], absolute.z - position[2]};
        const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (radius > distance)
            reinterpret_cast<SetPriority>((*reinterpret_cast<void***>(priority))[0x10 / 4])(
                priority, radius / distance);
    }
}
#else
// BF2.exe 0x72CFD0 (table 0x91C490 +0x10): the dedicated walk with the camera taken from the exe's
// "Renderer" singleton, ds:0x9A557C (g_renderer), reloaded before each getRenderView call. No
// renderer null check, as shipped.
void ObjectPriorityCalculator::update()
{
    std::vector<IObject*> objects;
#if defined(_MSC_VER)
    typedef void (__thiscall* SetPriority)(void*, float);
#else
    typedef void (*SetPriority)(void*, float);
#endif
    const float radius =
        2.0f * static_cast<dx9rend::IRenderer*>(g_renderer)->getRenderView()->getFarPlane();
    const Mat4& transform =
        *static_cast<dx9rend::IRenderer*>(g_renderer)->getRenderView()->getViewMatrixPtr();
    const float* position = &transform.m[12];
    const Vec3 center = {position[0], position[1], position[2]};
    objectManager->getObjectsFromGrid(center, radius, objects, 0);
    for (std::vector<IObject*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
    {
        IBase* priority = (*i)->queryComponent(geom::IID_IGeometry, io::IID_IResourcePriority);
        if (!priority) continue;
        const Vec3& absolute = (*i)->getAbsolutePosition();
        const Vec3 delta = {absolute.x - position[0], absolute.y - position[1], absolute.z - position[2]};
        const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (radius > distance)
            reinterpret_cast<SetPriority>((*reinterpret_cast<void***>(priority))[0x10 / 4])(
                priority, radius / distance);
    }
}
#endif
} } }
