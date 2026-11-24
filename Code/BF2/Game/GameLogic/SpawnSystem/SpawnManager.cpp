// Spawn-group registry, spawn-point selection, and squad relationships.

#include "SpawnManager.h"

#include <cstdio>                                      // the [FRONTIER] diagnostic
#include <cstdlib>                                     // std::abort - getCreateSpawnGroup's net-id guard

#include "SpawnGroup.h"                                // the group class + ISpawnGroup
#include "../../Objects/ISpawnPoint.h"                 // the point interface (SpawnPoint's second base)
#include "../../Objects/ISpawnPointTemplate.h"         // the template interface - getControlPointId
#include "../../Objects/IObject.h"
#include "../../Objects/IObjectTemplate.h"
#include "../../Objects/IPlayer.h"
#include "../../Objects/ObjectLayoutAccess.h"          // LayoutAccess::objectTemplate(object)
#include "../../IGame.h"                               // game, sendGameEventToAllPlaying / IfConnected
#include "../../Common/SpawnGroupEvents.h"             // the two GameEvents the announcements carry
#include "../../../IO/Network/NetworkManager.h"        // io::networkManager - the registry
#include "../../../IO/Network/INetworkable.h"
#include "../../../Core/SettingsRepostitory.h"        // createClientSpawnGroup: GLSWorldSizeX/Z
#include "../../../Core/Debug.h"
#include "../../../Core/Ids.h"

namespace dice {
namespace hfe {

// release decrements unconditionally and deletes only at zero, retaining the count before deletion.
void SpawnManager::addRef()
{
    ++m_refCount;
}

int SpawnManager::getRef()
{
    return m_refCount;
}

int SpawnManager::release()
{
    const int remaining = --m_refCount;

    if (remaining == 0)
        delete this;                 // the binary dispatches the deleting destructor, vtable +0x14

    return remaining;
}

// IID_IBase first, then IID_ISpawnManager (0xC486). Anything else returns null. The `const` is
// IBase's - see Core/IBase.h's note about a non-const override hiding rather than overriding.
void* SpawnManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<SpawnManager*>(this);

    if (iid == IID_ISpawnManager)
        return const_cast<SpawnManager*>(this);

    return 0;
}

// Seeds spawn-point types soldier_spawn and vehicle_spawn; these names are part
// of the map-script interface.
// [CHOSEN, not recovered] Retail fix "The spawn manager's unwritten words start at zero".
// ELF 0x081124F0 / 0x081126B0 leave +0x14 and +0x24 unwritten; no readers are identified.
SpawnManager::SpawnManager()
    : m_refCount(1)                  // the one-reference rule, same as every other IBase here
    , m_unknown14(0)
    , m_nextGroupId(1)
    , m_unknown24(0)
    , m_soldierSpawn("soldier_spawn")
    , m_vehicleSpawn("vehicle_spawn")
{
    // The four containers are empty-constructed, which is exactly what the binary's four
    // self-linked sentinel allocations amount to.
}

// [UNRESOLVED] Original destructor body is unread; owned-group teardown is not yet implemented.
SpawnManager::~SpawnManager()
{
}

// `lea eax, [this+0x0C]; ret`.
const std::map<int, ISpawnGroup*>& SpawnManager::getSpawnGroups()
{
    return m_spawnGroups;
}

// Rejects aggregation: a non-null outer returns null before allocation.
IBase* SpawnManager::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)
        return 0;

    SpawnManager* self = new SpawnManager();                    // operator new(0x34u)

    void* itf = self->queryInterface(iid);                      // virtual, vtable +0x0C
    if (itf == 0)
        self->release();                                        // virtual, vtable +0x08

    return static_cast<IBase*>(itf);
}

// Returns the registered spawn group or null when absent.
ISpawnGroup* SpawnManager::getSpawnGroup(int groupId)
{
    std::map<int, ISpawnGroup*>::iterator it = m_spawnGroups.find(groupId);
    return it != m_spawnGroups.end() ? it->second : 0;
}

