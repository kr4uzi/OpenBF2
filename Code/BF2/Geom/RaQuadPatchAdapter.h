// Terrain-water geometry and the pooled adapters retained by queued draws.
#ifndef BF2_GEOM_RAQUADPATCHADAPTER_H
#define BF2_GEOM_RAQUADPATCHADAPTER_H

#include "../RendDX9/Ra/RaParameterBlocks.h"
#include <stddef.h>

namespace dice { namespace hfe {
namespace dx9rend { struct RenderQueueEntry; namespace Ra { struct RaLightRecord; } }
namespace geom {

// Prefix of the hand-built render queue table (PE 0x101E1848). No C++ destructor slots.
class RaQuadPatchQueue
{
public:
    virtual void* destroy(unsigned int flags) = 0;
    virtual void add(dx9rend::RenderQueueEntry* entry) = 0;
};

struct RaQuadPatchAdapter
{
    RaQuadPatchAdapter();
    RaQuadPatchAdapter(void* vertices, void* compactVertices, void* indices, void* declaration);
    RaQuadPatchAdapter(const RaQuadPatchAdapter& source);
    ~RaQuadPatchAdapter();
    RaQuadPatchAdapter& operator=(const RaQuadPatchAdapter& source);
    // [INFERRED] name; PE 0x101BBB20 has no caller in 1.50 (raQuadPatchAdapterSubmit inlines the stores).
    void setDrawRange(int primitiveCount, int vertexCount, int primitiveType, int vertexStart,
                      int indexOffset, int indexStart);

    // Manual lifetime permits the shipped object-block copy constructor, without invoking
    // RaObjectParameters' unrelated default constructor on that path.
    union ObjectStorage
    {
        alignas(dx9rend::Ra::RaObjectParameters) unsigned char bytes[sizeof(dx9rend::Ra::RaObjectParameters)];
        ObjectStorage() {}
        ~ObjectStorage() {}
        dx9rend::Ra::RaObjectParameters& get()
        { return *reinterpret_cast<dx9rend::Ra::RaObjectParameters*>(bytes); }
        const dx9rend::Ra::RaObjectParameters& get() const
        { return *reinterpret_cast<const dx9rend::Ra::RaObjectParameters*>(bytes); }
    };

    void* const* vptr;                  // +0x000
    int primitiveCount;                // +0x004
    int vertexCount;                   // +0x008
    int primitiveType;                 // +0x00C
    int vertexStart;                   // +0x010
    int indexOffset;                   // +0x014
    int indexStart;                    // +0x018
    unsigned char bound;               // +0x01C
    ObjectStorage object;              // +0x020
    void* vertices;                    // +0x244, optional first stream
    void* compactVertices;             // +0x248
    void* indices;                     // +0x24C
    void* declaration;                 // +0x250
};

#if defined(_M_IX86)
static_assert(offsetof(RaQuadPatchAdapter, object) == 0x20, "adapter object parameters");
// The 0x224-byte object block holds one std::vector (UserData, +0x1E4) and has that size only with
// the image's VC7.1 STL; a current MSVC x86 build checks the block without it and the tail instead.
#if defined(_MSC_VER) && _MSC_VER == 1310
static_assert(sizeof(RaQuadPatchAdapter) == 0x254, "retail water adapter stride");
static_assert(offsetof(RaQuadPatchAdapter, compactVertices) == 0x248, "adapter compact stream");
#endif
static_assert(sizeof(dx9rend::Ra::RaObjectParameters) - sizeof(std::vector<unsigned char>) == 0x224 - 0x10,
              "object block without its UserData vector");
static_assert(offsetof(RaQuadPatchAdapter, vertices) - offsetof(RaQuadPatchAdapter, object)
                  == sizeof(dx9rend::Ra::RaObjectParameters)
              && offsetof(RaQuadPatchAdapter, compactVertices) - offsetof(RaQuadPatchAdapter, vertices) == 0x248 - 0x244
              && sizeof(RaQuadPatchAdapter) - offsetof(RaQuadPatchAdapter, vertices) == 0x254 - 0x244,
              "adapter streams follow the object block - stride 0x254, see 0x101BC506");
#endif

// Borrowed settings are copied into the server; queued draws own their object/lighting copies.
void raQuadPatchAdapterSetSettings(RaQuadPatchQueue* target, const RaQuadPatchAdapter& adapter,
                                  void* material, dx9rend::Ra::RaObjectParameters* object,
                                  const dx9rend::Ra::RaLightingParameters& lighting,
                                  const dx9rend::Ra::RaLightRecord& light);
void raQuadPatchAdapterSubmit(int primitiveCount, int vertexCount, int primitiveType,
                             int vertexStart, int indexOffset, int indexStart);
void raQuadPatchAdapterCheckWaterQueue(RaQuadPatchQueue* target);
void raQuadPatchAdapterServerDestroy();

} } }
#endif
