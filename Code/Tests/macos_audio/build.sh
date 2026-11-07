#!/bin/sh
# [TEMP] Builds bf2audio_play against an existing macos-x64 build directory (default
# out/build/s258-x64), after `cmake --build <dir> --target bf2_bf2audio_static bf2_vorbis_static
# bf2_plugin_kit_static`. Output: <dir>/macos_audio/bf2audio_play.
set -e
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
B=${1:-$ROOT/out/build/s258-x64}
mkdir -p "$B/macos_audio"
c++ -arch x86_64 -std=gnu++14 -DDEDICATED=0 -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
    -include "$ROOT/Code/BF2/Core/PlatformCompat.h" -o "$B/macos_audio/bf2audio_play" \
    "$ROOT/Code/Tests/macos_audio/bf2audio_play.cpp" \
    -Wl,-force_load,"$B/libbf2_bf2audio_static.a" "$B/libbf2_vorbis_static.a" \
    "$B/libbf2_plugin_kit_static.a" -framework CoreAudio -framework CoreFoundation
