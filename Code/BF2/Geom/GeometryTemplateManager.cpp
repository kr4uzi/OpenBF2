// Path:     attested - "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\GeometryTemplateManager.cpp"
//           appears verbatim in RendDX9.dll at 0x1021EAE8.
// Contents: BF2 1.50 PE recovery of the template registry/create/find/active ownership spine and
//           the stock-sky console subset.  There is no useful dedicated Linux renderer witness.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ModuleCommon.h"
#include <cstdint>
#include <string>
#include <vector>
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Core/EventManager.h"
#include "../RendDX9/RendDX9Base.h"
#include "../IO/Console/Console.h"          // getConsoleWorkingPath: typed (RW-266)
#include "MeshTemplate.h"
#include "BundledMeshTemplate.h"          // the geometryTemplate console's Bundled-only properties
#include "StaticMeshTemplate.h"           // ... and its StaticMesh-only fields
#include "SkinnedMeshTemplate.h"
#include "GeometryTemplateManager.h"
#include "../Core/StringUtil.h"
#include "../GeomMisc/Debug.h"

#include <new>
#include <string.h>

#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"

namespace dice {
namespace hfe {
namespace dx9rend { bool readLevelLoadBoolSetting(const char*, bool); }
namespace geom {

const unsigned int IID_IGeometryTemplate = 0x17701FFFu;
extern const unsigned int IID_IGeometryTemplateManager = 0xAD9665BCu;
extern const unsigned int CID_GeometryTemplateManager  = 0x870CA329u;

extern IBase* g_GeometryTemplateManager;

bool GeometryTemplateNameLess::operator()(const std::string& a, const std::string& b) const
{
    return ::_stricmp(a.c_str(), b.c_str()) < 0;
}

// Slot +0x10 on the interface prefix is still unknown.  Do not invent a declaration merely to
// reach the instruction-proven +0x14/+0x1C setters: keep the owner opaque and isolate those two
// exact x86 calls below.  IBase's four-slot prefix is independently established and remains typed.
class IGeometryTemplate;

namespace {

IBase* templateBase(IGeometryTemplate* value)
{
    return reinterpret_cast<IBase*>(value);
}

void setTemplateName(IGeometryTemplate* value, const std::string& name)
{
    typedef void (__thiscall *Fn)(void*, const std::string&);
    void* target = (*reinterpret_cast<void***>(value))[0x14 / 4];
    reinterpret_cast<Fn>(target)(value, name);
}

void setTemplatePath(IGeometryTemplate* value, const std::string& path)
{
    typedef void (__thiscall *Fn)(void*, const std::string&);
    void* target = (*reinterpret_cast<void***>(value))[0x1C / 4];
    reinterpret_cast<Fn>(target)(value, path);
}

// The owning map's pair destructor at 0x100B1174 releases its interface value first and destroys
// its key second.
void releaseMappedInterface(IGeometryTemplate* value)
{
    IBase* const base = templateBase(value);
    if (base)
        base->release();
}

// Unlinks one entry, then releases its interface - the observable re-entrant ordering of
// PE 0x100C7371: a re-entrant lookup during the release no longer finds the entry.
void eraseOwnedEntry(GeometryTemplateRegistry& map, GeometryTemplateRegistry::iterator entry)
{
    IGeometryTemplate* const value = entry->second;
    map.erase(entry);
    releaseMappedInterface(value);
}

// std::map::_Erase from the root: right subtree, node, left subtree - descending key order -
// with the tree still linked while each interface is released; then the map is relinked empty.
template <class Registry>
void clearOwnedMap(Registry& map)
{
    for (typename Registry::reverse_iterator it = map.rbegin(); it != map.rend(); ++it)
        if (it->second)
            reinterpret_cast<IBase*>(it->second)->release();
    map.clear();
}

// clear, then the head node. The owner frees its storage afterwards without running a C++
// destructor, so the map is destroyed here.
template <class Registry>
void destroyOwnedMap(Registry& map)
{
    clearOwnedMap(map);
    map.~Registry();
}

// A non-owning, manually-lived slot lets release() reproduce retail's exact member teardown order:
// optimizer map, vector, string, active reference, template map.  A normal C++ string member would
// be destroyed after the destructor body and therefore move it past the active release.
union ManualStringStorage
{
    char  bytes[sizeof(std::string)];
    void* alignment;

    ManualStringStorage() {}
    ~ManualStringStorage() {}

    std::string& get() { return *reinterpret_cast<std::string*>(bytes); }
};

static_assert(sizeof(ManualStringStorage) == sizeof(std::string),
              "manual string storage must match the string");

const renddx9abi::HostString& getConsoleWorkingPath()
{
    // The published pointer addresses the Console object even though it has no IBase base.
    return reinterpret_cast<io::Console*>(mainConsole)->getWorkingPath();
}

} // anonymous namespace

// Primary vtable 0x10218E48, exactly 25 slots.  Names on the stripped-PE methods are semantic;
// slot positions, signatures, ownership, call ordering, and return behavior are instruction-proven.
class GeometryTemplateManager : public IBase
{
public:
    GeometryTemplateManager();

    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    virtual void registerTemplate(IGeometryTemplate* value,
                                  const ::renddx9abi::HostString& name);      // +0x10
    virtual IGeometryTemplate* createTemplate(unsigned int classId,
                                               const ::renddx9abi::HostString& name); // +0x14
    virtual IGeometryTemplate* getTemplate(const ::renddx9abi::HostString& name);      // +0x18
    virtual void setActive(IGeometryTemplate* value);                          // +0x1C
    virtual IGeometryTemplate* getActive() const;                              // +0x20
    virtual void removeTemplate(const ::renddx9abi::HostString& name);         // +0x24
    virtual void clear();                                                       // +0x28
    // [CHOSEN, not recovered] element and argument type, under the unobservable-type exception.
    virtual const std::vector<std::uint32_t>& slot_0x2C();                     // +0x2C
    virtual const ::renddx9abi::HostString& slot_0x30(std::uint32_t unused);   // +0x30
    virtual void slot_0x34();
    virtual void slot_0x38();
    virtual void loadUsedTemplates();                                        // +0x3C
    virtual void resetTemplateUse();                                         // +0x40
    virtual int getUsedTemplateCount();                                      // +0x44
    virtual void slot_0x48();
    virtual IGeometryOptimizer* createOptimizer(unsigned int classId,
                                                const std::string& key);       // +0x4C
    virtual IGeometryOptimizer* registerOptimizer(const std::string& className,
                                                   const std::string& key);    // +0x50
    virtual bool runOptimizers();                                               // +0x54
    virtual void clearOptimizers();                                             // +0x58
    virtual void visitTemplateClassIds();                                       // +0x5C
    virtual GeometryTemplateRegistry* getTemplateRegistry();                    // +0x60

private:
    void destroyOwnedMembers();
    IGeometryTemplate* createQualified(const std::string& typeName,
                                       const std::string& fullName);

    int                 m_refCount;        // +0x04
    GeometryTemplateRegistry m_templates;  // +0x08, no-case string -> owning template pointer
    IGeometryTemplate*  m_active;          // +0x14, owning
    ManualStringStorage m_string18;        // +0x18, purpose unresolved
    // [UNRESOLVED] element type: nothing recovered reads or fills it.
    std::vector<unsigned char> m_vector34; // +0x34
    // PE 0x1015E4F0 uses the same _stricmp comparator as the template registry.
    typedef std::map<std::string, IGeometryOptimizer*, GeometryTemplateNameLess> OptimizerRegistry;
    OptimizerRegistry m_optimizers; // +0x44, owning
};

// Retail allocation (RefFactory_geom_GeometryTemplateManager) is 0x50 bytes. Both maps are
// default-constructed; the string slot is built in place.
// PE 0x100C7E95
GeometryTemplateManager::GeometryTemplateManager()
    : m_refCount(1)
    , m_active(0)
{
    new (m_string18.bytes) std::string();
}

void GeometryTemplateManager::addRef()
{
    ++m_refCount;
}

int GeometryTemplateManager::getRef()
{
    return m_refCount;
}

// PE 0x100C8751: the members in this exact order.
void GeometryTemplateManager::destroyOwnedMembers()
{
    destroyOwnedMap(m_optimizers);

    typedef std::vector<unsigned char> UnresolvedVector;
    m_vector34.~UnresolvedVector();

    m_string18.get().~basic_string();

    if (m_active)
        templateBase(m_active)->release();

    destroyOwnedMap(m_templates);
}

// PE 0x100C8732
int GeometryTemplateManager::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        destroyOwnedMembers();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// PE 0x100C7ED0
void* GeometryTemplateManager::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IGeometryTemplateManager)
        return const_cast<GeometryTemplateManager*>(this);
    return 0;
}

// PE 0x1015D470
void GeometryTemplateManager::setActive(IGeometryTemplate* value)
{
    if (m_active)
    {
        // Retail clears the field only after release; re-entrant getActive therefore still sees
        // the old pointer during its final callback.
        templateBase(m_active)->release();
        m_active = 0;
    }

    m_active = value;
    if (value)
        templateBase(value)->addRef();
}

IGeometryTemplate* GeometryTemplateManager::getActive() const
{
    return m_active;
}

// PE 0x1015DC30
void GeometryTemplateManager::registerTemplate(IGeometryTemplate* value,
                                               const ::renddx9abi::HostString& hostName)
{
    if (!value)
        return;

    // The registry's own key copy of the host string.
    const std::string& name = ::renddx9abi::fromHostString(hostName);

    // operator[]: the existing entry, or a fresh one holding null.
    const GeometryTemplateRegistry::iterator node =
        m_templates.insert(
            GeometryTemplateRegistry::value_type(name, static_cast<IGeometryTemplate*>(0))).first;

    IBase* old = templateBase(node->second);
    if (old)
    {
        old->release();
        node->second = 0;
    }

    node->second = value;
    templateBase(value)->addRef();
    setTemplateName(value, name);
    setActive(value);
}

// PE 0x1015DC80
IGeometryTemplate* GeometryTemplateManager::createTemplate(unsigned int classId,
                                                           const ::renddx9abi::HostString& hostName)
{
    const std::string& name = ::renddx9abi::fromHostString(hostName);

    const GeometryTemplateRegistry::iterator found = m_templates.find(name);
    if (found != m_templates.end())
    {
        IGeometryTemplate* value = found->second;
        setActive(value);
        return value;
    }

    IGeometryTemplate* value = reinterpret_cast<IGeometryTemplate*>(
        classManager->createInstance(classId, IID_IGeometryTemplate, 0));
    if (!value)
    {
        setActive(0);
        return 0;
    }

    // operator[] again: the factory may have registered the name in the meantime.
    const GeometryTemplateRegistry::iterator node =
        m_templates.insert(
            GeometryTemplateRegistry::value_type(name, static_cast<IGeometryTemplate*>(0))).first;

    node->second = value;
    reinterpret_cast<GeometryTemplate*>(value)->m_keepData = 0; // the shipped load/keep byte, +4
    templateBase(value)->addRef();                  // registry ownership
    setTemplateName(value, name);

    const renddx9abi::HostString& hostPath = getConsoleWorkingPath();
    const std::string templatePath(hostPath.data(),
                                    static_cast<unsigned int>(hostPath.size()));
    setTemplatePath(value, templatePath);

    setActive(value);
    templateBase(value)->release();                 // drop the factory reference
    return value;                                   // borrowed from registry/active
}

// PE 0x1015D4B0. The qualified class name is a copy of the appended temporary and is destroyed
// before the createTemplate slot (+0x14) runs.
IGeometryTemplate* GeometryTemplateManager::createQualified(const std::string& typeName,
                                                            const std::string& fullName)
{
    unsigned int classId = 0;
    bool found;
    {
        const std::string className = std::string("dice.hfe.geom.GeometryTemplate.") += typeName;
        found = classManager->getClassID(::renddx9abi::toHostString(className), &classId);
    }
    if (!found)
        return 0;
    // createTemplate is a host-typed slot; the internal caller passes a host string.
    return createTemplate(classId, ::renddx9abi::toHostString(fullName));
}

// PE 0x1015D820. A registry hit answers the stored template; a miss splits at the first ':' and asks
// createQualified for the prefix class.
IGeometryTemplate* GeometryTemplateManager::getTemplate(const ::renddx9abi::HostString& hostName)
{
    const std::string& name = ::renddx9abi::fromHostString(hostName);

    const GeometryTemplateRegistry::iterator found = m_templates.find(name);
    if (found != m_templates.end())
        return found->second;

    unsigned int colon = 0;
    while (colon != name.size() && name.c_str()[colon] != ':')
        ++colon;
    if (colon == name.size())
        return 0;

    const std::string prefix(name.c_str(), colon);
    const std::string suffix(name.c_str() + colon + 1, name.size() - colon - 1);
    (void)suffix; // Constructed and destroyed by retail, but never read (shipped quirk).
    return createQualified(prefix, name); // The full `type:name`, not the suffix, is the map key.
                                          // (createQualified re-enters createTemplate, which is
                                          //  a host-typed slot - see its toHostString below.)
}

// PE 0x1015D980
void GeometryTemplateManager::removeTemplate(const ::renddx9abi::HostString& hostName)
{
    const std::string& name = ::renddx9abi::fromHostString(hostName);

    const GeometryTemplateRegistry::iterator node = m_templates.find(name);
    if (node != m_templates.end())
        eraseOwnedEntry(m_templates, node);
}

// PE 0x1015DB90
void GeometryTemplateManager::clear()
{
    if (m_active)
    {
        templateBase(m_active)->release();
        m_active = 0;
    }
    clearOwnedMap(m_templates);
    clearOwnedMap(m_optimizers);
}

// PE 0x1015D940: a function-local static vector (0x102605FC, guard 0x1026060C) that nothing ever
// fills, returned by address. BF2.exe saveAllUsedTemplates walks its four-byte entries
// (0x00734703) and passes each nonzero one to +0x30 (0x00734727); it is always empty.
// [CHOSEN, not recovered] std::uint32_t element (unobservable-type exception, AGENTS.md): no
// image stores an element. _r 0x102CA220, editor 0x103152F0.
const std::vector<std::uint32_t>& GeometryTemplateManager::slot_0x2C()
{
    static std::vector<std::uint32_t> entries;
    return entries;
}

// PE 0x1015D430 (`retn 4`): the four-byte argument is never read; the answer is a function-local
// static empty string (0x102605DC, guard 0x102605F8). [CHOSEN, not recovered] argument type
// (unobservable-type exception). _r 0x102C9010, editor 0x10315290.
const ::renddx9abi::HostString& GeometryTemplateManager::slot_0x30(std::uint32_t)
{
    static const ::renddx9abi::HostString empty;
    return empty;
}

// PE 0x100C7EF8: both primary-table entries point to the same bare return.
void GeometryTemplateManager::slot_0x34() {}
// PE 0x100C7EF8.
void GeometryTemplateManager::slot_0x38() {}

// PE 0x1015D7C0. Clear each template's use byte without changing registry ownership.
void GeometryTemplateManager::resetTemplateUse()
{
    for (GeometryTemplateRegistry::iterator node = m_templates.begin();
         node != m_templates.end(); ++node)
        reinterpret_cast<GeometryTemplate*>(node->second)->m_keepData = 0;
}

// PE 0x1015D740. The ResourceManager gate precedes the walk; loaded state is not tested.
int GeometryTemplateManager::getUsedTemplateCount()
{
    if (!dx9rend::resourceManagerIsReady())
        return 0;
    int count = 0;
    for (GeometryTemplateRegistry::iterator node = m_templates.begin();
         node != m_templates.end(); ++node)
        if (reinterpret_cast<GeometryTemplate*>(node->second)->m_keepData)
            ++count;
    return count;
}

// PE 0x1015D9C0. Source-level map erase preserves the PE's advance-before-release order.
void GeometryTemplateManager::loadUsedTemplates()
{
    if (!dx9rend::resourceManagerIsReady())
        return;

    struct Progress { int current; float weight; const char* label; };
    // The count is dispatched through the published singleton, not necessarily this receiver.
    const int count = static_cast<GeometryTemplateManager*>(g_GeometryTemplateManager)
                          ->getUsedTemplateCount();
    Progress progress = { count + 1000, 1.0f, "Geometries" };
    eventManager->postEvent(ECCore, 4, &progress, 0.0f);

    if (m_active)
    {
        templateBase(m_active)->release();
        m_active = 0;
    }
    if (!dx9rend::readLevelLoadBoolSetting("GSKeepTemplates", false))
    {
        GeometryTemplateRegistry::iterator node = m_templates.begin();
        while (node != m_templates.end())
        {
            const GeometryTemplateRegistry::iterator current = node;
            ++node;
            if (!reinterpret_cast<GeometryTemplate*>(current->second)->m_keepData)
                eraseOwnedEntry(m_templates, current);
        }
    }
    else
    {
        // This counterintuitive message belongs to the keep-templates branch in the PE.
        ::renddx9abi::logf("GeometryTemplateManager.cpp:309: Do not keep geom templates");
    }

    for (GeometryTemplateRegistry::iterator node = m_templates.begin();
         node != m_templates.end(); ++node)
    {
        GeometryTemplate* const value = reinterpret_cast<GeometryTemplate*>(node->second);
        if (value->m_keepData && !value->isLoaded())
            value->load();
        value->m_keepData = 0;
    }
}


// Technique initialization gates start pending and clear after successful initialization.
// [INFERRED] Owns the skinned and bundled technique gates; the static-mesh gate lives with its
// reader in StaticMeshTemplate.cpp.

extern unsigned char g_staticMeshTechniquesPending;        // byte_1023982C, StaticMeshTemplate.cpp
unsigned char g_skinnedMeshTechniquesPending = 1;          // byte_102397D4, initial value 1
unsigned char g_bundledMeshTechniquesPending = 1;          // byte_10239294, initial value 1

// The three technique initializers the priming tail calls. 0x10116EA0 is file-local to
// StaticMeshTemplate.cpp and reached through its bridge; the other two are BundledMeshTemplate.h
// and SkinnedMesh.h's published bodies.
void staticMeshPrimeTechniques();                          // 0x10116EA0
void bundledMeshInitTechniques();                          // 0x100E8F20
void skinnedMeshInitTechniques();                          // 0x1010F190

// Re-arms geometry technique initialization after shader archives change.
// [INFERRED] slot name.
// An empty registry returns after re-arming; a nonempty registry primes each shared table
// through a temporary "temp_rel" template of the matching class.
void GeometryTemplateManager::slot_0x48()
{
    // The three once-gates, in the shipped store order (0x1015D53B..0x1015D545). [INFERRED]
    // placement for the skinned and bundled gates: this translation unit holds their only writer.
    g_staticMeshTechniquesPending   = 1;   // byte_1023982C
    g_skinnedMeshTechniquesPending  = 1;   // byte_102397D4
    g_bundledMeshTechniquesPending  = 1;   // byte_10239294

    if (m_templates.size() == 0)           // cmp [esi+10h], 0 / jbe loc_1015D652 - unsigned
        return;                            // `result` is still the 1 loaded at 1015D536

    // 0x1015D555..0x1015D64B: per class, createTemplate (+0x14) with a temporary "temp_rel"
    // string, the initializer called with ecx = the returned template, then removeTemplate (+0x24)
    // with a fresh temporary. The initializers pass ecx on to meshBuildTechniqueMasks, which
    // overwrites it unread (0x101921E2), so the template is not an argument here; creating and
    // removing it is kept for its registry, factory and active-template side effects.
    createTemplate(0x870CA653u, ::renddx9abi::HostString("temp_rel"));    // dword_1021AF04
    staticMeshPrimeTechniques();                                          // 0x10116EA0
    removeTemplate(::renddx9abi::HostString("temp_rel"));
    createTemplate(0x870CA651u, ::renddx9abi::HostString("temp_rel"));    // dword_1021AEFC
    bundledMeshInitTechniques();                                          // 0x100E8F20
    removeTemplate(::renddx9abi::HostString("temp_rel"));
    createTemplate(0x870CA652u, ::renddx9abi::HostString("temp_rel"));    // dword_1021AF00
    skinnedMeshInitTechniques();                                          // 0x1010F190
    removeTemplate(::renddx9abi::HostString("temp_rel"));
}

// PE 0x1015DD30. Duplicate keys refuse creation; the factory may re-enter the registry.
// The interface id is dword_1021AE54 = 0x138E7 (80103), the one the optimizers' queryInterface
// 0x10134E70 accepts besides 1.
IGeometryOptimizer* GeometryTemplateManager::createOptimizer(unsigned int classId,
                                                             const std::string& key)
{
    if (m_optimizers.find(key) != m_optimizers.end())
        return 0;
    IGeometryOptimizer* const value = static_cast<IGeometryOptimizer*>(
        classManager->createInstance(classId, 80103u, 0));
    if (!value)
        return 0;
    m_optimizers[key] = value;
    value->addRef();
    value->release();
    return value;
}

// PE 0x1015D660. Returns the optimizer pointer, not a success Boolean.
IGeometryOptimizer* GeometryTemplateManager::registerOptimizer(const std::string& className,
                                                               const std::string& key)
{
    unsigned int classId = 0;
    bool found;
    {
        const std::string full(std::string("dice.hfe.geom.GeometryOptimizer.") += className);
        found = classManager->getClassID(renddx9abi::HostString(full.c_str()), &classId);
    }
    if (!found)
        return 0;
    return createOptimizer(classId, key);
}

// PE 0x1015D8D0. The optimizer results are discarded; this method always returns false.
bool GeometryTemplateManager::runOptimizers()
{
    for (OptimizerRegistry::iterator it = m_optimizers.begin(); it != m_optimizers.end(); ++it)
        it->second->optimize();
    return false;
}

// PE 0x1015D6E0. Even the discarded class-ID queries remain virtual calls in registry order.
void GeometryTemplateManager::visitTemplateClassIds()
{
    for (GeometryTemplateRegistry::iterator it = m_templates.begin(); it != m_templates.end(); ++it)
        it->second->getClassId();
}

void GeometryTemplateManager::clearOptimizers()
{
    clearOwnedMap(m_optimizers);
}

GeometryTemplateRegistry* GeometryTemplateManager::getTemplateRegistry()
{
    return &m_templates;
}

IBase* __fastcall createGeometryTemplateManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    GeometryTemplateManager* self = new GeometryTemplateManager();
    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}


