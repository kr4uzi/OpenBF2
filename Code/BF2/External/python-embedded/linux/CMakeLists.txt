# =================================================================================================
# libdice_py.so - the embedded CPython 2.3.4 built as the Linux dedicated server's interpreter.
#
# SCAFFOLDING - NOT RECOVERED SOURCE. This build description is [CHOSEN, not recovered]; what it
# builds is vendored CPython 2.3.4 (../py-src-2.3.4, with local changes documented in README.md) with
# zlib 1.2.3 compiled in, which is what the retail interpreter is: the shipped Linux ELF's
# DT_NEEDED names libdice_py.so, and the Windows dice_py.dll's PDB path attests the
# External/python-embedded/ project layout and the `dice_py` output name (both recorded in
# Code/Build/python/build_python.cmd, the Windows counterpart of this file). README.md beside this
# file records every decision, every deviation from upstream and how pyconfig.h was generated.
#
# Usable two ways:
#   * as a subdirectory of the root project: root CMakeLists.txt adds it under BF2_BUILD_LINUX and
#     sets BF2_DICE_PY_OUTPUT_DIR to the `bf2` executable's directory, so the pair share an $ORIGIN;
#   * standalone:  cmake -S <this dir> -B <build> -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
#                  ninja -C <build> dice_py dice_py_check dice_py_check_run
#
# Targets:
#   dice_py            libdice_py.so (PUBLIC include dirs: this directory for pyconfig.h, then
#                      py-src-2.3.4/Include - exactly what a consumer of Python.h needs)
#   dice_py_check      Code/Build/python/pycheck.cpp linked against dice_py, $ORIGIN rpath
#   dice_py_check_lib  stages Lib/copy_reg.py and Lib/types.py (cPickle's imports) into the build
#                      tree, so Python 2.3 writes its .pyc files there and never into py-src-2.3.4
#   dice_py_check_run  runs dice_py_check against that staged directory
# =================================================================================================
cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(dice_py LANGUAGES C CXX)
endif()

