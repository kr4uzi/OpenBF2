// [TEMP] [CHOSEN, not recovered] Contract checks through the production backend.
#include "../../BF2/Platform/Gfx/Metal/MetalBridge.h"
#include <vector>
#include <algorithm>
#include <utility>

// Only models the ownership of an already-created shader; G4 shader compilation stays absent.
template<class Interface> struct ShaderReferenceFixture : Interface {
    ULONG refs=1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override { std::abort(); }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9**) override { std::abort(); }
    HRESULT STDMETHODCALLTYPE GetFunction(void*,UINT*) override { std::abort(); }
};

static int cpuChecks() {
    bf2metal::RawState s; bf2metal::StateMapping m;
    bf2metal::defaultState(s,true,320,240);
    CHECK(s.render[7]==1 && s.render[8]==3 && s.render[14]==1 && s.render[19]==2 && s.render[20]==1);
    CHECK(s.render[25]==8 && s.render[34]==0 && s.render[37]==0x3f800000 && s.render[182]==0x3f800000);
    CHECK(s.render[58]==0xffffffff && s.render[168]==15 && s.render[161]==1 && s.render[193]==0xffffffff);
    CHECK(s.stage[0][1]==4 && s.stage[1][1]==1 && s.stage[0][4]==2 && s.stage[7][11]==7);
    for(unsigned i=0;i<21;++i) CHECK(s.sampler[i][1]==1 && s.sampler[i][6]==1 && s.sampler[i][7]==0 && s.sampler[i][10]==1);
    CHECK(!bf2metal::mapState(s,m));
    CHECK(m.depthWrite && m.depthCompare==3 && m.cull==2 && m.srcRGB==1 && m.dstRGB==0 && !m.alphaTest);
    CHECK(m.sampler[0].address[0]==2 && m.sampler[0].minFilter==0 && m.sampler[0].mipFilter==0);
    s.render[27]=1; s.render[19]=5; s.render[20]=6; s.render[15]=1; s.render[25]=5; s.render[24]=128;
    s.render[185]=1; s.render[53]=4; s.render[186]=8;
    s.sampler[0][1]=2; s.sampler[0][2]=3; s.sampler[0][3]=5; s.sampler[0][6]=3; s.sampler[0][10]=16;
    CHECK(!bf2metal::mapState(s,m));
    CHECK(m.blend && m.srcRGB==4 && m.dstRGB==5 && m.alphaTest && m.alphaCompare==4);
    CHECK(m.front.fail==3 && m.back.fail==7 && m.sampler[0].address[0]==3 && m.sampler[0].address[1]==0 && m.sampler[0].address[2]==1);
    CHECK(m.sampler[0].anisotropy==16 && m.sampler[0].minFilter==1);
    for(unsigned i=1;i<=8;++i) {
        s.render[23]=i; s.render[53]=i; CHECK(!bf2metal::mapState(s,m)); CHECK(m.depthCompare==i-1 && m.front.fail==i-1);
    }
    s.render[8]=1; CHECK(bf2metal::mapState(s,m)); s.render[8]=3;
    s.render[19]=99; CHECK(bf2metal::mapState(s,m));
    bf2metal::defaultState(s,false,320,240); CHECK(s.render[7]==0 && !bf2metal::mapState(s,m) && !m.depthWrite);
    const unsigned char source[8]={0,0,0,255,255,255,255,255}; unsigned char result[12]={};
    CHECK(bf2metal::resample(D3DFMT_A8R8G8B8,source,2,1,8,result,3,1,12,true));
    CHECK(result[0]==0 && result[4]==128 && result[8]==255 && result[7]==255);
    std::puts("PASS: CPU D3D defaults, Metal mappings, unsupported values, bilinear packed-color copy"); return 0;
}

