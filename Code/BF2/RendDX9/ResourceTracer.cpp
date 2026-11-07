// Debug bookkeeping of D3D resources: each ResourceTracer holds an IDirect3DResource9 reference,
// a name, a call-stack string, its resource kind and an allocation number, and registers itself
// in one static registry. The retail image keeps the class but nothing creates a tracer: every
// body here is unreachable in 1.50 (no code or data references in RendDX9.dll, and the _r image's
// constructors have no callers either), so the registry is only ever empty.
// Placement: the _r image's assert names D:\DiceCanada\BoosterPack2\Code\BF2\RendDX9\ResourceTracer.cpp
// ("!hr && m_pResource", "Invalid resource type added.", "ResourceTracer released ").

#ifdef BF2_IMAGE_RENDDX9

#include "Texture.h"

#include <list>
#include <map>
#include <string>

namespace dice {
namespace hfe {
namespace dx9rend {

namespace {
// IID_IDirect3DResource9, the interface the constructors query (PE .rdata 0x102085A0).
const GUID kIID_Resource9 = { 0x05EEC05D, 0x8F7D, 0x4362, { 0xB9,0x99,0xD1,0xBA,0xF3,0x57,0xC7,0x04 } };
}

class ResourceTracer;

// The static registry at PE 0x1023BCAC (0x24 bytes in the x86 image). Its constructor
// (PE 0x101CB330) builds the list and the map and zeroes +0x18/+0x1C, not +0x20; its atexit
// (PE 0x101D6CA0) destroys only the map and the list, so the type has no user destructor.
// [CHOSEN, not recovered] The type name, the member names, the map's key/value types and the
// interface type of the two counted pointers: nothing inserts into the map (only its
// constructor/destructor touch it; node size 0x18 = an 8-byte value of two words), and the
// pointers are only stored, AddRef'd and Released (slots +4/+8), so no instruction exposes more.
// The registry is never filled in 1.50 (see the file comment).
class ResourceTracerRegistry
{
public:
    ResourceTracerRegistry() : m_resourceA(0), m_resourceB(0) {}

    void add(ResourceTracer* tracer);
    void setResourceA(IUnknown* resource);
    void setResourceB(IUnknown* resource);
    void releaseAll();

private:
    std::list<ResourceTracer*> m_tracers;                     // +0x00
    std::map<IDirect3DResource9*, ResourceTracer*> m_byResource; // +0x0C
    IUnknown* m_resourceA;                                    // +0x18
    IUnknown* m_resourceB;                                    // +0x1C
    ResourceTracer* m_last;                                   // +0x20 (static storage, zero)
};

// Kinds (+0x3C), named by the _r image's table at 0x104EFE70: 0 "VertexBuffer",
// 1 "IndexBuffer", 2 "Texture", 3 "Declaration".
class ResourceTracer
{
public:
    explicit ResourceTracer(IDirect3DVertexBuffer9* resource);
    explicit ResourceTracer(IDirect3DIndexBuffer9* resource);
    explicit ResourceTracer(IDirect3DBaseTexture9* resource);
    explicit ResourceTracer(IDirect3DVertexDeclaration9* resource);
    ~ResourceTracer();

    void setName(const std::string& name);
    std::string& getName();

