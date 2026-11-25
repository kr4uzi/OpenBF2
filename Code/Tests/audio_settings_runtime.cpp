// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: AUDIO-SETTINGS-FIXTURE]
// Actual owner/data/enum/apply methods. OS, allocation, typed sound, class-manager
// singleton and SwiffHost-seam callbacks are observed; no AudioSettings
// implementation is replaced by the fixture.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <limits>
#include <sstream>
#include <vector>
#include "../BF2/Game/AudioSettings.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "SwiffHostStandIn.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Sound/BfSound.h"
using namespace dice::hfe;
static unsigned checks, failures, memoryQueries, frees;
static std::vector<ULONGLONG> memorySamples;
static void* watchedOwner;
static bool captureOwner;
static std::vector<void*> watchedPointers;
static std::vector<unsigned> freedPointers;
static bool watchPointers;
static unsigned engineQueries, masterVolumeCalls, playMusicCalls;
static unsigned masterVolumeBits;
static std::vector<std::string> soundLog;   // every typed sound callback, in dispatch order
static unsigned seamCalls;
static int seamType, seamFlag;
static std::string seamText;
static IBase* seamHost;
static void logCall(const std::string& entry){ soundLog.push_back(entry); }

namespace dice { namespace hfe {
// [TEMP] The two Core/Globals.cpp definitions this isolated executable does not link.
IGame* game=nullptr;
ClassManager* classManager=nullptr;
} }

// [TEMP] Typed observers over the real null-sound classes. Only the callbacks the
// recovered apply path dispatches are overridden; their production definitions stay linked.
struct SoundEngineObserver : NullSoundEngine {
    void setMasterVolume(float value) override
    {
        ++masterVolumeCalls;
        std::memcpy(&masterVolumeBits,&value,sizeof(value));
        char text[32];std::sprintf(text,"master %08x",masterVolumeBits);logCall(text);
    }
    bool playMusic() override { ++playMusicCalls; logCall("playMusic"); return true; }
};
struct SoundFacadeObserver : BfSoundNull {
    ISoundEngine* observedEngine; int initializeResult=0;
    explicit SoundFacadeObserver(ISoundEngine* engine) : observedEngine(engine) {}
    ISoundEngine* engine() const override { ++engineQueries; logCall("engine"); return observedEngine; }
    void setVolumeForGroup(int group,float volume) override
    { std::ostringstream o;o<<"group "<<group<<' '<<volume;logCall(o.str()); }
    void setProperty(const std::string& name,float value) override
    { std::ostringstream o;o<<"property "<<name<<' '<<value;logCall(o.str()); }
    int initialize(int quality,const std::string& provider,int voices) override
    { std::ostringstream o;o<<"initialize "<<quality<<' '<<provider<<' '<<voices;logCall(o.str()); return initializeResult; }
};
// [TEMP] A SwiffHost stand-in whose address alone is observed; nothing dereferences it.
// [TEMP] The typed SwiffHost stand-in: postMessage records the {type, text, code} record
// AudioSettings::reportError posts (ISwiffHost +0x1E4); every other slot aborts.
struct HostStandIn : SwiffHostStandIn {
    bool postMessage(const dice::hfe::SwiffHostMessage& m) override
    { ++seamCalls; seamHost=this; seamType=m.type; seamText=m.text; seamFlag=m.code; return true; }
};
// [TEMP] The real ClassManager with only its singleton lookup observed.
struct ClassManagerObserver : ClassManager {
    IBase* host=nullptr; unsigned queries=0; std::string lastName;
    IBase* getSingleton(const std::string& name) override { ++queries; lastName=name; return host; }
};
static void check(bool value,const char* label)
{ ++checks; if(!value){++failures;std::printf("FAIL %s\n",label);} }
static BOOL WINAPI memoryObserver(LPMEMORYSTATUSEX status)
{
    check(status->dwLength==sizeof(*status),"extended memory status size initialized");
    unsigned index=memoryQueries++;
    status->ullTotalPhys=memorySamples.empty()?512ull*1024*1024:memorySamples[index%memorySamples.size()];
    return TRUE;
}
// Patch only this isolated executable's actual import slot; kernel32.lib otherwise
// resolves the symbol before /alternatename and bypasses the observer.
static bool installMemoryObserver()
{
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    auto* imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for(;imports->Name;++imports){
        if(!imports->OriginalFirstThunk)continue;
        auto* names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+imports->OriginalFirstThunk);
        auto* slots=reinterpret_cast<IMAGE_THUNK_DATA*>(base+imports->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots){
            if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
            auto* name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
            if(std::strcmp(reinterpret_cast<char*>(name->Name),"GlobalMemoryStatusEx"))continue;
            DWORD oldProtect=0,unused=0;
            if(!VirtualProtect(&slots->u1.Function,sizeof(slots->u1.Function),PAGE_READWRITE,&oldProtect))return false;
            slots->u1.Function=reinterpret_cast<ULONG_PTR>(&memoryObserver);
            VirtualProtect(&slots->u1.Function,sizeof(slots->u1.Function),oldProtect,&unused);
            return true;
        }
    }
    return false;
}
void* operator new(std::size_t size)
{
    void* pointer=std::malloc(size?size:1); if(!pointer)throw std::bad_alloc();
    if(captureOwner && size==sizeof(AudioSettings)){watchedOwner=pointer;captureOwner=false;}
    return pointer;
}
void operator delete(void* pointer) noexcept
{
    if(pointer && pointer==watchedOwner)++frees;
    if(watchPointers && pointer)for(unsigned i=0;i<watchedPointers.size();++i)
        if(pointer==watchedPointers[i]){freedPointers.push_back(i);break;}
    std::free(pointer);
}
void operator delete(void* pointer,std::size_t) noexcept{::operator delete(pointer);}