void geometryTemplateManagerRearmTechniques(IBase* manager)
{
    static_cast<GeometryTemplateManager*>(manager)->slot_0x48();
}

namespace {

const char kGeometryTemplateConsoleTag[] = "geom.geometryTemplate.sky";

GeometryTemplateManager* templateManager()
{
    return static_cast<GeometryTemplateManager*>(g_GeometryTemplateManager);
}

// PE 0x100B111A. The second getActive call reloads the published manager.
IGeometryTemplate* activeGeometryTemplate()
{
    GeometryTemplateManager* const manager = templateManager();
    if (!manager)
        return 0;
    if (!manager->getActive())
        return 0;
    IGeometryTemplate* const active = templateManager()->getActive();
    return static_cast<IGeometryTemplate*>(
        templateBase(active)->queryInterface(IID_IGeometryTemplate));
}

// sub_100B1269 / sub_100B128C / sub_100B12AF - the active template when its class id
// (+0x10) is BundledMesh (0x870CA651, dword_1021AEFC), StaticMesh (0x870CA653, dword_1021AF04),
// or any of BundledMesh, StaticMesh, SkinnedMesh (0x870CA652, dword_1021AF00); null otherwise.
// The class id is fetched again for every comparison, as shipped.
BundledMeshTemplate* activeBundledMeshTemplate()
{
    IGeometryTemplate* const value = activeGeometryTemplate();
    if (value && value->getClassId() == 0x870CA651u)
        return static_cast<BundledMeshTemplate*>(value);
    return 0;
}

// PE 0x100B128C
StaticMeshTemplate* activeStaticMeshTemplate()
{
    IGeometryTemplate* const value = activeGeometryTemplate();
    if (value && value->getClassId() == 0x870CA653u)
        return static_cast<StaticMeshTemplate*>(value);
    return 0;
}

// PE 0x100B12AF
MeshTemplate* activeMeshTemplate()
{
    IGeometryTemplate* const value = activeGeometryTemplate();
    if (value && (value->getClassId() == 0x870CA651u || value->getClassId() == 0x870CA653u
                  || value->getClassId() == 0x870CA652u))
        return static_cast<MeshTemplate*>(value);
    return 0;
}

void* templateVirtual(IGeometryTemplate* value, unsigned int byteOffset)
{
    return (*reinterpret_cast<void***>(value))[byteOffset / 4];
}

bool getTemplateBool(unsigned int byteOffset)
{
    IGeometryTemplate* value = activeGeometryTemplate();
    typedef bool (__thiscall *Fn)(void*);
    return reinterpret_cast<Fn>(templateVirtual(value, byteOffset))(value);
}

void setTemplateBool(unsigned int byteOffset, bool setting)
{
    IGeometryTemplate* value = activeGeometryTemplate();
    typedef void (__thiscall *Fn)(void*, bool);
    reinterpret_cast<Fn>(templateVirtual(value, byteOffset))(value, setting);
}

int getTemplateInt(unsigned int byteOffset)
{
    IGeometryTemplate* value = activeGeometryTemplate();
    typedef int (__thiscall *Fn)(void*);
    return reinterpret_cast<Fn>(templateVirtual(value, byteOffset))(value);
}

void setTemplateInt(unsigned int byteOffset, int setting)
{
    IGeometryTemplate* value = activeGeometryTemplate();
    typedef void (__thiscall *Fn)(void*, int);
    reinterpret_cast<Fn>(templateVirtual(value, byteOffset))(value, setting);
}

// PE 0x100B1389. The error path's __FILE__ attests ModuleGeom.cpp, line 0xB9, as the retail home of
// this helper; it stays here with the other geometryTemplate console code [INFERRED] placement.
IGeometryTemplate* consoleCreateTemplate(const std::string& type, const std::string& name)
{
    std::string className("dice.hfe.geom.GeometryTemplate.");
    className += type.c_str();

    unsigned int classId = 0;
    if (classManager->getClassID(renddx9abi::HostString(className.c_str()), &classId))
    {
        const std::string templateName(name.c_str());
        templateManager()->createTemplate(classId, ::renddx9abi::toHostString(templateName));
    }
    else
    {
        // An unknown type is reported only while the console has a working path.
        const renddx9abi::HostString& hostPath = getConsoleWorkingPath();
        if (hostPath.size() != 0)
        {
            const std::string workingPath(hostPath.data(), static_cast<unsigned int>(hostPath.size()));
            ::renddx9abi::Debug(DebugTypeError,
                "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\ModuleGeom.cpp",
                0xB9, "") << "GeometryTemplate type:" << type << " not found. File that fails:"
                          << workingPath;
        }
    }

    // PE helper 0x100B1389 always returns null even when creation succeeds; the generated binding
    // nevertheless advertises IGeometryTemplate* and marks that null as a return value.
    return 0;
}

BF2_CONSOLE_OBJECT("GeometryTemplate", 2, 0)

// A template result prints the template's name; a null template prints a fixed text (the same text as
// objectGeometryValueText in ObjectGeometryConsole.cpp).
// The retail create and active bindings format their result through it (PE 0x100B438E, the
// getReturnValueAsString of both), where the generic pointer insertion would print an address.
std::string geometryTemplateValueText(IGeometryTemplate* value)
{
    std::ostringstream out;
    if (value)
        out << value->getName();
    else
        out << "No geometry template";
    return out.str();
}

// PE 0x100B2337 (binding constructor), 0x100B23C3 (argument parser: two independent persistent
// std::string tokens), 0x100B2498 (executor: needs exactly two arguments).
class GeometryTemplateCreateBinding : public ConsoleObjectBaseImpl
{
public:
    // PE 0x100B2337
    GeometryTemplateCreateBinding()
        : m_returnValue(0)
    {
        m_objectName = consoleDefaultStaticName();   // "GeometryTemplate" as passed (dword_10247C20)
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "create";
        m_type = 0;
        m_minNoArgs = 2;
        m_maxNoArgs = 2;
        m_returnType = "IGeometryTemplate*";
        m_hasReturnValue = false;
        m_isDynamic = false;
        m_argTypes[0] = "std::string";
        m_argTypes[1] = "std::string";
        m_argNeedsConversion[0] = 0;
        m_argNeedsConversion[1] = 0;
        consoleBindingList(kGeometryTemplateConsoleTag).push_back(this);
    }

