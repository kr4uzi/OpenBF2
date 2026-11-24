// [TEMP] [CHOSEN, not recovered] Headless content-error regression fixture.
// Links the real engine bodies. Only logging streams, child objects and template
// lookup inputs are test collaborators. No startup, sockets, renderer or audio.
// Test-owned templates and objects stay alive until process exit.
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

// Supply an open recording stream to the real BF2Log/File sinks; production
// initLogFiles deliberately opens nothing in this tree.
#define private public
#include "../BF2/Game/Main/BF2Log.h"
#include "../BF2/IO/File/File.h"
#undef private
#include "../BF2/Core/ContentError.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Game/Main/Game.h"
#include "../BF2/Game/Objects/Object.h"
#include "../BF2/Game/Objects/Base/BundleTemplate.h"
#include "../BF2/Game/Objects/Game/KitTemplate.h"
#include "../BF2/Game/Objects/Game/SoldierTemplate.h"
#include "../BF2/IO/Stream/MemStream.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

using namespace dice::hfe;
using namespace dice::hfe::world;

namespace {
int checks;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", label); std::exit(1); }
}
std::size_t count(const std::string& text, const std::string& needle)
{
    std::size_t result = 0, pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos)
    { ++result; pos += needle.size(); }
    return result;
}
struct LogStream : io::MemStream
{
    // MemStream growth discards earlier bytes; reserve the fixture's entire log.
    LogStream() { setMaxSize(65536); }
    unsigned flushes = 0;
    void flushWrite() override { ++flushes; }
    std::string text() { return std::string(static_cast<const char*>(getPtr()), getSize()); }
};
struct Console : io::OldConsole
{
    std::string text;
    void output(const std::string& entry) override { text += entry; }
};
struct Child : Object
{
    int inits = 0;
    char id = -1;
    Mat4 placement = {};
    explicit Child(const IObjectTemplate* value) : Object(value) {}
    void setChildId(char value) override { id = value; }
    void setRelativeTransformation(const Mat4& value) override { placement = value; }
    void init() override { ++inits; }
};
struct Parent : Object
{
    std::vector<Child*> children;
    Parent() : Object(0) {}
    void addChild(IObject* child) override { children.push_back(static_cast<Child*>(child)); }
};
struct ChildTemplate : SimpleObjectTemplate
{
    IObject* createObject() const override { return new Child(this); }
};
struct Templates : ObjectTemplateManager
{
    std::map<std::string, IObjectTemplate*> inputs;
    IObjectTemplate* getTemplate(const std::string& name) const override
    {
        auto it = inputs.find(name);
        return it == inputs.end() ? 0 : it->second;
    }
};
struct Listener : IEventListener
{
    std::vector<unsigned> ids;
    void* lastData = 0;
    EventCategory lastCategory = ECGameLogic;
    void handleEvent(EventCategory category, unsigned id, EventNode*, void* data) override
    { ids.push_back(id); lastData = data; lastCategory = category; }
};
struct Node : SimpleEventNode
{
    int* releases;
    int* typeReads;
    explicit Node(unsigned id, int* counter = 0, int* reads = 0)
        : SimpleEventNode(id), releases(counter), typeReads(reads) {}
    int getType() const override
    { if (typeReads) ++*typeReads; return SimpleEventNode::getType(); }
    ~Node() override { if (releases) ++*releases; }
};

void children(BundleTemplate& owner, Parent& parent, const char* name)
{
    owner.setName(name);
    owner.addTemplate("missing-child");
    owner.addTemplate("valid-child");
    owner.setPosition(Vec3{1, 2, 3}, 1);
    owner.addBundleChilds(&parent);
    check(parent.children.size() == 1, "missing child skipped, later child created");
    Child* child = parent.children.back();
    check(child->id == 1 && child->inits == 1, "missing child still consumes its child ID");
    check(child->placement.m[12] == 1 && child->placement.m[13] == 2 &&
          child->placement.m[14] == 3, "later child retains authored placement");
    for (int i = 0; i != 10; ++i) owner.addBundleChilds(&parent);
    check(parent.children.size() == 11, "repeat errors do not stop child construction");
    owner.addTemplate("second-missing");
    owner.addBundleChilds(&parent);
}

