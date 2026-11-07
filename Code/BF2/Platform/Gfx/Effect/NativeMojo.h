// [CHOSEN, not recovered] MojoShader (zlib) reflection and MSL translation of one D3D9 shader.
#pragma once
#include "EffectData.h"
#include "FxBuild.h"

namespace bf2fx { namespace native {
// Same record fields and conventions as tools/macos/d3dxfx/cache-tool.cpp's describe().
bool reflect(bf2fx::Shader& shader, const std::vector<bf2fx::SamplerSlot>& samplers, std::string& error);
} // namespace native
} // namespace bf2fx