static int stateChecks(IDirect3DDevice9* d) {
    for(const auto& p:std::initializer_list<std::pair<unsigned,DWORD>>{{7,1},{8,3},{14,1},{15,0},{19,2},{20,1},
        {22,3},{23,4},{24,0},{25,8},{27,0},{52,0},{53,1},{54,1},{55,1},{56,8},{57,0},{58,0xffffffff},
        {59,0xffffffff},{60,0xffffffff},{128,0},{135,0},{152,0},{161,1},{168,15},{171,1},{174,0},{175,0},
        {185,0},{186,1},{187,1},{188,1},{189,8},{190,15},{191,15},{192,15},{193,0xffffffff},{195,0}}) {
        DWORD v=99; const auto state=static_cast<D3DRENDERSTATETYPE>(p.first);
        CHECK(d->GetRenderState(state,&v)==D3D_OK && v==p.second);
        CHECK(d->SetRenderState(state,p.second^0x1234)==D3D_OK);
        CHECK(d->GetRenderState(state,&v)==D3D_OK && v==(p.second^0x1234));
        CHECK(d->SetRenderState(state,p.second)==D3D_OK);
    }
    for(unsigned stage: {0u,7u,15u,256u,260u}) for(unsigned i=1;i<14;++i) {
        DWORD v=99; const auto type=static_cast<D3DSAMPLERSTATETYPE>(i);
        const DWORD expected=(i==1 || i==2 || i==3 || i==5 || i==6 || i==10)?1:0;
        CHECK(d->GetSamplerState(stage,type,&v)==D3D_OK && v==expected);
        CHECK(d->SetSamplerState(stage,type,0xabcdef)==D3D_OK);
        CHECK(d->GetSamplerState(stage,type,&v)==D3D_OK && v==0xabcdef);
        CHECK(d->SetSamplerState(stage,type,expected)==D3D_OK);
    }
    for(unsigned i=0;i<8;++i) {
        DWORD v=99; CHECK(d->GetTextureStageState(i,D3DTSS_TEXTURETRANSFORMFLAGS,&v)==D3D_OK && v==0);
        CHECK(d->SetTextureStageState(i,D3DTSS_TEXTURETRANSFORMFLAGS,256)==D3D_OK);
        CHECK(d->GetTextureStageState(i,D3DTSS_TEXTURETRANSFORMFLAGS,&v)==D3D_OK && v==256);
        CHECK(d->SetTextureStageState(i,D3DTSS_TEXTURETRANSFORMFLAGS,0)==D3D_OK);
    }
    float floats[8]={1,2,3,4,5,6,7,8}, outf[8]={}; int ints[8]={1,-2,3,-4,5,6,7,8}, outi[8]={};
    BOOL booleans[2]={-1,0}, outb[2]={};
    CHECK(d->SetVertexShaderConstantF(254,floats,2)==D3D_OK && d->GetVertexShaderConstantF(254,outf,2)==D3D_OK && !std::memcmp(floats,outf,sizeof(floats)));
    CHECK(d->SetPixelShaderConstantF(222,floats,2)==D3D_OK && d->GetPixelShaderConstantF(222,outf,2)==D3D_OK && !std::memcmp(floats,outf,sizeof(floats)));
    CHECK(d->SetVertexShaderConstantI(14,ints,2)==D3D_OK && d->GetVertexShaderConstantI(14,outi,2)==D3D_OK && !std::memcmp(ints,outi,sizeof(ints)));
    CHECK(d->SetPixelShaderConstantI(14,ints,2)==D3D_OK && d->GetPixelShaderConstantI(14,outi,2)==D3D_OK && !std::memcmp(ints,outi,sizeof(ints)));
    CHECK(d->SetVertexShaderConstantB(14,booleans,2)==D3D_OK && d->GetVertexShaderConstantB(14,outb,2)==D3D_OK && outb[0]==TRUE && outb[1]==FALSE);
    CHECK(d->SetPixelShaderConstantB(14,booleans,2)==D3D_OK && d->GetPixelShaderConstantB(14,outb,2)==D3D_OK && outb[0]==TRUE && outb[1]==FALSE);
    CHECK(d->SetVertexShaderConstantF(255,floats,2)==D3DERR_INVALIDCALL && d->SetPixelShaderConstantI(16,ints,1)==D3DERR_INVALIDCALL);
    CHECK(d->SetPixelShaderConstantB(0,nullptr,1)==D3DERR_INVALIDCALL);
    CHECK(d->SetClipPlane(5,floats)==D3D_OK && d->GetClipPlane(5,outf)==D3D_OK && !std::memcmp(floats,outf,16));
    RECT r={1,2,100,120}, out={}; CHECK(d->SetScissorRect(&r)==D3D_OK && d->GetScissorRect(&out)==D3D_OK && !std::memcmp(&r,&out,sizeof(r)));
    CHECK(d->SetVertexShader(nullptr)==D3D_OK && d->SetPixelShader(nullptr)==D3D_OK);
    ShaderReferenceFixture<IDirect3DVertexShader9> vs; ShaderReferenceFixture<IDirect3DPixelShader9> ps;
    CHECK(d->SetVertexShader(&vs)==D3D_OK && d->SetPixelShader(&ps)==D3D_OK && vs.refs==2 && ps.refs==2);
    IDirect3DVertexShader9* gotVS=nullptr; IDirect3DPixelShader9* gotPS=nullptr;
    CHECK(d->GetVertexShader(&gotVS)==D3D_OK && gotVS==&vs && gotVS->Release()==2);
    CHECK(d->GetPixelShader(&gotPS)==D3D_OK && gotPS==&ps && gotPS->Release()==2);
    CHECK(d->SetVertexShader(nullptr)==D3D_OK && d->SetPixelShader(nullptr)==D3D_OK && vs.refs==1 && ps.refs==1);
    bf2metal::RawState raw; bf2metal::StateMapping mapped; const char* refusal=nullptr;
    CHECK(bf2metal::inspectState(d,raw,mapped,&refusal));
    CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(19),0xdeadbeef)==D3D_OK);
    CHECK(!bf2metal::inspectState(d,raw,mapped,&refusal) && std::strstr(refusal,"blend factor"));
    CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(19),2)==D3D_OK);
    CHECK(bf2metal::inspectState(d,raw,mapped,&refusal));
    std::puts("PASS: state defaults/round trips, all constant banks, clip/scissor, deferred refusal"); return 0;
}

