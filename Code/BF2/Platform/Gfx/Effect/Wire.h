// [CHOSEN, not recovered] Bounded, versioned little-endian cache serialization.
#pragma once
#include "EffectData.h"
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace bf2fx {
std::string sha256(const Bytes& bytes);
Bytes readFile(const std::string& path);
void writeFile(const std::string& path, const Bytes& bytes);
struct Writer {
    Bytes bytes;
    template<class... T> void operator()(const T&... x) { int unused[] = {0,(put(x),0)...}; (void)unused; }
    template<class T> typename std::enable_if<std::is_integral<T>::value>::type put(T x) {
        for (size_t i=0;i<sizeof(T);++i) bytes.push_back(uint8_t(uint64_t(x) >> (i*8)));
    }
    void put(const std::string& x) { put(uint32_t(x.size())); bytes.insert(bytes.end(),x.begin(),x.end()); }
    template<class T> void put(const std::vector<T>& x) { put(uint32_t(x.size())); for (const auto& v:x) put(v); }
    template<class T> auto put(const T& x) -> decltype(const_cast<T&>(x).io(*this),void()) { const_cast<T&>(x).io(*this); }
};
struct Reader {
    const Bytes& bytes;
    size_t offset = 0, depth = 0, budget = 1000000;
    explicit Reader(const Bytes& b):bytes(b) {}
    template<class... T> void operator()(T&... x) { int unused[] = {0,(get(x),0)...}; (void)unused; }
    template<class T> typename std::enable_if<std::is_integral<T>::value>::type get(T& x) {
        if (bytes.size()-offset<sizeof(T)) throw std::runtime_error("Truncated cache record");
        uint64_t v=0; for(size_t i=0;i<sizeof(T);++i) v|=uint64_t(bytes[offset++])<<(i*8); x=T(v);
    }
    void get(std::string& x) {
        uint32_t n; get(n); if(n>bytes.size()-offset) throw std::runtime_error("Invalid cache string");
        x.assign(reinterpret_cast<const char*>(bytes.data()+offset),n); offset+=n;
    }
    template<class T> void get(std::vector<T>& x) {
        uint32_t n; get(n);
        if(n>bytes.size()-offset || n>budget) throw std::runtime_error("Invalid cache vector");
        budget-=n; x.resize(n); for(auto& v:x) get(v);
    }
    template<class T> auto get(T& x) -> decltype(x.io(*this),void()) {
        if(++depth>64) throw std::runtime_error("Cache nesting limit"); x.io(*this); --depth;
    }
};
template<class T> Bytes encode(const T& x) { Writer w; w(x); return w.bytes; }
template<class T> T decode(const Bytes& bytes) {
    Reader r(bytes); T x; r(x); if(r.offset!=bytes.size()) throw std::runtime_error("Trailing cache bytes"); return x;
}
template<class T> Bytes envelope(const T& x, const std::string& kind) {
    Bytes payload=encode(x); Writer w; w(std::string("BF2FX"),CacheVersion,kind,sha256(payload),payload); return w.bytes;
}
template<class T> T unenvelope(const Bytes& bytes,const std::string& expected) {
    Reader r(bytes); std::string magic,kind,hash; uint32_t version; Bytes payload;
    r(magic,version,kind,hash,payload);
    // Request snapshots are layout-stable since version 1, so an older cache can be retargeted.
    const bool compatible=version==CacheVersion || (expected=="request" && version==1);
    if(magic!="BF2FX" || !compatible || kind!=expected) throw std::runtime_error("BF2FX_VERSION_MISMATCH");
    if(r.offset!=bytes.size() || sha256(payload)!=hash) throw std::runtime_error("BF2FX_HASH_MISMATCH");
    return decode<T>(payload);
}
} // namespace bf2fx
