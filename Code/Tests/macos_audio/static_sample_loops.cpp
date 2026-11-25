// [TEMP] [CHOSEN, not recovered] Headless check of the rebuilt BF2Audio sample path for a weapon's
// first-person fire sound: BfSoundEngine scheduling and startWaiting, StaticSample's buffer queue,
// endLoop and the per-update gain, driven through a fake OpenAL. No audio device is opened: the
// image's dlopen/dlsym of OpenAL.framework bind to the fakes below, which record every queue, loop,
// play, stop, rewind, gain and pitch call and simulate playback at 30 updates per second.
// It plays a stock M16 Fire1P WAV (loop region = one shot, then the tail; stopType 1) the way
// DefaultSoundComp::postFire does for single (play(1)), burst (play(3)) and automatic fire
// (play(0) and a later stop). Repeated single shots also compete with quiet 3D loops at the
// selected quality's voice limit. Each attack must be submitted at non-zero gain. Optional
// --apple-deferred models the published Apple stop/queue transition that can discard a play;
// neither fake measures a real mixer's output.
// The retail dispatch is BF2.exe 0x608260; source cleanup is BF2Audio.dll 0x10012B50 followed
// by the pool's rewind/detach at 0x10013DD0. The source start is 0x10014260.
// Build and run: run_static_sample_loops.sh. [RETIRE-WHEN: native-audio-test-harness] Replace with
// a CMake test once the native audio image has a shared fixture host.
#include "../../BF2/Sound/NullSound.h"
#include "../../BF2/Sound/SoundEngine/BfOpenAL.h"
#include "../../BF2/Sound/SoundEngine/BfSoundEngine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>
#if !defined(BF2_TEST_REAL_OPENAL)
#include <CoreAudio/CoreAudio.h>
#endif

namespace dice { namespace hfe {
struct SoundEnvelopePoint { float time; float value; float tangent; };
} }

using namespace dice::hfe;
extern "C" ISoundEngine* getSoundEngine();
extern "C" void shutdownSoundEngine();