namespace dice { namespace hfe {
struct AudioSettingsFixture {
    static AudioSettingsData& current(AudioSettings& owner){return owner.m_current;}
    static AudioSettingsData& pending(AudioSettings& owner){return owner.m_pending;}
    static bool enums(const AudioSettings& owner)
    {return owner.m_providers.names==std::vector<std::string>({"software","hardware","creative"})
        && owner.m_providers.values==std::vector<int>({0,2,4})
        && owner.m_qualities.names==std::vector<std::string>({"Low","Medium","High","UltraHigh"})
        && owner.m_qualities.values==std::vector<int>({0,1,2,3});}
    static int status(const AudioSettings& owner){return owner.m_status;}
    static void watchDestruction(AudioSettings& owner)
    {
        owner.m_pending.soundQuality.assign(100,'a');owner.m_pending.provider.assign(100,'b');
        owner.m_current.soundQuality.assign(100,'c');owner.m_current.provider.assign(100,'d');
        owner.m_qualities.names[0].assign(100,'e');owner.m_providers.names[0].assign(100,'f');
        watchedPointers={const_cast<char*>(owner.m_pending.soundQuality.data()),const_cast<char*>(owner.m_pending.provider.data()),
            const_cast<char*>(owner.m_current.soundQuality.data()),const_cast<char*>(owner.m_current.provider.data()),
            owner.m_qualities.values.data(),const_cast<char*>(owner.m_qualities.names[0].data()),owner.m_qualities.names.data(),
            owner.m_providers.values.data(),const_cast<char*>(owner.m_providers.names[0].data()),owner.m_providers.names.data()};
    }
};
} }

