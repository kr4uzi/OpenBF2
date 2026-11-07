// BF2Audio envelope evaluation; excluded from host images.
#if defined(BF2_IMAGE_BF2AUDIO)
#include "SoundEvaluator.h"
#include "../../Platform/MsvcRand.h"   // [CHOSEN, not recovered] MSVCR71 rand range (RAND_MAX 0x7FFF)

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cfloat>

namespace dice {
namespace hfe {

namespace {
// Sorts envelope points by x only.
struct LessX
{
    bool operator()(const EnvelopePoint& a, const EnvelopePoint& b) const { return a.x < b.x; }
};
}

// The point copy is an unrolled memcpy in the image; the table is baked at once.
SoundEvaluator::SoundEvaluator(float xMin, float xMax, float yMin, float yMax, bool randomize,
                               unsigned int count, const EnvelopePoint* points)
    : m_xMin(xMin), m_xMax(xMax), m_yMin(yMin), m_yMax(yMax), m_randomize(randomize)
{
    m_points.resize(count);
    for (unsigned int i = 0; i < count; ++i)
        m_points[i] = points[i];
    rebuild();
}

// Order the bounds, clamp every point into them, sort by x, then bake.
void SoundEvaluator::rebuild()
{
    if (m_xMin > m_xMax) { const float t = m_xMin; m_xMin = m_xMax; m_xMax = t; }
    if (m_yMin > m_yMax) { const float t = m_yMin; m_yMin = m_yMax; m_yMax = t; }
    for (unsigned int i = 0; i < m_points.size(); ++i)
    {
        EnvelopePoint& p = m_points[i];
        if (p.x < m_xMin) p.x = m_xMin; else if (p.x > m_xMax) p.x = m_xMax;
        if (p.yMin < m_yMin) p.yMin = m_yMin; else if (p.yMin > m_yMax) p.yMin = m_yMax;
        if (p.yMax < m_yMin) p.yMax = m_yMin; else if (p.yMax > m_yMax) p.yMax = m_yMax;
    }
    std::sort(m_points.begin(), m_points.end(), LessX());
    const int rows = m_randomize ? 64 : 1;
    m_values.resize(rows * m_points.size(), 0.0f);
    for (unsigned int i = 0; i < m_points.size(); ++i)
        m_values[i] = m_points[i].yMin;
    for (int row = 1; row < rows; ++row)
    {
        for (unsigned int i = 0; i < m_points.size(); ++i)
        {
            const float r = (float) bf2crt::rand() * 0.000030518509f;  // rand() / 32767
            m_values[i + row * m_points.size()] = (m_points[i].yMax - m_points[i].yMin) * r + m_points[i].yMin;
        }
    }
}

// No points: the yMax bound. Otherwise the row is `sample & 63` when randomised
// (0 otherwise), clamped to the first/last point outside the curve, interpolated inside it - and a
// pair of points closer than FLT_EPSILON in x answers the later one's value.
float SoundEvaluator::getValue(float x, int sample)
{
    const unsigned int count = (unsigned int) m_points.size();
    if (count == 0)
        return m_yMax;
    const int row = m_randomize ? (sample & 0x3F) : 0;
    const float* values = &m_values[row * count];
    const EnvelopePoint* points = &m_points[0];
    if (x <= points[0].x)
        return values[0];
    if (x >= points[count - 1].x)
        return values[count - 1];
    unsigned int i = 1;
    if (count <= 1)
        return values[0];
    while (x >= points[i].x)
    {
        if (++i >= count)
            return values[0];
    }
    const float x1 = points[i].x;
    const float x0 = points[i - 1].x;
    const float span = x1 - x0;
    const float y1 = values[i];
    if (std::fabs(span) > FLT_EPSILON)
    {
        const float y0 = values[i - 1];
        return (x - x0) / span * (y1 - y0) + y0;
    }
    return y1;
}

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_BF2AUDIO
