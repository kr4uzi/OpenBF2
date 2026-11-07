// [TEMP] [CHOSEN, not recovered] Controlled COM and control-map fixture.
// No MouseDevice virtual body is replaced. Unmodeled fixture calls abort.
#include <windows.h>
#include <list>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <new>
#define private public
#include "../BF2/IO/Input/MouseDevice.h"
#undef private
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/Debug.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
using namespace dice::hfe;
using namespace dice::hfe::io;
namespace dice { namespace hfe {
BF2Render* g_bf2Render = 0;
namespace io { IBase* inputDeviceManager = 0; IBase* controlMapManager = 0; }
} }
[[noreturn]] void unexpected() { std::fprintf(stderr,"Unexpected mouse fixture dependency\n"); std::abort(); }
int checks = 0, failures = 0;
void check(bool ok, const char* name) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n",name); } }
bool nearFloat(float a,float b) { return std::fabs(a-b) < 0.00001f; }
struct FixtureMap : IControlMap {
    float scales[4] = {2,3,4,5};
    float sensitivity = 0.5f;
    bool invert = true;
    mutable std::vector<int> calls;
    void addRef() override { unexpected(); }
    int getRef() override { unexpected(); }
    int release() override { unexpected(); }
    void* queryInterface(unsigned int) const override { unexpected(); }
    void setName(const std::string& name) override { unexpected(); }
    const std::string& getName() const override { unexpected(); }
    int getID() const override { unexpected(); }
    void setID(int id) override { unexpected(); }
    bool axisExists(int axisId) const override { unexpected(); }
    bool triggerExists(int triggerId) const override { unexpected(); }
    void addAxisMapping(int axisId, int secondaryId, InputDeviceFlags device, InputDeviceAxes axis, bool a, bool b) override { unexpected(); }
    void addAxisMapping(int axisId, InputDeviceFlags device, InputDeviceAxes axis, bool a, bool b) override { unexpected(); }
    void addAxisMapping(int axisId, InputDeviceFlags device, InputDeviceButtons negative, InputDeviceButtons positive, bool a) override { unexpected(); }
    void addAxisMapping(int axisId, InputDeviceFlags device, InputDeviceKeys negative, InputDeviceKeys positive, bool a) override { unexpected(); }
    void addAxisMapping(int axisId, int secondaryId, InputDeviceFlags device, InputDevicePOVs pov, bool a, bool b, bool c) override { unexpected(); }
    void addAxisMapping(int axisId, InputDeviceFlags device, InputDeviceButtons button, InputDeviceButtons button2, InputDeviceKeys key, bool a) override { unexpected(); }
    void addAxisMapping(int axisId, InputDeviceFlags device, InputDeviceKeys key, InputDeviceKeys key2, InputDeviceButtons button, bool a) override { unexpected(); }
    void addTriggerMapping(int triggerId, InputDeviceFlags device, InputDeviceButtons button, float f, bool b) override { unexpected(); }
    void addTriggerMapping(int triggerId, InputDeviceFlags device, InputDeviceKeys key, float f, bool b) override { unexpected(); }
    void addTriggerMapping(int triggerId, int secondaryId, InputDeviceFlags device, InputDeviceAxes axis, bool b, float f, float g) override { unexpected(); }
    void removeMapping(unsigned int id) override { unexpected(); }
    void reset() override { unexpected(); }
    void reset(int id, bool a, bool b, bool c) override { unexpected(); }
    void setButtonRiseTime(float t) override { unexpected(); }
    void setButtonFallTime(float t) override { unexpected(); }
    void setDoubleTapTime(float t) override { unexpected(); }
    float getAxisValue(int axisId) const override { unexpected(); }
    bool isAxisValueAnalogue(int axisId) const override { unexpected(); }
    int getTriggerState(int triggerId) const override { unexpected(); }
    float getTriggerSampleLatency(int triggerId) const override { unexpected(); }
    bool validate() override { unexpected(); }
    void update(float deltaTime) override { unexpected(); }
    void setInvertMouse(bool invert) override { unexpected(); }
    bool getInvertMouse() override { calls.push_back(11); return invert; }
    void setMouseSensitivity(float s) override { unexpected(); }
    float getMouseSensitivity() override { calls.push_back(10); return sensitivity; }
    void setKeyboardSensitivity(float s) override { unexpected(); }
    float getKeyboardSensitivity() override { unexpected(); }
    void setAxisScale(InputDeviceAxes axis, float scale) override { unexpected(); }
    float getAxisScale(InputDeviceAxes axis) const override { calls.push_back(static_cast<int>(axis)); return scales[axis]; }
    void setYawFactor(float f) override { unexpected(); }
    void setPitchFactor(float f) override { unexpected(); }
    float getYawFactor() override { unexpected(); }
    float getPitchFactor() override { unexpected(); }
    void save(std::ostream& out, bool b) override { unexpected(); }
    void dump(bool b) override { unexpected(); }
    bool findDuplicate(InputDeviceFlags& device, InputDeviceAxes& axis, InputDeviceButtons& button, InputDeviceKeys& key, DuplicateKeyVector& out) override { unexpected(); }
    bool getInputMappedToMouseButton(bool b, PlayerInputMap& out) override { unexpected(); }
};

