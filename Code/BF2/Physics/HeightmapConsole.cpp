// Terrain console bindings; file placement is [CHOSEN].
// addHeightmap selects the tile used by later heightmap commands. setSize and setBitResolution must
// precede loadHeightData because they determine its input byte count.
#include "../Core/ConsoleObjectMacro.h"
#include "HeightmapCluster.h"
#include "HeightmapClusterEditable.h"
#include "Heightmap.h"
#include "HeightmapEditable.h"
#include "../Core/ClassManager.h"
#include "../Core/Ids.h"
#include <string>

namespace dice {
namespace hfe {

extern ClassManager* classManager;               // ds:0x8AC85A0, Core/Globals.cpp

namespace world {

extern IBase* heightmapCluster;                  // ds:0x8ABAF24, Core/Globals.cpp

// Returns the editable cluster interface, or null when the singleton is absent or plain.
IHeightmapClusterEditable* getHeightmapClusterEditable()
{
    if (!heightmapCluster)
        return 0;
    return static_cast<IHeightmapClusterEditable*>(
        heightmapCluster->queryInterface(IID_IHeightmapClusterEditable));
}

//   the active tile's IHeightmapEditable - getActiveHeightmap is called twice, as
//   shipped (once for the null test, once for the query). No caller in the ELF.
IHeightmapEditable* getActiveHeightmapEditable()
{
    if (heightmapCluster
        && reinterpret_cast<IHeightmapCluster*>(heightmapCluster)->getActiveHeightmap())
    {
        return static_cast<IHeightmapEditable*>(
            reinterpret_cast<IHeightmapCluster*>(heightmapCluster)->getActiveHeightmap()
                ->queryInterface(IID_IHeightmapEditable));
    }
    return 0;
}

namespace {

const char* const kHeightmapConsoleTag = "world.physics.heightmap";

inline IHeightmapCluster* cluster()
{
    return reinterpret_cast<IHeightmapCluster*>(heightmapCluster);
}
inline IHeightmap* activeHeightmap()
{
    return cluster()->getActiveHeightmap();
}

// PE 0x006DF574; ELF 0x088494E0. Zero arguments, no returned console value.
void consoleClusterIntersectCameraRay() { cluster()->intersectCameraRay(); }

// Resolves the requested cluster class and re-registers its singleton before creation.
// Game builds strip an Editable suffix; editor builds resolve the editable class. Unknown
// names do nothing, including the stock "HeighmapCluster" spelling.
void consoleClusterCreate(const std::string& className)
{
    std::string base = className.substr(0, className.rfind("Editable"));
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // The editor image only (this TU is compiled a second time for it, Code/Tools/BF2Editor/
    // CMakeLists.txt); every game image compiles the shipped strip alone.
    base += "Editable";
#endif
    unsigned int cid = 0;
    if (classManager->getClassID("dice.hfe.world." + base, cid))
    {
        // flags 2 = "create immediately inside registerSingleton" (ClassManager.h) - the .con
        // line instantiates the cluster on the spot, which is how the level picks its class.
        SingletonRegInfo info{"HeightmapCluster", &heightmapCluster,
                              IID_IHeightmapCluster, cid, 2};
        classManager->registerSingleton(info);
    }
}

void consoleClusterAddHeightmap(const std::string& name, int sectorX, int sectorZ)
{
    cluster()->addHeightmap(name, sectorX, sectorZ);
}

void consoleClusterSetClusterSize(int size)   { cluster()->setClusterSize(size); }
void consoleClusterSetHeightmapSize(int size) { cluster()->setHeightmapSize(size); }
// ConsoleClass151, ELF 0x8849850 / PE 0x6932D0: the authored sea level is passed directly
// to IHeightmapCluster::setSeaWaterLevel (ELF +0x60 / PE +0x5C).
// Without this binding stock Heightdata.con leaves every tile at the -10000 no-sea sentinel,
// disabling FloatingBundle lift and the submerged-propeller arm of the ship engine.
void consoleClusterSetSeaWaterLevel(float level) { cluster()->setSeaWaterLevel(level); }

// Editable-cluster console commands require an editable instance and discard its result.
// Import, smoothing, and stitching registrations remain non-dynamic.
void consoleClusterImportSurroundingRawHeightMap(const std::string& fileName)
{
    getHeightmapClusterEditable()->importSurroundingRawHeightMap(fileName);
}
void consoleClusterSmoothEdges(int width, float amplitude, float falloff)
{
    getHeightmapClusterEditable()->smoothEdges(width, amplitude, falloff);
}
void consoleClusterStitchEdges()
{
    getHeightmapClusterEditable()->stitchEdges();
}

// importSurroundingRawHeightMap's generated class, written out: its constructor declares "bool" as
// the return type (ELF 0x884538C) where every sibling says "void", yet the executor stores no value
// (m_hasReturnValue false, the call's answer dropped) - a shape no family macro carries. Otherwise
// the family 1a / static layout: argc exactly 1, the raw string slot.
class ConsoleClassImportSurroundingRawHeightMap : public ::dice::hfe::ConsoleObjectBaseImpl
{
public:
    ConsoleClassImportSurroundingRawHeightMap()
    {
        m_objectName     = ::dice::hfe::consoleDefaultName();
        m_access         = ::dice::hfe::consoleDefaultAccess();
        m_category       = ::dice::hfe::consoleDefaultCategory();
        m_functionName   = "importSurroundingRawHeightMap";
        m_type           = 0;
        m_minNoArgs      = 1;
        m_maxNoArgs      = 1;
        m_returnType     = "bool";
        m_hasReturnValue = false;
        m_isDynamic      = false;
        m_argTypes[0]    = "std::string";
        m_argNeedsConversion[0] = 0;
        ::dice::hfe::consoleBindingList(kHeightmapConsoleTag).push_back(this);
    }
    virtual bool isObjectActive() const { return getHeightmapClusterEditable() != 0; }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 1) return;
        m_hasReturnValue = false;
        consoleClusterImportSurroundingRawHeightMap(BF2_CC_ARG(std::string, 0));
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(std::string, 1)
    }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};

void consoleHmSetSize(int x, int z)             { activeHeightmap()->setSize(x, z); }
void consoleHmSetBitResolution(int bits)        { activeHeightmap()->setBitResolution(bits); }
void consoleHmSetScale(Vec3 scale)              { activeHeightmap()->setScale(scale); }
void consoleHmSetMaterialScale(int scale)       { activeHeightmap()->setMaterialScale(scale); }
void consoleHmSetModified(bool modified)        { activeHeightmap()->setModified(modified); }
void consoleHmLoadHeightData(const std::string& name)   { activeHeightmap()->loadHeightData(name); }
void consoleHmLoadMaterialData(const std::string& name) { activeHeightmap()->loadMaterialData(name); }

} // namespace

// The registered object names, as the ELF's strings spell them ("HeightmapCluster", "HeightMap");
// console dispatch is case-insensitive, so content's lowercase lines match.
BF2_CONSOLE_OBJECT("HeightmapCluster", 2, 0)
BF2_CONSOLE_METHOD_1(create, consoleClusterCreate, std::string, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_3(addHeightmap, consoleClusterAddHeightmap, heightmapCluster != 0,
                     std::string, int, int, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_1(setClusterSize, consoleClusterSetClusterSize, int, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_1(setHeightmapSize, consoleClusterSetHeightmapSize, int, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_1(setSeaWaterLevel, consoleClusterSetSeaWaterLevel, float, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(intersectCameraRay, consoleClusterIntersectCameraRay,
                                   heightmapCluster != 0, kHeightmapConsoleTag)
namespace {
ConsoleClassImportSurroundingRawHeightMap s_consoleClassImportSurroundingRawHeightMap;
}
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(smoothEdges, consoleClusterSmoothEdges,
                                   getHeightmapClusterEditable() != 0, int, float, float,
                                   kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(stitchEdges, consoleClusterStitchEdges,
                                   getHeightmapClusterEditable() != 0, kHeightmapConsoleTag)

BF2_CONSOLE_OBJECT("HeightMap", 2, 0)
BF2_CONSOLE_METHOD_2(setSize, consoleHmSetSize, int, int, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_1(setBitResolution, consoleHmSetBitResolution, int, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_1(setScale, consoleHmSetScale, Vec3, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_1(setMaterialScale, consoleHmSetMaterialScale, int, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_1(setModified, consoleHmSetModified, bool, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_1(loadHeightData, consoleHmLoadHeightData, std::string, kHeightmapConsoleTag)
BF2_CONSOLE_METHOD_1(loadMaterialData, consoleHmLoadMaterialData, std::string, kHeightmapConsoleTag)

// Handed to the console registry by PhysicsModule::init (Core/ModuleRegistry.cpp).
ConsoleObjectList& heightmapConsoleObjects()
{
    return consoleBindingList(kHeightmapConsoleTag);
}

} // namespace world
} // namespace hfe
} // namespace dice