    bool isObjectActive() const override { return true; }

    // PE 0x100B2498
    void executeObjectMethod() override
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = consoleCreateTemplate(BF2_CC_ARG(std::string, 0),
                                                   BF2_CC_ARG(std::string, 1));
        }
    }

    // PE 0x100B23C3
    void setArgFromString(int index, const std::string& value) override
    {
        BF2_CC_CONVERT(std::string, 1)
        BF2_CC_CONVERT(std::string, 2)
    }

    // PE 0x100B438E
    std::string getReturnValueAsString() const override
    {
        return m_hasReturnValue ? geometryTemplateValueText(m_returnValue) : std::string();
    }
    BF2_CC_COMMON_HOOKS

private:
    IGeometryTemplate* m_returnValue;
};

GeometryTemplateCreateBinding s_geometryTemplateCreateBinding;

BF2_CONSOLE_OBJECT("geometryTemplate", 2, 0)

// PE 0x100B1348: stream extraction of an IGeometryTemplate*: construct an empty string, extract one
// token, look it up in the template manager. Returns the stream.
std::istream& readTemplateReference(std::istream& in, IGeometryTemplate*& result)
{
    std::string templateName;
    in >> templateName;
    result = templateManager()->getTemplate(::renddx9abi::toHostString(templateName));
    return in;
}

