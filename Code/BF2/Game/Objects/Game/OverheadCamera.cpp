// Commander overhead-camera movement and view state.

#include "OverheadCamera.h"

#include "../../../Core/Ids.h"
#include "../../IGameLogic.h"                     // gameLogic->isServer
#include "../../IGame.h"                          // game (requestNewPosition's server test)
#include "../../../Physics/HeightmapCluster.h"    // IHeightmapCluster - pan's terrain queries
#include "../../../Physics/Heightmap.h"           // IHeightmap::getSize
#include "../../../Scene/Object/ObjectUtil.h"     // setNetworkableComp
#include "../../../IO/Input/PlayerInput.h"         // mapped yaw/throttle channels
#include "../../GameEvents/CommanderCamEvent.h"   // requestNewPosition's client message
#include "../../Utility/Vars.h"                    // overheadCamera-acceleration

namespace dice {
namespace hfe {
namespace world {

extern IBase* heightmapCluster;                   // ds:0x8ABAF24, Core/Globals.cpp - live at boot:
                                                  // PhysicsModule registers the singleton

// the symbol survives (_ZN...OverheadCamera9m_enabledE). The initialized .data byte is
// 1 in both shipped images (ELF 0x89882E4, PE 0x832804). Each image has exactly one code writer:
// OverheadCameraNetworkable::setNetUpdate (ELF 0x823CA84 / PE 0x5E5D49).
bool OverheadCamera::m_enabled = true;

namespace {
float* const g_overheadCameraAcceleration =
    hfe::Vars::getFloat("overheadCamera-acceleration", 250.0f);
}


// Terrain and water queries use typed heightmap interfaces because virtual layouts differ by platform.

// The constructor - C1 0x81E7FE0, 594 B, read in full (C2 0x81E7CE0 is 767 B - the delta is
// inlined-container expansion, same stores). Real std members carry most of it; the scalars are
// below, then Object::updateFlags(0x808, 0) closes it.
OverheadCamera::OverheadCamera(const OverheadCameraTemplate* objectTemplate)
    : Bundle(objectTemplate)
{
    m_refCount = 1;                              // +0x140
    m_isActive = false;                          // +0x15C
    m_cameraId = 0;                              // +0x160
    _float184 = -1.0f;                           // +0x184
    _bool188 = true;                             // +0x188
    _dword18C = 0;
    _float190[0] = 0.0f; _float190[1] = 0.0f;
    _dword198 = -1;
    m_transformation = Mat4::getIdentity();      // +0x19C the ctor's diagonal writes
    m_traceObjectId = static_cast<unsigned int>(-1);   // +0x1DC
    _bool1E0 = true;
    _dword1E4 = 0; _dword1E8 = 0; _dword1EC = 0;
    _float1F0 = 1.0f;
    _bool1F4 = false;
    m_speedScale = 1.0f;                         // +0x1F8
    m_panState[0] = m_panState[1] = m_panState[2] = 0.0f;   // +0x1FC
    m_defaultTeam = 0;                           // +0x20C
    m_viewMode = 3;                              // +0x180; raw ctor 0x81E8208..0x81E8213

    Object::updateFlags(0x808, 0);
}

// in the ELF - all container teardown (two strings, two lists, the camera map, two
// vectors) plus vptr restores. Real std members do it here.
OverheadCamera::~OverheadCamera() {}

void OverheadCamera::addRef()  { ++m_refCount; }
int  OverheadCamera::getRef()  { return m_refCount; }
int  OverheadCamera::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}

// four cases, and IID_IObject is accepted (the siblings' quartets stop at IBase).
void* OverheadCamera::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == world::IID_IObject)
        return const_cast<OverheadCamera*>(this);
    if (iid == world::IID_ICameraObject)
        return static_cast<ICameraObject*>(const_cast<OverheadCamera*>(this));   // the +0x13C sub-object
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<OverheadCamera*>(this));   // +0x80
    return 0;
}

