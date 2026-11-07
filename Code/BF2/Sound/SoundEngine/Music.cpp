
// BF2Audio.dll only. Without BF2_IMAGE_BF2AUDIO this file compiles to nothing: the recursive
// sweeps that build BF2.exe and bf2_w32ded.exe (CMake's bf2_client_objects glob, build.cmd's
// all-TU census) must not carry the DLL's engine: its DllMain, its two exports and its own null
// Sound/SoundTemplate pair would collide with the host's, which loads this image at run time.
// Same convention as
// Code/BF2/RendDX9/BF2Render.cpp's BF2_IMAGE_RENDDX9 guard. Build with Code\Build\bf2audio.cmd.
#if defined(BF2_IMAGE_BF2AUDIO)
#include "Music.h"
#include "SoundFile.h"
#include "OpenALSample.h"
#include "BfSoundEngine.h"
#include "BfOpenAL.h"
#include "SoundTrace.h"
#include "Win32System.h"

#include <climits>
#include <list>
#include <sstream>
#include <string>

namespace dice {
namespace hfe {

namespace {
const char* const kFile = "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\SoundEngine\\Music.cpp";

SoundFile*             s_musicFile    = 0;      // ds:10046D84
win32::Thread*         s_musicThread  = 0;      // ds:10046D88
bool                   s_musicPlaying = false;  // ds:10046D8C
#if defined(_WIN32)
volatile long          s_fadeMillis   = 0;      // ds:InterlockedExchange target
#else
// [CHOSEN, not recovered] POSIX volume/fade publication to the music worker is sequentially consistent.
std::atomic<long>      s_fadeMillis{0};
#endif
std::list<std::string> s_errors;
#if defined(_WIN32)
volatile long          s_volumeMilli  = 0;      // ds:1003F1F0 ("Target") - group 1 volume x 1000
#else
std::atomic<long>      s_volumeMilli{0};
#endif

// every collected line as an error, then a summary line.
void reportErrors(std::list<std::string>* errors)
{
    if (errors->empty())
        return;
    for (std::list<std::string>::iterator it = errors->begin(); it != errors->end(); ++it)
        g_soundTrace->warning(sndFormat(it->c_str()), kFile, 81);
    g_soundTrace->error(sndFormat("At least one error caused by music player (see log entries above)"), kFile, 83);
}

// the worker's own error check - alGetError, formatted through a stringstream
// (the shared sndFormat buffer is not thread-safe), onto the list.
void threadError(std::list<std::string>* errors, const char* what)
{
    const ALenum err = al_GetError();
    if (!err)
        return;
    std::stringstream ss;
    ss << "Music thread OpenAL Error: " << (what ? what : "") << " : " << al_GetString(err);
    errors->push_back(ss.str());
}

// the worker.
void __cdecl musicThreadMain(win32::Thread* thread)
{
    std::list<std::string>* errors = (std::list<std::string>*) win32::threadArgument(thread);
    ALuint source = 0;
    al_GenSources(1, &source);
    threadError(errors, "alGenSources");
    ALuint buffers[12];
    genBuffersAccessible(12, buffers);
    threadError(errors, "alGenStreamingBuffers");
    OggVorbis_File* vf = 0;
    int loops = INT_MAX;
    char buffer[0x2000];
    for (int i = 0; i < 12; ++i)
        streamStart(&vf, s_musicFile, buffer, &loops, -1, source, buffers[i], false);
    float gain = (float) s_volumeMilli * 0.001f;
    al_Sourcef(source, AL_GAIN, gain);
    threadError(errors, "alSourcef (AL_GAIN)");
    al_SourcePlay(source);
    threadError(errors, "alSourcePlay");
    bool  fading      = false;
    float fade        = 1.0f;
    float fadeSeconds = 0.0f;
    while (!win32::threadShouldStop(thread))
    {
        if (fading)
        {
            fade -= 0.0099999998f / fadeSeconds;
            if (fade <= 0.0f)
                break;
        }
        else
        {
            const long millis = s_fadeMillis;
            if (millis > 0)
            {
                fading = true;
                fadeSeconds = (float) millis * 0.001f;
            }
        }
        gain = (float) s_volumeMilli * 0.001f * fade;
        al_Sourcef(source, AL_GAIN, gain);
        if (gain > 0.0f)
        {
            for (int k = 2; k; --k)
                updateSourceBuffers(vf, s_musicFile, buffer, &loops, source, 0, false);
        }
        win32::sleep(10);
    }
    al_SourceStop(source);
    threadError(errors, "alSourceStop");
    for (int j = 0; j < 12; ++j)
    {
        ALuint b;
        al_SourceUnqueueBuffers(source, 1, &b);
        std::string what("alSourceUnqueueBuffers ");
        what += (char) ('0' + j);
        threadError(errors, what.c_str());
    }
#if defined(__APPLE__)
    // [CHOSEN, not recovered] macOS port: Apple's OpenAL.framework keeps buffers that were queued
    // on a source attached after they are unqueued, so alDeleteBuffers fails with
    // AL_INVALID_OPERATION and the twelve buffers leak, until the source's AL_BUFFER is cleared.
    // Clearing it is a no-op for the OpenAL implementations the Win32 image loads.
    al_Sourcei(source, AL_BUFFER, 0);
#endif
    al_DeleteBuffers(12, buffers);
    threadError(errors, "alDeleteBuffers");
    al_DeleteSources(1, &source);
    threadError(errors, "alDeleteSources");
}

// The same name is a no-op; otherwise the old file is deleted outright (through
// its deleting destructor, not release) and a fresh one made, marked used, referenced twice and
// wanted at every quality.
bool musicLoadFile(const char* name)
{
    if (s_musicFile)
    {
        if (s_musicFile->m_name == name)
            return true;
        if (s_musicFile)
            delete s_musicFile;
    }
    s_musicFile = 0;
    SoundFile* file = new SoundFile(std::string(name));
    file->setIsUsed();
    file->addRef();
    file->addRef();
    file->m_qualityMask |= 7;
    s_musicFile = file;
    return true;
}
}

bool musicLoad(const char* name)
{
    musicStop(0.0f);
    if (s_musicFile)
        s_musicFile->unloadIfUnused(true, true);
    if (name)
        return musicLoadFile(name);
    s_musicPlaying = false;
    return true;
}

bool musicPlay()
{
    if (!s_musicFile)
        return false;
    musicStop(0.0f);
    if (g_soundEngineInstance->isInitialized())
    {
        s_errors.clear();
        s_musicFile->load(44100, 16);
        if (s_musicFile->m_loaded)
        {
            s_musicPlaying = true;
            musicUpdateVolume();
            s_musicThread = win32::createThread(musicThreadMain, &s_errors);
            g_soundTrace->info(sndFormat("Music thread was started"), kFile, 203);
        }
    }
    return s_musicPlaying;
}

// A fade-out leaves the worker running but reports "not playing" at once,
// and a later stop with no fade joins it.
bool musicStop(float fadeSeconds)
{
    const bool wasPlaying = s_musicPlaying;
    if (s_musicThread)
    {
        if (fadeSeconds > 0.0f)
        {
            g_soundTrace->info(sndFormat("Music thread fading out"), kFile, 227);
            win32::exchange(&s_fadeMillis, (long) (fadeSeconds * 1000.0f));
        }
        else
        {
            win32::joinAndDeleteThread(s_musicThread);
            g_soundTrace->info(sndFormat("Music thread was stopped"), kFile, 217);
            checkOpenALError("stop music thread");
            reportErrors(&s_errors);
            s_errors.clear();
            s_musicThread = 0;
            win32::exchange(&s_fadeMillis, 0);
        }
    }
    s_musicPlaying = false;
    return wasPlaying;
}

void musicUpdateVolume()
{
    if (s_musicPlaying)
        win32::exchange(&s_volumeMilli, (long) (g_soundEngineInstance->getVolumeForGroup(1) * 1000.0f));
}

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_BF2AUDIO
