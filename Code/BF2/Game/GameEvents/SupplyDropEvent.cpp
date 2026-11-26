#include "SupplyDropEvent.h"

#include "../../Core/IBase.h"
#include "../../IO/BitStream.h"                  // readBits / writeBits
#include "../GameLogic/SquadManager/ISquadManager.h" // typed +0x70 dispatch
#include "../Objects/Game/SupplyDropManager.h"

namespace dice {
namespace hfe {

extern IBase* squadManager;                      // ds:0x8ACBFA4 - Core/Globals.cpp (IBase*, no layout)

namespace {
// .rodata dword GESupplyDrop (0x888A9C0) = 37.
const int GESupplyDrop = 37;

inline ISquadManager* squadManagerIface()
{
    return static_cast<ISquadManager*>(squadManager);
}

} // namespace

// [INFERRED] argument order - no ELF constructor exists.
SupplyDropEvent::SupplyDropEvent(float positionX, float positionZ, int team)
    : GameEvent()
{
    m_position.x = positionX;
    m_position.y = positionZ;
    // Both ports' manager construction path stores only the low byte at +0x14; the other three
    // bytes are deliberately left as the allocation supplied them. executeServer reads that same
    // byte, while serialize preserves the shipped indeterminate high-byte quirk on the wire.
    *reinterpret_cast<unsigned char*>(&m_team) = static_cast<unsigned char>(team);
}

int SupplyDropEvent::getType() const
{
    return GESupplyDrop;
}

// Requests supply drop only for the commander. Missing managers or rejected
// requests still return true. Commander ID and team comparisons use their low bytes.
bool SupplyDropEvent::executeServer(int connectionId)
{
    if (!squadManager)
        return true;

    const int team = static_cast<unsigned char>(m_team);              // byte of a 32-bit field

    if (squadManagerIface()->getCommander(team)                       // +0x70
            != static_cast<unsigned char>(connectionId))              // byte-truncated
        return true;

    world::supplyDropManager->requestSupplyDrop(static_cast<unsigned char>(team), m_position);

    return true;
}

// deSerialize - 0x807E5A0, 139 B. The team leads, then the two floats.
bool SupplyDropEvent::deSerialize(io::BitStream* stream)
{
    int team = 0;
    stream->readBits(&team, 32);
    m_team = team;

    stream->readBits(&m_position.x, 32);
    stream->readBits(&m_position.y, 32);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

bool SupplyDropEvent::serialize(io::BitStream* stream)
{
    int type = getType();
    stream->writeBits(&type, gameEventTypeIdBitCount());

    int team = m_team;
    stream->writeBits(&team, 32);

    stream->writeBits(&m_position.x, 32);
    stream->writeBits(&m_position.y, 32);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

} // namespace hfe
} // namespace dice