// PE 0x100B25B7 (argument parser). The token is read by PE 0x100B1348 (stream extraction of an
// IGeometryTemplate*: a std::string token, then the template manager's getTemplate).
class GeometryTemplateActiveBinding : public ConsoleObjectBaseImpl
{
public:
    // Binding constructor: PE 0x100B24F4.
    GeometryTemplateActiveBinding()
        : m_returnValue(0)
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "active";
        m_type = 1;
        m_minNoArgs = 0;
        m_maxNoArgs = 1;
        m_returnType = "IGeometryTemplate*";
        m_hasReturnValue = false;
        m_isDynamic = true;
        m_argTypes[0] = "IGeometryTemplate*";
        m_argNeedsConversion[0] = -1;
        consoleBindingList(kGeometryTemplateConsoleTag).push_back(this);
    }

    bool isObjectActive() const override { return templateManager() != 0; }

    void executeObjectMethod() override
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            templateManager()->setActive(BF2_CC_ARG(IGeometryTemplate*, 0));
        }
        else if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = templateManager()->getActive();
        }
    }

    // PE 0x100B25B7
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1)
        {
            static IGeometryTemplate* resolved = 0;
            std::istringstream in(value);
            readTemplateReference(in, resolved);
            m_args[0] = reinterpret_cast<std::string*>(&resolved);
        }
    }

    // Same formatter as the create binding (retail shares one body between them).
    std::string getReturnValueAsString() const override
    {
        return m_hasReturnValue ? geometryTemplateValueText(m_returnValue) : std::string();
    }
    BF2_CC_COMMON_HOOKS