void events(Console& console)
{
    EventManager manager;
    Listener listener;
    for (EventCategory category : {ECGame, ECGameLogic, ECHud, ECSound})
        check(manager.registerEventHandler(category, &listener, 0), "register event listener");
    const std::size_t before = console.text.size();
    int data = 7, released = 0;
    for (int i = 0; i != 10; ++i)
    {
        check(manager.postEvent(ECGame, 701, &data, 0) == 0, "raw event dispatched immediately");
        check(listener.ids.back() == 701 && listener.lastData == &data,
              "unregistered raw event keeps ID and data");
        manager.postEvent(ECGame, new Node(702, &released), 0);
        check(listener.ids.back() == 702, "unregistered typed event dispatched");
        const std::size_t delivered = listener.ids.size();
        check(manager.postEventNextUpdate(ECGame, new Node(703, &released)) != 0,
              "unregistered next-update event enqueued");
        check(listener.ids.size() == delivered, "next-update event waits");
        manager.processEvents(0.01f);
        check(listener.ids.back() == 703, "unregistered next-update event dispatched");
    }
    check(released == 20, "typed nodes released after dispatch");
    // The same category/ID reached through other overloads must not flood the log.
    manager.postEvent(ECGame, new Node(701), 0);
    manager.postEvent(ECGame, 702, &data, 0);
    manager.postEventNextUpdate(ECGame, new Node(702));
    manager.processEvents(0.01f);
    int typeReads = 0;
    manager.postEvent(ECGame, new Node(702, 0, &typeReads), 0);
    check(typeReads == (DEDICATED ? 2 : 4), "typed validation preserves virtual ID read count");
    typeReads = 0;
    manager.postEventNextUpdate(ECGame, new Node(702, 0, &typeReads));
    manager.processEvents(0.01f);
    check(typeReads == (DEDICATED ? 2 : 4), "queued validation preserves virtual ID read count");
    const std::size_t errorEnd = console.text.size();
    check(count(console.text.substr(before), "Attempted use of unregistred event") ==
          (DEDICATED ? 0u : 3u), "one Error per category/ID across overloads");

    manager.registerEvent(ECGame, 704, "registered");
    manager.postEvent(ECGame, 704, &data, 0);
    manager.postEvent(ECGame, new Node(704), 0);
    manager.postEventNextUpdate(ECGame, new Node(704));
    manager.processEvents(0.01f);
    manager.postEvent(ECHud, 705, &data, 0);
    manager.postEvent(ECSound, new Node(705), 0);
    manager.postEventNextUpdate(ECSound, new Node(705));
    manager.postEvent(ECGame, new NotificationEventNode(706, &data), 0);
    manager.postEventNextUpdate(ECGame, 706, &data);
    manager.processEvents(0.01f);
    manager.postEvent(ECGame, static_cast<EventNode*>(0), 0);
    manager.postEventNextUpdate(ECGame, static_cast<EventNode*>(0));
    manager.postEvent(ECPhysics, new Node(707, &released), 0);
    manager.postEventNextUpdate(ECPhysics, new Node(707, &released));
    check(released == 22, "no-listener nodes released without validation");
    check(console.text.size() == errorEnd, "registered events and exemptions keep their guards");

    // Category is part of the key; an identical ID in another category reports.
    manager.postEvent(ECGameLogic, 701, &data, 0);
    check(count(console.text.substr(before), "Attempted use of unregistred event") ==
          (DEDICATED ? 0u : 4u), "event category distinguishes error content");
#if !DEDICATED
    setDedicated(true);
    const std::size_t dedicatedStart = console.text.size();
    manager.postEvent(ECGame, 708, &data, 0);
    manager.postEvent(ECGame, new Node(709), 0);
    manager.postEventNextUpdate(ECGame, new Node(710));
    manager.processEvents(0.01f);
    check(console.text.size() == dedicatedStart, "runtime dedicated mode skips client event checks");
    setDedicated(false);
#endif
    for (EventCategory category : {ECGame, ECGameLogic, ECHud, ECSound})
        check(manager.unregisterEventHandler(category, &listener), "unregister event listener");
}
} // namespace

