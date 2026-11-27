// Server view of the game object, returned as an adjusted interface by queryInterface.
// Use typed calls and release-based lifetime; there is no virtual destructor.
// [UNRESOLVED] Marked return types remain provisional pending direct evidence.

#ifndef GAME_IGAMESERVER_H     // [INFERRED] guard style
#define GAME_IGAMESERVER_H

#include <string>
#include <iosfwd>

#include "../Core/IBase.h"
#include "IGame.h"                                // TargetChannel, ChannelMessageType (its enums)
#include "../IO/Network/ProtocolLayer/PResult.h"   // io::PResult - kickPlayer's reason

namespace dice {
namespace hfe {

class AutoRecorder;                      // Game/GameServer/AutoRecorder.h - getAutoRecorder
class IPlayerUnlockInfo;

class GameEvent;
class EventNode;
class HTTPRequest;                                // [UNRESOLVED] the persistence (ranked-stats) request
class IClientConnection;
namespace world { class IPlayer; }
namespace io {
    struct NetworkableDescriptor;
    class  NetServer;
    struct ConnectionStatus;                      // [UNRESOLVED] layout - getConnectionStatus's out
}
// BF2.exe 0x4346CC / ELF 0x85704C0 establish the names and their integer values.
enum ConnectionType : int
{
    CTConnecting = 0,
    CTModem28Kbps = 1,
    CTModem56Kbps = 2,
    CTCable128Kbps = 3,
    CTCable256Kbps = 4,
    CTLanT1 = 5,
    CTNumTypes = 6
};

std::istream& operator>>(std::istream& in, ConnectionType& type);
std::ostream& operator<<(std::ostream& out, ConnectionType type);

class IGameServer : public IBase
{
public:
    // the ghost hooks the network manager calls (slots 4..6)
    virtual void addScopableGhost(io::NetworkableDescriptor* descriptor) = 0;   // +0x10
    virtual void removeGhost(io::NetworkableDescriptor* descriptor) = 0;        // +0x14
    virtual void addStaticGhost(io::NetworkableDescriptor* descriptor) = 0;     // +0x18

    virtual bool startServer(const std::string& name, int port, int unused) = 0;    // +0x1C
    // Returns the new player ID, or -1 on failure. ID zero is valid.
    virtual int createPlayer(const std::string& name, bool isAI) = 0;              // +0x20
    virtual int createAndSpawnAIPlayer(const std::string& name, int teamId, int kitId) = 0;   // +0x24
    virtual void destroyPlayer(world::IPlayer* player) = 0;                    // +0x28
    virtual void challengeResponse(int connectionId, GameEvent* event) = 0;    // +0x2C
    virtual void setConnectionType(int connectionId, ConnectionType type) = 0; // +0x30
    virtual void chokeBandwidth() = 0;                                         // +0x34
    virtual bool changePlayerName(int playerId, const std::string& name) = 0;  // +0x38
    virtual IClientConnection* getConnection(int connectionId) const = 0;      // +0x3C
    virtual int  getConnectionState(int connectionId) const = 0;               // +0x40
    virtual bool getConnectionStatus(int connectionId, io::ConnectionStatus& out) = 0;   // +0x44
    virtual void toggleFreeCamera(world::IPlayer* player) = 0;                 // +0x48
    virtual void resetConnections() = 0;                                       // +0x4C
    virtual void debugShowActiveGhosts(int connectionId) = 0;                  // +0x50 (also IGame +0xEC)
    virtual void debugShowStreamManagerSizes() = 0;                            // +0x54
    virtual void debugShowGhostPriorities(int connectionId) = 0;               // +0x58
    virtual int  getTotalActiveGhosts() = 0;                                   // +0x5C
    virtual bool getPlayerConnectionIp(world::IPlayer* player, std::string& out) = 0;        // +0x60 [UNRESOLVED return]
    virtual bool getPlayerConnectionIpAndPort(world::IPlayer* player, std::string& out) = 0; // +0x64
    virtual void updatePlayersVoipStatusOnAllClients(int playerId, bool status) = 0;         // +0x68
    virtual void kickPlayer(int playerId, io::PResult reason, const std::string& text) = 0;  // +0x6C
    // String return is attested by both admin.kickPlayer executors and the two materialised
    // GameServer paths (empty on success, "Player not found." on failure).
    virtual std::string consoleKickPlayer(const std::string& name) = 0;        // +0x70
    virtual void* getStatsManager() = 0;                                       // +0x74 [UNRESOLVED return]
    virtual int  getNumberOfConnectedClients() const = 0;                      // +0x78
    virtual int  getNumberOfConnectingClients() const = 0;                     // +0x7C
    virtual int  getNumDroppedFrames() const = 0;                              // +0x80
    virtual int  getNumOvershotFrames() const = 0;                             // +0x84
    virtual bool getInitComplete() const = 0;                                  // +0x88
    virtual bool getIsInternetServer() const = 0;                              // +0x8C
    virtual void sendUDPPacket(const char* address, unsigned short port, void* data, int size) = 0;   // +0x90 (also IGame +0x10C)
    virtual bool say(const std::string& text) = 0;                             // +0x94 (also IGame +0x34)
    virtual bool sayAll(const std::string& text) = 0;                          // +0x98
    virtual bool sayTeam(int team, const std::string& text) = 0;               // +0x9C
    virtual bool say(const std::string& text, TargetChannel channel, ChannelMessageType type,
                     unsigned int, unsigned int) = 0;                          // +0xA0
    virtual void enqueueRadioMessage(EventNode* node) = 0;                     // +0xA4
    virtual bool addPersistenceConnection(HTTPRequest* request) = 0;           // +0xA8, ELF/PE materialize bool
    virtual bool removePersistenceConnection(HTTPRequest* request) = 0;        // +0xAC, ELF/PE materialize bool
    virtual void setCurrentPersistenceObject(IBase* object) = 0;               // +0xB0
    virtual IBase* getCurrentPersistenceObject() = 0;                          // +0xB4
    // Returns the server's borrowed automatic demo recorder.
    virtual AutoRecorder* getAutoRecorder() = 0;                               // +0xB8
    virtual io::NetServer* getNetServer() = 0;                                 // +0xBC
    virtual void onContentCheckEvent(int, const unsigned char*, const unsigned char*,
                                     const unsigned char*) = 0;                // +0xC0
    virtual bool useContentChecking() = 0;                                     // +0xC4
    virtual bool isClientReady(int connectionId) = 0;                          // +0xC8
    virtual IPlayerUnlockInfo* getPlayerUnlockInfo() = 0;                        // +0xCC
    virtual bool isPythonValid() = 0;                                          // +0xD0
    virtual void setPythonValid(int, bool valid) = 0;                          // +0xD4
    // Hidden return-object construction in both PE and ELF proves these return by value.
    virtual std::string getServerAddress() const = 0;                         // +0xD8
    virtual std::string getName() const = 0;                                  // +0xDC

protected:
    ~IGameServer() {}
};

} // namespace hfe
} // namespace dice

#endif // GAME_IGAMESERVER_H
