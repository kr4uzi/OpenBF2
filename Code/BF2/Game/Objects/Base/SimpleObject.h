// Path:     [INFERRED] beside the attested Game/Objects/Base/SimpleObject.cpp.
// Contents: runtime class recovered from both dedicated-server ports.

#ifndef GAME_OBJECTS_BASE_SIMPLEOBJECT_H
#define GAME_OBJECTS_BASE_SIMPLEOBJECT_H

#include "../Object.h"

namespace dice {
namespace hfe {
class SimpleObjectNetworkable;
namespace geom { class IGeometryTemplate; }
namespace world {

class SimpleObjectTemplate;

// Resolves geometry by name on clients; dedicated-server ports return false.
bool setGeometry(IObject* object, const std::string& geometry);
#if !DEDICATED
// Creates and attaches a geometry instance, releasing its temporary reference on either outcome.
// Returns false for null arguments, creation failure or refused attachment.
bool setGeometry(IObject* object, geom::IGeometryTemplate* geometryTemplate);
#endif

class SimpleObject : public Object
{
public:
    explicit SimpleObject(const SimpleObjectTemplate* objectTemplate);
    virtual ~SimpleObject();

    virtual void init();
    virtual void disableCachedObject();
    virtual void enableCachedObject();
    // the portal manager's unRegisterPortalPassingObject
    // (IPortalManager +0x1C) on this object; Soldier::exit / exitPlayer's last call.
    void purgePortalPassingObject();
    virtual void destroy(bool force);
    virtual void handleVisualUpdate(float, float);
    virtual void handleUpdate(float, unsigned int);
    virtual bool handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&,
                                 int, int, CollisionFaceInfo*, CollisionFaceInfo*);
    virtual int getGeometryPart() const;
    virtual int getMaterial() const;
    virtual void* getDecalObject() const;
    virtual void handleMessage(TemplateMessage, IPlayer*, bool);
    virtual std::string getArmorInfo();
    virtual void initNetworkableComp();

    // The menu-map classifier reads the two-bit held/root-team replication
    // state directly.  The shipped load is inlined; this typed inline view
    // keeps the field private to SimpleObject without manufacturing a virtual.
    unsigned char getReplicationState() const { return m_replicationState; }

protected:
    // Creates the AI object when appropriate; vehicle initialization calls this unconditionally.
    void aiObjectCreate();

private:
    // [CHOSEN] helper for AI-instance lookup, component creation, and portal registration.
    void aiObjectCreateWithInstance();

    // [CHOSEN] split of init()'s bots-gated tail (aiObjectCreate + the AI object's init() + the
    // portal registration) - non-virtual, invisible to the vtable. in full.
    void initAiObjectAndPortal();

    // [CHOSEN] helper for AI initialization and root portal registration during cache activation.
    void enableAiObjectAndPortal();

protected:
    // UV translation and rotation state; rotation is in radians.
    // The cached source object is non-owning and is not cleared on destruction.
    // The first update suppresses movement from the initially zero previous position.
    struct AnimatedUV
    {
        AnimatedUV();
        float    m_translation[2];   // +0x00 +0x04
        float    m_rotation;         // +0x08
        Vec3     m_lastPosition;     // +0x0C
        IObject* m_rotateSource;     // +0x18 non-owning
        bool     m_firstSample;      // +0x1C
    };

    AnimatedUV* m_animatedUV;       // ELF +0x12C / PE +0x144, owned
    void* m_decalObject;            // ELF +0x130 / PE +0x148, opaque renderer-decal interface
    unsigned char m_replicationState; // ELF +0x134 / PE +0x14C, serialized as two bits.
                                      // [CHOSEN] These bits hold the spawning vehicle's team while held.
    friend class ObjectSpawner;
    // The recovered replicator reads/writes this byte directly; native members preserve
    // that access on both architectures without treating the ELF +0x134 as an address.
    friend class ::dice::hfe::SimpleObjectNetworkable;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_BASE_SIMPLEOBJECT_H
