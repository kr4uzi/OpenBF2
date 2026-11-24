// Shared game lifecycle, frame helpers and network-event registration.
// Registration is one-shot because duplicate makers are fatal.
// [UNRESOLVED] Retail's discarded event temporaries have no reproduced source expression;
// the registry uses static makers and omits their side-effect-free construction.

#include "Game.h"

#include "../../Core/IBase.h"
#include "../../Core/EventManager.h"
#include "../../Core/Ids.h"
#include "../../Core/StringUtil.h"
#include "../../Core/System.h"
#include "../../IO/Network/INetworkable.h"
#include "../../IO/Network/NetworkManager.h"
#include "../../Physics/ResponsePhysicsManager.h"
#include "../../Scene/Object/ObjectManager.h"
#include "../../Scene/Object/PlayerManager.h"

#include "../Common/GameEvent.h"          // GameEventRegistry, IGameEventMaker, GameEventMakerMaker
#include "../Common/VoteEvent.h"
#include "../IGameLogic.h"
#include "../GameLogic/GameLogic.h"
#include "../GameLogic/SquadManager/ISquadInfo.h"
#include "../GameLogic/SquadManager/ISquadManager.h"
#include "../Objects/IObject.h"
#include "../Objects/ObjectLayoutAccess.h"
#include "../Objects/IObjectTemplate.h"
#include "../Objects/IPlayerControlObjectTemplate.h"
#include "../Objects/IPlayer.h"
#include "../Objects/ISoldier.h"
#include "../Objects/IVehicleHudComp.h"
#include "../Objects/GameLogicComp/IRadioGLComp.h"
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

#include "../GameEvents/CreatePlayerEvent.h"
#include "../Common/CreateObjectEvent.h"          // CreateObject + CreateKit
#include "../Common/SpawnGroupEvents.h"           // Create/RemoveSpawnGroup
#include "../GameEvents/AmbientEffectAreaEvent.h"
#include "../GameEvents/ArtilleryEvent.h"
#include "../GameEvents/BeginRoundEvent.h"
#include "../GameEvents/ChallengeEvent.h"
#include "../GameEvents/ChallengeResponseEvent.h"
#include "../GameEvents/ChangePlayerNameEvent.h"
#include "../GameEvents/ChangeSquadNameEvent.h"
#include "../GameEvents/CommanderCamEvent.h"
#include "../GameEvents/CommanderEvent.h"
#include "../GameEvents/ConnectionTypeEvent.h"
#include "../GameEvents/ContentCheckEvent.h"
#include "../GameEvents/DataBlockEvent.h"
#include "../GameEvents/DestroyObjectEvent.h"
#include "../GameEvents/DestroyPlayerEvent.h"
#include "../GameEvents/DropVehicleEvent.h"
#include "../GameEvents/EndOfRoundEvent.h"
#include "../GameEvents/EnterVehicleEvent.h"
#include "../GameEvents/ExitVehicleEvent.h"
#include "../GameEvents/FixPlayerTeamEvent.h"
#include "../GameEvents/GrapplingHookContainerCreateEvent.h"
#include "../GameEvents/GrapplingHookContainerUpdateEvent.h"
#include "../GameEvents/GrapplingHookContainerDetachEvent.h"
#include "../GameEvents/GrapplingHookCreateEvent.h"
#include "../GameEvents/GrapplingHookUpdateEvent.h"
#include "../GameEvents/HandleDropEvent.h"
#include "../GameEvents/HandlePickupEvent.h"
#include "../GameEvents/InviteEvent.h"
#include "../GameEvents/IssueSquadOrderEvent.h"
#include "../GameEvents/JoinSquadEvent.h"
#include "../GameEvents/KickBanEvent.h"
#include "../GameEvents/KilledByEvent.h"
#include "../GameEvents/LeaveSquadEvent.h"
#include "../GameEvents/MedalEvent.h"
#include "../GameEvents/MissileInitEvent.h"
#include "../GameEvents/PlayerTearGassedEvent.h"
#include "../GameEvents/RadioMessageEvent.h"
#include "../GameEvents/PythonCommandEvent.h"
#include "../GameEvents/RankEvent.h"
#include "../GameEvents/RequestEvent.h"
#include "../GameEvents/SetAcceptOrderEvent.h"
#include "../GameEvents/SetNightVisionEvent.h"
#include "../GameEvents/SetPrivateSquadEvent.h"
#include "../GameEvents/SetSquadLeaderEvent.h"
#include "../GameEvents/SpottedEvent.h"
#include "../GameEvents/StickyProjectileEvent.h"
#include "../GameEvents/StringBlockEvent.h"
#include "../GameEvents/SupplyDropEvent.h"
#include "../GameEvents/TargetDirectionEvent.h"
#include "../GameEvents/ToggleFreeCameraEvent.h"
#include "../GameEvents/UAVEvent.h"
#include "../GameEvents/UnlockEvent.h"
#include "../GameEvents/UpdateTriggerEvent.h"
#include "../GameEvents/VerifyPlayerTeamEvent.h"
#include "../GameEvents/VoipOnOffEvent.h"
#include "../GameEvents/VoipPlayerMuteEvent.h"
#include "../GameEvents/VoipSessionEvent.h"
#include "../GameLogic/VoiceMessages.h"
#include "../../Sound/BfSound.h"         // g_bfSound - the local soldier VO's quality test
#include "../../Core/Debug.h"            // the client chat display's unknown-type error
#if !DEDICATED
#include "../../Menu/BF2HudManager.h"    // the chat display and the radio speech localisation
#endif

#include "../Objects/Game/TriggerManager.h"
#include "../Objects/Game/TriggerableManager.h"
#include "../Utility/BuildNrUtil.h"          // getBF2Version's build stamp

