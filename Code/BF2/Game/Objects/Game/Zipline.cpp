// Path:     [INFERRED] - beside Zipline.h; see its header block for why this file exists at all.
// Contents: recovered Zipline object lifecycle, placement/collision geometry and soldier traversal
// hand-off, cross-checked between the symbolized ELF and Win32 dedicated-server PE.

#include "Zipline.h"

#include <cmath>

// ZiplineTemplate::checkForEntry
#include <vector>
#include "../TemplateLeaves.h"                     // ZiplineTemplate + its own fields
#include "../IPlayerControlObjectTemplate.h"       // container seat-animation setup
#include "../IPlayer.h"                            // getVehicle (+0x48) / getDefaultVehicle (+0xA4)
#include "../IObject.h"                            // getAbsolutePosition (+0x6C)
#include "../ObjectLayoutAccess.h"                 // the raw +0x14 template reads
#include "../ObjectTemplate.h"                     // getClassID on the read template
#include "../../../Core/Ids.h"                     // the four CIDs this switches on
#include "../../../Core/System.h"                  // WorldPref::getWorldTime
#include "../../../Scene/Object/ObjectManager.h"   // objectManager + the grid query
#include "../../../Scene/Object/ObjectPredicator.h"// ObjectFlagPredicator(0x200)
#include "Soldier.h"                               // m_byte399, getLadderId, getGrapplingHookId
#include "Ladder.h"                                // ladderMap + Ladder::detachSoldier
#include "GrapplingHookRope.h"                     // grapplingHookRopeMap + detachSoldier
#include "ZiplineContainer.h"                      // typed traversal endpoints
#include "ZiplineRope.h"
#include "../../../IO/Network/INetworkable.h"
#include "../../../Math/LocalBoundingBox.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Physics/ICollisionMesh.h"
#include "../../../Physics/PhysicsUtil.h"
#include "../GameLogicComp/ArmorGLComp.h"
#include "../Projectiles/GenericProjectile.h"
#include "../../../Scene/Object/ObjectUtil.h"       // getRootParent
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../GameLogic/Player.h"
#include "../../IGame.h"
#include "../../IGameLogic.h"
#include "../../Python/PyRef.h"
#include "../../Python/PythonHost.h"
#include "../../Utility/Vars.h"
#if !DEDICATED
#include "../../../Sound/BfSound.h"
#endif

namespace dice {
namespace hfe {
namespace world {

typedef object_detail::LayoutAccess L;

// ds:0x8AADDE0 - constructed empty by static init, exactly like Ladder.cpp's ladderMap.
std::map<int, Zipline*> ziplineMap;

// ds:0x8ABAF24, owned by Core/Globals.cpp. findStartPos does not null-guard it in retail.
extern IBase* heightmapCluster;

namespace {
// PE .rdata 0x830370..0x83048F; the ELF materializes the same table into BSS 0x8AADE00 during
// static initialization. The order is observable because getNewSoldierPosition accepts the first
// collision-free candidate rather than scoring all candidates.
const Vec3 kZiplineSearchDirections[24] = {
    {  1.0f,  0.0f,  0.0f }, { -1.0f,  0.0f,  0.0f },
    {  0.0f,  0.0f,  1.0f }, {  0.0f,  0.0f, -1.0f },
    {  1.0f,  0.0f,  1.0f }, {  1.0f,  0.0f, -1.0f },
    { -1.0f,  0.0f,  1.0f }, { -1.0f,  0.0f, -1.0f },
    {  1.0f,  1.0f,  0.0f }, { -1.0f,  1.0f,  0.0f },
    {  0.0f,  1.0f,  1.0f }, {  0.0f,  1.0f, -1.0f },
    {  1.0f,  1.0f,  1.0f }, {  1.0f,  1.0f, -1.0f },
    { -1.0f,  1.0f,  1.0f }, { -1.0f,  1.0f, -1.0f },
    {  1.0f, -1.0f,  0.0f }, { -1.0f, -1.0f,  0.0f },
    {  0.0f, -1.0f,  1.0f }, {  0.0f, -1.0f, -1.0f },
    {  1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f, -1.0f },
    { -1.0f, -1.0f,  1.0f }, { -1.0f, -1.0f, -1.0f }
};

void normalizeZiplineVector(Vec3& value)
{
    const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSq - 1.0f) < 1.1920929e-7f)
        return;
    if (std::fabs(lengthSq) < 1.1920929e-7f)
    {
        value.x = value.y = value.z = 0.0f;
        return;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSq);
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
}
} // namespace

// ELF C2 0x81CDE90 / C1 0x81CE040; PE 0x57C7E0. Source-level STL deliberately accounts for the
// ports' different list/iterator widths; both constructors agree on every scalar value and on
// updateFlags(0x1900, 0).
Zipline::Zipline(const ZiplineTemplate* objectTemplate)
    : SimpleObject(objectTemplate)
    , m_refCount(1)
    , m_soldiers()
    , m_soldierIterator()
    , m_template(objectTemplate)
    , m_rope(0)
    , m_networkableId(0)
    , m_boundsDirty(true)
    , m_ziplineHeight(2.0f)
    , m_absoluteTop(0.0f)
    , m_absoluteBottom(0.0f)
    , m_startPoint()
    , m_endPoint()
    , _byte17C(false)
    , _byte17D(false)
    , m_timeout(0.0f)
    , m_riderCount(0)
    , _byte18C(false)
{
    m_startPoint.x = m_startPoint.y = m_startPoint.z = 0.0f;
    m_endPoint.x = m_endPoint.y = m_endPoint.z = 0.0f;
    updateFlags(0x1900u, 0u);
}

// /1B0/220; PE counterpart. Erase is unconditional, including the zero-id case;
// std::list performs the inlined clear/sentinel release emitted by each platform's STL.
Zipline::~Zipline()
{
    ziplineMap.erase(m_networkableId);
}

void Zipline::addRef() { ++m_refCount; }
int Zipline::getRef() { return m_refCount; }
int Zipline::release()
{
    const int left = --m_refCount;
    if (left == 0)
        delete this;
    return left;
}

// exactly IBase/IObject, with no IConsoleSaveable answer.
void* Zipline::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObject)
        return const_cast<Zipline*>(this);
    return 0;
}

