#!/bin/bash
# [TEMP] [CHOSEN, not recovered] Stage the owner's offline M2 validation bundle.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../.." && pwd -P)
build=${1:?usage: package.sh BUILD_DIRECTORY [BUNDLE_DIRECTORY]}
dest=${2:-"$repo/out/m2/stage1a"}
mkdir -p "$dest/bin" "$dest/check-lib"
for name in bf2 dice_py_check bf2_aidll_loader_check libdice_py.dylib libaidll.dylib; do
    cp "$build/bin_macos/$name" "$dest/bin/$name"
done
for name in copy_reg types; do
    cp "$repo/Code/BF2/External/python-embedded/py-src-2.3.4/Lib/$name.py" "$dest/check-lib/"
done
for name in run.sh smoke.tcl offline.sb README.txt; do
    cp "$repo/Code/Tests/macos_stage1a/$name" "$dest/"
done
chmod +x "$dest/run.sh"
# Record slices and dependencies without requiring Command Line Tools on the M2.
{
    git -C "$repo" rev-parse HEAD
    for artifact in "$dest"/bin/*; do
        file "$artifact"
        otool -L "$artifact"
    done
} > "$dest/artifacts.txt"
echo "Bundle staged at $dest"