namespace dice {
namespace hfe {

namespace world {
// Manager updates are direct, nonvirtual calls in both retail images.
extern IBase* scoreManager;
extern ResponsePhysicsManager* responsePhysicsManager;
extern TriggerManager* triggerManager;
extern TriggerableManager* triggerableManager;
}

extern IBase* squadManager;
#if !DEDICATED
extern hud::BF2HudManager* hudManager;   // BF2.exe ds:0xA10890 - Game/Main/BF2Engine.cpp
#endif

namespace {
struct RadioGLInfo
{
    std::string failMessage;
    std::string useMessage;
    std::string destroyedMessage;
    std::string repairedMessage;
    std::string spottedMessage;
};

world::IPlayerControlObject* findPlayerControlObject(world::IObject* object)
{
    for (; object; object = object->getParent())
    {
        world::IPlayerControlObject* pco =
            static_cast<world::IPlayerControlObject*>(
                object->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            return pco;
    }
    return 0;
}

}

Game::Game()
    : m_mapInfo()
    // m_currentMapSize is deliberately absent: neither retail constructor initializes it.
    , m_playerInputs()
    , m_lastDeltaTimeMark(g_system->getWallClockTime())
{
    registerGameEvents();
    m_currentChannel = static_cast<TargetChannel>(0);
    m_currentType = static_cast<ChannelMessageType>(1);
    m_currentFlags = 0;
    m_failedExitVehicleSoundId = 0;
    m_abortLoadingLevel = false;
}

Game::~Game()
{
}

bool Game::load()
{
    return false;
}

bool Game::loadPath(const std::string&)
{
    return false;
}

TargetChannel Game::getCurrentChannel()
{
    return m_currentChannel;
}

void Game::setCurrentChannel(TargetChannel channel)
{
    m_currentChannel = channel;
}

int Game::getCurrentFlags()
{
    // Retail anomaly in both ports: the getter aliases the channel while the setter writes flags.
    return static_cast<int>(m_currentChannel);
}

void Game::setCurrentFlags(int flags)
{
    m_currentFlags = flags;
}

ChannelMessageType Game::getCurrentType()
{
    return m_currentType;
}

void Game::setCurrentType(ChannelMessageType type)
{
    m_currentType = type;
}

MapInfo Game::getMapInfo() const
{
    return m_mapInfo;
}

// Plays a named message for the local client without network traffic. Radio messages
// use the local player's identity; game-info messages have no sender. Requires game
// logic for voice lookup even though the initial status test admits a null pointer.
bool Game::localRadioMessage(TargetChannel channel, const std::string& message)
{
    if (!gameLogic || gameLogic->getGameStatus() == GameStatus_Playing)
    {
        world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
        if (player)
        {
            int messageId;
            const std::string empty1;
            const std::string empty2;
            const bool found = static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(
                &messageId, 0, 0, message, empty2, empty1);
            if (found && !isDedicated())
            {
                const int sender = channel == TCPlayer ? player->getId() : -1;
                playbackRadioMessage(channel, static_cast<ChannelMessageType>(3),
                                     player->getTeam(), sender, messageId);
            }
        }
    }
    return true;
}

bool Game::localGameInfoMessage(TargetChannel channel, const std::string& message)
{
    if (!gameLogic || gameLogic->getGameStatus() == GameStatus_Playing)
    {
        world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
        if (player)
        {
            int messageId;
            const std::string empty1;
            const std::string empty2;
            const bool found = static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(
                &messageId, 0, 0, message, empty2, empty1);
            if (found && !isDedicated())
                playbackRadioMessage(channel, static_cast<ChannelMessageType>(0),
                                     player->getTeam(), -1, messageId);
        }
    }
    return true;
}

void Game::addPlayerInput(const io::PlayerInput& input)
{
    m_playerInputs.push_back(input);
}

int Game::getCurrentInputIndex()
{
    return 0;
}

world::IObject*
Game::getObjectFromNetworkableId(const unsigned short& networkableId) const
{
    return io::networkManager ? io::networkManager->getObject(networkableId) : 0;
}

void Game::createAndDestroyObject(const world::IObjectTemplate* objectTemplate)
{
    if (!objectTemplate)
        return;

    world::IObject* object = world::objectManager->createObject(objectTemplate, 0, 0);
    if (!object)
        return;

    io::INetworkable* networkable = object->getNetworkable();
    if (networkable)
    {
        networkable->m_isInNetwork = gameLogic->isServer();
        io::networkManager->addNetworkable(networkable);
    }

    destroyObject(object);
}

bool Game::destroyObject(world::IObject* object)
{
    if (!object)
        return false;

    world::objectManager->destroyObject(object);
    return true;
}

void Game::joinSquad(int squad)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player && squadManager)
        static_cast<ISquadManager*>(squadManager)->addToSquad(
            player->getTeam(), squad, player->getId());
}

void Game::leaveSquad(int squad)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player && squadManager)
        static_cast<ISquadManager*>(squadManager)->removeFromSquad(
            player->getTeam(), squad, player->getId(), false);
}

std::string Game::listSquadMembers(int team, int squad)
{
    std::string result;
    const std::list<world::IPlayer*>& players =
        world::playerManager->getPlayersInSquad(team, squad);
    for (std::list<world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it)
    {
        world::IPlayer* player = *it;
        result += formatString("id:%d ", player->getId());
        result += player->getName();
        result += "\n";
    }
    return result;
}

std::string Game::listSquads(int team)
{
    std::string result;
    if (squadManager)
    {
        for (int squadId = 0; squadId != 8; ++squadId)
        {
            ISquadInfo* squad =
                static_cast<ISquadManager*>(squadManager)->getSquadInfo(team, squadId);
            if (!squad)
                continue;

            result += formatString("id:%d ", squad->getSquadId());
            result += squad->getSquadName();
            result += squad->getIsPrivate() ? " is private." : " is public.";
            result += "\n";
        }
    }
    return result;
}

void Game::changeSquadName(const std::string& name, bool)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player || !squadManager)
        return;

    if (!static_cast<ISquadManager*>(squadManager)->setSquadName(
            player->getTeam(), player->getSquadId(), player->getId(), name, false))
        eventManager->postEvent(ECHud, 15, 0, 0.0f);
}

void Game::makeMeCommander()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player && squadManager)
        static_cast<ISquadManager*>(squadManager)->setCommander(
            player->getTeam(), player->getId());
}

void Game::popNextCommanderApplicant(int team)
{
    if (squadManager)
        static_cast<ISquadManager*>(squadManager)->popNextCommanderApplicant(team);
}

std::string Game::getSpottedMessage(unsigned short networkableId)
{
    world::IObject* object = io::networkManager->getObject(networkableId);
    std::string result;
    if (object)
    {
        world::ISoldier* soldier = static_cast<world::ISoldier*>(
            object->queryInterface(world::IID_ISoldier));

        if (!soldier)
        {
            world::IVehicleHudComp* vehicleHud = static_cast<world::IVehicleHudComp*>(
                world::object_detail::LayoutAccess::vehicleHudComponent(object));
            if (vehicleHud && vehicleHud->getHudInfo()->usePlayerIcon)
            {
                world::IPlayerControlObject* rootPco = findPlayerControlObject(object);
                if (rootPco)
                {
                    std::map<int, world::IPlayerControlObject*>* pcos = rootPco->getPcos();
                    for (std::map<int, world::IPlayerControlObject*>::iterator it = pcos->begin();
                         it != pcos->end(); ++it)
                    {
                        world::IPlayerControlObject* pco = it->second;
                        if (!pco)
                            continue;
                        world::IPlayer* player = pco->getOccupingPlayer();
                        if (!player || !player->getDefaultVehicle())
                            continue;

                        // The second getter call is real.  Stop at this first candidate even when
                        // its current default vehicle does not expose the soldier interface.
                        soldier = static_cast<world::ISoldier*>(
                            player->getDefaultVehicle()->queryInterface(world::IID_ISoldier));
                        break;
                    }
                }
            }
        }

        world::IObjectComponent* radio = 0;
        if (soldier)
        {
            world::Kit* kit = soldier->getKit();
            if (kit)
                radio = world::object_detail::LayoutAccess::radioGLComponent(
                    reinterpret_cast<world::IObject*>(kit));
        }
        else
        {
            radio = world::object_detail::LayoutAccess::radioGLComponent(object);
        }

        if (radio)
            result = static_cast<const RadioGLInfo*>(
                static_cast<world::IRadioGLComp*>(radio)->getGLInfo())->spottedMessage;
    }

    if (result.empty())
        result = "spotted";
    return result;
}