// : the (controlPointId, group) -> id allocator. First ask returns a fresh id
// from m_nextGroupId and remembers the pair; every later ask returns the same id. This is how a
// spawn point attached to a control point gets a stable group id without content authoring one.
int SpawnManager::getMappedSpawnGroupId(int groupId, int forWhom)
{
    const std::pair<int, int> key(groupId, forWhom);

    std::map<std::pair<int, int>, int>::iterator it = m_mappedGroupIds.find(key);
    if (it != m_mappedGroupIds.end())
        return it->second;

    const int id = m_nextGroupId++;
    m_mappedGroupIds.insert(std::make_pair(key, id));
    return id;
}

// . The guard is the load-bearing half: no group, nothing to dirty - the precache
// case. The ensure-then-poke tail runs only for a live group; its map-ensure arm (insert a null
// entry if absent) is shipped dead code when the guard passed on the same map, and the poke writes
// the unrecovered SpawnGroup's +0x6C - split out, faults by name if a live group ever gets here.
void SpawnManager::makeAverageGroupPositionDirty(int groupId)
{
    ISpawnGroup* group = getSpawnGroup(groupId);     // virtual, as shipped
    if (!group)
        return;

    ISpawnGroup*& slot = m_spawnGroups[groupId];     // the shipped ensure - a no-op here, the
    (void)slot;                                      //   guard already proved the entry exists
    markGroupDirty(group);                           // the raw +0x6C write
}

// The +0x6C poke - the SpawnGroup's positionDirty flag, written raw (friend).
void SpawnManager::markGroupDirty(ISpawnGroup* group)
{
    static_cast<SpawnGroup*>(group)->m_positionDirty = true;
}

// Finds or creates a spawn group; server-only unless clientCreated is set. Server
// static groups receive ids starting at 515, dynamic groups use 514 + groupId - 128.
// Clients use the supplied network id. Enforces the static-group and network-id limits.
ISpawnGroup* SpawnManager::getCreateSpawnGroup(unsigned int groupId, unsigned int netId, bool clientCreated)
{
    if (!game->queryInterface(IID_IGameServer) && !clientCreated)
        return 0;
    ISpawnGroup* existing = getSpawnGroup(static_cast<int>(groupId));
    if (existing)
        return existing;

    SpawnGroup* group = new SpawnGroup();
    m_spawnGroups.insert(std::make_pair(static_cast<int>(groupId), static_cast<ISpawnGroup*>(group)));
    group->m_spawnGroupId = static_cast<int>(groupId);
    if (!clientCreated)
    {
        unsigned int slot;
        if (groupId <= 0xBFu)
        {
            unsigned int staticCount = 0;
            for (std::map<int, ISpawnGroup*>::iterator it = m_spawnGroups.begin(); it != m_spawnGroups.end(); ++it)
            {
                if (static_cast<unsigned int>(it->first) <= 0xBFu)
                    ++staticCount;
            }
            slot = staticCount;
            if (staticCount > 0x3Fu)
            {
                Debug(DEBUGTYPE_7, "Game/GameLogic/SpawnSystem/SpawnManager.cpp", 498, __FUNCTION__)
                    << "Too many static spawngroups " << staticCount;
            }
        }
        else
        {
            slot = groupId - 128;
        }
        netId = slot + 514;
    }
    io::INetworkable* networkable = static_cast<io::INetworkable*>(group->queryInterface(io::IID_INetworkable));
    if (networkable)
    {
        networkable->init(0);                                    // vptr +0x18 - the baseline block
        networkable->m_basePriority = -1.0f;                     // +0x10 - a static ghost
        if (netId - 514 > 127)
            std::abort();
        io::networkManager->addNetworkable(networkable, static_cast<unsigned short>(netId));
        if (!clientCreated)
            createSpawnGroupOnClients(-1, group);
    }
    return group;
}

