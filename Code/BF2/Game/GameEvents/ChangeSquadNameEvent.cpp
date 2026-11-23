#include "ChangeSquadNameEvent.h"

#include <cstring>                               // strncpy

#include "../../Core/IBase.h"
#include "../../IO/BitStream.h"                  // readBits / writeBits / skipBits
#include "../GameLogic/SquadManager/ISquadManager.h" // typed SquadManager dispatch
#include "../Objects/IPlayer.h"                  // getTeam - the team check
#include "../../Scene/Object/PlayerManager.h"    // getPlayerFromId - the team check

namespace dice {
namespace hfe {

extern IBase* squadManager;                      // ds:0x8ACBFA4 - Core/Globals.cpp (IBase*, no layout)

namespace {
// .rodata dword GEChangeSquadName (0x88887E4) = 22.
const int GEChangeSquadName = 22;

} // namespace

//  (C2) / 0x806B470 (C1), 128 B. Clamp, copy, terminate, then the three ids.
// The ELF's copy reads `strncpy(this + 0x0C, name.c_str(), m_nameLength)` with the c_str() call
// inlined as libstdc++'s "write _S_terminal at data()[size()]" - compiler scaffolding, replaced
// here by the ordinary C++ that produces it.
ChangeSquadNameEvent::ChangeSquadNameEvent(int team, int squadId, int playerId,
                                           const std::string& name)
    : GameEvent()
{
    unsigned int length = static_cast<unsigned int>(name.size());
    if (length > static_cast<unsigned int>(MAX_NAME_LENGTH))
        length = MAX_NAME_LENGTH;

    m_nameLength = static_cast<unsigned char>(length);                   // +0x2C
    strncpy(m_name, name.c_str(), length);                               // +0x0C
    m_name[length] = '\0';

    m_team     = static_cast<unsigned char>(team);                       // +0x2D
    m_squadId  = static_cast<unsigned char>(squadId);                    // +0x2E
    m_playerId = static_cast<unsigned char>(playerId);                   // +0x2F
}

int  ChangeSquadNameEvent::getType() const { return GEChangeSquadName; }
void ChangeSquadNameEvent::logInfo(bool, int) {}

// executeClient - 0x806B700, 152 B. Build a std::string from the inline buffer and apply it,
// suppressing the echo (the trailing true).
void ChangeSquadNameEvent::executeClient()
{
    if (!squadManager)
        return;

    static_cast<ISquadManager*>(squadManager)->setSquadName(
        m_team, m_squadId, m_playerId, std::string(m_name), true);       // +0x1C
}

// Renames a squad for its leader or while it has no leader. Compares the full
// connection id, but passes the payload's player id through unchecked. Ignored
// requests still return true.
bool ChangeSquadNameEvent::executeServer(int connectionId)
{
    if (!squadManager)
        return true;

    // [CHOSEN, not recovered] Retail fix "The server refuses a squad request for another team"
    // (docs/retail-fixes.md). Retail (ELF 0x806B7A0) renames the packet's team's squad and lets
    // setSquadName add the named player to it, whatever team that player is on.
    world::IPlayer* const target = world::playerManager->getPlayerFromId(m_playerId);
    if (target && target->getTeam() != m_team)
        return true;

    const int leader = static_cast<ISquadManager*>(squadManager)->getSquadLeader(
        m_team, m_squadId);                                                           // +0x3C

    if (leader == -1 || leader == connectionId)
    {
        static_cast<ISquadManager*>(squadManager)->setSquadName(
            m_team, m_squadId, m_playerId, std::string(m_name), false);               // +0x1C
    }

    return true;
}

// [CHOSEN, not recovered] Retail fix "Game event readers keep wire lengths and counts inside their
// buffers" (docs/retail-fixes.md). Bounds stored names to MAX_NAME_LENGTH, unlike the original
// unbounded reader. Consumes excess bytes so subsequent events remain aligned, and terminates the
// stored name.
bool ChangeSquadNameEvent::deSerialize(io::BitStream* stream)
{
    int squadId = 0;
    stream->readBits(&squadId, 8);
    m_squadId = static_cast<unsigned char>(squadId);

    int team = 0;
    stream->readBits(&team, 8);
    m_team = static_cast<unsigned char>(team);

    int playerId = 0;
    stream->readBits(&playerId, 8);
    m_playerId = static_cast<unsigned char>(playerId);

    int length = 0;
    stream->readBits(&length, 8);

    unsigned int onTheWire = static_cast<unsigned int>(length) & 0xFFu;
    unsigned int stored    = onTheWire;
    if (stored > static_cast<unsigned int>(MAX_NAME_LENGTH))
        stored = MAX_NAME_LENGTH;                          // the divergence - see above

    m_nameLength = static_cast<unsigned char>(stored);
    stream->readBits(m_name, stored * 8);
    if (onTheWire > stored)
        stream->skipBits((onTheWire - stored) * 8);        // keep the cursor where the ELF puts it
    m_name[stored] = '\0';

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

// serialize - 0x806B5F0, 261 B. The same swapped id order, then the length, then the bytes.
// Nothing to clamp: m_nameLength can only ever hold what the constructor or deSerialize allowed.
bool ChangeSquadNameEvent::serialize(io::BitStream* stream)
{
    int type = getType();
    stream->writeBits(&type, gameEventTypeIdBitCount());

    int squadId = m_squadId;
    stream->writeBits(&squadId, 8);

    int team = m_team;
    stream->writeBits(&team, 8);

    int playerId = m_playerId;
    stream->writeBits(&playerId, 8);

    int length = m_nameLength;
    stream->writeBits(&length, 8);

    stream->writeBits(m_name, static_cast<unsigned int>(m_nameLength) * 8);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

} // namespace hfe
} // namespace dice
