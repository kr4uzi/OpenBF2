// [TEMP] [CHOSEN, not recovered] Native effect/draw replay with real BF2 requests.
// Pass a native cache containing SwiffMenu.fx and Font.fx entries and request snapshots.
#include "../../BF2/Platform/Gfx/D3DX9Subset.h"
#include "../../BF2/Platform/Gfx/Effect/Cache.h"
#include "../../BF2/Platform/Gfx/Effect/Wire.h"
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace g4 {
// Serves a captured request's include snapshots through the public ID3DXInclude contract.
struct SnapshotInclude final : ID3DXInclude {
    const bf2fx::Request& request;
    std::map<const void*, std::uint32_t> ordinals;
    std::vector<std::unique_ptr<std::vector<std::uint8_t>>> storage;
    unsigned opens = 0, closes = 0;
    explicit SnapshotInclude(const bf2fx::Request& r) : request(r) {}
    HRESULT STDMETHODCALLTYPE Open(D3DXINCLUDE_TYPE type, LPCSTR name, LPCVOID parent, LPCVOID* data, UINT* bytes) override {
        const std::uint32_t p = parent ? ordinals.at(parent) : 0;
        for (std::size_t i = 0; i < request.includes.size(); ++i) {
            const auto& inc = request.includes[i];
            if (inc.parent != p || inc.kind != std::uint32_t(type) || inc.name != name) continue;
            if (!inc.found) return E_FAIL;
            storage.emplace_back(new std::vector<std::uint8_t>(inc.bytes));
            if (storage.back()->empty()) storage.back()->reserve(1);
            *data = storage.back()->data(); *bytes = UINT(inc.bytes.size());
            ordinals[*data] = std::uint32_t(i + 1); ++opens; return S_OK;
        }
        return E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE Close(LPCVOID data) override { ordinals.erase(data); ++closes; return S_OK; }
};
// Forwards to the device and records what D3DX-style pass application issued.
struct RecordingManager final : ID3DXEffectStateManager {
    IDirect3DDevice9* d; ULONG refs = 1;
    std::vector<std::pair<unsigned, DWORD>> render, sampler;
    std::vector<unsigned> vsConstants, psConstants, textures;
    unsigned shaders = 0;
    explicit RecordingManager(IDirect3DDevice9* device) : d(device) {}
    void clear() { render.clear(); sampler.clear(); vsConstants.clear(); psConstants.clear(); textures.clear(); shaders = 0; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; }
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE, const D3DMATRIX*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL9*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetLight(DWORD, const D3DLIGHT9*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE s, DWORD v) override { render.push_back({unsigned(s), v}); return d->SetRenderState(s, v); }
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD s, IDirect3DBaseTexture9* t) override { textures.push_back(s); return d->SetTexture(s, t); }
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD s, D3DTEXTURESTAGESTATETYPE t, DWORD v) override { return d->SetTextureStageState(s, t, v); }
    HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD s, D3DSAMPLERSTATETYPE t, DWORD v) override { sampler.push_back({s * 100 + unsigned(t), v}); return d->SetSamplerState(s, t, v); }
    HRESULT STDMETHODCALLTYPE SetNPatchMode(FLOAT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetFVF(DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9* s) override { ++shaders; return d->SetVertexShader(s); }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT r, const FLOAT* v, UINT n) override { vsConstants.push_back(r); return d->SetVertexShaderConstantF(r, v, n); }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(UINT r, const INT* v, UINT n) override { return d->SetVertexShaderConstantI(r, v, n); }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT r, const BOOL* v, UINT n) override { return d->SetVertexShaderConstantB(r, v, n); }
    HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9* s) override { ++shaders; return d->SetPixelShader(s); }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT r, const FLOAT* v, UINT n) override { psConstants.push_back(r); return d->SetPixelShaderConstantF(r, v, n); }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(UINT r, const INT* v, UINT n) override { return d->SetPixelShaderConstantI(r, v, n); }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT r, const BOOL* v, UINT n) override { return d->SetPixelShaderConstantB(r, v, n); }
};
// The request snapshot whose published entry has the given kind and whose source names `marker`
// (keys change with the cache format, so requests are found by content, not by key).
bool request(const std::string& cache, const char* marker, std::uint32_t kind, bf2fx::Request& out, std::uint32_t flags = 0x100)
{
    namespace fs = std::filesystem;
    for (const auto& file : fs::directory_iterator(cache + "/requests")) {
        if (file.path().extension() != ".request") continue;
        auto r = bf2fx::unenvelope<bf2fx::Request>(bf2fx::readFile(file.path().string()), "request");
        const std::string source(r.source.begin(), r.source.end()), key = bf2fx::cacheKey(r);
        if (r.flags != flags || source.find(marker) == std::string::npos) continue;
        const fs::path entry = kind == bf2fx::RefusedEntry ? fs::path(cache) / "refused" / (key + ".bin")
                                                           : fs::path(cache) / "entries" / key / "entry.bin";
        if (!fs::exists(entry) || bf2fx::unenvelope<bf2fx::Entry>(bf2fx::readFile(entry.string()), "entry").kind != kind) continue;
        out = std::move(r); return true;
    }
    return false;
}
// A real effect whose static vertex shader in some pass binds the Lights struct (Ra statics).
bool lightsEffect(const std::string& cache, bf2fx::Request& out, bf2fx::Entry& entry, std::uint32_t& technique,
                  std::uint32_t& pass, bf2fx::Binding& binding, const char* name = "Lights", std::uint32_t minimum = 3)
{
    namespace fs = std::filesystem;
    for (const auto& dir : fs::directory_iterator(cache + "/entries")) {
        const auto requestPath = fs::path(cache) / "requests" / (dir.path().filename().string() + ".request");
        if (!fs::exists(requestPath) || !fs::exists(dir.path() / "entry.bin")) continue;
        auto e = bf2fx::unenvelope<bf2fx::Entry>(bf2fx::readFile((dir.path() / "entry.bin").string()), "entry");
        if (e.kind != bf2fx::EffectEntry) continue;
        for (std::uint32_t t = 0; t < e.description.techniques.size(); ++t)
            for (std::uint32_t p = 0; p < e.description.techniques[t].passes.size(); ++p)
                for (const auto& st : e.description.techniques[t].passes[p].states) {
                    if (st.id != 146 || !st.value.expression.empty() || st.value.words.empty()) continue;
                    for (const auto& sh : e.description.shaders) if (sh.object == st.value.words[0] && !sh.selectorKind)
                        for (const auto& b : sh.bindings) if (b.type.name == name && b.count >= minimum) {
                            out = bf2fx::unenvelope<bf2fx::Request>(bf2fx::readFile(requestPath.string()), "request");
                            entry = std::move(e); technique = t; pass = p; binding = b; return true;
                        }
                }
    }
    return false;
}
std::vector<D3DXMACRO> macroList(const bf2fx::Request& r)
{
    std::vector<D3DXMACRO> macros;
    for (const auto& m : r.defines) macros.push_back({m.name.c_str(), m.value.c_str()});
    macros.push_back({nullptr, nullptr});
    return macros;
}
std::size_t requestLines(const std::string& cache)
{
    std::FILE* f = std::fopen((cache + "/requests.txt").c_str(), "rb"); if (!f) return 0;
    std::size_t n = 0; for (int c; (c = std::fgetc(f)) != EOF;) n += c == '\n'; std::fclose(f); return n;
}
// D3DXCreateEffect from a recorded request: its exact source, defines, flags and includes.
HRESULT create(IDirect3DDevice9* d, const bf2fx::Request& r, ID3DXEffect** fx, SnapshotInclude& include, ID3DXBuffer** errors)
{
    std::vector<D3DXMACRO> macros;
    for (const auto& m : r.defines) macros.push_back({m.name.c_str(), m.value.c_str()});
    macros.push_back({nullptr, nullptr});
    return D3DXCreateEffect(d, r.source.data(), UINT(r.source.size()), macros.data(), &include, r.flags, nullptr, fx, errors);
}
// Reads back a 64x64 A8R8G8B8 render target as B,G,R,A bytes.
bool readback(IDirect3DDevice9* d, IDirect3DSurface9* target, std::vector<std::uint8_t>& pixels)
{
    IDirect3DSurface9* cpu = nullptr; D3DLOCKED_RECT lock = {};
    if (d->CreateOffscreenPlainSurface(64, 64, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &cpu, nullptr) != D3D_OK) return false;
    bool ok = d->GetRenderTargetData(target, cpu) == D3D_OK && cpu->LockRect(&lock, nullptr, D3DLOCK_READONLY) == D3D_OK;
    if (ok) {
        pixels.resize(64 * 64 * 4);
        for (unsigned y = 0; y < 64; ++y) std::memcpy(&pixels[y * 256], static_cast<std::uint8_t*>(lock.pBits) + y * lock.Pitch, 256);
        cpu->UnlockRect();
    }
    cpu->Release(); return ok;
}
bool near(const std::vector<std::uint8_t>& p, unsigned x, unsigned y, int r, int g, int b, int a, int tolerance = 2)
{
    const std::uint8_t* q = &p[(y * 64 + x) * 4];
    const bool ok = std::abs(q[2] - r) <= tolerance && std::abs(q[1] - g) <= tolerance && std::abs(q[0] - b) <= tolerance &&
        (a < 0 || std::abs(q[3] - a) <= tolerance);
    if (!ok) std::fprintf(stderr, "pixel (%u,%u) = %u,%u,%u,%u expected %d,%d,%d,%d\n", x, y, q[2], q[1], q[0], q[3], r, g, b, a);
    return ok;
}
IDirect3DTexture9* texture(IDirect3DDevice9* d, unsigned w, unsigned h, const std::uint32_t* argb)
{
    IDirect3DTexture9* t = nullptr; D3DLOCKED_RECT lock = {};
    if (d->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, nullptr) != D3D_OK) return nullptr;
    if (t->LockRect(0, &lock, nullptr, 0) != D3D_OK) { t->Release(); return nullptr; }
    for (unsigned y = 0; y < h; ++y) std::memcpy(static_cast<std::uint8_t*>(lock.pBits) + y * lock.Pitch, argb + y * w, w * 4);
    t->UnlockRect(0); return t;
}
} // namespace g4

