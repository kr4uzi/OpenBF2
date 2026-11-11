#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Runs struct-oracle.exe with BF2's d3dx9_25.dll in a fresh,
# marked Wine prefix under out/ (same environment as build-cache.py). Prints D3DX's answers.
set -eu
cd "$(dirname "$0")/../../../.."
APP="$HOME/Applications/Battlefield 2.app/Contents/SharedSupport"
W="$PWD/out/struct-oracle"; rm -rf "$W"; mkdir -p "$W/prefix/home" "$W/prefix/tmp" "$W/prefix/cache"
echo "struct oracle only" > "$W/prefix/.openbf2-s2"
/usr/local/bin/i686-w64-mingw32-gcc -std=c99 -O1 -Wall tools/macos/d3dxfx/oracle/struct-oracle.c -o "$W/struct-oracle.exe"
cp "$APP/prefix/drive_c/Battlefield 2/d3dx9_25.dll" "$W/"; cp tools/macos/d3dxfx/oracle/structs.fx "$W/"
cd "$W"
env WINEPREFIX="$W/prefix" WINEARCH=win64 WINEDEBUG=-all WINEDLLOVERRIDES="d3dx9_25=n;mscoree,mshtml=" \
    HOME="$W/prefix/home" TMPDIR="$W/prefix/tmp" XDG_CACHE_HOME="$W/prefix/cache" \
    DYLD_LIBRARY_PATH="$APP/../Frameworks" "$APP/wine/bin/wine64" struct-oracle.exe structs.fx 2>/dev/null | tr -d '\r'
env WINEPREFIX="$W/prefix" "$APP/wine/bin/wineserver" -k 2>/dev/null || true
