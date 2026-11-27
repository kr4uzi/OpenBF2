// Client game-logic implementation shared by both images. Player requests use
// PostRemoteEvent with a four-bit payload; preserve that size for packed requests.
// [INFERRED] File placement.

#include "ClientGameLogic.h"
#include "../Main/Game.h"                                  // isLightmapRenderingNode

#include <cfloat>
#include <cmath>
#include <vector>

#include "GameLogicCollision.h"                            // calculateCollisionType / doProjectileImpact
#include "GameLogicExplosion.h"                            // clientHandleExplosion [FRONTIER]
#include "../IGame.h"                                      // game: sendGameEvent, failedExitVehicle
#include "../Common/GameEvent.h"                           // PostRemoteEvent
#include "../GameEvents/ToggleFreeCameraEvent.h"
#include "../Objects/IObject.h"                            // getRootParent, getArmorComponent, handleMessage
#include "../Objects/IPlayer.h"
#include "../Objects/IItemContainerObject.h"
#include "../Objects/GameLogicComp/IArmorGLComp.h"         // getHitPoints (+0x3C), getStatus (+0x5C), setSoldierIsRevived (+0x4C)
#include "../Objects/Projectiles/GenericProjectile.h"      // the IProjectileObject slots, as ServerGameLogic/GameLogicCollision cast them
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // isAllowedToExit (+0xA4)
#include "../../Scene/Object/PlayerManager.h"              // world::playerManager
#include "../../Scene/Object/ObjectManager.h"              // world::objectManager->destroyObjects
#include "../../IO/Network/NetworkManager.h"               // io::networkManager: getObject, regressNetworkableIds
#include "../../IO/Input/PlayerInput.h"
#include "../../Physics/MaterialManager.h"                 // getDamageMod (+0x58)
#include "../../Core/System.h"                             // WorldPref::getTickCount / incTickCount
#include "../../Core/Ids.h"

