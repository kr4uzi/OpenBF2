// Terrain-tile lifecycle and query dispatch for the heightmap cluster.

#include "HeightmapCluster.h"

#include <cmath>
#include <iostream>     // not decorative - see the file-scope statics block below
#include <string>
#include <vector>

#include "../Core/ClassManager.h"
#include "../Core/EventManager.h"
#include "../Core/Ids.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ICameraObject.h"
#include "../Game/Utility/Vars.h"
#include "../Math/Intersect.h"
#include "../Math/Vec3.h"
#include "../Scene/Object/ObjectManager.h"
#include "../Scene/Object/PlayerManager.h"
#include "CollisionStats.h"          // world::collisionStatsData - the shared 0x3C block

namespace dice {
namespace hfe {

extern ClassManager* classManager;      // ds:0x8AC85A0 = _ZN4dice3hfe12classManagerE, Core/Globals.cpp
// eventManager / frameEventManager come in from Core/EventManager.h:223.

namespace world {

// Height queries increment the shared collision-statistics counter before any early
// return. The following state holds cluster settings and the frozen-camera ray.
namespace {

// attested. `MEMORY[0x8ABAF2C] = dice::hfe::Vars::getInt("hc_freeze", 0);` at 0x8386DC3.
// Vars::getInt returns int* (Game/Utility/Vars.h:61) - it hands back the address of the console
// variable's storage, so this is a live handle, not a snapshot. "hc" is this class's initials and
// its only reader is intersectCameraRay: a debug freeze for the terrain picking ray.
int* hc_freeze = Vars::getInt("hc_freeze", 0);

// [INFERRED] source name; address and increment are recovered from the ray dispatcher.
int gVerticalRayCount = 0;

// [INFERRED] Names for the zero-initialized camera position and forward direction.
// PE 0x006E2A70 stores them while freezing is disabled; the fixed probes replace the local ray.
Vec3 gFrozenRayFrom;
Vec3 gFrozenRayDirection;

} // anonymous namespace

// Creates a cluster with one reference and returns the requested interface, releasing it on refusal.
// Rejects aggregation and does not call init again: the constructor already registers its handler.
IBase* HeightmapCluster::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)                                       // aggregation refused
        return 0;

    HeightmapCluster* self = new HeightmapCluster();      // 0x8846997, operator new(0x24u)
    void* itf = self->queryInterface(iid);                // 0x88469C2, virtual - call [edx+0Ch]
    if (itf == 0)
        self->release();                                  // 0x88469D4, virtual - call [eax+08h]
    return static_cast<IBase*>(itf);
}

// Starts with one reference and calls init for tile state.
// m_usePhongNormalInterpolation remains uninitialized.
HeightmapCluster::HeightmapCluster()
    : m_refCount(1)                                       // mov dword ptr [eax+8], 1
{
    init();                                               // direct call, not virtual
}

// HeightmapCluster::~HeightmapCluster - ELF D2 0x8384BD0 / D1 0x8384C10 (51) / D0 0x8384C50 (59).

// cleanup() then the base destructor. The D0 form adds `operator delete(this)`, which is the
// compiler's, not the author's. The call to cleanup is direct (`call 0x8384CF0`), not virtual -
// GCC devirtualises inside a destructor because the dynamic type is fixed there.
HeightmapCluster::~HeightmapCluster()
{
    cleanup();
}

// IBase quartet - ELF slots 0-3

// Returns the NEW count in eax; the interface declares void, so it is
// discarded. Matches Core/IBase.h's census.
void HeightmapCluster::addRef()
{
    ++m_refCount;
}


int HeightmapCluster::getRef()
{
    return m_refCount;
}

// Decrement, then `test edx, edx / jz` before the destructor call - that null
// test is GCC's own guard on `delete this`, not a source-level check. Returns the
// decremented count, per Core/IBase.h's 329-of-333 finding.
int HeightmapCluster::release()
{
    int count = --m_refCount;
    if (count == 0)
        delete this;                                      // virtual, call [eax+14h] = D0
    return count;
}

// two ids only, and both return the primary subobject pointer unadjusted -
// there is no offset arithmetic in the body, which is consistent with IHeightmapCluster being the
// primary base at offset 0.

// it does not answer IID_IEventListener, even though the class implements it. That is not an
// omission to be fixed: there is no such IID anywhere in Core/Ids.h or the binary, so there is no
// third id it could have answered. The same reasoning Core/MemoryPool.h records for IAllocator.
void* HeightmapCluster::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<HeightmapCluster*>(this);
    if (iid == IID_IHeightmapCluster)
        return const_cast<HeightmapCluster*>(this);
    return 0;
}