namespace {

const double kUpdateSeconds = 1.0 / 30.0;

struct FakeBuffer
{
    int frames = 0;
    int rate = 44100;
    long offset = 0;           // frame offset inside the file's PCM data
};

struct FakeSource
{
    int state = AL_INITIAL;
    bool looping = false;
    std::deque<ALuint> queue;
    unsigned int current = 0;  // index of the playing buffer in the queue
    double position = 0.0;     // frames into the current buffer
    int processed = 0;
    float gain = 1.0f;
    float pitch = 1.0f;
    bool stopping = false, flushing = false, playPending = false;
    unsigned int unqueued = 0;
    std::deque<ALuint> nextQueue;
};

std::map<ALuint, FakeBuffer> g_buffers;
std::map<ALuint, FakeSource> g_sources;
ALuint g_nextBuffer = 1, g_nextSource = 1;
ALenum g_error = 0;
const char* g_dataBase = 0;    // first byte of the PCM block the current file's buffers share
bool g_log = false;
// [TEMP] Model only Apple's deferred stop/rewind/flush interaction, not its mixer.
// OALSource::{Stop,Rewind,SetBuffer,GetQLengthPriv,Play}, SetQueueLength and
// alSourceQueueBuffers in https://github.com/apple-oss-distributions/OpenAL/tree/main/Source/OpenAL.
bool g_appleDeferred = false;
int g_ignoredPlays = 0;
std::string g_trace;           // the recorded calls of the current scenario
int g_update = 0;              // engine updates since the scenario started

// What the listener heard of each file region: frames played, and frames played at gain > 0.01.
struct Heard { double frames = 0, audible = 0, gainSum = 0, shotGainFrames = 0; };
std::map<long, Heard> g_heard;  // keyed by buffer offset

void record(const char* text)
{
    char stamp[16];
    std::snprintf(stamp, sizeof(stamp), "u%02d ", g_update);
    g_trace += stamp;
    g_trace += text;
    g_trace += '\n';
    if (g_log)
        std::printf("  %s%s\n", stamp, text);
}

const char* label(ALuint b)
{
    static char text[64];
    std::map<ALuint, FakeBuffer>::const_iterator it = g_buffers.find(b);
    if (it == g_buffers.end())
        std::snprintf(text, sizeof(text), "buf%u[?]", b);
    else
        std::snprintf(text, sizeof(text), "[%ld,%ld)", it->second.offset, it->second.offset + it->second.frames);
    return text;
}

// AL
ALint fGetInteger(ALenum) { return 0; }
const ALchar* fGetString(ALenum) { return "fake"; }
ALenum fGetError() { ALenum e = g_error; g_error = 0; return e; }
ALboolean fIsExtensionPresent(const ALchar*) { return 0; }
void* fGetProcAddress(const ALchar*) { return 0; }
ALenum fGetEnumValue(const ALchar*) { return 0; }
void fListenerf(ALenum, ALfloat) {}
void fListenerfv(ALenum, const ALfloat*) {}
void fListener3f(ALenum, ALfloat, ALfloat, ALfloat) {}
void fGetListenerfv(ALenum, ALfloat* v) { v[0] = v[1] = v[2] = 0.0f; }
void fGenSources(ALsizei n, ALuint* out)
{
    for (ALsizei i = 0; i < n; ++i)
    {
        if (g_sources.size() >= 64) { g_error = 0xA005; out[i] = 0; continue; } // AL_OUT_OF_MEMORY
        out[i] = g_nextSource++;
        g_sources[out[i]] = FakeSource();
    }
}
void fDeleteSources(ALsizei n, const ALuint* s)
{
    for (ALsizei i = 0; i < n; ++i)
        g_sources.erase(s[i]);
    // countVoices deletes every probe source; ids are reused from the start
    if (g_sources.empty())
        g_nextSource = 1;
}
void fSourcei(ALuint s, ALenum p, ALint v)
{
    FakeSource& src = g_sources[s];
    char text[128];
    if (p == AL_LOOPING)
    {
        src.looping = v != 0;
        std::snprintf(text, sizeof(text), "src%u LOOPING=%d", s, v);
        record(text);
    }
    else if (p == AL_BUFFER)
    {
        if (g_appleDeferred && src.stopping && v == 0)
        {
            src.flushing = true;
            src.nextQueue.clear();
            std::snprintf(text, sizeof(text), "src%u BUFFER=0 (deferred)", s);
            record(text);
            return;
        }
        src.queue.clear();
        if (v)
            src.queue.push_back((ALuint) v);
        src.current = 0; src.position = 0; src.processed = 0;
        std::snprintf(text, sizeof(text), "src%u BUFFER=%d", s, v);
        record(text);
    }
}
void fSourcef(ALuint s, ALenum p, ALfloat v)
{
    if (p == AL_GAIN) g_sources[s].gain = v;
    else if (p == AL_PITCH) g_sources[s].pitch = v;
    else return;
    char text[96];
    std::snprintf(text, sizeof(text), "src%u %s=%.3f", s, p == AL_GAIN ? "GAIN" : "PITCH", v);
    record(text);
}
void fSourcefv(ALuint, ALenum, const ALfloat*) {}
void fGetSourcei(ALuint s, ALenum p, ALint* v)
{
    FakeSource& src = g_sources[s];
    switch (p)
    {
    case AL_SOURCE_STATE:      *v = src.state; break;
    case AL_BUFFERS_QUEUED:    *v = src.flushing ? src.nextQueue.size() : src.queue.size() - src.unqueued; break;
    case AL_BUFFERS_PROCESSED:
        *v = src.flushing ? 0 : src.stopping ? src.queue.size() - src.unqueued : src.looping ? 0 : src.processed;
        break;
    case AL_LOOPING:           *v = src.looping; break;
    default:                   *v = 0; break;
    }
}
void fGetSourcef(ALuint s, ALenum p, ALfloat* v) { *v = p == AL_GAIN ? g_sources[s].gain : 0.0f; }
void fGetSourcefv(ALuint, ALenum, ALfloat* v) { v[0] = v[1] = v[2] = 0.0f; }
void fSourcePlay(ALuint s)
{
    FakeSource& src = g_sources[s];
    if (g_appleDeferred && src.stopping)
    {
        // Apple's Play tests the old queue (minus deferred unqueues), even though the new
        // buffers are already visible through AL_BUFFERS_QUEUED in the temporary queue.
        if (src.queue.size() == src.unqueued)
        {
            ++g_ignoredPlays;
            record("PLAY ignored: old queue empty, new buffers still deferred");
            return;
        }
        src.playPending = true;
        src.state = AL_PLAYING;
        record("PLAY deferred until stop/rewind/flush completes");
        return;
    }
    src.state = src.queue.empty() ? AL_STOPPED : AL_PLAYING;
    // A stopped or initial source restarts at its first unprocessed buffer.
    src.current = (unsigned int) src.processed;
    src.position = 0.0;
    if (src.current >= src.queue.size()) { src.current = 0; src.processed = 0; }
    char text[160];
    std::snprintf(text, sizeof(text), "src%u PLAY queue=%u looping=%d gain=%.3f", s,
                  (unsigned int) src.queue.size(), src.looping ? 1 : 0, src.gain);
    record(text);
}
void fSourceStop(ALuint s)
{
    FakeSource& src = g_sources[s];
    if (g_appleDeferred && src.state == AL_PLAYING)
        src.stopping = true;
    src.state = AL_STOPPED;
    src.processed = (int) src.queue.size();
    src.current = (unsigned int) src.queue.size();
    char text[64];
    std::snprintf(text, sizeof(text), "src%u STOP", s);
    record(text);
}
void fSourceRewind(ALuint s)
{
    FakeSource& src = g_sources[s];
    src.state = AL_INITIAL;
    if (src.stopping)
    {
        record("REWIND deferred");
        return;
    }
    src.current = 0;
    src.position = 0.0;
    src.processed = 0;
    char text[64];
    std::snprintf(text, sizeof(text), "src%u REWIND", s);
    record(text);
}
void fGenBuffers(ALsizei n, ALuint* out)
{
    for (ALsizei i = 0; i < n; ++i) { out[i] = g_nextBuffer++; g_buffers[out[i]] = FakeBuffer(); }
}
void fDeleteBuffers(ALsizei n, const ALuint* b) { for (ALsizei i = 0; i < n; ++i) g_buffers.erase(b[i]); }
void fBufferData(ALuint b, ALenum format, const ALvoid* data, ALsizei size, ALsizei freq)
{
    const int bytesPerFrame = format == AL_FORMAT_STEREO16 ? 4 : format == AL_FORMAT_MONO16 ? 2 : 1;
    // SoundFile fills intro, loop and outro in that order from one PCM block; the first call of a
    // file fixes the base the offsets are measured from.
    if (!g_dataBase || (const char*) data < g_dataBase || (const char*) data - g_dataBase > (1 << 26))
        g_dataBase = (const char*) data;
    FakeBuffer& buffer = g_buffers[b];
    buffer.frames = size / bytesPerFrame;
    buffer.rate = freq;
    buffer.offset = (long) (((const char*) data - g_dataBase) / bytesPerFrame);
    // Peak and RMS of the 16-bit PCM, to see what a format conversion left of a transient.
    double peak = 0.0, sum = 0.0;
    const int samples = format == AL_FORMAT_STEREO16 || format == AL_FORMAT_MONO16 ? size / 2 : 0;
    for (int i = 0; i < samples; ++i)
    {
        const double v = static_cast<const short*>(data)[i] / 32768.0;
        peak = v < 0 ? (-v > peak ? -v : peak) : (v > peak ? v : peak);
        sum += v * v;
    }
    char text[192];
    std::snprintf(text, sizeof(text), "buffer%u data %s format=0x%x bytes=%d rate=%d peak=%.3f rms=%.3f", b,
                  label(b), (unsigned int) format, (int) size, (int) freq, peak,
                  samples ? std::sqrt(sum / samples) : 0.0);
    record(text);
}
void fGetBufferi(ALuint b, ALenum, ALint* v) { *v = g_buffers[b].frames; }
void fSourceUnqueueBuffers(ALuint s, ALsizei n, ALuint* out)
{
    FakeSource& src = g_sources[s];
    char text[96];
    if (g_appleDeferred && src.stopping && !src.flushing)
    {
        for (int i = 0; i < n; ++i)
            out[i] = src.queue[src.unqueued++];
        record("UNQUEUE deferred; old logical queue length reduced");
        return;
    }
    if (src.looping || src.processed < n)
    {
        g_error = 0xA003;
        std::snprintf(text, sizeof(text), "src%u UNQUEUE %d REFUSED", s, n);
        record(text);
        return;
    }
    for (ALsizei i = 0; i < n; ++i)
    {
        out[i] = src.queue.front();
        std::snprintf(text, sizeof(text), "src%u UNQUEUE %s", s, label(out[i]));
        record(text);
        src.queue.pop_front();
        --src.processed;
        if (src.current) --src.current;
    }
}
void fSourceQueueBuffers(ALuint s, ALsizei n, const ALuint* b)
{
    FakeSource& src = g_sources[s];
    char text[96];
    for (ALsizei i = 0; i < n; ++i)
    {
        if (src.flushing) src.nextQueue.push_back(b[i]);
        else src.queue.push_back(b[i]);
        std::snprintf(text, sizeof(text), "src%u QUEUE %s", s, label(b[i]));
        record(text);
    }
}
void fDopplerFactor(ALfloat) {}
void fDistanceModel(ALenum) {}
int g_deviceToken, g_contextToken;
int g_contextRate = 0;
std::string g_deviceName, g_libraryName;
ALCcontext* fcCreateContext(ALCdevice*, const ALint* attrs)
{
    for (; attrs && *attrs; attrs += 2)
        if (*attrs == ALC_FREQUENCY) g_contextRate = attrs[1];
    return (ALCcontext*) &g_contextToken;
}
ALboolean fcMakeContextCurrent(ALCcontext*) { return 1; }
void fcDestroyContext(ALCcontext*) {}
ALenum fcGetError(ALCdevice*) { return 0; }
ALCdevice* fcOpenDevice(const ALchar* name)
{
    g_deviceName = name ? name : "<default>";
    return (ALCdevice*) &g_deviceToken;
}
ALboolean fcCloseDevice(ALCdevice*) { return 1; }
ALboolean fcIsExtensionPresent(ALCdevice*, const ALchar*) { return 0; }
const ALchar* fcGetString(ALCdevice*, ALenum) { return "Fake Device"; }
void fcGetIntegerv(ALCdevice*, ALenum, ALsizei, ALint* v) { *v = 0; }

struct Entry { const char* name; void* fn; };
const Entry kEntries[] = {
    { "alGetInteger", (void*) &fGetInteger }, { "alGetString", (void*) &fGetString },
    { "alGetError", (void*) &fGetError }, { "alIsExtensionPresent", (void*) &fIsExtensionPresent },
    { "alGetProcAddress", (void*) &fGetProcAddress }, { "alGetEnumValue", (void*) &fGetEnumValue },
    { "alListenerf", (void*) &fListenerf }, { "alListenerfv", (void*) &fListenerfv },
    { "alListener3f", (void*) &fListener3f }, { "alGetListenerfv", (void*) &fGetListenerfv },
    { "alGenSources", (void*) &fGenSources }, { "alDeleteSources", (void*) &fDeleteSources },
    { "alSourcei", (void*) &fSourcei }, { "alSourcef", (void*) &fSourcef },
    { "alSourcefv", (void*) &fSourcefv }, { "alGetSourcei", (void*) &fGetSourcei },
    { "alGetSourcef", (void*) &fGetSourcef }, { "alGetSourcefv", (void*) &fGetSourcefv },
    { "alSourcePlay", (void*) &fSourcePlay }, { "alSourceStop", (void*) &fSourceStop },
    { "alSourceRewind", (void*) &fSourceRewind }, { "alGenBuffers", (void*) &fGenBuffers },
    { "alDeleteBuffers", (void*) &fDeleteBuffers }, { "alBufferData", (void*) &fBufferData },
    { "alGetBufferi", (void*) &fGetBufferi }, { "alSourceUnqueueBuffers", (void*) &fSourceUnqueueBuffers },
    { "alSourceQueueBuffers", (void*) &fSourceQueueBuffers }, { "alDopplerFactor", (void*) &fDopplerFactor },
    { "alDistanceModel", (void*) &fDistanceModel }, { "alcCreateContext", (void*) &fcCreateContext },
    { "alcMakeContextCurrent", (void*) &fcMakeContextCurrent }, { "alcDestroyContext", (void*) &fcDestroyContext },
    { "alcGetError", (void*) &fcGetError }, { "alcOpenDevice", (void*) &fcOpenDevice },
    { "alcCloseDevice", (void*) &fcCloseDevice }, { "alcIsExtensionPresent", (void*) &fcIsExtensionPresent },
    { "alcGetString", (void*) &fcGetString }, { "alcGetIntegerv", (void*) &fcGetIntegerv },
};
int g_moduleToken;

// One engine update's worth of playback on every playing source.
void advance(double seconds)
{
    for (std::map<ALuint, FakeSource>::iterator it = g_sources.begin(); it != g_sources.end(); ++it)
    {
        FakeSource& src = it->second;
        if (src.stopping)
        {
            if (src.flushing) src.queue.swap(src.nextQueue);
            else for (unsigned int n = 0; n < src.unqueued; ++n) src.queue.pop_front();
            src.nextQueue.clear();
            src.current = 0; src.position = 0; src.processed = 0; src.unqueued = 0;
            src.state = src.playPending ? AL_PLAYING : AL_INITIAL;
            src.stopping = src.flushing = src.playPending = false;
        }
        if (src.state != AL_PLAYING)
            continue;
        double remaining = seconds * src.pitch;   // in seconds; converted per buffer
        while (remaining > 0.0 && src.state == AL_PLAYING)
        {
            if (src.current >= src.queue.size()) { src.state = AL_STOPPED; break; }
            const FakeBuffer& buffer = g_buffers[src.queue[src.current]];
            const double left = buffer.frames - src.position;
            const double want = remaining * buffer.rate;
            const double step = want < left ? want : left;
            Heard& heard = g_heard[buffer.offset];
            heard.frames += step;
            heard.gainSum += step * src.gain;
            if (src.gain > 0.01f)
                heard.audible += step;
            if (src.gain > 0.5f)
                heard.shotGainFrames += step;
            src.position += step;
            remaining -= step / buffer.rate;
            if (src.position >= buffer.frames)
            {
                src.position = 0.0;
                if (src.looping && src.current + 1 >= src.queue.size())
                    src.current = 0;                   // a looping source wraps its whole queue
                else
                {
                    ++src.current;
                    if (!src.looping)
                        ++src.processed;
                    if (src.current >= src.queue.size())
                        src.state = AL_STOPPED;
                }
            }
        }
    }
}

int g_failures = 0;
void check(bool ok, const char* what)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

} // namespace