void Game::sendSpottedMessage(world::IPlayer* player,
                              unsigned short targetNetworkableId,
                              Vec2 position,
                              world::VehicleType vehicleType)
{
    if (!player)
        return;

    unsigned short spotterVehicleNetworkableId = 0;
    if (player->getDefaultVehicle())
    {
        io::INetworkable* networkable = player->getDefaultVehicle()->getNetworkable();
        if (networkable)
            spotterVehicleNetworkableId = networkable->m_networkableId;
    }

    const unsigned char team = static_cast<unsigned char>(player->getTeam());
    const bool server = gameLogic->isServer();

    if (server)
    {
        SpottedEvent* event = new SpottedEvent;
        event->m_spotterVehicleNetId = spotterVehicleNetworkableId;
        event->m_targetNetId = targetNetworkableId;
        event->m_team = team;
        event->m_position = position;
        event->m_vehicleType = vehicleType;
        event->m_hudEventId = 43;
        eventManager->postEventNextUpdate(ECGameLogic, 15, event);
    }
    else
    {
        SpottedEvent* event = new SpottedEvent;
        event->m_spotterVehicleNetId = spotterVehicleNetworkableId;
        event->m_targetNetId = targetNetworkableId;
        event->m_team = team;
        event->m_position = position;
        event->m_vehicleType = vehicleType;
        event->m_hudEventId = 43;
        game->sendGameEvent(static_cast<unsigned int>(-1), event);
    }
}

void Game::callForSpecialist(world::IPlayer* player, HudNodeTypes hudNodeType)
{
    if (!player)
        return;

    if (!player->getDefaultVehicle())
        return;

    io::INetworkable* networkable = player->getDefaultVehicle()->getNetworkable();
    if (!networkable)
        return;

    const unsigned short spotterVehicleNetworkableId = networkable->m_networkableId;
    const unsigned char team = static_cast<unsigned char>(player->getTeam());
    const bool server = gameLogic->isServer();

    if (server)
    {
        SpottedEvent* event = new SpottedEvent;
        event->m_spotterVehicleNetId = spotterVehicleNetworkableId;
        event->m_targetNetId = 0;
        event->m_team = team;
        event->m_position.x = 0.0f;
        event->m_position.y = 0.0f;
        event->m_vehicleType = static_cast<world::VehicleType>(26);
        event->m_hudEventId = static_cast<int>(hudNodeType);
        event->executeServer(player->getId());
        event->release();
    }
    else
    {
        SpottedEvent* event = new SpottedEvent;
        event->m_spotterVehicleNetId = spotterVehicleNetworkableId;
        event->m_targetNetId = 0;
        event->m_team = team;
        event->m_position.x = 0.0f;
        event->m_position.y = 0.0f;
        event->m_vehicleType = static_cast<world::VehicleType>(26);
        event->m_hudEventId = static_cast<int>(hudNodeType);
        sendGameEvent(static_cast<unsigned int>(-1), event);
    }

    TargetChannel channel;
    const char* message;
    bool finalFlag = false;
    switch (static_cast<int>(hudNodeType))
    {
    case 46:
        channel = static_cast<TargetChannel>(6);
        message = "medic";
        break;
    case 47:
        channel = static_cast<TargetChannel>(17);
        message = "need_ammo";
        break;
    case 48:
        channel = static_cast<TargetChannel>(17);
        message = "need_repair";
        break;
    case 49:
        channel = static_cast<TargetChannel>(15);
        message = "revive";
        break;
    case 50:
        channel = static_cast<TargetChannel>(16);
        message = "revive";
        finalFlag = true;
        break;
    default:
        return;
    }

    radioMessageFromPlayer(channel, message, player->getId(), 0,
                           !player->getIsAIPlayer(), finalFlag);
}

const std::string& Game::getCurrentMapName() const
{
    return m_mapInfo.getMapName();
}

const std::string& Game::getCurrentMapGameMode() const
{
    return m_mapInfo.getGameMode();
}

const std::string& Game::getCurrentMapLevelPath() const
{
    return m_mapInfo.getLevelPath();
}

int Game::getCurrentMapMaxPlayers() const
{
    return m_mapInfo.getMaxNumberOfPlayers();
}

int Game::getCurrentMapSize() const
{
    return static_cast<int>(m_currentMapSize);
}

void Game::setCurrentMapSize(unsigned int mapSize)
{
    m_currentMapSize = mapSize;
}

void Game::failedExitVehicle(int)
{
    // The dedicated-server implementation is intentionally empty. The shared field is consumed
    // only by the retail client, which lazily resolves and plays the failed-exit sound.
}

bool Game::getAbortLoadLevel() const
{
    return m_abortLoadingLevel;
}

