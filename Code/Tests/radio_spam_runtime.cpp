// [TEMP] [CHOSEN, not recovered] Content-free component fixture. Real Player,
// GameServer/GameClient radio/relay, HUD event 51, RadioGLComp and world clock.
// The revive notification branch is copied from production; no health simulation,
// game startup, devices or sockets.
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
#include <utility>
#include <vector>
#include <csignal>
#include <cstring>
#include <execinfo.h>
#define private public
#define protected public
#include "../BF2/Game/GameLogic/Player.h"
#include "../BF2/Game/GameLogic/GameLogic.h"
#include "../BF2/Game/GameServer/GameServer.h"
#if !DEDICATED
#include "../BF2/Game/GameClient/GameClient.h"
#endif
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
#if !DEDICATED
#include "../BF2/Game/Common/ClientConnection.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "../BF2/Menu/BF2HudManager.h"
#endif

using namespace dice::hfe;
namespace dice { namespace hfe { extern ClassManager* classManager; } }
#include "radio_logic_observer.h"
#include "radio_revive_callout.h"

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
#if !DEDICATED
// [TEMP] Observe refusal text without constructing the HUD's movie or sound owners.
struct Hud : hud::BF2HudManager {
    int notices = 0;
    int seconds = 0;
    int team = -1;
    std::wstring text;
    std::wstring getLocalizedString(std::string key) override
    {
        check(key == "HUD_HUD_MENU_CHAT_SPAM_MESSAGE", "spam HUD localization key");
        return L"Blocked for #SEC# second(s).";
    }
    std::wstring replaceWideTokenFromInt(std::wstring value, std::string token, int count) override
    {
        check(value == L"Blocked for #SEC# second(s)." && token == "#SEC#", "spam HUD seconds token");
        seconds = count;
        return L"Blocked for " + std::to_wstring(count) + L" second(s).";
    }
    void addServerChat(std::wstring value, int senderTeam) override
    { ++notices; text = value; team = senderTeam; }
};
#endif
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
        // [TEMP] Forward local playback only for a sender already at the limit.
        bool observeLocalRefusal = false;
        std::vector<std::pair<TargetChannel, std::string>> notices;
        std::vector<PostRemoteEvent*> notifications;
        bool radioMessage(TargetChannel c, const std::string& m, world::IPlayer* p,
                          unsigned int e, bool playback, bool b) override
        { bypass = b; return GameServer::radioMessage(c, m, p, e, playback && observeLocalRefusal, b); }
        bool localGameInfoMessage(TargetChannel c, const std::string& m) override
        { notices.emplace_back(c, m); return true; }
        void sendGameEventIfPlaying(unsigned int id, GameEvent* event) override
        {
            check(id == 42 && event->getType() == 11, "revive notification targets remote medic");
            notifications.push_back(static_cast<PostRemoteEvent*>(event));
        }
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
    reviveCallout(player);
    check(!submission->bypass && player->getRadioSpamFlagCount() == 2,
          "automatic host revive charges one submission unit");
    relay();
    check(player->getRadioSpamFlagCount() == 2, "automatic host revive relay adds no second charge");
    players->local = player;
    server->radioMessage(TCLocal, "get_up", player, 0, false, false); relay();
    check(player->getRadioSpamFlagCount() == 3, "manual host get_up still counts");
    player->getIsThisASpam(true);
    players->local = player;
#if !DEDICATED
    auto* hud = new Hud; hudManager = hud;
    setDedicated(false);
#endif
    submission->observeLocalRefusal = true;
    const auto hostPosted = events->messages.size();
    reviveCallout(player);
    check(!submission->bypass && player->getRadioSpamFlagCount() == 4
          && player->getRadioSpamBlockedFlagCount() > 0 && events->messages.size() == hostPosted,
          "blocked host revive submits no speech event");
#if !DEDICATED
    check(submission->notices.size() == 1 && submission->notices.back().first == 14
          && submission->notices.back().second == "spam", "blocked host gets spam on channel 14");
    check(hud->notices == 1 && hud->seconds == config.radioBlockedDuration + 1
          && hud->team == player->getTeam() && hud->text == L"Blocked for 32 second(s).",
          "blocked host gets the HUD spam line with remaining seconds");
    setDedicated(true);
#else
    check(submission->notices.empty(), "dedicated host omits local spam notice");
