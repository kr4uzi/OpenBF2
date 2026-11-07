#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Builds the production gameswf container with the client's
# force-includes and optimization, then runs the case-insensitive hash check.
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
OUT="$ROOT/out/build/gameswf-stringi-check"
B="$ROOT/Code/BF2"
mkdir -p "$OUT"
c++ -arch "${ARCH:-x86_64}" -std=gnu++14 -O2 -DBF2_PLATFORM_POSIX -DDEDICATED=0 -D_MACOSX \
    -fno-strict-aliasing -fwrapv -fms-extensions -fpermissive -w \
    -include "$B/Core/BuildConfig.h" -include "$B/Core/PlatformCompat.h" \
    -I"$B/External" -I"$B/External/base" \
    "$ROOT/Code/Tests/macos_gameswf_stringi/stringi_hash_check.cpp" \
    "$B/External/base/container.cpp" "$B/External/base/utf8.cpp" -o "$OUT/stringi_hash_check"
"$OUT/stringi_hash_check"
