// Light state, attenuation, projection, and shadow queries.

#include "LightSource.h"

#include <cmath>
#include <cstdlib>
#include <string>

#include "LightSourceTemplate.h"
#include "../GameLogicComp/ArmorGLComp.h"
#include "../ObjectLayoutAccess.h"
#include "../../../Core/Ids.h"
#include "../../../Core/Debug.h"
#include "../../../Core/ClassManager.h"
#include "../../../Geom/XpLightManager.h"
#include "../../../Platform/MsvcRand.h"  // [CHOSEN, not recovered] the client CRT's rand (MSVCR71)

namespace dice {
namespace hfe {

extern ClassManager* classManager;              // ds:0x8AC85A0 - Core/Globals.cpp

namespace world {

// . Query order, dirty-bit transitions and repeated transform
// dispatch for moving parents are observable. Cone angle 2, not m_fov, drives non-projectors.
Frustum* LightSource::getSpotlightFrustum()
{
    if (m_lightDirty)
    {
        if (m_template->getLightTypeEnum() == 3)
        {
#if defined(_WIN32)
            // [PLATFORM] PE retains this template across virtual calls; ELF reloads it below.
            const LightSourceTemplate* objectTemplate = m_template;
#endif
            const bool transformed = m_transformDirty;
            const Mat4& transformation = transformed ? getAbsoluteTransformation() : Mat4::getIdentity();
#if defined(_WIN32)
            const float farDistance = objectTemplate->getProjectorFar();
            const float nearDistance = objectTemplate->getProjectorNear();
            const float aspect = objectTemplate->getProjectorAspect();
            const float fov = objectTemplate->getProjectorFov();
#else
            const float farDistance = m_template->getProjectorFar();
            const float nearDistance = m_template->getProjectorNear();
            const float aspect = m_template->getProjectorAspect();
            const float fov = m_template->getProjectorFov();
#endif
            m_frustum.setPerspective(fov, aspect, nearDistance, farDistance, transformation);
            if (transformed)
                m_transformDirty = false;
        }
        else
        {
            m_frustum.setFarDist(m_attenuationRange2);
            m_frustum.setFov(m_coneAngle2);
        }
        m_lightDirty = false;
    }
    if (m_transformDirty || (m_flags & 0x280000u))
    {
        m_frustum.transform(getAbsoluteTransformation());
        m_transformDirty = false;
    }
    return &m_frustum;
}

// Vec4 by value, including a constant fourth component.
// The explicit-mask fallback deliberately dispatches its getter three times.
Vec4 LightSource::getProjectorMask()
{
    Vec4 result = {1.0f, 1.0f, 1.0f, 1.0f};
    ArmorGLComp* armor = static_cast<ArmorGLComp*>(
        object_detail::LayoutAccess::armorComponent(getRootParent(this)));
    if (armor && m_template->getProjectorApplyMask())
    {
        result.y = armor->m_dword22C < 10.0f ? 1.0f : 0.0f;
        result.z = armor->m_dword230 < 10.0f ? 1.0f : 0.0f;
    }
    else
    {
        result.x = m_template->getProjectorExplicitMask().x;
        result.y = m_template->getProjectorExplicitMask().y;
        result.z = m_template->getProjectorExplicitMask().z;
    }
    return result;
}

namespace {
inline geom::LightManager* lightManagerSingleton()
{
    return geom::asLightManager(
        classManager->getSingleton(std::string("LightManager")));    // ClassManager +0x4C
}
} // namespace

// Copies template settings through capped setters and initializes a dirty perspective frustum.
LightSource::LightSource(const LightSourceTemplate* objectTemplate)
    : Object(objectTemplate)
{
    m_template = objectTemplate;                                 // +0x12C
    m_direction.x = m_direction.y = m_direction.z = 0.0f;        // +0x130
    m_absoluteDirection.x = m_absoluteDirection.y = m_absoluteDirection.z = 0.0f;   // +0x13C
    m_color.x = m_color.y = m_color.z = 0.0f;                    // +0x150
    m_specColor.x = m_specColor.y = m_specColor.z = 0.0f;        // +0x15C
    m_frameCounter = 0;                                          // +0x180
    m_scale.x = m_scale.y = m_scale.z = 0.0f;                    // +0x188
    m_enabled = false;                                           // +0x17C
    m_attenuationIntensity = 1.0f;                               // +0x194
    // m_frustum: Frustum() ran (member ctor)                     // +0x198
    m_lightDirty = true;                                         // +0x380
    m_lightManager = 0;                                          // +0x384
    m_transformDirty = true;                                     // +0x381
    m_absoluteDirectionDirty = true;                             // +0x382
    // (the ELF leaves +0x148/+0x14C, +0x168..+0x178, +0x184 to the setters below; the two
    // radians fields are never written on the server - zeroed here, unobservably)
    m_attenuationRange1 = m_attenuationRange2 = 0.0f;
    m_hdrIntensity = m_coneAngle1 = m_coneAngleRads1 = m_coneAngle2 = m_coneAngleRads2 = m_fov = 0.0f;

    updateFlags(0x8000000u, 0);
    setDirection(objectTemplate->getDirection());                // template +0x150
    setAttenuationRange1(objectTemplate->getAttenuationRange1()); // +0x158
    setAttenuationRange2(objectTemplate->getAttenuationRange2()); // +0x160
    setColor(objectTemplate->getColor());                        // +0x168
    setSpecColor(objectTemplate->getSpecColor());                // +0x170
    setHDRIntensity(objectTemplate->getHDRIntensity());          // +0x178
    setConeAngle1(objectTemplate->getConeAngle1());              // +0x180
    setConeAngle2(objectTemplate->getConeAngle2());              // +0x188
    setFov(objectTemplate->getFov());                            // +0x1C8
    setScale(objectTemplate->getScale());                        // +0x1EC
    m_frustum.setPerspective(1.0f, 1.0f, 0.000099999997f, 1.0f, Mat4::getIdentity());
}

// / 0x82B0550 / 0x82B0580: the two vptrs re-seated, then ~Object.
LightSource::~LightSource()
{
}

unsigned int LightSource::getClassID() const { return CID_LightSource; }

// init - 0x82B05C0, 190 B: Object::init(); the "LightManager" singleton; when there is one:
// addLight(this) (+0x8C), then the enabled state from the template - m_enabled = !getEnabled()
// so that setEnabled(getEnabled()) (this class's own virtual, +0x1CC) sees a change and does the
// full switch. Without a manager (the dedicated/no-renderer branch) the light keeps m_enabled =
// false, exactly as the symbolized ELF does.
void LightSource::init()
{
    Object::init();
    m_lightManager = lightManagerSingleton();                    // +0x384
    if (m_lightManager)
    {
        m_lightManager->addLight(this);
        const bool enabled = m_template->getEnabled();           // template +0x200
        m_enabled = !enabled;                                    // +0x17C
        setEnabled(enabled);                                     // vslot +0x1CC
    }
}

// unless already destroyed - disableLight (+0x98) if it was on, then removeLight
// (+0x90); then Object::destroy.
void LightSource::destroy(bool force)
{
    if (!isObjectDestroyed())
    {
        if (m_lightManager)
        {
            if (m_enabled)
                m_lightManager->disableLight(this);
            m_lightManager->removeLight(this);
        }
    }
    Object::destroy(force);
}

bool LightSource::makeScript(io::IStream* stream) { return Object::makeScript(stream); }

// handleMessage - 0x82B07D0, 181 B. By the template's affectingType (+0x148):
// 1        : message 0 -> setEnabled(true) and the frame counter reset; message 1 -> off
// 2 or 4   : message 4 -> on; message 5 -> off
// otherwise: message 18 -> toggle (setEnabled(!getEnabled()))
void LightSource::handleMessage(TemplateMessage message, IPlayer*, bool)
{
    const int affectingType = m_template->getAffectingType();
    const int m = static_cast<int>(message);
    if (affectingType == 1)
    {
        if (m == 0)
        {
            setEnabled(true);                                    // vslot +0x1CC
            m_frameCounter = 0;                                  // +0x180
        }
        else if (m == 1)
            setEnabled(false);
        return;
    }
    if (affectingType == 2 || affectingType == 4)
    {
        if (m == 4)
            setEnabled(true);
        else if (m == 5)
            setEnabled(false);
        return;
    }
    if (m == 18)
        setEnabled(!getEnabled());                               // +0x1D0 then +0x1CC
}

// handleFrameUpdate - 0x82B08A0, 251 B. The template's flicker (+0x1F8) > 0: a rand() in 0..1
// (rand() * (1.0f / RAND_MAX) - see the [PLATFORM] note in the body) scales both attenuation
// ranges: templateRange * ((r - 1) * flicker + 1) - a
// random dip of up to `flicker`; then both ranges are multiplied by the attenuation intensity.
// An affectingType-1 light that is on counts frames and turns itself off after the second.
void LightSource::handleFrameUpdate(float)
{
    const float flicker = m_template->getFlicker();              // template +0x1F8
    if (flicker > 0.0f)
    {
        // [PLATFORM] Scale by the CRT generator's RAND_MAX (bf2crt) to preserve the full [0, 1] flicker range.
        const float r = static_cast<float>(bf2crt::rand()) * (1.0f / bf2crt::kRandMax);
        m_attenuationRange1 = m_template->getAttenuationRange1() * ((r - 1.0f) * flicker + 1.0f);   // +0x148
        m_attenuationRange2 = m_template->getAttenuationRange2() * ((r - 1.0f) * flicker + 1.0f);   // +0x14C
    }
    const float scaledRange2 = m_attenuationIntensity * m_attenuationRange2;
    m_attenuationRange1 = m_attenuationRange1 * m_attenuationIntensity;
    m_attenuationRange2 = scaledRange2;
    if (m_template->getAffectingType() == 1 && m_enabled)
    {
        if (++m_frameCounter > 1)                                // +0x180
            setEnabled(false);                                   // vslot +0x1CC
    }
}

// 0 for a flag-bit-0 object, else attenuation range 2 (this class's +0x198).
float LightSource::getVisualBoundingRadius() const
{
    if ((m_flags & 1u) != 0)
        return 0.0f;
    return getAttenuationRange2();
}

// (31 B) / 0x82B15B0 / 0x82B15E0 (48 B each): the transformation accessors, with the
// two dirty flags raised on every write (and on a read while flags 0x280000 - a moving parent).
const Mat4& LightSource::getAbsoluteTransformation() const
{
    if ((m_flags & 0x280000u) != 0)
        const_cast<LightSource*>(this)->m_transformDirty = true;   // +0x381
    return Object::getAbsoluteTransformation();
}

void LightSource::setAbsoluteTransformation(const Mat4& m)
{
    Object::setAbsoluteTransformation(m);
    m_transformDirty = true;                                     // +0x381
    m_absoluteDirectionDirty = true;                             // +0x382
}

void LightSource::setRelativeTransformation(const Mat4& m)
{
    Object::setRelativeTransformation(m);
    m_transformDirty = true;
    m_absoluteDirectionDirty = true;
}

// setDirection - 0x82B09D0, 230 B: the store, then normalized IN place (|d|^2 within eps of 1 ->
// left alone; degenerate -> zero); the light and absolute-direction dirty flags raised.
void LightSource::setDirection(const Vec3& direction)
{
    m_direction = direction;                                     // +0x130
    const float lengthSq = direction.z * direction.z + direction.y * direction.y + direction.x * direction.x;
    if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-07f)
    {
        if (std::fabs(lengthSq) >= 1.1920929e-07f)
        {
            const float inv = 1.0f / std::sqrt(lengthSq);
            m_direction.x *= inv;
            m_direction.y *= inv;
            m_direction.z *= inv;
        }
        else
        {
            m_direction.x = m_direction.y = m_direction.z = 0.0f;
        }
    }
    m_lightDirty = true;                                         // +0x380
    m_absoluteDirectionDirty = true;                             // +0x382
}

const Vec3& LightSource::getDirection() const { return m_direction; }

// getAbsoluteDirection - 0x82B0AD0, 376 B: cached unless dirty or flags 0x280000 (a moving
// parent): the direction rotated by the absolute transformation's 3x3 (row-vector form),
// normalized in place the same way, dirty cleared.
const Vec3& LightSource::getAbsoluteDirection() const
{
    if (!m_absoluteDirectionDirty && (m_flags & 0x280000u) == 0)
        return m_absoluteDirection;
    LightSource* self = const_cast<LightSource*>(this);
    self->m_absoluteDirection = m_direction;
    const Mat4& m = getAbsoluteTransformation();                 // vslot +0x84
    const float x = m_absoluteDirection.x, y = m_absoluteDirection.y, z = m_absoluteDirection.z;
    const float rx = m.m[4] * y + m.m[0] * x + m.m[8] * z;
    const float ry = m.m[9] * z + (m.m[5] * y + m.m[1] * x);
    const float rz = z * m.m[10] + (y * m.m[6] + x * m.m[2]);
    self->m_absoluteDirection.x = rx;
    self->m_absoluteDirection.y = ry;
    self->m_absoluteDirection.z = rz;
    const float lengthSq = ry * ry + rx * rx + rz * rz;
    if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-07f)
    {
        if (std::fabs(lengthSq) >= 1.1920929e-07f)
        {
            const float inv = 1.0f / std::sqrt(lengthSq);
            self->m_absoluteDirection.x *= inv;
            self->m_absoluteDirection.y *= inv;
            self->m_absoluteDirection.z *= inv;
        }
        else
        {
            self->m_absoluteDirection.x = self->m_absoluteDirection.y = self->m_absoluteDirection.z = 0.0f;
        }
    }
    self->m_absoluteDirectionDirty = false;                     // +0x382
    return m_absoluteDirection;
}

// The three capped setters - 350 B each: over the cap, a type-6 Debug ("Light " << name << ...)
// with dice's own __LINE__ (271 / 288 / 335 / 360 / 387; the 4th argument is the .rodata empty
// string) and the cap stored instead. Debug type 6 aborts as shipped (Core/Debug.h) - a light
// beyond the caps is a content error the server dies on. setAttenuationRange2 (0x82B0DC0, 353 B,
// " attenuation2 >100, capped to 100.", line 288) and setConeAngle2 (0x82B1190, 353 B,
// " coneAngle2 >180, capped to 180.", line 360) are the same shape - all five texts read.
void LightSource::setAttenuationRange1(float range)
{
    if (range > 100.0f)
    {
        const std::string& name = getTemplate()->getName();     // IObject +0x14 -> template +0x2C
        Debug(DEBUGTYPE_6, "Game/Objects/Visual/LightSource.cpp", 271)
            << "Light " << name << " attenuation1 >100, capped to 100.";
        range = 100.0f;
    }
    m_attenuationRange1 = range;                                 // +0x148
}

void LightSource::setAttenuationRange2(float range)
{
    if (range > 100.0f)
    {
        const std::string& name = getTemplate()->getName();
        Debug(DEBUGTYPE_6, "Game/Objects/Visual/LightSource.cpp", 288)
            << "Light " << name << " attenuation2 >100, capped to 100.";
        range = 100.0f;
    }
    m_attenuationRange2 = range;                                 // +0x14C
}

float LightSource::getAttenuationRange1() const { return m_attenuationRange1; }
float LightSource::getAttenuationRange2() const { return m_attenuationRange2; }

void LightSource::setColor(const Vec3& color)         { m_color = color; }
const Vec3& LightSource::getColor() const             { return m_color; }
void LightSource::setSpecColor(const Vec3& color)     { m_specColor = color; }
const Vec3& LightSource::getSpecColor() const         { return m_specColor; }
void LightSource::setHDRIntensity(float intensity)    { m_hdrIntensity = intensity; }
float LightSource::getHDRIntensity() const            { return m_hdrIntensity; }

void LightSource::setConeAngle1(float degrees)
{
    if (degrees > 180.0f)
    {
        const std::string& name = getTemplate()->getName();
        Debug(DEBUGTYPE_6, "Game/Objects/Visual/LightSource.cpp", 335)
            << "Light " << name << " coneAngle1>180, capped to 180.";
        degrees = 180.0f;
    }
    m_coneAngle1 = degrees;                                      // +0x16C
}
float LightSource::getConeAngle1() const     { return m_coneAngle1; }
float LightSource::getConeAngleRads1() const { return m_coneAngleRads1; }    // +0x170

void LightSource::setConeAngle2(float degrees)
{
    if (degrees > 180.0f)
    {
        const std::string& name = getTemplate()->getName();
        Debug(DEBUGTYPE_6, "Game/Objects/Visual/LightSource.cpp", 360)
            << "Light " << name << " coneAngle2 >180, capped to 180.";
        degrees = 180.0f;
    }
    m_coneAngle2 = degrees;                                      // +0x174
}
float LightSource::getConeAngle2() const     { return m_coneAngle2; }
float LightSource::getConeAngleRads2() const { return m_coneAngleRads2; }    // +0x178

// setEnabled - 0x82B1490, 229 B. No-op when unchanged. ON: object flags += 0x2000 (with bit 0
// cleared) when the template's affectingType (+0x148) is non-zero or its flicker (+0x1F8) > 0,
// else flags cleared of bit 0 only; the manager told (+0x94). OFF: bit 0 set and 0x2000 cleared;
// the manager told (+0x98). Then the store.
void LightSource::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    if (enabled)
    {
        if (m_template->getAffectingType() != 0 || m_template->getFlicker() > 0.0f)
            updateFlags(0x2000u, 1u);                            // +0x60
        else
            updateFlags(0u, 1u);
        if (m_lightManager)
            m_lightManager->enableLight(this);
    }
    else
    {
        updateFlags(1u, 0x2000u);
        if (m_lightManager)
            m_lightManager->disableLight(this);
    }
    m_enabled = enabled;                                         // +0x17C
}

