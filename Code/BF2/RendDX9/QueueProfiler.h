#ifndef RENDDX9_QUEUE_PROFILER_H
#define RENDDX9_QUEUE_PROFILER_H

#include <cstdint>
#include <set>
#include <string>
#include <vector>

struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;

namespace dice { namespace hfe { namespace dx9rend {

// [INFERRED] Source names and grouping. Offsets describe the retail fields;
// native containers and pointers retain their own layout under issue #7.
struct QueueProfiler
{
    struct ResourceGroup
    {
        // [CHOSEN, not recovered] Unaccessed key spelling; native pointer width.
        std::set<std::uintptr_t> keys; // +0x00
        unsigned int untouched0C[4];  // +0x0C: count, unique count, bytes, unread word
        float sum;                   // +0x1C
        unsigned int peak;           // +0x20
    };

    explicit QueueProfiler(const std::string& title);
    ~QueueProfiler();           // leaves its registry entry behind
    void reset();
    void begin();
    static void end();
    unsigned int counter(unsigned int index) const;
    const char* counterName(unsigned int index) const;

    std::string name;                 // +0x000
    unsigned int id;                 // +0x01C
    unsigned int untouched20[16];     // +0x020
    unsigned int words60[16];         // +0x060
    unsigned int wordsA0[16];         // +0x0A0
    unsigned int wordsE0[16];         // +0x0E0
    unsigned int count120, count124;  // +0x120, +0x124
    unsigned int words128[16];        // +0x128
    float time168;                   // +0x168
    unsigned int untouched16C[3];     // +0x16C
    unsigned int words178[16];        // +0x178
    float framesPerSecond;           // +0x1B8
    unsigned int count1BC;            // +0x1BC
    float milliseconds1C0;            // +0x1C0
    unsigned char flag1C4, flag1C5, flag1C6, pad1C7;
    float scale1C8;                   // +0x1C8
    ResourceGroup resources[4];       // +0x1CC
    unsigned int untouched25C;        // +0x25C
    // GetFunction at PE 0x1002980F / 0x10029845; roles attested by vsv/psv headings.
    IDirect3DVertexShader9* vertexShader; // +0x260, no Release in the destructor
    IDirect3DPixelShader9* pixelShader;   // +0x264
};

extern std::vector<QueueProfiler*> g_queueProfilers;
extern std::vector<QueueProfiler*>* g_activeQueueProfilers;
extern unsigned int g_nextQueueProfilerId;
extern QueueProfiler* g_currentQueueProfiler;
extern QueueProfiler* g_defaultQueueProfiler;
extern QueueProfiler* g_totalQueueProfiler;

}}}
#endif
