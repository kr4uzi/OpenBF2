// [TEMP] [CHOSEN, not recovered] Synthetic observer of production hash sizing
// and rehash lifetime. No game content, engine startup or replacement bodies.
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <new>
#include <vector>
#include "base/utility.h"
#define private public
#include "base/container.h"
#undef private

static unsigned checks;
static void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(1); }
}

// First request for each capacity, from the inspected x87 instruction sequence
// under CW=0x007f. This is a test oracle, separate from the production search.
struct Interval { int first; uint32_t capacity; };
static const Interval retail[] = {
    {1, 1u}, {2, 2u}, {3, 4u}, {5, 8u}, {9, 16u}, {17, 32u},
    {33, 64u}, {65, 128u}, {129, 256u}, {257, 512u}, {513, 1024u},
    {1025, 2048u}, {2049, 4096u}, {4097, 8192u}, {8193, 16384u},
    {16385, 32768u}, {32769, 65536u}, {65537, 131072u},
    {131073, 262144u}, {262145, 524288u}, {524289, 1048576u},
    {1048577, 2097152u}, {2097153, 4194304u}, {4194304, 8388608u},
    {8388606, 16777216u}, {16777210, 33554432u},
    {33554419, 67108864u}, {67108837, 134217728u},
    {134217674, 268435456u}, {268435350, 536870912u},
    {536870703, 1073741824u}, {1073741414, 2147483648u}, {2147482846, 1u}
};

static uint32_t expected(int n)
{
    for (unsigned i = 1; i < sizeof(retail) / sizeof(retail[0]); ++i)
        if (n < retail[i].first) return retail[i - 1].capacity;
    return retail[32].capacity;
}

static void sizing()
{
    const uint32_t small[] = {1, 2, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16, 32};
    for (int n = 1; n <= 17; ++n)
        check(tu_util::hash_raw_capacity(n) == small[n - 1], "retail sizes 1..17");
    for (unsigned i = 0; i < sizeof(retail) / sizeof(retail[0]); ++i) {
        const int first = retail[i].first;
        const int last = i == 32 ? INT_MAX : retail[i + 1].first - 1;
        check(tu_util::hash_raw_capacity(first) == retail[i].capacity, "interval first");
        check(tu_util::hash_raw_capacity(last) == retail[i].capacity, "interval last");
        check(tu_util::hash_raw_capacity(first + (last - first) / 2) == retail[i].capacity,
              "interval midpoint");
    }
    for (int n = 1; n <= 1048576; ++n)
        check(tu_util::hash_raw_capacity(n) == expected(n), "exhaustive small requests");
    for (unsigned e = 0; e <= 31; ++e) {
        for (int delta = -2048; delta <= 2048; ++delta) {
            const int64_t n = (int64_t(1) << e) + delta;
            if (n > 0 && n <= INT_MAX)
                check(tu_util::hash_raw_capacity(int(n)) == expected(int(n)), "power-of-two edge");
        }
    }
    uint32_t state = 0x574653u;
    for (unsigned i = 0; i < 100000; ++i) {
        state = state * 1664525u + 1013904223u;
        const int n = int(state & 0x7fffffffu);
        if (n) check(tu_util::hash_raw_capacity(n) == expected(n), "full-range sample");
    }
}

static bool recording;
static std::vector<int> events;
struct Key {
    int id;
    explicit Key(int n = 0) : id(n) {}
    Key(const Key& k) : id(k.id) { if (recording) events.push_back(100 + id); }
    ~Key() { if (recording) events.push_back(300 + id); }
    bool operator==(const Key& k) const { return id == k.id; }
};
struct Value {
    int id;
    explicit Value(int n = 0) : id(n) {}
    Value(const Value& v) : id(v.id) { if (recording) events.push_back(200 + id); }
    ~Value() { if (recording) events.push_back(400 + id); }
};
struct KeyHash { size_t operator()(const Key& k) const { return size_t(k.id); } };

static void rehash()
{
    hash<Key, Value, KeyHash> h;
    h.set_raw_capacity(4);
    h.add(Key(0), Value(0)); h.add(Key(4), Value(4)); h.add(Key(1), Value(1));
    check(h.m_table->m_size_mask == 3, "no minimum-16 clamp");
    check(h.E(0).first.id == 4 && h.E(1).first.id == 1 && h.E(2).first.id == 0,
          "colliding and displaced chains before rehash");
    events.clear(); recording = true;
    h.set_raw_capacity(17);
    recording = false;
    const int order[] = {104, 204, 304, 404, 101, 201, 301, 401, 100, 200, 300, 400};
    check(events == std::vector<int>(order, order + 12), "index-order copy then key/value destruction");
    check(h.size() == 3 && h.m_table->m_size_mask == 31, "rehash publishes count/mask");
    for (int key : {0, 4, 1}) {
        Value out;
        check(h.get(Key(key), &out) && out.id == key, "lookup survives rehash");
    }
    void* previous = h.m_table;
    h.set_raw_capacity(17);
    check(h.m_table != previous, "same capacity still allocates before releasing old storage");
    h.clear();
    check(h.m_table == NULL, "explicit clear resets storage word");
}

template<int Words> struct Payload { int words[Words]; };
template<int Words> static void trivial_instance()
{
    hash<int, Payload<Words>, identity_hash<int> > h;
    for (int n = 1; n <= 1024; ++n) {
        h.set_raw_capacity(n);
        check(uint32_t(h.m_table->m_size_mask) + 1u == expected(n), "typed allocation mask");
        check(h.size() == 0, "empty entry count");
        for (int j = 0; j <= h.m_table->m_size_mask; ++j)
            if (h.E(j).m_next_in_chain != -2) check(false, "empty sentinels");
    }
    h.set_raw_capacity(1);
    Payload<Words> value = {};
    h.add(7, value);
    check(h.size() == 1 && h.get(7, NULL), "one-bucket table accepts its first element");
    h.add(8, value);
    check(h.size() == 2 && h.m_table->m_size_mask == 1, "tiny table expands from entry count");
    // The existing native adapter uses clear; older retail template instances
    // leave a dangling word here and later ones clear it. No owner is invented.
    h.set_raw_capacity(0);
    check(h.m_table == NULL, "native nonpositive clear adapter");
    h.set_raw_capacity(-1);
    check(h.m_table == NULL, "nonpositive request on empty table");
    h.set_raw_capacity(INT_MAX);
    check(h.m_table->m_size_mask == 0, "exponent 32 wraps to a one-bucket allocation");
}

int main()
{
    sizing(); rehash();
    trivial_instance<1>(); trivial_instance<3>(); trivial_instance<4>();
    trivial_instance<5>(); trivial_instance<8>(); trivial_instance<11>();
    std::printf("swf_hash_capacity: %u checks passed\n", checks);
}