namespace dice {
namespace hfe {

namespace world { extern IBase* materialManager; }         // ds:0x8AD2820 - Core/Globals.cpp defines it as IBase*

namespace {

inline unsigned long long inputMask(const io::PlayerInput& input)
{
    return input.mappingFlags;                             // +0x100 - the 64-bit mapping mask
}

inline bool inputPressed(const io::PlayerInput& input, io::PlayerInputMap index)
{
    if (!(inputMask(input) & (1ULL << index)))
        return false;
    return input.inputs[index] > 0.5f;
}

// The parent walk to the nearest armour component (IObject +0x3C via +0x28 links), as the three
// client collision bodies inline it and GameLogicCollision.cpp names it.
world::IArmorGLComp* findArmor(world::IObject* start)
{
    for (world::IObject* o = start; o; o = o->getParent())
        if (world::IObjectComponent* c = o->getArmorComponent())
            return static_cast<world::IArmorGLComp*>(c);
    return 0;
}

// The epsilon-guarded normalisation every client collision body opens with: leave a unit vector
// alone, scale anything else by 1/sqrt, zero a null vector.
void normaliseIfNeeded(Vec3& v)
{
    const float lenSq = v.z * v.z + v.y * v.y + v.x * v.x;
    if (fabsf(lenSq - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(lenSq) >= FLT_EPSILON)
        {
            const float inv = 1.0f / sqrtf(lenSq);
            v.x *= inv; v.y *= inv; v.z *= inv;
        }
        else
        {
            v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;
        }
    }
}

inline world::MaterialManager* materials()
{
    return static_cast<world::MaterialManager*>(world::materialManager);
}

// The player requests share one shape: a PostRemoteEvent (category 6, id N, not local, a 4-bit
// payload) sent to the server addressed with the player's own id. Written once.
void sendPlayerRequest(world::IPlayer* player, unsigned int eventId, void* payload, unsigned int payloadBits)
{
    PostRemoteEvent* event = new PostRemoteEvent(static_cast<EventCategory>(6), eventId, false,
                                                 payload, payloadBits, 0.0f);       // operator new(0x54)
    game->sendGameEvent(player->getId(), event);                                     // IGame +0x90
}

bool s_inReset = false;                                    // ds:0x8AAC26C - reset()'s re-entrancy guard

} // namespace

// construction - 0x80C4370 (C2), 156 B: GameLogic(), the client vptrs, the refcount, the thirteen
// client dwords (the ticket-history vector among them), m_isServer = false, then this class's
// reset() - called directly, not through the vtable.
ClientGameLogic::ClientGameLogic()
    : m_refCount(1)
    , m_dword1F0(0)
    , m_dword1F4(0)
    , m_dword1F8(0)
    , m_dword1FC(0)
    , m_dword200(0)
    , m_dword204(0)
    , m_pendingExitTick(0)
    , m_exitKeyDownTick(0)
    , m_lastExitClearedTick(0)
{
    m_isServer = false;                                    // +0x3A
    ClientGameLogic::reset();                              // : the direct call
}

//  (D2), 299 B: the ticket vector's strings and buffer, then ~GameLogic - all of it the
// members' own destructors.
ClientGameLogic::~ClientGameLogic()
{
}

// the IBase quartet - 0x80C6B50 / 0x80C6B70 / 0x80C6B80 / 0x80C6BC0
void ClientGameLogic::addRef()  { ++m_refCount; }
int  ClientGameLogic::getRef()  { return m_refCount; }
int  ClientGameLogic::release()
{
    const int r = --m_refCount;
    if (r == 0 && this)                                    // the shipped `this` test, as on ServerGameLogic
        delete this;                                       // vt +0x2C8 = D0
    return r;
}
void* ClientGameLogic::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<ClientGameLogic*>(this);
    if (iid == IID_IGameLogic)
        return static_cast<IGameLogic*>(const_cast<ClientGameLogic*>(this));
    return 0;
}

// create - 0x8531250, 89 B. The aggregation-aware factory shape (ServerGameLogic::create, the two
// SquadManager factories): refuse an outer, new(0x220) + construct, answer queryInterface(iid),
// release the half-built object when the interface is not supported.
IBase* ClientGameLogic::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    ClientGameLogic* logic = new ClientGameLogic;          // operator new(0x220)
    IBase* result = static_cast<IBase*>(logic->queryInterface(iid));   // vt +0x0C
    if (!result)
        logic->release();                                  // vt +0x08
    return result;
}

// lifecycle

// : the base init, its answer dropped, true.
bool ClientGameLogic::init()
{
    GameLogic::init();
    return true;
}

//  clears EAX; PE 0x45C340 explicitly clears AL. Retail false result.
bool ClientGameLogic::close()
{
    return false;
}

// : the client advances the world tick itself while the round is in status 1
// (asked through the vtable - +0x158), then the base update.
void ClientGameLogic::update(float deltaTime)
{
    if (getGameStatus() == 1)
        WorldPref::incTickCount(1);
    GameLogic::update(deltaTime);
}

// . A TU-static guard makes it non-re-entrant: GameLogic::reset can post events that
// come straight back here, and the second entry is dropped. Status 3 first, then the base reset.
void ClientGameLogic::reset()
{
    if (s_inReset)
        return;
    s_inReset = true;
    m_gameStatus = 3;                                      // +0x34 - [UNRESOLVED] the GameStatus enumerator
    GameLogic::reset();
    s_inReset = false;
}

// : tear the level's objects down and rewind the networkable id space.
void ClientGameLogic::restartDemoPlayback()
{
    destroyAllControlPoints();
    reset();                                               // through the vtable (+0x24)
    world::objectManager->destroyObjects();                // vt +0x98
    io::networkManager->regressNetworkableIds();
}


void ClientGameLogic::runNextLevel() {}
void ClientGameLogic::setNextLevel(int) {}

// the player requests

// : the local human player's suicide request - PostRemote id 12, no payload.
void ClientGameLogic::suicide()
{
    if (world::IPlayer* local = world::playerManager->getLocalHumanPlayer())   // vt +0x30
        sendPlayerRequest(local, 12, 0, 0);
}

