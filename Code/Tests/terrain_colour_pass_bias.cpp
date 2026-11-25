// [TEMP] [CHOSEN, not recovered] Headless check of the terrain colour-pass retail fix.
#include "../BF2/Geom/TerrainColourPassBias.h"

#include <cmath>
#include <cstdio>
#include <limits>

using dice::hfe::Math::Vec3;
using dice::hfe::geom::terrainColourPassDepthBias;

namespace {

int failures = 0;

void checkBias(const char* name, const float* camera, const Vec3& min, const Vec3& max,
               float expected)
{
    const float actual = terrainColourPassDepthBias(camera, min, max);
    if (!std::isfinite(actual) || actual < -1.0e-6f || actual > 0.0f
        || std::fabs(actual - expected) > std::fabs(expected) * 1.0e-6f)
    {
        std::fprintf(stderr, "%s: bias %.9g, expected %.9g within [-1e-6, 0]\n",
                     name, actual, expected);
        ++failures;
    }
}

} // namespace

int main()
{
    const Vec3 min = { 100.0f, 200.0f, 300.0f };
    const Vec3 max = { 200.0f, 400.0f, 500.0f };
    const float distances[] = { 10.0f, 60.0f, 120.0f, 600.0f };
    const float expected[] = { -1.0e-6f, -1.0e-6f, -2.5e-7f, -1.0e-8f };
    for (unsigned int i = 0; i != 4; ++i)
    {
        const float camera[] = { min.x - distances[i], 300.0f, 400.0f };
        checkBias("distance", camera, min, max, expected[i]);
        std::printf("d=%.0f m: bias=%.9g\n", distances[i],
                    terrainColourPassDepthBias(camera, min, max));
    }

    // Inside, on a face, and outside on each axis: distance is to the box, not its centre.
    const float positions[][3] = {
        { 150.0f, 300.0f, 400.0f }, { 100.0f, 300.0f, 400.0f },
        { 320.0f, 300.0f, 400.0f }, { 150.0f, 80.0f, 400.0f },
        { 150.0f, 520.0f, 400.0f }, { 150.0f, 300.0f, 180.0f },
        { 150.0f, 300.0f, 620.0f },
        { 64.0f, 152.0f, 400.0f },   // 36/48/0 -> 60 m from an edge
        { 28.0f, 104.0f, 400.0f },   // 72/96/0 -> 120 m from an edge
        { 140.0f, 440.0f, 520.0f }   // 0/40/20 -> inside the 60 m range
    };
    for (unsigned int i = 0; i != sizeof(positions) / sizeof(positions[0]); ++i)
        checkBias("nearest point", positions[i], min, max,
                  i < 2 || i == 7 || i == 9 ? -1.0e-6f : -2.5e-7f);
    const float corner[] = { 60.0f, 160.0f, 260.0f }; // 40/40/40 -> d squared = 4800
    checkBias("corner", corner, min, max, -7.5e-7f);

    float previous = -1.0e-6f;
    for (int i = 0; i <= 40000; ++i)
    {
        const float camera[] = { min.x - i * 0.5f, 300.0f, 400.0f };
        const float bias = terrainColourPassDepthBias(camera, min, max);
        if (!std::isfinite(bias) || bias < -1.0e-6f || bias > 0.0f || bias < previous)
        {
            std::fprintf(stderr, "bias must stay bounded and weaken monotonically at d=%.1f\n",
                         i * 0.5f);
            ++failures;
            break;
        }
        previous = bias;
    }
    const float largest = std::numeric_limits<float>::max();
    const float farCamera[] = { largest, largest, largest };
    const Vec3 farMin = { -largest, -largest, -largest };
    checkBias("finite coordinate extremes", farCamera, farMin, farMin, 0.0f);

    if (failures)
        return 1;
    std::puts("terrain colour-pass bias: PASS (distances, bounds, magnitude, monotonicity)");
    return 0;
}
