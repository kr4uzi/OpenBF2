// Server voting and vote-result application.
#include "ServerVoteManager.h"

#include <cmath>                                   // ceil - calcVotesNeeded

#include "../Common/VoteEvent.h"
#include "../Main/Game.h"
#if !DEDICATED
#include "../Common/ClientVoteManager.h"
#include "../GameServerSettings.h"
#include "../../Core/ClassManager.h"
#endif
#include "../IGame.h"                              // game - sendGameEvent* / queryInterface
#include "../IGameServer.h"                        // kickPlayer / getConnectionState / getPlayerConnectionIp
#include "../IGameLogic.h"                         // gameLogic - setNextLevel / runNextLevel
#include "../MapList.h"                            // mapList - getEntry / getCurrentMap
#include "../Objects/IPlayer.h"
#include "../../Scene/Object/PlayerManager.h"      // world::playerManager
#include "../../Core/System.h"                     // g_system->getExactTime()
#include "../../Core/Ids.h"                        // IID_IGameServer
#include "../../Core/IBase.h"
#include "ServerSettings.h"                        // serverSettings - the vote settings
#include "BanList.h"                               // consoleAddAddressToBanList on a passed kick
#include "../GameLogic/SquadManager/ISquadManager.h" // typed resignCommander dispatch

