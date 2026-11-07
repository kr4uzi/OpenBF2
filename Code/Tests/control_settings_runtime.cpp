// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: CONTROL-SETTINGS-FIXTURE]
// Tests ControlSettings lifetime, defaults, backups, capture, and forwarding with typed device/map observers.
// Real ControlMap assignment runs; the console variant also records default-script execution.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "../BF2/Game/ControlSettings.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "../BF2/IO/Input/Input.h"
#include "../BF2/IO/Input/ControlMap.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/Ids.h"
using namespace dice::hfe;
static unsigned checks, failures;
static void check(bool value,const char* label)
{ ++checks; if(!value){++failures;std::printf("FAIL %s\n",label);} }
static std::vector<std::string> callLog;
static std::string joined(){ std::string s; for(const std::string& e:callLog){ s+=e; s+=';'; } return s; }
static std::string nameOf(const io::IControlMap* map){ return map ? map->getName() : std::string("null"); }

namespace dice { namespace hfe {
// [TEMP] Globals this isolated executable does not link from their owning units.
ClassManager* classManager=nullptr;
EventManager* eventManager=nullptr;
EventManager* frameEventManager=nullptr;
namespace io {
IBase* inputDeviceManager=nullptr; IBase* controlMapManager=nullptr;
#ifndef BF2_CONTROL_CONSOLE_FIXTURE
OldConsole* mainConsole=nullptr;          // the console build links the real definition
#endif
}
struct ControlSettingsFixture {
    static ControlSettings& owner(IBase* base){ return *static_cast<ControlSettings*>(base); }
    static int refs(ControlSettings& c){ return c.m_refCount; }
    static io::IControlMap* map(ControlSettings& c){ return c.m_map; }
};
} }

