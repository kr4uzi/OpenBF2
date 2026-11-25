#!/bin/bash
# [TEMP] [RETIRE-WHEN: terrain-black-hexagons] One windowed run of a test folder with the terrain
# draw trace on (BF2_TERRAIN_TRACE=1), for the black mid-range terrain hexagons. Copy it into the
# test folder next to preflight.sh and start it from Terminal; click the window once if it does
# not come to the front by itself. Go to a spot where the black patches show (e.g.
# Strike_at_Karkand), look at them for about 10 s without moving, then quit.
# Log: ~/Desktop/OpenBF2-terrain-trace.log ([TT] lines are the trace; pair.py reads them).
HERE=$(cd "$(dirname "$0")" && pwd); E=$(cd "$HERE/.." && pwd)
LOG="$HOME/Desktop/OpenBF2-terrain-trace.log"
"$HERE/preflight.sh" "$HERE" > /dev/null || { echo "terrain-trace: preflight failed, not starting" >&2; exit 2; }
cd "$HERE/root" || exit 1
env HOME="$HERE/home" BF2_TERRAIN_TRACE=1 /usr/bin/sandbox-exec -D "OWNER_HOME=$HOME" -D "RUN_OUT=$E" \
  -D "GAME_ROOT=$(cat "$HERE/game-root.txt")" -D "SUPPORT_ROOT=$HOME/Library/Application Support/OpenBF2" \
  -f ../e2e.sb ../bin/bf2_client +modPath mods/bf2 +fullscreen 0 +noSound 1 +restart 1 > "$LOG" 2>&1
echo "done; log: $LOG ($(grep -c '^\[TT\] frame=' "$LOG") traced frames, $(grep -c '^\[TT\] T ' "$LOG") terrain draws, $(grep -c '^\[TT\] F ' "$LOG") other depth-writing draws)"
