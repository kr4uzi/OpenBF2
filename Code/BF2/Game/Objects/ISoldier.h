// Infantry posture, inventory and traversal interface. Use compiler-adjusted casts across its multiple bases.
// Methods shared with IItemContainerObject are still separate interface slots.
// Return-type qualifications are recorded on declarations. [INFERRED] Source placement.

#ifndef GAME_OBJECTS_ISOLDIER_H     // [INFERRED] guard style
#define GAME_OBJECTS_ISOLDIER_H

#include <string>

#include "../../Core/IBase.h"
#include "../../Math/Vec3.h"

namespace dice {
namespace hfe {

struct Mat4;

namespace world {

class IObject;
class IPlayer;
class Kit;

// Soldier poses used by spawn and movement logic. Enumerator names are [CHOSEN]; numeric values are fixed.
enum SoldierPose
{
    SoldierPose_Stand  = 0,   // the only pose with walk / run / sprint speeds
    SoldierPose_Crouch = 1,
    SoldierPose_Prone  = 2,   // attested by setProneOnSpawn(pose == 2)
    SoldierPose_Swim   = 3
};

class ISoldier : public IBase
{
public:
    // Temporarily changes pose for the physics test, then restores it.
    virtual bool testIfValidTransformation(SoldierPose pose, const Mat4& transform, bool) = 0;  // +0x10
    virtual IObject* getActiveWeapon(int index) const = 0;                   // +0x14
    virtual int  getActiveItemIndex() = 0;                                   // +0x18
#if defined(_MSC_VER)
    // [PLATFORM] PE puts the name-based overload here; ELF puts it after the recoil predicate.
    // This early insertion shifts PE's object-item and pose slots by one through disableItem.
    virtual int addItem(const std::string& templateName, int index) = 0;     // +0x1C PE: item index / -1
#endif
    virtual bool addItem(IObject* item, int index) = 0;                      // +0x20 +0x1C ELF
    virtual IObject* getItem(int index) = 0;                                 // +0x24 +0x20 ELF
    // Detaches and returns the item, or null when no match is found.
    virtual IObject* removeItem(int index) = 0;                              // +0x28 +0x24 ELF
    virtual IObject* removeItem(IObject* item) = 0;                          // +0x2C +0x28 ELF
    virtual IObject* getUsedVehicle() const = 0;                             // +0x30 +0x2C ELF
    virtual IPlayer* getOccupingPlayer() const = 0;                          // +0x34 +0x30 ELF
    virtual SoldierPose getPose() const = 0;                                 // +0x38 +0x34 ELF
    virtual SoldierPose getRequestedPose() const = 0;                        // +0x3C +0x38 ELF
    virtual void executePoseChange() = 0;                                    // +0x40 +0x3C ELF
    virtual bool getIsNewPoseRequested() const = 0;                          // +0x44 +0x40 ELF
    // Returns whether the item was drawn.
    virtual bool enableItem(char index) = 0;                                 // +0x48 +0x44 ELF
    virtual void disableItem(char index) = 0;                                // +0x4C +0x48 ELF
    // [PLATFORM] The name-based addItem overload is PE +0x1C but ELF +0x50. Thus the common tail
    // begins at getKit +0x54 on both platforms even though the middle of the interface differs.
#if defined(_MSC_VER)
    // Soldier's body is TEN bytes: `return true;`. Named as a question, answered as a constant -
    // ServerGameLogic::selectWeapon's gate on it is therefore always open for a soldier.
    virtual bool isNotFireingOrHaveRecoil() = 0;                             // +0x50 PE
#else
    // Same retail body and gate as the PE declaration above.
    virtual bool isNotFireingOrHaveRecoil() = 0;                             // +0x4C ELF
    virtual int addItem(const std::string& templateName, int index) = 0;     // +0x50 ELF: item index / -1
#endif
    virtual Kit* getKit() = 0;                                               // +0x54
    virtual int  getKitType() const = 0;                                     // +0x58 13 with no kit
    virtual int  getLastContactMaterial() const = 0;                         // +0x5C
    virtual void bulletFlyBy(IObject* projectile, const Vec3& a, const Vec3& b) = 0;  // +0x60
    virtual void dropWeaponFx(bool, Vec3&, int) = 0;                         // +0x64 client falling-weapon effect
    virtual float getSoldierRadius() const = 0;                              // +0x68 a global, not a field
    virtual float getStandingHeight() const = 0;                             // +0x6C a global, not a field
    // Returns the ragdoll's lowest particle or a point one metre below the soldier.
    virtual Vec3 getLowestVisiblePosition() const = 0;                       // +0x70
    virtual bool getIsSkyDiving() const = 0;                                 // +0x74
    virtual void setLadderId(unsigned int id) = 0;                           // +0x78
    virtual unsigned int getLadderId() const = 0;                            // +0x7C
    virtual void setZiplineId(unsigned int id) = 0;                          // +0x80
    virtual unsigned int getZiplineId() const = 0;                           // +0x84
    virtual void setGrapplingHookId(unsigned int id) = 0;                    // +0x88
    virtual unsigned int getGrapplingHookId() const = 0;                     // +0x8C
    virtual void setReadyToClimbTime(float t) = 0;                           // +0x90
    virtual void increaseSoldierCameraTransformation(float dx, float dy) = 0;// +0x94
    virtual float getCurrentAbility() const = 0;                             // +0x98
    virtual void setCurrentAbility(float a) = 0;                             // +0x9C
    // spend, NOT SET: returns false and changes nothing when the meter is short. The kit's
    // ability resource - see the KitTemplate notes in Soldier.cpp.
    virtual bool taxAbility(float amount) = 0;                               // +0xA0
    virtual const Vec3& getNameTagOffset() = 0;                              // +0xA4
    virtual float getFireDelayAfterJump() const = 0;                         // +0xA8
    virtual bool getCanFireWeapon() const = 0;                               // +0xAC
protected:
    ~ISoldier() {}
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_ISOLDIER_H