// Selects the sender's voice/filter, plays nearby soldier speech or the team's radio
// sample, and displays text for eligible local listeners.
// [RETIRE-WHEN: radio-client-playback] The dedicated arm retains its chosen text substitute.
void Game::playbackRadioMessage(TargetChannel channel, ChannelMessageType type,
                                int team, int senderId, int messageId)
{
    VoiceMessages* const messages = static_cast<GameLogic*>(gameLogic)->getVoiceMessages();
    std::string voice;
    std::string filter;
    world::IPlayer* const sender = world::playerManager->getPlayerFromId(senderId);
    bool useDefaultFilter = false;
    bool senderIsSoldier = false;

    if (!sender)
    {
        voice = "commander";
        filter = "commander";
    }
    else
    {
        world::IObject* vehicle = sender->getVehicle();
        if (vehicle)
        {
            senderIsSoldier =
                vehicle->getTemplate()->getClassID() == world::CID_SoldierTemplate;
            world::IPlayerControlObject* const pco = findPlayerControlObject(vehicle);
            if (pco)
            {
                filter = pco->getSoundFilter();
                if (filter.empty())
                    useDefaultFilter = true;
            }
        }

        if (sender->getIsCommander())
        {
            voice = "commander";
            filter = "commander";
        }
        else if (sender->getIsSquadLeader())
            voice = "squadleader";
        else
            voice = "grunt";
    }

    const bool senderIsLocal = world::playerManager->getLocalHumanPlayer() == sender;
    if (senderIsLocal)
    {
        filter.clear();
        useDefaultFilter = false;
    }

    int voiceId;
    int filterId;
    const std::string empty;
    messages->namesToIds(0, &voiceId, &filterId, empty, voice, filter);

    int speechId = -1;
    if (world::playerManager->getLocalHumanPlayer() && sender && !senderIsLocal && senderIsSoldier)
    {
        if (g_bfSound->engine()->getQualityLevel() != 2)
        {
            world::IObject* const soldier = sender->getVehicle();
            Mat4 camera = Mat4::getIdentity();
            world::objectManager->getCameraInfo(&camera, 0, 0);
            const Vec3 cameraPosition = {camera.m[12], camera.m[13], camera.m[14]};
            speechId = messages->playSoldierVO(
                world::playerManager->getLocalHumanPlayer()->getTeam(), messageId, voiceId,
                cameraPosition, reinterpret_cast<world::Soldier*>(soldier));
        }
    }

    if (speechId < 0)
    {
        if (useDefaultFilter)
            filterId = messages->getDefaultFilter();
        speechId = messages->playRadioMessage(team, messageId, voiceId, filterId);
    }

    if (speechId >= 0 &&
        (!sender ||
         (world::playerManager->getLocalHumanPlayer() &&
          world::playerManager->getLocalHumanPlayer()->getTeam() == sender->getTeam())))
    {
#if !DEDICATED
        // BF2.exe 0x4441D0: getSpeech's string BY value into the HUD manager's +0x38
        // getLocalizedString, whose wide result goes straight to the wide outputChatString. The
        // speech texts are localisation keys, so this is what turns one into the line a player
        // reads. Not null-tested, as shipped.
        const std::wstring speech = hudManager->getLocalizedString(messages->getSpeech(speechId));
#else
        // [CHOSEN, not recovered] Both dedicated images load an absolute-zero global here (they
        // have no HUD manager) and call +0x3C (ELF) / +0x38 (PE) on it. makeWide stands in; a
        // dedicated server cannot pass the local-player guard in serverPlayerMessageEvent and does
        // not reach this tail through radio relay.
        const std::wstring speech = makeWide(messages->getSpeech(speechId));
#endif
        outputChatString(channel, type, senderId, -3, speech);
    }
}

// Parses exact, case-sensitive channel tokens. Unknown tokens retain TCGlobal
// without setting failbit; only the string extraction changes stream error state.
std::istream& operator>>(std::istream& stream, TargetChannel& channel)
{
    std::string token;
    stream >> token;
    channel = TCGlobal;
    if (token == "TCGlobal")                    channel = TCGlobal;
    else if (token == "TCTeam")                 channel = TCTeam;
    else if (token == "TCTeamAndLocalEnemies")  channel = TCTeamAndLocalEnemies;
    else if (token == "TCCommander")            channel = TCCommander;
    else if (token == "TCSquad")                channel = TCSquad;
    else if (token == "TCSquadId")              channel = TCSquadId;
    else if (token == "TCSquadLeader")          channel = TCSquadLeader;
    else if (token == "TCSquadAndLocalEnemies") channel = TCSquadAndLocalEnemies;
    else if (token == "TCLocal")                channel = TCLocal;
    else if (token == "TCLocalTeamMedic")       channel = TCLocalTeamMedic;
    else if (token == "TCLocalRevive")          channel = TCLocalRevive;
    else if (token == "TCAutoLocalRevive")      channel = TCAutoLocalRevive;
    else if (token == "TCLocalGroundVehicle")   channel = TCLocalGroundVehicle;
    else if (token == "TCLocalAirVehicle")      channel = TCLocalAirVehicle;
    else if (token == "TCLocalTeam")            channel = TCLocalTeam;
    else if (token == "TCVehicle")              channel = TCVehicle;
    else if (token == "TCServerMessage")        channel = TCServerMessage;
    else if (token == "TCServerTeamMessage")    channel = TCServerTeamMessage;
    else if (token == "TCPlayer")               channel = TCPlayer;
    return stream;
}

void Game::outputChatString(TargetChannel channel, ChannelMessageType type,
                            int playerId, int value, const std::string& text)
{
    const std::wstring wide = makeWide(text);
    outputChatString(channel, type, playerId, value, wide);
}

void Game::outputChatString(TargetChannel channel, ChannelMessageType type,
                            int playerId, int, const std::wstring& text)
{
    world::IPlayer* player = world::playerManager->getPlayerFromId(playerId);
    int team = 0;
    if (player)
        team = player->getTeam();

    switch (static_cast<int>(channel))
    {
    case 12:
    case 13:
        outputChatString(type, text, player, 0);
        break;

    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
        if (world::playerManager->getLocalHumanPlayer())
            outputChatString(type, text, player, team);
        break;

    default:
        break;
    }
}

void Game::outputChatString(ChannelMessageType, const std::string&,
                            world::IPlayer*, int)
{
    // Explicit five-byte no-op in the dedicated-server ELF (0x805CE20). It has NO caller in the
    // ELF, and the client's wide dispatcher (0x4424A0) calls only the wide form below, so the
    // client's own body for this overload was not located; nothing reaches it in either image.
}

// Routes chat to the client HUD; dedicated mode is inert. Requires a HUD manager.
// Squad messages can post a remote soldier marker before displaying nonempty text.
// Unknown message types are fatal.
void Game::outputChatString(ChannelMessageType type, const std::wstring& text,
                            world::IPlayer* player, int team)
{
#if DEDICATED
    // Explicit five-byte no-op in the dedicated-server ELF.
    (void)type;
    (void)text;
    (void)player;
    (void)team;
#else
    switch (static_cast<int>(type))
    {
    case 0:
    case 1:
        hudManager->addTeamChat(text, 0, 0);                                        // +0x120
        break;
    case 2:
        hudManager->addGlobalChat(text, player, team);                              // +0x11C
        break;
    case 3:
    {
        if (player && player != world::playerManager->getLocalHumanPlayer())        // +0x30
        {
            if (world::IObject* const soldier = player->getDefaultVehicle())       // PE +0xA0
            {
                if (io::INetworkable* const networkable = soldier->getNetworkable())   // PE +0xD4
                {
                    int networkableId = networkable->m_networkableId;              // +0x04
                    eventManager->postEvent(static_cast<EventCategory>(2), 60,
                                            &networkableId, 0.0f);                  // +0x10
                }
            }
        }
        std::wstring line;
        if (player)
            line = formatString(L"\xA7" L"0%S: \xA7" L"1", player->getName().c_str())
                   + text;
        else
            line = text;
        if (text.size() > 0)
            hudManager->addSquadChat(line, player, team);                           // +0x124
        break;
    }
    default:
        Debug(DEBUGTYPE_6, "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Game\\game.cpp",
              1061, "")
            << "Unknown Channel Message Type.";
        break;
    }
#endif
}