// : clear the player's spawn tick, then the base kill. eax carries the base's answer.
bool ClientGameLogic::killPlayer(world::IPlayer* player, bool a, bool b)
{
    if (player)
        player->setSpawnAtTick(0);                         // IPlayer +0xF0
    return GameLogic::killPlayer(player, a, b);
}

// : never on the client.
bool ClientGameLogic::reSpawnPlayer(world::IPlayer*)
{
    return false;
}

//  / 0x80C4950, 143 B / 0x80C49E0, 176 B: PostRemote ids 6, 7 and 8. The kit
// request packs (unlockLevel << 4 | kit & 0xF) into the int whose low 4 bits go out.
bool ClientGameLogic::selectSpawnGroup(world::IPlayer* player, int spawnGroup)
{
    if (!player)
        return false;
#if !DEDICATED
    // BF2.exe 0x4E1C42..0x4E1C49: lightmap workers accept without posting a request.
    if (isLightmapRenderingNode())
        return true;
#endif
    sendPlayerRequest(player, 6, &spawnGroup, 4);
    return true;
}

bool ClientGameLogic::selectTeam(world::IPlayer* player, int team)
{
    if (!player)
        return false;
    sendPlayerRequest(player, 7, &team, 4);
    return true;
}

bool ClientGameLogic::selectKitAndUnlockLevel(world::IPlayer* player, int kit, int unlockLevel)
{
    if (!player)
        return false;
    int packed = (unlockLevel << 4) | (kit & 0xF);         // (16 * a4) | a3 & 0xF
    sendPlayerRequest(player, 8, &packed, 4);
    return true;
}

// the ticket surface - the server's; inert here (5- and 7-byte bodies)
void  ClientGameLogic::setTicketState(unsigned int, int) {}
int   ClientGameLogic::getTicketState(unsigned int) { return 0; }
void  ClientGameLogic::setDefaultNumberOfTickets(unsigned int, unsigned int) {}
unsigned int ClientGameLogic::getDefaultNumberOfTickets(unsigned int) { return 0; }
void  ClientGameLogic::setDefaultNumberOfTicketsEx(unsigned int, unsigned int, unsigned int) {}
unsigned int ClientGameLogic::getDefaultNumberOfTicketsEx(unsigned int, unsigned int) { return 0; }
void  ClientGameLogic::setDefaultTimeToNextAIWave(float) {}
float ClientGameLogic::getDefaultTimeToNextAIWave() { return 0.0f; }
void  ClientGameLogic::setTicketLossAtEndPerMin(float) {}
float ClientGameLogic::getTicketLossAtEndPerMin() { return 0.0f; }
void  ClientGameLogic::setTicketLossPerMin(unsigned int, float) {}
float ClientGameLogic::getTicketLossPerMin(unsigned int) { return 0.0f; }

//  / 0x80C5E80, 34 B: the vector at +0x208, by reference and by assignment.
TicketHistory& ClientGameLogic::getTicketHistory()
{
    return m_ticketHistory;
}

void ClientGameLogic::setTicketHistory(const TicketHistory& history)
{
    m_ticketHistory.tickets = history.tickets;             // vector<Ticket>::operator=
}

// effects, damage, pickups

//  / 0x80C4AA0, 7 B (both giveDamage overloads share the `xor eax,eax` body).
void ClientGameLogic::playEffect(Vec3, const world::IObjectTemplate*, Vec3) {}

bool ClientGameLogic::giveDamage(world::IObject*, float, world::IPlayer*,
                                 std::list<WeakPtr<world::IPlayer> >*,
                                 world::IObject*, int, world::IObject*,
                                 world::IObject*, Vec3, Vec3, int, bool, bool, bool)
{
    return false;
}

bool ClientGameLogic::giveDamage(world::IObject*, float, world::IPlayer*,
                                 std::list<WeakPtr<world::IPlayer> >*,
                                 int, world::IObject*, world::IObject*,
                                 Vec3, Vec3, int, bool, bool)
{
    return false;
}

