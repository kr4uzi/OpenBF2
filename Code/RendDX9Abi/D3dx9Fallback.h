// [CHOSEN, not recovered] D3DX9 declarations for building RendDX9.dll and TextureAtlasBuilder.dll
// without the April 2005 DirectX SDK.
//
// Included through D3dxFallbackInclude/d3dx9.h, which renddx9.cmd and textureatlasbuilder.cmd put
// on the include path when Code/Build/dxsdk.cmd selects the fallback (BF2_D3DX=auto without an
// April 2005 SDK, or BF2_D3DX=def). The installed SDK stays the preferred path; see
// docs/building.md, "D3DX without the DirectX SDK".
//
// The declarations are the renderer's existing D3DX subset (Platform/Gfx/D3DX9Subset.h), placed on
// top of the Windows SDK's <d3d9.h> instead of the subset's own D3D9 declarations. The rebuilt DLL
// calls the real d3dx9_25.dll through them, so every COM interface declared there must list its
// methods in the slot order of that DLL. tools/d3dx/check_d3dx9_25.py checks that order, and the
// export names in D3dx9_25.def, against the x86 and x64 d3dx9_25.dll and their public symbols.
//
// Linking: Code/Build/renddx9.cmd and textureatlasbuilder.cmd generate d3dx9_25.lib from
// D3dx9_25.def. On x86 the D3DX functions are __stdcall, so a call references the decorated
// _Name@bytes while d3dx9_25.dll exports the plain Name. lib.exe cannot produce that pairing from a
// .def file, so the import library defines _Name and BF2_D3DX_IMPORT below maps each decorated
// reference onto it. A wrong byte count leaves the decorated name unresolved and fails the link.

#ifndef RENDDX9ABI_D3DX9FALLBACK_H
#define RENDDX9ABI_D3DX9FALLBACK_H

#if !defined(_WIN32)
#error "D3dx9Fallback.h is the Windows no-SDK D3DX path; other platforms use Platform/Gfx/Include"
#endif

#include <windows.h>
#include <unknwn.h>
#include <d3d9.h>

// The Windows SDK's <d3d9.h> already declares every D3D9 type the subset needs. Marking the
// subset's own D3D9 header as included keeps D3DX9Subset.h from declaring them a second time.
#ifndef BF2_GFX_D3D9SUBSET_H
#define BF2_GFX_D3D9SUBSET_H
#endif
#include "../BF2/Platform/Gfx/D3DX9Subset.h"

// Mesh entry points the debug-mesh TUs call. The macOS build declares them with C++ linkage in
// Platform/Gfx/Metal/D3DXMesh.h because its own D3DX implements them; here they are d3dx9_25.dll
// exports.
extern "C" {
HRESULT WINAPI D3DXCreatePolygon(IDirect3DDevice9* pDevice, FLOAT Length, UINT Sides,
    ID3DXMesh** ppMesh, ID3DXBuffer** ppAdjacency);
HRESULT WINAPI D3DXCreateTorus(IDirect3DDevice9* pDevice, FLOAT InnerRadius, FLOAT OuterRadius,
    UINT Sides, UINT Rings, ID3DXMesh** ppMesh, ID3DXBuffer** ppAdjacency);
HRESULT WINAPI D3DXComputeNormals(ID3DXBaseMesh* pMesh, const DWORD* pAdjacency);
}

// Shader disassembly and constant tables, used only by the Windows arm of the projected-texture
// pixel-shader rewrite in Ra/EffectManager.cpp. The shared subset leaves ID3DXConstantTable
// incomplete because the macOS build never calls it. Declarations follow the April 2005 SDK's
// d3dx9shader.h and d3dx9effect.h; tools/d3dx/check_d3dx9_25.py checks the interface slot order
// against CConstantTable in d3dx9_25.dll.
enum D3DXREGISTER_SET { D3DXRS_BOOL = 0, D3DXRS_INT4 = 1, D3DXRS_FLOAT4 = 2, D3DXRS_SAMPLER = 3,
    D3DXRS_FORCE_DWORD = 0x7fffffff };
