// Collision mesh loading, level hooks and debug geometry.
// Path attested by __FILE__; bodies recovered from the dedicated ELF.
// Mesh ownership and deferred loading are part of the level lifecycle.


#include "CollisionManager.h"

#include "CollisionMesh.h"           // getCollisionMeshPart's product
#include "ICollisionMesh.h"           // drawDebugCollisionMeshRecursive's typed mesh dispatch
#include "HeightmapCluster.h"         // debugWater's water-level query
#include "Heightmap.h"                // debugHeightmap's typed terrain queries
#include "PhysicsUtil.h"              // setSleepinessRecursive
#include "../Core/ClassManager.h"     // the two shipped named-singleton lookups
#include "../Core/ConsoleObjectMacro.h" // CollisionManager.createTemplate binding
#include "../Core/Debug.h"           // Debug, DEBUGTYPE_6 - the two constructor failure paths
#include "../Core/EventManager.h"    // dice::hfe::eventManager, EventManager
#include "../Core/Ids.h"             // IID_IBase, world::IID_ICollisionManager
#include "../GeomMisc/DebugLineDrawer.h" // typed retail debug submissions
#include "../Core/StringUtil.h"      // getFixPath / getLowerCase / goUpInPath - postLevelLoad's grouping
#include "../Game/Objects/ICameraObject.h"      // debug camera transformation
#include "../Game/Objects/IObject.h"            // recursive child walk
#include "../Game/Objects/IPlayer.h"            // local player / vehicle / camera
#include "../Game/Objects/ObjectLayoutAccess.h" // shipped direct collision-mesh field read
#include "../Game/Utility/Vars.h"                // coll-load-debugmeshes
#include "../IO/Console/Console.h"   // io::mainConsole->getWorkingPath() - create()'s directory
#include "../IO/File/FileManager.h"  // io::fileManager - load()'s .collisionMesh open
#include <cstdio>                    // the [PLATFORM] miss-arm diagnostic - getCollisionMeshPart
#include <cstdlib>                   // std::exit - postLevelLoad's shipped tool-mode quit
#include <cmath>
#include <cfloat>
#include "../Scene/Object/ObjectManager.h"      // radius grid query
#include "../Scene/Object/ObjectUtil.h"         // updateFlagsRecursive
#include "../Scene/Object/ObjectPredicator.h"   // ObjectFlagPredicator(0x01000100)
#include "../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"  // the active-template bind
#include "../Scene/Object/PlayerManager.h"      // getLocalHumanPlayer

namespace dice {
namespace hfe {

extern ClassManager* classManager;               // Core/Globals.cpp, ELF ds:0x8AC85A0

namespace world {

extern IBase* heightmapCluster;                  // Core/Globals.cpp, ELF ds:0x8ABAF24

// CollisionMeshTemplate.cpp owns these two frame-local debug streams. ResponsePhysicsManager
// clears them after collision response; this draw pass consumes the same vectors before clearing
// only its own line/triangle streams.
extern std::vector<Vec3> collisionTestedFaces;
extern std::vector<Vec3> collisionIntersectFaces;

namespace {

// Three of CollisionManager.cpp's five retail (value, pointer-to-value) debug-variable pairs.
// The other two pairs live beside load() below. Nothing recovered rebinds these pointers.
int   s_debugHeightmapValue = 0;
int*  s_debugHeightmapPtr   = &s_debugHeightmapValue;
int   s_debugWaterValue     = 0;
int*  s_debugWaterPtr       = &s_debugWaterValue;
float s_debugLineWidthValue = 0.01f;
float* s_debugLineWidthPtr  = &s_debugLineWidthValue;

} // namespace

// the factory - written first because it is the entry point the boot reaches

// Creates a one-reference collision manager and returns its requested interface.
// Rejects aggregation, releases the object when the IID is unsupported and does not addRef again.
// Does not call init(); that method resets per-level state separately.
IBase* CollisionManager::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)
        return 0;