// lifecycle - ELF slots 6-8


// Registers the ECCore listener at priority zero before initializing fields.
// Preserve subscription-before-initialization order.
void HeightmapCluster::init()
{
    eventManager->registerEventHandler(ECCore, this, 0);  // EventManager vtable +0x2C

    m_activeHeightmap  = 0;
    m_primaryHeightmap = 0;
    m_clusterSize      = 0;
    m_heightmaps       = 0;
    m_heightmapSize    = 0.0f;                            // stored as the integer 0
}


// Releases tiles and array storage before unsubscribing. Leaves array and cached tile
// pointers unchanged; only destruction or immediate init may safely follow cleanup.
void HeightmapCluster::cleanup()
{
    for (int i = 0; i < m_clusterSize * m_clusterSize; ++i)
    {
        if (m_heightmaps[i] != 0)
            m_heightmaps[i]->release();                   // virtual, call [eax+08h]
    }

    if (m_heightmaps != 0)
        delete[] m_heightmaps;

    eventManager->unregisterEventHandler(ECCore, this);   // EventManager vtable +0x30
}

// Both calls are virtual here (`call [eax+1Ch]` then `call [eax+18h]`), unlike
// the destructor's direct call to cleanup - so a derived HeightmapClusterEditable::cleanup would be
// reached through reset() but not through ~HeightmapCluster. Read, not chosen.
void HeightmapCluster::reset()
{
    cleanup();                                            // vtable +0x1C
    init();                                               // vtable +0x18
}

// geometry - ELF slots 9-13

// Allocates a symmetric odd-sized sector grid, rounding even sizes upward.
// Call only once: replacing the array does not free previous tiles or storage.
void HeightmapCluster::setClusterSize(int size)
{
    m_clusterSize = size;
    if ((m_clusterSize & 1) == 0)                         // low byte of the member
        m_clusterSize = size + 1;

    // `new IHeightmap*[n*n]` followed by a zeroing memset. The alignment prologue GCC puts in front
    // of the memset is compiler output; the source is a value-initialised array.
    const int count = m_clusterSize * m_clusterSize;
    m_heightmaps = new IHeightmap*[count]();              // 0x8384DD9 operator new[], 0x8384E1B memset
}


int HeightmapCluster::getClusterSize() const
{
    return m_clusterSize;
}


// an int goes in and A float comes out. `cvtsi2ss xmm0, [ebp+arg_4]; movss [eax+1Ch], xmm0` - the
// member is genuinely a float, and getHeightmapSize truncates it back with cvttss2si. The float is
// not decoration: getHeightmapInWorldCoords, getSectorOfWorldCoords and isInsideHeightmapCluster all
// do float arithmetic with it directly, so storing it pre-converted saves a conversion on every
// terrain query. Frozen as shipped, misfeature and all.
void HeightmapCluster::setHeightmapSize(int size)
{
    m_heightmapSize = static_cast<float>(size);
}


int HeightmapCluster::getHeightmapSize() const
{
    return static_cast<int>(m_heightmapSize);             // cvttss2si - truncation, not rounding
}

// Tiles across, times metres per tile.
int HeightmapCluster::getWorldSize() const
{
    return static_cast<int>(m_heightmapSize) * m_clusterSize;
}

// Creates a tile after stripping the Editable class suffix. Requires a registered class
// and successful creation; the result is used without a null guard.
void HeightmapCluster::addHeightmap(const std::string& name, int sectorX, int sectorZ)
{
    IHeightmap** slot = &m_heightmaps[getGridIndex(sectorX, sectorZ)];

    std::string base;
    base.assign(name.substr(0, name.rfind("Editable")));                 // 0x8384EA1 rfind, 0x8384EDA assign
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // The editor image only (this TU is compiled a second time for it, Code/Tools/BF2Editor/
    // CMakeLists.txt): the retail editor's engine appends "Editable" after the strip, so every tile
    // a level's Heightdata.con adds ("Heightmap") is a HeightmapEditable (EditorEngine.dll
    // 0x105DAFE0, rfind + substr + `+= "Editable"`; its asserts and "Failed to create heightmap"
    // text are the 2006 debug build's, not 1.50's). Every game image compiles the strip alone.
    base += "Editable";
#endif

    IHeightmap* heightmap = static_cast<IHeightmap*>(
        classManager->createInstance("dice.hfe.world." + base,           // 0x8384F0F std::operator+
                                     IID_IHeightmap, 0));                // vtable +0x38

    *slot              = heightmap;
    m_activeHeightmap  = heightmap;
    if (sectorX == 0 && sectorZ == 0)
        m_primaryHeightmap = heightmap;

    // Tile origin in grid units: sector index times tile size, truncated to int by the ABI.
    heightmap->setPos(static_cast<int>(sectorX * m_heightmapSize),
                      static_cast<int>(sectorZ * m_heightmapSize));      // vtable +0x34
}


