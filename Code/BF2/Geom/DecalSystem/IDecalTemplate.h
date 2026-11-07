// Renderer decal-template contract shared with world objects.
// Cross-image strings use HostString; field offsets describe the renderer object only.
// The destructor is last, so preceding slots agree between the PE and ELF interfaces.
#ifndef BF2_GEOM_DECALSYSTEM_IDECALTEMPLATE_H
#define BF2_GEOM_DECALSYSTEM_IDECALTEMPLATE_H

#include "../../Math/Vec3.h"

#if defined(BF2_IMAGE_RENDDX9)
#include "../../../RendDX9Abi/PluginAbi.h"
#else
#include <string>
#endif

namespace dice {
namespace hfe {
namespace geom {

#if defined(BF2_IMAGE_RENDDX9)
typedef ::renddx9abi::HostString DecalTemplateString;
#else
typedef std::string DecalTemplateString;
#endif

class IDecalTemplate
{
public:
    virtual const DecalTemplateString& getName() const = 0;               // +0x00 / +0x5C
    // +0x04 101AFA10 (85 B). Every world::DecalTemplate setter calls it after its set, and
    // templateInitialize once after addTemplate. [INFERRED] name - its body recomputes the animation
    // frame data from +0x98 (the frame count across).
    virtual void  refresh() = 0;                                          // +0x04
    virtual void  setSize(float size) = 0;                                // +0x08 / +0x18
    virtual float getSize() const = 0;                                    // +0x0C
    virtual void  setRandomSize(float randomSize) = 0;                    // +0x10 / +0x1C
    virtual float getRandomSize() const = 0;                              // +0x14
    virtual void  setRandomColorIntensity(float intensity) = 0;          // +0x18 / +0x04
    virtual float getRandomColorIntensity() const = 0;                    // +0x1C
    virtual void  setColor(const Vec3& color) = 0;                        // +0x20 / +0x08
    virtual Vec3  getColor() const = 0;                                   // +0x24 (hidden ptr); body DecalTemplate::getColor
    virtual void  setIsStatic(bool isStatic) = 0;                         // +0x28 / +0x20
    virtual bool  getIsStatic() const = 0;                                // +0x2C
    virtual void  setUsedOnTransparentMaterials(bool used) = 0;           // +0x30 / +0x21
    virtual bool  getUsedOnTransparentMaterials() const = 0;              // +0x34
    // +0x38 / +0x3C: the UV extent (+0xA0) - refresh / updateUv compute it (the atlas entry's size,
    // or one animation frame) and DecalObject reads it. No world::DecalTemplate accessor and no
    // console row; [INFERRED] name (D2).
    virtual void  setUvExtent(const Vec2& extent) = 0;                    // +0x38
    virtual Vec2  getUvExtent() const = 0;                                // +0x3C 101AF470 (hidden ptr)
    virtual void  setRandomRotation(float randomRotation) = 0;            // +0x40 / +0x24
    virtual float getRandomRotation() const = 0;                          // +0x44
    virtual void  setRotation(float rotation) = 0;                        // +0x48 / +0x28
    virtual float getRotation() const = 0;                                // +0x4C
    virtual void  setDecalTextureName(const DecalTemplateString& name) = 0; // +0x50 / +0x3C
    virtual const DecalTemplateString& getDecalTextureName() const = 0;   // +0x54
    virtual void  setAnimationEnable(bool enable) = 0;                    // +0x58 / +0x88
    virtual bool  getAnimationEnable() const = 0;                         // +0x5C
    virtual void  setAnimationRandomizedStartFrame(bool randomized) = 0;  // +0x60 / +0x80
    virtual bool  getAnimationRandomizedStartFrame() const = 0;           // +0x64
    virtual void  setAnimationSpeed(float speed) = 0;                     // +0x68 / +0x84
    virtual float getAnimationSpeed() const = 0;                          // +0x6C
    // +0x70..+0x7C: the frame size in texels, derived through the texture (+0x78). The ELF's
    // setters take `unsigned int` (mangled `j`); the getters' return type is [INFERRED] from them.
    virtual void  setAnimationFrameWidth(unsigned int width) = 0;         // +0x70
    virtual unsigned int getAnimationFrameWidth() const = 0;              // +0x74
    virtual void  setAnimationFrameHeight(unsigned int height) = 0;       // +0x78
    virtual unsigned int getAnimationFrameHeight() const = 0;             // +0x7C
    virtual void  setAnimationFrameWidthRelative(float width) = 0;        // +0x80 / +0x8C
    virtual float getAnimationFrameWidthRelative() const = 0;             // +0x84
    virtual void  setAnimationFrameHeightRelative(float height) = 0;      // +0x88 / +0x90
    virtual float getAnimationFrameHeightRelative() const = 0;            // +0x8C
    virtual void  setAnimationFrameCount(unsigned int count) = 0;         // +0x90 / +0x94
    virtual unsigned int getAnimationFrameCount() const = 0;              // +0x94
    virtual void  setAnimationFrameCountX(unsigned int count) = 0;        // +0x98
    virtual unsigned int getAnimationFrameCountX() const = 0;             // +0x9C
    // +0xA0 the scalar deleting destructor (the manager owns the templates).
    virtual ~IDecalTemplate() {}
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif
