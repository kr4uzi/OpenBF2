// Camera template defaults and object creation.
#include "CameraTemplate.h"
#include "Base/Camera.h"
#include "../../Core/Ids.h"
#include "../../IO/Stream/IStream.h"
#include <cmath>
#include <sstream>

namespace dice {
namespace hfe {
namespace world {

// Initializes an uncached parent-control id and an unnumbered camera id.
// Inside, chase, fly-by and front-chase start enabled; trace, external trace and nose views start disabled.
CameraTemplate::CameraTemplate()
    : m_pcoId(-1)                    // +532  the uncached sentinel
    , m_hasTarget(false)             // +536
    , m_cameraId(0)                  // +540  "unnumbered" - findCameras assigns a negative id
    , m_CVMInside(true)              // +544
    , m_CVMChase(true)               // +545
    , m_CVMFlyBy(true)               // +546
    , m_CVMTrace(false)              // +547
    , m_CVMExternTrace(false)        // +548
    , m_CVMFrontChase(true)          // +549
    , m_CVMNose(false)               // +550
    , m_nosePos{0.0f, 0.0f, 0.0f}   // +552
    , m_toggleMouseLook(false)       // +564
    , m_followStiffness(1.0f)        // +568
    , m_chaseDistance(-1.0f)         // +572
    , m_chaseAngle(0.30000001f)      // +576
    , m_chaseOffset{0.0f, 0.0f, 0.0f} // +580
    // ELF C1/C2 0x81DE2F0 / 0x81DE0C0 write 1.2 literally at +0x2DC/+0x2E0; PE ctor
    // BF2.exe 0x5A3210 writes the same values at +0x38C/+0x390 (0x5A33B7/0x5A33C1). These defaults
    // matter even before content overrides them; stock SoldierCamera authors both as 1.1.
    // ELF 0x81DE1CF..0x81DE20D / BF2.exe 0x5A3395..0x5A33B1: shake and bounce defaults. Camera's
    // updateCameraShake reads the two speeds every frame; content sets them per camera (e.g. the
    // FAAV pilot camera's 20 / 30). The bounce strength and damping are left unset, as shipped.
    , m_cameraShakeSensitivity(0.1f)     // +704
    , m_cameraShakeStartSpeed(0.0f)      // +708
    , m_cameraShakeMaxSpeed(0.0f)        // +712
    , m_bounceLength(0.0f)               // +716
    , m_bounceSensitivity(1.0f)          // +720
    , m_worldFOV(1.2f)               // +732
    , m_insideFOV(1.2f)              // +736
    // Cockpits permit dynamic lights by default, but not static sunlight.
    , m_allowInsideDynamicLights(true)   // +740
    , m_allowInsideStaticSun(false)      // +741
    // ELF 0x81DE1EB..0x81DE277 / BF2.exe 0x5A33D7..0x5A343E: the inside lighting defaults.
    , m_insideStaticSunDirection{-0.25f, -0.5f, 0.75f}   // +744
    , m_insideStaticSunColor{1.0f, 1.0f, 1.0f}           // +756
    , m_insideStaticSunDamageColor{1.0f, 0.0f, 0.0f}     // +768
    , m_insideDamageFrequency(3.0f)                      // +780
    , m_insideDamageThreshold(0.15f)                     // +784
    , m_insideStaticSunAmbientColor{0.7f, 0.7f, 0.7f}    // +788
    // ELF C1 0x81DE4B2/0x81DE4BD stores -1 at +0x320/+0x324; BF2.exe ctor 0x5A3210
    // stores -1 at +0x3D0/+0x3D4 (0x5A3444/0x5A344E). Camera::changeSubGeometry uses -1
    // to select the PCO's authored cockpit values. Omitting these defaults can pass uninitialized
    // indices to SkinnedMesh::setGeomIndex, whose shipped implementation does not clamp them.
    , m_cockpitLod(-1)
    , m_cockpitSubGeom(-1)
{
}

// . Vtable +0x10.
unsigned int CameraTemplate::getClassID() const
{
    return CID_CameraTemplate;
}

// . Vtable +0x50. A tail JMP to the base, like ItemTemplate's.
void* CameraTemplate::queryImplementation(unsigned int cid)
{
    if (cid == CID_CameraTemplate)
        return this;
    return RotationalBundleTemplate::queryImplementation(cid);
}

// Creates a camera using this template.
IObject* CameraTemplate::createObject() const
{
    return new Camera(this);
}

// BF2.exe 0x005A1E50 / ELF 0x081DE520. The duplicate FOV pair and grouped shake/bounce
// omissions are shipped output; the nose-position test includes +/-0.0001 and unordered values.
bool CameraTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream out;
    if (!RotationalBundleTemplate::makeScript(stream)) return false;
    if (m_hasTarget) out << "ObjectTemplate.setHasTarget 1" << std::endl;
    if (m_toggleMouseLook) out << "ObjectTemplate.toggleMouseLook 1" << std::endl;
    if (m_cameraId) out << "ObjectTemplate.CameraId " << m_cameraId << std::endl;
    if (!m_CVMInside) out << "ObjectTemplate.CVMInside 0" << std::endl;
    if (!m_CVMChase) out << "ObjectTemplate.CVMChase 0" << std::endl;
    if (!m_CVMFrontChase) out << "ObjectTemplate.CVMFrontChase 0" << std::endl;
    if (!m_CVMFlyBy) out << "ObjectTemplate.CVMFlyBy 0" << std::endl;
    if (m_CVMTrace) out << "ObjectTemplate.CVMTrace 1" << std::endl;
    if (m_CVMExternTrace) out << "ObjectTemplate.CVMExternTrace 1" << std::endl;
    if (m_CVMNose) out << "ObjectTemplate.CVMNose 1" << std::endl;
    if (m_cameraShakeSensitivity != 0.1f)
        out << "ObjectTemplate.cameraShakeSensitivity " << m_cameraShakeSensitivity << std::endl;
    if (m_cameraShakeStartSpeed != 0.0f)
    {
        out << "ObjectTemplate.cameraShakeStartSpeed " << m_cameraShakeStartSpeed << std::endl;
        out << "ObjectTemplate.cameraShakeMaxSpeed " << m_cameraShakeMaxSpeed << std::endl;
    }
    if (m_bounceLength != 0.0f)
    {
        out << "ObjectTemplate.bounceLength " << m_bounceLength << std::endl;
        if (m_bounceSensitivity != 1.0f)
            out << "ObjectTemplate.bounceSensitivity " << m_bounceSensitivity << std::endl;
        out << "ObjectTemplate.bounceStrength " << m_bounceStrength << std::endl;
        out << "ObjectTemplate.bounceDamping " << m_bounceDamping << std::endl;
    }
    if (m_nosePos.x < -0.0001f || m_nosePos.x > 0.0001f ||
        m_nosePos.y < -0.0001f || m_nosePos.y > 0.0001f ||
        m_nosePos.z < -0.0001f || m_nosePos.z > 0.0001f)
        out << "ObjectTemplate.nosePos " << m_nosePos << std::endl;
    if (m_followStiffness != 1.0f)
        out << "ObjectTemplate.followStiffness " << m_followStiffness << std::endl;
    if (m_chaseDistance != -1.0f)
        out << "ObjectTemplate.chaseDistance " << m_chaseDistance << std::endl;
    if (m_chaseAngle != 0.3f)
        out << "ObjectTemplate.chaseAngle " << m_chaseAngle << std::endl;
    if (std::sqrt(m_chaseOffset.x * m_chaseOffset.x + m_chaseOffset.y * m_chaseOffset.y +
                  m_chaseOffset.z * m_chaseOffset.z) > 0.0f)
        out << "ObjectTemplate.chaseOffset " << m_chaseOffset << std::endl;
    out << "ObjectTemplate.worldFOV " << m_worldFOV << std::endl;
    out << "ObjectTemplate.insideFOV " << m_insideFOV << std::endl;
    out << "ObjectTemplate.worldFOV " << m_worldFOV << std::endl;
    out << "ObjectTemplate.insideFOV " << m_insideFOV << std::endl;
    out << "ObjectTemplate.allowInsideDynamicLights " << m_allowInsideDynamicLights << std::endl;
    out << "ObjectTemplate.allowInsideStaticSun " << m_allowInsideStaticSun << std::endl;
    out << "ObjectTemplate.insideStaticSunDirection " << m_insideStaticSunDirection << std::endl;
    out << "ObjectTemplate.insideStaticSunColor " << m_insideStaticSunColor << std::endl;
    out << "ObjectTemplate.insideStaticSunDamageColor " << m_insideStaticSunDamageColor << std::endl;
    out << "ObjectTemplate.insideDamageFrequency " << m_insideDamageFrequency << std::endl;
    out << "ObjectTemplate.insideDamageThreshold " << m_insideDamageThreshold << std::endl;
    out << "ObjectTemplate.insideStaticSunAmbientColor " << m_insideStaticSunAmbientColor << std::endl;
    if (m_cockpitLod != -1) out << "ObjectTemplate.cockpitLod " << m_cockpitLod << std::endl;
    if (m_cockpitSubGeom != -1) out << "ObjectTemplate.cockpitSubGeom " << m_cockpitSubGeom << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

} // namespace world
} // namespace hfe
} // namespace dice
