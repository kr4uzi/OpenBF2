// Path: [INFERRED]. Dedicated ELF 0x816A9C0..0x816B1E0, PE 0x59BC30..0x59C3AF.
// Both primary vtables add only init/frame-update/destructor overrides to Bundle.
#include "AreaObject.h"
#include "../TemplateLeaves.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include <cmath>

namespace dice { namespace hfe { namespace world {
AreaObject::AreaObject(const AreaObjectTemplate* objectTemplate)
    : Bundle(objectTemplate), m_areaTransformation(Mat4::getIdentity())
{
    updateFlags(0x2000, 0);
    updateFlags(0x800, 0);
    updateFlags(0x20000, 0);
    updateFlags(0x400, 0);
    updateFlags(2, 0);
    m_areaTemplate = objectTemplate;
    m_areaTransformation = Mat4::getIdentity();
}
AreaObject::~AreaObject() {}
void AreaObject::init() { Bundle::init(); }

// Both images measure from the observer at (0,0,0): the client body BF2.exe 0x5FF790 (table
// 0x8F3838 +0xA8) is the dedicated one, with the bounds test as a call (0x5CD130, inclusive
// min/max) instead of inline compares.
void AreaObject::handleFrameUpdate(float)
{
    const Mat4& absolute = getAbsoluteTransformation();
    const float x = absolute.m[12];
    const float z = absolute.m[14];
    const int count = static_cast<int>(m_areaTemplate->m_linePoints.size());
    if (count < 3) return;
    const Vec3 relative = {-x, 0, -z};
    const LocalBoundingBox& bounds = m_areaTemplate->m_areaBounds;
    if (!(relative.x >= bounds.m_min.x && relative.y >= bounds.m_min.y &&
          relative.z >= bounds.m_min.z && relative.x <= bounds.m_max.x &&
          relative.y <= bounds.m_max.y && relative.z <= bounds.m_max.z))
        return;

    float nearestDistanceSquared = 6666666500.0f; // retail bits 0x4FC6AEA1
    Vec2 nearest = {0, 0};
    for (int i = 0; i < count; ++i)
    {
        const Vec2& a = m_areaTemplate->m_linePoints[i];
        const Vec2& b = m_areaTemplate->m_linePoints[(i + 1) % count];
        const Vec2 start = {x + a.x, z + a.y};
        const Vec2 end = {x + b.x, z + b.y};
        Vec2 direction = {end.x - start.x, end.y - start.y};
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        const float inverseLength = 1.0f / length; // retail does not guard degenerate segments
        direction.x *= inverseLength;
        direction.y *= inverseLength;
        const float projection = -start.x * direction.x - start.y * direction.y;
        Vec2 point;
        if (projection < 0.0f) point = start;
        else if (projection > length) point = end;
        else
        {
            point.x = start.x + direction.x * projection;
            point.y = start.y + direction.y * projection;
        }
        const float distanceSquared = point.x * point.x + point.y * point.y;
        if (distanceSquared < nearestDistanceSquared)
        {
            nearestDistanceSquared = distanceSquared;
            nearest = point;
        }
    }
    if (m_areaTemplate->m_triggerRadius * m_areaTemplate->m_triggerRadius > nearestDistanceSquared)
    {
        m_areaTransformation.m[12] = nearest.x;
        m_areaTransformation.m[13] = absolute.m[13];
        m_areaTransformation.m[14] = nearest.y;
    }
}

#if DEDICATED
// Dedicated-only: no client caller.
void AreaObject::addAbsolutePoint()
{
    const Vec3& position = getAbsolutePosition();
    const Vec2 point = {-position.x, -position.z};
    const_cast<AreaObjectTemplate*>(m_areaTemplate)->addLinePoint(point);
    IObjectTemplate* arrowTemplate = objectTemplateManager->getTemplate("DirectionArrow");
    if (arrowTemplate)
    {
        IObject* arrow = arrowTemplate->createObject(); // no object-null check in either port
        arrow->init();
        const Vec3& updatedPosition = getAbsolutePosition();
        const Vec3 arrowPosition = {point.x + updatedPosition.x, updatedPosition.y,
                                    point.y + updatedPosition.z};
        arrow->setAbsolutePosition(arrowPosition);
    }
}
#endif
} } }