bool LightSource::getEnabled() const { return m_enabled; }

void LightSource::setFov(float degrees)
{
    if (degrees > 180.0f)
    {
        const std::string& name = getTemplate()->getName();
        Debug(DEBUGTYPE_6, "Game/Objects/Visual/LightSource.cpp", 387)
            << "Light " << name << " fov >180, capped to 180.";
        degrees = 180.0f;
    }
    m_fov = degrees;                                             // +0x184
}
float LightSource::getFov() const { return m_fov; }

const Vec3& LightSource::getScale() const { return m_scale; }
void LightSource::setScale(const Vec3& scale)
{
    m_scale = scale;                                             // +0x188
    m_lightDirty = true;                                         // +0x380
}

// the "LightManager" singleton's +0x24 with this light - the sky light.
void LightSource::setAsSkyLight()
{
    geom::LightManager* manager = lightManagerSingleton();
    if (manager)
        manager->setSkyLight(this);
}


void LightSource::setAttenuationIntensity(float intensity)
{
    if (intensity < 0.0f)      intensity = 0.0f;
    else if (intensity > 1.0f) intensity = 1.0f;
    m_attenuationIntensity = intensity;                          // +0x194
    m_lightDirty = true;                                         // +0x380
}
float LightSource::getAttenuationIntensity() const { return m_attenuationIntensity; }

