// Path: [INFERRED], see SupplyDepot.h.
// ..0x81BD027; dedicated PE 0x571E60..0x5740A0 (reset 0x5736F0).
#include "SupplyDepot.h"
#include "../../../Core/Ids.h"
#include "../../../Core/System.h"
#include "../../../IO/Stream/IStream.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../GameLogicComp/IArmorGLComp.h"
#include "../IPlayer.h"
#include "../IWeaponObject.h"
#include "../../IGame.h"
#include "../../IGameLogic.h"
#include <cmath>
#include <cstdlib>
#include <sstream>
#include "../../../Platform/MsvcRand.h"  // [CHOSEN, not recovered] the client CRT's rand (MSVCR71)

namespace dice { namespace hfe { namespace world {
namespace {
IPlayerControlObject* depotPco(IObject* object)
{
    for (; object; object = object->getParent())
    {
        IPlayerControlObject* pco = static_cast<IPlayerControlObject*>(object->queryInterface(IID_IPlayerControlObject));
        if (pco) return pco;
    }
    return 0;
}
IArmorGLComp* depotArmor(IObject* object)
{
    for (; object; object = object->getParent())
    {
        IArmorGLComp* armor = static_cast<IArmorGLComp*>(object->getArmorComponent());
        if (armor) return armor;
    }
    return 0;
}
}

SupplyDepotTemplate::SupplyDepotTemplate()
    : m_radius(2.0f), m_team(0), m_workOnVehicles(false), m_workOnSoldiers(true),
      m_healRate(0.5f), m_inverseHealRate(2.0f), m_initialHealth(-1.0f),
      m_healthRegeneration(0.25f), m_updateInterval(0.5f)
{}
SupplyDepotTemplate::~SupplyDepotTemplate() {}
unsigned int SupplyDepotTemplate::getClassID() const { return CID_SupplyDepotTemplate; }
void* SupplyDepotTemplate::queryImplementation(unsigned int cid)
{
    return cid == CID_SupplyDepotTemplate || cid == CID_SimpleObjectTemplate || cid == CID_ObjectTemplate ? this : 0;
}
IObject* SupplyDepotTemplate::createObject() const { return new SupplyDepot(this); }
void SupplyDepotTemplate::addAmmoType(int type, int amount, float rate, float regeneration)
{
    m_ammoTypes.push_back(type);
    m_initialAmmo.push_back(static_cast<float>(amount));
    m_maxAmmo.push_back(static_cast<float>(amount));
    m_ammoRate.push_back(rate);
    m_inverseAmmoRate.push_back(1.0f / rate);
    m_ammoRegeneration.push_back(regeneration);
}
void SupplyDepotTemplate::addVehicleType(std::string name, int amount, float rate, float regeneration)
{
    m_vehicleNames.push_back(name);
    m_initialRepair.push_back(static_cast<float>(amount));
    m_maxRepair.push_back(static_cast<float>(amount));
    m_repairRate.push_back(rate);
    m_inverseRepairRate.push_back(1.0f / rate);
    m_repairRegeneration.push_back(regeneration);
}
void SupplyDepotTemplate::setHealth(int amount, float rate, float regeneration)
{
    m_healRate = rate;
    m_inverseHealRate = 1.0f / rate;
    m_initialHealth = static_cast<float>(amount);
    m_maxHealth = static_cast<float>(amount);
    m_healthRegeneration = regeneration;
}
bool SupplyDepotTemplate::hasAmmoType(int type)
{
    for (std::size_t i = 0; i < m_ammoTypes.size(); ++i)
        if (m_ammoTypes[i] == type) return true;
    return false;
}
bool SupplyDepotTemplate::makeScript(io::IStream* stream)
{
    if (!SimpleObjectTemplate::makeScript(stream)) return false;
    std::ostringstream script;
    if (m_radius != 2.0f) script << "ObjectTemplate.radius " << m_radius << std::endl;
    if (m_team) script << "ObjectTemplate.team " << m_team << std::endl;
    if (!m_workOnSoldiers) script << "ObjectTemplate.workOnSoldiers " << m_workOnSoldiers << std::endl;
    if (m_workOnVehicles) script << "ObjectTemplate.workOnVehicles " << m_workOnVehicles << std::endl;
    for (std::size_t i = 0; i < m_ammoTypes.size(); ++i)
        script << "ObjectTemplate.addAmmoType " << m_ammoTypes[i] << ' ' << m_initialAmmo[i]
               << ' ' << m_ammoRate[i] << ' ' << m_ammoRegeneration[i] << std::endl;
    for (std::size_t i = 0; i < m_vehicleNames.size(); ++i)
        script << "ObjectTemplate.addVehicleType " << m_vehicleNames[i] << ' ' << m_initialRepair[i]
               << ' ' << m_repairRate[i] << ' ' << m_repairRegeneration[i] << std::endl;
    script << "ObjectTemplate.setHealth " << m_initialHealth << ' ' << m_healRate
           << ' ' << m_healthRegeneration << std::endl;
    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

SupplyDepot::SupplyDepot(const SupplyDepotTemplate* objectTemplate)
    : SimpleObject(objectTemplate), m_depotTemplate(objectTemplate), m_team(objectTemplate->m_team)
{
    m_ammo = m_depotTemplate->m_initialAmmo;
    m_ammoToGive.resize(m_ammo.size());
    m_ammoTime.resize(m_ammo.size());
    for (std::size_t i = 0; i < m_ammo.size(); ++i) m_ammoTime[i] = 0.0f;
    m_health = m_depotTemplate->m_initialHealth;
    m_repair = m_depotTemplate->m_initialRepair;
    for (std::size_t i = 0; i < m_repair.size(); ++i)
    {
        IObjectTemplate* vehicle = objectTemplateManager->getTemplate(m_depotTemplate->m_vehicleNames[i]);
        m_vehicleIds.push_back(vehicle ? vehicle->getId() : static_cast<unsigned int>(-1));
    }
    m_hasRepair = !m_repair.empty();
    m_hasHeal = objectTemplate->m_healRate != 0.0f;
    if (game->queryInterface(IID_IGameServer))
    {
#if defined(_MSC_VER)
        // [PLATFORM] PE 0x573A96 multiplies by float bits 0x3D088889; ELF divides by 30.
        m_lastUpdate = static_cast<float>(bf2crt::rand() % 30) * 0.033333335f;
#else
        m_lastUpdate = static_cast<float>(bf2crt::rand() % 30) / 30.0f;
#endif
        Object::updateFlags(0x800, 2);
    }
}
SupplyDepot::~SupplyDepot() {}
void SupplyDepot::init()
{
    SimpleObject::init();
    m_createdOnLoad = true;
    gameLogic->addSupplyDepot(this);
}
void SupplyDepot::destroy(bool force)
{
    objectManager->removeAllMessagesTo(this);
    gameLogic->removeSupplyDepot(this);
    SimpleObject::destroy(force);
}
// Empty in both dedicated ports, independent of the corresponding SimpleObject behavior.
void SupplyDepot::handleMessage(TemplateMessage, IPlayer*, bool) {}
void SupplyDepot::healSoldier(IObject*) {}
void SupplyDepot::handleUpdate(float, unsigned int)
{
#if defined(_MSC_VER)
    // [PLATFORM] PE 0x5736A0 retains the getter's double through subtraction; ELF 0x81BA670
    // stores both first and second reads to float before using them.
    const double now = WorldPref::getWorldTime();
#else
    const float now = static_cast<float>(WorldPref::getWorldTime());
#endif
    if (now - m_lastUpdate > m_depotTemplate->m_updateInterval)
    {
#if defined(_MSC_VER)
        const double updateTime = WorldPref::getWorldTime();
#else
        const float updateTime = static_cast<float>(WorldPref::getWorldTime());
#endif
        update(static_cast<float>(updateTime - m_lastUpdate));
        m_lastUpdate = static_cast<float>(WorldPref::getWorldTime());
    }
}
void SupplyDepot::update(float dt)
{
    IArmorGLComp* armor = depotArmor(this);
    if (armor && armor->isDestroyed()) return;
    m_hasAmmo = false;
    for (std::size_t i = 0; i < m_ammoTime.size(); ++i)
    {
        if (m_ammo[i] != -1.0f)
        {
            m_ammo[i] += dt * m_depotTemplate->m_ammoRegeneration[i];
            if (m_ammo[i] > m_depotTemplate->m_maxAmmo[i]) m_ammo[i] = m_depotTemplate->m_maxAmmo[i];
        }
        if (m_ammo[i] != 0.0f)
        {
            m_ammoTime[i] -= dt;
            if (m_ammoTime[i] < 0.0f)
            {
                m_ammoToGive[i] = static_cast<int>(-m_ammoTime[i] * m_depotTemplate->m_ammoRate[i]);
                if (m_ammoToGive[i] > 0)
                {
                    m_ammoTime[i] += static_cast<float>(m_ammoToGive[i]) * m_depotTemplate->m_inverseAmmoRate[i];
                    m_hasAmmo = true;
                }
            }
        }
    }
    if (m_depotTemplate->m_workOnSoldiers == true)
    {
        if (m_hasHeal)
        {
            if (m_health != -1.0f)
            {
                m_health += dt * m_depotTemplate->m_healthRegeneration;
                if (m_health > m_depotTemplate->m_maxHealth) m_health = m_depotTemplate->m_maxHealth;
            }
            m_healthToGive = dt * m_depotTemplate->m_healRate;
        }
        workOnSoldiers();
    }
    if (m_depotTemplate->m_workOnVehicles == true)
    {
        if (m_hasRepair)
            for (std::size_t i = 0; i < m_repair.size(); ++i)
                if (m_repair[i] != -1.0f)
                {
                    m_repair[i] += dt * m_depotTemplate->m_repairRegeneration[i];
                    if (m_repair[i] > m_depotTemplate->m_maxRepair[i]) m_repair[i] = m_depotTemplate->m_maxRepair[i];
                }
        workOnVehicles();
    }
}
void SupplyDepot::reloadAmmo(IObject* object)
{
    IPlayerControlObject* pco = depotPco(object);
    if (!pco) return;
    std::map<int, IPlayerControlObject*>* pcos = pco->getPcos();
    for (auto seat = pcos->begin(); seat != pcos->end(); ++seat)
    {
        if (!seat->second) continue;
        const std::vector<IObject*>& weapons = *static_cast<const std::vector<IObject*>*>(seat->second->getWeapons(0));
        for (IObject* weaponObject : weapons)
        {
            IWeaponObject* weapon = weaponObject ? static_cast<IWeaponObject*>(weaponObject->queryInterface(IID_IWeaponObject)) : 0;
            if (!weapon) continue;
            for (std::size_t i = 0; i < m_ammo.size(); ++i)
            {
                // Both ports test the ammo type only when the requested amount is nonzero.
                if (m_ammoToGive[i] && weapon->getAmmoType() != m_depotTemplate->m_ammoTypes[i]) continue;
                if (m_ammo[i] == -1.0f)
                    weapon->reloadAmmo(m_ammoToGive[i], false);
                else if (m_ammo[i] <= static_cast<float>(m_ammoToGive[i]))
                {
                    const unsigned int unused = weapon->reloadAmmo(static_cast<unsigned int>(m_ammo[i]), false);
                    m_ammo[i] -= m_ammo[i] - static_cast<float>(unused);
                }
                else
                {
                    const unsigned int unused = weapon->reloadAmmo(m_ammoToGive[i], false);
                    m_ammo[i] -= static_cast<float>(m_ammoToGive[i]) - static_cast<float>(unused);
                }
            }
        }
    }
}
void SupplyDepot::repairVehicle(IObject* object)
{
    const IObjectTemplate* objectTemplate = object->getTemplate();
    if (!objectTemplate) return;
    if (objectTemplate->getClassID() == CID_PlayerControlObjectTemplate)
    {
        if (!object->queryInterface(IID_IPlayerControlObject)) return;
        if (static_cast<IPlayerControlObject*>(object->queryInterface(IID_IPlayerControlObject))->getTimeToLiveUnused() == 0.0f) return;
    }
    for (std::size_t i = 0; i < m_repair.size(); ++i)
    {
        if (object->getTemplate()->getId() != m_vehicleIds[i]) continue;
        IArmorGLComp* armor = depotArmor(object);
        if (!armor) return;
        if (m_repair[i] == -1.0f)
            armor->heal(m_depotTemplate->m_repairRate[i]);
        else
        {
            // Preserve the shipped integer truncations and the amount sent to heal, including
            // sending the whole requested amount even if the missing health is smaller.
            const bool limited = m_repair[i] <= m_depotTemplate->m_repairRate[i];
#if defined(_MSC_VER)
            const float amount = limited ? m_repair[i] : m_depotTemplate->m_repairRate[i];
#endif
            const float maximum = armor->getMaxHitPoints();
            const float current = armor->getHitPoints();
#if !defined(_MSC_VER)
            // [PLATFORM] ELF 0x81B9F80 snapshots the amount after the two armor getters;
            // /0x573054 snapshots before them.
            const float amount = limited ? m_repair[i] : m_depotTemplate->m_repairRate[i];
#endif
            const int leftover = static_cast<int>(amount - (maximum - current));
            const float rereadAmount = limited ? m_repair[i] : m_depotTemplate->m_repairRate[i];
            const int used = static_cast<int>(leftover < 0 ? rereadAmount : rereadAmount - static_cast<float>(leftover));
            armor->heal(rereadAmount);
            m_repair[i] -= static_cast<float>(used);
        }
        return;
    }
}
void SupplyDepot::workOnSoldiers()
{
    bool canHeal = true, canReload = true;
    if (getParent())
    {
        IPlayerControlObject* pco = depotPco(getRootParent(this));
        canHeal = pco->getVehicleMod(static_cast<VehicleMod>(1)) != 0;
        canReload = pco->getVehicleMod(static_cast<VehicleMod>(2)) != 0;
    }
    const std::list<IPlayer*>& players = playerManager->getPlayers();
    for (IPlayer* player : players)
    {
        if (!player->getIsAlive()) continue;
        IObject* soldier = player->getDefaultVehicle();
        if (!soldier) continue;
        if (soldier->getParent() && getRootParent(soldier) != getRootParent(this)) continue;
        if (player->getTeam() != m_team && m_team != 0) continue;
#if defined(_MSC_VER)
        // [PLATFORM] PE 0x572530/0x572539 reads target first; ELF 0x81BA32E reads depot first.
        const Vec3& target = soldier->getAbsolutePosition();
        const Vec3& position = getAbsolutePosition();
#else
        const Vec3& position = getAbsolutePosition();
        const Vec3& target = soldier->getAbsolutePosition();
#endif
        const float dx = target.x-position.x, dy = target.y-position.y, dz = target.z-position.z;
        // Both ports admit unordered comparisons; <= would change the NaN branch.
        if (!(std::sqrt(dx*dx+dy*dy+dz*dz) > m_depotTemplate->m_radius))
        {
            if (m_hasAmmo && canReload) reloadAmmo(soldier);
            if (m_hasHeal && canHeal) healSoldier(soldier);
        }
    }
}
void SupplyDepot::workOnVehicles()
{
    bool canRepair = true, canReload = true;
    if (getParent())
    {
        IPlayerControlObject* pco = depotPco(getRootParent(this));
        canRepair = pco->getVehicleMod(static_cast<VehicleMod>(3)) != 0;
        canReload = pco->getVehicleMod(static_cast<VehicleMod>(2)) != 0;
    }
    const auto& pcos = *gameLogic->getPcoMap();
    for (const auto& entry : pcos)
    {
        IObject* object = entry.second;
        if (object->getParent()) continue;
        const int team = depotPco(object)->getTeam();
        if (team != m_team && m_team != 0) continue;
        if (!object->getTemplate() || object->getTemplate()->getClassID() == CID_SoldierTemplate) continue;
#if defined(_MSC_VER)
        // [PLATFORM] PE 0x5731F8/0x573202; ELF 0x81BA570 uses the opposite getter order.
        const Vec3& target = object->getAbsolutePosition();
        const Vec3& position = getAbsolutePosition();
#else
        const Vec3& position = getAbsolutePosition();
        const Vec3& target = object->getAbsolutePosition();
#endif
        const float dx = target.x-position.x, dy = target.y-position.y, dz = target.z-position.z;
        if (!(std::sqrt(dx*dx+dy*dy+dz*dz) > m_depotTemplate->m_radius))
        {
            if (m_hasAmmo && canReload) reloadAmmo(object);
            if (m_hasRepair && canRepair) repairVehicle(object);
        }
    }
}
void SupplyDepot::reset()
{
    m_ammo = m_depotTemplate->m_initialAmmo;
    for (std::size_t i = 0; i < m_ammo.size(); ++i) m_ammoTime[i] = 0.0f;
    m_health = m_depotTemplate->m_initialHealth;
    m_repair = m_depotTemplate->m_initialRepair;
    m_hasHeal = m_depotTemplate->m_healRate != 0.0f;
    m_hasRepair = !m_repair.empty();
#if defined(_MSC_VER)
    // uses the same multiply as its constructor; ELF uses division.
    m_lastUpdate = static_cast<float>(bf2crt::rand() % 30) * 0.033333335f;
#else
    m_lastUpdate = static_cast<float>(bf2crt::rand() % 30) / 30.0f;
#endif
}
bool SupplyDepot::getWorkOnVehicles() { return m_depotTemplate->m_workOnVehicles; }
bool SupplyDepot::getWorkOnSoldiers() { return m_depotTemplate->m_workOnSoldiers; }
bool SupplyDepot::hasVehicleType(unsigned int id)
{
    for (std::size_t i = 0; i < m_vehicleIds.size(); ++i) if (m_vehicleIds[i] == id) return true;
    return false;
}
bool SupplyDepot::isHealing() { return m_depotTemplate->m_healRate > 0.0f; }
bool SupplyDepot::isRepairing() { return !m_depotTemplate->m_repairRate.empty(); }
bool SupplyDepot::isReloading() { return !m_depotTemplate->m_ammoRate.empty(); }
void SupplyDepot::debugInfo()
{
    for (std::size_t i = 0; i < m_ammoTime.size(); ++i) {}
    for (std::size_t i = 0; i < m_repair.size(); ++i) {}
}

} } }
