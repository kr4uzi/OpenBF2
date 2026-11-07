// [CHOSEN, not recovered] D3DX9 public API declarations used by the renderer.
// April 2005 effect contract; declarations are not an implementation of D3DX.
#ifndef BF2_GFX_D3DX9SUBSET_H
#define BF2_GFX_D3DX9SUBSET_H
#include "D3D9Subset.h"
#define D3DX_SDK_VERSION 25
constexpr UINT D3DX_DEFAULT=0xffffffffu;
constexpr UINT D3DX_DEFAULT_NONPOW2=0xfffffffeu, D3DX_FROM_FILE=0xfffffffdu;
constexpr DWORD D3DX_FILTER_NONE=1;
constexpr DWORD D3DX_FILTER_POINT=2, D3DX_FILTER_LINEAR=3, D3DX_FILTER_TRIANGLE=4, D3DX_FILTER_BOX=5;
constexpr DWORD D3DX_FILTER_MIRROR_U=0x10000, D3DX_FILTER_MIRROR_V=0x20000, D3DX_FILTER_MIRROR_W=0x40000;
constexpr DWORD D3DX_FILTER_DITHER=0x80000, D3DX_FILTER_SRGB_IN=0x200000, D3DX_FILTER_SRGB_OUT=0x400000;
constexpr DWORD D3DXMESH_32BIT=1, D3DXMESH_MANAGED=0x220, D3DXMESH_SYSTEMMEM=0x110;
using D3DXHANDLE = const char*;
struct D3DXVECTOR2 { float x,y; D3DXVECTOR2() = default; D3DXVECTOR2(float a,float b):x(a),y(b){} };
struct D3DXVECTOR3 : D3DVECTOR { D3DXVECTOR3() = default; D3DXVECTOR3(float a,float b,float c) { x=a;y=b;z=c; } };
struct D3DXVECTOR4 { float x,y,z,w; D3DXVECTOR4() = default; D3DXVECTOR4(float a,float b,float c,float d):x(a),y(b),z(c),w(d){} };
struct D3DXMATRIX : D3DMATRIX {};
struct D3DXFLOAT16 { WORD value; };
struct D3DXMACRO { LPCSTR Name,Definition; };
enum D3DXINCLUDE_TYPE { D3DXINC_LOCAL=0, D3DXINC_SYSTEM=1 };
enum D3DXIMAGE_FILEFORMAT { D3DXIFF_BMP=0, D3DXIFF_JPG=1, D3DXIFF_TGA=2,
    D3DXIFF_PNG=3, D3DXIFF_DDS=4, D3DXIFF_PPM=5, D3DXIFF_DIB=6, D3DXIFF_HDR=7, D3DXIFF_PFM=8 };
enum D3DXPARAMETER_CLASS { D3DXPC_SCALAR=0, D3DXPC_VECTOR=1, D3DXPC_MATRIX_ROWS=2,
    D3DXPC_MATRIX_COLUMNS=3, D3DXPC_OBJECT=4, D3DXPC_STRUCT=5 };
enum D3DXPARAMETER_TYPE { D3DXPT_VOID=0,D3DXPT_BOOL=1,D3DXPT_INT=2,D3DXPT_FLOAT=3,
    D3DXPT_STRING=4,D3DXPT_TEXTURE=5,D3DXPT_TEXTURE1D=6,D3DXPT_TEXTURE2D=7,
    D3DXPT_TEXTURE3D=8,D3DXPT_TEXTURECUBE=9,D3DXPT_SAMPLER=10,D3DXPT_SAMPLER1D=11,
    D3DXPT_SAMPLER2D=12,D3DXPT_SAMPLER3D=13,D3DXPT_SAMPLERCUBE=14,
    D3DXPT_PIXELSHADER=15,D3DXPT_VERTEXSHADER=16,D3DXPT_PIXELFRAGMENT=17,D3DXPT_VERTEXFRAGMENT=18 };
struct D3DXIMAGE_INFO { UINT Width,Height,Depth,MipLevels; D3DFORMAT Format;
    D3DRESOURCETYPE ResourceType; D3DXIMAGE_FILEFORMAT ImageFileFormat; };
struct D3DXPARAMETER_DESC { LPCSTR Name,Semantic; D3DXPARAMETER_CLASS Class;
    D3DXPARAMETER_TYPE Type; UINT Rows,Columns,Elements,Annotations,StructMembers; DWORD Flags; UINT Bytes; };
