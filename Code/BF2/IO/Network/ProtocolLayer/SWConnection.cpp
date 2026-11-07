// Sliding-window connection lifecycle and packet handling. Native STL layouts determine actual member offsets.

#include "SWConnection.h"

#include "../../BitStream.h"
#include "../../../Core/Debug.h"
#include "../../../Core/StringUtil.h"        // formatString
#include "NetServer.h"                       // writeBasicHeader / writeExtendedHeader (free fns in io::)

#include <cstring>                           // memcpy - getPacketFromRecvQueue

// [PLATFORM] Uses wall-clock timestamps: gettimeofday on POSIX and the Windows filetime equivalent.
#if defined(_WIN32)
#  include <windows.h>
#else
#  include <sys/time.h>
#endif

namespace dice {
namespace hfe {
namespace io {

#if !defined(_WIN32)
// [CHOSEN, not recovered] c-loopback: Darwin's struct timeval has a 32-bit tv_usec, so writing it
// over a {long, long} TimeStamp left the upper half of tv_usec stale. A TimeStamp on the stack
// then held garbage microseconds and the handshake timeout (state 1, 10 s) fired at random:
// about one loopback join in three was dropped with reason 10. ELF and LP64 Linux timevals are
// {long, long}, where this is the same as the shipped direct write.
template <class Stamp>
inline void gettimeofdayInto(Stamp& stamp)
{
    struct timeval now;
    gettimeofday(&now, 0);
    stamp.tv_sec  = now.tv_sec;
    stamp.tv_usec = now.tv_usec;
}
#endif

namespace {

// Seeds a wall-clock timestamp. Keep the Windows epoch conversion consistent with NetServer.
void seedTimeStamp(SWConnection::TimeStamp& ts)
{
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= 116444736000000000ULL;                     // 1601-01-01 -> 1970-01-01, in 100ns ticks
    ts.tv_sec  = (long)(t / 10000000ULL);
    ts.tv_usec = (long)((t % 10000000ULL) / 10);
#else
    gettimeofdayInto(ts);
#endif
}

} // anonymous namespace

// Allocates the 256-entry receive queue, 32-entry send buffer and send stream, then resets protocol state.
// Allocation failure is fatal.
SWConnection::SWConnection()
    : m_dword4(0)                                       // +0x04  delete[]'d by the destructor
    , m_dword08(0)
    , m_dword0C(0)
    , m_dword10(0)
    , m_dword14(0)
    , m_dword18(0)
    , m_dword1C(0)
    , m_dword20(0)
    , m_dword24(0)
    , m_dword28(0)
    , m_recvBuffer(0)                                   // +0x3C - assigned below
    , m_sendBuffer(0)                                   // +0x40
    , m_sendStream(0)                                   // +0x44
    , m_handshakeState(0)                                      // +0xA0
    , m_connectionId(-1)                                // +0xA4  unassigned
    , m_dwordCC(0)                                      // +0xCC
{
    // The Address at +0xA8: sixteen zero bytes, then the length byte set separately to 0x10 - the
    // same idiom UDPSocket's constructor uses, and the reason ISocket::Address is 17 bytes.
    for (int i = 0; i < 16; ++i)
        m_address.sockaddr[i] = 0;
    m_address.length = 0x10;                            // +0xB8

    seedTimeStamp(m_time);                              // +0xBC, gettimeofday

    m_recvBuffer = new PacketQueue(0x100);              // operator new(0x20)
    if (!m_recvBuffer)
    {
        Debug(DEBUGTYPE_6, "IO/Network/ProtocolLayer/SWConnection.cpp", 37)
            << formatString("Failed to create recv queue");
        return;                                         // unreachable - the Debug aborts
    }

    m_sendBuffer = new PacketBuffer(0x20);              // operator new(0x1C)
    if (!m_sendBuffer)
    {
        Debug(DEBUGTYPE_6, "IO/Network/ProtocolLayer/SWConnection.cpp", 44)
            << formatString("Failed to create send buffer");
        return;
    }

    m_sendStream = new BitStream(0, 0);                 // operator new(0x64)
    if (!m_sendStream)
    {
        Debug(DEBUGTYPE_6, "IO/Network/ProtocolLayer/SWConnection.cpp", 51)
            << formatString("Failed to create packetStream");
        return;
    }

    reset();                                            // this is what sets m_state
}

// Resets the connection, then frees its receive/send buffers, stream and statistics array.
SWConnection::~SWConnection()
{
    reset();                                            // drains and clears before freeing

    if (m_recvBuffer)
    {
        delete m_recvBuffer;
        m_recvBuffer = 0;
    }
    if (m_sendBuffer)
    {
        delete m_sendBuffer;
        m_sendBuffer = 0;
    }
    if (m_sendStream)
    {
        delete m_sendStream;
        m_sendStream = 0;
    }
    if (m_dword4)
    {
        delete[] m_dword4;                              // array delete - see above
        m_dword4 = 0;
    }
    // ~deque<bool>() runs implicitly.
}

// Resets protocol state while retaining queue and stream storage.
// [UNRESOLVED] Meaning of initial state ten.
void SWConnection::reset()
{
    m_state = 10;                                       // +0x00  [UNRESOLVED] meaning

    m_dword08 = 0;                                      // +0x08 .. +0x28
    m_dword0C = 0;
    m_dword10 = 0;
    m_dword14 = 0;
    m_dword18 = 0;
    m_dword1C = 0;
    m_dword20 = 0;
    m_dword24 = 0;
    m_dword28 = 0;

    m_dword2C = 0;                                      // +0x2C
    m_byte30  = true;                                   // +0x30  the only true seed
    m_dword34 = 0;                                      // +0x34
    m_hasReservedSlot  = false;                                  // +0x38

    m_dword48             = 1;                          // +0x48
    m_remotePacketCounter = 0;                      // +0x4C
    m_dword50             = 0;                          // +0x50
    m_localPacketCounter = -1;                      // +0x54  "nothing sent yet" - not a pinged" - see above

    m_dword58 = 0;
    m_dword5C = 0;
    m_dword60 = 0;
    m_dword64 = 0;
    m_dword68 = 0;
    m_dword6C = 0;
    m_dword70 = 0;
    m_dword74 = 0;

    m_byte104 = false;                                  // +0x104 the four-bytes-short member

    m_recvBuffer->clear();                              // +0x3C  unguarded - see below
    m_sendBuffer->clear();                              // +0x40
    m_sendStream->clear();                              // +0x44

    m_dword80 = 0;                                      // +0x80 .. +0x9C
    m_lastPingSentMs = 0;                               // +0x84  the ping stamp - see the header
    m_dword88 = 0;
    m_dword8C = 0;
    m_dword90 = 0;
    m_dword94 = 0;
    m_dword98 = 0;
    m_dword9C = 0;

    m_handshakeStampMs = 0;                                      // +0x7C
    m_dwordCC = 0;                                      // +0xCC

    seedTimeStamp(m_time);                              // +0xBC, gettimeofday again

    m_dword78 = 0;                                      // +0x78
    m_dwordD0 = 0;                                      // +0xD0
    m_dwordD4 = 0;                                      // +0xD4
    m_delayScaleFactor = 0;                             // +0xD8

    // the pop_front drain, not a clear() - see the note above.
    while (!m_ackWindow.empty())
        m_ackWindow.pop_front();
}

// Returns the stored peer address by reference.
const ISocket::Address& SWConnection::getPeerAddress() const
{
    return m_address;                                   // `add eax, 0A8h` - by reference, no copy
}

void SWConnection::setPeerAddress(const ISocket::Address& address)
{
    m_address.length = address.length;                  // +0xB8 <- source +0x10
    if (m_address.length > 16u)                         // stored first, clamped second
        m_address.length = 16;

    const unsigned int count = m_address.length;
    for (unsigned int i = 0; i < count; ++i)
        m_address.sockaddr[i] = address.sockaddr[i];    // +0xA8
}

// Serializes a ping request into stream. The caller sends it.
// Returns zero when ready or 9 after a serialization failure, clearing the stream error.
unsigned int SWConnection::sendPingRequest(BitStream* stream, unsigned char connectionId,
                                           unsigned int timeStamp)
{
    if (!writeBasicHeader(stream, connectionId, 7)                  // type 7 = ping request
        || !writeExtendedHeader(stream, m_localPacketCounter,
                                m_remotePacketCounter, m_delayScaleFactor))
    {
        return 9;
    }

    if (m_localPacketCounter == -1)                                 // see the note above
    {
        int nothingSentYet = 0;
        stream->writeBits(&nothingSentYet, 1);
    }
    else
    {
        int haveSent = 1;
        stream->writeBits(&haveSent, 1);
    }

    unsigned int stamp = timeStamp;
    stream->writeBits(&stamp, 32);

    if (stream->hasError())
    {
        stream->clearError();
        return 9;
    }
    return 0;
}

// Records delivery results from ackSequence and ackBitmask. Returns false when acknowledgements exceed packets sent.
// Duplicate or out-of-window acknowledgements return true without recording slots.
bool SWConnection::confirmPacket(unsigned int ackSequence, unsigned int ackBitmask)
{
    const unsigned int advance = (ackSequence - m_dword50) & 0x3F;
    if (advance > 0x1F)
        return true;                                 // stale/out-of-window ack - fail open

    m_dword50 = ackSequence;
    if (!advance)
        return true;                                 // duplicate ack

    unsigned int bit = advance;
    do
    {
        --bit;                                       // slots advance-1 .. 0, high bit first
        if (((1u << bit) & ackBitmask) != 0)
        {
            m_ackWindow.push_back(true);             // this sent packet was acknowledged
        }
        else
        {
            ++m_dword74;                             // a lost slot
            m_ackWindow.push_back(false);
        }
        ++m_dword5C;                                 // every slot recorded
    }
    while (bit != 0);

    return m_dword5C <= m_dword58;                   // corruption guard - never confirm > sent
}

// Pops one acknowledgement result into delivered. Returns false when no result is available.
bool SWConnection::getPacketDeliveryStatus(bool& delivered)
{
    if (m_ackWindow.empty())
        return false;
    delivered = m_ackWindow.front();
    m_ackWindow.pop_front();
    return true;
}

// Writes a ping response with connection counters and echoed/current timestamps.
// Returns 0 on success or 9 on stream overflow.
unsigned int SWConnection::sendPingResponse(BitStream* stream, unsigned char connectionId,
                                            unsigned int echoTimeStamp, unsigned int serverTimeStamp)
{
    if (writeBasicHeader(stream, connectionId, 8)
        && writeExtendedHeader(stream, m_localPacketCounter, m_remotePacketCounter, m_delayScaleFactor))
    {
        unsigned int haveSent = (m_localPacketCounter == -1) ? 0u : 1u;
        stream->writeBits(&haveSent, 1);

        unsigned int echo = echoTimeStamp;
        stream->writeBits(&echo, 32);

        unsigned int server = serverTimeStamp;
        stream->writeBits(&server, 32);

        if (!stream->hasError())
            return 0;
        stream->clearError();
    }
    return 9;
}

void SWConnection::setConnectionId(int id)
{
    m_connectionId = id;
}

int SWConnection::addPacketToRecvQueue(Packet& packet)
{
    return m_recvBuffer->insert(packet);                // +0x3C - the 0x100-slot receive ring
}

int SWConnection::getPacketPtrFromRecvQueue(unsigned char** buff, unsigned int* length)
{
    Packet* const front = m_recvBuffer->getFront();
    if (!front)
        return 8;                                                // 8 = the queue is empty

    const int ev = front->connectionEventType;                   // +0x0C
    if (ev == 0 && front->size != 0)
    {
        const unsigned char* const slot =
            static_cast<const unsigned char*>(front->pvoid14) + front->currBytePositionBitStream;

#if defined(_WIN32)
        const unsigned int len = *reinterpret_cast<const unsigned short*>(slot);
#else
        // [CHOSEN, not recovered] POSIX packet prefixes need not be aligned for a short load.
        unsigned short encodedLength;
        std::memcpy(&encodedLength, slot, sizeof(encodedLength));
        const unsigned int len = encodedLength;
#endif
        if (len == 0)
        {
            m_recvBuffer->popFront();
            return 9;                                            // 9 = a zero-length payload
        }

        // The encoded length is not clamped to the received buffer size.
        *length = len;
        *buff   = const_cast<unsigned char*>(slot) + 2;           // past the prefix
    }

    m_recvBuffer->popFront();                                    // before the caller reads *buff
    return ev;
}

int SWConnection::getPacketFromRecvQueue(unsigned char* dest, unsigned int* length)
{
    Packet* const front = m_recvBuffer->getFront();
    if (!front)
        return 8;

    const int ev = front->connectionEventType;
    if (ev == 0 && front->size != 0)
    {
        const unsigned char* const slot =
            static_cast<const unsigned char*>(front->pvoid14) + front->currBytePositionBitStream;

#if defined(_WIN32)
        unsigned int len = *reinterpret_cast<const unsigned short*>(slot);
#else
        // [CHOSEN, not recovered] Preserve the little-endian prefix without a typed overlay.
        unsigned short encodedLength;
        std::memcpy(&encodedLength, slot, sizeof(encodedLength));
        unsigned int len = encodedLength;
#endif

        // the clamp its sibling does not have. Note that it clamps to `size - dword8`, not to
        // `size - currBytePositionBitStream`, so whatever +0x08 counts is what bounds the tail.
        if (len + front->currBytePositionBitStream > front->size)
            len = front->size - front->dword8;

        if (len == 0)
        {
            // [FRONTIER-ADJACENT] Debug type 6 aborts on this build.
            Debug(DEBUGTYPE_6, "IO/Network/ProtocolLayer/SWConnection.cpp", 574)
                << "how did we end up here?";
            m_recvBuffer->popFront();
            return 9;
        }

        *length = len;
        std::memcpy(dest, slot + 2, len);
    }

    m_recvBuffer->popFront();
    return ev;
}

// Queued packets carry placeholder headers; send replaces them with current sequence and acknowledgement values.

// Queues a data packet with a nine-byte header. The third argument is unused.
// Returns 0 when queued, 15 when full or 16 when n + 14 exceeds 1472. Requires an assigned connection id.
int SWConnection::addPacketToSendBuffer(unsigned char* src, unsigned int n, unsigned int /*unused*/)
{
    if (m_connectionId == -1)
    {
        Debug(DEBUGTYPE_6, "IO/Network/ProtocolLayer/SWConnection.cpp", 364)
            << formatString("connection ID must be set");
        return 9;                                   // unreachable - the Debug aborts
    }

    if (n + 14 > 1472)                              // 0x5C0 - one packet cannot exceed the slice
    {
        Debug(DEBUGTYPE_6, "IO/Network/ProtocolLayer/SWConnection.cpp", 370)
            << formatString("Packet to big %d, packet splitting is not supported.", n);
        return 16;                                  // unreachable - the Debug aborts
    }

    Packet* const slot = m_sendBuffer->getNewPacketPtr();
    if (!slot)
        return 15;                                  // the send ring is full

    unsigned char* const buffer =
        static_cast<unsigned char*>(const_cast<void*>(slot->pvoid14));   // the permanent slice

    m_sendStream->setBuffer(buffer, 0x5C1u);        // 1473 - see the Packet note on the extra byte
    m_sendStream->setBitSize(0);

    // placeholder header - send() overwrites the counters with live values before transmit.
    writeBasicHeader(m_sendStream, static_cast<unsigned char>(m_connectionId), 15);
    writeExtendedHeader(m_sendStream, 0, 0, 0);

    unsigned int length = n;
    m_sendStream->writeBits(&length, 16);           // the 16-bit payload-length prefix

    // The header should already be byte-aligned; unexpected padding emits a fatal diagnostic.
    const unsigned int alignedBits = ((m_sendStream->m_highWaterBits + 7u) >> 3) << 3;
    const unsigned int cursor = (m_sendStream->m_readWriteSwitch == 1)
                                    ? m_sendStream->m_readBitPos : m_sendStream->m_bitPos;
    if (alignedBits - cursor != 0)
    {
        m_sendStream->skipBits(alignedBits - cursor);
        Debug(DEBUGTYPE_6, "IO/Network/ProtocolLayer/SWConnection.cpp", 395)
            << formatString("Padding should not be needed");
    }

    // headerBytes == 9. The ELF computes it twice with equivalent cmov idioms; one value.
    const unsigned int headerBytes = (m_sendStream->m_highWaterBits + 7u) >> 3;
    slot->size                      = headerBytes;
    slot->currBytePositionBitStream = headerBytes;
    slot->dword8                    = m_dword48 & 0x3Fu;      // this packet's 6-bit sequence number

    std::memcpy(buffer + headerBytes, src, n);
    slot->currBytePositionBitStream += n;
    slot->size                      += n;

    ++m_dword48;                                             // advance the outbound sequence
    return 0;
}

// Drains queued data to socket, rewriting each header with live counters.
// Returns 15 when the acknowledgement window is full or 6 when the socket refuses; unsent packets stay queued.
unsigned int SWConnection::send(ISocket* socket)
{
    if (!socket)
        return 5;                                   // the decompiler's initial result

    socket->setPeerAddress(m_address);              // aim at this connection's stored peer

    while (m_sendBuffer->m_count != 0)
    {
        Packet* const front = m_sendBuffer->getFront();
        if (front)                                  // always true while m_count != 0; the ELF guards anyway
        {
            // the send window - refuse to run more than 30 packets ahead of the ack horizon.
            if (((front->dword8 - m_dword50) & 0x3Fu) > 0x1Eu)
            {
                ++m_dword78;                        // a window-stall count
                return 15;
            }

            unsigned char* const slot =
                static_cast<unsigned char*>(const_cast<void*>(front->pvoid14));

            // rewrite the header in place with live counters - setBuffer resets the write cursor.
            m_sendStream->setBuffer(slot, 0x5C1u);
            writeBasicHeader(m_sendStream, static_cast<unsigned char>(m_connectionId), 15);
            writeExtendedHeader(m_sendStream, front->dword8, m_remotePacketCounter, m_delayScaleFactor);

            if (!socket->send(slot, static_cast<int>(front->size)))
                return 6;                           // the socket refused it - do not pop

            ++m_dword58;                            // packets sent
            m_localPacketCounter = static_cast<int>(front->dword8);
            m_dword68 += front->size;               // bytes sent
            m_sendBuffer->popFront();
        }
    }

    updateStats();
    return 0;
}

// Samples throughput history at most every 500 milliseconds using the fixed reference clock.
void SWConnection::updateStats()
{
    if (!m_byte30)                                  // +0x30 - stats gathering must be enabled
        return;

    if (!m_dword4)                                 // lazily allocate the ring: m_state records of 36 B
    {
        m_dword4 = new unsigned char[36u * m_state];
        std::memset(m_dword4, 0, 36u * m_state);   // the ELF zeroes 9 dwords per record
        if (!m_dword4)
            return;                                // dead (new throws), but the ELF checks
    }

    SWConnection::TimeStamp now;
    seedTimeStamp(now);                            // [PLATFORM] the same wall clock the ctor uses

    // a millisecond clock relative to m_time, offset by m_dwordCC. m_time is not re-seeded here.
    const int nowMs = static_cast<int>(m_dwordCC)
        + (now.tv_usec - m_time.tv_usec) / 1000
        + 1000 * (now.tv_sec - m_time.tv_sec);

    const unsigned int elapsed = static_cast<unsigned int>(nowMs - static_cast<int>(m_dword2C));
    if (elapsed <= 0x1F3u)                          // 499 ms - sample at most twice a second
        return;

    const float perSec = 1000.0f / static_cast<float>(elapsed);

    const unsigned int idx = m_dword34;            // the OLD write index
    float* const rec  = reinterpret_cast<float*>(m_dword4 + 36u * idx);
    // +0x08..+0x1C are six contiguous 4-byte slots holding float snapshots - see the field map above.
    float* const prev = reinterpret_cast<float*>(&m_dword08);

    rec[0] = (static_cast<float>(m_dword68) - prev[0]) * perSec;
    rec[1] = (static_cast<float>(m_dword6C) - prev[1]) * perSec;
    rec[2] = (static_cast<float>(m_dword58) - prev[2]) * perSec;
    rec[3] = (static_cast<float>(m_dword60) - prev[3]) * perSec;
    rec[4] = (static_cast<float>(m_dword70) - prev[4]) * perSec;
    rec[5] = (static_cast<float>(m_dword74) - prev[5]) * perSec;
    *reinterpret_cast<unsigned int*>(m_dword4 + 36u * idx + 32) = m_dword88;   // slot 8, stored raw

    m_dword2C = static_cast<unsigned int>(nowMs);  // the new last-sample stamp
    m_dword34 = (idx + 1) % m_state;               // advance the ring

    prev[0] = static_cast<float>(m_dword68);
    prev[1] = static_cast<float>(m_dword6C);
    prev[2] = static_cast<float>(m_dword58);
    prev[3] = static_cast<float>(m_dword60);
    prev[4] = static_cast<float>(m_dword70);
    prev[5] = static_cast<float>(m_dword74);
}

// Returns averaged connection statistics when available, otherwise leaves stats untouched.
// Requires nonnull output and nonzero capacity; the average includes unfilled zero records.
bool SWConnection::getConnectionStats(CONNSTATSENTRY* stats)
{
    if (!m_byte30 || !m_dword4)
        return false;

    *stats = CONNSTATSENTRY{};
    for (int i = 0; i < static_cast<int>(m_state); ++i)
    {
        CONNSTATSENTRY sample;
        std::memcpy(&sample, m_dword4 + sizeof(sample) * i, sizeof(sample));
        stats->bytesSentPerSecond += sample.bytesSentPerSecond;
        stats->bytesReceivedPerSecond += sample.bytesReceivedPerSecond;
        stats->packetsSentPerSecond += sample.packetsSentPerSecond;
        stats->packetsReceivedPerSecond += sample.packetsReceivedPerSecond;
        stats->packetsLostReceivePerSecond += sample.packetsLostReceivePerSecond;
        stats->packetsLostSendPerSecond += sample.packetsLostSendPerSecond;
        stats->ping += sample.ping;
    }
    stats->bytesSentPerSecond /= static_cast<float>(static_cast<int>(m_state));
    stats->bytesReceivedPerSecond /= static_cast<float>(static_cast<int>(m_state));
    stats->packetsSentPerSecond /= static_cast<float>(static_cast<int>(m_state));
    stats->packetsReceivedPerSecond /= static_cast<float>(static_cast<int>(m_state));
    stats->packetsLostReceivePerSecond /= static_cast<float>(static_cast<int>(m_state));
    stats->packetsLostSendPerSecond /= static_cast<float>(static_cast<int>(m_state));
    stats->ping /= m_state;
    return true;
}

} // namespace io
} // namespace hfe
} // namespace dice
