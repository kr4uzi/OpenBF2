// [CHOSEN, not recovered] vkd3d-shader (LGPL-2.1+) glue for native effect compilation.
#pragma once
#include "EffectData.h"
#include <string>

namespace bf2fx { namespace native {
// Preprocesses the request root with its macros; includes come from the request's snapshot.
bool preprocessSource(const bf2fx::Request& request, std::string& out, std::string& log);
// Compiles one entry point to D3D9 bytecode (vs/ps 1.x-3.0).
int compileShader(const std::string& source, const std::string& entry, const std::string& profile,
                  bf2fx::Bytes& out, std::string& log);
// Disassembles D3D9 bytecode with vkd3d (diagnostics only).
bool disassemble(const bf2fx::Bytes& code, std::string& out);
} // namespace native
} // namespace bf2fx
