// Shared view of the geometry-template registry. The concrete manager is private.
// Reserved entries preserve ABI positions and must not be called.

#ifndef BF2_GEOM_GEOMETRYTEMPLATEMANAGER_H
#define BF2_GEOM_GEOMETRYTEMPLATEMANAGER_H

#include <cstdint>
#include <vector>
#include "../../RendDX9Abi/HfeAbi.h"
#include <string>
#include "../Core/IBase.h"

#include <map>

namespace dice {
namespace hfe {
namespace geom {

class IGeometryTemplate;

// Optimizer interface prefix, IID 80103 (0x138E7). The concrete mesh optimizers extend this prefix.
// PE tables 0x1021D208/0x1021D2E0: +0x10 returns the class ID; +0x14 returns AL=false.
class IGeometryOptimizer : public IBase
{
public:
    virtual unsigned int getClassId() const = 0;
    virtual bool optimize() = 0;
};

// The registry's key order: `_stricmp` over the two names, the instruction-proven comparison. A
// name keeps the spelling it was first registered with. Defined in GeometryTemplateManager.cpp.
struct GeometryTemplateNameLess
{
    bool operator()(const std::string& a, const std::string& b) const;
};

// name -> owning template pointer, in name order.
typedef std::map<std::string, IGeometryTemplate*, GeometryTemplateNameLess>
    GeometryTemplateRegistry;

#define BF2_GTM_RESERVED(offset) virtual void reserved_##offset() = 0

class IGeometryTemplateManager : public IBase
{
public:
    // Cross-image methods use HostString, then convert once to the internal string storage.
    // Do not expose std::string directly to a native-host caller.
    virtual void registerTemplate(IGeometryTemplate* value,
                                  const ::renddx9abi::HostString& name) = 0;    // +0x10
    virtual IGeometryTemplate* createTemplate(unsigned int classId,
                                              const ::renddx9abi::HostString& name) = 0; // +0x14
    virtual IGeometryTemplate* getTemplate(const ::renddx9abi::HostString& name) = 0;    // +0x18
    virtual void setActive(IGeometryTemplate* value) = 0;                        // +0x1C
    virtual IGeometryTemplate* getActive() const = 0;                            // +0x20
    virtual void removeTemplate(const ::renddx9abi::HostString& name) = 0;       // +0x24
    virtual void clear() = 0;                                                    // +0x28

    // PE 0x1015D940 / 0x1015D430: an always-empty static vector and a static empty string.
    // [CHOSEN, not recovered] element and argument type (unobservable-type exception, AGENTS.md).
    virtual const std::vector<std::uint32_t>& slot_0x2C() = 0;                  // +0x2C
    virtual const ::renddx9abi::HostString& slot_0x30(std::uint32_t unused) = 0; // +0x30
    BF2_GTM_RESERVED(34);
    BF2_GTM_RESERVED(38);
    // PE 0x1015D9C0/0x1015D7C0/0x1015D740, called by ResourceManager's load lifecycle.
    // Names are [INFERRED]; signatures and the template-use byte at +4 are instruction-proven.
    virtual void loadUsedTemplates() = 0;                                // +0x3C
    virtual void resetTemplateUse() = 0;                                 // +0x40
    virtual int getUsedTemplateCount() = 0;                              // +0x44
    BF2_GTM_RESERVED(48);
    // Names are [INFERRED]. Returned optimizers are borrowed from the owning registry.
    virtual IGeometryOptimizer* createOptimizer(unsigned int classId, const std::string& key) = 0; // +0x4C
    virtual IGeometryOptimizer* registerOptimizer(const std::string& className,
                                                 const std::string& key) = 0;    // +0x50
    virtual bool runOptimizers() = 0;                                            // +0x54
    virtual void clearOptimizers() = 0;                                         // +0x58
    virtual void visitTemplateClassIds() = 0;                                    // +0x5C
    // Returns the case-insensitive name/template registry for ordered traversal.
    virtual GeometryTemplateRegistry* getTemplateRegistry() = 0;           // +0x60

protected:
    ~IGeometryTemplateManager() {}
};

// ModuleGeom.cpp, dword_1024B8F4 (G1 verifier: 0x1024B910 is
// g_RoadEditableRenderer). The concrete object is created by createGeometryTemplateManager.
extern IBase* g_GeometryTemplateManager;

// The manager's +0x48 slot (PE 0x1015D530): re-arms the three mesh technique gates after the
// shader set changed. Typed entry for callers outside this module (VideoOptions' ECCore arm).
void geometryTemplateManagerRearmTechniques(IBase* manager);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_GEOMETRYTEMPLATEMANAGER_H