void HeightmapCluster::setActiveHeightmap(IHeightmap* heightmap)
{
    m_activeHeightmap = heightmap;
}


IHeightmap* HeightmapCluster::getActiveHeightmap() const
{
    return m_activeHeightmap;
}

// Sector (0,0)'s tile - the playable terrain.
IHeightmap* HeightmapCluster::getPrimaryHeightmap() const
{
    return m_primaryHeightmap;
}

// The `if (!result) return 0; return result;` shape in the decompilation is a
// branch merge, not a source-level redundancy.
IHeightmap* HeightmapCluster::getHeightmapInSector(int sectorX, int sectorZ) const
{
    if (!isSectorValid(sectorX, sectorZ))                 // direct call
        return 0;
    return m_heightmaps[getGridIndex(sectorX, sectorZ)];
}


// Finds a tile by linear scan through virtual getHeightmapInSector, preserving editable
// overrides. Uses output parameters as loop counters, so failure still changes them.
bool HeightmapCluster::getSectorOfHeightmap(const IHeightmap* heightmap,
                                            int& sectorX, int& sectorZ) const
{
    const int half = m_clusterSize / 2;

    for (sectorZ = -(m_clusterSize / 2); sectorZ <= half; ++sectorZ)
    {
        for (sectorX = -(m_clusterSize / 2); sectorX <= half; ++sectorX)
        {
            if (getHeightmapInSector(sectorX, sectorZ) == heightmap)     // vtable +0x48
                return true;
        }
    }
    return false;
}


// Checks cluster bounds before finding the tile by repeated subtraction.
// Its boundary rules differ from getSectorOfWorldCoords; keep the independent traversal.
IHeightmap* HeightmapCluster::getHeightmapInWorldCoords(float x, float z) const
{
    if (m_heightmaps == 0)
        return 0;

    const float half  = 0.5f * m_heightmapSize;
    const float bound = std::fabs(static_cast<float>(m_clusterSize)) * half;

    if (-bound >= x || x >= bound)
        return 0;
    if (-bound >= z || z >= bound)
        return 0;

    int sectorX = 0;
    if (x >= 0.0f)
    {
        for (float v = x; v >= half; v -= m_heightmapSize)
            ++sectorX;
    }
    else
    {
        for (float v = x; -half > v; v += m_heightmapSize)
            --sectorX;
    }

    int sectorZ = 0;
    if (z >= 0.0f)
    {
        for (float v = z; v >= half; v -= m_heightmapSize)
            ++sectorZ;
    }
    else
    {
        for (float v = z; -half > v; v += m_heightmapSize)
            --sectorZ;
    }

    return m_heightmaps[getGridIndex(sectorX, sectorZ)];
}

// The same walk without the cluster bounds check - it will happily report a
// sector outside the cluster.
void HeightmapCluster::getSectorOfWorldCoords(float x, float z, int* sectorX, int* sectorZ) const
{
    const float half = 0.5f * m_heightmapSize;

    *sectorX = 0;
    if (x >= 0.0f)
    {
        for (float v = x; v >= half; v -= m_heightmapSize)
            ++*sectorX;
    }
    else if (-half > x)
    {
        for (float v = x; -half > v; v += m_heightmapSize)
            --*sectorX;
    }

    *sectorZ = 0;
    if (z >= 0.0f)
    {
        for (float v = z; v >= half; v -= m_heightmapSize)
            ++*sectorZ;
    }
    else if (-half > z)
    {
        for (float v = z; -half > v; v += m_heightmapSize)
            --*sectorZ;
    }
}

// scale - ELF slots 22-23

// Vec3::getZero() returns a reference to a shared zero (Math/Vec3.h), which is
// what makes returning it from a const-reference getter legal.
const Vec3& HeightmapCluster::getPrimaryScale() const
{
    if (m_primaryHeightmap != 0)
        return m_primaryHeightmap->getScale();                           // vtable +0x58
    return Vec3::getZero();
}

