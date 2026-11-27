// [TEMP] [CHOSEN, not recovered] Typed observers around the linked production effector.
// No world, bot, audio device, renderer or production startup is constructed.
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Game/IGameLogic.h"
#include "../BF2/IO/Input/PlayerInput.h"
#include "../BF2/Game/Objects/Base/SimpleObjectTemplate.h"
#include "../BF2/Physics/MaterialManager.h"
#include "../BF2/AI/AIInterface/BFKnowledge/BFGameEffector.h"
#include "../BF2/AI/AIInterface/BFKnowledge/BFTimer.h"
#include "../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <vector>

using namespace dice::hfe;
using namespace dice::hfe::ai;
namespace dice { namespace hfe { extern ClassManager* classManager;
namespace world { extern IBase* materialManager; }
} }
namespace dice { namespace hfe { namespace ai {
// [TEMP] Test-only access to seed the consumer while its production caller is missing.
struct BFGameEffectorQueueFixture
{
    static void seed(BFGameEffector& effector, const Vec3& point, int a, int b)
    { effector.m_soundEffects.push_back(BFGameEffector::SoundEffect{point, a, b}); }
    static std::size_t size(const BFGameEffector& effector)
    { return effector.m_soundEffects.size(); }
    static std::size_t capacity(const BFGameEffector& effector)
    { return effector.m_soundEffects.capacity(); }
};
} } }
static int checks, failures;
static void check(bool pass, const char* text)
{ ++checks; if (!pass) { ++failures; std::fprintf(stderr, "FAIL %s\n", text); } }
struct Dispatch { Vec3 point; const world::IObjectTemplate* effect; };
static std::vector<Dispatch> dispatches;
static std::vector<std::pair<unsigned int, unsigned int>> selections;
static void recordEffect(Vec3 point, const world::IObjectTemplate* effect, Vec3 up)
{
    check(up.x == 0 && up.y == 1 && up.z == 0, "up direction");
    dispatches.push_back({point, effect});
}
#include <ai_effect_observer.h>
struct Materials : world::MaterialManager
{
    world::SimpleObjectTemplate first, second;
    void* getEffectTemplate(unsigned int a, unsigned int b, int index) override
    {
        check(index == 0, "material variant zero");
        selections.push_back({a, b});
        return a == 4 ? &first : &second;
    }
};
struct Timer : IBFTimer
{
    mutable unsigned int reads = 0;
    std::vector<double> times;
    void resetTimes(std::initializer_list<double> values) { times = values; reads = 0; }
    double getUpdateTime() const override
    { check(reads < times.size(), "timer read count"); if (reads >= times.size()) std::abort(); return times[reads++]; }
    double getGameTime() const override { std::abort(); }
    double getTimeIncrease() const override { std::abort(); }
    double getWallClockTime() const override { std::abort(); }
    void setPause(bool) override { std::abort(); }
    bool getPause() override { std::abort(); }
    void reset() override { std::abort(); }
};
int main()
{
    classManager = new ClassManager;
    auto* timer = new Timer;
    auto* materials = new Materials;
    gameLogic = new ObservedGameLogic;
    world::materialManager = materials;
    CommonSingletonServer::m_iBFTimer = timer;
    BFGameEffector effector;
    Vec3 position = {1, 2, 3};
    effector.playEffect(position, 4, 8);
    effector.playEffect(Vec3{5, 6, 7}, 9, 11);
    position = Vec3{100, 200, 300};
    check(dispatches.size() == 2 && selections[0] == std::make_pair(4u, 8u)
          && selections[1] == std::make_pair(9u, 11u), "immediate dispatch order");
    check(BFGameEffectorQueueFixture::size(effector) == 0
          && BFGameEffectorQueueFixture::capacity(effector) == 0,
          "playEffect dispatches without allocating or growing the queue");
    for (int i = 0; i < 256; ++i) effector.playEffect(position, 4, 8);
    check(dispatches.size() == 258 && BFGameEffectorQueueFixture::size(effector) == 0
          && BFGameEffectorQueueFixture::capacity(effector) == 0,
          "repeated immediate effects never accumulate queued records");
    dispatches.clear(); selections.clear();
#if DEDICATED
    effector.playSoundEffects(10);
    check(dispatches.empty() && selections.empty() && timer->reads == 0, "dedicated consumer is inert");
#else
    position = Vec3{1, 2, 3};
    BFGameEffectorQueueFixture::seed(effector, position, 4, 8);
    BFGameEffectorQueueFixture::seed(effector, Vec3{5, 6, 7}, 9, 11);
    position = Vec3{100, 200, 300};
    timer->resetTimes({10, 10.5, 11}); effector.playSoundEffects(5);
    check(dispatches.size() == 2 && dispatches[0].point.x == 1 && dispatches[0].point.y == 2
          && dispatches[0].point.z == 3 && dispatches[1].point.x == 5, "queued positions copied in order");
    check(dispatches[0].effect == &materials->first && dispatches[1].effect == &materials->second
          && selections[0] == std::make_pair(4u, 8u) && selections[1] == std::make_pair(9u, 11u), "queued IDs and template ownership");
    check(timer->reads == 3, "initial clock plus one read after each dispatch");
    check(BFGameEffectorQueueFixture::size(effector) == 0, "full drain clears queue");
    BFGameEffectorQueueFixture::seed(effector, Vec3{1, 0, 0}, 4, 8);
    BFGameEffectorQueueFixture::seed(effector, Vec3{2, 0, 0}, 9, 11);
    effector.playEffect(position, 4, 8);
    check(BFGameEffectorQueueFixture::size(effector) == 2,
          "playEffect leaves existing queued records unchanged");
    dispatches.clear(); selections.clear();
    timer->resetTimes({20, 21}); effector.playSoundEffects(0);
    check(dispatches.size() == 1 && dispatches[0].point.x == 1, "zero budget still dispatches first item");
    check(BFGameEffectorQueueFixture::size(effector) == 0, "budget stop clears entire queue");
    dispatches.clear(); selections.clear();
    timer->resetTimes({30}); effector.playSoundEffects(10);
    check(dispatches.empty() && selections.empty() && timer->reads == 1, "budget stop discards queued suffix");
    for (int i = 1; i <= 3; ++i)
        BFGameEffectorQueueFixture::seed(effector, Vec3{static_cast<float>(i), 0, 0}, 4, i);
    dispatches.clear(); selections.clear();
    timer->resetTimes({1, 2, 3}); effector.playSoundEffects(1);
    check(dispatches.size() == 2, "budget equality continues to next item");
    BFGameEffectorQueueFixture::seed(effector, position, 4, 8);
    BFGameEffectorQueueFixture::seed(effector, position, 9, 11);
    dispatches.clear(); selections.clear();
    timer->resetTimes({1, std::numeric_limits<double>::quiet_NaN(), 4});
    effector.playSoundEffects(1);
    check(dispatches.size() == 2, "unordered clock does not stop the walk");
#endif
    std::printf("AI material effects DEDICATED=%d: %d checks, %d failures\n", DEDICATED, checks, failures);
    return failures ? 1 : 0;
}
