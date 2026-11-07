#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Engine-free checks of the production macOS path/lock helpers.
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
OUT="$ROOT/out/build/s2-11-startup/path-check"
mkdir -p "$OUT"
c++ -arch x86_64 -std=gnu++14 -DDEDICATED=0 \
    -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
    -include "$ROOT/Code/BF2/Core/PlatformCompat.h" \
    "$ROOT/Code/Tests/macos_client_paths/client_paths_check.cpp" \
    "$ROOT/Code/BF2/Platform/macOS/ClientPaths.cpp" -o "$OUT/client_paths_check"
TEST_HOME=$(mktemp -d "$OUT/home.XXXXXX")
HOME="$TEST_HOME" "$OUT/client_paths_check"