static bool defaults(const AudioSettingsData& d)
{
    return d.voipEnabled && d.voipPlaybackVolume==1 && d.voipCaptureVolume==1 && d.voipCaptureThreshold==0.1f
        && !d.voipBoostEnabled && d.voipUsePushToTalk && d.provider=="software"
        && d.effectsVolume==1 && d.musicVolume==0.33f && d.helpVoiceVolume==1
        && !d.englishOnlyVoices && d.enableEAX && !d.alwaysAllowSystemOpenAL && d.forceNumberOfVoices==-1;
}
struct FlushBuffer : std::stringbuf {
    unsigned flushes=0; AudioSettingsData* mutate=nullptr;
    int sync() override
    {
        ++flushes;
        if(mutate && flushes==1)mutate->voipPlaybackVolume=0.25f;
        return 0;
    }
};
struct LabelBuffer : std::stringbuf {
    AudioSettingsData* data=nullptr;
    std::streamsize xsputn(const char* text,std::streamsize size) override
    {
        if(std::string(text,static_cast<std::size_t>(size))=="AudioSettings.setVoipEnabled ")data->voipEnabled=false;
        if(std::string(text,static_cast<std::size_t>(size))=="AudioSettings.setVoipPlaybackVolume ")data->voipPlaybackVolume=5.0f;
        return std::stringbuf::xsputn(text,size);
    }
};
static void checkData()
{
    memorySamples={512u*1024*1024}; memoryQueries=0;
    AudioSettingsData data;
    check(defaults(data) && data.soundQuality=="Low" && memoryQueries==1,"actual data constructor/defaults query once");
    struct Sample{ULONGLONG bytes;const char* quality;};
    for(const auto& sample:{Sample{0,"Low"},Sample{512ull*1024*1024,"Low"},
        Sample{1024ull*1024*1024,"Medium"},Sample{16ull*1024*1024*1024,"Medium"},
        Sample{764u*1024*1024,"Low"},Sample{764u*1024*1024+0x80000,"Low"},
        Sample{764u*1024*1024+0x80001,"Medium"},Sample{768u*1024*1024,"Medium"},
        Sample{0xffffffffu-0x7ffffu,"Medium"},Sample{0xffffffffu,"Medium"},
        Sample{4ull*1024*1024*1024,"Medium"},Sample{~0ull,"Medium"}}){
        memorySamples={sample.bytes}; data.reset();check(data.soundQuality==sample.quality,"full memory size with shipped rounding and quality threshold");
    }
    memorySamples={512u*1024*1024};
    data.reset();
    AudioSettingsData other;check(!data.differsFrom(other),"equal default records");
    bool AudioSettingsData::* boolFields[]={&AudioSettingsData::voipEnabled,&AudioSettingsData::voipBoostEnabled,
        &AudioSettingsData::voipUsePushToTalk,&AudioSettingsData::englishOnlyVoices,&AudioSettingsData::enableEAX,
        &AudioSettingsData::alwaysAllowSystemOpenAL};
    for(auto member:boolFields){
        other.*member=!(data.*member);check(data.differsFrom(other),"each independent bool participates in comparison");other.copyFrom(data);
    }
    float AudioSettingsData::* floatFields[]={&AudioSettingsData::voipPlaybackVolume,&AudioSettingsData::voipCaptureVolume,
        &AudioSettingsData::voipCaptureThreshold,&AudioSettingsData::effectsVolume,&AudioSettingsData::musicVolume,&AudioSettingsData::helpVoiceVolume};
    for(auto member:floatFields){
        other.*member=5.25f;check(data.differsFrom(other),"each independent float participates in comparison");other.copyFrom(data);
        data.*member=other.*member=std::numeric_limits<float>::quiet_NaN();check(data.differsFrom(other),"matching NaNs remain different");
        data.*member=other.*member=std::numeric_limits<float>::infinity();check(!data.differsFrom(other),"matching infinities remain equal");
        data.*member=0.0f;other.*member=-0.0f;check(!data.differsFrom(other),"signed zero remains equal");data.reset();other.copyFrom(data);
    }
    other.provider="different";check(data.differsFrom(other),"provider comparison");other.copyFrom(data);
    other.soundQuality="different";check(data.differsFrom(other),"quality comparison");other.copyFrom(data);
    other.forceNumberOfVoices=17;check(data.differsFrom(other),"voice-count comparison");other.copyFrom(data);
    data.provider=std::string("a\0b",3);other.provider=std::string("a\0c",3);
    check(data.differsFrom(other),"comparison uses full strings beyond embedded NUL");other.copyFrom(data);
    check(!data.differsFrom(other),"native record copy includes full strings");
    data.copyFrom(data);check(data.provider==std::string("a\0b",3),"self copy preserves full native string");
    data.reset();FlushBuffer buffer;std::ostream out(&buffer);data.write(out);
    const std::string expected="AudioSettings.setVoipEnabled 1\nAudioSettings.setVoipPlaybackVolume 1\n"
        "AudioSettings.setVoipCaptureVolume 1\nAudioSettings.setVoipCaptureThreshold 0.1\nAudioSettings.setVoipBoostEnabled 0\n"
        "AudioSettings.setVoipUsePushToTalk 1\nAudioSettings.setProvider \"software\"\nAudioSettings.setSoundQuality \"Low\"\n"
        "AudioSettings.setEffectsVolume 1\nAudioSettings.setMusicVolume 0.33\nAudioSettings.setHelpVoiceVolume 1\n"
        "AudioSettings.setEnglishOnlyVoices 0\nAudioSettings.setEnableEAX 1\n";
    check(buffer.str()==expected && buffer.flushes==13,"exact default script and endl after every row");
    data.provider=std::string("a\0b",3);data.soundQuality="Hi\"gh";data.alwaysAllowSystemOpenAL=true;data.forceNumberOfVoices=16;
    std::ostringstream custom;custom<<std::boolalpha;data.write(custom);
    check(custom.str().find("setProvider \"a\"\n")!=std::string::npos,"script C-string insertion truncates embedded NUL");
    check(custom.str().find("setSoundQuality \"Hi\"gh\"\n")!=std::string::npos,"script does not escape embedded quotes");
    check(custom.str().find("setAlwaysAllowSystemOpenAL true\n")!=std::string::npos
        && custom.str().find("setForceNumberOfVoices 16\n")!=std::string::npos,"conditional rows and caller stream boolalpha flags");
    for(int count:{0,-1,-50}){data.forceNumberOfVoices=count;std::ostringstream s;data.write(s);check(s.str().find("setForceNumberOfVoices")==std::string::npos,"nonpositive voice count omitted");}
    data.reset();FlushBuffer reloading;reloading.mutate=&data;std::ostream reloadingOut(&reloading);data.write(reloadingOut);
    check(reloading.str().find("setVoipPlaybackVolume 0.25\n")!=std::string::npos,"later field reads occur after earlier stream flush callback");
    data.reset();LabelBuffer labels;labels.data=&data;std::ostream labelOut(&labels);data.write(labelOut);
    check(labels.str().find("setVoipEnabled 1\n")!=std::string::npos && labels.str().find("setVoipPlaybackVolume 1\n")!=std::string::npos,
        "each scalar is captured before its own stream label callback");
}
static void checkApply()
{
    memorySamples={512u*1024*1024};
    AudioSettings owner;
    IAudioSettings* api=&owner;
    auto& current=AudioSettingsFixture::current(owner);
    auto& pending=AudioSettingsFixture::pending(owner);
    g_bfSound=nullptr;
    check(current.provider.empty() && pending.provider=="software" && api->hasChanges(),
        "fresh owner requires apply because only current provider is cleared");
    pending.copyFrom(current);
    check(!api->hasChanges() && api->apply(false),"matching empty-provider records succeed without a sound facade");
    current.provider=std::string("software\0suffix",15);
    pending.copyFrom(current);
    check(!api->hasChanges(),"matching nonempty native records are unchanged");
    engineQueries=masterVolumeCalls=0;
    for(bool reportErrors:{false,true})
        check(api->apply(reportErrors),"unchanged apply succeeds with no sound facade for either flag");
    check(!engineQueries && !masterVolumeCalls,"unchanged apply has no sound callbacks");
    SoundEngineObserver engine;
    SoundFacadeObserver facade(&engine);
    g_bfSound=&facade;
    for(bool reportErrors:{false,true})
        check(api->apply(reportErrors),"unchanged apply returns true with a typed sound observer installed");
    check(!engineQueries && !masterVolumeCalls,"unchanged apply does not query available sound either");
    check(!api->hasChanges() && current.provider==std::string("software\0suffix",15)
        && pending.provider==current.provider && AudioSettingsFixture::status(owner)==0,
        "unchanged apply preserves native records and status");
    pending.musicVolume=0.125f;
    check(api->hasChanges(),"non-provider record change reaches real hasChanges comparison");
    pending.copyFrom(current);
    current.effectsVolume=pending.effectsVolume=std::numeric_limits<float>::quiet_NaN();
    check(api->hasChanges(),"matching unordered floats remain changed through the owner virtual");
    current.effectsVolume=1.0f;pending.copyFrom(current);pending.musicVolume=0.125f;
    check(api->hasChanges(),"pending edit is visible before revert");
    api->revertChanges();
    check(!api->hasChanges() && pending.musicVolume==current.musicVolume,"revertChanges copies current into pending without sound calls");
    pending.provider="hardware";pending.enableEAX=false;pending.soundQuality="UltraHigh";
    api->setDefaults();
    check(defaults(pending) && pending.soundQuality=="Low" && AudioSettingsFixture::status(owner)==0 && !engineQueries,
        "setDefaults resets the pending record through its own memory query and clears status");
    g_bfSound=nullptr;
}

