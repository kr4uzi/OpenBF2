// [TEMP] [CHOSEN, not recovered] Actual parser/HUD behavior with a bounded
// control-map lookup and binding-description fixture. No HUD body is shadowed.
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <cstdint>
#include <clocale>
#include "../BF2/IO/Input/Input.h"
#if !defined(INPUT_STREAM_ONLY)
#define private public
#include "../BF2/Menu/BF2HudManager.h"
#include "../BF2/Menu/HudBlinker.h"
#include "../BF2/IO/Localization/ExcelLexicon.h"
#undef private
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "../BF2/Game/ControlSettings.h"
namespace dice { namespace hfe {
hud::BF2HudManager* hudManager = nullptr;
IBase* g_controlSettings = nullptr;
namespace io { IBase* lexicon = nullptr; IBase* controlMapManager = nullptr; }
} }
using namespace dice::hfe;
using namespace dice::hfe::io;
namespace {
int gameMarker, playerMarker, activeMarker;
IControlMap* gameMap = reinterpret_cast<IControlMap*>(&gameMarker);
IControlMap* playerMap = reinterpret_cast<IControlMap*>(&playerMarker);
IControlMap* activeMap = reinterpret_cast<IControlMap*>(&activeMarker);
struct BindingCall { IControlMap* map; int id; bool alternate, second, game; };
std::vector<BindingCall> calls;
bool fallback = false, localizedBinding = false;
// Opaque identifiers above are never dereferenced; this fixture models lookups
// only. Every unrelated virtual call stops immediately.
struct Maps : IControlMapManager {
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    void update(float) override { std::abort(); }
    IControlMap* create(const std::string&) override { std::abort(); }
    IControlMap* getControlMap(const std::string& name) const override {
        if (name == "defaultGameControlMap") return gameMap;
        if (name == "defaultPlayerInputControlMap") return playerMap;
        std::abort();
    }
    IControlMap* getControlMap(int) const override { std::abort(); }
    void deleteControlMap(const std::string&) override { std::abort(); }
    void deleteControlMap(int) override { std::abort(); }
    void deleteControlMap(const IControlMap*) override { std::abort(); }
    void setActive(const std::string&) override { std::abort(); }
    void setActive(const IControlMap*) override { std::abort(); }
    IControlMap* getActive() const override { return activeMap; }
    int mapNextInput(IControlMap*, int, bool, bool, bool, bool, std::string&) override { std::abort(); }
    float getRepeatRate(int, bool) override { std::abort(); }
    void applyActive() override { std::abort(); }
};
}
// [TEMP] The ControlSettings singleton as the HUD sees it: only the binding-description slot
// answers; every other slot of the 38-slot table stops immediately.
struct Bindings : IControlSettings {
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    std::string getFileName() override { std::abort(); }
    std::string getSettingsScript() override { std::abort(); }
    bool restoreControlMaps() override { std::abort(); }
    void backupControlMaps() override { std::abort(); }
    void backupControlMapsByName() override { std::abort(); }
    bool setDefaults() override { std::abort(); }
    void resetControlMaps() override { std::abort(); }
    void updateInputDevices() override { std::abort(); }
    int mapNextInput(int, bool, int, bool, bool) override { std::abort(); }
    bool selectControlMap(const std::string&) override { std::abort(); }
    bool getBindingDescription(IControlMap* map, int id, bool alternate, bool second, bool game,
                               std::string& key, std::wstring& text) override
    {
        calls.push_back({map, id, alternate, second, game});
        if (fallback && map != playerMap) return false;
        key = "BINDING";
        if (!localizedBinding) text = second ? L"KEY_Second" : L"KEY_First";
        return true;
    }
    std::string getActiveControlMapName() override { std::abort(); }
#define CS_FLOAT_PAIR(G,S) float G() override { std::abort(); } void S(float) override { std::abort(); }
    CS_FLOAT_PAIR(getMouseSensitivity,setMouseSensitivity) CS_FLOAT_PAIR(getSlot48,setSlot4C) CS_FLOAT_PAIR(getSlot50,setSlot54)
    CS_FLOAT_PAIR(getYawFactor,setYawFactor) CS_FLOAT_PAIR(getPitchFactor,setPitchFactor)
    bool getInvertMouse() override { std::abort(); } void setInvertMouse(bool) override { std::abort(); }
    CS_FLOAT_PAIR(getKeyboardSensitivity,setKeyboardSensitivity)
#undef CS_FLOAT_PAIR
    bool apply() override { std::abort(); }
    void deleteControlMapBackups() override { std::abort(); }
    void activateMouse() override { std::abort(); }
    void deactivateMouse() override { std::abort(); }
    bool isMappingInput() override { std::abort(); }
    void setMappingInput(bool) override { std::abort(); }
    std::string getMappedInputName() override { std::abort(); }
};
#endif
int main()
{
    using namespace dice::hfe::io;
    std::setlocale(LC_ALL, "C");
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char* name) {
        ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", name); }
    };
    auto pi = [&](const std::string& text) { PlayerInputMap p; std::istringstream s(text); s >> p; return p; };
    auto gi = [&](const std::string& text) { GameInputMap p; std::istringstream s(text); s >> p; return p; };
    check(pi("PIFire") == PIFire && pi("8") == PIFire, "PI names and numeric binding");
    check(pi("8x") == PIFire && pi("+8") == PIFire, "sscanf accepts two-character partial/signed values");
    check(pi("008") == PINone && pi("-1") == PINone, "PI long numeric and negative sentinel");
    check(pi("PINumInput") == PINone && pi("PINotInUse") == PINone, "PI sentinel aliases");
    check(pi("pifire") == PINone && pi("unknown") == PINone, "PI case-sensitive and unknown");
    check(gi("GIVoipPushToTalk") == GIVoipPushToTalk && gi("61") == GIVoipPushToTalk, "GI reordered voice id");
    check(gi("GIVoipUseLeaderChannel") == GIVoipUseLeaderChannel && gi("56") == GIVoipUseLeaderChannel, "GI leader id");
    check(gi("GILeader") == GILeader && gi("60") == GILeader, "GI late comparison");
    check(gi("73") == GIScript12 && gi("74") == GINone && gi("99") == GINone, "GI last valid and sentinel");
    check(gi("GINumInput") == GINone && gi("gimenu") == GINone, "GI sentinel alias and case");
    for (int i = 0; i <= 64; ++i) {
        std::ostringstream s; s << static_cast<PlayerInputMap>(i);
        check(!s.str().empty() && pi(s.str()) == i, "PI name table round trip");
    }
    for (int i = 0; i <= 74; ++i) {
        std::ostringstream s; s << static_cast<GameInputMap>(i);
        check(!s.str().empty() && gi(s.str()) == i, "GI name table round trip");
    }
    std::ostringstream unknown; unknown << static_cast<PlayerInputMap>(-1) << static_cast<GameInputMap>(75);
    check(unknown.str().empty(), "out-of-range writers produce no text");
    PlayerInputMap p; GameInputMap g;
    std::wistringstream wide(L"PIFire 61"); wide >> p >> g;
    check(p == PIFire && g == GIVoipPushToTalk, "wide readers use same numeric/name grammar");
    std::istringstream empty; empty >> p;
    check(empty.fail() && p == PINone, "empty extraction preserves stream fail and stores sentinel");