// Registers a server spawn point. Group -1 creates/uses the point's own group and
// its template's control-point ID; an explicit group must already exist and updates
// the point's group ID. Null points or absent groups are ignored.
void SpawnManager::addSpawnPoint(ISpawnPoint* point, int groupId)
{
    if (!point)
        return;
    if (!game->queryInterface(IID_IGameServer))
        return;
    world::IObject* object = static_cast<world::IObject*>(point->queryInterface(world::IID_IObject));
    ISpawnPointTemplate* spawnTemplate = static_cast<ISpawnPointTemplate*>(
        world::object_detail::LayoutAccess::objectTemplate(object)->queryInterface(IID_ISpawnPointTemplate));
    ISpawnGroup* group;
    if (groupId == -1)
    {
        group = getCreateSpawnGroup(static_cast<unsigned int>(point->getSpawnGroupId()), 0, false);
        spawnTemplate->getControlPointId();                      // called twice - the first discarded
        group->setControlPointId(static_cast<unsigned int>(spawnTemplate->getControlPointId()));
    }
    else
    {
        group = getSpawnGroup(groupId);
        point->setSpawnGroupId(groupId);                         // SpawnPoint's +0x19C override
    }
    if (!group)
        return;
    m_spawnPoints.push_back(point);
    static_cast<SpawnGroup*>(group)->addSpawnPoint(point);
}

// Removes the point from its current group when present, then erases its first
// matching entry from the manager's point list.
void SpawnManager::remove(const ISpawnPoint* point)
{
    ISpawnPoint* mutablePoint = const_cast<ISpawnPoint*>(point);
    const int groupId = mutablePoint->getSpawnGroupId();
    if (getSpawnGroup(groupId))
    {
        ISpawnGroup*& slot = m_spawnGroups[groupId];
        static_cast<SpawnGroup*>(slot)->removeSpawnPoint(mutablePoint);
    }
    for (std::list<ISpawnPoint*>::iterator it = m_spawnPoints.begin(); it != m_spawnPoints.end(); ++it)
    {
        if (*it == point)
        {
            m_spawnPoints.erase(it);
            return;
        }
    }
}

// : server-only -> _removeGroup(id, false).
void SpawnManager::removeGroup(int groupId)
{
    if (game->queryInterface(IID_IGameServer))
        _removeGroup(groupId, false);
}

// _removeGroup - 0x8113A30, 253 B: find; announce the removal to every client unless the client
// asked; NetworkManager::removeNetworkable(the group's net id) and clear it; delete the group
// (D0, +0x14); erase the map entry.
void SpawnManager::_removeGroup(int groupId, bool clientRemoved)
{
    std::map<int, ISpawnGroup*>::iterator it = m_spawnGroups.find(groupId);
    if (it == m_spawnGroups.end())
        return;
    SpawnGroup* group = static_cast<SpawnGroup*>(it->second);
    if (!clientRemoved)
        removeSpawnGroupOnClients(-1, group);
    io::networkManager->removeNetworkable(group->m_networkableId);
    group->m_networkableId = 0;
    if (group)
        delete group;
    m_spawnGroups.erase(it);
}

// : every group through _removeGroup(id, true) (no announcements - the whole level
// is going), the mapped-id table cleared, the counter back to 1.
void SpawnManager::removeAll()
{
    while (!m_spawnGroups.empty())
        _removeGroup(m_spawnGroups.begin()->first, true);
    m_mappedGroupIds.clear();
    m_nextGroupId = 1;
}

// : rebuild m_groupsForTeam = the ids of every group whose getTeam() == team and
// hasSpawnPoints. 0x8113820, 120 B: is there any group NOT of this team, with points, whose +0x5D
// byte (m_teamSettable - the byte after the active flag) is set? - the "can the enemy still spawn"
// test, read raw as shipped.
const std::list<int>& SpawnManager::getGroupsForTeam(int team)
{
    m_groupsForTeam.clear();
    for (std::map<int, ISpawnGroup*>::iterator it = m_spawnGroups.begin(); it != m_spawnGroups.end(); ++it)
    {
        SpawnGroup* group = static_cast<SpawnGroup*>(it->second);
        if (group->getTeam() == team && group->m_hasSpawnPoints)
            m_groupsForTeam.push_back(it->first);
    }
    return m_groupsForTeam;
}

