#include "UAVEvent.h"

#include "../../Core/EventManager.h"             // eventManager->postEvent
#include "../../Core/IBase.h"
#include "../../IO/BitStream.h"                  // readBits / writeBits
#include "../../Math/Vec3.h"
#include "../GameLogic/SquadManager/ISquadManager.h" // typed +0x70 dispatch
#include "../Objects/Game/CommanderToolsManager.h"   // shared 16-byte GL notification payload

namespace dice {
namespace hfe {

extern IBase* squadManager;                      // ds:0x8ACBFA4 - Core/Globals.cpp (IBase*, no layout)

namespace {
// .rodata dword geuav (0x888AC04) = 45.
const int GEUAV = 45;

// The two notifications executeServer can raise, both in ECGameLogic. Bare immediates in the
// binary - 0x11 at 0x807F752 and 0x12 at 0x807F6F2 - with no named constant to cite.
const unsigned int NOTIFY_UAV_TEAM   = 0x11u;    // 17 - the m_isScan == false arm
const unsigned int NOTIFY_UAV_PLAYER = 0x12u;    // 18 - the m_isScan == true  arm

inline ISquadManager* squadManagerIface()
{
    return static_cast<ISquadManager*>(squadManager);
}

} // namespace

// [INFERRED] argument order - no ELF constructor exists.
UAVEvent::UAVEvent(float positionX, float positionZ, int team, bool isScan)
    : GameEvent()
    , m_positionX(positionX)
    , m_positionZ(positionZ)
    , m_team(static_cast<unsigned char>(team))
    , m_isScan(isScan)
{
}

#if !DEDICATED
// PE7B2430, all18 instructions read. The dedicated ELF has no producer ctor.
UAVEvent::UAVEvent(int team, Vec2 position, bool isScan) : GameEvent()
{
    m_positionX = position.x;
    m_positionZ = position.y;
    m_isScan = isScan;
    m_team = static_cast<unsigned char>(team);
}
#endif

int UAVEvent::getType() const
{
    return GEUAV;
}

// an explicit empty override.
void UAVEvent::executeClient()
{
}

// Routes commander UAV requests using the connection id's low byte for authorization.
// Scan payloads identify the connection; the other arm identifies the team. Y is zero.
// Missing manager and unauthorized requests still return true.
bool UAVEvent::executeServer(int connectionId)
{
    if (!squadManager)
        return true;

    if (squadManagerIface()->getCommander(m_team)                            // +0x70
            != static_cast<unsigned char>(connectionId))                     // byte-truncated
        return true;

    world::CommanderToolNotification notification;
    notification.position.x = m_positionX;
    notification.position.y = 0.0f;                                          // hard-zeroed by the ELF
    notification.position.z = m_positionZ;

    if (m_isScan)
    {
        notification.id = connectionId;
        eventManager->postEvent(ECGameLogic, NOTIFY_UAV_PLAYER, &notification, 0.0f);   // +0x14
    }
    else
    {
        notification.id = m_team;
        eventManager->postEvent(ECGameLogic, NOTIFY_UAV_TEAM, &notification, 0.0f);     // +0x14
    }

    return true;
}

// deSerialize - 0x807F4A0, 167 B. Identical to DropVehicleEvent's.
bool UAVEvent::deSerialize(io::BitStream* stream)
{
    stream->readBits(&m_positionX, 32);
    stream->readBits(&m_positionZ, 32);

    int team = 0;
    stream->readBits(&team, 8);
    m_team = static_cast<unsigned char>(team);

    int isScan = 0;
    stream->readBits(&isScan, 1);
    m_isScan = (isScan == 1);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

bool UAVEvent::serialize(io::BitStream* stream)
{
    int type = getType();
    stream->writeBits(&type, gameEventTypeIdBitCount());

    stream->writeBits(&m_positionX, 32);
    stream->writeBits(&m_positionZ, 32);

    int team = m_team;
    stream->writeBits(&team, 8);

    int isScan = m_isScan ? 1 : 0;
    stream->writeBits(&isScan, 1);

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

} // namespace hfe
} // namespace dice
