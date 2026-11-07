// Blob shadows as decals: for a geometry instance, finds the surface below it and places a
// "DecalShadowTemplate" decal there, sized by the geometry's bounding radius and bound to the
// geometry's shadow record. [INFERRED] header placement beside the attested
// Geom/DecalSystem/DecalShadowManager.cpp, and every method name. No shipped image calls
// clearShadows or addShadow; in 1.50 the singleton only registers its decal template.

#ifndef BF2_GEOM_DECALSYSTEM_DECALSHADOWMANAGER_H
#define BF2_GEOM_DECALSYSTEM_DECALSHADOWMANAGER_H

#ifdef BF2_IMAGE_RENDDX9

#include "../../Core/IBase.h"

#include <vector>

namespace dice {
namespace hfe {
namespace geom {

class DecalObject;
class IDecalTemplate;

// The decal-shadow interface: six slots, no destructor (release destroys the object directly).
// IID 820021, class id 821020, singleton "DecalShadowManager" (dword_1025BED8).
class IDecalShadowManager : public IBase
{
public:
    // Hands every placed shadow decal back to the decal manager's deletion queue.
    virtual void clearShadows() = 0;                                      // +0x10
    // Casts a ray 10 units down from the top of `geometry`'s bounding sphere, places a shadow decal
    // at the hit (on the hit object's collision mesh when an object was hit) and binds it to the
    // geometry's shadow record. `geometry` is a geometry instance (StaticMesh, SkinnedMesh,
    // BundledMesh). False when nothing was hit, the hit object has no collision mesh, the mesh
    // query misses or the decal manager refuses a client.
    virtual bool addShadow(void* geometry) = 0;                           // +0x14

protected:
    ~IDecalShadowManager() {}
};

// DecalShadowManager - 0x1C bytes, vtable 0x1021CB9C, constructor 0x10127160, factory 0x100B0793.
class DecalShadowManager : public IDecalShadowManager
{
public:
    DecalShadowManager();                                                 // 0x10127160
    ~DecalShadowManager();                                                // 0x101271D0

    virtual void  addRef();                                               // 0x1000CB20 (folded)
    virtual int   getRef();                                               // 0x101A9A10 (folded)
    virtual int   release();                                              // 0x101274E0
    virtual void* queryInterface(unsigned int iid) const;                 // 0x10127460

    virtual void clearShadows();                                          // 0x10127200
    virtual bool addShadow(void* geometry);                               // 0x10127260

private:
    DecalShadowManager(const DecalShadowManager&);
    DecalShadowManager& operator=(const DecalShadowManager&);

    // data, in shipped order
    int                        m_refCount;      // +0x04
    std::vector<DecalObject*>  m_shadows;       // +0x08  VC7.1 vector (allocator, first, last, end)
    IDecalTemplate*            m_template;      // +0x18  "DecalShadowTemplate", size 1
};

// dword_1025BED8. ModuleGeom.cpp owns the definition.
extern IBase* g_DecalShadowManager;

// The factory registered under "dice.hfe.geom.DecalShadowManager" (0x100B0793).
IBase* __fastcall createDecalShadowManager(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
#endif // BF2_GEOM_DECALSYSTEM_DECALSHADOWMANAGER_H
