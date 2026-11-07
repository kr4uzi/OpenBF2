// DiceEffect state and compiled-technique storage; header placement is [INFERRED].
#ifndef BF2_RENDDX9_DICEEFFECT_H
#define BF2_RENDDX9_DICEEFFECT_H

#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace dice { namespace hfe { namespace dx9rend {

void shaderManagerSetCurrentDiceEffect(void* effect);
void* shaderManagerInclude(void* shaderManager);
extern void* g_shaderManager;
namespace Ra {
// Ra/RaShaderPermutator.cpp: copy, then upper-case through 0x10074BE0 (five Latin-1 pairs, else CRT).
std::string vc71UpperCopy(const std::string& s);
}
#if !defined(_WIN32)
// [CHOSEN, not recovered] Keep the include type's literal bits: the native SDK
// subset omits D3DXINC_FORCE_DWORD, making 0x7FFFFFFF outside its enum's range.
void shaderManagerOpenDiceInclude(const char* name, const void* parent,
                                  const void** data, unsigned int* size);
#endif

// [CHOSEN, not recovered] PCRegExp.dll replacement for the three fixed "mg"
// patterns below. Keep PME's global cursor, LF multiline anchors and dot semantics
// (dot matches CR). The patterns themselves remain verbatim at their call sites.
class DiceEffectSourcePattern
{
public:
    explicit DiceEffectSourcePattern(const char* pattern) : begin(0), end(0), cursor(0)
    {
        std::string expression;
        for (const char* p = pattern + 1; *p; ++p) // all three patterns start with ^
        {
            if (*p == '.')
                expression += "[^\\n]";
            else
                expression += *p;
        }
        regex.assign(expression);
    }

    bool match(const std::string& text)
    {
        std::string::size_type at = cursor;
        while (at <= text.size())
        {
            if (at == 0 || text[at - 1] == '\n')
            {
                std::cmatch result;
                if (std::regex_search(text.c_str() + at, text.c_str() + text.size(), result,
                                      regex, std::regex_constants::match_continuous))
                {
                    begin = at;
                    end = at + result.length();
                    cursor = end;
                    for (unsigned int i = 0; i != 3; ++i)
                        captures[i] = i < result.size() ? result[i].str() : std::string();
                    return true;
                }
            }
            const auto newline = text.find('\n', at);
            if (newline == std::string::npos)
                break;
            at = newline + 1;
        }
        cursor = text.size() + 1;
        return false;
    }

    void reset() { cursor = 0; }
    const std::string& operator[](unsigned int group) const { return captures[group]; }

    std::string::size_type begin, end;

private:
    std::regex regex;
    std::string::size_type cursor;
    std::string captures[3];
};

struct DiceEffect;

// [CHOSEN, not recovered] Call adapter for the loader prefix read by PE 0x1001B5C0.
// Callback names are [INFERRED]; the loader's identity remains [UNRESOLVED]. Each call prepares this and one
// DiceEffect pointer; only +0x10's AL is read. The first two results are discarded.
// +0x08/+0x0C are unread slot addresses, with no invented function declarations.
// This describes call contracts only; it neither supplies nor publishes a loader.
struct DiceEffectLoaderCalls
{
    void (__thiscall* beforeReload)(void*, DiceEffect*); // PE 0x1001B5C8..0x1001B5C9
    void (__thiscall* afterParse)(void*, DiceEffect*);   // PE 0x1001B5DE..0x1001B5DF
    void* unknown08;
    void* unknown0C;
    unsigned char (__thiscall* complete)(void*, DiceEffect*); // PE 0x1001B5E7..0x1001B5ED
};
struct DiceEffectLoaderFace { const DiceEffectLoaderCalls* calls; };

// [CHOSEN, not recovered] Typed call facade for PE table 0x101DE814; every slot
// has a proved body (0x10051870, 0x1001A010, 0x1001D710). No destructor slot.
class DiceEffectReferences
{
public:
    virtual int getRef() = 0;
    virtual int addRef() = 0;
    virtual int release() = 0;
};