    CollisionManager* self = new CollisionManager();    // 0x8846B20, operator new(0x70u)
    void* itf = self->queryInterface(iid);              // 0x8846B42, virtual - call [edx+0Ch]
    if (itf == 0)
        self->release();                                // 0x8846B54, virtual - call [eax+08h]
    return static_cast<IBase*>(itf);
}

// construction and destruction

// Initializes collision state and registers physics/core event handlers at priority zero.
// Pass the adjusted IEventListener pointer. Event registration's result is discarded; failure of
// either handler registration emits the fatal diagnostic. Containers start empty.
CollisionManager::CollisionManager()
    : m_refCount(1)                     // 0x839749B  `mov dword ptr [ebx+8], 1`
    , m_useDeferredLoading(true)        // 0x8397506  `mov byte ptr [ebx+24h], 1`
    , m_unknown25(false)
    , m_activeLod(0)
    , m_geometry(false)
    , m_drawDistance(200.0f)
    , m_drawOnlyRoot(false)
    , m_drawMeshes(false)
    , m_drawBoundingBoxes(false)
    , m_drawPivots(false)
    , m_drawNormals(false)
    , m_drawTestedFaces(false)
    , m_drawIntersectFaces(false)
    , m_drawSoldierMesh(false)
    , m_drawPrimitives(false)
    , m_drawForces(false)
{
    // 0x83975AE - 0x83975D0. The event name is the console-visible id for the draw pass that
    // handleEvent below answers on ECPhysics id 0.
    eventManager->registerEvent(ECPhysics, 0, "ECPhysicsEDrawDebugPrimitives");

    if (!eventManager->registerEventHandler(ECPhysics, this, 0))
    {
        Debug(DEBUGTYPE_6, "Physics/CollisionManager.cpp", 91, "")
            << "Failed to register CollisionManager as event handler for ECPhysics.";
    }

    if (!eventManager->registerEventHandler(ECCore, this, 0))
    {
        Debug(DEBUGTYPE_6, "Physics/CollisionManager.cpp", 95, "")
            << "Failed to register CollisionManager as event handler for ECCore.";
    }
}

// Unregisters physics/core handlers and deletes every owned collision template.
// Unregistration failures are ignored. Each registry entry must contain at least one template;
// the inner traversal reads its first element before checking the size.
CollisionManager::~CollisionManager()
{
    eventManager->unregisterEvent(ECPhysics, 0);
    eventManager->unregisterEventHandler(ECPhysics, this);
    eventManager->unregisterEventHandler(ECCore, this);


    for (std::map<std::string, std::vector<CollisionMeshTemplate*>,
                  NoCaseStringCompare>::iterator it = m_collisionMeshTemplates.begin();
         it != m_collisionMeshTemplates.end(); ++it)
    {
        for (int i = 0; i < static_cast<int>(it->second.size()); ++i)
            delete it->second[i];                                   // 0x83978CD, call [vptr+4]
    }
}

// the IBase quartet

// CollisionManager::addRef - Vtable +0x00.
void CollisionManager::addRef()
{
    ++m_refCount;
}

// CollisionManager::getRef - Vtable +0x04.
int CollisionManager::getRef()
{
    return m_refCount;
}

// Releases one reference and deletes at zero; return the saved count without rereading freed storage.
int CollisionManager::release()
{
    const int refs = --m_refCount;

    if (refs == 0)
        delete this;                // 0x839D880, call [eax+14h]

    return refs;
}

// Returns the IBase or ICollisionManager view, otherwise null.
// Event registration receives the listener subobject directly; it has no queried IID here.
void* CollisionManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<CollisionManager*>(this);

    if (iid == IID_ICollisionManager)                           // 0x839D8B5, cmovz
        return const_cast<CollisionManager*>(this);

    return 0;
}

// PER-level reset

// Resets per-level debug state and the post-frame queue.
// Preserves deferred-loading policy, templates and recorded debug-primitive vectors.
void CollisionManager::init()
{
    m_geometry           = false;
    m_activeLod          = 0;
    m_drawDistance       = 200.0f;
    m_drawOnlyRoot       = false;
    m_drawMeshes         = false;
    m_drawBoundingBoxes  = false;
    m_drawPivots         = false;
    m_drawNormals        = false;
    m_drawTestedFaces    = false;
    m_drawIntersectFaces = false;
    m_drawSoldierMesh    = false;
    m_drawPrimitives     = false;
    m_drawForces         = false;

    m_postFrameUpdate.resize(0);
}

// Resets level state through virtual init and clears loadRequested on part 0 of each template entry.
// Every registered parts vector must be nonempty.
void CollisionManager::preLevelLoad()
{
    init();                                                         // 0x83987DA, virtual

    for (std::map<std::string, std::vector<CollisionMeshTemplate*>,
                  NoCaseStringCompare>::iterator it = m_collisionMeshTemplates.begin();
         it != m_collisionMeshTemplates.end(); ++it)
    {

        it->second[0]->m_loadRequested = 0;
    }
}

namespace {
// Shared collision configuration value and its indirection, used by creation and tool-mode exit.
int  s_collisionConfigValue = 0;                            // ds:0x8ABB8A8
int* s_collisionConfigPtr   = &s_collisionConfigValue;      // ds:0x8ABB8AC
}

// Resolves deferred collision loads after the level loads, with progress events.
// Drops unrequested entries, groups normalized directories case-insensitively (first entry wins),
// then loads each group. The optional load-and-quit tool mode exits after loading.
// The former geometry [FRONTIER: 2,305 B + CollisionMeshTemplate::load 6,710 B, the geometry interior]
// is implemented by load and CollisionMeshTemplate::load below.
void CollisionManager::postLevelLoad()
{
    typedef std::map<std::string, std::vector<CollisionMeshTemplate*>, NoCaseStringCompare> Registry;

    if (!m_useDeferredLoading)
        return;

    eventManager->postEvent(static_cast<EventCategory>(2), 7u,
                            const_cast<char*>("Loading Collision Meshes.."), 0.0f);

    // Prunes templates that were registered but never requested by instantiated objects.
    for (Registry::iterator it = m_collisionMeshTemplates.begin();
         it != m_collisionMeshTemplates.end(); )
        it = erase(it);

    // 3. group by directory
    std::map<std::string, Registry::iterator, NoCaseStringCompare> byFolder;
    for (Registry::iterator it = m_collisionMeshTemplates.begin();
         it != m_collisionMeshTemplates.end(); ++it)
    {
        CollisionMeshTemplate* first = it->second[0];             // [0], unchecked, as read
        std::string folder = getLowerCase(getFixPath(first->m_directory, '/') + first->m_name);
        goUpInPath(folder);
        byFolder.insert(std::make_pair(folder, it));
    }

    // Posts a category-5/id-4 progress event with a stack payload, not an EventNode.
    // [CHOSEN] Local struct name; preserve the int/float/string-pointer field order.
    {
        struct LoadProgress { int count; float fraction; const char* label; } progress;
        progress.count    = static_cast<int>(byFolder.size());
        progress.fraction = 0.5f;
        progress.label    = "Collision";
        eventManager->postEvent(static_cast<EventCategory>(5), 4u, &progress, 0.0f);
    }

    // 5. load each folder's meshes
    for (std::map<std::string, Registry::iterator, NoCaseStringCompare>::iterator g = byFolder.begin();
         g != byFolder.end(); ++g)
        load(g->second->second);                                   // -> the parser

    // 6. the shipped quit-after-load tool mode
    if (*s_collisionConfigPtr)
        std::exit(0);
}

