// [TEMP] [CHOSEN, not recovered] D3DX checks through real G3b GPU resources.
#include "../../BF2/Platform/Gfx/Metal/D3DXHelpers.h"
#include <cmath>
#include <array>
#include <set>
#include <string>
#include <unistd.h>
#include "mesh.inc"
// The standalone tool has no engine filesystem. Production uses PluginEntry's
// host singleton; this null fixture selects the documented physical-file path.
namespace dice { namespace hfe { namespace io { class IFileManager; } io::IFileManager* fileManager=nullptr; } }
static void WINAPI fillD3dx(D3DXVECTOR4* out,const D3DXVECTOR2* uv,const D3DXVECTOR2* size,void* data) {
    unsigned& calls=*static_cast<unsigned*>(data); ++calls;
    if(!(size->x>0 && size->y>0)) std::abort();
    *out=D3DXVECTOR4(uv->x,uv->y,.25f,1);
}
static int d3dxTextureChecks(IDirect3DDevice9* d) {
    using namespace bf2d3dx;
    IDirect3DTexture9* t=nullptr;
    CHECK(D3DXCreateTexture(d,4,4,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&t)==D3D_OK && t->GetLevelCount()==3);
    unsigned calls=0; CHECK(D3DXFillTexture(t,fillD3dx,&calls)==D3D_OK && calls==21);
    Bytes bytes; CHECK(textureBytes(t,0,0,bytes,false)==D3D_OK && bytes[2]==32 && bytes[1]==32 && bytes[0]==64);
    CHECK(D3DXFilterTexture(t,nullptr,0,D3DX_FILTER_BOX)==D3D_OK);
    CHECK(textureBytes(t,0,2,bytes,false)==D3D_OK && bytes[0]==64 && bytes[1]==128 && bytes[2]==128 && bytes[3]==255);
    IDirect3DSurface9 *src=nullptr,*dst=nullptr;
    CHECK(t->GetSurfaceLevel(0,&src)==D3D_OK);
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A16B16G16R16F,D3DMULTISAMPLE_NONE,0,FALSE,&dst,nullptr)==D3D_OK);
    RECT sr={1,1,3,3},dr={2,2,6,6};
    CHECK(D3DXLoadSurfaceFromSurface(dst,nullptr,&dr,src,nullptr,&sr,D3DX_FILTER_POINT,0)==D3D_OK);
    CHECK(surfaceBytes(dst,bytes,false)==D3D_OK);
    auto c=decode(D3DFMT_A16B16G16R16F,bytes.data()+(2*8+2)*8); CHECK(std::abs(c.r-96/255.)<.001 && c.a==1);
    const unsigned char red[]={0,0,255,255}; RECT pixel={0,0,1,1},corner={7,7,8,8};
    CHECK(D3DXLoadSurfaceFromMemory(dst,nullptr,&corner,red,D3DFMT_A8R8G8B8,4,nullptr,&pixel,D3DX_FILTER_NONE,0xffff0000)==D3D_OK);
    CHECK(surfaceBytes(dst,bytes,false)==D3D_OK && decode(D3DFMT_A16B16G16R16F,bytes.data()+63*8).a==0);
    CHECK(D3DXLoadSurfaceFromSurface(dst,nullptr,&dr,dst,nullptr,&dr,D3DX_FILTER_NONE,0)==D3D_OK);
    src->Release(); dst->Release();
    // Save/load all DDS mip bytes and a TGA screenshot through physical tool paths.
    char temp[]="/private/tmp/openbf2-g3c-XXXXXX"; CHECK(mkdtemp(temp));
    const std::string dds=std::string(temp)+"/test.dds",tga=std::string(temp)+"/test.tga";
    CHECK(D3DXSaveTextureToFileA(dds.c_str(),D3DXIFF_DDS,t,nullptr)==D3D_OK);
    D3DXIMAGE_INFO info; CHECK(D3DXGetImageInfoFromFileA(dds.c_str(),&info)==D3D_OK && info.Width==4 && info.MipLevels==3);
    IDirect3DTexture9* copy=nullptr;
    CHECK(D3DXCreateTextureFromFileExA(d,dds.c_str(),D3DX_DEFAULT,D3DX_DEFAULT,0,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_DEFAULT,0,nullptr,nullptr,&copy)==D3D_OK);
    for(unsigned l=0;l<3;++l) { Bytes a,b; CHECK(textureBytes(t,0,l,a,false)==D3D_OK && textureBytes(copy,0,l,b,false)==D3D_OK && a==b); }
    CHECK(copy->GetSurfaceLevel(0,&src)==D3D_OK && D3DXSaveSurfaceToFileA(tga.c_str(),D3DXIFF_TGA,src,nullptr,nullptr)==D3D_OK); src->Release(); copy->Release();
    CHECK(D3DXGetImageInfoFromFileA(tga.c_str(),&info)==D3D_OK && info.ImageFileFormat==D3DXIFF_TGA);
    unlink(dds.c_str()); unlink(tga.c_str()); rmdir(temp); t->Release();
    Bytes oddTga(27,0); oddTga[2]=2; oddTga[12]=3; oddTga[14]=1; oddTga[16]=24; oddTga[17]=32;
    oddTga[20]=255; oddTga[22]=255; oddTga[24]=255;
    CHECK(D3DXCreateTextureFromFileInMemory(d,oddTga.data(),UINT(oddTga.size()),&t)==D3D_OK);
    D3DSURFACE_DESC loaded; CHECK(t->GetLevelDesc(0,&loaded)==D3D_OK && loaded.Width==4 && loaded.Height==1 && t->GetLevelCount()==3); t->Release();
    // Cube filtering is independent per face. Volume BOX averages 2x2x2.
    IDirect3DCubeTexture9* cube=nullptr; CHECK(d->CreateCubeTexture(4,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&cube,nullptr)==D3D_OK);
    for(unsigned face=0;face<6;++face) { Bytes b(64); for(unsigned j=0;j<16;++j) { b[j*4]=face*30; b[j*4+3]=255; } CHECK(textureBytes(cube,face,0,b,true)==D3D_OK); }
    CHECK(D3DXFilterTexture(cube,nullptr,0,D3DX_DEFAULT)==D3D_OK);
    for(unsigned face=0;face<6;++face) { Bytes b; CHECK(textureBytes(cube,face,2,b,false)==D3D_OK && b[0]==face*30); } cube->Release();
    IDirect3DVolumeTexture9* volume=nullptr; CHECK(d->CreateVolumeTexture(2,2,2,0,0,D3DFMT_L8,D3DPOOL_MANAGED,&volume,nullptr)==D3D_OK);
    bytes={0,10,20,30,40,50,60,70}; CHECK(textureBytes(volume,0,0,bytes,true)==D3D_OK && D3DXFilterTexture(volume,nullptr,0,D3DX_DEFAULT)==D3D_OK);
    CHECK(textureBytes(volume,0,1,bytes,false)==D3D_OK && bytes[0]==35); volume->Release();
    // Render into a nonlockable target, read it to a SYSTEMMEM surface, and
    // restore the original target/depth/viewport after EndScene.
    IDirect3DSurface9 *old=nullptr,*now=nullptr; D3DVIEWPORT9 viewport,restored;
    CHECK(d->GetRenderTarget(0,&old)==D3D_OK && d->GetViewport(&viewport)==D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(4,4,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&dst,nullptr)==D3D_OK);
    ID3DXRenderToSurface* rts=nullptr; CHECK(D3DXCreateRenderToSurface(d,4,4,D3DFMT_A8R8G8B8,TRUE,D3DFMT_D16,&rts)==D3D_OK);
    CHECK(rts->BeginScene(dst,nullptr)==D3D_OK && rts->BeginScene(dst,nullptr)==D3DERR_INVALIDCALL);
    CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,1,0)==D3D_OK);
    CHECK(rts->EndScene(D3DX_FILTER_NONE)==D3D_OK && rts->EndScene(D3DX_FILTER_NONE)==D3DERR_INVALIDCALL);
    CHECK(surfaceBytes(dst,bytes,false)==D3D_OK && bytes[0]==0x56 && bytes[1]==0x34 && bytes[2]==0x12 && bytes[3]==255);
    CHECK(d->GetRenderTarget(0,&now)==D3D_OK && now==old && d->GetViewport(&restored)==D3D_OK && !std::memcmp(&viewport,&restored,sizeof(viewport)));
    now->Release(); old->Release(); dst->Release(); CHECK(rts->Release()==0);
    std::puts("PASS: D3DX texture conversion/rectangles/key, mip/callback, DDS/TGA I/O, cube/volume and render-to-surface"); return 0;
}
static int d3dxMeshChecks(IDirect3DDevice9* d) {
    CHECK(d3dxNewShapeChecks(d)==0 && d3dxNormalOptimizeChecks(d)==0);
    for(unsigned kind=0;kind<3;++kind) {
        ID3DXMesh* mesh=nullptr; ID3DXBuffer* adjacent=nullptr;
        HRESULT hr=kind==0?D3DXCreateSphere(d,1,4,4,&mesh,&adjacent):kind==1?D3DXCreateBox(d,2,4,6,&mesh,&adjacent):D3DXCreateCylinder(d,1,1,2,4,2,&mesh,&adjacent);
        CHECK(hr==D3D_OK && mesh && adjacent);
        CHECK(mesh->GetNumVertices()==(kind==0?14u:kind==1?24u:22u));
        CHECK(mesh->GetNumFaces()==(kind==0?24u:kind==1?12u:24u));
        CHECK(mesh->GetFVF()==0x12 && mesh->GetNumBytesPerVertex()==24 && adjacent->GetBufferSize()==mesh->GetNumFaces()*12);
        IDirect3DVertexBuffer9* vb=nullptr; IDirect3DIndexBuffer9* ib=nullptr;
        CHECK(mesh->GetVertexBuffer(&vb)==D3D_OK && mesh->GetIndexBuffer(&ib)==D3D_OK); vb->Release(); ib->Release();
        struct Vertex { D3DXVECTOR3 p,n; }; Vertex* data=nullptr;
        CHECK(mesh->LockVertexBuffer(D3DLOCK_READONLY,reinterpret_cast<void**>(&data))==D3D_OK);
        D3DXVECTOR3 lo,hi,center; float radius;
        CHECK(D3DXComputeBoundingBox(&data[0].p,mesh->GetNumVertices(),24,&lo,&hi)==D3D_OK);
        CHECK(D3DXComputeBoundingSphere(&data[0].p,mesh->GetNumVertices(),24,&center,&radius)==D3D_OK);
        CHECK(std::abs(center.x)<1e-6 && std::abs(center.y)<1e-6 && std::abs(center.z)<1e-6);
        CHECK(std::abs(radius-(kind==0?1:kind==1?std::sqrt(14.):std::sqrt(2.)))<1e-5);
        CHECK(std::abs(lo.x+1)<1e-6 && std::abs(hi.x-1)<1e-6 && lo.z==(kind==1?-3:-1) && hi.z==(kind==1?3:1));
        CHECK(std::abs(data[0].p.z-(kind==0?1:kind==1?-3:-1))<1e-6);
        for(unsigned i=0;i<mesh->GetNumVertices();++i) CHECK(std::abs(data[i].n.x*data[i].n.x+data[i].n.y*data[i].n.y+data[i].n.z*data[i].n.z-1)<1e-5);
        CHECK(mesh->UnlockVertexBuffer()==D3D_OK);
        WORD* indices=nullptr; CHECK(mesh->LockIndexBuffer(D3DLOCK_READONLY,reinterpret_cast<void**>(&indices))==D3D_OK);
        std::vector<std::array<WORD,3>> original;
        for(unsigned i=0;i<mesh->GetNumFaces();++i) { original.push_back({indices[i*3],indices[i*3+1],indices[i*3+2]}); for(unsigned e=0;e<3;++e) CHECK(indices[i*3+e]<mesh->GetNumVertices()); }
        CHECK(original[0][0]==0 && original[0][1]==(kind==0?2:1) && original[0][2]==(kind==0?1:2));
        CHECK(mesh->UnlockIndexBuffer()==D3D_OK);
        auto* adj=static_cast<DWORD*>(adjacent->GetBufferPointer());
        for(unsigned i=0;i<mesh->GetNumFaces()*3;++i) CHECK(adj[i]<mesh->GetNumFaces());
        ID3DXMesh *clone=nullptr,*clone2=nullptr;
        CHECK(mesh->CloneMeshFVF(0x660,2,d,&clone)==D3D_OK && clone->GetNumBytesPerVertex()==12 && clone->GetNumVertices()==mesh->GetNumVertices());
        const D3DVERTEXELEMENT9 decl[]={{0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITION,0},D3DDECL_END()};
        CHECK(mesh->CloneMesh(D3DXMESH_SYSTEMMEM|D3DXMESH_32BIT,decl,d,&clone2)==D3D_OK && clone2->GetNumBytesPerVertex()==16);
        float* pos=nullptr; CHECK(clone2->LockVertexBuffer(D3DLOCK_READONLY,reinterpret_cast<void**>(&pos))==D3D_OK && pos[3]==1); CHECK(clone2->UnlockVertexBuffer()==D3D_OK);
        clone2->Release(); clone->Release();
        std::vector<DWORD> remap(mesh->GetNumFaces()),outAdj(mesh->GetNumFaces()*3); ID3DXBuffer* vertexRemap=nullptr;
        CHECK(mesh->OptimizeInplace(0x14400000,adj,outAdj.data(),remap.data(),&vertexRemap)==D3D_OK && vertexRemap->GetBufferSize()==mesh->GetNumVertices()*4);
        CHECK(mesh->LockIndexBuffer(D3DLOCK_READONLY,reinterpret_cast<void**>(&indices))==D3D_OK);
        std::set<DWORD> seen;
        for(unsigned i=0;i<mesh->GetNumFaces();++i) { CHECK(remap[i]<mesh->GetNumFaces()); seen.insert(remap[i]); for(unsigned e=0;e<3;++e) CHECK(indices[i*3+e]==original[remap[i]][e]); }
        CHECK(seen.size()==mesh->GetNumFaces() && mesh->UnlockIndexBuffer()==D3D_OK);
        vertexRemap->Release(); adjacent->Release(); CHECK(mesh->Release()==0);
    }
    std::puts("PASS: D3DX mesh counts/bounds/normals/order, buffer lifetime, cloning, adjacency and cache face remaps"); return 0;
}