static int declarationChecks(IDirect3DDevice9* d) {
    const D3DVERTEXELEMENT9 input[]={
        {0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},
        {1,0,D3DDECLTYPE_SHORT2,0,D3DDECLUSAGE_NORMAL,0},
        {1,4,D3DDECLTYPE_SHORT4,0,D3DDECLUSAGE_TANGENT,0},
        {1,12,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},
        {2,0,D3DDECLTYPE_FLOAT1,0,D3DDECLUSAGE_TEXCOORD,1},
        {2,4,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_BINORMAL,0},D3DDECL_END()};
    const unsigned expected[]={30,29,16,18,42,28,31};
    IDirect3DVertexDeclaration9 *decl=nullptr,*again=nullptr;
    CHECK(d->CreateVertexDeclaration(input,&decl)==D3D_OK && decl);
    UINT count=0; CHECK(decl->GetDeclaration(nullptr,&count)==D3D_OK && count==8);
    D3DVERTEXELEMENT9 output[8]={}; CHECK(decl->GetDeclaration(output,&count)==D3D_OK && !std::memcmp(input,output,sizeof(input)));
    for(unsigned i=0;i<7;++i) {
        bf2metal::VertexElement e={}; unsigned f=0; CHECK(bf2metal::inspectDeclaration(decl,i,e,f));
        CHECK(f==expected[i] && e.stream==input[i].Stream && e.offset==input[i].Offset && e.usage==input[i].Usage && e.index==input[i].UsageIndex);
    }
    CHECK(d->SetVertexDeclaration(decl)==D3D_OK && decl->Release()==0); // binding owns it internally
    CHECK(d->GetVertexDeclaration(&again)==D3D_OK && again==decl);
    CHECK(again->AddRef()==2 && again->Release()==1 && again->Release()==0);
    CHECK(d->SetVertexDeclaration(nullptr)==D3D_OK);
    std::puts("PASS: vertex descriptor types/semantics/offsets and declaration lifetime"); return 0;
}