#if defined(BF2_TEST_REAL_OPENAL)
// Real-framework mode: the image loads OpenAL.framework itself. After initialize every entry point
// the sample path uses is wrapped: calls are recorded, and every gain (listener and source) is
// forced to 0 before any source plays, so nothing is ever audible.
namespace {
PFN_alSourcei              r_Sourcei;
PFN_alSourcef              r_Sourcef;
PFN_alSourcePlay           r_SourcePlay;
PFN_alSourceStop           r_SourceStop;
PFN_alSourceRewind         r_SourceRewind;
PFN_alSourceQueueBuffers   r_SourceQueueBuffers;
PFN_alSourceUnqueueBuffers r_SourceUnqueueBuffers;
PFN_alBufferData           r_BufferData;
PFN_alListenerf            r_Listenerf;
ALuint g_watched = 0;          // the source the current scenario's sample queued on
int g_requestedGainCalls = 0;

void wSourcei(ALuint s, ALenum p, ALint v)
{
    char text[96];
    if (p == AL_LOOPING || p == AL_BUFFER)
    {
        std::snprintf(text, sizeof(text), "src%u %s=%d", s, p == AL_LOOPING ? "LOOPING" : "BUFFER", v);
        record(text);
    }
    r_Sourcei(s, p, v);
}
void wSourcef(ALuint s, ALenum p, ALfloat v)
{
    if (p == AL_GAIN)
    {
        g_sources[s].gain = v;     // what the engine asked for
        ++g_requestedGainCalls;
        v = 0.0f;                  // silent measurement
    }
    r_Sourcef(s, p, v);
}
void wSourcePlay(ALuint s)
{
    char text[96];
    std::snprintf(text, sizeof(text), "src%u PLAY (engine gain %.3f, sent 0)", s, g_sources[s].gain);
    record(text);
    r_SourcePlay(s);
}
void wSourceStop(ALuint s)
{
    char text[64];
    std::snprintf(text, sizeof(text), "src%u STOP", s);
    record(text);
    r_SourceStop(s);
}
void wSourceRewind(ALuint s)
{
    char text[64];
    std::snprintf(text, sizeof(text), "src%u REWIND", s);
    record(text);
    r_SourceRewind(s);
}
void wSourceQueueBuffers(ALuint s, ALsizei n, const ALuint* b)
{
    g_watched = s;
    char text[96];
    for (ALsizei i = 0; i < n; ++i)
    {
        std::snprintf(text, sizeof(text), "src%u QUEUE %s", s, label(b[i]));
        record(text);
    }
    r_SourceQueueBuffers(s, n, b);
}
void wSourceUnqueueBuffers(ALuint s, ALsizei n, ALuint* b)
{
    r_SourceUnqueueBuffers(s, n, b);
    char text[96];
    for (ALsizei i = 0; i < n; ++i)
    {
        std::snprintf(text, sizeof(text), "src%u UNQUEUE %s", s, label(b[i]));
        record(text);
    }
}
void wBufferData(ALuint b, ALenum format, const ALvoid* data, ALsizei size, ALsizei freq)
{
    fBufferData(b, format, data, size, freq);   // the fake's bookkeeping: frames, rate, offset
    r_BufferData(b, format, data, size, freq);
}
void wListenerf(ALenum p, ALfloat v)
{
    r_Listenerf(p, p == AL_GAIN ? 0.0f : v);
}

void installWrappers()
{
    r_Sourcei = al_Sourcei;                       al_Sourcei = &wSourcei;
    r_Sourcef = al_Sourcef;                       al_Sourcef = &wSourcef;
    r_SourcePlay = al_SourcePlay;                 al_SourcePlay = &wSourcePlay;
    r_SourceStop = al_SourceStop;                 al_SourceStop = &wSourceStop;
    r_SourceRewind = al_SourceRewind;             al_SourceRewind = &wSourceRewind;
    r_SourceQueueBuffers = al_SourceQueueBuffers; al_SourceQueueBuffers = &wSourceQueueBuffers;
    r_SourceUnqueueBuffers = al_SourceUnqueueBuffers; al_SourceUnqueueBuffers = &wSourceUnqueueBuffers;
    r_BufferData = al_BufferData;                 al_BufferData = &wBufferData;
    r_Listenerf = al_Listenerf;                   al_Listenerf = &wListenerf;
    r_Listenerf(AL_GAIN, 0.0f);
}

// Polls the watched source every 5 ms for one engine update; prints each change of state.
std::string g_lastPoll;
void pollRealtime(double seconds)
{
    const int steps = (int) (seconds / 0.005 + 0.5);
    for (int i = 0; i < steps; ++i)
    {
        if (g_watched)
        {
            ALint state = 0, queued = 0, processed = 0, offset = 0, buffer = 0;
            al_GetSourcei(g_watched, AL_SOURCE_STATE, &state);
            al_GetSourcei(g_watched, AL_BUFFERS_QUEUED, &queued);
            al_GetSourcei(g_watched, AL_BUFFERS_PROCESSED, &processed);
            al_GetSourcei(g_watched, 0x1025 /* AL_SAMPLE_OFFSET */, &offset);
            al_GetSourcei(g_watched, AL_BUFFER, &buffer);
            ALfloat gain = -1.0f, listener = -1.0f;
            al_GetSourcef(g_watched, AL_GAIN, &gain);
            char text[200];
            std::snprintf(text, sizeof(text), "src%u state=0x%x queued=%d processed=%d offset=%d buffer=%d",
                          g_watched, state, queued, processed, offset, buffer);
            if (g_lastPoll != text)
            {
                g_lastPoll = text;
                char line[260];
                std::snprintf(line, sizeof(line), "+%3dms %s srcGain=%.1f", i * 5, text, gain);
                record(line);
            }
            (void) listener;
            if (gain != 0.0f)
            {
                // Never let a source be audible: stop everything at once.
                std::fprintf(stderr, "[test] source gain %.3f is not 0 - aborting\n", gain);
                std::exit(3);
            }
        }
        struct timespec ts = { 0, 5000000 };
        nanosleep(&ts, 0);
    }
}
} // namespace
#else
// [TEMP] The fake backend needs no physical output device, including in a sandbox.
extern "C" OSStatus AudioObjectGetPropertyData(AudioObjectID,
    const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* size, void* data)
{
    if (address->mSelector == kAudioHardwarePropertyDefaultOutputDevice && *size == sizeof(AudioObjectID))
    {
        *static_cast<AudioObjectID*>(data) = 1;
        return noErr;
    }
    return kAudioHardwareUnknownPropertyError;
}

