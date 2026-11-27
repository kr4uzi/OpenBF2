#!/bin/sh
# [TEMP] [CHOSEN, not recovered] macOS runner for the real AudioSettings fixture.
# The executable intercepts only the memory query and opens no audio device.
set -eu
test "$(uname -s)" = Darwin
AUDIO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
AUDIO_BUILD=${1:-"$AUDIO_ROOT/out/build/audio-settings-runtime"}
mkdir -p "$AUDIO_BUILD"
AUDIO_BUILD=$(cd "$AUDIO_BUILD" && pwd)
cd "$AUDIO_ROOT"
c++ -std=gnu++14 -arch x86_64 -O2 -g -fms-extensions \
    -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks -ffp-contract=off \
    -DDEDICATED=0 -DBF2_PLUGIN_ABI_NATIVE=1 \
    -include Code/BF2/Core/BuildConfig.h -include Code/BF2/Core/PlatformCompat.h \
    Code/Tests/audio_settings_runtime.cpp Code/BF2/Game/AudioSettings.cpp \
    Code/BF2/Sound/BfSound.cpp Code/BF2/Sound/NullSound.cpp \
    Code/BF2/Core/ClassManager.cpp Code/BF2/Core/Debug.cpp Code/BF2/Core/StringUtil.cpp \
    Code/BF2/Core/ConsoleObjectBase.cpp Code/BF2/Game/Utility/Vars.cpp \
    Code/BF2/Scene/Object/ObjectTemplateUtil.cpp Code/BF2/Math/Vec3.cpp \
    -Wl,-dead_strip -o "$AUDIO_BUILD/audio_settings_runtime"
"$AUDIO_BUILD/audio_settings_runtime"
