#ifndef GEOM_TERRAINCOLOURPASSBIAS_H
#define GEOM_TERRAINCOLOURPASSBIAS_H

#include "../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace geom {

// [CHOSEN, not recovered] Retail fix "Terrain colour passes no longer leave black patches in
// the middle distance" (docs/retail-fixes.md).
// Uses the nearest point of the node's world bounds; camera contains at least xyz.
inline float terrainColourPassDepthBias(const float* camera, const Vec3& min, const Vec3& max)
{
    const double x = camera[0] < min.x ? double(min.x) - camera[0]
                   : camera[0] > max.x ? double(camera[0]) - max.x : 0.0;
    const double y = camera[1] < min.y ? double(min.y) - camera[1]
                   : camera[1] > max.y ? double(camera[1]) - max.y : 0.0;
    const double z = camera[2] < min.z ? double(min.z) - camera[2]
                   : camera[2] > max.z ? double(camera[2]) - max.z : 0.0;
    const double distanceSquared = x * x + y * y + z * z;
    const double referenceSquared = 60.0 * 60.0;
    return static_cast<float>(-1.0e-6f * (distanceSquared > referenceSquared
        ? referenceSquared / distanceSquared : 1.0));
}

// [CHOSEN, not recovered] Starts the drawFinal state cache for this frame's terrain colour passes.
void terrainResetColourPassBiasCache();

} // namespace geom
} // namespace hfe
} // namespace dice

#endif