// The scale of the first tile that is not the primary - the filler terrain
// around the playable area can be scaled differently from it. Returns zero if the cluster is one
// tile, or if every tile is the primary one.
const Vec3& HeightmapCluster::getSecondaryScale() const
{
    for (int i = 0; i < m_clusterSize * m_clusterSize; ++i)
    {
        IHeightmap* heightmap = m_heightmaps[i];
        if (heightmap != 0 && heightmap != m_primaryHeightmap)
            return heightmap->getScale();                                // vtable +0x58
    }
    return Vec3::getZero();
}

// water level - ELF slots 24-25


// no null check on the tiles, unlike setUsePhongNormalInterpolation two slots down which does
// exactly the same walk and does check. One of the pair is wrong and the binary does not say which;
// both are reproduced as written. A cluster with a hole in it faults here.
void HeightmapCluster::setSeaWaterLevel(float level)
{
    for (int i = 0; i < m_clusterSize * m_clusterSize; ++i)
        m_heightmaps[i]->setSeaWaterLevel(level);                        // vtable +0x64
}

// Non-const, matching IHeightmap::getSeaWaterLevel. The fallback is the
// literal at 0x88CF764, which is -10000.0f - "no water", a sentinel far below any terrain.
float HeightmapCluster::getSeaWaterLevel()
{
    if (m_primaryHeightmap != 0)
        return m_primaryHeightmap->getSeaWaterLevel();                   // vtable +0x68
    return -10000.0f;                                                    // flt_88CF764
}

// Cluster queries use virtual bounds checking and tile selection before forwarding
// to the selected world-coordinate query, preserving editable overrides.


// A shipped stub. The whole body is `xor eax, eax; ret` inside a 0x148-byte frame the compiler
// reserved and never used - the arguments are not touched and no tile is consulted. The cluster
// simply does not implement grid-space height lookup; callers that want it must go to a tile.
// The dead 0x148 frame is what a large body looks like after everything in it was deleted.
// Reproduced exactly. Do not "complete" it.
int HeightmapCluster::getHeightFromGrid(int /*x*/, int /*z*/) const
{
    return 0;
}


float HeightmapCluster::getHeightInWorldCoords(float x, float z) const
{
    if (isInsideHeightmapCluster(x, z) != true)                          // vtable +0xA0
        return 0.0f;
    IHeightmap* heightmap = getHeightmapInWorldCoords(x, z);             // vtable +0x50
    if (heightmap == 0)
        return 0.0f;
    return heightmap->getHeightInWorldCoords(x, z);                      // vtable +0x90
}

// Y is discarded - the vector's X and Z are the ground-plane coordinates.
// Virtual re-dispatch to the (float,float) form, vtable +0x6C.
float HeightmapCluster::getHeightInWorldCoords(const Vec3& p) const
{
    return getHeightInWorldCoords(p.x, p.z);
}

// Same shape, tile vtable +0xAC.
float HeightmapCluster::getWorldScaledHeightInWorldCoords(float x, float z) const
{
    if (isInsideHeightmapCluster(x, z) != true)
        return 0.0f;
    IHeightmap* heightmap = getHeightmapInWorldCoords(x, z);
    if (heightmap == 0)
        return 0.0f;
    return heightmap->getWorldScaledHeightInWorldCoords(x, z);           // vtable +0xAC
}


float HeightmapCluster::getWorldScaledHeightInWorldCoords(const Vec3& p) const
{
    return getWorldScaledHeightInWorldCoords(p.x, p.z);                  // vtable +0x74
}


// the fallback is +Y, not zero. Off the cluster it writes (0, 1, 0) - straight up - where the
// height getters fall back to 0.0f. A zero normal would break every dot product downstream; this is
// deliberate. The constant 1065353216 in the decompilation is 1.0f.
void HeightmapCluster::getNormalInWorldCoords(float x, float z, Vec3& normal) const
{
    if (isInsideHeightmapCluster(x, z) == true)
    {
        IHeightmap* heightmap = getHeightmapInWorldCoords(x, z);
        if (heightmap != 0)
        {
            heightmap->getNormalInWorldCoords(x, z, normal);             // vtable +0xA0
            return;
        }
    }
    normal.x = 0.0f;
    normal.y = 1.0f;
    normal.z = 0.0f;
}


void HeightmapCluster::getNormalInWorldCoords(const Vec3& p, Vec3& normal) const
{
    getNormalInWorldCoords(p.x, p.z, normal);                            // vtable +0x7C
}


// the stats counter is incremented first, before every early-out - so it counts attempts, not
// hits. See the CollisionStatsData note at the top of this file for why the type is [CHOSEN].

