// Rotor lifecycle and visual rotation.

#include "Rotor.h"
#include "../GeometryPartAccess.h"   // named geometry operations (non-MSVC)

#include <string>

#include "EngineBase.h"                                      // the resolved engine, read typed
#include "../../../Math/Mat4.h"                              // Mat4, setRotationZXY
#include "../../../Math/Vec3.h"                              // Vec3::getZero
#include "../../../Scene/Object/ObjectUtil.h"                // findEngine
#include "../ObjectLayoutAccess.h"                          // +0x38 geometry
#include "../TemplateLeaves.h"                               // RotorTemplate

namespace dice {
namespace hfe {
namespace world {

namespace {
// Selects sharp blades or a blurred disc through the renderer geometry interface.
// The PE and ELF slot offsets differ; getGeomCount counts geometries, not LODs.
#if defined(_MSC_VER)
#define BF2_ROTOR_GEOM_CALL __thiscall
const unsigned int setGeomSlot = 0x6C, getGeomCountSlot = 0x74;
template<class F> F geometrySlot(void* geometry, unsigned int offset)
{
    return reinterpret_cast<F>((*static_cast<void***>(geometry))[offset / 4]);
}
void setGeometryGeom(void* geometry, int geom)
{
    typedef void (BF2_ROTOR_GEOM_CALL* F)(void*, int);
    geometrySlot<F>(geometry, setGeomSlot)(geometry, geom);
}
int getGeometryGeomCount(void* geometry)
{
    typedef int (BF2_ROTOR_GEOM_CALL* F)(void*);
    return geometrySlot<F>(geometry, getGeomCountSlot)(geometry);
}
#undef BF2_ROTOR_GEOM_CALL
#else
// Non-MSVC: the named geometry_part operations (ELF +0x70 / +0x78).
void setGeometryGeom(void* geometry, int geom) { geometry_part::setSubgeom(geometry, geom); }
int getGeometryGeomCount(void* geometry) { return geometry_part::getSubgeomCount(geometry); }
#endif
}

// Bundle(template); the vptrs; the block (m_geomState -1, m_spinDownTimer 3.0,
// the engine flag set, the rest 0); flags 0x1802.
Rotor::Rotor(const RotorTemplate* objectTemplate)
    : Bundle(objectTemplate)
{
    m_geomState = -1;                                                                // +0x140
    m_spinDownTimer = 3.0f;                                                          // +0x144
    m_spinDownSpeed = 0.0f;                                                          // +0x148
    m_rotation.x = 0.0f;                                                             // +0x150
    m_needsEngine = true;                                                            // +0x13C
    m_engine = 0;                                                                    // +0x14C
    m_rotation.y = 0.0f;                                                             // +0x154
    m_rotation.z = 0.0f;                                                             // +0x158
    updateFlags(0x1802u, 0u);
}

Rotor::~Rotor() {}                                                                   // ~Bundle

// SimpleObject's (Bundle's is skipped) / 0x82AC810, 78 B.
void Rotor::disableCachedObject()
{
    SimpleObject::disableCachedObject();
}

void Rotor::enableCachedObject()
{
    Bundle::enableCachedObject();
    m_rotation.x = 0.0f;
    m_rotation.y = 0.0f;
    m_rotation.z = 0.0f;
    m_geomState = -1;
    m_spinDownTimer = 3.0f;
    m_spinDownSpeed = 0.0f;
}

// cached; else findEngine over the root by the template's engine name.
IObject* Rotor::getEngine()
{
    if (!m_engine)
    {
        const RotorTemplate* t = static_cast<const RotorTemplate*>(m_template);
        m_engine = findEngine(getRootParent(this), t->m_engineName);                 // template +0x184
    }
    return m_engine;
}

// the one-shot - resolve the engine, then stop ticking.
void Rotor::handleUpdate(float, unsigned int)
{
    if (m_needsEngine)
    {
        m_engine = getEngine();
        m_needsEngine = false;
    }
    updateFlags(0u, 0x800u);
}

// with a geometry - lod 1: fewer than three geoms -> flag 1 set, else the
// geometry's geom 2; any other lod: fewer than three -> flag 1 cleared, else geom 0.
void Rotor::SetActiveLod(ObjectLodType lod)
{
    void* geometry = m_geometry;                                                     // +0x38
    if (!geometry)
        return;
    if (static_cast<int>(lod) == 1)
    {
        if (getGeometryGeomCount(geometry) <= 2)
        {
            updateFlags(1u, 0u);
            return;
        }
        setGeometryGeom(geometry, 2);
    }
    else
    {
        if (getGeometryGeomCount(geometry) <= 2)
        {
            updateFlags(0u, 1u);
            return;
        }
        setGeometryGeom(geometry, 0);
    }
}

// Adjusts the selected rotation component by at most one turn toward (-180, 180].
// Callers must keep accumulated rotation within one turn of that range.
void clipRotation(Vec3& rotation, int axis)
{
    // `lea eax, [edx+eax*4]` on the Vec3's own base - the component is selected by index, not by
    // name, because the template's rotationAxle picks it at run time.
    float* const component = reinterpret_cast<float*>(&rotation) + axis;
    if (*component > 180.0f)
        *component -= 360.0f;
    if (*component < -180.0f)
        *component += 360.0f;
}

// Updates rotor spin and blade/disc selection from engine state, then reapplies its transform.
// A stopped engine spins down over three seconds; geometry changes use throttle hysteresis.
// Nonpositive deltaTime preserves rotation but still reapplies the pose.
void Rotor::handleVisualUpdate(float deltaTime, float distance)
{
    // The two field reads below are raw in both shipped images; EngineBase befriends this class so
    // they are reproduced typed. The downcast is a no-op - IObject is Object's first base.
    EngineBase* const engine = static_cast<EngineBase*>(m_engine);                   // +0x14C
    if (engine)
    {
        const RotorTemplate* const t = static_cast<const RotorTemplate*>(m_template);

        if (deltaTime > 0.0f)
        {
            const float frameScale = deltaTime * 30.0f;                              // ELF flt_888B174

            Vec3 rate = engine->getCurrentRotation();                                // ELF +0x184 / PE +0x17C
            if (engine->m_engineRunning)                                             // +0x1B4 PE +0x1CC
            {
                m_spinDownTimer = 3.0f;
                m_spinDownSpeed = rate.z;
            }
            else if (m_spinDownTimer <= 0.0f)
            {
                rate = Vec3::getZero();
            }
            else
            {
                m_spinDownTimer -= deltaTime;
                rate.z = m_spinDownTimer / 3.0f * m_spinDownSpeed;
            }

            float speedMod;
            if (m_geomState == 0)
            {
                if (t->m_changeLodAt + t->m_tolerance <= engine->m_input)            // +0x1C4 PE +0x1DC
                {
                    void* geometry = m_geometry;                                     // +0x38
                    m_geomState = 1;
                    if (geometry)
                        setGeometryGeom(geometry, 1);
                }
                speedMod = t->m_rotationSpeedMod;                                    // template +0x188
            }
            else if (m_geomState == 1)
            {
                if (t->m_changeLodAt - t->m_tolerance >= engine->m_input)
                {
                    void* geometry = m_geometry;
                    m_geomState = 0;
                    if (geometry)
                        setGeometryGeom(geometry, 0);
                }
                speedMod = t->m_rotationSpeedModBlur;                                // template +0x18C
            }
            else
            {
                void* geometry = m_geometry;
                m_geomState = 0;
                if (geometry)
                    setGeometryGeom(geometry, 0);
                speedMod = t->m_rotationSpeedMod;
            }

            reinterpret_cast<float*>(&m_rotation)[t->m_rotationAxle] +=
                speedMod * rate.z * frameScale;
            clipRotation(m_rotation, t->m_rotationAxle);
        }

        // The shipped code inlines dice's Mat4 default constructor here - a 64-byte zero-fill
        // followed by the four diagonal ones - which this tree's plain-aggregate Mat4 does not
        // model (Math/Mat4.h says why). The identity it produces is the same, and it is
        // load-bearing for `spin`: setRotationZXY writes only the nine 3x3 elements and leaves the
        // translation row and m[15] alone. On `transform` it is dead, because mult writes all
        // sixteen - dead in the shipped code too, and kept for that reason.
        Mat4 spin = Mat4::getIdentity();
        setRotationZXY(spin, -m_rotation.z, -m_rotation.y, -m_rotation.x);
        Mat4 transform = Mat4::getIdentity();
        // Dereferenced unguarded, as shipped - and safely: Bundle's constructor seeds
        // m_bundleTransformation with &Mat4::getIdentity() and only ever repoints it at a parent
        // template's transform, so this pointer is never null. Not a missing guard.
        transform.mult(spin, *getBundleTransformation());                            // Bundle +0x138
        setRelativeTransformation(transform);                                        // +0xEC PE +0xE8
    }
    SimpleObject::handleVisualUpdate(deltaTime, distance);
}

} // namespace world
} // namespace hfe
} // namespace dice
