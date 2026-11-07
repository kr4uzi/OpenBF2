#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Offline milestone-1 sandbox; no retail content.
# [RETIRE-WHEN: n1-loopback] Retire when G6 automates this lifecycle.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)
build=${1:-"$root/out/build/n1-posix-net"}
profile="$root/Code/Tests/macos_stage1a/offline.sb"
# Fail closed if the agent sandbox refuses nested sandbox-exec.
sandbox-exec -D "REFERENCE=$HOME/Applications" -f "$profile" /usr/bin/true
exec sandbox-exec -D "REFERENCE=$HOME/Applications" -f "$profile" "$build/bin_macos/n1_loopback"
