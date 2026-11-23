// Soldier sound playback. Dedicated-server sound factories return null.

#include "SoldierSound.h"

#include <string>
#include <cstdio>                                    // the bounded update() stop
#include <cstdlib>
#include <cfloat>
#include <cmath>

#include "BfSound.h"                                  // g_bfSound - playSound / stopSound by name
#include "NullSound.h"                                // ISound
#include "SoundObjectTemplate.h"
#include "../Core/Ids.h"
#include "../Math/Mat4.h"
#include "../Math/Vec3.h"
#include "../Physics/IPhysicsNode.h"
#include "../Physics/MaterialManager.h"
#include "../Physics/SoldierResponse.h"
#include "../Game/Objects/Game/Soldier.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Game/Objects/IPlayerControlObjectTemplate.h"
#include "../Game/Objects/ObjectLayoutAccess.h"
#include "../Game/Objects/Base/SimpleObjectTemplate.h"

namespace dice {
namespace hfe {


// Material sound ids distinguish fatal hits (5500) from bullet flybys (5501).

namespace world { extern IBase* materialManager; }


// BF2.exe 0x7E2AD0 / ELF 0x083F2EC0: preserve creation order and the two impl() calls.
void SoldierSound::initTemplates(world::SimpleObjectTemplate* objectTemplate)
{
    const char* const names[] = { "Death", "Revive", "Swim", "Swim_Still", "Swim_Sprint" };
    for (const char* name : names)
    {
        world::ObjectTemplate* const child = objectTemplate->addSoundTemplate(std::string(name));
        child->updateFlags(0x400, 0);
        SoundObjectTemplate* const soundTemplate = static_cast<SoundObjectTemplate*>(child);
        soundTemplate->impl()->setIs3dSound(true);
        soundTemplate->impl()->setLoopCount(1);
    }
}


// Initializes per-soldier sound slots and requests their configured voices.

SoldierSound::SoldierSound(world::Soldier* soldier)
    : m_soldier(soldier)
    , m_lastMaterial(-1)
    , m_movementSpeedEvaluator(0.0f)
    , m_phaseLatch(true)
    , m_drowningPlaying(false)
{
    for (int i = 0; i < 9; ++i)
        m_sounds[i] = 0;

    const world::SimpleObjectTemplate* t =
        static_cast<const world::SimpleObjectTemplate*>(soldier->getTemplate());

    m_sounds[4] = static_cast<ISound*>(t->createSoundFromTemplate(std::string("Death"), soldier));
    if (m_sounds[4])
    {
        m_sounds[4]->setParent(this);                                   // +0x3C
        m_sounds[4]->setVolumeEvaluator(0, &m_movementSpeedEvaluator);   // +0x44
        m_sounds[4]->setPitchEvaluator(0, &m_movementSpeedEvaluator);    // +0x4C
    }

    m_sounds[5] = static_cast<ISound*>(t->createSoundFromTemplate(std::string("Revive"), soldier));
    if (m_sounds[5])
        m_sounds[5]->setParent(this);

    m_sounds[6] = static_cast<ISound*>(t->createSoundFromTemplate(std::string("Swim"), soldier));
    if (m_sounds[6])
        m_sounds[6]->setParent(this);

    m_sounds[7] = static_cast<ISound*>(t->createSoundFromTemplate(std::string("Swim_Sprint"), soldier));
    if (m_sounds[7])
        m_sounds[7]->setParent(this);

    m_sounds[8] = static_cast<ISound*>(t->createSoundFromTemplate(std::string("Swim_Still"), soldier));
    if (m_sounds[8])
        m_sounds[8]->setParent(this);
}

// Walk all nine slots - not the five the constructor filled - and stop then
// release each one that exists. It does not null them; the object is being destroyed.
SoldierSound::~SoldierSound()
{
    for (int i = 0; i < 9; ++i)
    {
        if (m_sounds[i])
        {
            m_sounds[i]->stop();                                        // +0x30
            m_sounds[i]->release();                                     // +0x04
        }
    }
}

// . live, not cached: the sound's position is whatever the soldier's transformation says
// right now. Object slot +0x84.
const Mat4& SoldierSound::getTransform()
{
    return m_soldier->getAbsoluteTransformation();
}

// `return 0;`. The same "no velocity" answer RotationalBundle gives - so a soldier's sounds
// get no doppler.
const Math::Vec3* SoldierSound::getVelocity()
{
    return 0;
}


// the movement sounds


// each - both are a bare forward. Jumping and landing in third person make the same noise, and
// dice kept the two names rather than collapsing them.
void SoldierSound::jump()             { playJumpSample(); }
void SoldierSound::trigger3PLanding() { playJumpSample(); }

// . first person only: it needs an occupying player and that player to have a first-person
// view. The 3P version above has no such gate.
void SoldierSound::trigger1PLanding()
{
    world::IPlayer* player = m_soldier->getOccupingPlayer();
    if (!player)
        return;
    if (!player->hasFirstPersonView())                                  // IPlayer +0x188
        return;
    playJumpSample();
}

// . two slots, first-wins, and only one plays: sound[3] if it exists, otherwise sound[0].
// Both come from updateContactMaterial.
void SoldierSound::playJumpSample()
{
    if (!allowMovementSounds())
        return;
    if (m_sounds[3])
    {
        m_sounds[3]->play();                                            // +0x10
        return;
    }
    if (m_sounds[0])
        m_sounds[0]->play();
}


// Returns whether the occupied soldier may play movement sounds.
// Parented riders are silent except when attached to a ladder.

bool SoldierSound::allowMovementSounds() const
{
    if (!m_soldier->getOccupingPlayer())                                // Soldier +0x180
        return false;
    if (!m_soldier->getParent())                                        // object +0x28
        return true;
    return m_soldier->getLadderId() != 0;                               // Soldier +0x2CC
}


// the death / revive pair


// BF2.exe 0x7E2CB0 / ELF 0x083F35E0: always query the occupant; damage type 1 requires
// first-person view, while other damage types play without that gate.
void SoldierSound::die(bool firstPersonOnly)
{
    world::IPlayer* player = m_soldier->getOccupingPlayer();
    if (firstPersonOnly)
    {
        if (!player || !player->hasFirstPersonView())
            return;
    }
    if (m_sounds[4])
        m_sounds[4]->play();
}

// . No gate at all - a revive is heard by everyone.
void SoldierSound::revive()
{
    if (m_sounds[5])
        m_sounds[5]->play();
}


// update - / Wind handling precedes the movement-sound
// gate. The active animation's half-cycle drives a phase latch; speed and pose select one of the
// four material footsteps or the three swim sounds. The output material is written before the pose
// switch, so an unknown pose can leave an output value while returning without advancing the latch.

void SoldierSound::update(float /*deltaTime*/, int* outMaterial)
{
    float animationLength = 0.0f;
    float animationTime = 0.0f;
    if (outMaterial)
        *outMaterial = -1;

    world::IPlayer* player = m_soldier->getOccupingPlayer();
    if (player)
    {
        world::IObject* vehicle = player->getVehicle();
        if (vehicle)
        {
            const world::IObjectTemplate* objectTemplate = vehicle->getTemplate();
            if (objectTemplate)
            {
                world::IPlayerControlObjectTemplate* pcoTemplate =
                    static_cast<world::IPlayerControlObjectTemplate*>(
                        objectTemplate->queryInterface(
                            world::IID_IPlayerControlObjectTemplate));
                if (pcoTemplate)
                {
                    const float obstruction = pcoTemplate->getListenerObstruction();
                    if (!(obstruction > FLT_EPSILON))
                        g_bfSound->startWind();
                }
                else if (vehicle->getTemplate()->getClassID() == world::CID_SoldierTemplate)
                {
                    g_bfSound->startWind();
                }
            }
        }
    }

    if (!allowMovementSounds())
        return;

    m_soldier->getPlayingAnimationInfo(animationLength, animationTime);

    world::IPhysicsNode* physicsNode = static_cast<world::IPhysicsNode*>(
        world::object_detail::LayoutAccess::physicsNode(m_soldier));
    if (!physicsNode)
        return;
    const Vec3& speed = physicsNode->getAbsolutePositionalSpeed();
    m_movementSpeedEvaluator = std::sqrt(
        speed.x * speed.x + speed.y * speed.y + speed.z * speed.z);

    world::SoldierResponsePhysics* response = static_cast<world::SoldierResponsePhysics*>(
        world::object_detail::LayoutAccess::responsePhysics(m_soldier));
    if (!response)
        return;

    const world::SoldierPose pose = m_soldier->getPose();
    const bool onLadder = m_soldier->getLadderId() != 0;
    int material;
    if (pose == world::SoldierPose_Swim || response->m_byteFA)
    {
        material = onLadder ? 6600 : m_soldier->getLastContactMaterial();
    }
    else
    {
        if (!onLadder)
            return;
        material = 6600;
    }

    updateContactMaterial(material);

    const bool phase = 0.5f * animationLength > animationTime;
    if (m_movementSpeedEvaluator >= 0.1f)
    {
        if (phase != m_phaseLatch)
        {
            if (outMaterial)
                *outMaterial = material;

            int soundIndex;
            switch (pose)
            {
            case world::SoldierPose_Stand:
                soundIndex = m_soldier->getIsSprinting() ? 3 : 0;
                break;
            case world::SoldierPose_Crouch:
                soundIndex = 1;
                break;
            case world::SoldierPose_Prone:
                soundIndex = 2;
                break;
            case world::SoldierPose_Swim:
                soundIndex = m_soldier->getIsSprinting() ? 7 : 6;
                break;
            default:
                return;
            }

            if (pose == world::SoldierPose_Swim)
            {
                if (m_sounds[soundIndex])
                {
                    const Vec3 position = getSwimStrokePos(m_phaseLatch);
                    m_sounds[soundIndex]->playAt(position);
                }
            }
            else if (m_sounds[soundIndex])
            {
                m_sounds[soundIndex]->play();
            }
        }
    }
    else if (pose == world::SoldierPose_Swim
             && phase != m_phaseLatch
             && m_sounds[8])
    {
        const Vec3 position = getSwimStrokePos(m_phaseLatch);
        m_sounds[8]->playAt(position);
    }

    m_phaseLatch = phase;
}

// updateContactMaterial - / Rebuild the four footstep
// voices when the contact material changes. Missing nonzero template slots alias slot zero and take
// their own reference; a present template whose createSound fails deliberately does not fall back.
void SoldierSound::updateContactMaterial(int material)
{
    if (material == m_lastMaterial)
        return;

    for (int i = 0; i < 4; ++i)
    {
        if (m_sounds[i])
        {
            m_sounds[i]->release();
            m_sounds[i] = 0;
        }
    }

    m_lastMaterial = material;
    for (int i = 0; i < 4; ++i)
    {
        world::IObjectTemplate* base =
            static_cast<world::MaterialManager*>(world::materialManager)
                ->getSoundTemplate(5500u, static_cast<unsigned int>(material), i);
        if (base)
        {
            SoundObjectTemplate* soundTemplate = static_cast<SoundObjectTemplate*>(base);
            m_sounds[i] = g_bfSound->createSound(soundTemplate->impl(), m_soldier);
            if (m_sounds[i])
                m_sounds[i]->setParent(this);
            if (m_sounds[i])
            {
                m_sounds[i]->setVolumeEvaluator(0, &m_movementSpeedEvaluator);
                if (m_sounds[i])
                    m_sounds[i]->setPitchEvaluator(0, &m_movementSpeedEvaluator);
            }
        }
        else if (i != 0)
        {
            m_sounds[i] = m_sounds[0];
            if (m_sounds[0])
                m_sounds[0]->addRef();
        }
    }
}

// . not an owned sound: the killing blow's noise comes from the material system, id 5500,
// played at the soldier's absolute position. the float argument is unused - it is in the mangling
// (dieHit(int, float)) and the body never reads it.
void SoldierSound::dieHit(int material, float /*unused*/)
{
    ISound* const sound = static_cast<ISound*>(
        static_cast<world::MaterialManager*>(world::materialManager)
            ->getSound(5500u, static_cast<unsigned int>(material), 4, false));
    if (sound)
        sound->playAt(m_soldier->getAbsolutePosition());                // ISound +0x18
}

// Plays near-miss sound 5501 only in first-person view. Preserve both occupying-player queries.
void SoldierSound::bulletFlyBy(int material, const Vec3& at)
{
    if (!m_soldier->getOccupingPlayer())
        return;
    if (!m_soldier->getOccupingPlayer()->hasFirstPersonView())
        return;

    ISound* const sound = static_cast<ISound*>(
        static_cast<world::MaterialManager*>(world::materialManager)
            ->getSound(5501u, static_cast<unsigned int>(material), 0, true));
    if (sound)
        sound->playAt(at);                                               // ISound +0x18
}


// drowning - the odd one out. it is played by name through g_bfSound rather than as one of the
// nine owned sounds, and it is latched: the start is a no-op while it is already playing and the
// stop a no-op while it is not. "S_Drowning" is the only sound name in this class that is not one
// of the constructor's five.

void SoldierSound::playDrowningSound()
{
    if (m_drowningPlaying)
        return;
    g_bfSound->playSound(std::string("S_Drowning"));                    // IBfSound +0x2C
    m_drowningPlaying = true;
}

void SoldierSound::stopDrowningSound()
{
    if (!m_drowningPlaying)
        return;
    g_bfSound->stopSound(std::string("S_Drowning"));                    // IBfSound +0x30
    m_drowningPlaying = false;
}


// . A constant, not state - and the only thing the argument changes is the sign of X.
// (+/-0.05, 0, 0.4): the two hands enter the water 10 cm apart and 40 cm in front of the soldier's
// origin. Returned by value through an sret.

Vec3 SoldierSound::getSwimStrokePos(bool leftStroke) const
{
    Vec3 pos;
    pos.x = leftStroke ? -0.05f : 0.05f;
    pos.y = 0.0f;
    pos.z = 0.4f;
    return pos;
}

} // namespace hfe
} // namespace dice
