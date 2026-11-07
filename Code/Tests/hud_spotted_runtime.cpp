// [TEMP] Actual HUD spotted/color bodies, real network lookup and commander
// state. Win32 registry and external virtual interfaces are typed observers.
// [RETIRE-WHEN: HUD-SPOTTED-FIXTURE] Real HUD/world lifetime covers these actions.
// [RETIRE-WHEN: HUD-SPOTTED-FIXTURE] Dependency observers/process-lifetime
// retention remain until the complete HUD and object lifecycles are executable.
#include <string>
#include <vector>
#include <map>
#include <list>
#define private public
#include "../BF2/IO/Network/NetworkManager.h"
#undef private
#define main abilityActionsRegressionMain
#include "hud_ability_actions_runtime.cpp"
#undef main
#include "../BF2/IO/Network/INetworkable.h"
#include "../BF2/IO/Network/INetworkableObject.h"
#include "../BF2/IO/Input/ControlMapManager.h"
#include "../BF2/Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../BF2/Game/Objects/Game/CommanderToolsManager.h"
#include "hud_radio_action_observers.h"
namespace dice { namespace hfe { namespace io {
NetworkManager* networkManager=nullptr;IBase* controlMapManager=nullptr;
} namespace world { CommanderToolsManager* commanderToolsManager=nullptr; } } }
struct SpotMaps : io::IControlMapManager {
    io::IControlMap* getActive() const override {trace.push_back("mouse");return nullptr;}
    void addRef() override {actionUnsupported("maps addRef");}
    int getRef() override {actionUnsupported("maps getRef");}
    int release() override {actionUnsupported("maps release");}
    void* queryInterface(unsigned) const override {actionUnsupported("maps queryInterface");}
    void update(float) override {actionUnsupported("maps update");}
    io::IControlMap* create(const std::string&) override {actionUnsupported("maps create");}
    io::IControlMap* getControlMap(const std::string&) const override {actionUnsupported("maps lookup");}
    io::IControlMap* getControlMap(int) const override {actionUnsupported("maps lookup");}
    void deleteControlMap(const std::string&) override {actionUnsupported("maps delete");}
    void deleteControlMap(int) override {actionUnsupported("maps delete");}
    void deleteControlMap(const io::IControlMap*) override {actionUnsupported("maps delete");}
    void setActive(const std::string&) override {actionUnsupported("maps activate");}
    void setActive(const io::IControlMap*) override {actionUnsupported("maps activate");}
    int mapNextInput(io::IControlMap*,int,bool,bool,bool,bool,std::string&) override {actionUnsupported("maps next");}
    float getRepeatRate(int,bool) override {actionUnsupported("maps repeat");}
    void applyActive() override {actionUnsupported("maps apply");}
};
struct SpotPlayer : CommandPlayer {
    bool wounded=false,spam=false;
    bool getIsManDown() override {trace.push_back("wounded");return wounded;}
    bool getIsThisASpam(bool value) override {check(!value,"spot spam flag false");trace.push_back("spam");if(onSpam)onSpam();return spam;}
};
struct SpotHud : CommandHud {
    std::function<void()> onHelp;
    void enableVOHelpMessage(int id) override {check(id==50,"spot enables help50");trace.push_back("help");if(onHelp)onHelp();}
    int getMode() override {trace.push_back("mode");return mode;}
    int getPreviousMode() override {trace.push_back("previous");return 17;}
    void setMode(int value,bool save) override {check(value==17&&save,"spot restores previous mode");trace.push_back("setMode");mode=value;}
};
struct SpotGame : CommandGame {
    unsigned spots=0,radios=0;
    std::function<void()> onMessage,onSpot;
    Vec2 lastPosition;
    world::IPlayer* lastPlayer=nullptr;
    unsigned short lastId=0,messageId=0;
    void sendSpottedMessage(world::IPlayer* player,unsigned short id,Vec2 position,world::VehicleType type) override {
        check(static_cast<int>(type)==3,"spot preserves vehicle type");++spots;lastPlayer=player;lastId=id;lastPosition=position;trace.push_back("spot");if(onSpot)onSpot();
    }
    std::string getSpottedMessage(unsigned short id) override {messageId=id;trace.push_back("text");if(onMessage)onMessage();return std::string("s\0pot",5);}
    bool radioMessage(TargetChannel channel,const std::string& text,world::IPlayer* player,unsigned recipient,bool announce,bool local) override {
        check(channel==1&&text==std::string("s\0pot",5)&&player&&recipient==0&&announce&&!local,"outer radio parameters and full owned string");
        ++radios;trace.push_back("radio");return false;
    }
};
struct SpotPco : world::RadioPcoBoundary {
    world::IPlayer* occupant=nullptr;
    std::map<int,world::IPlayerControlObject*> pcos;
    std::function<void()> onRead;
    world::IPlayer* getOccupingPlayer() const override {trace.push_back("occupant");if(onRead)onRead();return occupant;}
    std::map<int,world::IPlayerControlObject*>* getPcos() override {trace.push_back("pcos");return &pcos;}
};
struct SpotObject : AbilityObject {
    SpotPco* pco=nullptr;
    void* queryInterface(unsigned iid) const override {check(iid==world::IID_IPlayerControlObject,"spot queries IPCO directly");trace.push_back("query");return pco;}
};
struct SpotNetObject : io::NetworkableObjectBase {
    void addRef() override {actionUnsupported("netobj addref");} int getRef() override {actionUnsupported("netobj getref");}
    int release() override {actionUnsupported("netobj release");} void* queryInterface(unsigned) const override {actionUnsupported("netobj query");}
    void predict(float) override {actionUnsupported("netobj predict");}
};
struct SpotNet : io::INetworkable {
    void addRef() override {actionUnsupported("net addRef");} int getRef() override {actionUnsupported("net getRef");}
    int release() override {actionUnsupported("net release");} void* queryInterface(unsigned) const override {actionUnsupported("net query");}
    bool init(void*) override {actionUnsupported("net init");} unsigned getGhostStateMask() const override {actionUnsupported("net mask");}
    void updateStateMask(unsigned*) override {actionUnsupported("net state");}
    bool getNetUpdate(io::BitStream&,io::BaseLineData*,unsigned,int) override {actionUnsupported("net update");}
    void setNetUpdate(io::BitStream&,float,bool,io::BaseLineData*,int) override {actionUnsupported("net set");}
    bool getBaseLine(io::BaseLineData&) override {actionUnsupported("net baseline");}
};
static LSTATUS registryResult=ERROR_FILE_NOT_FOUND;
static unsigned registryReads=0;
static std::function<void()> registryCallback;
static LSTATUS WINAPI spottedRegistry(HKEY root,LPCSTR key,DWORD options,REGSAM access,PHKEY output) {
    check(root==HKEY_LOCAL_MACHINE&&std::string(key)=="SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2142\\InstallDir"&&options==0&&access==KEY_QUERY_VALUE,"retail BF2142 registry query");
    ++registryReads;trace.push_back("registry");*output=reinterpret_cast<HKEY>(123);if(registryCallback)registryCallback();return registryResult;
}
extern "C" { LSTATUS (WINAPI* pSpottedRegistry)(HKEY,LPCSTR,DWORD,REGSAM,PHKEY)=spottedRegistry; }
#pragma comment(linker,"/alternatename:__imp__RegOpenKeyExA@20=_pSpottedRegistry")
int main() {
    if(abilityActionsRegressionMain())return 1;
    std::puts("Spotted fixture: native construction");
    checks=failures=0;trace.clear();
    ActionClock clock;g_system=&clock;
    AbilityActionEvents events;eventManager=&events;Players players;world::playerManager=&players;
    SpotPlayer local,occupant;players.local=&local;SpotGame current,replacement;game=&current;
    SpotMaps maps;io::controlMapManager=&maps;io::networkManager=new io::NetworkManager;
    auto* hud=new SpotHud;hudManager=hud;hud->m_field1e4=0;hud->m_field1e8={10,20,30};
    const auto type=static_cast<world::VehicleType>(3);
    std::puts("Spotted fixture: absent targets and announcements");
    players.local=nullptr;hud->sendSpottedMessage(true,type);check(trace==std::vector<std::string>{"player"},"missing player skips all spotting work");
    std::puts("Spotted fixture: missing player passed");
    players.local=&local;trace.clear();hud->mode=6;hud->sendSpottedMessage(false,type);
    check(trace==std::vector<std::string>{"player","spam","spot","mode","previous","setMode","mouse"},"spot then mode restore then mouse cache");
    check(current.lastPosition.x==10&&current.lastPosition.y==30&&current.lastId==0,"spotting sends X/Z without Y");
    std::puts("Spotted fixture: normal send passed");
    local.spam=true;trace.clear();hud->sendSpottedMessage(true,type);
    check(current.spots==1&&current.radios==1&&trace==std::vector<std::string>{"player","spam","help","text","radio","mouse"},"spam suppresses position event but not announcement");
    std::puts("Spotted fixture: spam announcement passed");
    local.spam=false;hud->m_field1e8={-4000,-4000,-4000};trace.clear();hud->sendSpottedMessage(true,type);
    check(current.spots==1&&std::find(trace.begin(),trace.end(),"spam")==trace.end(),"sentinel skips spam query but still announces");
    hud->m_field1e8.x=std::numeric_limits<float>::quiet_NaN();hud->sendSpottedMessage(false,type);check(current.spots==2,"NaN position differs from sentinel");
    hud->onHelp=[&]{hud->m_field1e4=61;};current.onMessage=[&]{game=&replacement;};hud->sendSpottedMessage(true,type);
    check(current.messageId==61&&replacement.radios==1,"announcement reloads target after help and game after text callback");
    std::puts("Spotted fixture: callback global reload passed");
    hud->onHelp=nullptr;current.onMessage=nullptr;game=&current;
    // Keep the real Object alive: its complete production destruction requires
    // explicit world teardown, which this bounded fixture does not exercise.
    auto& object=*new SpotObject;SpotPco root,first,second,third;object.pco=&root;SpotNetObject netObject;netObject.m_object=&object;SpotNet net;net.m_networkableObject=&netObject;
    std::puts("Spotted fixture: native network lookup and occupied seats");
    io::NetworkableDescriptor descriptor;descriptor.properties.resize(1);descriptor.properties[0].networkables.push_back(&net);
    io::networkManager->m_descriptorHash.insert(61u,&descriptor);
    root.pcos[1]=&first;root.pcos[2]=&second;root.pcos[3]=&third;second.occupant=&occupant;
    first.onRead=[&]{root.pcos.clear();};third.onRead=[]{actionUnsupported("should stop at first occupied seat");};
    occupant.wounded=true;auto sent=current.spots;auto radios=current.radios;trace.clear();hud->sendSpottedMessage(true,type);
    check(registryReads==1&&current.spots==sent&&current.radios==radios&&trace.back()=="mouse","wounded target missing install skips event and radio");
    check(std::count(trace.begin(),trace.end(),"occupant")==3&&root.pcos.empty(),"seat snapshot survives first empty seat clearing source map and stops at next occupant");
    registryResult=ERROR_SUCCESS;root.occupant=&occupant;local.spam=true;trace.clear();hud->sendSpottedMessage(true,type);
    check(registryReads==2&&current.spots==sent+1&&current.radios==radios&&std::find(trace.begin(),trace.end(),"spam")==trace.end(),"installed wounded target bypasses spam and radio");
    occupant.wounded=false;local.spam=false;hud->sendSpottedMessage(false,type);check(current.spots==sent+2,"ordinary occupied target returns to normal path");
    object.pco=nullptr;trace.clear();sent=current.spots;hud->sendSpottedMessage(false,type);
    check(current.spots==sent+1&&std::find(trace.begin(),trace.end(),"query")!=trace.end()&&std::find(trace.begin(),trace.end(),"occupant")==trace.end(),"object without PCO follows ordinary path");
    object.pco=&root;root.occupant=nullptr;trace.clear();sent=current.spots;hud->sendSpottedMessage(false,type);
    check(current.spots==sent+1&&std::find(trace.begin(),trace.end(),"pcos")!=trace.end()&&std::find(trace.begin(),trace.end(),"wounded")==trace.end(),"empty seat map follows ordinary path without wounded query");
    root.pcos[1]=&first;first.onRead=nullptr;trace.clear();sent=current.spots;hud->sendSpottedMessage(false,type);
    check(current.spots==sent+1&&std::count(trace.begin(),trace.end(),"occupant")==2&&std::find(trace.begin(),trace.end(),"wounded")==trace.end(),"all empty seats follow ordinary path");
    net.m_networkableObject=nullptr;trace.clear();sent=current.spots;hud->sendSpottedMessage(false,type);
    check(current.spots==sent+1&&std::find(trace.begin(),trace.end(),"query")==trace.end(),"real network lookup with null networkable object skips query");
    net.m_networkableObject=&netObject;
    hud->m_field1e4=999;hud->m_field1e8={-4000,-4000,-4000};trace.clear();sent=current.spots;hud->sendSpottedMessage(false,type);
    check(current.spots==sent+1&&current.lastId==999&&std::find(trace.begin(),trace.end(),"query")==trace.end(),"missing network descriptor and nonzero id bypass position sentinel");
    hud->m_field1e4=0;hud->mode=19;trace.clear();sent=current.spots;hud->sendSpottedMessage(false,type);
    check(current.spots==sent&&trace==std::vector<std::string>{"player","mode","mouse"},"invalid position without announcement preserves non6 mode and caches mouse");
    for(int coordinate=0;coordinate<3;++coordinate)for(float unusual:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        hud->m_field1e8={-4000,-4000,-4000};
        if(coordinate==0)hud->m_field1e8.x=unusual;
        else if(coordinate==1)hud->m_field1e8.y=unusual;
        else hud->m_field1e8.z=unusual;
        sent=current.spots;
        hud->sendSpottedMessage(false,type);check(current.spots==sent+1,"every unordered/infinite coordinate differs from sentinel");
    }
    local.onSpam=[&]{hud->m_field1e4=77;hud->m_field1e8={71,72,73};game=&replacement;players.local=&occupant;};
    sent=replacement.spots;hud->sendSpottedMessage(false,type);
    check(replacement.spots==sent+1&&replacement.lastId==77&&replacement.lastPosition.x==71&&replacement.lastPosition.y==73&&replacement.lastPlayer==&local,"spam callback reloads target/position/game but preserves initial local player");
    local.onSpam=nullptr;players.local=&local;game=&current;hud->m_field1e4=61;root.occupant=&occupant;occupant.wounded=true;
    registryCallback=[&]{hud->m_field1e4=0;hud->m_field1e8={-4000,-4000,-4000};};
    sent=current.spots;radios=current.radios;trace.clear();hud->sendSpottedMessage(true,type);
    check(current.spots==sent&&current.radios==radios&&std::find(trace.begin(),trace.end(),"spam")==trace.end()&&trace.back()=="mouse","installed wounded target reloads cleared id and invalid position after registry callback");
    hud->m_field1e4=61;registryCallback=[&]{hud->m_field1e4=0;hud->m_field1e8={8,9,10};game=&replacement;};sent=replacement.spots;
    hud->sendSpottedMessage(true,type);
    check(replacement.spots==sent+1&&replacement.lastId==0&&replacement.lastPosition.x==8&&replacement.lastPosition.y==10,"installed wounded target reloads valid position and game after registry callback");
    registryCallback=nullptr;game=&current;
    world::commanderToolsManager=nullptr;hud->m_field35c=true;hud->updateOrderPositionColor(hud->m_field1e8);
    std::puts("Spotted fixture: real shared position validator");
    check(!hud->m_field35c&&hud->m_field350==1&&hud->m_field354==1&&hud->m_field358==1,"missing commander tools resets color without query");
    AbilityClasses classes;classManager=&classes;auto* objects=new AbilityObjects;world::objectManager=objects;
    AbilityActionLogic logic;gameLogic=&logic;world::CommanderToolsManager tools;
    objects->onQuery=[&]{check(!hud->m_field35c&&hud->m_field350==1,"color cleared before shared query callback");};
    hud->updateOrderPositionColor(hud->m_field1e8);check(!hud->m_field35c&&objects->lods==std::vector<int>{1,-1},"valid position leaves white and restores LOD");
    AbilityTemplate objectTemplate;object.setTemplateForTest(&objectTemplate);objects->hits={&object};logic.invalid={{"blocked",true}};
    hud->updateOrderPositionColor(hud->m_field1e8);
    check(hud->m_field35c&&hud->m_field350==0.6f&&hud->m_field354==0.6f&&hud->m_field358==0.6f&&objects->lods.back()==1,"invalid shared query gives gray and keeps retail LOD leak");
    // NetworkManager's actual empty object-map destructor clears its native hash
    // and unregisters the listener. The descriptor above belongs to this fixture
    // and was never inserted into its owning descriptor map.
    delete io::networkManager;io::networkManager=nullptr;
    // Native HUD/query objects are retained to process exit; no missing wider lifecycle is invoked.
    std::printf("HUD spotted/color: %u/%u checks passed\n",checks-failures,checks);return failures?1:0;
}