struct FixtureManager : IControlMapManager {
    IControlMap* map;
    mutable int calls = 0;
    void addRef() override { unexpected(); }
    int getRef() override { unexpected(); }
    int release() override { unexpected(); }
    void* queryInterface(unsigned int) const override { unexpected(); }
    void update(float deltaTime) override { unexpected(); }
    IControlMap* create(const std::string& name) override { unexpected(); }
    IControlMap* getControlMap(const std::string& name) const override { unexpected(); }
    IControlMap* getControlMap(int id) const override { unexpected(); }
    void deleteControlMap(const std::string& name) override { unexpected(); }
    void deleteControlMap(int id) override { unexpected(); }
    void deleteControlMap(const IControlMap* map) override { unexpected(); }
    void setActive(const std::string& name) override { unexpected(); }
    void setActive(const IControlMap* map) override { unexpected(); }
    IControlMap* getActive() const override { ++calls; return map; }
    int mapNextInput(IControlMap* map, int id, bool a, bool b, bool c, bool d, std::string& out) override { unexpected(); }
    float getRepeatRate(int id, bool b) override { unexpected(); }
    void applyActive() override { unexpected(); }
};

struct FixtureDevice : IDirectInputDevice8A {
    HRESULT acquireResult = DI_OK, cooperativeResult = DI_OK, formatResult = DI_OK, propertyResult = DI_OK;
    int acquires=0, unacquires=0, releases=0, reads=0, formats=0, properties=0;
    std::vector<DWORD> coopFlags;
    std::vector<HWND> coopWindows;
    std::vector<HRESULT> readResults;
    std::vector<DIDEVICEOBJECTDATA> events;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,LPVOID*) override { unexpected(); }
    ULONG STDMETHODCALLTYPE AddRef() override { unexpected(); }
    ULONG STDMETHODCALLTYPE Release() override { ++releases; return 0; }
    HRESULT STDMETHODCALLTYPE Acquire() override { ++acquires; return acquireResult; }
    HRESULT STDMETHODCALLTYPE Unacquire() override { ++unacquires; return DIERR_NOTACQUIRED; }
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS caps) override {
        check(caps->dwSize==sizeof(*caps),"capabilities size"); caps->dwButtons=8; caps->dwAxes=3; return E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID id,LPCDIPROPHEADER p) override {
        ++properties; const DIPROPDWORD* value=reinterpret_cast<const DIPROPDWORD*>(p);
        check(&id==&DIPROP_BUFFERSIZE && p->dwSize==20 && p->dwHeaderSize==16 && p->dwObj==0 && p->dwHow==DIPH_DEVICE && value->dwData==128,"buffer property"); return propertyResult;
    }
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT format) override {
        ++formats; check(format==&c_dfDIMouse2 && format->dwDataSize==20 && format->dwNumObjs==11,"mouse2 format"); return formatResult;
    }
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND window,DWORD flags) override {
        coopWindows.push_back(window); coopFlags.push_back(flags); return cooperativeResult;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD size,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags) override {
        check(size==20 && *count==128 && flags==0,"poll request");
        HRESULT result=reads<static_cast<int>(readResults.size())?readResults[reads]:DI_OK; ++reads;
        *count=static_cast<DWORD>(events.size());
        for(std::size_t i=0;i<events.size();++i) data[i]=events[i];
        return result;
    }
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA,LPVOID,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID,LPDIPROPHEADER) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD,LPVOID) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA,DWORD,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID,LPCDIEFFECT,LPDIRECTINPUTEFFECT *,LPUNKNOWN) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA,LPVOID,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA,REFGUID) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE Poll() override { unexpected(); }
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR,LPDIENUMEFFECTSINFILECALLBACK,LPVOID,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR,DWORD,LPDIFILEEFFECT,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override { unexpected(); }
};
struct FixtureDirectInput : IDirectInput8A {
    FixtureDevice* device = 0;
    HRESULT createResult = DI_OK;
    int releases=0, creates=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,LPVOID*) override { unexpected(); }
    ULONG STDMETHODCALLTYPE AddRef() override { unexpected(); }
    ULONG STDMETHODCALLTYPE Release() override { ++releases; return 0; }
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID guid,LPDIRECTINPUTDEVICE8A* out,LPUNKNOWN outer) override {
        ++creates; check(guid==GUID_SysMouse && outer==0,"system mouse create"); if(createResult==DI_OK) *out=device; return createResult;
    }
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD,LPDIENUMDEVICESCALLBACKA,LPVOID,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID,LPCSTR,LPGUID) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(LPCSTR,LPDIACTIONFORMATA,LPDIENUMDEVICESBYSEMANTICSCBA,LPVOID,DWORD) override { unexpected(); }
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK,LPDICONFIGUREDEVICESPARAMSA,DWORD,LPVOID) override { unexpected(); }
};
FixtureDirectInput* factory = 0;
HRESULT factoryResult = DI_OK;
extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance,DWORD version,REFIID iid,LPVOID* out,LPUNKNOWN outer) {
    check(instance==GetModuleHandleA(0) && version==0x800 && iid==IID_IDirectInput8A && outer==0,"DirectInput create args");
    *out=factoryResult==DI_OK?factory:0; return factoryResult;
}
DIDEVICEOBJECTDATA event(DWORD offset,LONG data,DWORD time,DWORD sequence) {
    DIDEVICEOBJECTDATA e = {offset,static_cast<DWORD>(data),time,sequence,0}; return e;
}