int effectChecks(IDirect3DDevice9* d, const char* cache)
{
    using namespace g4;
    if (!cache) { std::fprintf(stderr, "effect checks need --effect FxNative-directory (SwiffMenu/Font)\n"); return 1; }
    setenv("BF2_FX_NATIVE_CACHE", cache, 1);
    ID3DXEffect* swiff = nullptr; ID3DXEffect* font = nullptr; ID3DXBuffer* errors = nullptr;
    bf2fx::Request swiffRequest, fontRequest;
    CHECK(request(cache, "#line 2 \"SwiffMenu.fx\"", bf2fx::EffectEntry, swiffRequest));
    CHECK(request(cache, "Font.fx", bf2fx::EffectEntry, fontRequest));

    // A warm native refusal records no request: the compiler object parses,
    // CompileEffect fails, and D3DXCreateEffect returns the native refusal directly.
    bf2fx::Request refusedRequest;
    if (request(cache, "AutoGenerated shader file", bf2fx::RefusedEntry, refusedRequest)) {
        const std::size_t before = requestLines(cache);
        auto macros = macroList(refusedRequest); SnapshotInclude include(refusedRequest);
        ID3DXEffectCompiler* compiler = nullptr; ID3DXBuffer* blob = nullptr;
        CHECK(D3DXCreateEffectCompiler(reinterpret_cast<const char*>(refusedRequest.source.data()), UINT(refusedRequest.source.size()),
            macros.data(), &include, refusedRequest.flags, &compiler, &errors) == D3D_OK && compiler && !errors);
        // Refusals remain failures through both compiler and effect creation APIs.
        CHECK(FAILED(compiler->CompileEffect(refusedRequest.flags, &blob, &errors)) && !blob && errors);
        // Native refusal reasons preserve failure without consulting an oracle.
        const char* why = static_cast<const char*>(errors->GetBufferPointer());
        CHECK(std::strstr(why, "BF2FX native:") && errors->Release() == 0);
        CHECK(compiler->Release() == 0);
        ID3DXEffect* none = nullptr; SnapshotInclude again(refusedRequest);
        CHECK(FAILED(create(d, refusedRequest, &none, again, &errors)) && !none && errors && errors->Release() == 0);
        CHECK(requestLines(cache) == before);
    } else std::fprintf(stderr, "note: no refusal record in %s; refusal checks skipped\n", cache);
    // A reflection-only record (RaShader*.mfx): MetaFx's string arrays, element strings and
    // literal annotations, then CompileEffect fails as D3DX's "no techniques".
    bf2fx::Request metaRequest;
    if (request(cache, "BundledMesh MetaFx", bf2fx::ReflectionOnlyEntry, metaRequest)) {
        const std::size_t before = requestLines(cache);
        auto macros = macroList(metaRequest);
        ID3DXEffectCompiler* compiler = nullptr; ID3DXBuffer* blob = nullptr;
        CHECK(D3DXCreateEffectCompiler(reinterpret_cast<const char*>(metaRequest.source.data()), UINT(metaRequest.source.size()),
            macros.data(), nullptr, metaRequest.flags, &compiler, &errors) == D3D_OK && compiler && !errors);
        D3DXHANDLE perms = compiler->GetParameterByName(nullptr, "GenerateShaderPermutations");
        D3DXPARAMETER_DESC pd = {}; LPCSTR text = nullptr;
        CHECK(perms && compiler->GetParameterDesc(perms, &pd) == D3D_OK && pd.Type == D3DXPT_STRING && pd.Elements == 32);
        CHECK(compiler->GetString(compiler->GetParameterElement(perms, 0), &text) == D3D_OK && !std::strcmp(text, "_nothing_"));
        CHECK(compiler->GetString(compiler->GetParameterElement(perms, 31), &text) == D3D_OK &&
              !std::strcmp(text, "_hasuvanimation_|_hasnormalmap_|_hasgimap_|_hasenvmap_|_usehemimap_"));
        D3DXHANDLE literal = compiler->GetParameterByName(nullptr, "_hasnormalmap_");
        D3DXHANDLE required = compiler->GetAnnotationByName(literal, "reqVertexElement");
        CHECK(literal && required && compiler->GetParameterDesc(required, &pd) == D3D_OK && pd.Elements >= 1);
        CHECK(compiler->GetString(compiler->GetParameterElement(required, 0), &text) == D3D_OK && text && *text);
        CHECK(!compiler->GetParameterElement(required, pd.Elements));
        CHECK(compiler->CompileEffect(metaRequest.flags, &blob, &errors) == E_FAIL && !blob && errors);
        CHECK(std::strstr(static_cast<const char*>(errors->GetBufferPointer()), "no techniques") && errors->Release() == 0);
        CHECK(compiler->Release() == 0 && requestLines(cache) == before);
    } else std::fprintf(stderr, "note: no .mfx reflection record in %s; reflection checks skipped\n", cache);

    // Creation through the cache key: source, sorted defines, flags and every include callback.
    SnapshotInclude swiffInclude(swiffRequest), fontInclude(fontRequest);
    CHECK(create(d, swiffRequest, &swiff, swiffInclude, &errors) == D3D_OK && swiff && !errors);
    CHECK(swiffInclude.opens > 0 && swiffInclude.opens == swiffInclude.closes);
    CHECK(create(d, fontRequest, &font, fontInclude, &errors) == D3D_OK && font);

    // A changed, valid source compiles natively rather than requiring an offline cache fill.
    {
        bf2fx::Request changed = swiffRequest; changed.source.insert(changed.source.begin(), ' ');
        SnapshotInclude include(changed); ID3DXEffect* compiled = nullptr;
        CHECK(create(d, changed, &compiled, include, &errors) == D3D_OK && compiled && !errors);
        CHECK(compiled->GetTechniqueByName("ShapeTextureClamp") && compiled->Release() == 0);
    }
    // Compiler path: introspection, then CompileEffect's tagged request recreates the effect.
    {
        ID3DXEffectCompiler* compiler = nullptr; ID3DXBuffer* blob = nullptr;
        std::vector<D3DXMACRO> macros;
        for (const auto& m : swiffRequest.defines) macros.push_back({m.name.c_str(), m.value.c_str()});
        macros.push_back({nullptr, nullptr});
        SnapshotInclude include(swiffRequest);
        CHECK(D3DXCreateEffectCompiler(reinterpret_cast<const char*>(swiffRequest.source.data()), UINT(swiffRequest.source.size()),
            macros.data(), &include, swiffRequest.flags, &compiler, &errors) == D3D_OK && compiler);
        D3DXPARAMETER_DESC pd = {};
        CHECK(compiler->GetParameterDesc(compiler->GetParameterByName(nullptr, "TexGenS"), &pd) == D3D_OK && pd.Class == D3DXPC_VECTOR && pd.Columns == 4);
        CHECK(compiler->CompileEffect(swiffRequest.flags, &blob, &errors) == D3D_OK && blob && blob->GetBufferSize() > 16);
        ID3DXEffect* again = nullptr;
        CHECK(D3DXCreateEffect(d, blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, nullptr, swiffRequest.flags | 0x800, nullptr, &again, &errors) == D3D_OK && again);
        CHECK(again->GetTechniqueByName("ShapeTextureClamp"));
        CHECK(again->Release() == 0 && blob->Release() == 0 && compiler->Release() == 0);
    }

    // Introspection the engine relies on.
    D3DXHANDLE worldView = swiff->GetParameterByName(nullptr, "WorldView");
    D3DXPARAMETER_DESC desc = {};
    CHECK(worldView && swiff->GetParameterDesc(worldView, &desc) == D3D_OK && desc.Class == D3DXPC_MATRIX_ROWS &&
          desc.Rows == 4 && desc.Columns == 4 && desc.Semantic && !std::strcmp(desc.Semantic, "TRANSFORM"));
    CHECK(swiff->GetParameterBySemantic(nullptr, "transform") == worldView);
    CHECK(swiff->GetParameterDesc(swiff->GetParameter(nullptr, 0), &desc) == D3D_OK && !swiff->GetParameterByName(nullptr, "Missing"));
    D3DXHANDLE texMap = swiff->GetParameterByName(nullptr, "TexMap");
    CHECK(texMap && swiff->GetParameterDesc(texMap, &desc) == D3D_OK && desc.Type == D3DXPT_TEXTURE && desc.Class == D3DXPC_OBJECT);
    D3DXHANDLE clamp = swiff->GetTechniqueByName("ShapeTextureClamp"), first = nullptr;
    D3DXTECHNIQUE_DESC td = {};
    CHECK(clamp && swiff->GetTechniqueDesc(clamp, &td) == D3D_OK && td.Passes == 1 && !std::strcmp(td.Name, "ShapeTextureClamp"));
    CHECK(swiff->FindNextValidTechnique(nullptr, &first) == D3D_OK && first == swiff->GetTechnique(0));
    CHECK(swiff->ValidateTechnique(clamp) == D3D_OK && swiff->ValidateTechnique("ShapeTextureClamp") == D3D_OK);
    CHECK(swiff->IsParameterUsed(texMap, clamp) && swiff->IsParameterUsed("TexGenS", clamp) && !swiff->IsParameterUsed(texMap, "Shape"));
    D3DXPASS_DESC passDesc = {};
    CHECK(swiff->GetPassDesc(swiff->GetPass(clamp, 0), &passDesc) == D3D_OK && passDesc.pVertexShaderFunction && passDesc.pPixelShaderFunction);

    // Scene: 64x64 target, no depth, clear blue.
    IDirect3DSurface9 *back = nullptr, *target = nullptr, *depth = nullptr;
    CHECK(d->GetRenderTarget(0, &back) == D3D_OK && d->GetDepthStencilSurface(&depth) == D3D_OK);
    CHECK(d->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &target, nullptr) == D3D_OK);
    CHECK(d->SetRenderTarget(0, target) == D3D_OK && d->SetDepthStencilSurface(nullptr) == D3D_OK);
    const D3DVERTEXELEMENT9 xyz[] = {{0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0}, D3DDECL_END()};
    IDirect3DVertexDeclaration9* declaration = nullptr;
    CHECK(d->CreateVertexDeclaration(xyz, &declaration) == D3D_OK);
    // Quad over the whole target in pixel units, z = Selector = 1 (textured).
    const float quad[] = {0, 0, 1, 64, 0, 1, 0, 64, 1, 64, 64, 1};
    IDirect3DVertexBuffer9* vb = nullptr; void* bytes = nullptr;
    CHECK(d->CreateVertexBuffer(sizeof(quad), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vb, nullptr) == D3D_OK);
    CHECK(vb->Lock(0, 0, &bytes, 0) == D3D_OK); std::memcpy(bytes, quad, sizeof(quad)); CHECK(vb->Unlock() == D3D_OK);
    // Row-vector pixel->clip transform: x/32 - 1, 1 - y/32. Exercises D3DX row-major -> CTAB packing.
    D3DXMATRIX m = {}; m._11 = 1 / 32.0f; m._22 = -1 / 32.0f; m._33 = 1; m._41 = -1; m._42 = 1; m._44 = 1;
    const D3DXVECTOR4 white(1, 1, 1, 1), s(1 / 64.0f, 0, 0, 0), t(0, 1 / 64.0f, 0, 0);
    CHECK(swiff->SetMatrix(worldView, &m) == D3D_OK && swiff->SetVector("DiffuseColor", &white) == D3D_OK);
    CHECK(swiff->SetVector("TexGenS", &s) == D3D_OK && swiff->SetVector("TexGenT", &t) == D3D_OK);
    const std::uint32_t quadrants[] = {0xffff0000u, 0x0000ff00u, 0xff0000ffu, 0xffffffffu}; // red, green (alpha 0), blue, white
    IDirect3DTexture9* corners = texture(d, 2, 2, quadrants);
    std::uint32_t stripes[64]; for (unsigned i = 0; i < 64; ++i) stripes[i] = i & 1 ? 0xffffffffu : 0xff000000u;
    IDirect3DTexture9* stripe = texture(d, 64, 1, stripes);
    CHECK(corners && stripe && swiff->SetTexture(texMap, corners) == D3D_OK);
    RecordingManager sm(d);
    CHECK(swiff->SetStateManager(&sm) == D3D_OK && sm.refs == 2);
    std::vector<std::uint8_t> px;

    // 1. Textured quad through the state manager, D3D9 orientation and filtering.
    CHECK(swiff->SetTechnique(clamp) == D3D_OK && swiff->GetCurrentTechnique() == clamp);
    UINT passes = 0;
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff0000ffu, 1, 0) == D3D_OK);
    CHECK(d->SetVertexDeclaration(declaration) == D3D_OK && d->SetStreamSource(0, vb, 0, 12) == D3D_OK);
    CHECK(swiff->Begin(&passes, 7) == D3D_OK && passes == 1 && swiff->BeginPass(0) == D3D_OK);
    bool clampU = false;
    for (const auto& r : sm.sampler) clampU = clampU || (r.first == 1 && r.second == 3);
    CHECK(clampU && sm.shaders == 2 && !sm.vsConstants.empty() && sm.textures.size() == 1 && sm.textures[0] == 0);
    CHECK(d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2) == D3D_OK);
    sm.clear();
    CHECK(swiff->CommitChanges() == D3D_OK && sm.render.empty() && sm.vsConstants.empty());   // nothing changed
    CHECK(swiff->SetTexture(texMap, stripe) == D3D_OK && swiff->CommitChanges() == D3D_OK);
    CHECK(sm.render.empty() && sm.shaders == 0 && !sm.vsConstants.empty() && sm.textures.size() == 1); // constants + texture only
    CHECK(swiff->EndPass() == D3D_OK && swiff->End() == D3D_OK && d->EndScene() == D3D_OK);
    // 2. Stripes after the commit are not drawn; read the quadrant draw.
    CHECK(readback(d, target, px));
    CHECK(near(px, 4, 4, 255, 0, 0, 255) && near(px, 59, 4, 0, 255, 0, 0) && near(px, 4, 59, 0, 0, 255, 255) && near(px, 59, 59, 255, 255, 255, 255));
    CHECK(near(px, 32, 32, 128, 128, 128, 191, 3)); // bilinear centre of the 2x2 texture

    // 3. D3D9 pixel centres: u = x/64 lands on texel boundaries, so linear stripes average to grey.
    CHECK(d->BeginScene() == D3D_OK && swiff->Begin(&passes, 7) == D3D_OK && swiff->BeginPass(0) == D3D_OK);
    CHECK(d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2) == D3D_OK);
    CHECK(swiff->EndPass() == D3D_OK && swiff->End() == D3D_OK && d->EndScene() == D3D_OK && readback(d, target, px));
    for (unsigned x = 1; x < 63; x += 7) CHECK(near(px, x, 30, 128, 128, 128, 255, 3));

    // 4. Alpha test after the pass: GREATER 0x80 discards the alpha-0 green quadrant.
    CHECK(swiff->SetTexture(texMap, corners) == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff0000ffu, 1, 0) == D3D_OK);
    CHECK(swiff->Begin(&passes, 7) == D3D_OK && swiff->BeginPass(0) == D3D_OK);
    CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(15), TRUE) == D3D_OK &&
          d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(25), D3DCMP_GREATER) == D3D_OK &&
          d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(24), 0x80) == D3D_OK);
    CHECK(d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2) == D3D_OK);
    CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(15), FALSE) == D3D_OK);
    CHECK(swiff->EndPass() == D3D_OK && swiff->End() == D3D_OK && d->EndScene() == D3D_OK && readback(d, target, px));
    CHECK(near(px, 4, 4, 255, 0, 0, 255) && near(px, 59, 4, 0, 0, 255, 255) && near(px, 59, 59, 255, 255, 255, 255));
    CHECK(swiff->SetStateManager(nullptr) == D3D_OK && sm.refs == 1);

    // 4a. DISCARD renames a dynamic buffer without waiting (c-join-stall): a draw encoded
    // before the discard keeps its generation. Left half, discard, right half, one scene:
    // both halves must show the quadrant texture (red top-left, green top-right).
    {
        const float left[] = {0, 0, 1, 32, 0, 1, 0, 64, 1, 32, 64, 1};
        const float right[] = {32, 0, 1, 64, 0, 1, 32, 64, 1, 64, 64, 1};
        IDirect3DVertexBuffer9* dyn = nullptr; void* w = nullptr;
        CHECK(d->CreateVertexBuffer(sizeof(left), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &dyn, nullptr) == D3D_OK);
        CHECK(dyn->Lock(0, 0, &w, D3DLOCK_DISCARD) == D3D_OK); std::memcpy(w, left, sizeof(left)); CHECK(dyn->Unlock() == D3D_OK);
        CHECK(d->BeginScene() == D3D_OK && d->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff0000ffu, 1, 0) == D3D_OK);
        CHECK(d->SetStreamSource(0, dyn, 0, 12) == D3D_OK);
        CHECK(swiff->Begin(&passes, 7) == D3D_OK && swiff->BeginPass(0) == D3D_OK);
        CHECK(d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2) == D3D_OK);
        CHECK(dyn->Lock(0, 0, &w, D3DLOCK_DISCARD) == D3D_OK); std::memcpy(w, right, sizeof(right)); CHECK(dyn->Unlock() == D3D_OK);
        CHECK(d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2) == D3D_OK);
        CHECK(swiff->EndPass() == D3D_OK && swiff->End() == D3D_OK && d->EndScene() == D3D_OK && readback(d, target, px));
        CHECK(near(px, 4, 4, 255, 0, 0, 255) && near(px, 59, 4, 0, 255, 0, 0));
        CHECK(d->SetStreamSource(0, vb, 0, 12) == D3D_OK && dyn->Release() == 0);
        std::puts("PASS: dynamic DISCARD keeps the in-flight generation");
    }

    // 4b. Windows sampling of R32F is (r,1,1,1): a 0.5 texel draws (128,255,255,255).
    IDirect3DTexture9* single = nullptr; D3DLOCKED_RECT fl = {};
    CHECK(d->CreateTexture(2, 2, 1, 0, D3DFMT_R32F, D3DPOOL_MANAGED, &single, nullptr) == D3D_OK);
    CHECK(single->LockRect(0, &fl, nullptr, 0) == D3D_OK);
    for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 2; ++x) reinterpret_cast<float*>(static_cast<std::uint8_t*>(fl.pBits) + y * fl.Pitch)[x] = 0.5f;
    CHECK(single->UnlockRect(0) == D3D_OK && swiff->SetTexture(texMap, single) == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK && swiff->Begin(&passes, 7) == D3D_OK && swiff->BeginPass(0) == D3D_OK);
    CHECK(d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2) == D3D_OK);
    CHECK(swiff->EndPass() == D3D_OK && swiff->End() == D3D_OK && d->EndScene() == D3D_OK && readback(d, target, px));
    CHECK(near(px, 20, 20, 128, 255, 255, 255));

    // 4c. A viewport past the target keeps D3D's transform: 128 wide on a 64-pixel target maps
    // the quad's clip x [-1,1] onto window [0,128), so only its left half lands on pixels 0..63.
    // D3D pixel centre x samples u = x/128, texel coordinate x/2 - 0.5: odd x lands exactly on a
    // texel (texel (x-1)/2), even x halfway between two (grey).
    CHECK(swiff->SetTexture(texMap, stripe) == D3D_OK);
    D3DVIEWPORT9 wide = {};
    CHECK(d->GetViewport(&wide) == D3D_OK);
    D3DVIEWPORT9 outside = wide; outside.Width = 128;
    // Clear first: Clear with a partial or oversized viewport is G3b's own named frontier.
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff0000ffu, 1, 0) == D3D_OK);
    CHECK(d->SetViewport(&outside) == D3D_OK);
    CHECK(swiff->Begin(&passes, 7) == D3D_OK && swiff->BeginPass(0) == D3D_OK);
    CHECK(d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2) == D3D_OK);
    CHECK(swiff->EndPass() == D3D_OK && swiff->End() == D3D_OK && d->EndScene() == D3D_OK && readback(d, target, px));
    CHECK(d->SetViewport(&wide) == D3D_OK);
    CHECK(near(px, 2, 30, 128, 128, 128, 255, 3) && near(px, 3, 30, 255, 255, 255, 255, 3) &&
          near(px, 5, 30, 0, 0, 0, 255, 3) && near(px, 62, 30, 128, 128, 128, 255, 3));

    // 5. Font "Regular": DrawPrimitiveUP, D3DCOLOR, alpha blending SRCALPHA/INVSRCALPHA, no state manager.
    struct FontVertex { float x, y, z, w; std::uint32_t color; float u, v; };
    const FontVertex glyph[] = {{-1, 1, 0.5f, 1, 0x80ff0000u, 0, 0}, {1, 1, 0.5f, 1, 0x80ff0000u, 0, 0},
                                {-1, -1, 0.5f, 1, 0x80ff0000u, 0, 0}, {1, -1, 0.5f, 1, 0x80ff0000u, 0, 0}};
    const D3DVERTEXELEMENT9 fontElements[] = {{0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0},
        {0, 16, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0}, {0, 20, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0}, D3DDECL_END()};
    IDirect3DVertexDeclaration9* fontDecl = nullptr;
    const std::uint32_t opaqueWhite[] = {0xffffffffu};
    IDirect3DTexture9* whiteTexture = texture(d, 1, 1, opaqueWhite);
    CHECK(whiteTexture && d->CreateVertexDeclaration(fontElements, &fontDecl) == D3D_OK);
    CHECK(font->SetTexture("TexMap", whiteTexture) == D3D_OK && font->SetTechnique("Regular") == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff0000ffu, 1, 0) == D3D_OK);
    CHECK(d->SetVertexDeclaration(fontDecl) == D3D_OK && font->Begin(&passes, 7) == D3D_OK && font->BeginPass(0) == D3D_OK);
    CHECK(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, glyph, sizeof(FontVertex)) == D3D_OK);
    IDirect3DVertexBuffer9* cleared = vb; UINT offset = 9, stride = 9;
    CHECK(d->GetStreamSource(0, &cleared, &offset, &stride) == D3D_OK && !cleared && !offset && !stride); // UP resets stream 0
    CHECK(font->EndPass() == D3D_OK && font->End() == D3D_OK && d->EndScene() == D3D_OK && readback(d, target, px));
    CHECK(near(px, 10, 10, 128, 0, 127, -1) && near(px, 50, 40, 128, 0, 127, -1));

    // 6. Font "SelectionQuad": PixelShader = NULL with SelectArg1(Diffuse) texture stages, a fan.
    const D3DXVECTOR4 green(0, 1, 0, 1);
    D3DXMATRIX identity = {}; identity._11 = identity._22 = identity._33 = identity._44 = 1;
    const float fan[] = {-1, -1, 0, -1, 1, 0, 1, 1, 0, 1, -1, 0};
    CHECK(font->SetMatrix("WorldView", &identity) == D3D_OK && font->SetVector("DiffuseColor", &green) == D3D_OK);
    CHECK(font->SetTechnique("SelectionQuad") == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff0000ffu, 1, 0) == D3D_OK);
    CHECK(d->SetVertexDeclaration(declaration) == D3D_OK && font->Begin(&passes, 7) == D3D_OK && font->BeginPass(0) == D3D_OK);
    CHECK(d->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, fan, 12) == D3D_OK);
    CHECK(font->EndPass() == D3D_OK && font->End() == D3D_OK && d->EndScene() == D3D_OK && readback(d, target, px));
    CHECK(near(px, 1, 1, 0, 255, 0, -1) && near(px, 62, 62, 0, 255, 0, -1) && near(px, 32, 20, 0, 255, 0, -1));

    // Lifetime: textures held by effects are released with them; device references return.
    CHECK(d->SetTexture(0, nullptr) == D3D_OK && d->SetStreamSource(0, nullptr, 0, 0) == D3D_OK &&
          d->SetVertexDeclaration(nullptr) == D3D_OK && d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK);
    CHECK(swiff->Release() == 0 && font->Release() == 0);
    CHECK(corners->Release() == 0 && stripe->Release() == 0 && whiteTexture->Release() == 0 && single->Release() == 0);
    CHECK(vb->Release() == 0 && declaration->Release() == 0 && fontDecl->Release() == 0);
    CHECK(d->SetRenderTarget(0, back) == D3D_OK && d->SetDepthStencilSurface(depth) == D3D_OK);
    CHECK(target->Release() == 0);
    back->Release(); depth->Release();
    // Struct parameters (Ra's Light Lights[1]): D3DX9_25 handles, RaShader's 68-byte SetValue,
    // and BeginPass/CommitChanges uploading one register per member through the state manager.
    {
        bf2fx::Request lr; bf2fx::Entry le; std::uint32_t lt = 0, lp = 0; bf2fx::Binding lb;
        CHECK(lightsEffect(cache, lr, le, lt, lp, lb));
        SnapshotInclude include(lr); ID3DXEffect* ra = nullptr;
        CHECK(create(d, lr, &ra, include, &errors) == D3D_OK && ra);
        RecordingManager rsm(d);
        CHECK(ra->SetStateManager(&rsm) == D3D_OK);
        D3DXHANDLE lights = ra->GetParameterByName(nullptr, "Lights"), element = ra->GetParameterElement(lights, 0);
        D3DXPARAMETER_DESC ld = {};
        CHECK(lights && element && ra->GetParameterDesc(lights, &ld) == D3D_OK && ld.Class == D3DXPC_STRUCT && ld.Bytes == 60 &&
              ld.StructMembers == 5 && !ld.Semantic);
        CHECK(ra->GetParameterDesc(ra->GetParameter(element, 2), &ld) == D3D_OK && !std::strcmp(ld.Name, "color") && ld.Columns == 4 && !ld.Semantic);
        CHECK(ra->GetParameterByName(element, "dir") == ra->GetParameterByName(nullptr, "Lights[0].dir"));
        CHECK(!ra->GetParameter(lights, 0) && !ra->GetParameterByName(nullptr, "Lights.pos"));
        float record[17]; for (int i = 0; i < 17; ++i) record[i] = float(i + 1);
        CHECK(ra->SetValue(lights, record, 56) == D3DERR_INVALIDCALL && ra->SetValue(lights, record, 68) == D3D_OK);
        D3DXVECTOR4 color(0, 0, 0, 0);
        CHECK(ra->GetVector("Lights[0].color", &color) == D3D_OK && color.x == 7 && color.w == 10);
        CHECK(ra->SetTechnique(ra->GetTechnique(lt)) == D3D_OK && d->BeginScene() == D3D_OK);
        UINT lpasses = 0; float regs[32] = {};
        CHECK(ra->Begin(&lpasses, 7) == D3D_OK && ra->BeginPass(lp) == D3D_OK);
        CHECK(d->GetVertexShaderConstantF(lb.index, regs, 3) == D3D_OK);
        CHECK(regs[0] == 1 && regs[2] == 3 && regs[4] == 4 && regs[6] == 6 && regs[8] == 7 && regs[11] == 10);
        record[6] = 70; CHECK(ra->SetValue(lights, record, 68) == D3D_OK && ra->CommitChanges() == D3D_OK);
        CHECK(d->GetVertexShaderConstantF(lb.index, regs, 3) == D3D_OK && regs[8] == 70);
        CHECK(ra->EndPass() == D3D_OK && ra->End() == D3D_OK && d->EndScene() == D3D_OK);
        CHECK(ra->SetStateManager(nullptr) == D3D_OK && ra->Release() == 0);
        CHECK(d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK);
        for (DWORD stage = 0; stage < 16; ++stage) d->SetTexture(stage, nullptr);
        std::printf("G4 struct: %s technique %u pass %u, Lights at c%u x%u\n", le.key.substr(0, 12).c_str(), lt, lp, lb.index, lb.count);
    }
    // Matrix members (RaShaderBM's UserData { float4x4 uvMatrix[7]; }): RaShader's SetValue of the
    // packed row-major matrices, then column registers after BeginPass and after CommitChanges.
    {
        bf2fx::Request mr; bf2fx::Entry me; std::uint32_t mt = 0, mp = 0; bf2fx::Binding mb;
        if (lightsEffect(cache, mr, me, mt, mp, mb, "UserData", 8)) {
            SnapshotInclude include(mr); ID3DXEffect* bm = nullptr;
            CHECK(create(d, mr, &bm, include, &errors) == D3D_OK && bm);
            D3DXHANDLE userData = bm->GetParameterByName(nullptr, "UserData"), uv = bm->GetParameter(userData, 0);
            D3DXHANDLE uv1 = bm->GetParameterElement(uv, 1);
            D3DXPARAMETER_DESC md = {};
            CHECK(uv && uv1 && uv1 == bm->GetParameterByName(nullptr, "UserData.uvMatrix[1]"));
            CHECK(bm->GetParameterDesc(uv, &md) == D3D_OK && md.Class == D3DXPC_MATRIX_ROWS && md.Rows == 4 && md.Columns == 4 && md.Elements == 7 && md.Bytes == 448);
            float mats[112]; for (int i = 0; i < 112; ++i) mats[i] = float(i + 1);
            CHECK(bm->SetValue(userData, mats, 440) == D3DERR_INVALIDCALL && bm->SetValue(userData, mats, 448) == D3D_OK);
            CHECK(bm->SetTechnique(bm->GetTechnique(mt)) == D3D_OK && d->BeginScene() == D3D_OK);
            UINT mpasses = 0; float regs[32] = {};
            CHECK(bm->Begin(&mpasses, 7) == D3D_OK && bm->BeginPass(mp) == D3D_OK);
            CHECK(d->GetVertexShaderConstantF(mb.index, regs, 8) == D3D_OK);
            CHECK(regs[0] == 1 && regs[1] == 5 && regs[2] == 9 && regs[3] == 13 && regs[4] == 2 && regs[16] == 17 && regs[17] == 21);
            D3DXMATRIX m = {}; for (int i = 0; i < 16; ++i) (&m._11)[i] = float(200 + i);
            CHECK(bm->SetMatrix(uv1, &m) == D3D_OK && bm->CommitChanges() == D3D_OK);
            CHECK(d->GetVertexShaderConstantF(mb.index + 4, regs, 4) == D3D_OK && regs[0] == 200 && regs[1] == 204 && regs[4] == 201);
            CHECK(bm->EndPass() == D3D_OK && bm->End() == D3D_OK && d->EndScene() == D3D_OK && bm->Release() == 0);
            CHECK(d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK);
            for (DWORD stage = 0; stage < 16; ++stage) d->SetTexture(stage, nullptr);
            std::printf("G4 struct matrix: %s technique %u pass %u, UserData at c%u x%u\n", me.key.substr(0, 12).c_str(), mt, mp, mb.index, mb.count);
        } else std::fprintf(stderr, "note: no UserData effect in %s; struct-matrix checks skipped\n", cache);
    }
    std::puts("PASS: G4 effects (SwiffMenu, Font): cache key/miss/compiler alias, state manager, commit, draws and pixels");
    return 0;
}
