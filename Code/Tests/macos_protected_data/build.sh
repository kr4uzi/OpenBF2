#!/bin/sh
# [TEMP] Builds and runs protected_data_check with HOME under out/ (the key file lands there).
set -e
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
OUT=$ROOT/out/s258-protected-data
mkdir -p "$OUT/home"
c++ -arch x86_64 -std=gnu++14 -DDEDICATED=0 -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
    -include "$ROOT/Code/BF2/Core/PlatformCompat.h" -o "$OUT/protected_data_check" \
    "$ROOT/Code/Tests/macos_protected_data/protected_data_check.cpp" \
    "$ROOT/Code/BF2/Game/PasswordCodec.cpp" "$ROOT/Code/BF2/Game/MatchMaking/Client/CdKeyStore.cpp" \
    "$ROOT/Code/BF2/Core/Registry.cpp" "$ROOT/Code/BF2/Core/StringUtil.cpp" \
    "$ROOT/Code/BF2/Platform/macOS/ProtectedData.cpp" \
    "$ROOT/Code/BF2/Platform/macOS/ClientPaths.cpp"
HOME="$OUT/home" "$OUT/protected_data_check"