// : every argument forwarded to the client explosion routine.
void ClientGameLogic::handleExplosion(Vec3 position, float a, int b, float c, world::IPlayer* player,
                                      world::IArmorGLComp* armor, int d, world::IObject* e,
                                      world::IObject* f, float g, world::IObject* h,
                                      world::IObject* i, float j, bool k, bool l)
{
    world::GameLogicExplosion::clientHandleExplosion(position, a, b, c, player, armor, d, e, f, g, h, i, j, k, l);
}

//  / 0x80C4B70, 66 B: the container's item hooks (IItemContainerObject +0x24 / +0x28),
// the container's interface asked through queryInterface and then dereferenced without a null test -
// a null container faults, as shipped. The fourth argument is not read.
void ClientGameLogic::handlePickup(world::IObject* item, world::IPlayer* player, world::IObject* container, bool)
{
    world::IItemContainerObject* c = 0;
    if (container)
        c = static_cast<world::IItemContainerObject*>(container->queryInterface(world::IID_IItemContainerObject));
    c->handlePickupOnItem(player, item);                   // +0x24
}

void ClientGameLogic::handleDrop(world::IObject* item, world::IPlayer* player, world::IObject* container, bool)
{
    world::IItemContainerObject* c = 0;
    if (container)
        c = static_cast<world::IItemContainerObject*>(container->queryInterface(world::IID_IItemContainerObject));
    c->handleDropOnItem(player, item);                     // +0x28
}

// the three collision handlers. The server's twins live in GameLogicCollision.cpp; the client's
// have no damage arm - they decide ricochet / penetrate and play the impact.

bool ClientGameLogic::handleCollisionForProjectile(world::IObject* projectile, world::IObject* hitObject,
                                                   const Vec3& velocity, const Vec3& normal,
                                                   const Vec3& collisionOffset, int materialA, int materialB,
                                                   world::CollisionFaceInfo* /*faceA*/,
                                                   world::CollisionFaceInfo* faceB)
{
    Vec3 unitVelocity = velocity;
    normaliseIfNeeded(unitVelocity);
    const float speed = sqrtf(velocity.z * velocity.z + velocity.y * velocity.y + velocity.x * velocity.x);
    bool playImpact = (velocity.z * normal.z + velocity.y * normal.y + velocity.x * normal.x) < -0.5
                   || speed > 10.0;

    world::IObject* const root = world::getRootParent(projectile);
    world::IProjectileObject* proj = 0;
    if (root)
        proj = static_cast<world::IProjectileObject*>(root->queryInterface(world::IID_IProjectileObject));
    world::GenericProjectile* const gp = static_cast<world::GenericProjectile*>(proj);

    const unsigned int tick = WorldPref::getTickCount();
    if (tick - gp->getPlayedImpactEffectAtTick() <= 10u)                       // +0x11C - an unsigned compare
        playImpact = false;

    Vec3 contact;
    {
        const Vec3& rootPos = root->getAbsolutePosition();                      // unguarded, as shipped
        contact.x = collisionOffset.x + rootPos.x;
        contact.y = collisionOffset.y + rootPos.y;
        contact.z = collisionOffset.z + rootPos.z;
    }

    float ricochetFraction = 0.0f;
    int collisionType = gp->getDefaultCollisionType(materialB);                // +0xE8
    if (!collisionType)
    {
        collisionType = world::GameLogicCollision::calculateCollisionType(normal, unitVelocity, materialA,
                                                                          materialB, ricochetFraction) ? 1 : 0;
    }
    if (collisionType)
    {
        if (collisionType == 1)
            gp->ricochet(collisionOffset, velocity, normal, ricochetFraction);  // +0xEC - the RAW fraction
    }
    else
    {
        gp->penetrate(hitObject, collisionOffset, materialA, materialB, 1.0f - ricochetFraction);   // +0xF4
    }
    if (playImpact)
    {
        gp->setPlayedImpactEffectAtTick(tick);                                 // +0x120
        world::GameLogicCollision::doProjectileImpact(materialA, materialB,
                                                     static_cast<world::MaterialCollisionType>(collisionType),
                                                     contact, normal, unitVelocity,
                                                     proj, projectile, hitObject, 0, false, faceB);   // : the
                                                     //   hit object in the ninth slot on both arms, no armour, no damage
    }
    return true;
}

