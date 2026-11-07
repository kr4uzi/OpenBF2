// UDP socket lifecycle and I/O. [PLATFORM] Windows uses pointer-sized socket handles; POSIX uses descriptors.
// Packet and address widths remain fixed across platforms.

#include "UDPSocket.h"
#include "../../../../Core/Debug.h"

#if defined(_WIN32)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>       // gethostbyname / hostent - nameToAddress's DNS arm
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#endif

#include <cstdio>        // sprintf - UDPaddressToString formats the dotted quad
#include <cstring>
#include <cstdlib>

namespace {

#if defined(_WIN32)
typedef SOCKET NativeSocket;
#else
typedef int NativeSocket;
#endif

// [PLATFORM] the one call whose name differs. The ELF's is `close(fd)`.
inline void closeSocketHandle(dice::hfe::io::UDPSocket::SocketHandle handle)
{
#if defined(_WIN32)
    ::closesocket(static_cast<SOCKET>(handle));
#else
    ::close(handle);
#endif
}

static_assert(sizeof(dice::hfe::io::UDPSocket::SocketHandle) == sizeof(NativeSocket),
              "UDPSocket must retain the complete native socket handle");

} // anonymous namespace

namespace dice {
namespace hfe {
namespace io {

// Starts with an invalid socket, empty addresses, nonblocking mode and no simulated packet loss.
UDPSocket::UDPSocket()
    : m_socket(-1)                                      // +0x04  invalid, not 0
    , m_useOwnAddress(false)                            // +0x19  send's selector
    , m_blocking(false)                                 // +0x2B
    , m_float2C(100.0f)                                 // +0x2C
    , m_packetsSent(0)                                  // +0x30  successful sends
{
    // The two Address blocks: 16 zero bytes then length = 0x10 (sizeof(sockaddr_in)).
    for (int i = 0; i < 16; ++i)
    {
        m_address.sockaddr[i]     = 0;                  // +0x08 .. +0x17
        m_peerAddress.sockaddr[i] = 0;                  // +0x1A .. +0x29
    }
    m_address.length     = 0x10;                        // +0x18
    m_peerAddress.length = 0x10;                        // +0x2A
}

// Closes the socket before destroying the interface.
UDPSocket::~UDPSocket()
{
    close();
}

// Closes an open socket and resets its handle to -1. Safe when already closed.
void UDPSocket::close()
{
    if (m_socket != -1)
    {
        closeSocketHandle(m_socket);                    // [PLATFORM] closesocket on Win32
        m_socket = -1;
    }
}

// Sets the broadcast destination port; zero disables broadcast. Returns false if the socket option fails.
// The option length remains one byte, including on systems that reject it.
bool UDPSocket::setBroadcast(unsigned short port)
{
    m_useOwnAddress = port != 0;
    int option = port != 0;
    if (::setsockopt(static_cast<NativeSocket>(m_socket), SOL_SOCKET, SO_BROADCAST,
                     reinterpret_cast<const char*>(&option), 1) != 0)
    {
        m_useOwnAddress = false;
        return false;
    }
    if (m_useOwnAddress)
    {
        struct sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_addr.s_addr = htonl(INADDR_BROADCAST);   // unqualified: a macro on macOS
        address.sin_family = AF_INET;
        address.sin_port = htons(port);   // unqualified: a macro on macOS
        std::memcpy(m_address.sockaddr, &address, 16);
    }
    return true;
}

// Clears the platform's pending socket error without reporting it.
// [PLATFORM] Uses errno on POSIX and the Winsock error state on Windows.
void UDPSocket::showError()
{
#if defined(_WIN32)
    ::WSASetLastError(0);                               // [PLATFORM] the errno analogue
#else
    errno = 0;
#endif
}

// Transfers datagram bytes; returns zero on error, indistinguishable from an empty datagram.
// receiveFrom remembers the sender. send targets either the configured address or that last peer,
// depending on m_useOwnAddress, and increments m_packetsSent only after a successful sendto.
// IPv4 address length remains 16. [PLATFORM] use each socket API's native buffer and length types.
int UDPSocket::send(const unsigned char* buffer, int size)
{
    // the selector - see the note above
    const ISocket::Address& to = m_useOwnAddress ? m_address : m_peerAddress;

    if (::sendto(static_cast<NativeSocket>(m_socket),
                 reinterpret_cast<const char*>(buffer), size, 0,
                 reinterpret_cast<const struct sockaddr*>(to.sockaddr), 16) == -1)
    {
        showError();
        return 0;
    }

    ++m_packetsSent;                                    // +0x30, successes only
    return 1;                                           // 1, not the byte count
}

int UDPSocket::receiveFrom(unsigned char* buffer, int size)
{
#if defined(_WIN32)
    int addrLen = 16;
#else
    socklen_t addrLen = 16;
#endif
    // the sender is written straight into m_peerAddress - see the note above
    const int result = ::recvfrom(static_cast<NativeSocket>(m_socket),
                                  reinterpret_cast<char*>(buffer), size, 0,
                                  reinterpret_cast<struct sockaddr*>(m_peerAddress.sockaddr),
                                  &addrLen);
    if (result == -1)
    {
        showError();
        return 0;                                       // not -1 - see above
    }
    return result;
}

int UDPSocket::receive(unsigned char* buffer, int size)
{
    const int result = ::recv(static_cast<NativeSocket>(m_socket),
                              reinterpret_cast<char*>(buffer), size, 0);
    if (result == -1)
    {
        showError();
        return 0;                                       // not -1 - see above
    }
    return result;
}

// Changes the socket's blocking mode. Returns false on OS failure and retains the previous cached value.
// [PLATFORM] POSIX uses fcntl; Windows uses ioctlsocket.
bool UDPSocket::getBlockingMode() const
{
    return m_blocking;                                  // +0x2B
}

bool UDPSocket::setBlockingMode(bool blocking)
{
#if defined(_WIN32)
    // [PLATFORM] fionbio's argument is NON-blocking, so it is inverted here exactly as the
    // O_NONBLOCK bit is inverted below.
    u_long nonBlocking = blocking ? 0u : 1u;
    if (::ioctlsocket(static_cast<SOCKET>(m_socket), FIONBIO, &nonBlocking) != 0)
        return false;
#else
    const int flags  = ::fcntl(m_socket, F_GETFL, 0);
    const int wanted = blocking ? (flags & ~O_NONBLOCK) // blocking clears the bit
                                : (flags |  O_NONBLOCK);
    if (::fcntl(m_socket, F_SETFL, wanted) != 0)
        return false;
#endif
    m_blocking = blocking;                              // +0x2B, only on success
    return true;
}

// Returns whether both IPv4 addresses and ports match, comparing the address first.
bool UDPSocket::equalAddressAndPort(const Address* a, const Address* b)
{
    const struct sockaddr_in* const sa = reinterpret_cast<const struct sockaddr_in*>(a->sockaddr);
    const struct sockaddr_in* const sb = reinterpret_cast<const struct sockaddr_in*>(b->sockaddr);

    if (sa->sin_addr.s_addr != sb->sin_addr.s_addr)     // dword at byte +4
        return false;
    return sa->sin_port == sb->sin_port;                // word  at byte +2
}

// Compares host addresses only, ignoring ports; used for host bans and duplicate-host checks.
bool UDPSocket::equalAddress(const Address* a, const Address* b)
{
    const struct sockaddr_in* const sa = reinterpret_cast<const struct sockaddr_in*>(a->sockaddr);
    const struct sockaddr_in* const sb = reinterpret_cast<const struct sockaddr_in*>(b->sockaddr);
    return sa->sin_addr.s_addr == sb->sin_addr.s_addr;   // dword at byte +4, nothing else
}

// Writes the IPv4 address as a.b.c.d:port and returns true.
bool UDPSocket::UDPaddressToString(const Address& address, std::string* out)
{
    char s[32];
    std::memset(s, 0, sizeof(s));

    const struct sockaddr_in* const sa =
        reinterpret_cast<const struct sockaddr_in*>(address.sockaddr);
    const unsigned int   ip   = static_cast<unsigned int>(sa->sin_addr.s_addr);
    const unsigned short port = sa->sin_port;

    std::sprintf(s, "%d.%d.%d.%d:%d",
                 (int)( ip        & 0xFFu),
                 (int)((ip >>  8) & 0xFFu),
                 (int)((ip >> 16) & 0xFFu),
                 (int)((ip >> 24) & 0xFFu),
                 (int)(unsigned short)((port >> 8) | (port << 8)));   // ror ax, 8

    out->assign(s, std::strlen(s));
    return true;                                        // `mov eax, 1` at 0x847E418
}

// Copies a peer address, clamps its stored length to 16 bytes and selects it for subsequent sends.
void UDPSocket::setPeerAddress(const Address& address)
{
    m_peerAddress.length = address.length;              // +0x2A <- +0x10
    if (m_peerAddress.length > 16u)
        m_peerAddress.length = 16;

    const unsigned int count = m_peerAddress.length;
    for (unsigned int i = 0; i < count; ++i)
        m_peerAddress.sockaddr[i] = address.sockaddr[i];   // +0x1A

    m_useOwnAddress = false;                            // +0x19
}

// `add eax, 1Ah; retn` - the member itself, by reference. No copy.
const ISocket::Address& UDPSocket::getPeerAddress() const
{
    return m_peerAddress;
}

// `mov eax, [eax+4]` - the raw sin_addr, network byte order, straight out of the caller's Address.
unsigned int UDPSocket::getChallengeIp(const Address& address) const
{
    const struct sockaddr_in* const sa =
        reinterpret_cast<const struct sockaddr_in*>(address.sockaddr);
    return static_cast<unsigned int>(sa->sin_addr.s_addr);
}

// Formats an IPv4 address and port through UDPaddressToString and returns its success result.
bool UDPSocket::addressToString(const Address& address, std::string* out)
{
    return UDPaddressToString(address, out);
}

// Resolves an address/port string into out; an empty host resolves the local machine's hostname.
// Returns 0 on success, 1 on DNS failure after zeroing out, or 2 for an accepted numeric address
// longer than 15 characters, leaving out unchanged. The length test follows inet_addr.
// The retail h_length/4 loop visits only one IPv4 address, so the additional-address list stays empty.
// [PLATFORM] access hostent members by name; their widths and offsets differ between platforms.
int UDPSocket::nameToAddress(const std::string& addressAndPort, Address* out,
                             std::list<Address>* extraAddresses)
{
    // Zeroed at entry, before anything else - the same `rep stosd` + length-byte idiom the
    // constructor uses for its own two Addresses (0x847E06B-0x847E082).
    Address extra;
    std::memset(extra.sockaddr, 0, sizeof(extra.sockaddr));
    extra.length = 16;

    std::string    name;
    unsigned short port = 0;
    stringToNameAndPort(addressAndPort, &name, &port);

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;                          // `mov word ptr [ebp+var_58], 2`

    // Try numeric IPv4 parsing before hostname resolution.
    unsigned long ip = INADDR_NONE;                     // `mov ecx, 0FFFFFFFFh`
    if (name != "")
        ip = ::inet_addr(name.c_str());

    if (ip != INADDR_NONE)
    {
        if (name.length() > 15u)                        // see the note above - after inet_addr
            return 2;

        addr.sin_port        = htons(port);             // `ror ax, 8`
        addr.sin_addr.s_addr = static_cast<unsigned int>(ip);
        std::memcpy(out->sockaddr, &addr, 16);          // the four dword stores at 0x847E141
        return 0;
    }

    // The DNS arm
    if (name == "")
    {
        char host[256];
        host[0] = '\0';
        std::memset(host + 1, 0, sizeof(host) - 1);     // the byte then the memset
        if (::gethostname(host, sizeof(host)) == 0)
            name.assign(host, std::strlen(host));
    }

    struct hostent* const he = ::gethostbyname(name.c_str());
    if (!he)
    {
        std::memset(out->sockaddr, 0, 16);              // zeroed on failure
        return 1;
    }

    for (int i = 0; i < he->h_length / 4; ++i)          // h_length, not a count - see above
    {
        unsigned int hostIp;
        std::memcpy(&hostIp, he->h_addr_list[i], sizeof(hostIp));

        addr.sin_addr.s_addr = hostIp;
        addr.sin_port        = htons(port);

        if (i)
        {
            // Unreachable while h_length is 4. Kept because the instructions are there.
            // insert at begin, not at end (0x847E20E loads the list's _M_next).
            std::memcpy(extra.sockaddr, &addr, 16);
            extraAddresses->insert(extraAddresses->begin(), extra);
        }
        else
        {
            std::memcpy(out->sockaddr, &addr, 16);
            if (!extraAddresses)                        // no list wanted - one address is enough
                break;
        }
    }
    return 0;
}

// Splits at the last colon, defaulting port to zero when absent.
// Port parsing uses atoi and truncates to unsigned short without range validation.
bool UDPSocket::stringToNameAndPort(const std::string& s, std::string* name, unsigned short* port)
{
    *port = 0;                                          // 0x847DE94, before anything else

    const std::string::size_type colon = s.rfind(':');  // the backward scan
    if (colon != std::string::npos)
    {
        *port = (unsigned short)atoi(s.substr(colon + 1).c_str());
        *name = s.substr(0, colon);
    }
    else
    {
        *name = s;                                      // the whole string, port left at 0
    }
    return true;
}

// Parses a numeric IPv4 address and port. An empty host means INADDR_ANY.
// Does not resolve hostnames or reject inet_addr failures.
bool UDPSocket::stringToAddress(const std::string& s, struct sockaddr_in* addr)
{
    memset(addr, 0, 16);                                // the four dword stores at 0x847DFC1

    std::string name;
    unsigned short port = 0;
    stringToNameAndPort(s, &name, &port);

    addr->sin_addr.s_addr = name.length() ? ::inet_addr(name.c_str())
                                          : 0;          // INADDR_ANY
    addr->sin_family = AF_INET;                         // 2 on both platforms
    addr->sin_port   = htons(port);                     // the ROR2 at 0x847E028
    return true;
}

// Creates and binds an IPv4 datagram socket, then sets its blocking mode; returns false on failure.
// SO_REUSEADDR is applied only when the high port byte is nonzero (ports at least 256).
// Entry overwrites m_socket with zero. Failure closes the descriptor without resetting the member,
// so destruction may attempt to close it again; preserve this retail behavior.
// [PLATFORM] use native option constants rather than copied Linux numeric values.
bool UDPSocket::listen(std::string addressAndPort, bool blocking)
{
    m_socket = 0;                                       // 0x847DDA9  see above

    struct sockaddr_in addr;
    stringToAddress(addressAndPort, &addr);

    m_socket = static_cast<SocketHandle>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (m_socket == -1)
        return false;                                   // no close - there is nothing to close

    // addr.sa_data[0] - the high byte of the network-order port. See the note above.
    if (reinterpret_cast<const unsigned char*>(&addr)[2] != 0)
    {
        int optval = 1;
        if (::setsockopt(static_cast<NativeSocket>(m_socket), SOL_SOCKET, SO_REUSEADDR,
                         reinterpret_cast<const char*>(&optval), sizeof(optval)) == -1)
        {
            closeSocketHandle(m_socket);                // raw close - no handle reset
            return false;
        }
    }

    if (::bind(static_cast<NativeSocket>(m_socket),
               reinterpret_cast<const struct sockaddr*>(&addr), 16) != 0)
    {
#if defined(__APPLE__)
        // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: macos-singleplayer-hosting-validated]
        // BF2_HOST_TRACE=1: name the listen address and errno of a failed bind (read-only).
        if (std::getenv("BF2_HOST_TRACE"))
            std::fprintf(stderr, "[host] UDPSocket::listen bind %s failed errno %d\n", addressAndPort.c_str(), errno);
#endif
        closeSocketHandle(m_socket);
        return false;
    }

    if (!setBlockingMode(blocking))                     // through the vtable, +0x34
    {
        closeSocketHandle(m_socket);
        return false;
    }
    return true;
}

// Creates a UDP socket bound to any interface on an ephemeral port, with the requested blocking mode.
// [PLATFORM] Windows reports wsaemfile through an error diagnostic.
bool UDPSocket::create(bool blocking)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = 0;                                // ephemeral