// DiceEffect.cpp is attested by the diagnostics in PE 0x1001AD50. The private
// containers use native STL storage; offsets below document the retail x86 fields.
// Native construction through ShaderManager is gated at effect-pool support.
struct DiceEffect
{
    // PE 0x1001A880.
    DiceEffect(const std::string& stem, const std::string& suffix, IUnknown* effectPool)
        : vptr(const_cast<void**>(table())), refCount(1), shared(0)
        , name(stem), extension(suffix), loader(0), begun(0), passBegun(0)
        , selected(0), selectedKey(~0u), source(0), sourceSize(0)
    {
        pool = effectPool;
        pool->AddRef();
    }

    // PE 0x1001A5A0.
    ~DiceEffect()
    {
        vptr = const_cast<void**>(table());
        for (const auto& effect : effects)
            effect.second.first->Release();
        if (shared)
            shared->Release();
        pool->Release();
        // shipped defect: the source buffer at +0x5C is not freed.
    }

    void* vptr;                              // +0x00, table 0x101DE814
    unsigned char unknown04[4];              // +0x04, untouched by the inspected bodies
    int refCount;                            // +0x08
    IUnknown* shared;                        // +0x0C, optional counted COM object
    IUnknown* pool;                          // +0x10, ID3DXEffectPool's IUnknown face
    std::string name;                        // +0x14
    std::string extension;                   // +0x30
    void* loader;                            // +0x4C, borrowed; [UNRESOLVED] loader identity
    unsigned char begun;                     // +0x50
    unsigned char passBegun;                 // +0x51
    unsigned char pad52[2];
    ID3DXEffect** selected;                  // +0x54, pointer to a map value's effect word
    unsigned int selectedKey;                // +0x58, -1 means no technique
    char* source;                            // +0x5C
    unsigned int sourceSize;                 // +0x60
    unsigned char unknown64[4];              // +0x64, untouched by the inspected bodies
    unsigned int fileTime[2];                // +0x68
    std::map<std::string, std::string> substitutions; // +0x70
    struct Literal { std::string name; int value; };
    std::vector<Literal> literals;            // +0x7C
    std::vector<std::string> defines;         // +0x8C
    // +0x9C: technique key -> {effect, its selected technique}. The 0x1C-byte nodes (0x1001C900,
    // _Isnil at +0x19) carry both words; PE 0x1001AD50 inserts the pair.
    std::map<unsigned int, std::pair<ID3DXEffect*, D3DXHANDLE> > effects;

    // PE 0x10051870 / 0x1001A010, table 0x101DE814.
    static int RD9_THUNK getRefThunk(void* self RD9_THUNK_EDX)
    {
        return static_cast<DiceEffect*>(self)->refCount;
    }
    static int RD9_THUNK addRefThunk(void* self RD9_THUNK_EDX)
    {
        return ++static_cast<DiceEffect*>(self)->refCount;
    }
    // PE 0x1001D710 (listing read: decrement +0x08, destructor and free at zero).
    static int RD9_THUNK releaseThunk(void* self RD9_THUNK_EDX)
    {
        DiceEffect* const effect = static_cast<DiceEffect*>(self);
        const int remaining = --effect->refCount;
        if (!remaining)
        {
            effect->~DiceEffect();
            ::renddx9abi::engineFree(effect);
        }
        return remaining;
    }
    static void* const* table()
    {
        static void* const entries[3] =
        {
            reinterpret_cast<void*>(&getRefThunk),
            reinterpret_cast<void*>(&addRefThunk),
            reinterpret_cast<void*>(&releaseThunk)
        };
        return entries;
    }

    // PE 0x1001A0A0. A missing selection returns null; callers retain their own
    // shipped null-guard behavior, including the unchecked pre-draw commit.
    ID3DXEffect* currentEffect() const
    {
        return selectedKey != ~0u ? *selected : 0;
    }

