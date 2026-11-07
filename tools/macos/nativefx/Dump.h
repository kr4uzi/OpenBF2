// [CHOSEN, not recovered] Canonical text form of an effect Description, for comparisons.
#pragma once
#include "EffectData.h"
#include <string>
namespace nativefx {
// Object IDs are resolved (texture names, shader ordinals); shader bytecode is summarized by
// stage, model and reflection so that native and D3DX output compare structurally.
std::string dump(const bf2fx::Description& d, bool registers = false);
}