// . The first argument is not read.
bool ClientGameLogic::handleCollisionForReplenisher(bool, world::IObject* projectile, world::IObject* hitObject,
                                                    const Vec3& velocity, const Vec3& normal,
                                                    const Vec3& collisionOffset, int materialA, int materialB)
{
    Vec3 direction = velocity;
    normaliseIfNeeded(direction);

    world::IObject* const root = world::getRootParent(projectile);
    world::IProjectileObject* proj = 0;
    if (root)
        proj = static_cast<world::IProjectileObject*>(root->queryInterface(world::IID_IProjectileObject));
    world::GenericProjectile* const gp = static_cast<world::GenericProjectile*>(proj);

    Vec3 contact;
    {
        const Vec3& rootPos = root->getAbsolutePosition();
        contact.x = collisionOffset.x + rootPos.x;
        contact.y = collisionOffset.y + rootPos.y;
        contact.z = collisionOffset.z + rootPos.z;
    }
    const Vec3 zero = { 0.0f, 0.0f, 0.0f };

    if (!hitObject)
    {
        doImpact(materialA, materialB, static_cast<world::MaterialCollisionType>(1), contact, normal, direction,
                 projectile, 0, 0, 0, zero, false);                            // this +0x10C
        gp->ricochet(collisionOffset, velocity, normal, 0.0f);                 // +0xEC
        return true;
    }

    bool replenished = false;
    if (findArmor(hitObject))
    {
        if (root == world::getRootParent(hitObject))
            return false;                                                       // the self-hit drop: nothing at all
        const float amount = gp->getHealEffect(contact);                       // IProjectileObject +0x78 (secondary table 0x88BC658 + 0x78).
                                                                               //   returns A float: 0x8284AC0 hands back st0 from the collision
                                                                               //   component's +0x4C and this body compares it against 0.01.
        if (amount > 0.0099999998f)
        {
            world::IArmorGLComp* const armor = findArmor(hitObject);           // walked a second time
            if (armor && armor->getHitPoints() > 0.001)                        // +0x3C, against the double
            {
                if (materials()->getDamageMod(materialA, materialB) * amount > 0.0f)   // +0x58
                    replenished = true;
            }
        }
    }
    doImpact(materialA, materialB, static_cast<world::MaterialCollisionType>(replenished ? 0 : 1),
             contact, normal, direction, projectile, hitObject, 0, 0, zero, false);
    if (replenished)
    {
        gp->penetrate(hitObject, collisionOffset, materialA, materialB, 1.0f);  // +0xF4
        return true;
    }
    gp->ricochet(collisionOffset, velocity, normal, 0.0f);
    return true;
}

