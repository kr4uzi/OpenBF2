// Server matchmaking and CD-key authentication through the linked GameSpy SDK.

#include "MatchMakingServer.h"

#include <cstdio>
#include <cstdlib>

#include "../../../Core/StringUtil.h"        // compareIgnoreCase (auth_cb_)

extern "C" {
#include "../../../External/GameSpy/gcdkey/gcdkeys.h"   // gcd_authenticate_user, gcd_getkeyhash, gcd_think
}

#include "MMBackend.h"
#include "GameSpyBackend.h"
#include "ServerQueryCache.h"      // serverQueryCache - MatchMakingServer::update ticks it
#include "../../../Platform/MsvcRand.h"  // [CHOSEN, not recovered] the client CRT's rand (MSVCR71)

namespace dice {
namespace hfe {

namespace gamespy {
const char* const g_gameName = "battlefield2";   // ds:0x8986550 - the GameSpy gamename ()
int g_gameID      = 1059;   // ds:0x888DF48 - [INFERRED] BF2's documented GameSpy gameid (0x423);
int g_XPackGameID = 1256;   // ds:0x888DF4C   verify against the ELF dwords next IDA session. The LAN
                            //   path does not depend on the value (gcd_getkeyhash returns "").
} // namespace gamespy

// Maps SDK results to authorization status; missing error text maps to AR_NO_AUTH.
// These callbacks run on the internet authentication path.
static AuthResult mapGcdResult(int authenticated, char* errmsg)
{
    if (authenticated)
        return AR_AUTHORIZED;
    if (errmsg && *errmsg)
        return (compareIgnoreCase(errmsg,
                    "Your CD Key is disabled. Contact customer service.") == 0)
               ? AR_KEY_DISABLED : AR_KEY_INVALID;
    return AR_NO_AUTH;
}

void auth_cb_(int /*gameId*/, int localId, int authenticated, char* errmsg, void* instance)
{
    GameSpyCDKeyAuthenticator* a = static_cast<GameSpyCDKeyAuthenticator*>(instance);
    a->m_statusCallback(localId, mapGcdResult(authenticated, errmsg), a->m_gameServer);
}

void refresh_cb_(int /*gameId*/, int localId, int authenticated, char* errmsg, void* instance)
{
    GameSpyCDKeyAuthenticator* a = static_cast<GameSpyCDKeyAuthenticator*>(instance);
    a->m_statusCallback(localId, mapGcdResult(authenticated, errmsg), a->m_gameServer);
}

// GameSpyCDKeyAuthenticator
GameSpyCDKeyAuthenticator::GameSpyCDKeyAuthenticator(bool internetServer)
    : m_statusCallback(0)
    , m_gameServer(0)
    , m_internetServer(internetServer)
{
}

GameSpyCDKeyAuthenticator::~GameSpyCDKeyAuthenticator()   // vptr restore only (base is empty)
{
}

void GameSpyCDKeyAuthenticator::setStatusCallback(AuthStatusCallback callback, void* context)
{
    m_statusCallback = callback;
    m_gameServer = context;
}

//  / PE 0x633CE0: one shared, lazily generated eight-character
// challenge. Retail reuses it across instances and consumes exactly eight rand() calls.
const char* GameSpyCDKeyAuthenticator::getChallengeString()
{
    static const char alphabet[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.:,;!#$%&/()=+-^";
    static char challenge[9] = {};
    if (!challenge[0])
    {
        for (unsigned int i = 0; i < 8; ++i)
            challenge[i] = alphabet[bf2crt::rand() % (sizeof(alphabet) - 1)];
        challenge[8] = 0;
    }
    return challenge;
}

// the authentication kickoff. See the header: internet -> gcd_authenticate_user;
// local (LAN) -> the status callback fires synchronously with AR_AUTHORIZED, which reaches authOk.
unsigned int GameSpyCDKeyAuthenticator::connectPlayer(int gameId, int localId, unsigned int userIp,
                                                      const char* challenge, const char* response)
{
    if (m_internetServer)
        gcd_authenticate_user(gameId, localId, userIp, challenge, response,
                              &auth_cb_, &refresh_cb_, this);
    else
        m_statusCallback(localId, AR_AUTHORIZED, m_gameServer);   // the LAN path -> cdkey_auth_callback -> authOk
    return 0;   // the ELF return is register residue; no caller reads it
}

void GameSpyCDKeyAuthenticator::disconnectPlayer(int gameId, int localId)
{
    if (m_internetServer)
        gcd_disconnect_user(gameId, localId);   // internet only; a no-op on LAN
}

void GameSpyCDKeyAuthenticator::update()
{
    if (m_internetServer)
        gcd_think();   // internet only; on LAN this is a no-op
}

// MatchMakingServer
MatchMakingServer::MatchMakingServer(GameServer* gameServer)
{
    m_server = new Server;
    m_server->gameServer            = gameServer;
    m_server->authenticator         = 0;
    // m_server->backends default-constructs empty.
    m_server->internetServer        = false;
    m_server->queryCacheUpdateCount = 30;     // 0x1E
    m_server->updateCount           = 0;
}

// stands up the query cache and the GameSpy backend. addBackend adopts the backend's
// authenticator IFF it initialised (isInitialized). A failed backend leaves no authenticator;
// offline behavior also depends on the vendored SDK's availability fallback. Returns whether
// any backend is registered.
bool MatchMakingServer::initializeServices(bool internetServer)
{
    //  / PE 0x5483B0: a second initialization fails before changing the mode.
    if (!m_server->backends.empty())
        return false;
    m_server->internetServer = internetServer;
    if (!serverQueryCache)
        serverQueryCache = new ServerQueryCache();   // the report cache the qr2 callbacks read
    GameSpyBackend* backend = new GameSpyBackend(m_server);
    addBackend(backend, m_server);
    return !m_server->backends.empty();
}

// every queryCacheUpdateCount-th tick refresh the browser snapshot, then tick each
// backend (GameSpyBackend::update = qr2_think while up).
void MatchMakingServer::update()
{
    if (++m_server->updateCount == m_server->queryCacheUpdateCount)
    {
        m_server->updateCount = 0;
        serverQueryCache->update();
    }
    for (std::list<MMBackend*>::iterator it = m_server->backends.begin();
         it != m_server->backends.end(); ++it)
        (*it)->update();
}

// delete every registered backend and clear the list.
bool MatchMakingServer::shutdownServices()
{
    for (std::list<MMBackend*>::iterator it = m_server->backends.begin();
         it != m_server->backends.end(); ++it)
        delete *it;
    m_server->backends.clear();
    return true;
}

GameSpyCDKeyAuthenticator* MatchMakingServer::getAuthenticator() const
{
    return m_server->authenticator;
}

MatchMakingServer::~MatchMakingServer()
{
    if (m_server)
    {
        shutdownServices();   // deletes the backends (each owns its authenticator as a member)
        delete m_server;
    }
}

// addBackend - 0x80BF590. Register the backend only if it initialised; adopt its authenticator.
// Otherwise delete it. The constructor's early returns do not log; GameServer::init reports failure.
void addBackend(MMBackend* backend, MatchMakingServer::Server* mmSv)
{
    if (backend && backend->isInitialized())
    {
        mmSv->backends.push_back(backend);
        mmSv->authenticator = backend->getAuthenticator();
    }
    else
    {
        delete backend;
    }
}

} // namespace hfe
} // namespace dice
