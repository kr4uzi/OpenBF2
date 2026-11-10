#!/bin/sh
# [CHOSEN, not recovered] Developer oracle, not used by the client.
# Offline build from the existing local s1c MojoShader copy.
set -eu
cd "$(dirname "$0")/../../.."
root=$PWD
build="$root/out/build/s2-fx-runtime"
source="$root/out/third_party/mojoshader"
mkdir -p "$build"
test -f "$source/mojoshader.h" || { echo "Missing local MojoShader; no download will be attempted" >&2; exit 2; }
python3 tools/macos/d3dxfx/patch-mojo.py "$source" "$build/patches.json"
cmake -S "$source" -B "$build/mojo" -G 'Unix Makefiles' \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_BUILD_TYPE=Release \
    -DPROFILE_METAL=ON -DPROFILE_BYTECODE=ON -DEFFECT_SUPPORT=ON \
    -DFLIP_VIEWPORT=OFF -DDEPTH_CLIPPING=OFF -DXNA4_VERTEXTEXTURE=OFF \
    > "$build/mojo-configure.log" 2>&1
cmake --build "$build/mojo" --target mojoshader -j"${JOBS:-2}" > "$build/mojo-build.log" 2>&1
cmake --preset macos-x64 -B "$build" \
    -DBF2_FX_DEVELOPER_ORACLE=ON \
    -DBF2_FX_MOJOSHADER_SOURCE="$source" -DBF2_FX_MOJOSHADER_LIBRARY="$build/mojo/libmojoshader.a"
cmake --build "$build" --target bf2_fx_cache_tool macos_fx_runtime -j"${JOBS:-2}"
"${MINGW_CC:-/usr/local/bin/i686-w64-mingw32-gcc}" -std=c99 -O2 -Wall -Wextra -Werror -static-libgcc \
    tools/macos/d3dxfx/fxcompile.c -o "$build/fxcompile.exe"
python3 tools/macos/d3dxfx/build-cache.py --record-toolchain
