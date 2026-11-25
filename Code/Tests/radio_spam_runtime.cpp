// [TEMP] [CHOSEN, not recovered] Content-free component fixture. Real Player,
// GameServer radio/relay, RadioGLComp and world clock. No game startup or devices.
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <csignal>
#include <execinfo.h>
#define private public
#define protected public
#include "../BF2/Game/GameLogic/Player.h"
#include "../BF2/Game/GameLogic/GameLogic.h"
#include "../BF2/Game/GameServer/GameServer.h"
#undef protected
#undef private
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/Common/GameEvent.h"
#include "../BF2/Game/GameEvents/RadioMessageEvent.h"
#include "../BF2/Game/GameLogic/VoiceMessages.h"
#include "../BF2/Game/GameServer/ServerInfo.h"
#include "../BF2/Game/Objects/GenericFireArmTemplate.h"
#include "../BF2/Game/Objects/RadioGLCompTemplate.h"
#include "../BF2/Game/Objects/GameLogicComp/RadioGLComp.h"
#include "../BF2/Game/Objects/Weapons/GenericFireArm.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/BinTreeCuller.h"
#include "../BF2/Scene/Object/QuadTreeCuller.h"
#include "../BF2/Core/Ids.h"

using namespace dice::hfe;
namespace dice { namespace hfe { extern ClassManager* classManager; } }
#include "radio_logic_observer.h"

namespace {
int checks;
void check(bool ok, const char* name)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", name); std::abort(); }
}
struct Events : EventManager {
    std::vector<RadioMessageReceivedEvent*> messages;
    unsigned int postEvent(EventCategory category, EventNode* node, float delay) override
    {
        check(category == ECNetwork && delay == 0 && node->getType() == 13, "radio event contract");
        messages.push_back(static_cast<RadioMessageReceivedEvent*>(node));
        return 1;
    }
};
struct Classes : ClassManager {
    IBase* createInstance(unsigned int cid, unsigned int iid, IBase* outer) override
    {
        if (cid == world::CID_BinTreeCuller) return world::BinTreeCuller::create(iid, outer);
        if (cid == world::CID_QuadTreeCuller) return world::QuadTreeCuller::create(iid, outer);
        std::abort();
    }
};
struct Players : world::PlayerManager {
    world::IPlayer* speaker = nullptr;
    world::IPlayer* host = nullptr;
    world::IPlayer* local = nullptr;
    world::IPlayer* getLocalHumanPlayer() override { return local; }
    world::IPlayer* getPlayerFromId(int id) override
    {
        if (speaker && id == speaker->getId()) return speaker;
        return host && id == host->getId() ? host : nullptr;
    }
};
struct Speaker : Player {
    Speaker() : Player(-1, nullptr) { m_playerId = 255; m_checkVoip = false; }
    bool hasFirstPersonView() const override { return true; }
};
struct Weapon : world::GenericFireArm {
    world::IPlayer* speaker;
    explicit Weapon(world::IPlayer* p) : GenericFireArm(new world::GenericFireArmTemplate), speaker(p) {}
    world::IPlayer* getPlayer() override { return speaker; }
};
}