// . The placement matrix's second row is content data: it carries the
// far endpoint rather than an orientation axis. The body saves that row, rewrites the Zipline's
// frame, and (for a non-trivial endpoint) builds a separately scaled ZiplineRope at the midpoint.
// The server-only deployment relocation and ScriptEvent 9 retain their original ordering.
void Zipline::init()
{
    IPlayer* player = m_template->m_player;
    Mat4& transformation = const_cast<Mat4&>(getAbsoluteTransformation());

    if (gameLogic->isServer() && player)
    {
        IObject* soldierObject = player->getVehicle();
        const Vec3 soldierPosition = soldierObject->getAbsolutePosition();
        transformation.m[12] = soldierPosition.x;
        transformation.m[13] = soldierPosition.y - 1.0f;
        transformation.m[14] = soldierPosition.z;
        soldierObject->setAbsolutePosition(
            getNewSoldierPosition(soldierPosition, soldierPosition));
        _dword180 = static_cast<Soldier*>(soldierObject)->getZiplineId();
    }

    const Vec3 endpoint = {
        transformation.m[4], transformation.m[5], transformation.m[6]
    };
    transformation.m[4] = 0.0f;
    transformation.m[5] = 1.0f;
    transformation.m[6] = 0.0f;

    Vec3 direction = {
        endpoint.x - transformation.m[12],
        endpoint.y - transformation.m[13],
        endpoint.z - transformation.m[14]
    };
    normalizeZiplineVector(direction);
    transformation.m[8] = direction.x;
    transformation.m[9] = direction.y;
    transformation.m[10] = direction.z;
    reinterpret_cast<Vec3*>(&transformation.m[0])->makeOrthonormalBasis(
        *reinterpret_cast<Vec3*>(&transformation.m[4]),
        *reinterpret_cast<Vec3*>(&transformation.m[8]));
    setAbsoluteTransformation(transformation);

    // Retail performs this second virtual snapshot even when the endpoint fails the length gate.
    Mat4 ropeTransformation = getAbsoluteTransformation();
    const float endpointLength = std::sqrt(endpoint.x * endpoint.x +
                                           endpoint.y * endpoint.y +
                                           endpoint.z * endpoint.z);
    if (endpointLength > 0.5f)
    {
        if (gameLogic->isServer())
        {
            python::PyRef args = python::buildArgs(player);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(9), args.get());
        }

        _byte17D = true;
        m_endPoint = endpoint;

#if !DEDICATED
        // BF2.exe 0x005DD523..0x005DD56C: the facade owns the sound; play at the
        // saved endpoint before building the rope, without acquiring a reference.
        ISound* const sound = g_bfSound->getSound(std::string("S_ZiplineAttached"));
        if (sound)
            sound->playAt(endpoint);
#endif

        Vec3 ropeDirection = {
            m_endPoint.x - ropeTransformation.m[12],
            m_endPoint.y - ropeTransformation.m[13],
            m_endPoint.z - ropeTransformation.m[14]
        };
        normalizeZiplineVector(ropeDirection);
        ropeTransformation.m[8] = ropeDirection.x;
        ropeTransformation.m[9] = ropeDirection.y;
        ropeTransformation.m[10] = ropeDirection.z;
        reinterpret_cast<Vec3*>(&ropeTransformation.m[0])->makeOrthonormalBasis(
            *reinterpret_cast<Vec3*>(&ropeTransformation.m[4]),
            *reinterpret_cast<Vec3*>(&ropeTransformation.m[8]));

        const Vec3 oldThirdRow = {
            ropeTransformation.m[8], ropeTransformation.m[9], ropeTransformation.m[10]
        };
        ropeTransformation.m[8] = ropeTransformation.m[0];
        ropeTransformation.m[9] = ropeTransformation.m[1];
        ropeTransformation.m[10] = ropeTransformation.m[2];
        ropeTransformation.m[0] = -oldThirdRow.x;
        ropeTransformation.m[1] = -oldThirdRow.y;
        ropeTransformation.m[2] = -oldThirdRow.z;

        m_startPoint.x = ropeTransformation.m[12];
        m_startPoint.y = ropeTransformation.m[13] + 1.6f;
        m_startPoint.z = ropeTransformation.m[14];

        Vec3 span = {
            m_endPoint.x - m_startPoint.x,
            m_endPoint.y - m_startPoint.y,
            m_endPoint.z - m_startPoint.z
        };
        const float spanLength = std::sqrt(span.x * span.x + span.y * span.y + span.z * span.z);
        normalizeZiplineVector(span);
        ropeTransformation.m[0] = span.x;
        ropeTransformation.m[1] = span.y;
        ropeTransformation.m[2] = span.z;
        const float halfLength = spanLength * 0.5f;
        ropeTransformation.m[12] = m_startPoint.x + span.x * halfLength;
        ropeTransformation.m[13] = m_startPoint.y + span.y * halfLength;
        ropeTransformation.m[14] = m_startPoint.z + span.z * halfLength;

        const Vec3 scaleSpan = {
            m_endPoint.x - m_startPoint.x,
            m_endPoint.y - m_startPoint.y,
            m_endPoint.z - m_startPoint.z
        };
        const float scaleLength = std::sqrt(scaleSpan.x * scaleSpan.x +
                                            scaleSpan.y * scaleSpan.y +
                                            scaleSpan.z * scaleSpan.z);

        ZiplineRopeTemplate* ropeTemplate = static_cast<ZiplineRopeTemplate*>(
            objectTemplateManager->getTemplate(std::string("ZiplineRope")));
        Vec3 scaledAxis = {
            ropeTransformation.m[0], ropeTransformation.m[1], ropeTransformation.m[2]
        };
        normalizeZiplineVector(scaledAxis);
        const float ropeScale = scaleLength * ropeTemplate->m_lengthScale;
        ropeTransformation.m[0] = scaledAxis.x * ropeScale;
        ropeTransformation.m[1] = scaledAxis.y * ropeScale;
        ropeTransformation.m[2] = scaledAxis.z * ropeScale;

        ZiplineRope* rope = static_cast<ZiplineRope*>(ropeTemplate->createObject());
        if (rope)
        {
            rope->init();
            objectManager->setActiveObject(rope);
            m_rope = rope;
            rope->setAbsoluteTransformation(ropeTransformation);
            rope->m_boundingRadiusModifier = ropeScale;
        }
    }

    SimpleObject::init();
}