// [TEMP] A real ControlMap whose reset() and save() report themselves; the base reset() still runs,
// so a copy through ControlMap::operator= logs one "reset <target>" and then really copies.
struct Map : io::ControlMap {
    unsigned resets=0;
    explicit Map(const std::string& name){ setName(name); }
    void reset() override { io::ControlMap::reset(); ++resets; callLog.push_back("reset "+getName()); }
    void save(std::ostream& out,bool player) override { out<<"map "<<getName()<<" player="<<player<<"\n"; }
};
// [TEMP] The control-map manager as ControlSettings sees it: a named table plus the active map.
struct Manager : io::IControlMapManager {
    std::map<std::string,Map*> maps; io::IControlMap* active=nullptr;
    ~Manager(){ for(auto& entry:maps) delete entry.second; }
    Map* add(const std::string& name){ Map* map=new Map(name); maps[name]=map; return map; }
    Map* find(const std::string& name){ auto it=maps.find(name); return it==maps.end()?nullptr:it->second; }
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    void update(float) override { std::abort(); }
    io::IControlMap* create(const std::string& name) override
    {
        callLog.push_back("create "+name);
        auto it=maps.find(name); if(it!=maps.end()){ delete it->second; maps.erase(it); }
        return add(name);
    }
    io::IControlMap* getControlMap(const std::string& name) const override
    { callLog.push_back("get "+name); auto it=maps.find(name); return it==maps.end()?nullptr:it->second; }
    io::IControlMap* getControlMap(int) const override { std::abort(); }
    void deleteControlMap(const std::string& name) override
    { callLog.push_back("delete "+name); auto it=maps.find(name); if(it!=maps.end()){ delete it->second; maps.erase(it); } }
    void deleteControlMap(int) override { std::abort(); }
    void deleteControlMap(const io::IControlMap* map) override
    {
        callLog.push_back("deleteByPointer "+nameOf(map));
        for(auto it=maps.begin();it!=maps.end();++it) if(it->second==map){ delete it->second; maps.erase(it); return; }
    }
    void setActive(const std::string&) override { std::abort(); }
    void setActive(const io::IControlMap* map) override { callLog.push_back("setActive "+nameOf(map)); active=const_cast<io::IControlMap*>(map); }
    io::IControlMap* getActive() const override { callLog.push_back("getActive"); return active; }
    int mapNextInput(io::IControlMap* map,int id,bool a,bool b,bool c,bool d,std::string& out) override
    {
        char buffer[128]; std::snprintf(buffer,sizeof buffer,"mapNextInput %s %d %d %d %d %d",nameOf(map).c_str(),id,a,b,c,d);
        callLog.push_back(buffer); out="KEY_X"; return 2;
    }
    float getRepeatRate(int,bool) override { std::abort(); }
    void applyActive() override { std::abort(); }
};
// [TEMP] The device manager and its mouse.
struct Mouse : io::IInputDevice {
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    io::InputDeviceTypes getType() const override { return io::IDTMouse; }
    bool setCooperativeMode(io::InputDeviceMode) override { std::abort(); }
    io::InputDeviceMode getCooperativeMode() const override { std::abort(); }
    bool activate() override { callLog.push_back("activate mouse"); return true; }
    bool deActivate() override { callLog.push_back("deActivate mouse"); return true; }
    void update(float) override { std::abort(); }
    bool isActive() const override { std::abort(); }
    bool getNextInput(io::InputDeviceAxes&,io::InputDeviceButtons&,io::InputDeviceKeys&,bool&) override { std::abort(); }
};
struct Devices : io::IInputDeviceManager {
    Mouse mouse;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    bool initDevices(unsigned int*) override { std::abort(); }
    io::IInputDevice* getInputDevice(io::InputDeviceFlags flags) const override
    { char buffer[48]; std::snprintf(buffer,sizeof buffer,"getInputDevice %d",static_cast<int>(flags)); callLog.push_back(buffer); return const_cast<Mouse*>(&mouse); }
    unsigned int getInputDevices() const override { std::abort(); }
    bool activateDevices(unsigned int) override { std::abort(); }
    bool deActivateDevices(unsigned int) override { std::abort(); }
    void update(float dt) override { char buffer[48]; std::snprintf(buffer,sizeof buffer,"update %g",dt); callLog.push_back(buffer); }
    unsigned int getActiveInputDevices() const override { std::abort(); }
    void* getAllDevices() override { std::abort(); }
};
// [TEMP] The frame event manager: only the id/data/delay postEvent is expected.
struct FrameEvents : EventManager {
    unsigned int postEvent(EventCategory category,unsigned int id,void* data,float delay) override
    {
        char buffer[64]; std::snprintf(buffer,sizeof buffer,"postEvent %d %u %s %g",static_cast<int>(category),id,data?"data":"null",delay);
        callLog.push_back(buffer); return 1;
    }
};
#ifdef BF2_CONTROL_CONSOLE_FIXTURE
// [TEMP] The real console with its run observed; only the console build links its constructor.
struct ConsoleObserver : io::OldConsole {
    bool run(const std::string& file,const std::string&,const std::string&,const std::string&,const std::string&,
             const std::string&,const std::string&,const std::string&,const std::string&,std::string&) override
    { callLog.push_back("run "+file); return true; }
};
#endif

static const char* const names[7]={"InfantryPlayerInputControlMap","LandPlayerInputControlMap","AirPlayerInputControlMap",
    "HelicopterPlayerInputControlMap","SeaPlayerInputControlMap","defaultGameControlMap","defaultPlayerInputControlMap"};
static bool allPresent=false;
static bool present(int index){ return allPresent || index==0 || index==1 || index==5 || index==6; }
// backupControlMaps / apply / setDefaults' second loop: the copy's own reset() is what the log shows.
static std::string backupSequence(const char* active)
{
    std::string s="getActive;";
    for(int i=0;i<7;++i){ std::string n=names[i]; s+="delete _"+n+";create _"+n+";get "+n+";get _"+n+";"; if(present(i)) s+="reset _"+n+";"; }
    return s+"setActive "+active+";";
}

