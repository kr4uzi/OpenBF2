#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Runs matrix-oracle.exe with BF2's d3dx9_25 in a fresh prefix under out/.
set -eu
cd "$(dirname "$0")/../../../.."
APP="$HOME/Applications/Battlefield 2.app/Contents/SharedSupport"
W="$PWD/out/matrix-oracle"; rm -rf "$W"; mkdir -p "$W/prefix/home" "$W/prefix/tmp" "$W/prefix/cache"
echo "matrix oracle only" > "$W/prefix/.openbf2-s2"
/usr/local/bin/i686-w64-mingw32-gcc -std=c99 -O1 -Wall tools/macos/d3dxfx/oracle/matrix-oracle.c -o "$W/matrix-oracle.exe"
cp "$APP/prefix/drive_c/Battlefield 2/d3dx9_25.dll" "$W/"; cp tools/macos/d3dxfx/oracle/matrices.fx "$W/"
cd "$W"
env WINEPREFIX="$W/prefix" WINEARCH=win64 WINEDEBUG=-all WINEDLLOVERRIDES="d3dx9_25=n;mscoree,mshtml=" \
    HOME="$W/prefix/home" TMPDIR="$W/prefix/tmp" XDG_CACHE_HOME="$W/prefix/cache" \
    DYLD_LIBRARY_PATH="$APP/../Frameworks" "$APP/wine/bin/wine64" matrix-oracle.exe matrices.fx 2>/dev/null | tr -d '\r'
env WINEPREFIX="$W/prefix" "$APP/wine/bin/wineserver" -k 2>/dev/null || true
