#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Content-free production-container check.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$ROOT/out/swf-hashsize/hash-check"
B="$ROOT/Code/BF2"
mkdir -p "$OUT"
c++ -arch "${ARCH:-x86_64}" -std=gnu++14 -O2 -g -DDEDICATED=0 -DBF2_PLATFORM_POSIX -D_MACOSX \
    -fno-strict-aliasing -fwrapv -fms-extensions -w \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -include "$B/Core/BuildConfig.h" -include "$B/Core/PlatformCompat.h" \
    -I"$B/External" -I"$B/External/base" \
    "$ROOT/Code/Tests/swf_hash_capacity.cpp" -o "$OUT/swf_hash_capacity"
"$OUT/swf_hash_capacity"