#endif
    submission->observeLocalRefusal = false;
    player->resetSpamFilter();
    auto* bot = new Speaker; bot->m_playerId = 254; bot->m_isAI = true;
    players->speaker = bot;
    server->radioMessage(TCLocalTeam, "out_of_heal", bot, 0, false, false); relay();
    check(bot->getRadioSpamFlagCount() == 1 && player->getRadioSpamFlagCount() == 0,
          "bot server-origin message never charges the host");
    bot->resetSpamFilter();
    server->radioMessageFromPlayer(TCTeam, "roger_that", 254, 0, false, false); relay();
    check(bot->getRadioSpamFlagCount() == 1 && player->getRadioSpamFlagCount() == 0,
          "AI from-player path charges its own counter once, never the host");
    players->speaker = player;
    player->m_playerId = 42; player->resetSpamFilter();
    RadioMessageReceivedEvent remote(42, 0, 42, 1, 0, TCTeam, 3);
    server->handleRadioMessageReceivedEvent(&remote, nullptr);
    check(player->getRadioSpamFlagCount() == 1, "remote connection is charged at server relay");

    int reviveId;
    check(logic->m_voiceMessages->namesToIds(&reviveId, nullptr, nullptr, "get_up", "", ""),
          "revive id comes from the level voice registry");
    auto relayRadio = [&](int messageId, int channel = TCLocal, int type = 3, int extra = 0) {
        RadioMessageReceivedEvent event(42, static_cast<unsigned char>(messageId), 42, 1,
                                       static_cast<unsigned char>(extra), channel, type);
        server->handleRadioMessageReceivedEvent(&event, nullptr);
    };
    relayRadio(reviveId);
    check(player->getRadioSpamFlagCount() == 2, "remote human get_up on TCLocal charges at relay");
    auto* voices = logic->m_voiceMessages;
    logic->m_voiceMessages = new VoiceMessages("", "", "");
    relayRadio(reviveId);
    check(player->getRadioSpamFlagCount() == 3, "relay counts without a registered message name");
    logic->m_voiceMessages = nullptr;
    relayRadio(reviveId);
    check(player->getRadioSpamFlagCount() == 4, "relay counts without a voice registry");
    player->resetSpamFilter();
    gameLogic = nullptr;
    relayRadio(reviveId);
    check(player->getRadioSpamFlagCount() == 1, "relay counts without game logic");
    gameLogic = logic; logic->m_voiceMessages = voices;
    player->resetSpamFilter(); relayRadio(0);
    relayRadio(reviveId, TCTeam);
    check(player->getRadioSpamFlagCount() == 2, "get_up on another channel still counts");
    relayRadio(reviveId, TCLocal, 2);
    check(player->getRadioSpamFlagCount() == 3, "get_up with another message type still counts");
    player->resetSpamFilter();
    relayRadio(reviveId, TCLocal, 3, 1);
    check(player->getRadioSpamFlagCount() == 1, "get_up with a recipient flag still counts");
    player->m_isAI = true;
    relayRadio(reviveId);
    check(player->getRadioSpamFlagCount() == 2, "AI revive accounting stays retail");
    player->m_isAI = false;
    struct TextWrapper : GameServer::IEventWrapper {
        int getSenderId() override { return 42; }
        int getChannel() override { return TCLocal; }
        int getFlags() override { return 0; }
        void sendMessage(ServerConnection*) override { std::abort(); }
    } text;
    server->playerMessageEvent(text);
    check(player->getRadioSpamFlagCount() == 3, "text on TCLocal still counts");
    server->adminToPlayerEvent(text);
    check(player->getRadioSpamFlagCount() == 4, "admin-to-player accounting still counts");
    relayRadio(reviveId);
    check(player->getRadioSpamFlagCount() == 4 && player->getRadioSpamBlockedFlagCount() > 0,
          "revive relay still observes an existing block");
    const int blocked = player->getRadioSpamBlockedFlagCount();
    relayRadio(0);
    check(player->getRadioSpamFlagCount() == 4 && player->getRadioSpamBlockedFlagCount() > blocked,
          "other radio remains blocked");
    server->playerMessageEvent(text);
    server->adminToPlayerEvent(text);
    check(player->getRadioSpamFlagCount() == 4, "blocked text/admin messages gain no allowance");