private:
    IGeometryTemplate* m_returnValue;
};

GeometryTemplateActiveBinding s_geometryTemplateActiveBinding;

// The type spelling the shipped bindings carry for unsigned properties ("uint").
typedef unsigned int uint;

// dumpInstances - ctor 0x100B2604, active while the manager exists (0x100B2565), executor
// 0x100B2656: the manager's +0x5C with no argument.
void consoleDumpInstances()
{
    templateManager()->visitTemplateClassIds();
}

BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(dumpInstances, consoleDumpInstances, templateManager() != 0,
                                   kGeometryTemplateConsoleTag)

// PE 0x100B2671 / 0x100B26DE: read-only, not a zero/one-argument property.
class GeometryTemplateNameBinding : public ConsoleObjectBaseImpl
{
public:
    // PE 0x100B2671
    GeometryTemplateNameBinding()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "name";
        m_type = 2;
        m_minNoArgs = m_maxNoArgs = 0;
        m_returnType = "std::string";
        m_hasReturnValue = false;
        m_isDynamic = true;
        consoleBindingList(kGeometryTemplateConsoleTag).push_back(this);
    }

    virtual bool isObjectActive() const { return activeGeometryTemplate() != 0; }
    // PE 0x100B26DE
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = activeGeometryTemplate()->getName();
        }
    }
    virtual void setArgFromString(int, const std::string&) {}
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};

GeometryTemplateNameBinding s_geometryTemplateNameBinding;

// setSubGeometryLodDistance geom lod distance - ctor 0x100B2711, active 0x100B26D2 (an active
// template), executor 0x100B287F: the active template's +0x30 setLodDistance.
void setSubGeometryLodDistance(int geom, int lod, float distance)
{
    activeGeometryTemplate()->setLodDistance(geom, lod, distance);
}

// setMaterialParallaxHeightScale geom lod material value - ctor 0x100B28BF, executor 0x100B2A81:
// the active template's +0x38 setMaterialValue (the material index pushed as a full dword).
void setMaterialParallaxHeightScale(int geom, int lod, int material, float value)
{
    activeGeometryTemplate()->setMaterialValue(geom, lod, static_cast<unsigned char>(material), value);
}

// PE 0x100B27C0
// PE 0x100B287F
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setSubGeometryLodDistance, setSubGeometryLodDistance,
                                   activeGeometryTemplate() != 0, int, int, float,
                                   kGeometryTemplateConsoleTag)
// PE 0x100B298A
// PE 0x100B2A81
BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(setMaterialParallaxHeightScale, setMaterialParallaxHeightScale,
                                   activeGeometryTemplate() != 0, int, int, int, float,
                                   kGeometryTemplateConsoleTag)

// maxSkip3pLods / maxSkipWreckLods (constructors and executors cited at the bindings below; the
// active test at 0x100B2B87 asks for a mesh template): MeshTemplate +0xE4 / +0xE0 and +0xEC / +0xE8.
uint getMaxSkip3pLods() { return static_cast<uint>(activeMeshTemplate()->getMaxSkip3pLods()); }
void setMaxSkip3pLods(uint value) { activeMeshTemplate()->setMaxSkip3pLods(static_cast<int>(value)); }
uint getMaxSkipWreckLods() { return static_cast<uint>(activeMeshTemplate()->getMaxSkipWreckLods()); }
void setMaxSkipWreckLods(uint value) { activeMeshTemplate()->setMaxSkipWreckLods(static_cast<int>(value)); }

// PE 0x100B2AC9, 0x100B2B3A, 0x100B2B93: maxSkip3pLods constructor ('uint'), converter, executor.
BF2_CONSOLE_PROPERTY_FN(maxSkip3pLods, uint, activeMeshTemplate() != 0,
                        getMaxSkip3pLods(), setMaxSkip3pLods, kGeometryTemplateConsoleTag)
// PE 0x100B2BE6, 0x100B2CFD, 0x100B2C57, 0x100B2CB0
BF2_CONSOLE_PROPERTY_FN(maxSkipWreckLods, uint, activeMeshTemplate() != 0,
                        getMaxSkipWreckLods(), setMaxSkipWreckLods, kGeometryTemplateConsoleTag)

// The StaticMesh-only fields, active 0x100B2E0E (a StaticMesh template); each executor (cited at
// its binding below) reads or stores the field directly: shadowDetailLevel +0x2B4,
// meshDetailLevel +0x2B8, glowing +0x2A5, noLighting +0x2B0, glowFadeOutDistStart / End
// +0x2A8 / +0x2AC, ignoreLeafLighting +0x2B1. StaticMesh's draw reads them (glow fade, cone
// lighting, the LOD budget).
int  getShadowDetailLevel() { return activeStaticMeshTemplate()->m_dword2B4; }
void setShadowDetailLevel(int v) { activeStaticMeshTemplate()->m_dword2B4 = v; }
int  getMeshDetailLevel() { return activeStaticMeshTemplate()->m_dword2B8; }
void setMeshDetailLevel(int v) { activeStaticMeshTemplate()->m_dword2B8 = v; }
bool getGlowing() { return activeStaticMeshTemplate()->m_byte2A5 != 0; }
void setGlowing(bool v) { activeStaticMeshTemplate()->m_byte2A5 = v; }
bool getNoLighting() { return activeStaticMeshTemplate()->m_byte2B0 != 0; }
void setNoLighting(bool v) { activeStaticMeshTemplate()->m_byte2B0 = v; }
float getGlowFadeOutDistStart() { return activeStaticMeshTemplate()->m_float2A8; }
void  setGlowFadeOutDistStart(float v) { activeStaticMeshTemplate()->m_float2A8 = v; }
float getGlowFadeOutDistEnd() { return activeStaticMeshTemplate()->m_float2AC; }
void  setGlowFadeOutDistEnd(float v) { activeStaticMeshTemplate()->m_float2AC = v; }
bool getIgnoreLeafLighting() { return activeStaticMeshTemplate()->m_keepVegetationNormals != 0; }
void setIgnoreLeafLighting(bool v) { activeStaticMeshTemplate()->m_keepVegetationNormals = v; }