// Initializes Bundle, registers camera 0, then snaps toward the terrain when a heightmap exists.
void OverheadCamera::init()
{
    Bundle::init();
    m_cameras.insert(std::make_pair(0,
        static_cast<ICameraObject*>(queryInterface(world::IID_ICameraObject))));
    pan(0.0f, 0.0f, 0.001f);
}

// the one destroy in the family with a re-entry gate.
void OverheadCamera::destroy(bool force)
{
    if (!isObjectDestroyed())
        SimpleObject::destroy(force);
}

// the sibling shape minus the Debug arm: a failed networkable build is silent here.
void OverheadCamera::initNetworkableComp()
{
    const OverheadCameraTemplate* templ = static_cast<const OverheadCameraTemplate*>(m_template);
    if (templ && templ->getNetworkableInfo().compare("") != 0)
        setNetworkableComp(this, CID_OverheadCameraNetworkable);
}

// input flows only on the server (or with no gameLogic at all), and only to a root camera.
void OverheadCamera::handlePlayerInput(IPlayer* player, const io::PlayerInput& input,
                                       float a, unsigned int b)
{
    if ((!gameLogic || gameLogic->isServer()) && !getParent())
        handlePlayerInputDefault(player, input, a, b);
}

// Accelerates and decelerates pan velocity from clamped yaw and throttle input, then pans while moving.
// Server-only; paused game state admits input even when the camera is disabled. Requires GameLogic.
void OverheadCamera::handlePlayerInputDefault(IPlayer*, const io::PlayerInput& input,
                                              float deltaTime, unsigned int)
{
    if ((gameLogic && !gameLogic->isServer())
        || (!m_enabled && gameLogic->getGameStatus() != GameStatus_Paused))
        return;

    float yaw = 0.0f;
    if ((input.mappingFlags & (1ULL << io::PIYaw)) != 0)
        yaw = input.inputs[io::PIYaw];
    if (yaw < -1.0f)
        yaw = -1.0f;
    else if (yaw > 1.0f)
        yaw = 1.0f;

    float throttle = 0.0f;
    if ((input.mappingFlags & (1ULL << io::PIThrottle)) != 0)
        throttle = input.inputs[io::PIThrottle];
    if (throttle < -1.0f)
        throttle = -1.0f;
    else if (throttle > 1.0f)
        throttle = 1.0f;

    const float maxSpeed = getMaxSpeed();
    const float accelerationStep = deltaTime * *g_overheadCameraAcceleration;

    if (yaw == 0.0f)
    {
        if (m_panState[0] > 0.0f)
        {
            m_panState[0] -= accelerationStep;
            if (m_panState[0] <= 0.0f)
                m_panState[0] = 0.0f;
        }
        else if (m_panState[0] < 0.0f)
        {
            m_panState[0] += accelerationStep;
            if (m_panState[0] > 0.0f)
                m_panState[0] = 0.0f;
        }
    }
    else
    {
        m_panState[0] += yaw * accelerationStep;
        if (m_panState[0] < -maxSpeed)
            m_panState[0] = -maxSpeed;
        else if (m_panState[0] > maxSpeed)
            m_panState[0] = maxSpeed;
    }

    m_panState[1] = 0.0f;

    if (throttle == 0.0f)
    {
        if (m_panState[2] > 0.0f)
        {
            m_panState[2] -= accelerationStep;
            if (m_panState[2] <= 0.0f)
                m_panState[2] = 0.0f;
        }
        else if (m_panState[2] < 0.0f)
        {
            m_panState[2] += accelerationStep;
            if (m_panState[2] > 0.0f)
                m_panState[2] = 0.0f;
        }
    }
    else
    {
        m_panState[2] += throttle * accelerationStep;
        if (m_panState[2] < -maxSpeed)
            m_panState[2] = -maxSpeed;
        else if (m_panState[2] > maxSpeed)
            m_panState[2] = maxSpeed;
    }

    if (m_panState[0] != 0.0f || m_panState[2] != 0.0f)
        pan(m_panState[0], m_panState[2], deltaTime);
}

