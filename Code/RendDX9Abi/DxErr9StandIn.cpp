// [CHOSEN, not recovered] DxErr9 functions for RendDX9.dll and TextureAtlasBuilder.dll builds
// without the April 2005 DirectX SDK (the D3DX fallback, docs/building.md "D3DX without the
// DirectX SDK").
//
// The shipped images link the SDK's DxErr9.lib, whose functions return the HRESULT's symbolic name
// (DXGetErrorString9A) and its description (DXGetErrorDescription9A). No other library provides
// them, so both return "HRESULT 0x...." instead. The callers only put the text into error messages
// and dialogs; none branches on it. renddx9.cmd and textureatlasbuilder.cmd compile this file only
// when dxsdk.cmd selected the fallback.
#if defined(BF2_IMAGE_TEXTUREATLASBUILDER) || defined(BF2_IMAGE_RENDDX9)

#include <dxerr9.h>

#include <cstdio>

namespace {

/// Formats @p hr as "HRESULT 0xXXXXXXXX" into @p text.
const char* formatHresult(char (&text)[32], HRESULT hr)
{
    std::snprintf(text, sizeof(text), "HRESULT 0x%08lX", static_cast<unsigned long>(hr));
    return text;
}

} // namespace

/// Stand-in for the symbolic name. The text lives in a static buffer that the next call overwrites;
/// callers copy it at once, as they do the SDK function's static strings.
extern "C" const char* WINAPI DXGetErrorString9A(HRESULT hr)
{
    static char text[32];
    return formatHresult(text, hr);
}

/// Stand-in for the description, with the same buffer contract as DXGetErrorString9A.
extern "C" const char* WINAPI DXGetErrorDescription9A(HRESULT hr)
{
    static char text[32];
    return formatHresult(text, hr);
}

#endif // BF2_IMAGE_TEXTUREATLASBUILDER || BF2_IMAGE_RENDDX9
