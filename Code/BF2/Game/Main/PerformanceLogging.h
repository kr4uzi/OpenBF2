// [INFERRED] path and file. No __FILE__ string names this class; the inline handleEvent is emitted
// as a weak symbol beside BF2Engine's code in the Linux ELF.
#ifndef GAME_MAIN_PERFORMANCELOGGING_H
#define GAME_MAIN_PERFORMANCELOGGING_H

#include "../IEventListener.h"

namespace dice {
namespace hfe {

// The performance logger BF2Engine keeps at +0x74 (BF2Engine::performanceLogging). Retail 1.50 never
// creates one: in the Linux ELF the vtable (0x08892F60, a single handleEvent slot; typeinfo 0x08892FA8
// with the single base IEventListener) is stored only by the two inlined destructors in
// BF2Engine::shutdownGame (0x08138259) and BF2Engine::shutdownEngine (0x081394DF), and its setting
// GSPerfLogAtPlayerCount is only registered as a default (BF2Engine::initDefaultSettings, 0x0812F44D).
// The client PE is [INFERRED] to match. The class is complete here so that BF2Engine's `delete`
// runs its inline, non-virtual destructor and then operator delete, as both inlined destructors do.
// [CHOSEN, not recovered] No data members: nothing constructs the object or reads a field.
class PerformanceLogging : public IEventListener
{
public:
    // ELF 0x0813AFF0: an empty body.
    virtual void handleEvent(EventCategory, unsigned int, EventNode*, void*) {}
};

} // namespace hfe
} // namespace dice

#endif // GAME_MAIN_PERFORMANCELOGGING_H