static const std::vector<std::string> volumePrefix={"engine","master 3f800000","group 0 1","group 3 1","group 1 0.33","group 2 1"};
static bool logStartsWith(const std::vector<std::string>& expected,std::size_t at=0)
{
    if(soundLog.size()<at+expected.size())return false;
    for(std::size_t i=0;i<expected.size();++i)if(soundLog[at+i]!=expected[i])return false;
    return true;
}
static bool logEntry(std::size_t at,const std::string& expected)
{ return at<soundLog.size() && soundLog[at]==expected; }

static void checkApplyRecord()
{
    memorySamples={1024u*1024*1024};   // both records reset to Medium
    SoundEngineObserver engine;SoundFacadeObserver facade(&engine);g_bfSound=&facade;game=nullptr;
    HostStandIn host;ClassManagerObserver manager;manager.host=&host;classManager=&manager;
    {
        AudioSettings owner;IAudioSettings* api=&owner;
        soundLog.clear();engineQueries=masterVolumeCalls=playMusicCalls=0;
        check(api->apply(false),"changed apply succeeds on the null facade's zero initialize result");
        check(logStartsWith(volumePrefix) && soundLog.size()==9 && logEntry(6,"property enable-culling 1")
            && logEntry(7,"property allow-any-system-openal 0") && logEntry(8,"initialize 1 0 32"),
            "exact callback order: master, four groups, two properties, then initialize");
        check(engineQueries==1 && masterVolumeCalls==1,"one engine query and one master-volume call per record");
        check(AudioSettingsFixture::current(owner).provider=="software" && !api->hasChanges() && AudioSettingsFixture::status(owner)==0,
            "success copies pending into current and clears status");
        check(manager.queries==0 && seamCalls==0 && playMusicCalls==0,"success reports nothing and leaves the music alone");
    }
    struct ProviderCase{const char* provider;bool eax;const char* digit;};
    for(const auto& sample:{ProviderCase{"software",false,"6"},ProviderCase{"software",true,"0"},ProviderCase{"hardware",false,"1"},
        ProviderCase{"hardware",true,"2"},ProviderCase{"creative",false,"3"},ProviderCase{"creative",true,"4"},ProviderCase{"unknown",true,"6"}}){
        AudioSettings owner;IAudioSettings* api=&owner;
        auto& pending=AudioSettingsFixture::pending(owner);pending.provider=sample.provider;pending.enableEAX=sample.eax;
        soundLog.clear();
        check(api->apply(false) && soundLog.size()==9 && logEntry(8,std::string("initialize 1 ")+sample.digit+" 32"),
            "provider row plus EAX flag selects the retail provider digit");
    }
    {
        AudioSettings owner;IAudioSettings* api=&owner;
        auto& pending=AudioSettingsFixture::pending(owner);pending.provider="unknown";pending.enableEAX=false;
        soundLog.clear();playMusicCalls=0;
        check(!api->apply(false),"unknown provider without EAX fails");
        // The wrapper queries the facade's engine again (PE 4B17A8) before the +C0 music restart.
        check(soundLog.size()==17 && logStartsWith(volumePrefix) && logStartsWith(volumePrefix,6)
            && logEntry(12,"property enable-culling 1") && logEntry(14,"initialize 1 6 32") && logEntry(15,"engine") && logEntry(16,"playMusic"),
            "failed provider lookup stops before the properties, retries current, then restarts the music");
        check(AudioSettingsFixture::current(owner).provider.empty() && AudioSettingsFixture::status(owner)==0 && api->hasChanges(),
            "no copy after failure; status reflects the successful retry of the current record");
        check(playMusicCalls==1,"one music restart per failed apply");
    }
    {
        AudioSettings owner;IAudioSettings* api=&owner;
        auto& pending=AudioSettingsFixture::pending(owner);pending.soundQuality="UltraHigh";
        soundLog.clear();
        check(api->apply(false) && pending.soundQuality=="High" && AudioSettingsFixture::current(owner).soundQuality=="High",
            "UltraHigh is downgraded in the pending record for a non-creative provider before the copy");
        check(logEntry(6,"property enable-culling 1") && logEntry(8,"initialize 0 0 32"),"downgraded quality drives culling, engine quality and voices");
    }
    {
        AudioSettings owner;IAudioSettings* api=&owner;
        auto& pending=AudioSettingsFixture::pending(owner);pending.soundQuality="UltraHigh";pending.provider="creative";pending.enableEAX=false;
        soundLog.clear();
        check(api->apply(false) && pending.soundQuality=="UltraHigh" && logEntry(6,"property enable-culling 0") && logEntry(8,"initialize 0 3 128"),
            "creative keeps UltraHigh: culling off, engine quality 0, 128 voices");
    }
    for(int voices:{48,0,-7}){
        AudioSettings owner;IAudioSettings* api=&owner;
        AudioSettingsFixture::pending(owner).forceNumberOfVoices=voices;
        soundLog.clear();
        check(api->apply(false) && logEntry(8,voices>0?"initialize 1 0 48":"initialize 1 0 32"),"positive forced voice count overrides the quality table");
    }
    {
        AudioSettings owner;IAudioSettings* api=&owner;
        AudioSettingsFixture::pending(owner).alwaysAllowSystemOpenAL=true;
        soundLog.clear();
        check(api->apply(false) && logEntry(7,"property allow-any-system-openal 1"),"allow-any-system-openal follows the record flag");
    }
    {
        int sentinel=0;game=reinterpret_cast<IGame*>(&sentinel);   // [TEMP] only its nullness is read
        AudioSettings owner;IAudioSettings* api=&owner;
        soundLog.clear();
        check(api->apply(false) && soundLog==volumePrefix,"in-game apply stops after the volumes and succeeds");
        check(AudioSettingsFixture::current(owner).provider=="software" && AudioSettingsFixture::status(owner)==0,"in-game success still copies the record");
        game=nullptr;
    }
    const char* messages[]={"","noprovider","noproviderdll","openaldenied","openalerror",""};
    for(int code=1;code<=5;++code){
        facade.initializeResult=code;
        {
            AudioSettings owner;IAudioSettings* api=&owner;
            soundLog.clear();playMusicCalls=0;manager.queries=0;seamCalls=0;
            check(!api->apply(false) && AudioSettingsFixture::status(owner)==1 && api->hasChanges(),"failed initialize returns false with status1 and no copy");
            check(soundLog.size()==20 && logEntry(8,"initialize 1 0 32") && logEntry(17,"initialize 1 6 32") && logEntry(18,"engine")
                && logEntry(19,"playMusic") && playMusicCalls==1,
                "failure retries the current record, queries the engine again and restarts the music once");
            check(manager.queries==0 && seamCalls==0,"silent failure never looks up SwiffHost");
        }
        {
            AudioSettings owner;IAudioSettings* api=&owner;
            manager.queries=0;seamCalls=0;seamText.clear();
            check(!api->apply(true),"reported failure still returns false");
            if(code<=4)
                check(manager.queries==2 && manager.lastName=="SwiffHost" && seamCalls==2 && seamHost==&host && seamType==3 && seamFlag==3 && seamText==messages[code],
                    "each failed record posts the {3, message, 3} SwiffHost record");
            else
                check(manager.queries==0 && seamCalls==0,"an unnamed code reports nothing");
        }
        {
            manager.host=nullptr;
            AudioSettings owner;IAudioSettings* api=&owner;
            manager.queries=0;seamCalls=0;
            check(!api->apply(true) && manager.queries==(code<=4?2u:0u) && seamCalls==0,"absent SwiffHost is looked up and really does nothing");
            manager.host=&host;
        }
    }
    facade.initializeResult=0;classManager=nullptr;g_bfSound=nullptr;
}

