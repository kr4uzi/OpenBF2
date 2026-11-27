// dice::hfe::CommanderManager - construction, copy, teardown, commander election, and commander
// camera lookup/spawn. See CommanderManager.h for the layout.
// path: [INFERRED] filename in the attested SquadManager/ directory.

#include "CommanderManager.h"

#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

#include "Team.h"                                    // getTeamId, for the CommanderEvent
#include "Squad.h"                                   // removeFromSquad in the assignment arm
#include "../../IGame.h"                             // game - sendGameEventToAllConnected (+0x94)
#include "../../IGameLogic.h"                        // gameLogic - isServer / spawnObject
#include "../../GameEvents/CommanderEvent.h"
#include "../../Objects/Game/OverheadCamera.h"
#include "../../Objects/Game/CommanderToolsManager.h" // assignment's drop-vehicle percentage
#include "../../Objects/IObject.h"
#include "../../Objects/IObjectTemplate.h"
#include "../../Objects/IPlayer.h"                   // setIsCommander (+0x110)
#include "../../Python/PythonHost.h"                 // commander-change script event 12
#include "../../Python/PyRef.h"
#include "../../../Core/Ids.h"                       // world::CID_OverheadCameraTemplate
#include "../../../Core/System.h"                    // g_system->getExactTime
#include "../../../Math/Vec3.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/PlayerManager.h"     // getPlayerFromId (+0x20)
#include "../../../Scene/Object/PlayerScoreData.h"   // m_rank - popNextCommanderApplicant's pick
#include "../../../Core/EventManager.h"              // eventManager->postEvent - decline/resign
#include "../../Common/GameEvent.h"                  // PostRemoteEvent - the ECHud 35 / 37 sends
#include "SquadManager.h"                            // m_commanderSelectDelay - resignCommander's election delay
#include "ISquadManager.h"                           // squadManager - ds:0x8ACBFA4
#include "../../../AI/AIInterface/AIInstance.h"
#include "../../../AI/AIInterface/IAIEventCourier.h"
#include "../../../AI/AIInterface/SingletonServer.h"

namespace dice {
namespace hfe {
extern IBase* squadManager;                          // ds:0x8ACBFA4 - Core/Globals.cpp (resignCommander's delay)

// Initializes both commander IDs to -1, since player ID 0 is valid.
CommanderManager::CommanderManager()
    : m_commander(-1)                                   // +0x04
    , m_proposedCommander(-1)                           // +0x08
    , m_commanderCam()                                  // +0x18
{
}

// Copies election state and retains the camera's weak control block so temporary
// Team copies cannot release shared camera state prematurely.
CommanderManager::CommanderManager(const CommanderManager& other)
    : m_commander(other.m_commander)                    // +0x04
    , m_proposedCommander(other.m_proposedCommander)    // +0x08
    , m_applicants(other.m_applicants)                  // +0x0C - a real tree copy
    , m_commanderCam(other.m_commanderCam)              // +0x18
{
}

// Destroys the commander camera through the game, clears its weak handle, and
// releases the applicant list. The repeated weak-handle clear is harmless.
CommanderManager::~CommanderManager()
{
    world::IObject* const camera = m_commanderCam.get();
    if (camera)
        game->destroyObject(camera);

    // Assignment releases the live control block. The compiler-emitted member destructor is the
    // second release sequence seen in the binary, and is a no-op because this is now null.
    m_commanderCam = WeakPtr<world::IObject>();
}

// The two trivial accessors - ELF 0x811BAD0 / 0x811BAE0, 11 bytes each.
int CommanderManager::getCommander() const         { return m_commander; }          // +0x04
int CommanderManager::getProposedCommander() const { return m_proposedCommander; }  // +0x08

// Clears applicants and resets commander/proposal IDs to -1.
void CommanderManager::reset()
{
    m_commander         = -1;                           // +0x04
    m_proposedCommander = -1;                           // +0x08
    m_applicants.clear();                               // +0x0C
}

// Requests a camera before rate-limiting an existing player. Missing players are
// still admitted. With no active proposal, later applicants retain the original
// unused team-payload allocation.
void CommanderManager::applyForCommander(int playerId, Team* team)
{
    const int teamId = team->getTeamId();
    spawnCommanderCam(teamId);

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (player)
    {
        const double elapsedMinutes =
            std::fabs(g_system->getExactTime() - player->getLastTimeCommander()) / 60.0;
        if (player->getCommanderReapplyWaitTime() > elapsedMinutes)
            return;
    }

    // BF2.exe 0x4E7633..0x4E764F / ELF applyForCommander export 0x0811B270.
    if (ai::g_aiInstance && ai::aiInstance()->isInitialized())
        ai::BFSingletonServer::m_iaiEventCourier->event_appliedForCommander(playerId);

    if (m_proposedCommander < 0)
    {
        int* const data = new int[1];                                 // may leak; see above
        *data = teamId;
        if (m_applicants.empty())
        {
            SquadManager* const manager =
                static_cast<SquadManager*>(static_cast<ISquadManager*>(squadManager));
            eventManager->postEvent(ECGameLogic, 7, data,
                                    manager->m_firstCommanderSelectDelay);
        }
    }

    m_applicants.insert(playerId);
}

// Assign a commander, or stand down with playerId -1. Reassigning the current
// commander succeeds; replacing another commander or selecting a missing player fails.
// Stand-down clears the player's flag before the stored id and always broadcasts.
// Assignment removes squad membership and notifies applicants before clearing them.
// Python receives (team truth value, old player, new player).
bool CommanderManager::setCommander(int playerId, Team* team)
{
    return setCommander(-1, team, playerId);
}

bool CommanderManager::setCommander(int, Team* team, int playerId)
{
    if (playerId == -1)
    {
        // the stand-down arm
        world::IPlayer* const old = world::playerManager->getPlayerFromId(m_commander);   // +0x20
        if (old)
            old->setIsCommander(false);                                                   // +0x110

        m_commander = -1;

        // unconditional, even with nobody standing down - see the note above.
        // ID 3 BY number. CommanderEvent.h recovers only two of the nine CommanderEventTypes
        // names (CESoldierView = 6, CEOverheadView = 7) and deliberately does not invent the rest.
        game->sendGameEventToAllConnected(
            new CommanderEvent(static_cast<CommanderEvent::CommanderEventTypes>(3),
                               team->getTeamId(), -1));                               // IGame +0x94

        // BF2.exe 0x4E8743..0x4E8777 / ELF setCommander export 0x0811B570.
        if (ai::g_aiInstance && ai::aiInstance()->isInitialized())
            ai::BFSingletonServer::m_iaiEventCourier->event_resignAsCommander(
                playerId, team->getTeamId());

        if (old)
        {
            world::IPlayer* const noNewCommander = 0;
            python::PyRef args = python::buildArgs(team, old, noNewCommander);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(12), args.get());
        }
        return true;
    }

