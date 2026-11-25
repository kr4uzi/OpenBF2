// [CHOSEN, not recovered] Host servicing during synchronous engine work.
#ifndef PLATFORM_HOST_BUSYSERVICE_H
#define PLATFORM_HOST_BUSYSERVICE_H

#if defined(__APPLE__) && !DEDICATED && !(defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR)
#include "ClientHost.h"
#include <pthread.h>
#endif

namespace dice { namespace hfe { namespace host {

// Keeps the native busy cursor moving without dispatching input into partially torn-down state.
// Other images/hosts need no service; worker threads must never enter AppKit.
inline void serviceWhileBusy()
{
#if defined(__APPLE__) && !DEDICATED && !(defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR)
    if (pthread_main_np())
        ClientHost::instance().serviceWhileBusy();
#endif
}

} } }
#endif