// Keeps a registry entry when its first template was requested for loading; otherwise deletes its
// non-null templates and removes the entry. Returns the iterator at which pruning should continue.
std::map<std::string, std::vector<CollisionMeshTemplate*>, NoCaseStringCompare>::iterator
CollisionManager::erase(
    std::map<std::string, std::vector<CollisionMeshTemplate*>, NoCaseStringCompare>::iterator it)
{
    std::vector<CollisionMeshTemplate*>& parts = it->second;
    if (parts[0]->m_loadRequested)                                 // +0x2D on [0], as read
        return ++it;
    for (unsigned int i = 0; i < parts.size(); ++i)
        delete parts[i];                                           // vcall +0x04 = the virtual dtor
    std::map<std::string, std::vector<CollisionMeshTemplate*>, NoCaseStringCompare>::iterator next = it;
    ++next;
    m_collisionMeshTemplates.erase(it);
    return next;
}

// the event half

// Handles physics debug drawing and core pre/post-level-load notifications.
// node/data are unused. Primitive drawing is direct; mesh drawing uses the virtual entry point.
void CollisionManager::handleEvent(EventCategory category, unsigned int id,
                                   EventNode* node, void* userData)
{
    (void)node;
    (void)userData;

    if (category == ECPhysics)
    {
        if (id == 0)
        {
            drawDebugPrimitives();              // 0x839B7A1, direct - non-virtual
            drawDebugCollisionMeshes();         // 0x839B7B0, call [eax+74h] - virtual
        }
    }
    else if (category == ECCore)
    {
        if (id == 2)
            preLevelLoad();
        else if (id == 3)
            postLevelLoad();
    }
}

// Appends a two-triangle water square centered on the local camera at sampled height.
// Requires classManager and heightmapCluster; missing player or camera leaves out unchanged.
// Preserves signed extent conversion and duplicate diagonal vertices.
void debugWater(std::vector<Vec3>& out, int extent)
{
    IBase* const base = classManager->getSingleton(std::string("PlayerManager"));
    if (!base)
        return;

    // The singleton is a concrete PlayerManager; this cast crosses its unmodeled interface base.
    PlayerManager* const manager =
        static_cast<PlayerManager*>(static_cast<void*>(base));
    IPlayer* const player = manager->getLocalHumanPlayer();
    if (!player)
        return;

    ICameraObject* const camera = player->getCamera();
    if (!camera)
        return;

    const Mat4& transform = camera->getTransformation(0.0f);
    Vec3 centre = { transform.m[12], transform.m[13], transform.m[14] };
    centre.y = static_cast<IHeightmapCluster*>(heightmapCluster)
                   ->getWaterLevelInWorldCoords(centre);

    const float distance = static_cast<float>(extent);
    const float minX = centre.x - distance;
    const float maxX = centre.x + distance;
    const float minZ = centre.z - distance;
    const float maxZ = centre.z + distance;

    const Vec3 minMin = { minX, centre.y, minZ };
    const Vec3 maxMin = { maxX, centre.y, minZ };
    const Vec3 minMax = { minX, centre.y, maxZ };
    const Vec3 maxMax = { maxX, centre.y, maxZ };

    out.push_back(minMin);
    out.push_back(maxMin);
    out.push_back(minMax);
    out.push_back(maxMin);
    out.push_back(minMax);
    out.push_back(maxMax);
}

// PE 0x006FD4E0 / ELF 0x0839B7F0.
// Draws enabled debug streams, then clears all four member streams. Requires the renderer
// singleton. Collision-face vectors are cleared by ResponsePhysicsManager instead.
void CollisionManager::drawDebugPrimitives()
{
    std::vector<Vec3> waterVertices;
    std::vector<Vec3> heightmapVertices;
    std::vector<unsigned int> colors;

    geom::IDebugLineDrawer* const drawer = static_cast<geom::IDebugLineDrawer*>(
        classManager->getSingleton(std::string("DebugLineDrawer")));

    if (m_drawTestedFaces == true)
    {
        colors.resize(0);
        colors.push_back(0xFF00FF00u);
        drawer->drawTriangles(collisionTestedFaces, colors, true);
    }

    if (m_drawIntersectFaces == true)
    {
        colors.resize(0);
        colors.push_back(0xFFFF00FFu);
        drawer->drawTriangles(collisionIntersectFaces, colors, true);
    }

    if (m_drawPrimitives == true)
    {
        drawer->drawLineList(m_lineVertices, m_lineColors, *s_debugLineWidthPtr, false);
        drawer->drawTriangles(m_triangleVertices, m_triangleColors, false);
    }

    if (*s_debugHeightmapPtr > 0)
    {
        colors.resize(0);
        heightmapVertices.resize(0);
        colors.push_back(0xFFFF00FFu);
        debugHeightmap(heightmapVertices, *s_debugHeightmapPtr);
        drawer->drawLineList(heightmapVertices, colors, 0.01f, false);
    }

    if (*s_debugWaterPtr > 0)
    {
        colors.resize(0);
        waterVertices.resize(0);
        colors.push_back(0xFF0000FFu);
        debugWater(waterVertices, *s_debugWaterPtr);
        drawer->drawTriangles(waterVertices, colors, true);
    }

    m_lineVertices.resize(0);
    m_lineColors.resize(0);
    m_triangleVertices.resize(0);
    m_triangleColors.resize(0);
}

