// Bundle template owning a skeleton and animation-system templates. [INFERRED] Source placement.
// The original ELF base occupies 404 bytes, including two unidentified trailing words; native STL layouts differ.

#ifndef GAME_OBJECTS_ANIMATEDBUNDLETEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_ANIMATEDBUNDLETEMPLATE_H

#include "Base/BundleTemplate.h"
#include <string>

namespace dice {
namespace anim {
class IAnimationSystemTemplate;
class Skeleton;
}
namespace hfe {
namespace world {

class AnimatedBundle;                            // the object half reads the three pointers (Game/Objects/Game/)
class AnimatedBundleTemplate : public BundleTemplate
{
public:
    friend class AnimatedBundle;                 // the ctor's three template reads (+0x180 / +0x184 / +0x188)
    // the header had a layout and no methods at all.
    AnimatedBundleTemplate();                        // refcount 1, three empty strings
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;
    virtual unsigned int getClassID() const;
    virtual ~AnimatedBundleTemplate();
    virtual void* queryImplementation(unsigned int);
    virtual IObject* createObject() const;           // new(0x154) AnimatedBundle(this)
    virtual bool makeScript(io::IStream* stream);

    // The three ObjectTemplate authoring methods. Each resolves its path immediately to the
    // object stored below; the animation systems themselves defer reading their .inc files until
    // ECCore event 3. None of the three retail bodies releases a previously assigned value.
    void skeleton(const std::string& name);
    void animationSystem1P(const std::string& name);
    void animationSystem3P(const std::string& name);

protected:
    // layout, offsets 380..403. BundleTemplate occupies 0..379.
    /* 380 */ int         m_refCount3;

    // Resolved resources: the template owns the skeleton and releases animation templates through their manager.
    /* 384 */ anim::Skeleton* m_skeleton;            // owned - the dtor deletes it
    /* 388 */ anim::IAnimationSystemTemplate* m_animationSystem1P; // manager-owned
    /* 392 */ anim::IAnimationSystemTemplate* m_animationSystem3P; // manager-owned

    // [UNRESOLVED] two dwords the IDB never identified. Their existence is proven by RTTI (see the
    // header block); only their meaning is unknown. Do not remove - every offset in
    // GenericFireArmTemplate and MaskObjectTemplate depends on them.
    /* 396 */ char _pad18C[8];
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_ANIMATEDBUNDLETEMPLATE_H