// . The input flag is deliberately ignored: Zipline always tears its
// owned rope and base object down with force=true. detachSoldier returns a real bool (not encoded
// by the Itanium name); this caller consumes it to decide whether erase already invalidated the
// saved list iterator. The child collision-component query repeats the same IID twice, as shipped.
void Zipline::destroy(bool /*force*/)
{
    if (m_armorComponent)
        static_cast<IArmorGLComp*>(m_armorComponent)->ClearEffects();

    if (m_rope)
    {
        objectManager->destroyObject(m_rope);
        m_rope = 0;
    }

    m_soldierIterator = m_soldiers.begin();
    while (m_soldierIterator != m_soldiers.end())
    {
        ISoldier* soldier = *m_soldierIterator;
        if (detachSoldier(soldier))
            m_soldierIterator = m_soldiers.begin();
        else
            ++m_soldierIterator;
    }

    while (getChild())
    {
        IObject* child = getChild();
        IProjectileCollisionComp* collision = static_cast<IProjectileCollisionComp*>(
            child->queryComponent(IID_IProjectileCollisionComp,
                                  IID_IProjectileCollisionComp));
        collision->resetProjectile(false);
    }

    SimpleObject::destroy(true);
}

// Reads radii directly from the required template.
float Zipline::getEntryRadius() const          { return m_template->m_entryRadius; }          // +0x170
float Zipline::getCollisionStartRadius() const { return m_template->m_collisionStartRadius; }  // +0x174

// . This is a deliberately sparse wire-frame box, not a pill or solid
// overlap test: nine upward rays are followed by nine radial rays at each of five height slices.
// Direction entries are not normalized, and the first nine entries of the shared table are used
// verbatim (including the ninth entry's positive Y component).
bool Zipline::wireBoxCheckForCollision(const Vec3& position, const float& radius,
                                       const float& up)
{
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart = 0;
    ObjectFlagPredicator predicator(0x100);

    const Vec3 vertical = { 0.0f, up, 0.0f };
    for (int directionIndex = 0; directionIndex <= 8; ++directionIndex)
    {
        const Vec3& basis = kZiplineSearchDirections[directionIndex];
        Vec3 from = {
            position.x + basis.x * radius,
            position.y + basis.y * radius,
            position.z + basis.z * radius
        };
        if (objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                         from, vertical, &predicator,
                                         false, false, true, true, 0))
            return true;
    }

    for (int heightIndex = 0; heightIndex <= 4; ++heightIndex)
    {
        Vec3 from = {
            position.x,
            position.y + static_cast<float>(heightIndex) * 0.25f * up,
            position.z
        };
        for (int directionIndex = 0; directionIndex <= 8; ++directionIndex)
        {
            const Vec3& basis = kZiplineSearchDirections[directionIndex];
            Vec3 direction = {
                basis.x * radius,
                basis.y * radius,
                basis.z * radius
            };
            if (objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                             from, direction, &predicator,
                                             false, false, true, true, 0))
                return true;
        }
    }
    return false;
}