// When a mesh-view switch is enabled, queries around the local camera and recursively
// draws returned roots in ObjectManager order, optionally excluding the player vehicle.
// Requires classManager and objectManager.
void CollisionManager::drawDebugCollisionMeshes()
{
    if (!m_drawMeshes && !m_drawBoundingBoxes && !m_drawPivots)
        return;
    if (*Vars::getInt("coll-load-debugmeshes", 1) == 0)
        return;

    Vec3 centre = { 0.0f, 0.0f, 0.0f };
    IObject* excludedVehicle = 0;

    IBase* const base = classManager->getSingleton(std::string("PlayerManager"));
    if (base)
    {
        PlayerManager* const manager =
            static_cast<PlayerManager*>(static_cast<void*>(base));
        IPlayer* const player = manager->getLocalHumanPlayer();
        if (player)
        {
            if (!m_drawSoldierMesh)
                excludedVehicle = player->getVehicle();

            ICameraObject* const camera = player->getCamera();
            if (camera)
            {
                const Mat4& transform = camera->getTransformation(0.0f);
                centre.x = transform.m[12];
                centre.y = transform.m[13];
                centre.z = transform.m[14];
            }
        }
    }

    ObjectFlagPredicator predicator(0x01000100u);
    std::vector<IObject*> objects;
    objectManager->getObjectsFromGrid(centre, m_drawDistance, objects, &predicator);

    for (std::vector<IObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        if (*it != excludedVehicle)
            drawDebugCollisionMeshRecursive(*it);
    }
}

// Recursively draws root and sibling collision meshes. Requires a nonnull root;
// children are neither null-guarded nor deduplicated beyond the explicit traversal checks.
void CollisionManager::drawDebugCollisionMeshRecursive(IObject* root)
{
    ICollisionMesh* const mesh = static_cast<ICollisionMesh*>(
        object_detail::LayoutAccess::collisionMesh(root));
    if (mesh)
    {
        mesh->drawDebugMesh(m_geometry, m_activeLod,
                            root->getAbsoluteTransformation(),
                            m_drawMeshes, m_drawBoundingBoxes, m_drawPivots);
    }

    for (IObject* child = root->getChild(); child; child = child->getSibling())
        drawDebugCollisionMeshRecursive(child);
}

// accessors - fourteen setters and one getter, 14 bytes each except the getter's 12

// Vtable +0x1C.  `mov [eax+24h], dl`
void CollisionManager::setUseDeferredLoading(bool on)
{
    m_useDeferredLoading = on;
}

// Vtable +0x20. const. `movzx eax, byte ptr [eax+24h]` - a real bool
// return, materialised, not a leftover.
bool CollisionManager::getUseDeferredLoading() const
{
    return m_useDeferredLoading;
}

// Vtable +0x40.  `mov [eax+2Ch], dl`
void CollisionManager::setGeometry(bool on)
{
    m_geometry = on;
}

// Vtable +0x44.  `mov [eax+28h], edx` - a full dword, so int not bool.
void CollisionManager::setActiveLod(int lod)
{
    m_activeLod = lod;
}

// Vtable +0x48.  `mov [eax+30h], edx` - the float is moved as a dword
// through a general register, which is what GCC does for a by-value float parameter it only stores.
void CollisionManager::setDrawDistance(float distance)
{
    m_drawDistance = distance;
}

// Vtable +0x4C.  `mov [eax+34h], dl`
void CollisionManager::drawOnlyRoot(bool on)
{
    m_drawOnlyRoot = on;
}

// Vtable +0x50.  `mov [eax+35h], dl`
void CollisionManager::drawMeshes(bool on)
{
    m_drawMeshes = on;
}

// Vtable +0x54, field +0x37. The vtable order and the struct order
// disagree across this pair - see CollisionManager.h. Read, not transcribed from the neighbour.
void CollisionManager::drawPivots(bool on)
{
    m_drawPivots = on;
}

// Vtable +0x58, field +0x36. The other half of the same crossing.
void CollisionManager::drawBoundingBoxes(bool on)
{
    m_drawBoundingBoxes = on;
}

// Vtable +0x5C.  `mov [eax+38h], dl`
void CollisionManager::drawNormals(bool on)
{
    m_drawNormals = on;
}

// Vtable +0x60.  `mov [eax+39h], dl`
void CollisionManager::drawTestedFaces(bool on)
{
    m_drawTestedFaces = on;
}

// Vtable +0x64.  `mov [eax+3Ah], dl`
void CollisionManager::drawIntersectFaces(bool on)
{
    m_drawIntersectFaces = on;
}

// Vtable +0x68.  `mov [eax+3Bh], dl`
void CollisionManager::drawSoldierMesh(bool on)
{
    m_drawSoldierMesh = on;
}

// Vtable +0x6C.  `mov [eax+3Ch], dl`
void CollisionManager::drawPrimitives(bool on)
{
    m_drawPrimitives = on;
}

