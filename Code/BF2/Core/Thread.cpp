// Platform thread creation and joining. Source path [INFERRED].
// The shared trampoline invokes run(); platform-specific lifecycle behavior stays in each arm.

#include "Thread.h"

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
#else
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

    if (pthread_create(&m_impl->tid, 0, pthreads_thread_trampoline, this) != 0)
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
    void* threadReturn;                              // read by pthread_join, never used

    if (pthread_join(m_impl->tid, &threadReturn) != 0)
        return false;                                // `running` deliberately not cleared here

    m_impl->running = false;
    return true;
}
#endif

} // namespace hfe
} // namespace dice
