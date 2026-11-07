// [CHOSEN, not recovered] GPU equivalence check for native vs D3DX-compiled effect shaders.
//
//   gpucheck NATIVE.entry REFERENCE.entry [OUT.tsv] [LABEL]
//
// Both entries describe the same effect with the same object numbering (nativefx structure).
// For every shader object that both sides compiled, the MojoShader MSL of each side runs on
// the GPU with identical inputs: parameters get deterministic values by name (packed through
// each side's own constant reflection with the s2 Runtime), vertex attributes by usage/index,
// pixel-shader interpolants by their MSL [[user(...)]] name, textures by sampler parameter.
// Vertex shaders run as compute kernels (the MSL vertex function called per thread); pixel
// shaders render one pixel per test case. Outputs are compared by semantic.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "Cache.h"
#include "Runtime.h"
#include "Wire.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace bf2fx;

namespace {
const int kCases = 64;          // Test vectors per shader.
const int kSide = 8;            // Pixel shader target is kSide x kSide.
float gVaryingLo = 0.f, gVaryingHi = 1.f;

uint64_t hash(const std::string& s, uint64_t seed = 1469598103934665603ull) {
    uint64_t h = seed * 1099511628211ull + 7;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    return h;
}
float unit(uint64_t h) { h ^= h >> 33; h *= 0xff51afd7ed558ccdull; h ^= h >> 33; return float(h % 1000003) / 1000003.0f; }
// Parameter values: magnitudes in [0.25, 1] with either sign, so divisions stay finite.
bool gPositiveParams = false; // Diagnostic: colours/intensities are non-negative in game data.
float paramValue(const std::string& name, uint32_t k) {
    float u = unit(hash(name, k + 7)), sgn = !gPositiveParams && unit(hash(name, k + 9001)) < 0.5f ? -1.f : 1.f;
    return sgn * (0.25f + 0.75f * u);
}

struct Member { std::string type, name, attr; };
struct MslInfo {
    std::string function, kind;          // "vertex" / "fragment".
    std::vector<Member> input, output;   // Struct members in declaration order.
    std::vector<std::string> params;     // Function parameters as written.
};
std::vector<Member> structMembers(const std::string& msl, const std::string& name) {
    std::vector<Member> out;
    auto at = msl.find("struct " + name + "\n");
    if (at == std::string::npos) return out;
    auto end = msl.find("};", at);
    std::istringstream body(msl.substr(at, end - at));
    std::string line;
    std::regex re(R"(^\s*(\w+)\s+(\w+)\s*(\[\[[^\]]*\]\])?\s*;)");
    std::smatch m;
    while (std::getline(body, line))
        if (std::regex_search(line, m, re)) out.push_back({m[1], m[2], m[3]});
    return out;
}
bool parseMsl(const std::string& msl, MslInfo& info) {
    std::smatch m;
    if (!std::regex_search(msl, m, std::regex(R"((vertex|fragment) (\w+)_Output (\w+) \()"))) return false;
    info.kind = m[1]; info.function = m[3];
    info.input = structMembers(msl, info.function + "_Input");
    info.output = structMembers(msl, info.function + "_Output");
    auto open = msl.find(info.function + " (\n");
    auto close = msl.find(") {", open);
    if (open == std::string::npos || close == std::string::npos) return false;
    std::istringstream ps(msl.substr(open + info.function.size() + 3, close - open - info.function.size() - 3));
    std::string line;
    while (std::getline(ps, line)) {
        auto b = line.find_first_not_of(" \t");
        if (b == std::string::npos) continue;
        line = line.substr(b);
        if (!line.empty() && line.back() == ',') line.pop_back();
        info.params.push_back(line);
    }
    return true;
}
std::string stripAttr(std::string s) { return std::regex_replace(s, std::regex(R"(\s*\[\[[^\]]*\]\])"), ""); }

struct RunResult {
    bool ok = false;
    std::string error;
    std::map<std::string, std::vector<float>> outputs;
    std::map<std::string, unsigned> written; // Components the shader assigns, per output.
};
// Components of an output register that the MSL assigns (MojoShader: `oD0.xyz = ...;`).
unsigned writtenMask(const std::string& msl, const std::string& name) {
    unsigned mask = 0;
    std::regex re("(^|[;{\\s])" + name + "(\\.([xyzw]{1,4}))?\\s*=[^=]");
    for (auto it = std::sregex_iterator(msl.begin(), msl.end(), re); it != std::sregex_iterator(); ++it) {
        std::string sw = (*it)[3];
        if (sw.empty()) return 0xF;
        for (char c : sw) mask |= 1u << (c == 'x' ? 0 : c == 'y' ? 1 : c == 'z' ? 2 : 3);
    }
    return mask;
}

class Gpu {
public:
    Gpu() { device_ = MTLCreateSystemDefaultDevice(); queue_ = [device_ newCommandQueue]; }
    bool ok() const { return device_ != nil; }
    id<MTLLibrary> library(const std::string& src, std::string& error) {
        NSError* err = nil;
        id<MTLLibrary> lib = [device_ newLibraryWithSource:[NSString stringWithUTF8String:src.c_str()] options:[MTLCompileOptions new] error:&err];
        if (!lib) error = err ? [[err localizedDescription] UTF8String] : "library failed";
        return lib;
    }
    // Deterministic RGBA8 textures by sampler parameter name; kind 0 2D, 1 cube, 2 volume.
    id<MTLTexture> texture(const std::string& name, int kind) {
        auto key = name + "#" + std::to_string(kind);
        auto it = textures_.find(key);
        if (it != textures_.end()) return it->second;
        const int n = 16;
        MTLTextureDescriptor* d = [MTLTextureDescriptor new];
        d.pixelFormat = MTLPixelFormatRGBA8Unorm; d.width = n; d.height = n;
        d.textureType = kind == 1 ? MTLTextureTypeCube : kind == 2 ? MTLTextureType3D : MTLTextureType2D;
        if (kind == 2) d.depth = n;
        id<MTLTexture> t = [device_ newTextureWithDescriptor:d];
        int slices = kind == 1 ? 6 : 1, depth = kind == 2 ? n : 1;
        std::vector<uint8_t> px(n * n * 4);
        for (int s = 0; s < slices; ++s)
            for (int z = 0; z < depth; ++z) {
                for (int i = 0; i < n * n * 4; ++i) px[i] = uint8_t(255 * unit(hash(name, uint64_t(i + 1000 * (s + 7 * z)))));
                [t replaceRegion:MTLRegionMake3D(0, 0, z, n, n, 1) mipmapLevel:0 slice:s withBytes:px.data() bytesPerRow:n * 4 bytesPerImage:n * n * 4];
            }
        textures_[key] = t;
        return t;
    }
    id<MTLSamplerState> sampler() {
        if (!sampler_) {
            MTLSamplerDescriptor* d = [MTLSamplerDescriptor new];
            d.minFilter = d.magFilter = MTLSamplerMinMagFilterLinear;
            d.sAddressMode = d.tAddressMode = d.rAddressMode = MTLSamplerAddressModeRepeat;
            sampler_ = [device_ newSamplerStateWithDescriptor:d];
        }
        return sampler_;
    }
    id<MTLDevice> device_ = nil;
    id<MTLCommandQueue> queue_ = nil;
    std::map<std::string, id<MTLTexture>> textures_;
    id<MTLSamplerState> sampler_ = nil;
};

// Constant buffer(0) for one shader, through the s2 runtime's own register mapping.
std::vector<uint8_t> constants(const Description& d, const Shader& s) {
    Runtime rt(d);
    for (const auto& p : d.parameters) {
        const Type& t = p.value.type;
        if (t.cls > 3) continue;
        bf2fx::Handle h = rt.getParameterByName(t.name);
        if (!h) continue;
        size_t n = std::max<size_t>(1, t.elements) * std::max(1u, t.rows) * std::max(1u, t.columns);
        if (t.type == Float) {
            std::vector<float> v(n);
            for (size_t k = 0; k < n; ++k) v[k] = paramValue(t.name, uint32_t(k));
            rt.setFloatArray(h, v.data(), n);
        } else if (t.type == Int) {
            std::vector<int32_t> v(n);
            for (size_t k = 0; k < n; ++k) v[k] = int32_t(unit(hash(t.name, k + 3)) * 3);
            rt.setIntArray(h, v.data(), n);
        } else if (t.type == Bool) {
            std::vector<int32_t> v(n);
            for (size_t k = 0; k < n; ++k) v[k] = unit(hash(t.name, k + 5)) < 0.5f;
            rt.setBoolArray(h, v.data(), n);
        }
    }
    // s2 Runtime::mapConstants stops on struct constants (a runtime frontier). Map the other
    // bindings through it and give each struct register deterministic values by parameter
    // name and register offset (both compilers lay struct members out one per register).
    Shader plain = s;
    std::vector<Binding> structs;
    plain.bindings.clear();
    for (const auto& b : s.bindings) (b.set != 3 && (b.type.cls == 5 || b.type.type == 0) ? structs : plain.bindings).push_back(b);
    Registers r = rt.mapConstants(plain);
    for (const auto& b : structs)
        for (uint32_t k = 0; k < b.count; ++k) {
            size_t reg = b.index + k;
            if (r.f.size() < (reg + 1) * 4) { r.f.resize((reg + 1) * 4); r.knownF.resize((reg + 1) * 4); }
            for (int c = 0; c < 4; ++c) r.f[reg * 4 + c] = paramValue(b.type.name + "#" + std::to_string(k), uint32_t(c));
        }
    // Dense MojoShader layout: float4 uniforms, then int4, then bools, in reflection order.
    std::vector<uint8_t> out;
    auto put = [&](const void* p, size_t n) { auto b = static_cast<const uint8_t*>(p); out.insert(out.end(), b, b + n); };
    for (uint32_t pass = 0; pass < 3; ++pass)
        for (const auto& u : s.uniforms) {
            if (u.kind != pass || u.constant) continue; // DEF constants are inline in the MSL.
            for (uint32_t i = 0; i < u.count; ++i) {
                size_t reg = u.index + i;
                if (pass == 0) { float v[4] = {}; for (int c = 0; c < 4; ++c) if (reg * 4 + c < r.f.size()) v[c] = r.f[reg * 4 + c]; put(v, 16); }
                else if (pass == 1) { int32_t v[4] = {}; for (int c = 0; c < 4; ++c) if (reg * 4 + c < r.i.size()) v[c] = r.i[reg * 4 + c]; put(v, 16); }
                else { uint8_t v = reg < r.b.size() ? r.b[reg] : 0; put(&v, 1); }
            }
        }
    while (out.size() % 16 || out.empty()) out.push_back(0);
    return out;
}

// Vertex attributes in their realistic domain by D3DDECLUSAGE: positions w = 1, blend
// indices as D3DCOLOR bytes of small integers, weights/colors/texture coordinates in [0,1].
bool gIntegerAttributes = false;
int gMaxIndex = 26; // Blend indices in [0, gMaxIndex): within GeomBones[26].
float attrValue(uint32_t usage, const std::string& key, int vertex, int c) {
    float u = unit(hash(key, uint64_t(vertex * 4 + c + 11)));
    // Diagnostic mode: texture coordinates carrying array indices (Decals) as integers.
    if (gIntegerAttributes && usage == 5) return float(int(u * 4));
    switch (usage) {
        case 0: return c == 3 ? 1.f : 2.f * u - 1.f;                 // POSITION
        case 2: return float(int(u * gMaxIndex)) / 255.f;             // BLENDINDICES (UBYTE4 via D3DCOLOR)
        case 1: case 5: case 10: return u;                            // BLENDWEIGHT, TEXCOORD, COLOR
        default: return 2.f * u - 1.f;                                // NORMAL, TANGENT, BINORMAL, ...
    }
}
float varyingValue(const std::string& attr, int sample, int c) {
    return gVaryingLo + (gVaryingHi - gVaryingLo) * unit(hash(attr, uint64_t(sample * 4 + c + 13)));
}

RunResult runVertex(Gpu& gpu, const Description& d, const Shader& s) {
    RunResult res;
    MslInfo info;
    if (!parseMsl(s.msl, info) || info.kind != "vertex") { res.error = "unparsed vertex MSL"; return res; }
    std::string src = s.msl;
    // The vertex function becomes a plain function called from a compute kernel.
    auto at = src.find("vertex " + info.function + "_Output " + info.function + " (");
    auto close = src.find(") {", at);
    std::string params, call;
    for (size_t i = 0; i < info.params.size(); ++i) {
        std::string p = stripAttr(info.params[i]);
        params += (i ? ", " : "") + p;
        call += i ? ", " : "";
        if (p.find("_Uniforms") != std::string::npos) call += "uniforms";
        else if (p.find("_Input") != std::string::npos) call += "inputs[id]";
        else { res.error = "vertex parameter not supported: " + p; return res; }
    }
    src.replace(at, close - at, "static " + info.function + "_Output " + info.function + " (" + params);
    for (const char* suffix : {"_Input", "_Output"}) {
        auto sa = src.find("struct " + info.function + suffix + "\n");
        if (sa == std::string::npos) continue;
        auto se = src.find("};", sa);
        src.replace(sa, se - sa, stripAttr(src.substr(sa, se - sa)));
    }
    bool hasUniforms = src.find("struct " + info.function + "_Uniforms\n") != std::string::npos;
    bool hasInput = src.find("struct " + info.function + "_Input\n") != std::string::npos;
    std::string prefix;
    if (!hasUniforms) prefix += "struct " + info.function + "_Uniforms { float4 unused; };\n";
    if (!hasInput) prefix += "struct " + info.function + "_Input { float4 unused; };\n";
    auto ns = src.find("using namespace metal;\n");
    src.insert(ns == std::string::npos ? 0 : ns + 22, prefix);
    src += "\nkernel void bf2fx_run(constant " + info.function + "_Uniforms &uniforms [[buffer(0)]],\n"
           "    device const " + info.function + "_Input *inputs [[buffer(1)]], device float4 *out [[buffer(2)]],\n"
           "    uint id [[thread_position_in_grid]]) {\n    (void)inputs; (void)uniforms;\n    " + info.function + "_Output o = " +
           info.function + "(" + call + ");\n";
    for (size_t k = 0; k < info.output.size(); ++k)
        src += "    out[id * " + std::to_string(info.output.size()) + " + " + std::to_string(k) + "] = float4(o." + info.output[k].name + ");\n";
    src += "}\n";
    std::string err;
    id<MTLLibrary> lib = gpu.library(src, err);
    if (!lib) { res.error = "msl: " + err.substr(0, 300); return res; }
    NSError* nerr = nil;
    id<MTLComputePipelineState> pso = [gpu.device_ newComputePipelineStateWithFunction:[lib newFunctionWithName:@"bf2fx_run"] error:&nerr];
    if (!pso) { res.error = "pipeline"; return res; }
    size_t members = std::max<size_t>(1, info.input.size());
    std::vector<float> in(size_t(kCases) * members * 4);
    for (size_t m = 0; m < info.input.size(); ++m) {
        std::string key = info.input[m].name;
        uint32_t usage = 99;
        for (const auto& a : s.attributes)
            if (a.name == info.input[m].name) { key = "u" + std::to_string(a.usage) + "i" + std::to_string(a.index); usage = a.usage; }
        for (int v = 0; v < kCases; ++v) for (int c = 0; c < 4; ++c) in[(size_t(v) * members + m) * 4 + c] = attrValue(usage, key, v, c);
    }
    auto cb = constants(d, s);
    size_t outCount = size_t(kCases) * std::max<size_t>(1, info.output.size()) * 4;
    id<MTLBuffer> bc = [gpu.device_ newBufferWithBytes:cb.data() length:cb.size() options:MTLResourceStorageModeShared];
    id<MTLBuffer> bi = [gpu.device_ newBufferWithBytes:in.data() length:in.size() * 4 options:MTLResourceStorageModeShared];
    id<MTLBuffer> bo = [gpu.device_ newBufferWithLength:outCount * 4 options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> cmd = [gpu.queue_ commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    [enc setComputePipelineState:pso];
    [enc setBuffer:bc offset:0 atIndex:0]; [enc setBuffer:bi offset:0 atIndex:1]; [enc setBuffer:bo offset:0 atIndex:2];
    [enc dispatchThreads:MTLSizeMake(kCases, 1, 1) threadsPerThreadgroup:MTLSizeMake(std::min<NSUInteger>(64, pso.maxTotalThreadsPerThreadgroup), 1, 1)];
    [enc endEncoding]; [cmd commit]; [cmd waitUntilCompleted];
    const float* o = static_cast<const float*>([bo contents]);
    for (size_t k = 0; k < info.output.size(); ++k) {
        std::string key = info.output[k].attr.empty() ? info.output[k].name : info.output[k].attr;
        res.written[key] = writtenMask(s.msl, info.output[k].name);
        auto& v = res.outputs[key];
        for (int i = 0; i < kCases; ++i) for (int c = 0; c < 4; ++c) v.push_back(o[(size_t(i) * info.output.size() + k) * 4 + c]);
    }
    res.ok = true;
    return res;
}

RunResult runPixel(Gpu& gpu, const Description& d, const Shader& s) {
    RunResult res;
    MslInfo info;
    if (!parseMsl(s.msl, info) || info.kind != "fragment") { res.error = "unparsed fragment MSL"; return res; }
    for (const auto& o : info.output)
        if (o.attr.find("color(") == std::string::npos) { res.error = "output " + o.attr + " not supported"; return res; }
    for (const auto& p : info.params)
        if (p.find("[[stage_in]]") == std::string::npos && p.find("[[texture(") == std::string::npos &&
            p.find("[[sampler(") == std::string::npos && p.find("[[buffer(") == std::string::npos) {
            res.error = "fragment parameter not supported: " + p; return res;
        }
    // A feeding vertex function: one quad per test case, interpolants constant across it.
    size_t members = std::max<size_t>(1, info.input.size());
    std::string vs = "#include <metal_stdlib>\nusing namespace metal;\nstruct FeedOut {\n    float4 pos [[position]];\n";
    for (size_t k = 0; k < info.input.size(); ++k) vs += "    float4 m" + std::to_string(k) + " " + info.input[k].attr + ";\n";
    vs += "};\nvertex FeedOut bf2fx_feed(uint vid [[vertex_id]], device const float4 *v [[buffer(1)]]) {\n"
          "    uint quad = vid / 6, corner = vid % 6;\n"
          "    const float2 c[6] = {float2(0,0), float2(1,0), float2(0,1), float2(1,0), float2(1,1), float2(0,1)};\n"
          "    float2 p = (float2(quad % " + std::to_string(kSide) + ", quad / " + std::to_string(kSide) + ") + c[corner]) / " +
          std::to_string(kSide) + ".0;\n    FeedOut o;\n    o.pos = float4(p.x * 2 - 1, 1 - p.y * 2, 0.5, 1);\n    (void)v;\n";
    for (size_t k = 0; k < info.input.size(); ++k)
        vs += "    o.m" + std::to_string(k) + " = v[quad * " + std::to_string(members) + " + " + std::to_string(k) + "];\n";
    vs += "    return o;\n}\n";
    std::string err;
    id<MTLLibrary> vlib = gpu.library(vs, err);
    if (!vlib) { res.error = "feed msl: " + err.substr(0, 300); return res; }
    id<MTLLibrary> flib = gpu.library(s.msl, err);
    if (!flib) { res.error = "msl: " + err.substr(0, 300); return res; }
    MTLRenderPipelineDescriptor* pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [vlib newFunctionWithName:@"bf2fx_feed"];
    pd.fragmentFunction = [flib newFunctionWithName:[NSString stringWithUTF8String:info.function.c_str()]];
    std::vector<int> colors;
    for (const auto& o : info.output) colors.push_back(std::stoi(o.attr.substr(o.attr.find("color(") + 6)));
    for (int c : colors) pd.colorAttachments[c].pixelFormat = MTLPixelFormatRGBA32Float;
    NSError* nerr = nil;
    id<MTLRenderPipelineState> pso = [gpu.device_ newRenderPipelineStateWithDescriptor:pd error:&nerr];
    if (!pso) { res.error = std::string("pipeline: ") + (nerr ? [[nerr localizedDescription] UTF8String] : ""); return res; }
    std::vector<float> feed(size_t(kCases) * members * 4);
    for (size_t k = 0; k < info.input.size(); ++k)
        for (int i = 0; i < kCases; ++i) for (int c = 0; c < 4; ++c)
            feed[(size_t(i) * members + k) * 4 + c] = varyingValue(info.input[k].attr, i, c);
    MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor renderPassDescriptor];
    std::vector<id<MTLTexture>> targets;
    for (int c : colors) {
        MTLTextureDescriptor* td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:kSide height:kSide mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget; td.storageMode = MTLStorageModeManaged;
        id<MTLTexture> t = [gpu.device_ newTextureWithDescriptor:td];
        rp.colorAttachments[c].texture = t; rp.colorAttachments[c].loadAction = MTLLoadActionClear;
        rp.colorAttachments[c].clearColor = MTLClearColorMake(-7, -7, -7, -7); // Discarded pixels keep this.
        rp.colorAttachments[c].storeAction = MTLStoreActionStore;
        targets.push_back(t);
    }
    auto cb = constants(d, s);
    id<MTLCommandBuffer> cmd = [gpu.queue_ commandBuffer];
    id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:rp];
    [enc setRenderPipelineState:pso];
    [enc setVertexBytes:feed.data() length:feed.size() * 4 atIndex:1];
    [enc setFragmentBytes:cb.data() length:cb.size() atIndex:0];
    for (const auto& smp : s.samplers) {
        // Texture of the sampler parameter bound to this register (constant table).
        std::string param = smp.name;
        for (const auto& b : s.bindings)
            if (b.set == 3 && smp.index >= b.index && smp.index < b.index + std::max(1u, b.count)) param = b.type.name;
        int kind = smp.kind == 1 ? 1 : smp.kind == 2 ? 2 : 0; // MojoShader: 0 2D, 1 cube, 2 volume.
        [enc setFragmentTexture:gpu.texture(param, kind) atIndex:smp.index];
        [enc setFragmentSamplerState:gpu.sampler() atIndex:smp.index];
    }
    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:kCases * 6];
    [enc endEncoding];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    for (auto t : targets) [blit synchronizeResource:t];
    [blit endEncoding];
    [cmd commit]; [cmd waitUntilCompleted];
    for (size_t k = 0; k < targets.size(); ++k) {
        std::vector<float> px(size_t(kSide) * kSide * 4);
        [targets[k] getBytes:px.data() bytesPerRow:kSide * 16 fromRegion:MTLRegionMake2D(0, 0, kSide, kSide) mipmapLevel:0];
        res.outputs[info.output[k].attr] = px;
        res.written[info.output[k].attr] = writtenMask(s.msl, info.output[k].name);
    }
    res.ok = true;
    return res;
}

