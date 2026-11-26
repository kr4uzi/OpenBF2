#include "ArtilleryEvent.h"

#include "../Objects/IObject.h"                  // queryInterface, handleMessage (slot 65 = +0x104)
#include "../Objects/Game/ArtilleryHandler.h"
#include "../Objects/Weapons/GenericFireArm.h"
#include "../../Core/Debug.h"                    // both abort sites
#include "../../Core/IBase.h"
#include "../GameLogic/SquadManager/ISquadManager.h" // typed +0x70 dispatch
#include "../../Core/Ids.h"                      // IID_IPlayerControlObject
#include "../../IO/BitStream.h"                  // readBits / writeBits
#include "../../IO/Network/NetworkManager.h"     // getObject
#include "../../Scene/Object/PlayerManager.h"    // getPlayerFromId
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // getWeapons(int)

namespace dice {
namespace hfe {

extern IBase* squadManager;                      // ds:0x8ACBFA4 - Core/Globals.cpp (IBase*, no layout)
namespace io    { extern NetworkManager* networkManager; }              // ds:0x8AC3D1C

namespace {
// .rodata dword GEArtillery (0x88883E4) = 30.
const int GEArtillery = 30;

// The message the fired weapon is handed. IObject::handleMessage takes a TemplateMessage; 7 is a
// bare immediate here and Game/Objects/IObject.h does not name the enumerators.
const int WEAPON_MESSAGE_FIRE = 7;               // [UNRESOLVED] enumerator name

inline ISquadManager* squadManagerIface()
{
    return static_cast<ISquadManager*>(squadManager);
}
} // namespace

// Zeros only the target vector; the remaining payload must be supplied by a producer or deserialization.
ArtilleryEvent::ArtilleryEvent()
    : GameEvent()
{
    m_target.x = 0.0f;                           // +0x0C
    m_target.y = 0.0f;                           // +0x10
}

#if !DEDICATED
// Complete PE7B23F0. The request constructor does not initialize the other
// direction's fire tick/net id or the surrounding padding. See header for the
// explicitly chosen source spelling of the unread fourth stack word.
ArtilleryEvent::ArtilleryEvent(int team, Vec2 target, unsigned int /*unusedWord*/)
    : GameEvent(), m_target(target), m_team(static_cast<unsigned char>(team)),
      m_artilleryType(ArtilleryTypeRequestFire)
{
}
#endif

int ArtilleryEvent::getType() const
{
    return GEArtillery;
}

// Plays artillery firing on the control object's first weapon. Only DidFire is valid;
// other types are fatal. Commander lookup may yield null. Preserve the weapon
// operation order: tick stamp, enable, player, firing flag, then message 7.
void ArtilleryEvent::executeClient()
{
    if (m_artilleryType != ArtilleryTypeDidFire)
    {
        Debug(DEBUGTYPE_6, "Game/GameEvents/ArtilleryEvent.cpp", 128, "")
            << "Should never be sent to client";                  // aborts
        return;
    }

    world::IObject* object = io::networkManager->getObject(m_weaponNetId);
    if (!object)
        return;

    world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
        object->queryInterface(world::IID_IPlayerControlObject));  // IObject +0x0C
    if (!pco)
        return;

    // getWeapons(0) returns a vector-like; the first word is `begin`, and element zero is the gun.
    world::IObject* const* const* weapons =
        static_cast<world::IObject* const* const*>(pco->getWeapons(0));        // +0x74
    world::IObject* weaponObject = (*weapons)[0];
    if (!weaponObject)
        return;
    world::GenericFireArm* weapon = static_cast<world::GenericFireArm*>(weaponObject);

    world::IPlayer* commander = 0;
    if (squadManager)
    {
        const int commanderId = squadManagerIface()->getCommander(m_team);        // +0x70
        commander = world::playerManager->getPlayerFromId(commanderId);           // +0x20
    }

    weapon->m_dword1C4 = m_fireTick;
    weapon->enableWeapon();
    weapon->setPlayer(commander);
    weapon->m_byte208 = true;
    weapon->handleMessage(static_cast<world::TemplateMessage>(WEAPON_MESSAGE_FIRE), 0, false);
}

// Requests artillery fire only for the commander; compares the connection's low
// byte. Wrong event type is fatal. Missing managers/handler or unauthorized senders
// are ignored with true. Team indexing is unchecked.
bool ArtilleryEvent::executeServer(int connectionId)
{
    if (m_artilleryType != ArtilleryTypeRequestFire)
    {
        Debug(DEBUGTYPE_6, "Game/GameEvents/ArtilleryEvent.cpp", 140, "")
            << "Should never be sent to server";                  // aborts
        return false;
    }

    if (!squadManager)
        return true;

    // THE truncation IS ON THE connection ID, NOT ON getCommander's result - see above.
    if (squadManagerIface()->getCommander(m_team) !=
        static_cast<int>(static_cast<unsigned char>(connectionId)))
        return true;

    if (!world::artilleryManager)
        return true;

    world::ArtilleryHandler* handler =
        world::artilleryManager->getHandler(m_team);                   // unbounded, as shipped
    if (!handler)
        return true;

    // copied TO A stack Vec2 first. setTarget takes a NON-const reference, so it may write back;
    // the ELF builds the pair at [ebp-0x148] from +0x0C and +0x10 rather than passing the member.
    Vec2 target;
    target.x = m_target.x;
    target.y = m_target.y;

    if (handler->setTarget(target))
        handler->requestFire();
    else
        handler->failedFire();

    return true;
}

// Reads type, rereads its member to select payload, and truncates the 32-bit team
// to a byte. Unknown types consume only type/team without reporting a stream error.
bool ArtilleryEvent::deSerialize(io::BitStream* stream)
{
    int artilleryType = 0;
    stream->readBits(&artilleryType, 2);
    m_artilleryType = artilleryType;

    int team = 0;
    stream->readBits(&team, 32);                 // 32 bits in, one byte kept
    m_team = static_cast<unsigned char>(team);

    if (m_artilleryType == ArtilleryTypeRequestFire)
    {
        stream->readBits(&m_target.x, 32);
        stream->readBits(&m_target.y, 32);
    }
    else if (m_artilleryType == ArtilleryTypeDidFire)
    {
        stream->readBits(&m_weaponNetId, 16);    // in place - the field is exactly 16 bits wide
        stream->readBits(&m_fireTick, 32);
    }

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

// serialize - 0x8069760, 304 B. The exact mirror.
// THE type IS  straight from THE field (`lea eax, [esi+1Ch]`, no load), and so is the net
// id - only the team needs a temporary, because it is one byte going into thirty-two.
bool ArtilleryEvent::serialize(io::BitStream* stream)
{
    int type = getType();
    stream->writeBits(&type, gameEventTypeIdBitCount());

    stream->writeBits(&m_artilleryType, 2);      // from the field, no load

    int team = m_team;                           // movzx - zero-extended into 32 wire bits
    stream->writeBits(&team, 32);

    if (m_artilleryType == ArtilleryTypeRequestFire)
    {
        stream->writeBits(&m_target.x, 32);
        stream->writeBits(&m_target.y, 32);
    }
    else if (m_artilleryType == ArtilleryTypeDidFire)
    {
        stream->writeBits(&m_weaponNetId, 16);
        stream->writeBits(&m_fireTick, 32);
    }

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

} // namespace hfe
} // namespace dice