// Copies light settings and frustum, retaining this object's template, frame state, and dirty flags.
void LightSource::copySettings(const LightSource& other)
{
    m_direction            = other.m_direction;              // +0x130
    m_absoluteDirection    = other.m_absoluteDirection;      // +0x13C
    m_attenuationRange1    = other.m_attenuationRange1;      // +0x148
    m_attenuationRange2    = other.m_attenuationRange2;      // +0x14C
    m_color                = other.m_color;                  // +0x150
    m_specColor            = other.m_specColor;              // +0x15C
    m_hdrIntensity         = other.m_hdrIntensity;           // +0x168
    m_coneAngle1           = other.m_coneAngle1;             // +0x16C
    m_coneAngleRads1       = other.m_coneAngleRads1;         // +0x170
    m_coneAngle2           = other.m_coneAngle2;             // +0x174
    m_coneAngleRads2       = other.m_coneAngleRads2;         // +0x178
    m_enabled              = other.m_enabled;                // +0x17C (a byte move)
    m_frameCounter         = other.m_frameCounter;           // +0x180
    m_fov                  = other.m_fov;                    // +0x184
    m_scale                = other.m_scale;                  // +0x188
    m_attenuationIntensity = other.m_attenuationIntensity;   // +0x194
    m_frustum              = other.m_frustum;                // +0x198 Frustum::operator=
}

} // namespace world
} // namespace hfe
} // namespace dice