    static int s_allocCount;                  // PE 0x1023BCA8
    static ResourceTracerRegistry s_registry; // PE 0x1023BCAC

private:
    IDirect3DResource9* m_pResource; // +0x00
    std::string m_name;              // +0x04
    std::string m_callstack;         // +0x20
    int m_type;                      // +0x3C
    int m_allocId;                   // +0x40
};

int ResourceTracer::s_allocCount;
ResourceTracerRegistry ResourceTracer::s_registry;

// [CHOSEN, not recovered] The four constructors' parameter types: each body only calls slot 0
// (QueryInterface) of its argument, so the D3D interface type is not observable; they follow the
// kind names above. The constructors differ only in the kind they store (PE 0x1000E840 kind 0,
// 0x1000E8C0 kind 1, 0x1000E940 kind 2, 0x1000E9C0 kind 3). The QueryInterface result is not
// tested (the _r build asserts "!hr && m_pResource" there, "Invalid resource type added.").
// unreachable in 1.50: no code or data references
// PE 0x1000E840
ResourceTracer::ResourceTracer(IDirect3DVertexBuffer9* resource) : m_type(0)
{
    resource->QueryInterface(kIID_Resource9, reinterpret_cast<void**>(&m_pResource));
    m_allocId = ++s_allocCount;
    s_registry.add(this);
}

// unreachable in 1.50: no code or data references
// PE 0x1000E8C0
ResourceTracer::ResourceTracer(IDirect3DIndexBuffer9* resource) : m_type(1)
{
    resource->QueryInterface(kIID_Resource9, reinterpret_cast<void**>(&m_pResource));
    m_allocId = ++s_allocCount;
    s_registry.add(this);
}

// unreachable in 1.50: no code or data references
// PE 0x1000E940
ResourceTracer::ResourceTracer(IDirect3DBaseTexture9* resource) : m_type(2)
{
    resource->QueryInterface(kIID_Resource9, reinterpret_cast<void**>(&m_pResource));
    m_allocId = ++s_allocCount;
    s_registry.add(this);
}

// unreachable in 1.50: no code or data references
// PE 0x1000E9C0
ResourceTracer::ResourceTracer(IDirect3DVertexDeclaration9* resource) : m_type(3)
{
    resource->QueryInterface(kIID_Resource9, reinterpret_cast<void**>(&m_pResource));
    m_allocId = ++s_allocCount;
    s_registry.add(this);
}

// Drops every remaining reference of the traced resource. The _r build logs
// "ResourceTracer released <kind> <name> allocId: <id> w/ refCnt: <count>" and the call stack
// inside the first branch; the retail build has no logging there.
// unreachable in 1.50: no code or data references
// PE 0x1000E740
ResourceTracer::~ResourceTracer()
{
    if (m_pResource->Release() != 0) {
        while (m_pResource->Release() != 0) {
        }
    }
}

// unreachable in 1.50: no code or data references
// PE 0x1000E6D0
void ResourceTracer::setName(const std::string& name)
{
    m_name = name;
}

// [INFERRED] The getter next to setName (lea eax, [ecx+4]; ret); Ghidra lists no function there.
// unreachable in 1.50: no code or data references
// PE 0x1000E6E0
std::string& ResourceTracer::getName()
{
    return m_name;
}

// unreachable in 1.50: no code or data references
// PE 0x1000E800 (inlined into the four constructors)
void ResourceTracerRegistry::add(ResourceTracer* tracer)
{
    m_tracers.push_back(tracer);
    m_last = tracer;
}

// Stores the pointer and takes a reference; the previous pointer is not released.
// unreachable in 1.50: no code or data references
// PE 0x1000E6F0
void ResourceTracerRegistry::setResourceA(IUnknown* resource)
{
    m_resourceA = resource;
    resource->AddRef();
}

// unreachable in 1.50: no code or data references
// PE 0x1000E710
void ResourceTracerRegistry::setResourceB(IUnknown* resource)
{
    m_resourceB = resource;
    resource->AddRef();
}

// Deletes every tracer, empties the list, then releases +0x1C and +0x18 without a null test
// and without clearing them. Ghidra lists no function at this address.
// unreachable in 1.50: no code or data references
// PE 0x1000E770
void ResourceTracerRegistry::releaseAll()
{
    std::list<ResourceTracer*>::iterator it = m_tracers.begin();
    while (it != m_tracers.end()) {
        delete *it;
        it = m_tracers.erase(it);
    }
    m_resourceB->Release();
    m_resourceA->Release();
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