    // PE 0x1001A230.
    unsigned int begin()
    {
        unsigned int passes = 0;
        (*selected)->Begin(&passes, 7);
        begun = 1;
        shaderManagerSetCurrentDiceEffect(this);
        return passes;
    }

    // PE 0x1001A270.
    void beginPass(unsigned int pass)
    {
        if (passBegun)
        {
            (*selected)->EndPass();
            passBegun = 0;
        }
        (*selected)->BeginPass(pass);
        passBegun = 1;
    }

    // PE 0x1001A2B0.
    void end()
    {
        if (passBegun)
        {
            (*selected)->EndPass();
            passBegun = 0;
        }
        (*selected)->End();
        begun = 0;
        shaderManagerSetCurrentDiceEffect(0);
    }

    // PE 0x1001A360 (listing read). Unreachable in 1.50: no code or data references. The first literal
    // whose name equals `wanted`, or literals.end(); the argument is copied first, as shipped.
    std::vector<Literal>::const_iterator findLiteral(const std::string& wanted) const
    {
        const std::string copy(wanted);
        std::vector<Literal>::const_iterator it = literals.begin();
        for (; it != literals.end(); ++it)
        {
            if (it->name == copy)
                break;
        }
        return it;
    }

    // PE 0x1001A3C0.
    bool selectTechnique(unsigned int key)
    {
        if (key != selectedKey)
        {
            const auto hit = effects.find(key);
            if (hit == effects.end())
                return false;
            selectedKey = key;
            selected = &hit->second.first;
        }
        return true;
    }

    // PE 0x1001A2F0.
    void applyLiterals(ID3DXEffectCompiler* compiler)
    {
        for (const auto& literal : literals)
        {
            D3DXHANDLE parameter = compiler->GetParameterByName(0, literal.name.c_str());
            compiler->SetLiteral(parameter, TRUE);
            compiler->SetInt(parameter, literal.value);
        }
    }

    // PE 0x1001A090. The loader is borrowed, with no reference-count operation.
    bool setLoader(void* value)
    {
        loader = value;
        return true;
    }

    // PE 0x1001A0B0. Expand matching includes through ShaderManager's include face.
    // Each splice preserves the terminal byte in sourceSize and restarts matching.
    void expandIncludes(DiceEffectSourcePattern& pattern, char*& text, unsigned int& textSize)
    {
        while (pattern.match(std::string(text)))
        {
            const void* included = 0;
            unsigned int includedSize = 0;
#if defined(_WIN32)
            static_cast<ID3DXInclude*>(shaderManagerInclude(g_shaderManager))->Open(
                static_cast<D3DXINCLUDE_TYPE>(0x7FFFFFFF), pattern[1].c_str(), text,
                &included, &includedSize);
#else
            shaderManagerOpenDiceInclude(pattern[1].c_str(), text, &included, &includedSize);
#endif

            const unsigned int first = static_cast<unsigned int>(pattern.begin);
            const unsigned int last = static_cast<unsigned int>(pattern.end);
            const unsigned int size = first - last + textSize + includedSize;
            char* const expanded = static_cast<char*>(::renddx9abi::engineAlloc(size));
            std::memcpy(expanded, text, first);
            std::memcpy(expanded + first, included, includedSize);
            std::memcpy(expanded + first + includedSize, text + last, textSize - last);
            ::renddx9abi::engineFree(text);
            text = expanded;
            textSize = size;
            static_cast<ID3DXInclude*>(shaderManagerInclude(g_shaderManager))->Close(included);
            pattern.reset();
        }
    }

    // PE 0x1001A730. The literal vector loses its storage (0x1001D610), not only
    // its elements.
    void parseLiterals(DiceEffectSourcePattern& pattern, const char* text)
    {
        std::vector<Literal>().swap(literals);
        while (pattern.match(std::string(text)))
        {
            {
                const Literal literal = { pattern[1], 0 };
                literals.push_back(literal);
            }
            const std::string key = Ra::vc71UpperCopy(pattern[1]); // 0x10074BE0 on the copy
            substitutions[key] = "0";
        }
    }

