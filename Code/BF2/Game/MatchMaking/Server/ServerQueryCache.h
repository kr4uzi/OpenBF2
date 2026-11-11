// GameSpy query snapshot containing 45 server strings and 64 rows of eight player
// strings. update rebuilds it from live game state; getters check their indices.
// Preserve virtual order across the differing PE/ELF destructor-slot conventions.

#ifndef GAME_MATCHMAKING_SERVER_SERVERQUERYCACHE_H
#define GAME_MATCHMAKING_SERVER_SERVERQUERYCACHE_H

#include <string>

namespace dice {
namespace hfe {

class IModInfo;

// ServerQueryKey - the 45 server-scope slots, in ELF offset order (index = (offset-4)/4).
enum ServerQueryKey
{
    SQK_serverName = 0, SQK_buildNr, SQK_os, SQK_currentMapId, SQK_currentMapGameMode, SQK_modName,
    SQK_numberOfAllClients, SQK_maxPlayers, SQK_scoreLimit, SQK_roundsPerMap, SQK_fixme, SQK_timeLimit,
    SQK_timeUntilRestartMap, SQK_hasPassword, SQK_isRanked, SQK_hardcoded1, SQK_hasPunkbuster,
    SQK_autoRecording, SQK_demoIndexURL, SQK_demoDownloadURL, SQK_hasVoipServerInfo, SQK_autoBalance,
    SQK_friendlyFireEnabled, SQK_punish, SQK_startDelay, SQK_spawnTime, SQK_sponsorText,
    SQK_sponsorLogoURL, SQK_ticketRatio, SQK_teamRatioPercent, SQK_teamName1, SQK_teamName2, SQK_hasBots,
    SQK_useContentChecking, SQK_currentMapSize, SQK_serverPort, SQK_useGlobalUnlocks, SQK_avgFps,
    SQK_communityLogoURL, SQK_usesPlasma, SQK_numReservedSlots,
    SQK_coopBotRatio, SQK_coopBotCount, SQK_coopBotDifficulty,
    SQK_noVehicles,
    NUM_SERVER_KEYS
};

// PlayerQueryKey - the 8 player-scope slots (ELF PlayerInfo order).
enum PlayerQueryKey
{
    PQK_playerName = 0, PQK_ping, PQK_team, PQK_totalScore, PQK_someScore1, PQK_someScore2,
    PQK_profileId, PQK_isAIPlayer,
    NUM_PLAYER_KEYS
};

enum { NUM_PLAYERS = 64 };

class IServerQueryCache
{
public:
    virtual ~IServerQueryCache() {}
    virtual const char* getServerKey(ServerQueryKey) const = 0;
    virtual int getNumPlayers() const = 0;
    virtual const char* getPlayerKey(int, PlayerQueryKey) const = 0;
    virtual void update() = 0;
};

class ServerQueryCache : public IServerQueryCache
{
public:
    ServerQueryCache();
    virtual ~ServerQueryCache();

    const char* getServerKey(ServerQueryKey serverQueryKey) const override;
    int         getNumPlayers() const override;
    const char* getPlayerKey(int player, PlayerQueryKey playerQueryKey) const override;

    virtual void update();                                         // the browser snapshot

private:
    std::string m_serverKeys[NUM_SERVER_KEYS];     // ServerQueryKey 0..44 (contiguous, getServerKey indexes)
    int         m_numPlayers;
    struct PlayerInfo { std::string keys[NUM_PLAYER_KEYS]; };
    PlayerInfo  m_playerInfo[NUM_PLAYERS];
    IModInfo*   m_modInfo;
};

extern ServerQueryCache* serverQueryCache;

} // namespace hfe
} // namespace dice

#endif // GAME_MATCHMAKING_SERVER_SERVERQUERYCACHE_H