    // the assign arm
    if (playerId == m_commander)
        return true;
    if (m_commander != -1)
        return false;

    // Shipped, despite the proven -1 id at this point. Preserve the virtual call and guard.
    world::IPlayer* const old = world::playerManager->getPlayerFromId(m_commander);
    if (old)
        old->setIsCommander(false);

    world::IPlayer* const next = world::playerManager->getPlayerFromId(playerId);
    if (!next)
        return false;

    Squad* const squad = team->getSquad(next->getSquadId());
    if (squad)
        squad->removeFromSquad(playerId, false, false);

    next->setIsCommander(true);
    m_commander = playerId;

    game->sendGameEventToAllConnected(
        new CommanderEvent(static_cast<CommanderEvent::CommanderEventTypes>(3),
                           team->getTeamId(), playerId));

    struct CommanderAssignedPayload
    {
        int playerId;
        float remainingDropVehicleFraction;
    } payload;
    payload.playerId = playerId;
    payload.remainingDropVehicleFraction =
        world::commanderToolsManager->getPercentOfDropVehicles(next->getTeam());
    game->sendGameEventToAllConnected(
        new PostRemoteEvent(ECHud, 33, true, &payload, 8, 0.0f));

    // BF2.exe 0x4E8927..0x4E895B / ELF setCommander export 0x0811B570:
    // stop the AI commander's policy while a player commands this team.
    if (ai::g_aiInstance && ai::aiInstance()->isInitialized())
        ai::BFSingletonServer::m_iaiEventCourier->event_acceptedCommander(
            playerId, team->getTeamId());

    for (std::set<int>::iterator it = m_applicants.begin(); it != m_applicants.end(); ++it)
    {
        world::IPlayer* const applicant = world::playerManager->getPlayerFromId(*it);
        if (!applicant || applicant == next)
            continue;

        if (applicant->getId() == 255)
        {
            eventManager->postEvent(ECHud, 34, 0, 0.0f);
        }
        else
        {
            game->sendGameEventIfConnected(
                applicant->getId(),
                new PostRemoteEvent(ECHud, 34, false, 0, 0, 0.0f));
        }
    }

    {
        python::PyRef args = python::buildArgs(team, old, next);
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(12), args.get());
    }

    m_applicants.clear();
    m_proposedCommander = -1;
    return true;
}

// Removes the applicant and returns whether they were in the set.
bool CommanderManager::removeCommanderApplicant(int playerId)
{
    const bool wasApplicant = m_applicants.find(playerId) != m_applicants.end();
    m_applicants.erase(playerId);
    return wasApplicant;
}

// Proposes and removes the highest-ranked valid applicant, or returns -1. A
// nonempty set clears the prior proposal before searching; unresolved players stay
// in the set. Preserve the repeated score-data reads.
int CommanderManager::popNextCommanderApplicant()
{
    if (m_applicants.empty())                                    // `if (!dword10)` - the node count
        return -1;

    m_proposedCommander = -1;
    std::set<int>::iterator best = m_applicants.end();
    int bestRank = -2;
    for (std::set<int>::iterator it = m_applicants.begin(); it != m_applicants.end(); ++it)
    {
        world::IPlayer* const player = world::playerManager->getPlayerFromId(*it);   // +0x20
        if (player && player->getScoreData()                                           // +0x174
            && player->getScoreData()->m_rank > bestRank)
        {
            bestRank = player->getScoreData()->m_rank;                                 // +0x20
            best = it;
        }
    }
    if (best != m_applicants.end())
    {
        m_proposedCommander = *best;
        m_applicants.erase(best);                                // _Rb_tree_rebalance_for_erase + free
    }
    return m_proposedCommander;
}

