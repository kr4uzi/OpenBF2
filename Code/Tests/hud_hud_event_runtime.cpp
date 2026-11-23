// [TEMP][CHOSEN, not recovered][RETIRE-WHEN: HUD-HUD-EVENT-FIXTURE]
// Actual dispatcher, native catalogue/payload and short owner helpers. External
// typed services only; no declaration-only leaf has a substitute implementation.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cfloat>
#include <deque>
#include <functional>
#include <map>
#include <list>
#include <vector>
#include <string>
#include <new>
#include "../BF2/Menu/BF2HudManager.h"
#define private public
#include "../BF2/Menu/HudLoadingScreen.h"
#include "../BF2/Menu/HudQuestionManager.h"
#include "../BF2/Menu/HudScoreboard.h"
#include "../BF2/Menu/HudInformationAdapter.h"
#include "../BF2/Menu/HudSquad.h"
#undef private
#include "../BF2/Menu/HudAwardCatalogue.h"
#include "../BF2/Menu/HudLocalRadioMessage.h"
#include "../BF2/Menu/HudBlinker.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/LoadStat.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/IGameLogic.h"
#include "../BF2/Game/MapInfo.h"
#include "../BF2/Game/HudInformationLayer.h"
#include "../BF2/Game/Objects/IPlayer.h"
#include "../BF2/Game/GameLogic/SquadManager/ISquadManager.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Scene/Object/PlayerScoreData.h"
#include "../BF2/Sound/BfSound.h"
#include "../BF2/Sound/NullSound.h"
[[noreturn]] static void actionUnsupported(const char* name) {
    std::fprintf(stderr,"[FRONTIER] HUD event fixture unexpected %s\n",name);std::fflush(stderr);std::abort();
}
#include "hud_action_observers.h"
namespace dice { namespace hfe {
EventManager* eventManager=nullptr; IGame* game=nullptr; IGameLogic* gameLogic=nullptr;
IBase* squadManager=nullptr; IBase* g_hudInformationLayer=nullptr;
IBfSound* g_bfSound=nullptr; System* g_system=nullptr;
hud::BF2HudManager* hudManager=nullptr;
namespace world {PlayerManager* playerManager=nullptr;class CPersistentExplosionEffectManager;CPersistentExplosionEffectManager* explosionEffectManager=nullptr;}
} }
using namespace dice::hfe;
using namespace dice::hfe::hud;
static unsigned checks=0,failures=0;
static std::vector<std::string> trace;
static void check(bool ok,const char* label) {++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}}
static std::wstring widen(const std::string& value){return std::wstring(value.begin(),value.end());}
static std::string narrow(const std::wstring& value){return std::string(value.begin(),value.end());}
struct Hud : BF2HudManager {
    int mode=0,previous=0,loaded=0,unloaded=0,toggled=0;
    bool commander=false;
    std::vector<std::wstring> texts;
    std::vector<int> categories,teams;
    std::vector<std::string> icons;
    std::function<void(const std::string&)> onLocalization;
    std::function<void()> onIcon;
    bool onLevelLoaded() override {++loaded;trace.push_back("loaded");return false;}
    bool onLevelUnloaded() override {++unloaded;trace.push_back("unloaded");return false;}
    std::wstring getLocalizedString(std::string key) override {
        trace.push_back("L:"+key);if(onLocalization)onLocalization(key);return widen(key);
    }
    std::wstring replaceWideInputKeys(std::wstring text) override {trace.push_back("keys");return text+L"|keys";}
    std::wstring replaceWideToken(std::wstring text,std::string token,std::wstring replacement) override {
        trace.push_back("W:"+token);return text+L"|"+widen(token)+L"="+replacement;
    }
    std::wstring replaceWideTokenFromString(std::wstring text,std::string token,std::string replacement) override {
        trace.push_back("S:"+token);return text+L"|"+widen(token)+L"="+widen(replacement);
    }
    std::wstring replaceWideTokenFromInt(std::wstring text,std::string token,int replacement) override {
        trace.push_back("I:"+token);return text+L"|"+widen(token)+L"="+std::to_wstring(replacement);
    }
    void addServerChat(std::wstring text,int team) override {trace.push_back("server");texts.push_back(text);teams.push_back(team);}
    void addServerTeamChat(std::wstring text,int team) override {trace.push_back("serverTeam");texts.push_back(text);teams.push_back(team);}
    void addTeamChat(std::wstring text,world::IPlayer*,int team) override {trace.push_back("teamChat");texts.push_back(text);teams.push_back(team);}
    void addRadioChat(std::wstring text,world::IPlayer*,int rank,int personal) override {
        trace.push_back(personal?"rankSelf":"rankOther");texts.push_back(text);teams.push_back(rank);
    }
    void addCommanderChat(std::wstring text,world::IPlayer*,int level) override {trace.push_back("medal");texts.push_back(text);teams.push_back(level);}
    void showRankUpIcon(std::string name,float seconds) override {check(seconds==10,"rank ten-second icon");trace.push_back("rankIcon");icons.push_back(name);if(onIcon)onIcon();}
    void showMedalIcon(std::string name,float seconds) override {check(seconds==10,"medal ten-second icon");trace.push_back("medalIcon");icons.push_back(name);if(onIcon)onIcon();}
    int getMode() override {trace.push_back("mode");return mode;}
    int getPreviousMode() override {return previous;}
    void setMode(int value,bool remember) override {check(remember,"mode remembers previous");trace.push_back("setMode");mode=value;}
    bool isCommanderShown() override {trace.push_back("commanderShown");return commander;}
    void toggleSquadCommander() override {trace.push_back("toggleSquad");++toggled;}
    void enableVOHelpMessage(int id) override {trace.push_back("enable"+std::to_string(id));}
    void disableVOHelpMessage(int id) override {trace.push_back("disable"+std::to_string(id));}
    void refreshChatCategory(int value) override {trace.push_back("refresh"+std::to_string(value));categories.push_back(value);}
    void clearConnectionMessage() override {trace.push_back("clearConnection");}
};
struct Player : world::ActionPlayerBoundary {
    int getKit() const override {actionUnsupported("player kit");}
    int id=7,team=2,squad=1;bool ai=false,commander=false,leader=false,spam=false;
    std::string name="Alice";world::PlayerScoreData score;bool noScore=false;
    std::function<void()> onTeam;
    int getId() const override {trace.push_back("id");return id;}
    int getTeam() const override {trace.push_back("team");if(onTeam)onTeam();return team;}
    int getSquadId() const override {trace.push_back("squad");return squad;}
    const std::string& getName() const override {trace.push_back("name");return name;}
    bool getIsAIPlayer() const override {trace.push_back("ai");return ai;}
    bool getIsCommander() const override {trace.push_back("commander");return commander;}
    bool getIsSquadLeader() const override {trace.push_back("leader");return leader;}
    bool getIsThisASpam(bool count) override {check(!count,"order spam query does not count");trace.push_back("spam");return spam;}
    world::IObject* getVehicle() const override {trace.push_back("vehicle");return nullptr;}
    world::PlayerScoreData* getScoreData() override {trace.push_back("score");return noScore?nullptr:&score;}
    void enableVOHelpMessage(int id) override {trace.push_back("playerHelp"+std::to_string(id));}
    void enableVOHelp(bool flag) override {trace.push_back(flag?"playerVO1":"playerVO0");}
};
struct Players : world::PlayerManager {
    world::IPlayer* local=nullptr;world::IPlayer* found=nullptr;int queries=0,lookups=0,count=0;
    std::deque<world::IPlayer*> replies;
    world::IPlayer* getLocalHumanPlayer() override {trace.push_back("player");++queries;if(replies.empty())return local;auto*r=replies.front();replies.pop_front();return r;}
    world::IPlayer* getPlayerFromId(int id) override {trace.push_back("lookup"+std::to_string(id));++lookups;return found;}
    int getNrOfPlayersInSquad(int team,int squad) override {trace.push_back("count");check(team==2&&squad==1,"squad count exact arguments");return count;}
};
struct Game : ActionGameBoundary {
    void sendGameEvent(unsigned,GameEvent*) override {actionUnsupported("game event");}
    std::vector<std::string> notices,radio,localRadio;
    bool radioMessage(TargetChannel channel,const std::string& text,world::IPlayer*,unsigned target,bool announce,bool silent) override {
        trace.push_back("radio:"+text);radio.push_back(text);check(announce&&!silent,"radio flags preserved");return false;
    }
    bool localRadioMessage(TargetChannel channel,const std::string& text) override {trace.push_back("localRadio:"+text);localRadio.push_back(text);return false;}
    bool localGameInfoMessage(TargetChannel channel,const std::string& text) override {trace.push_back("notice:"+text);notices.push_back(text);return false;}
};
struct Logic : ActionLogicBoundary {
    bool isServer() const override {actionUnsupported("server query");}
    int state=0;bool help=true;
    BeginRoundState getBeginRoundState() override {trace.push_back("round");return static_cast<BeginRoundState>(state);}
    bool getEnableVoHelp() override {trace.push_back("helpEnabled");return help;}
};
struct SquadInfo : ISquadInfo {
    std::string name="Alpha";
    const std::string& getSquadName() override {trace.push_back("squadName");return name;}
    bool getIsPrivate() override {actionUnsupported("info private");}
    bool getIsAccepted(SquadOrderType,OrderType) override {actionUnsupported("info accepted");}
    int getSquadId() override {actionUnsupported("info id");}
    SquadOrder* getSquadOrders() override {actionUnsupported("info orders");}
    SquadOrder* getCommanderOrders() override {actionUnsupported("info commander orders");}
    SquadOrder* getRequest(SquadOrderType) override {actionUnsupported("info request");}
    int getSpawnGroupId() override {actionUnsupported("info spawn");}
    void addSquadLeaderSpawnPoint(world::IPlayer*) override {actionUnsupported("info add");}
    void removeSquadLeaderSpawnPoint(world::IPlayer*) override {actionUnsupported("info remove");}
};
struct Squads : ActionSquadBoundary {
    bool setSquadName(int,int,int,const std::string&,bool) override {actionUnsupported("squad name");}
    void inviteToSquad(int,int,int,bool,bool) override {actionUnsupported("squad invite");}
    void removeFromSquad(int,int,int,bool) override {actionUnsupported("squad remove");}
    int getSquadLeader(int,int) override {actionUnsupported("squad leader");}
    SquadInfo info;bool missing=false;
    ISquadInfo* getSquadInfo(int team,int squad) override {trace.push_back("info"+std::to_string(squad));return missing?nullptr:&info;}
    int getCommander(int team) override {trace.push_back("getCommander");return 19;}
    void setCommanderToOverheadCamMode(int id) override {trace.push_back("overhead"+std::to_string(id));}
    void setIsPrivate(int team,int squad,bool flag,bool broadcast) override {trace.push_back(flag?"private1":"private0");check(!broadcast,"privacy broadcast false");}
    bool setIsAccepted(int team,int squad,bool yes,OrderType order,SquadOrderType type,bool flag) override {
        trace.push_back("accepted");check(team==2&&squad==1&&yes&&order==1&&type==5&&!flag,"exact autoaccept arguments");return false;
    }
};
struct SoundInstance : NullSound {
    float sample[3]={1,2,3};bool missing=false;int plays=0;
    void* getSampleInfo(int index) const override {trace.push_back("sample");check(index==0,"stinger sample zero");return missing?nullptr:const_cast<float*>(sample);}
    void play() override {trace.push_back("play");++plays;}
    void play(int) override {actionUnsupported("wrong sound overload");}
};
struct Sound : BfSoundNull {
    SoundInstance instance;bool missing=false;
    std::vector<std::string> played;
    void playSound(const std::string& name) override {trace.push_back("sound:"+name);played.push_back(name);}
    void* getSound(const std::string& name) const override {trace.push_back("soundLookup:"+name);return missing?nullptr:const_cast<SoundInstance*>(&instance);}
};
struct Events : EventManager {
    struct Posted {unsigned id;void* data;float delay;};std::vector<Posted> posted;
    std::function<void()> onPost;
    unsigned postEvent(EventCategory cat,unsigned id,void* data,float delay) override {
        check(cat==ECHud&&id==73,"native delayed event category/id");trace.push_back("post");posted.push_back({id,data,delay});if(onPost)onPost();return 0;
    }
};
namespace dice { namespace hfe { namespace hud {
struct BF2HudOrdersFixture {
    static void dispatch(BF2HudManager& h,unsigned id,void* data=nullptr){h.handleHudEvent(id,nullptr,data);}
    static int run() {
        SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);setvbuf(stdout,nullptr,_IONBF,0);
        static_assert(sizeof(void*)==4,"x86 only");static_assert(sizeof(BF2HudManager)==824,"HUD ABI unchanged");
        Events events;eventManager=&events;Players players;world::playerManager=&players;
        Game currentGame;game=&currentGame;Logic logic;gameLogic=&logic;Squads squads;squadManager=&squads;
        Sound sound;g_bfSound=&sound;HudInformationLayer information;g_hudInformationLayer=&information;
        LoadStat loadingState;loadStat=&loadingState;
        auto* h=new Hud;hudManager=h;Player player,other;other.id=9;other.name="Bob";
        players.local=&player;players.found=&player;player.score.m_rank=8;other.score.m_rank=10;
        auto call=[&](unsigned id,void* payload=nullptr){trace.clear();dispatch(*h,id,payload);};
        game=nullptr;call(38);check(trace.empty(),"null game precedes any player or payload access");game=&currentGame;
        for(unsigned id: {0u,1u,9u,10u,13u,25u,31u,41u,43u,44u,46u,50u,52u,60u,69u,70u,71u,75u,77u,82u,84u,0xffffffffu}) {
            call(id);check(trace==std::vector<std::string>{"player"},"ignored id still reads local player exactly once");
        }
        call(3);check(!h->m_field248,"event3 clears load state");call(4);check(h->m_field248,"event4 sets load state");
        call(5);check(h->unloaded==1,"event5 calls actual virtual unload selector");call(8);check(h->loaded==1,"event8 calls load selector");
        h->m_field249=false;call(2);check(trace.size()==1,"mode-transition false gate avoids named frontier");
        call(6);call(7);check(trace.size()==1,"loading owner null protects payload and callback");
        HudLoadingScreen loading(nullptr);h->m_field134=&loading;float progress=25;call(6,&progress);
        check(loading.m_progress==static_cast<float>(1.0-25.0*static_cast<double>(0.01f)),"event6 invokes real inverted percentage setter");
        char debug[520];std::memset(debug,'Q',sizeof debug);call(7,debug);check(std::memcmp(loading.m_debug,debug,512)==0,"event7 invokes real512-byte copy and real guarded load callback");h->m_field134=nullptr;
        call(76);check(trace==std::vector<std::string>{"player","commanderShown"},"hidden commander avoids repeated local query");
        h->commander=true;call(76);check(trace==std::vector<std::string>{"player","commanderShown","player","id","overhead7"},"commander camera uses repeated query and player id");h->commander=false;
        int words[4]={7,1,1,42};unsigned char byte=0;
        call(35);call(36,words);check(h->m_field100==nullptr,"null questions avoid display frontier");
        HudScoreboard scoreboard(nullptr);h->m_field120=&scoreboard;
        for(int page: {-1,0,1,2,3,256}) {scoreboard.setPage(page);check(scoreboard.m_toggleScore==(page==0)&&scoreboard.m_toggleSquads==(page==1)&&scoreboard.m_toggleManage==(page==2),"scoreboard page uses full int not bool");}
        players.found=&other;call(33,words);check(currentGame.notices.back()=="new_commander","other player commander notice");
        players.found=&player;HudQuestionManager questions;h->m_field100=&questions;
        call(33,words);check(scoreboard.m_toggleSquads&&!scoreboard.m_toggleScore&&!scoreboard.m_toggleManage,"assignment selects squad page after empty category replies");
        for(int i:{0,1,2,4})check(questions.m_questions[i].capacity()==0,"real category replies release empty native buffers");h->m_field100=nullptr;
        call(34);check(h->categories.back()==8&&h->texts.back()==L"HUD_COMMANDERAPPLY_POSFILLED","filled commander message then category refresh");
        for(int id:{18,19}) {call(id,words);check(h->texts.back().find(L"#SQUADNAME#=Alpha")!=std::wstring::npos,"join response substitutes name");check(sound.played.back()==(id==19?"accepted":"warning"),"join response sound");check(players.queries>0,"player query remains");}
        Squads replacementSquads;replacementSquads.info.name="Replacement";
        player.onTeam=[&]{squadManager=&replacementSquads;};call(18,words);
        check(h->texts.back().find(L"#SQUADNAME#=Replacement")!=std::wstring::npos,"join response reloads squad singleton after nested team callback");
        player.onTeam=nullptr;squadManager=&squads;
        Sound replacementSound;h->onLocalization=[&](const std::string&){g_bfSound=&replacementSound;};call(19,words);
        check(replacementSound.played==std::vector<std::string>{"accepted"},"join response reloads sound singleton after localization callback");
        h->onLocalization=nullptr;g_bfSound=&sound;
        squads.missing=true;call(18,words);check(trace.back()=="sound:warning","missing info still plays warning");call(19,words);check(trace.back()=="sound:accepted","missing info still plays acceptance");squads.missing=false;
        call(15,words);check(trace[3]=="squadName"&&trace.back()=="sound:warning","existing squad name read before localization and sound last");
        char radio[36]={};std::strcpy(radio,"TEST\0trailing");int channel=14;std::memcpy(radio+32,&channel,4);players.local=nullptr;call(51,radio);
        check(currentGame.radio.back()=="TEST","radio byte payload truncates at NUL and permits null local");players.local=&player;
        player.squad=1;player.leader=false;call(23);check(trace==std::vector<std::string>{"player","squad","team","info1","leader","L:HUD_HUD_SQUAD_RECIEVED_NEW_ORDER_SQUADLEADER","keys","server"},"squad-order received exact callback order");
        player.leader=true;h->m_field241=true;call(26);check(trace.back()=="accepted","autoaccept follows radio and local squad/team reads");check(currentGame.radio.back()=="roger_that","autoaccept radio key");
        auto radioPos=std::find(trace.begin(),trace.end(),"radio:roger_that");check(radioPos!=trace.end()&&std::vector<std::string>(radioPos+1,trace.end())==std::vector<std::string>{"squad","team","accepted"},"autoaccept later callbacks occur after radio");player.leader=false;
        players.found=&other;call(20,words);check(h->toggled==1&&sound.played.back()=="kicked","kick uses payload id rather than lookup identity");words[0]=9;call(20,words);check(h->texts.back().find(L"#PLAYERNAME#=Bob")!=std::wstring::npos,"other kick names lookup player");
        {auto texts=h->texts.size();call(21,words);check(h->texts.size()==texts,"squad leave text is for the leader only");}player.leader=true;call(21,words);check(h->texts.back().find(L"HUD_HUD_SQUAD_HAS_LEFT")!=std::wstring::npos,"other squad leave text");player.leader=false;call(22,words);check(h->texts.back().find(L"HUD_HUD_SQUAD_HAS_JOINED")!=std::wstring::npos,"other squad join text");
        call(24,words);check(trace.back()=="teamChat","new other squad leader uses team chat");players.found=&player;words[0]=7;players.count=2;call(24,words);check(currentGame.radio.back()=="new_squadleader","local new leader announces for more than one member");
        words[1]=1;call(68,words);check(h->texts.back().find(L"HUD_TKPUNISH_banned|#PLAYERNAME#=Alice|#KEY#=7")!=std::wstring::npos,"ban event composes player and numeric key without input replacement");
        byte=0;call(62,&byte);check(h->teams.back()==2&&trace==std::vector<std::string>{"player","L:HUD_TEXT_TEAMKILL_PUNISHED","server"},"teamkill punishment exact message branch");byte=255;call(62,&byte);check(trace.size()==1,"any nonzero punishment byte suppresses notification");
        call(45,words);check(h->texts.back().find(L"HUD_HUD_FIRST_UNLOCK")!=std::wstring::npos,"first unlock byte");words[1]=0;call(45,words);check(h->texts.back().find(L"HUD_HUD_NEW_UNLOCK")!=std::wstring::npos,"later unlock byte");
        for(int id:{-1,0,82,83,0x7fffffff}) {words[0]=id;call(66,words);check(trace.back()=="playerHelp"+std::to_string(id<0?0:id>82?82:id),"VO help clamps signed id to0..82");}
        call(67);check(trace==std::vector<std::string>{"player","ai","helpEnabled","playerVO1"},"VO help enable typed selectors");
        for(int state:{-1,0,1,2,3}) {logic.state=state;call(72);check(trace.back()==(state==1?"setMode":state==2?"round":"clearConnection"),"round-state branching");}
        HudAwardCatalogue catalogue;h->m_field14c=&catalogue;
        auto flush=[&]{for(auto item:events.posted)dispatch(*h,73,item.data);events.posted.clear();};
        call(38,&player);check(h->m_field230&&events.posted.size()==1&&events.posted[0].delay==6,"rank uses native score and summed sound duration");
        check(h->icons.back()=="Ingame/GeneralIcons/Ranks/rank_08_Large.tga","exact rank icon path");
        check(trace==std::vector<std::string>{"player","ai","score","score","L:RANK_8","L:HUD_HUD_RANK_RANK_DEFAULT","keys","W:#RANK#","rankSelf","rankIcon","soundLookup:promotionStinger","sample","play","post"},"rank callback and sound overload ordering");
        auto* owned=static_cast<HudLocalRadioMessage*>(events.posted[0].data);check(owned->channel==TCCommander&&owned->message=="AUTO_RULES_youbeenrecommendedforpromotion","owned native promotion payload");flush();check(currentGame.localRadio.back()=="AUTO_RULES_youbeenrecommendedforpromotion","event73 consumes payload through local radio");
        call(38,&other);check(trace[1]=="score"&&trace[2]=="name"&&trace[3]=="L:RANK_10"&&trace.back()=="rankOther","other rank preserves name snapshot before localization");
        player.noScore=true;call(38,&player);check(trace==std::vector<std::string>{"player","ai","score"},"local rank null score guard");player.noScore=false;
        unsigned award[3]={7,1220118,1};call(39,award);check(events.posted.size()==1&&events.posted[0].delay==6,"award lookup drives delayed native message");
        check(h->icons.back()=="Ingame/GeneralIcons/Medals/AWARD_NAME_1220118_1.tga"&&trace.back()=="medal","award display follows event post");flush();
        player.ai=true;call(39,award);check(h->texts.back()==L"Medal not found (MedalType:1220118 Value:1)","AI known award preserves retail not-found path");player.ai=false;
        award[1]=0xffffffffu;award[2]=0x80000000u;call(39,award);check(h->texts.back()==L"Medal not found (MedalType:4294967295 Value:2147483648)","missing award formats unsigned words");
        sound.instance.sample[0]=-FLT_MAX;sound.instance.sample[1]=FLT_MAX;sound.instance.sample[2]=FLT_MAX;call(38,&player);check(events.posted.back().delay==FLT_MAX,"x87 stinger sum avoids premature overflow");flush();
        sound.instance.sample[0]=1;sound.instance.sample[1]=0;sound.instance.sample[2]=std::numeric_limits<float>::quiet_NaN();call(38,&player);check(std::isnan(events.posted.back().delay),"NaN stinger delay survives");flush();
        sound.instance.missing=true;call(38,&player);check(events.posted.back().delay==0&&trace[trace.size()-2]=="play","missing sample still plays with zero delay");flush();sound.instance.missing=false;
        sound.missing=true;call(38,&player);check(events.posted.back().delay==0&&trace[trace.size()-2]=="soundLookup:promotionStinger","missing sound posts zero-delay message");flush();
        players.local=nullptr;for(unsigned id:{11u,23u,26u,28u,67u}) {call(id,words);check(trace.size()==1,"null local rejects guarded event arm");}
        for(unsigned id:{39u,40u,42u,51u,68u,73u,74u,83u}) {call(id);check(trace.size()==1,"null payload guarded arm");}
        HudInformationAdapter adapter;adapter.setField014(0x7fffffff);check(adapter.m_field014==0x7fffffff,"adapter helper preserves whole signed word");
        h->m_field120=nullptr;h->m_field14c=nullptr;h->m_field100=nullptr;
        // The pre-init HUD is intentionally retained. No full destructor or UI
        // factory is exercised; native automatic catalogue/owner destructors run.
        std::printf("HUD HUD events: %u/%u checks passed\n",checks-failures,checks);
        return failures?1:0;
    }
};
} } }
int main(){return BF2HudOrdersFixture::run();}
