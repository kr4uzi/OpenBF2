# [TEMP] [CHOSEN, not recovered] CPU fixtures; no game, socket or GPU is started.
add_executable(macos_fx_runtime EXCLUDE_FROM_ALL main.cpp)
target_link_libraries(macos_fx_runtime PRIVATE bf2_fx_runtime)
target_compile_definitions(macos_fx_runtime PRIVATE DEDICATED=0 BF2_IMAGE_RENDDX9=1)
target_compile_options(macos_fx_runtime PRIVATE
    "SHELL:-include ${BF2_ROOT}/Core/BuildConfig.h" -Wall -Wextra -Werror)
set(BF2_FX_TEST_CACHE "" CACHE PATH "Optional real BF2 effect cache built by build-cache.py")
add_custom_target(macos_fx_runtime_check
    COMMAND macos_fx_runtime "${CMAKE_BINARY_DIR}/fx-tests" "${BF2_FX_TEST_CACHE}"
    DEPENDS macos_fx_runtime USES_TERMINAL)
if(BUILD_TESTING)
    add_test(NAME macos_fx_runtime COMMAND macos_fx_runtime "${CMAKE_BINARY_DIR}/fx-tests" "${BF2_FX_TEST_CACHE}")
endif()
