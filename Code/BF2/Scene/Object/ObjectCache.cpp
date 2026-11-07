// Caches reusable objects by template and stored network ID (RW-5).
// Keep source-level map operations and their iteration order. Source placement is [INFERRED].

#include "ObjectCache.h"

#include <cstdio>                                  // the [TEMP] parented-push diagnostic
#include <cstdlib>

#include "../../Game/Objects/IObject.h"
#include "../../Game/Objects/IObjectTemplate.h"
#include "../../IO/Network/INetworkable.h"
#include "../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace world {

// [INFERRED] - no ELF constructor symbol exists (the counter is reset to 0 by deleteAll, and
// ObjectManager's constructor builds the instance). Zero is the only defensible seed.
ObjectCache::ObjectCache()
    : m_cacheIdCounter(0)
{
}

// Caches roots only; children abort and already-cached objects do nothing. Disables the
// object before assigning a fresh object id, invalidating weak pointers before insertion.
// A zero networkable key becomes the new object id + 0xFFFF.
// PE 0x00728930; ELF 0x0830D8D0. The parented-object stop is a shipped invariant.
void ObjectCache::pushObject(IObject* object, unsigned int networkableId)
{
    if (object->getFlags() & 0x20u)
        return;
    if (object->getParent())                           // +0x28 - a child must never be pooled
    {
        // [TEMP] [RETIRE-WHEN: warmup-green] the abort is the ELF's own; the print names the
        // offender so the first cache-live boot's divergence can be read off the log instead of
        // guessed. Remove the print (keep the abort) once the warm-up drains clean.
        IObject* const parent = object->getParent();
        std::fprintf(stderr,
            "[TEMP] ObjectCache::pushObject PARENTED object: '%s' (cacheID %u, netKey %u) "
            "parent '%s' (cacheID %u)\n",
            object->getTemplate() ? object->getTemplate()->getName().c_str() : "<no template>",
            object->getObjectCacheID(), networkableId,
            parent->getTemplate() ? parent->getTemplate()->getName().c_str() : "<no template>",
            parent->getObjectCacheID());
        std::fflush(stderr);
        std::abort();
    }

    const unsigned int templateId = object->getTemplate()->getId();
    object->disableCachedObject();                     // IObject slot 9 - parks it, sets flag 0x20
    object->internal_assignNewObjectID();              // IObject slot 85

    Pool& pool = m_pools[templateId];                  // the ELF's find + insert_unique(empty) pair

    unsigned int key = networkableId;
    if (key == 0)
        key = object->getObjectId() + 0xFFFFu;
    pool.insert(std::make_pair(key, object));
}

// Revives the requested cached id, or the last entry when id is zero. Applies an optional
// transform before enabling the object, then resets its networkable for an absolute first update.
IObject* ObjectCache::popObject(const IObjectTemplate* objectTemplate, unsigned int id,
                                const Mat4* transform)
{
    IObject* object = 0;
    PoolMap::iterator poolIt = m_pools.find(objectTemplate->getId());
    if (poolIt == m_pools.end() || poolIt->second.empty())
        return 0;

    Pool& pool = poolIt->second;
    if (id != 0)
    {
        Pool::iterator it = pool.find(id);
        if (it == pool.end())
            return 0;
        object = it->second;
        pool.erase(it);
    }
    else
    {
        Pool::iterator it = pool.end();
        --it;                                          // the rightmost = newest-keyed entry
        object = it->second;
        pool.erase(it);
    }

    if (object)
    {
        if (transform)
            object->setAbsoluteTransformation(*transform);
        object->enableCachedObject();                  // IObject slot 10 - the revive
        if (object->getNetworkable())
            object->getNetworkable()->reset();         // INetworkable vptr +0x1C
    }
    return object;
}

// markObjectAsCreatedInCache - 0x830D6B0 (18 B) + _internal 0x830D6D0 (70 B). One counter tick per
// top-level creation, then the whole child/sibling tree is stamped with that batch number - the
// same number NetworkManager::addDescriptor later records as a PCO descriptor's controlObjId.
void ObjectCache::markObjectAsCreatedInCache(IObject* object)
{
    ++m_cacheIdCounter;
    markObjectAsCreatedInCache_internal(object);
}

void ObjectCache::markObjectAsCreatedInCache_internal(IObject* object)
{
    while (object)
    {
        object->setObjectCacheID(m_cacheIdCounter);    // IObject slot 12
        markObjectAsCreatedInCache_internal(object->getChild());
        object = object->getSibling();                 // +0x2C
    }
}

// unmarkObjectAsCached - 0x830D650, 92 B. The inverse walk: cache id 0 and clear flag 0x20
// (updateFlags(0, 32) - the one place outside enableCachedObject that lowers the carcass bit).
// deleteAll runs it before the force-destroy so teardown sees ordinary objects.
void ObjectCache::unmarkObjectAsCached(IObject* object)
{
    while (object)
    {
        object->setObjectCacheID(0);
        object->updateFlags(0, 0x20u);
        unmarkObjectAsCached(object->getChild());
        object = object->getSibling();
    }
}

// Unmarks and force-destroys every pooled object in map order, then resets the count.
void ObjectCache::deleteAll()
{
    while (!m_pools.empty())
    {
        PoolMap::iterator poolIt = m_pools.begin();
        Pool& pool = poolIt->second;
        while (!pool.empty())
        {
            Pool::iterator it = pool.begin();
            IObject* object = it->second;
            pool.erase(it);
            unmarkObjectAsCached(object);
            object->forceDestroy();                    // IObject slot 14
        }
        m_pools.erase(poolIt);
    }
    m_cacheIdCounter = 0;
}

} // namespace world
} // namespace hfe
} // namespace dice