// Same +Y fallback as getNormalInWorldCoords, and the same 0.0f height fallback as the height
// getters, in one function.
float HeightmapCluster::getHeightAndNormalInWorldCoords(float x, float z, Vec3& normal,
                                                        unsigned char& material) const
{
    ++collisionStatsData->heightAndNormalQueries;

    if (isInsideHeightmapCluster(x, z) == true)
    {
        IHeightmap* heightmap = getHeightmapInWorldCoords(x, z);
        if (heightmap != 0)
            return heightmap->getHeightAndNormalInWorldCoords(x, z, normal, material);  // vtable +0xB8
    }

    normal.x = 0.0f;
    normal.y = 1.0f;
    normal.z = 0.0f;
    return 0.0f;
}


// no isInsideHeightmapCluster check. This one and getSquareInWorldCoords below go straight to the
// tile pick, unlike every other query in this block. getHeightmapInWorldCoords does its own bounds
// test so the behaviour is equivalent; the inconsistency is in the source, not in the recovery.
bool HeightmapCluster::getTriangleAndNormalAndMaterialInWorldCoords(
        float x, float z, Vec3& normal, Vec3& v0, Vec3& v1,
        Vec3& v2, unsigned char& material) const
{
    IHeightmap* heightmap = getHeightmapInWorldCoords(x, z);             // vtable +0x50
    if (heightmap == 0)
        return false;

    heightmap->getTriangleAndNormalAndMaterialInWorldCoords(
        x, z, normal, v0, v1, v2, material);                             // normal first, void tile query
    return true;
}


bool HeightmapCluster::getSquareInWorldCoords(float x, float z, Vec3* corners, bool& flipped) const
{
    IHeightmap* heightmap = getHeightmapInWorldCoords(x, z);             // vtable +0x50
    if (heightmap == 0)
        return false;

    heightmap->getSquareInWorldCoords(x, z, corners, flipped);           // vtable +0xC4
    return true;
}

