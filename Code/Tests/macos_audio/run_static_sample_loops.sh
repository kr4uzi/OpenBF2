#!/bin/sh
# [TEMP] Builds and runs static_sample_loops against an existing macos-x64 build directory (after
# `cmake --build <dir> --target bf2_bf2audio_static bf2_vorbis_static bf2_plugin_kit_static`, which
# bf2_client builds). The M16 first-person fire WAV is read from the game's Objects_client.zip into a
# temporary directory; nothing from the game is kept. No audio device is opened.
# Usage: run_static_sample_loops.sh <build dir> <game mods/bf2 dir> [-v] [-q<quality>] [--real]
# --real links no fake: the real OpenAL.framework plays with every gain forced to 0 (opens the
# default output device, silently) and the framework's source counters are printed. Opt-in only.
set -e
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
B=${1:?build directory}
MOD=${2:?game mods/bf2 directory}
shift 2
REAL=""
ARGS=""
for a in "$@"; do
    if [ "$a" = "--real" ]; then REAL="-DBF2_TEST_REAL_OPENAL=1"; else ARGS="$ARGS $a"; fi
done
mkdir -p "$B/macos_audio"
c++ $REAL -arch x86_64 -std=gnu++14 -DDEDICATED=0 -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
    -include "$ROOT/Code/BF2/Core/PlatformCompat.h" -o "$B/macos_audio/static_sample_loops" \
    "$ROOT/Code/Tests/macos_audio/static_sample_loops.cpp" \
    -Wl,-force_load,"$B/libbf2_bf2audio_static.a" "$B/libbf2_vorbis_static.a" \
    "$B/libbf2_plugin_kit_static.a" -framework CoreAudio -framework CoreFoundation
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
unzip -p "$MOD/Objects_client.zip" Weapons/Handheld/USRIF_M203/Sounds/M16_fire_1p_outdoor.wav \
    > "$TMP/M16_fire_1p_outdoor.wav"
"$B/macos_audio/static_sample_loops" "$TMP/M16_fire_1p_outdoor.wav" $ARGS
