// Object query filters; header path [INFERRED]. Keep predicate/getFlags/performCulling/predicateSurface
// in order, with no virtual destructor. The default surface test accepts every surface.
// Flag filters require all requested bits. Additional unrecovered predicates remain [FRONTIER].

#ifndef SCENE_OBJECT_OBJECTPREDICATOR_H     // [INFERRED] guard style
#define SCENE_OBJECT_OBJECTPREDICATOR_H

namespace dice {
namespace hfe {
namespace world {

class IObject;

class ObjectPredicator
{
public:
    virtual bool         predicate(IObject* object) = 0;            // +0x00
    virtual unsigned int getFlags() const = 0;                      // +0x04
    virtual bool         performCulling(IObject* object) = 0;       // +0x08
    virtual bool         predicateSurface(int surface);             // +0x0C  0x8163FC0 -> true
protected:
    ~ObjectPredicator() {}                                          // non-virtual, as the tables say
};

class ObjectFlagPredicator : public ObjectPredicator
{
public:
    explicit ObjectFlagPredicator(unsigned int flags) : m_flags(flags) {}
    virtual bool         predicate(IObject* object);                // all bits set
    virtual unsigned int getFlags() const { return m_flags; }
    virtual bool         performCulling(IObject*) { return true; }

    unsigned int m_flags;                                           // +0x04
};

// Filters combine required/forbidden flags, object/root exclusion or interface membership.
// No virtual destructor is added. ObjectInterfacePredicator's unused word remains [UNRESOLVED role].
class ObjectFlagPredicatorEx : public ObjectPredicator
{
public:
    // PE 0x101C7ED0: RendDX9's copy of this constructor; unreachable in 1.50: no code or data references.
    ObjectFlagPredicatorEx(unsigned int flags, unsigned int clearFlags)
        : m_flags(flags), m_clearFlags(clearFlags) {}
    virtual bool         predicate(IObject* object);
    virtual unsigned int getFlags() const { return m_flags; }
    virtual bool         performCulling(IObject*) { return true; }

    unsigned int m_flags;                                           // +0x04
    unsigned int m_clearFlags;                                      // +0x08
};

class ObjectIgnorePredicator : public ObjectFlagPredicator
{
public:
    // PE 0x101C7F90: RendDX9's copy of this constructor; unreachable in 1.50: no code or data references.
    ObjectIgnorePredicator(unsigned int flags, IObject* ignored)
        : ObjectFlagPredicator(flags), m_ignored(ignored) {}
    virtual bool predicate(IObject* object);

    IObject* m_ignored;                                             // +0x08
};

class RootObjectIgnorePredicator : public ObjectFlagPredicator
{
public:
    RootObjectIgnorePredicator(unsigned int flags, IObject* object);   // stores getRootParent(object)
    virtual bool predicate(IObject* object);

    IObject* m_root;                                                // +0x08
};

class ObjectInterfacePredicator : public ObjectFlagPredicator
{
public:
    // PE 0x101C7FE0: RendDX9's copy of this constructor; unreachable in 1.50: no code or data references.
    ObjectInterfacePredicator(unsigned int flags, unsigned int iid)
        : ObjectFlagPredicator(flags), m_iid(iid) {}
    virtual bool predicate(IObject* object);

    unsigned int _unused08;                                         // +0x08 [UNRESOLVED] - retail leaves it untouched
    unsigned int m_iid;                                             // +0x0C
};

// off_88C98D0 / PE off_7FA0A8. GCC constructs ObjectInterfacePredicator and then stamps this
// derived vptr inline at all three SoldierResponse use sites. The only changed slot is predicate:
// reject objects which answer the interface, then apply the inherited flag mask. No separate ctor
// symbol is emitted and the inherited 16-byte layout is unchanged.
class ObjectIgnoreInterfacePredicator : public ObjectInterfacePredicator
{
public:
    ObjectIgnoreInterfacePredicator(unsigned int flags, unsigned int iid)
        : ObjectInterfacePredicator(flags, iid) {}
    virtual bool predicate(IObject* object);
};

// Accepts objects a player can control. Uses the base flag/culling behavior with mask zero.
class PCOPredicator : public ObjectFlagPredicator
{
public:
    PCOPredicator() : ObjectFlagPredicator(0) {}                    // inlined at every use site
    virtual bool predicate(IObject* object);
};

// Filters ability targets by root armor and positive material damage modifier.
// Optional armorless parachutes pass directly; projectile fallback uses the base ignore test.
// Destroyed roots require the armor-template exception.
class AbilityPredicator : public ObjectIgnorePredicator
{
public:
    AbilityPredicator(unsigned int flags, IObject* ignored, unsigned int abilityMaterial,
                      bool allowParachute)
        : ObjectIgnorePredicator(flags, ignored)
        , m_abilityMaterial(abilityMaterial)
        , m_allowParachute(allowParachute) {}
    virtual bool predicate(IObject* object);

    unsigned int m_abilityMaterial;                                 // +0x0C - KitTemplate's
    bool         m_allowParachute;                                  // +0x10
};

// Projectile-ray predicate requiring object flags, excluding root flags and the roots
// of two supplied objects. Keep its 20-byte x86 field order.
class ProjectileVsTargetPredicator : public ObjectPredicator
{
public:
    ProjectileVsTargetPredicator(unsigned int flags, unsigned int rootExcludeFlags,
                                 IObject* excluded, IObject* excluded2)
        : m_flags(flags), m_rootExcludeFlags(rootExcludeFlags),
          m_excluded(excluded), m_excluded2(excluded2) {}
    virtual bool         predicate(IObject* object);
    virtual unsigned int getFlags() const;
    virtual bool         performCulling(IObject* object);           // true
    virtual bool         predicateSurface(int surface);

    unsigned int m_flags;                                           // +0x04
    unsigned int m_rootExcludeFlags;                                // +0x08
    IObject*     m_excluded;                                        // +0x0C
    IObject*     m_excluded2;                                       // +0x10
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // SCENE_OBJECT_OBJECTPREDICATOR_H
