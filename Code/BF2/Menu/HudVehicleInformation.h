// Vehicle HUD state using native string and set storage. [INFERRED] Class, header and field names.
#ifndef MENU_HUDVEHICLEINFORMATION_H
#define MENU_HUDVEHICLEINFORMATION_H
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "../Game/IEventListener.h"
#include <cstdint>
#include <set>
#include <string>
namespace dice { namespace meme { class System; class BfOccupiedVehicleData; class Bf2MenuMapNode; } }
namespace dice { namespace hfe { class HudInformationLayer; namespace world { class IObject; class IPlayer; class IObjectComponent; } } }
namespace dice { namespace hfe { namespace hud {
class BF2HudManager;
class HudBlinkManager;

// [CHOSEN] Typed helper applying vehicle zoom to the map node.
void dispatchHudMapApplyVehicleZoom(meme::Bf2MenuMapNode*);

// [INFERRED] primary-interface carrier. A first polymorphic base keeps the
// separately inherited event listener at +4. All ten observed slots are named;
// the four shared RET-only slots have no observed arguments or return value.
class HudVehicleInformationPrimary {
public:
    virtual void setSystem(meme::System*) = 0;
    virtual bool init() = 0;
    virtual bool refresh() = 0; // +08 5A5E20, real constant true
    virtual void unknown0C() = 0; // +0C 83D270, observed empty body
    virtual void unknown10() = 0;
    virtual void unknown14() = 0;
    virtual void unknown18() = 0;
    virtual void update(float) = 0; // +1C 78D0F0
    virtual bool updatePostProcessing() = 0;
    virtual int getGuiIndex() = 0;
protected:
    ~HudVehicleInformationPrimary() = default;
};

class HudVehicleInformation : public HudVehicleInformationPrimary, public IEventListener {
public:
    HudVehicleInformation(); // 78C560; publication remains withheld by outer HUD guard
    ~HudVehicleInformation(); // 78BE00, nonvirtual
    void setSystem(meme::System*) override;
    bool init() override; // 789480, occupied-data/binding family
    bool refresh() override;
    void unknown0C() override;
    void unknown10() override;
    void unknown14() override;
    void unknown18() override;
    void update(float) override; // 78D0F0; every frame helper below is written
    bool updatePostProcessing() override; // 78B1E0; no caller in BF2.exe 1.50
    int getGuiIndex() override;
    void handleEvent(EventCategory, unsigned int, EventNode*, void*) override;
    void reset(bool keepKilledState);
    void clear();
    void setCloseToMedic(bool);
    void setCloseToRepair(bool);
    void setCloseToAmmo(bool);
    void setMouseLook(bool); // 78A790; [INFERRED] name, actual camera/HUD transition
    bool canSelectWeapon(bool checkVehicle); // 78A830; [INFERRED] name
    // [INFERRED] name. Recomputes m_field170, then the index. Public: Soldier's revive calls it
    // on the client (BF2.exe 0x5A796A).
    void updateHudMode();
private:
    // [INFERRED] names, caller-proven shapes; all bodies are transcribed (HudVehicleFrontiers.cpp,
    // except updateProximity, which lives in HudVehicleInformation.cpp).
    void updateLayout();
    void updateProximity(float);
    void updatePlayerInformation(world::IObject*);
    void updatePlayerAngles(world::IPlayer*);
    // [INFERRED] name. - the occupant-strip display timer over m_field018
    // (elapsed), m_field01c (window) and m_field020 (armed).
    float advanceOccupantTimer(float deltaTime);
    // [INFERRED] name. - the armour/sprint colour pass and the vehicle icon.
    void updateVehicleArmor(world::IObject*);
    void updateVehicleInformation(world::IObject*, float);
    void updateVehicleAngles(world::IObject*);
    // Routes game and HUD event payloads.
    void handleGameEvent(unsigned int, EventNode*, void*);
    void handleHudEvent(unsigned int, EventNode*, void*);
    void refreshVehicle(); // 78D8F0, actual wrapper
    // HUD state and vehicle-index helpers. [INFERRED] Names.
    void applyHudState(int mode);  // the ECHud 2 worker
    void updateGuiIndex();         // recomputes m_guiIndex from the HUD component
    // Bodies in HudVehicleFrontiers.cpp; all written.
    // [INFERRED] name. - the occupant icon pass, driven both on entry
    // (setupOccupiedVehicle) and per frame (updateVehicleInformation, 78CE60).
    void updateOccupantIcons(world::IObject*);
    void setupVehicle(world::IObject*);
    void setupOccupiedVehicle(world::IObject*, bool isLocalPlayer); // 78D840, handleGameEvent case 9's tail
    // [INFERRED] Split event helpers share the router's single local-player lookup.
    void handleMissileLockEvent(world::IPlayer* local, void* data);                 // 78C037, case 44
    void handleCrossHairEvent(unsigned int id, world::IPlayer* local, void* data);
    void assignString(char*&, const char*); // 78A6D0, inferred member spelling
    void assignString(wchar_t*&, const wchar_t*); // 78A740, the wide twin of the above
    friend class BF2HudManager;
    // HudAmmo's two weapon channels read the sprint flag straight off this object
    // (PE 7A883A / 7A8E71 / 7A92FB load ds:A10890 +24C then byte +244).
    friend class HudAmmo;
    friend class ::dice::hfe::HudInformationLayer;
    friend struct HudVehicleInformationFixture;
    meme::System* m_system; // +08, borrowed
    std::uint32_t m_unknown00c; // [UNRESOLVED], untouched by ctor
    HudBlinkManager* m_blinkManager; // +10, owned
    meme::BfOccupiedVehicleData* m_occupiedData; // +14, assigned by init; not deleted here
    float m_field018, m_field01c; // ctor only18=0
    bool m_field020, m_field021;
    float m_field024; // ctor1
    std::string m_connectionMessage; // +28, native STL
    std::uint32_t m_unknown044, m_unknown048; // untouched
    int m_occupantX[10]; // +4C, untouched until setup
    int m_occupantY[10]; // +74
    // Vehicle climb-speed limit used by the HUD; defaults to 10.
    int m_maxClimbSpeed; // +9C=10
    int m_guiIndex, m_field0a4, m_field0a8, m_field0ac, m_field0b0;
    float m_field0b4;
    bool m_usingParachute; // +B8
    float m_field0bc; // -1
    std::uint32_t m_unknown0c0; // untouched
    char* m_occupantNames[10]; // +C4, owned or sentinel
    bool m_occupantShow[10]; // +EC
    float m_occupantColors[10][3]; // +F8
    // Integer HUD mode tested by state and GUI-index updates.
    int m_field170;
    // Integer layout mode selected by updateLayout.
    int   m_field174;
    float m_field178, m_field17c, m_field180, m_field184, m_field188;
    float m_bottomLeftHealthAlpha, m_bottomLeftVehicleAlpha; // +18C,+190
    float m_bottomLeftHealthFadedAlpha, m_bottomLeftVehicleFadedAlpha; // +194,+198, untouched
    float m_field19c, m_field1a0;
    std::uint32_t m_unknown1a4; // untouched
    float m_playerHealth, m_playerStamina, m_nightVisionGauge, m_vehicleStamina, m_vehicleArmor;
    float m_playerAngle, m_playerBanking, m_playerElevation;
    float m_vehicleAngle, m_vehicleTurretAngle, m_vehicleBanking, m_vehicleElevation;
    float m_vehicleElevationSpeedAngle, m_unknown1dc, m_gunnerAngle, m_torque, m_field1e8, m_torqueAngle;
    int m_angleOfAttack; // +1F0
    float m_playerHealthColor[4], m_vehicleArmorColor[4]; // +1F4,+204 initialized white
    float m_playerSprintColor[4], m_vehicleSprintColor[4], m_nightVisionColor[4]; // untouched
    // m_field244 is the sprint flag. updateVehicleInformation (78D047) stores
    // `pco->getIsSprinting()` into it and posts ECHud 0x1F on every change. Name left as the
    // placeholder only because HudVehicleInformation.cpp:345 already reads it under that spelling.
    bool m_field244, m_playerStaminaShow, m_playerStaminaBlink, m_nightVisionGaugeShow;
    bool m_nightVisionGaugeBlink, m_vehicleStaminaShow, m_vehicleStaminaBlink, m_playerHealthShow;
    bool m_playerHealthBlink, m_unknown24d, m_vehiclePassengersShow, m_vehicleIconShow;
    bool m_vehicleArmorShow, m_vehicleArmorBlink, m_turretIconShow, m_killedState, m_warningIconShow;
    std::set<int> m_warningSources; // +258, native RB tree; four-byte payload at node+C
    bool m_referenceCrossShow; // +264
    char* m_warningIconPath; // +268, owned/sentinel
    char* m_vehicleArmorString;
    char* m_playerHealthString;
    char* m_localKitName;
    char* m_vehicleName;
    char* m_vehicleIconPath;
    char* m_speedString;
    char* m_altitudeString;
    char* m_torqueString;
    // Raw torque input; updateVehicleInformation approaches its nonnegative value at 0.01 per call.
    float m_unknown28c; // untouched by ctor; filter752F50 stores mapped axis3
    bool m_closeToMedic, m_closeToRepair, m_closeToAmmo, m_field293, m_field294;
    bool m_squadInfoBarShow, m_showCommanderIcon, m_showSquadIcon;
    wchar_t* m_squadInfoText; // +298, owned/wide sentinel
    char* m_squadInfoIconPath; // +29C, owned/narrow sentinel
    int m_crossHairMode; // +2A0
    float m_field2a4, m_field2a8;
    bool m_field2ac;
    static char s_emptyString; // PE A1DF82, BSS zero byte, address sentinel
    static wchar_t s_emptyWideString; // PE A1DF80, BSS zero word
};
} } }
#endif
#endif
