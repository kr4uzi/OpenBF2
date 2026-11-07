// Debug cylinder geometry. Source placement is attested by the PE diagnostic path.
#ifdef BF2_IMAGE_RENDDX9
#include "DebugMeshCommon.h"
#include "Debug.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/BfMemory.h"
#include <d3dx9.h>
#include <list>
#include <new>

namespace dice { namespace hfe { namespace dx9rend {
class Effect;
extern void* g_rendererBase;
extern void* g_shaderManager;
void* shaderManagerGetEffect(void*, const char*);
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect*, const std::string&);
int effectRelease(Effect*);
bool effectSetTechniqueByName(void*, const std::string&);
unsigned int effectBegin(void*);
void effectBeginPass(void*, unsigned int);
void effectEnd(void*);
void rendererDrawIndexedPrimitive(unsigned int, unsigned int, int, unsigned int, unsigned int, unsigned int);
} } }

namespace dice { namespace hfe { namespace geom {
extern const unsigned int CID_DebugCylinderMesh = 81016u;
namespace {
using namespace debugmesh;
struct DebugCylinderMeshTemplate;
extern void* const kCylinderTable[70];
extern void* const kCylinderTemplateTable[60];

// [INFERRED] Member names; native strings, lists and pointers follow the existing issue #7 ABI.
struct DebugCylinderMesh : GeometryInstanceData
{
    explicit DebugCylinderMesh(DebugCylinderMeshTemplate*);
    ~DebugCylinderMesh();
    void initializeParameters();
    void draw();
    int refs;                       // PE +0x30
    Vec3 dimensions;                 // +0x34
    Math::Vec4 colour;               // +0x40
    float radius1;                  // +0x50
    float radius2;                  // +0x54
    DebugCylinderMeshTemplate* owner; // +0x58
    dx9rend::ShaderParameter* ambient; // +0x5C
    dx9rend::ShaderParameter* coneSkin; // +0x60, not initialized by the constructor
    Mat4 transform;                 // +0x64
};

struct DebugCylinderMeshTemplate
{
    DebugCylinderMeshTemplate();
    ~DebugCylinderMeshTemplate();
    void createResources();
    void load();
    DebugCylinderMesh* createInstance();
    void* const* table;              // PE +0x00
    unsigned char loaded;            // +0x04
    unsigned int field08;            // +0x08
    int refs;                       // +0x0C
    std::list<DebugCylinderMesh*> instances; // +0x10
    float radius1;                  // +0x1C
    float radius2;                  // +0x20
    float length;                   // +0x24
    unsigned int slices;            // +0x28
    unsigned int stacks;            // +0x2C
    unsigned int field30;            // +0x30
    std::string name;               // +0x34
    std::string path;               // +0x50
    Vec3 instanceSize;              // +0x6C
    Vec3 vector78;                  // +0x78
    std::string shaderName;         // +0x84
    ID3DXMesh* mesh;                // +0xA0
    dx9rend::Effect* effect;         // +0xA4
    dx9rend::ShaderParameter* viewProjection; // +0xA8
    dx9rend::ShaderParameter* world; // +0xAC
    IDirect3DVertexBuffer9* vertices; // +0xB0
    IDirect3DIndexBuffer9* indices;  // +0xB4
    unsigned int faceCount;         // +0xB8
    unsigned int vertexCount;       // +0xBC
};

// PE 0x101071D0 / 0x101071E0 / 0x101071F0 / 0x10090A70.
int RD9_THUNK cylinder_addRef(DebugCylinderMesh* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK cylinder_getRef(DebugCylinderMesh* s RD9_THUNK_EDX) { return s->refs; }
void* RD9_THUNK cylinder_destroy(DebugCylinderMesh* s RD9_THUNK_EDX, unsigned int flags)
{
    s->~DebugCylinderMesh();
    if (flags & 1) ::renddx9abi::engineFree(s);
    return s;
}
int RD9_THUNK cylinder_release(DebugCylinderMesh* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs)
    {
        typedef void* (__thiscall* Destroy)(void*, unsigned int);
        reinterpret_cast<Destroy>(s->m_vptr[13])(s, 1);
    }
    return refs;
}
// PE 0x10090A30.
void* RD9_THUNK cylinder_query(DebugCylinderMesh* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0xADCBF4FCu || iid == 0x492FE0FEu || iid == 80007u ? s : 0; }
// PE 0x100904D0 / 0x10090080 / 0x10090090 / 0x10090000 / 0x10090010.
void RD9_THUNK cylinder_draw(DebugCylinderMesh* s RD9_THUNK_EDX, void*, void*, unsigned char) { s->draw(); }
int RD9_THUNK cylinder_type(void* RD9_THUNK_EDX) { return 2; }
bool RD9_THUNK cylinder_hasGeom(void* RD9_THUNK_EDX, int index) { return index == 0; }
void* RD9_THUNK cylinder_owner(DebugCylinderMesh* s RD9_THUNK_EDX) { return s->owner; }
const Mat4* RD9_THUNK cylinder_transform(DebugCylinderMesh* s RD9_THUNK_EDX) { return &s->transform; }
// PE 0x100902A0. All instances overwrite one local-bounds object.
const LocalBoundingBox* RD9_THUNK cylinder_bounds(DebugCylinderMesh* s RD9_THUNK_EDX)
{
    static LocalBoundingBox box;
    const float z = s->dimensions.z * 0.5f;
    box.m_derivedDataDirty = true;
    box.m_max.z = z;
    box.m_min.x = -(s->dimensions.x * 0.5f);
    box.m_min.y = -(s->dimensions.y * 0.5f);
    box.m_min.z = -z;
    box.m_max.x = s->dimensions.x * 0.5f;
    box.m_max.y = s->dimensions.y * 0.5f;
    return &box;
}
// PE 0x100903B0. Ordered comparisons retain the shipped NaN selection.
float RD9_THUNK cylinder_radius(DebugCylinderMesh* s RD9_THUNK_EDX)
{
    const float xz = s->dimensions.x < s->dimensions.z ? s->dimensions.z : s->dimensions.x;
    const float xy = s->dimensions.x < s->dimensions.y ? s->dimensions.y : s->dimensions.x;
    return (xy < xz ? xz : xy) * 0.5f;
}
// PE 0x10090410.
void RD9_THUNK cylinder_setTransform(DebugCylinderMesh* s RD9_THUNK_EDX, const Mat4& v)
{
    s->transform = v;
    const Vec3 unit{1,1,1};
    if (!equalVector(s->dimensions, unit))
    {
        s->transform.m[0] *= s->dimensions.x;
        s->transform.m[5] *= s->dimensions.y;
        s->transform.m[10] *= s->dimensions.z;
    }
}
// PE 0x10090480 / 0x10090020 / 0x100904B0 / 0x10090030.
void RD9_THUNK cylinder_setColour(DebugCylinderMesh* s RD9_THUNK_EDX, const Math::Vec4& v) { s->colour = v; }
const Math::Vec4& RD9_THUNK cylinder_colour(DebugCylinderMesh* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK cylinder_setDimensions(DebugCylinderMesh* s RD9_THUNK_EDX, const Vec3& v) { s->dimensions = v; }
const Vec3& RD9_THUNK cylinder_dimensions(DebugCylinderMesh* s RD9_THUNK_EDX) { return s->dimensions; }
// PE 0x10090040 / 0x10090050 / 0x10090060 / 0x10090070.
void RD9_THUNK cylinder_setRadius1(DebugCylinderMesh* s RD9_THUNK_EDX, float v) { s->radius1 = v; }
float RD9_THUNK cylinder_radius1(DebugCylinderMesh* s RD9_THUNK_EDX) { return s->radius1; }
void RD9_THUNK cylinder_setRadius2(DebugCylinderMesh* s RD9_THUNK_EDX, float v) { s->radius2 = v; }
float RD9_THUNK cylinder_radius2(DebugCylinderMesh* s RD9_THUNK_EDX) { return s->radius2; }

#define CYL_SLOT(f) reinterpret_cast<void*>(&f)
// PE 0x1020BD00.
void* const kCylinderTable[70] = {
    CYL_SLOT(cylinder_addRef), CYL_SLOT(cylinder_getRef), CYL_SLOT(cylinder_release), CYL_SLOT(cylinder_query),
    CYL_SLOT(empty1), CYL_SLOT(minusOneFloat), CYL_SLOT(cylinder_draw), CYL_SLOT(empty2),
    CYL_SLOT(empty6), CYL_SLOT(empty1), CYL_SLOT(zero0), CYL_SLOT(cylinder_type),
    CYL_SLOT(cylinder_hasGeom), CYL_SLOT(cylinder_destroy), CYL_SLOT(cylinder_owner), CYL_SLOT(zero1),
    CYL_SLOT(cylinder_bounds), CYL_SLOT(cylinder_radius), CYL_SLOT(cylinder_transform), CYL_SLOT(empty2),
    CYL_SLOT(cylinder_setTransform), CYL_SLOT(empty2), CYL_SLOT(empty1), CYL_SLOT(zero0),
    CYL_SLOT(empty2), CYL_SLOT(empty1), CYL_SLOT(setName), CYL_SLOT(empty1),
    CYL_SLOT(zero0), CYL_SLOT(one), CYL_SLOT(empty1), CYL_SLOT(zero0),
    CYL_SLOT(one), CYL_SLOT(empty1), CYL_SLOT(minusOne), CYL_SLOT(zero0),
    CYL_SLOT(empty3), CYL_SLOT(empty2), CYL_SLOT(empty2), CYL_SLOT(empty1),
    CYL_SLOT(zero0), CYL_SLOT(zero0), CYL_SLOT(false0), CYL_SLOT(empty1),
    CYL_SLOT(zero0), CYL_SLOT(empty1), CYL_SLOT(empty1), CYL_SLOT(false0),
    CYL_SLOT(empty2), CYL_SLOT(zero0), CYL_SLOT(zero1), CYL_SLOT(zero0),
    CYL_SLOT(false0), CYL_SLOT(empty1), CYL_SLOT(false0), CYL_SLOT(empty1),
    CYL_SLOT(empty1), CYL_SLOT(seven), CYL_SLOT(zero0), CYL_SLOT(zero1),
    CYL_SLOT(empty0), CYL_SLOT(empty1), CYL_SLOT(cylinder_setColour), CYL_SLOT(cylinder_colour),
    CYL_SLOT(cylinder_setDimensions), CYL_SLOT(cylinder_dimensions), CYL_SLOT(cylinder_setRadius1), CYL_SLOT(cylinder_radius1),
    CYL_SLOT(cylinder_setRadius2), CYL_SLOT(cylinder_radius2)
};
// PE 0x10132630 / 0x10051710 / 0x10090C00 / 0x10090BD0.
int RD9_THUNK cylinderTemplate_addRef(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK cylinderTemplate_getRef(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->refs; }
int RD9_THUNK cylinderTemplate_release(DebugCylinderMeshTemplate* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs) { s->~DebugCylinderMeshTemplate(); ::renddx9abi::engineFree(s); }
    return refs;
}
void* RD9_THUNK cylinderTemplate_query(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0x17701FFFu || iid == 80008u ? s : 0; }
// PE 0x1008FD70 / 0x1008FD80 / 0x1008FD90 / 0x1008FDA0 / 0x1008FDB0.
unsigned int RD9_THUNK cylinderTemplate_class(void* RD9_THUNK_EDX) { return CID_DebugCylinderMesh; }
void RD9_THUNK cylinderTemplate_setName(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->name = v; }
const std::string& RD9_THUNK cylinderTemplate_name(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->name; }
void RD9_THUNK cylinderTemplate_setPath(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->path = v; }
const std::string& RD9_THUNK cylinderTemplate_path(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->path; }
// PE 0x10090990 / 0x100906F0.
void* RD9_THUNK cylinderTemplate_create(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->createInstance(); }
bool RD9_THUNK cylinderTemplate_load(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { s->load(); return true; }
// PE 0x1008FDC0 / 0x1008FDD0 / 0x100900A0 / 0x1008FDE0 / 0x100900C0 / 0x1008FDF0.
void RD9_THUNK cylinderTemplate_setShader(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->shaderName = v; }
const std::string& RD9_THUNK cylinderTemplate_shader(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->shaderName; }
void RD9_THUNK cylinderTemplate_setVector(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, const Vec3& v) { s->vector78 = v; }
const Vec3& RD9_THUNK cylinderTemplate_vector(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->vector78; }
void RD9_THUNK cylinderTemplate_setSize(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, const Vec3& v) { s->instanceSize = v; }
const Vec3& RD9_THUNK cylinderTemplate_size(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->instanceSize; }
// PE 0x1008FE90 / 0x1008FEA0 / 0x1008FEB0 / 0x1008FEC0 / 0x1008FED0 / 0x1008FEE0.
void RD9_THUNK cylinderTemplate_setLength(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, float v) { s->length = v; }
float RD9_THUNK cylinderTemplate_length(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->length; }
void RD9_THUNK cylinderTemplate_setRadius1(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, float v) { s->radius1 = v; }
float RD9_THUNK cylinderTemplate_radius1(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->radius1; }
void RD9_THUNK cylinderTemplate_setRadius2(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, float v) { s->radius2 = v; }
float RD9_THUNK cylinderTemplate_radius2(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->radius2; }
// PE 0x1008FEF0 / 0x1008FF00 / 0x1008FF10 / 0x1008FF20.
void RD9_THUNK cylinderTemplate_setSlices(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, unsigned int v) { s->slices = v; }
unsigned int RD9_THUNK cylinderTemplate_slices(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->slices; }
void RD9_THUNK cylinderTemplate_setStacks(DebugCylinderMeshTemplate* s RD9_THUNK_EDX, unsigned int v) { s->stacks = v; }
unsigned int RD9_THUNK cylinderTemplate_stacks(DebugCylinderMeshTemplate* s RD9_THUNK_EDX) { return s->stacks; }
// PE 0x1020BE78.
void* const kCylinderTemplateTable[60] = {
    CYL_SLOT(cylinderTemplate_addRef), CYL_SLOT(cylinderTemplate_getRef), CYL_SLOT(cylinderTemplate_release), CYL_SLOT(cylinderTemplate_query),
    CYL_SLOT(cylinderTemplate_class), CYL_SLOT(cylinderTemplate_setName), CYL_SLOT(cylinderTemplate_name), CYL_SLOT(cylinderTemplate_setPath),
    CYL_SLOT(cylinderTemplate_path), CYL_SLOT(cylinderTemplate_create), CYL_SLOT(false0), CYL_SLOT(cylinderTemplate_load),
    CYL_SLOT(empty3), CYL_SLOT(lodDistance), CYL_SLOT(empty4), CYL_SLOT(materialValue),
    CYL_SLOT(empty4), CYL_SLOT(quarter), CYL_SLOT(empty1), CYL_SLOT(false0),
    CYL_SLOT(false0), CYL_SLOT(zero1), CYL_SLOT(zero3), CYL_SLOT(zero3),
    CYL_SLOT(empty1), CYL_SLOT(false0), CYL_SLOT(empty1), CYL_SLOT(false0),
    CYL_SLOT(empty3), CYL_SLOT(zero2), CYL_SLOT(empty1), CYL_SLOT(true0),
    CYL_SLOT(empty1), CYL_SLOT(false0), CYL_SLOT(empty1), CYL_SLOT(sixteen),
    CYL_SLOT(empty1), CYL_SLOT(false0), CYL_SLOT(empty1), CYL_SLOT(false0),
    CYL_SLOT(zero1), CYL_SLOT(zero1), CYL_SLOT(decompress), CYL_SLOT(empty0),
    CYL_SLOT(cylinderTemplate_setShader), CYL_SLOT(cylinderTemplate_shader), CYL_SLOT(cylinderTemplate_setVector), CYL_SLOT(cylinderTemplate_vector),
    CYL_SLOT(cylinderTemplate_setLength), CYL_SLOT(cylinderTemplate_length), CYL_SLOT(cylinderTemplate_setRadius1), CYL_SLOT(cylinderTemplate_radius1),
    CYL_SLOT(cylinderTemplate_setRadius2), CYL_SLOT(cylinderTemplate_radius2), CYL_SLOT(cylinderTemplate_setSlices), CYL_SLOT(cylinderTemplate_slices),
    CYL_SLOT(cylinderTemplate_setStacks), CYL_SLOT(cylinderTemplate_stacks), CYL_SLOT(cylinderTemplate_setSize), CYL_SLOT(cylinderTemplate_size)
};
#undef CYL_SLOT

// PE 0x100908D0.
DebugCylinderMeshTemplate::DebugCylinderMeshTemplate()
    : table(kCylinderTemplateTable), loaded(0), field08(0), refs(1), radius1(1), radius2(1), length(1),
      slices(24), stacks(1), field30(0), instanceSize{1,1,1}, vector78{1,1,1},
      shaderName("DebugD3DXMeshShapeShader"), mesh(0), effect(0), viewProjection(0), world(0),
      vertices(0), indices(0), faceCount(0), vertexCount(0) {}
// PE 0x100907F0.
DebugCylinderMeshTemplate::~DebugCylinderMeshTemplate()
{
    if (vertices) vertices->Release();
    if (indices) indices->Release();
    if (mesh) mesh->Release();
    if (viewProjection) viewProjection->release();
    if (world) world->release();
    if (effect) dx9rend::effectRelease(effect);
}
// PE 0x1008FE00. The declaration is fetched and discarded; no HRESULT is tested.
void DebugCylinderMeshTemplate::createResources()
{
    D3DXCreateCylinder(static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device,
                       radius1, radius2, length, slices, stacks, &mesh, 0);
    D3DVERTEXELEMENT9 elements[65];
    mesh->GetDeclaration(elements);
    mesh->GetVertexBuffer(&vertices);
    mesh->GetIndexBuffer(&indices);
    vertexCount = mesh->GetNumVertices();
    faceCount = mesh->GetNumFaces();
}
// PE 0x100900E0. A repeated load overwrites the old resource references.
void DebugCylinderMeshTemplate::load()
{
    createResources();
    {
        const std::string prefix("Shaders/");
        const std::string shader = prefix + shaderName;
        effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, shader.c_str()));
    }
    viewProjection = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "WorldViewProjection");
    if (viewProjection) viewProjection->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugCylinderMesh.cpp", 184, "")
        << "Could not load WorldViewProjection parameter from shader";
    world = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "World");
    if (world) world->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugCylinderMesh.cpp", 192, "")
        << "Could not load WorldViewProjection parameter from shader";
}
// PE 0x10090700. No template reference is taken; coneSkin stays uninitialized.
DebugCylinderMesh::DebugCylinderMesh(DebugCylinderMeshTemplate* value)
    : GeometryInstanceData(kCylinderTable), refs(1), dimensions{0,0,0}, colour{0.5f,0.5f,0.5f,1},
      owner(value), ambient(0)
{
    transform = Mat4::getIdentity();
    if (owner)
    {
        typedef const Vec3& (__thiscall* GetSize)(void*);
        dimensions = reinterpret_cast<GetSize>(owner->table[59])(owner);
        typedef float (__thiscall* GetRadius)(void*);
        radius1 = reinterpret_cast<GetRadius>(owner->table[51])(owner);
        radius2 = reinterpret_cast<GetRadius>(owner->table[53])(owner);
    }
}
// PE 0x1008FF30. Shipped defect: the borrowed template list keeps the stale instance pointer.
DebugCylinderMesh::~DebugCylinderMesh()
{
    if (ambient) ambient->release();
    if (coneSkin) coneSkin->release();
}
// PE 0x1008FF70.
void DebugCylinderMesh::initializeParameters()
{
    if (owner)
    {
        ambient = dx9rend::effectGetShaderParameterBorrowedTyped(owner->effect, "MATERIALAMBIENT");
        if (ambient) ambient->addRef();
        coneSkin = dx9rend::effectGetShaderParameterBorrowedTyped(owner->effect, "CONESKINVALUES");
        if (coneSkin) coneSkin->addRef();
    }
}
// PE 0x10090990. Null allocation is inserted and dereferenced in the tail, as shipped.
DebugCylinderMesh* DebugCylinderMeshTemplate::createInstance()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugCylinderMesh));
    DebugCylinderMesh* instance = storage ? new (storage) DebugCylinderMesh(this) : 0;
    instances.push_back(instance);
    if (!loaded && !dx9rend::resourceManagerIsReady())
    {
        typedef bool (__thiscall* Load)(void*);
        reinterpret_cast<Load>(table[11])(this);
    }
    loaded = 1;
    instance->initializeParameters();
    return instance;
}
// PE 0x100904D0. The draw arguments are unused and no declaration is bound here.
void DebugCylinderMesh::draw()
{
    if (!owner->mesh) return;
    if (!m_name.empty()) dx9rend::effectSetTechniqueByName(owner->effect, m_name);
    else dx9rend::effectSetTechniqueByName(owner->effect, "t0");
    dx9rend::stateManagerSetStreamSource(0, owner->vertices, 0, 24);
    dx9rend::stateManagerSetIndices(owner->indices);
    dx9rend::IRenderView* view = dx9rend::rendererGetRenderViewDup();
    Mat4 viewProjection = Mat4::getIdentity();
    const Mat4& projection = *view->getProjectionMatrixPtr();
    const Mat4& viewMatrix = *view->getInverseViewMatrixPtr();
    viewProjection.fullMult(viewMatrix, projection);
    ambient->setValue(&colour, 0);
    Math::Vec4 values{radius1, radius2, 0, 0};
    typedef float (__thiscall* GetLength)(void*);
    values.z = reinterpret_cast<GetLength>(owner->table[49])(owner);
    coneSkin->setValue(&values, 0);
    owner->viewProjection->setValue(&viewProjection, 0);
    owner->world->setValue(&transform, 0);
    const int passes = static_cast<int>(dx9rend::effectBegin(owner->effect));
    for (int pass = 0; pass < passes; ++pass)
    {
        dx9rend::effectBeginPass(owner->effect, static_cast<unsigned int>(pass));
        // PE 0x10005820's argument order is adapted by the current renderer helper.
        dx9rend::rendererDrawIndexedPrimitive(owner->faceCount, 4, 0, 0, owner->vertexCount, 0);
    }
    const Math::Vec4 unit{1,1,1,1};
    coneSkin->setValue(&unit, 0);
    dx9rend::effectEnd(owner->effect);
}
#if defined(_M_IX86) && defined(_MSC_VER) && _MSC_VER == 1310
static_assert(sizeof(DebugCylinderMesh) == 0xA4, "retail cylinder instance layout");
static_assert(sizeof(DebugCylinderMeshTemplate) == 0xC0, "retail cylinder template layout");
#endif
} // namespace

// PE 0x10090880. Erases only the first matching borrowed instance.
void debugCylinderMeshRemoveInstance(void* value, void* instance)
{
    DebugCylinderMeshTemplate* owner = static_cast<DebugCylinderMeshTemplate*>(value);
    for (auto cursor = owner->instances.begin(); cursor != owner->instances.end(); ++cursor)
        if (*cursor == instance) { owner->instances.erase(cursor); return; }
}
// PE 0x1007CA90. Allocation failure is not guarded before the virtual query.
IBase* __fastcall createDebugCylinderMesh(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugCylinderMeshTemplate));
    DebugCylinderMeshTemplate* instance = storage ? new (storage) DebugCylinderMeshTemplate() : 0;
    typedef void* (__thiscall* Query)(void*, unsigned int);
    void* result = reinterpret_cast<Query>(instance->table[3])(instance, iid);
    if (!result) cylinderTemplate_release(instance RD9_THUNK_ARG);
    return static_cast<IBase*>(result);
}
} } }
#endif
