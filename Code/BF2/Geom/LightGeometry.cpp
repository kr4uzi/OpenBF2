// Owns the named texture selected through ILightGeometry.
// [INFERRED] source placement from the reflection class and its factory; no __FILE__ witness.

#ifdef BF2_IMAGE_RENDDX9

#include "../Core/IBase.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/Texture.h"
#include "../../RendDX9Abi/BfMemory.h"

#include <new>
#include <string>

namespace dice {
namespace hfe {
namespace geom {

// Reflection class id at 0x1021AF24.
extern const unsigned int CID_LightGeometry = 0x3103A73Cu;

namespace {

const unsigned int kIID_ILightGeometry = 0x0F3EFCFBu;

void releaseLightTexture(dx9rend::EngineTexture* texture)
{
    if (texture && --texture->refCount == 0)
    {
        dx9rend::textureDtor(texture);
        ::renddx9abi::engineFree(texture);
    }
}

// The five entries at 0x1021D1C8 are IBase followed by the texture-name setter.
class LightGeometry : public IBase
{
public:
    LightGeometry();
    ~LightGeometry();

    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;

    // [INFERRED] name for the sole added virtual; an empty or unchanged name keeps the texture.
    virtual void setTextureName(const std::string& name);

private:
    int m_refCount;                          // retail +0x04
    dx9rend::EngineTexture* m_texture;        // retail +0x08, owning reference
    std::string m_textureName;               // retail +0x0C
};

// PE 0x10132970
LightGeometry::LightGeometry()
    : m_refCount(1), m_texture(0)
{
}

// PE 0x101328C0
LightGeometry::~LightGeometry()
{
    releaseLightTexture(m_texture);
}

// PE 0x1000CB20, shared IBase increment body.
void LightGeometry::addRef()
{
    ++m_refCount;
}

// PE 0x101A9A10, shared count getter.
int LightGeometry::getRef()
{
    return m_refCount;
}

// PE 0x101329D0
int LightGeometry::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        this->~LightGeometry();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// PE 0x101329A0
void* LightGeometry::queryInterface(unsigned int iid) const
{
    if (iid == 1u || iid == kIID_ILightGeometry)
        return const_cast<LightGeometry*>(this);
    return 0;
}

// PE 0x10132900
void LightGeometry::setTextureName(const std::string& name)
{
    if (name.empty() || name == m_textureName)
        return;

    m_textureName = name;
    releaseLightTexture(m_texture);
    // The old texture slot stays published until the new lookup returns.
    m_texture = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerGetTexture(m_textureName));
}

} // anonymous namespace

// PE 0x100B0895
IBase* __fastcall createLightGeometry(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(LightGeometry));
    LightGeometry* const self = storage ? new (storage) LightGeometry() : 0;
    IBase* const result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