# [CHOSEN, not recovered] The same description also builds libdice_py.dylib for the macOS port.
# Only the configure output differs: ../macos/pyconfig.h is the vendored configure's answer on
# Darwin (see ../macos/README.md); config.c, the source list and every flag are shared.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(DICE_PY_MACOS OFF)
    set(DICE_PY_PYCONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        message(FATAL_ERROR
            "dice_py/linux: pyconfig.h here is configure's output for Linux x86-64 (LP64, "
            "little-endian, 16-byte fpos_t); this is ${CMAKE_SYSTEM_PROCESSOR} with "
            "${CMAKE_SIZEOF_VOID_P}-byte pointers. Regenerate pyconfig.h for the platform first - "
            "README.md says how.")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(DICE_PY_MACOS ON)
    get_filename_component(DICE_PY_PYCONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../macos" ABSOLUTE)
    # The Darwin pyconfig.h holds for both x86_64 and arm64 (LP64, little-endian, same libSystem
    # sizes); see ../macos/README.md. [CHOSEN, not recovered] Universal builds use the
    # compiler's per-slice architecture macros for the va_list override, not the CMake host.
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "dice_py/macos: only 64-bit macOS is supported.")
    endif()
else()
    message(FATAL_ERROR
        "dice_py/linux: this directory builds libdice_py.so for Linux and libdice_py.dylib for "
        "macOS only (this is ${CMAKE_SYSTEM_NAME}). The Windows dice_py.dll is built by "
        "Code/Build/python/build_python.cmd.")
endif()

# Where libdice_py.so and dice_py_check land. The root project points this at the `bf2` directory.
if(NOT DEFINED BF2_DICE_PY_OUTPUT_DIR)
    set(BF2_DICE_PY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin_linux")
endif()

# -------------------------------------------------------------------------------------------------
# Paths. This file sits at Code/BF2/External/python-embedded/linux/.
# -------------------------------------------------------------------------------------------------
get_filename_component(DICE_PY_EMBEDDED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(DICE_PY_BF2_ROOT "${DICE_PY_EMBEDDED_ROOT}/../.." ABSOLUTE)
get_filename_component(DICE_PY_CODE_ROOT "${DICE_PY_BF2_ROOT}/.." ABSOLUTE)
set(DICE_PY_SRC "${DICE_PY_EMBEDDED_ROOT}/py-src-2.3.4")
set(DICE_PY_ZLIB "${DICE_PY_BF2_ROOT}/External/zlib-1.2.3")
set(DICE_PY_BUILDCONFIG_H "${DICE_PY_BF2_ROOT}/Core/BuildConfig.h")
set(DICE_PY_WINDOWS_MANIFEST "${DICE_PY_CODE_ROOT}/Build/python/pythoncore.files")
set(DICE_PY_CHECK_SOURCE "${DICE_PY_CODE_ROOT}/Build/python/pycheck.cpp")
foreach(required IN ITEMS
        DICE_PY_SRC DICE_PY_ZLIB DICE_PY_BUILDCONFIG_H DICE_PY_WINDOWS_MANIFEST DICE_PY_CHECK_SOURCE)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "dice_py/linux: ${required} is missing: ${${required}}")
    endif()
endforeach()

# -------------------------------------------------------------------------------------------------
# Sources: the Windows manifest (Code/Build/python/pythoncore.files, 119 pythoncore units read
# from pythoncore.dsp) with the Unix substitutions applied at configure time, so the two ports can
# only drift where this file says so. Re-reading the manifest keeps the derivation honest: an entry
# added on Windows appears here on the next configure, and an expected entry that vanishes from the
# manifest is a hard error rather than a silent gap.
# -------------------------------------------------------------------------------------------------
set(DICE_PY_MANIFEST_WINDOWS_ONLY
    PC/config.c             # the Windows builtin table; config.c beside this file is the Linux one
    PC/dl_nt.c              # DllMain
    PC/getpathp.c           # registry/exe-relative sys.path; Modules/getpath.c is the Unix reader
    PC/import_nt.c          # registry lookup of extension modules
    PC/msvcrtmodule.c       # the msvcrt module
    Python/dynload_win.c    # LoadLibrary; Python/dynload_shlib.c is the dlopen loader
)
# Upstream's own rule, not a local one: setup.py 2.3.4 lines 395-405 ("These don't work for 64-bit
# platforms!!!") builds audioop, imageop and rgbimg only when sys.maxint is 32-bit, and PC/config.c
# registers the same three only #ifndef MS_WIN64. yuvconvert.c is their neighbour in the .dsp and
# is referenced by nothing but the SGI svmodule.c, which is not in the manifest. The retail
# dice_py.dll's inittab carries none of the four either.
set(DICE_PY_MANIFEST_NOT_64BIT_CLEAN
    Modules/audioop.c
    Modules/imageop.c
    Modules/rgbimgmodule.c
    Modules/yuvconvert.c
)
set(DICE_PY_UNIX_ONLY
    Modules/getpath.c       # sys.path/prefix computation (Makefile.pre.in: Modules/getpath.o)
    Python/dynload_shlib.c  # dlopen-based extension loading (configure: DYNLOADFILE)
)

file(STRINGS "${DICE_PY_WINDOWS_MANIFEST}" dice_py_manifest_entries)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DICE_PY_WINDOWS_MANIFEST}")
set(dice_py_sources "")
set(dice_py_dropped "")
foreach(entry IN LISTS dice_py_manifest_entries)
    string(STRIP "${entry}" entry)
    if(entry STREQUAL "" OR entry MATCHES "^#")
        continue()
    endif()
    if(entry IN_LIST DICE_PY_MANIFEST_WINDOWS_ONLY OR entry IN_LIST DICE_PY_MANIFEST_NOT_64BIT_CLEAN)
        list(APPEND dice_py_dropped "${entry}")
        continue()
    endif()
    list(APPEND dice_py_sources "${DICE_PY_SRC}/${entry}")
endforeach()
foreach(entry IN LISTS DICE_PY_MANIFEST_WINDOWS_ONLY DICE_PY_MANIFEST_NOT_64BIT_CLEAN)
    if(NOT entry IN_LIST dice_py_dropped)
        message(FATAL_ERROR
            "dice_py/linux: ${entry} is no longer listed in ${DICE_PY_WINDOWS_MANIFEST}. The "
            "Windows manifest changed shape; re-derive the substitution lists in this file and "
            "README.md before building.")
    endif()
endforeach()
foreach(entry IN LISTS DICE_PY_UNIX_ONLY)
    list(APPEND dice_py_sources "${DICE_PY_SRC}/${entry}")
endforeach()
list(APPEND dice_py_sources "${CMAKE_CURRENT_SOURCE_DIR}/config.c")

# zlib, built IN, exactly as build_python.cmd does it: the retail dice_py carries zlib inside
# (version string, inflate and deflate are all in its bytes - 1.2.2 there, the engine's vendored
# 1.2.3 here, a recorded [PLATFORM]-class substitution). Without it zipimport cannot read
# pylib-2.3.4.zip and every stdlib import dies with "can't decompress data; zlib not available".
# The nine units are the closure build_python.cmd established by linking (compress.c supplies
# compressBound for deflate.c); uncompr.c, infback.c and gzio.c are unused, example.c and
# minigzip.c are programs. All nine are required here - there is no zlib-less fallback build.
set(dice_py_zlib_sources "")
foreach(unit IN ITEMS adler32 crc32 inffast inflate inftrees zutil deflate trees compress)
    list(APPEND dice_py_zlib_sources "${DICE_PY_ZLIB}/${unit}.c")
endforeach()
list(APPEND dice_py_zlib_sources "${DICE_PY_SRC}/Modules/zlibmodule.c")

foreach(source IN LISTS dice_py_sources dice_py_zlib_sources)
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "dice_py/linux: source is missing: ${source}")
    endif()