// acceptCommander - ELF 0x811BAF0, 45 B. Only the currently proposed player may take the chair;
// the long setCommander result is deliberately ignored.
void CommanderManager::acceptCommander(int playerId, Team* team)
{
    if (playerId == m_proposedCommander)
        setCommander(playerId, team);
}

// Replaces a declined proposal and notifies the next applicant locally for ID 255
// or remotely otherwise. Player lookup precedes the -1 test; absent players are
// not rejected by the team check.
void CommanderManager::declineCommander(int playerId, Team* team)
{
    if (playerId == m_proposedCommander)
        m_proposedCommander = -1;

    int next = popNextCommanderApplicant();
    world::IPlayer* const player = world::playerManager->getPlayerFromId(next);   // +0x20
    if (next == -1)
        return;
    if (player && player->getTeam() != team->getTeamId())                        // +0xE8
        return;

    if (next == 255)
        eventManager->postEvent(ECHud, 35, &next, 0.0f);                          // +0x14
    else
        game->sendGameEventIfConnected(next,                                       // IGame +0x98
            new PostRemoteEvent(ECHud, 35, false, &next, 4, 0.0f));
}

// Removes the commander and notifies clients. With applicants, schedules the next
// election using a heap payload consumed by ServerGameLogic. The unconditional
// payload allocation leaks when there are no applicants, matching original behavior.
void CommanderManager::resignCommander(Team* team)
{
    int oldCommander = team->getCommander();
    const int teamId = team->getTeamId();

    setCommander(-1, team);

    game->sendGameEventToAllConnected(                                            // IGame +0x94
        new PostRemoteEvent(ECHud, 37, true, &oldCommander, 4, 0.0f));

    int* const data = new int[1];                                                 // leaks - see above
    *data = teamId;
    if (!m_applicants.empty())                                                    // `[esi+10h]`
    {
        SquadManager* const manager =
            static_cast<SquadManager*>(static_cast<ISquadManager*>(squadManager));   // ds:0x8ACBFA4
        eventManager->postEvent(ECGameLogic, 7, data, manager->m_commanderSelectDelay);   // +0x14 / +0x18
    }
}

// spawnCommanderCam - ELF 0x811BD70, 76 B; PE's inlined copy in applyForCommander at 0x45F971.
// Server-only, and deliberately no squadManager null guard. The SquadManager call is responsible
// for creating/locating the camera and populating this manager's weak handle through getCommanderCam.
void CommanderManager::spawnCommanderCam(int teamId)
{
    if (gameLogic && gameLogic->isServer() && !m_commanderCam.get())
        static_cast<ISquadManager*>(squadManager)->getCommanderCam(teamId);
}

namespace {

void commanderCameraInvariantFailure()
{
#if defined(_MSC_VER)
    // [PLATFORM] PE 0x45D9BB executes int 3; ELF 0x811BEAD calls abort.
    __debugbreak();
#else
    std::abort();
#endif
}

} // anonymous namespace

// Server creates and weakly retains the team's camera, requiring cache ID zero.
// Client finds it by template class in registry-key order.
world::IObject* CommanderManager::getCommanderCam(Team* team)
{
    world::IObject* camera = m_commanderCam.get();
    if (camera)
        return camera;

    // Shipped code directly dereferences gameLogic here; only spawnCommanderCam guards it.
    if (gameLogic->isServer())
    {
        world::IObjectTemplate* const cameraTemplate =
            world::objectTemplateManager->getTemplate(std::string("MultiPlayerOverheadCamera"));
        if (!cameraTemplate)
            return 0;

        camera = gameLogic->spawnObject(cameraTemplate, Math::Vec3::getZero(),
                                        Math::Vec3::getZero(), team->getTeamId());
        if (!camera)
            return 0;

        if (camera->getObjectCacheID() != 0)
            commanderCameraInvariantFailure();

        m_commanderCam = camera->getWeakPtr();
        static_cast<world::OverheadCamera*>(camera)->setTeam(team->getTeamId());
        return camera;
    }

    const std::map<unsigned int, world::IObject*>& objects =
        world::objectManager->getAllRegisteredObjects();
    for (std::map<unsigned int, world::IObject*>::const_iterator it = objects.begin();
         it != objects.end(); ++it)
    {
        world::IObject* const object = it->second;
        if (object->getTemplate()->getClassID() == world::CID_OverheadCameraTemplate)
        {
            world::OverheadCamera* const overhead = static_cast<world::OverheadCamera*>(object);
            if (overhead->getTeam() == team->getTeamId())
            {
                m_commanderCam = object->getWeakPtr();
                return object;
            }
        }
    }
    return 0;
}

} // namespace hfe
} // namespace dice
