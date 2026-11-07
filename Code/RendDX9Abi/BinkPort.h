// [CHOSEN, not recovered] Port-only seams of the Bink stand-in (BinkFFmpeg.cpp) used by the macOS
// client and Windows x64. The game reaches the stand-in only through the ten Bink entry points;
// bf2_bink_decode_test and the black-box harness (Code/Tests/movies) use the hooks, and the audio
// backends (BinkAudioCoreAudio.cpp, BinkAudioDirectSound.cpp) implement BinkPortAudioOutput.
#pragma once

#include "Bink.h"

#if BF2_BINK_PORT
#include <cstddef>

#include "../BF2/External/ffmpeg-bink/shim/ffbink.h"

namespace renddx9abi {

// The YUV picture of the last BinkDoFrame, or false before the first one.
bool binkPortPicture(HBINK bnk, FFBinkPicture* out);

// When set, decoded Bink audio goes to the tap instead of the sound output (also when no sound
// system was selected). Planar float samples, one call per decoded block.
typedef void (*BinkPortAudioTap)(void* user, const float* const* planes, int channels,
                                 int samples, int sampleRate);
void binkPortSetAudioTap(BinkPortAudioTap tap, void* user);

// One movie's decoded audio, as the output pulls it: interleaved float, [-1, 1] nominal. pull
// fills `frames` frames and answers how many came from the movie; the rest is silence
// (underrun). Called from the output's own thread or callback.
struct BinkPortAudioSource
{
    virtual size_t pull(float* out, size_t frames) = 0;
protected:
    ~BinkPortAudioSource() {}
};

// One movie's sound device. playedFrames is the device play position in frames since open; it
// only advances while running.
struct BinkPortAudioOutput
{
    virtual ~BinkPortAudioOutput() {}
    virtual bool open(int sampleRate, int channels, BinkPortAudioSource* source) = 0;
    virtual void setRunning(bool run) = 0;
    virtual void close() = 0;
    virtual unsigned long long playedFrames() = 0;
};

// The platform backend: AudioQueue on macOS (BinkAudioCoreAudio.cpp), DirectSound on Windows x64
// (BinkAudioDirectSound.cpp).
BinkPortAudioOutput* binkPortCreateSystemAudioOutput();

// Test hook: movies opened afterwards use the factory's output instead (0 restores the backend).
typedef BinkPortAudioOutput* (*BinkPortAudioOutputFactory)();
void binkPortSetAudioOutput(BinkPortAudioOutputFactory factory);

} // namespace renddx9abi
#endif
