// [TEMP] [CHOSEN, not recovered] Headless control-point lookup scaffolding. Real native headers
// and exact production lookup bodies; chosen lifetimes omit singleton registration and world work.
// This exercises missing IDs and live owner/capture dispatch, not level loading or engine teardown.
#include "bf_game_counter_runtime.h"
#include "lookup_collaborators.inl"
#include <limits>

namespace dice { namespace hfe {
IGameLogic* gameLogic = nullptr;
namespace ai {
IAIGame::IAIGame() {}
IAIGame::~IAIGame() {}
BFGame::BFGame() : m_isPrecaching(true) {}
BFGame::~BFGame() {}
}
namespace world {
IObject::IObject() {}
IObject::~IObject() {}
Object::Object(const IObjectTemplate*) {}
Object::~Object() {}
SimpleObject::SimpleObject(const SimpleObjectTemplate*) : Object(nullptr) {}
SimpleObject::~SimpleObject() {}
Bundle::Bundle(const BundleTemplate*) : SimpleObject(nullptr) {}
Bundle::~Bundle() {}
ControlPoint::ControlPoint(const ControlPointTemplate*) : Bundle(nullptr) {}
ControlPoint::~ControlPoint() {}
}
} }

using namespace dice::hfe;
using namespace dice::hfe::ai;
using namespace dice::hfe::world;

class LookupPoint : public ControlPoint
{
public:
    LookupPoint() : ControlPoint(nullptr) {}
    int team = 2;
    float counter = 0.75f;
    int teamReads = 0;
    int counterReads = 0;
    int getTeam() override { ++teamReads; return team; }
    float getTakeOverCounter() override { ++counterReads; return counter; }
};

static void missing(const IAIGame& adapter, LookupLogic& logic, unsigned int id)
{
    const auto before = logic.points;
    const int calls = logic.calls;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Vec3 position = {nan, 13.0f, -27.0f};
    float radius = nan;
    bool active = true;
    require(!adapter.getControlPointInfo(id, position, radius, active), "missing info is false");
    require(position.x == 0.0f && position.y == 0.0f && position.z == 0.0f,
            "all missing position components are zero");
    require(radius == 0.0f && !active, "missing radius and active are zero");
    require(adapter.getOwnerOfControlPoint(id) == 0, "missing owner is neutral");
    require(adapter.getControlPointTakeOverCounter(id) == 0.0f, "missing capture progress is zero");
    require(logic.calls == calls + 3, "one map query per lookup");
    require(logic.points == before, "missing lookups do not insert an entry");
}

int main()
{
    BFGame adapter;
    LookupLogic logic;
    LookupPoint point;
    gameLogic = &logic;
    for (unsigned int id : {0u, 602u, 0x80000000u, 0xffffffffu})
        missing(adapter, logic, id);

    for (unsigned int id : {101u, 103u, 104u, 109u})
        logic.points[id] = &point;
    for (unsigned int id : {0u, 102u, 108u, 601u, 602u, 603u, 604u, 0x80000000u, 0xffffffffu})
        missing(adapter, logic, id);
    require(point.teamReads == 0 && point.counterReads == 0, "missing IDs never dispatch to another point");

    // A valid key still dispatches exactly once and forwards current values, including neutral
    // ownership and negative capture progress; the guard must not change or cache these results.
    for (unsigned int id : {0u, 103u, 0x80000000u, 0xffffffffu})
    {
        logic.points[id] = &point;
        point.team = 2;
        point.counter = 0.75f;
        require(adapter.getOwnerOfControlPoint(id) == 2, "valid owner");
        require(adapter.getControlPointTakeOverCounter(id) == 0.75f, "valid capture progress");
        point.team = 0;
        point.counter = -0.5f;
        require(adapter.getOwnerOfControlPoint(id) == 0, "live neutral owner");
        require(adapter.getControlPointTakeOverCounter(id) == -0.5f, "live negative capture progress");
        logic.points.erase(id);
        missing(adapter, logic, id);
    }
    require(point.teamReads == 8 && point.counterReads == 8, "exact valid dispatch count");
    std::printf("PASS control-point lookups: %d checks\n", checks);
}
