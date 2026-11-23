#include "JoinSquadEvent.h"

#include "../IGame.h"                            // game->queryInterface(IID_IGameServer)
#include "../GameLogic/SquadManager/ISquadManager.h" // typed SquadManager +0x30 dispatch
#include "../../Core/Ids.h"                      // IID_IGameServer
#include "../../IO/BitStream.h"                  // readBits / writeBits
#include "../Objects/IPlayer.h"                  // getTeam - the team check
#include "../../Scene/Object/PlayerManager.h"    // getPlayerFromId - the team check

namespace dice {
namespace hfe {

extern IBase* squadManager;                      // ds:0x8ACBFA4 - Core/Globals.cpp (IBase*, no layout)

namespace {
// .rodata dword GEJoinSquad (0x8889B04) = 16. Read from the global, as getType does.
const int GEJoinSquad = 16;

//   vtable _ZTVN4dice3hfe12SquadManagerE (0x8891AE0) slot +0x30
//     = SquadManager::addToSquad(int team, int squad, int playerId)   ELF 0x811CBE0, 54 B
// which forwards to the four-argument addToSquad(0x811CC20) as addToSquad(64, squad, player, team).
} // namespace

// [INFERRED] argument order - no ELF constructor exists. The stores are the attested layout.
JoinSquadEvent::JoinSquadEvent(int team, int playerId, int squadId)
    : m_team(static_cast<unsigned char>(team))
    , m_playerId(static_cast<unsigned char>(playerId))
    , m_squadId(static_cast<unsigned char>(squadId))
{
}

int JoinSquadEvent::getType() const
{
    return GEJoinSquad;
}

// Intentionally empty: squad state reaches clients through SquadManager broadcasts.
void JoinSquadEvent::executeClient()
{
}

// Joins the transport sender to a squad, ignoring the packet's player ID.
// Returns false when the game does not expose IGameServer.
bool JoinSquadEvent::executeServer(int connectionId)
{
    if (!game->queryInterface(IID_IGameServer))
        return false;

    // [CHOSEN, not recovered] Retail fix "The server refuses a squad request for another team"
    // (docs/retail-fixes.md). Retail passes the packet's team to addToSquad unchecked (ELF
    // 0x8075270), and addToSquad (ELF 0x811CC20) never compares it with the player's team.
    world::IPlayer* const sender = world::playerManager->getPlayerFromId(connectionId);
    if (sender && sender->getTeam() != m_team)
        return true;

    if (squadManager)
        static_cast<ISquadManager*>(squadManager)->addToSquad(
            m_team, m_squadId, connectionId);                                  // +0x30

    return true;
}

// deSerialize - 0x80750F0, 158 B. Three bytes, in field order.
// inherited wholesale BY SetSquadLeaderEvent - its vtable slot +0x2C holds this function. Any
// change to the wire here silently changes that event's wire too. LeaveSquadEvent, which adds a
// fourth field, overrides both halves instead.
bool JoinSquadEvent::deSerialize(io::BitStream* stream)
{
    int team = 0;
    stream->readBits(&team, 8);
    m_team = static_cast<unsigned char>(team);

    int playerId = 0;
    stream->readBits(&playerId, 8);
    m_playerId = static_cast<unsigned char>(playerId);

    int squadId = 0;
    stream->readBits(&squadId, 8);
    m_squadId = static_cast<unsigned char>(squadId);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

// serialize - 0x8075190, 204 B. The type id in the registry's width, then the same three bytes.
bool JoinSquadEvent::serialize(io::BitStream* stream)
{
    int type = getType();
    stream->writeBits(&type, gameEventTypeIdBitCount());

    int team = m_team;
    stream->writeBits(&team, 8);

    int playerId = m_playerId;
    stream->writeBits(&playerId, 8);

    int squadId = m_squadId;
    stream->writeBits(&squadId, 8);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

} // namespace hfe
} // namespace dice