namespace dice {
namespace hfe {

#if !DEDICATED
extern ClassManager* classManager;
#endif
extern IBase* squadManager;                        // ds:0x8ACBFA4 - Core/Globals.cpp (IBase*, no layout)

namespace {
inline ISquadManager* squadManagerIface()
{
    return static_cast<ISquadManager*>(squadManager);
}

// The idle-tick service's one literal: flt_888FA04 = 5.0f - the delay between a map vote passing
// and the level change (update), and the commander reapply wait a mutiny imposes (0x40140000 =
// 5.0 as a double, handlePrematureVoteEnd / updateVote).
const float kMapChangeDelay = 5.0f;
const double kCommanderReapplyWait = 5.0;
} // namespace

ServerVoteManager* ServerVoteManager::_instance = 0;   // ds:0x8AACAC0

ServerVoteManager* ServerVoteManager::getInstance()
{
    if (!_instance)
        _instance = new ServerVoteManager();
    return _instance;
}

// : the inlined implicit dtor (list, the seven Votes in reverse) and delete.
void ServerVoteManager::release()
{
    if (_instance)
    {
        delete _instance;
        _instance = 0;
    }
}

// : seven Vote ctors (this, three from +0x58, three from +0x130), the list header,
// then the type stamps and the two map-change fields. m_mapChangeRequestTime (+0x50) is NOT stored
// it is written together with the pending flag before it is ever read.
ServerVoteManager::ServerVoteManager()
    : m_mapChangePending(false)                    // +0x048
    , m_nextMapIndex(0)                            // +0x04C
{
    m_mutinyVotes[1].m_type = Vote::VTMutiny;      // +0x1B4
    m_mapVote.m_type        = Vote::VTMap;         // +0x03C (re-stored 1)
    m_kickVotes[1].m_type   = Vote::VTKickPlayer;  // +0x0DC
    m_kickVotes[2].m_type   = Vote::VTKickPlayer;  // +0x124
    m_mutinyVotes[2].m_type = Vote::VTMutiny;      // +0x1FC
}

// : the five live votes' running flag and end time, and the pending map change.
void ServerVoteManager::reset()
{
    m_kickVotes[1].m_inProgress = false;   m_kickVotes[1].m_endTime = 0.0f;
    m_kickVotes[2].m_inProgress = false;   m_kickVotes[2].m_endTime = 0.0f;
    m_mutinyVotes[1].m_inProgress = false; m_mutinyVotes[1].m_endTime = 0.0f;
    m_mutinyVotes[2].m_inProgress = false; m_mutinyVotes[2].m_endTime = 0.0f;
    m_mapVote.m_inProgress = false;        m_mapVote.m_endTime = 0.0f;
    m_mapChangePending = false;
    m_nextMapIndex = 0;
}

// : who could vote on this issue - never AI players, only players who participate
// in voting; a kick vote also excludes the target and (team-vote-only) other teams; a mutiny vote
// excludes the target and is always the team's own.
int ServerVoteManager::getNumberOfPotentialVoters(Vote::VoteType type, int team, int item)
{
    int count = 0;
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();   // +0x3C
    std::list<world::IPlayer*>::const_iterator it;
    switch (type)
    {
    case Vote::VTKickPlayer:
        for (it = players.begin(); it != players.end(); ++it)
        {
            world::IPlayer* p = *it;
            if (!p->getIsAIPlayer() && p->getParticipateInVoting() && p->getId() != item
                && (!getTeamVoteOnly() || p->getTeam() == team))
                ++count;
        }
        break;
    case Vote::VTMutiny:
        for (it = players.begin(); it != players.end(); ++it)
        {
            world::IPlayer* p = *it;
            if (!p->getIsAIPlayer() && p->getParticipateInVoting() && p->getId() != item
                && p->getTeam() == team)
                ++count;
        }
        break;
    case Vote::VTMap:
        for (it = players.begin(); it != players.end(); ++it)
        {
            world::IPlayer* p = *it;
            if (!p->getIsAIPlayer() && p->getParticipateInVoting())
                ++count;
        }
        break;
    default:
        break;
    }
    return count;
}

// : ceil(majority * potential voters), through a float, truncated.
int ServerVoteManager::calcVotesNeeded(Vote::VoteType type, Vote& vote, int team, int item)
{
    const float voters = (float)getNumberOfPotentialVoters(type, team, item);
    const float needed = (float)std::ceil((double)vote.getMajority() * (double)voters);
    return (int)needed;
}

void  ServerVoteManager::setVoteMapMajority(float majority) { m_mapVote.setMajority(majority); }
float ServerVoteManager::getVoteMapMajority() const         { return m_mapVote.getMajority(); }

// The team-indexed pair clamp any team outside {1, 2} to 1 (`(team - 1) >= 2` unsigned).
void ServerVoteManager::setVoteKickPlayerMajority(float majority, int team)
{
    const int index = ((unsigned int)(team - 1) >= 2u) ? 1 : team;
    m_kickVotes[index].setMajority(majority);
}

float ServerVoteManager::getVoteKickPlayerMajority(int team) const
{
    const int index = ((unsigned int)(team - 1) >= 2u) ? 1 : team;
    return m_kickVotes[index].getMajority();
}

void ServerVoteManager::setEnableMapVote(bool enable) { m_mapVote.m_enabled = enable; }
bool ServerVoteManager::getEnableMapVote() const      { return m_mapVote.m_enabled; }

void ServerVoteManager::setEnableKickPlayerVote(bool enable, int team)
{
    const int index = ((unsigned int)(team - 1) >= 2u) ? 1 : team;
    m_kickVotes[index].m_enabled = enable;
}

bool ServerVoteManager::getEnableKickPlayerVote(int team) const
{
    const int index = ((unsigned int)(team - 1) >= 2u) ? 1 : team;
    return m_kickVotes[index].m_enabled;
}

// Dedicated settings use ServerSettings or defaults; a hosted game uses its settings singleton.
float ServerVoteManager::getVotingTime() const
{
#if !DEDICATED
    // BF2.exe 0x5BE19C..0x5BE1D2; the singleton is required, with no null guard.
    if (!isDedicated())
    {
        IGameServerSettings* settings = static_cast<IGameServerSettings*>(
            classManager->getSingleton(std::string("GameServerSettings")));
        return static_cast<float>(settings->getVoteTime());
    }
#endif
    if (serverSettings)
        return (float)serverSettings->m_voteTime;          // +0x28 (cvtsi2ss)
    return 60.0f;                                          // flt_8891AD4
}

int ServerVoteManager::getMinPlayersForIssue() const
{
#if !DEDICATED
    // BF2.exe 0x5BE20B..0x5BE238.
    if (!isDedicated())
    {
        IGameServerSettings* settings = static_cast<IGameServerSettings*>(
            classManager->getSingleton(std::string("GameServerSettings")));
        return settings->getMinPlayersForVoting();
    }
#endif
    if (serverSettings)
        return serverSettings->m_minPlayersForVoting;      // +0x2C
    return 3;
}

bool ServerVoteManager::getTeamVoteOnly() const
{
#if !DEDICATED
    // BF2.exe 0x5BE278..0x5BE2A5.
    if (!isDedicated())
    {
        IGameServerSettings* settings = static_cast<IGameServerSettings*>(
            classManager->getSingleton(std::string("GameServerSettings")));
        return settings->getTeamVoteOnly();
    }
#endif
    if (serverSettings)
        return serverSettings->m_teamVoteOnly;             // +0x30
    return true;
}

// : ServerGameLogic::update's tail. Every queued player still present is kicked
// through IGameServer::kickPlayer(id, PResult 21, "") - the reason enumerator is [UNRESOLVED],
// the value is the wire's. (The ELF also builds a std::string("KICKED") per entry that nothing
// reads - a leftover it destroys unused; not reproduced.) Then the queue is cleared.
void ServerVoteManager::updateKickPlayers()
{
    for (std::list<int>::iterator it = m_playersToKick.begin(); it != m_playersToKick.end(); ++it)
    {
        if (world::playerManager->getPlayerFromId(*it))
        {
            IGameServer* server = (IGameServer*)game->queryInterface(IID_IGameServer);
            if (server)
                server->kickPlayer(*it, (io::PResult)21, std::string(""));   // IGameServer +0x6C
        }
    }
    m_playersToKick.clear();
}

// : only a server, only a present player - queue the id (the bool is unused).
void ServerVoteManager::kickPlayer(int playerId, bool)
{
    if (game->queryInterface(IID_IGameServer))
    {
        if (world::playerManager->getPlayerFromId(playerId))
            m_playersToKick.push_back(playerId);   // insert at end()
    }
}

void ServerVoteManager::changeMap(int mapIndex)
{
    gameLogic->setNextLevel(mapIndex);             // +0x30
    gameLogic->runNextLevel();                     // +0x2C
}

// Checks conflicts against the relevant team vote, or all running votes for a map issue.
// Team-only votes reject teams outside 1 and 2.
bool ServerVoteManager::voteInProgressConflictsWithNewIssue(Vote::VoteType type, int team)
{
    if (type == Vote::VTMutiny)
    {
        if ((unsigned int)(team - 1) > 1u)
            return true;
        return m_mutinyVotes[team].voteInProgress();
    }
    if (type == Vote::VTKickPlayer)
    {
        if (getTeamVoteOnly())
        {
            if ((unsigned int)(team - 1) > 1u)
                return true;
        }
        if (getTeamVoteOnly())
        {
            if (m_kickVotes[team].voteInProgress())
                return true;
        }
        if (getTeamVoteOnly())
            return false;
        return m_kickVotes[1].voteInProgress();
    }
    return m_kickVotes[1].voteInProgress()
        || m_kickVotes[2].voteInProgress()
        || m_mapVote.voteInProgress()
        || m_mutinyVotes[1].voteInProgress()
        || m_mutinyVotes[2].voteInProgress();
}

// : the Vote object an issue lands in.
//   map -> m_mapVote (and any unknown type: `this`, as shipped);
//   kick -> team-vote-only with a team in {1, 2}: that team's; else kickVotes[1] (the "all" box);
//   mutiny -> the team's for {1, 2}, else mutinyVotes[0].
Vote* ServerVoteManager::getVoteObjectFromType(Vote::VoteType type, int team)
{
    if (type == Vote::VTKickPlayer)
    {
        if ((getTeamVoteOnly() && (unsigned int)(team - 1) > 1u) || !getTeamVoteOnly())
            return &m_kickVotes[1];                // +0x0A0
        return &m_kickVotes[team];                 // +0x058 + 72 * team
    }
    if (type == Vote::VTMutiny)
    {
        if ((unsigned int)(team - 1) <= 1u)
            return &m_mutinyVotes[team];           // +0x130 + 72 * team
        return &m_mutinyVotes[0];                  // +0x130
    }
    return &m_mapVote;                             // +0x000 - VTMap and everything else
}

// the per-tick service (the header). The 5-second test is (float exact time
// double request time) > 5.0f, in x87 (fucomip / ja).
void ServerVoteManager::update(float dt)
{
    if (!world::playerManager)
        return;
    if (m_mapChangePending
        && (double)g_system->getExactTime() - m_mapChangeRequestTime > (double)kMapChangeDelay)
    {
        changeMap(m_nextMapIndex);
        m_mapChangePending = false;
        return;
    }
    updateVote(m_kickVotes[1], dt);                // +0x0A0
    updateVote(m_kickVotes[2], dt);                // +0x0E8
    updateVote(m_mutinyVotes[1], dt);              // +0x178
    updateVote(m_mutinyVotes[2], dt);              // +0x1C0
    updateVote(m_mapVote, dt);                     // +0x000
}

// Ends eligible kick or mutiny votes early and broadcasts the tally.
// A departed kick target is still banned for the round; a mutiny whose target ceased being commander continues.
bool ServerVoteManager::handlePrematureVoteEnd(Vote& vote)
{
    if (vote.m_type != Vote::VTMutiny)
    {
        if (vote.m_type == Vote::VTKickPlayer && !world::playerManager->getPlayerFromId(vote.m_voteItem))
        {
            // on BanList::_instance directly (ds:0x8AAB6A4) - a null this on a pure client, as shipped
            BanList::_instance->consoleAddAddressToBanList(std::string(vote.m_voteItemName), std::string("round"));
        }
        else
            return false;
    }
    else
    {
        world::IPlayer* target = world::playerManager->getPlayerFromId(vote.m_voteItem);
        if (target)
        {
            if (target->getIsCommander())                                    // +0x114
                return false;
            target->setCommanderReapplyWaitTime(kCommanderReapplyWait);      // +0x130
        }
    }
    vote.m_inProgress = false;
    vote.m_endTime = 0.0f;
    VoteEvent* event = new VoteEvent();
    event->m_voteType = vote.m_type;
    event->setVoteItem(vote.m_voteItem);
    std::vector<int>& counts = vote.getVoteCount();
    event->m_votesFor     = (unsigned char)counts[0];
    event->m_votesAgainst = (unsigned char)counts[1];
    event->m_voteStatus   = Vote::VSPrematureEnd;
    event->m_votesNeeded  = (unsigned char)vote.m_votesNeeded;
#if !DEDICATED
    // BF2.exe 0x5BEBB9..0x5BEBCA: local delivery precedes the owning network send.
    if (!isDedicated())
        ClientVoteManager::getInstance()->receiveVote(*event);
#endif
    game->sendGameEventToAllPlaying(event);                                  // IGame +0x84
    return true;
}

// Completes a running vote when time or ballots suffice, broadcasts the result, and applies a passed issue.
void ServerVoteManager::updateVote(Vote& vote, float dt)
{
    const int type = vote.m_type;
    vote.update(dt);
    if (!vote.m_inProgress || handlePrematureVoteEnd(vote))
        return;

    std::vector<int>& counts = vote.getVoteCount();
    const int item        = vote.m_voteItem;
    const int votesNeeded = vote.m_votesNeeded;
    bool notComplete = !vote.voteComplete();
    if (vote.m_inProgress && vote.voteInProgress() && counts[0] >= votesNeeded)
        notComplete = false;
    if (notComplete)
        return;

    vote.m_inProgress = false;
    vote.m_endTime = 0.0f;
    VoteEvent* event = new VoteEvent();
    event->m_voteType = type;
    event->setVoteItem(item);
    const unsigned char votesFor = (unsigned char)counts[0];
    event->m_votesFor     = votesFor;
    event->m_votesAgainst = (unsigned char)counts[1];
    bool passed;
    if (votesFor < votesNeeded)
    {
        event->m_voteStatus  = Vote::VSFailed;
        event->m_votesNeeded = (unsigned char)(votesNeeded - counts[0]);
        passed = false;
    }
    else
    {
        event->m_votesNeeded = 0;
        event->m_voteStatus  = Vote::VSPassed;
        passed = true;
    }
#if !DEDICATED
    // BF2.exe 0x5BF0D7..0x5BF0E8: notify the host before broadcast and result application.
    if (!isDedicated())
        ClientVoteManager::getInstance()->receiveVote(*event);
#endif
    game->sendGameEventToAllPlaying(event);                                  // IGame +0x84
    if (!passed)
        return;

    if (type == Vote::VTKickPlayer)
    {
        kickPlayer(item, false);
        BanList::_instance->consoleAddAddressToBanList(std::string(vote.m_voteItemName), std::string("round"));   // ds:0x8AAB6A4 directly
    }
    else if (type == Vote::VTMutiny)
    {
        world::IPlayer* target = world::playerManager->getPlayerFromId(item);
        if (target && squadManager)
        {
            target->setCommanderReapplyWaitTime(kCommanderReapplyWait);      // +0x130
            if (target->getIsCommander())                                    // +0x114
                squadManagerIface()->resignCommander(target->getTeam());       // +0x68
        }
    }
    else if (type == Vote::VTMap)
    {
        m_mapChangePending = true;
        m_mapChangeRequestTime = (double)g_system->getExactTime();
        m_nextMapIndex = item;
    }
}

// : tell ONE player why their ballot was dropped.
void ServerVoteManager::discardVote(int playerId, Vote::VoteType type, int item, Vote::VoteStatus status)
{
    VoteEvent* event = new VoteEvent();
    event->m_voteStatus = status;
    event->m_voteType   = type;
    event->setVoteItem(item);
    event->setVoteCaster(playerId);
    event->m_time = 0.0f;
#if !DEDICATED
    // BF2.exe 0x5BE47F..0x5BE490 delivers locally even for a remote caster's discard.
    if (!isDedicated())
        ClientVoteManager::getInstance()->receiveVote(*event);
#endif
    game->sendGameEvent((unsigned int)playerId, event);                      // IGame +0x90
}

// : the item is a map-list index; it must exist and not be the current map.
void ServerVoteManager::receiveMapVote(int playerId, const VoteEvent& event)
{
    if (!mapList->getEntry(event.m_voteItem))
        return;
    if (mapList->getCurrentMap() == event.m_voteItem)
        return;
    int option = 0;
    if (event.m_voteStatus)
        option = event.m_voteOption;
    getInstance()->castVote((Vote::VoteType)event.m_voteType, (Vote::VoteStatus)event.m_voteStatus,
                            playerId, event.m_voteItem, (Vote::VoteOption)option, 0);
}

// : a NEW mutiny issue needs both the target and the caster present and on the
// same team; a ballot on a running one just goes to castVote.
void ServerVoteManager::receiveMutinyVote(int playerId, const VoteEvent& event)
{
    if (!event.m_voteStatus)
    {
        world::IPlayer* target = world::playerManager->getPlayerFromId(event.m_voteItem);
        if (!target)
            return;
        world::IPlayer* caster = world::playerManager->getPlayerFromId(playerId);
        if (!caster)
            return;
        if (caster->getTeam() != target->getTeam())
            return;
    }
    int option = 0;
    if (event.m_voteStatus)
        option = event.m_voteOption;
    getInstance()->castVote((Vote::VoteType)event.m_voteType, (Vote::VoteStatus)event.m_voteStatus,
                            playerId, event.m_voteItem, (Vote::VoteOption)option, 0);
}

// : a NEW kick issue needs a present, non-local, non-AI target; the target must be
// fully connected (IGameServer::getConnectionState == 4) or the ballot is discarded
// (VSDiscardTarget). A ballot on a running vote (status != 0) skips the target checks.
void ServerVoteManager::receiveKickVote(int playerId, const VoteEvent& event)
{
    if (!event.m_voteStatus)
    {
        world::IPlayer* target = world::playerManager->getPlayerFromId(event.m_voteItem);
        if (!target)
            return;
        if (target == world::playerManager->getLocalHumanPlayer())         // +0x30
            return;
        if (target->getIsAIPlayer())                                       // +0x64
            return;
    }
    IGameServer* server = (IGameServer*)game->queryInterface(IID_IGameServer);
    if (server && server->getConnectionState(event.m_voteItem) != 4)       // IGameServer +0x40
    {
        discardVote(playerId, (Vote::VoteType)event.m_voteType, event.m_voteItem, Vote::VSDiscardTarget);
        return;
    }
    int option = 0;
    if (event.m_voteStatus)
        option = event.m_voteOption;
    getInstance()->castVote((Vote::VoteType)event.m_voteType, (Vote::VoteStatus)event.m_voteStatus,
                            playerId, event.m_voteItem, (Vote::VoteOption)option, 0);
}

// the ballot box (the header's flow).
bool ServerVoteManager::castVote(Vote::VoteType type, Vote::VoteStatus status, int playerId, int item,
                                 Vote::VoteOption option, int team)
{
    world::IPlayer* player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
        return false;
    if (!team)
        team = player->getTeam();                                          // +0xE8
    bool newIssue = false;
    Vote* vote = getVoteObjectFromType(type, team);
    if (!vote->m_enabled)
        return false;
    if (m_mapChangePending || (status == 0 && voteInProgressConflictsWithNewIssue(type, team)))
    {
        discardVote(playerId, type, item, Vote::VSDiscardConflict);
        return false;
    }
    if ((unsigned int)(type - 2) <= 1u && playerId == item)               // kick / mutiny: not yourself
        return false;
    if (vote->getPlayerVote(playerId) >= 0)                                // one ballot each
        return false;
    if (!vote->voteInProgress())
    {
        newIssue = true;
        const int potentialVoters = getNumberOfPotentialVoters(type, team, item);
        std::string itemName;
        if (type == Vote::VTKickPlayer)
        {
            IGameServer* server = (IGameServer*)game->queryInterface(IID_IGameServer);
            // the vote object's previous item is looked up here (before issueVote stores the new
            // one) - as shipped; the IP recorded is that player's, when present.
            world::IPlayer* target = world::playerManager->getPlayerFromId(vote->m_voteItem);
            if (server && target)
                server->getPlayerConnectionIp(target, itemName);           // IGameServer +0x60
        }
        const int votesNeeded = calcVotesNeeded(type, *vote, team, item);
        vote->issueVote(getVotingTime(), item, itemName, playerId, 0, 2, votesNeeded,
                        (unsigned int)potentialVoters);
    }
    const bool laid = vote->layVote(playerId, option);
    std::vector<int>& counts = vote->getVoteCount();
    if (laid)
    {
        VoteEvent* event = new VoteEvent();
        event->m_voteType = type;
        event->setVoteItem(item);
        event->m_votesNeeded  = (unsigned char)vote->m_votesNeeded;
        event->m_votesFor     = (unsigned char)counts[0];
        event->m_votesAgainst = (unsigned char)counts[1];
        event->setVoteCaster(playerId);
        event->m_voteOption = option;
        if (newIssue)
        {
            event->m_voteStatus = Vote::VSIssue;
            event->m_time = getVotingTime();
        }
        else
        {
            event->m_voteStatus = Vote::VSUpdate;
        }
#if !DEDICATED
        // BF2.exe 0x5BEA62..0x5BEA73: receive the issue/update before broadcasting it.
        if (!isDedicated())
            ClientVoteManager::getInstance()->receiveVote(*event);
#endif
        game->sendGameEventToAllPlaying(event);                            // IGame +0x84
    }
    return laid;
}

// : a player toggles participation - the flag on the player (participate when the
// option is 0), and a VSParticipation event carrying the option to everyone playing.
void ServerVoteManager::updateVotingParticipationStatus(int playerId, Vote::VoteOption option)
{
    world::IPlayer* player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
        return;
    player->setParticipateInVoting(option == 0);                           // +0x29C
    VoteEvent* event = new VoteEvent();
    event->m_voteType   = 0;
    event->m_voteStatus = Vote::VSParticipation;
    event->m_voteOption = option;
    event->setVoteItem(playerId);
    event->setVoteCaster(playerId);
    event->m_time = 0.0f;
#if !DEDICATED
    // BF2.exe 0x5BE529..0x5BE53A.
    if (!isDedicated())
        ClientVoteManager::getInstance()->receiveVote(*event);
#endif
    game->sendGameEventToAllPlaying(event);                                // IGame +0x84
}

// Sends a human player's applicable running vote and existing ballots on joining.
// Adds a pending ballot when absent. Local delivery consumes and deletes each event;
// network delivery transfers ownership to the game.
void ServerVoteManager::updateClientState(int playerId)
{
    if (!world::playerManager)
        return;
    world::IPlayer* player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
        return;
    if (player->getIsAIPlayer() || (player->getTeam() != 1 && player->getTeam() != 2))
        return;
    const int team = player->getTeam();
    Vote* vote;
    if (m_mapVote.m_inProgress)
        vote = &m_mapVote;
    else
        vote = m_mutinyVotes[team].m_inProgress ? &m_mutinyVotes[team] : 0;
    if (getTeamVoteOnly() && m_kickVotes[team].m_inProgress)
        vote = &m_kickVotes[team];
    else if (!getTeamVoteOnly())
    {
        if (m_kickVotes[1].m_inProgress)
            vote = &m_kickVotes[1];
    }
    if (!vote)
        return;

    if (vote->getPlayerVote(playerId) < 0)
        vote->layVote(playerId, Vote::VONotVoted);
    std::vector<int>& counts = vote->getVoteCount();
#if DEDICATED
    world::IPlayer* localHuman = world::playerManager->getLocalHumanPlayer();   // +0x30
#endif

    VoteEvent* issue = new VoteEvent();
    issue->m_voteType = vote->m_type;
    issue->setVoteItem(vote->m_voteItem);
    issue->m_votesNeeded  = (unsigned char)vote->m_votesNeeded;
    issue->m_votesFor     = (unsigned char)counts[0];
    issue->m_votesAgainst = (unsigned char)counts[1];
    issue->setVoteCaster(vote->m_issuer);
    issue->m_voteOption = 0;
    issue->m_voteStatus = Vote::VSIssue;
    issue->m_time = vote->timeRemaining();
#if !DEDICATED
    // BF2.exe 0x5BED55..0x5BED97 repeats the local lookup and calls the deleting destructor.
    if (world::playerManager->getLocalHumanPlayer() &&
        world::playerManager->getLocalHumanPlayer()->getId() == playerId)
    {
        if (!isDedicated())
        {
            ClientVoteManager::getInstance()->receiveVote(*issue);
            delete issue;
        }
    }
    else
#else
    if (!localHuman || localHuman->getId() != playerId)
#endif
        game->sendGameEvent((unsigned int)playerId, issue);                // IGame +0x90

    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* p = *it;
        if (p->getIsAIPlayer() || !p->getParticipateInVoting())
            continue;
        const int votingStatus = (int)p->getVotingStatus();                // +0x298
        if (votingStatus != 2 && votingStatus != 3)
            continue;
        VoteEvent* update = new VoteEvent();
        update->m_voteType = vote->m_type;
        update->setVoteItem(vote->m_voteItem);
        update->m_votesNeeded  = (unsigned char)vote->m_votesNeeded;
        update->m_votesFor     = (unsigned char)counts[0];
        update->m_votesAgainst = (unsigned char)counts[1];
        update->setVoteCaster(vote->m_issuer);
        update->m_voteStatus = Vote::VSUpdate;
        update->m_voteOption = ((int)p->getVotingStatus() != 2) ? 1 : 0;
        update->m_time = vote->timeRemaining();
#if !DEDICATED
        // BF2.exe 0x5BEE92..0x5BEED4: same local ownership rule for every ballot update.
        if (world::playerManager->getLocalHumanPlayer() &&
            world::playerManager->getLocalHumanPlayer()->getId() == playerId)
        {
            if (!isDedicated())
            {
                ClientVoteManager::getInstance()->receiveVote(*update);
                delete update;
            }
        }
        else
#else
        localHuman = world::playerManager->getLocalHumanPlayer();
        if (!localHuman || localHuman->getId() != playerId)
#endif
            game->sendGameEvent((unsigned int)playerId, update);
    }
}

// VoteEvent::executeServer's target (the header's flow). A caster id of -1
// travels as 255.
void ServerVoteManager::receiveVote(int playerId, const VoteEvent& event)
{
    if (serverSettings && !serverSettings->m_votingEnabled)                // +0x24
        return;
    if (!world::playerManager)
        return;
    if (playerId == -1)
        playerId = 255;
    if (!event.m_voteStatus)                                               // a NEW issue
    {
        world::IPlayer* player = world::playerManager->getPlayerFromId(playerId);
        if (!player)
            return;
#if !DEDICATED
        // BF2.exe 0x5BF759..0x5BF778; PlayerManager +0x8C counts humans, not bots.
        if (!isDedicated() && world::playerManager->getNumberOfHumanPlayers() == 1 &&
            event.m_voteType == Vote::VTMap)
        {
            receiveMapVote(playerId, event);
            return;
        }
#endif
        const int voters = getNumberOfPotentialVoters((Vote::VoteType)event.m_voteType,
                                                      player->getTeam(), playerId);
        if (voters < getMinPlayersForIssue())
        {
            VoteEvent* reply = new VoteEvent();
            reply->m_voteStatus = Vote::VSTooFewPlayers;
            reply->m_voteType   = event.m_voteType;
            reply->setVoteItem(event.m_voteItem);
            reply->setVoteCaster(playerId);
            reply->m_time = 0.0f;
#if !DEDICATED
            // BF2.exe 0x5BF7EC..0x5BF7FD delivers the refusal before broadcasting it.
            if (!isDedicated())
                ClientVoteManager::getInstance()->receiveVote(*reply);
#endif
            game->sendGameEventToAllPlaying(reply);                        // IGame +0x84
            return;
        }
    }
    switch (event.m_voteType)
    {
    case Vote::VTMap:        receiveMapVote(playerId, event);    break;
    case Vote::VTKickPlayer: receiveKickVote(playerId, event);   break;
    case Vote::VTMutiny:     receiveMutinyVote(playerId, event); break;
    case Vote::VTNone:
        if (event.m_voteStatus == Vote::VSParticipation)
            updateVotingParticipationStatus(playerId, (Vote::VoteOption)event.m_voteOption);
        else if (event.m_voteStatus == Vote::VSClientState)
            getInstance()->updateClientState(playerId);
        break;
    default:
        break;
    }
}

} // namespace hfe
} // namespace dice
