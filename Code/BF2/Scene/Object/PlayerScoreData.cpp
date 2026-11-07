// Player score accumulation and per-template statistics. [INFERRED] Source path.

#include "PlayerScoreData.h"

#include <utility>
#include <map>
#include "../../Core/Ids.h"                                   // IID_IProjectileObject
#include "../../Core/System.h"                                // WorldPref::getTickCount
#include "../../Game/Objects/IObject.h"                       // queryInterface
#include "../../Game/Objects/Projectiles/GenericProjectile.h" // the count-once latch
#include "../../Game/SingletonServer.h"                       // setRank's nametag notify

namespace dice {
namespace hfe {
namespace world {

// Initializes score counters/maps and rank -1. reset deliberately preserves rank.
PlayerScoreData::PlayerScoreData()
    : m_score(0)
    , m_field04(0)
    , m_field08(0)
    , m_field0C(0)
    , m_field10(0)
    , m_field14(0)
    , m_field18(0)
    , m_fractionalScore(0.0f)
    , m_rank(-1)                    // the object's only sentinel
    , m_healScoreAmount(0.0f)
    , m_repairScoreAmount(0.0f)
    , m_ammoScoreAmount(0.0f)
    // the three maps default-construct empty, which is what the ELF's three
    // allocate(24)-and-self-link sequences do by hand
    , m_field54(0)
    , m_field58(0)
    , m_field5C(0)
{
}

// Clears per-round counters/maps while retaining rank and m_field54/58/5C.
void PlayerScoreData::reset()
{
    m_score             = 0;              // +0x00
    m_field04           = 0;              // +0x04
    m_field08           = 0;              // +0x08
    m_field0C           = 0;              // +0x0C
    m_field10           = 0;              // +0x10
    m_field14           = 0;              // +0x14
    m_field18           = 0;              // +0x18
    m_fractionalScore   = 0.0f;           // +0x1C
    //                                       +0x20 m_rank - deliberately skipped, see above
    m_healScoreAmount   = 0.0f;           // +0x24
    m_repairScoreAmount = 0.0f;           // +0x28
    m_ammoScoreAmount   = 0.0f;           // +0x2C

    m_destroyedVehicles.clear();          // +0x30
    m_bulletsFired.clear();               // +0x3C
    m_bulletsGivingDamage.clear();        // +0x48

    // +0x54 / +0x58 / +0x5C are career-scoped and survive the round - not touched here.
}

// Stores rank and touches the supplied player's nametag so it can be recreated after level loading.
// The nametag manager must already be installed; dedicated images use NullNametagManager.
// The unsigned input is stored without range checking in the signed rank field, whose sentinel is -1.
void PlayerScoreData::setRank(unsigned int rank, IPlayer* player)
{
    m_rank = static_cast<int>(rank);              // +0x20

    // ELF ds:0x8AB0980 vslot +0x1C / BF2.exe ds:0x9A5970 vslot +0x30 - see the block above.
    SingletonServer::m_iNametagManager->addTouchItem(player);
}

// The first insertion deliberately records one regardless of `count`; only an
// existing entry adds the supplied count. The 16-bit store wraps naturally, with no clamp.
void PlayerScoreData::addBulletsFired(unsigned int id, int count)
{
    std::map<unsigned int, unsigned short>::iterator it = m_bulletsFired.find(id);
    if (it == m_bulletsFired.end())
    {
        m_bulletsFired.insert(
            std::make_pair(id, static_cast<unsigned short>(1)));
    }
    else
    {
        it->second = static_cast<unsigned short>(it->second + count);
    }
}

// Counts damaging hits by weapon ID. Projectiles require countHit and an unset last-hit tick;
// the tick is a one-shot latch until resetProjectile, while nonprojectiles count unconditionally.
// The first insertion records 1; only existing entries add count. Callers ignore the original return value.
void PlayerScoreData::addBulletsGivingDamage(unsigned int id, int count, IObject* object)
{
    if (object)
    {
        void* const proj = object->queryInterface(IID_IProjectileObject);
        if (proj)
        {
            // GenericProjectile is the interface's only implementor; the typed downcast is the
            // route IProjectileObject.h mandates until its own vtable is declared in full.
            GenericProjectile* const gp = static_cast<GenericProjectile*>(
                static_cast<IProjectileObject*>(proj));

            if (!gp->getCountHit())                                  // +0x108
                return;
            if (gp->getLastCountedHitAtTick())                       // +0x100 - already counted
                return;
            gp->setLastCountedHitAtTick(WorldPref::getTickCount());  // +0x0FC
        }
    }

    std::map<unsigned int, unsigned short>::iterator it = m_bulletsGivingDamage.find(id);
    if (it == m_bulletsGivingDamage.end())
    {
        m_bulletsGivingDamage.insert(
            std::make_pair(id, static_cast<unsigned short>(1)));
    }
    else
    {
        it->second = static_cast<unsigned short>(it->second + count);
    }
}

// The three accumulator pairs - . Each addTo* adds to its float (and,
// as the header warns, ignores the IObject* entirely); each receive* answers whether the
// accumulator has reached the limit it is handed - and if so subtracts the limit and reports
// true, which is what ServerGameLogic::heal / replenishAmmo turn into Python events 29 / 30 / 31.
// The test is `limit <= accumulated`, so an exact hit pays out.
void PlayerScoreData::addToHealScoreAmount(float amount, IObject* /*ignored*/)
{
    m_healScoreAmount += amount;                                         // +0x24
}

bool PlayerScoreData::receiveHealScore(float limit)
{
    if (limit <= m_healScoreAmount)
    {
        m_healScoreAmount -= limit;
        return true;
    }
    return false;
}

// Banks whole points once the accumulator reaches -1 or +1, truncating toward zero
// and retaining the signed fractional remainder.
void PlayerScoreData::addFractionalScore(float amount)
{
    m_fractionalScore += amount;
}

bool PlayerScoreData::receiveFractionalScore()
{
    if (m_fractionalScore <= -1.0f || m_fractionalScore >= 1.0f)
    {
        const int whole = static_cast<int>(m_fractionalScore);
        m_score += whole;
        m_fractionalScore -= static_cast<float>(whole);
        return true;
    }
    return false;
}

void PlayerScoreData::addToRepairScoreAmount(float amount, IObject* /*ignored*/)
{
    m_repairScoreAmount += amount;                                       // +0x28
}

bool PlayerScoreData::receiveRepairScore(float limit)
{
    if (limit <= m_repairScoreAmount)
    {
        m_repairScoreAmount -= limit;
        return true;
    }
    return false;
}

void PlayerScoreData::addToAmmoScoreAmount(float amount, IObject* /*ignored*/)
{
    m_ammoScoreAmount += amount;                                         // +0x2C
}

bool PlayerScoreData::receiveAmmoScore(float limit)
{
    if (limit <= m_ammoScoreAmount)
    {
        m_ammoScoreAmount -= limit;
        return true;
    }
    return false;
}

} // namespace world
} // namespace hfe
} // namespace dice