int main()
{
    FixtureMap map;
    FixtureManager manager;
    manager.map=&map;
    controlMapManager=&manager;
    inputDeviceManager=&manager;
    FixtureDevice device;
    FixtureDirectInput direct;
    direct.device=&device;
    factory=&direct;

    alignas(MouseDevice) unsigned char storage[sizeof(MouseDevice)];
    std::memset(storage,0x5A,sizeof(storage));
    MouseDevice* mouse=new(storage) MouseDevice;
    const unsigned char untouched[4]={0x5A,0x5A,0x5A,0x5A};
    check(std::memcmp(&mouse->m_directInput,untouched,4)==0 &&
          std::memcmp(&mouse->m_sampleTime,untouched,4)==0 &&
          std::memcmp(&mouse->m_mode,untouched,4)==0,"constructor untouched fields");
    check(mouse->getRef()==1 && !mouse->isActive() && mouse->m_device==0,"constructor active/ref/device");
    bool arrays=true;
    for(int i=0;i<4;++i) arrays &= mouse->m_axes[i]==0 && nearFloat(mouse->m_axisScale[i],0.03f) &&
                                  !mouse->m_invertAxis[i] && !mouse->m_axisArmed[i];
    check(arrays && mouse->m_samples.empty() && mouse->m_peakSamples.empty(),"constructor arrays/queues");
    check(mouse->getType()==IDTMouse && mouse->queryInterface(IID_IBase)==mouse &&
          mouse->queryInterface(IID_IInputDevice)==mouse && mouse->queryInterface(IID_IAdvancedInputDevice)==mouse &&
          mouse->queryInterface(CID_MouseDevice)==0,"interface query/type");
    mouse->addRef();
    check(mouse->getRef()==2 && mouse->release()==1,"reference increment/decrement");
    check(!mouse->activate() && !mouse->deActivate(),"missing device lifecycle");
    check(mouse->init(),"init success despite ignored capabilities failure");
    check(mouse->getNumAxes()==3 && mouse->getNumButtons()==8 && mouse->getCooperativeMode()==IDM_EXCLUSIVE,"init counts/mode");
    check(device.coopFlags.back()==5,"default cooperative flags");
    check(mouse->setCooperativeMode(IDM_NONEXCLUSIVE) && device.coopFlags.back()==10,"background mode flags");
    HWND token=reinterpret_cast<HWND>(0x1234);
    check(mouse->setCooperativeModeForWindow(token,IDM_NONEXCLUSIVE) &&
          device.coopFlags.back()==6 && device.coopWindows.back()==token,"explicit window flags");
    device.cooperativeResult=E_FAIL;
    check(!mouse->setCooperativeModeForWindow(token,IDM_EXCLUSIVE) && mouse->getCooperativeMode()==IDM_NONEXCLUSIVE,"mode failure retains state");
    device.cooperativeResult=DI_OK;
    device.acquireResult=DIERR_OTHERAPPHASPRIO;
    check(mouse->activate() && !mouse->isActive(),"activate returns true after failed acquire");
    device.acquireResult=DI_OK;
    check(mouse->activate() && mouse->isActive(),"successful acquire");
    int acquisitions=device.acquires;
    check(mouse->activate() && device.acquires==acquisitions,"active acquire suppressed");

    mouse->setInvertAxis(IDA_W,true);
    mouse->setAxisScale(IDA_W,7.0f);
    mouse->m_axes[3]=2.0f;
    check(mouse->getInvertAxis(IDA_W) && mouse->getAxisScale(IDA_W)==7 &&
          mouse->readAxis(IDA_W,false)==-2 && mouse->readAxis(IDA_W,true)==-2,"inclusive axis bound and ignored read flag");
    mouse->setInvertAxis(static_cast<InputDeviceAxes>(4),true);
    mouse->setAxisScale(static_cast<InputDeviceAxes>(4),9);
    check(!mouse->getInvertAxis(static_cast<InputDeviceAxes>(4)) && mouse->getAxisScale(static_cast<InputDeviceAxes>(4))==0,"upper axis bound");
    mouse->setAxisAdd(IDA_X,99);
    check(mouse->getAxisAdd(IDA_X)==0,"retail axis-add no-op");

    mouse->m_peakSamples.push_back(event(DIMOFS_X,100,0,9));
    mouse->m_peakSamples.push_back(event(DIMOFS_X,4,0,11));
    mouse->m_peakSamples.push_back(event(DIMOFS_Y,-2,0,12));
    mouse->m_peakSamples.push_back(event(DIMOFS_Z,99,0,13));
    mouse->m_lastSequence=10;
    float x=99,y=99;
    map.calls.clear();
    mouse->peakXYAxis(x,y);
    check(x==4 && y==3 && mouse->m_peakSamples.empty(),"peak signed axes/sequence/drain");
    check(map.calls==std::vector<int>({0,10,1,10,11}),"peak repeated map calls/order");
    mouse->peakXYAxis(x,y);
    check(x==0 && y==0,"peak drains all samples");

    mouse->m_sampleTime=900;
    mouse->m_pollTime=1000;
    mouse->m_samples.push_back(event(DIMOFS_X,5,949,20));
    mouse->m_samples.push_back(event(DIMOFS_BUTTON0,0x80,949,21));
    mouse->m_samples.push_back(event(99,0,949,22));
    mouse->m_samples.push_back(event(DIMOFS_X,7,950,23));
    mouse->m_axes[3]=2;
    map.calls.clear();
    mouse->update(0.05f);
    check(mouse->m_sampleTime==950 && mouse->m_samples.size()==1 && mouse->m_axes[0]==10,"strict timestamp boundary/scaling");
    check(mouse->m_axes[3]==10 && map.calls==std::vector<int>({0,1,2,3}),"fourth axis retained/scaled");
    check(mouse->buttonPressed(static_cast<InputDeviceButtons>(1)) && mouse->m_lastSequence==22 &&
          nearFloat(mouse->buttonPressedSampleLatency(static_cast<InputDeviceButtons>(1)),0.033333335f),"button latency clamp/ignored offset sequence");
    mouse->update(0.001f);
    check(mouse->m_axes[0]==14 && mouse->m_axes[3]==50 && mouse->m_samples.empty() &&
          !mouse->buttonPressed(static_cast<InputDeviceButtons>(1)) && mouse->buttonDown(static_cast<InputDeviceButtons>(1)),"next sample/held button");
    mouse->m_samples.push_back(event(DIMOFS_BUTTON0,0,950,24));
    mouse->update(0.001f);
    check(mouse->buttonReleased(static_cast<InputDeviceButtons>(1)) && !mouse->buttonDown(static_cast<InputDeviceButtons>(1)),"button release");
    mouse->m_previousButtons=1; mouse->m_buttons=3;
    check(mouse->buttonPressed(static_cast<InputDeviceButtons>(2)) && !mouse->buttonPressed(static_cast<InputDeviceButtons>(3)),"whole button-mask predicate");
    mouse->m_buttonLatency[1]=0.012f; mouse->m_buttonLatency[31]=0.021f;
    check(nearFloat(mouse->buttonPressedSampleLatency(static_cast<InputDeviceButtons>(6)),0.012f) &&
          nearFloat(mouse->buttonPressedSampleLatency(static_cast<InputDeviceButtons>(0x80000000u)),0.021f) &&
          mouse->buttonPressedSampleLatency(static_cast<InputDeviceButtons>(0))==0,"lowest-set-bit latency");

    InputDeviceAxes axis=IDA_X; InputDeviceButtons button=IDB_NONE; InputDeviceKeys key=IDK_NONE; bool negative=true;
    check(mouse->getNextInput(axis,button,key,negative) && static_cast<int>(axis)==15 &&
          static_cast<int>(button)==2 && negative,"capture button priority/untouched direction");
    mouse->m_buttons=mouse->m_previousButtons=0;
    mouse->m_axes[0]=mouse->m_axes[1]=mouse->m_axes[2]=0;
    check(!mouse->getNextInput(axis,button,key,negative),"capture rearm near center");
    mouse->m_axes[1]=-0.5f;
    check(mouse->getNextInput(axis,button,key,negative) && axis==IDA_Y && button==0 && key==0 && negative,"capture negative axis");
    check(!mouse->getNextInput(axis,button,key,negative),"capture requires rearm");

    mouse->m_samples.clear(); mouse->m_sampleTime=100;
    mouse->update(0.01f);
    check(mouse->m_sampleTime==109,"x87 fractional milliseconds truncate before float rounding");

    mouse->m_samples.clear(); mouse->m_peakSamples.clear();
    device.events={event(DIMOFS_X,-9,2,30),event(DIMOFS_BUTTON7,0x80,3,31)};
    device.readResults={DIERR_INPUTLOST,DI_OK}; device.reads=0;
    mouse->m_pollTime=500;
    mouse->m_buttons=mouse->m_previousButtons=0xff;
    mouse->m_axes[3]=99;
    mouse->update(0);
    check(device.reads==2 && mouse->m_samples.size()==2 && mouse->m_peakSamples.size()==2 &&
          mouse->m_sampleTime==500 && mouse->m_buttons==0 && mouse->m_axes[3]==0,"lost input reset/reacquire/dual queue");
    device.readResults={DIERR_INPUTLOST}; device.reads=0; device.acquireResult=E_FAIL;
    mouse->update(0);
    check(device.reads==1 && mouse->m_samples.empty() && mouse->m_peakSamples.empty() && mouse->isActive(),"failed reacquire returns but active retained");
    device.acquireResult=DI_OK;
    mouse->m_samples.push_back(event(DIMOFS_X,1,0,1)); mouse->m_peakSamples.push_back(event(DIMOFS_X,1,0,1));
    device.readResults={DI_BUFFEROVERFLOW}; device.reads=0;
    mouse->update(0);
    check(mouse->m_samples.empty() && mouse->m_peakSamples.empty(),"buffer overflow clears both queues");

    device.events.clear(); device.readResults.clear(); device.reads=0;
    mouse->m_samples.assign(256,event(DIMOFS_X,0,0,0)); mouse->m_peakSamples.assign(256,event(DIMOFS_X,0,0,0));
    mouse->update(0);
    check(mouse->m_samples.size()==256 && mouse->m_peakSamples.size()==256,"queue threshold is strictly greater");
    mouse->m_peakSamples.push_back(event(DIMOFS_X,0,0,0));
    mouse->update(-1);
    check(mouse->m_samples.size()==256 && mouse->m_peakSamples.empty(),"secondary queue threshold independent");
    mouse->m_samples.push_back(event(DIMOFS_X,0,0,0)); mouse->m_peakSamples.push_back(event(DIMOFS_X,0,0,0));
    mouse->update(0);
    check(mouse->m_samples.empty() && mouse->m_peakSamples.empty(),"primary overflow clears both queues");
    mouse->m_samples.push_back(event(DIMOFS_X,0,0,0)); mouse->m_peakSamples.push_back(event(DIMOFS_X,0,0,0));
    check(mouse->deActivate() && !mouse->isActive() && mouse->m_samples.empty() && mouse->m_peakSamples.size()==1,"deactivate keeps peak queue");
    int reads=device.reads;
    mouse->update(0); mouse->update(1);
    check(device.reads==reads && mouse->m_peakSamples.size()==1,"inactive updates do nothing");
    mouse->~MouseDevice();
    check(device.releases==1 && direct.releases==1 && device.unacquires==2,"destructor unacquires even after deactivate");

    // Recoverable error returns are tested with the real Debug filter disabled.
    debugOn[DebugTypeError]=false;
    factoryResult=E_FAIL;
    { MouseDevice failed; check(!failed.init(),"DirectInput creation failure"); }
    factoryResult=DI_OK; direct.createResult=E_FAIL;
    { MouseDevice failed; check(!failed.init(),"device creation failure"); }
    direct.createResult=DI_OK; device.formatResult=E_FAIL;
    { MouseDevice failed; check(!failed.init(),"data format failure"); }
    device.formatResult=DI_OK; device.propertyResult=E_FAIL;
    { MouseDevice failed; check(failed.init(),"filtered property diagnostic continues to cooperative mode"); }
    device.propertyResult=DI_OK;
    inputDeviceManager=0;
    { MouseDevice failed; failed.m_directInput=0; check(!failed.init(),"missing manager init gate"); }
    debugOn[DebugTypeError]=true;
    inputDeviceManager=&manager;
    MouseDevice* counted=new MouseDevice;
    counted->m_directInput=0; // fixture chooses safe never-initialized teardown
    check(counted->release()==0,"release destroys at zero");
    std::printf("Mouse device: %d/%d checks passed; native x86 size %u\n",checks-failures,checks,static_cast<unsigned>(sizeof(MouseDevice)));
    return failures?1:0;
}