// The audio image's OpenAL loader (Win32System's POSIX arm) binds to these in this executable.
extern "C" void* dlopen(const char* path, int)
{
    g_libraryName = path ? path : "";
    return path && std::strstr(path, "OpenAL") ? &g_moduleToken : 0;
}
extern "C" void* dlsym(void* module, const char* name)
{
    if (module != &g_moduleToken)
        return 0;
    for (unsigned int i = 0; i < sizeof(kEntries) / sizeof(kEntries[0]); ++i)
        if (!std::strcmp(kEntries[i].name, name))
            return kEntries[i].fn;
    return 0;
}
extern "C" int dlclose(void*) { return 0; }
#endif

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <M16_fire_1p_outdoor.wav> [-v] [-q0|1|2] [-p0|2] [--apple-deferred]\n", argv[0]);
        return 2;
    }
    int quality = 1;                   // medium, the client's default
    const char* provider = "0";
    for (int a = 2; a < argc; ++a)
    {
        if (!std::strcmp(argv[a], "-v"))
            g_log = true;
        else if (!std::strncmp(argv[a], "-q", 2))
            quality = std::atoi(argv[a] + 2);
        else if (!std::strncmp(argv[a], "-p", 2))
            provider = argv[a] + 2;
        else if (!std::strcmp(argv[a], "--apple-deferred"))
            g_appleDeferred = true;
    }
    if (quality < 0 || quality > 2 || (std::strcmp(provider, "0") && std::strcmp(provider, "2")))
        return 2;
