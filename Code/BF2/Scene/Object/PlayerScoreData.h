// Per-player score counters and per-template combat statistics. Header path [INFERRED].
// Fractional points accumulate until a positive or negative whole point can be banked.
// Heal, repair and ammo pools require strictly more than the requested amount to pay out.
// reset() clears round data but preserves rank and the three trailing persistent fields.
// ELF x86 instances occupy 96 bytes; use native STL containers rather than copied map offsets.
// The ammo-pool and outer combat-map identities are [INFERRED].

#ifndef SCENE_OBJECT_PLAYERSCOREDATA_H     // [INFERRED] guard style
#define SCENE_OBJECT_PLAYERSCOREDATA_H

#include <map>

namespace dice {
namespace hfe {
namespace world {

class IPlayer;
class IObject;

class PlayerScoreData                        // 0x60
{
public:
    PlayerScoreData();

    void reset();
    // reset clears round-scoped counters/maps while preserving rank and career values.
    void setRank(unsigned int rank, IPlayer* player);
    // (ServerGameLogic::heal / replenishAmmo consume them). The three
    // receive* return bool - "has the accumulator reached the limit? then take the limit out of it"
    // - the ELF's `mov eax,1` behind `fucomi` and every caller's `test al`. Retyped from void.
    void addToHealScoreAmount(float amount, IObject* object);
    bool receiveHealScore(float limit);
    void addFractionalScore(float amount);
    bool receiveFractionalScore();
    void addToRepairScoreAmount(float amount, IObject* object);
    bool receiveRepairScore(float limit);
    void addToAmmoScoreAmount(float amount, IObject* object);
    bool receiveAmmoScore(float limit);
    // [FRONTIER] Declaration only: addVehicleDestroyed and the three set* below have no bodies.
    // Nothing on the current drive calls them.
    void addVehicleDestroyed(unsigned int id);
    void addBulletsFired(unsigned int id, int count);
    void addBulletsGivingDamage(unsigned int id, int count, IObject* object);
    void setVehicleDestroyed(unsigned int id, unsigned short count);
    void setBulletsFired(unsigned int id, unsigned short count);
    void setBulletsGivingDamage(unsigned int id, unsigned short count);

    // Fields, in ELF order. Offsets recorded, not enforced (see Player.h for why).
    /* +0x00 */ int   m_score;                  // named by receiveFractionalScore
    /* +0x04 */ int m_field04;
    /* +0x08 */ int m_field08;
    /* +0x0C */ int m_field0C;
    /* +0x10 */ int m_field10;
    /* +0x14 */ int m_field14;
    /* +0x18 */ int m_field18;
    /* +0x1C */ float m_fractionalScore;        // named by addFractionalScore
    /* +0x20 */ int   m_rank;                   // -1 confirmed three ways - see above
    /* +0x24 */ float m_healScoreAmount;        // named by its accessor pair
    /* +0x28 */ float m_repairScoreAmount;      // named by its accessor pair
    /* +0x2C */ float m_ammoScoreAmount;        // [INFERRED] from the pattern
    /* +0x30 */ std::map<unsigned int, unsigned short> m_destroyedVehicles;    // [INFERRED] - see above
    /* +0x3C */ std::map<unsigned int, unsigned short> m_bulletsFired;         // verified
    /* +0x48 */ std::map<unsigned int, unsigned short> m_bulletsGivingDamage;  // [INFERRED] - see above
    /* +0x54 */ int m_field54;
    /* +0x58 */ int m_field58;
    /* +0x5C */ int m_field5C;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // SCENE_OBJECT_PLAYERSCOREDATA_H
