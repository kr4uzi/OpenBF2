// Manages supply-drop requests, placement, landing and expiry. [INFERRED] Source placement.

#include "SupplyDropManager.h"

#include <list>
#include <string>
#include <vector>

#include "../../../Core/Ids.h"
#include "../../../Core/EventManager.h"
#include "../../../Core/System.h"
#include "../../../Math/Vec3.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../../Common/GameEvent.h"
#include "../../GameEvents/SupplyDropEvent.h"
#include "../../IGame.h"
#include "../../IGameLogic.h"
#include "../../Main/Game.h"
#include "../IObject.h"
#include "../IObjectTemplate.h"
#include "../IPlayer.h"

namespace dice {
namespace hfe {

namespace world { extern IBase* heightmapCluster; }

namespace world {

SupplyDropManager* SupplyDropManager::m_pInstance = 0;       // ds:0x8AADAA8

SupplyDropManager::SupplyDropManager()
    : m_dropTimers()                                         // +0x04 the hand-built header
{
    reset();
}

SupplyDropManager::~SupplyDropManager()
{
}

SupplyDropManager* SupplyDropManager::Instance()
{
    if (!m_pInstance)
        m_pInstance = new SupplyDropManager();               // operator new(0x18)
    return m_pInstance;
}

void SupplyDropManager::Release()
{
    if (m_pInstance)
    {
        delete m_pInstance;                                  // vt +4 - the deleting destructor
        m_pInstance = 0;
    }
}

int SupplyDropManager::reset()
{
    m_dropTimers.clear();                                    // +0x04 guarded by the count as shipped
    m_lastDropTimes[1] = 0.0f;                               // +0x14
    m_lastDropTimes[0] = 0.0f;                               // +0x10
    return 0;
}

// The request throttle is per team and is stamped before either the server spawn or client relay.
// The spawn result is deliberately ignored: an accepted request returns true even when object
// creation fails.
bool SupplyDropManager::requestSupplyDrop(unsigned char team, Vec2& where)
{
    const unsigned char index = static_cast<unsigned char>(team - 1);
    if (index > 1)
        return false;

    const float now = static_cast<float>(g_system->getExactTime());
    if (now - m_lastDropTimes[index] < 59.900002f)
        return false;

    m_lastDropTimes[index] = now;
    if (game->queryInterface(IID_IGameServer))
    {
        spawnSupplyDrop(team, where);
        return true;
    }

    game->sendGameEvent(0xFFFFFFFFu,
                        new SupplyDropEvent(where.x, where.y, static_cast<int>(team)));
    return true;
}

bool SupplyDropManager::spawnSupplyDrop(unsigned char team, const Vec2& where)
{
    Vec3 position = { where.x, 500.0f, where.y };
    IHeightmapCluster* const cluster = static_cast<IHeightmapCluster*>(heightmapCluster);

    float landingHeight = cluster->getHeightInWorldCoords(where.x, where.y);
    const float waterHeight = cluster->getWaterLevelInWorldCoords(position);
    if (waterHeight > landingHeight)
        landingHeight = waterHeight;

    const Vec3 rayEnd = { position.x, position.y - 500.0f, position.z };
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    if (checkIfRayHitAnyObjectOrGround(position, rayEnd, 0, hitObject,
                                       false, false, true, &hitPoint)
        && hitPoint.y > landingHeight)
    {
        landingHeight = hitPoint.y;
    }

    position.y = gameLogic->getSupplyDropHeight() + landingHeight;
    return spawnSupplyDrop(team, position);
}

bool SupplyDropManager::spawnSupplyDrop(unsigned char team, const Vec3& where)
{
    const std::string templateName("supply_crate");
    IObjectTemplate* const objectTemplate = objectTemplateManager->getTemplate(templateName);
    const Vec3 rotation = { 0.0f, 0.0f, 0.0f };

    IObject* const crate =
        game->createObjectOnAllClients(objectTemplate, where, rotation, true, 0, 0);
    if (!crate)
        return false;

    reportSupplyDrop(team, where);
    return true;
}

void SupplyDropManager::reportSupplyDrop(unsigned char team, const Vec3& where)
{
    IPlayer* const localHuman = playerManager->getLocalHumanPlayer();
    const std::list<IPlayer*>& players = playerManager->getPlayers(team);
    Vec3 payload = where;

    for (std::list<IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
#if !DEDICATED
        // BF2.exe 0x4524C0..0x452506: deliver HUD 83 directly for the host's
        // player (also for a null local player), retaining the runtime dedicated gate.
        if (!isDedicated() && (!localHuman || localHuman->getId() == (*it)->getId()))
        {
            if (!isDedicated())
                eventManager->postEvent(ECHud, 83u, &payload, 0.0f);
            continue;
        }
#endif
        PostRemoteEvent* const event =
            new PostRemoteEvent(ECHud, 83, false, &payload, 12, 0.0f);
        game->sendGameEventIfConnected((*it)->getId(), event);
    }
}

bool SupplyDropManager::collidedWithSpawningObject(IObject* object)
{
    if (!object)
        return false;

    // Both ports exact-compare the immediate template class to 50370. This is intentionally not
    // queryImplementation: derived/specialised template classes do not pass this shipped gate.
    const IObjectTemplate* const objectTemplate = object->getTemplate();
    if (!objectTemplate || objectTemplate->getClassID() != CID_PlayerControlObjectTemplate)
        return false;

    IPlayerControlObject* playerControlObject = 0;
    for (IObject* current = object; current; current = current->getParent())
    {
        playerControlObject = static_cast<IPlayerControlObject*>(
            current->queryInterface(IID_IPlayerControlObject));
        if (playerControlObject)
            break;
    }
    if (!playerControlObject)
        return false;

    const float now = static_cast<float>(g_system->getExactTime());
    return now - playerControlObject->getTimeSpawned() < 3.0f;
}

bool SupplyDropManager::disableChute(IObject*, IObject* chute)
{
    if (!chute)
        return false;
    chute->updateFlags(1, 0);
    return true;
}

bool SupplyDropManager::handleSuppliesLanding(IObject* supplyObject, IObject* collidedWith)
{
    if (!supplyObject)
        return false;
    // Shipped oddity, raw-verified in both ports: this calls IObject::getClassID (+0x10), not the
    // object's template. SupplyObject inherits Object's CID_Object answer, so comparing it to the
    // template CID 40007 is an always-pass mismatch for the intended object.
    if (supplyObject->getClassID() == CID_SupplyObjectTemplate)
        return false;

    if (collidedWithSpawningObject(collidedWith))
    {
        destroyCrate(supplyObject);
        return true;
    }

    std::vector<IObject*> children;
    getChilds(supplyObject, children, 0, 0);

    IObject* chute = 0;
    const IObjectTemplate* chuteTemplate = 0;
    for (std::vector<IObject*>::const_iterator it = children.begin(); it != children.end(); ++it)
    {
        IObject* const candidate = *it;
        const IObjectTemplate* const candidateTemplate =
            candidate ? candidate->getTemplate() : 0;
        if (candidateTemplate && candidateTemplate->getName() == "supply_parachute")
        {
            chute = candidate;
            chuteTemplate = candidateTemplate;
            break;
        }
    }
    if (!chute || !chuteTemplate)
        return false;

    addCrate(static_cast<int>(supplyObject->getObjectId()));
    return disableChute(supplyObject, chute);
}

bool SupplyDropManager::destroyCrate(IObject* crate)
{
    if (!crate)
        return false;
    removeCrate(static_cast<int>(crate->getObjectId()));
    return game->destroyObject(crate);
}

bool SupplyDropManager::addCrate(int id)
{
    if (m_dropTimers.find(id) != m_dropTimers.end())
        return false;
    m_dropTimers.insert(std::make_pair(id, static_cast<float>(g_system->getExactTime())));
    return true;
}

bool SupplyDropManager::removeCrate(int id)
{
    std::map<int, float>::iterator it = m_dropTimers.find(id);
    if (it == m_dropTimers.end())
        return false;
    m_dropTimers.erase(it);
    return true;
}

void SupplyDropManager::update()
{
    for (std::map<int, float>::iterator it = m_dropTimers.begin(); it != m_dropTimers.end();)
    {
        std::map<int, float>::iterator current = it;
        IObject* const crate =
            objectManager->getObjectFromGrid(static_cast<unsigned int>(current->first));
        if (!crate)
        {
            ++it;
            m_dropTimers.erase(current);
            continue;
        }

        const double elapsed = g_system->getExactTime() - static_cast<double>(current->second);
        if (elapsed <= static_cast<double>(gameLogic->getSupplyDropNumSecsToLive()))
        {
            ++it;
            continue;
        }

        ++it;
        m_dropTimers.erase(current);
        destroyCrate(crate);                                 // repeats removeCrate after the erase
    }
}

namespace {
// Core/Globals.cpp owns the externally linked pointer; this TU owns the shipped dynamic
// initialization that assigns it from the singleton before the server's first update.
SupplyDropManager* const initializedSupplyDropManager =
    (supplyDropManager = SupplyDropManager::Instance());
}

} // namespace world
} // namespace hfe
} // namespace dice
