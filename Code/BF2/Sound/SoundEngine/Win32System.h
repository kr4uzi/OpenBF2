// Windows audio-DLL lifecycle declarations.

#ifndef SOUND_SOUNDENGINE_WIN32SYSTEM_H
#define SOUND_SOUNDENGINE_WIN32SYSTEM_H

#if !defined(_WIN32)
#include <atomic>
#endif

namespace dice {
namespace hfe {
namespace win32 {

// LoadLibraryA, FreeLibrary-if-not-null,
// GetProcAddress-if-module. Opaque void* so that no TU but this one needs <windows.h>.
void* loadLibrary(const char* fileName);
void  freeLibrary(void* module);
void* getProcAddress(void* module, const char* name);

// OutputDebugStringA. The default ISoundTrace writes through this.
void  outputDebugString(const char* text);

// Sleep(ms).
void  sleep(unsigned int milliseconds);

// InterlockedExchange. The music thread's volume and fade words are written this way.
#if defined(_WIN32)
long  exchange(volatile long* target, long value);
#else
// [CHOSEN, not recovered] POSIX music hand-offs use C++ atomics, including their readers.
long  exchange(std::atomic<long>* target, long value);
#endif

// Worker thread raises its priority to THREAD_PRIORITY_TIME_CRITICAL before calling entry(this).
struct Thread
{
    void (__cdecl* entry)(Thread*);     // +0x00
    void*          argument;
    // +0x04
    void*          handle;              // +0x08
    unsigned long  id;                  // +0x0C
#if defined(_WIN32)
    volatile long  stopRequested;
#else
    // [CHOSEN, not recovered] POSIX worker stop request; no retail layout crosses this boundary.
    std::atomic<long> stopRequested;
#endif
    // +0x10
};

Thread* createThread(void (__cdecl* entry)(Thread*), void* argument);
void    joinAndDeleteThread(Thread* thread);
bool    threadShouldStop(const Thread* thread);
void*   threadArgument(const Thread* thread);

// Returns whether DirectSound enumeration finds a device. Enumeration failure logs a warning and returns false.
bool hasSoundDevice();

// ds:the DLL's own hinstance, stored by DllMain. No reader in the image.
extern void* g_dllInstance;

} // namespace win32
} // namespace hfe
} // namespace dice

#endif // SOUND_SOUNDENGINE_WIN32SYSTEM_H