// Moves the overhead camera above averaged terrain and water height within map borders.
// Negative dt resets pan state; the resulting camera transform faces straight down.
void OverheadCamera::pan(float dx, float dz, float dt)
{
    if (gameLogic && !gameLogic->isServer())
        return;

    const OverheadCameraTemplate* templ = static_cast<const OverheadCameraTemplate*>(m_template);
    IObject* root = const_cast<IObject*>(getRootParent(this));
    const Vec3& rootPos = root->getAbsolutePosition();
    float x = rootPos.x, y = rootPos.y, z = rootPos.z;

    if (dt < 0.0f)
    {
        m_panState[0] = 0.0f;
        m_panState[1] = 0.0f;
        m_panState[2] = 0.0f;
    }
    else
    {
        x += dx * dt;
        z += dz * dt;
    }

    IHeightmapCluster* cluster = reinterpret_cast<IHeightmapCluster*>(heightmapCluster);
    IHeightmap* heightmap = cluster->getPrimaryHeightmap();         // cluster itself unchecked,
    if (heightmap)                                                  // as shipped
    {
        int dimX = 0, dimZ = 0;
        heightmap->getSize(dimX, dimZ);
        const float limX = static_cast<float>(dimX) - 80.0f;
        if (-limX > x)      x = -limX;
        else if (x > limX)  x = limX;
        const float limZ = static_cast<float>(dimZ) - 80.0f;
        if (-limZ > z)      z = -limZ;
        else if (z > limZ)  z = limZ;

        Vec3 probe;
        float sum;
        probe.x = x; probe.y = 0.0f; probe.z = z + 2.0f;
        sum  = cluster->getHeightInWorldCoords(probe);
        probe.z = z - 2.0f;
        sum += cluster->getHeightInWorldCoords(probe);
        probe.x = x + 2.0f; probe.z = z;
        sum += cluster->getHeightInWorldCoords(probe);
        probe.x = x - 2.0f;
        sum += cluster->getHeightInWorldCoords(probe);
        probe.x = x;
        sum += cluster->getHeightInWorldCoords(probe);
        y = sum / 5.0f;

        probe.x = x; probe.y = 0.0f; probe.z = z;
        const float waterLevel = cluster->getWaterLevelInWorldCoords(probe);
        if (waterLevel > y)
            y = waterLevel;                                         // never sink below the sea
        y += templ->m_height;                                       // template +0x188
    }

    Mat4 m = Mat4::getIdentity();
    float* f = reinterpret_cast<float*>(&m);
    // BF2.exe 0x6264A3..0x6264D4; ELF 0x081E8631..0x081E86DB:
    // right=(1,0,0), up=(0,0,1), forward=(0,-1,0).
    f[5] = 0.0f; f[6] = 1.0f;
    f[8] = 0.0f; f[9] = -1.0f; f[10] = 0.0f;                        // straight down
    f[12] = x; f[13] = y; f[14] = z; f[15] = 1.0f;
    root->setAbsoluteTransformation(m);
}

// server gate, teleport the root, reset pan state via the dt<0 arm.
bool OverheadCamera::forceSetPosition(const Vec3& pos)
{
    if (gameLogic && !gameLogic->isServer())
        return false;
    IObject* root = const_cast<IObject*>(getRootParent(this));
    root->setAbsolutePosition(pos);
    pan(0.0f, 0.0f, -1.0f);
    return true;
}

// . A server applies the move immediately; a client sends one
// CommanderCamEvent to the server pseudo-recipient (-1). Every path that gets past the initial
// queryInterface call returns true. A null `game` does NOT reach that return: retail dereferences
// it for queryInterface before the later client-arm null test.
bool OverheadCamera::requestNewPosition(const Vec3& pos)
{
    // Retail calls queryInterface before its later `game` guard, so this dereference is
    // intentionally unguarded.
    if (game->queryInterface(IID_IGameServer))
    {
        forceSetPosition(pos);                         // result deliberately ignored
    }
    else if (game)
    {
        game->sendGameEvent(-1, new CommanderCamEvent(pos));
    }
    return true;
}