#if defined(BF2_TEST_REAL_OPENAL)
    if (g_appleDeferred) return 2;
#endif
    ISoundEngine* engine = getSoundEngine();
    const int voices = quality == 2 ? 16 : 32;
    const int err = engine->initialize(static_cast<SoundQuality>(quality), provider, voices);
    std::printf("[test] initialize provider %s quality %d voices %d -> %d\n", provider, quality, voices, err);
    if (err)
        return 1;
#if defined(BF2_TEST_REAL_OPENAL)
    installWrappers();
    std::printf("[test] real OpenAL.framework, listener and source gains forced to 0\n");
#else
    std::printf("[test] %s OpenAL model\n", g_appleDeferred ? "Apple deferred" : "synchronous");
    check(g_libraryName == "/System/Library/Frameworks/OpenAL.framework/OpenAL", "loads Apple OpenAL.framework");
    check(g_deviceName == (!std::strcmp(provider, "0") ? "DirectSound" : "DirectSound3D"), "opens the selected retail device name");
    check(g_contextRate == 44100, "context mixes at 44100 Hz for either provider");
    const SoundStats* stats = static_cast<const SoundStats*>(engine->getStats());
    check(stats->maxVoices == voices + 4, "voice limit includes four reserved pool sources");
    check(engine->getProperty("use-source-pool") == 1.0f, "either provider uses the source pool");
    check(engine->getProperty("use-2d-occlusion") == 0.0f, "either provider disables 2D occlusion");
