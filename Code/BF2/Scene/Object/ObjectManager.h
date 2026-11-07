// World-object registration, lookup and update scheduling.
// [INFERRED] Header placement beside attested Scene/Object/ObjectManager.cpp.
// Virtual declaration order is ABI; native ports use their actual member layouts.

#ifndef SCENE_OBJECT_OBJECTMANAGER_H     // [INFERRED] guard style
#define SCENE_OBJECT_OBJECTMANAGER_H

#include <list>
#include "../../Core/Hash.h"        // M_nameHashes (+0x24) - the name lookup,
#include <map>
#include <string>
#include <vector>

#include "../../Core/WeakPtr.h"

// world::IObject, world::Object, and (via IObjectComponent.h) world::TemplateMessage.

// not Game/Objects/Object.h, and that is not a preference. Object.h includes IObject.h and then
// defines `class Object` a second time - IObject.h already defines it at its line 180 - so any TU
// that includes Object.h dies on C2011 before reaching anything else. Reported, not fixed here:
// that file belongs to whoever owns Game/Objects/. The two enums Object.h would have supplied are
// forward-declared below instead.
#include "../../Game/Objects/IObject.h"
#include "../../Math/Vec3.h"   // hfe::Vec2, hfe::Vec3 (typedefs of Math::Vec2 / Math::Vec3)
#include "../../Math/Mat4.h"   // hfe::Mat4

namespace dice {
namespace hfe {

// [UNRESOLVED] layout and members. Named by the demangled ObjectManager signatures only.
class Frustum;
struct LocalBoundingBox;

namespace world {

// dice's misspelling - frozen. Same declaration Game/Objects/Object.h carries; forward-declared
// here rather than included for the C2011 reason given at the include block. Spelled as a C++11
// opaque enum with Object.h's fixed `: int` underlying type, which every compiler
// accepts; it becomes an ordinary include the day Object.h compiles.
enum UpdateFreqencyType : int;

class CullerObject;          // an object's slot in the culler; IObject holds one at +0x1C
class IObjectTemplate;
class ObjectCache;           // Scene/Object/ObjectCache.h - the cached-destroy pool (declaration only)
class ObjectPredicator;      // the query filter every grid/raycast overload takes
class ICuller;               // implemented by BinTreeCuller (_ZTV @ 0x88C8DE0) and QuadTreeCuller
                             // (@ 0x88CA300); optimize() is its vtable +0x28
class ObjectGrid;            // [UNRESOLVED] - getObjectGrid's 7-byte body just returns a field

// Adds an enabled sibling chain and its descendants depth-first, applying the optional
// predicator at each enabled node.  The radius query uses it to expand root culler hits.
void addChildObjects(IObject* object, std::vector<IObject*>& out, ObjectPredicator* predicator);

// The vtable, in slot order, 76 slots / 304 bytes.
class ObjectManager /* : public IObjectManager */
{
public:
    // IBase quartet, +0x00..+0x0C
    virtual void  addRef();                                                     // +0x00
    virtual int   getRef();                                                     // +0x04
    virtual int   release();                                                    // +0x08
    virtual void* queryInterface(unsigned int iid) const;                       // +0x0C

    // Object lifecycle
    virtual IObject* createObject(const IObjectTemplate* templ, unsigned int id,
                                  const Mat4* transform);                       // +0x10
    virtual void  destroyObject(IObject* object);                               // +0x14

    // Statistics
    virtual void  setDrawStats(int level);                                      // +0x18
    virtual int   getDrawStats() const;                                         // +0x1C

    // The grid
    virtual void  initGrid(const Vec2& min, const Vec2& max, int cells,
                           const Vec3& origin);                                 // +0x20
    virtual IObject* getObjectFromNameHash(const std::string& name);            // +0x24