bool Game::playerControlsLaserGuidedWeapon(world::IPlayer* player)
{
    if (!player)
        return false;
    if (!findPlayerControlObject(player->getVehicle()))
        return false;
    if (!player->getVehicle())
        return false;
    if (!player->getVehicle()->getTemplate())
        return false;

    world::IPlayerControlObjectTemplate* pcoTemplate =
        static_cast<world::IPlayerControlObjectTemplate*>(
            player->getVehicle()->getTemplate()->queryInterface(
                world::IID_IPlayerControlObjectTemplate));
    return pcoTemplate && pcoTemplate->getVehicleCategory() == 2;
}

void Game::internal_exitPco(world::IObject* object)
{
    if (!object)
        return;

    world::IPlayerControlObject* pco = 0;
    world::ISoldier* soldier = static_cast<world::ISoldier*>(
        object->queryInterface(world::IID_ISoldier));
    if (soldier)
        pco = findPlayerControlObject(soldier->getUsedVehicle());
    if (!pco)
        pco = findPlayerControlObject(object);
    if (!pco)
        return;

    world::IPlayerControlObject* occupied =
        findPlayerControlObject(pco->getOccupingObject());
    if (occupied)
        occupied->onExit();
    pco->exit();
}

void Game::internal_exitPlayerPco(world::IObject* object)
{
    if (!object)
        return;

    world::IPlayerControlObject* pco = 0;
    world::ISoldier* soldier = static_cast<world::ISoldier*>(
        object->queryInterface(world::IID_ISoldier));
    if (soldier)
        pco = findPlayerControlObject(soldier->getUsedVehicle());
    if (!pco)
        pco = findPlayerControlObject(object);
    if (!pco)
        return;

    world::IPlayerControlObject* occupied =
        findPlayerControlObject(pco->getOccupingObject());
    if (occupied)
        occupied->onExit();
    pco->exitPlayer();
}

float Game::getDeltaTime()
{
    const double now = g_system->getWallClockTime();
    const float elapsed = static_cast<float>(now - m_lastDeltaTimeMark);
    m_lastDeltaTimeMark = now;
    return elapsed;
}

void Game::updateTriggerManager(float dt)
{
    if (world::triggerManager)
        world::triggerManager->handleUpdate(dt, 0);
}

void Game::updateTriggerableManager(float dt)
{
    if (world::triggerableManager)
        world::triggerableManager->handleUpdate(dt, 0);
}

void Game::updateWorldCollision(float dt)
{
    getStatsMonitor();
    world::objectManager->updateObjectsInGrid();
    world::responsePhysicsManager->reset();
    world::responsePhysicsManager->update(dt, 0);
}

void Game::registerStaticNetworkables()
{
    io::INetworkable* networkable =
        static_cast<io::INetworkable*>(
            world::scoreManager->queryInterface(io::IID_INetworkable));
    if (!networkable)
        return;

    networkable->init(0);
    networkable->m_basePriority = -1.0f;
    io::networkManager->addNetworkable(networkable, 0x0201);
}

void Game::unregisterStaticNetworkables()
{
    io::INetworkable* networkable =
        static_cast<io::INetworkable*>(
            world::scoreManager->queryInterface(io::IID_INetworkable));
    if (!networkable)
        return;

    io::networkManager->removeNetworkable(networkable->m_networkableId);
    networkable->m_networkableId = 0;
}

void Game::destroyAllPlayers()
{
    while (!world::playerManager->getPlayers().empty())
        destroyPlayer(world::playerManager->getPlayers().front());

    world::objectManager->destroyObjects();
}