static std::vector<unsigned char> pattern(D3DFORMAT f,unsigned pixels) {
    const unsigned size=bf2metal::formatBytes(f); std::vector<unsigned char> v(pixels*size);
    for(unsigned i=0;i<v.size();++i) v[i]=static_cast<unsigned char>(i*37+11);
    for(unsigned i=0;i<pixels;++i) {
        auto* p=v.data()+i*size;
        if(f==D3DFMT_X8R8G8B8 || f==D3DFMT_X8B8G8R8) p[3]=255;
        if(f==D3DFMT_X1R5G5B5) p[1]|=0x80;
        if(f==D3DFMT_X4R4G4B4) p[1]|=0xf0;
        if(f==D3DFMT_R16F || f==D3DFMT_G16R16F || f==D3DFMT_A16B16G16R16F)
            for(unsigned c=0;c<size/2;++c) { const WORD h=WORD(0x3000+c*0x400); std::memcpy(p+c*2,&h,2); }
        if(f==D3DFMT_R32F || f==D3DFMT_G32R32F || f==D3DFMT_A32B32G32R32F)
            for(unsigned c=0;c<size/4;++c) { const float value=(c+1)*0.125f; std::memcpy(p+c*4,&value,4); }
    }
    return v;
}
static int cpuFormatChecks() {
    for(auto f:{D3DFMT_R5G6B5,D3DFMT_A1R5G5B5,D3DFMT_A4R4G4B4}) {
        std::vector<WORD> source(65536),dest(65536);
        for(unsigned i=0;i<65536;++i) source[i]=WORD(i);
        CHECK(bf2metal::resample(f,source.data(),65536,1,131072,dest.data(),65536,1,131072,true));
        CHECK(source==dest);
    }
    for(auto f:{D3DFMT_R8G8B8,D3DFMT_A8R8G8B8,D3DFMT_X8R8G8B8,D3DFMT_A8,D3DFMT_L8,D3DFMT_A8L8,
        D3DFMT_A2B10G10R10,D3DFMT_A2R10G10B10,D3DFMT_A8B8G8R8,D3DFMT_X8B8G8R8,D3DFMT_G16R16,
        D3DFMT_A16B16G16R16,D3DFMT_L16,D3DFMT_R16F,D3DFMT_G16R16F,D3DFMT_A16B16G16R16F,D3DFMT_R32F,
        D3DFMT_G32R32F,D3DFMT_A32B32G32R32F}) {
        auto data=pattern(f,16); std::vector<unsigned char> dest(data.size());
        CHECK(bf2metal::resample(f,data.data(),16,1,unsigned(data.size()),dest.data(),16,1,unsigned(dest.size()),true));
        CHECK(data==dest);
    }
    std::puts("PASS: exhaustive packed-16 conversion and integer/float/luminance/alpha CPU round trips"); return 0;
}
static int textureChecks(IDirect3DDevice9* d) {
    const D3DFORMAT formats[]={D3DFMT_R8G8B8,D3DFMT_A8R8G8B8,D3DFMT_X8R8G8B8,D3DFMT_R5G6B5,
        D3DFMT_X1R5G5B5,D3DFMT_A1R5G5B5,D3DFMT_A4R4G4B4,D3DFMT_X4R4G4B4,D3DFMT_A8,D3DFMT_L8,D3DFMT_A8L8,
        D3DFMT_A2B10G10R10,D3DFMT_A2R10G10B10,D3DFMT_A8B8G8R8,D3DFMT_X8B8G8R8,D3DFMT_G16R16,D3DFMT_A16B16G16R16,
        D3DFMT_L16,D3DFMT_R16F,D3DFMT_G16R16F,D3DFMT_A16B16G16R16F,D3DFMT_R32F,D3DFMT_G32R32F,D3DFMT_A32B32G32R32F};
    for(D3DFORMAT f:formats) {
        const unsigned row=4*bf2metal::formatBytes(f); auto data=pattern(f,16);
        IDirect3DTexture9* t=nullptr; IDirect3DSurface9 *s=nullptr,*again=nullptr,*cpu=nullptr;
        CHECK(d->CreateTexture(4,4,1,D3DUSAGE_RENDERTARGET,f,D3DPOOL_DEFAULT,&t,nullptr)==D3D_OK);
        CHECK(t->GetSurfaceLevel(0,&s)==D3D_OK && t->GetSurfaceLevel(0,&again)==D3D_OK && s==again && again->Release()==1);
        D3DSURFACE_DESC desc={}; CHECK(t->GetLevelDesc(0,&desc)==D3D_OK && desc.Format==f && desc.Width==4);
        D3DLOCKED_RECT lock={}; CHECK(t->LockRect(0,&lock,nullptr,0)==D3D_OK && unsigned(lock.Pitch)==row);
        for(unsigned y=0;y<4;++y) std::memcpy(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,data.data()+y*row,row);
        CHECK(s->LockRect(&lock,nullptr,0)==D3DERR_INVALIDCALL); // same lock state through the surface view
        CHECK(t->UnlockRect(0)==D3D_OK && t->UnlockRect(0)==D3DERR_INVALIDCALL);
        CHECK(d->CreateOffscreenPlainSurface(4,4,f,D3DPOOL_SYSTEMMEM,&cpu,nullptr)==D3D_OK);
        CHECK(d->GetRenderTargetData(s,cpu)==D3D_OK && cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY)==D3D_OK);
        for(unsigned y=0;y<4;++y) CHECK(!std::memcmp(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,data.data()+y*row,row));
        CHECK(cpu->UnlockRect()==D3D_OK);
        CHECK(d->UpdateSurface(cpu,nullptr,s,nullptr)==D3D_OK);
        CHECK(t->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)==D3D_OK && !std::memcmp(lock.pBits,data.data(),data.size()));
        CHECK(t->UnlockRect(0)==D3D_OK);
        // A surface keeps its texture alive and QueryInterface returns the same container.
        const GUID textureIID={0x85c31227,0x3de5,0x4f00,{0x9b,0x3a,0xf1,0x1a,0xc3,0x8c,0x18,0xb5}};
        CHECK(t->Release()==0); IDirect3DTexture9* container=nullptr;
        CHECK(s->GetContainer(textureIID,reinterpret_cast<void**>(&container))==D3D_OK && container==t);
        CHECK(container->Release()==0 && s->Release()==0 && cpu->Release()==0);
        std::printf("PASS: texture upload/Lock/RT readback/UpdateSurface format %u\n",unsigned(f));
    }
    for(auto f:{D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5}) {
        IDirect3DTexture9* t=nullptr; CHECK(d->CreateTexture(8,8,0,0,f,D3DPOOL_MANAGED,&t,nullptr)==D3D_OK && t->GetLevelCount()==4);
        for(unsigned l=0;l<4;++l) {
            const unsigned w=std::max(1u,8u>>l), blocks=(w+3)/4, row=blocks*bf2metal::formatBytes(f);
            std::vector<unsigned char> expected(row*blocks,static_cast<unsigned char>(0x21+l)); D3DLOCKED_RECT lock={};
            CHECK(t->LockRect(l,&lock,nullptr,0)==D3D_OK && unsigned(lock.Pitch)==row);
            std::memcpy(lock.pBits,expected.data(),expected.size()); CHECK(t->UnlockRect(l)==D3D_OK);
            CHECK(t->LockRect(l,&lock,nullptr,D3DLOCK_READONLY)==D3D_OK && !std::memcmp(lock.pBits,expected.data(),expected.size()));
            CHECK(t->UnlockRect(l)==D3D_OK);
        }
        CHECK(t->SetPriority(5)==0); t->PreLoad(); CHECK(t->Release()==0);
    }
    IDirect3DCubeTexture9* cube=nullptr; CHECK(d->CreateCubeTexture(4,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&cube,nullptr)==D3D_OK);
    for(unsigned face=0;face<6;++face) for(unsigned level=0;level<3;++level) {
        D3DLOCKED_RECT lock={}; CHECK(cube->LockRect(static_cast<D3DCUBEMAP_FACES>(face),level,&lock,nullptr,0)==D3D_OK);
        const unsigned n=std::max(1u,4u>>level); std::memset(lock.pBits,int(31+face*3+level),lock.Pitch*n);
        CHECK(cube->UnlockRect(static_cast<D3DCUBEMAP_FACES>(face),level)==D3D_OK);
        IDirect3DSurface9* s=nullptr; CHECK(cube->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(face),level,&s)==D3D_OK);
        CHECK(s->LockRect(&lock,nullptr,D3DLOCK_READONLY)==D3D_OK && static_cast<unsigned char*>(lock.pBits)[0]==31+face*3+level);
        CHECK(s->UnlockRect()==D3D_OK && s->Release()==0);
    }
    CHECK(cube->Release()==0);
    IDirect3DVolumeTexture9* volume=nullptr; CHECK(d->CreateVolumeTexture(4,4,4,0,0,D3DFMT_L8,D3DPOOL_MANAGED,&volume,nullptr)==D3D_OK);
    for(unsigned l=0;l<3;++l) {
        D3DVOLUME_DESC desc={}; CHECK(volume->GetLevelDesc(l,&desc)==D3D_OK && desc.Depth==std::max(1u,4u>>l));
        D3DLOCKED_BOX box={}; CHECK(volume->LockBox(l,&box,nullptr,0)==D3D_OK);
        std::memset(box.pBits,int(70+l),box.SlicePitch*desc.Depth); CHECK(volume->UnlockBox(l)==D3D_OK);
        CHECK(volume->LockBox(l,&box,nullptr,D3DLOCK_READONLY)==D3D_OK);
        const auto* p=static_cast<const unsigned char*>(box.pBits);
        CHECK(p[0]==70+l && p[box.SlicePitch*desc.Depth-1]==70+l && volume->UnlockBox(l)==D3D_OK);
    }
    CHECK(volume->Release()==0);
    IDirect3DTexture9* dynamic=nullptr; D3DLOCKED_RECT rect={};
    CHECK(d->CreateTexture(4,4,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&dynamic,nullptr)==D3D_OK);
    CHECK(dynamic->LockRect(0,&rect,nullptr,D3DLOCK_DISCARD)==D3D_OK); std::memset(rect.pBits,42,rect.Pitch*4);
    CHECK(dynamic->UnlockRect(0)==D3D_OK && dynamic->LockRect(0,&rect,nullptr,D3DLOCK_READONLY)==D3D_OK && static_cast<unsigned char*>(rect.pBits)[0]==42);
    CHECK(dynamic->UnlockRect(0)==D3D_OK && dynamic->Release()==0);
    IDirect3DTexture9* bound=nullptr; IDirect3DBaseTexture9* getter=nullptr;
    CHECK(d->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&bound,nullptr)==D3D_OK);
    CHECK(d->SetTexture(0,bound)==D3D_OK && bound->Release()==0);
    CHECK(d->GetTexture(0,&getter)==D3D_OK && getter==bound && getter->GetLevelCount()==1);
    CHECK(d->SetTexture(0,nullptr)==D3D_OK && getter->GetType()==D3DRTYPE_TEXTURE && getter->Release()==0);
    CHECK(d->SetTexture(16,nullptr)==D3DERR_INVALIDCALL);
    std::puts("PASS: BC1/2/3 block mips, all cube faces/mips, 3D upload/readback, DISCARD texture"); return 0;
}