// . The two-pass search is intentionally first-match: two downward
// shelves (0, -0.5) demand an upward-facing floor, then two upward shelves (0, +0.5) omit that
// floor test. Each shelf walks four radii and all 24 directions in table order. A nearby Ladder
// rejects a candidate using X/Z distance only—the Y subtraction in both ports is literally zero.
Vec3 Zipline::getNewSoldierPosition(const Vec3& start, const Vec3& fallback)
{
    static const float radius = 0.3f;  // ; ELF function-local static
    static const float up = 1.5f;      // ; ELF function-local static

    Vec3 result = fallback;
    Vec3 shelf = start;
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart = 0;
    ObjectFlagPredicator predicator(0x100);

    for (int shelfIndex = 0; shelfIndex >= -1; --shelfIndex)
    {
        const Vec3 shelfStep = {
            0.0f, static_cast<float>(shelfIndex) * 0.5f, 0.0f
        };
        if (objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                         shelf, shelfStep, &predicator,
                                         false, false, true, true, 0))
            break;
        shelf.y += shelfStep.y;

        const Vec3 floorProbe = {
            0.0f, static_cast<float>(shelfIndex) - 1.0f, 0.0f
        };
        for (int radiusIndex = 0; radiusIndex <= 3; ++radiusIndex)
        {
            const float scale = static_cast<float>(radiusIndex + 1) * 0.4f;
            for (int directionIndex = 0; directionIndex <= 23; ++directionIndex)
            {
                const Vec3& basis = kZiplineSearchDirections[directionIndex];
                Vec3 direction = {
                    basis.x * scale, basis.y * scale, basis.z * scale
                };
                if (objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                                 shelf, direction, &predicator,
                                                 false, false, true, true, 0))
                    continue;

                Vec3 candidate = {
                    shelf.x + direction.x,
                    shelf.y + direction.y,
                    shelf.z + direction.z
                };
                if (!objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                                  candidate, floorProbe, &predicator,
                                                  false, false, true, true, 0) ||
                    hitNormal.y <= 0.5f ||
                    wireBoxCheckForCollision(candidate, radius, up))
                    continue;

                std::vector<IObject*> objects;
                objectManager->getNoCollisionObjectsFromGrid(candidate, 0.6f,
                                                              objects, &predicator);
                bool besideLadder = false;
                for (std::vector<IObject*>::iterator it = objects.begin();
                     it != objects.end(); ++it)
                {
                    if ((*it)->getTemplate()->getClassID() != CID_LadderTemplate)
                        continue;
                    const Vec3& ladderPosition = (*it)->getAbsolutePosition();
                    const float dx = candidate.x - ladderPosition.x;
                    const float dz = candidate.z - ladderPosition.z;
                    if (std::sqrt(dx * dx + dz * dz) < 0.6f)
                    {
                        besideLadder = true;
                        break;
                    }
                }
                if (!besideLadder)
                {
                    result = candidate;
                    result.y += 0.9f;
                    return result;
                }
            }
        }
    }

    shelf = start;
    for (int shelfIndex = 0; shelfIndex <= 1; ++shelfIndex)
    {
        const Vec3 shelfStep = {
            0.0f, static_cast<float>(shelfIndex) * 0.5f, 0.0f
        };
        if (objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                         shelf, shelfStep, &predicator,
                                         false, false, true, true, 0))
            break;
        shelf.y += shelfStep.y;

        for (int radiusIndex = 0; radiusIndex <= 3; ++radiusIndex)
        {
            const float scale = static_cast<float>(radiusIndex + 1) * 0.4f;
            for (int directionIndex = 0; directionIndex <= 23; ++directionIndex)
            {
                const Vec3& basis = kZiplineSearchDirections[directionIndex];
                Vec3 direction = {
                    basis.x * scale, basis.y * scale, basis.z * scale
                };
                if (objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                                 shelf, direction, &predicator,
                                                 false, false, true, true, 0))
                    continue;

                Vec3 candidate = {
                    shelf.x + direction.x,
                    shelf.y + direction.y,
                    shelf.z + direction.z
                };
                if (wireBoxCheckForCollision(candidate, radius, up))
                    continue;

                std::vector<IObject*> objects;
                objectManager->getNoCollisionObjectsFromGrid(candidate, 0.6f,
                                                              objects, &predicator);
                bool besideLadder = false;
                for (std::vector<IObject*>::iterator it = objects.begin();
                     it != objects.end(); ++it)
                {
                    if ((*it)->getTemplate()->getClassID() != CID_LadderTemplate)
                        continue;
                    const Vec3& ladderPosition = (*it)->getAbsolutePosition();
                    const float dx = candidate.x - ladderPosition.x;
                    const float dz = candidate.z - ladderPosition.z;
                    if (std::sqrt(dx * dx + dz * dz) < 0.6f)
                    {
                        besideLadder = true;
                        break;
                    }
                }
                if (!besideLadder)
                {
                    result = candidate;
                    result.y += 0.9f;
                    return result;
                }
            }
        }
    }
    return result;
}

// one unchecked singleton virtual call.
void Zipline::destroyZipline()
{
    objectManager->destroyObject(this);
}

