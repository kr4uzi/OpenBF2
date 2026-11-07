// [CHOSEN, not recovered] Windows preamble of the G2w forwarding wrappers (BF2_GFX_SEAM=ON).
//
// One translation unit has to name two incompatible sets of D3D9/D3DX declarations: the real SDK
// (global namespace, as always) and the renderer's subset headers. The subset is therefore
// included INSIDE `namespace sub`, after
//   * the SDK macro values the subset re-declares were captured (checked against the subset in
//     Generated/ForwardChecks.inc) and the macros dropped, and
//   * the subset's extern "C" function names were renamed, so that they do not clash with the
//     SDK's same-named C functions of different (real vs subset) interface types.
// `namespace real` then re-exports the SDK names the generated code uses. Compile this TU WITHOUT
// the renderer's Platform/Gfx/Include prefix, so <d3d9.h> / <d3dx9.h> are the SDK's.
// The macOS unit test supplies its own ForwardSdk.h (a generated fixture) instead of this one.
#ifndef BF2_GFX_D3D9FORWARD_SDK_H
#define BF2_GFX_D3D9FORWARD_SDK_H

#if !defined(_WIN32)
#error "D3D9Forward/ForwardSdk.h is the Windows preamble; the macOS test fixture provides its own"
#endif

#include <windows.h>
#include <unknwn.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
// Everything the subset headers include is included here first, so the include inside
// `namespace sub` below cannot drag a system header into that namespace.
#include "../../../Core/PlatformCompat.h"

#include "Generated/ForwardMacroValues.inc"
#include "Generated/ForwardUndef.inc"
#include "Generated/ForwardRename.inc"
namespace sub {
#include "../D3D9Subset.h"
#include "../D3DX9Subset.h"
}
#include "Generated/ForwardUnrename.inc"
#include "Generated/ForwardNames.inc"

#endif