static int bufferChecks(IDirect3DDevice9* d) {
    IDirect3DVertexBuffer9* v=nullptr; void* bytes=nullptr;
    CHECK(d->CreateVertexBuffer(64,D3DUSAGE_DYNAMIC,0,D3DPOOL_DEFAULT,&v,nullptr)==D3D_OK);
    CHECK(v->Lock(0,32,&bytes,D3DLOCK_DISCARD)==D3D_OK); std::memset(bytes,0x11,32); CHECK(v->Unlock()==D3D_OK);
    CHECK(v->Lock(32,32,&bytes,D3DLOCK_NOOVERWRITE)==D3D_OK); std::memset(bytes,0x22,32); CHECK(v->Unlock()==D3D_OK);
    CHECK(v->Lock(0,0,&bytes,D3DLOCK_READONLY)==D3D_OK);
    CHECK(static_cast<unsigned char*>(bytes)[0]==0x11 && static_cast<unsigned char*>(bytes)[63]==0x22 && v->Unlock()==D3D_OK);
    CHECK(v->Unlock()==D3DERR_INVALIDCALL && v->Lock(60,8,&bytes,0)==D3DERR_INVALIDCALL);
    CHECK(v->Lock(0,64,&bytes,D3DLOCK_READONLY|D3DLOCK_DISCARD)==D3DERR_INVALIDCALL);
    D3DVERTEXBUFFER_DESC desc={}; CHECK(v->GetDesc(&desc)==D3D_OK && desc.Size==64);
    CHECK(d->SetStreamSource(0,v,4,16)==D3D_OK && v->Release()==0);
    IDirect3DVertexBuffer9* again=nullptr; UINT offset=0,stride=0;
    CHECK(d->GetStreamSource(0,&again,&offset,&stride)==D3D_OK && again==v && offset==4 && stride==16 && again->Release()==0);
    CHECK(d->SetStreamSource(0,nullptr,0,0)==D3D_OK);
    IDirect3DIndexBuffer9* index=nullptr;
    CHECK(d->CreateIndexBuffer(32,0,D3DFMT_INDEX16,D3DPOOL_MANAGED,&index,nullptr)==D3D_OK);
    // Review: Windows ignores DISCARD on a static buffer (see review.inc); no error.
    CHECK(index->Lock(0,0,&bytes,0)==D3D_OK); std::memset(bytes,0x23,32); CHECK(index->Unlock()==D3D_OK);
    CHECK(d->SetIndices(index)==D3D_OK && index->Release()==0);
    CHECK(d->GetIndices(&index)==D3D_OK && index->Lock(0,0,&bytes,D3DLOCK_READONLY)==D3D_OK && static_cast<unsigned char*>(bytes)[31]==0x23);
    CHECK(index->Unlock()==D3D_OK && index->Release()==0 && d->SetIndices(nullptr)==D3D_OK);
    std::puts("PASS: dynamic DISCARD/NOOVERWRITE/READONLY, bounds, stream/index binding lifetimes"); return 0;
}

