// [CHOSEN, not recovered] Shared effect-cache storage; the client compiles misses natively.
#pragma once
#include "EffectData.h"
#include <functional>

namespace bf2fx {
// Refused: a published D3DX refusal record; the entry carries D3DX's HRESULTs and error log.
// Unlike Miss it records no request: a warm cache does not re-request known refusals.
enum class Error { Ok, Miss, VersionMismatch, HashMismatch, InvalidInput, Io, Refused };
struct Result {
    Error error = Error::Ok;
    std::string detail;
    explicit operator bool() const { return error==Error::Ok; }
};
// G4 adapts ID3DXInclude::Open/Close to this interface. Buffers stay open until close.
class IncludeResolver {
public:
    virtual ~IncludeResolver() = default;
    virtual bool open(uint32_t kind,const std::string& name,const void* parent,
                      const void*& bytes,size_t& size) = 0;
    virtual void close(const void* bytes) = 0;
};
// Captures a conservative literal-include closure, including inactive branches.
// Missing optional includes are recorded. Macro include operands fail explicitly.
Result capture(const void* source,size_t size,const std::vector<Define>& defines,
               uint32_t flags,const std::string& dllHash,const std::string& translator,
               IncludeResolver* resolver,Request& request);
std::string cacheKey(const Request& request);
// On miss, atomically writes requests/<key>.request and appends the key to requests.txt.
Result load(const std::string& cache,const Request& request,Entry& entry);
// Creates entries/<key>/entry.bin atomically after validation, for native builds or dev tools.
// Refused entries go to refused/<key>.bin; they publish no FX2 alias.
Result publish(const std::string& cache,const Request& request,const Entry& entry);
} // namespace bf2fx