// Vtable +0x70.  `mov [eax+3Dh], dl`
void CollisionManager::drawForces(bool on)
{
    m_drawForces = on;
}

// the mesh-template MAP

// Returns part 0 of a case-insensitive mesh-name lookup, or null if absent.
// Existing entries must have nonempty parts vectors.
CollisionMeshTemplate* CollisionManager::getCollisionMeshTemplate(const std::string& name)
{
    std::map<std::string, std::vector<CollisionMeshTemplate*>,
             NoCaseStringCompare>::iterator it = m_collisionMeshTemplates.find(name);

    if (it != m_collisionMeshTemplates.end())
        return it->second[0];

    return 0;
}

// Returns a new per-object handle for a shared mesh part, creating and requesting the part as needed.
// Negative parts return null. The name must already be registered; lookup has no missing-name guard.
ICollisionMesh* CollisionManager::getCollisionMeshPart(const std::string& name, int part)
{
    if (part < 0)
        return 0;

    std::map<std::string, std::vector<CollisionMeshTemplate*>,
             NoCaseStringCompare>::iterator it = m_collisionMeshTemplates.find(name);

    // [PLATFORM] [CHOSEN, not recovered] A missing registered soldier mesh diagnoses an
    // upstream registration, precache, or reload failure. This portability arm retains an
    // unloaded template so hasLod fails closed; normal loading requires prior registration.
    if (it == m_collisionMeshTemplates.end())
    {
        std::fprintf(stderr,
                     "[PLATFORM] CollisionManager::getCollisionMeshPart miss: \"%s\" part %d "
                     "(pruned or never created; retail walks the tree header here)\n",
                     name.c_str(), part);
        std::fflush(stderr);
        it = m_collisionMeshTemplates.insert(
                 std::make_pair(name, std::vector<CollisionMeshTemplate*>())).first;
    }

    std::vector<CollisionMeshTemplate*>& parts = it->second;    // unguarded in the ELF, see above

    if (part >= static_cast<int>(parts.size()))
        parts.resize(part + 1);                                 // null-filled growth

    CollisionMeshTemplate*& slot = parts[static_cast<unsigned int>(part)];
    if (!slot)
        slot = new CollisionMeshTemplate();

    slot->m_loadRequested = 1;
    return new CollisionMesh(slot);
}

// Registers a collision template with the active object's owner and current Meshes/ directory.
// Duplicate names are ignored case-insensitively. Immediate mode invokes the implemented load body
// (formerly [FRONTIER]); deferred mode only registers. The debug variable controlling eager requests is [UNRESOLVED].

void CollisionManager::create(const std::string& name)
{
    if (m_collisionMeshTemplates.find(name) != m_collisionMeshTemplates.end())
        return;                                             // dup: silently refused

    CollisionMeshTemplate* meshTemplate = new CollisionMeshTemplate;
    meshTemplate->m_name = name;
    meshTemplate->m_directory = io::mainConsole->getWorkingPath() + std::string("Meshes/");
    meshTemplate->m_ownerTemplate = objectTemplateManager->getActiveObjectTemplate();
    if (*s_collisionConfigPtr)
        meshTemplate->m_loadRequested = 1;

    std::vector<CollisionMeshTemplate*> entry;
    entry.push_back(meshTemplate);
    std::map<std::string, std::vector<CollisionMeshTemplate*>,
             NoCaseStringCompare>::iterator inserted =
        m_collisionMeshTemplates.insert(std::make_pair(name, entry)).first;

    if (!m_useDeferredLoading)
    {
        meshTemplate->m_loadRequested = 1;
        load(inserted->second);                             // written below
    }
}

// Deletes matching mesh parts virtually in index order using a signed count snapshot,
// then erases the case-insensitive registry entry. Performs no weak-pointer sweep.
void CollisionManager::deleteCollisionMesh(const std::string& name)
{
    const auto found = m_collisionMeshTemplates.find(name);
    if (found == m_collisionMeshTemplates.end())
        return;
    const int count = static_cast<int>(found->second.size());
    for (int i = 0; i < count; ++i)
        delete found->second[i];
    m_collisionMeshTemplates.erase(found);
}

// Console registration additionally rebinds an existing part-zero directory to the current script's Meshes/.
// Keep that behavior in this wrapper: the underlying create method only rejects duplicates.
namespace {

const char* const kCollisionManagerConsoleTag = "world.physics.collisionManager";

void collisionManager_createTemplate(const std::string& name)
{
    CollisionMeshTemplate* meshTemplate =
        collisionManager->getCollisionMeshTemplate(name);
    if (meshTemplate)
    {
        meshTemplate->m_directory =
            io::mainConsole->getWorkingPath() + std::string("Meshes/");
    }
    else
    {
        collisionManager->create(name);
    }
}

} // namespace

BF2_CONSOLE_OBJECT("CollisionManager", 2, 0)
BF2_CONSOLE_METHOD_1(createTemplate, collisionManager_createTemplate, std::string,
                     kCollisionManagerConsoleTag)

ConsoleObjectList& collisionManagerConsoleObjects()
{
    return consoleBindingList(kCollisionManagerConsoleTag);
}

// Loads all parts of a named collision mesh, or builds the exact Soldier_CollisionMesh fallback.
// Skips loaded entries; a failed part deletes and truncates the remaining tail.
// If no part supplies geometry 0, enables first-person collision on every surviving part.
// The optional pre-v8 save-as-.new tool branch remains [FRONTIER].
// buildEdgeInfo runs only on part 0; its former [FRONTIER - 1,399 B] body is implemented.
namespace {
int  s_collisionNewSuffixValue = 0;                         // ds:0x8ABB8B0
int* s_collisionNewSuffixPtr   = &s_collisionNewSuffixValue; // ds:0x8ABB8B4 (the ".new" debug cell)
} // namespace

