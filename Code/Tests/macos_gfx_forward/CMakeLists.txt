# [CHOSEN, not recovered] G2w forwarding wrappers over generated fake "real" objects. No GPU, DLL or network.
# The fixture (a stand-in SDK built from the subset headers) exists only in this test's build directory.
set(BF2_GFX_DIR "${BF2_ROOT}/Platform/Gfx")
set(BF2_FWD_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/fixture")
add_custom_command(
    OUTPUT "${BF2_FWD_FIXTURE}/FakeSdk.h" "${BF2_FWD_FIXTURE}/FakeObjects.h" "${BF2_FWD_FIXTURE}/ForwardSdk.h"
    COMMAND python3 "${PROJECT_SOURCE_DIR}/tools/macos/gen_d3d9_forward.py"
            --out-dir "${CMAKE_CURRENT_BINARY_DIR}/generated" --fixture-dir "${BF2_FWD_FIXTURE}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/macos/gen_d3d9_forward.py"
            "${BF2_GFX_DIR}/D3D9Subset.h" "${BF2_GFX_DIR}/D3DX9Subset.h"
    COMMENT "Generating the G2w test fixture (fake SDK and fake objects)"
    VERBATIM)
add_executable(macos_gfx_forward EXCLUDE_FROM_ALL main.cpp
    "${BF2_GFX_DIR}/D3D9Forward/Generated/ForwardWrappers.cpp"
    "${BF2_GFX_DIR}/D3D9Forward/Generated/ForwardFunctions.cpp"
    "${BF2_FWD_FIXTURE}/FakeObjects.h")
target_include_directories(macos_gfx_forward PRIVATE "${BF2_FWD_FIXTURE}" "${BF2_GFX_DIR}")
target_compile_features(macos_gfx_forward PRIVATE cxx_std_14)
target_compile_definitions(macos_gfx_forward PRIVATE BF2_GFX_FORWARD_TU=1)
set_target_properties(macos_gfx_forward PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(macos_gfx_forward PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
add_custom_target(macos_gfx_forward_check
    COMMAND macos_gfx_forward
    COMMAND python3 "${PROJECT_SOURCE_DIR}/tools/macos/gen_d3d9_forward.py" --check
    DEPENDS macos_gfx_forward USES_TERMINAL)
if(BUILD_TESTING)
    add_test(NAME macos_gfx_forward COMMAND macos_gfx_forward)
    add_test(NAME macos_gfx_forward_generated_current
        COMMAND python3 "${PROJECT_SOURCE_DIR}/tools/macos/gen_d3d9_forward.py" --check)
endif()
