# [CHOSEN, not recovered] Standalone native effect harness (macOS port tooling). Not part of an engine image.
cmake_minimum_required(VERSION 3.20)
project(nativefx C CXX OBJCXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(FX "${BF2_REPO}/Code/BF2/Platform/Gfx/Effect")
add_library(fxcore STATIC "${FX}/Cache.cpp" "${FX}/Runtime.cpp" "${FX}/FxParser.cpp" "${FX}/FxStates.cpp"
    "${FX}/FxBuild.cpp" "${FX}/D3DAsm.cpp" "${FX}/Native.cpp" "${FX}/NativeVkd3d.cpp" "${FX}/NativeMojo.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/gen/refusals.inc")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gen/refusals.inc"
    COMMAND "${CMAKE_COMMAND}" -DIN="${FX}/data/d3dx-refusals.txt" -DOUT="${CMAKE_CURRENT_BINARY_DIR}/gen/refusals.inc" -P "${FX}/refusals.cmake"
    DEPENDS "${FX}/data/d3dx-refusals.txt" "${FX}/refusals.cmake")
target_include_directories(fxcore PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/gen" "${VKD3D_SRC}/include" "${MOJO_SRC}")
target_compile_definitions(fxcore PRIVATE MOJOSHADER_EFFECT_SUPPORT)
target_include_directories(fxcore PUBLIC "${FX}")
target_compile_definitions(fxcore PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1)
target_compile_options(fxcore PRIVATE "SHELL:-include ${BF2_REPO}/Code/BF2/Core/BuildConfig.h"
    -Wall -Wextra -Werror -Wno-deprecated-declarations)
add_executable(nativefx nativefx.cpp Dump.cpp)
target_include_directories(nativefx PRIVATE "${VKD3D_SRC}/include" "${MOJO_SRC}")
target_compile_definitions(nativefx PRIVATE MOJOSHADER_EFFECT_SUPPORT)
target_compile_options(nativefx PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
target_link_libraries(nativefx PRIVATE fxcore "${VKD3D_BUILD}/.libs/libvkd3d-shader.a"
    "${VKD3D_BUILD}/.libs/libvkd3d-common.a" "${MOJO_LIB}")
add_executable(gpucheck gpucheck.mm)
target_include_directories(gpucheck PRIVATE "${MOJO_SRC}")
target_compile_definitions(gpucheck PRIVATE MOJOSHADER_EFFECT_SUPPORT)
target_compile_options(gpucheck PRIVATE -fobjc-arc -Wall -Wno-unused-parameter -Wno-missing-field-initializers)
target_link_libraries(gpucheck PRIVATE fxcore "${VKD3D_BUILD}/.libs/libvkd3d-shader.a" "${VKD3D_BUILD}/.libs/libvkd3d-common.a" "${MOJO_LIB}" "-framework Metal" "-framework Foundation")