// PE 0x100B2D50, 0x100B2DC1, 0x100B2E1A: shadowDetailLevel constructor, converter, executor.
BF2_CONSOLE_PROPERTY_FN(shadowDetailLevel, int, activeStaticMeshTemplate() != 0,
                        getShadowDetailLevel(), setShadowDetailLevel, kGeometryTemplateConsoleTag)
// PE 0x100B2E64, 0x100B2F22, 0x100B2ED5
BF2_CONSOLE_PROPERTY_FN(meshDetailLevel, int, activeStaticMeshTemplate() != 0,
                        getMeshDetailLevel(), setMeshDetailLevel, kGeometryTemplateConsoleTag)
// PE 0x100B2F6C, 0x100B302A, 0x100B2FDD
BF2_CONSOLE_PROPERTY_FN(glowing, bool, activeStaticMeshTemplate() != 0,
                        getGlowing(), setGlowing, kGeometryTemplateConsoleTag)
// PE 0x100B3074, 0x100B3132, 0x100B30E5
BF2_CONSOLE_PROPERTY_FN(noLighting, bool, activeStaticMeshTemplate() != 0,
                        getNoLighting(), setNoLighting, kGeometryTemplateConsoleTag)
// PE 0x100B317C, 0x100B323A, 0x100B31ED
BF2_CONSOLE_PROPERTY_FN(glowFadeOutDistStart, float, activeStaticMeshTemplate() != 0,
                        getGlowFadeOutDistStart(), setGlowFadeOutDistStart, kGeometryTemplateConsoleTag)
// PE 0x100B328D, 0x100B334B, 0x100B32FE
BF2_CONSOLE_PROPERTY_FN(glowFadeOutDistEnd, float, activeStaticMeshTemplate() != 0,
                        getGlowFadeOutDistEnd(), setGlowFadeOutDistEnd, kGeometryTemplateConsoleTag)
// PE 0x100B339E, 0x100B345C, 0x100B340F
BF2_CONSOLE_PROPERTY_FN(ignoreLeafLighting, bool, activeStaticMeshTemplate() != 0,
                        getIgnoreLeafLighting(), setIgnoreLeafLighting, kGeometryTemplateConsoleTag)

// The BundledMesh-only bindings, active 0x100B3564 (a BundledMesh template).
// nrOfAnimatedUVMatrix: the executor (cited at the binding) reads or stores the int +0x10C directly.
int  getNrOfAnimatedUVMatrix() { return static_cast<int>(activeBundledMeshTemplate()->m_dword10C); }
void setNrOfAnimatedUVMatrix(int v) { activeBundledMeshTemplate()->m_dword10C = static_cast<unsigned int>(v); }

// PE 0x100B34A6, 0x100B3517, 0x100B3570: nrOfAnimatedUVMatrix constructor, converter, executor.
BF2_CONSOLE_PROPERTY_FN(nrOfAnimatedUVMatrix, int, activeBundledMeshTemplate() != 0,
                        getNrOfAnimatedUVMatrix(), setNrOfAnimatedUVMatrix, kGeometryTemplateConsoleTag)

// setMaterialReflectionScale geom lod material value - executor 0x100B377C: the Bundled
// template's +0x40 with the float pushed in the fourth dword (the slot declares it as raw bits).
void setMaterialReflectionScale(int geom, int lod, int material, float value)
{
    int bits;
    ::memcpy(&bits, &value, sizeof(bits));
    activeBundledMeshTemplate()->slot_0x40(geom, lod, material, bits);
}

// setSpecularStaticGloss geom lod material value - executor 0x100B3986: the Bundled template's
// +0xF0 setSpecularStaticGloss.
void setSpecularStaticGloss(int geom, int lod, int material, float value)
{
    activeBundledMeshTemplate()->setSpecularStaticGloss(geom, lod,
                                                        static_cast<unsigned char>(material), value);
}

// PE 0x100B35BA, 0x100B3685
BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(setMaterialReflectionScale, setMaterialReflectionScale,
                                   activeBundledMeshTemplate() != 0, int, int, int, float,
                                   kGeometryTemplateConsoleTag)
// PE 0x100B37C4, 0x100B388F
BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(setSpecularStaticGloss, setSpecularStaticGloss,
                                   activeBundledMeshTemplate() != 0, int, int, int, float,
                                   kGeometryTemplateConsoleTag)

bool getLightmapOverride() { return getTemplateBool(0x6C); }
void setLightmapOverride(bool value) { setTemplateBool(0x68, value); }
bool getDoNotGenerateLightmaps() { return getTemplateBool(0x64); }
void setDoNotGenerateLightmaps(bool value) { setTemplateBool(0x60, value); }
bool getCompressVertexData() { return getTemplateBool(0x7C); }
void setCompressVertexData(bool value) { setTemplateBool(0x78, value); }
int getMaxTextureRepeat() { return getTemplateInt(0x8C); }
void setMaxTextureRepeat(int value) { setTemplateInt(0x88, value); }

// setLightmapSizeAll geom lod size: the active template's setLightmapSize (+0x70) with the three
// arguments as given; a StaticMesh template stores the size only while lightmapOverride is set.
// The executor fetches the template again without a null check.
void setLightmapSizeAll(int geom, int lod, int size)
{
    activeGeometryTemplate()->setLightmapSize(geom, lod, size);
}

// PE 0x100B39D1, 0x100B3A79
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setLightmapSizeAll, setLightmapSizeAll,
                                   activeGeometryTemplate() != 0, int, int, int,
                                   kGeometryTemplateConsoleTag)

// PE 0x100B3B6C, 0x100B3C2A, 0x100B3BDD
BF2_CONSOLE_PROPERTY_FN(lightmapOverride, bool, activeGeometryTemplate() != 0,
                        getLightmapOverride(), setLightmapOverride,
                        kGeometryTemplateConsoleTag)