// . A collision can attach only after the line owns a network id and
// either its first rider latch is set or one second has elapsed. The candidate is the point at the
// soldier's radial distance along the line; the signed 0.3-metre vertical gate selects the travel
// direction. Repeated occupying-player calls, the old-id map operator[] and the two-axis reversal
// are all observable shipped behaviour.
void Zipline::attachSoldier(ISoldier* soldier)
{
    if (!soldier->getOccupingPlayer())
        return;
    if (!_byte17D && gameLogic->isServer())
        return;
    if (!m_networkableId)
        return;
    if (!_byte18C && m_timeout < 1.0f)
        return;

    _byte18C = true;
    Soldier* concreteSoldier = static_cast<Soldier*>(soldier);
    const Vec3 soldierPosition = concreteSoldier->getAbsolutePosition();

    const Vec3& objectPosition = getAbsolutePosition();
    const float originX = objectPosition.x - soldierPosition.x;
    const float originY = objectPosition.y - soldierPosition.y;
    const float originZ = objectPosition.z - soldierPosition.z;
    const float endpointX = m_endPoint.x - soldierPosition.x;
    const float endpointY = m_endPoint.y - soldierPosition.y;
    const float endpointZ = m_endPoint.z - soldierPosition.z;
    _byte17C = originX * originX + originY * originY + originZ * originZ >
               endpointX * endpointX + endpointY * endpointY + endpointZ * endpointZ;

    const unsigned int oldId = soldier->getZiplineId();
    const Vec3& lineOrigin = getAbsolutePosition();
    const float lineStartX = lineOrigin.x;
    const float lineHeight = getZiplineHeight();
    const float lineStartZ = lineOrigin.z;
    const float lineStartY = lineOrigin.y + lineHeight * 0.94999999f;
    Vec3 lineStart = {
        lineStartX, lineStartY, lineStartZ
    };
    const float radialX = lineStart.x - soldierPosition.x;
    const float radialY = lineStart.y - soldierPosition.y;
    const float radialZ = lineStart.z - soldierPosition.z;
    const float radialDistance = std::sqrt(
        radialX * radialX + radialY * radialY + radialZ * radialZ);

    Vec3 direction = {
        m_endPoint.x - lineStart.x,
        m_endPoint.y - lineStart.y,
        m_endPoint.z - lineStart.z
    };
    normalizeZiplineVector(direction);
    Vec3 candidate = {
        lineStart.x + direction.x * radialDistance,
        lineStart.y + direction.y * radialDistance,
        lineStart.z + direction.z * radialDistance
    };

    const float endpointDeltaY = candidate.y - m_endPoint.y;
    if (std::fabs(endpointDeltaY) < 0.30000001f)
        return;
    if ((_byte17C && endpointDeltaY > 0.0f) || (!_byte17C && endpointDeltaY < 0.0f))
        return;
    if (oldId == static_cast<unsigned int>(m_networkableId))
        return;

    if (oldId)
        ziplineMap[static_cast<int>(oldId)]->detachSoldier(soldier); // no null guard in retail

    IObjectTemplate* objectTemplate =
        objectTemplateManager->getTemplate(std::string("ZiplineContainer"));
    IPlayerControlObjectTemplate* containerTemplate =
        static_cast<IPlayerControlObjectTemplate*>(
            objectTemplate->queryInterface(IID_IPlayerControlObjectTemplate));
    if (containerTemplate && !m_template->_string168.empty())
        containerTemplate->setSeatAnimationSystem(m_template->_string168);

    ZiplineContainer* container =
        static_cast<ZiplineContainer*>(objectTemplate->createObject());
    Mat4 transformation = getAbsoluteTransformation();
    if (_byte17C)
    {
        transformation.m[8] = -transformation.m[8];
        transformation.m[9] = -transformation.m[9];
        transformation.m[10] = -transformation.m[10];
        transformation.m[0] = -transformation.m[0];
        transformation.m[1] = -transformation.m[1];
        transformation.m[2] = -transformation.m[2];
    }
    container->setAbsoluteTransformation(transformation);
    container->m_zipline = this;
    ++m_riderCount;

    if (!game->queryInterface(IID_IGameClient) ||
        !soldier->getOccupingPlayer()->getIsRemote())
    {
        disablePhysics(container, false);
    }
    container->init();
    container->updateFlags(0x100u, 0u);
    container->_enter(soldier->getOccupingPlayer());
    container->m_traversalStart = candidate;
    container->m_traversalEnd = _byte17C ? lineStart : m_endPoint;
    soldier->setZiplineId(static_cast<unsigned int>(m_networkableId));
    m_soldiers.insert(m_soldiers.begin(), soldier);
}

