// Transient controlled object for a soldier riding a zipline.
// [PLATFORM] Member offsets vary with the PlayerControlObject/STL layout.

#ifndef GAME_OBJECTS_GAME_ZIPLINECONTAINER_H
#define GAME_OBJECTS_GAME_ZIPLINECONTAINER_H

#include "PlayerControlObject.h"
#if !DEDICATED
#include "../../../Sound/ObjectSoundNode.h"
#endif

namespace dice {
namespace hfe {
class ISound;
namespace world {

class Zipline;
class ZiplineContainerTemplate;
class ISoldier;

class ZiplineContainer : public PlayerControlObject
{
public:
    explicit ZiplineContainer(const ZiplineContainerTemplate* objectTemplate);
    virtual ~ZiplineContainer();

    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    virtual void handleUpdate(float deltaTime, unsigned int flags);
    virtual bool handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&,
                                 int, int, CollisionFaceInfo*, CollisionFaceInfo*);
    virtual io::INetworkable* getNetworkable() const;
    virtual void handlePlayerInput(IPlayer*, const io::PlayerInput&, float, unsigned int);
    virtual void handleMessage(TemplateMessage, IPlayer*, bool);

    // Ordinary vehicle entry is rejected; Zipline::attachSoldier calls _enter directly.
    virtual bool enter(IPlayer* player);
    virtual void exit();
    virtual bool getExitPosition(Vec3& out);
    virtual bool isAllowedToExit();
    virtual Vec3* getExitRotation();

    // Four class-own slots appended to the primary table, followed by the two concrete overrides
    // of IPlayerControlObject's existing camera/vehicle-entry slots. Declaration order is retail.
    virtual bool _enter(IPlayer* player);
    virtual void _exit();
    virtual void addForce(float force);
    virtual void setExitPosition(Vec3 position);
    virtual bool applyCameraAnimation(const Mat4& input, Mat4& output);
    virtual bool allowEnterOtherVehicle();

    bool CheckObjectCollision(ISoldier* soldier);
    bool CheckTerrainCollision(ISoldier* soldier);
    void updateZiplineAnimation(float deltaTime);
    void drawDebugline(Vec3& start, Vec3& end, float width, Vec4 colour);
    Vec3 _getExitPosition();
    IObject* getCamera();

private:

    friend class Zipline;

    // Dedicated ELF offsets are shown. Dedicated PE adds +0x4C; client PE also
    // inserts the sound node and pointer before m_slopeSpeed (+0x49C).
    /* +0x3BC */ int      m_refCount2;
    /* +0x3C0 */ Zipline* m_zipline;
    /* +0x3C4 */ float    m_action;                  // PIAction
    /* +0x3C8 */ float    m_use;                     // PIUse; ctor -1 suppresses an initial hold
    /* +0x3CC */ float    m_throttle;                // PIThrottle clamped to [-1,+1]
    /* +0x3D0 */ int      _dword3D0;                 // ctor/_exit zero [UNRESOLVED]
    /* +0x3D4 */ float    m_positionRefreshTimer;    // once-per-second fallback refresh
    /* +0x3D8 */ Vec3     m_motion;                  // accumulated animation displacement
    /* +0x3E4 */ Vec3     _vec3E4;                   // ctor zero; no recovered reader [UNRESOLVED]
    /* +0x3F0 */ Vec3     _vec3F0;                   // ctor zero; no recovered reader [UNRESOLVED]
    /* +0x3FC */ Vec3     m_exitRotation;
    /* +0x408 */ Vec3     m_exitPosition;
    /* +0x414 */ Vec3     m_traversalStart;
    /* +0x420 */ Vec3     m_traversalEnd;
    /* +0x42C */ Vec3     m_lastSoldierPosition;     // detach fallback, periodically refreshed
    /* +0x438 */ Vec3     m_accumulatedSlopeMotion;
#if !DEDICATED
    // Client PE 0x5DAD4E: the node borrows this object; BfSound owns the sound.
    ObjectSoundNode m_soundNode;                    // client +0x490
    ISound* m_usedSound;                            // client +0x498, borrowed
#endif
    /* +0x444 */ float    m_slopeSpeed;
    /* +0x448 */ float    m_force;                   // addForce clamps to [-30,+30]; no own reader
    /* +0x44C */ bool     _byte44C;                  // ctor/_exit zero [UNRESOLVED]
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_GAME_ZIPLINECONTAINER_H
