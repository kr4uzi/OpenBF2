// Shared road appearance, texture mapping and geometry-generation settings.


#ifdef BF2_IMAGE_RENDDX9

#include "RoadTemplate.h"
#include "Terrain.h"                     // TerrainRawReader - the raw file family (writers added for this)

#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"   // dice::hfe::fileManager
#include <sstream>
#include <string>

#include <new>

namespace dice {
namespace hfe {
namespace geom {

// dword_1021AEB4 - 821008, read as data by getClassList (0x100C83D2).
extern const unsigned int CID_RoadTemplate = 821008u;

namespace {

// dword_1021AE20 - 820008, the one id queryInterface tests after IBase's 1; read as data.
const unsigned int kIID_IRoadTemplate = 820008u;

} // anonymous namespace


// Construction and destruction.


// The stores in shipped order: vptr, refCount 1, the four vectors' pointers (and the
// vector<bool>'s size) zeroed - the allocator words are not written - then blend 1.0f, fade-off
// (200, 300), width 1.0f, the +0x5C / +0x60 / +0x64 zeros, the name, priority 0. Then the default
// contents: two empty texture files (each through a temporary std::string("")), two (1, 1) scales,
// two false mirror flags, and one 0.25f max tesselated error (the inline push_back).
// PE 0x1013A700
RoadTemplate::RoadTemplate()
    : m_refCount(1)
    , m_blendFactor(1.0f)
    , m_fadeOffDistance{200.0f, 300.0f}
    , m_width(1.0f)
    , m_unknown5C(0)
    , m_unknown60(0.0f)
    , m_material(0)
    , m_priority(0)
{
    m_textureFiles.push_back(::renddx9abi::HostString(""));
    m_textureFiles.push_back(::renddx9abi::HostString(""));

    Vec2 unit;
    unit.x = 1.0f;
    unit.y = 1.0f;
    m_textureScales.push_back(unit);
    unit.x = 1.0f;
    unit.y = 1.0f;
    m_textureScales.push_back(unit);

    m_mirrors.push_back(false);
    m_mirrors.push_back(false);

    m_maxTesselatedErrors.push_back(0.25f);                                  // inline; _Insert_n 0x1004E420
}

// the members in reverse: the name, the error vector, the mirror flags, the scales, the texture
// files (sub_100111E0). The C++ member order is the shipped one.
// PE 0x1013A690
RoadTemplate::~RoadTemplate()
{
}


// IBase.

void RoadTemplate::addRef()
{
    ++m_refCount;                                                            // (shared)
}

int RoadTemplate::getRef()
{
    return m_refCount;                                                       // (shared)
}

// 0x1013ADF0: `--ref`; at zero the destructor body (called directly) and the engine heap.
int RoadTemplate::release()
{
    const int count = --m_refCount;
    if (!count)
    {
        this->~RoadTemplate();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// 0x1013ACF0: 1 answers `this`; otherwise `this & ((iid != 820008) - 1)` - the branch-free
// "820008 ? this : 0".
void* RoadTemplate::queryInterface(unsigned int iid) const
{
    RoadTemplate* const self = const_cast<RoadTemplate*>(this);
    if (iid == RD9_IID_IBASE)
        return static_cast<IRoadTemplate*>(self);
    return iid == kIID_IRoadTemplate ? static_cast<IRoadTemplate*>(self) : 0;
}


// The accessors. None checks its index (shipped).


// PE 0x1013A1A0: std::string::operator= on the element.
void RoadTemplate::setTextureFile(int index, const ::renddx9abi::HostString& file)
{
    m_textureFiles[static_cast<unsigned int>(index)] = file;
}

// PE 0x1013A1D0: the element's address.
const ::renddx9abi::HostString& RoadTemplate::getTextureFile(int index)
{
    return m_textureFiles[static_cast<unsigned int>(index)];
}

// PE 0x1013A1F0: y through the FPU, x as a dword: a Vec2 copy into the element.
void RoadTemplate::setTextureScale(int index, const Vec2& scale)
{
    Vec2& element = m_textureScales[static_cast<unsigned int>(index)];
    element.y = scale.y;
    element.x = scale.x;
}

// PE 0x1013A210: the element's address.
const Vec2& RoadTemplate::getTextureScale(int index)
{
    return m_textureScales[static_cast<unsigned int>(index)];
}

// PE 0x1013A650: the inline bit or / and-not on the word `index >> 5` (unsigned `shr`).
void RoadTemplate::setMirror(unsigned int index, bool mirror)
{
    m_mirrors[index] = mirror;
}

// PE 0x1013A620: the inline bit test, answered as 0 / 1 (`neg / sbb / neg`).
bool RoadTemplate::getMirror(unsigned int index)
{
    return m_mirrors[index];
}

// PE 0x1013A0F0
void  RoadTemplate::setWidth(float width)           { m_width = width; }
float RoadTemplate::getWidth()                      { return m_width; }

// PE 0x1013A220: element `lod` of the float vector (+0x3C), unchecked.
void RoadTemplate::setMaxTesselatedError(int lod, float error)
{
    m_maxTesselatedErrors[static_cast<unsigned int>(lod)] = error;
}

// PE 0x1013A240
float RoadTemplate::getMaxTesselatedError(int lod)
{
    return m_maxTesselatedErrors[static_cast<unsigned int>(lod)];
}

// PE 0x1013A110
void  RoadTemplate::setBlendFactor(float factor)    { m_blendFactor = factor; }
float RoadTemplate::getBlendFactor()                { return m_blendFactor; }

// PE 0x1013A180: y through the FPU, x as a dword.
void RoadTemplate::setFadeOffDistance(const Vec2& distance)
{
    m_fadeOffDistance.y = distance.y;
    m_fadeOffDistance.x = distance.x;
}

const Vec2& RoadTemplate::getFadeOffDistance()      { return m_fadeOffDistance; }

// PE 0x10178BC0 (ICF with the ParticleSystemTemplate +0x84 dword setter: one dword store at +0x84)
void RoadTemplate::setPriority(int priority)        { m_priority = priority; }
int  RoadTemplate::getPriority()                    { return m_priority; }
// PE 0x10178B40 (ICF with the ParticleSystemTemplate +0x64 dword setter: one dword store at +0x64)
void RoadTemplate::setMaterial(int material)        { m_material = material; }
int  RoadTemplate::getMaterial()                    { return m_material; }

// 0x1013AD20 - `add ecx, 68h; jmp std::string::operator=`.
void RoadTemplate::setName(const ::renddx9abi::HostString& name)
{
    m_name = name;
}

// PE 0x1013AD30: a copy of the name into the caller's return slot.
::renddx9abi::HostString RoadTemplate::getName()
{
    return m_name;
}

// PE 0x1013A140
void  RoadTemplate::setUnknown5C(int value)         { m_unknown5C = value; }
int   RoadTemplate::getUnknown5C()                  { return m_unknown5C; }
// PE 0x1013A160
void  RoadTemplate::setUnknown60(float value)       { m_unknown60 = value; }
float RoadTemplate::getUnknown60()                  { return m_unknown60; }


// PE 0x1013A260 (slot +0x60 of IRoadTemplate's table 0x1021D610)
// Writes the template's _compiled.con and _compiled.dat beside each other.
// Requires writable output streams; failures are unchecked and the result is always true.
bool RoadTemplate::compile(const ::renddx9abi::HostString& path)
{
    ::renddx9abi::HostString conFile(path);
    conFile += "\\";
    conFile += getName();                                                    // [eax+5Ch] #1
    conFile += "_compiled.con";

    ::renddx9abi::HostString datFile(path);
    datFile += "\\";
    datFile += getName();                                                    // #2
    datFile += "_compiled.dat";

    ::renddx9abi::HostStream* const stream = ::dice::hfe::fileManager->open(
        conFile,
        static_cast< ::renddx9abi::HostFileMode>(0),
        static_cast< ::renddx9abi::HostFileAccess>(2), 0);                   // FileManager +0x18

    // Three lines through an ostringstream; `endl` puts '\n' and flushes the string buffer (no effect).
    std::ostringstream text;
    text << "GeometryTemplate.create RoadCompiled " << getName() << std::endl;   // #3
    text << "ObjectTemplate.create SimpleObject " << getName() << std::endl;     // #4
    text << "ObjectTemplate.geometry " << getName() << std::endl;                // #5

    // str() is taken twice: once for the size, once for the data (0x1013A4CD / 0x1013A4E0).
    stream->write(text.str().c_str(), static_cast<unsigned int>(text.str().size()));   // +0x14, unguarded
    stream->release();                                                       // +0x08

    TerrainRawReader raw;
    raw.open(::renddx9abi::fromHostString(datFile), 0, 2, 0);

    std::string line0(::renddx9abi::fromHostString(m_textureFiles[0u]));
    line0 += '\n';                                                           // operator+=(char) 0x0A
    raw.writeString(line0);

    std::string line1(::renddx9abi::fromHostString(m_textureFiles[1u]));
    line1 += '\n';
    raw.writeString(line1);

    raw.writeF32(m_blendFactor);
    raw.writeVec2(&m_fadeOffDistance);
    raw.writeI32(m_priority);                                                // sub_10076160, the int writer
    raw.close();
    return true;
}


// RefFactory refuse aggregation, the object (retail 0x88 bytes), construct, QI with NO null
// test on the object (shipped), release on refusal.

IBase* __fastcall createRoadTemplate(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(RoadTemplate));
    RoadTemplate* self = 0;
    if (storage)
        self = new (storage) RoadTemplate();

    IBase* const result = static_cast<IBase*>(self->queryInterface(iid));    // [eax+0Ch]
    if (!result)
        self->release();                                                     // [eax+8]
    return result;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