void CollisionManager::load(std::vector<CollisionMeshTemplate*>& parts)
{
    CollisionMeshTemplate* first = parts[0];
    if (first->m_loaded == 1)
        return;

    const std::string name = first->m_name;
    const IObjectTemplate* owner = first->m_ownerTemplate;

    // ELF 0x8398D44..0x8398D84 / PE 0x6B0409..0x6B0446: case-sensitive std::string equality.
    if (name == "Soldier_CollisionMesh")
    {
        first->load(0, owner, false);                        // the soldier capsule
        first->m_loadRequested = 0;
        return;
    }

    std::string path = first->m_directory + name;
    path.append(".collisionMesh");
    if (*s_collisionNewSuffixPtr)
        path.append(".new");

    io::IStream* f = io::fileManager->open(path, (io::IFileSystem::FileMode)2,
                                           (io::IFileSystem::FileAccess)0, 0);
    if (!f && *s_collisionConfigPtr)
    {
        path = first->m_directory + name;
        path.append(".collisionMesh");
        f = io::fileManager->open(path, (io::IFileSystem::FileMode)2,
                                  (io::IFileSystem::FileAccess)0, 0);
    }
    if (!f)
        return;

    unsigned int major = 0, minor = 0, partCount = 0;
    f->read(&major, 4);
    f->read(&minor, 4);
    f->read(&partCount, 4);
    if (partCount > parts.size())
        parts.resize(partCount);                             // null-filled

    bool anyGeom0 = false;
    int firstFailed = -1;
    for (unsigned int i = 0; i < partCount; ++i)
    {
        CollisionMeshTemplate* t = parts[i];
        if (!t)
            t = new CollisionMeshTemplate();
        t->setVersionToLoad(major, minor);
        if (!t->load(static_cast<io::INonlinearStream*>(f), owner, true))
        {
            if (firstFailed == -1) firstFailed = static_cast<int>(i);
        }
        else
        {
            firstFailed = -1;
        }
        t->m_loadRequested = 0;
        parts[i] = t;
        anyGeom0 = t->isGeomValid(0) || anyGeom0;
        if (i == 0)
            t->verifyPivot();
    }
    if (firstFailed > 0)
    {
        for (unsigned int i = firstFailed; i < partCount; ++i)
            delete parts[i];
        parts.resize(firstFailed);
        partCount = firstFailed;
    }

    f->release();

    if (!anyGeom0)
        for (unsigned int i = 0; i < partCount; ++i)
            parts[i]->setUseCollisionAsFirstPerson();

    // (tool mode: `if (*s_collisionConfigPtr && minor <= 7)` re-save as .new via
    //  CollisionMeshTemplate::save - [FRONTIER], the cell is dormant, the arm is banked)

    parts[0]->buildEdgeInfo();                               // [FRONTIER] - part 0 only, as read
}

// Records a line as two vertices and one segment color. drawDebugPrimitives consumes
// and clears the accumulated streams during the physics draw event.
void CollisionManager::drawLine(const Vec3& a, const Vec3& b, unsigned int color)
{
    m_lineVertices.push_back(a);
    m_lineVertices.push_back(b);
    m_lineColors.push_back(color);
}

// CollisionManager::drawTriangle - Vtable +0x7C.
// The same shape with three vertices against one colour, into the other pair of vectors.
void CollisionManager::drawTriangle(const Vec3& a, const Vec3& b, const Vec3& c, unsigned int color)
{
    m_triangleVertices.push_back(a);
    m_triangleVertices.push_back(b);
    m_triangleVertices.push_back(c);
    m_triangleColors.push_back(color);
}

// Records force only when drawForces and drawPrimitives are enabled.
// Virtual drawLine calls emit the shaft and a blue last-tenth segment, not a barbed arrowhead.
void CollisionManager::drawForce(const Vec3& origin, const Vec3& force, unsigned int color)
{
    if (m_drawForces && m_drawPrimitives)
    {
        Vec3 tip;
        tip.x = origin.x + force.x;
        tip.y = origin.y + force.y;
        tip.z = origin.z + force.z;

        drawLine(origin, tip, color);                   // 0x839B3B4, call [eax+78h]

        Vec3 neck;
        neck.x = origin.x + force.x * 0.9f;
        neck.y = origin.y + force.y * 0.9f;
        neck.z = origin.z + force.z * 0.9f;

        drawLine(neck, tip, 0xFF0000FFu);               // 0x839B429, colour literal at 0x839B41B
    }
}

// CollisionManager::rotateObjectInFocusAroundVector - Vtable +0x98.

// The focus family below now uses the shared object/camera interfaces. Unlike the axis wrapper,
// this wrapper performs no second Vars::getFloat call on a focus miss.
void CollisionManager::rotateObjectInFocusAroundVector(const Vec3& axis, float radians)
{
    IObject* object = getObjectInFocus();               // 0x839B74F, static
    if (object)
        rotateObjectAroundVector(object, axis, radians);// 0x839B769, direct member call
}

