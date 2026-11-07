// Client music player for mod and map tracks.

#ifndef SOUND_BFSOUND_MUSICPLAYER_H           // [INFERRED] guard style
#define SOUND_BFSOUND_MUSICPLAYER_H

#include "../../Core/BuildConfig.h"
#include "../../Game/IEventListener.h"

namespace dice {
namespace hfe {

// sizeof == 8: the IEventListener vptr and one int of state. The constructor registers the
// object as an ECMenu (category 10) handler at priority 100 - the same category BF2Engine
// registers itself for.
class MusicPlayer : public IEventListener
{
public:
    MusicPlayer();
    // Unregisters the ECMenu handler. Nonvirtual: BF2Engine::shutdown deletes the player by its
    // concrete type, and the vtable holds only handleEvent.
    ~MusicPlayer();
    // Changes between idle, mod music, and map music using the supplied level path.
    void setState(int mode, const char* levelPath);
    // Ignores dedicated mode and categories other than 10; event 3 starts map music and 5 stops it.
    virtual void handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data);

private:
    // [INFERRED] helper names; signatures and bodies read at these client PE addresses.
    void startMenuMusic();
    void startMapMusic(const char* levelPath);
    void invalidEvent(int mode);
    /* +0x04 */ int m_state;                         // ctor = 0
};

} // namespace hfe
} // namespace dice

#endif // SOUND_BFSOUND_MUSICPLAYER_H