namespace {

// THE ONE-shot guard - ds:0x8AAA660, a single byte. [INFERRED] name; the address carries no symbol.
bool g_gameEventsRegistered = false;

// Event makers require complete virtual bodies. Keep a withheld maker and its
// registration withheld together. [INFERRED] static names.
GameEventMakerMaker<ChallengeEvent>            g_challengeEventMaker;
GameEventMakerMaker<ChallengeResponseEvent>    g_challengeResponseEventMaker;
GameEventMakerMaker<ConnectionTypeEvent>       g_connectionTypeEventMaker;
GameEventMakerMaker<DataBlockEvent>            g_dataBlockEventMaker;
GameEventMakerMaker<CreatePlayerEvent>    g_createPlayerEventMaker;
GameEventMakerMaker<CreateObjectEvent>    g_createObjectEventMaker;
GameEventMakerMaker<CreateKitEvent>       g_createKitEventMaker;
GameEventMakerMaker<DestroyObjectEvent>        g_destroyObjectEventMaker;
GameEventMakerMaker<DestroyPlayerEvent>        g_destroyPlayerEventMaker;
GameEventMakerMaker<EnterVehicleEvent>         g_enterVehicleEventMaker;
GameEventMakerMaker<ExitVehicleEvent>          g_exitVehicleEventMaker;
GameEventMakerMaker<PostRemoteEvent>      g_postRemoteEventMaker;
GameEventMakerMaker<ChangePlayerNameEvent>     g_changePlayerNameEventMaker;
GameEventMakerMaker<HandlePickupEvent>         g_handlePickupEventMaker;
GameEventMakerMaker<HandleDropEvent>           g_handleDropEventMaker;
GameEventMakerMaker<StringBlockEvent>          g_stringBlockEventMaker;
GameEventMakerMaker<JoinSquadEvent>            g_joinSquadEventMaker;
GameEventMakerMaker<LeaveSquadEvent>           g_leaveSquadEventMaker;
GameEventMakerMaker<CommanderEvent>            g_commanderEventMaker;
GameEventMakerMaker<RadioMessageEvent>         g_radioMessageEventMaker;
GameEventMakerMaker<KilledByEvent>             g_killedByEventMaker;
GameEventMakerMaker<ChangeSquadNameEvent>      g_changeSquadNameEventMaker;
GameEventMakerMaker<SetPrivateSquadEvent>      g_setPrivateSquadEventMaker;
GameEventMakerMaker<IssueSquadOrderEvent>      g_issueSquadOrderEventMaker;
GameEventMakerMaker<InviteEvent>               g_inviteEventMaker;
GameEventMakerMaker<RankEvent>                 g_rankEventMaker;
GameEventMakerMaker<KickBanEvent>              g_kickBanEventMaker;
GameEventMakerMaker<SetAcceptOrderEvent>       g_setAcceptOrderEventMaker;
GameEventMakerMaker<SetSquadLeaderEvent>       g_setSquadLeaderEventMaker;
GameEventMakerMaker<SpottedEvent>              g_spottedEventMaker;
GameEventMakerMaker<ArtilleryEvent>            g_artilleryEventMaker;
GameEventMakerMaker<StickyProjectileEvent>     g_stickyProjectileEventMaker;
GameEventMakerMaker<AmbientEffectAreaEvent>    g_ambientEffectAreaEventMaker;
GameEventMakerMaker<VoipOnOffEvent>            g_voipOnOffEventMaker;
GameEventMakerMaker<CommanderCamEvent>         g_commanderCamEventMaker;
GameEventMakerMaker<SupplyDropEvent>           g_supplyDropEventMaker;
GameEventMakerMaker<VoipPlayerMuteEvent>       g_voipPlayerMuteEventMaker;
GameEventMakerMaker<VoteEvent>                 g_voteEventMaker;
GameEventMakerMaker<TargetDirectionEvent>      g_targetDirectionEventMaker;
GameEventMakerMaker<BeginRoundEvent>           g_beginRoundEventMaker;
GameEventMakerMaker<EndOfRoundEvent>           g_endOfRoundEventMaker;
GameEventMakerMaker<PythonCommandEvent>        g_pythonCommandEventMaker;
GameEventMakerMaker<RequestEvent>              g_requestEventMaker;
GameEventMakerMaker<VoipSessionEvent>          g_voipSessionEventMaker;
GameEventMakerMaker<ToggleFreeCameraEvent>     g_toggleFreeCameraEventMaker;
GameEventMakerMaker<MedalEvent>                g_medalEventMaker;
GameEventMakerMaker<UnlockEvent>               g_unlockEventMaker;
GameEventMakerMaker<MissileInitEvent>          g_missileInitEventMaker;
GameEventMakerMaker<UAVEvent>                  g_uavEventMaker;
GameEventMakerMaker<ContentCheckEvent>         g_contentCheckEventMaker;
GameEventMakerMaker<DropVehicleEvent>          g_dropVehicleEventMaker;
GameEventMakerMaker<CreateSpawnGroupEvent> g_createSpawnGroupEventMaker;
GameEventMakerMaker<RemoveSpawnGroupEvent> g_removeSpawnGroupEventMaker;
GameEventMakerMaker<UpdateTriggerEvent>        g_updateTriggerEventMaker;
GameEventMakerMaker<GrapplingHookContainerCreateEvent> g_grapplingHookContainerCreateEventMaker;
GameEventMakerMaker<GrapplingHookContainerUpdateEvent> g_grapplingHookContainerUpdateEventMaker;
GameEventMakerMaker<GrapplingHookContainerDetachEvent> g_grapplingHookContainerDetachEventMaker;
GameEventMakerMaker<GrapplingHookCreateEvent>  g_grapplingHookCreateEventMaker;
GameEventMakerMaker<GrapplingHookUpdateEvent>  g_grapplingHookUpdateEventMaker;
GameEventMakerMaker<PlayerTearGassedEvent>     g_playerTearGassedEventMaker;
GameEventMakerMaker<SetNightVisionEvent>       g_setNightVisionEventMaker;
GameEventMakerMaker<VerifyPlayerTeamEvent>     g_verifyPlayerTeamEventMaker;
GameEventMakerMaker<FixPlayerTeamEvent>        g_fixPlayerTeamEventMaker;

// Protocol event IDs, independent of registration order. Event implementations
// keep matching local constants; preserve those values when changing registration.
const unsigned int GEChallenge                        = 1;
const unsigned int GEChallengeResponse                = 2;
const unsigned int GEConnectionType                   = 3;
const unsigned int GEDataBlock                        = 4;
// Player, object, kit, remote-event and spawn-group wire IDs from .rodata.
const unsigned int GECreatePlayer                     = 5;
const unsigned int GECreateObject                     = 6;
const unsigned int GECreateKit                        = 31;
const unsigned int GEPostRemote                       = 11;
const unsigned int GECreateSpawnGroupEvent            = 57;
const unsigned int GERemoveSpawnGroupEvent            = 58;
const unsigned int GEDestroyObject                    = 7;
const unsigned int GEDestroyPlayer                    = 8;
const unsigned int GEEnterVehicle                     = 9;
const unsigned int GEExitVehicle                      = 10;
const unsigned int GEChangePlayerName                 = 12;
const unsigned int GEHandlePickup                     = 14;
const unsigned int GEHandleDrop                       = 13;
const unsigned int GEStringBlock                      = 15;
const unsigned int GEJoinSquad                        = 16;
const unsigned int GELeaveSquad                       = 17;
const unsigned int GECommander                        = 19;
const unsigned int GERadioMessage                           = 20;
const unsigned int GEKilledBy                         = 21;
const unsigned int GEChangeSquadName                  = 22;
const unsigned int GESetPrivateSquad                  = 23;
const unsigned int GEIssueSquadOrder                        = 24;
const unsigned int GEInvite                           = 25;
const unsigned int GERank                             = 26;
const unsigned int GEKickBan                          = 55;
const unsigned int GESetAcceptOrder                   = 27;
const unsigned int GESetSquadLeader                   = 28;
const unsigned int GESpotted                          = 29;
const unsigned int GEArtillery                        = 30;
const unsigned int GEStickyProjectile                       = 32;
const unsigned int GEAmbientEffectArea                = 34;
const unsigned int GEVoipOnOff                        = 35;
const unsigned int GECommanderCam                     = 36;
const unsigned int GESupplyDrop                       = 37;
const unsigned int GEVoipPlayerMuteEvent              = 38;
const unsigned int GEVote                             = 39;
const unsigned int GETargetDirection                  = 47;
const unsigned int GEBeginRound                       = 56;
const unsigned int GEEndOfRound                       = 48;
const unsigned int GEPythonCommand                          = 49;
const unsigned int GERequest                                = 50;
const unsigned int GEVoipSession                      = 54;
const unsigned int GEToggleFreeCamera                 = 40;
const unsigned int GEMedal                            = 41;
const unsigned int GEUnlock                           = 42;
const unsigned int GEMissileInit                            = 43;
const unsigned int GEUAV                              = 45;
const unsigned int GEContentCheck                     = 46;
const unsigned int GEDropVehicle                      = 51;
const unsigned int GEUpdateTrigger                    = 59;
const unsigned int GEGrapplingHookContainerCreateEvent      = 60;
const unsigned int GEGrapplingHookContainerUpdateEvent      = 61;
const unsigned int GEGrapplingHookContainerDetachEvent      = 62;
const unsigned int GEGrapplingHookCreateEvent               = 63;
const unsigned int GEGrapplingHookUpdateEvent               = 64;
const unsigned int GEPlayerTearGassedEvent            = 65;
const unsigned int GESetNightVisionEvent              = 66;
const unsigned int GEVerifyPlayerTeam                 = 68;
const unsigned int GEFixPlayerTeam                    = 69;

} // namespace

