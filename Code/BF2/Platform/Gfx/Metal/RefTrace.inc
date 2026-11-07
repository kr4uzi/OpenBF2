// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: rend-deinit-reftrace] Definitions for RefTrace.h;
// included once by Device.cpp, outside its anonymous namespace. Main thread only, like the device.
#include <execinfo.h>
#include <mach-o/dyld.h>
#include <cstdlib>
#include <unordered_map>

namespace bf2metal {
namespace reftrace {
namespace {

const int kFrames = 12;

struct Stack
{
    void* frames[kFrames];
    int count;
    bool operator<(const Stack& o) const
    {
        if (count != o.count) return count < o.count;
        return std::memcmp(frames, o.frames, sizeof(void*) * count) < 0;
    }
};

struct Holder
{
    const char* kind;
    RefsFn refs;
    Stack stack;
};

std::unordered_map<const void*, Holder>& holders()
{
    static std::unordered_map<const void*, Holder>* map = new std::unordered_map<const void*, Holder>();
    return *map;
}
std::map<Stack, long>& adds()     { static std::map<Stack, long>* m = new std::map<Stack, long>(); return *m; }
std::map<Stack, long>& releases() { static std::map<Stack, long>* m = new std::map<Stack, long>(); return *m; }
int s_owned = 0;

Stack capture(int skip)
{
    void* raw[kFrames + 4];
    const int n = ::backtrace(raw, kFrames + 4);
    Stack s = {};
    for (int i = skip; i < n && s.count < kFrames; ++i)
        s.frames[s.count++] = raw[i];
    return s;
}

void printStack(const Stack& s)
{
    std::fprintf(stderr, "[REFTRACE]     at");
    for (int i = 0; i < s.count; ++i)
        std::fprintf(stderr, " %p", s.frames[i]);
    std::fprintf(stderr, "\n");
}

} // namespace

bool enabled()
{
    static const int on = [] {
        const char* v = std::getenv("BF2_METAL_REFTRACE");
        return v && *v && *v != '0' ? 1 : 0;
    }();
    return on != 0;
}

void born(const void* object, const char* kind, RefsFn refs)
{
    if (!enabled()) return;
    Holder h = { kind, refs, capture(2) };
    holders()[object] = h;
}

void died(const void* object)
{
    if (!enabled()) return;
    holders().erase(object);
}

Owned::Owned()  { ++s_owned; }
Owned::~Owned() { --s_owned; }

void deviceRef(int delta)
{
    if (!enabled() || s_owned) return;
    ++(delta > 0 ? adds() : releases())[capture(2)];
}

void report(const void* device, unsigned long deviceRefs)
{
    if (!enabled()) return;
    std::fprintf(stderr, "[REFTRACE] device %p refs %lu; image base %p (atos -o bf2_client -l <base>)\n",
                 device, deviceRefs, static_cast<const void*>(_dyld_get_image_header(0)));
    struct Group { const char* kind; Stack stack; unsigned long refs; unsigned long objects; };
    std::map<std::pair<std::string, Stack>, Group> groups;
    unsigned long held = 0, objects = 0;
    for (const auto& entry : holders())
    {
        const unsigned long r = entry.second.refs(entry.first);
        if (!r) continue;
        held += r; ++objects;
        Group& g = groups[std::make_pair(std::string(entry.second.kind), entry.second.stack)];
        g.kind = entry.second.kind; g.stack = entry.second.stack; g.refs += r; ++g.objects;
    }
    std::fprintf(stderr, "[REFTRACE] %lu traced objects hold %lu device references\n", objects, held);
    std::vector<const Group*> order;
    for (const auto& g : groups) order.push_back(&g.second);
    std::sort(order.begin(), order.end(), [](const Group* a, const Group* b) { return a->refs > b->refs; });
    for (const Group* g : order)
    {
        std::fprintf(stderr, "[REFTRACE] holder %s: %lu objects, %lu refs\n", g->kind, g->objects, g->refs);
        printStack(g->stack);
    }
    long direct = 0;
    for (const auto& a : adds()) direct += a.second;
    for (const auto& r : releases()) direct -= r.second;
    std::fprintf(stderr, "[REFTRACE] direct device AddRef minus Release: %ld (%zu add stacks, %zu release stacks)\n",
                 direct, adds().size(), releases().size());
    auto top = [](const std::map<Stack, long>& m, const char* what) {
        std::vector<std::pair<long, const Stack*>> v;
        for (const auto& e : m) v.push_back(std::make_pair(e.second, &e.first));
        std::sort(v.begin(), v.end(), [](const std::pair<long, const Stack*>& a,
                                         const std::pair<long, const Stack*>& b) { return a.first > b.first; });
        for (std::size_t i = 0; i < v.size() && i < 40; ++i)
        {
            std::fprintf(stderr, "[REFTRACE] direct %s x%ld\n", what, v[i].first);
            printStack(*v[i].second);
        }
    };
    top(adds(), "add");
    top(releases(), "release");
    std::fflush(stderr);
}

} // namespace reftrace
} // namespace bf2metal