#if !defined(INPUT_STREAM_ONLY)
    Maps maps; io::controlMapManager = &maps;
    Bindings bindings; g_controlSettings = &bindings;
    io::ExcelLexicon lex; io::lexicon = &lex;
    lex.m_strings.insert("BINDING", L"KEY_Localized");
    void* storage = ::operator new(sizeof(hud::BF2HudManager));
    hud::BF2HudManager* h = new (storage) hud::BF2HudManager;
    hudManager = h;
    check(h->replaceInputKeys("Press #PIFire#") == "Press First", "narrow player key");
    check(calls.size() == 1 && calls[0].map == activeMap && calls[0].id == PIFire && !calls[0].game && !calls[0].alternate, "active player lookup");
    calls.clear();
    check(h->replaceWideInputKeys(L"#PIUse# then #GIMenu_2#") == L"First then Second", "mixed wide key replacement");
    check(calls.size() == 2 && calls[0].game && calls[0].second && calls[0].map == gameMap && calls[1].id == PIUse, "GI preferred even after PI");
    calls.clear(); fallback = true;
    check(h->replaceInputKeys("#GIMenu_2#") == "Second", "fallback player map");
    check(calls.size() == 2 && calls[1].map == playerMap && calls[1].game && calls[1].second, "fallback preserves flags");
    calls.clear(); fallback = false; localizedBinding = true;
    check(h->replaceWideInputKeys(L"#PIFire#") == L"Localized", "empty binding label is localized then prefix stripped");
    calls.clear();
    check(h->replaceInputKeys("#PIFire") == "#PIFire" && calls.empty(), "unterminated token untouched");
    check(h->replaceWideInputKeys(L"plain") == L"plain" && calls.empty(), "plain text untouched");
    check(h->replaceInputKeys("#PIUnknown#") == "Localized" && calls.back().id == PINone, "unknown PI reaches retail sentinel binding lookup");
    hudManager = nullptr; io::lexicon = nullptr; io::controlMapManager = nullptr; g_controlSettings = nullptr;
    // The HUD destructor still owns unrecovered widgets. Tear down only the real
    // constructor's objects/containers; this fixture never claims HUD shutdown.
    delete h->m_field2dc;
    using WS = std::wstring; using S = std::string;
    using WL = std::list<int>; using SL = std::list<std::string>;
    using WV = std::vector<hud::HudPendingRequest>;   // m_field32c's current element type (BF2HudManager.h)
    h->m_field010.~WS(); h->m_field038.~WS(); h->m_field060.~WS();
    h->m_field08c.~S(); h->m_field0c0.~S(); h->m_field304.~S();
    h->m_field0e8.~WL(); h->m_field2e0.~SL(); h->m_field33c.~SL(); h->m_field32c.~WV();
    ::operator delete(storage);
#endif
    std::printf("HUD input streams/keys: %d/%d PASS\n", checks - failures, checks);
    return failures ? 1 : 0;
}