// . The list erase is intentionally unchecked in the ELF source-level
// STL path. Projection is clamped only at the far end (negative distances remain negative). The
// downward ray starts from that projection and its miss fallback is the ray endpoint 1.5 metres
// below it; the call uses the unusual true,false,true,true flag tuple. The Player timestamp is the
// current world time with no grappling-hook-style subtraction.
bool Zipline::detachSoldier(ISoldier* soldier)
{
    if (!soldier)
        return false;

    _byte17C = false;
    if (soldier->getZiplineId() != static_cast<unsigned int>(m_networkableId))
        return false;

    std::list<ISoldier*>::iterator found = m_soldiers.begin();
    while (found != m_soldiers.end() && *found != soldier)
        ++found;
    m_soldierIterator = found;
    m_soldiers.erase(found);

    Vec3 direction = {
        m_endPoint.x - m_startPoint.x,
        m_endPoint.y - m_startPoint.y,
        m_endPoint.z - m_startPoint.z
    };
    const float lineLength = std::sqrt(
        direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    const Vec3& soldierPosition = static_cast<Soldier*>(soldier)->getAbsolutePosition();
    const Vec3 relative = {
        soldierPosition.x - m_startPoint.x,
        soldierPosition.y - m_startPoint.y,
        soldierPosition.z - m_startPoint.z
    };
    normalizeZiplineVector(direction);
    float projection = direction.x * relative.x +
                       direction.y * relative.y +
                       direction.z * relative.z;
    if (projection > lineLength)
        projection = lineLength;

    Vec3 candidate = {
        m_startPoint.x + direction.x * projection,
        m_startPoint.y + direction.y * projection,
        m_startPoint.z + direction.z * projection
    };
    const Vec3 down = { 0.0f, -1.5f, 0.0f };
    Vec3 exitPoint = {
        candidate.x + down.x,
        candidate.y + down.y,
        candidate.z + down.z
    };
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart; // deliberately uninitialized in both shipped ports
    ObjectFlagPredicator predicator(0x100u);
    if (objectManager->intersectLine(
            hitObject, hitPoint, hitNormal, hitPart, candidate, down, &predicator,
            true, false, true, true, 0))
    {
        exitPoint = hitPoint;
    }

    IObject* soldierObject =
        static_cast<IObject*>(soldier->queryInterface(IID_IObject));
    ZiplineContainer* container =
        static_cast<ZiplineContainer*>(soldierObject->getParent());
    container->setExitPosition(
        getNewSoldierPosition(exitPoint, container->m_lastSoldierPosition));

    m_riderCount = m_riderCount < 1 ? 0 : m_riderCount - 1;
    container->_exit();
    container->destroy(true);
    if (!game->queryInterface(IID_IGameClient) ||
        !soldier->getOccupingPlayer()->getIsRemote())
    {
        disablePhysics(soldierObject, false);
    }
    soldier->setZiplineId(0);

    IPlayer* occupyingPlayer = soldier->getOccupingPlayer();
    if (occupyingPlayer)
    {
        static_cast<::dice::hfe::Player*>(occupyingPlayer)->m_worldTime =
            static_cast<float>(WorldPref::getWorldTime());
    }
    return true;
}

// . The soldier's root is the ZiplineContainer; its endpoints are adjusted down by
// 0.07 before both the line length and travelled distance are measured. There is no zero-length
// guard in retail.
float Zipline::getCurrentState(ISoldier* soldier)
{
    IObject* object = static_cast<IObject*>(soldier->queryInterface(IID_IObject));
    IObject* root = getRootParent(object);
    if (!root)
        return 0.0f;

    ZiplineContainer* container = static_cast<ZiplineContainer*>(root);
    Vec3 start = container->m_traversalStart;
    Vec3 end = container->m_traversalEnd;
    start.y -= 0.07f;
    end.y -= 0.07f;

    const float lineX = end.x - start.x;
    const float lineY = end.y - start.y;
    const float lineZ = end.z - start.z;
    const float length = std::sqrt(lineX * lineX + lineY * lineY + lineZ * lineZ);

    const Vec3& position = root->getAbsolutePosition();
    const float dx = position.x - start.x;
    const float dy = position.y - start.y;
    const float dz = position.z - start.z;
    const float state = std::sqrt(dx * dx + dy * dy + dz * dz) / length;
    if (state < 0.0f)
        return 0.0f;
    if (state > 1.0f)
        return 1.0f;
    return state;
}

// . Same endpoint adjustment; the direction uses the engine's conditional
// normalisation and the requested state is deliberately not clamped here.
void Zipline::setCurrentState(ISoldier* soldier, float state)
{
    IObject* object = static_cast<IObject*>(soldier->queryInterface(IID_IObject));
    IObject* root = getRootParent(object);
    if (!root)
        return;

    ZiplineContainer* container = static_cast<ZiplineContainer*>(root);
    Vec3 start = container->m_traversalStart;
    Vec3 end = container->m_traversalEnd;
    start.y -= 0.07f;
    end.y -= 0.07f;

    Vec3 direction;
    direction.x = end.x - start.x;
    direction.y = end.y - start.y;
    direction.z = end.z - start.z;
    const float lengthSq = direction.x * direction.x + direction.y * direction.y +
                           direction.z * direction.z;
    const float distance = std::sqrt(lengthSq) * state;

    if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-7f)
    {
        if (std::fabs(lengthSq) >= 1.1920929e-7f)
        {
            const float inverseLength = 1.0f / std::sqrt(lengthSq);
            direction.x *= inverseLength;
            direction.y *= inverseLength;
            direction.z *= inverseLength;
        }
        else
        {
            direction.x = direction.y = direction.z = 0.0f;
        }
    }

    Vec3 position;
    position.x = start.x + direction.x * distance;
    position.y = start.y + direction.y * distance;
    position.z = start.z + direction.z * distance;
    root->setAbsolutePosition(position);
}

// . A soldier is queried twice before success; all other objects take the base path.
bool Zipline::handleCollision(IObject* object, const Vec3& a, const Vec3& b, const Vec3& c,
                              int d, int e, CollisionFaceInfo* f, CollisionFaceInfo* g)
{
    if (!object->queryInterface(IID_ISoldier))
        return SimpleObject::handleCollision(object, a, b, c, d, e, f, g);

    ISoldier* soldier = static_cast<ISoldier*>(object->queryInterface(IID_ISoldier));
    Soldier* concrete = soldier ? static_cast<Soldier*>(soldier) : 0;
    if (!concrete)
        return SimpleObject::handleCollision(object, a, b, c, d, e, f, g);
    return true;
}

// exact base forward.
void Zipline::handleVisualUpdate(float deltaTime, float interpolation)
{
    SimpleObject::handleVisualUpdate(deltaTime, interpolation);
}

// . The timer is reset while occupied; after expiry, an empty zipline queues itself
// for destruction and zeroes the accumulator.
void Zipline::updateTimeOut(float deltaTime)
{
    if (m_timeout < m_template->_float17C)
    {
        if (m_riderCount)
            m_timeout = 0.0f;
        else
            m_timeout += deltaTime;
    }
    else if (m_riderCount <= 0)
    {
        destroyZipline();
        m_timeout = 0.0f;
    }
}