#endif

    // S_usrif_m16a2_Fire1P as DefaultSoundCompTemplate::initTemplates and the stock tweak set it up.
    ISoundTemplate* t = engine->createTemplate();
    t->setFilename(argv[1]);
    t->setIs3dSound(false);
    t->setLoopCount(0);
    t->setStopType(1);
    t->setVolume(0.7f);
    t->setPitch(1.0f);
    t->setReverbLevel(0.0f);
    const SoundEnvelopePoint volumePoint = { 0.0f, 0.9f, 1.0f };
    t->setVolumeEnvelope(0.0f, 1.0f, 0.0f, 1.0f, true, 1, &volumePoint);
    const SoundEnvelopePoint pitchPoint = { 0.0f, 0.98f, 1.04f };
    t->setPitchEnvelope(0.0f, 1.0f, 0.9f, 1.1f, true, 1, &pitchPoint);
    t->setIsUsed();
    engine->loadSoundFiles();
    ISound* sound = t->createSound();

    // The shot region is the file's loop block: the first buffer the file filled.
    const int shotFrames = g_buffers.empty() ? 0 : g_buffers.begin()->second.frames;
    std::printf("[test] shot region %d frames at %d Hz\n", shotFrames,
                g_buffers.empty() ? 0 : g_buffers.begin()->second.rate);

    // stopAfter < 0: never stopped. replays: further play() calls on the following updates.
    struct Scenario { const char* name; int loops; int stopAfter; int replays; int interval = 1; int stopType = 1; };
    const Scenario scenarios[] = {
        { "single play(1)", 1, -1, 0 },
        { "single play(1), stopped in the same update", 1, 0, 0 },
        { "single play(1) on three consecutive updates", 1, -1, 2 },
        { "burst play(3)", 3, -1, 0 },
        { "automatic play(0), stop after 4 updates", 0, 4, 0 },
        { "automatic play(0), stop after 1 update", 0, 1, 0 },
        { "single shots every 100 ms", 1, -1, 9, 3 },
        { "single shots every 200 ms", 1, -1, 7, 6 },
        { "single shots after each tail finishes", 1, -1, 2, 35 },
        { "automatic play(0), immediate outro after 4 updates", 0, 4, 0, 1, 2 },
    };
    for (unsigned int i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); ++i)
    {
        const Scenario& s = scenarios[i];
        sound->forceStop();
        engine->update(1);
        t->setStopType(s.stopType);
        g_heard.clear();
        g_trace.clear();
        g_update = 0;
#if defined(BF2_TEST_REAL_OPENAL)
        g_watched = 0;
        g_lastPoll.clear();
#endif
        std::printf("[test] %s\n", s.name);
        sound->play(s.loops);
        for (int u = 0; u < 60 + s.replays * s.interval; ++u)
        {
            if (u > 0 && u % s.interval == 0 && u / s.interval <= s.replays)
                sound->play(s.loops);
            if (u == s.stopAfter)
                sound->stop();
            engine->update(1);
#if defined(BF2_TEST_REAL_OPENAL)
            pollRealtime(kUpdateSeconds);
#else
            advance(kUpdateSeconds);
#endif
            ++g_update;
        }
#if defined(BF2_TEST_REAL_OPENAL)
        // The framework's own counters are the measurement; nothing is judged here.
        if (!g_log)
            std::printf("%s", g_trace.c_str());
        continue;
#endif
        const Heard& shot = g_heard[0];
        double tail = 0;
        for (std::map<long, Heard>::const_iterator it = g_heard.begin(); it != g_heard.end(); ++it)
            if (it->first > 0)
                tail += it->second.audible;
        std::printf("  shot region: %.0f frames played, %.0f audible, mean gain %.3f; tail audible %.0f frames\n",
                    shot.frames, shot.audible, shot.frames > 0 ? shot.gainSum / shot.frames : 0.0, tail);
        char what[160];
        std::snprintf(what, sizeof(what), "%s plays the shot region audibly", s.name);
        const int expectedShots = s.replays ? s.replays + 1 : s.loops > 0 ? s.loops : 1;
        const bool ok = shotFrames > 0 && shot.audible >= 0.99 * shotFrames * expectedShots;
        check(ok, what);
        if (s.stopType == 2)
            check(tail > 0, "immediate outro survives the deferred stop and buffer replacement");
        if (!g_log && !ok)
            std::printf("%s", g_trace.c_str());
    }

