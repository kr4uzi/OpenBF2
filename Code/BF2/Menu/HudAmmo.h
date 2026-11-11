// Ammo and weapon-status HUD state. [INFERRED] Class, header and member names.
#ifndef MENU_HUDAMMO_H
#define MENU_HUDAMMO_H
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "../Game/IEventListener.h"
#include <cstdint>
#include <string>
namespace dice { namespace meme { class System; class Bf2ObjectSelectionNode; } }
namespace dice { namespace hfe { struct PlayerVOHelp; } }
namespace dice { namespace hfe { namespace world {
class IObject; class IPlayer; class IPlayerControlObject; class IWeaponObject; class IAbilityGLComp;
} } }
namespace dice { namespace hfe { namespace hud {
class BF2HudManager;
class HudBlinkManager;
class IHudAmmo {
public:
    virtual void setSystem(meme::System*) = 0;
    virtual bool registerData() = 0;
    virtual bool registerFunctions() = 0;
    virtual void unknown0C() = 0;              // +0C 83D270, RET only
    virtual void unknown10() = 0;
    virtual void unknown14() = 0;
    virtual void update(float) = 0;
};
class HudAmmo : public IHudAmmo, public IEventListener {
public:
    explicit HudAmmo(meme::System*); // 7A5B10; ECGame/ECHud subscriptions, native blink owners
    ~HudAmmo(); // 7A60B0, nonvirtual explicit HUD ownership
    void setSystem(meme::System*) override;
    bool registerData() override;
    bool registerFunctions() override;
    void unknown0C() override;
    void unknown10() override;
    void unknown14() override;
    void update(float) override; // 7AA400; the weapon display below it is written (RW-160, ha_ammo)
    void handleEvent(EventCategory, unsigned int, EventNode*, void*) override;
    void hide(bool updatePointer); // 7A8280; name and void spelling [INFERRED]
    static bool canChangeWeapon(world::IObject*); // 7A5400, ECX unused; name inferred
private:
    friend class BF2HudManager;
    friend struct ::dice::hfe::PlayerVOHelp;
    friend struct HudAmmoFixture;
    friend struct HudAmmoConstructionFixture;
    friend struct HudAmmoFrameFixture;
    // Bf2ObjectSelectionNode::updateSelf (7CBC8D..7CBC93) reads +0xA0 straight off
    // BF2HudManager::getAmmo(), with no accessor call - the same field enablePointer reads.
    friend class meme::Bf2ObjectSelectionNode;
    static void updateString(const char**, const char*);
    // the wide twin, TargetDistanceString's (+190) only writer. [INFERRED] overload
    // spelling. Like 7A5340 it is a `ret 8` body that every caller hands ECX (`mov ecx, esi`) and
    // that never reads it; both are kept static, which changes no behaviour.
    static void updateString(const wchar_t**, const wchar_t*);
    // Nonvirtual helper names/void return spellings are [INFERRED] from these callers.
    void handleGameEvent(unsigned int, EventNode*, void*);
    void handleHudEvent(unsigned int, EventNode*, void*);
    void setHudMode(int);
    void updatePointer(bool);
    void updateCrosshair(world::IObject*);
    static void stopAbilitySounds(world::IObject*); // 7A59D0; ECX unused
    void updateWeapon(world::IObject*);
    // 7A5470. [INFERRED] name: the body hunts the PIFlareFire weapon and drives
    // FlaresReloadTimeShow/+188. Called with ECX live from updateWeapon's prologue.
    void updateFlares(world::IObject*);
    // 7A9090. [INFERRED] name. MSVC emitted this 48-byte helper once and inlined it
    // again at 7A9A3A inside updateWeapon; both call sites are the same source call.
    void syncHudMode();
    // channel 0 and channel 1 of the active player-control object.
    // [INFERRED] names, from the member blocks each one writes (Primary* vs Secondary*).
    bool updatePrimaryWeapon(world::IObject*);   // 7A85F0 (written, RW-160)
    // The leaves those two channels share. [INFERRED] names, from the members each writes.
    std::string getFireRateIconPath(int fireRate);          // 7A5560, hidden sret
    void updateCrosshairPositions(world::IWeaponObject*);
    void updatePrimaryClips(int clips);
    void updateSecondaryClips(int clips);
    void updateBombFuel(world::IWeaponObject*);             // 7A79D0 (whole body, ha_ammo)
    void updateRangeFinder(world::IObject*, bool firing);   // 7A7B80 (whole body, ha_ammo)
    void updateSecondaryWeapon(world::IObject*); // 7A90C0 (written, RW-160)
    // [CHOSEN, not recovered] caller-shaped float carrier:7AA4E0 pushes delta,
    // but7A9E70 consumes that stack word without reading its payload.
    void updateAbility(float); // 7A9E70; the consumed argument remains unused
    // Drives the five VoiceOver* booleans at +1BC..+1C0 from the local
    // player's three VoIP flags and the voice client's channel state.
    void updateVoipIcons();
    static world::IPlayerControlObject* findControlParent(world::IObject*);
    static world::IWeaponObject* findAbilityWeapon(world::IPlayer*);
    static bool isReplenishingWeapon(world::IWeaponObject*);
    float getRepairAbility(world::IPlayer*, world::IAbilityGLComp*);
    meme::System* m_system; // +008
    HudBlinkManager* m_blinkManager; // +00C
    float m_field010; // +010 BottomRightAlpha
    float m_field014; // +014 BottomRightFadedAlpha
    bool m_field018; // +018 BottomRightDirection
    float m_field01c; // +01C
    float m_field020; // +020
    float m_field024; // +024
    float m_field028; // +028
    float m_field02c; // +02C
    float m_field030; // +030 PrimaryAmmoColorRed
    float m_field034; // +034 PrimaryAmmoColorGreen
    float m_field038; // +038 PrimaryAmmoColorBlue
    float m_field03c; // +03C PrimaryAmmoColorAlpha
    float m_field040; // +040 PrimaryClipColorRed
    float m_field044; // +044 PrimaryClipColorGreen
    float m_field048; // +048 PrimaryClipColorBlue
    float m_field04c; // +04C PrimaryClipColorAlpha
    float m_field050; // +050 SecondaryAmmoColorRed
    float m_field054; // +054 SecondaryAmmoColorGreen
    float m_field058; // +058 SecondaryAmmoColorBlue
    float m_field05c; // +05C SecondaryAmmoColorAlpha
    float m_field060; // +060 SecondaryClipColorRed
    float m_field064; // +064 SecondaryClipColorGreen
    float m_field068; // +068 SecondaryClipColorBlue
    float m_field06c; // +06C SecondaryClipColorAlpha
    float m_field070; // +070 PrimaryHeatColorRed
    float m_field074; // +074 PrimaryHeatColorGreen
    float m_field078; // +078 PrimaryHeatColorBlue
    float m_field07c; // +07C PrimaryHeatColorAlpha
    float m_field080; // +080 SecondaryHeatColorRed
    float m_field084; // +084 SecondaryHeatColorGreen
    float m_field088; // +088 SecondaryHeatColorBlue
    float m_field08c; // +08C SecondaryHeatColorAlpha
    float m_field090; // +090
    float m_field094; // +094
    int m_field098; // +098 MissileTargetLockId
    float m_field09c; // +09C
    int m_field0a0; // +0A0 crosshair icon size (32, or WeaponHudInfo::crosshairIconSize); the pointer size in modes 0/10
    bool m_field0a4; // +0A4
    std::uint32_t m_field0a8; // +0A8 [UNRESOLVED] untouched word
    bool m_field0ac; // +0AC PrimaryAmmoBarShow
    bool m_field0ad; // +0AD PrimaryClipsShow
    bool m_field0ae; // +0AE SecondaryAmmoBarShow
    bool m_field0af; // +0AF SecondaryClipsShow
    bool m_field0b0; // +0B0
    float m_field0b4; // +0B4 PrimaryAmmo
    float m_field0b8; // +0B8 PrimaryReloadTime
    bool m_field0bc; // +0BC PrimaryReloadTimeShow
    bool m_field0bd; // +0BD PrimaryEternalShow
    bool m_field0be; // +0BE PrimaryEternalMagsShow
    bool m_field0bf; // +0BF PrimaryGrenadeLoadShow
    bool m_field0c0; // +0C0 SecondaryGrenadeLoadShow
    bool m_field0c1; // +0C1 PrimarySingleFireShow
    float m_field0c4; // +0C4 SecondaryAmmo
    float m_field0c8; // +0C8 SecondaryReloadTime
    bool m_field0cc; // +0CC SecondaryReloadTimeShow
    bool m_field0cd; // +0CD SecondaryEternalShow
    bool m_field0ce; // +0CE SecondaryEternalMagsShow
    bool m_field0cf; // +0CF SecondarySingleFireShow
    float m_field0d0; // +0D0 BombFuel
    float m_field0d4; // +0D4 LaserTargetRed
    float m_field0d8; // +0D8 LaserTargetGreen
    float m_field0dc; // +0DC LaserTargetBlue
    float m_field0e0; // +0E0 LaserTargetAlpha
    int m_field0e4; // +0E4 FiringIndex
    bool m_field0e8; // +0E8 CrosshairIconShow
    bool m_field0e9; // +0E9 CrosshairActive
    bool m_field0ea; // +0EA
    bool m_field0eb; // +0EB HitIndicatorIconShow
    bool m_field0ec; // +0EC SecondaryHeatShow
    bool m_field0ed; // +0ED PrimaryHeatShow
    bool m_field0ee; // +0EE WrenchRepairingShow
    float m_field0f0; // +0F0 SecondaryHeatValue
    float m_field0f4; // +0F4 PrimaryHeatValue
    float m_field0f8; // +0F8
    float m_field0fc; // +0FC HitIndicatorIconAlpha
    int m_field100; // +100
    int m_field104; // +104
    int m_field108; // +108
    int m_field10c; // +10C CrosshairUpPos
    int m_field110; // +110 CrosshairDownPos
    int m_field114; // +114 CrosshairLeftPos
    int m_field118; // +118 CrosshairRightPos
    int m_field11c; // +11C
    int m_field120; // +120
    int m_field124; // +124
    int m_field128; // +128
    bool m_field12c; // +12C
    bool m_field12d; // +12D PrimaryAmmoShow
    bool m_field12e; // +12E PrimaryAmmoBlink
    bool m_field12f; // +12F PrimaryHeatBlink
    bool m_field130; // +130 PrimaryAmmoIconShow
    bool m_field131; // +131 PrimaryClip1Show
    bool m_field132; // +132 PrimaryClip2Show
    bool m_field133; // +133 PrimaryClip3Show
    bool m_field134; // +134 PrimaryClipStringShow
    bool m_field135; // +135 PrimaryClipBlink
    bool m_field136; // +136 SecondaryAmmoShow
    bool m_field137; // +137 SecondaryAmmoBlink
    bool m_field138; // +138 SecondaryHeatBlink
    bool m_field139; // +139 SecondaryAmmoIconShow
    bool m_field13a; // +13A
    bool m_field13b; // +13B SecondaryClip1Show
    bool m_field13c; // +13C SecondaryClip2Show
    bool m_field13d; // +13D SecondaryClip3Show
    bool m_field13e; // +13E SecondaryClipStringShow
    bool m_field13f; // +13F SecondaryClipBlink
    float m_field140; // +140 PrimaryClips
    const char* m_field144; // +144 PrimaryAmmoString
    const char* m_field148; // +148 PrimaryClipString
    const char* m_field14c; // +14C PrimaryClipPathString
    const char* m_field150; // +150 PrimaryAmmoIconPathString
    const char* m_field154; // +154 PrimaryAmmoBarFrontPathString
    const char* m_field158; // +158 PrimaryAmmoBarBackPathString
    float m_field15c; // +15C SecondaryClips
    const char* m_field160; // +160 SecondaryAmmoString
    const char* m_field164; // +164 SecondaryClipString
    const char* m_field168; // +168 SecondaryClipPathString
    const char* m_field16c; // +16C SecondaryAmmoIconPathString
    const char* m_field170; // +170 SecondaryAmmoBarFrontPathString
    const char* m_field174; // +174 SecondaryAmmoBarBackPathString
    float m_field178; // +178 TargetDirection
    const char* m_field17c; // +17C PrimaryWeaponNameString
    const char* m_field180; // +180 SecondaryWeaponNameString
    bool m_field184; // +184 FlaresReloadTimeShow
    float m_field188; // +188 FlaresReloadTime
    bool m_field18c; // +18C FireRateIconShow
    const wchar_t* m_field190; // +190 TargetDistanceString
    const char* m_field194; // +194 TargetCoordXString
    const char* m_field198; // +198 TargetCoordYString
    const char* m_field19c; // +19C TargetCoordZString
    const char* m_field1a0; // +1A0 CrosshairIconPathString
    const char* m_field1a4; // +1A4 FireRateIconPathString
    float m_field1a8; // +1A8
    float m_field1ac; // +1AC AbilityBar
    bool m_field1b0; // +1B0 AbilityBarShow
    bool m_field1b1; // +1B1 AbilityValueShow
    const char* m_field1b4; // +1B4 AbilityIconPath
    float m_field1b8; // +1B8 AbilityValue
    bool m_field1bc; // +1BC VoiceOverIconShow
    bool m_field1bd; // +1BD VoiceOverTalkIconShow
    bool m_field1be; // +1BE VoiceOverDistIconShow
    bool m_field1bf; // +1BF VoiceOverCommanderIconShow
    bool m_field1c0; // +1C0 VoiceOverMuted
    bool m_field1c1; // +1C1 HasMissileConnection
    bool m_field1c2; // +1C2
    static char s_emptyString; // address-only BSS sentinel
    static wchar_t s_emptyWideString; // distinct BSS sentinel
};
} } }
#endif
#endif