// ELF 0x839C020 / PE 0x6AFB40. The body never reads an owner; preserve the existing static
// declaration. The ELF callers do pass an unused owner, so that alone does not prove staticness.
IObject* CollisionManager::getObjectInFocus()
{
    Vec3 origin = {0.0f, 0.0f, 0.0f};
    Vec3 direction = {0.0f, 0.0f, 0.0f};
    PlayerManager* players = reinterpret_cast<PlayerManager*>(
        classManager->getSingleton(std::string("PlayerManager")));
    if (players)
    {
        IPlayer* player = players->getLocalHumanPlayer();
        if (player)
        {
            // Unlike debugHeightmap, retail does not guard the camera pointer here.
            const Mat4& camera = player->getCamera()->getTransformation(0.0f);
            origin = {camera.m[12], camera.m[13], camera.m[14]};
            direction = {camera.m[8], camera.m[9], camera.m[10]};
            origin.x += direction.x;
            origin.y += direction.y;
            origin.z += direction.z;
        }
    }
    ObjectFlagPredicator predicator(0x100);
    const float distance = *Vars::getFloat("coll-name-dist", 100.0f);
    direction.x *= distance;
    direction.y *= distance;
    direction.z *= distance;
    IObject* hitObject = 0;
    Vec3 hitPoint = {0.0f, 0.0f, 0.0f};
    Vec3 hitNormal = {0.0f, 0.0f, 0.0f};
    int hitPart;
    objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, origin, direction,
                                 &predicator, true, false, false, true, 0);
    return hitObject;
}

// ELF 0x839B440 / PE 0x6AFCB0. The formatted text is discarded in both shipped images.
void CollisionManager::getNameOfObjectInFocus()
{
    IObject* object = getObjectInFocus();
    if (!object)
    {
        (void)Vars::getFloat("coll-name-dist", 100.0f);
        return;
    }
    ICollisionMesh* mesh = static_cast<ICollisionMesh*>(
        object_detail::LayoutAccess::collisionMesh(getRootParent(object)));
    char buffer[1024];
    if (mesh)
    {
        const std::string name = mesh->getPathAndName();
        std::sprintf(buffer, "Name: %s\nCreated by: %s\nLast modified by: %s\n",
                     name.c_str(), "(unknown)", "(unknown)");
    }
    else
        std::sprintf(buffer, "Name: %s\nCreated by: %s\nLast modified by: %s\n",
                     "(unknown)", "(unknown)", "(unknown)");
}

// ELF 0x839B550 / PE 0x6AFD80. Preserve the second variable lookup on the no-object arm.
void CollisionManager::updateFlagsOfObjectInFocus(unsigned int mask, unsigned int value)
{
    IObject* object = getObjectInFocus();
    if (object)
        updateFlagsRecursive(object, mask, value);
    else
        (void)Vars::getFloat("coll-name-dist", 100.0f);
}


void CollisionManager::setSleepinessOfObjectInFocus(int sleepiness)
{
    IObject* object = getObjectInFocus();
    if (object)
        setSleepinessRecursive(object, sleepiness);
    else
        (void)Vars::getFloat("coll-name-dist", 100.0f);
}

// ELF 0x839B5F0 / PE 0x6AFDF0. Save the position by value before the identity setter.
void CollisionManager::identityRotateObjectInFocus()
{
    IObject* object = getObjectInFocus();
    if (object)
    {
        const Vec3 position = object->getAbsolutePosition();
        object->setAbsoluteTransformation(Mat4::getIdentity());
        object->setAbsolutePosition(position);
    }
}

// ELF 0x839B660 / PE 0x6AFFD0. Invalid axis ids still rotate about the zero vector.
void CollisionManager::rotateObjectInFocusAroundAxis(unsigned int axis, float degrees)
{
    IObject* object = getObjectInFocus();
    if (object)
    {
        Vec3 direction = {0.0f, 0.0f, 0.0f};
        switch (axis)
        {
        case 0: direction.x = 1.0f; break;
        case 1: direction.y = 1.0f; break;
        case 2: direction.z = 1.0f; break;
        }
        rotateObjectAroundVector(object, direction, degrees);
    }
    else
        (void)Vars::getFloat("coll-name-dist", 100.0f);
}

// ELF 0x839C1E0 / PE 0x6AFFD0 and 0x6B0090 inline the 0x5815B0 helper. Angles are
// degrees (setRotateAboutLine). Translation and the fourth column survive the 3x3 product.
void CollisionManager::rotateObjectAroundVector(IObject* object, const Vec3& axis, float degrees)
{
    Mat4 transformation = object->getAbsoluteTransformation();
    if (std::fabs(degrees) > FLT_EPSILON)
    {
        Mat4 rotation = Mat4::getIdentity(); // Mat4 is currently an aggregate in this tree.
        setRotateAboutLine(rotation, degrees, axis);
        const Mat4 original = transformation;
        transformation.rotMult(original, rotation);
    }
    // Zero/epsilon/NaN still invoke the setter with the original matrix.
    object->setAbsoluteTransformation(transformation);
}

// ELF 0x839ABB0 / PE 0x6AFEF0. Loading is tested on the first template, not on each part.
void CollisionManager::buildDebugCollisionMeshes()
{
    Vars::set("coll-load-debugmeshes", "1");
    for (auto it = m_collisionMeshTemplates.begin(); it != m_collisionMeshTemplates.end(); ++it)
        if (it->second[0]->m_loaded == 1)
            for (int i = 0; i < static_cast<int>(it->second.size()); ++i)
                it->second[i]->buildDebugMeshes(false);
}