int main(int argc,char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    check(sizeof(void*)==4,"x86 fixture");
    Manager manager; io::controlMapManager=&manager;
    Devices devices; io::inputDeviceManager=&devices;
    FrameEvents frame; frameEventManager=&frame;
    Map* infantry=manager.add(names[0]); Map* land=manager.add(names[1]); Map* game=manager.add(names[5]); Map* player=manager.add(names[6]);
    (void)argc; (void)argv;

    // lifetime and identity
    IBase* object=ControlSettings::create(IID_IControlSettings,nullptr);
    check(object!=nullptr,"factory yields the owner for its interface id");
    IControlSettings* api=static_cast<IControlSettings*>(object);
    ControlSettings& owner=ControlSettingsFixture::owner(object);
    check(ControlSettingsFixture::refs(owner)==1 && api->getRef()==1,"refcount starts at one and queryInterface does not retain");
    check(api->queryInterface(IID_IBase)==object && api->queryInterface(IID_IControlSettings)==object && !api->queryInterface(0xffffffff),
        "queryInterface accepts IBase and IControlSettings only");
    check(!ControlSettings::create(IID_IControlSettings,reinterpret_cast<IBase*>(1)),"aggregation is refused before allocation");
    check(!ControlSettings::create(0xffffffff,nullptr),"unsupported factory id releases the fresh owner");
    check(api->getFileName()=="Controls.con","actual filename virtual");

    // constructed state and the unselected forwards
    check(!api->isMappingInput() && api->getMappedInputName().empty() && !ControlSettingsFixture::map(owner),"flag clear, name empty, no selected map");
    check(api->getMouseSensitivity()==1.0f && api->getYawFactor()==1.0f && api->getPitchFactor()==1.0f && !api->getInvertMouse()
        && api->getKeyboardSensitivity()==1.0f && api->getSlot48()==1.0f && api->getSlot50()==0.0f,"getter fallbacks without a selected map");
    api->setMouseSensitivity(2.0f); api->setYawFactor(2.0f); api->setPitchFactor(2.0f); api->setKeyboardSensitivity(2.0f); api->setSlot4C(9.0f); api->setSlot54(9.0f);
    check(api->getMouseSensitivity()==1.0f && api->getYawFactor()==1.0f && api->getPitchFactor()==1.0f && api->getKeyboardSensitivity()==1.0f
        && api->getSlot48()==1.0f && api->getSlot50()==0.0f,"setters without a selected map are inert (setInvertMouse excluded: retail forwards it unguarded)");
    check(api->getActiveControlMapName()=="","no active map: empty name");
    manager.active=infantry;
    check(api->getActiveControlMapName()=="InfantryPlayerInputControlMap","the active map's name");

    // selection and forwarding
    callLog.clear();
    check(api->selectControlMap(std::string("LandPlayerInputControlMap")) && ControlSettingsFixture::map(owner)==land && joined()=="get LandPlayerInputControlMap;",
        "selectControlMap looks the map up by name and keeps it");
    api->setMouseSensitivity(2.5f); api->setYawFactor(0.5f); api->setPitchFactor(0.25f); api->setInvertMouse(true); api->setKeyboardSensitivity(3.0f);
    check(land->getMouseSensitivity()==2.5f && api->getMouseSensitivity()==2.5f && api->getYawFactor()==0.5f && api->getPitchFactor()==0.25f
        && api->getInvertMouse() && api->getKeyboardSensitivity()==3.0f,"the feel accessors forward to the selected map");
    check(api->selectControlMap(std::string("missing")) && !ControlSettingsFixture::map(owner),"selecting an absent map clears the selection and still returns true");
    api->selectControlMap(std::string("LandPlayerInputControlMap"));

    // script
    callLog.clear();
    check(api->getSettingsScript()=="map InfantryPlayerInputControlMap player=1\nmap LandPlayerInputControlMap player=1\n"
        "map defaultGameControlMap player=0\nmap defaultPlayerInputControlMap player=1\n","script: the present maps in table order; the game map is told it is not a player map");
    { std::string gets; for(int i=0;i<7;++i) gets+=std::string("get ")+names[i]+";"; check(joined()==gets,"the script asks for all seven names in order"); }

    // backups: +1C and +78 with three live maps absent
    callLog.clear();
    api->backupControlMaps();
    check(joined()==backupSequence("InfantryPlayerInputControlMap"),"backupControlMaps: active read first, delete/create/get/get per name, the copy (its reset) only when both exist, active restored");
    check(manager.maps.size()==11 && manager.active==infantry,"seven backups exist, empty ones for the absent live maps");
    Map* landBackup=manager.find("_LandPlayerInputControlMap");
    check(landBackup && landBackup->getMouseSensitivity()==2.5f && landBackup->getInvertMouse() && landBackup->getYawFactor()==0.5f
        && landBackup->getName()=="_LandPlayerInputControlMap","the backup really copied the live map's feel scalars and kept its own name");
    callLog.clear();
    check(api->apply() && joined()==backupSequence("InfantryPlayerInputControlMap"),"apply backs up through the table and returns true");

    // restore, backup by name and setDefaults need every live map: retail assigns with no null tests
    Map* air=manager.add(names[2]); Map* heli=manager.add(names[3]); Map* sea=manager.add(names[4]); allPresent=true;
    (void)air; (void)heli; (void)sea; (void)game; (void)player;
    callLog.clear();
    api->backupControlMaps();
    check(joined()==backupSequence("InfantryPlayerInputControlMap") && manager.maps.size()==14,"with every live map present each backup is copied");
    land->setMouseSensitivity(4.0f);
    callLog.clear();
    check(api->restoreControlMaps(),"restoreControlMaps returns true");
    { std::string s; for(int i=0;i<7;++i){ std::string n=names[i]; s+="get "+n+";reset "+n+";get _"+n+";reset "+n+";"; }
      check(joined()==s,"restore: each live map reset, then assigned from its backup (the copy resets it again)"); }
    check(land->getMouseSensitivity()==2.5f && land->getInvertMouse(),"the live map took the backup's values back");
    callLog.clear();
    api->backupControlMapsByName();
    { std::string s="getActive;getActive;"; for(int i=0;i<7;++i){ std::string n=names[i]; s+="delete _"+n+";create _"+n+";get _"+n+";get "+n+";reset _"+n+";"; }
      s+="get InfantryPlayerInputControlMap;setActive InfantryPlayerInputControlMap;";
      check(joined()==s,"backupControlMapsByName: name kept, backup then live looked up, copied, re-selected by name"); }
    manager.active=nullptr; callLog.clear();
    api->backupControlMapsByName();
    check(joined().find("getActive;delete")==0 && joined().find("setActive")==std::string::npos,"no active map: nothing is re-selected");
    manager.active=infantry; callLog.clear();
    api->resetControlMaps();
    { std::string s; for(int i=0;i<7;++i){ std::string n=names[i]; s+="get "+n+";reset "+n+";"; } check(joined()==s,"resetControlMaps resets every live map"); }
    callLog.clear();
    api->deleteControlMapBackups();
    { std::string s; for(int i=0;i<7;++i){ std::string n=names[i]; s+="get _"+n+";deleteByPointer _"+n+";"; } check(joined()==s && manager.maps.size()==7,"the seven backups are deleted by pointer"); }
    callLog.clear();
    api->deleteControlMapBackups();
    { std::string s; for(int i=0;i<7;++i){ std::string n=names[i]; s+="get _"+n+";deleteByPointer null;"; } check(joined()==s,"absent backups are still handed over as null"); }

    // capture
    api->selectControlMap(std::string("InfantryPlayerInputControlMap")); callLog.clear();
    check(api->mapNextInput(8,true,0,false,false)==2,"mapNextInput returns the manager's full int answer");
    check(joined()=="mapNextInput InfantryPlayerInputControlMap 8 1 0 0 1;postEvent 10 12 null 2;","trigger input: alternate passed, not an axis, player input, event (10,12) after two seconds");
    check(api->isMappingInput() && api->getMappedInputName()=="KEY_X","the flag is raised and the manager's name kept");
    callLog.clear(); api->mapNextInput(3,false,0,true,false);
    check(joined()=="mapNextInput InfantryPlayerInputControlMap 3 0 1 1 1;postEvent 10 12 null 2;","PIThrottle is an axis; second passed");
    callLog.clear(); api->mapNextInput(3,false,0,false,true);
    check(joined()=="mapNextInput InfantryPlayerInputControlMap 3 0 0 0 0;postEvent 10 12 null 2;","game input: never classified as an axis, player flag off");
    api->setMappingInput(false);
    check(!api->isMappingInput(),"setMappingInput clears the flag");

    // devices
    callLog.clear(); api->updateInputDevices();
    check(joined()=="update -1;update 0.1;","updateInputDevices: -1.0f then 0.1f");
    callLog.clear(); api->activateMouse(); api->deactivateMouse();
    check(joined()=="getInputDevice 1;activate mouse;getInputDevice 1;deActivate mouse;","the mouse (flag 1) is activated and deactivated through the device manager");

    // Binding descriptions.
    {
        IBase* second=ControlSettings::create(IID_IControlSettings,nullptr);
        IControlSettings* cs=static_cast<IControlSettings*>(second);
        std::string key; std::wstring text(L"untouched");
        check(!cs->getBindingDescription(nullptr,8,false,false,false,key,text),"no map and no selection: false");
        // A keyboard trigger: no device prefix, "CONTROLLER_" + the key's name; the keyboard device is
        // fetched (and dropped) before anything else.
        infantry->addTriggerMapping(8,io::IDFKeyboard,io::IDKey_X,0.0f,false);
        callLog.clear(); key="stale";
        check(cs->getBindingDescription(infantry,8,false,false,false,key,text) && key=="CONTROLLER_IDKey_X","keyboard trigger: CONTROLLER_IDKey_X");
        check(joined()=="getInputDevice 2;","the keyboard device is asked for, and nothing else");
        check(text==L"untouched","text is never written");
        // Unbound: false, and the key is the (empty) stream.
        key="stale";
        check(!cs->getBindingDescription(infantry,9,false,false,false,key,text) && key.empty(),"unbound trigger: false, empty key");
        // A mouse-button trigger: the device prefix, "_", the button's name.
        infantry->addTriggerMapping(9,io::IDFMouse,io::IDButton_0,0.0f,false);
        check(cs->getBindingDescription(infantry,9,false,false,false,key,text) && key=="CONTROLLER_IDFMouse_IDButton_0","mouse button trigger");
        // An axis player input on two keys: `second` selects the negative key (invert clear).
        infantry->addAxisMapping(3,io::IDFKeyboard,io::IDKey_W,io::IDKey_S,false);
        check(cs->getBindingDescription(infantry,3,false,false,false,key,text) && key=="CONTROLLER_IDKey_W","axis keys: the positive side");
        check(cs->getBindingDescription(infantry,3,false,true,false,key,text) && key=="CONTROLLER_IDKey_S","axis keys: second = the negative side");
        // The same id as a game input goes to the trigger map, where 3 is unbound.
        check(!cs->getBindingDescription(infantry,3,false,false,true,key,text) && key.empty(),"game input 3: the trigger map, unbound");
        // The selected map when none is passed.
        cs->selectControlMap(std::string("InfantryPlayerInputControlMap"));
        check(cs->getBindingDescription(nullptr,8,false,false,false,key,text) && key=="CONTROLLER_IDKey_X","the selected map when none is passed");
        check(cs->release()==0,"second owner released");
    }

#ifdef BF2_CONTROL_CONSOLE_FIXTURE
    // setDefaults with the real console
    ConsoleObserver console; io::mainConsole=&console;
    manager.active=infantry; callLog.clear();
    check(api->setDefaults(),"setDefaults returns true");
    { std::string s="getActive;"; for(int i=0;i<7;++i){ std::string n=names[i]; s+="get "+n+";reset "+n+";"; }
      s+="run Settings/Controls.con;";
      for(int i=0;i<7;++i){ std::string n=names[i]; s+="delete _"+n+";create _"+n+";get "+n+";get _"+n+";reset _"+n+";"; }
      s+="setActive InfantryPlayerInputControlMap;";
      check(joined()==s,"setDefaults: reset every live map, run Settings/Controls.con, rebuild the backups, restore the active map"); }
    io::mainConsole=nullptr;
#endif

    // release
    api->addRef();
    check(api->release()==1,"non-final release keeps the owner");
    check(api->release()==0,"final release reports zero");
    std::printf("ControlSettings: %u/%u passed; native owner=%zu\n",checks-failures,checks,sizeof(ControlSettings));
    return failures?1:0;
}