static int copyChecks(IDirect3DDevice9* d) {
    IDirect3DSurface9 *cpu=nullptr,*source=nullptr,*target=nullptr,*readback=nullptr;
    CHECK(d->CreateOffscreenPlainSurface(2,2,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr)==D3D_OK);
    CHECK(d->CreateRenderTarget(2,2,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&source,nullptr)==D3D_OK);
    CHECK(d->CreateRenderTarget(4,4,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr)==D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(4,4,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr)==D3D_OK);
    D3DLOCKED_RECT lock={}; CHECK(cpu->LockRect(&lock,nullptr,0)==D3D_OK);
    const DWORD line[2]={0xff000000u,0xffffffffu};
    for(unsigned y=0;y<2;++y) std::memcpy(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,line,8);
    CHECK(cpu->UnlockRect()==D3D_OK && d->UpdateSurface(cpu,nullptr,source,nullptr)==D3D_OK);
    for(auto filter:{D3DTEXF_POINT,D3DTEXF_LINEAR}) {
        CHECK(d->StretchRect(source,nullptr,target,nullptr,filter)==D3D_OK);
        CHECK(d->GetRenderTargetData(target,readback)==D3D_OK && readback->LockRect(&lock,nullptr,D3DLOCK_READONLY)==D3D_OK);
        for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
            const unsigned expected=filter==D3DTEXF_POINT?(x<2?0:255):(x==0?0:x==1?64:x==2?191:255);
            const auto* p=static_cast<const unsigned char*>(lock.pBits)+y*lock.Pitch+x*4;
            CHECK(p[0]==expected && p[1]==expected && p[2]==expected && p[3]==255);
        }
        CHECK(readback->UnlockRect()==D3D_OK);
    }
    RECT region={0,0,1,2}; POINT position={1,1};
    CHECK(d->UpdateSurface(cpu,&region,target,&position)==D3D_OK && d->GetRenderTargetData(target,readback)==D3D_OK);
    CHECK(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY)==D3D_OK);
    CHECK(static_cast<unsigned char*>(lock.pBits)[lock.Pitch+4]==0 && static_cast<unsigned char*>(lock.pBits)[4]==64);
    CHECK(readback->UnlockRect()==D3D_OK);
    CHECK(cpu->Release()==0 && source->Release()==0 && target->Release()==0 && readback->Release()==0);
    std::puts("PASS: point/linear StretchRect, UpdateSurface subrect/offset preserves surrounding pixels"); return 0;
}

