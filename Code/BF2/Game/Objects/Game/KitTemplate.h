// Soldier loadout configuration and unlock settings.
// This world template is separate from ai::KitTemplate. [INFERRED] Source placement.

#ifndef GAME_OBJECTS_KITTEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_KITTEMPLATE_H

#include "../ItemTemplate.h"
#include <iosfwd>     // the KitType stream pair below
#include <string>
#include <vector>
#include <utility>
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // VehicleMod - m_kitVehicleMods

namespace dice {
namespace hfe {

class Player;                                // reads m_creationIndex raw - see the friend below
class HudInformationLayer;
namespace hud { class HudAmmo; }

// `dice::hfe::KitType` - the vocabulary `ObjectTemplate.kitType` writes into KitTemplate::m_kitType
// and Kit::getType / Soldier::getKitType hand back (mangled `NS0_7KitType`: hfe scope, not world).
// Opaque, exactly as the AI headers already declare it: the seven stock values and their tokens are
// attested by the world:: stream pair (0 Specops, 1 Sniper, 2 Assault, 3 Support, 4 Engineer,
// 5 Medic, 6 AT; 13 is every "no kit" answer), the source enumerator identifiers are not.
enum KitType : int;

namespace world {

class IObject;

// The console vocabulary of `ObjectTemplate.kitType` (ConsoleClass952 of ModuleWorldObjTemplGame,
// argType "hfe::KitType"): ELF 0x857E120 / 0x857E1A0, BF2.exe 0x506870 / 0x508B9A. Defined with
// that TU's other stream pairs in ModuleWorldObjTemplGame.cpp. Distinct from the AI's
// ai::operator>>(std::istream&, KitType&) (0x842CCA0), which also accepts Extended1..6.
std::ostream& operator<<(std::ostream& stream, KitType type);
std::istream& operator>>(std::istream& stream, KitType& type);

// Reads creation-event values from the live Kit. The selected unlock level is per-kit state.
unsigned int kitCreationIndex(IObject* kit);
int          kitChosenUnlockLevel(IObject* kit);

class KitTemplate : public ItemTemplate
{
public:
    // Initializes kit type, weapon selection, dropped geometry, lifetime and disabled ability defaults.
    KitTemplate();

    // the 46 bytes the kit front was waiting on.
    virtual unsigned int getClassID() const;              // +0x10
    virtual void* queryImplementation(unsigned int cid);  // +0x50

    // Creates a Kit with its kit-specific replication component.
    // [RETIRE-WHEN: kit-class]
    virtual IObject* createObject() const;                // +0x54 retail 47 B: new(0x188) Kit

    // Resolves each child by name and uses its authored relative transformation unchanged.
    virtual void addBundleChilds(IObject* object) const;

    // Player::getUnlockLevel / isUnlocked key the profile-unlock maps by m_creationIndex, read
    // RAW off this template (`mov eax,[template+1A8h]`, 0x80F6240/0x80F6190) - no getter exists in
    // the ELF (only Kit::getCreationIndex, which reads the same field through the kit's template
    // pointer). Friend rather than an invented accessor, the RotationalBundleTemplate precedent.
    friend class ::dice::hfe::Player;
    friend class ::dice::hfe::HudInformationLayer;
    // kitCreationIndex above is that same Kit::getCreationIndex read, and it is inlined in the ELF
    // too - so there is no accessor for it to mirror either. Same treatment.
    friend unsigned int kitCreationIndex(IObject* kit);
    // Kit reads configuration and vehicle modifiers directly.
    friend class Kit;
    // Soldier reads the default weapon index when selecting its fallback weapon.
    friend class Soldier;
    friend class hud::HudAmmo; // PE7A9B00: in-vehicle material/strength/radius

    friend struct GameTemplateConsoleAccess;     // recovered ObjectTemplate.* console executors

    // NON-virtual (no vtable reference anywhere in the image; both of its callers
    // reach it with a direct call, one of them Kit::getType's tail jump).
    int getKitType() const;

    // Console setters that use accessors.
    void setKitType(int type) { m_kitType = type; }
    void setDropHeadwear(const std::string& value) { m_dropHeadwear = value; }

    // Stores the restore rate and rate/30. Rates of 0.2 or greater terminate the process.
    void setAbilityRestoreRate(float rate);

    // appends one (name, mod) pair to m_kitVehicleMods. The console binding is a
    // two-argument method (std::string, int), argc exactly 2, isDynamic false.
    void addKitVehicleMod(std::string name, int mod);

    // ; non-virtual, non-const. Body in the attested kit.cpp TU.
    int getHighestUnlockLevel();
    std::vector<IObjectTemplate*>* getUnlockItems(int level); // ELF8186080/client595C70

protected:
    // layout, offsets 408..495. ItemTemplate occupies 0..407.
    // Names below come from the console property -> field offset join:
    // the .con property that writes each offset is known, so these are read, not guessed.
    /* 408 */ int   m_defaultGeomIndex;              // console property is `geometry.kit`
    /* 412 */ float m_timeToLiveAfterDeath;          // ctor 30.0f
    /* 416 */ int   m_kitType;
    /* 420 */ int   m_kitTeam;                       // was `_dword1A4`
    // The console property unlockIndex maps to m_creationIndex.
    /* 424 */ int   m_creationIndex;                 // .con property name: `unlockIndex`
    /* 428 */ int   m_defaultWeaponIndex;            // was `_dword1AC`
    /* 432 */ int   m_dropGeom;                      // was `_dword1B0`; property `geometry.dropGeom`
    // Optional headwear template name used when the kit is dropped.
    /* 436 */ std::string m_dropHeadwear;            // ctor ""
    /* 440 */ float m_sprintStaminaDissipationFactor; // ctor 1.0f

    // Vehicle modifications are stored as string/value pairs in a native vector.
    // Original ELF element and vector sizes do not apply to native MSVC STL storage.
    /* 444 */ std::vector<std::pair<std::string, VehicleMod> > m_kitVehicleMods;
                                                     // ctor empty. console binding: addKitVehicleMod
                                                     // (444..455 in the ELF)

    /* 456 */ float m_abilityRestoreRate;
    /* 460 */ float m_abilityUpdate;

    // In-vehicle ability material, strength, and radius used by Player::updateAbility.
    /* 464 */ int   m_abilityInVehicleMaterial;      // was `_dword1D0`
    /* 468 */ float m_abilityInVehicleStrength;      // was `_dword1D4` float, not int
    /* 472 */ float m_abilityInVehicleRadius;        // was `_dword1D8` float, not int
    // Constructor and console bindings leave this word uninitialized.
    /* 476 */ int   _dword1DC;                       // [UNRESOLVED] - ctor does NOT write it
    // [UNRESOLVED] element type of the list represented by this sentinel pointer.
    /* 480 */ void* _list1E0;                        // a std::list's sentinel - see above

    // All three zeroed by the ctor - which is what an empty vector looks like, and supports
    // the existing begin/end/capacity reading.
    /* ELF484 / clientPE2B8 */ std::vector<IObjectTemplate*> m_unlockItems;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_KITTEMPLATE_H
