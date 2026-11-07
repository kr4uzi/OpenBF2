// Client protocol interface. Preserve all 29 slots; release owns teardown. [INFERRED] Header placement.
// [CHOSEN, not recovered] Decoded fields replace integer obfuscation. The request-attempt limit
// remains chosen until the original semantics are established.
#ifndef IO_NETWORK_PROTOCOLLAYER_NETCLIENT_H         // [INFERRED] guard style
#define IO_NETWORK_PROTOCOLLAYER_NETCLIENT_H
#if !DEDICATED

#include <string>
#if !defined(_WIN32)
#include <atomic>
#include "../../../Core/Mutex.h"
#include "../../../Core/Thread.h"
#endif

#include "../PacketFilter.h"                         // by-value member at +0x128
#include "Packet.h"                                 // io::Packet - by-value member at +0xC8
#include "NetServer.h"                              // io::ConnectionStatus, the basic/extended header helpers

namespace dice {
namespace hfe {
namespace io {

class BitStream;
class UDPSocket;
class SWConnection;

// INetClient - the 29 slots above the destructor. Return types come from the PE bodies.
class INetClient
{
public:
    virtual int   addRef()                                                        = 0;  // +0x00 - returns the incremented count
    virtual int   getRef()                                                        = 0;  // +0x04
    virtual int   release()                                                       = 0;  // +0x08
    virtual void* queryInterface(unsigned int) const                              = 0;  // +0x0C

    // +0x10  0 up; 6 no socket manager; 7 an allocation failed. The bool is whether a receive
    //        thread will be started by joinServer (and so whether the mutex guards the calls).
    virtual int   init(bool threaded)                                             = 0;
    // +0x14  0 joined; 4 no socket / the canary refused; 6 the request could not be sent; 9 the
    //        thread could not be started; 17 the password is 32+ characters; else the server's
    //        denial reason (17 / 30 / 24 / 23 / 2 / 31 / 36 / 22 ... - GameClient maps them).
    virtual int   joinServer(const std::string& address, int port, bool blocking,
                             const std::string& password, unsigned int timeoutMs,
                             const std::string& modName, bool punkBuster)         = 0;
    virtual void  shutDown()                                                      = 0;  // +0x18
    virtual int   closeConnection(unsigned int reason)                            = 0;  // +0x1C - the disconnect sent three times; 9 without a socket
    virtual int   bufferPacket(BitStream* stream, unsigned int extra)             = 0;  // +0x20 - SWConnection::addPacketToSendBuffer's codes (15 = ring full)
    virtual int   bufferPacket(unsigned char* src, unsigned int n, unsigned int extra) = 0;  // +0x24
    virtual int   send()                                                          = 0;  // +0x28 - 9 before init / without a socket
    virtual bool  getPacketDeliveryStatus(bool& delivered)                        = 0;  // +0x2C
    // +0x30  pops the next receive-queue entry: 0 a data packet (the stream is set over it, in
    //        read mode), 8 the queue is empty, any other value a connection event code.
    virtual int   receive(BitStream* stream)                                      = 0;
    virtual int   receive(unsigned char* dest, unsigned int* length)              = 0;  // +0x34
    virtual int   _update(int* outConnectionId)                                   = 0;  // +0x38 - 8 drained, 14 ring full, 11 malformed, 6 / 13 / the handlers' codes
    virtual void         setConnectionTimeOut(unsigned int)                       = 0;  // +0x3C
    virtual unsigned int getConnectionTimeOut() const                             = 0;  // +0x40
    virtual void         setVersion(unsigned int)                                 = 0;  // +0x44
    virtual unsigned int getVersion() const                                       = 0;  // +0x48
    virtual int          getTime() const                                          = 0;  // +0x4C - m_timeBase + ms since the counter was taken
    virtual unsigned int getMaxPacketSize() const                                 = 0;  // +0x50 - 1472
    virtual float        getPacketDropRatio() const                               = 0;  // +0x54
    virtual void         setPacketDropRatio(float)                                = 0;  // +0x58
    virtual void setStatsGatheringEnabled(bool)                                   = 0;  // +0x5C
    virtual bool getStatsGatheringEnabled() const                                 = 0;  // +0x60
    virtual bool isConnected() const                                              = 0;  // +0x64 - [INFERRED] name: the handshake state is 2
    virtual bool getPing(int* ping) const                                         = 0;  // +0x68 - the last round trip
    virtual bool getConnectionStatus(ConnectionStatus&)                           = 0;  // +0x6C
    virtual const std::string& getDisconnectText() const                          = 0;  // +0x70 - [INFERRED] name: the +0x13C string
};

// NetClient
class NetClient : public INetClient
{
public:
    NetClient();
    virtual ~NetClient();                            // the deleting destructor, slot +0x74

    virtual int   addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;