// getGroupsForPlayer - ELF 0x8113670, 291 B. This is the spawn-screen view rebuilt after a
// control point changes a SpawnGroup's team: same-team groups with points are kept, AI-only groups
// require an AI player, and squad groups are offered only to a non-leader member of that squad.
// The repeated getSquad() call in the non--1 arm is shipped ordering, not folded away.
const std::list<int>& SpawnManager::getGroupsForPlayer(world::IPlayer* player)
{
    m_groupsForTeam.clear();
    if (!player)
        return m_groupsForTeam;

    for (std::map<int, ISpawnGroup*>::iterator it = m_spawnGroups.begin();
         it != m_spawnGroups.end(); ++it)
    {
        SpawnGroup* group = static_cast<SpawnGroup*>(it->second);
        if (group->getTeam() == player->getTeam() &&
            (!group->m_onlyForAI || player->getIsAIPlayer()))
        {
            if (group->getSquad() == -1 ||
                (group->getSquad() == player->getSquadId() && !player->getIsSquadLeader()))
            {
                if (group->m_hasSpawnPoints)
                    m_groupsForTeam.push_back(it->first);
            }
        }
    }
    return m_groupsForTeam;
}

bool SpawnManager::canTeamGetAnySpawnGroup(int team)
{
    for (std::map<int, ISpawnGroup*>::iterator it = m_spawnGroups.begin(); it != m_spawnGroups.end(); ++it)
    {
        SpawnGroup* group = static_cast<SpawnGroup*>(it->second);
        if (group->getTeam() != team && !group->m_spawnPoints.empty() && group->m_teamSettable)
            return true;
    }
    return false;
}

// : the first free dynamic id in 192..255 -> getCreateSpawnGroup(id, 0, false), id
// back; -1 when all 64 are taken. 0x8113400, 77 B: create that dynamic id if free, else -1.
int SpawnManager::createDynamicSpawnGroup()
{
    unsigned int id = 192;
    while (getSpawnGroup(static_cast<int>(id)))
    {
        if (++id > 0xFFu)
            return -1;
    }
    getCreateSpawnGroup(id, 0, false);
    return static_cast<int>(id);
}
int SpawnManager::createExistingDynamicSpawnGroup(int groupId)
{
    if (getSpawnGroup(groupId))
        return -1;                                   // already there - "not created"
    getCreateSpawnGroup(static_cast<unsigned int>(groupId), 0, false);
    return groupId;
}

