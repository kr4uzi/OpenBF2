// [INFERRED] diagnostic companions to BF2ClientSeams.h, not recovered bodies.
#include "BF2ClientSeams.h"
#if !DEDICATED
#include <cstdio>
#include <cstdlib>

namespace dice { namespace hfe {
// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: swiff-host-server-browser]
// Haptic targets remain null until their implementations exist; menu initialization
// checks for null before dispatching to them.
IBase* g_hapticForceTargets[6] = { 0, 0, 0, 0, 0, 0 };
#define BF2_MENU_SEAM_FRONTIER(name, text) \
    std::fprintf(stderr, "[FRONTIER] dice::hfe::" #name ": " text "\n"); \
    std::fflush(stderr); \
    std::abort()
void   hapticTarget_vslot0x4C(IBase*, const double*)                     { BF2_MENU_SEAM_FRONTIER(hapticTarget_vslot0x4C, "the haptic force target's +0x4C (a NovintHFX::Vect3 setter) is unrecovered"); }
#undef BF2_MENU_SEAM_FRONTIER
} }
#endif