// Uses the height/water shortcut for nearly vertical single-tile rays. Otherwise clips
// to cluster bounds and traverses tile segments in travel order, returning the nearest
// successful hit. Preserve the sign-adjusted boundary-selection expression.
bool HeightmapCluster::intersectRayInWorldCoords(const Vec3& from, const Vec3& to,
                                                 Vec3& hitPoint, float& hitFraction,
                                                 Vec3& hitNormal, unsigned char& material,
                                                 bool twoSided) const
{
    if (!m_primaryHeightmap)
        return false;

    Vec3 direction;
    direction.x = to.x - from.x;
    direction.y = to.y - from.y;
    direction.z = to.z - from.z;
    if (direction.x * direction.x + direction.z * direction.z < 0.00000001f)
    {
        IHeightmap* fromHeightmap = getHeightmapInWorldCoords(from.x, from.z);
        if (fromHeightmap == getHeightmapInWorldCoords(to.x, to.z))
        {
            ++gVerticalRayCount;
            const float terrainHeight =
                getHeightAndNormalInWorldCoords(to.x, to.z, hitNormal, material);
            const float waterHeight = getWaterLevelInWorldCoords(to.x, to.z);
            if (twoSided && waterHeight > terrainHeight && from.y >= waterHeight)
            {
                if (waterHeight >= to.y)
                {
                    hitPoint.x = to.x;
                    hitPoint.y = waterHeight;
                    hitPoint.z = to.z;
                    hitNormal.x = 0.0f;
                    hitNormal.y = 1.0f;
                    hitNormal.z = 0.0f;
                    hitFraction = to.y - waterHeight;
                    material = 1;
                    return true;
                }
            }
            else if (terrainHeight >= to.y)
            {
                hitPoint.x = to.x;
                hitPoint.y = terrainHeight;
                hitPoint.z = to.z;
                hitFraction = to.y - terrainHeight;
                return true;
            }
            return false;
        }
    }

    ++collisionStatsData->unknown34;
    const Vec3& scale = getPrimaryScale();
    const float horizontalBound =
        0.5f * m_heightmapSize * static_cast<float>(m_clusterSize) - 1.0f;
    Vec3 boxMinimum;
    boxMinimum.x = -horizontalBound;
    boxMinimum.y = 0.0f;
    boxMinimum.z = -horizontalBound;
    Vec3 boxMaximum;
    boxMaximum.x = horizontalBound;
    boxMaximum.y = 65536.0f * scale.y;
    boxMaximum.z = horizontalBound;

    float firstFraction;
    float lastFraction;
    if (!rayIntersectsAABB(from, direction, boxMinimum, boxMaximum,
                           firstFraction, lastFraction))
        return false;

    Vec3 segmentFrom;
    if (firstFraction >= 0.0f)
    {
        segmentFrom.x = from.x + direction.x * firstFraction;
        segmentFrom.y = from.y + direction.y * firstFraction;
        segmentFrom.z = from.z + direction.z * firstFraction;
    }
    else
    {
        segmentFrom = from;
    }

    Vec3 segmentTo;
    if (lastFraction <= 1.0f)
    {
        segmentTo.x = from.x + direction.x * lastFraction;
        segmentTo.y = from.y + direction.y * lastFraction;
        segmentTo.z = from.z + direction.z * lastFraction;
    }
    else
    {
        segmentTo = to;
    }

    if (!(firstFraction < 0.0f && lastFraction > 1.0f))
    {
        direction.x = segmentTo.x - segmentFrom.x;
        direction.y = segmentTo.y - segmentFrom.y;
        direction.z = segmentTo.z - segmentFrom.z;
    }

    int sectorX = 0;
    int sectorZ = 0;
    getSectorOfWorldCoords(segmentFrom.x, segmentFrom.z, &sectorX, &sectorZ);
    for (;;)
    {
        int endSectorX = 0;
        int endSectorZ = 0;
        getSectorOfWorldCoords(segmentTo.x, segmentTo.z, &endSectorX, &endSectorZ);
        IHeightmap* heightmap = getHeightmapInSector(sectorX, sectorZ);
        if (sectorX == endSectorX && sectorZ == endSectorZ)
            return heightmap->intersectRayInWorldCoords(segmentFrom, segmentTo, hitPoint,
                                                        hitFraction, hitNormal, material, twoSided);
        if (!heightmap)
            return false;

        const float boundaryX = direction.x <= 0.0f
                              ? heightmap->getLowBoundaryX()
                              : heightmap->getHighBoundaryX();
        const float boundaryZ = direction.z <= 0.0f
                              ? heightmap->getLowBoundaryZ()
                              : heightmap->getHighBoundaryZ();
        const float distanceX = boundaryX - segmentFrom.x;
        const float distanceZ = boundaryZ - segmentFrom.z;
        float boundaryOrder = direction.x * distanceZ - direction.z * distanceX;
        if ((distanceZ < 0.0f) != (distanceX < 0.0f))
            boundaryOrder = -boundaryOrder;

        Vec3 next;
        if (boundaryOrder <= 0.0f)
        {
            const float fraction = distanceZ / direction.z;
            next.x = segmentFrom.x + direction.x * fraction;
            next.y = segmentFrom.y + direction.y * fraction;
            next.z = boundaryZ;
            sectorZ += direction.z <= 0.0f ? -1 : 1;
        }
        else
        {
            const float fraction = distanceX / direction.x;
            next.x = boundaryX;
            next.y = segmentFrom.y + direction.y * fraction;
            next.z = segmentFrom.z + direction.z * fraction;
            sectorX += direction.x <= 0.0f ? -1 : 1;
        }

        direction.x = next.x - segmentFrom.x;
        direction.y = next.y - segmentFrom.y;
        direction.z = next.z - segmentFrom.z;
        if (heightmap->intersectRayInWorldCoords(segmentFrom, next, hitPoint, hitFraction,
                                                 hitNormal, material, twoSided))
            return true;
        segmentFrom = next;
    }
}


unsigned char HeightmapCluster::getMaterialInWorldCoords(float x, float z) const
{
    if (isInsideHeightmapCluster(x, z) == true)
    {
        IHeightmap* heightmap = getHeightmapInWorldCoords(x, z);
        if (heightmap != 0)
            return heightmap->getMaterialInWorldCoords(x, z);            // vtable +0x80
    }
    return 0;
}

// Water level is a per-cell grid query, not a mesh intersection - see
// Physics/Heightmap.cpp on water being baked into the heightmap.
float HeightmapCluster::getWaterLevelInWorldCoords(float x, float z) const
{
    if (isInsideHeightmapCluster(x, z) != true)
        return 0.0f;
    IHeightmap* heightmap = getHeightmapInWorldCoords(x, z);
    if (heightmap == 0)
        return 0.0f;
    return heightmap->getWaterLevelInWorldCoords(x, z);                  // vtable +0xDC
}


float HeightmapCluster::getWaterLevelInWorldCoords(const Vec3& p) const
{
    return getWaterLevelInWorldCoords(p.x, p.z);                         // vtable +0x98
}