    virtual int   init(bool threaded);
    virtual int   joinServer(const std::string& address, int port, bool blocking,
                             const std::string& password, unsigned int timeoutMs,
                             const std::string& modName, bool punkBuster);
    virtual void  shutDown();
    virtual int   closeConnection(unsigned int reason);
    virtual int   bufferPacket(BitStream* stream, unsigned int extra);
    virtual int   bufferPacket(unsigned char* src, unsigned int n, unsigned int extra);
    virtual int   send();
    virtual bool  getPacketDeliveryStatus(bool& delivered);
    virtual int   receive(BitStream* stream);
    virtual int   receive(unsigned char* dest, unsigned int* length);
    virtual int   _update(int* outConnectionId);
    virtual void         setConnectionTimeOut(unsigned int timeout);
    virtual unsigned int getConnectionTimeOut() const;
    virtual void         setVersion(unsigned int version);
    virtual unsigned int getVersion() const;
    virtual int          getTime() const;
    virtual unsigned int getMaxPacketSize() const;
    virtual float        getPacketDropRatio() const;
    virtual void         setPacketDropRatio(float ratio);
    virtual void setStatsGatheringEnabled(bool enabled);
    virtual bool getStatsGatheringEnabled() const;
    virtual bool isConnected() const;
    virtual bool getPing(int* ping) const;
    virtual bool getConnectionStatus(ConnectionStatus& status);
    virtual const std::string& getDisconnectText() const;

    // The four named handlers (their mangled names carry the int parameter)
    int  handlePingRequest(int connectionId);
    int  handlePingResponse(int connectionId);
    int  handleDataPacket(int connectionId, unsigned int size);
    int  handleDisconnect(int connectionId);

    // The connect sequence
    int  connect(const std::string& address, int port, bool blocking, const std::string& password,
                 unsigned int timeoutMs, const std::string& modName, bool punkBuster);
    int  sendConnectRequest(bool punkBuster, const std::string& password,
                            const std::string& modName);    // type 1
    int  handleConnectAccept();                             // type 2
    int  handleConnectDenied();                             // type 3
    int  sendConnectAcceptAck();                            // type 4
    int  sendDisconnect(unsigned int reason);               // type 5
    int  sendPingRequest();                                 // type 7
    int  sendPingResponse(unsigned int echoTimeStamp);      // type 8
    int  pingConfirmation(unsigned int ackSequence);
    int  storeConnectionEvent(unsigned int reason);         // NetServer's twin, no id
    void updatePingStatistics(unsigned int rtt, unsigned int clientStamp, int now);
    void sendRawPacket(const char* address, unsigned short port, const void* data, int size);

    // The receive thread (raw Win32 in the PE: CreateMutexA / CreateThread)
    bool startReceiveThread();
    void stopReceiveThread();

    unsigned int elapsedMs() const;                         // ms since m_startCounter

    // [CHOSEN, not recovered] Retry limit; requests are resent at timeoutMs / 3 intervals before refusing with code four.
    static const int kConnectRequestAttempts = 3;

    // Members (see the layout block)
    int            m_refCount;                   // +0x004
    int            m_clockAdjustThreshold;       // +0x008
    unsigned int   m_pingInterval;               // +0x00C
    bool           m_syncClock;                  // +0x010
    bool           m_pingRingFull;               // +0x011
    int            m_pingSamples[32];            // +0x014
    int            m_pingSampleIndex;            // +0x094
    int            m_pingLow;                    // +0x098
    int            m_pingMedian;                 // +0x09C
    int            m_pingHigh;                   // +0x0A0
    int            m_connectionId;               // +0x0A4
    unsigned int   m_version;                    // +0x0AC (plain here; encoded in the PE)
    unsigned int   m_connectionTimeOut;          // +0x0B0
    unsigned int   m_requestSentMs;              // +0x0B4
    bool           m_mustSendRequest;            // +0x0B8
    int            m_connectAttemptsLeft;        // +0x0BC/+0x0C0 - the canary pair's plain meaning
    SWConnection*  m_connection;                 // +0x0C4
    Packet         m_dataPacket;                 // +0x0C8
    UDPSocket*     m_socket;                     // +0x0E0
    long long      m_startCounter;               // +0x0E8
    long long      m_counterFrequency;           // +0x0F0
    int            m_timeBase;                   // +0x0F8
    unsigned char* m_packetOutBuffer;            // +0x100
    BitStream*     m_bitStreamIn;                // +0x104
    BitStream*     m_bitStreamOut;               // +0x108
    bool           m_bInit;                      // +0x10C
    bool           m_threaded;                   // +0x10D
    bool           m_byte10E;                    // +0x10E [UNRESOLVED]
#if defined(_WIN32)
    void*          m_mutex;                      // +0x110 handle
    void*          m_thread;                     // +0x114 handle
    bool           m_stopRequested;              // +0x118
#else
    // [CHOSEN, not recovered] POSIX ownership; create publishes state, stop joins
    // before freeing the mutex/socket. The stop request uses S2.10 seq-cst publication.
    Mutex*         m_mutex;
    Thread*        m_thread;
    std::atomic<bool> m_stopRequested;
#endif
    unsigned int   m_totalTimeSentinel;          // +0x11C
    bool           m_bDebugNetwork;              // +0x124
    PacketFilter   m_packetFilter;               // +0x128
    std::string    m_disconnectText;             // +0x13C
};

} // namespace io
} // namespace hfe
} // namespace dice

#endif // !dedicated
#endif // IO_NETWORK_PROTOCOLLAYER_NETCLIENT_H
