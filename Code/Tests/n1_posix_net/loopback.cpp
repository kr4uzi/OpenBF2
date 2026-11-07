// [TEMP] [CHOSEN, not recovered] Production receive-loop fixture, no engine boot.
// [RETIRE-WHEN: n1-loopback] Retire when automated G6 coverage checks this lifecycle.
#include "IO/Network/ProtocolLayer/NetClient.h"
#include "IO/Network/NetworkLayer/Socket/UDPSocket.h"
#include "IO/Network/SocketManager.h"
#include "IO/BitStream.h"
#include "Core/SettingsRepostitory.h"
#include "External/python-embedded/py-src-2.3.4/Include/Python.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace dice::hfe;
using namespace dice::hfe::io;

static void check(bool value, const char* message)
{
    if (!value) { std::perror(message); std::exit(1); }
}

// Production listen enables SO_REUSEADDR. An exclusive probe must fail on a busy
// port and must succeed after teardown, so reuse cannot hide a leaked listener.
static void checkPortFree()
{
    const int probe = socket(AF_INET, SOCK_DGRAM, 0);
    check(probe >= 0, "exclusive probe socket");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(16890);
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    check(bind(probe, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
          "exclusive bind 127.0.0.1:16890 (port must be unused)");
    close(probe);
}

// Observes the real dispatch result; does not replace packet processing.
struct ObservedClient : NetClient
{
    std::atomic<int> malformed{0};
    int _update(int* id) override
    {
        const int result = NetClient::_update(id);
        if (result == 11) ++malformed;
        return result;
    }
};

int main(int argc, char** argv)
{
    alarm(20); // A stuck join fails the test rather than hanging the runner.
    Py_Initialize();
    check(PyRun_SimpleString(
        "import _socket as socket\n"
        "assert socket.inet_ntop(socket.AF_INET, '\\x7f\\x00\\x00\\x01') == '127.0.0.1'\n"
        "assert socket.inet_ntop(socket.AF_INET6, '\\x00'*15+'\\x01') == '::1'\n"
        "try:\n socket.inet_ntop(socket.AF_INET, 'x')\n"
        "except ValueError:\n pass\n"
        "else:\n raise AssertionError('short IPv4 accepted')\n") == 0, "inet_ntop");
    Py_Finalize();
#if defined(N1_PYTHON_ONLY)
    std::puts("PASS: inet_ntop IPv4/IPv6 and invalid length (no sockets)");
    return 0;
#else
    if (argc == 2 && std::strcmp(argv[1], "--python-only") == 0)
    {
        std::puts("PASS: inet_ntop IPv4/IPv6 and invalid length (no sockets)");
        return 0;
    }
    SettingsRepostitory repository;
    settings = &repository;
    SocketManager manager;
    socketManager = &manager;
    checkPortFree();
    {
        ObservedClient client;
        check(client.init(true) == 0, "client init");
        client.m_socket = new UDPSocket;
        check(client.m_socket->listen("127.0.0.1:16890", false), "bind 16890");
        const int sender = socket(AF_INET, SOCK_DGRAM, 0);
        check(sender >= 0, "sender socket");
        sockaddr_in destination{};
        destination.sin_family = AF_INET;
        destination.sin_port = htons(16890);
        destination.sin_addr.s_addr = inet_addr("127.0.0.1");
        // Keep shutdown's production disconnect sends on the same loopback port.
        ISocket::Address peer{};
        std::memcpy(peer.sockaddr, &destination, sizeof(destination));
        peer.length = sizeof(destination);
        client.m_socket->setPeerAddress(peer);
        auto sendPacket = [&](const unsigned char* bytes, size_t size) {
            check(sendto(sender, bytes, size, 0, reinterpret_cast<sockaddr*>(&destination),
                         sizeof(destination)) == static_cast<ssize_t>(size), "sendto 16890");
        };
        check(client.startReceiveThread(), "start receive thread");
        unsigned char packet[8]{};
        BitStream stream(packet, sizeof(packet));
        check(writeBasicHeader(&stream, 0, 5), "disconnect header");
        unsigned int reason = 23, length = 0;
        stream.writeBits(&reason, 32);
        stream.writeBits(&length, 16);
        sendPacket(packet, sizeof(packet));
        BitStream received(0, 0);
        int result = 8;
        for (int i = 0; i < 500 && result == 8; ++i)
        {
            result = client.receive(&received);
            if (result == 8) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        check(result == 23 && client.receive(&received) == 13, "disconnect events delivered");
        const unsigned char invalid[2]{}; // Unknown type zero, then truncated header.
        sendPacket(invalid, 2);
        sendPacket(invalid, 1);
        for (int i = 0; i < 500 && client.malformed.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        check(client.malformed.load() == 2, "malformed and unknown rejected");
        client.stopReceiveThread();
        check(!client.m_thread && !client.m_mutex, "joined and released");
        client.stopReceiveThread(); // Idempotent after join.
        check(client.startReceiveThread(), "restart receive thread");
        client.stopReceiveThread(); // Idle stop may wait for the three-second select.
        close(sender);
    } // Shutdown destroys socket only after joining.
    checkPortFree();
    UDPSocket rebound;
    check(rebound.listen("127.0.0.1:16890", false), "immediate rebind 16890");
    socketManager = nullptr;
    settings = nullptr;
    std::puts("PASS: inet_ntop, receive dispatch, malformed input, stop/join/restart/rebind");
#endif
}
