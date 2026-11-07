#ifndef BF2_TEMP_FONT_RUNTIME_SUPPORT_H
#define BF2_TEMP_FONT_RUNTIME_SUPPORT_H
// [TEMP] [CHOSEN, not recovered] Native client font component fixture.
// FileManager overrides only scripted file bytes/existence, ClassManager only
// MemStream/texture construction. Real Font/metadata/loaders/FileIStream/File/
// MemStream execute. No renderer, UI root, descriptor factory or engine init.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <cmath>
#define private public
#define protected public
#include "../BF2/Menu/Font.h"
#include "../BF2/Menu/DifFontMetadata.h"
#include "../BF2/External/BFMeme/Style.h"
#undef protected
#undef private
#include "../BF2/External/BFMeme/Quad.h"
#include "../BF2/GeomMisc/MenuRend/IMemeTexture.h"
#include "../BF2/IO/File/FileManager.h"
#include "../BF2/IO/Stream/MemStream.h"
#include "../BF2/Core/ClassManager.h"
using namespace dice::hfe;
using namespace dice::hfe::menu;
static int checks, failures;
static void check(bool value, const char* name)
{ ++checks; if (!value) { ++failures; std::printf("FAIL %s\n", name); } }
static bool closeFloat(float a,float b) { return std::fabs(a-b)<0.00001f; }
struct Texture : geom::IMemeTexture
{
    int draws=0, releases=0;
    float uv[4];
    std::string name,prefix;
    unsigned char flag=0;
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { ++releases; return 0; }
    void* queryInterface(unsigned int) const override { return nullptr; }
    void setTexture(const char* n,const char* p,int) override { name=n;prefix=p; }
    void createTexture(unsigned int,unsigned int,unsigned int) override {}
    bool drawUV(dice::meme::Quad*,float a,float b,float c,float d) override
    { ++draws;uv[0]=a;uv[1]=b;uv[2]=c;uv[3]=d;return false; }
    bool drawOther(geom::IMemeTexture*,dice::meme::Quad*) override { return false; }
    const char* getName() const override { return name.c_str(); }
    void setUserObject(void*) override {}
    void* getUserObject() const override { return nullptr; }
    float getUvWidthCopy() const override { return 0.5f; }
    float getUvHeightCopy() const override { return 0.25f; }
    int getPixelWidth() const override { return 128; }
    int getPixelHeight() const override { return 64; }
    int getPageWidth() const override { return 256; }
    int getPageHeight() const override { return 256; }
    unsigned char setByte66(unsigned char v) override { flag=v; return v; }
    unsigned char getByte66() const override { return flag; }
    float getUOffset() const override { return 0; }
    float getVOffset() const override { return 0; }
    float getUvWidth() const override { return 0.5f; }
    float getUvHeight() const override { return 0.25f; }
    void releaseTexture() override {}
};
static Texture texture;
struct ObservingFont : DifFont
{
    explicit ObservingFont(const char* name) : DifFont(name) {}
    int previous=0,current=0,calls=0;
    float getKerning(int a,int b) override
    { ++calls; previous=a;current=b;return DifFont::getKerning(a,b); }
};
struct Files : io::FileManager
{
    std::map<std::string,std::string> data;
    std::vector<std::string> opens, exists;
    io::IStream* open(const std::string& path,FileMode,FileAccess,unsigned int) override
    {
        opens.push_back(path);
        auto it=data.find(path);
        if(it==data.end()) return nullptr;
        io::MemStream* result=new io::MemStream;
        result->setSize(static_cast<unsigned int>(it->second.size()));
        std::memcpy(result->getPtr(),it->second.data(),it->second.size());
        return static_cast<io::IMemStream*>(result);
    }
    bool fileExists(const std::string& path) override
    { exists.push_back(path);return data.find(path)!=data.end(); }
};
struct Classes : ClassManager
{
    IBase* createInstance(unsigned int cid,unsigned int iid,IBase*) override
    {
        if(cid==io::CID_MemStream && iid==io::IID_IMemStream)
            return static_cast<io::IMemStream*>(new io::MemStream);
        check(cid==geom::CID_MemeTexture && iid==geom::IID_IMemeTexture,"texture IDs");
        return &texture;
    }
};
namespace dice { namespace hfe {
ClassManager* classManager;
namespace io { FileManager* fileManager; }
} }
#endif
