#!/bin/bash
# [TEMP] [CHOSEN, not recovered] Offline stage 1a validation; no build tools required.
set -euo pipefail
bundle=$(cd "$(dirname "$0")" && pwd -P)
content=${1:?usage: ./run.sh '/path/to/Battlefield 2'}
content=$(cd "$content" && pwd -P)
case "$bundle/" in "$content/"*) echo 'Bundle must be outside the read-only installation' >&2; exit 1;; esac
exec > "$bundle/results.txt" 2>&1
trap 'echo "run.sh exit: $?"' EXIT
echo '[TEMP] [CHOSEN, not recovered] OpenBF2 stage 1a'
date -u
/usr/bin/uname -m
/usr/sbin/sysctl hw.model || true
echo "Content (read-only): $content"
echo "Bundle: $bundle"
cat "$bundle/artifacts.txt"
status=0
set +e
# This is the dice_py_check_run target's executable and staged Python library input.
(cd "$bundle" && ./bin/dice_py_check "$bundle/check-lib") > "$bundle/python.log" 2>&1
rc=$?
echo "dice_py_check_run exit: $rc"
tail -50 "$bundle/python.log"
if [ "$rc" -ne 0 ]; then status=1; fi
(cd "$bundle/bin" && ./bf2_aidll_loader_check) > "$bundle/aidll.log" 2>&1
rc=$?
echo "bf2_aidll_loader_check exit: $rc"
tail -50 "$bundle/aidll.log"
if [ "$rc" -ne 0 ]; then status=1; fi
/usr/bin/sandbox-exec -D "REFERENCE=$content" -f "$bundle/offline.sb" /usr/bin/true > "$bundle/sandbox.log" 2>&1
rc=$?
set -e
echo "sandbox preflight exit: $rc"
cat "$bundle/sandbox.log"
if [ "$rc" -ne 0 ]; then
    echo 'Server smoke NOT RUN: sandbox-exec unavailable. No unsandboxed fallback.'
    exit 1
fi
# Refuse an occupied game/query port; never stop another process to free it.
for port in 16890 29911; do
    set +e
    /usr/sbin/lsof -nP -iUDP:"$port" > "$bundle/port-$port.log" 2>&1
    rc=$?
    set -e
    if [ "$rc" -ne 1 ] || [ -s "$bundle/port-$port.log" ]; then
        cat "$bundle/port-$port.log"
        echo "Server smoke NOT RUN: UDP $port occupied or cannot establish availability (lsof $rc)"
        exit 1
    fi
done
if [ ! -d "$bundle/root" ]; then
    for item in mods python pylib-2.3.4.zip; do
        test -e "$content/$item"
    done
    # Publish a complete clone only; a failed copy never becomes a runnable content root.
    staging=$(mktemp -d "$bundle/clone.XXXXXX")
    for item in mods python pylib-2.3.4.zip; do
        /bin/cp -Rc "$content/$item" "$staging/"
    done
    mv "$staging" "$bundle/root"
fi
mkdir -p "$bundle/root/smoke_cfg"
cat > "$bundle/root/smoke_cfg/ServerSettings.con" <<'SETTINGS'
sv.serverName "stage1a offline smoke"
sv.serverPort 16890
sv.internet 0
sv.serverIP "127.0.0.1"
sv.interfaceIP "127.0.0.1"
sv.gameSpyPort 29911
sv.allowNATNegotiation 0
sv.punkBuster 0
sv.voipEnabled 0
sv.useGlobalRank 0
sv.useGlobalUnlocks 0
sv.autoRecord 0
sv.maxPlayers 16
sv.numPlayersNeededToStart 2
SETTINGS
echo 'mapList.append dalian_plant gpm_cq 16' > "$bundle/root/smoke_cfg/maplist.con"
set +e
/usr/bin/expect "$bundle/smoke.tcl" "$bundle" "$content" > "$bundle/server.log" 2>&1
rc=$?
set -e
echo "60 s server smoke + quit exit: $rc"
tail -50 "$bundle/server.log"
if [ "$rc" -ne 0 ]; then status=1; fi
echo "Overall exit: $status"
echo 'A clean exit alone does not prove map load; inspect server.log for playing and frontiers.'
exit "$status"
