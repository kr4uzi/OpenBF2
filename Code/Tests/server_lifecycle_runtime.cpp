// [TEMP] [CHOSEN, not recovered] Headless checks over actual engine objects.
// Real announcement, serialization, connection queues, spawn registries and destruction.
// Services/connections live until exit; no engine init, listener, hook or map is started.
#include <cstdio>
#include <string>
#include <vector>
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/Common/ClientConnection.h"
#include "../BF2/Game/Common/DataBlockManager.h"
#include "../BF2/Game/Common/GameEvent.h"
#include "../BF2/Game/DemoAvailableInfo.h"
#include "../BF2/Game/GameLogic/SpawnSystem/SpawnManager.h"
#include "../BF2/Game/GameServer/GameServer.h"
#include "../BF2/Game/GameServer/ServerSettings.h"
#include "../BF2/IO/Network/NetworkManager.h"

using namespace dice::hfe;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    std::fflush(stdout);
}

void checkBlock(ClientConnection* connection, const std::string& url, const std::string& map)
{
    const DataBlock* block = connection->getDataBlockManager()->m_pending.back();
    std::vector<unsigned char> expected;
    expected.push_back(static_cast<unsigned char>(url.size()));
    expected.insert(expected.end(), url.begin(), url.end());
    expected.push_back(static_cast<unsigned char>(map.size()));
    expected.insert(expected.end(), map.begin(), map.end());
    check(block->m_blockEventId == 9 && block->m_size == expected.size() &&
          std::vector<unsigned char>(block->m_data, block->m_data + block->m_size) == expected,
          "queued announcement has independent block-9 wire payload");
    DemoAvailableInfo decoded;
    decoded.setFromDataBlock(block->m_data, block->m_size);
    check(decoded.getUrl() == url && decoded.getMapName() == map,
          "queued bytes survive announcement stack lifetime and decode correctly");
}
}

int main()
{
    g_system = new System();
    settings = new SettingsRepostitory();
    eventManager = new EventManager();
    gameEventRegistry = new GameEventRegistry();
    io::networkManager = new io::NetworkManager();
    auto* server = new GameServer();
    auto* caller = new GameServer();
    game = server; // Deliberately differs from the receiver of onAutoRecordingAvailable.
    ClientConnection* connections[4];
    const int states[] = {-1, 0, 1, 2};
    for (int i = 0; i < 4; ++i)
    {
        connections[i] = new ClientConnection(static_cast<ConnectionDemoMode>(0));
        connections[i]->m_connectionState = states[i];
        server->m_connections[static_cast<unsigned char>(i)] = connections[i];
    }
    auto* callerConnection = new ClientConnection(static_cast<ConnectionDemoMode>(0));
    callerConnection->m_connectionState = 1;
    caller->m_connections[7] = callerConnection;
    const std::string patterns[] = {"https://example.invalid/{file}", "demos/", "",
                                    "{file}/{file}", "prefix{file}suffix"};
    const std::string urls[] = {"https://example.invalid/round.bf2demo", "demos/round.bf2demo",
                               "round.bf2demo", "round.bf2demo/{file}", "prefixround.bf2demosuffix"};
    for (int i = 0; i < 5; ++i)
    {
        serverSettings->m_demoDownloadURL = patterns[i];
        caller->onAutoRecordingAvailable("round.bf2demo", "strike_at_karkand");
        check(connections[0]->getDataBlockManager()->m_pending.empty() &&
              connections[1]->getDataBlockManager()->m_pending.empty(),
              "unstarted connections receive no announcement");
        check(callerConnection->getDataBlockManager()->m_pending.empty(),
              "broadcast uses global game rather than method receiver");
        for (int n = 2; n < 4; ++n)
        {
            check(connections[n]->getDataBlockManager()->m_pending.size() == static_cast<unsigned>(i + 1),
                  "every started connection receives exactly one announcement");
            checkBlock(connections[n], urls[i], "strike_at_karkand");
        }
        check(serverSettings->m_demoDownloadURL == patterns[i], "configured URL is not mutated");
    }

    // Exercise the real group factory/registry and both direct and refcounted teardown.
    for (int iteration = 0; iteration < 3; ++iteration)
    {
        const unsigned short first = static_cast<unsigned short>(515 + iteration * 2);
        const unsigned short second = static_cast<unsigned short>(first + 1);
        auto* manager = new SpawnManager();
        manager->getCreateSpawnGroup(1, first, true);
        manager->getCreateSpawnGroup(2, second, true);
        check(manager->getSpawnGroups().size() == 2 && io::networkManager->getDescriptor(first) &&
              io::networkManager->getDescriptor(second), "spawn groups are registered before teardown");
        game = 0; // removeAll must suppress removal announcements during teardown.
        if (iteration == 0)
            delete manager;
        else
        {
            if (iteration == 2) manager->removeAll();
            check(manager->release() == 0, "last spawn-manager reference is released");
        }
        check(!io::networkManager->getDescriptor(first) && !io::networkManager->getDescriptor(second),
              "destructor unregisters every remaining group; explicit removeAll is repeatable");
        game = server;
    }
    delete new SpawnManager(); // Empty registry destruction needs no group collaborators.
    std::printf("Server lifecycle: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
