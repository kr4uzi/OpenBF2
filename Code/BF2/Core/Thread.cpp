// Platform thread creation and joining. Source path [INFERRED].
// The shared trampoline invokes run(); platform-specific lifecycle behavior stays in each arm.

#include "Thread.h"
#include "../Platform/Host/BusyService.h"

#if defined(__APPLE__) && !DEDICATED && !(defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR)
#include <chrono>
#include <condition_variable>
#include <mutex>
#endif

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <pthread.h>
#endif

namespace dice {
namespace hfe {

// Owns the platform thread state. An indirect control block keeps the x86 Thread layout at eight bytes.
// Windows stores a handle and thread ID; POSIX stores a pthread_t.
struct Thread::Impl
{
#if defined(_WIN32)
    HANDLE    handle;        // +0x00
    DWORD     tid;           // +0x04
    bool      running;       // +0x08
#else
    pthread_t tid;           // +0x00
    bool      running;       // +0x04
#if defined(__APPLE__) && !DEDICATED && !(defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR)
    // [CHOSEN, not recovered] A main-thread join services the host while waiting, including
    // the listen server's receive thread (which may be in its four-second socket wait).
    Thread* owner;
    std::mutex mutex;
    std::condition_variable completed;
    bool finished = false;
    static void* entry(void* param)
    {
        Impl* impl = static_cast<Impl*>(param);
        impl->owner->run();
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            impl->finished = true;
        }
        impl->completed.notify_one();
        return 0;
    }
#endif
#endif
};

namespace {

// Dispatches the thread body through a typed virtual call so differing destructor-slot counts
// on MSVC and Itanium do not change which function runs.
#if defined(_WIN32)
DWORD WINAPI threadTrampoline(LPVOID param)
{
    static_cast<Thread*>(param)->run();
    return 0;
}
#elif !(defined(__APPLE__) && !DEDICATED && !(defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR))
void* pthreads_thread_trampoline(void* param)
{
    static_cast<Thread*>(param)->run();
    return 0;
}
#endif

} // anonymous namespace

// Allocates platform state. [PLATFORM] Windows permits a null allocation; the POSIX arm dereferences it.
Thread::Thread()
{
#if defined(_WIN32)
    m_impl = new Impl;
    if (m_impl)
    {
        m_impl->handle  = 0;
        m_impl->tid     = 0;
        m_impl->running = false;
    }
#else
    m_impl = new Impl;
    m_impl->running = false;        // the ctor zeroes only this field; tid is left alone
#endif
}

// Frees the control block without stopping or joining the thread. Call stop before destruction to avoid use-after-free.
Thread::~Thread()
{
    delete m_impl;
}

// start — / The 102-byte difference is the priority switch.

// `name` is ignored on both platforms. `priority` is ignored on linux. See Core/Thread.h; both
// are preserved as written rather than "fixed".
bool Thread::start(const char* /*name*/, Priority priority)
{
#if defined(_WIN32)
    m_impl->handle = CreateThread(0, 0, threadTrampoline, this, 0, &m_impl->tid);

    if (!m_impl->handle)
    {
        m_impl->running = false;
        return false;
    }
    m_impl->running = true;

    // Priority_Normal (2) deliberately makes no call — CreateThread already starts a thread at
    // normal priority. The default arm does call, with 0. Same observable result, different path;
    // see the enum's note in Core/Thread.h.
    switch (priority)
    {
        case Priority_Idle:         SetThreadPriority(m_impl->handle, -15); break;
        case Priority_BelowNormal:  SetThreadPriority(m_impl->handle,  -1); break;
        case Priority_Normal:                                               break;
        case Priority_AboveNormal:  SetThreadPriority(m_impl->handle,  +1); break;
        case Priority_TimeCritical: SetThreadPriority(m_impl->handle, +15); break;
        default:                    SetThreadPriority(m_impl->handle,   0); break;
    }
    return true;
#else
    // no priority handling at all on this side, and none is missing — the ELF body has none.
    (void)priority;

#if defined(__APPLE__) && !DEDICATED && !(defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR)
    // [CHOSEN, not recovered] Completion is published before joining; thread ownership and
    // the final pthread_join remain unchanged.
    m_impl->owner = this;
    m_impl->finished = false;
    const int result = pthread_create(&m_impl->tid, 0, Impl::entry, m_impl);
#else
    const int result = pthread_create(&m_impl->tid, 0, pthreads_thread_trampoline, this);
#endif
    if (result != 0)
    {
        m_impl->running = false;
        return false;
    }
    m_impl->running = true;
    return true;
#endif
}

// Waits for run() to finish; does not request cancellation or return its result.
// [PLATFORM] POSIX blocks in pthread_join and clears running only on success, returning false on
// join failure. Windows busy-polls GetExitCodeThread, closes the handle and always returns true.
// The Windows loop re-reads m_impl each iteration. Callers must signal the worker before joining.
#if defined(_WIN32)
bool Thread::stop()
{
    if (m_impl->running)                             // impl+8
    {
        DWORD exitCode = STILL_ACTIVE;               // 0x103 - seeded so the loop test is well-defined
        while (GetExitCodeThread(m_impl->handle, &exitCode) && exitCode == STILL_ACTIVE)
            ;                                        // busy spin, as shipped
        CloseHandle(m_impl->handle);
        m_impl->handle  = 0;
        m_impl->running = false;
    }
    return true;
}
#else
bool Thread::stop()
{
#if defined(__APPLE__) && !DEDICATED && !(defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR)
    // [CHOSEN, not recovered] Never dispatch engine callbacks during teardown. Workers keep
    // the blocking join; only the AppKit thread uses the loading host's held-event service.
    if (m_impl->running && pthread_main_np())
    {
        std::unique_lock<std::mutex> lock(m_impl->mutex);
        while (!m_impl->finished)
        {
            lock.unlock();
            host::serviceWhileBusy();
            lock.lock();
            m_impl->completed.wait_for(lock, std::chrono::milliseconds(50),
                                      [this] { return m_impl->finished; });
        }
    }
#endif
    void* threadReturn;                              // read by pthread_join, never used

    if (pthread_join(m_impl->tid, &threadReturn) != 0)
        return false;                                // `running` deliberately not cleared here

    m_impl->running = false;
    return true;
}
#endif

} // namespace hfe
} // namespace dice
