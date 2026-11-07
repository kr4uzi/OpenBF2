// [TEMP] [CHOSEN, not recovered] Stored ImageIO/sips reference, no runtime encoder.
#pragma once
#include "../../BF2/Platform/Gfx/Metal/D3DXHelpers.h"
#include <fstream>
#include <iterator>
#include <string>
namespace menuFixture {
inline bf2d3dx::Bytes read(const char* name) {
    std::ifstream file(std::string(BF2_MENU_FIXTURES)+"/"+name,std::ios::binary);
    return bf2d3dx::Bytes(std::istreambuf_iterator<char>(file),{});
}
}