#if !DEDICATED
    struct Client : GameClient {
        bool bypass = false;
        bool observeLocalRefusal = false;
        std::vector<std::pair<TargetChannel, std::string>> notices;
        std::vector<RadioMessageEvent*> sent;
        bool radioMessage(TargetChannel c, const std::string& m, world::IPlayer* p,
                          unsigned int e, bool playback, bool b) override
        { bypass = b; return GameClient::radioMessage(c, m, p, e, playback && observeLocalRefusal, b); }
        bool localGameInfoMessage(TargetChannel c, const std::string& m) override
        { notices.emplace_back(c, m); return true; }
        void sendGameEvent(unsigned int id, GameEvent* event) override
        {
            check(id == 0xFFFFFFFFu && event->getType() == 20, "client radio transport contract");
            sent.push_back(static_cast<RadioMessageEvent*>(event));
        }
    };
    auto* client = new Client;
    client->m_connection = new ClientConnection(CONNECTIONDEMOMODE_0);
    auto* remoteHuman = new Speaker; remoteHuman->m_playerId = 42;
    players->local = players->speaker = remoteHuman;
    game = client;
    reviveCallout(remoteHuman);
    check(!client->bypass && remoteHuman->getRadioSpamFlagCount() == 1 && client->sent.size() == 1,
          "local-human revive through GameClient charges submission");

    // The server's remote-human arm and the client's actual HUD dispatcher use the
    // same notification payload as retail. No local playback is allowed by the observer.
    game = submission; players->local = nullptr; players->speaker = player;
    reviveCallout(player);
    check(submission->notifications.size() == 1, "remote successful revive notifies the medic");
    auto* notification = submission->notifications.back();
    int channel;
    std::memcpy(&channel, notification->m_payload + 32, sizeof channel);
    check(notification->m_eventCategory == ECHud && notification->m_eventId == 51
          && notification->m_payloadBitSize == 36 && channel == TCLocal,
          "remote revive payload carries TCLocal at byte 32");
    game = client; players->local = players->speaker = remoteHuman;
    hud->handleEvent(ECHud, 51, nullptr, notification->m_payload);
    check(!client->bypass && remoteHuman->getRadioSpamFlagCount() == 2 && client->sent.size() == 2,
          "HUD event 51 charges the client's counter with bypass false");
    auto* sent = client->sent.back();
    check(sent->m_messageId == reviveId && sent->m_senderId == 42 && sent->m_field0F == 0
          && sent->m_channel == TCLocal && sent->m_type == 3, "revive wire fields stay retail");
    game = server; players->local = nullptr; players->speaker = player; player->resetSpamFilter();
    RadioMessageReceivedEvent received(42, sent->m_messageId, sent->m_senderId, sent->m_team,
                                       sent->m_field0F, sent->m_channel, sent->m_type);
    server->handleRadioMessageReceivedEvent(&received, nullptr);
    check(player->getRadioSpamFlagCount() == 1 && remoteHuman->getRadioSpamFlagCount() == 2,
          "client submission and server relay charge their own revive counters");
    game = client; players->local = players->speaker = remoteHuman;
    remoteHuman->resetSpamFilter();
    client->radioMessage(TCLocal, "get_up", remoteHuman, 0, false, false);
    check(!client->bypass && remoteHuman->getRadioSpamFlagCount() == 1,
          "manual client get_up still charges submission");
    client->radioMessage(TCTeam, "roger_that", remoteHuman, 0, false, false);
    check(remoteHuman->getRadioSpamFlagCount() == 2, "manual client radio still counts");
    char other[36] = {};
    channel = TCTeam;
    std::strcpy(other, "roger_that"); std::memcpy(other + 32, &channel, sizeof channel);
    hud->handleEvent(ECHud, 51, nullptr, other);
    check(!client->bypass && remoteHuman->getRadioSpamFlagCount() == 3
          && client->sent.back()->m_channel == TCTeam,
          "other HUD radio notifications count and read the payload channel");
    client->radioMessage(TCTeam, "roger_that", remoteHuman, 0, false, false);
    const auto clientSent = client->sent.size();
    client->radioMessage(TCTeam, "roger_that", remoteHuman, 0, false, false);
    check(client->sent.size() == clientSent && remoteHuman->getRadioSpamBlockedFlagCount() > 0,
          "manual client radio remains blocked at the limit");
    client->observeLocalRefusal = true;
    const int hudNotices = hud->notices;
    hud->handleEvent(ECHud, 51, nullptr, notification->m_payload);
    check(!client->bypass && client->sent.size() == clientSent
          && remoteHuman->getRadioSpamFlagCount() == 4,
          "blocked HUD event 51 submits no revive speech event");
    check(client->notices.size() == 1 && client->notices.back().first == 14
          && client->notices.back().second == "spam", "blocked client gets spam on channel 14");
    check(hud->notices == hudNotices + 1 && hud->seconds == config.radioBlockedDuration + 1
          && hud->team == remoteHuman->getTeam() && hud->text == L"Blocked for 32 second(s).",
          "blocked client gets the HUD spam line with remaining seconds");
    game = server; players->local = nullptr; players->speaker = player;
#endif
    config.radioMaxSpamFlagCount = 6;
    WorldPref::resetTickCount(180);
    check(WorldPref::getWorldTime() == 6, "world clock is in seconds at thirty ticks per second");
    std::printf("radio spam DEDICATED=%d: %d checks passed\n", DEDICATED, checks);
    // Fixture collaborators have process lifetime; production teardown is outside this check.
}