struct D3DXTECHNIQUE_DESC { LPCSTR Name; UINT Passes,Annotations; };
struct D3DXPASS_DESC { LPCSTR Name; UINT Annotations; const DWORD* pVertexShaderFunction; const DWORD* pPixelShaderFunction; };
struct D3DXEFFECT_DESC { LPCSTR Creator; UINT Parameters,Techniques,Functions; };
struct D3DXFUNCTION_DESC { LPCSTR Name; UINT Annotations; };
struct D3DXATTRIBUTERANGE { DWORD AttribId,FaceStart,FaceCount,VertexStart,VertexCount; };
struct D3DXATTRIBUTEWEIGHTS;
struct D3DXRTS_DESC { UINT Width,Height; D3DFORMAT Format; BOOL DepthStencil; D3DFORMAT DepthStencilFormat; };

struct ID3DXEffectPool;
struct ID3DXEffect;
struct ID3DXEffectCompiler;
struct ID3DXEffectStateManager;
struct ID3DXMesh;
struct ID3DXBuffer;
struct ID3DXRenderToSurface;
struct ID3DXConstantTable;
using LPD3DXBUFFER = ID3DXBuffer*;
using LPD3DXMESH = ID3DXMesh*;
using LPD3DXEFFECT = ID3DXEffect*;
using LPD3DXEFFECTCOMPILER = ID3DXEffectCompiler*;
using LPD3DXEFFECTPOOL = ID3DXEffectPool*;
using LPD3DXEFFECTSTATEMANAGER = ID3DXEffectStateManager*;
#define BF2_D3DX_METHOD(ret,name,args) virtual ret STDMETHODCALLTYPE name args = 0
struct ID3DXBuffer : IUnknown {
    BF2_D3DX_METHOD(void*,GetBufferPointer,());
    BF2_D3DX_METHOD(DWORD,GetBufferSize,());
};
// April 2005 d3dx9core.h order, including the FLOAT/BOOL-valued getters.
struct ID3DXLine : IUnknown {
    BF2_D3DX_METHOD(HRESULT,GetDevice,(IDirect3DDevice9**));
    BF2_D3DX_METHOD(HRESULT,Begin,());
    BF2_D3DX_METHOD(HRESULT,Draw,(const D3DXVECTOR2*,DWORD,D3DCOLOR));
    BF2_D3DX_METHOD(HRESULT,DrawTransform,(const D3DXVECTOR3*,DWORD,const D3DXMATRIX*,D3DCOLOR));
    BF2_D3DX_METHOD(HRESULT,SetPattern,(DWORD));
    BF2_D3DX_METHOD(DWORD,GetPattern,());
    BF2_D3DX_METHOD(HRESULT,SetPatternScale,(FLOAT));
    BF2_D3DX_METHOD(FLOAT,GetPatternScale,());
    BF2_D3DX_METHOD(HRESULT,SetWidth,(FLOAT));
    BF2_D3DX_METHOD(FLOAT,GetWidth,());
    BF2_D3DX_METHOD(HRESULT,SetAntialias,(BOOL));
    BF2_D3DX_METHOD(BOOL,GetAntialias,());
    BF2_D3DX_METHOD(HRESULT,SetGLLines,(BOOL));
    BF2_D3DX_METHOD(BOOL,GetGLLines,());
    BF2_D3DX_METHOD(HRESULT,End,());
    BF2_D3DX_METHOD(HRESULT,OnLostDevice,());
    BF2_D3DX_METHOD(HRESULT,OnResetDevice,());
};
using LPD3DXLINE = ID3DXLine*;
// ID3DXInclude is deliberately not derived from IUnknown and has no destructor slot.
struct ID3DXInclude {
    BF2_D3DX_METHOD(HRESULT,Open,(D3DXINCLUDE_TYPE,LPCSTR,LPCVOID,LPCVOID*,UINT*));
    BF2_D3DX_METHOD(HRESULT,Close,(LPCVOID));
};
using LPD3DXINCLUDE = ID3DXInclude*;
// ID3DXEffectPool adds no methods to IUnknown; D3DXCreateEffect shares `shared` parameters through it.
// (Body on two lines: tools/macos/gen_d3d9_forward.py parses interface bodies up to a line "};".)
struct ID3DXEffectPool : IUnknown {
};
struct ID3DXBaseEffect : IUnknown {
    BF2_D3DX_METHOD(HRESULT,GetDesc,(D3DXEFFECT_DESC*));
    BF2_D3DX_METHOD(HRESULT,GetParameterDesc,(D3DXHANDLE,D3DXPARAMETER_DESC*));
    BF2_D3DX_METHOD(HRESULT,GetTechniqueDesc,(D3DXHANDLE,D3DXTECHNIQUE_DESC*));
    BF2_D3DX_METHOD(HRESULT,GetPassDesc,(D3DXHANDLE,D3DXPASS_DESC*));
    BF2_D3DX_METHOD(HRESULT,GetFunctionDesc,(D3DXHANDLE,D3DXFUNCTION_DESC*));
    BF2_D3DX_METHOD(D3DXHANDLE,GetParameter,(D3DXHANDLE,UINT));
    BF2_D3DX_METHOD(D3DXHANDLE,GetParameterByName,(D3DXHANDLE,LPCSTR));
    BF2_D3DX_METHOD(D3DXHANDLE,GetParameterBySemantic,(D3DXHANDLE,LPCSTR));
    BF2_D3DX_METHOD(D3DXHANDLE,GetParameterElement,(D3DXHANDLE,UINT));
    BF2_D3DX_METHOD(D3DXHANDLE,GetTechnique,(UINT));
    BF2_D3DX_METHOD(D3DXHANDLE,GetTechniqueByName,(LPCSTR));
    BF2_D3DX_METHOD(D3DXHANDLE,GetPass,(D3DXHANDLE,UINT));
    BF2_D3DX_METHOD(D3DXHANDLE,GetPassByName,(D3DXHANDLE,LPCSTR));
    BF2_D3DX_METHOD(D3DXHANDLE,GetFunction,(UINT));
    BF2_D3DX_METHOD(D3DXHANDLE,GetFunctionByName,(LPCSTR));
    BF2_D3DX_METHOD(D3DXHANDLE,GetAnnotation,(D3DXHANDLE,UINT));
    BF2_D3DX_METHOD(D3DXHANDLE,GetAnnotationByName,(D3DXHANDLE,LPCSTR));
    BF2_D3DX_METHOD(HRESULT,SetValue,(D3DXHANDLE,LPCVOID,UINT));
    BF2_D3DX_METHOD(HRESULT,GetValue,(D3DXHANDLE,LPVOID,UINT));
    BF2_D3DX_METHOD(HRESULT,SetBool,(D3DXHANDLE,BOOL));
    BF2_D3DX_METHOD(HRESULT,GetBool,(D3DXHANDLE,BOOL*));
    BF2_D3DX_METHOD(HRESULT,SetBoolArray,(D3DXHANDLE,const BOOL*,UINT));
    BF2_D3DX_METHOD(HRESULT,GetBoolArray,(D3DXHANDLE,BOOL*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetInt,(D3DXHANDLE,INT));
    BF2_D3DX_METHOD(HRESULT,GetInt,(D3DXHANDLE,INT*));
    BF2_D3DX_METHOD(HRESULT,SetIntArray,(D3DXHANDLE,const INT*,UINT));
    BF2_D3DX_METHOD(HRESULT,GetIntArray,(D3DXHANDLE,INT*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetFloat,(D3DXHANDLE,FLOAT));
    BF2_D3DX_METHOD(HRESULT,GetFloat,(D3DXHANDLE,FLOAT*));
    BF2_D3DX_METHOD(HRESULT,SetFloatArray,(D3DXHANDLE,const FLOAT*,UINT));
    BF2_D3DX_METHOD(HRESULT,GetFloatArray,(D3DXHANDLE,FLOAT*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetVector,(D3DXHANDLE,const D3DXVECTOR4*));
    BF2_D3DX_METHOD(HRESULT,GetVector,(D3DXHANDLE,D3DXVECTOR4*));
    BF2_D3DX_METHOD(HRESULT,SetVectorArray,(D3DXHANDLE,const D3DXVECTOR4*,UINT));
    BF2_D3DX_METHOD(HRESULT,GetVectorArray,(D3DXHANDLE,D3DXVECTOR4*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetMatrix,(D3DXHANDLE,const D3DXMATRIX*));
    BF2_D3DX_METHOD(HRESULT,GetMatrix,(D3DXHANDLE,D3DXMATRIX*));
    BF2_D3DX_METHOD(HRESULT,SetMatrixArray,(D3DXHANDLE,const D3DXMATRIX*,UINT));
    BF2_D3DX_METHOD(HRESULT,GetMatrixArray,(D3DXHANDLE,D3DXMATRIX*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetMatrixPointerArray,(D3DXHANDLE,const D3DXMATRIX**,UINT));
    BF2_D3DX_METHOD(HRESULT,GetMatrixPointerArray,(D3DXHANDLE,D3DXMATRIX**,UINT));
    BF2_D3DX_METHOD(HRESULT,SetMatrixTranspose,(D3DXHANDLE,const D3DXMATRIX*));
    BF2_D3DX_METHOD(HRESULT,GetMatrixTranspose,(D3DXHANDLE,D3DXMATRIX*));
    BF2_D3DX_METHOD(HRESULT,SetMatrixTransposeArray,(D3DXHANDLE,const D3DXMATRIX*,UINT));
    BF2_D3DX_METHOD(HRESULT,GetMatrixTransposeArray,(D3DXHANDLE,D3DXMATRIX*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetMatrixTransposePointerArray,(D3DXHANDLE,const D3DXMATRIX**,UINT));
    BF2_D3DX_METHOD(HRESULT,GetMatrixTransposePointerArray,(D3DXHANDLE,D3DXMATRIX**,UINT));
    BF2_D3DX_METHOD(HRESULT,SetString,(D3DXHANDLE,LPCSTR));
    BF2_D3DX_METHOD(HRESULT,GetString,(D3DXHANDLE,LPCSTR*));
    BF2_D3DX_METHOD(HRESULT,SetTexture,(D3DXHANDLE,IDirect3DBaseTexture9*));
    BF2_D3DX_METHOD(HRESULT,GetTexture,(D3DXHANDLE,IDirect3DBaseTexture9**));
    BF2_D3DX_METHOD(HRESULT,GetPixelShader,(D3DXHANDLE,IDirect3DPixelShader9**));
    BF2_D3DX_METHOD(HRESULT,GetVertexShader,(D3DXHANDLE,IDirect3DVertexShader9**));
    BF2_D3DX_METHOD(HRESULT,SetArrayRange,(D3DXHANDLE,UINT,UINT));
};
struct ID3DXEffect : ID3DXBaseEffect {
    BF2_D3DX_METHOD(HRESULT,GetPool,(ID3DXEffectPool**));
    BF2_D3DX_METHOD(HRESULT,SetTechnique,(D3DXHANDLE));
    BF2_D3DX_METHOD(D3DXHANDLE,GetCurrentTechnique,());
    BF2_D3DX_METHOD(HRESULT,ValidateTechnique,(D3DXHANDLE));
    BF2_D3DX_METHOD(HRESULT,FindNextValidTechnique,(D3DXHANDLE,D3DXHANDLE*));
    BF2_D3DX_METHOD(BOOL,IsParameterUsed,(D3DXHANDLE,D3DXHANDLE));
    BF2_D3DX_METHOD(HRESULT,Begin,(UINT*,DWORD));
    BF2_D3DX_METHOD(HRESULT,BeginPass,(UINT));
    BF2_D3DX_METHOD(HRESULT,CommitChanges,());
    BF2_D3DX_METHOD(HRESULT,EndPass,());
    BF2_D3DX_METHOD(HRESULT,End,());
    BF2_D3DX_METHOD(HRESULT,GetDevice,(IDirect3DDevice9**));
    BF2_D3DX_METHOD(HRESULT,OnLostDevice,());
    BF2_D3DX_METHOD(HRESULT,OnResetDevice,());
    BF2_D3DX_METHOD(HRESULT,SetStateManager,(ID3DXEffectStateManager*));
};
struct ID3DXEffectCompiler : ID3DXBaseEffect {
    BF2_D3DX_METHOD(HRESULT,SetLiteral,(D3DXHANDLE,BOOL));
    BF2_D3DX_METHOD(HRESULT,GetLiteral,(D3DXHANDLE,BOOL*));
    BF2_D3DX_METHOD(HRESULT,CompileEffect,(DWORD,ID3DXBuffer**,ID3DXBuffer**));
};
struct ID3DXEffectStateManager : IUnknown {
    BF2_D3DX_METHOD(HRESULT,SetTransform,(D3DTRANSFORMSTATETYPE,const D3DMATRIX*));
    BF2_D3DX_METHOD(HRESULT,SetMaterial,(const D3DMATERIAL9*));
    BF2_D3DX_METHOD(HRESULT,SetLight,(DWORD,const D3DLIGHT9*));
    BF2_D3DX_METHOD(HRESULT,LightEnable,(DWORD,BOOL));
    BF2_D3DX_METHOD(HRESULT,SetRenderState,(D3DRENDERSTATETYPE,DWORD));
    BF2_D3DX_METHOD(HRESULT,SetTexture,(DWORD,IDirect3DBaseTexture9*));
    BF2_D3DX_METHOD(HRESULT,SetTextureStageState,(DWORD,D3DTEXTURESTAGESTATETYPE,DWORD));
    BF2_D3DX_METHOD(HRESULT,SetSamplerState,(DWORD,D3DSAMPLERSTATETYPE,DWORD));
    BF2_D3DX_METHOD(HRESULT,SetNPatchMode,(FLOAT));
    BF2_D3DX_METHOD(HRESULT,SetFVF,(DWORD));
    BF2_D3DX_METHOD(HRESULT,SetVertexShader,(IDirect3DVertexShader9*));
    BF2_D3DX_METHOD(HRESULT,SetVertexShaderConstantF,(UINT,const FLOAT*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetVertexShaderConstantI,(UINT,const INT*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetVertexShaderConstantB,(UINT,const BOOL*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetPixelShader,(IDirect3DPixelShader9*));
    BF2_D3DX_METHOD(HRESULT,SetPixelShaderConstantF,(UINT,const FLOAT*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetPixelShaderConstantI,(UINT,const INT*,UINT));
    BF2_D3DX_METHOD(HRESULT,SetPixelShaderConstantB,(UINT,const BOOL*,UINT));
};
struct ID3DXBaseMesh : IUnknown {
    BF2_D3DX_METHOD(HRESULT,DrawSubset,(DWORD));
    BF2_D3DX_METHOD(DWORD,GetNumFaces,());
    BF2_D3DX_METHOD(DWORD,GetNumVertices,());
    BF2_D3DX_METHOD(DWORD,GetFVF,());
    BF2_D3DX_METHOD(HRESULT,GetDeclaration,(D3DVERTEXELEMENT9*));
    BF2_D3DX_METHOD(DWORD,GetNumBytesPerVertex,());
    BF2_D3DX_METHOD(DWORD,GetOptions,());
    BF2_D3DX_METHOD(HRESULT,GetDevice,(IDirect3DDevice9**));
    BF2_D3DX_METHOD(HRESULT,CloneMeshFVF,(DWORD,DWORD,IDirect3DDevice9*,ID3DXMesh**));
    BF2_D3DX_METHOD(HRESULT,CloneMesh,(DWORD,const D3DVERTEXELEMENT9*,IDirect3DDevice9*,ID3DXMesh**));
    BF2_D3DX_METHOD(HRESULT,GetVertexBuffer,(IDirect3DVertexBuffer9**));
    BF2_D3DX_METHOD(HRESULT,GetIndexBuffer,(IDirect3DIndexBuffer9**));
    BF2_D3DX_METHOD(HRESULT,LockVertexBuffer,(DWORD,void**));
    BF2_D3DX_METHOD(HRESULT,UnlockVertexBuffer,());
    BF2_D3DX_METHOD(HRESULT,LockIndexBuffer,(DWORD,void**));
    BF2_D3DX_METHOD(HRESULT,UnlockIndexBuffer,());
    BF2_D3DX_METHOD(HRESULT,GetAttributeTable,(D3DXATTRIBUTERANGE*,DWORD*));
    BF2_D3DX_METHOD(HRESULT,ConvertPointRepsToAdjacency,(const DWORD*,DWORD*));
    BF2_D3DX_METHOD(HRESULT,ConvertAdjacencyToPointReps,(const DWORD*,DWORD*));
    BF2_D3DX_METHOD(HRESULT,GenerateAdjacency,(FLOAT,DWORD*));
    BF2_D3DX_METHOD(HRESULT,UpdateSemantics,(D3DVERTEXELEMENT9*));
};
struct ID3DXMesh : ID3DXBaseMesh {
    BF2_D3DX_METHOD(HRESULT,LockAttributeBuffer,(DWORD,DWORD**));
    BF2_D3DX_METHOD(HRESULT,UnlockAttributeBuffer,());
    BF2_D3DX_METHOD(HRESULT,Optimize,(DWORD,const DWORD*,DWORD*,DWORD*,ID3DXBuffer**,ID3DXMesh**));
    BF2_D3DX_METHOD(HRESULT,OptimizeInplace,(DWORD,const DWORD*,DWORD*,DWORD*,ID3DXBuffer**));
};
struct ID3DXRenderToSurface : IUnknown {
    BF2_D3DX_METHOD(HRESULT,GetDevice,(IDirect3DDevice9**));
    BF2_D3DX_METHOD(HRESULT,GetDesc,(D3DXRTS_DESC*));
    BF2_D3DX_METHOD(HRESULT,BeginScene,(IDirect3DSurface9*,const D3DVIEWPORT9*));
    BF2_D3DX_METHOD(HRESULT,EndScene,(DWORD));
};
#undef BF2_D3DX_METHOD

using LPD3DXFILL2D = void (WINAPI *)(D3DXVECTOR4*,const D3DXVECTOR2*,const D3DXVECTOR2*,void*);
extern "C" {
HRESULT WINAPI D3DXCreateLine(IDirect3DDevice9* pDevice, ID3DXLine** ppLine);
HRESULT WINAPI D3DXCreateBuffer(DWORD NumBytes, ID3DXBuffer** ppBuffer);
HRESULT WINAPI D3DXCreateEffect(IDirect3DDevice9* pDevice, LPCVOID pSrcData, UINT SrcDataLen,
    const D3DXMACRO* pDefines, ID3DXInclude* pInclude, DWORD Flags, ID3DXEffectPool* pPool,
    ID3DXEffect** ppEffect, ID3DXBuffer** ppCompilationErrors);
HRESULT WINAPI D3DXCreateEffectPool(ID3DXEffectPool** ppPool);
HRESULT WINAPI D3DXCreateEffectCompiler(LPCSTR pSrcData, UINT SrcDataLen, const D3DXMACRO* pDefines,
    ID3DXInclude* pInclude, DWORD Flags, ID3DXEffectCompiler** ppCompiler, ID3DXBuffer** ppParseErrors);
HRESULT WINAPI D3DXAssembleShader(LPCSTR pSrcData, UINT SrcDataLen, const D3DXMACRO* pDefines,
    ID3DXInclude* pInclude, DWORD Flags, ID3DXBuffer** ppShader, ID3DXBuffer** ppErrorMsgs);
DWORD WINAPI D3DXGetShaderVersion(const DWORD* pFunction);
HRESULT WINAPI D3DXCreateTexture(IDirect3DDevice9* pDevice, UINT Width, UINT Height, UINT MipLevels,
    DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture);
HRESULT WINAPI D3DXCreateTextureFromFileExA(IDirect3DDevice9* pDevice, LPCSTR pSrcFile, UINT Width,
    UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter,
    DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo, PALETTEENTRY* pPalette,
    IDirect3DTexture9** ppTexture);
HRESULT WINAPI D3DXCreateTextureFromFileInMemory(IDirect3DDevice9* pDevice, LPCVOID pSrcData,
    UINT SrcDataSize, IDirect3DTexture9** ppTexture);
HRESULT WINAPI D3DXGetImageInfoFromFileA(LPCSTR pSrcFile, D3DXIMAGE_INFO* pSrcInfo);
HRESULT WINAPI D3DXGetImageInfoFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, D3DXIMAGE_INFO* pSrcInfo);
HRESULT WINAPI D3DXFilterTexture(IDirect3DBaseTexture9* pBaseTexture, const PALETTEENTRY* pPalette,
    UINT SrcLevel, DWORD Filter);
HRESULT WINAPI D3DXFillTexture(IDirect3DTexture9* pTexture, LPD3DXFILL2D pFunction, void* pData);
HRESULT WINAPI D3DXSaveTextureToFileA(LPCSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat,
    IDirect3DBaseTexture9* pSrcTexture, const PALETTEENTRY* pSrcPalette);
HRESULT WINAPI D3DXSaveSurfaceToFileA(LPCSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat,
    IDirect3DSurface9* pSrcSurface, const PALETTEENTRY* pSrcPalette, const RECT* pSrcRect);
HRESULT WINAPI D3DXLoadSurfaceFromSurface(IDirect3DSurface9* pDestSurface, const PALETTEENTRY* pDestPalette,
    const RECT* pDestRect, IDirect3DSurface9* pSrcSurface, const PALETTEENTRY* pSrcPalette,
    const RECT* pSrcRect, DWORD Filter, D3DCOLOR ColorKey);
HRESULT WINAPI D3DXLoadSurfaceFromMemory(IDirect3DSurface9* pDestSurface, const PALETTEENTRY* pDestPalette,
    const RECT* pDestRect, LPCVOID pSrcMemory, D3DFORMAT SrcFormat, UINT SrcPitch,
    const PALETTEENTRY* pSrcPalette, const RECT* pSrcRect, DWORD Filter, D3DCOLOR ColorKey);
HRESULT WINAPI D3DXCreateRenderToSurface(IDirect3DDevice9* pDevice, UINT Width, UINT Height,
    D3DFORMAT Format, BOOL DepthStencil, D3DFORMAT DepthStencilFormat, ID3DXRenderToSurface** ppRenderToSurface);
HRESULT WINAPI D3DXCreateMesh(DWORD NumFaces, DWORD NumVertices, DWORD Options,
    const D3DVERTEXELEMENT9* pDeclaration, IDirect3DDevice9* pD3DDevice, ID3DXMesh** ppMesh);
HRESULT WINAPI D3DXCreateMeshFVF(DWORD NumFaces, DWORD NumVertices, DWORD Options, DWORD FVF,
    IDirect3DDevice9* pD3DDevice, ID3DXMesh** ppMesh);
// The mesh optimizers' strip conversion (RendDX9 0x10133549, 0x1013596A).
HRESULT WINAPI D3DXConvertMeshSubsetToSingleStrip(ID3DXBaseMesh* MeshIn, DWORD AttribId,
    DWORD IBOptions, IDirect3DIndexBuffer9** ppIndexBuffer, DWORD* pNumIndices);
HRESULT WINAPI D3DXCreateSphere(IDirect3DDevice9* pDevice, FLOAT Radius, UINT Slices, UINT Stacks,
    ID3DXMesh** ppMesh, ID3DXBuffer** ppAdjacency);
HRESULT WINAPI D3DXCreateBox(IDirect3DDevice9* pDevice, FLOAT Width, FLOAT Height, FLOAT Depth,
    ID3DXMesh** ppMesh, ID3DXBuffer** ppAdjacency);
HRESULT WINAPI D3DXCreateCylinder(IDirect3DDevice9* pDevice, FLOAT Radius1, FLOAT Radius2, FLOAT Length,
    UINT Slices, UINT Stacks, ID3DXMesh** ppMesh, ID3DXBuffer** ppAdjacency);
HRESULT WINAPI D3DXComputeBoundingBox(const D3DXVECTOR3* pFirstPosition, DWORD NumVertices,
    DWORD dwStride, D3DXVECTOR3* pMin, D3DXVECTOR3* pMax);
HRESULT WINAPI D3DXComputeBoundingSphere(const D3DXVECTOR3* pFirstPosition, DWORD NumVertices,
    DWORD dwStride, D3DXVECTOR3* pCenter, FLOAT* pRadius);
D3DXFLOAT16* WINAPI D3DXFloat32To16Array(D3DXFLOAT16* pOut, const FLOAT* pIn, UINT n);
D3DXMATRIX* WINAPI D3DXMatrixMultiply(D3DXMATRIX* pOut, const D3DXMATRIX* pM1, const D3DXMATRIX* pM2);
D3DXMATRIX* WINAPI D3DXMatrixInverse(D3DXMATRIX* pOut, FLOAT* pDeterminant, const D3DXMATRIX* pM);
D3DXMATRIX* WINAPI D3DXMatrixTranspose(D3DXMATRIX* pOut, const D3DXMATRIX* pM);
D3DXMATRIX* WINAPI D3DXMatrixRotationZ(D3DXMATRIX* pOut, FLOAT Angle);
D3DXMATRIX* WINAPI D3DXMatrixOrthoLH(D3DXMATRIX* pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf);
D3DXMATRIX* WINAPI D3DXMatrixLookAtLH(D3DXMATRIX* pOut, const D3DXVECTOR3* pEye,
    const D3DXVECTOR3* pAt, const D3DXVECTOR3* pUp);
}
static_assert(sizeof(D3DXIMAGE_INFO)==28 && sizeof(D3DXVECTOR3)==12 && sizeof(D3DXMATRIX)==64, "D3DX value layouts");
#endif