// Creates or updates the client's spawn-group copy and expands byte X/Z coordinates.
// Uses signed integer half-world sizes and divide-before-multiply arithmetic; Y is zero.
// The client-created flag must allow getCreateSpawnGroup to return a valid group.
int SpawnManager::createClientSpawnGroup(unsigned char spawnGroupId, unsigned int netId, int team,
                                         bool active, bool onlyForAI, bool hasSpawnPoints,
                                         unsigned char quantisedX, unsigned char quantisedZ)
{
    ISpawnGroup* const group = getCreateSpawnGroup(spawnGroupId, netId, true);

    const int worldSizeX = settings->intGet("GLSWorldSizeX", 0);                   // settings +0x10
    const int worldSizeZ = settings->intGet("GLSWorldSizeZ", 0);

    static_cast<SpawnGroup*>(group)->m_hasSpawnPoints = hasSpawnPoints;            // +0x74
    group->setTeam(team);                                                          // +0x18
    group->setActive(active);                                                      // +0x28
    static_cast<SpawnGroup*>(group)->m_onlyForAI = onlyForAI;                      // +0x6E

    Vec3 position;
    position.x = quantisedX / 255.0f * worldSizeX - (worldSizeX / 2);   // integer /2
    position.y = 0.0f;
    position.z = quantisedZ / 255.0f * worldSizeZ - (worldSizeZ / 2);
    group->setPos(position);                                                       // +0x30

    // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: objectdb-trace] - the same env gate the object
    // database uses (BF2_TRACE_OBJECTDB), because the groups arrive in the same burst and nothing
    // else on the client says they did. Off unless the variable is set.
    {
        static int s_trace = -1;
        static int s_count = 0;
        if (s_trace < 0)
            s_trace = std::getenv("BF2_TRACE_OBJECTDB") ? 1 : 0;
        if (s_trace)
        {
            std::fprintf(stderr,
                "[SPAWNGRP] #%d id=%u netId=%u team=%d active=%d aiOnly=%d points=%d "
                "q=(%u,%u) world=(%d,%d) pos=(%.1f, %.1f)\n",
                ++s_count, static_cast<unsigned>(spawnGroupId), netId, team,
                active ? 1 : 0, onlyForAI ? 1 : 0, hasSpawnPoints ? 1 : 0,
                static_cast<unsigned>(quantisedX), static_cast<unsigned>(quantisedZ),
                worldSizeX, worldSizeZ,
                static_cast<double>(position.x), static_cast<double>(position.z));
            std::fflush(stderr);
        }
    }

    return static_cast<SpawnGroup*>(group)->m_spawnGroupId;                        // +0x78
}


void SpawnManager::removeClientSpawnGroup(int groupId) { _removeGroup(groupId, true); }
void SpawnManager::createAllSpawnGroupsOnClient(int connectionId)
{
    for (std::map<int, ISpawnGroup*>::iterator it = m_spawnGroups.begin(); it != m_spawnGroups.end(); ++it)
        createSpawnGroupOnClients(connectionId, it->second);
}

// Sends spawn-group creation to every playing client for connection -1, otherwise
// to the named connection if connected. Removal uses the same routing.
bool SpawnManager::createSpawnGroupOnClients(int connectionId, ISpawnGroup* igroup)
{
    SpawnGroup* group = static_cast<SpawnGroup*>(igroup);
    unsigned char x = 0, z = 0;
    group->getUnsignedWorldPosition(&x, &z);
    CreateSpawnGroupEvent* event = new CreateSpawnGroupEvent(
        static_cast<unsigned char>(group->m_spawnGroupId), group->m_networkableId,
        group->getTeam(), group->getActive(false), group->m_onlyForAI, group->m_hasSpawnPoints, x, z);
    if (connectionId == -1)
        game->sendGameEventToAllPlaying(event);
    else
        game->sendGameEventIfConnected(static_cast<unsigned int>(connectionId), event);
    return true;
}
void SpawnManager::removeSpawnGroupOnClients(int connectionId, ISpawnGroup* igroup)
{
    SpawnGroup* group = static_cast<SpawnGroup*>(igroup);
    RemoveSpawnGroupEvent* event = new RemoveSpawnGroupEvent(
        static_cast<unsigned char>(group->m_spawnGroupId), group->m_networkableId);
    if (connectionId == -1)
        game->sendGameEventToAllPlaying(event);
    else
        game->sendGameEventIfConnected(static_cast<unsigned int>(connectionId), event);
}

// : for every group with exactly one spawn point, that point's ISpawnPoint slot 16
// (+0x40 = getOverrideEnterOnSpawn) is called and the result dropped - a console listing whose
// printing was compiled out, leaving the accessor call. Reproduced as the call it is.
void SpawnManager::listSpawnGroups()
{
    for (std::map<int, ISpawnGroup*>::iterator it = m_spawnGroups.begin(); it != m_spawnGroups.end(); ++it)
    {
        std::vector<ISpawnPoint*>& points = it->second->getSpawnPoints();
        if (points.size() == 1)
            points[0]->getOverrideEnterOnSpawn();
    }
}

} // namespace hfe
} // namespace dice
