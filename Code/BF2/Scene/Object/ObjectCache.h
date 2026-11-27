// Caches destroyed objects by template and network ID for later reuse. Header path [INFERRED].
// Parking disables the object, detaches components and refreshes its object ID/weak handles, while
// keeping its network descriptor alive. Both peers rely on that retained network ID during joins.
// Key zero uses objectId + 0xFFFF on push and the last map entry on pop.
// Creation batches stamp the whole child/sibling tree. Constructor initialization is [INFERRED].

#ifndef SCENE_OBJECT_OBJECTCACHE_H     // [INFERRED] guard style
#define SCENE_OBJECT_OBJECTCACHE_H

#include <map>

namespace dice {
namespace hfe {

struct Mat4;

namespace world {

class IObject;
class IObjectTemplate;

class ObjectCache
{
public:
    ObjectCache();                                     // [INFERRED] - see the header note

    void pushObject(IObject* object, unsigned int networkableId);
    IObject* popObject(const IObjectTemplate* objectTemplate,
                       unsigned int id, const Mat4* transform);
    void markObjectAsCreatedInCache(IObject* object);
    void unmarkObjectAsCached(IObject* object);
    void deleteAll();

private:
    friend class ObjectManager;
    void markObjectAsCreatedInCache_internal(IObject* object);

    typedef std::map<unsigned int, IObject*> Pool;      // key: networkableId, or objectId + 0xFFFF
    typedef std::map<unsigned int, Pool>     PoolMap;   // key: template id

    /* +0x00 */ PoolMap      m_pools;
    /* +0x0C */ unsigned int m_cacheIdCounter;          // one ++ per marked creation batch
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // SCENE_OBJECT_OBJECTCACHE_H