#if !defined(BF2_TEST_REAL_OPENAL)
    // [TEMP] Saturate the real engine scheduler with quiet 3D loops. A first-person shot
    // must steal a source and play its attack, including when the previous shot has a tail.
    sound->forceStop();
    t->setStopType(1);
    ISoundTemplate* ambientTemplate = engine->createTemplate();
    ambientTemplate->setFilename(argv[1]);
    ambientTemplate->setIs3dSound(true);
    ambientTemplate->setLoopCount(0);
    ambientTemplate->setVolume(0.1f);
    ambientTemplate->setIsUsed();
    engine->loadSoundFiles();
    std::vector<ISound*> ambients;
    for (int i = 0; i < voices; ++i)
    {
        ISound* ambient = ambientTemplate->createSound();
        ambients.push_back(ambient);
        ambient->play(0);
    }
    engine->update(1);
    check(static_cast<const SoundStats*>(engine->getStats())->playing == voices,
          "looping voices fill the requested voice budget");
    for (int shot = 0; shot < 40; ++shot)
    {
        g_heard.clear();
        g_trace.clear();
        sound->play(1);
        for (int u = 0; u < 6; ++u)
        {
            engine->update(1);
            advance(kUpdateSeconds);
            ++g_update;
        }
        char what[128];
        std::snprintf(what, sizeof(what), "shot %d under voice pressure plays the complete attack", shot + 1);
        const bool ok = g_heard[0].shotGainFrames >= 0.99 * shotFrames;
        check(ok, what);
        if (!ok && !g_log) std::printf("%s", g_trace.c_str());
    }
    sound->forceStop();
    engine->update(1);
    advance(kUpdateSeconds);
    // More new one-shots than the four spare sources can arrive in one engine update.
    // Each uses its own Sound, just as independent weapons/effects do in a busy scene.
    std::vector<ISound*> simultaneous;
    g_heard.clear();
    g_trace.clear();
    const int ignoredBefore = g_ignoredPlays;
    for (int i = 0; i < 8; ++i)
    {
        ISound* shot = t->createSound();
        simultaneous.push_back(shot);
        shot->play(1);
    }
    for (int u = 0; u < 6; ++u)
    {
        engine->update(1);
        advance(kUpdateSeconds);
    }
    const bool allAttacks = g_heard[0].shotGainFrames >= 0.99 * shotFrames * simultaneous.size();
    check(allAttacks, "eight simultaneous shots under voice pressure keep every attack");
    check(g_ignoredPlays == ignoredBefore, "no play is lost during a deferred source reset");
    if ((!allAttacks || g_ignoredPlays != ignoredBefore) && !g_log)
        std::printf("%s", g_trace.c_str());
    for (ISound* shot : simultaneous) { shot->forceStop(); shot->release(); }
    for (ISound* ambient : ambients)
    {
        ambient->forceStop();
        ambient->release();
    }
    ambientTemplate->release();
#endif

    sound->release();
    t->release();
    engine->shutdown();
    shutdownSoundEngine();
    std::printf("[test] %d failure(s)\n", g_failures);
    return g_failures ? 1 : 0;
}
