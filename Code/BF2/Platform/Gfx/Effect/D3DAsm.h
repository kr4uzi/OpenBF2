// [CHOSEN, not recovered] Assembler for the Direct3D 9 shader assembly that BF2's effects embed
// in `asm { }` blocks: ps_1_1..ps_1_4 and vs_1_1, the instruction subset the corpus uses.
// Token encoding follows the public D3D9 shader bytecode format. Input is the preprocessed
// block text (the effect preprocessor already expanded macros and removed C comments).
#pragma once
#include "EffectData.h"
#include <string>

namespace bf2fx {
bool assemble(const std::string& text, Bytes& out, std::string& error);
} // namespace bf2fx