// . The id insert occurs only once; insert_unique does not replace an
// existing key. The orientation edits the matrix returned by getAbsoluteTransformation in place,
// as the raw body does, before handing the same matrix back through the setter.
void Zipline::handleUpdate(float deltaTime, unsigned int flags)
{
    updateTimeOut(deltaTime);

    if (!m_networkableId)
    {
        io::INetworkable* networkable = getNetworkable();
        m_networkableId = networkable ? networkable->m_networkableId : 0;
        ziplineMap.insert(std::make_pair(m_networkableId, this));
    }

    Mat4& transform = const_cast<Mat4&>(getAbsoluteTransformation());
    transform.m[4] = 0.0f;
    transform.m[5] = 1.0f;
    transform.m[6] = 0.0f;

    Vec3 direction;
    direction.x = m_endPoint.x - transform.m[12];
    direction.y = m_endPoint.y - transform.m[13];
    direction.z = m_endPoint.z - transform.m[14];
    const float lengthSq = direction.x * direction.x + direction.y * direction.y +
                           direction.z * direction.z;
    if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-7f)
    {
        if (std::fabs(lengthSq) >= 1.1920929e-7f)
        {
            const float inverseLength = 1.0f / std::sqrt(lengthSq);
            direction.x *= inverseLength;
            direction.y *= inverseLength;
            direction.z *= inverseLength;
        }
        else
        {
            direction.x = direction.y = direction.z = 0.0f;
        }
    }
    transform.m[8] = direction.x;
    transform.m[9] = direction.y;
    transform.m[10] = direction.z;

    reinterpret_cast<Vec3*>(&transform.m[0])->makeOrthonormalBasis(
        *reinterpret_cast<Vec3*>(&transform.m[4]),
        *reinterpret_cast<Vec3*>(&transform.m[8]));
    setAbsoluteTransformation(transform);
    SimpleObject::handleUpdate(deltaTime, flags);
}

float Zipline::getZiplineHeight()
{
    if (m_boundsDirty)
        recalcBounds();
    return m_ziplineHeight;
}

float Zipline::getAbsoluteTop()
{
    if (m_boundsDirty)
        recalcBounds();
    return m_absoluteTop;
}

float Zipline::getAbsoluteBottom()
{
    if (m_boundsDirty)
        recalcBounds();
    return m_absoluteBottom;
}

// LOD 0 only, no fallback values when the mesh/bounds are absent, and the dirty
// byte is cleared on every path.
void Zipline::recalcBounds()
{
    ICollisionMesh* mesh = static_cast<ICollisionMesh*>(m_collisionMesh);
    const LocalBoundingBox* bounds = 0;
    if (mesh && mesh->hasLod(0))
        bounds = static_cast<const LocalBoundingBox*>(mesh->getBoundingBox(0));

    if (bounds)
    {
        m_ziplineHeight = bounds->m_max.y - bounds->m_min.y;
        m_absoluteTop = bounds->m_max.y + getAbsolutePosition().y;
        m_absoluteBottom = bounds->m_min.y + getAbsolutePosition().y;
    }
    m_boundsDirty = false;
}

// . Despite the names, this zipline routine registers/reads the shared ladder Vars
// independently on every call, just as shipped.
float Zipline::findStartPos(float lowerBound)
{
    const Vec3 position = getAbsolutePosition();
    const float topLimit = getAbsoluteTop() - getAbsolutePosition().y -
        *Vars::getFloat("ladder-soldier-start-getoff", 2.1f);
    float result = getAbsoluteBottom() - getAbsolutePosition().y;
    const float terrain = static_cast<IHeightmapCluster*>(heightmapCluster)
        ->getHeightInWorldCoords(position.x, position.z);

    if (topLimit >= result)
    {
        for (;;)
        {
            if (terrain <= position.y + result && result > lowerBound)
                return result;
            result += *Vars::getFloat("ladder-step-size", 0.3f);
            if (topLimit < result)
                break;
        }
    }

    return getAbsoluteTop() - getAbsolutePosition().y -
        *Vars::getFloat("ladder-offset-top", 0.43f);
}


