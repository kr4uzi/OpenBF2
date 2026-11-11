/* [TEMP] [CHOSEN, not recovered] D3DX9_25 matrix-in-struct oracle (matrices.fx):
   1. D3DXCompileShader + ID3DXConstantTable: per-member register index/count/class and the
      registers ID3DXConstantTable::SetValue/SetMatrix write (recorded by a fake device).
   2. ID3DXEffectCompiler: member descriptors, element handles, SetMatrix/SetMatrixTranspose/
      SetValue/GetValue layouts. Run under Wine with BF2's d3dx9_25.dll. */
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <d3dx9.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef HRESULT (WINAPI *CreateCompiler)(const char *, UINT, const D3DXMACRO *, ID3DXInclude *, DWORD,
                                         ID3DXEffectCompiler **, ID3DXBuffer **);
typedef HRESULT (WINAPI *CompileShader)(const char *, UINT, const D3DXMACRO *, ID3DXInclude *, const char *,
                                        const char *, DWORD, ID3DXBuffer **, ID3DXBuffer **, ID3DXConstantTable **);
/* Fake device: every slot aborts with its index except the vertex constant setters. */
static void *slots[119];
static struct { void **vtbl; } fakeDevice = {slots};
static float written[256][4]; static int writtenMask[256];
#define STUB(n) static void __stdcall stub##n(void) { printf("unexpected device slot %d\n", n); fflush(stdout); ExitProcess(3); }
STUB(0) STUB(1) STUB(2) STUB(3)
static ULONG __stdcall addref(void *t) { (void)t; return 2; }
static ULONG __stdcall release(void *t) { (void)t; return 1; }
static HRESULT __stdcall setF(void *t, UINT reg, const float *v, UINT n)
{
    UINT i; (void)t;
    for (i = 0; i < n && reg + i < 256; ++i) { memcpy(written[reg + i], v + i * 4, 16); writtenMask[reg + i] = 1; }
    return S_OK;
}
static HRESULT __stdcall setIB(void *t, UINT reg, const void *v, UINT n) { (void)t; (void)v; printf("int/bool constants %u x%u\n", reg, n); return S_OK; }
static void clearWrites(void) { memset(written, 0, sizeof(written)); memset(writtenMask, 0, sizeof(writtenMask)); }
static void showWrites(const char *label)
{
    int r; printf("%s:\n", label);
    for (r = 0; r < 256; ++r) if (writtenMask[r])
        printf("  c%-3d %g %g %g %g\n", r, written[r][0], written[r][1], written[r][2], written[r][3]);
}
static ID3DXConstantTable *ct;
#define CT (ct->lpVtbl)
static void cdesc(const char *label, D3DXHANDLE h)
{
    D3DXCONSTANT_DESC d; UINT n = 1; HRESULT hr;
    if (!h) { printf("%s: null\n", label); return; }
    memset(&d, 0, sizeof(d));
    hr = CT->GetConstantDesc(ct, h, &d, &n);
    printf("%s: hr=%08lx name=%s set=%u reg=%u count=%u class=%u type=%u rows=%u cols=%u elements=%u members=%u bytes=%u\n",
           label, (unsigned long)hr, d.Name ? d.Name : "(null)", d.RegisterSet, d.RegisterIndex, d.RegisterCount,
           d.Class, d.Type, d.Rows, d.Columns, d.Elements, d.StructMembers, d.Bytes);
}
static ID3DXEffectCompiler *fx;
#define V (fx->lpVtbl)
static void pdesc(const char *label, D3DXHANDLE h)
{
    D3DXPARAMETER_DESC d; HRESULT hr;
    if (!h) { printf("%s: null\n", label); return; }
    memset(&d, 0, sizeof(d));
    hr = V->GetParameterDesc(fx, h, &d);
    printf("%s: hr=%08lx name=%s sem=%s class=%u type=%u rows=%u cols=%u elements=%u members=%u bytes=%u\n",
           label, (unsigned long)hr, d.Name ? d.Name : "(null)", d.Semantic ? d.Semantic : "(null)", d.Class, d.Type,
           d.Rows, d.Columns, d.Elements, d.StructMembers, d.Bytes);
}
static void dumpf(const char *label, const float *f, unsigned n)
{
    unsigned i; printf("%s:", label);
    for (i = 0; i < n; ++i) printf(" %g", f[i]);
    printf("\n");
}
int main(int argc, char **argv)
{
    HMODULE m = LoadLibraryA("d3dx9_25.dll");
    CreateCompiler create = m ? (CreateCompiler)(void *)GetProcAddress(m, "D3DXCreateEffectCompiler") : NULL;
    CompileShader compile = m ? (CompileShader)(void *)GetProcAddress(m, "D3DXCompileShader") : NULL;
    FILE *f; long n; char *src; ID3DXBuffer *err = NULL, *code = NULL; HRESULT hr;
    float in[512], out[512]; int i;
    IDirect3DDevice9 *dev = (IDirect3DDevice9 *)&fakeDevice;
    D3DXHANDLE ud, uv, uv2, mixed;
    void (__stdcall *stubs[4])(void) = {stub0, stub1, stub2, stub3};
    for (i = 0; i < 119; ++i) slots[i] = (void *)stubs[i % 4];
    slots[1] = (void *)addref; slots[2] = (void *)release;
    slots[94] = (void *)setF; slots[96] = (void *)setIB; slots[98] = (void *)setIB;
    if (!create || !compile || argc < 2 || !(f = fopen(argv[1], "rb"))) { printf("setup failed\n"); return 2; }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    src = (char *)malloc(n + 1); fread(src, 1, n, f); fclose(f); src[n] = 0;
    for (i = 0; i < 512; ++i) in[i] = (float)(i + 1);

    /* 1. Constant table */
    hr = compile(src, (UINT)n, NULL, NULL, "vs", "vs_2_0", 0, &code, &err, &ct);
    printf("compile hr=%08lx\n", (unsigned long)hr);
    if (FAILED(hr)) { if (err) printf("%s\n", (char *)ID3DXBuffer_GetBufferPointer(err)); return 1; }
    ud = CT->GetConstantByName(ct, NULL, "UserData"); cdesc("CT UserData", ud);
    uv = CT->GetConstant(ct, ud, 0); cdesc("CT UserData.member0", uv);
    cdesc("CT UserData.uvMatrix by name", CT->GetConstantByName(ct, NULL, "UserData.uvMatrix"));
    uv2 = CT->GetConstantElement(ct, uv, 2); cdesc("CT uvMatrix[2]", uv2);
    cdesc("CT uvMatrix[6]", CT->GetConstantElement(ct, uv, 6));
    mixed = CT->GetConstantByName(ct, NULL, "Mixed"); cdesc("CT Mixed", mixed);
    for (i = 0; i < 5; ++i) { char l[32]; sprintf(l, "CT Mixed.member%d", i); cdesc(l, CT->GetConstant(ct, mixed, i)); }
    cdesc("CT Static", CT->GetConstantByName(ct, NULL, "Static"));
    cdesc("CT Static.uvMatrix", CT->GetConstant(ct, CT->GetConstantByName(ct, NULL, "Static"), 0));
    clearWrites(); hr = CT->SetValue(ct, dev, ud, in, 7 * 64); printf("CT SetValue(UserData,448)=%08lx\n", (unsigned long)hr); showWrites("  regs");
    clearWrites(); hr = CT->SetMatrix(ct, dev, uv2, (const D3DXMATRIX *)in); printf("CT SetMatrix(uvMatrix[2])=%08lx\n", (unsigned long)hr); showWrites("  regs");
    clearWrites(); hr = CT->SetValue(ct, dev, mixed, in, 400); printf("CT SetValue(Mixed,400)=%08lx\n", (unsigned long)hr); showWrites("  regs");
    clearWrites(); hr = CT->SetValue(ct, dev, CT->GetConstantByName(ct, NULL, "Static"), in, 7 * 64);
    printf("CT SetValue(Static,448)=%08lx\n", (unsigned long)hr); showWrites("  regs");

    /* 2. Effect parameters */
    hr = create(src, (UINT)n, NULL, NULL, 0, &fx, &err);
    printf("effect hr=%08lx\n", (unsigned long)hr);
    if (FAILED(hr)) { if (err) printf("%s\n", (char *)ID3DXBuffer_GetBufferPointer(err)); return 1; }
    ud = V->GetParameterByName(fx, NULL, "UserData"); pdesc("FX UserData", ud);
    uv = V->GetParameter(fx, ud, 0); pdesc("FX member0", uv);
    pdesc("FX UserData.uvMatrix", V->GetParameterByName(fx, NULL, "UserData.uvMatrix"));
    printf("FX byname uvMatrix == member0: %d\n", V->GetParameterByName(fx, NULL, "UserData.uvMatrix") == uv);
    uv2 = V->GetParameterElement(fx, uv, 2); pdesc("FX member0 elem2", uv2);
    pdesc("FX UserData.uvMatrix[2]", V->GetParameterByName(fx, NULL, "UserData.uvMatrix[2]"));
    printf("FX byname elem2 == element: %d\n", V->GetParameterByName(fx, NULL, "UserData.uvMatrix[2]") == uv2);
    mixed = V->GetParameterByName(fx, NULL, "Mixed"); pdesc("FX Mixed", mixed);
    for (i = 0; i < 5; ++i) { char l[32]; sprintf(l, "FX Mixed.member%d", i); pdesc(l, V->GetParameter(fx, mixed, i)); }
    printf("SetMatrix(elem2)=%08lx\n", (unsigned long)V->SetMatrix(fx, uv2, (const D3DXMATRIX *)in));
    memset(out, 0, sizeof(out)); printf("GetValue(UserData,448)=%08lx\n", (unsigned long)V->GetValue(fx, ud, out, 448));
    dumpf("  elem2 words", out + 32, 16);
    printf("SetMatrixTranspose(elem2)=%08lx\n", (unsigned long)V->SetMatrixTranspose(fx, uv2, (const D3DXMATRIX *)in));
    memset(out, 0, sizeof(out)); V->GetValue(fx, ud, out, 448); dumpf("  elem2 words", out + 32, 16);
    printf("SetValue(UserData,448)=%08lx SetValue(UserData,480)=%08lx SetValue(UserData,440)=%08lx\n",
           (unsigned long)V->SetValue(fx, ud, in, 448), (unsigned long)V->SetValue(fx, ud, in, 480), (unsigned long)V->SetValue(fx, ud, in, 440));
    { D3DXMATRIX mm; memset(&mm, 0, sizeof(mm));
      printf("GetMatrix(elem2)=%08lx", (unsigned long)V->GetMatrix(fx, uv2, &mm)); dumpf("", (float *)&mm, 16);
      printf("GetMatrixTranspose(elem2)=%08lx", (unsigned long)V->GetMatrixTranspose(fx, uv2, &mm)); dumpf("", (float *)&mm, 16); }
    memset(out, 0, sizeof(out));
    printf("GetMatrixArray(member0,7)=%08lx\n", (unsigned long)V->GetMatrixArray(fx, uv, (D3DXMATRIX *)out, 7)); dumpf("  m[1]", out + 16, 16);
    printf("SetMatrixArray(member0,in+100,2)=%08lx\n", (unsigned long)V->SetMatrixArray(fx, uv, (const D3DXMATRIX *)(in + 100), 2));
    memset(out, 0, sizeof(out)); V->GetValue(fx, ud, out, 448); dumpf("  words 0..31", out, 32);
    printf("SetValue(member0,448)=%08lx SetValue(elem2,64)=%08lx SetValue(elem2,60)=%08lx\n",
           (unsigned long)V->SetValue(fx, uv, in, 448), (unsigned long)V->SetValue(fx, uv2, in + 200, 64), (unsigned long)V->SetValue(fx, uv2, in, 60));
    memset(out, 0, sizeof(out)); V->GetValue(fx, ud, out, 448); dumpf("  elem2 words", out + 32, 16);
    { float fl[32]; memset(fl, 0, sizeof(fl));
      printf("SetFloatArray(elem2,16)=%08lx GetFloatArray(elem2,16)=%08lx", (unsigned long)V->SetFloatArray(fx, uv2, in + 300, 16),
             (unsigned long)V->GetFloatArray(fx, uv2, fl, 16)); dumpf("", fl, 16); }
    /* Mixed: per-member sizes and SetMatrix on 3x3 / 4x3 / row_major / 3x4 members. */
    printf("SetValue(Mixed)=%08lx\n", (unsigned long)V->SetValue(fx, mixed, in, 400));
    for (i = 0; i < 5; ++i) {
        D3DXMATRIX mm; char l[48]; memset(&mm, 0, sizeof(mm));
        sprintf(l, "GetMatrix(Mixed.member%d)=%08lx", i, (unsigned long)V->GetMatrix(fx, V->GetParameter(fx, mixed, i), &mm));
        dumpf(l, (float *)&mm, 16);
    }
    memset(out, 0, sizeof(out)); V->SetMatrix(fx, V->GetParameter(fx, mixed, 1), (const D3DXMATRIX *)in);
    V->GetValue(fx, mixed, out, 400); dumpf("  Mixed after SetMatrix(b=4x3) words", out, 40);
    return 0;
}
