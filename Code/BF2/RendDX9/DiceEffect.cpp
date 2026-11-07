// Path:     attested - "C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\RendDX9\DiceEffect.cpp",
//           quoted with lines 309 and 315 by PE 0x1001AD50.
// Contents: the out-of-line DiceEffect members: technique compilation with its binary cache, and
//           the define and substitution builders it runs first. The rest of DiceEffect is inline in
//           DiceEffect.h. Every body here is unreachable in 1.50: compile has no code or data
//           references, and it is the only caller of updateLiteralSubstitutions and buildDefines.

// See Code/BF2/RendDX9/BF2Render.cpp for the BF2_IMAGE_RENDDX9 guard and why it is here.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "RendDX9Base.h"
#include "DiceEffect.h"
#include "../GeomMisc/Debug.h"
#include "../Core/StringUtil.h"

#include <map>
#include <stdio.h>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

namespace dice {
namespace hfe {
namespace dx9rend {

extern void* g_rendererBase;   // RendDX9Init.cpp, dword_1023B970 - its +0x0C device
extern void* g_stateManager;   // StateManager.cpp, dword_1023B984

// ShaderManager.cpp.
struct ShaderManager;
bool shaderManagerGetFlag7E(const ShaderManager* sm);   // PE 0x100225D0, the byte at +0x7E
const void* shaderManagerDefineTable();                // the D3DXMACRO table at 0x101DEBD0

namespace {

// Byte 0x10238090: initialised to 1 in .data, read only by compile (PE 0x1001B2FF), never written.
// [INFERRED] name.
bool g_writeDiceEffectCache = true;

const char kDiceEffectFile[] =
    "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\DiceEffect.cpp";

// The three MB_RETRYCANCEL (0x50015) failure dialogs of compile; IDRETRY is 4.
// Every call passes a null owner window.
int diceEffectRetryDialog(const char* text, const char* caption)
{
#if defined(_WIN32)
    return ::MessageBoxA(0, text, caption, 0x50015);
#else
    // [CHOSEN, not recovered] No Win32 modal UI: choose IDCANCEL (2), the policy of Effect.cpp's
    // effectRetryDialog. Cancel continues past the failure exactly as the shipped body does.
    std::fprintf(stderr, "dice::hfe::dx9rend::DiceEffect::compile: %s: %s (Win32 retry dialog "
                         "unavailable; Cancel)\n", caption, text);
    std::fflush(stderr);
    return 2;
#endif
}

} // anonymous namespace


// PE 0x1001A410 (listing read). Unreachable in 1.50: no code or data references.
// [CHOSEN, not recovered] name; the result is the node's value_type address (+0xC), which is also
// its key's address, and no caller in any image shows which of the two the source returned.
std::map<std::string, std::string>::value_type& DiceEffect::substitution(const std::string& key)
{
    return *substitutions.find(key);
}

// PE 0x1001A510 (listing read). Each literal (0x20-byte elements: name, value at +0x1C) sets
// substitutions[upper-cased copy of its name] to its value printed with "%u".
// Unreachable in 1.50: only the unreachable compile calls it. [INFERRED] name.
void DiceEffect::updateLiteralSubstitutions()
{
    for (std::vector<Literal>::const_iterator literal = literals.begin();
         literal != literals.end(); ++literal)
    {
        const std::string key = Ra::vc71UpperCopy(literal->name);
        substitutions[key] = formatString("%u", literal->value);
    }
}

// PE 0x1001AC30 (listing read). Unreachable in 1.50: only the unreachable compile calls it.
// [INFERRED] name. resize(0) (0x1001D650), assign from ShaderManager +0x88 (erase 0x1001D180, insert
// 0x1001D1F0), pop_back (0x1001D5C0), then each substitution's key and value as new strings.
// shipped defect: the shader manager's list alternates name, value and ends with a value, so the
// pop_back leaves its last name unpaired and every appended substitution shifts by one position.
void DiceEffect::buildDefines()
{
    defines.resize(0);
    const ShaderManagerDefines global = shaderManagerDefines(g_shaderManager);
    defines.assign(global.first, global.first + global.count);
    if (!defines.empty())   // VC7.1 pop_back's own empty test (0x1001D5C7)
        defines.pop_back();
    for (std::map<std::string, std::string>::const_iterator node = substitutions.begin();
         node != substitutions.end(); ++node)
    {
        defines.push_back(std::string(node->first.c_str()));
        defines.push_back(std::string(node->second.c_str()));
    }
}

// PE 0x1001AD50 (listing read). Unreachable in 1.50: no code or data references. [INFERRED] name.
// With ShaderManager +0x7E set, "<name>_<technique>_dfx.bin" is accepted when its source time and
// adapter identifier match; otherwise the source is compiled with the per-effect defines, the
// literals are applied, and the binary is written back while byte 0x10238090 is set. The effect is
// then created in this effect's pool with the static define table, the valid technique whose name
// after its first character prints as `technique` is selected, and the pair is inserted at +0x9C.
// Retry on any failure dialog returns false; Cancel continues with the shipped unchecked state.
bool DiceEffect::compile(unsigned int technique)
{
    const std::string techniqueLabel = formatString(" techName: %u", technique);
    const std::string cacheName = formatString("%s_%u_dfx.bin", name.c_str(), technique);
    updateLiteralSubstitutions();
    buildDefines();

    ID3DXBuffer* compiled = 0;
    bool cached = false;
    if (shaderManagerGetFlag7E(static_cast<const ShaderManager*>(g_shaderManager)))
    {
        io::IStream* const stream = ::dice::hfe::fileManager->open(
            cacheName, static_cast<io::FileMode>(2), static_cast<io::FileAccess>(0), 0);
        if (stream)
        {
            cached = true;
            unsigned int size = 0;
            unsigned int cachedTime[2] = { 0, 0 };
            stream->read(cachedTime, 8);
            GUID cachedDevice;
            stream->read(&cachedDevice, 16);
            if (cachedTime[0] == fileTime[0] && cachedTime[1] == fileTime[1])
            {
                const GUID device = deviceCapsDeviceIdentifier();
                if (std::memcmp(&cachedDevice, &device, sizeof(GUID)) == 0)
                {
                    stream->read(&size, 4);
                    if (D3DXCreateBuffer(size, &compiled))
                    {
                        {
                            ::renddx9abi::Debug diagnostic(DebugTypeError, kDiceEffectFile, 309, "");
                            diagnostic << "Failed to load diceeffect: " << name << "\r\n"
                                       << "Failed to create buffer when reading cached data";
                        }
                        cached = false;
                    }
                    // shipped defect: the read goes ahead after a failed D3DXCreateBuffer, through
                    // the still-null buffer pointer.
                    if (stream->read(compiled->GetBufferPointer(), size) != size)
                    {
                        {
                            ::renddx9abi::Debug diagnostic(DebugTypeError, kDiceEffectFile, 315, "");
                            diagnostic << "Failed to load diceeffect: " << name << "\r\n"
                                       << "Invalid size of cached data";
                        }
                        // shipped defect: the released buffer stays in `compiled` until
                        // CompileEffect below overwrites it.
                        compiled->Release();
                        cached = false;
                    }
                }
                else
                {
                    cached = false;
                }
            }
            else
            {
                cached = false;
            }
            stream->release();
        }
    }

    ID3DXEffectCompiler* compiler = 0;
    ID3DXBuffer* errors = 0;
    if (!cached)
    {
        std::vector<const char*> macros;
        for (unsigned int i = 0; i < defines.size(); ++i)
            macros.push_back(defines[i].c_str());
        macros.push_back(0);
        // shipped defect: the list is read as D3DXMACRO pairs. After buildDefines' pop_back it
        // normally holds an odd number of strings, so the terminator becomes the last pair's
        // Definition and D3DX reads the next Name past the end of the list.
        if (D3DXCreateEffectCompiler(source, sourceSize,
                                     reinterpret_cast<const D3DXMACRO*>(&macros[0]), 0,
                                     shaderManagerEffectFlags(g_shaderManager), &compiler, &errors))
        {
            bool retry;
            {
                const std::string effectFile = extractFileName(name);
                const std::string message = "Failed to load Effect: " + effectFile;
                const std::string caption = message + techniqueLabel;
                retry = diceEffectRetryDialog(static_cast<const char*>(errors->GetBufferPointer()),
                                              caption.c_str()) == 4;
            }
            errors->Release();
            if (retry)
                return false;
            // shipped defect: Cancel continues with the compiler the failed call left behind.
        }

        applyLiterals(compiler);
        if (compiler->CompileEffect(shaderManagerEffectFlags(g_shaderManager), &compiled, &errors))
        {
            bool retry;
            {
                const std::string effectFile = extractFileName(name);
                const std::string message = "Failed to load Effect: " + effectFile;
                const std::string caption = message + techniqueLabel;
                retry = diceEffectRetryDialog(static_cast<const char*>(errors->GetBufferPointer()),
                                              caption.c_str()) == 4;
            }
            errors->Release();
            // shipped defect: the compiler is released here and again before the final return.
            compiler->Release();
            if (retry)
                return false;
        }

        if (g_writeDiceEffectCache)
        {
            io::IStream* const out = ::dice::hfe::fileManager->open(
                cacheName, static_cast<io::FileMode>(0), static_cast<io::FileAccess>(2), 0);
            unsigned int size = compiled->GetBufferSize();
            out->write(fileTime, 8);
            const GUID device = deviceCapsDeviceIdentifier();
            out->write(&device, 16);
            out->write(&size, 4);
            out->write(compiled->GetBufferPointer(), size);
            out->release();
        }
    }

    ID3DXEffect* effect = 0;
    IDirect3DDevice9* const device = static_cast<RendDX9Base*>(g_rendererBase)->device;
    if (D3DXCreateEffect(device, compiled->GetBufferPointer(), compiled->GetBufferSize(),
                         static_cast<const D3DXMACRO*>(shaderManagerDefineTable()), 0,
                         shaderManagerEffectFlags(g_shaderManager),
                         static_cast<ID3DXEffectPool*>(pool), &effect, &errors))
    {
        bool retry;
        {
            const std::string effectFile = extractFileName(name);
            const std::string message = "Failed to load Effect: " + effectFile;
            const std::string caption = message + techniqueLabel;
            retry = diceEffectRetryDialog(static_cast<const char*>(errors->GetBufferPointer()),
                                          caption.c_str()) == 4;
        }
        errors->Release();
        // shipped defect: no null test; a cache hit never created a compiler.
        compiler->Release();
        compiled->Release();
        if (retry)
            return false;
    }

    // shipped defect: the search has no end test; a technique that never prints as `technique`
    // keeps it walking past the last valid one.
    D3DXHANDLE handle = 0;
    for (;;)
    {
        effect->FindNextValidTechnique(handle, &handle);
        D3DXTECHNIQUE_DESC desc;
        effect->GetTechniqueDesc(handle, &desc);
        if (formatString("%u", technique).compare(std::string(desc.Name + 1)) == 0)
            break;
    }
    effect->SetTechnique(handle);
    effect->SetStateManager(static_cast<ID3DXEffectStateManager*>(g_stateManager));
    effects.insert(std::map<unsigned int, std::pair<ID3DXEffect*, D3DXHANDLE> >::value_type(
        technique, std::pair<ID3DXEffect*, D3DXHANDLE>(effect, handle)));

    if (compiler)
        compiler->Release();
    if (compiled)
        compiled->Release();
    return true;
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
