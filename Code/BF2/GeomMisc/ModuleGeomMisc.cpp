// Registers geometry utility classes. [INFERRED] Source placement.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginModules.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../Geom/GeometryTemplateManager.h"
#include "../Scene/Object/ObjectManager.h"
#include "FSQuadDrawer.h"
#include "DebugRenderer.h"
#include "DebugCircleDrawer.h"
#include "DebugLineDrawer.h"
#include <type_traits>

namespace dice {
namespace hfe {
extern IBase* objectManager; // Renderer-local host singleton, PluginEntry.cpp.
namespace geom {

// DebugTextWriter.cpp. Declared rather than pulled through a header - four symbols.
IBase* __fastcall createDebugTextWriter(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugTextWriterManager(unsigned int iid, IBase* outer);
extern const unsigned int IID_IDebugTextWriter;
extern const unsigned int CID_DebugTextWriter;

// FontSystem/FontSystem.cpp.
IBase* __fastcall createFontPorted(unsigned int iid, IBase* outer);
IBase* __fastcall createEditorFont(unsigned int iid, IBase* outer);
extern const unsigned int CID_EditorFont;
extern const unsigned int IID_IFontPorted;
extern const unsigned int CID_FontPorted;

// OverlayTexture.cpp.
IBase* __fastcall createOverlayTexture(unsigned int iid, IBase* outer);
extern const unsigned int IID_IOverlayTexture;
extern const unsigned int CID_OverlayTexture;

// Complete debug geometry owners.
extern const unsigned int CID_DebugSphereMesh;
extern const unsigned int CID_DebugBoxMesh;
extern const unsigned int CID_DebugPlaneMesh;
extern const unsigned int CID_DebugCylinderMesh;
extern const unsigned int CID_DebugPolygonMesh;
extern const unsigned int CID_DebugTorusMesh;
extern const unsigned int CID_DebugXMesh;
IBase* __fastcall createDebugSphereMesh(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugBoxMesh(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugPlaneMesh(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugCylinderMesh(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugPolygonMesh(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugTorusMesh(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugXMesh(unsigned int iid, IBase* outer);
extern const unsigned int CID_DebugRenderer;
extern const unsigned int CID_DebugTextDrawer;

// DebugLineGraphD3d.cpp
extern const unsigned int CID_DebugLineGraph;
IBase* __fastcall createDebugLineGraph(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugBlockGraph(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugGraphRenderer(unsigned int iid, IBase* outer);   // DebugBlockGraphD3D.cpp
IBase* __fastcall createLoadingScreen(unsigned int iid, IBase* outer);

// MenuRend/MemeTexture.cpp
extern const unsigned int CID_MemeTexture;
IBase* __fastcall createMemeTexture(unsigned int iid, IBase* outer);
extern const unsigned int CID_FSMoviePlayer;
IBase* __fastcall createFSMoviePlayer(unsigned int iid, IBase* outer);
extern const unsigned int CID_QuadDrawer;
IBase* __fastcall createQuadDrawer(unsigned int iid, IBase* outer);
IBase* __fastcall createQuadOutputCache(unsigned int iid, IBase* outer);

// EditorSpline.cpp / PerlinNoise.cpp - the editor's spline (81033) and the road-edge noise
// (81035); created only through createInstance.
extern const unsigned int CID_EditorSpline;
IBase* __fastcall createEditorSpline(unsigned int iid, IBase* outer);
extern const unsigned int CID_PerlinNoise;
IBase* __fastcall createPerlinNoise(unsigned int iid, IBase* outer);

// FontSystem/SwiffFont.cpp - the Flash font stack (RW-80). Both are getSingletonList entries below
// (FontSystem cid 81044, FontRenderer cid 81047); registering the classes here is what makes those
// two singletons resolvable, so SwiffRender::createFont can build a real font wrapper.
extern const unsigned int CID_FontSystem;      // 81044
IBase* __fastcall createFontSystem(unsigned int iid, IBase* outer);
extern const unsigned int CID_FontRenderer;    // 81047
IBase* __fastcall createFontRenderer(unsigned int iid, IBase* outer);


// One shared pointer per singleton registration.
IBase* g_QuadDrawer             = 0;
IBase* g_DebugRenderer          = 0;
IBase* g_DebugLineDrawer        = 0;
IBase* g_DebugCircleDrawer      = 0;
IBase* g_DebugTextDrawer        = 0;
IBase* g_DebugLineGraphRenderer = 0;
IBase* g_DebugTextWriter        = 0;
IBase* g_DebugTextWriterFixed   = 0;
IBase* g_DebugTextWriterManager = 0;
IBase* g_FontSystem             = 0;
IBase* g_FontRenderer           = 0;

RD9_MODULE_IBASE_BODIES(ModuleGeomMisc)

namespace {

// PE 0x10248B18; constructor order is PE 0x101CEFD0..0x101CF425.
const char kGeomMiscConsoleTag[] = "geom.misc";

// PE 0x1007D0F0 / 0x1007D150 / 0x1007D1B0 / 0x1007D210 /
// 0x1007D270 / 0x1007D2D0 / 0x1007D330, inlined into the bindings below.
// The second getActive call and the borrowed query result are deliberate.
IBase* activeDebugTemplate(unsigned int iid)
{
    IGeometryTemplateManager* manager =
        static_cast<IGeometryTemplateManager*>(g_GeometryTemplateManager);
    if (!manager || !manager->getActive()) return 0;
    manager = static_cast<IGeometryTemplateManager*>(g_GeometryTemplateManager);
    return static_cast<IBase*>(reinterpret_cast<IBase*>(manager->getActive())->queryInterface(iid));
}

// PE 0x1007D0C0 / 0x10082B60. Only the sphere instance colour is bound.
IBase* activeDebugSphere()
{
    world::ObjectManager* manager = reinterpret_cast<world::ObjectManager*>(::dice::hfe::objectManager);
    if (!manager) return 0;
    world::IObject* object = manager->getActiveObject();
    return object ? object->queryComponent(0x492FE0FEu, 80003u) : 0;
}

// [INFERRED] Local spelling of the attested BaseVector2<uint> console type.
// Sphere and Plane return a reference to two unsigned words in this order.
struct MeshConsoleTessellation { unsigned int x, y; };
static_assert(sizeof(MeshConsoleTessellation) == 8, "debug tessellation console value");

// [CHOSEN, not recovered] Named views of the owners' explicit PE-order tables.
// Unused prefix entries are opaque, with no invented callable signatures.
// Native table fields use native pointer widths; no object offsets are copied.
template<class T> struct MeshConsolePropertySlots
{
    typedef typename std::conditional<std::is_arithmetic<T>::value, T, const T&>::type Value;
    void (__thiscall* set)(void*, Value);
    Value (__thiscall* get)(void*);
};

// PE 0x1020B870 / 0x1020BBA0: the Sphere and Plane template tails agree.
struct TexturedMeshConsoleSlots
{
    void* prefix[44];
    MeshConsolePropertySlots<std::string> shader;
    MeshConsolePropertySlots<std::string> texture;
    MeshConsolePropertySlots<Vec3> colour;
    MeshConsolePropertySlots<MeshConsoleTessellation> tessellation;
};
// PE 0x1020A928.
struct BoxMeshConsoleSlots
{
    void* prefix[44];
    MeshConsolePropertySlots<std::string> shader;
    MeshConsolePropertySlots<Vec3> colour;
    MeshConsolePropertySlots<float> width, height, depth;
};
// PE 0x1020BE78.
struct CylinderMeshConsoleSlots
{
    void* prefix[44];
    MeshConsolePropertySlots<std::string> shader;
    MeshConsolePropertySlots<Vec3> colour;
    MeshConsolePropertySlots<float> length, radius1, radius2;
    MeshConsolePropertySlots<unsigned int> slices, stacks;
};
// PE 0x1020C0E8.
struct PolygonMeshConsoleSlots
{
    void* prefix[44];
    MeshConsolePropertySlots<std::string> shader;
    MeshConsolePropertySlots<Vec3> colour;
    MeshConsolePropertySlots<float> length;
    MeshConsolePropertySlots<unsigned int> sides;
};
// PE 0x1020C340.
struct TorusMeshConsoleSlots
{
    void* prefix[44];
    MeshConsolePropertySlots<std::string> shader;
    MeshConsolePropertySlots<Vec3> colour;
    MeshConsolePropertySlots<float> innerRadius, outerRadius;
    MeshConsolePropertySlots<unsigned int> sides, rings;
};
// PE 0x1020C5C0.
struct XMeshConsoleSlots
{
    void* prefix[44];
    MeshConsolePropertySlots<std::string> shader;
    MeshConsolePropertySlots<Vec3> colour;
};
// PE 0x1020B768: Sphere instance colour is Vec4, unlike its template colour.
struct SphereInstanceConsoleSlots
{
    void* prefix[62];
    MeshConsolePropertySlots<Vec4> colour;
};

#define GEOMMISC_CHECK_SLOTS(Table, member, slot) \
    static_assert(offsetof(Table, member) == (slot) * sizeof(void*), "debug console table " #member)
GEOMMISC_CHECK_SLOTS(TexturedMeshConsoleSlots, shader, 44);
GEOMMISC_CHECK_SLOTS(TexturedMeshConsoleSlots, texture, 46);
GEOMMISC_CHECK_SLOTS(TexturedMeshConsoleSlots, colour, 48);
GEOMMISC_CHECK_SLOTS(TexturedMeshConsoleSlots, tessellation, 50);
GEOMMISC_CHECK_SLOTS(BoxMeshConsoleSlots, shader, 44);
GEOMMISC_CHECK_SLOTS(BoxMeshConsoleSlots, colour, 46);
GEOMMISC_CHECK_SLOTS(BoxMeshConsoleSlots, width, 48);
GEOMMISC_CHECK_SLOTS(BoxMeshConsoleSlots, height, 50);
GEOMMISC_CHECK_SLOTS(BoxMeshConsoleSlots, depth, 52);
GEOMMISC_CHECK_SLOTS(CylinderMeshConsoleSlots, shader, 44);
GEOMMISC_CHECK_SLOTS(CylinderMeshConsoleSlots, colour, 46);
GEOMMISC_CHECK_SLOTS(CylinderMeshConsoleSlots, length, 48);
GEOMMISC_CHECK_SLOTS(CylinderMeshConsoleSlots, radius1, 50);
GEOMMISC_CHECK_SLOTS(CylinderMeshConsoleSlots, radius2, 52);
GEOMMISC_CHECK_SLOTS(CylinderMeshConsoleSlots, slices, 54);
GEOMMISC_CHECK_SLOTS(CylinderMeshConsoleSlots, stacks, 56);
GEOMMISC_CHECK_SLOTS(PolygonMeshConsoleSlots, shader, 44);
GEOMMISC_CHECK_SLOTS(PolygonMeshConsoleSlots, colour, 46);
GEOMMISC_CHECK_SLOTS(PolygonMeshConsoleSlots, length, 48);
GEOMMISC_CHECK_SLOTS(PolygonMeshConsoleSlots, sides, 50);
GEOMMISC_CHECK_SLOTS(TorusMeshConsoleSlots, shader, 44);
GEOMMISC_CHECK_SLOTS(TorusMeshConsoleSlots, colour, 46);
GEOMMISC_CHECK_SLOTS(TorusMeshConsoleSlots, innerRadius, 48);
GEOMMISC_CHECK_SLOTS(TorusMeshConsoleSlots, outerRadius, 50);
GEOMMISC_CHECK_SLOTS(TorusMeshConsoleSlots, sides, 52);
GEOMMISC_CHECK_SLOTS(TorusMeshConsoleSlots, rings, 54);
GEOMMISC_CHECK_SLOTS(XMeshConsoleSlots, shader, 44);
GEOMMISC_CHECK_SLOTS(XMeshConsoleSlots, colour, 46);
GEOMMISC_CHECK_SLOTS(SphereInstanceConsoleSlots, colour, 62);
#undef GEOMMISC_CHECK_SLOTS

template<class Table> const Table& debugMeshConsoleSlots(void* receiver)
{
    return **reinterpret_cast<const Table* const*>(receiver);
}

template<class Table, class T> typename MeshConsolePropertySlots<T>::Value
getDebugMeshConsoleValue(void* receiver,
    MeshConsolePropertySlots<T> Table::* member)
{
    return (debugMeshConsoleSlots<Table>(receiver).*member).get(receiver);
}

template<class Table, class T> void setDebugMeshConsoleValue(void* receiver,
    MeshConsolePropertySlots<T> Table::* member, const T& value)
{
    (debugMeshConsoleSlots<Table>(receiver).*member).set(receiver, value);
}

// PE 0x1007E990; inlined at 0x1007FEB6. The separator need not be '/'.
std::istream& operator>>(std::istream& in, MeshConsoleTessellation& value)
{
    unsigned int component;
    char separator;
    in >> component >> separator;
    value.x = component;
    in >> component;
    value.y = component;
    return in;
}

// PE 0x1007E900; inlined at 0x1007FE12.
std::ostream& operator<<(std::ostream& out, const MeshConsoleTessellation& value)
{
    out << value.x << '/';
    out << value.y;
    return out;
}

// PE 0x10055CA0. One temporary is reused; a failed later extraction can
// repeat the preceding component. Do not replace this with the host Vec3 parser.
void readDebugMeshConsoleValue(std::istream& in, Vec3& value)
{
    float component;
    char separator;
    in >> component >> separator;
    value.x = component;
    in >> component >> separator;
    value.y = component;
    in >> component;
    value.z = component;
}

// PE 0x100585A0, called by the Vec4 binding's PE 0x10082BB0 converter.
void readDebugMeshConsoleValue(std::istream& in, Vec4& value)
{
    float component;
    char separator;
    in >> component >> separator;
    value.x = component;
    in >> component >> separator;
    value.y = component;
    in >> component >> separator;
    value.z = component;
    in >> component;
    value.w = component;
}

template<class T> void readDebugMeshConsoleValue(std::istream& in, T& value) { in >> value; }

// [CHOSEN, not recovered] Local generator spelling. Each row reproduces its
// PE constructor, converter, executor and formatting hooks; metadata type text
// is explicit because BaseVector2<uint> has a native local representation.
#define GEOMMISC_CONSOLE_PROPERTY(nameText, T, typeText, receiverExpr, Table, member)       \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                          \
{                                                                                        \
public:                                                                                  \
    BF2_CC_NAME()                                                                        \
    {                                                                                    \
        m_objectName = consoleDefaultName();                                              \
        m_functionName = (nameText);                                                      \
        m_type = 1;                                                                      \
        m_access = consoleDefaultAccess();                                                \
        m_category = consoleDefaultCategory();                                            \
        m_minNoArgs = 0;                                                                  \
        m_maxNoArgs = 1;                                                                  \
        m_returnType = (typeText);                                                        \
        m_hasReturnValue = false;                                                        \
        m_isDynamic = true;                                                              \
        consoleBindingList(kGeomMiscConsoleTag).push_back(this);                          \
        m_argTypes[0] = (typeText);                                                       \
        m_argNeedsConversion[0] = std::string(typeText) == "std::string" ? 0 : -1;         \
    }                                                                                    \
    virtual bool isObjectActive() const { return (receiverExpr) != 0; }                   \
    virtual void executeObjectMethod()                                                   \
    {                                                                                    \
        if (m_argCount == 0)                                                              \
        {                                                                                \
            m_hasReturnValue = true;                                                     \
            m_returnValue = getDebugMeshConsoleValue((receiverExpr), &Table::member);     \
        }                                                                                \
        else if (m_argCount == 1)                                                         \
        {                                                                                \
            const T value = BF2_CC_ARG(T, 0);                                             \
            m_hasReturnValue = false;                                                    \
            setDebugMeshConsoleValue((receiverExpr), &Table::member, value);              \
        }                                                                                \
    }                                                                                    \
    virtual void setArgFromString(int index, const std::string& value)                    \
    {                                                                                    \
        if (index == 1)                                                                   \
        {                                                                                \
            static T stored;                                                             \
            std::istringstream in(value);                                                \
            readDebugMeshConsoleValue(in, stored);                                        \
            m_args[0] = reinterpret_cast<std::string*>(&stored);                          \
        }                                                                                \
    }                                                                                    \
    BF2_CC_RETURN_AS_STRING                                                               \
    BF2_CC_COMMON_HOOKS                                                                   \
private:                                                                                 \
    T m_returnValue;                                                                      \
};                                                                                       \
static BF2_CC_NAME BF2_CC_INSTANCE;

// PE 0x101CEFD0: access 2, category 0; all 32 template properties are dynamic.
BF2_CONSOLE_OBJECT("GeometryTemplate", 2, 0)

// Sphere: PE constructor / executor / converter.
// PE 0x1007DFA0 / 0x1007E120 / 0x1007E0A0.
GEOMMISC_CONSOLE_PROPERTY("color", Vec3, "Vec3", activeDebugTemplate(80004), TexturedMeshConsoleSlots, colour)
// PE 0x1007E200 / 0x1007E2F0 / 0x1007E3C0.
GEOMMISC_CONSOLE_PROPERTY("shader", std::string, "std::string", activeDebugTemplate(80004), TexturedMeshConsoleSlots, shader)
// PE 0x1007E440 / 0x1007E530 / 0x1007E600.
GEOMMISC_CONSOLE_PROPERTY("texture", std::string, "std::string", activeDebugTemplate(80004), TexturedMeshConsoleSlots, texture)
// PE 0x1007E680 / 0x1007E7C0 / 0x1007E930.
// PE 0x1007E930: setArgFromString specialization.
// PE 0x1007E890: return formatter (flag +0xA0, unsigned pair at +0xA4 with a slash, empty string without a value).
GEOMMISC_CONSOLE_PROPERTY("tesselation", MeshConsoleTessellation, "BaseVector2<uint>", activeDebugTemplate(80004), TexturedMeshConsoleSlots, tessellation)

// Box.
// PE 0x1007E9E0 / 0x1007EB60 / 0x1007EAE0.
GEOMMISC_CONSOLE_PROPERTY("color", Vec3, "Vec3", activeDebugTemplate(80006), BoxMeshConsoleSlots, colour)
// PE 0x1007EC40 / 0x1007ED30 / 0x1007EE00.
GEOMMISC_CONSOLE_PROPERTY("shader", std::string, "std::string", activeDebugTemplate(80006), BoxMeshConsoleSlots, shader)
// PE 0x1007EE80 / 0x1007EFD0 / 0x1007EF70.
GEOMMISC_CONSOLE_PROPERTY("width", float, "float", activeDebugTemplate(80006), BoxMeshConsoleSlots, width)
// PE 0x1007F090 / 0x1007F230 / 0x1007F180.
GEOMMISC_CONSOLE_PROPERTY("height", float, "float", activeDebugTemplate(80006), BoxMeshConsoleSlots, height)
// PE 0x1007F2F0 / 0x1007F440 / 0x1007F3E0.
GEOMMISC_CONSOLE_PROPERTY("depth", float, "float", activeDebugTemplate(80006), BoxMeshConsoleSlots, depth)

// Plane and Cylinder deliberately query the same IID; no class-ID filter.
// PE 0x1007F500 / 0x1007F680 / 0x1007F600.
GEOMMISC_CONSOLE_PROPERTY("color", Vec3, "Vec3", activeDebugTemplate(80008), TexturedMeshConsoleSlots, colour)
// PE 0x1007F760 / 0x1007F850 / 0x1007F920.
GEOMMISC_CONSOLE_PROPERTY("shader", std::string, "std::string", activeDebugTemplate(80008), TexturedMeshConsoleSlots, shader)
// PE 0x1007F9A0 / 0x1007FA90 / 0x1007FB60.
GEOMMISC_CONSOLE_PROPERTY("texture", std::string, "std::string", activeDebugTemplate(80008), TexturedMeshConsoleSlots, texture)
// PE 0x1007FBE0 / 0x1007FD20 / 0x1007FE80 / 0x1007FDF0: constructor, executor, converter, return formatter.
// The formatter (flag +0xA0, the unsigned pair at +0xA4/+0xA8 with a slash, empty string without a
// value) is the same pair formatting as 0x1007E890.
GEOMMISC_CONSOLE_PROPERTY("tesselation", MeshConsoleTessellation, "BaseVector2<uint>", activeDebugTemplate(80008), TexturedMeshConsoleSlots, tessellation)
// PE 0x1007FF10 / 0x10080090 / 0x10080010.
GEOMMISC_CONSOLE_PROPERTY("color", Vec3, "Vec3", activeDebugTemplate(80008), CylinderMeshConsoleSlots, colour)
// PE 0x10080170 / 0x100802B0 / 0x10080380.
GEOMMISC_CONSOLE_PROPERTY("shader", std::string, "std::string", activeDebugTemplate(80008), CylinderMeshConsoleSlots, shader)
// PE 0x10080400 / 0x10080550 / 0x100804F0. Shipped spelling: lenght.
GEOMMISC_CONSOLE_PROPERTY("lenght", float, "float", activeDebugTemplate(80008), CylinderMeshConsoleSlots, length)
// PE 0x10080610 / 0x10080760 / 0x10080700.
GEOMMISC_CONSOLE_PROPERTY("radius1", float, "float", activeDebugTemplate(80008), CylinderMeshConsoleSlots, radius1)
// PE 0x10080820 / 0x10080970 / 0x10080910.
GEOMMISC_CONSOLE_PROPERTY("radius2", float, "float", activeDebugTemplate(80008), CylinderMeshConsoleSlots, radius2)
// PE 0x10080A30 / 0x10080B80 / 0x10080B20.
GEOMMISC_CONSOLE_PROPERTY("slices", unsigned int, "uint", activeDebugTemplate(80008), CylinderMeshConsoleSlots, slices)
// PE 0x10080C40 / 0x10080D90 / 0x10080D30.
GEOMMISC_CONSOLE_PROPERTY("stacks", unsigned int, "uint", activeDebugTemplate(80008), CylinderMeshConsoleSlots, stacks)

// Polygon.
// PE 0x10080E50 / 0x10080FD0 / 0x10080F50.
GEOMMISC_CONSOLE_PROPERTY("color", Vec3, "Vec3", activeDebugTemplate(80010), PolygonMeshConsoleSlots, colour)
// PE 0x100810B0 / 0x100811A0 / 0x10081270.
GEOMMISC_CONSOLE_PROPERTY("shader", std::string, "std::string", activeDebugTemplate(80010), PolygonMeshConsoleSlots, shader)
// PE 0x100812F0 / 0x10081490 / 0x100813E0.
GEOMMISC_CONSOLE_PROPERTY("length", float, "float", activeDebugTemplate(80010), PolygonMeshConsoleSlots, length)
// PE 0x10081550 / 0x100816A0 / 0x10081640.
GEOMMISC_CONSOLE_PROPERTY("sides", unsigned int, "uint", activeDebugTemplate(80010), PolygonMeshConsoleSlots, sides)

// Torus.
// PE 0x10081760 / 0x100818E0 / 0x10081860.
GEOMMISC_CONSOLE_PROPERTY("color", Vec3, "Vec3", activeDebugTemplate(80012), TorusMeshConsoleSlots, colour)
// PE 0x100819C0 / 0x10081AB0 / 0x10081B80.
GEOMMISC_CONSOLE_PROPERTY("shader", std::string, "std::string", activeDebugTemplate(80012), TorusMeshConsoleSlots, shader)
// PE 0x10081C00 / 0x10081D50 / 0x10081CF0.
GEOMMISC_CONSOLE_PROPERTY("innerRadius", float, "float", activeDebugTemplate(80012), TorusMeshConsoleSlots, innerRadius)
// PE 0x10081E10 / 0x10082020 / 0x10081F70.
GEOMMISC_CONSOLE_PROPERTY("outerRadius", float, "float", activeDebugTemplate(80012), TorusMeshConsoleSlots, outerRadius)
// PE 0x100820E0 / 0x100822A0 / 0x10082240.
GEOMMISC_CONSOLE_PROPERTY("sides", unsigned int, "uint", activeDebugTemplate(80012), TorusMeshConsoleSlots, sides)
// PE 0x10082360 / 0x100824B0 / 0x10082450.
GEOMMISC_CONSOLE_PROPERTY("rings", unsigned int, "uint", activeDebugTemplate(80012), TorusMeshConsoleSlots, rings)

// X uses template IID 80037, distinct from the debug-circle interface.
// PE 0x10082570 / 0x10082740 / 0x100826C0.
GEOMMISC_CONSOLE_PROPERTY("color", Vec3, "Vec3", activeDebugTemplate(80037), XMeshConsoleSlots, colour)
// PE 0x10082820 / 0x10082910 / 0x100829E0.
GEOMMISC_CONSOLE_PROPERTY("shader", std::string, "std::string", activeDebugTemplate(80037), XMeshConsoleSlots, shader)

// PE 0x101CF3F0 / 0x10082A60 / 0x10082C40.
BF2_CONSOLE_OBJECT("Object", 2, 0)
// PE 0x10082BB0: setArgFromString specialization.
GEOMMISC_CONSOLE_PROPERTY("geometry.color", Vec4, "Vec4", activeDebugSphere(), SphereInstanceConsoleSlots, colour)

#undef GEOMMISC_CONSOLE_PROPERTY
} // namespace

// Native TU-sharing adapters for the renderer's calls to the same vector readers.
void readRendererConsoleVector(std::istream& in, Vec3& value)
{
    readDebugMeshConsoleValue(in, value);
}

void readRendererConsoleVector(std::istream& in, Vec4& value)
{
    readDebugMeshConsoleValue(in, value);
}


// PE 0x1007D7C0 (vtable 0x101E2EF4, `retn 4`). Appends the 28 geometry-utility class registrations in
// the shipped push order (Font.Ported first, FSMoviePlayer last; factories 0x1007C7C0..0x1007D030).
void ModuleGeomMisc::getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::geom::ModuleGeomMisc::getClassList", 0x10);

    ::renddx9abi::HostVectorWriter<ClassRegInfo> out(hostVector);

    // Font implementation used by the console overlay.
    ClassRegInfo font;
    font.classId = CID_FontPorted;                  // 81007
    font.name    = "dice.hfe.geom.Font.Ported";
    font.create  = RD9_CLASS_FACTORY(&createFontPorted);
    out.push_back(font);

    // The editor's D3DX font, registered between the two at 0x1007D811..0x1007D841.
    ClassRegInfo editorFont;
    editorFont.classId = CID_EditorFont;            // 81043
    editorFont.name    = "dice.hfe.geom.Font.Editor";
    editorFont.create  = RD9_CLASS_FACTORY(&createEditorFont);
    out.push_back(editorFont);

    // OverlayTexture - the second half of initializeFontAndConsoleOverlay's pair. Its name and
    // classId are the ones getClassList uses at 0x1007D858 and 0x1007D866.
    ClassRegInfo overlay;
    overlay.classId = CID_OverlayTexture;           // 81008
    overlay.name    = "dice.hfe.geom.OverlayTexture";
    overlay.create  = RD9_CLASS_FACTORY(&createOverlayTexture);
    out.push_back(overlay);

    // PE 0x1007D89F..0x1007D9B5: Sphere, Box, Plane and Cylinder in shipped order.
    ClassRegInfo sphere;
    sphere.classId = CID_DebugSphereMesh;
    sphere.name = "dice.hfe.geom.GeometryTemplate.DebugSphereMesh";
    sphere.create = RD9_CLASS_FACTORY(&createDebugSphereMesh);
    out.push_back(sphere);

    ClassRegInfo box;
    box.classId = CID_DebugBoxMesh;
    box.name = "dice.hfe.geom.GeometryTemplate.DebugBoxMesh";
    box.create = RD9_CLASS_FACTORY(&createDebugBoxMesh);
    out.push_back(box);

    ClassRegInfo plane;
    plane.classId = CID_DebugPlaneMesh;
    plane.name = "dice.hfe.geom.GeometryTemplate.DebugPlaneMesh";
    plane.create = RD9_CLASS_FACTORY(&createDebugPlaneMesh);
    out.push_back(plane);

    ClassRegInfo cylinder;
    cylinder.classId = CID_DebugCylinderMesh;
    cylinder.name = "dice.hfe.geom.GeometryTemplate.DebugCylinderMesh";
    cylinder.create = RD9_CLASS_FACTORY(&createDebugCylinderMesh);
    out.push_back(cylinder);

    // PE 0x1007D9BB..0x1007DA02.
    ClassRegInfo polygon;
    polygon.classId = CID_DebugPolygonMesh;
    polygon.name = "dice.hfe.geom.GeometryTemplate.DebugPolygonMesh";
    polygon.create = RD9_CLASS_FACTORY(&createDebugPolygonMesh);
    out.push_back(polygon);

    // PE 0x1007DA02..0x1007DA49.
    ClassRegInfo torus;
    torus.classId = CID_DebugTorusMesh;
    torus.name = "dice.hfe.geom.GeometryTemplate.DebugTorusMesh";
    torus.create = RD9_CLASS_FACTORY(&createDebugTorusMesh);
    out.push_back(torus);

    // PE 0x1007DA49..0x1007DA90.
    ClassRegInfo x;
    x.classId = CID_DebugXMesh;
    x.name = "dice.hfe.geom.GeometryTemplate.DebugXMesh";
    x.create = RD9_CLASS_FACTORY(&createDebugXMesh);
    out.push_back(x);

    // Both DebugTextWriter singleton names use this class and interface ID.
    ClassRegInfo info;
    info.classId = CID_DebugTextWriter;             // 81041
    info.name    = "dice.hfe.geom.DebugTextWriter";
    info.create  = RD9_CLASS_FACTORY(&createDebugTextWriter);
    out.push_back(info);

    // getClassList 0x1007DAD7..0x1007DB07: the manager's row directly follows the writer's, class
    // id dword_1020AF1C (0x13C92), factory 0x1007CE50. The singleton list binds it to
    // g_DebugTextWriterManager under "DebugTextWriterManager".
    ClassRegInfo writerManager;
    writerManager.classId = 81042u;
    writerManager.name = "dice.hfe.geom.DebugTextWriterManager";
    writerManager.create = RD9_CLASS_FACTORY(&createDebugTextWriterManager);
    out.push_back(writerManager);

    // Registers font classes; GPU resources are acquired when an instance first needs them.
    // [INFERRED] Fully qualified class names.
    ClassRegInfo fontSys;
    fontSys.classId = CID_FontSystem;               // 81044
    fontSys.name    = "dice.hfe.geom.FontSystem";
    fontSys.create  = RD9_CLASS_FACTORY(&createFontSystem);
    out.push_back(fontSys);

    ClassRegInfo fontRend;
    fontRend.classId = CID_FontRenderer;            // 81047
    fontRend.name    = "dice.hfe.geom.FontRenderer";
    fontRend.create  = RD9_CLASS_FACTORY(&createFontRenderer);
    out.push_back(fontRend);

    // PE 0x1007DBAC..0x1007DBF3. IID 80013 / CID 81021 already has a singleton row.
    ClassRegInfo lineDrawer;
    lineDrawer.classId = CID_DebugLineDrawer;
    lineDrawer.name = "dice.hfe.geom.DebugLineDrawer";
    lineDrawer.create = RD9_CLASS_FACTORY(&createDebugLineDrawer);
    out.push_back(lineDrawer);

    // PE 0x1007DBF3..0x1007DC3A. The singleton row already names this factory's class.
    ClassRegInfo circleDrawer;
    circleDrawer.classId = CID_DebugCircleDrawer;
    circleDrawer.name = "dice.hfe.geom.DebugCircleDrawer";
    circleDrawer.create = RD9_CLASS_FACTORY(&createDebugCircleDrawer);
    out.push_back(circleDrawer);

    // PE 0x1007DC3A..0x1007DCC2. These classes already have singleton rows below.
    ClassRegInfo textDrawer;
    textDrawer.classId = CID_DebugTextDrawer;
    textDrawer.name = "dice.hfe.geom.DebugTextDrawer";
    textDrawer.create = RD9_CLASS_FACTORY(&createDebugTextDrawer);
    out.push_back(textDrawer);

    ClassRegInfo debugRenderer;
    debugRenderer.classId = CID_DebugRenderer;
    debugRenderer.name = "dice.hfe.geom.DebugRenderer";
    debugRenderer.create = RD9_CLASS_FACTORY(&createDebugRenderer);
    out.push_back(debugRenderer);

    // Registers the graphs created by Renderer::initialize.
    ClassRegInfo graph;
    graph.classId = CID_DebugLineGraph;             // 81038
    graph.name    = "dice.hfe.geom.DebugLineGraph";
    graph.create  = RD9_CLASS_FACTORY(&createDebugLineGraph);
    out.push_back(graph);

    // PE 0x1007DD0F..0x1007DD3F.
    ClassRegInfo blockGraph;
    blockGraph.classId = 81040u;
    blockGraph.name = "dice.hfe.geom.DebugBlockGraph";
    blockGraph.create = RD9_CLASS_FACTORY(&createDebugBlockGraph);
    out.push_back(blockGraph);

    // PE 0x1007DD56..0x1007DD86: the graph manager behind the "DebugLineGraphRenderer" singleton
    // row below, class id dword_1020AF10 (81039), factory 0x1007CCC0.
    ClassRegInfo graphRenderer;
    graphRenderer.classId = 81039u;
    graphRenderer.name = "dice.hfe.geom.DebugGraphRenderer";
    graphRenderer.create = RD9_CLASS_FACTORY(&createDebugGraphRenderer);
    out.push_back(graphRenderer);

    // HUD construction requires Meme.Texture; its factory result is used without a null check.
    ClassRegInfo meme;
    meme.classId = CID_MemeTexture;                 // 81032
    meme.name    = "dice.hfe.geom.Meme.Texture";
    meme.create  = RD9_CLASS_FACTORY(&createMemeTexture);
    out.push_back(meme);

    // PE 0x1007DDE4: the complete cache class is created only on demand.
    ClassRegInfo quadCache;
    quadCache.classId = 81030u;
    quadCache.name = "dice.hfe.geom.Meme.QuadOutputCache";
    quadCache.create = RD9_CLASS_FACTORY(&createQuadOutputCache);
    out.push_back(quadCache);

    // Menu texture batch renderer published through g_QuadDrawer.
    ClassRegInfo quad;
    quad.classId = CID_QuadDrawer;                  // 81031, dword_1020AEF0
    quad.name    = "dice.hfe.geom.Meme.QuadDrawer";
    quad.create  = RD9_CLASS_FACTORY(&createQuadDrawer);
    out.push_back(quad);

    // Editor spline and road-noise factories; created on demand rather than as singletons.
    ClassRegInfo spline;
    spline.classId = CID_EditorSpline;              // 81033
    spline.name    = "dice.hfe.geom.EditorSpline";
    spline.create  = RD9_CLASS_FACTORY(&createEditorSpline);
    out.push_back(spline);

    ClassRegInfo noise;
    noise.classId = CID_PerlinNoise;                // 81035
    noise.name    = "dice.hfe.geom.PerlinNoise";
    noise.create  = RD9_CLASS_FACTORY(&createPerlinNoise);
    out.push_back(noise);

    // PE 0x1007DF00..0x1007DF30.
    ClassRegInfo loading;
    loading.classId = 81045u;
    loading.name = "dice.hfe.geom.LoadingScreen";
    loading.create = RD9_CLASS_FACTORY(&createLoadingScreen);
    out.push_back(loading);

    // Full-screen movie player used by the client's startup-movie queue.
    ClassRegInfo movie;
    movie.classId = CID_FSMoviePlayer;              // 81046, dword_1020AF2C
    movie.name    = "dice.hfe.geom.FSMoviePlayer";
    movie.create  = RD9_CLASS_FACTORY(&createFSMoviePlayer);
    out.push_back(movie);

    // Video and VideoTarget are ModuleGeom's (PE 0x100C7F43), registered there.
    ::renddx9abi::logf("ModuleGeomMisc::getClassList: 28 class factories registered");
}


// PE 0x1007D3D0 (eleven entries, flags 2, QuadDrawer through FontRenderer; each entry is pushed
// through the vector<SingletonRegInfo> push_back 0x1005F1E0). Appends geometry-utility singleton
// registrations. Proportional and fixed debug text use separate instances of one class.
void ModuleGeomMisc::getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::geom::ModuleGeomMisc::getSingletonList", 0x14);

    struct Entry { const char* name; IBase** instance; unsigned int iid; unsigned int classId; };
    static const Entry entries[11] =
    {
        { "QuadDrawer",             &g_QuadDrawer,             80031u, 81031u },
        { "DebugRenderer",          &g_DebugRenderer,          80034u, 81034u },
        { "DebugLineDrawer",        &g_DebugLineDrawer,        80013u, 81021u },
        { "DebugCircleDrawer",      &g_DebugCircleDrawer,      80014u, 81022u },
        { "DebugTextDrawer",        &g_DebugTextDrawer,        80038u, 81037u },
        { "DebugLineGraphRenderer", &g_DebugLineGraphRenderer, 80040u, 81039u },
        { "DebugTextWriterManager", &g_DebugTextWriterManager, 80044u, 81042u },
        { "DebugTextWriter",        &g_DebugTextWriter,        80043u, 81041u },
        { "DebugTextWriterFixed",   &g_DebugTextWriterFixed,   80043u, 81041u },
        { "FontSystem",             &g_FontSystem,             80045u, 81044u },
        { "FontRenderer",           &g_FontRenderer,           80048u, 81047u },
    };

    ::renddx9abi::HostVectorWriter<SingletonRegInfo> out(hostVector);

    for (int i = 0; i < 11; ++i)
    {
        SingletonRegInfo info;
        info.name     = entries[i].name;
        info.instance = entries[i].instance;
        info.iid      = entries[i].iid;
        info.classId  = entries[i].classId;
        info.flags    = 2;
        out.push_back(info);
    }
}

// PE 0x1007D080.
bool ModuleGeomMisc::init()
{
    RD9_TRACE("dice::hfe::geom::ModuleGeomMisc::init", 0x18);

    mainConsoleObjects->registerConsoleObjects(consoleBindingList(kGeomMiscConsoleTag));

    // The remainder of the shipped body allocates 0xE4 bytes, constructs FSQuadDrawer and stores
    // it in dword_10249978.  initializeFSQuadDrawer preserves the null-on-allocation-failure arm.
    initializeFSQuadDrawer();
    return true;
}

// PE 0x1007D360.
bool ModuleGeomMisc::close()
{
    RD9_TRACE("dice::hfe::geom::ModuleGeomMisc::close", 0x1C);

    mainConsoleObjects->unregisterConsoleObjects(consoleBindingList(kGeomMiscConsoleTag));

    // Shipped order after unregister: FSQuadDrawer::~FSQuadDrawer, then engineFree.  Retail leaves
    // dword_10249978 holding the freed address; destroyFSQuadDrawer deliberately preserves that.
    destroyFSQuadDrawer();
    return true;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
