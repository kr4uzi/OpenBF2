#!/bin/sh
# [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: menu-interact-runtime]
# Builds and runs Code/Tests/menu_interact_runtime.cpp on macOS (x86_64 or arm64 host).
# Usage: Code/Tests/menu_interact_runtime.sh <out-dir> [jobs]
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:?out dir}
JOBS=${2:-2}
mkdir -p "$OUT"
BF2="$ROOT/Code/BF2"
FLAGS="-std=gnu++14 -O1 -g -DBF2_PLATFORM_POSIX -DDEDICATED=0 -D_ITERATOR_DEBUG_LEVEL=0 -D_MACOSX
  -fms-extensions -fpermissive -Wno-narrowing -Wno-unknown-pragmas -Wno-multichar
  -Wno-deprecated-declarations -Wno-write-strings -fno-strict-aliasing -fwrapv
  -include $BF2/Core/BuildConfig.h -include $BF2/Core/PlatformCompat.h"
SOURCES="Game/ProfileManager Core/ClassManager Core/StringUtil Core/Debug IO/File/UnixFileSystem IO/File/UnixFileStream"
n=0
for s in $SOURCES; do
  c++ $FLAGS -c "$BF2/$s.cpp" -o "$OUT/$(echo "$s" | tr / _).o" &
  n=$((n + 1))
  if [ "$n" -ge "$JOBS" ]; then wait; n=0; fi
done
wait
c++ $FLAGS -c "$ROOT/Code/Tests/menu_interact_runtime.cpp" -o "$OUT/fixture.o"
c++ -o "$OUT/menu_interact_runtime" "$OUT"/*.o
"$OUT/menu_interact_runtime"