// PE 0x100B3C7F, 0x100B3D3D, 0x100B3CF0
BF2_CONSOLE_PROPERTY_FN(doNotGenerateLightmaps, bool, activeGeometryTemplate() != 0,
                        getDoNotGenerateLightmaps(), setDoNotGenerateLightmaps,
                        kGeometryTemplateConsoleTag)
// PE 0x100B3D92, 0x100B3E50, 0x100B3E03
BF2_CONSOLE_PROPERTY_FN(compressVertexData, bool, activeGeometryTemplate() != 0,
                        getCompressVertexData(), setCompressVertexData,
                        kGeometryTemplateConsoleTag)
// useRadiusForShadowDepth / useRadiusForShadowSpatial - ctors 0x100B3EA5 / 0x100B3FBE, active
// 0x100B26D2, executors 0x100B3F63 / 0x100B407C: the active template's +0x94 / +0x90 and
// +0x9C / +0x98 (the bool passed as a full dword).
// PE 0x100B3F16
// PE 0x100B3F63
BF2_CONSOLE_PROPERTY_FN(useRadiusForShadowDepth, bool, activeGeometryTemplate() != 0,
                        activeGeometryTemplate()->slot_0x94(),
                        activeGeometryTemplate()->slot_0x90, kGeometryTemplateConsoleTag)
// PE 0x100B402F
// PE 0x100B3FBE, 0x100B407C
BF2_CONSOLE_PROPERTY_FN(useRadiusForShadowSpatial, bool, activeGeometryTemplate() != 0,
                        activeGeometryTemplate()->slot_0x9C(),
                        activeGeometryTemplate()->slot_0x98, kGeometryTemplateConsoleTag)
// PE 0x100B40D7, 0x100B4195, 0x100B4148
BF2_CONSOLE_PROPERTY_FN(maxTextureRepeat, int, activeGeometryTemplate() != 0,
                        getMaxTextureRepeat(), setMaxTextureRepeat,
                        kGeometryTemplateConsoleTag)

// PE 0x100B1B4D. [INFERRED] helper name; the report only includes the three mesh classes.
std::string __fastcall consoleGeometryTemplateInfo(bool header)
{
    std::string result;
    if (header)
        result += formatString("%-4s %-35s %5s %5s %5s %5s %5s  mats     polys\n",
                               "type", "name", "count", "tot", "texs", "verts", "inxs");

    GeometryTemplateRegistry* const registry = templateManager()->getTemplateRegistry();
    for (GeometryTemplateRegistry::iterator it = registry->begin(); it != registry->end(); ++it)
    {
        IGeometryTemplate* const value = it->second;
        std::string type("-");
        int count;
        if (value->getClassId() == 0x870CA653u)
        {
            type = "Stat";
            count = static_cast<int>(static_cast<StaticMeshTemplate*>(value)->m_instances.size());
        }
        else if (value->getClassId() == 0x870CA651u)
        {
            type = "Bund";
            count = static_cast<int>(static_cast<BundledMeshTemplate*>(value)->m_instances.size());
        }
        else if (value->getClassId() == 0x870CA652u)
        {
            type = "Skin";
            count = static_cast<int>(static_cast<SkinnedMeshTemplate*>(value)->m_instances.size());
        }
        else
            continue;

        const char* const name = value->getName().c_str();
        const char* const label = type.c_str();
        // The four queries are ordered, and the shipped divisions interpret EAX as signed.
        const int indices = static_cast<int>(value->getMemoryUsage(2)) / 1024;
        const int vertices = static_cast<int>(value->getMemoryUsage(1)) / 1024;
        const int textures = static_cast<int>(value->getMemoryUsage(4)) / 1024;
        const int total = static_cast<int>(value->getMemoryUsage(-1)) / 1024;
        result += formatString("%-4s %-35s %5d %5d %5d %5d %5d ", label, name,
                               count, total, textures, vertices, indices);
        result += " ";
        for (unsigned int lod = 0; lod != 4; ++lod)
        {
            const int materials = value->getMaterialCount(lod);
            result += formatString(materials != 0 ? "%d " : "- ", materials);
        }
        result += " ";
        for (unsigned int lod = 0; lod != 4; ++lod)
        {
            const int primitives = value->getLodPrimitiveCount(lod);
            result += formatString(primitives != 0 ? "%5d " : "    - ", primitives);
        }
        result += "\n";
    }
    return result;
}

// PE 0x100B41E8 / 0x100B42B5 / 0x100B4268 (Boolean argument parser). The original-case object name and optional header are shipped.
// The constructor reads the defaults' static name (dword_10247C20), which retail's initializer has set
// to "GeometryTemplate" at that point (0x101D084C..0x101D0A87); this file's order has switched the
// defaults to "geometryTemplate" by then, so the value is spelled out.
class GeometryTemplatePrintInfoBinding : public ConsoleObjectBaseImpl
{
public:
    // PE 0x100B41E8
    GeometryTemplatePrintInfoBinding()
    {
        m_objectName = "GeometryTemplate";
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "printInfo";
        m_type = 0;
        m_minNoArgs = 0;
        m_maxNoArgs = 1;
        m_returnType = "std::string";
        m_hasReturnValue = false;
        m_isDynamic = false;
        m_argTypes[0] = "bool";
        m_argNeedsConversion[0] = -1;
        consoleBindingList(kGeometryTemplateConsoleTag).push_back(this);
    }
    virtual bool isObjectActive() const { return true; }
    // PE 0x100B42B5
    virtual void executeObjectMethod()
    {
        if (m_argCount != 0 && m_argCount != 1)
            return;
        const bool header = m_argCount == 0 ? true : BF2_CC_ARG(bool, 0);
        m_hasReturnValue = true;
        m_returnValue = consoleGeometryTemplateInfo(header);
    }
    // PE 0x100B4268
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(bool, 1)
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};

GeometryTemplatePrintInfoBinding s_geometryTemplatePrintInfoBinding;

} // anonymous namespace

bool registerGeometryTemplateConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kGeometryTemplateConsoleTag));
}

bool unregisterGeometryTemplateConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kGeometryTemplateConsoleTag));
}


} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