    // The two update channels
    // two separate lists and two separate entry points. updateObjects runs per simulation step and
    // is frequency-filtered; handleFrameUpdates runs per frame and is not. See Game/Main/BF2Engine.cpp
    // for the fixed-timestep loop that makes the distinction observable.
    virtual void  updateObjects(float deltaTime, UpdateFreqencyType type,
                                unsigned int frame);                            // +0x28
    virtual void  handleFrameUpdates(float deltaTime);                          // +0x2C
    // Grid queries.
    virtual IObject* getObjectFromGrid(unsigned int id);                        // +0x30
    // Object maps iterate in ID order, which determines network-ID assignment order.
    virtual std::map<unsigned int, IObject*>& getAllObjectsFromGrid();          // +0x34
    virtual const std::map<unsigned int, IObject*>&
                  getAllRegisteredGamePlayObjects() const;                      // +0x38
    // The second vector in direction queries is a delta, not an absolute endpoint.
#if defined(_WIN32)
    // [PLATFORM] MSVC reverses declarations within same-name overload groups. Keep this
    // query family in its current source order so emitted PE slots match the interface;
    // PE and ELF tables rejoin at intersectLine.
    virtual void  getObjectsFromGrid(std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x48
    virtual void  getObjectsFromGrid(const Vec3& centre, float radius,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x44
    virtual void  getObjectsFromGrid(const Frustum& frustum,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred, bool b,
                                     float f1, float f2);                       // +0x40
    virtual void  getObjectsFromGrid(const Vec3& from, const Vec3& direction,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred, bool testCollisionMesh,
                                     int collisionLod, bool firstHit);           // +0x3C
    virtual void  getNoCollisionObjectsFromGrid(const Vec3& centre, float radius,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x50
    virtual void  getNoCollisionObjectsFromGrid(const Frustum& frustum,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred, bool b,
                                     float f1, float f2);                       // +0x4C
    virtual void  appendGetObjectsFromGrid(const Vec3& centre, float radius,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x54
    virtual void  clearGetObjectsFromGrid();                                    // +0x58
    virtual void  getObjectsFromGridNoCullConst(const Frustum& frustum,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred, bool b,
                                     float f1, float f2);                       // +0x5C
    // A spatial query with LAG compensation. The "latency" pair queries a past state of the grid;
    // that is a property of the object manager here, not of the netcode.
    virtual void  getObjectsFromGridWithLatency(const Vec3& from, const Vec3& direction,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x64
    virtual void  getObjectsFromGridWithLatency(const Vec3& centre, float radius,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x60
#else
    virtual void  getObjectsFromGrid(std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x3C
    virtual void  getObjectsFromGrid(const Vec3& centre, float radius,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x40
    virtual void  getNoCollisionObjectsFromGrid(const Vec3& centre, float radius,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x44
    virtual void  appendGetObjectsFromGrid(const Vec3& centre, float radius,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x48
    virtual void  clearGetObjectsFromGrid();                                    // +0x4C
    virtual void  getObjectsFromGrid(const Frustum& frustum,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred, bool b,
                                     float f1, float f2);                       // +0x50
    virtual void  getObjectsFromGridNoCullConst(const Frustum& frustum,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred, bool b,
                                     float f1, float f2);                       // +0x54
    virtual void  getNoCollisionObjectsFromGrid(const Frustum& frustum,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred, bool b,
                                     float f1, float f2);                       // +0x58
    virtual void  getObjectsFromGrid(const Vec3& from, const Vec3& direction,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred, bool testCollisionMesh,
                                     int collisionLod, bool firstHit);           // +0x5C
    virtual void  getObjectsFromGridWithLatency(const Vec3& from, const Vec3& direction,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x60
    virtual void  getObjectsFromGridWithLatency(const Vec3& centre, float radius,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* pred);                   // +0x64
#endif

    // Raycasting and intersection
    // The second Vec3 is a delta from `from`, not an endpoint. Parameter names do not affect ABI.
    virtual bool  intersectLine(IObject*& hitObject, Vec3& hitPoint, Vec3& hitNormal,
                                int& hitPart, const Vec3& from, const Vec3& direction,
                                ObjectPredicator* pred, bool b1, bool b2, bool b3,
                                bool b4, unsigned int u);                       // +0x68
    // Sweeps a sphere along a direction delta; returns whether candidates existed.
    virtual bool  intersectPill(std::vector<Vec3>& hitPoints, std::vector<Vec3>& hitNormals,
                                const Vec3& from, const Vec3& direction, float radius,
                                ObjectPredicator* pred, bool b);                // +0x6C
    virtual bool  intersectLineWithSpheres(IObject*& hitObject, Vec3& hitPoint,
                                Vec3& hitNormal, int& hitPart, const Vec3& from,
                                const Vec3& direction, ObjectPredicator* pred,
                                ObjectPredicator* pred2, float probabilityScale); // +0x70
    // PE 0x00727850 / ELF 0x08318940 return a Boolean; outPart is unused.
    virtual bool raycast(const Vec3& from, const Vec3& direction, ObjectPredicator* pred,
                                ObjectPredicator* pred2, unsigned int* outPart); // +0x74

    // [PLATFORM] Keep the shared source order for render-culling overloads. GCC preserves
    // it; MSVC groups and reverses same-name overloads to produce the required PE table.
    virtual void  getStaticObjectsFromGridRend(const Frustum& frustum,
                                std::vector<IObject*>& out, LocalBoundingBox* box,
                                ObjectPredicator* pred, unsigned int u, bool b); // +0x78
    virtual void  getDynamicObjectsFromGridRend(const Frustum& frustum,
                                std::vector<IObject*>& out, LocalBoundingBox* box,
                                ObjectPredicator* pred, unsigned int u, bool b); // +0x7C
    virtual void  getStaticObjectsFromGridRend(const Vec3& centre, float radius,
                                std::vector<IObject*>& out, LocalBoundingBox* box,
                                ObjectPredicator* pred, unsigned int u, bool b); // +0x80
    virtual void  getDynamicObjectsFromGridRend(const Vec3& centre, float radius,
                                std::vector<IObject*>& out, LocalBoundingBox* box,
                                ObjectPredicator* pred, unsigned int u, bool b); // +0x84
    virtual void  addChildrenRend(const std::vector<IObject*>& in,
                                std::vector<IObject*>& out, LocalBoundingBox* box,
                                ObjectPredicator* pred);                        // +0x88
    virtual void  addChildrenRend(IObject* object, std::vector<IObject*>& out,
                                LocalBoundingBox* box, ObjectPredicator* pred);  // +0x8C

    // The active object
    virtual IObject* getActiveObject() const;                                   // +0x90
    virtual void  setActiveObject(IObject* object);                             // +0x94

    // Deferred destruction
    virtual void  destroyObjects();                                             // +0x98

    // Grid maintenance
    virtual void  refreshTerrainSampling();                                     // +0x9C  0x83128E0, 5 B empty
    virtual void  refreshGridObject(IObject* object);                           // +0xA0
    // no arguments, and it is the per-frame culler pass, not a spatial rebuild.
    virtual void  updateObjectsInGrid();                                        // +0xA4
                                                                                //        recovered, see .cpp
                                                                                //        mainLoop 0x81377F3
    virtual std::vector<IObject*>& getObjectTempVector();                       // +0xA8
    virtual void  getObjectsInBoxFromGrid(const Vec3& min, const Vec3& max,
                                std::vector<IObject*>& out,
                                ObjectPredicator* pred);                        // +0xAC

    // The registry
    virtual IObject* getRegisteredObject(int id);                               // +0xB0
                                                                                //   = m_allRegisteredObjects
                                                                                //   .find(id) - read in full
    virtual const std::map<unsigned int, IObject*>&
                  getAllRegisteredObjects() const;                              // +0xB4
                                                                                //   (see the map correction
                                                                                //   at the grid queries)

    // Cull-distance constants (ELF +0x110/+0x114/+0x118; ctor 4.0f / 5.0f / 100.0f)
    virtual void  setMinCullDistance(float d);                                  // +0xB8
    virtual float getMinCullDistance();                                         // +0xBC
    virtual void  setDistanceCullConst(float c);                                // +0xC0
    virtual float getDistanceCullConst();                                       // +0xC4
    virtual void  setDistanceCullConstPCO(float c);                             // +0xC8
    virtual float getDistanceCullConstPCO();                                    // +0xCC

    // The object message queue
    // Objects talk to each other asynchronously through here, with targeted teardown. A rebuild that
    // substitutes direct calls changes ordering guarantees.
    virtual void  postMessage(IObject* to, TemplateMessage message, float delay); // +0xD0
    // (non-virtual) the message queue's tick - called at the end of every updateObjects
    void checkMessages(float deltaTime);
    virtual void  removeAllMessagesTo(IObject* to);                             // +0xD4

    // Registration
    virtual unsigned int getNewID();                                            // +0xD8
    virtual void  registerObject(IObject* object);                              // +0xDC
    virtual void  unregisterObject(IObject* object);                            // +0xE0
    virtual void  deleteAllRegisteredObjects();                                 // +0xE4  0x8310C00, 1190 B  the unload's object teardown

    // The name index
    virtual void  addObjectToNameHash(IObject* object);                         // +0xE8
    virtual void  removeObjectFromNameHash(IObject* object);                    // +0xEC

    // The two update lists
    virtual void  addObjectToUpdate(IObject* object);                           // +0xF0
    virtual void  removeObjectFromUpdate(IObject* object);                      // +0xF4
    // both of these defer while m_inFrameUpdate is SET - see the note at the top of this header.
    virtual void  addObjectToFrameUpdate(IObject* object);                      // +0xF8
    virtual void  removeObjectFromFrameUpdate(IObject* object);                 // +0xFC

    // Grid membership
    virtual void  addObjectToGrid(IObject* object);                             // +0x100
    virtual void  updateObjectInGrid(IObject* object);                          // +0x104 0x8313070, 5 B empty
    virtual void  removeObjectFromGrid(IObject* object);                        // +0x108
    virtual void  getCameraInfo(Mat4* transform, float* f1, float* f2);         // +0x10C

    // Hit-detection LOD override
    // A server testing against a different LOD than the client renders will disagree about hits.
    virtual void  setOverrideLineTestLod(int lod);                              // +0x110
    virtual int   getOverrideLineTestLod();                                     // +0x114

    virtual void  addObjectToDestroyList(IObject* object);                      // +0x118

    // slot position is deliberate - see the vtable note at the top. D1 0x830F650 (1441 B) /

    virtual ~ObjectManager();                                                   // +0x11C / +0x120

    virtual ObjectGrid* getObjectGrid();                                        // +0x124

public:
    // C2 0x830DDE0 / C1 0x830E740. declared only - the member-init list is documented in the layout
    // block above, the body is not recovered.
    ObjectManager();

    // The factory ClassManager::createInstance calls. Signature is fixed by ClassRegInfo::create in
    // Core/ClassManager.h and by the mangled name
    // _ZN4dice3hfe5world13ObjectManager6createEjPNS0_5IBaseE. Registered by world::ObjectModule as
    // "dice.hfe.world.ObjectManager.Standard" (CID_ObjectManager); the singleton row that makes
    // ClassManager actually call it is world::Module's, flags 1.
    static IBase* create(unsigned int iid, IBase* outer);

    // Non-virtual members the recovered bodies call. declared only.
    // Their absence from the vtable is not an accident: nothing outside this class calls them.
    void drawStats();
    void addObjectToCuller(IObject* object);
    void updateObjectInCuller(IObject* object);
    void removeObjectFromCuller(IObject* object);

private:
    void getObjectsFromGridByRadiusInternal(bool collisionObjects, bool latencyObjects,
                                            const Vec3& centre, float radius,
                                            std::vector<IObject*>& out,
                                            ObjectPredicator* predicator);

    // The two overloads and their common postprocessor are symbol-attested in bf2.i64.  Return
    // void is fixed by every caller discarding the incidental iterator/pointer left in eax.
    void getObjectsFromGridRendInternal(const Frustum& frustum,
                                        std::vector<IObject*>& out,
                                        LocalBoundingBox* box,
                                        ObjectPredicator* predicator,
                                        unsigned int mode,
                                        bool staticObjects,
                                        bool useCollisionBoundsAndChildren);
    void getObjectsFromGridRendInternal(const Vec3& centre, float radius,
                                        std::vector<IObject*>& out,
                                        LocalBoundingBox* box,
                                        ObjectPredicator* predicator,
                                        unsigned int mode,
                                        bool staticObjects,
                                        bool useCollisionBoundsAndChildren);
    void getObjectsFromGridRendInternal(std::vector<IObject*>& out,
                                        LocalBoundingBox* box,
                                        ObjectPredicator* predicator,
                                        unsigned int mode,
                                        bool staticObjects,
                                        bool useCollisionBoundsAndChildren);

    // Per-query distance-cull record of nine floats. Keep it nested in ObjectManager for signature linkage;
    // field names marked [INFERRED] are interpreted from use.
    struct CullConsts
    {
        float minCullDistance;      // +0x00  [0] quality-scaled; setObjectCullConsts scales it
        float radiusBias;           // +0x04  [1] [INFERRED] added to a radius by setCullFade
        float cullConstScale;       // +0x08  [2] the (quality * 10) factor the distances multiply
        float distanceCullConst;    // +0x0C  [3] the squared cull distance for this object
        float cameraX;              // +0x10  [4] the three together are the camera position,
        float cameraY;              // +0x14  [5]   copied out of the frustum's [105..107]
        float cameraZ;              // +0x18  [6]
        float fadeStep;             // +0x1C  [7] [INFERRED] setCullFade's per-frame fade delta
        float radiusThreshold;      // +0x20  [8] cullRadiusScale * visualRadius * subRadiusThreshold
    };

    // Fills both records for one object.  See the body.
    void setObjectCullConsts(IObject* object, CullConsts& consts, CullConsts& subConsts);

    // Drives one object's distance fade.  See the body.
    void setCullFade(IObject* object, float radius, const CullConsts& consts);

    // The frustum grid query all three public overloads reach.
    void getObjectsFromGridInternal(bool collisionObjects, const Frustum& frustum,
                                    std::vector<IObject*>& out,
                                    ObjectPredicator* predicator,
                                    bool frustumTestEachObject,
                                    float unused, float fadeStep);

    void getObjectsFromGridNoCullConstInternal(bool collisionObjects, const Frustum& frustum,
                                               std::vector<IObject*>& out,
                                               ObjectPredicator* predicator,
                                               bool frustumTestEachObject,
                                               float unused, float unusedFadeStep);

    // Fades one object, adds it, and recurses into its children.
    void cullAndAddObjectHierarchy(IObject* object, std::vector<IObject*>& out,
                                   ObjectPredicator* predicator,
                                   const CullConsts& consts,
                                   const CullConsts& subConsts);

    // only the fields the recovered bodies read. ELF offsets in the comments; see the layout block.
    int          m_refCount;                                   // +0x04
    ICuller*     m_quadTreeCuller;                             // +0x08 collision/dynamic
    bool         m_gridCullerOptimized;                        // +0x0C  [INFERRED name]
    ICuller*     m_gridCuller;                                 // +0x10 collision/static
    ICuller*     m_noCollisionGridCuller;                      // +0x14 no-collision/static
    ICuller*     m_noCollisionQuadTreeCuller;                  // +0x18 no-collision/dynamic
    ICuller*     m_latencyCompensationCuller;                  // +0x1C latency-expanded
    // Case-insensitive name index, initialized with 3011 buckets.
    Hash<std::string, IObject*, StringHashNoCaseHashFunc> m_nameHashes;   // +0x24..+0x2F
    // +0x90 - the "deleting all registered objects" latch, set for the whole of
    // deleteAllRegisteredObjects. [INFERRED name]; nothing else recovered reads it yet.
    bool         m_deletingAllObjects;                         // +0x90
    std::map<unsigned int, IObject*> m_frameUpdateObjects;     // +0x50  keyed by IObject +0x10
    bool         m_inFrameUpdate;                              // +0x68  [INFERRED name]
    std::vector<IObject*> m_frameUpdateAddList;                // +0x6C  [INFERRED name]
    std::vector<IObject*> m_frameUpdateRemoveList;             // +0x78  [INFERRED name]
    std::vector<WeakPtr<IObject> > m_objectsToUpdateInGrid;    // +0x84  [INFERRED name]
    int          m_drawStats;                                  // +0x11C

    // Active object selected by the manager.
    IObject*     m_activeObject;                               // +0x20  get 0x831B1A0 / set 0x831B1B0
    float        m_distanceCullConst;                          // +0x110 ctor 4.0f
    float        m_distanceCullConstPCO;                       // +0x114 ctor 5.0f
    float        m_minCullDistance;                            // +0x118 ctor 100.0f
    int          m_overrideLineTestLod;                        // +0x124 ctor -1
    unsigned int m_newId;                                      // +0x0E4 getNewID PRE-increments
    // Registered objects keyed by object id; registering an existing id replaces its value.
    std::map<unsigned int, IObject*> m_allRegisteredObjects;   // +0x094 keyed by IObject +0x10;
                                                               //   registerObject overwrites an
                                                               //   existing id's mapped value
    std::map<unsigned int, IObject*> m_allRegisteredGamePlayObjects;  // +0x0A0 (key/mapped inferred
                                                               //   - layout-scan map, sibling types)
    std::map<unsigned int, IObject*> m_objectsFromGrid;        // +0x0D8 - getAllObjectsFromGrid's
                                                               // Simulation-step updates visit enabled objects in id order and filter by update
                                                               // frequency. Removals during traversal are deferred; frame updates use a separate map.
    std::map<unsigned int, IObject*> m_updateObjects;          // +0x044 (was m_field44Map)
    bool                  m_isUpdatingObjects;                 // +0x030 ctor 0 - the update walk's re-entrancy latch
    std::vector<IObject*> m_removeFromUpdateList;              // +0x034 removals deferred while the latch is up
    // The deferred-destroy double buffer: addObjectToDestroyList appends to *m_currentDestroyList,
    // destroyObjects flips the buffer, drains the old one (destroyObject / the object cache) and
    // recurses until a drain finds nothing new. Ctor: index 0, current = &m_destroyLists[0].
    std::vector<IObject*> m_destroyLists[2];                   // +0x0B0 / +0x0BC
    std::vector<IObject*>* m_currentDestroyList;               // +0x0AC
    int                   m_destroyListIndex;                  // +0x0C8
    // The template message queue: postMessage inserts {due time, WeakPtr target, message} sorted
    // by due time; checkMessages (called at the end of every updateObjects) advances the clock by dt
    // and delivers everything due to still-live targets through IObject::handleMessage(msg, 0, false).
    struct Message
    {
        /* +0x00 */ double           time;                     // due time on the manager's own clock
        /* +0x08 */ WeakPtr<IObject> target;
        /* +0x0C */ TemplateMessage  message;
    };
    double                m_lastMessageCheckTime;              // +0x0E8 ctor 0.0 - the message clock (sum of dts)
    std::list<Message>    m_messages;                          // +0x0F0
    unsigned int          m_clientUpdateTick;                  // +0x10C ctor 0 - the client path's next full-update tick
    ObjectCache*          m_objectCache;                       // +0x120 Owned object pool, created during member initialization.
    std::vector<IObject*> m_objectTempVector;                  // +0x0CC
    // Direction-query candidates.  Retail keeps the equivalent pair vector at ELF +0x5C and
    // clears it before/after intersectLine; source-level placement is intentionally independent of
    // either port's STL layout.
    std::vector<std::pair<float, IObject*> > m_lineCandidates;
    // Render-query scratch.  The ELF owns these at +0xF4/+0x100; names are [INFERRED], while the
    // types and the two-list roles are fixed by the culler signatures and full method bodies.
    std::vector<IObject*> m_rendVisibleObjects;
    std::vector<IObject*> m_rendAllVisibleObjects;
    // [UNRESOLVED] the remaining ~20 fields listed in the layout block.
};

// ds:0x8B1B920, symbol _ZN4dice3hfe5world13objectManagerE. Defined (null) in Core/Globals.cpp and
// filled by the module registration pass. objectTemplateManager is ds:0x8B1B924, one pointer away.
extern ObjectManager* objectManager;

} // namespace world
} // namespace hfe
} // namespace dice

#endif // SCENE_OBJECT_OBJECTMANAGER_H
