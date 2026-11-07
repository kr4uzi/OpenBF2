# [TEMP] [CHOSEN, not recovered] CPU-only native D3DX contracts, no window/GPU/engine.
cmake_minimum_required(VERSION 3.21)
project(macos_d3dx_helpers LANGUAGES CXX)
set(BF2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../BF2")
add_executable(macos_d3dx_helpers main.cpp
    "${BF2_ROOT}/Platform/Gfx/Metal/D3DXMath.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/D3DXGeometry.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/D3DXPixels.cpp"
    "${BF2_ROOT}/Platform/Gfx/Metal/D3DXImages.cpp")
set_target_properties(macos_d3dx_helpers PROPERTIES CXX_STANDARD 14)
target_compile_definitions(macos_d3dx_helpers PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1)
target_compile_definitions(macos_d3dx_helpers PRIVATE BF2_MENU_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_options(macos_d3dx_helpers PRIVATE
    "SHELL:-include ${BF2_ROOT}/Core/BuildConfig.h" -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(macos_d3dx_helpers PRIVATE z)
target_link_libraries(macos_d3dx_helpers PRIVATE "-framework ImageIO" "-framework CoreGraphics" "-framework CoreFoundation")
enable_testing()
add_test(NAME d3dx_cpu COMMAND macos_d3dx_helpers)