struct Compare { double maxAbs = 0; size_t values = 0, bad = 0, unwritten = 0; std::string worst; };
Compare compare(const RunResult& ref, const RunResult& nat) {
    Compare c;
    for (const auto& [key, va] : ref.outputs) {
        auto it = nat.outputs.find(key);
        if (it == nat.outputs.end()) { c.bad += va.size(); c.worst = "missing output " + key; continue; }
        const auto& vb = it->second;
        unsigned mr = ref.written.count(key) ? ref.written.at(key) : 0xF, mn = nat.written.count(key) ? nat.written.at(key) : 0xF;
        for (size_t i = 0; i < va.size() && i < vb.size(); ++i) {
            unsigned bit = 1u << (i % 4);
            // Components one side never assigns are undefined there (HLSL never declared them).
            if (!(mr & bit) || !(mn & bit)) { if ((mr & bit) && !(mn & bit)) ++c.unwritten; continue; }
            ++c.values;
            double x = va[i], y = vb[i];
            if ((std::isnan(x) && std::isnan(y)) || x == y) continue; // Also equal infinities.
            double diff = std::fabs(x - y), tol = 1e-3 * std::max({1.0, std::fabs(x), std::fabs(y)});
            if (!(diff <= tol)) {
                ++c.bad;
                if (!(diff <= c.maxAbs)) {
                    c.maxAbs = std::isfinite(diff) ? diff : 1e30;
                    c.worst = key + "[" + std::to_string(i / 4) + "." + "xyzw"[i % 4] + "] native " + std::to_string(y) + " ref " + std::to_string(x);
                }
            }
        }
    }
    for (const auto& [key, vb] : nat.outputs) if (!ref.outputs.count(key)) { c.bad += vb.size(); c.worst = "extra output " + key; }
    return c;
}
} // namespace