static int targetQueryChecks(IDirect3DDevice9* d) {
    IDirect3DSurface9 *original=nullptr,*oldDepth=nullptr,*rt=nullptr,*depth=nullptr,*cpu=nullptr;
    CHECK(d->GetRenderTarget(0,&original)==D3D_OK && d->GetDepthStencilSurface(&oldDepth)==D3D_OK);
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&rt,nullptr)==D3D_OK);
    CHECK(d->CreateDepthStencilSurface(8,8,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&depth,nullptr)==D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(8,8,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr)==D3D_OK);
    CHECK(d->SetRenderTarget(0,rt)==D3D_OK && d->SetDepthStencilSurface(depth)==D3D_OK);
    CHECK(rt->Release()==0 && depth->Release()==0); // device bindings keep both live
    IDirect3DSurface9* again=nullptr; CHECK(d->GetRenderTarget(0,&again)==D3D_OK && again==rt && again->Release()==0);
    D3DVIEWPORT9 viewport={}; CHECK(d->GetViewport(&viewport)==D3D_OK && viewport.Width==8 && viewport.Height==8);
    RECT scissor={}; CHECK(d->GetScissorRect(&scissor)==D3D_OK && scissor.left==0 && scissor.top==0 && scissor.right==8 && scissor.bottom==8);
    CHECK(d->Clear(0,nullptr,7,0xff204080,0.25f,37)==D3D_OK && d->GetRenderTargetData(rt,cpu)==D3D_OK);
    D3DLOCKED_RECT lock={}; CHECK(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY)==D3D_OK);
    DWORD pixel=0; std::memcpy(&pixel,lock.pBits,4); CHECK(pixel==0xff204080 && cpu->UnlockRect()==D3D_OK);
    CHECK(depth->LockRect(&lock,nullptr,0)==D3DERR_INVALIDCALL);
    DWORD depths[64]={}; CHECK(bf2metal::inspectSurface(depth,depths,sizeof(depths)));
    for(DWORD value:depths) CHECK(value==0x40000025u); // Quantized .25 depth and stencil 37.
    CHECK(d->SetRenderTarget(0,original)==D3D_OK && d->SetDepthStencilSurface(oldDepth)==D3D_OK);
    CHECK(original->Release()==0 && oldDepth->Release()==0 && cpu->Release()==0);
    for(auto type:{D3DQUERYTYPE_EVENT,D3DQUERYTYPE_OCCLUSION}) {
        IDirect3DQuery9* q=nullptr; CHECK(d->CreateQuery(type,nullptr)==D3D_OK && d->CreateQuery(type,&q)==D3D_OK);
        DWORD value=99; CHECK(q->GetType()==type && q->GetDataSize()==4); // unissued GetData: see review.inc
        if(type==D3DQUERYTYPE_OCCLUSION) CHECK(q->Issue(2)==D3D_OK);
        CHECK(q->Issue(D3DISSUE_END)==D3D_OK && q->GetData(&value,4,D3DGETDATA_FLUSH)==D3D_OK);
        CHECK(value==(type==D3DQUERYTYPE_EVENT?1u:0u) && q->GetData(nullptr,0,0)==D3D_OK && q->Release()==0);
    }
    std::puts("PASS: offscreen targets/depth Clear, getter/binding lifetimes, event and empty occlusion queries"); return 0;
}