// Finds a nearby usable zipline and attaches player after detaching any ladder or rope.
// Entry radius is compared with squared distance; out-of-segment candidates retain their original sentinel ordering.
void ZiplineTemplate::checkForEntry(IPlayer* player) const
{
    if (!player)
        return;
    if (!player->getVehicle())                                             // IPlayer +0x48
        return;
    if (L::objectTemplate(player->getVehicle())->getClassID()              // raw +0x14
            == CID_ZiplineContainerTemplate)
        return;                                                            // already riding one

    // the one grid sweep
    const float searchRadius = m_searchRadius;                             // +0x16C 50.0f
    std::vector<IObject*> candidates;
    ObjectFlagPredicator predicator(0x200);

    std::vector<IObject*>& scratch = objectManager->getObjectTempVector();  // +0xA8
    scratch.erase(scratch.begin(), scratch.end());                          // retail's clear()
    objectManager->getObjectsFromGrid(player->getVehicle()->getAbsolutePosition(),   // +0x48/+0x6C
                                      searchRadius, scratch, &predicator);           // +0x40
    for (unsigned int i = 0; i < scratch.size(); ++i)
        candidates.push_back(scratch[i]);

    bool     found    = false;
    Zipline* best     = 0;
    float    bestDist = 3.4028235e38f;                                      // flt_888FFE8 = FLT_MAX
    IObject* soldier  = player->getVehicle();                               // +0x48

    // a raw +0x399 read, no accessor in either port.
    if (!static_cast<Soldier*>(soldier)->m_byte399)
    {
        for (unsigned int c = 0; c < candidates.size(); ++c)
        {
            IObject* const object = candidates[c];
            if (L::objectTemplate(object)->getClassID() != CID_ZiplineTemplate)   // raw +0x14
                continue;

            Zipline* const zipline = static_cast<Zipline*>(object);
            if (!zipline || !soldier)
                continue;
            found = true;

            // (a) how far to the zipline object's own origin
            const Vec3& ziplinePos = zipline->getAbsolutePosition();        // +0x6C
            const Vec3& soldierPos = soldier->getAbsolutePosition();        // +0x6C
            Vec3 toOrigin;
            toOrigin.x = ziplinePos.x - soldierPos.x;
            toOrigin.y = ziplinePos.y - soldierPos.y;
            toOrigin.z = ziplinePos.z - soldierPos.z;
            const float originDistSq = toOrigin.x * toOrigin.x +
                                       toOrigin.y * toOrigin.y +
                                       toOrigin.z * toOrigin.z;

            // (b) how far to the closest point on the line
            // both positions are fetched a second time here, as compiled.
            const Vec3& lineStart = zipline->getAbsolutePosition();         // +0x6C again
            const Vec3& from      = soldier->getAbsolutePosition();         // +0x6C again
            Vec3 segment;
            segment.x = zipline->m_endPoint.x - lineStart.x;                // +0x170
            segment.y = zipline->m_endPoint.y - lineStart.y;
            segment.z = zipline->m_endPoint.z - lineStart.z;
            const float segmentLenSq = segment.x * segment.x +
                                       segment.y * segment.y +
                                       segment.z * segment.z;

            float segmentDistSq = -1.0f;                                    // flt_888B590
            if (segmentLenSq >= 1.1920929e-7f)                              // flt_8888D4C
            {
                const float t = ((from.x - lineStart.x) * (zipline->m_endPoint.x - lineStart.x) +
                                 (from.y - lineStart.y) * (zipline->m_endPoint.y - lineStart.y) +
                                 (from.z - lineStart.z) * (zipline->m_endPoint.z - lineStart.z))
                                / segmentLenSq;
                if (t >= 0.0f && t <= 1.0f)
                {
                    Vec3 closest;
                    closest.x = lineStart.x + (zipline->m_endPoint.x - lineStart.x) * t;
                    closest.y = lineStart.y + (zipline->m_endPoint.y - lineStart.y) * t;
                    closest.z = lineStart.z + (zipline->m_endPoint.z - lineStart.z) * t;
                    Vec3 toLine;
                    toLine.x = from.x - closest.x;
                    toLine.y = from.y - closest.y;
                    toLine.z = from.z - closest.z;
                    segmentDistSq = toLine.x * toLine.x +
                                    toLine.y * toLine.y +
                                    toLine.z * toLine.z;
                }
                // else: outside the segment - the -1.0f sentinel stands
            }
            // else: degenerate line - the -1.0f sentinel stands

            // (c) score it
            // squared distances against an unsquared radius - see the note above.
            if (segmentDistSq > m_entryRadius || 0.0f > segmentDistSq)      // +0x170
            {
                // the segment answer is unusable (too far, or the sentinel): fall back to requiring
                // the origin to be in range.
                if (originDistSq > m_entryRadius)
                    continue;
            }
            float score = originDistSq;
            if (originDistSq > segmentDistSq)
                score = segmentDistSq;                                      // can be -1.0f
            if (bestDist > score)
            {
                bestDist = score;
                best     = zipline;
            }
        }

        if (found && best && soldier)
        {
            // the line must be long enough
            // this one IS squared before comparing, unlike m_entryRadius above.
            Vec3 span;
            span.x = best->m_endPoint.x - best->m_startPoint.x;             // +0x170 +0x164
            span.y = best->m_endPoint.y - best->m_startPoint.y;
            span.z = best->m_endPoint.z - best->m_startPoint.z;
            if (m_minZiplineLength * m_minZiplineLength <=                  // +0x184
                    span.x * span.x + span.y * span.y + span.z * span.z)
            {
                // detach from whatever the soldier is on right now
                // the ids come from the default vehicle (the body), the class test from the
                // current one (whatever is being ridden). getDefaultVehicle() is called twice.
                if (player->getVehicle() != player->getDefaultVehicle())    // +0x48 +0xA4
                {
                    IObject* const body = player->getDefaultVehicle();      // +0xA4 again
                    const unsigned int currentClass =
                        L::objectTemplate(soldier)->getClassID();           // raw +0x14

                    if (currentClass == CID_LadderContainerTemplate)
                    {
                        const unsigned int ladderId =
                            static_cast<Soldier*>(body)->getLadderId();     // +0x2CC
                        if (ladderId)
                        {
                            // operator[] - it inserts a null entry for an unknown id, exactly as
                            // the ELF's lower_bound + insert_unique pair does.
                            Ladder* const ladder = ladderMap[static_cast<int>(ladderId)];
                            if (ladder)
                                ladder->detachSoldier(
                                    static_cast<ISoldier*>(static_cast<Soldier*>(body)));
                        }
                    }
                    else if (currentClass == CID_GrapplingHookRopeContainerTemplate)
                    {
                        const unsigned int ropeId =
                            static_cast<Soldier*>(body)->getGrapplingHookId();   // +0x2DC
                        if (ropeId)
                        {
                            GrapplingHookRope* const rope =
                                grapplingHookRopeMap[static_cast<int>(ropeId)];
                            if (rope)
                                rope->detachSoldier(
                                    static_cast<ISoldier*>(static_cast<Soldier*>(body)));
                        }
                    }
                    soldier = player->getVehicle();                         // +0x48 refreshed
                }

                // the static_cast to the ISoldier base is what emits retail's null-guarded
                // +0x144 adjustment (`cmovnz` at 0x81D23ED).
                best->attachSoldier(
                    static_cast<ISoldier*>(static_cast<Soldier*>(soldier)));
            }
        }
    }
}

} // namespace world
} // namespace hfe
} // namespace dice
