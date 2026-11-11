#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Content-free native tessellator check.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$ROOT/out/swf-tess-check"
B="$ROOT/Code/BF2"
mkdir -p "$OUT"
c++ -arch "${ARCH:-x86_64}" -isysroot "$(xcrun --show-sdk-path)" -std=gnu++14 -O2 -g \
    -DDEDICATED=0 -DBF2_PLATFORM_POSIX -D_MACOSX \
    -fno-strict-aliasing -fwrapv -fms-extensions \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -include "$B/Core/BuildConfig.h" -include "$B/Core/PlatformCompat.h" \
    -I"$B/External" -I"$B/External/base" \
    "$ROOT/Code/Tests/swf_tesselate_runtime.cpp" -o "$OUT/swf_tesselate_runtime"
"$OUT/swf_tesselate_runtime"
