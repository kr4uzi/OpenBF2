// [TEMP] [CHOSEN, not recovered] macOS port, package S2.5: plays one menu sound and a few seconds
// of the menu music through the statically built BF2Audio image (getSoundEngine, the OpenAL
// provider on OpenAL.framework, the POSIX Win32System arm and its music thread), at a modest
// volume. Scaffolding only: it uses the image's own default trace and file IO, which the client
// replaces with its own (BfSound::init). [RETIRE-WHEN: macos-client-audio] Remove once the macOS
// client links and plays its menu sounds itself. Build and run: see build.sh.
#include "../../BF2/Sound/NullSound.h"
#include <chrono>
#include <cstdio>
#include <thread>

using namespace dice::hfe;
extern "C" ISoundEngine* getSoundEngine();
extern "C" void shutdownSoundEngine();

static void pump(ISoundEngine* engine, int milliseconds)
{
    for (int t = 0; t < milliseconds; t += 33) {
        engine->update(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <menu.wav> <music.ogg> [music seconds]\n", argv[0]);
        return 2;
    }
    const int musicMs = argc > 3 ? std::atoi(argv[3]) * 1000 : 3000;
    ISoundEngine* engine = getSoundEngine();
    // Provider code "6" is "Software Audio", AudioSettings' first row; quality 1 is medium.
    const int err = engine->initialize(static_cast<SoundQuality>(1), "6", 32);
    std::printf("[test] initialize -> %d, initialized=%d, eax=%d\n", err, engine->isInitialized(),
                engine->getEaxVersion());
    if (err)
        return 1;
    engine->setMasterVolume(0.25f);

    ISoundTemplate* menu = engine->createTemplate();
    menu->setFilename(argv[1]);
    menu->setIs3dSound(false);
    menu->setVolume(1.0f);
    engine->loadSoundFiles();
    ISound* sound = menu->createSound();
    sound->play();
    pump(engine, 100);
    std::printf("[test] menu sound playing=%d\n", sound->isPlaying());
    pump(engine, 900);

    const bool loaded = engine->loadMusic(argv[2]);
    const bool playing = engine->playMusic();
    std::printf("[test] music load=%d play=%d\n", loaded, playing);
    pump(engine, musicMs);
    std::printf("[test] music stop=%d\n", engine->stopMusic(0.0f));

    sound->release();
    menu->release();
    engine->shutdown();
    shutdownSoundEngine();
    std::printf("[test] done\n");
    return 0;
}
