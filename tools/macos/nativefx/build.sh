#!/bin/sh
# [CHOSEN, not recovered] Builds the native effect tool chain: vkd3d-shader (patched, LGPL),
# MojoShader (s1c patches, zlib) and the nativefx harness. Offline; everything under out/.
set -eu
cd "$(dirname "$0")/../../.."
root=$PWD
# [CHOSEN, not recovered] Keep the historical x86_64 location; other targets are isolated.
arch=${1:-x86_64}
case "$arch" in
    x86_64) out=out/build/nativefx ;;
    arm64|universal) out=out/build/nativefx-$arch ;;
    *) echo "Usage: $0 [x86_64|arm64|universal]" >&2; exit 2 ;;
esac
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-11.0}
mkdir -p "$out/logs"
for source in out/third_party/vkd3d/configure out/third_party/mojoshader/mojoshader.h; do
    test -f "$source" || { echo "Missing local native source: $source (no download/install attempted)" >&2; exit 2; }
done
if [ "$arch" = universal ]; then
    sh tools/macos/nativefx/build.sh x86_64 > /dev/null
    sh tools/macos/nativefx/build.sh arm64 > /dev/null
    sh tools/macos/nativefx/build-vkd3d.sh universal > /dev/null
    mkdir -p "$out/mojo" "$out/mojo-src"
    rsync -a out/build/nativefx/mojo-src/ "$out/mojo-src/"
    lipo -create out/build/nativefx/mojo/libmojoshader.a out/build/nativefx-arm64/mojo/libmojoshader.a \
        -output "$out/mojo/libmojoshader.a"
    target_archs='arm64;x86_64'
else
    sh tools/macos/nativefx/build-vkd3d.sh "$arch" > /dev/null
    target_archs=$arch
fi
if [ ! -f "$out/mojo-src/mojoshader.h" ]; then
    rsync -a --exclude .git out/third_party/mojoshader/ "$out/mojo-src/"
fi
python3 tools/macos/nativefx/patch-mojo.py "$out/mojo-src" "$out/mojo-patches.json"
if [ "$arch" != universal ]; then
    cmake -S "$out/mojo-src" -B "$out/mojo" -G 'Unix Makefiles' \
        -DCMAKE_OSX_ARCHITECTURES="$target_archs" -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" \
        -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_BUILD_TYPE=Release \
        -DPROFILE_METAL=ON -DPROFILE_BYTECODE=ON -DEFFECT_SUPPORT=ON \
        -DFLIP_VIEWPORT=OFF -DDEPTH_CLIPPING=OFF -DXNA4_VERTEXTEXTURE=OFF -DGIT=GIT-NOTFOUND > "$out/logs/mojo-configure.log" 2>&1
    cmake --build "$out/mojo" --target mojoshader -j6 > "$out/logs/mojo-build.log" 2>&1
fi
cmake -S tools/macos/nativefx -B "$out/tool" -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_OSX_ARCHITECTURES="$target_archs" -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" -DBF2_REPO="$root" \
    -DVKD3D_SRC="$root/$out/vkd3d-src" -DVKD3D_BUILD="$root/$out/vkd3d" \
    -DMOJO_SRC="$root/$out/mojo-src" -DMOJO_LIB="$root/$out/mojo/libmojoshader.a" > "$out/logs/tool-configure.log" 2>&1
cmake --build "$out/tool" -j4 > "$out/logs/tool-build.log" 2>&1 || { cat "$out/logs/tool-build.log" >&2; exit 1; }
test -x "$out/tool/nativefx" && echo "$root/$out/tool/nativefx"
