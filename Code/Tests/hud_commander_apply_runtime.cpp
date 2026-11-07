// [TEMP] Actual commanderApply/cacheMouseInput with complete typed collaborators.
// Includes the independent camera regression. No tested HUD body is replaced.
#define main cameraRegressionMain
#include "hud_camera_runtime.cpp"
#undef main
#include <float.h>
#include <limits>
#include "../BF2/Core/System.h"
#include "../BF2/IO/Input/Input.h"
namespace dice { namespace hfe {
System* g_system=nullptr;
namespace io { IBase* controlMapManager=nullptr; }
} }
using namespace dice::hfe::io;
struct ApplyPlayer : Player {
    bool commander=false;
    double last=0,cooldown=0;
    mutable int lastReads=0,teamReads=0;
    int secondTeam=3;
    bool getIsCommander() const override {trace+='C';return commander;}
    double getLastTimeCommander() const override {trace+='H';++lastReads;return last;}
    double getCommanderReapplyWaitTime() const override {trace+='W';return cooldown;}
    int getTeam() const override {trace+='T';return teamReads++?secondTeam:team;}
};
struct ApplySystem : System {
    double now=0;
    int reads=0;
    double getExactTime() override {trace+='X';++reads;return now;}
};
struct ApplyLogic : Logic {
    std::string name="uSMC";
    int namedTeam=0;
    const std::string& getTeamName(int team) override {trace+='N';namedTeam=team;return name;}
};
struct ApplySquads : Squads {
    int applications=0,appliedTeam=0,appliedId=0;
    void applyForCommander(int team,int id) override {
        trace+='A';++applications;appliedTeam=team;appliedId=id;
    }
};
struct ApplyHud : BF2HudManager {
    std::wstring message;
    std::vector<std::string> keys;
    std::vector<int> enabled,disabled;
    char enableTag='E',disableTag='F';
    std::function<void()> onEnable;
    void enableVOHelpMessage(int id) override {trace+=enableTag;enabled.push_back(id);if(onEnable)onEnable();}
    void disableVOHelpMessage(int id) override {trace+=disableTag;disabled.push_back(id);}
    std::wstring getLocalizedString(std::string key) override {
        trace+='K';keys.push_back(key);
        if(key=="HUD_TEAMNAME_usmc")return std::wstring(L"Team\0discard",12);
        if(key=="HUD_HUD_COMMANDER_APPLIED")return L"Applied #TEAMNAME# then #TEAMNAME#";
        if(key=="HUD_HUD_COMMANDER_CANNOT_APPLY_FOR_MINUTE")return L"Wait #MINUTE# minute";
        if(key=="HUD_HUD_COMMANDER_CANNOT_APPLY_FOR_MINUTES")return L"Wait #MINUTES# minutes";
        actionUnsupported("apply localization key");
    }
    void addServerChat(std::wstring text,int team) override {
        trace+='S';check(team==0,"apply server chat team zero");message=text;
    }
    void setMode(int mode,bool save) override {trace+='M';check(mode==0&&save,"apply restores mode zero with save");}
};
struct MouseMap : IControlMap {
    bool success=true,lastButton=false;
    PlayerInputMap result=PIFire;
    int initial=-1,calls=0;
    bool getInputMappedToMouseButton(bool button,PlayerInputMap& out) override {
        trace+='B';++calls;lastButton=button;initial=out;out=result;return success;
    }
    void addRef() override {actionUnsupported("map addRef");}
    int getRef() override {actionUnsupported("map getRef");}
    int release() override {actionUnsupported("map release");}
    void* queryInterface(unsigned) const override {actionUnsupported("map queryInterface");}
    void setName(const std::string&) override {actionUnsupported("map setName");}
    const std::string& getName() const override {actionUnsupported("map getName");}
    int getID() const override {actionUnsupported("map getID");}
    void setID(int) override {actionUnsupported("map setID");}
    bool axisExists(int) const override {actionUnsupported("map axisExists");}
    bool triggerExists(int) const override {actionUnsupported("map triggerExists");}
    void addAxisMapping(int,int,InputDeviceFlags,InputDeviceAxes,bool,bool) override {actionUnsupported("map addAxisMapping");}
    void addAxisMapping(int,InputDeviceFlags,InputDeviceAxes,bool,bool) override {actionUnsupported("map addAxisMapping");}
    void addAxisMapping(int,InputDeviceFlags,InputDeviceButtons,InputDeviceButtons,bool) override {actionUnsupported("map addAxisMapping");}
    void addAxisMapping(int,InputDeviceFlags,InputDeviceKeys,InputDeviceKeys,bool) override {actionUnsupported("map addAxisMapping");}
    void addAxisMapping(int,int,InputDeviceFlags,InputDevicePOVs,bool,bool,bool) override {actionUnsupported("map addAxisMapping");}
    void addAxisMapping(int,InputDeviceFlags,InputDeviceButtons,InputDeviceButtons,InputDeviceKeys,bool) override {actionUnsupported("map addAxisMapping");}
    void addAxisMapping(int,InputDeviceFlags,InputDeviceKeys,InputDeviceKeys,InputDeviceButtons,bool) override {actionUnsupported("map addAxisMapping");}
    void addTriggerMapping(int,InputDeviceFlags,InputDeviceButtons,float,bool) override {actionUnsupported("map addTriggerMapping");}
    void addTriggerMapping(int,InputDeviceFlags,InputDeviceKeys,float,bool) override {actionUnsupported("map addTriggerMapping");}
    void addTriggerMapping(int,int,InputDeviceFlags,InputDeviceAxes,bool,float,float) override {actionUnsupported("map addTriggerMapping");}
    void removeMapping(unsigned) override {actionUnsupported("map removeMapping");}
    void reset() override {actionUnsupported("map reset");}
    void reset(int,bool,bool,bool) override {actionUnsupported("map reset");}
    void setButtonRiseTime(float) override {actionUnsupported("map setButtonRiseTime");}
    void setButtonFallTime(float) override {actionUnsupported("map setButtonFallTime");}
    void setDoubleTapTime(float) override {actionUnsupported("map setDoubleTapTime");}
    float getAxisValue(int) const override {actionUnsupported("map getAxisValue");}
    bool isAxisValueAnalogue(int) const override {actionUnsupported("map isAxisValueAnalogue");}
    int getTriggerState(int) const override {actionUnsupported("map getTriggerState");}
    float getTriggerSampleLatency(int) const override {actionUnsupported("map getTriggerSampleLatency");}
    bool validate() override {actionUnsupported("map validate");}
    void update(float) override {actionUnsupported("map update");}
    void setInvertMouse(bool) override {actionUnsupported("map setInvertMouse");}
    bool getInvertMouse() override {actionUnsupported("map getInvertMouse");}
    void setMouseSensitivity(float) override {actionUnsupported("map setMouseSensitivity");}
    float getMouseSensitivity() override {actionUnsupported("map getMouseSensitivity");}
    void setKeyboardSensitivity(float) override {actionUnsupported("map setKeyboardSensitivity");}
    float getKeyboardSensitivity() override {actionUnsupported("map getKeyboardSensitivity");}
    void setAxisScale(InputDeviceAxes,float) override {actionUnsupported("map setAxisScale");}
    float getAxisScale(InputDeviceAxes) const override {actionUnsupported("map getAxisScale");}
    void setYawFactor(float) override {actionUnsupported("map setYawFactor");}
    void setPitchFactor(float) override {actionUnsupported("map setPitchFactor");}
    float getYawFactor() override {actionUnsupported("map getYawFactor");}
    float getPitchFactor() override {actionUnsupported("map getPitchFactor");}
    void save(std::ostream&,bool) override {actionUnsupported("map save");}
    void dump(bool) override {actionUnsupported("map dump");}
    bool findDuplicate(InputDeviceFlags&,InputDeviceAxes&,InputDeviceButtons&,InputDeviceKeys&,DuplicateKeyVector&) override {actionUnsupported("map findDuplicate");}
};
struct MouseMaps : IControlMapManager {
    IControlMap* active=nullptr;
    IControlMap* getActive() const override {trace+='V';return active;}
    void addRef() override {actionUnsupported("maps addRef");}
    int getRef() override {actionUnsupported("maps getRef");}
    int release() override {actionUnsupported("maps release");}
    void* queryInterface(unsigned) const override {actionUnsupported("maps queryInterface");}
    void update(float) override {actionUnsupported("maps update");}
    IControlMap* create(const std::string&) override {actionUnsupported("maps create");}
    IControlMap* getControlMap(const std::string&) const override {actionUnsupported("maps lookup");}
    IControlMap* getControlMap(int) const override {actionUnsupported("maps lookup");}
    void deleteControlMap(const std::string&) override {actionUnsupported("maps delete");}
    void deleteControlMap(int) override {actionUnsupported("maps delete");}
    void deleteControlMap(const IControlMap*) override {actionUnsupported("maps delete");}
    void setActive(const std::string&) override {actionUnsupported("maps activate");}
    void setActive(const IControlMap*) override {actionUnsupported("maps activate");}
    int mapNextInput(IControlMap*,int,bool,bool,bool,bool,std::string&) override {actionUnsupported("maps next");}
    float getRepeatRate(int,bool) override {actionUnsupported("maps repeat");}
    void applyActive() override {actionUnsupported("maps apply");}
};
static void destroyApplyHud(ApplyHud* hud) {
    hud->keys.~vector();hud->enabled.~vector();hud->disabled.~vector();
    hud->message.~basic_string();hud->onEnable.~function();
    delete hud->m_field2dc;
    using W=std::wstring;using S=std::string;using L=std::list<int>;using SL=std::list<std::string>;using V=std::vector<dice::hfe::hud::HudPendingRequest>;
    hud->m_field010.~W();hud->m_field038.~W();hud->m_field060.~W();
    hud->m_field08c.~S();hud->m_field0c0.~S();hud->m_field304.~S();
    hud->m_field0e8.~L();hud->m_field2e0.~SL();hud->m_field33c.~SL();hud->m_field32c.~V();
    ::operator delete(hud);
}
static unsigned short readControl(){unsigned short result;__asm {fnstcw result} return result;}
static void writeControl(unsigned short value){__asm {fldcw value}}
int main() {
    if(cameraRegressionMain())return 1;
    checks=failures=0;trace.clear();
    Players players;ApplyPlayer player;ApplySystem system;ApplyLogic logic;ApplySquads squads;
    MouseMap map;MouseMaps maps;maps.active=&map;
    world::playerManager=&players;g_system=&system;gameLogic=&logic;squadManager=&squads;controlMapManager=&maps;
    auto* hud=new ApplyHud;auto* other=new ApplyHud;hudManager=hud;
    other->enableTag='e';other->disableTag='f';
    trace.clear();hud->commanderApply();check(trace=="P","missing player has no downstream calls");
    players.local=&player;player.commander=true;
    trace.clear();hud->commanderApply();check(trace=="PC","existing commander has no downstream calls");
    player.commander=false;hud->onEnable=[&]{hudManager=other;};
    trace.clear();hud->commanderApply();
    check(trace=="PCHWXEfDTAXHTNKKSMVBe","allowed path preserves complete callback ordering");
    check(squads.applications==1&&squads.appliedTeam==2&&squads.appliedId==17,"application forwards first team and player ID");
    check(player.lastReads==2&&system.reads==2&&player.teamReads==2&&logic.namedTeam==3,"allowed path repeats clock last-time and team reads");
    check(hud->keys==std::vector<std::string>{"HUD_TEAMNAME_usmc","HUD_HUD_COMMANDER_APPLIED"},"lowercase team key is localized before applied message");
    check(hud->message==L"Applied Team then #TEAMNAME#","team text is copied through c_str and first token only is replaced");
    check(hud->enabled==std::vector<int>{22}&&other->disabled==std::vector<int>{23}&&other->enabled==std::vector<int>{22},"help global is reread after initial callback and at end");
    check(hud->m_field328==PIFire&&map.lastButton&&map.initial==1,"apply caches true mouse button after mode callback");
    hud->onEnable=nullptr;hudManager=hud;
    const unsigned short original=readControl();
    struct Case {double wait;const wchar_t* expected;};
    const Case cases[]={{0.1,L"Wait 1 minute"},{1.0,L"Wait 1 minute"},{1.00001,L"Wait 2 minutes"},
        {2.25,L"Wait 3 minutes"},{std::numeric_limits<double>::quiet_NaN(),L"Wait -2147483648 minute"},
        {std::numeric_limits<double>::infinity(),L"Wait -2147483648 minute"},
        {2147483648.0,L"Wait -2147483648 minute"}};
    for(unsigned rounding=0;rounding<4;++rounding) {
        const unsigned short chosen=static_cast<unsigned short>((original&~0x0c00u)|(rounding<<10));
        writeControl(chosen);
        for(const auto& c:cases) {
            player.cooldown=c.wait;player.lastReads=player.teamReads=0;system.reads=0;
            const int before=squads.applications;hud->keys.clear();trace.clear();
            hud->commanderApply();
            check(trace=="PCHWXKSMVBE","denied path ordering excludes application and extra time reads");
            check(hud->message==c.expected,"denied ceil signed integer and singular/plural result");
            check(squads.applications==before&&player.lastReads==1&&system.reads==1,"denied path submits nothing");
            check(readControl()==chosen,"caller x87 control word restored after conversion");
        }
    }
    writeControl(original);
    player.last=100;system.now=220;player.cooldown=3.25;trace.clear();hud->commanderApply();
    check(hud->message==L"Wait 2 minutes","elapsed seconds since last command are converted to minutes before ceil");
    player.last=0;system.now=0;
    player.cooldown=-1;trace.clear();const int applications=squads.applications;hud->commanderApply();
    check(squads.applications==applications+1,"negative remaining cooldown allows application");
    player.cooldown=0.5;maps.active=nullptr;hud->m_field328=123;trace.clear();hud->commanderApply();
    check(trace=="PCHWXKSMVE"&&hud->m_field328==123,"no active map preserves cached input");
    maps.active=&map;map.success=false;map.result=PIUse;trace.clear();hud->commanderApply();
    check(trace=="PCHWXKSMVBE"&&hud->m_field328==123&&map.lastButton&&map.initial==1,"failed mapping preserves cache despite out write");
    map.success=true;trace.clear();hud->commanderApply();
    check(trace=="PCHWXKSMVBE"&&hud->m_field328==PIUse,"successful mapping stores returned input");
    destroyApplyHud(other);destroyApplyHud(hud);hudManager=nullptr;
    std::printf("HUD commander apply: %u/%u checks passed\n",checks-failures,checks);
    return failures?1:0;
}
