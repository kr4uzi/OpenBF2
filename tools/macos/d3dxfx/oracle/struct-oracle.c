/* [TEMP] [CHOSEN, not recovered] Prints D3DX9_25's struct-parameter semantics for structs.fx:
   descriptions, name/element/member handles, Set/GetValue byte rules and packed layouts.
   ID3DXEffectCompiler implements ID3DXBaseEffect without a device. Run under Wine. */
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <d3dx9.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef HRESULT (WINAPI *CreateCompiler)(const char *, UINT, const D3DXMACRO *, ID3DXInclude *, DWORD,
                                         ID3DXEffectCompiler **, ID3DXBuffer **);
static ID3DXEffectCompiler *fx;
#define V (fx->lpVtbl)
static void desc(const char *label, D3DXHANDLE h)
{
    D3DXPARAMETER_DESC d; HRESULT hr;
    memset(&d, 0, sizeof(d));
    if (!h) { printf("%s: null handle\n", label); return; }
    hr = V->GetParameterDesc(fx, h, &d);
    printf("%s: hr=%08lx name=%s sem=%s class=%u type=%u rows=%u cols=%u elements=%u members=%u bytes=%u\n",
           label, (unsigned long)hr, d.Name ? d.Name : "(null)", d.Semantic ? d.Semantic : "(null)",
           d.Class, d.Type, d.Rows, d.Columns, d.Elements, d.StructMembers, d.Bytes);
}
static void dumpf(const char *label, const float *f, unsigned n)
{
    unsigned i; printf("%s:", label);
    for (i = 0; i < n; ++i) printf(" %g", f[i]);
    printf("\n");
}
static D3DXHANDLE by(D3DXHANDLE parent, const char *name)
{
    D3DXHANDLE h = V->GetParameterByName(fx, parent, name);
    printf("byname(%s,%s)=%s\n", parent ? "h" : "0", name, h ? "ok" : "null");
    return h;
}
int main(int argc, char **argv)
{
    HMODULE m = LoadLibraryA("d3dx9_25.dll");
    CreateCompiler create = m ? (CreateCompiler)(void *)GetProcAddress(m, "D3DXCreateEffectCompiler") : NULL;
    FILE *f; long n; char *src; ID3DXBuffer *err = NULL; HRESULT hr;
    float in[40], out[40]; unsigned i;
    D3DXHANDLE lights, l0, l1, pos, mix;
    if (!create || argc < 2 || !(f = fopen(argv[1], "rb"))) { printf("setup failed\n"); return 2; }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    src = (char *)malloc(n + 1); fread(src, 1, n, f); fclose(f); src[n] = 0;
    hr = create(src, (UINT)n, NULL, NULL, 0, &fx, &err);
    printf("create hr=%08lx\n", (unsigned long)hr);
    if (FAILED(hr)) { if (err) printf("%s\n", (char *)ID3DXBuffer_GetBufferPointer(err)); return 1; }
    for (i = 0; i < 40; ++i) in[i] = (float)(i + 1);

    lights = by(NULL, "Lights"); desc("Lights", lights);
    l0 = V->GetParameterElement(fx, lights, 0); desc("Lights elem0", l0);
    l1 = by(NULL, "Lights[1]"); desc("Lights[1]", l1);
    desc("member(elem0,0)", V->GetParameter(fx, l0, 0));
    desc("member(elem0,4)", V->GetParameter(fx, l0, 4));
    printf("member(elem0,5)=%s\n", V->GetParameter(fx, l0, 5) ? "ok" : "null");
    printf("member(Lights,0)=%s\n", V->GetParameter(fx, lights, 0) ? "ok" : "null");
    pos = by(NULL, "Lights[0].pos"); desc("Lights[0].pos", pos);
    desc("Lights[1].color", by(NULL, "Lights[1].color"));
    desc("byname(elem0,dir)", by(l0, "dir"));
    by(NULL, "Lights.pos"); by(lights, "pos"); by(NULL, "One.pos"); by(NULL, "One[0]"); by(NULL, "Lights[2]");
    desc("One", by(NULL, "One")); desc("One.attenuation", by(NULL, "One.attenuation"));
    printf("elem(One,0)=%s\n", V->GetParameterElement(fx, by(NULL, "One"), 0) ? "ok" : "null");
    mix = by(NULL, "Mix"); desc("Mix", mix);
    desc("Mix.n", by(NULL, "Mix.n")); desc("Mix.b", by(NULL, "Mix.b"));

    /* Byte rules: exact, larger and smaller sizes on struct arrays, elements, members, vectors. */
    printf("SetValue(Lights,120)=%08lx\n", (unsigned long)V->SetValue(fx, lights, in, 120));
    memset(out, 0, sizeof(out));
    printf("GetValue(Lights,120)=%08lx\n", (unsigned long)V->GetValue(fx, lights, out, 120)); dumpf("  Lights", out, 30);
    printf("SetValue(Lights,136)=%08lx\n", (unsigned long)V->SetValue(fx, lights, in + 1, 136));
    memset(out, 0, sizeof(out)); V->GetValue(fx, lights, out, 120); dumpf("  after136", out, 30);
    printf("SetValue(Lights,116)=%08lx\n", (unsigned long)V->SetValue(fx, lights, in + 2, 116));
    memset(out, 0, sizeof(out)); V->GetValue(fx, lights, out, 120); dumpf("  after116", out, 30);
    printf("GetValue(Lights,116)=%08lx GetValue(Lights,124)=%08lx\n",
           (unsigned long)V->GetValue(fx, lights, out, 116), (unsigned long)V->GetValue(fx, lights, out, 124));
    printf("SetValue(Lights[1],68)=%08lx\n", (unsigned long)V->SetValue(fx, l1, in + 20, 68));
    memset(out, 0, sizeof(out)); V->GetValue(fx, lights, out, 120); dumpf("  afterElem1", out, 30);
    printf("SetValue(Lights[0],60)=%08lx\n", (unsigned long)V->SetValue(fx, l0, in, 60));
    printf("SetValue(pos,16)=%08lx SetValue(pos,8)=%08lx\n", (unsigned long)V->SetValue(fx, pos, in + 30, 16),
           (unsigned long)V->SetValue(fx, pos, in + 33, 8));
    memset(out, 0, sizeof(out)); V->GetValue(fx, l0, out, 60); dumpf("  elem0", out, 15);
    { D3DXVECTOR4 v = {101, 102, 103, 104};
      printf("SetVector(pos)=%08lx\n", (unsigned long)V->SetVector(fx, pos, &v)); }
    printf("SetFloat(Lights[0].attenuation)=%08lx\n", (unsigned long)V->SetFloat(fx, by(NULL, "Lights[0].attenuation"), 7.5f));
    printf("SetFloat(Lights)=%08lx SetFloat(Lights[0])=%08lx\n", (unsigned long)V->SetFloat(fx, lights, 9),
           (unsigned long)V->SetFloat(fx, l0, 9));
    memset(out, 0, sizeof(out));
    printf("GetFloatArray(Lights,30)=%08lx\n", (unsigned long)V->GetFloatArray(fx, lights, out, 30)); dumpf("  floats", out, 30);
    printf("SetFloatArray(Lights[0],15)=%08lx\n", (unsigned long)V->SetFloatArray(fx, l0, in, 15));
    printf("SetFloatArray(Lights[0],20)=%08lx\n", (unsigned long)V->SetFloatArray(fx, l0, in, 20));
    printf("SetFloatArray(Lights,4)=%08lx\n", (unsigned long)V->SetFloatArray(fx, lights, in + 10, 4));
    memset(out, 0, sizeof(out)); V->GetValue(fx, lights, out, 120); dumpf("  afterFA", out, 30);
    { D3DXVECTOR4 v; memset(&v, 0, sizeof(v));
      printf("GetVector(Lights[1].color)=%08lx", (unsigned long)V->GetVector(fx, by(NULL, "Lights[1].color"), &v));
      printf(" %g %g %g %g\n", v.x, v.y, v.z, v.w); }
    printf("SetValue(Mix,20)=%08lx\n", (unsigned long)V->SetValue(fx, mix, in, 20));
    { int iv = 0; BOOL bv = 0; float fv = 0;
      V->GetInt(fx, by(NULL, "Mix.n"), &iv); V->GetBool(fx, by(NULL, "Mix.b"), &bv); V->GetFloat(fx, by(NULL, "Mix.s"), &fv);
      printf("  Mix.n=%d Mix.b=%d Mix.s=%g\n", iv, bv, fv);
      memset(out, 0, sizeof(out)); V->GetValue(fx, mix, out, 20);
      printf("  Mix words: %08lx %08lx %08lx %08lx %08lx\n", ((unsigned long *)out)[0], ((unsigned long *)out)[1],
             ((unsigned long *)out)[2], ((unsigned long *)out)[3], ((unsigned long *)out)[4]); }
    printf("SetInt(Mix.n,5)=%08lx", (unsigned long)V->SetInt(fx, by(NULL, "Mix.n"), 5));
    { int iv = 0; V->GetInt(fx, by(NULL, "Mix.n"), &iv); printf(" -> %d\n", iv); }

    /* Non-struct numeric byte rules for comparison. */
    printf("SetValue(Vec3,12)=%08lx SetValue(Vec3,16)=%08lx SetValue(Vec3,8)=%08lx\n",
           (unsigned long)V->SetValue(fx, by(NULL, "Vec3"), in, 12), (unsigned long)V->SetValue(fx, by(NULL, "Vec3"), in + 3, 16),
           (unsigned long)V->SetValue(fx, by(NULL, "Vec3"), in + 6, 8));
    memset(out, 0, sizeof(out)); V->GetValue(fx, by(NULL, "Vec3"), out, 12); dumpf("  Vec3", out, 3);
    printf("GetValue(Vec3,8)=%08lx GetValue(Vec3,16)=%08lx\n",
           (unsigned long)V->GetValue(fx, by(NULL, "Vec3"), out, 8), (unsigned long)V->GetValue(fx, by(NULL, "Vec3"), out, 16));
    printf("SetValue(Vec4,0 bytes)=%08lx SetValue(Vec4,null,0)=%08lx\n",
           (unsigned long)V->SetValue(fx, by(NULL, "Vec4"), in, 0), (unsigned long)V->SetValue(fx, by(NULL, "Vec4"), NULL, 0));
    return 0;
}
