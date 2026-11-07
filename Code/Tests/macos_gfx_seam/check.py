#!/usr/bin/env python3
"""[TEMP] [CHOSEN, not recovered] Verify subset declarations and remaining stops.

Uses the production headers/backend, no game content, device, window or network.
The SIGABRT observer exits immediately to avoid generating host crash reports.
"""
import pathlib
import re
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[3]
out = root / 'out/build/g2-seam-headers/seam-check'
out.mkdir(parents=True, exist_ok=True)
gfx = root / 'Code/BF2/Platform/Gfx'
# G3 implements Direct3DCreate9 in Device.cpp; its GPU contract has its own harness.
functions = []
frontiers = (gfx / 'Metal/Frontiers.cpp').read_text()
header = (gfx / 'D3DX9Subset.h').read_text().split('extern "C" {')[1].split('\n}')[0]
for name, args in re.findall(r'WINAPI (D3DX\w+)\((.*?)\);', header, re.S):
    # Implemented helpers have CPU/GPU tests. Only the remaining link frontiers
    # are callable with empty arguments and expected to abort here.
    if '{ stop("' + name + '"); }' in frontiers:
        functions.append((name, len(args.split(','))))
functions.append(('_purecall', 0))
source = r'''
#include "D3DX9Subset.h"
#include "MacSupport.h"
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <cstddef>
#include <type_traits>
#include <cstdio>

static_assert(!std::has_virtual_destructor<IUnknown>::value, "COM has no destructor slot");
static_assert(!std::is_base_of<IUnknown, ID3DXInclude>::value, "Include is not COM");
static_assert(sizeof(D3DADAPTER_IDENTIFIER9)==1104, "adapter identifier size");
static_assert(offsetof(D3DADAPTER_IDENTIFIER9, DriverVersion)==1056, "adapter version offset");
static_assert(sizeof(D3DPRESENT_PARAMETERS)==64, "native presentation size");
static_assert(offsetof(D3DPRESENT_PARAMETERS, hDeviceWindow)==32, "native window offset");
static_assert(sizeof(D3DXPARAMETER_DESC)==56, "native parameter size");
static_assert(offsetof(D3DXPARAMETER_DESC, Bytes)==48, "native parameter Bytes");
static_assert(sizeof(D3DXPASS_DESC)==32, "native pass size");
static_assert(sizeof(D3DLOCKED_RECT)==16 && sizeof(D3DLOCKED_BOX)==16, "native lock layouts");
static_assert(D3DFMT_DXT1==0x31545844 && D3DFMT_D24S8==75, "format values");
static_assert(D3DRS_SCISSORTESTENABLE==174 && D3DLOCK_DISCARD==0x2000, "state/lock values");
static_assert(FAILED(D3DERR_INVALIDCALL) && SUCCEEDED(S_FALSE), "signed HRESULT");

// Itanium member pointers encode a virtual slot's byte offset plus one (x86-64); the ARM variant
// (Apple arm64) keeps the byte offset and sets the low bit of the adjustment instead.
// This observes actual compiler layout, including inherited slots, without a mock backend.
template<class M> bool slot(M method, std::ptrdiff_t expected)
{
    struct Representation { std::ptrdiff_t pointer, adjustment; } rep;
    static_assert(sizeof(method)==sizeof(rep), "native Itanium member pointer");
    std::memcpy(&rep, &method, sizeof(rep));
#if defined(__aarch64__) || defined(__arm64__)
    return rep.pointer == expected * std::ptrdiff_t(sizeof(void*)) && rep.adjustment == 1;
#else
    return rep.pointer == expected * std::ptrdiff_t(sizeof(void*)) + 1 && rep.adjustment == 0;
#endif
}
int main(int argc, char** argv)
{
    if (argc == 1) {
        bool ok = slot(&IDirect3D9::CreateDevice,16)
            && slot(&IDirect3DDevice9::CreateQuery,118)
            && slot(&IDirect3DDevice9::DrawIndexedPrimitive,82)
            && slot(&IDirect3DDevice9::SetTexture,65)
            && slot(&IDirect3DTexture9::LockRect,19)
            && slot(&IDirect3DVolumeTexture9::UnlockBox,20)
            && slot(&IDirect3DSurface9::UnlockRect,14)
            && slot(&IDirect3DQuery9::GetData,7)
            && slot(&ID3DXBaseEffect::SetArrayRange,56)
            && slot(&ID3DXEffect::CommitChanges,65)
            && slot(&ID3DXEffect::SetStateManager,71)
            && slot(&ID3DXEffectCompiler::CompileEffect,59)
            && slot(&ID3DXMesh::OptimizeInplace,27)
            && slot(&ID3DXEffectStateManager::SetPixelShaderConstantB,20)
            && slot(&ID3DXRenderToSurface::EndScene,6)
            && slot(&ID3DXInclude::Close,1);
        char text[4];
        ok = ok && _snprintf_s(text, sizeof(text), _TRUNCATE, "%s", "abcdef")==-1
            && std::strcmp(text,"abc")==0;
        ok = ok && sprintf_s(text, "%s", "abc")==3 && std::strcmp(text,"abc")==0;
        ok = ok && strncpy_s(text,sizeof(text),"abcdef",_TRUNCATE)==80
            && std::strcmp(text,"abc")==0;
        setenv("BF2_G2_SEAM_CHECK", "1234", 1);
        ok = ok && GetEnvironmentVariableA("BF2_G2_SEAM_CHECK",text,sizeof(text))==5;
        char value[5];
        ok = ok && GetEnvironmentVariableA("BF2_G2_SEAM_CHECK",value,sizeof(value))==4
            && std::strcmp(value,"1234")==0;
        LARGE_INTEGER frequency,first,second;
        QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&first);
        QueryPerformanceCounter(&second);
        ok = ok && frequency.QuadPart==1000000000 && second.QuadPart>=first.QuadPart;
        RECT rectangle={0,0,10,10}; InflateRect(&rectangle,-3,-3);
        ok = ok && rectangle.left==3 && rectangle.top==3 && rectangle.right==7 && rectangle.bottom==7;
        std::puts(ok ? "layout/slot checks pass" : "layout/slot checks FAIL");
        return ok ? 0 : 1;
    }
    std::signal(SIGABRT, [](int) { _exit(93); });
'''
for name, count in functions:
    source += '    if (std::strcmp(argv[1], "' + name + '")==0) ' + name + '(' + ','.join(['{}'] * count) + ');\n'
source += '    return 1;\n}\n'
(out / 'check.cpp').write_text(source)
subprocess.run(['clang++', '-arch', 'x86_64', '-std=c++14', '-fms-extensions',
                '-DDEDICATED=0', '-DBF2_IMAGE_RENDDX9=1',
                '-include', str(root / 'Code/BF2/Core/BuildConfig.h'), '-I', str(gfx),
                str(out / 'check.cpp'), str(gfx / 'Metal/Frontiers.cpp'),
                '-o', str(out / 'check')], check=True)
subprocess.run([str(out / 'check')], check=True)
for name, _ in functions:
    p = subprocess.run([str(out / 'check'), name], capture_output=True, text=True)
    if p.returncode != 93 or '[FRONTIER] ' + name + ' reached:' not in p.stderr:
        sys.exit(f'{name}: stop contract failed: {p.returncode}, {p.stderr!r}')
print(f'{len(functions)-1} graphics free functions and _purecall diagnosed and raised SIGABRT')
