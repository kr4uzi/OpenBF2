// [CHOSEN, not recovered] The shipped Windows images (BF2Audio.dll, RendDX9.dll) import rand/srand
// from MSVCR71.DLL, whose generator yields 0..32767 (RAND_MAX 0x7FFF), and the recovered code
// depends on that range: BF2Audio's random envelope rows scale rand() by 1/32767 (the helper at
// 0x10017D90: FILD rand(); FMUL [0x1003B298]), the sample seeds take its low two bytes, and
// RendDX9's Swiff range pick scales by -1/32767. The macOS libc rand() yields 0..0x7FFFFFFF, so
// those products come out ~65536 times too large (pitch pinned at the 2.0 clamp, volume in the
// thousands). This reproduces the MSVCR71 generator: a per-thread state (MSVCR71 keeps _holdrand
// in the per-thread data, initially 1), state = state * 214013 + 2531011, result (state >> 16) &
// 0x7FFF. All client-image rand/srand calls in recovered code go through it, so they share one
// generator per thread as the retail modules share MSVCR71's. The platform reason is the CRT, not a
// behaviour change. Windows keeps std::rand (it is MSVCR71's), and so does the dedicated server
// (DEDICATED=1): its fidelity target, the Linux bf2, imports rand/srand from glibc.
#ifndef BF2_PLATFORM_MSVCRAND_H
#define BF2_PLATFORM_MSVCRAND_H

#include <cstdlib>

namespace bf2crt {

#if defined(_WIN32) || (defined(DEDICATED) && DEDICATED)
const int kRandMax = RAND_MAX;
inline int  rand()                 { return std::rand(); }
inline void srand(unsigned int s)  { std::srand(s); }
#else
const int kRandMax = 0x7FFF;
inline unsigned int& randState()
{
    static thread_local unsigned int state = 1u;
    return state;
}
inline int rand()
{
    unsigned int& s = randState();
    s = s * 214013u + 2531011u;
    return (int) ((s >> 16) & 0x7FFFu);
}
inline void srand(unsigned int seed) { randState() = seed; }
#endif

} // namespace bf2crt

#endif // BF2_PLATFORM_MSVCRAND_H
