#!/bin/bash
# [TEMP] [RETIRE-WHEN: terrain-black-hexagons] One windowed run of a test folder with the terrain
# colour-pass depth switches (no trace), for the black mid-range terrain patches. Copy it next to
# preflight.sh and start it from Terminal, e.g.:
#   ./terrain-depth-test.sh                   no switch (baseline)
#   ./terrain-depth-test.sh always            drawFinal draws with D3DRS_ZFUNC = ALWAYS
#   ./terrain-depth-test.sh bias -0.00001     D3DRS_DEPTHBIAS for drawFinal
#   ./terrain-depth-test.sh bias -0.00001 slope -1   plus D3DRS_SLOPESCALEDEPTHBIAS
# Look at the spot where the patches show, take a screenshot, quit. Log: runs/terrain-depth-*.log.
HERE=$(cd "$(dirname "$0")" && pwd); E=$(cd "$HERE/.." && pwd)
SWITCHES=(); TAG=baseline
while [ $# -gt 0 ]; do
  case "$1" in
    always) SWITCHES+=("BF2_TERRAIN_FINAL_ZFUNC=always"); TAG="${TAG/baseline/}always"; shift ;;
    bias)   [ -n "$2" ] || { echo "bias needs a value" >&2; exit 2; }
            SWITCHES+=("BF2_TERRAIN_FINAL_DEPTH_BIAS=$2"); TAG="${TAG/baseline/}bias$2"; shift 2 ;;
    slope)  [ -n "$2" ] || { echo "slope needs a value" >&2; exit 2; }
            SWITCHES+=("BF2_TERRAIN_FINAL_SLOPE_BIAS=$2"); TAG="${TAG/baseline/}slope$2"; shift 2 ;;
    *) echo "usage: $0 [always] [bias <float>] [slope <float>]" >&2; exit 2 ;;
  esac
done
mkdir -p "$HERE/runs"
LOG="$HERE/runs/terrain-depth-$(date +%Y%m%d-%H%M%S)-$TAG.log"
"$HERE/preflight.sh" "$HERE" > /dev/null || { echo "terrain-depth-test: preflight failed, not starting" >&2; exit 2; }
cd "$HERE/root" || exit 1
env HOME="$HERE/home" "${SWITCHES[@]}" /usr/bin/sandbox-exec -D "OWNER_HOME=$HOME" -D "RUN_OUT=$E" \
  -D "GAME_ROOT=$(cat "$HERE/game-root.txt")" -D "SUPPORT_ROOT=$HOME/Library/Application Support/OpenBF2" \
  -f ../e2e.sb ../bin/bf2_client +modPath mods/bf2 +fullscreen 0 +noSound 1 +restart 1 > "$LOG" 2>&1
echo "done ($TAG); log: $LOG"
grep -m1 '^\[TT\] colour-pass depth switches' "$LOG" || echo "(no switch line: baseline run)"
