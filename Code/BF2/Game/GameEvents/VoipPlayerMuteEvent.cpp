#include "VoipPlayerMuteEvent.h"

#include "../IGame.h"                            // isPlayingDemo, sendGameEvent(IfConnected)
#include "../Main/Game.h"
#include "../GameLogic/Player.h"                 // setIsMuted - the +0x288 slot
#include "../../Core/EventManager.h"             // eventManager->postEvent
#include "../../IO/BitStream.h"                  // readBits / writeBits
#include "../../Scene/Object/PlayerManager.h"    // getPlayerFromId / getLocalHumanPlayer

namespace dice {
namespace hfe {

namespace {
// .rodata dword GEVoipPlayerMuteEvent (0x888AEC4) = 38.
const int GEVoipPlayerMuteEvent = 38;

// The local notification both arms raise: category ECHud (2), event id 0x3B = 59.
const unsigned int NOTIFY_VOIP_PLAYER_MUTE = 0x3Bu;

// Mute notification with a byte flag and aligned player id. The server forwards
// only the flag's eight bits; local client dispatch passes the complete payload.
// [UNRESOLVED] Original payload type name.
struct VoipPlayerMuteNotification
{
    unsigned char muted;
    int           mutedPlayerId;
};
} // namespace

// NO OUT-OF-line ELF address - executeServer builds one inline at 0x8081789. These are its
// stores.
VoipPlayerMuteEvent::VoipPlayerMuteEvent(bool muted, int mutedPlayerId, int targetPlayerId)
    : GameEvent()
    , m_muted(muted)                                                      // +0x0C
    , m_mutedPlayerId(static_cast<unsigned char>(mutedPlayerId))          // +0x0D
    , m_targetPlayerId(static_cast<unsigned char>(targetPlayerId))        // +0x0E
{
}

int  VoipPlayerMuteEvent::getType() const { return GEVoipPlayerMuteEvent; }
void VoipPlayerMuteEvent::logInfo(bool, int) {}

// Applies the mute only on the addressed player's client.
// Posts the HUD notification before updating the Player flag; the second lookup may fail.
void VoipPlayerMuteEvent::executeClient()
{
    if (game->isPlayingDemo())                                                        // +0xE0
        return;

    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();               // +0x30
    if (!local || local->getId() != m_targetPlayerId)                                  // +0x54
        return;

    VoipPlayerMuteNotification notification;
    notification.muted         = static_cast<unsigned char>(m_muted ? 1 : 0);
    notification.mutedPlayerId = m_mutedPlayerId;
    eventManager->postEvent(ECHud, NOTIFY_VOIP_PLAYER_MUTE, &notification, 0.0f);      // +0x14

    world::IPlayer* muted = world::playerManager->getPlayerFromId(m_mutedPlayerId);    // +0x20
    if (muted)
        static_cast<Player*>(muted)->setIsMuted(m_muted);                              // +0x288
}

// Delivers locally to a hosting target, otherwise forwards by payload id.
// A local target in dedicated mode is refused; connectionId is unused.
bool VoipPlayerMuteEvent::executeServer(int /*connectionId*/)
{
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();               // +0x30
#if !DEDICATED
    // BF2.exe 0x542176..0x5421DB snapshots the HUD payload before testing the target.
    VoipPlayerMuteNotification notification;
    notification.muted = static_cast<unsigned char>(m_muted ? 1 : 0);
    notification.mutedPlayerId = m_mutedPlayerId;
#endif
    if (local && local->getId() == m_targetPlayerId)                                   // +0x54
    {
#if !DEDICATED
        if (!isDedicated())
        {
            eventManager->postEvent(ECHud, NOTIFY_VOIP_PLAYER_MUTE, &notification, 0.0f);
            world::IPlayer* muted = world::playerManager->getPlayerFromId(m_mutedPlayerId);
            if (muted)
                static_cast<Player*>(muted)->setIsMuted(m_muted);
            return true;
        }
#endif
        return false;
    }

    game->sendGameEvent(m_targetPlayerId,
                        new VoipPlayerMuteEvent(m_muted, m_mutedPlayerId,
                                                m_targetPlayerId));                    // +0x90

#if DEDICATED
    VoipPlayerMuteNotification notification;
    notification.muted         = static_cast<unsigned char>(m_muted ? 1 : 0);
    notification.mutedPlayerId = m_mutedPlayerId;                                      // never sent - see above
#endif
    game->sendGameEventIfConnected(
        m_targetPlayerId,
        new PostRemoteEvent(ECHud, NOTIFY_VOIP_PLAYER_MUTE, true,
                            &notification, 8u, 0.0f));                                 // +0x98

    return true;
}

// deSerialize - 0x80814D0, 167 B. The bool leads - see the header.
bool VoipPlayerMuteEvent::deSerialize(io::BitStream* stream)
{
    int muted = 0;
    stream->readBits(&muted, 1);
    m_muted = (muted == 1);

    int mutedPlayerId = 0;
    stream->readBits(&mutedPlayerId, 8);
    m_mutedPlayerId = static_cast<unsigned char>(mutedPlayerId);

    int targetPlayerId = 0;
    stream->readBits(&targetPlayerId, 8);
    m_targetPlayerId = static_cast<unsigned char>(targetPlayerId);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

bool VoipPlayerMuteEvent::serialize(io::BitStream* stream)
{
    int type = getType();
    stream->writeBits(&type, gameEventTypeIdBitCount());

    int muted = m_muted ? 1 : 0;
    stream->writeBits(&muted, 1);

    int mutedPlayerId = m_mutedPlayerId;
    stream->writeBits(&mutedPlayerId, 8);

    int targetPlayerId = m_targetPlayerId;
    stream->writeBits(&targetPlayerId, 8);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

} // namespace hfe
} // namespace dice