struct D3DXCONSTANTTABLE_DESC { LPCSTR Creator; DWORD Version; UINT Constants; };
struct D3DXCONSTANT_DESC { LPCSTR Name; D3DXREGISTER_SET RegisterSet; UINT RegisterIndex,
    RegisterCount; D3DXPARAMETER_CLASS Class; D3DXPARAMETER_TYPE Type; UINT Rows, Columns,
    Elements, StructMembers, Bytes; LPCVOID DefaultValue; };
#define BF2_D3DX_METHOD(ret,name,args) virtual ret STDMETHODCALLTYPE name args = 0
struct ID3DXConstantTable : ID3DXBuffer {
    BF2_D3DX_METHOD(HRESULT,GetDesc,(D3DXCONSTANTTABLE_DESC*));
    BF2_D3DX_METHOD(HRESULT,GetConstantDesc,(D3DXHANDLE,D3DXCONSTANT_DESC*,UINT*));
    BF2_D3DX_METHOD(UINT,GetSamplerIndex,(D3DXHANDLE));
    BF2_D3DX_METHOD(D3DXHANDLE,GetConstant,(D3DXHANDLE,UINT));
    BF2_D3DX_METHOD(D3DXHANDLE,GetConstantByName,(D3DXHANDLE,LPCSTR));
    BF2_D3DX_METHOD(D3DXHANDLE,GetConstantElement,(D3DXHANDLE,UINT));
    BF2_D3DX_METHOD(HRESULT,SetDefaults,(IDirect3DDevice9*));
    BF2_D3DX_METHOD(HRESULT,SetValue,(IDirect3DDevice9*,D3DXHANDLE,LPCVOID,UINT));
    BF2_D3DX_METHOD(HRESULT,SetBool,(IDirect3DDevice9*,D3DXHANDLE,BOOL));
    BF2_D3DX_METHOD(HRESULT,SetBoolArray,(IDirect3DDevice9*,D3DXHANDLE,const BOOL*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetInt,(IDirect3DDevice9*,D3DXHANDLE,INT));
    BF2_D3DX_METHOD(HRESULT,SetIntArray,(IDirect3DDevice9*,D3DXHANDLE,const INT*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetFloat,(IDirect3DDevice9*,D3DXHANDLE,FLOAT));
    BF2_D3DX_METHOD(HRESULT,SetFloatArray,(IDirect3DDevice9*,D3DXHANDLE,const FLOAT*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetVector,(IDirect3DDevice9*,D3DXHANDLE,const D3DXVECTOR4*));
    BF2_D3DX_METHOD(HRESULT,SetVectorArray,(IDirect3DDevice9*,D3DXHANDLE,const D3DXVECTOR4*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetMatrix,(IDirect3DDevice9*,D3DXHANDLE,const D3DXMATRIX*));
    BF2_D3DX_METHOD(HRESULT,SetMatrixArray,(IDirect3DDevice9*,D3DXHANDLE,const D3DXMATRIX*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetMatrixPointerArray,(IDirect3DDevice9*,D3DXHANDLE,const D3DXMATRIX**,UINT));
    BF2_D3DX_METHOD(HRESULT,SetMatrixTranspose,(IDirect3DDevice9*,D3DXHANDLE,const D3DXMATRIX*));
    BF2_D3DX_METHOD(HRESULT,SetMatrixTransposeArray,(IDirect3DDevice9*,D3DXHANDLE,const D3DXMATRIX*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetMatrixTransposePointerArray,(IDirect3DDevice9*,D3DXHANDLE,const D3DXMATRIX**,UINT));
};
#undef BF2_D3DX_METHOD
using LPD3DXCONSTANTTABLE = ID3DXConstantTable*;
extern "C" {
HRESULT WINAPI D3DXDisassembleEffect(ID3DXEffect* pEffect, BOOL EnableColorCode,
    ID3DXBuffer** ppDisassembly);
HRESULT WINAPI D3DXGetShaderConstantTable(const DWORD* pFunction,
    ID3DXConstantTable** ppConstantTable);
// SwiffFont's Win32 arms (the native subset builds these matrices inline).
D3DXMATRIX* WINAPI D3DXMatrixScaling(D3DXMATRIX* pOut, FLOAT sx, FLOAT sy, FLOAT sz);
D3DXMATRIX* WINAPI D3DXMatrixTranslation(D3DXMATRIX* pOut, FLOAT x, FLOAT y, FLOAT z);
}

#if defined(_M_IX86)
#define BF2_D3DX_IMPORT(name, bytes) \
    __pragma(comment(linker, "/alternatename:_" #name "@" #bytes "=_" #name))
#else
#define BF2_D3DX_IMPORT(name, bytes)
#endif

// One line per D3dx9_25.def export: the x86 argument bytes of the declaration above.
BF2_D3DX_IMPORT(D3DXAssembleShader, 28)
BF2_D3DX_IMPORT(D3DXComputeBoundingBox, 20)
BF2_D3DX_IMPORT(D3DXComputeBoundingSphere, 20)
BF2_D3DX_IMPORT(D3DXComputeNormals, 8)
BF2_D3DX_IMPORT(D3DXConvertMeshSubsetToSingleStrip, 20)
BF2_D3DX_IMPORT(D3DXCreateBox, 24)
BF2_D3DX_IMPORT(D3DXCreateBuffer, 8)
BF2_D3DX_IMPORT(D3DXCreateCylinder, 32)
BF2_D3DX_IMPORT(D3DXCreateEffect, 36)
BF2_D3DX_IMPORT(D3DXCreateEffectCompiler, 28)
BF2_D3DX_IMPORT(D3DXCreateEffectPool, 4)
BF2_D3DX_IMPORT(D3DXCreateLine, 8)
BF2_D3DX_IMPORT(D3DXCreateMesh, 24)
BF2_D3DX_IMPORT(D3DXCreateMeshFVF, 24)
BF2_D3DX_IMPORT(D3DXCreatePolygon, 20)
BF2_D3DX_IMPORT(D3DXCreateRenderToSurface, 28)
BF2_D3DX_IMPORT(D3DXCreateSphere, 24)
BF2_D3DX_IMPORT(D3DXCreateTexture, 32)
BF2_D3DX_IMPORT(D3DXCreateTextureFromFileExA, 56)
BF2_D3DX_IMPORT(D3DXCreateTextureFromFileInMemory, 16)
BF2_D3DX_IMPORT(D3DXCreateTorus, 28)
BF2_D3DX_IMPORT(D3DXDisassembleEffect, 12)
BF2_D3DX_IMPORT(D3DXFillTexture, 12)
BF2_D3DX_IMPORT(D3DXFilterTexture, 16)
BF2_D3DX_IMPORT(D3DXFloat32To16Array, 12)
BF2_D3DX_IMPORT(D3DXGetImageInfoFromFileA, 8)
BF2_D3DX_IMPORT(D3DXGetImageInfoFromFileInMemory, 12)
BF2_D3DX_IMPORT(D3DXGetShaderConstantTable, 8)
BF2_D3DX_IMPORT(D3DXGetShaderVersion, 4)
BF2_D3DX_IMPORT(D3DXLoadSurfaceFromMemory, 40)
BF2_D3DX_IMPORT(D3DXLoadSurfaceFromSurface, 32)
BF2_D3DX_IMPORT(D3DXMatrixInverse, 12)
BF2_D3DX_IMPORT(D3DXMatrixLookAtLH, 16)
BF2_D3DX_IMPORT(D3DXMatrixMultiply, 12)
BF2_D3DX_IMPORT(D3DXMatrixOrthoLH, 20)
BF2_D3DX_IMPORT(D3DXMatrixRotationZ, 8)
BF2_D3DX_IMPORT(D3DXMatrixScaling, 16)
BF2_D3DX_IMPORT(D3DXMatrixTranslation, 16)
BF2_D3DX_IMPORT(D3DXMatrixTranspose, 8)
BF2_D3DX_IMPORT(D3DXSaveSurfaceToFileA, 20)
BF2_D3DX_IMPORT(D3DXSaveTextureToFileA, 16)

#undef BF2_D3DX_IMPORT

#endif // RENDDX9ABI_D3DX9FALLBACK_H
