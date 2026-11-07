// [TEMP] [CHOSEN, not recovered] POSIX BF2Audio worker stop/exchange fixture; no audio device.
// [RETIRE-WHEN: s2-10-audio-worker] Retire when the audio suite covers concurrent volume/fade
// publication and worker join on both native Mac architectures under TSan.
#include "../../BF2/Sound/SoundEngine/Win32System.h"
#include "../../BF2/Sound/SoundEngine/SoundTrace.h"
#include <cstdio>
#include <cstdlib>
#include <thread>

// Test-only dependencies of hasSoundDevice, which this fixture does not call. Reaching the
// hardware-query trace path is an error in this fixture and stops visibly.
namespace dice { namespace hfe {
ISoundTrace* g_soundTrace = nullptr;
char* sndFormat(const char*, ...)
{
    std::fputs("[TEMP] s2_10 audio worker unexpectedly queried a sound device\n", stderr);
    std::fflush(stderr);
    std::abort();
}
} }
using namespace dice::hfe;
struct Handoff {
    std::atomic<long> volume{0};
    std::atomic<bool> ready{false};
    long result = 0;
};

static void worker(win32::Thread* thread)
{
    Handoff* handoff = static_cast<Handoff*>(win32::threadArgument(thread));
    handoff->ready = true;
    while (!win32::threadShouldStop(thread)) std::this_thread::yield();
    handoff->result = handoff->volume.load();
}

int main()
{
    for (int i = 0; i < 100; ++i) {
        Handoff handoff;
        win32::Thread* thread = win32::createThread(worker, &handoff);
        for (int attempt = 0; !handoff.ready && attempt < 500; ++attempt) win32::sleep(1);
        if (!handoff.ready || win32::exchange(&handoff.volume, i + 1) != 0) std::abort();
        win32::joinAndDeleteThread(thread);
        if (handoff.result != i + 1) std::abort();
    }
    std::puts("PASS BF2Audio: 100 production POSIX worker stop/exchange/join cycles");
}