float OverheadCamera::getMaxSpeed() const
{ return m_speedScale * static_cast<const OverheadCameraTemplate*>(m_template)->m_speed; }

// the camera surface, slots 96..133 - tiny bodies, all read
const Mat4& OverheadCamera::getTransformation(float) { return getAbsoluteTransformation(); }
void OverheadCamera::setInterpolatedMode(bool) {}
bool OverheadCamera::setViewMode(CameraViewMode, IObject*, bool) { return true; }
CameraViewMode OverheadCamera::getViewMode() const { return static_cast<CameraViewMode>(m_viewMode); }
bool OverheadCamera::isActive() const { return m_isActive; }
void OverheadCamera::setIsActive(bool active) { m_isActive = active; }
void OverheadCamera::setCameraId(int id) { m_cameraId = id; }
int  OverheadCamera::getCameraId() { return m_cameraId; }
void OverheadCamera::resetCameraView(bool) {}
void OverheadCamera::updateCameraView(bool b) { resetCameraView(b); }   // vcall slot 104, as shipped
void OverheadCamera::prevViewMode() {}
std::map<int, ICameraObject*>* OverheadCamera::getCameras()    { return &m_cameras; }
std::map<int, ICameraObject*>* OverheadCamera::getOwnCameras() { return &m_cameras; }
void OverheadCamera::nextCamera() {}
ICameraObject* OverheadCamera::getActiveCamera()
{ return static_cast<ICameraObject*>(this); }                        // the +0x13C sub-object
void OverheadCamera::setActiveCamera(int) {}
void OverheadCamera::setTraceObjectId(unsigned int id) { m_traceObjectId = id; }
unsigned int OverheadCamera::getTraceObjectId() { return m_traceObjectId; }
void OverheadCamera::setExternCameraTrans(Mat4) {}
Mat4* OverheadCamera::getExternCameraTrans() { return 0; }
void OverheadCamera::setTeam(int team)        { m_defaultTeam = team; }   // the alias pair -
void OverheadCamera::setDefaultTeam(int team) { m_defaultTeam = team; }   // both write one field
int  OverheadCamera::getTeam()        { return m_defaultTeam; }
int  OverheadCamera::getDefaultTeam() { return m_defaultTeam; }
bool OverheadCamera::getToggleMouseLook() { return false; }
void OverheadCamera::setMenuVisibleForTeam(unsigned char) {}
unsigned char OverheadCamera::getMenuVisibleForTeam() { return 0; }
int  OverheadCamera::getPcoId() { return 0; }
void OverheadCamera::handleExplosion(float) {}
float OverheadCamera::getShakeFactor() const { return 0.0f; }
bool OverheadCamera::getIsUnderwater() const { return false; }
float OverheadCamera::getNormalizedUnderwaterTime() const { return 0.0f; }
float OverheadCamera::getActiveFOV() const
{
    const OverheadCameraTemplate* t = static_cast<const OverheadCameraTemplate*>(m_template);
    return t ? t->m_activeFOV : 2.3f;
}
const Vec3& OverheadCamera::getStaticSunDirection() const    { return Vec3::getIdentity(); }
const Vec3& OverheadCamera::getStaticSunColor() const        { return Vec3::getIdentity(); }
const Vec3& OverheadCamera::getStaticSunDamageColor() const  { return Vec3::getIdentity(); }
const Vec3& OverheadCamera::getStaticSunAmbientColor() const { return Vec3::getIdentity(); }
bool OverheadCamera::isOverheadCamera() const { return true; }

// The commander-camera motion path is complete: local input accelerates pan, requestNewPosition
// sends CommanderCamEvent, and executeServer calls forceSetPosition.

} // namespace world
} // namespace hfe
} // namespace dice
