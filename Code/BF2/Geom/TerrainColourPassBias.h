#ifndef GEOM_TERRAINCOLOURPASSBIAS_H
#define GEOM_TERRAINCOLOURPASSBIAS_H

#include "../Math/Vec3.h"
#include <cmath>

namespace dice {
namespace hfe {
namespace geom {

// [CHOSEN, not recovered] Retail fix "Terrain colour passes no longer leave black patches in
// the middle distance" (docs/retail-fixes.md).
// Tests whether the node's bounds overlap 0 < interpVal < 1 in TerrainShader_Shared.fx's
// geoMorphPosition: cameraDist = dot(vCamerapos.xwz - wPos.xwz, ...), with wPos.w = 1.
// Terrain::applyFrameConstants uploads camera = m_cameraPosition (w = height above terrain + 1);
// Terrain::setMorphConstants uploads limits = &m_morphLimits[2 * node->lod] (TerrainDraw.cpp).
inline bool terrainNodeInMorphBand(const float* camera, const Vec3& min, const Vec3& max,
                                   const float* limits)
{
    if (limits[0] <= 0.0f)
        return false;
    const double nearX = camera[0] < min.x ? double(min.x) - camera[0]
                       : camera[0] > max.x ? double(camera[0]) - max.x : 0.0;
    const double nearZ = camera[2] < min.z ? double(min.z) - camera[2]
                       : camera[2] > max.z ? double(camera[2]) - max.z : 0.0;
    const double farX = std::fmax(std::fabs(double(camera[0]) - min.x),
                                  std::fabs(double(camera[0]) - max.x));
    const double farZ = std::fmax(std::fabs(double(camera[2]) - min.z),
                                  std::fabs(double(camera[2]) - max.z));
    const double height = double(camera[3]) - 1.0;
    const double nearestSquared = nearX * nearX + height * height + nearZ * nearZ;
    const double farthestSquared = farX * farX + height * height + farZ * farZ;
    return farthestSquared > double(limits[1]) / limits[0]
        && nearestSquared < (1.0 + double(limits[1])) / limits[0];
}

// [CHOSEN, not recovered] Bias only morph-band nodes; scale using the nearest point of the
// world bounds in xyz. Camera and limits are the shader constants described above.
inline float terrainColourPassDepthBias(const float* camera, const Vec3& min, const Vec3& max,
                                       const float* limits)
{
    if (!terrainNodeInMorphBand(camera, min, max, limits))
        return 0.0f;
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