// . The two floats are not read. Same skeleton as the replenisher with the
// projectile in doImpact's other object slot on the no-object arm, getDamage for the amount, and
// "status != 2" (IArmorGLComp +0x5C) instead of the hit-point test.
bool ClientGameLogic::handleCollisionForResurrecter(world::IObject* projectile, world::IObject* hitObject,
                                                    float, float, const Vec3& velocity, const Vec3& normal,
                                                    const Vec3& collisionOffset, int materialA, int materialB)
{
    Vec3 direction = velocity;
    normaliseIfNeeded(direction);

    world::IObject* const root = world::getRootParent(projectile);
    world::IProjectileObject* proj = 0;
    if (root)
        proj = static_cast<world::IProjectileObject*>(root->queryInterface(world::IID_IProjectileObject));
    world::GenericProjectile* const gp = static_cast<world::GenericProjectile*>(proj);

    Vec3 contact;
    {
        const Vec3& rootPos = root->getAbsolutePosition();
        contact.x = collisionOffset.x + rootPos.x;
        contact.y = collisionOffset.y + rootPos.y;
        contact.z = collisionOffset.z + rootPos.z;
    }
    const Vec3 zero = { 0.0f, 0.0f, 0.0f };

    if (!hitObject)
    {
        doImpact(materialA, materialB, static_cast<world::MaterialCollisionType>(1), contact, normal, direction,
                 0, projectile, 0, 0, zero, false);                            // note the (0, projectile) order
        gp->ricochet(collisionOffset, velocity, normal, 0.0f);
        return false;
    }

    bool revived = false;
    if (findArmor(hitObject))
    {
        if (root == world::getRootParent(hitObject))
            return false;
        Vec3 at = contact;
        const float amount = gp->getDamage(at);                                // +0xBC, by value
        if (amount > 0.0099999998f)
        {
            world::IArmorGLComp* const armor = findArmor(hitObject);
            if (armor && armor->getStatus() != 2)                              // +0x5C
            {
                if (materials()->getDamageMod(materialA, materialB) * amount > 0.0f)
                    revived = true;
            }
        }
    }
    doImpact(materialA, materialB, static_cast<world::MaterialCollisionType>(revived ? 0 : 1),
             contact, normal, direction, projectile, hitObject, 0, 0, zero, false);
    if (revived)
    {
        gp->penetrate(hitObject, collisionOffset, materialA, materialB, 1.0f);
        return false;
    }
    gp->ricochet(collisionOffset, velocity, normal, 0.0f);
    return false;
}

void ClientGameLogic::handleCollision(world::IObject*, world::IObject*, const Vec3&, const Vec3&, const Vec3&,
                                      int, int, world::CollisionFaceInfo*, world::CollisionFaceInfo*)
{
}

// : nothing but a function-local static's guarded construction survives - the same
// `static std::vector<IObject*> spawned` ServerGameLogic::spawnMultipleObjects keeps (this TU's copy is
// ds:0x8AAC260); the client's body around it is compiled out.
void ClientGameLogic::spawnMultipleObjects(const world::IObjectTemplate*, int)
{
    static std::vector<world::IObject*> spawned;           // ds:0x8AAC260, guard 0x8AAC258
    (void)spawned;
}


world::IObject* ClientGameLogic::spawnObject(const std::string&, Vec3, Vec3, int) { return 0; }
world::IObject* ClientGameLogic::spawnObject(const world::IObjectTemplate*, Vec3, Vec3, int) { return 0; }

// Processes client sprint, fire, weapon, reload, and vehicle input each tick.
// Includes pending-exit bookkeeping and forwards flag to message helpers.
void ClientGameLogic::checkPlayerTriggers(world::IPlayer* player, world::IObject* vehicle,
                                          io::PlayerInput input, bool flag)
{
    if (!player->getIsAlive() &&                                         // +0x6C
        player->getVehicle() != player->getFreeCamera())                 // +0x48 / +0xA8
        return;
    if (player->getOverheadCameraEnabled())                              // +0xBC
        return;

    const bool wasSprinting = player->getSprintState();                  // +0x1BC
    const bool wantsSprint  = GameLogic::getIsSprintRequested(input, vehicle);   // the copy
    player->setSprintStateLastTick(player->getSprintState());            // +0x1C0 <- +0x1BC
    player->setSprintState(wantsSprint);                                 // +0x1B8
    if (vehicle && player->getSprintState())                             // a third +0x1BC read
        vehicle->handleMessage(static_cast<world::TemplateMessage>(wasSprinting ? 41 : 42), player, flag);   // +0x104

    unsigned int fireMask = 0;
    if (inputPressed(input, io::PIFire))      fireMask |= 1u;            // bit 8
    if (inputPressed(input, io::PIAltFire))   fireMask |= 2u;            // bit 31
    if (inputPressed(input, io::PIFlareFire)) fireMask |= 4u;            // bit 47
    if (vehicle && fireMask)
        sendFireMessages(fireMask, player, vehicle, flag);

    selectWeapon(player, vehicle, input, flag);                          // a second copy of the input

    if (!flag)
    {
        if (inputPressed(input, io::PIReload) && vehicle)                // bit 32
            vehicle->handleMessage(static_cast<world::TemplateMessage>(10), player, flag);
    }

    if (inputPressed(input, io::PIUse))                                  // bit 10: enter / exit
    {
        if (!m_exitKeyDownTick)
            m_exitKeyDownTick = WorldPref::getTickCount();
        if (player->getVehicle() != player->getDefaultVehicle())         // +0x48 vs +0xA4
        {
            world::IObject* const current = player->getVehicle();
            world::IPlayerControlObject* const pco =
                static_cast<world::IPlayerControlObject*>(current->queryInterface(world::IID_IPlayerControlObject));
            if (!pco || pco->isAllowedToExit())                          // +0xA4
            {
                if (static_cast<double>(m_exitKeyDownTick) > m_lastExitClearedTick + 15.0)   // held 15 ticks past the last clear
                    m_pendingExitTick = WorldPref::getTickCount();
            }
            else
            {
                game->failedExitVehicle(player->getId());                // IGame +0x114
            }
        }
    }
    else if (m_exitKeyDownTick)
    {
        m_exitKeyDownTick = 0;
    }
}