// Tests inclusive square bounds using absolute X/Z. Exact-edge points pass here but
// fail getHeightmapInWorldCoords' strict boundary check; retain that distinction.
bool HeightmapCluster::isInsideHeightmapCluster(float x, float z) const
{
    const float half = 0.5f * m_heightmapSize * std::fabs(static_cast<float>(m_clusterSize));

    if (half < std::fabs(x))
        return false;
    if (half < std::fabs(z))
        return false;
    return true;
}

// PE 0x006E2A70; ELF 0x08386110. The console discards the result (PE 0x006DF574).
// Retail retains the camera/freeze lookup but tests these 34 fixed rays instead of that ray.
void HeightmapCluster::intersectCameraRay()
{
    Vec3 from{0.0f, 0.0f, 0.0f};
    Vec3 direction{0.0f, 0.0f, 0.0f};
    if (!*hc_freeze)
    {
        PlayerManager* const manager =
            reinterpret_cast<PlayerManager*>(classManager->getSingleton("PlayerManager"));
        if (manager)
        {
            IPlayer* const player = manager->getLocalHumanPlayer();
            if (player)
            {
                ICameraObject* const camera = player->getCamera();
                if (camera)
                {
                    const Mat4& transform = camera->getTransformation(0.0f);
                    from = Vec3{transform.m[12], transform.m[13], transform.m[14]};
                    direction = Vec3{transform.m[8], transform.m[9], transform.m[10]};
                    gFrozenRayFrom = from;
                    gFrozenRayDirection = direction;
                }
            }
        }
    }
    else
    {
        from = gFrozenRayFrom;
        direction = gFrozenRayDirection;
    }
    const float length = *Vars::getFloat("coll-ray-len", 10.0f);
    Vec3 to{from.x + direction.x * length, from.y + direction.y * length,
            from.z + direction.z * length};
    Vec3 hitPoint{0.0f, 0.0f, 0.0f};
    Vec3 normal{0.0f, 0.0f, 0.0f};
    const Vec3 starts[] = {
        Vec3{749.239f, 61.0f, -491.325f}, Vec3{749.239f, 61.0000114f, -491.325f},
        Vec3{10, 45, 11}, Vec3{10, 45, 11}, Vec3{10, 45, 11}, Vec3{10, 45, 11},
        Vec3{10, 45, 11}, Vec3{10, 45, 11}, Vec3{10, 45, 11}, Vec3{10, 45, 11},
        Vec3{11, 45, 10}, Vec3{11, 45, 10}, Vec3{11, 45, 10}, Vec3{11, 45, 10},
        Vec3{11, 45, 10}, Vec3{11, 45, 10}, Vec3{11, 45, 10}, Vec3{11, 45, 10},
        Vec3{11, 1, 11}, Vec3{11, 1, 13}, Vec3{10, 1, 13}, Vec3{9, 1, 13},
        Vec3{9, 1, 11}, Vec3{9, 1, 9}, Vec3{10, 1, 9}, Vec3{11, 1, 9},
        Vec3{13, 1, 10}, Vec3{13, 1, 11}, Vec3{11, 1, 11}, Vec3{9, 1, 11},
        Vec3{9, 1, 10}, Vec3{9, 1, 9}, Vec3{11, 1, 9}, Vec3{13, 1, 9}
    };
    const Vec3 ends[] = {
        Vec3{747.994f, 61.0f, -492.89f}, Vec3{747.994f, 61.0f, -492.89f},
        Vec3{11, 1, 11}, Vec3{11, 1, 13}, Vec3{10, 1, 13}, Vec3{9, 1, 13},
        Vec3{9, 1, 11}, Vec3{9, 1, 9}, Vec3{10, 1, 9}, Vec3{11, 1, 9},
        Vec3{13, 1, 10}, Vec3{13, 1, 11}, Vec3{11, 1, 11}, Vec3{9, 1, 11},
        Vec3{9, 1, 10}, Vec3{9, 1, 9}, Vec3{11, 1, 9}, Vec3{13, 1, 9},
        Vec3{10, 45, 11}, Vec3{10, 45, 11}, Vec3{10, 45, 11}, Vec3{10, 45, 11},
        Vec3{10, 45, 11}, Vec3{10, 45, 11}, Vec3{10, 45, 11}, Vec3{10, 45, 11},
        Vec3{11, 45, 10}, Vec3{11, 45, 10}, Vec3{11, 45, 10}, Vec3{11, 45, 10},
        Vec3{11, 45, 10}, Vec3{11, 45, 10}, Vec3{11, 45, 10}, Vec3{11, 45, 10}
    };
    float fraction;
    unsigned char material;
    for (unsigned int i = 0; i < 34; ++i)
    {
        from = starts[i];
        to = ends[i];
        intersectRayInWorldCoords(from, to, hitPoint, fraction, normal, material, true);
    }
}