// ELF 0x839ACD0 / PE 0x6B02F0. Both images copy the whole vector on each iteration;
// retain the copy and the per-item null check rather than hoisting the copy out of the loop.
void CollisionManager::destroyDebugCollisionMeshes()
{
    for (auto it = m_collisionMeshTemplates.begin(); it != m_collisionMeshTemplates.end(); ++it)
        for (int i = 0; i < static_cast<int>(it->second.size()); ++i)
        {
            const std::vector<CollisionMeshTemplate*> templates = it->second;
            if (templates[i])
                templates[i]->destroyDebugMeshes(false);
        }
}

// Appends a camera-centred grid of normal/edge endpoints (mask 1) and vertical ray-hit normals (mask 2).
void CollisionManager::debugHeightmap(std::vector<Vec3>& out, int mode) const
{
    PlayerManager* players = reinterpret_cast<PlayerManager*>(
        classManager->getSingleton(std::string("PlayerManager")));
    if (!players)
        return;
    IPlayer* player = players->getLocalHumanPlayer();
    if (!player)
        return;
    ICameraObject* camera = player->getCamera();
    if (!camera)
        return;
    const Mat4& cameraTransform = camera->getTransformation(0.0f);
    IHeightmap* heightmap = static_cast<IHeightmapCluster*>(heightmapCluster)->getPrimaryHeightmap();
    const float step = heightmap->getScale().x;
    mode = static_cast<int>(static_cast<float>(mode) * step);
    const int centerX = static_cast<int>(static_cast<float>(
        static_cast<int>(cameraTransform.m[12] / step + 0.5f)) * step);
    const int centerZ = static_cast<int>(static_cast<float>(
        static_cast<int>(cameraTransform.m[14] / step + 0.5f)) * step);
    const float oneThird = step / 3.0f;
    const float twoThirds = (2.0f * step) / 3.0f;
    for (int x = -mode / 2; x < mode / 2; x += static_cast<int>(step))
        for (int z = -mode / 2; z < mode / 2; z += static_cast<int>(step))
        {
            const float cellX = static_cast<float>(centerX + x);
            const float cellZ = static_cast<float>(centerZ + z);
            Vec3 normal = {0.0f, 0.0f, 0.0f};
            Vec3 a = {cellX + oneThird, 0.0f, cellZ + oneThird};
            Vec3 b = {cellX + twoThirds, 0.0f, cellZ + twoThirds};
            unsigned char material;
            const int mask = *Vars::getInt("hc_mask", 1);
            if (mask & 1)
            {
                a.y = heightmap->getHeightAndNormalInWorldCoords(a.x, a.z, normal, material);
                out.push_back(a);
                out.push_back({a.x + normal.x, a.y + normal.y, a.z + normal.z});
                b.y = heightmap->getHeightAndNormalInWorldCoords(b.x, b.z, normal, material);
                out.push_back(b);
                out.push_back({b.x + normal.x, b.y + normal.y, b.z + normal.z});
                // Two triangles, with the retail repeated height queries and duplicated endpoints.
                const Vec3 corners[] = {
                    {cellX, 0.0f, cellZ}, {cellX + step, 0.0f, cellZ},
                    {cellX, 0.0f, cellZ + step}, {cellX, 0.0f, cellZ},
                    {cellX + step, 0.0f, cellZ}, {cellX, 0.0f, cellZ + step},
                    {cellX + step, 0.0f, cellZ + step}, {cellX + step, 0.0f, cellZ}
                };
                const bool duplicate[] = {false, true, true, false, false, true, true, false};
                for (int i = 0; i < 8; ++i)
                {
                    Vec3 point = corners[i];
                    point.y = heightmap->getHeightAndNormalInWorldCoords(point.x, point.z, normal, material);
                    out.push_back(point);
                    if (duplicate[i])
                        out.push_back(point);
                }
            }
            if (mask & 2)
            {
                Vec3 hit = {0.0f, 0.0f, 0.0f};
                float fraction;
                const Vec3 points[] = {a, b};
                for (int i = 0; i < 2; ++i)
                {
                    const Vec3 from = {points[i].x, points[i].y + 100.0f, points[i].z};
                    const Vec3 to = {points[i].x, points[i].y - 100.0f, points[i].z};
                    if (heightmap->intersectRayInWorldCoords(from, to, hit, fraction, normal, material, true))
                    {
                        out.push_back(hit);
                        out.push_back({hit.x + normal.x, hit.y + normal.y, hit.z + normal.z});
                    }
                }
            }
        }
}

// Queues collision meshes weakly for geometry changes between physics frames.
// postFrameUpdate applies executeSetGeometry(false) to surviving targets, then clears the queue.
void CollisionManager::addToPostFrameUpdate(ICollisionMesh* mesh)
{
    m_postFrameUpdate.push_back(mesh->getWeakPtr());    // slot 7 - the lazy control block
}

void CollisionManager::postFrameUpdate()
{
    const unsigned int count = static_cast<unsigned int>(m_postFrameUpdate.size());
    for (unsigned int i = 0; i < count; ++i)
    {
        ICollisionMesh* mesh = m_postFrameUpdate[i].get();
        if (mesh)
            mesh->executeSetGeometry(false);            // slot 14, +0x38 - the deferred swap
    }
    m_postFrameUpdate.resize(0);
}

} // namespace world
} // namespace hfe
} // namespace dice

// Debug geometry still requires the complete geometry backend. Line drawing retains
// its named interface frontiers; manager helpers do not replace downstream resource work.
