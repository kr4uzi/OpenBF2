// [INFERRED] path: see header. ELF 812C030..812CD34 / dedicated PE 620780..62113E.
// All lifecycle, immediate/persistent/update instructions and both vtables read in full.
#include "ExplosionEffectFlashBang.h"
#if defined(BF2_STATIC_PLUGINS) && !defined(_MSC_VER)
// [CHOSEN, not recovered] This native provider is an explicit PE-order table.
#include "../../../RendMisc/NativePostProductionSlots.h"
#endif
#include "PersistentExplosionEffectManager.h"
#include "../IPlayer.h"
#include "../ICameraObject.h"
#include "../IObjectTemplate.h"
#include "../Game/PlayerControlObject.h"
#include "../Game/Soldier.h"
#include "../Projectiles/GenericProjectile.h"
#include "../../../Core/Ids.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectPredicator.h"
#if !DEDICATED
#include "../../Main/BF2ClientSeams.h"
#endif
#include <cfloat>
#include <cmath>

namespace dice { namespace hfe { namespace world {
namespace {
// [CHOSEN source grouping] These records name the seven settings and seven clocks
// per channel. Retail emits separate named statics, not per-effect instance members.
struct FadeSettings { float minAlpha, maxAlpha, ramp, minHold, maxHold, minHeal, maxHeal; };
struct FadeState { float maxAlpha, curRamp, maxRamp, curHold, maxHold, curHeal, maxHeal; };
FadeSettings billboardSettings = {0.5f, 1.0f, 0.12f, 2.0f, 6.0f, 2.0f, 6.0f};
FadeSettings glowSettings = {0.5f, 1.0f, 0.02f, 1.0f, 5.0f, 1.0f, 5.0f};
FadeSettings blurSettings = {0.96f, 1.0f, 0.02f, 2.0f, 10.0f, 2.0f, 10.0f};
FadeState billboardState = {}, glowState = {}, blurState = {};
float glowAlpha = 0.0f, blurAlpha = 0.0f;
CExplosionEffectFlashBang* owner = 0; // ELF 8AACB50 / PE 892768, native pointer

#if defined(_MSC_VER)
#define FLASH_VCALL __thiscall
#else
#define FLASH_VCALL
#endif

// Narrow caller-shaped adapters. The post-production published subobject is not IBase.
// PE +44/+48 take float; +4C/+50 take bool. ELF has an extra destructor slot,
// hence +48/+4C/+50/+54. Native MSVC uses logical ordinals, never byte/sizeof(pointer).
template<class Function> Function postSlot(void* object, unsigned int peOrdinal)
{
#if defined(_MSC_VER)
    const unsigned int ordinal = peOrdinal;
#elif defined(BF2_STATIC_PLUGINS)
    // [CHOSEN, not recovered] No compiler-generated destructor is inserted here.
    const unsigned int ordinal = dx9rend::native_post::slot(peOrdinal);
#else
    const unsigned int ordinal = peOrdinal + 1;
#endif
    return reinterpret_cast<Function>((*static_cast<void***>(object))[ordinal]);
}
void setGlow(void* post, float value)
{ postSlot<void (FLASH_VCALL*)(void*, float)>(post, 17)(post, value); }
void setBlur(void* post, float value)
{ postSlot<void (FLASH_VCALL*)(void*, float)>(post, 18)(post, value); }
void setEnabled(void* post, bool value)
{ postSlot<void (FLASH_VCALL*)(void*, bool)>(post, 19)(post, value); }
void setCapture(void* post, bool value)
{ postSlot<void (FLASH_VCALL*)(void*, bool)>(post, 20)(post, value); }

void* postProduction()
{
#if DEDICATED
    // BOTH dedicated images literally use a null BF2Render receiver: ELF 812C117
    // loads [0], passes this=0 and calls +38; PE 620821 does the same at +34.
    // No dedicated renderer exists. Preserve this unreachable-in-normal-server
    // client presentation path, including its missing guard, rather than return a fake owner.
    void* const renderer = 0;
    return postSlot<void* (FLASH_VCALL*)(void*)>(renderer, 13)(renderer);
#else
    return g_bf2Render->getPostProductionFrameWork();
#endif
}

// PE 620780/6207C0 clamp with ordered comparisons; NaN is returned unchanged.
float clampUnit(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}
enum FadeStep { FadeChanged, FadeHolding, FadeFinished };
FadeStep updateChannel(FadeState& state, float& alpha, float deltaTime)
{
    if (state.curRamp < state.maxRamp) {
        state.curRamp += deltaTime;
        alpha = clampUnit(state.curRamp / state.maxRamp) * state.maxAlpha;
    } else if (state.curHold < state.maxHold) {
        state.curHold += deltaTime;
        return FadeHolding;
    } else if (state.curHeal < state.maxHeal) {
        state.curHeal += deltaTime;
        alpha = clampUnit(1.0f - state.curHeal / state.maxHeal) * state.maxAlpha;
    } else {
        return FadeFinished;
    }
    return FadeChanged;
}
bool strengthenChannel(FadeState& state, float alpha, const FadeSettings& settings,
                       float strength, float complement)
{
    const float maximum = complement * settings.minAlpha + strength * settings.maxAlpha;
    if (!(maximum > alpha)) return false;
    state.maxAlpha = maximum;
    state.maxRamp = settings.ramp;
    state.curHold = 0.0f;
    state.curHeal = 0.0f;
    state.curRamp = alpha / maximum * settings.ramp;
    state.maxHold = complement * settings.minHold + strength * settings.maxHold;
    state.maxHeal = complement * settings.minHeal + strength * settings.maxHeal;
    return true;
}
void normalise(Vec3& v)
{
    const float squared = v.x*v.x + v.y*v.y + v.z*v.z;
    if (std::fabs(squared - 1.0f) < FLT_EPSILON) return;
    if (std::fabs(squared) < FLT_EPSILON) {
        v = Vec3{0.0f, 0.0f, 0.0f};
    } else {
        const float inverse = 1.0f / std::sqrt(squared);
        v.x *= inverse; v.y *= inverse; v.z *= inverse;
    }
}
} // anonymous namespace

CExplosionEffectFlashBang::CExplosionEffectFlashBang(
        const Vec3& position, const IProjectileObject*) : m_position{0.0f, 0.0f, 0.0f}
{
    m_position = position;
}

CExplosionEffectFlashBang::~CExplosionEffectFlashBang()
{
    if (owner == this) {
        void* const post = postProduction();
        blurAlpha = 0.0f;
        glowAlpha = 0.0f;
        blindingLightDamageAlpha = 0.0f;
        setEnabled(post, false);
        blindingLightDamageShow = false;
        owner = 0;
    }
}

// ELF 812CD30 / PE 620B00 are empty; FlashBang affects only the immediate player.
void CExplosionEffectFlashBang::handlePersistentEffect(IPlayer*) {}

bool CExplosionEffectFlashBang::updateAndCull(float deltaTime)
{
    if (owner != this) return false;
    void* const post = postProduction();
    const FadeStep billboard = updateChannel(billboardState, blindingLightDamageAlpha, deltaTime);
    const FadeStep glow = updateChannel(glowState, glowAlpha, deltaTime);
    if (glow == FadeChanged) setGlow(post, glowAlpha);
    const FadeStep blur = updateChannel(blurState, blurAlpha, deltaTime);
    if (blur == FadeChanged) setBlur(post, blurAlpha);
    return billboard != FadeFinished || glow != FadeFinished || blur != FadeFinished;
}

void CExplosionEffectFlashBang::handleImmediateEffect(
        IPlayer* player, const IProjectileObject* projectileFace)
{
    // IProjectileObject's reconstructed face is intentionally minimal. All retail
    // queried implementations are GenericProjectile; its real virtual getters keep
    // dispatch after the compiler performs the native secondary-base adjustment.
    const GenericProjectile* const projectile = static_cast<const GenericProjectile*>(projectileFace);
    IObject* const vehicle = player->getVehicle();
    if (!vehicle || !player->getIsAlive()) return;
    ICameraObject* camera = 0;
    Mat4 transform = {{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};
    if (vehicle->getTemplate()->getClassID() == CID_PlayerControlObjectTemplate)
        camera = static_cast<PlayerControlObject*>(vehicle)->getNonCommanderCamera();
    if (vehicle->getTemplate()->getClassID() == CID_SoldierTemplate)
        camera = static_cast<Soldier*>(vehicle)->getSoldierCamera();
    transform = camera ? camera->getTransformation(0.0f) : vehicle->getAbsoluteTransformation();
    Vec3 direction = {m_position.x-transform.m[12], m_position.y-transform.m[13],
                      m_position.z-transform.m[14]};
    Vec3 hitPoint = {0,0,0}, hitNormal = {0,0,0};
    IObject* hitObject = 0;
    int hitPart;
    ObjectFlagPredicator predicator(0x102u);
    const Vec3 ray = {-direction.x, -direction.y, -direction.z};
    objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, m_position,
                                ray, &predicator, true, false, false, true, 0);
    if (hitObject && hitObject != vehicle) return;

    // [PLATFORM] PE 620C97 sums Z,Y,X; ELF 812C480 sums X,Y,Z.
#if defined(_WIN32)
    const float distanceSquared = direction.z*direction.z + direction.y*direction.y
                               + direction.x*direction.x;
#else
    const float distanceSquared = direction.x*direction.x + direction.y*direction.y
                               + direction.z*direction.z;
#endif
    float radius = projectile->getExplosionRadius();
    if (player->getNightVision()) radius = projectile->getFlashbangRadiusWithNightVision();
    const float radiusSquared = radius * radius;
    if (!(distanceSquared < radiusSquared)) return;
    owner = this;

    billboardSettings.minAlpha = projectile->getFlashbangBillboardMinAlpha();
    billboardSettings.maxAlpha = projectile->getFlashbangBillboardMaxAlpha();
    billboardSettings.ramp = projectile->getFlashbangBillboardMaxRampTime();
    billboardSettings.minHold = projectile->getFlashbangBillboardMinHoldTime();
    billboardSettings.maxHold = projectile->getFlashbangBillboardMaxHoldTime();
    billboardSettings.minHeal = projectile->getFlashbangBillboardMinHealTime();
    billboardSettings.maxHeal = projectile->getFlashbangBillboardMaxHealTime();
    glowSettings.minAlpha = projectile->getFlashbangAdditiveGlowMinAlpha();
    glowSettings.maxAlpha = projectile->getFlashbangAdditiveGlowMaxAlpha();
    glowSettings.ramp = projectile->getFlashbangAdditiveGlowMaxRampTime();
    glowSettings.minHold = projectile->getFlashbangAdditiveGlowMinHoldTime();
    glowSettings.maxHold = projectile->getFlashbangAdditiveGlowMaxHoldTime();
    glowSettings.minHeal = projectile->getFlashbangAdditiveGlowMinHealTime();
    glowSettings.maxHeal = projectile->getFlashbangAdditiveGlowMaxHealTime();
    blurSettings.minAlpha = projectile->getFlashbangMotionBlurMinAlpha();
    blurSettings.maxAlpha = projectile->getFlashbangMotionBlurMaxAlpha();
    blurSettings.ramp = projectile->getFlashbangMotionBlurMaxRampTime();
    blurSettings.minHold = projectile->getFlashbangMotionBlurMinHoldTime();
    blurSettings.maxHold = projectile->getFlashbangMotionBlurMaxHoldTime();
    blurSettings.minHeal = projectile->getFlashbangMotionBlurMinHealTime();
    blurSettings.maxHeal = projectile->getFlashbangMotionBlurMaxHealTime();
    void* const post = postProduction();
    setEnabled(post, true);
    setCapture(post, true);
    blindingLightDamageShow = true;

    const float innerRadius = projectile->getExplosionInnerConeRadius();
    const float innerSquared = projectile->getExplosionInnerConeRadius() * innerRadius;
    float strength = innerSquared < distanceSquared ? 1.0f-distanceSquared/radiusSquared : 1.0f;
    normalise(direction);
#if defined(_WIN32)
    const float dot = transform.m[10]*direction.z + transform.m[9]*direction.y
                    + transform.m[8]*direction.x;
#else
    const float dot = direction.x*transform.m[8] + direction.y*transform.m[9]
                    + direction.z*transform.m[10];
#endif
    if ((dot <= 0.5f && dot >= 0.0f) || (dot <= 0.0f && dot >= -0.5f))
        strength = (dot + 0.5f) * strength;
    else if (dot < -0.5f)
        strength = 0.0f;
    const float complement = 1.0f-strength;
    strengthenChannel(billboardState, blindingLightDamageAlpha, billboardSettings, strength, complement);
    if (strengthenChannel(glowState, glowAlpha, glowSettings, strength, complement))
        setGlow(post, glowAlpha);
    if (strengthenChannel(blurState, blurAlpha, blurSettings, strength, complement))
        setBlur(post, blurAlpha);
}
#undef FLASH_VCALL
} } }