int main(int argc, char** argv) {
    @autoreleasepool {
        if (argc < 3) { fprintf(stderr, "usage: gpucheck NATIVE.entry REFERENCE.entry [OUT.tsv] [LABEL]\n"); return 2; }
        if (const char* r = getenv("GPUCHECK_VARYING_RANGE")) sscanf(r, "%f,%f", &gVaryingLo, &gVaryingHi);
        gIntegerAttributes = getenv("GPUCHECK_INTEGER_TEXCOORDS") != nullptr;
        gPositiveParams = getenv("GPUCHECK_POSITIVE_PARAMS") != nullptr;
        if (const char* m = getenv("GPUCHECK_MAX_INDEX")) gMaxIndex = atoi(m);
        Gpu gpu;
        if (!gpu.ok()) { fprintf(stderr, "no Metal device\n"); return 3; }
        Entry native = unenvelope<Entry>(readFile(argv[1]), "entry");
        Entry ref = unenvelope<Entry>(readFile(argv[2]), "entry");
        std::ofstream file;
        if (argc > 3) file.open(argv[3], std::ios::app);
        std::ostream& out = argc > 3 ? static_cast<std::ostream&>(file) : std::cout;
        std::string label = argc > 4 ? argv[4] : argv[1];
        for (const auto& rs : ref.description.shaders) {
            if (rs.selectorKind || rs.msl.empty()) continue;
            const Shader* ns = nullptr;
            for (const auto& s : native.description.shaders) if (s.object == rs.object) ns = &s;
            std::string status, detail;
            // s2 Runtime::mapConstants stops (frontier) on struct constants (Ra `Lights`).
            auto structConstant = [](const Shader& sh) {
                for (const auto& b : sh.bindings) if (b.set != 3 && (b.type.cls == 5 || b.type.type == 0)) return true;
                return false;
            };
            if (!ns || ns->msl.empty()) status = "native_missing";
            else if (getenv("GPUCHECK_SKIP_STRUCTS") && (structConstant(rs) || structConstant(*ns))) { status = "untested"; detail = "struct constant"; }
            else {
                RunResult a = rs.stage == VertexShader ? runVertex(gpu, ref.description, rs) : runPixel(gpu, ref.description, rs);
                RunResult b = rs.stage == VertexShader ? runVertex(gpu, native.description, *ns) : runPixel(gpu, native.description, *ns);
                if (!a.ok) { status = "ref_untested"; detail = a.error; }
                else if (!b.ok) { status = "native_untested"; detail = b.error; }
                else {
                    if (getenv("GPUCHECK_DUMP") && std::to_string(rs.object) == getenv("GPUCHECK_DUMP"))
                        for (const auto& [k, v] : a.outputs) {
                            const auto& w = b.outputs[k];
                            for (int i = 0; i < 3 && size_t(i * 4 + 3) < v.size(); ++i)
                                fprintf(stderr, "%s case %d ref %g %g %g %g native %g %g %g %g\n", k.c_str(), i, v[i*4], v[i*4+1], v[i*4+2], v[i*4+3],
                                        w[i*4], w[i*4+1], w[i*4+2], w[i*4+3]);
                        }
                    Compare c = compare(a, b);
                    char buf[96]; snprintf(buf, sizeof buf, "%zu/%zu max %.3g unwritten %zu", c.bad, c.values, c.maxAbs, c.unwritten);
                    status = c.bad ? "differs" : "equal"; detail = std::string(buf) + (c.bad ? " " + c.worst : "");
                }
            }
            for (auto& ch : detail) if (ch == '\t' || ch == '\n') ch = ' ';
            out << label << "\t" << rs.object << "\t" << (rs.stage == VertexShader ? "vs" : "ps") << "\t" << rs.major << "." << rs.minor
                << "\t" << (ns ? std::to_string(ns->major) + "." + std::to_string(ns->minor) : "-") << "\t" << status << "\t" << detail << "\n";
        }
    }
    return 0;
}
