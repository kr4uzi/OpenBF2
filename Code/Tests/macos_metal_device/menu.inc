// [TEMP] [CHOSEN, not recovered] P6 GPU contracts. Run with --menu; no effect cache.
#include "../macos_d3dx_helpers/jpeg-fixture.h"
#include <chrono>
#include <thread>
static int menuClearChecks(IDirect3DDevice9* d) {
    const auto colorWrite=static_cast<D3DRENDERSTATETYPE>(168),zWrite=static_cast<D3DRENDERSTATETYPE>(14),
        stencilWrite=static_cast<D3DRENDERSTATETYPE>(59);
    IDirect3DSurface9 *old=nullptr,*oldDepth=nullptr,*rt=nullptr,*depth=nullptr;
    D3DVIEWPORT9 oldViewport; RECT oldScissor;
    CHECK(d->GetRenderTarget(0,&old)==D3D_OK && d->GetDepthStencilSurface(&oldDepth)==D3D_OK);
    CHECK(d->GetViewport(&oldViewport)==D3D_OK && d->GetScissorRect(&oldScissor)==D3D_OK);
    DWORD scissor,mask,zwrite,zfunc,stencilMask;
    CHECK(d->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor)==D3D_OK);
    CHECK(d->GetRenderState(colorWrite,&mask)==D3D_OK);
    CHECK(d->GetRenderState(zWrite,&zwrite)==D3D_OK);
    CHECK(d->GetRenderState(D3DRS_ZFUNC,&zfunc)==D3D_OK);
    CHECK(d->GetRenderState(stencilWrite,&stencilMask)==D3D_OK);
    CHECK(d->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&rt,nullptr)==D3D_OK);
    CHECK(d->CreateDepthStencilSurface(64,64,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,FALSE,&depth,nullptr)==D3D_OK);
    CHECK(d->SetRenderTarget(0,rt)==D3D_OK && d->SetDepthStencilSurface(depth)==D3D_OK);
    CHECK(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE)==D3D_OK);
    CHECK(d->Clear(0,nullptr,7,0xff000000,1,9)==D3D_OK);
    DWORD colors[64*64],depths[64*64];
    std::fill_n(colors,64*64,0xff000000u); std::fill_n(depths,64*64,0xffffff09u);
    auto expected=[&](unsigned l,unsigned t,unsigned r,unsigned b,unsigned flags,DWORD color,DWORD z,DWORD s) {
        for(unsigned y=t;y<b;++y) for(unsigned x=l;x<r;++x) {
            unsigned i=y*64+x; if(flags&1) colors[i]=color;
            if(flags&2) depths[i]=(z<<8)|(depths[i]&255);
            if(flags&4) depths[i]=(depths[i]&0xffffff00u)|(s&255);
        }
    };
    auto matches=[&] {
        DWORD gotColors[64*64],gotDepths[64*64];
        return bf2metal::inspectSurface(rt,gotColors,sizeof(gotColors)) && bf2metal::inspectSurface(depth,gotDepths,sizeof(gotDepths)) &&
            !std::memcmp(colors,gotColors,sizeof(colors)) && !std::memcmp(depths,gotDepths,sizeof(depths));
    };
    // These draw masks/tests must have no effect on any Clear operation.
    CHECK(d->SetRenderState(colorWrite,0)==D3D_OK && d->SetRenderState(zWrite,FALSE)==D3D_OK);
    CHECK(d->SetRenderState(D3DRS_ZFUNC,1)==D3D_OK && d->SetRenderState(stencilWrite,0)==D3D_OK);
    D3DVIEWPORT9 v={0,0,64,32,0.7f,0.8f};
    CHECK(d->SetViewport(&v)==D3D_OK && d->Clear(0,nullptr,7,0xff123456,.25f,0x137)==D3D_OK);
    expected(0,0,64,32,7,0xff123456,0x400000,0x37); CHECK(matches());
    // Disabled scissor is ignored; Clear Z is not mapped through viewport MinZ/MaxZ.
    RECT s={8,8,24,24}; CHECK(d->SetScissorRect(&s)==D3D_OK);
    CHECK(d->Clear(0,nullptr,2,0,.5f,0)==D3D_OK);
    expected(0,0,64,32,2,0,0x800000,0); CHECK(matches());
    CHECK(d->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE)==D3D_OK);
    CHECK(d->Clear(0,nullptr,4,0,0,0x1ab)==D3D_OK);
    expected(8,8,24,24,4,0,0,0xab); CHECK(matches());
    const D3DRECT rects[]={{-2,-5,12,16},{16,16,50,50},{40,40,50,50},{20,10,10,20}};
    CHECK(d->Clear(4,rects,1,0xffaabbcc,0,0)==D3D_OK);
    expected(8,8,12,16,1,0xffaabbcc,0,0); expected(16,16,24,24,1,0xffaabbcc,0,0); CHECK(matches());
    CHECK(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE)==D3D_OK);
    v={32,16,128,128,0,1}; CHECK(d->SetViewport(&v)==D3D_OK && d->Clear(0,nullptr,7,0xff00ff00,0,3)==D3D_OK);
    expected(32,16,64,64,7,0xff00ff00,0,3); CHECK(matches());
    v={70,70,128,128,0,1}; CHECK(d->SetViewport(&v)==D3D_OK && d->Clear(0,nullptr,7,0xffffffff,1,255)==D3D_OK); CHECK(matches());
    // A clear must leave every caller-visible state untouched.
    DWORD got; CHECK(d->GetRenderState(colorWrite,&got)==D3D_OK && got==0);
    D3DVIEWPORT9 gotViewport; CHECK(d->GetViewport(&gotViewport)==D3D_OK && gotViewport.X==70);
    CHECK(d->SetDepthStencilSurface(nullptr)==D3D_OK && d->Clear(0,nullptr,2,0,1,0)==D3DERR_INVALIDCALL);
    CHECK(d->SetDepthStencilSurface(depth)==D3D_OK && d->Clear(1,nullptr,1,0,0,0)==D3DERR_INVALIDCALL);
    CHECK(d->SetRenderTarget(0,old)==D3D_OK && d->SetDepthStencilSurface(oldDepth)==D3D_OK);
    CHECK(d->SetViewport(&oldViewport)==D3D_OK && d->SetScissorRect(&oldScissor)==D3D_OK);
    CHECK(d->SetRenderState(D3DRS_SCISSORTESTENABLE,scissor)==D3D_OK && d->SetRenderState(colorWrite,mask)==D3D_OK);
    CHECK(d->SetRenderState(zWrite,zwrite)==D3D_OK && d->SetRenderState(D3DRS_ZFUNC,zfunc)==D3D_OK);
    CHECK(d->SetRenderState(stencilWrite,stencilMask)==D3D_OK);
    CHECK(old->Release()==0 && oldDepth->Release()==0 && rt->Release()==0 && depth->Release()==0);
    std::puts("PASS: P6 partial/oversized viewport, enabled/disabled scissor, rectangles, independent color/Z/stencil clears"); return 0;
}
static int menuDepthExtentChecks(IDirect3DDevice9* d) {
    IDirect3DSurface9 *old=nullptr,*oldDepth=nullptr,*small=nullptr,*large=nullptr,*depth=nullptr,*d16=nullptr;
    D3DVIEWPORT9 viewport; RECT scissorRect; DWORD scissor;
    CHECK(d->GetRenderTarget(0,&old)==D3D_OK && d->GetDepthStencilSurface(&oldDepth)==D3D_OK);
    CHECK(d->GetViewport(&viewport)==D3D_OK && d->GetScissorRect(&scissorRect)==D3D_OK &&
        d->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor)==D3D_OK);
    CHECK(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE)==D3D_OK);
    CHECK(d->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&small,nullptr)==D3D_OK);
    CHECK(d->CreateRenderTarget(128,128,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&large,nullptr)==D3D_OK);
    CHECK(d->CreateDepthStencilSurface(128,128,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,FALSE,&depth,nullptr)==D3D_OK);
    CHECK(d->CreateDepthStencilSurface(64,64,D3DFMT_D16,D3DMULTISAMPLE_NONE,0,FALSE,&d16,nullptr)==D3D_OK);
    CHECK(d->SetRenderTarget(0,large)==D3D_OK && d->SetDepthStencilSurface(depth)==D3D_OK);
    CHECK(d->Clear(0,nullptr,6,0,1,9)==D3D_OK && d->SetRenderTarget(0,small)==D3D_OK);
    const D3DVIEWPORT9 oversized={0,0,128,128,0,1};
    CHECK(d->SetViewport(&oversized)==D3D_OK && d->Clear(0,nullptr,6,0,.25f,7)==D3D_OK);
    DWORD values[128*128]; CHECK(bf2metal::inspectSurface(depth,values,sizeof(values)));
    for(unsigned y=0;y<128;++y) for(unsigned x=0;x<128;++x)
        CHECK(values[y*128+x]==(x<64 && y<64?0x40000007u:0xffffff09u));
    CHECK(d->SetDepthStencilSurface(d16)==D3D_OK && d->Clear(0,nullptr,2,0,1,0)==D3D_OK);
    const D3DVIEWPORT9 half={0,0,64,32,0,1};
    CHECK(d->SetViewport(&half)==D3D_OK && d->Clear(0,nullptr,2,0,0,0)==D3D_OK);
    WORD z[64*64]; CHECK(bf2metal::inspectSurface(d16,z,sizeof(z)));
    for(unsigned i=0;i<64*64;++i) CHECK(z[i]==(i<64*32?0:65535));
    CHECK(d->Clear(0,nullptr,4,0,0,0)==D3DERR_INVALIDCALL);
    CHECK(d->SetRenderTarget(0,large)==D3D_OK && d->Clear(0,nullptr,2,0,0,0)==D3DERR_INVALIDCALL);
    CHECK(d->SetRenderTarget(0,old)==D3D_OK && d->SetDepthStencilSurface(oldDepth)==D3D_OK);
    CHECK(d->SetViewport(&viewport)==D3D_OK && d->SetScissorRect(&scissorRect)==D3D_OK &&
        d->SetRenderState(D3DRS_SCISSORTESTENABLE,scissor)==D3D_OK);
    CHECK(old->Release()==0 && oldDepth->Release()==0 && small->Release()==0 && large->Release()==0 && depth->Release()==0 && d16->Release()==0);
    std::puts("PASS: P6 larger depth attachment clipped to color target, D16 partial clear and invalid depth/stencil combinations"); return 0;
}
static int menuJPEGChecks(IDirect3DDevice9* d) {
    using namespace bf2d3dx;
    const auto jpeg=menuFixture::read("quadrants.jpg"),reference=menuFixture::read("quadrants.bgra");
    CHECK(!jpeg.empty() && reference.size()==32*32*4);
    IDirect3DTexture9* t=nullptr; D3DSURFACE_DESC desc;
    CHECK(D3DXCreateTextureFromFileInMemory(d,jpeg.data(),UINT(jpeg.size()),&t)==D3D_OK);
    CHECK(t->GetLevelDesc(0,&desc)==D3D_OK && desc.Format==D3DFMT_X8R8G8B8 && desc.Width==32 && desc.Height==32 && t->GetLevelCount()==6);
    Bytes pixels; CHECK(textureBytes(t,0,0,pixels,false)==D3D_OK && pixels.size()==reference.size());
    D3DCOLOR key; std::memcpy(&key,pixels.data(),sizeof(key)); key|=0xff000000u;
    for(std::size_t i=0;i<pixels.size();++i) CHECK(std::abs(int(pixels[i])-int(reference[i]))<=2);
    const auto mips=menuFixture::read("quadrants-mips.bgra"); std::size_t at=0;
    for(unsigned l=1;l<6;++l) {
        CHECK(textureBytes(t,0,l,pixels,false)==D3D_OK && pixels.size()==std::size_t(32>>l)*(32>>l)*4);
        CHECK(at+pixels.size()<=mips.size());
        for(std::size_t i=0;i<pixels.size();++i) CHECK(std::abs(int(pixels[i])-int(mips[at+i]))<=2);
        at+=pixels.size();
    }
    CHECK(at==mips.size());
    CHECK(t->Release()==0);
    const std::string path=std::string(BF2_MENU_FIXTURES)+"/quadrants.jpg";
    D3DXIMAGE_INFO info;
    CHECK(D3DXCreateTextureFromFileExA(d,path.c_str(),D3DX_FROM_FILE,D3DX_FROM_FILE,1,0,D3DFMT_R5G6B5,
        D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_FILTER_BOX,0,&info,nullptr,&t)==D3D_OK);
    CHECK(t->GetLevelCount()==1 && t->GetLevelDesc(0,&desc)==D3D_OK && desc.Format==D3DFMT_R5G6B5 && info.Format==D3DFMT_X8R8G8B8);
    CHECK(textureBytes(t,0,0,pixels,false)==D3D_OK && pixels.size()==32*32*2);
    auto c=decode(D3DFMT_R5G6B5,pixels.data()+(4*32+4)*2);
    CHECK(c.r>.9 && c.g<.1 && c.b<.2 && c.a==1 && t->Release()==0);
    CHECK(D3DXCreateTextureFromFileExA(d,path.c_str(),D3DX_FROM_FILE,D3DX_FROM_FILE,1,0,D3DFMT_UNKNOWN,
        D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_FILTER_BOX,key,&info,nullptr,&t)==D3D_OK);
    CHECK(t->GetLevelDesc(0,&desc)==D3D_OK && desc.Format==D3DFMT_A8R8G8B8 && info.Format==D3DFMT_X8R8G8B8);
    CHECK(textureBytes(t,0,0,pixels,false)==D3D_OK && pixels[3]==0 && t->Release()==0);
    const auto gray=menuFixture::read("gray.jpg"),grayReference=menuFixture::read("gray.l8");
    CHECK(D3DXCreateTextureFromFileInMemory(d,gray.data(),UINT(gray.size()),&t)==D3D_OK);
    CHECK(t->GetLevelDesc(0,&desc)==D3D_OK && desc.Format==D3DFMT_L8 && t->GetLevelCount()==6);
    CHECK(textureBytes(t,0,0,pixels,false)==D3D_OK && pixels.size()==grayReference.size());
    for(std::size_t i=0;i<pixels.size();++i) CHECK(std::abs(int(pixels[i])-int(grayReference[i]))<=2);
    const D3DCOLOR grayKey=0xff000000u|DWORD(pixels[0])*0x010101u;
    CHECK(t->Release()==0);
    const std::string grayPath=std::string(BF2_MENU_FIXTURES)+"/gray.jpg";
    CHECK(D3DXCreateTextureFromFileExA(d,grayPath.c_str(),D3DX_FROM_FILE,D3DX_FROM_FILE,1,0,D3DFMT_UNKNOWN,
        D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_FILTER_BOX,grayKey,&info,nullptr,&t)==D3D_OK);
    CHECK(t->GetLevelDesc(0,&desc)==D3D_OK && desc.Format==D3DFMT_A8L8 && info.Format==D3DFMT_L8);
    CHECK(textureBytes(t,0,0,pixels,false)==D3D_OK && pixels[1]==0 && t->Release()==0);
    std::puts("PASS: P6 JPEG texture reference, default format/full mip chain and explicit R5G6B5"); return 0;
}
static int menuPresentChecks(IDirect3DDevice9* d,void* window) {
    IDirect3DSwapChain9* chain=nullptr; CHECK(d->GetSwapChain(0,&chain)==D3D_OK);
    CHECK(d->Clear(0,nullptr,1,0xff204080,0,0)==D3D_OK);
    RECT rect={0,0,16,16}; const auto* dirty=reinterpret_cast<const RGNDATA*>(&rect);
    CHECK(d->Present(&rect,nullptr,nullptr,nullptr)==D3DERR_INVALIDCALL);
    CHECK(chain->Present(&rect,nullptr,nullptr,nullptr,0)==D3DERR_INVALIDCALL);
    CHECK(d->Present(nullptr,&rect,nullptr,nullptr)==D3DERR_INVALIDCALL);
    CHECK(chain->Present(nullptr,&rect,nullptr,nullptr,0)==D3DERR_INVALIDCALL);
    CHECK(chain->Present(nullptr,nullptr,nullptr,dirty,0)==D3DERR_INVALIDCALL);
    CHECK(chain->Present(nullptr,nullptr,nullptr,nullptr,4)==D3DERR_INVALIDCALL);
    CHECK(d->BeginScene()==D3D_OK && chain->Present(nullptr,nullptr,nullptr,nullptr,1)==D3DERR_INVALIDCALL && d->EndScene()==D3D_OK);
    const HRESULT busy=static_cast<HRESULT>(0x8876021cu);
    // First nonblocking call starts drawable acquisition and returns immediately.
    const auto start=std::chrono::steady_clock::now();
    CHECK(chain->Present(nullptr,nullptr,nullptr,nullptr,1)==busy);
    CHECK(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(100));
    CHECK(d->Present(nullptr,nullptr,nullptr,nullptr)==D3D_OK); // blocking retry consumes worker request
    CHECK(chain->Present(nullptr,nullptr,nullptr,nullptr,1)==busy);
    HRESULT result=busy;
    for(unsigned i=0;i<2000 && result==busy;++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        result=chain->Present(nullptr,nullptr,nullptr,nullptr,3);
    }
    CHECK(result==D3D_OK);
    // LINEAR_CONTENT is accepted with no conversion: Caps3 does not advertise it.
    CHECK(chain->Present(nullptr,nullptr,metalTestView(window),nullptr,2)==D3D_OK);
    void* alternate=metalTestWindow(); CHECK(alternate); metalTestResize(alternate);
    CHECK(d->Present(nullptr,nullptr,metalTestView(alternate),nullptr)==D3D_OK); // scaled 320x240 -> 400x300
    metalTestDestroy(alternate);
    CHECK(chain->Release()==0);
    std::puts("PASS: P6 Present flags/busy/retry, DISCARD rectangle/dirty refusal, window override/stretch"); return 0;
}