int main()
{
    std::signal(SIGSEGV, [](int) {
        void* frames[32];
        const int count = backtrace(frames, 32);
        backtrace_symbols_fd(frames, count, 2);
        std::_Exit(2);
    });
#if !DEDICATED
    setDedicated(true); // Constructors only: omit renderer/tutorial dependencies.
#endif
    g_system = new System;
    settings = new SettingsRepostitory;
    classManager = new Classes;
    world::objectManager = new world::ObjectManager;
    auto* events = new Events; eventManager = events;
    gameEventRegistry = new GameEventRegistry;
    auto* players = new Players; world::playerManager = players;
    auto* logic = new RadioLogic; gameLogic = logic;
    logic->m_voiceMessages = new VoiceMessages("", "", "");
    for (const char* name : {"roger_that", "req_uav", "get_up", "AUTO_MOODGP_usingshockpaddles", "out_of_heal"})
        logic->m_voiceMessages->addMessage(name);
    auto* server = new GameServer; game = server;
    auto* player = new Speaker; players->speaker = players->host = player;
#if !DEDICATED
    setDedicated(false);
#endif
    auto& config = serverInfo->m_block;
    check(config.radioSpamInterval == 6 && config.radioMaxSpamFlagCount == 6 && config.radioBlockedDuration == 30,
          "retail ServerInfo defaults");
    auto* defaults = new ServerSettings;
    check(defaults->m_radioSpamInterval == 6 && defaults->m_radioMaxSpamFlagCount == 6
          && defaults->m_radioBlockedDuration == 30, "retail ServerSettings defaults");
    config.radioMaxSpamFlagCount = 4;
    WorldPref::resetWorldTime(100);
    auto relay = [&] {
        auto* node = events->messages.back(); events->messages.pop_back();
        // No local receiver or connections: exercise the real relay's counting without playback.
        players->local = nullptr;
        server->handleRadioMessageReceivedEvent(node, nullptr);
        delete node;
    };
    for (int i = 1; i <= 4; ++i) {
        check(server->radioMessage(TCPlayer, i % 2 ? "roger_that" : "req_uav", player, 0, false, false),
              "local submission");
        check(player->getRadioSpamFlagCount() == i, "one submission charge");
        relay();
        check(player->getRadioSpamFlagCount() == i, "ID 255 relay adds no second charge");
    }
    const auto posted = events->messages.size();
    server->radioMessage(TCPlayer, "req_uav", player, 0, false, false);
    check(events->messages.size() == posted && player->getRadioSpamFlagCount() == 4,
          "next message refused at configured count");
    check(player->getRadioSpamBlockedFlagCount() > 0, "refusal recorded");
    WorldPref::resetWorldTime(129.999); player->update(0);
    check(player->getRadioSpamFlagCount() == 4, "blocked duration not elapsed");
    WorldPref::resetWorldTime(130); player->update(0);
    check(player->getRadioSpamFlagCount() == 0 && player->getRadioSpamBlockedFlagCount() == 0
          && player->getRadioSpamFilterFlagTimer() == -1, "block expires at equality");
    WorldPref::resetWorldTime(200); player->getIsThisASpam(true);
    WorldPref::resetWorldTime(205.999); player->update(0);
    check(player->getRadioSpamFlagCount() == 1, "interval window retained");
    WorldPref::resetWorldTime(206); player->update(0);
    check(player->getRadioSpamFlagCount() == 0, "interval resets whole window");
    config.radioSpamInterval = 0;
    WorldPref::resetWorldTime(300); player->getIsThisASpam(true); player->update(0);
    check(player->getRadioSpamFlagCount() == 1, "equal clock skips interval reset");
    WorldPref::resetWorldTime(std::nextafter(300.0, 301.0)); player->update(0);
    check(player->getRadioSpamFlagCount() == (DEDICATED ? 1 : 0), "image clock precision retained");
    player->resetSpamFilter(); config.radioSpamInterval = 10000000;
    player->setRadioSpamFlagCount(1); player->setRadioSpamFilterFlagTimer(0.5f);
    WorldPref::resetWorldTime(10000000); player->update(0);
    check(player->getRadioSpamFlagCount() == 1, "subtraction does not round upward to interval boundary");
    player->resetSpamFilter(); config.radioSpamInterval = 6;
    player->m_isCommander = true; config.radioBlockedDuration = 31;
    WorldPref::resetWorldTime(400);
    for (int i = 0; i < 8; ++i) check(!player->getIsThisASpam(true), "commander doubled allowance");
    check(player->getIsThisASpam(true), "commander limit reached");
    WorldPref::resetWorldTime(414.999); player->update(0);
    check(player->getRadioSpamFlagCount() == 8, "commander block retained");
    WorldPref::resetWorldTime(415); player->update(0);
    check(player->getRadioSpamFlagCount() == 0, "odd commander duration truncates to fifteen seconds");
    player->m_isCommander = false;
    auto* weapon = new Weapon(player);
    auto* radioTemplate = new world::RadioGLCompTemplate;
    radioTemplate->setUseMessage("AUTO_MOODGP_usingshockpaddles");
    auto* radio = new world::RadioGLComp(radioTemplate); radio->init(weapon);
    players->local = player;
    // localPlayback is suppressed by dedicated runtime mode to keep all audio paths closed.
#if !DEDICATED
    setDedicated(true);
#endif
    // Observe the component's actual bypass argument through a transport override, below.
    struct Submission : GameServer {
        bool bypass = false;
        bool radioMessage(TargetChannel c, const std::string& m, world::IPlayer* p,
                          unsigned int e, bool, bool b) override
        { bypass = b; return GameServer::radioMessage(c, m, p, e, false, b); }
    };
    auto* submission = new Submission; game = submission;
    radio->playUseMessage(true);
    check(submission->bypass && player->getRadioSpamFlagCount() == 0, "real AUTO_ use bypasses submission");
    relay();
    check(player->getRadioSpamFlagCount() == 0, "AUTO_ host relay stays uncounted");
    radioTemplate->setUseMessage("out_of_heal"); radio->setUseMessageEnabled(true);
    players->local = player;
    radio->playUseMessage(true);
    check(!submission->bypass && player->getRadioSpamFlagCount() == 1, "non-AUTO use counts");
    relay();
    check(player->getRadioSpamFlagCount() == 1, "ordinary use relay counts once");
    players->local = player;
    server->radioMessage(TCLocal, "get_up", player, 0, false, false); relay();
    check(player->getRadioSpamFlagCount() == 2, "retail automatic get_up still counts once");
    auto* bot = new Speaker; bot->m_playerId = 254; bot->m_isAI = true;
    players->speaker = bot;
    server->radioMessage(TCLocalTeam, "out_of_heal", bot, 0, false, false); relay();
    check(bot->getRadioSpamFlagCount() == 1 && player->getRadioSpamFlagCount() == 2,
          "bot server-origin message never charges the host");
    bot->resetSpamFilter();
    server->radioMessageFromPlayer(TCTeam, "roger_that", 254, 0, false, false); relay();
    check(bot->getRadioSpamFlagCount() == 2 && player->getRadioSpamFlagCount() == 2,
          "retail AI from-player path charges its own counter twice, never the host");
    players->speaker = player;
    player->m_playerId = 42; player->resetSpamFilter();
    RadioMessageReceivedEvent remote(42, 0, 42, 1, 0, TCTeam, 3);
    server->handleRadioMessageReceivedEvent(&remote, nullptr);
    check(player->getRadioSpamFlagCount() == 1, "remote connection is charged at server relay");
    config.radioMaxSpamFlagCount = 6;
    WorldPref::resetTickCount(180);
    check(WorldPref::getWorldTime() == 6, "world clock is in seconds at thirty ticks per second");
    std::printf("radio spam DEDICATED=%d: %d checks passed\n", DEDICATED, checks);
    // Fixture collaborators have process lifetime; production teardown is outside this check.
}