endforeach()

# -------------------------------------------------------------------------------------------------
# The library.
# -------------------------------------------------------------------------------------------------
add_library(dice_py SHARED ${dice_py_sources} ${dice_py_zlib_sources})
set_target_properties(dice_py PROPERTIES
    OUTPUT_NAME "dice_py"                       # libdice_py.so, and its SONAME - the DT_NEEDED name
    LIBRARY_OUTPUT_DIRECTORY "${BF2_DICE_PY_OUTPUT_DIR}"
    # gnu89 (CMake spells it -std=gnu90): the dialect this 2003 C was written in and the one the
    # vendored configure was run under to produce pyconfig.h, so probes and compile agree.
    C_STANDARD 90
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
    # Every PyAPI_FUNC/PyAPI_DATA must be exported: on Unix pyport.h gives them no attribute, so
    # the ELF's dynamic symbol table relies on GCC's default visibility. Pin it so a global
    # -fvisibility=hidden in a parent project can never strip Py_Initialize from the .so.
    C_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(dice_py
    PUBLIC
        "${DICE_PY_PYCONFIG_DIR}"               # pyconfig.h (configure's output, see README.md)
        "${DICE_PY_SRC}/Include"
    PRIVATE
        "${DICE_PY_ZLIB}"                       # zlib.h for Modules/zlibmodule.c
    # Not needed, unlike the Windows build: PC/import_nt.c was the only unit that included
    # Python/importdl.h from outside Python/, and dynload_shlib.c lives in Python/ itself.
)
target_compile_definitions(dice_py PRIVATE
    Py_BUILD_CORE       # what Makefile.pre.in's PY_CFLAGS passes; exposes the core-only headers
    NDEBUG              # build_python.cmd is /O2 /DNDEBUG in every configuration; so is this
    BF2_HAVE_ZLIB       # registers zlib in config.c (build_python.cmd's switch; always on here)
    DEDICATED=1         # the image define every Code/BF2 compile line states (AGENTS.md); the
                        # force-included Core/BuildConfig.h below refuses to compile without it
)
target_compile_options(dice_py PRIVATE
    "-include${DICE_PY_BUILDCONFIG_H}"
    -fno-strict-aliasing    # upstream BASECFLAGS: the object system type-puns through PyObject*
    -fwrapv                 # intobject.c's overflow checks assume two's-complement wraparound
    -O2                     # fixed, like build_python.cmd's /O2, whatever the configuration
    -g
    -Wno-deprecated-declarations    # tmpnam/tempnam/siginterrupt: glibc deprecations postdating 2004
)
# GCC 14 turned four 2003-era C constructs into hard errors (implicit int, implicit function
# declaration, int/pointer conversion, mismatched return); these demote them back to what they
# were for the compilers this source was written for. Probed rather than assumed, so an older GCC
# or Clang that lacks a spelling (Clang has no -Wreturn-mismatch) does not warn on every unit.
include(CheckCCompilerFlag)
foreach(flag IN ITEMS
        -Wno-implicit-function-declaration
        -Wno-incompatible-pointer-types
        -Wno-int-conversion
        -Wno-implicit-int
        -Wno-return-mismatch)
    string(MAKE_C_IDENTIFIER "DICE_PY_HAS${flag}" flag_var)
    check_c_compiler_flag("${flag}" ${flag_var})
    if(${flag_var})
        target_compile_options(dice_py PRIVATE "$<$<COMPILE_LANGUAGE:C>:${flag}>")
    endif()
endforeach()

# What Makefile.pre.in passes to exactly these two units (its getpath.o and getplatform.o rules).
# The engine replaces sys.path after Py_Initialize (pylib-2.3.4.zip and python/ relative to the
# working directory), so these only have to be sane defaults that never fault. PREFIX/EXEC_PREFIX/
# VERSION are configure's values (/usr/local, 2.3). VPATH is "" rather than configure's "." because
# the block it enables probes for an in-tree build directory beside the executable, which this
# build never is. PYTHONPATH/PLATFORM say linux2 where this kernel's configure says linux6:
# [CHOSEN, not recovered] sys.platform is the retail-era value, the only one 2004 scripts could
# have tested; see README.md.
# [CHOSEN, not recovered] macOS keeps "linux2" too: the game's Python scripts were only ever run
# on win32 and linux2, so reporting the retail Linux value keeps their platform tests on the
# path the retail Linux server took. See ../macos/README.md.
set_source_files_properties("${DICE_PY_SRC}/Modules/getpath.c" PROPERTIES COMPILE_DEFINITIONS
    "PREFIX=\"/usr/local\";EXEC_PREFIX=\"/usr/local\";VERSION=\"2.3\";VPATH=\"\";PYTHONPATH=\":plat-linux2:lib-tk\"")
set_source_files_properties("${DICE_PY_SRC}/Python/getplatform.c" PROPERTIES COMPILE_DEFINITIONS
    "PLATFORM=\"linux2\"")

# [CHOSEN, not recovered] macOS: Modules/_localemodule.c's __APPLE__ branch includes Mac/Include's
# macglue.h and calls PyMac_getscript from Python/mactoolboxglue.c, the Carbon toolbox glue that
# upstream links only into a framework build. That glue is not part of this build, so the unit is
# compiled as the generic POSIX module it is on Linux: setlocale/localeconv/nl_langinfo, and no
# _getdefaultlocale (locale.py then reads the LANG/LC_* environment, as on the Linux server).
if(DICE_PY_MACOS)
    set_property(SOURCE "${DICE_PY_SRC}/Modules/_localemodule.c" APPEND PROPERTY COMPILE_OPTIONS
        "-U__APPLE__")
endif()

# System libraries: libm (mathmodule, timemodule), libdl (dynload_shlib), pthreads (thread.c) and,
# where glibc still ships it separately (before 2.34), libutil for posixmodule's openpty/forkpty.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
include(CheckFunctionExists)
check_function_exists(openpty DICE_PY_OPENPTY_IN_LIBC)
target_link_libraries(dice_py PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
if(NOT DICE_PY_OPENPTY_IN_LIBC)
    find_library(DICE_PY_UTIL_LIBRARY NAMES util REQUIRED)
    target_link_libraries(dice_py PRIVATE "${DICE_PY_UTIL_LIBRARY}")
endif()
# A shared object links even with unresolved symbols; refuse that, so a missing system library or
# a module compiled without its dependency fails HERE rather than when `bf2` first dlopens it.
# ld64 (macOS) already rejects undefined symbols in a dylib by default and has no --no-undefined.
if(NOT DICE_PY_MACOS)
    target_link_options(dice_py PRIVATE -Wl,--no-undefined)
endif()

# -------------------------------------------------------------------------------------------------
# The runtime check: the same pycheck.cpp the Windows build runs, linked against libdice_py.so and
# finding it beside itself through $ORIGIN, exactly as `bf2` will.
# -------------------------------------------------------------------------------------------------
add_executable(dice_py_check "${DICE_PY_CHECK_SOURCE}")
target_link_libraries(dice_py_check PRIVATE dice_py)
target_compile_definitions(dice_py_check PRIVATE DEDICATED=1)
target_compile_options(dice_py_check PRIVATE "-include${DICE_PY_BUILDCONFIG_H}")
if(DICE_PY_MACOS)
    set(DICE_PY_RPATH "@loader_path")             # the Mach-O spelling of $ORIGIN
else()
    set(DICE_PY_RPATH "\$ORIGIN")
endif()
set_target_properties(dice_py_check PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BF2_DICE_PY_OUTPUT_DIR}"
    BUILD_RPATH "${DICE_PY_RPATH}"
    INSTALL_RPATH "${DICE_PY_RPATH}"
    BUILD_WITH_INSTALL_RPATH TRUE
    CXX_STANDARD 14                             # the engine's standard; Python.h is consumed from C++14
    CXX_STANDARD_REQUIRED ON
)
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generators would otherwise append a per-configuration subdirectory.
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${config}" config_upper)
        set_target_properties(dice_py PROPERTIES
            "LIBRARY_OUTPUT_DIRECTORY_${config_upper}" "${BF2_DICE_PY_OUTPUT_DIR}")
        set_target_properties(dice_py_check PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${BF2_DICE_PY_OUTPUT_DIR}")
    endforeach()
endif()

# cPickle imports copy_reg, which imports types. Stage the two upstream library modules into the
# build tree (build_python.cmd does the same under OBJ\python\release\check\lib): Python 2.3
# writes copy_reg.pyc and types.pyc beside whatever it imports, and that must never be the
# vendored py-src-2.3.4/Lib.
set(DICE_PY_CHECK_DIR "${CMAKE_BINARY_DIR}/dice_py/check")
set(DICE_PY_CHECK_LIB_DIR "${DICE_PY_CHECK_DIR}/lib")
file(MAKE_DIRECTORY "${DICE_PY_CHECK_LIB_DIR}")
set(dice_py_check_staged "")
foreach(module IN ITEMS copy_reg types)
    add_custom_command(
        OUTPUT "${DICE_PY_CHECK_LIB_DIR}/${module}.py"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${DICE_PY_SRC}/Lib/${module}.py" "${DICE_PY_CHECK_LIB_DIR}/${module}.py"
        DEPENDS "${DICE_PY_SRC}/Lib/${module}.py"
        COMMENT "dice_py_check: staging Lib/${module}.py"
        VERBATIM
    )
    list(APPEND dice_py_check_staged "${DICE_PY_CHECK_LIB_DIR}/${module}.py")
endforeach()
add_custom_target(dice_py_check_lib DEPENDS ${dice_py_check_staged})
add_dependencies(dice_py_check dice_py_check_lib)

# Runs the check. It binds one loopback UDP socket on an ephemeral port and asserts the port is not
# 16567 (the owner's reserved port); it never listens on a fixed port.
add_custom_target(dice_py_check_run
    COMMAND dice_py_check "${DICE_PY_CHECK_LIB_DIR}"
    DEPENDS dice_py_check dice_py_check_lib
    WORKING_DIRECTORY "${DICE_PY_CHECK_DIR}"
    COMMENT "dice_py_check: starting libdice_py.so, importing every builtin, zlib + loopback UDP"
    USES_TERMINAL
    VERBATIM
)

list(LENGTH dice_py_sources dice_py_source_count)
list(LENGTH dice_py_zlib_sources dice_py_zlib_count)
list(LENGTH dice_py_dropped dice_py_dropped_count)
message(STATUS
    "dice_py: libdice_py.so from ${dice_py_source_count} CPython 2.3.4 units "
    "(${dice_py_dropped_count} Windows/32-bit-only manifest entries dropped, 3 Unix units added) "
    "+ ${dice_py_zlib_count} zlib units -> ${BF2_DICE_PY_OUTPUT_DIR}")
message(STATUS "dice_py: dice_py_check stages copy_reg.py/types.py in ${DICE_PY_CHECK_LIB_DIR}")
