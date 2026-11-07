#!/bin/bash
# [TEMP] [CHOSEN, not recovered] Run only from the prepared out/m2/tsan bundle.
set -euo pipefail
bundle=$(cd "$(dirname "$0")" && pwd)
cd "$bundle"
run_checks() {
architecture=$(/usr/bin/uname -m)
case "$architecture" in arm64|x86_64) ;; *) echo "Unsupported architecture: $architecture"; exit 2 ;; esac
echo "S2.10 TSan checks on $architecture"
/usr/bin/sw_vers
test -x "$bundle/$architecture/bf2"
test -x "$bundle/$architecture/s2_10_boundaries"
test -f "$bundle/root/smoke_cfg/ServerSettings.con"
# The profile is tested before any engine/fixture is launched. A nested-sandbox refusal stops here.
/usr/bin/sandbox-exec -f "$bundle/offline.sb" /usr/bin/true
check_ports() {
    if /usr/sbin/lsof -nP -iUDP:16890 -iTCP:16890 -iUDP:29911 -t 2>/dev/null | /usr/bin/grep -q .; then
        echo "Port 16890 or query port 29911 already in use; nothing stopped."; return 1
    fi
}
settings="$bundle/root/smoke_cfg/ServerSettings.con"
# Refuse a modified bundle rather than relying on default settings or a last-assignment override.
for setting in 'sv.serverPort 16890' 'sv.internet 0' 'sv.serverIP "127.0.0.1"' \
               'sv.interfaceIP "127.0.0.1"' 'sv.gameSpyPort 29911' 'sv.voipEnabled 0' \
               'sv.useGlobalRank 0' 'sv.useGlobalUnlocks 0' 'sv.autoRecord 0'; do
    key=${setting%% *}
    actual=$(/usr/bin/awk -v key="$key" '$1 == key {sub(/\r$/, ""); print}' "$settings")
    test "$actual" = "$setting" || { echo "Unsafe or duplicate setting: $key"; exit 2; }
done
check_ports
mkdir -p "$bundle/checks-$architecture"
cd "$bundle/checks-$architecture"
TSAN_OPTIONS="halt_on_error=0:exitcode=66:symbolize=0:log_path=$bundle/tsan-unit" \
    /usr/bin/sandbox-exec -f "$bundle/offline.sb" "$bundle/$architecture/s2_10_boundaries"
TSAN_OPTIONS="halt_on_error=0:exitcode=66:symbolize=0:log_path=$bundle/tsan-audio" \
    /usr/bin/sandbox-exec -f "$bundle/offline.sb" "$bundle/$architecture/s2_10_audio_worker"
cd "$bundle"
for run in 1 2; do
    check_ports
    /usr/bin/expect "$bundle/smoke.tcl" "$bundle" "$architecture" "$run"
    check_ports
done
echo "Completed: generated UTF-16/packet/atomic checks and two offline map-load/tick/quit runs."
echo "Inspect tsan-* reports and server-*.log; this does not exercise client joins, reloads or VoIP."
}
run_checks 2>&1 | /usr/bin/tee "$bundle/results.txt"