//  / 0x80C5C10, 38 B: the pending exit lives 60 ticks; clearing it stamps the tick.
bool ClientGameLogic::getHasPendingExit()
{
    return m_pendingExitTick + 60 > WorldPref::getTickCount();
}

void ClientGameLogic::clearPendingExit()
{
    m_pendingExitTick = 0;
    m_lastExitClearedTick = WorldPref::getTickCount();
}

void ClientGameLogic::createObject(std::string)
{
}

// toggleLocalFreeCamera - 0x80C5DD0, 160 B: flip the local player's free camera, tell the server,
// and seat the camera where the vehicle is.
void ClientGameLogic::toggleLocalFreeCamera()
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();   // vt +0x30
    if (!local)
        return;
    local->toggleFreeCameraEnabled();                                    // +0xB4
    GameEvent* event = new ToggleFreeCameraEvent();                      // operator new(0xC)
    game->sendGameEvent(local->getId(), event);                          // IGame +0x90
    world::IObject* const vehicle = local->getVehicle();                 // +0x48
    const Mat4& transform = vehicle->getAbsoluteTransformation();        // slot 33 (+0x84)
    local->getFreeCamera()->setAbsoluteTransformation(transform);        // +0xA8, then slot 35 (+0x8C)
}

// Handles game-status, reset, and revive notifications. Swallows ECGame IDs
// 1..4, 6, and 8; forwards unhandled events to the base listener.
void ClientGameLogic::handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data)
{
    if (category == static_cast<EventCategory>(0))
    {
        if (id == 6)
        {
            reset();                                                     // through the vtable
            return;
        }
        if (id <= 6)
        {
            if (id == 1)
            {
                m_gameStatus = *static_cast<int*>(data);                 // read through the node's +0x20 payload
                return;
            }
            GameLogic::handleEvent(category, id, node, data);
            return;
        }
        if (id != 14)
        {
            GameLogic::handleEvent(category, id, node, data);
            return;
        }
        world::IObject* object = io::networkManager->getObject(*static_cast<unsigned short*>(data));
        if (object)
        {
            if (world::IObjectComponent* armor = object->getArmorComponent())          // +0x3C
                static_cast<world::IArmorGLComp*>(armor)->setSoldierIsRevived();       // +0x4C
        }
        return;
    }
    if (category == static_cast<EventCategory>(1))
    {
        if (id == 6 || id == 8 || (id >= 1 && id <= 4))
            return;                                                      // swallowed on the client
        GameLogic::handleEvent(category, id, node, data);
        return;
    }
    GameLogic::handleEvent(category, id, node, data);
}

} // namespace hfe
} // namespace dice
