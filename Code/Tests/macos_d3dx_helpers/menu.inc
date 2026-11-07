// [TEMP] [CHOSEN, not recovered] P6 image reference and CPU clipping contracts.
#include "jpeg-fixture.h"
#include "../../BF2/Platform/Gfx/Metal/MenuBackend.h"
static int menuCPUChecks() {
    const auto jpeg=menuFixture::read("quadrants.jpg"),reference=menuFixture::read("quadrants.bgra");
    CHECK(!jpeg.empty() && reference.size()==32*32*4);
    bf2d3dx::Image image;
    CHECK(bf2d3dx::parse(jpeg.data(),jpeg.size(),image));
    D3DXIMAGE_INFO info={};
    CHECK(D3DXGetImageInfoFromFileInMemory(jpeg.data(),UINT(jpeg.size()),&info)==D3D_OK);
    CHECK(info.Width==32 && info.Height==32 && info.Depth==1 && info.MipLevels==1 &&
        info.Format==D3DFMT_X8R8G8B8 && info.ResourceType==D3DRTYPE_TEXTURE && info.ImageFileFormat==D3DXIFF_JPG);
    CHECK(image.sourceFormat==D3DFMT_X8R8G8B8 && image.data.size()==1 && image.data[0].size()==reference.size());
    for(std::size_t i=0;i<reference.size();++i)
        CHECK(std::abs(int(image.data[0][i])-int(reference[i]))<=2);
    // D3DX treats these as texture rows, ignoring a photo's EXIF rotation.
    const unsigned char exif[]={0xff,0xe1,0,34,'E','x','i','f',0,0,'I','I',42,0,8,0,0,0,
        1,0,0x12,1,3,0,1,0,0,0,6,0,0,0,0,0,0,0};
    auto rotated=jpeg; rotated.insert(rotated.begin()+2,exif,exif+sizeof(exif));
    bf2d3dx::Image tagged;
    CHECK(bf2d3dx::parse(rotated.data(),rotated.size(),tagged) && tagged.data==image.data);
    for(std::size_t n:{0u,2u,18u,124u,unsigned(jpeg.size()/2)})
        CHECK(!bf2d3dx::parse(jpeg.data(),n,image));
    auto damaged=jpeg; damaged[0]=0; CHECK(!bf2d3dx::parse(damaged.data(),damaged.size(),image));
    auto noEnd=jpeg; noEnd.resize(noEnd.size()-2); CHECK(!bf2d3dx::parse(noEnd.data(),noEnd.size(),image));
    const auto gray=menuFixture::read("gray.jpg"),grayReference=menuFixture::read("gray.l8");
    CHECK(!gray.empty() && grayReference.size()==32*32 && bf2d3dx::parse(gray.data(),gray.size(),image));
    CHECK(image.info.Format==D3DFMT_L8 && image.sourceFormat==D3DFMT_L8 && image.data[0].size()==grayReference.size());
    for(std::size_t i=0;i<grayReference.size();++i) CHECK(std::abs(int(image.data[0][i])-int(grayReference[i]))<=2);
    using bf2metal::clearRectangles;
    const D3DVIEWPORT9 viewport={0,0,64,32,0,1};
    auto r=clearRectangles(viewport,64,64,0,nullptr,nullptr);
    CHECK(r.size()==1 && r[0].width==64 && r[0].height==32);
    const D3DRECT rectangles[]={{-10,-20,20,80},{30,10,100,100},{90,90,100,100},{9,8,2,1}};
    const RECT scissor={10,5,50,25};
    r=clearRectangles(viewport,64,64,4,rectangles,&scissor);
    CHECK(r.size()==2 && r[0].x==10 && r[0].y==5 && r[0].width==10 && r[0].height==20);
    CHECK(r[1].x==30 && r[1].y==10 && r[1].width==20 && r[1].height==15);
    r=clearRectangles({32,16,0xffffffffu,0xffffffffu,0,1},64,64,0,nullptr,nullptr);
    CHECK(r.size()==1 && r[0].x==32 && r[0].y==16 && r[0].width==32 && r[0].height==48);
    CHECK(clearRectangles({0xffffffffu,0xffffffffu,4,4,0,1},64,64,0,nullptr,nullptr).empty());
    const RECT outside={40,40,60,60};
    CHECK(clearRectangles(viewport,64,64,0,nullptr,&outside).empty());
    RECT rect={0,0,10,10}; const auto* dirty=reinterpret_cast<const RGNDATA*>(&rect);
    for(DWORD flags:{0u,1u,2u,3u}) CHECK(bf2metal::presentParameters(D3DSWAPEFFECT_DISCARD,nullptr,nullptr,nullptr,flags)==D3D_OK);
    CHECK(bf2metal::presentParameters(D3DSWAPEFFECT_DISCARD,&rect,nullptr,nullptr,0)==D3DERR_INVALIDCALL);
    CHECK(bf2metal::presentParameters(D3DSWAPEFFECT_DISCARD,nullptr,&rect,nullptr,0)==D3DERR_INVALIDCALL);
    CHECK(bf2metal::presentParameters(D3DSWAPEFFECT_DISCARD,nullptr,nullptr,dirty,0)==D3DERR_INVALIDCALL);
    CHECK(bf2metal::presentParameters(D3DSWAPEFFECT_DISCARD,nullptr,nullptr,nullptr,4)==D3DERR_INVALIDCALL);
    std::puts("PASS: JPEG decoded reference/metadata/rejection, Clear intersections/overflow, Present parameters"); return 0;
}
