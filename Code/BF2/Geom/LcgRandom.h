// Path:     [INFERRED] shared Geom header for RendDX9.dll's copy of the engine random step.
// Contents: lcgrand_step, one body for every RendDX9 caller (NametagManager.cpp,
//           UndergrowthSystemLoad.cpp, GeomMisc/PerlinNoise.cpp). Core/Random.cpp, which holds the
//           engine's own dice::hfe::Random::getRaw, is not in the RendDX9 manifest.

#ifndef GEOM_LCGRANDOM_H
#define GEOM_LCGRANDOM_H

namespace dice {
namespace hfe {
namespace geom {

// One Schrage half of the Law-Kelton lcgrand: value * multiplier mod (2^31 - 1), computed in 16-bit
// halves with the image's arithmetic shifts (`sar`) and wrapping adds. Part of lcgrandStep below,
// not a retail function of its own.
inline unsigned int lcgrandHalf(unsigned int value, unsigned int multiplier)
{
    const unsigned int low = (value & 0xFFFFu) * multiplier;
    const unsigned int high = static_cast<unsigned int>(static_cast<int>(value) >> 16) * multiplier;
    const int middle = static_cast<int>((low >> 16) + high);
    const unsigned int next = (low & 0xFFFFu)
        + ((static_cast<unsigned int>(middle) & 0x7FFFu) << 16)
        + static_cast<unsigned int>(middle >> 15);
    const int reduced = static_cast<int>(next - 0x7FFFFFFFu);
    return reduced < 0 ? next : static_cast<unsigned int>(reduced);
}

// PE 0x1009EF30 (lcgrand_step). RendDX9's copy of dice::hfe::Random::getRaw (ELF 0x8166CE0):
// __thiscall on the state word, two Schrage steps (24112 = 0x5E30, then 26143 = 0x661F), the new
// state stored back and returned in eax.
inline unsigned int lcgrandStep(unsigned int* state)
{
    *state = lcgrandHalf(lcgrandHalf(*state, 24112u), 26143u);
    return *state;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // GEOM_LCGRANDOM_H
