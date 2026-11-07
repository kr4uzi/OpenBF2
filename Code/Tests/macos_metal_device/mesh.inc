// [TEMP] [CHOSEN, not recovered] Mesh contracts through production GPU buffers.
#include "../../BF2/Platform/Gfx/Metal/D3DXMesh.h"
#include <limits>
static int d3dxNewShapeChecks(IDirect3DDevice9* d) {
    using bf2d3dx::MeshVertex;
    ID3DXMesh* mesh=nullptr; ID3DXBuffer* adjacency=nullptr;
    CHECK(D3DXCreatePolygon(d,2,2,&mesh,&adjacency)==D3DERR_INVALIDCALL && !mesh && !adjacency);
    CHECK(D3DXCreatePolygon(d,-1,4,&mesh,nullptr)==D3DERR_INVALIDCALL);
    CHECK(D3DXCreateTorus(d,1,2,2,4,&mesh,nullptr)==D3DERR_INVALIDCALL);
    CHECK(D3DXCreateTorus(d,1,std::numeric_limits<float>::infinity(),4,4,&mesh,nullptr)==D3DERR_INVALIDCALL);
    for(unsigned kind=0;kind<2;++kind) {
        CHECK((kind?D3DXCreateTorus(d,.5f,2,4,8,&mesh,&adjacency):D3DXCreatePolygon(d,2,4,&mesh,&adjacency))==D3D_OK);
        CHECK(mesh->GetFVF()==0x12 && mesh->GetOptions()==D3DXMESH_MANAGED && mesh->GetNumBytesPerVertex()==24);
        CHECK(mesh->GetNumVertices()==(kind?32u:5u) && mesh->GetNumFaces()==(kind?64u:4u));
        D3DVERTEXELEMENT9 decl[65]; CHECK(mesh->GetDeclaration(decl)==D3D_OK);
        CHECK(decl[0].Offset==0 && decl[0].Type==D3DDECLTYPE_FLOAT3 && decl[0].Usage==D3DDECLUSAGE_POSITION);
        CHECK(decl[1].Offset==12 && decl[1].Type==D3DDECLTYPE_FLOAT3 && decl[1].Usage==D3DDECLUSAGE_NORMAL && decl[2].Stream==255);
        MeshVertex* v=nullptr; WORD* ix=nullptr;
        CHECK(mesh->LockVertexBuffer(D3DLOCK_READONLY,reinterpret_cast<void**>(&v))==D3D_OK);
        CHECK(mesh->LockIndexBuffer(D3DLOCK_READONLY,reinterpret_cast<void**>(&ix))==D3D_OK);
        if(kind) CHECK(v[0].position.x==2.5f && v[0].position.y==0 && v[0].position.z==0 && v[0].normal.x==1);
        else CHECK(v[0].position.x==0 && v[1].position.y>1.4142f && v[1].normal.z==-1 && ix[0]==0 && ix[1]==1 && ix[2]==2);
        for(unsigned f=0;f<mesh->GetNumFaces();++f) {
            const auto &a=v[ix[f*3]],&b=v[ix[f*3+1]],&c=v[ix[f*3+2]];
            const float ux=b.position.x-a.position.x,uy=b.position.y-a.position.y,uz=b.position.z-a.position.z;
            const float vx=c.position.x-a.position.x,vy=c.position.y-a.position.y,vz=c.position.z-a.position.z;
            CHECK((uy*vz-uz*vy)*a.normal.x+(uz*vx-ux*vz)*a.normal.y+(ux*vy-uy*vx)*a.normal.z>0);
        }
        for(unsigned i=0;i<mesh->GetNumVertices();++i) CHECK(std::abs(v[i].normal.x*v[i].normal.x+v[i].normal.y*v[i].normal.y+v[i].normal.z*v[i].normal.z-1)<1e-6);
        CHECK(mesh->UnlockVertexBuffer()==D3D_OK && mesh->UnlockIndexBuffer()==D3D_OK);
        auto* adj=static_cast<DWORD*>(adjacency->GetBufferPointer()); unsigned boundary=0;
        for(unsigned e=0;e<mesh->GetNumFaces()*3;++e) {
            if(adj[e]==0xffffffffu) { ++boundary; continue; }
            CHECK(adj[e]<mesh->GetNumFaces()); bool reciprocal=false;
            for(unsigned j=0;j<3;++j) if(adj[adj[e]*3+j]==e/3) reciprocal=true;
            CHECK(reciprocal);
        }
        CHECK(boundary==(kind?0u:4u));
        DWORD count=0; CHECK(mesh->GetAttributeTable(nullptr,&count)==D3D_OK && count==1);
        D3DXATTRIBUTERANGE range; count=0;
        CHECK(mesh->GetAttributeTable(&range,&count)==D3DERR_INVALIDCALL && count==1);
        CHECK(mesh->GetAttributeTable(&range,&count)==D3D_OK && range.AttribId==0 && range.FaceStart==0 && range.FaceCount==mesh->GetNumFaces() && range.VertexStart==0 && range.VertexCount==mesh->GetNumVertices());
        adjacency->Release(); mesh->Release();
    }
    // Exercise the 32-bit shape-index boundary without creating GPU draw work.
    CHECK(D3DXCreateTorus(d,.5f,2,256,256,&mesh,nullptr)==D3D_OK && (mesh->GetOptions()&D3DXMESH_32BIT));
    mesh->Release();
    std::puts("PASS: Polygon/Torus topology, winding, normals, FVF, adjacency, ranges and 32-bit indices"); return 0;
}
static int d3dxNormalOptimizeChecks(IDirect3DDevice9* d) {
    using bf2d3dx::MeshVertex;
    for(unsigned wide=0;wide<2;++wide) {
        ID3DXMesh* mesh=nullptr;
        CHECK(D3DXCreateMeshFVF(2,6,D3DXMESH_SYSTEMMEM|wide,0x12,d,&mesh)==D3D_OK);
        MeshVertex original[]={{{0,0,0},{9,9,9}},{{1,0,0},{9,9,9}},{{0,1,0},{9,9,9}},
            {{0,0,0},{9,9,9}},{{0,0,10},{9,9,9}},{{1,0,0},{9,9,9}}};
        void* data=nullptr; CHECK(mesh->LockVertexBuffer(0,&data)==D3D_OK); std::memcpy(data,original,sizeof(original)); CHECK(mesh->UnlockVertexBuffer()==D3D_OK);
        CHECK(mesh->LockIndexBuffer(0,&data)==D3D_OK);
        for(unsigned i=0;i<6;++i) if(wide) static_cast<DWORD*>(data)[i]=i; else static_cast<WORD*>(data)[i]=WORD(i);
        CHECK(mesh->UnlockIndexBuffer()==D3D_OK && D3DXComputeNormals(mesh,nullptr)==D3D_OK);
        CHECK(mesh->LockVertexBuffer(D3DLOCK_READONLY,&data)==D3D_OK); auto* v=static_cast<MeshVertex*>(data);
        CHECK(v[0].normal.z==1 && v[0].normal.y==0 && v[3].normal.y==1 && v[3].normal.z==0);
        CHECK(mesh->UnlockVertexBuffer()==D3D_OK);
        DWORD adj[]={1,0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu,0};
        CHECK(D3DXComputeNormals(mesh,adj)==D3D_OK);
        CHECK(mesh->LockVertexBuffer(D3DLOCK_READONLY,&data)==D3D_OK); v=static_cast<MeshVertex*>(data);
        CHECK(std::abs(v[0].normal.y-std::sqrt(.5f))<1e-6 && std::abs(v[0].normal.z-std::sqrt(.5f))<1e-6);
        CHECK(std::memcmp(&v[0].normal,&v[3].normal,12)==0);
        MeshVertex source[6]; std::memcpy(source,v,sizeof(source)); CHECK(mesh->UnlockVertexBuffer()==D3D_OK);
        DWORD* attrs=nullptr; CHECK(mesh->LockAttributeBuffer(0,&attrs)==D3D_OK); attrs[0]=7; attrs[1]=3; CHECK(mesh->UnlockAttributeBuffer()==D3D_OK);
        ID3DXMesh* optimized=nullptr; ID3DXBuffer* remap=nullptr; DWORD faces[2],outAdj[6];
        CHECK(mesh->Optimize(0x04000110,adj,outAdj,faces,&remap,&optimized)==D3D_OK);
        CHECK(optimized!=mesh && optimized->GetOptions()==(D3DXMESH_SYSTEMMEM|wide) && optimized->GetFVF()==0x12);
        CHECK(faces[0]==1 && faces[1]==0 && remap->GetBufferSize()==24);
        auto* vertices=static_cast<DWORD*>(remap->GetBufferPointer());
        CHECK(optimized->LockVertexBuffer(D3DLOCK_READONLY,&data)==D3D_OK); v=static_cast<MeshVertex*>(data);
        for(unsigned i=0;i<6;++i) CHECK(vertices[i]<6 && std::memcmp(&v[i],&source[vertices[i]],24)==0);
        CHECK(optimized->UnlockVertexBuffer()==D3D_OK && optimized->LockIndexBuffer(D3DLOCK_READONLY,&data)==D3D_OK);
        for(unsigned f=0;f<2;++f) for(unsigned e=0;e<3;++e) { DWORD index=wide?static_cast<DWORD*>(data)[f*3+e]:static_cast<WORD*>(data)[f*3+e]; CHECK(vertices[index]==faces[f]*3+e); }
        CHECK(optimized->UnlockIndexBuffer()==D3D_OK);
        CHECK(outAdj[2]==1 && outAdj[3]==0);
        DWORD count=2; D3DXATTRIBUTERANGE ranges[2];
        CHECK(optimized->GetAttributeTable(ranges,&count)==D3D_OK && count==2 && ranges[0].AttribId==3 && ranges[1].AttribId==7 && ranges[0].VertexCount==3);
        CHECK(mesh->LockVertexBuffer(D3DLOCK_READONLY,&data)==D3D_OK && std::memcmp(source,data,sizeof(source))==0 && mesh->UnlockVertexBuffer()==D3D_OK);
        remap->Release(); optimized->Release(); optimized=nullptr;
        for(DWORD flags:{0x08000110u,0x01000110u,0x04000111u,0x04000150u,0x80000110u}) CHECK(mesh->Optimize(flags,adj,nullptr,nullptr,nullptr,&optimized)==D3DERR_INVALIDCALL && !optimized);
        CHECK(mesh->Optimize(0x04000110,nullptr,nullptr,nullptr,nullptr,&optimized)==D3DERR_INVALIDCALL && !optimized);
        adj[0]=2; CHECK(D3DXComputeNormals(mesh,adj)==D3DERR_INVALIDCALL);
        CHECK(mesh->Optimize(0x04000110,adj,nullptr,nullptr,nullptr,&optimized)==D3DERR_INVALIDCALL && !optimized);
        mesh->Release();
    }
    ID3DXMesh* positions=nullptr; CHECK(D3DXCreateMeshFVF(1,3,D3DXMESH_SYSTEMMEM,2,d,&positions)==D3D_OK);
    CHECK(D3DXComputeNormals(positions,nullptr)==D3DERR_INVALIDCALL); positions->Release();
    // A vertex shared across material groups must split unless prohibited.
    ID3DXMesh* shared=nullptr; CHECK(D3DXCreateMeshFVF(2,4,D3DXMESH_SYSTEMMEM,2,d,&shared)==D3D_OK);
    void* raw=nullptr; const float square[]={0,0,0,1,0,0,0,1,0,1,1,0}; const WORD triangles[]={0,1,2,2,1,3};
    CHECK(shared->LockVertexBuffer(0,&raw)==D3D_OK); std::memcpy(raw,square,sizeof(square)); CHECK(shared->UnlockVertexBuffer()==D3D_OK);
    CHECK(shared->LockIndexBuffer(0,&raw)==D3D_OK); std::memcpy(raw,triangles,sizeof(triangles)); CHECK(shared->UnlockIndexBuffer()==D3D_OK);
    DWORD* attrs=nullptr; CHECK(shared->LockAttributeBuffer(0,&attrs)==D3D_OK); attrs[0]=2; attrs[1]=1; CHECK(shared->UnlockAttributeBuffer()==D3D_OK);
    const DWORD adjacent[]={0xffffffffu,1,0xffffffffu,0,0xffffffffu,0xffffffffu};
    for(DWORD flags:{0x04000110u,0x24000110u,0x14000110u}) {
        ID3DXMesh* copy=nullptr; ID3DXBuffer* remap=nullptr;
        CHECK(shared->Optimize(flags,adjacent,nullptr,nullptr,&remap,&copy)==D3D_OK);
        CHECK(copy->GetNumVertices()==(flags==0x04000110u?6u:4u) && remap->GetBufferSize()==copy->GetNumVertices()*4);
        if(flags==0x14000110u) { const auto* map=static_cast<DWORD*>(remap->GetBufferPointer()); for(unsigned i=0;i<4;++i) CHECK(map[i]==i); }
        remap->Release(); copy->Release();
    }
    shared->Release();
    std::puts("PASS: normals with/without seams, angle weighting, Optimize storage/attributes/remaps/source lifetime and refused flags"); return 0;
}
