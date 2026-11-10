// [TEMP] [CHOSEN, not recovered] CPU gameswf test link scaffolding. SwiffPlayer's
// archive also carries game-owned UI natives; this fixture must never use them.
// [RETIRE-WHEN: swf-xml-link-isolation] Remove when the XML and popup fixtures no longer
// pulls those natives from the production archive, and verify its strict link.
#include <cstdio>
#include <cstdlib>

#include "../../BF2/Platform/macOS/CocoaHost.h"

namespace dice {
namespace hfe {
namespace macos {

namespace {
[[noreturn]] void unavailable(const char* method)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::macos::CocoaHost::%s reached in CPU gameswf fixture\n", method);
    std::fflush(stderr);
    std::abort();
}
}

CocoaHost& CocoaHost::instance()
{
    unavailable("instance");
}

void CocoaHost::setCursor(void*)
{
    unavailable("setCursor");
}

unsigned int CocoaHost::modifierFlags() const
{
    unavailable("modifierFlags");
}

unsigned short CocoaHost::consumeAsyncEnterState()
{
    unavailable("consumeAsyncEnterState");
}

} // namespace macos
} // namespace hfe
} // namespace dice
