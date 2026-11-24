// [TEMP] [CHOSEN, not recovered] Rejection lifecycle with real control-point area constructor,
// destructor, position query and manager post-load pass. Valid post-load/world work is observed
// at a typed boundary; unused collaborators abort. No game, server, sockets or sound.
#include "control_point_area_runtime.h"
#include "area_collaborators.inl"

using namespace dice::hfe;
using namespace dice::hfe::ai;

static std::vector<unsigned int> loaded;
static int deletedWaypoints;
static int initializedWaypoints;

namespace dice { namespace hfe {
Debug::Debug(DebugType, const std::string&, int, const std::string&) { unexpected("Debug"); }
Debug::~Debug() { unexpected("~Debug"); }
namespace ai {
void* CommonSingletonServer::m_iaiGame = nullptr;
void* CommonSingletonServer::m_iaiTerrain = nullptr;
void* CommonSingletonServer::m_iaiSettings = nullptr;
void* CommonSingletonServer::m_iBFTimer = nullptr;
IAIPathfinding* IAIPathfinding::instance = nullptr;
IAIPathfinding::IAIPathfinding() { instance = this; }
IAIPathfinding::~IAIPathfinding() { instance = nullptr; }
AIRenderUtil* AIRenderUtil::m_instance = nullptr;
void AIRenderUtil::updateNumberOfStrategicAreas() { unexpected("debug roster"); }
void AIRenderUtil::unrenderStrategicArea(AIStrategicArea*) { unexpected("debug area"); }

// Chosen base lifetime: no statistics/singletons needed to test deferred-area destruction.
AIStrategicObject::AIStrategicObject(float radius, float height) : m_f148(height)
{ m_size[0] = radius; }
AIStrategicObject::~AIStrategicObject() {}
SAI::SAI(int, int, int, Vec2, Vec2) {}
SAI::~SAI() {}
ISAI::~ISAI() {}
IAISettings::~IAISettings() { unexpected("~IAISettings"); }
IBFTimer::~IBFTimer() { unexpected("~IBFTimer"); }
void AIStrategicArea::postLoadInit() { loaded.push_back(m_controlPointId); }
void AIStrategicObjectManager::deleteBridges() { require(m_bridgeSegments.empty(), "no fixture bridges"); }
SAWayPoint::SAWayPoint(const Vec3&, int) {}
SAWayPoint::~SAWayPoint() { ++deletedWaypoints; }
void SAWayPoint::init() { ++initializedWaypoints; }
} } }

static AIStrategicArea* add(AIStrategicObjectManager& manager, const char* name,
                            unsigned int cp, bool fromControlPoint = true)
{
    auto* area = new AIStrategicArea(name, cp, 50.0f);
    area->m_fromControlPoint = fromControlPoint;
    area->m_index = static_cast<int>(manager.m_strategicObjects.size());
    manager.m_strategicObjects.push_back(area);
    manager.m_objectsPerCategory[0].push_back(area);
    manager.m_objectsPerCategory[1].push_back(nullptr);
    manager.m_strategicAreaMap[name] = area;
    manager.m_areaNames[area] = name;
    return area;
}

void checkMissingControlPointAreas()
{
    AreaPathfinding pathfinding;
    AreaGame game;
    CommonSingletonServer::m_iaiGame = &game;
    // The observed crashing dependency is deliberately absent. Rejected areas must not load.
    require(CommonSingletonServer::m_iaiTerrain == nullptr, "missing terrain precondition");
    for (bool mixed : {false, true})
    {
        loaded.clear();
        deletedWaypoints = initializedWaypoints = 0;
        AIStrategicObjectManager manager;
        manager.m_objectsPerCategory.resize(2);
        auto* missingFirst = add(manager, "hotel", 602);
        AIStrategicArea* base = mixed ? add(manager, "main", 101) : nullptr;
        auto* missingLater = add(manager, "northbridge", 604);
        AIStrategicArea* flag = mixed ? add(manager, "flag", 103) : nullptr;
        AIStrategicArea* bounded = mixed ? add(manager, "bounded", 0xffffffffu, false) : nullptr;
        add(manager, "roadconstruct", 603);
        add(manager, "southblock", 601);
        manager.m_activeArea = manager.m_strategicAreaMap.find("hotel");
        const Vec3 point = {};
        manager.m_waypointMap[{missingFirst->m_index, missingLater->m_index}].getWayPoints(0)
            .push_back(new SAWayPoint(point, 0));
        if (mixed)
        {
            base->m_neighbours = {missingFirst, flag, missingLater};
            base->m_neighbouringObjects = {missingLater, bounded, missingFirst};
            flag->m_neighbours = {base, missingLater};
            manager.m_waypointMap[{base->m_index, flag->m_index}].getWayPoints(1)
                .push_back(new SAWayPoint(point, 1));
            manager.m_wayPointsToDrawFirst = base->m_index;
            manager.m_wayPointsToDrawSecond = flag->m_index;
        }
        manager.postLoadInitAreas();
        require(deletedWaypoints == 1, "rejected pair's waypoint deleted once");
        require(manager.getActiveArea() == nullptr, "rejected active selection cleared");
        for (const char* name : {"hotel", "northbridge", "roadconstruct", "southblock"})
            require(manager.getStrategicObject(name) == nullptr, "missing area name unpublished");
        const auto& objects = manager.getStrategicObjects();
        require(objects.size() == (mixed ? 3u : 0u), "only valid areas published");
        require(manager.getStrategicAreas().size() == objects.size(), "name roster agrees");
        require(manager.m_areaNames.size() == objects.size(), "reverse names agree");
        require(loaded.size() == objects.size(), "only survivors initialized");
        for (std::size_t index = 0; index < objects.size(); ++index)
        {
            require(objects[index] != nullptr && objects[index]->getIndex() == static_cast<int>(index),
                    "dense non-null roster with consistent indices");
            require(manager.getStrategicObject(static_cast<int>(index)) == objects[index], "indexed lookup");
            require(manager.m_objectsPerCategory[0][index] == objects[index], "category index compacted");
            require(manager.m_objectsPerCategory[1][index] == nullptr, "other category remains empty");
        }
        if (mixed)
        {
            require(base->m_neighbours == std::vector<AIStrategicObject*>{flag}, "neighbour references removed");
            require(base->m_neighbouringObjects == std::vector<AIStrategicObject*>{bounded},
                    "neighbouring object references removed");
            require(manager.m_waypointMap.count({0, 1}) == 1, "surviving waypoint pair reindexed");
            require(manager.getWaypoints(1, base, flag).size() == 1, "surviving waypoint retained");
            require(initializedWaypoints == 1, "only surviving waypoint initialized");
            require(manager.m_wayPointsToDrawFirst == 0 && manager.m_wayPointsToDrawSecond == 1,
                    "debug selection reindexed");
        }
        else
        {
            require(manager.m_waypointMap.empty(), "no phantom waypoint pair");
            require(initializedWaypoints == 0, "no waypoint initialized for missing areas");
            SAI sai(1, 0, 0, {}, {});
            sai.update(0.0); // Real update must return before timers, strategies, planners or orders.
            AIStrategicObject* object = nullptr;
            require(!sai.isPosInsideOwnedControlPoint(object, {}, 10.0f), "no owned point in empty roster");
        }
        // Manager destruction uses the real deferred-area destructor, including survivors.
    }
    CommonSingletonServer::m_iaiGame = nullptr;
    std::printf("PASS missing control-point areas: %d checks\n", checks);
}
