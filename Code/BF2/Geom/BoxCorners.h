// Path:     [INFERRED] shared Geom header for RendDX9.dll's 96-byte eight-corner object box.
// Contents: the box's constructors, corner layout, world transform, frustum test and component
//           extremes, one body per retail function. ObjectDrawer.cpp, XpLightManager.cpp and
//           Overgrowth/OverGrowthRender.cpp use this type. The type name is
//           [CHOSEN, not recovered].

#ifndef GEOM_BOXCORNERS_H
#define GEOM_BOXCORNERS_H

#include <cstddef>
#include "QuadPatch.h"                 // aabbCorners - the corner layout body
#include "../Math/Frustum.h"
#include "../Math/LocalBoundingBox.h"
#include "../Math/Mat4.h"
#include "../Math/Plane3.h"
#include "../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace geom {

// The eight corners of a box, corner i taking x from the maximum when bit 2 is set, y when bit 1
// is set and z when bit 0 is set. Plain data: 24 floats, no vtable.
struct BoxCorners
{
    Vec3 corner[8];

    // PE 0x100AE110. Zeroes the 24 floats (retn, no arguments). Callers that zero the same floats
    // inline in retail use this constructor as well.
    BoxCorners()
    {
        for (unsigned int i = 0; i < 8; ++i)
            corner[i].x = corner[i].y = corner[i].z = 0.0f;
    }

    // PE 0x100CE120. Zeroes the 24 floats inline, then lays out `bounds` (setBounds) and moves the
    // corners through `world` (transform); retn 8.
    BoxCorners(const LocalBoundingBox& bounds, const Mat4& world)
    {
        for (unsigned int i = 0; i < 8; ++i)
            corner[i].x = corner[i].y = corner[i].z = 0.0f;
        setBounds(bounds);
        transform(world);
    }

    // The same retail function as QuadPatch.h's aabbCorners, not a separate one: the corners from
    // the box's (min, max) pair, which the box carries contiguously at +0x00 / +0x0C.
    void setBounds(const LocalBoundingBox& bounds)
    {
        aabbCorners(&corner[0].x, &bounds.m_min.x);
    }

    // PE 0x101C67F0. Every corner through the full matrix, translation included; all three rows
    // are formed from the untouched corner before any is stored. Corner 0's y and x rows associate
    // differently from corners 1..7 (101C67F6..101C683D against 101C6845..101C688C).
    void transform(const Mat4& world)
    {
        const float* const m = world.m;
        for (unsigned int i = 0; i < 8; ++i)
        {
            const float x = corner[i].x;
            const float y = corner[i].y;
            const float z = corner[i].z;
            const float tz = z * m[10] + x * m[2] + y * m[6] + m[14];
            const float ty = i == 0 ? x * m[1] + y * m[5] + z * m[9] + m[13]
                                    : x * m[1] + z * m[9] + y * m[5] + m[13];
            const float tx = i == 0 ? z * m[8] + x * m[0] + y * m[4] + m[12]
                                    : y * m[4] + z * m[8] + x * m[0] + m[12];
            corner[i].x = tx;
            corner[i].y = ty;
            corner[i].z = tz;
        }
    }

    // PE 0x101C6A90. False as soon as one of the frustum's six planes has all eight corners
    // strictly in front of it (distance > 0, ordered; all eight are counted for each plane), true
    // otherwise; retn 4. Corner 0's distance is (z*n.z + x*n.x) + y*n.y + d, corners 1..7's
    // (y*n.y + z*n.z) + x*n.x + d (101C6AA0..101C6AB4 against 101C6ACD..101C6AE2).
    bool insideFrustum(const Frustum& frustum) const
    {
        for (unsigned int plane = 0; plane < 6; ++plane)
        {
            const Plane3& p = frustum.m_planes[plane];
            int outside = 0;
            {
                const Vec3& c = corner[0];
                if (c.z * p.normal.z + c.x * p.normal.x + c.y * p.normal.y + p.d > 0.0f)
                    outside += 1;
            }
            for (unsigned int i = 1; i < 8; ++i)
            {
                const Vec3& c = corner[i];
                if (c.y * p.normal.y + c.z * p.normal.z + c.x * p.normal.x + p.d > 0.0f)
                    outside += 1;
            }
            if (outside == 8)
                return false;
        }
        return true;
    }

    // PE 0x101C6C50. The negation of insideFrustum (`neg al; sbb eax, eax; add eax, 1`); retn 4.
    bool outside(const Frustum& frustum) const
    {
        return !insideFrustum(frustum);
    }

    // PE 0x101C6C70. The component-wise maximum, starting from corner 0: a component moves only
    // when it is strictly below the corner's (`test ah, 5; jp`), so a NaN corner never wins.
    Vec3 maximum() const
    {
        Vec3 result = corner[0];
        for (unsigned int i = 1; i < 8; ++i)
        {
            if (result.x < corner[i].x) result.x = corner[i].x;
            if (result.y < corner[i].y) result.y = corner[i].y;
            if (result.z < corner[i].z) result.z = corner[i].z;
        }
        return result;
    }

    // PE 0x101C6E10. The component-wise minimum, starting from corner 0: a component moves only
    // when it is strictly above the corner's (`test ah, 41h; jne`).
    Vec3 minimum() const
    {
        Vec3 result = corner[0];
        for (unsigned int i = 1; i < 8; ++i)
        {
            if (result.x > corner[i].x) result.x = corner[i].x;
            if (result.y > corner[i].y) result.y = corner[i].y;
            if (result.z > corner[i].z) result.z = corner[i].z;
        }
        return result;
    }
};

// setBounds hands both objects to aabbCorners as flat float arrays.
static_assert(sizeof(Vec3) == 12, "a corner is three floats");
static_assert(offsetof(LocalBoundingBox, m_max) == 0x0C, "the box's max follows its min");
#if defined(_M_IX86)
static_assert(sizeof(BoxCorners) == 0x60, "the eight-corner box is 96 bytes");
#endif

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // GEOM_BOXCORNERS_H