// Updates the object grid, bakes intersecting water meshes into each tile, then processes consumed objects.
// Requires every sector to contain a heightmap; tile results are not null-checked.
void HeightmapCluster::compileWaterMeshesIntoHeightmap()
{
    std::vector<IObject*> waterObjects;                                  // three zeroed words

    objectManager->updateObjectsInGrid();                                // vtable +0xA4

    const int n = getClusterSize();                                      // vtable +0x28
    for (int z = -(n / 2); z <= n / 2; ++z)
    {
        for (int x = -(n / 2); x <= n / 2; ++x)
        {
            getHeightmapInSector(x, z)                                   // vtable +0x48
                ->compileWaterMeshesIntoHeightmap(waterObjects);         // vtable +0xE8
        }
    }

    for (size_t i = 0; i < waterObjects.size(); ++i)
    {
        // [UNRESOLVED] Missing absorbed-object state update with arguments (0,256).
        // Identify and implement the correct IObject dispatch before treating this loop as complete.
        (void)waterObjects[i];                                           // see above
    }
}

// Caches the flag, then pushes it into every tile. this one null-checks the
// tiles; setSeaWaterLevel, which walks the identical loop, does not. Both reproduced as written.
void HeightmapCluster::setUsePhongNormalInterpolation(bool on)
{
    m_usePhongNormalInterpolation = on;

    for (int i = 0; i < m_clusterSize * m_clusterSize; ++i)
    {
        IHeightmap* heightmap = m_heightmaps[i];
        if (heightmap != 0)
            heightmap->setUsePhongNormalInterpolation(on);               // vtable +0x5C
    }
}


// reads an uninitialised member. m_usePhongNormalInterpolation is written by nothing but
// setUsePhongNormalInterpolation - not by the constructor, not by init(). See the header block for
// the byte-level argument. Until the setter is called this returns whatever the allocator left at
// +0x20. Preserve this shipped uninitialized read.
bool HeightmapCluster::getUsePhongNormalInterpolation() const
{
    return m_usePhongNormalInterpolation;
}

// IEventListener - ELF slot 45 (primary table), thunk at secondary +0x00 (0x8386E30, 10 bytes,
// `add [esp+arg_0], -4; jmp`)

// On core resources-loaded event 7, invokes the virtual water-mesh bake.
void HeightmapCluster::handleEvent(EventCategory category, unsigned int id,
                                   EventNode* /*node*/, void* /*userData*/)
{
    if (category != ECCore)
        return;
    if (id != 7 /* CEResourcesLoaded - Core/Module.cpp:260 */)
        return;

    compileWaterMeshesIntoHeightmap();                                   // vtable +0xA8
}

// NON-virtual members - present in the binary, reached only by direct calls


// signed sector coordinates into A linear index. The `shr edx,31 / add / sar 1` sequence is the
// compiler's division-by-two-rounding-toward-zero idiom; the source is n/2.

// it does not bounds-check. getHeightmapInSector calls isSectorValid first; addHeightmap and
// getHeightmapInWorldCoords call this one directly and do not.
int HeightmapCluster::getGridIndex(int sectorX, int sectorZ) const
{
    const int half = m_clusterSize / 2;
    return (sectorX + half) + (sectorZ + half) * m_clusterSize;
}

// The inclusive range that the odd-size coercion in setClusterSize exists to
// make symmetric.
bool HeightmapCluster::isSectorValid(int sectorX, int sectorZ) const
{
    const int half = m_clusterSize / 2;
    if (sectorX < -(m_clusterSize / 2)) return false;
    if (sectorX > half)                 return false;
    if (sectorZ < -(m_clusterSize / 2)) return false;
    if (sectorZ > half)                 return false;
    return true;
}

// Clips to in place against the selected axis-aligned plane; the ray must not be parallel to it.
// Returns a pointer to the last component written. That float* return spelling is [CHOSEN]; callers discard it.
float* HeightmapCluster::clipRay(const Vec3& from, Vec3& to, float plane, int axis) const
{
    float*       out = &to.x;
    const float* in  = &from.x;

    const int next = (axis + 1) % 3;
    const int prev = (axis + 2) % 3;

    const float base  = in[axis];
    const float delta = out[axis] - base;
    out[axis] = plane;
    const float t = (plane - base) / delta;

    out[next] = in[next] + (out[next] - in[next]) * t;
    out[prev] = in[prev] + (out[prev] - in[prev]) * t;
    return &out[prev];
}

} // namespace world
} // namespace hfe
} // namespace dice