int main(int argc, char** argv)
{
    check(argc == 3 && std::string(argv[1]) == "+noSound" && std::string(argv[2]) == "1",
          "headless fixture must be invoked with +noSound 1");
    io::mainConsole = 0;
    reportContentError("Core/ContentFixture.cpp", 1, "early content error");
    reportContentError("Core/ContentFixture.cpp", 1, "early content error");
    Console console;
    io::mainConsole = &console;
    BF2Log log;
    log.initDebugCallback();
    auto* stream = new LogStream();
    log.m_logFile = new io::File();
    log.m_logFile->m_file = stream;
    check(debugOn[DebugTypeError], "Error diagnostics remain enabled");
#if !DEDICATED
    setDedicated(false);
#endif
    eventManager = new EventManager();
    auto* templates = new Templates();
    objectTemplateManager = templates;
    templates->inputs["valid-child"] = new ChildTemplate();

    auto* bundle = new BundleTemplate();
    auto* bundleParent = new Parent();
    children(*bundle, *bundleParent, "bundle-fixture");
    check(count(console.text, "bundle-fixture addBundleChilds() template not found:") ==
          (DEDICATED ? 0u : 2u), "bundle reports once per distinct missing child");
    const std::size_t soundsStart = console.text.size();
    bundle->addTemplate("s_optional");
    bundle->addBundleChilds(bundleParent);
    check(console.text.size() == soundsStart, "optional S_ sound remains exempt");
#if !DEDICATED
    setDedicated(true);
    auto* dedicatedBundle = new BundleTemplate(); dedicatedBundle->setName("runtime-dedicated");
    dedicatedBundle->addTemplate("missing-child");
    dedicatedBundle->addBundleChilds(bundleParent);
    check(console.text.size() == soundsStart, "runtime dedicated mode skips bundle diagnostic");
    setDedicated(false);
#endif

    auto* kit = new KitTemplate();
    children(*kit, *new Parent(), "kit-fixture");
    check(count(console.text, "kit-fixture addBundleChilds() template not found:") == 2,
          "kit reports once per distinct missing child");
    auto* otherKit = new KitTemplate(); otherKit->setName("other-kit");
    otherKit->addTemplate("missing-child"); otherKit->addBundleChilds(new Parent());
    check(count(console.text, "other-kit addBundleChilds() template not found:") == 1,
          "parent name distinguishes missing-child content");

    events(console);
    auto* soldier = new SoldierTemplate(); soldier->setName("soldier-fixture");
    io::MemStream save;
    io::IStream* saveStream = static_cast<io::IMemStream*>(&save);
    for (int i = 0; i != 10; ++i)
        check(!soldier->makeScript(saveStream) && save.getSize() == 0,
              "soldier save returns false without writing");
    auto* otherSoldier = new SoldierTemplate(); otherSoldier->setName("other-soldier");
    check(!otherSoldier->makeScript(saveStream), "another soldier save still refuses");
    check(count(console.text, "SoldierTemplate doesn't have makeScript implemented. Save failed...") == 2,
          "one refusal Error per distinct soldier template");
    if (stream->text() != console.text)
        std::fprintf(stderr, "Typed log:\n%sConsole:\n%s", stream->text().c_str(), console.text.c_str());
    check(stream->text() == console.text && !console.text.empty(),
          "all content messages reach both normal typed log and console");
    check(count(console.text, "): Error: ") == count(console.text, "\n"),
          "all messages retain Error severity");
    check(stream->flushes == count(console.text, "\n"), "each emitted Error flushes the log");

    // An unrelated DebugTypeError must still be fatal with the real BF2Log callback.
    const pid_t child = fork();
    check(child != -1, "fork fatal-policy check");
    if (child == 0)
    {
        Debug(DebugTypeError, "Core/ContentFixture.cpp", 2) << "unrelated error remains fatal";
        _exit(0);
    }
    int status = 0;
    check(waitpid(child, &status, 0) == child &&
          (WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0)),
          "unrelated Error still terminates");
    std::printf("%s", console.text.c_str());
    std::printf("content errors DEDICATED=%d: %d checks passed\n", DEDICATED, checks);
    return 0;
}
