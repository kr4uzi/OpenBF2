// [CHOSEN, not recovered] Shared friend access for direct IObject field reads.
// Keep this definition minimal and add access only when required by an evidenced consumer.

#ifndef GAME_OBJECTS_OBJECTLAYOUTACCESS_H     // [INFERRED] guard style
#define GAME_OBJECTS_OBJECTLAYOUTACCESS_H

#include "IObject.h"

namespace dice {
namespace hfe {
namespace world {
namespace object_detail {

struct LayoutAccess
{
    // +0x04 the flag word. Known bits so far: 0 ([UNRESOLVED] - the physics update loops skip
    // objects carrying it), 8 = "has response physics" (addObject's gate), 24 = "is a root"
    // (getRootParent's early-out).
    static unsigned int& flags(IObject* p) { return p->m_flags; }
    // Retail +0x10: ObjectManager registry keys (ELF8310A80/8310B10/8313820,
    // PE6E0C20/6E0820/6E0900). IDs stay 32-bit; their native member offset does not.
    static unsigned int objectId(const IObject* p) { return p->m_objectId; }
    static IObject* parent(IObject* p) { return p->m_parent; }
    static IObject* sibling(IObject* p) { return p->m_sibling; }
    static Mat4*& interpolationTransformation(IObject* p)
    {
        return p->m_interpolationTransformation;
    }
    static IObject*& cachedRootParent(IObject* p) { return p->m_cachedRootParent; }
    static void*& aiObject(IObject* p) { return p->m_aiObject; }             // +0x34 ArmorGLComp
    static void*& geometry(IObject* p) { return p->m_geometry; }             // +0x38 ArmorGLComp
    static void*& physicsNode(IObject* p) { return p->m_physicsNode; }
    static void*& responsePhysics(IObject* p) { return p->m_responsePhysics; }
    static void*& collisionMesh(IObject* p) { return p->m_collisionMesh; }
    // +0x3C the armor component, read by ObjectSpawner's wreck tests (as IArmorGLComp) - the ELF's
    // recurring `obj+0x3C` walk.
    static IObjectComponent*& armorComponent(IObject* p) { return p->m_armorComponent; }
    // +0x50/+0x5C - VehicleHudComp and WeaponHudComp read these cached component slots directly;
    // neither retail body performs a queryComponent virtual call.
    static IObjectComponent*& postProcessComponent(IObject* p)
    {
        return p->m_postProcessComponent;
    }
    static IObjectComponent*& vehicleHudComponent(IObject* p)
    {
        return p->m_vehicleHudComponent;
    }
    static IObjectComponent*& weaponHudComponent(IObject* p)
    {
        return p->m_weaponHudComponent;
    }
    // BF2.exe 0x5EDF21/0x5EDF2D: VO help reads the cached slots, without a query.
    static IObjectComponent*& helpHudComponent(IObject* p) { return p->m_helpHudComponent; }
    static IObjectComponent*& abilityHudComponent(IObject* p) { return p->m_abilityHudComponent; }
    static IObjectComponent*& radioGLComponent(IObject* p)
    {
        return p->m_radioGLComponent;
    }
    // +0x64 the ability component, GameServer::filterMessage's medic-channel read.
    static IObjectComponent*& abilityGLComponent(IObject* p) { return p->m_abilityGLComponent; }
    // +0x74 CommanderToolState's landing obstruction test reads the cached strategic-object
    // component directly after resolving the hit's root. There is no accessor call in either port.
    static IObjectComponent*& strategicObjectComponent(IObject* p)
    {
        return p->m_strategicObjectComponent;
    }

    // +0x14 the template pointer GameLogic::removeControlPoint reads raw (the field has no
    // accessor; ObjectTemplateManager's three [CHOSEN] stubs document the same gap).
    static const IObjectTemplate* objectTemplate(const IObject* p) { return p->m_template; }

    // +0x78 ControlPoint::init's `byte [this+0x78] = 1`.
    static bool& createdOnLoad(IObject* p) { return p->m_createdOnLoad; }
    // +0x79 ObjectManager's pending-grid-update queue latch.
    static bool& gridRefreshQueued(IObject* p) { return p->m_gridRefreshQueued; }
};

} // namespace object_detail
} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_OBJECTLAYOUTACCESS_H