    memcpy(m_peerAddress.sockaddr, &addr, 16);          // 0x847DCEC..0x847DD02, dword by dword

    m_socket = static_cast<SocketHandle>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (m_socket == -1)
    {
#if defined(_WIN32)
        // PE 0x6673FB..0x66746D; Debug's type 6, attested source line, and empty expression.
        if (::WSAGetLastError() == WSAEMFILE)
            Debug(DebugTypeError, "IO/Network/NetworkLayer/Socket/UDPSocket.cpp", 213, "")
                << "out of socket descs";
#endif
        return false;
    }

    if (::bind(static_cast<NativeSocket>(m_socket),
               reinterpret_cast<const struct sockaddr*>(&addr), 16) == -1   // == -1, see above
        || !setBlockingMode(blocking))
    {
        closeSocketHandle(m_socket);                    // raw close again - no handle reset
        return false;
    }
    return true;
}

// Creates a nonblocking socket and copies the peer address. Returns the create result; the Boolean argument is ignored.
// No OS connect occurs, and copying leaves the length and broadcast selector unchanged.
bool UDPSocket::connect(const Address& address, bool)
{
    const bool result = create(false);
    std::memcpy(m_peerAddress.sockaddr, address.sockaddr, 16);
    return result;
}

// ELF 0x847E4A0 / PE 0x6677A0. Fixed telemetry words, written in the observed order, no null guard.
// [PLATFORM] The PE writes two additional words; the ELF leaves them untouched.
bool UDPSocket::getPacketInfo(PacketInfo* info)
{
    info->value04 = 1448;
    info->value00 = 1;
    info->value08 = 1064;
#if defined(_WIN32)
    info->value10 = 1;
    info->value0C = 28;
#endif
    return true;
}

// Returns the complement of the retained packet percentage.
// [PLATFORM] Windows multiplies by float 0.01; POSIX divides by 100.
float UDPSocket::getPacketDropRatio() const
{
#if defined(_WIN32)
    return 1.0f - m_float2C * 0.01f;
#else
    return 1.0f - m_float2C / 100.0f;
#endif
}

// ELF 0x847E4F0 / PE 0x6677E0. Ordered bounds checks preserve NaN (including
// the ELF's unordered fcmov flags); do not substitute a min/max that changes that.
void UDPSocket::setPacketDropRatio(float ratio)
{
    if (ratio > 1.0f)
        ratio = 1.0f;
    else if (ratio < 0.0f)
        ratio = 0.0f;
    m_float2C = (1.0f - ratio) * 100.0f;
}

} // namespace io
} // namespace hfe
} // namespace dice