// Registers event names and factories in protocol order. Factories require their
// constructors and virtual bodies to be defined. The highest event ID (69) sets
// a seven-bit registry width. Registration returns true.
bool Game::registerGameEvents()
{
    if (g_gameEventsRegistered)                      // ds:0x8AAA660
        return true;
    g_gameEventsRegistered = true;

    gameEventRegistry->registerEventMaker(GEChallenge,           "ChallengeEvent",           &g_challengeEventMaker);
    gameEventRegistry->registerEventMaker(GEChallengeResponse,   "ChallengeResponseEvent",   &g_challengeResponseEventMaker);
    gameEventRegistry->registerEventMaker(GEConnectionType,      "ConnectionTypeEvent",      &g_connectionTypeEventMaker);
    gameEventRegistry->registerEventMaker(GEDataBlock,           "DataBlockEvent",           &g_dataBlockEventMaker);
    gameEventRegistry->registerEventMaker(GECreatePlayer, "CreatePlayerEvent", &g_createPlayerEventMaker);
    gameEventRegistry->registerEventMaker(GECreateObject, "CreateObjectEvent", &g_createObjectEventMaker);
    gameEventRegistry->registerEventMaker(GECreateKit, "CreateKitEvent", &g_createKitEventMaker);
    gameEventRegistry->registerEventMaker(GEDestroyObject,       "DestroyObjectEvent",       &g_destroyObjectEventMaker);
    gameEventRegistry->registerEventMaker(GEDestroyPlayer,       "DestroyPlayerEvent",       &g_destroyPlayerEventMaker);
    gameEventRegistry->registerEventMaker(GEEnterVehicle,        "EnterVehicleEvent",        &g_enterVehicleEventMaker);
    gameEventRegistry->registerEventMaker(GEExitVehicle,         "ExitVehicleEvent",         &g_exitVehicleEventMaker);
    gameEventRegistry->registerEventMaker(GEPostRemote, "PostRemoteEvent", &g_postRemoteEventMaker);
    gameEventRegistry->registerEventMaker(GEChangePlayerName,    "ChangePlayerNameEvent",    &g_changePlayerNameEventMaker);
    gameEventRegistry->registerEventMaker(GEHandlePickup,        "HandlePickupEvent",        &g_handlePickupEventMaker);
    gameEventRegistry->registerEventMaker(GEHandleDrop,          "HandleDropEvent",          &g_handleDropEventMaker);
    gameEventRegistry->registerEventMaker(GEStringBlock,         "StringBlockEvent",         &g_stringBlockEventMaker);
    gameEventRegistry->registerEventMaker(GEJoinSquad,           "JoinSquadEvent",           &g_joinSquadEventMaker);
    gameEventRegistry->registerEventMaker(GELeaveSquad,          "LeaveSquadEvent",          &g_leaveSquadEventMaker);
    gameEventRegistry->registerEventMaker(GECommander,           "CommanderEvent",           &g_commanderEventMaker);
    gameEventRegistry->registerEventMaker(GERadioMessage,        "RadioMessageEvent",                 &g_radioMessageEventMaker);
    gameEventRegistry->registerEventMaker(GEKilledBy,            "KilledByEvent",           &g_killedByEventMaker);
    gameEventRegistry->registerEventMaker(GEChangeSquadName,     "ChangeSquadNameEvent",     &g_changeSquadNameEventMaker);
    gameEventRegistry->registerEventMaker(GESetPrivateSquad,     "SetPrivateSquadEvent",     &g_setPrivateSquadEventMaker);
    gameEventRegistry->registerEventMaker(GEIssueSquadOrder,     "IssueSquadOrderEvent",              &g_issueSquadOrderEventMaker);
    gameEventRegistry->registerEventMaker(GEInvite,              "InviteEvent",              &g_inviteEventMaker);
    gameEventRegistry->registerEventMaker(GERank,                "RankEvent",               &g_rankEventMaker);
    gameEventRegistry->registerEventMaker(GEKickBan,             "KickBanEvent",             &g_kickBanEventMaker);
    gameEventRegistry->registerEventMaker(GESetAcceptOrder,      "SetAcceptOrderEvent",      &g_setAcceptOrderEventMaker);
    gameEventRegistry->registerEventMaker(GESetSquadLeader,      "SetSquadLeaderEvent",      &g_setSquadLeaderEventMaker);
    gameEventRegistry->registerEventMaker(GESpotted,             "SpottedEvent",            &g_spottedEventMaker);
    gameEventRegistry->registerEventMaker(GEArtillery,           "ArtilleryEvent",          &g_artilleryEventMaker);
    gameEventRegistry->registerEventMaker(GEStickyProjectile,    "StickyProjectileEvent",             &g_stickyProjectileEventMaker);
    gameEventRegistry->registerEventMaker(GEAmbientEffectArea,   "AmbientEffectAreaEvent",  &g_ambientEffectAreaEventMaker);
    gameEventRegistry->registerEventMaker(GEVoipOnOff,           "VoipOnOffEvent",           &g_voipOnOffEventMaker);
    gameEventRegistry->registerEventMaker(GECommanderCam,        "CommanderCamEvent",        &g_commanderCamEventMaker);
    gameEventRegistry->registerEventMaker(GESupplyDrop,          "SupplyDropEvent",          &g_supplyDropEventMaker);
    gameEventRegistry->registerEventMaker(GEVoipPlayerMuteEvent, "VoipPlayerMuteEvent",      &g_voipPlayerMuteEventMaker);
    gameEventRegistry->registerEventMaker(GEVote,                "VoteEvent",                &g_voteEventMaker);
    gameEventRegistry->registerEventMaker(GETargetDirection,     "TargetDirectionEvent",    &g_targetDirectionEventMaker);
    gameEventRegistry->registerEventMaker(GEBeginRound,          "BeginRoundEvent",          &g_beginRoundEventMaker);
    gameEventRegistry->registerEventMaker(GEEndOfRound,          "EndOfRoundEvent",          &g_endOfRoundEventMaker);
    gameEventRegistry->registerEventMaker(GEPythonCommand,       "PythonCommandEvent",                &g_pythonCommandEventMaker);
    gameEventRegistry->registerEventMaker(GERequest,             "RequestEvent",                      &g_requestEventMaker);
    gameEventRegistry->registerEventMaker(GEVoipSession,         "VoipSessionEvent",         &g_voipSessionEventMaker);
    gameEventRegistry->registerEventMaker(GEToggleFreeCamera,    "ToggleFreeCameraEvent",    &g_toggleFreeCameraEventMaker);
    gameEventRegistry->registerEventMaker(GEMedal,               "MedalEvent",               &g_medalEventMaker);
    gameEventRegistry->registerEventMaker(GEUnlock,              "UnlockEvent",             &g_unlockEventMaker);
    gameEventRegistry->registerEventMaker(GEMissileInit,         "MissileInitEvent",                  &g_missileInitEventMaker);
    gameEventRegistry->registerEventMaker(GEUAV,                 "UAVEvent",                 &g_uavEventMaker);
    gameEventRegistry->registerEventMaker(GEContentCheck,        "ContentCheckEvent",        &g_contentCheckEventMaker);
    gameEventRegistry->registerEventMaker(GEDropVehicle,         "DropVehicleEvent",         &g_dropVehicleEventMaker);
    gameEventRegistry->registerEventMaker(GECreateSpawnGroupEvent, "CreateSpawnGroupEvent", &g_createSpawnGroupEventMaker);
    gameEventRegistry->registerEventMaker(GERemoveSpawnGroupEvent, "RemoveSpawnGroupEvent", &g_removeSpawnGroupEventMaker);
    gameEventRegistry->registerEventMaker(GEUpdateTrigger,       "UpdateTriggerEvent",       &g_updateTriggerEventMaker);
    gameEventRegistry->registerEventMaker(GEGrapplingHookContainerCreateEvent, "GrapplingHookContainerCreateEvent", &g_grapplingHookContainerCreateEventMaker);
    gameEventRegistry->registerEventMaker(GEGrapplingHookContainerUpdateEvent, "GrapplingHookContainerUpdateEvent", &g_grapplingHookContainerUpdateEventMaker);
    gameEventRegistry->registerEventMaker(GEGrapplingHookContainerDetachEvent, "GrapplingHookContainerDetachEvent", &g_grapplingHookContainerDetachEventMaker);
    gameEventRegistry->registerEventMaker(GEGrapplingHookCreateEvent,          "GrapplingHookCreateEvent",          &g_grapplingHookCreateEventMaker);
    gameEventRegistry->registerEventMaker(GEGrapplingHookUpdateEvent,          "GrapplingHookUpdateEvent",          &g_grapplingHookUpdateEventMaker);
    gameEventRegistry->registerEventMaker(GEPlayerTearGassedEvent, "PlayerTearGassedEvent",  &g_playerTearGassedEventMaker);
    gameEventRegistry->registerEventMaker(GESetNightVisionEvent, "SetNightVisionEvent",      &g_setNightVisionEventMaker);
    gameEventRegistry->registerEventMaker(GEVerifyPlayerTeam,    "VerifyPlayerTeamEvent",    &g_verifyPlayerTeamEventMaker);
    gameEventRegistry->registerEventMaker(GEFixPlayerTeam,       "FixPlayerTeamEvent",       &g_fixPlayerTeamEventMaker);

    // BF2.exe 0x443CAA..0x4441BA, DED PE 0x435812..0x435D28,
    // ELF 0x805A01A..0x805AAAC: notification names follow the network makers
    // in the same order under this function's one-time guard in every image
    // (DED PE 0x434A79 / ELF 0x8057AFC), before GameClient/GameServer construction.
    eventManager->registerEvent(ECNetwork, 1, "ECNetworkNEDataBlockReady");
    eventManager->registerEvent(ECNetwork, 11, "ECNetworkNEStringReceived");
    eventManager->registerEvent(ECNetwork, 2, "ECNetworkNELoadComplete");
    eventManager->registerEvent(ECNetwork, 3, "ECNetworkNEStartSimulation");
    eventManager->registerEvent(ECNetwork, 4, "ECNetworkNEDatabaseComplete");
    eventManager->registerEvent(ECNetwork, 5, "ECNetworkNEReset");
    eventManager->registerEvent(ECNetwork, 6, "ECNetworkNESelectSpawnGroup");
    eventManager->registerEvent(ECNetwork, 7, "ECNetworkNESelectTeam");
    eventManager->registerEvent(ECNetwork, 8, "ECNetworkNESelectKit");
    eventManager->registerEvent(ECNetwork, 9, "ECNetworkNEPlayerSpawned");
    eventManager->registerEvent(ECNetwork, 10, "ECNetworkNEPlayerDead");
    eventManager->registerEvent(ECNetwork, 12, "ECNetworkNESuicide");
    eventManager->registerEvent(ECNetwork, 13, "ECNetworkNERadioMessageReceived");
    eventManager->registerEvent(ECNetwork, 14, "ECNetworkNETraceComplete");
    eventManager->registerEvent(ECNetwork, 15, "ECNetworkNETraceFailed");
    eventManager->registerEvent(ECNetwork, 16, "ECNetworkNETraceResourceConnect");
    eventManager->registerEvent(ECNetwork, 17, "ECNetworkNENetworkableDestroyed");
    eventManager->registerEvent(ECNetwork, 18, "ECNetworkNERemoteConsoleCommand");
    eventManager->registerEvent(ECNetwork, 19, "ECNetworkNERemoteConsoleFeedback");
    eventManager->registerEvent(ECNetwork, 20, "ECNetworkNEEndOfRoundGuard");
    eventManager->registerEvent(ECGame, 1, "ECGameGNDestroyObject");
    eventManager->registerEvent(ECGame, 2, "ECGameGNDestroyPlayer");
    eventManager->registerEvent(ECGame, 3, "ECGameGNCreateObject");
    eventManager->registerEvent(ECGame, 4, "ECGameGNCreatePlayer");
    eventManager->registerEvent(ECGame, 5, "ECGameGNImmediateCreatePlayer");
    eventManager->registerEvent(ECGame, 6, "ECGameGNPlayerSpawned");
    eventManager->registerEvent(ECGame, 7, "ECGameGNSelectSpawnGroup");
    eventManager->registerEvent(ECGame, 8, "ECGameGNPlayerDead");
    eventManager->registerEvent(ECGame, 9, "ECGameGNEnterVehicle");
    eventManager->registerEvent(ECGame, 11, "ECGameGNExitVehicle");
    eventManager->registerEvent(ECGame, 10, "ECGameGNFreeCameraFire");

    return true;
}

// Formats the cached build stamp into the version string.
std::string getBF2Version()
{
    return formatString("BF2 build %s", BuildNrUtil::getBuildNrString());
}

#if !DEDICATED
// Client runtime dedicated flag starts false and can be set by +dedicated.
// Dedicated images use a compile-time true value.
namespace {
    bool s_dedicated = false;               // byte_9A4381   [INFERRED] name
}

bool isDedicated()
{
    return s_dedicated;
}

void setDedicated(bool dedicated)
{
    s_dedicated = dedicated;
}

// The lightmap-rendering-node flag - byte_9A4382, the next byte; see Game.h.
namespace {
    bool s_lightmapRenderingNode = false;   // byte_9A4382   [INFERRED] name
}

bool isLightmapRenderingNode()
{
    return s_lightmapRenderingNode;
}

void setLightmapRenderingNode(bool node)
{
    s_lightmapRenderingNode = node;
}
#endif // !dedicated

} // namespace hfe
} // namespace dice
