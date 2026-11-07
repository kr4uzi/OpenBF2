#!/bin/sh
# [TEMP] [CHOSEN, not recovered] CPU/file-only test; never launches a client or server.
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
CHECK_OUT="$ROOT/out/build/r1-registry-file/registry-check"
mkdir -p "$CHECK_OUT"
for IMAGE in 0 1; do
    if [ "$IMAGE" = 0 ]; then
        c++ -arch x86_64 -std=gnu++14 -DDEDICATED=0 \
            -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
            -include "$ROOT/Code/BF2/Core/PlatformCompat.h" \
            "$ROOT/Code/Tests/posix_registry/registry_check.cpp" \
            "$ROOT/Code/BF2/Core/Registry.cpp" \
            "$ROOT/Code/BF2/Core/StringUtil.cpp" \
            "$ROOT/Code/BF2/Game/MatchMaking/Client/CdKeyStore.cpp" \
            "$ROOT/Code/BF2/Platform/macOS/ProtectedData.cpp" \
            "$ROOT/Code/BF2/Platform/macOS/ClientPaths.cpp" \
            -o "$CHECK_OUT/client_check"
        CHECK_HOME=$(mktemp -d "$CHECK_OUT/home.XXXXXX")
        HOME="$CHECK_HOME" "$CHECK_OUT/client_check"
    else
        c++ -arch x86_64 -std=gnu++14 -DDEDICATED=1 \
            -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
            -include "$ROOT/Code/BF2/Core/PlatformCompat.h" \
            "$ROOT/Code/Tests/posix_registry/registry_check.cpp" \
            "$ROOT/Code/BF2/Core/Registry.cpp" -o "$CHECK_OUT/server_check"
        "$CHECK_OUT/server_check"
    fi
done