// Checks raw interface slot placement and exercises restart, provider-code, and count accessors.
static void checkPhase2Slots()
{
    memorySamples={512u*1024*1024};
    AudioSettings owner;
    IAudioSettings* api=&owner;
    auto& current=AudioSettingsFixture::current(owner);
    auto& pending=AudioSettingsFixture::pending(owner);
    g_bfSound=nullptr;game=nullptr;
    void** table=*reinterpret_cast<void***>(api);
    typedef unsigned char(__thiscall* BoolSlot)(void*);
    typedef int(__thiscall* IntSlot)(void*);
    typedef float(__thiscall* FloatSlot)(void*);
    pending.voipEnabled=true;pending.musicVolume=0.25f;pending.alwaysAllowSystemOpenAL=true;pending.forceNumberOfVoices=24;
    check(reinterpret_cast<BoolSlot>(table[0x38/4])(api)==1 && reinterpret_cast<FloatSlot>(table[0x80/4])(api)==0.25f
        && reinterpret_cast<BoolSlot>(table[0xA8/4])(api)==1 && reinterpret_cast<IntSlot>(table[0xB0/4])(api)==24
        && reinterpret_cast<IntSlot>(table[0x34/4])(api)==0,
        "the table holds getVoipEnabled +0x38, getMusicVolume +0x80, getAlwaysAllowSystemOpenAL +0xA8, getForceNumberOfVoices +0xB0, getStatus +0x34");

    // 4B1090: true when ANY of the five restart fields is equal - false only when all five differ.
    AudioSettingsData a,b;
    check(a.anyRestartFieldEqual(b),"identical records compare as needing a restart");
    b.provider="creative";b.soundQuality="UltraHigh";b.enableEAX=!a.enableEAX;b.alwaysAllowSystemOpenAL=!a.alwaysAllowSystemOpenAL;
    b.forceNumberOfVoices=a.forceNumberOfVoices+1;
    check(!a.anyRestartFieldEqual(b),"only a record differing in all five restart fields answers false");
    for(int keep=0;keep<5;++keep){
        AudioSettingsData c(b);
        if(keep==0)c.provider=a.provider; else if(keep==1)c.soundQuality=a.soundQuality; else if(keep==2)c.enableEAX=a.enableEAX;
        else if(keep==3)c.alwaysAllowSystemOpenAL=a.alwaysAllowSystemOpenAL; else c.forceNumberOfVoices=a.forceNumberOfVoices;
        check(a.anyRestartFieldEqual(c),"any single equal restart field is enough for true (the shipped sense)");
    }

    // +0x28: false outside a game and without changes; otherwise 4B1090's answer.
    pending.copyFrom(current);pending.musicVolume=0.125f;
    check(!reinterpret_cast<BoolSlot>(table[0x28/4])(api),"needsRestart is false outside a game");
    int sentinel=0;game=reinterpret_cast<IGame*>(&sentinel);   // [TEMP] only its nullness is read
    pending.copyFrom(current);
    check(!api->needsRestart(),"needsRestart is false without pending changes");
    pending.musicVolume=0.125f;
    check(api->needsRestart(),"a volume-only change needs a restart: every restart field is still equal");
    pending.copyFrom(current);
    pending.provider=current.provider=="creative"?"software":"creative";pending.soundQuality=current.soundQuality=="High"?"Low":"High";
    pending.enableEAX=!current.enableEAX;pending.alwaysAllowSystemOpenAL=!current.alwaysAllowSystemOpenAL;
    pending.forceNumberOfVoices=current.forceNumberOfVoices+8;
    check(api->hasChanges() && !api->needsRestart(),"changing all five restart fields answers false");
    game=nullptr;

    // +0x2C / +0x30: the pending provider's row value plus the EAX byte, through {6,0,1,2,3,4}.
    struct Case { const char* provider; bool eax; bool hasEax; bool zenith; };
    const Case cases[]={{"software",false,false,false},{"software",true,false,false},{"hardware",false,false,false},
        {"hardware",true,true,false},{"creative",false,false,true},{"creative",true,true,true},
        {"bogus",false,false,false},{"bogus",true,false,false}};
    for(const Case& k:cases){
        pending.provider=k.provider;pending.enableEAX=k.eax;
        check(reinterpret_cast<BoolSlot>(table[0x2C/4])(api)==(k.hasEax?1:0) && reinterpret_cast<BoolSlot>(table[0x30/4])(api)==(k.zenith?1:0),
            "hasEax is provider code 2 or 4, hasZenithBoard code 3 or 4");
    }

    // +0xA0 / +0xA4: the name lists, copied, and their counts - one quality fewer without a Zenith board.
    int count=-7;
    check(api->getProviderNames(&count)==std::vector<std::string>({"software","hardware","creative"}) && count==3,
        "getProviderNames copies the provider names and writes their count");
    check(api->getProviderNames(nullptr).size()==3,"getProviderNames without a count pointer still answers the names");
    pending.provider="hardware";pending.enableEAX=true;count=-7;
    check(api->getSoundQualityNames(&count)==std::vector<std::string>({"Low","Medium","High","UltraHigh"}) && count==3,
        "without a Zenith board UltraHigh falls outside the quality count");
    pending.provider="creative";pending.enableEAX=false;count=-7;
    check(api->getSoundQualityNames(&count).size()==4 && count==4,"a creative provider counts every quality");
}