    // PE 0x1001A430. Shipped ordering clears the literal-generated substitutions.
    void parseDefines(DiceEffectSourcePattern& pattern, const char* text)
    {
        substitutions.clear();
        while (pattern.match(std::string(text)))
        {
            const std::string replacement(pattern[2]);
            const std::string key(pattern[1]);
            substitutions[key] = replacement;
        }
    }

    // PE 0x1001A960. Replaces compiled techniques and source, then expands includes
    // and scans literals followed by commented defines. File and read failures
    // retain the shipped unchecked behavior; the counted source includes its NUL.
    bool parseSource()
    {
        for (const auto& effect : effects)
            effect.second.first->Release();
        effects.clear();

        io::FileInfo info;
        io::IStream* const stream = ::dice::hfe::fileManager->open(
            std::string(name) + extension, static_cast<io::FileMode>(2),
            static_cast<io::FileAccess>(0), 0);
        if (::dice::hfe::fileManager->getFileInfo(std::string(name) + extension, info))
        {
            fileTime[0] = info.lastWriteTimeLow;
            fileTime[1] = info.lastWriteTimeHigh;
        }
        else
        {
            fileTime[0] = 0;
            fileTime[1] = 0;
        }
        if (source)
        {
            ::renddx9abi::engineFree(source);
            source = 0;
        }
        sourceSize = static_cast<io::INonlinearStream*>(stream)->getSize();
        source = static_cast<char*>(::renddx9abi::engineAlloc(sourceSize + 1));
        stream->read(source, sourceSize);
        stream->release();
        source[sourceSize] = 0;
        ++sourceSize;

        DiceEffectSourcePattern includes("^#include\\s+\"(.*.d?fx)\".*");
        expandIncludes(includes, source, sourceSize);
        DiceEffectSourcePattern literalPattern(
            "^const\\s+(?:(?:bool)|(?:int)|(?:dword))\\s+([a-zA-Z0-9_]+)\\s+:\\s+LITERAL.*");
        parseLiterals(literalPattern, source);
        DiceEffectSourcePattern definePattern("^//#define\\s+([a-zA-Z0-9_]+)\\s+([a-zA-Z0-9_]+).*");
        parseDefines(definePattern, source);
        return true;
    }

    // The bodies below are in DiceEffect.cpp; all four are unreachable in 1.50 (compile has no code or
    // data references, and it is the only caller of the middle two).

    // PE 0x1001A410. The substitution entry for `key`: map::find (0x1001BD40), then the node's
    // value_type (+0xC). A missing key yields end()'s storage, as shipped.
    std::map<std::string, std::string>::value_type& substitution(const std::string& key);

    // PE 0x1001A510. substitutions[upper-cased literal name] = its value formatted "%u".
    void updateLiteralSubstitutions();

    // PE 0x1001AC30. defines = ShaderManager's flat define list minus its last element, then each
    // substitution's name and value.
    void buildDefines();

    // PE 0x1001AD50. Builds, caches and selects the effect compiled for technique `technique`.
    // Returns false only when the user answers Retry to a failure dialog.
    bool compile(unsigned int technique);

    // PE 0x1001B5C0. Notify once before invalidating selection; repeat parse/notify
    // until the current loader reports completion. Callbacks may replace loader.
    bool reload()
    {
        DiceEffectLoaderFace* face = static_cast<DiceEffectLoaderFace*>(loader);
        face->calls->beforeReload(face, this);
        selectedKey = ~0u;
        for (;;)
        {
            parseSource();
            face = static_cast<DiceEffectLoaderFace*>(loader);
            face->calls->afterParse(face, this);
            face = static_cast<DiceEffectLoaderFace*>(loader);
            if (face->calls->complete(face, this))
                return true;
        }
    }
};

DiceEffect* shaderManagerGetDiceEffect(void* shaderManager, const std::string& name);

} } }
#endif
