// Ground effect bundles: on start, cast a ray along the object's down axis and play a material
// effect where it meets an object or the terrain. Created only by client factories; the
// dedicated ELF carries the class but its template never instantiates it.
#include "GroundEffectBundle.h"
#include "../TemplateLeaves.h"                         // GroundEffectBundleTemplate::m_maxTriggerDistance
#include "../ObjectLayoutAccess.h"
#include "../../IGameLogic.h"                          // gameLogic->playCollisionEffect (+0x110)
#include "../../../Physics/HeightmapCluster.h"         // intersectRayInWorldCoords
#include "../../../Physics/MaterialManager.h"          // getEffectTemplate (+0x5C)
#include "../../../Scene/Object/ObjectManager.h"       // intersectLine (+0x68)
#include "../../../Scene/Object/ObjectPredicator.h"    // ObjectFlagPredicator(0x100)
#include <cmath>

namespace dice { namespace hfe {
extern IGameLogic* gameLogic;                          // ds:0x8ACBFA0 / PE ds:0x99EF80 - Core/Globals.cpp
namespace world {
extern IBase* materialManager;                         // ds:0x8AD2820 / PE ds:0x9C6C88 - Core/Globals.cpp
extern IBase* heightmapCluster;                        // ds:0x8ABAF24 / PE ds:0xA07E28 - Core/Globals.cpp
namespace { typedef object_detail::LayoutAccess L; }

// ELF 8202950 (C2; C1 8202990) / BF2.exe inlined in 0x646A10: the base, then the vtables.
GroundEffectBundle::GroundEffectBundle(const GroundEffectBundleTemplate* objectTemplate)
    : SimpleObject(objectTemplate)
{
}

// ELF 82029D0/8202A00/8202A30 / BF2.exe 0x646B20 (deleting): the vtables, then the base.
GroundEffectBundle::~GroundEffectBundle() {}

// ELF 8202A70 / BF2.exe 0x5D3DB0 (folded `jmp` to SimpleObject::init).
void GroundEffectBundle::init() { SimpleObject::init(); }

// ELF 8202A90 / BF2.exe 0x646D90: message 0 (start) from a non-recursive dispatch plays the
// effect. No base forwarding, nothing for other messages.
void GroundEffectBundle::handleMessage(TemplateMessage message, IPlayer*, bool recursive)
{
    if (!recursive && message == 0)
        calculateCurrentDistanceAndEffectPosition();
}

// ELF 8202AB0 (789 B) / BF2.exe 0x646B60 (552 B). The ray runs from the absolute position along
// the visual transformation's negated up row, scaled by maxTriggerDistance. Objects are tested
// first (no dynamic objects, no terrain in that query); only when no object is hit is the terrain
// asked, two-sided, and a terrain miss ends the call. Within maxTriggerDistance the effect of the
// material cell (template material, hit material), index 0, plays at the hit point with the hit
// normal and the visual transformation's forward row.
void GroundEffectBundle::calculateCurrentDistanceAndEffectPosition()
{
    const GroundEffectBundleTemplate* const objectTemplate =
        static_cast<const GroundEffectBundleTemplate*>(L::objectTemplate(this));   // +0x14
    ObjectFlagPredicator predicator(0x100u);
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitMaterial = -1;
    const float distance = objectTemplate->m_maxTriggerDistance;          // ELF +0x164 / PE +0x210

    const Vec3 down = { 0.0f, -1.0f, 0.0f };
    const Mat4& transform = getVisualAbsoluteTransformation();             // ELF +0x90 / PE +0x8C
    Vec3 direction;
    direction.x = (transform.m[8] * down.z + transform.m[0] * down.x + transform.m[4] * down.y) * distance;
    direction.y = (transform.m[1] * down.x + transform.m[5] * down.y + transform.m[9] * down.z) * distance;
    direction.z = (transform.m[2] * down.x + transform.m[6] * down.y + transform.m[10] * down.z) * distance;

    IObject* hitObject = 0;
    // The Boolean result is ignored; the hit object decides (ELF 0x8202C08 / PE 0x646C68).
    objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitMaterial,
                                 getAbsolutePosition(), direction, &predicator,
                                 false, false, false, true, 0);
    if (!hitObject)
    {
        const Vec3& from = getAbsolutePosition();
        const Vec3 to = { direction.x + from.x, direction.y + from.y, direction.z + from.z };
        float hitFraction;
        unsigned char terrainMaterial;
        if (!static_cast<IHeightmapCluster*>(heightmapCluster)->intersectRayInWorldCoords(
                getAbsolutePosition(), to, hitPoint, hitFraction, hitNormal, terrainMaterial,
                true))
            return;
        hitMaterial = terrainMaterial;      // [CHOSEN, not recovered] see the material lookup below
    }

    const Vec3& position = getAbsolutePosition();
    const float dx = position.x - hitPoint.x;
    const float dy = position.y - hitPoint.y;
    const float dz = position.z - hitPoint.z;
    // Ordered `<=` in both images (PE `fcomp; test ah, 0x41; jp`): a NaN distance plays nothing.
    if (!(std::sqrt(dx * dx + dy * dy + dz * dz) <= objectTemplate->m_maxTriggerDistance))
        return;

    // [CHOSEN, not recovered] Retail fix "GroundEffectBundle plays the effect of the ground
    // material it hits" (docs/retail-fixes.md). Retail asks the (template material, 0) cell and
    // drops the hit material (ELF 0x8202CF6..0x8202D1F / PE 0x646D05..0x646D1D pass material,
    // 0, 0), so only the cell's default effect ever plays. The hit material is the object face's
    // material, or the terrain's; a Soldier hit leaves it at -1, whose missing cell falls back to
    // (template material, 0) inside getEffectTemplate, as does every other missing cell.
    IObjectTemplate* const effectTemplate = static_cast<IObjectTemplate*>(
        static_cast<MaterialManager*>(materialManager)->getEffectTemplate(
            static_cast<unsigned int>(L::objectTemplate(this)->getMaterial()),
            static_cast<unsigned int>(hitMaterial), 0));
    if (!effectTemplate)
        return;

    const Mat4& visual = getVisualAbsoluteTransformation();
    const Vec3 forward = { visual.m[8], visual.m[9], visual.m[10] };
    gameLogic->playCollisionEffect(hitPoint, effectTemplate, hitNormal, forward);   // +0x110
}
} } }