int main()
{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    check(sizeof(void*)==4,"x86 fixture");
    check(installMemoryObserver(),"install actual import observer");
    checkData();
    memorySamples={512u*1024*1024,1024u*1024*1024};memoryQueries=0;
    for(unsigned iid:{IID_IBase,IID_IAudioSettings}){
        auto* owner=static_cast<AudioSettings*>(AudioSettings::create(iid,nullptr));
        IAudioSettings* api=owner;watchedOwner=owner;frees=0;
        check(memoryQueries%2==0,"constructor makes two independent memory queries");
        check(AudioSettingsFixture::current(*owner).soundQuality=="Low" && AudioSettingsFixture::pending(*owner).soundQuality=="Medium","two resets observe separate memory-query results");
        check(AudioSettingsFixture::current(*owner).provider.empty() && defaults(AudioSettingsFixture::pending(*owner)),"only current provider cleared after resets");
        check(AudioSettingsFixture::enums(*owner) && AudioSettingsFixture::status(*owner)==0,"actual ordered enum metadata and status");
        check(api->getFileName()=="Audio.con","actual filename virtual");
        check(api->getSettingsScript().find("setProvider \"\"\n")!=std::string::npos,"script virtual serializes current record");
        check(api->getRef()==1 && api->queryInterface(IID_IBase)==owner && api->queryInterface(IID_IAudioSettings)==owner,"factory/interface identity and no retain");
        check(!api->queryInterface(0xffffffff) && api->getRef()==1,"unsupported IID does not retain");
        api->addRef();check(api->release()==1 && !frees,"nonfinal release retains owner");
        AudioSettingsFixture::watchDestruction(*owner);freedPointers.clear();freedPointers.reserve(16);watchPointers=true;
        check(api->release()==0,"final release result");watchPointers=false;
        check(frees==1 && freedPointers==std::vector<unsigned>({0,1,2,3,4,5,6,7,8,9}),"actual reverse member/vector destruction order");
        watchedOwner=nullptr;
    }
    const unsigned before=memoryQueries;check(!AudioSettings::create(IID_IAudioSettings,reinterpret_cast<IBase*>(1)) && memoryQueries==before,"aggregation refusal before construction");
    captureOwner=true;frees=0;check(!AudioSettings::create(0xffffffff,nullptr) && frees==1 && !captureOwner,"unknown factory IID releases real native owner");watchedOwner=nullptr;
    checkApply();
    checkApplyRecord();
    checkPhase2Slots();
    std::printf("AudioSettings:%u/%u passed; native owner=%zu data=%zu enum=%zu\n",checks-failures,checks,sizeof(AudioSettings),sizeof(AudioSettingsData),sizeof(AudioSettingsEnum));
    return failures?1:0;
}
