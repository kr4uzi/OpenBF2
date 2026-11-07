// [TEMP] [CHOSEN, not recovered] Numeric boundary fixture; links the real codec.
// No network, game content, or alternate production bodies.
#include "../BF2/IO/BitStream.h"
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#pragma STDC FENV_ACCESS ON
namespace {
unsigned checks;
void check(bool condition, int line)
{
    ++checks;
    if (!condition) { std::fprintf(stderr, "macos_bitstream_numeric.cpp:%d failed\n", line); std::exit(1); }
}
#define CHECK(x) check((x), __LINE__)
}
int main()
{
    using namespace dice::hfe;
    unsigned char storage[128] = {};
    io::BitStream stream(storage, sizeof storage);
    Quat q;
    q.w = 1; q.x = 0; q.y = 0.5f; q.z = -0.5f;
    stream.writeUnitQuaternion(q, 8);
    CHECK(stream.getWriteBitPosition() == 32);
    CHECK(storage[0] == 255 && storage[1] == 127 && storage[2] == 191 && storage[3] == 63);
    CHECK(!stream.m_error);

    // The wire width masks the invalid dword's high bit; NaN is not rejected
    // by these shipped codecs. Preserve that behavior rather than adding a guard.
    q.x = std::numeric_limits<float>::quiet_NaN();
    std::feclearexcept(FE_ALL_EXCEPT);
    stream.clear(); stream.writeUnitQuaternion(q, 8);
    CHECK(storage[1] == 0 && !stream.m_error);
    CHECK(std::fetestexcept(FE_INVALID));
    stream.clear(); stream.writeUnitQuaternion3Comp(q, 8);
    unsigned sign = 99, component = 99;
    CHECK(stream.readBits(&sign, 1) && sign == 0);
    CHECK(stream.readBits(&component, 8) && component == 0);

    // Width 32 bypasses conversion and keeps the raw NaN payload.
    stream.clear(); stream.writeUnitQuaternion(q, 32);
    float raw[4] = {};
    CHECK(stream.readBits(raw, 128));
    CHECK(raw[0] == 1 && std::isnan(raw[1]) && raw[2] == 0.5f && raw[3] == -0.5f);

    // Truncated/malformed payloads still take the codec's read-overrun path.
    stream.clear();
    CHECK(!stream.readBits(raw, 1) && stream.m_error && stream.m_errorCode == 2);
    stream.clear();
    CHECK(!stream.writeBits(raw, sizeof(storage) * 8 + 1) && stream.m_error && stream.m_errorCode == 1);
    std::printf("production BitStream numeric boundaries: %u checks passed\n", checks);
}
