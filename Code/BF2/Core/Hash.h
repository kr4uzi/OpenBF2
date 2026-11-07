// Chained hash table with caller-computed bucket indices. Header path [INFERRED].
// The policy supplies both hashing and key comparison; exists() reuses the caller's bucket.
// StringHashFunc hashes without case but compares with case. StringHashNoCaseHashFunc ignores case
// for both operations; UIntHashFunc uses the integer key directly.
// [UNRESOLVED] original policy helper spelling; the compare() helper is [CHOSEN].
// Node layout follows next/key/value and therefore depends on the key and compiler's STL ABI.

#ifndef CORE_HASH_H     // [INFERRED] guard style
#define CORE_HASH_H

#include <string>
#include <string.h>     // strcasecmp / _stricmp - see hashKeyStrCaseCmp below

namespace dice {
namespace hfe {

// Free function, attested by name. Used by both instantiations above and by
// ObjectTemplateManager::createTemplate.
unsigned int calcStringHashValueNoCase(const std::string&);

// [CHOSEN, not recovered] Compares strings case-insensitively using the host C runtime.
inline int hashKeyStrCaseCmp(const char* a, const char* b)
{
#ifdef _MSC_VER
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}

// Case-insensitive string hashing and equality. [CHOSEN, not recovered] Equality-policy method name.
// [UNRESOLVED] Original functor body; behavior follows its observed callers.
struct StringHashNoCaseHashFunc
{
    unsigned int operator()(const std::string& s) const { return calcStringHashValueNoCase(s); }

    static bool equals(const std::string& stored, const std::string& key)
    {
        return hashKeyStrCaseCmp(stored.c_str(), key.c_str()) == 0;
    }
};

// Hashes strings without case but compares keys case-sensitively.
// [UNRESOLVED] Original functor body; behavior follows its observed callers.
struct StringHashFunc
{
    unsigned int operator()(const std::string& s) const { return calcStringHashValueNoCase(s); }

    // [CHOSEN] Equality-policy method name; key comparison is case-sensitive.
    static bool equals(const std::string& stored, const std::string& key) { return stored == key; }
};

// Hashes unsigned keys without mixing and compares them exactly.
// [UNRESOLVED] Original functor body; behavior follows its observed callers.
struct UIntHashFunc
{
    unsigned int operator()(unsigned int key) const { return key; }

    static bool equals(unsigned int stored, unsigned int key) { return stored == key; }
};

template <typename K, typename V, typename HashFunc>
class Hash
{
public:
    struct Node
    {
        Node*        next;     // +0x00
        K            key;      // +0x04
        V            value;    // +0x08
    };

    // Constructs maxSize buckets and explicitly zeroes them.
    // RendDX9.dll copy PE 0x1007B840 (`retn 8`: functor reference, maxSize; it stores the functor
    // address at +0x08, which this rebuild keeps null).
    // The default 3011 is [CHOSEN, not recovered] syntax for the observed standard bucket count.
    explicit Hash(unsigned int maxSize = 3011)
    {
        m_maxSize = maxSize;

        // [CHOSEN, not recovered] Keep the unused layout slot null instead of storing a dangling reference to a stateless functor.
        m_hashFunc = 0;

        m_buckets = new Node*[m_maxSize];
        for (unsigned int i = 0; i < m_maxSize; ++i)
            m_buckets[i] = 0;
    }

    // Deletes all nodes and the owned bucket array.
    ~Hash()
    {
        clear();
        delete[] m_buckets;
    }

    // Returns whether key exists in the specified bucket. The caller must supply a valid reduced bucket index.
    // RendDX9.dll copy (StringHashFunc) PE 0x100748F0.
    bool exists(const K& key, unsigned int bucket) const
    {
        for (Node* n = m_buckets[bucket]; n != 0; n = n->next)
        {
            if (HashFunc::equals(n->key, key))
                return true;
        }
        return false;
    }

    // Pushes a copied key/value onto the bucket chain without checking duplicates.
    // Callers must check exists first when duplicate nodes are unwanted.
    void insert(const K& key, const V& value, unsigned int bucket)
    {
        Node* n  = new Node;
        n->next  = m_buckets[bucket];
        n->key   = key;
        n->value = value;
        m_buckets[bucket] = n;
    }

    // Copies the matching value to value and returns true. A miss leaves value unchanged and returns false.
    // RendDX9.dll copy (StringHashFunc, string value) PE 0x1007BA20, unreached there.
    bool get(const K& key, V& value) const
    {
        for (Node* n = m_buckets[bucketOf(key)]; n != 0; n = n->next)
        {
            if (HashFunc::equals(n->key, key))
            {
                value = n->value;
                return true;
            }
        }
        return false;
    }

    // Deletes every node while retaining the bucket array and capacity for reuse.
    // RendDX9.dll copy (StringHashFunc) PE 0x10074A50.
    void clear()
    {
        for (unsigned int i = 0; i < m_maxSize; ++i)
        {
            Node* n = m_buckets[i];
            while (n != 0)
            {
                Node* dead = n;
                n = n->next;
                delete dead;          // ~Node releases value then key - the two _M_destroy pairs
            }
            m_buckets[i] = 0;
        }
    }

    // Removes the first matching key and returns true. Returns false when the key is absent.
    // RendDX9.dll copy (StringHashFunc) PE 0x100749C0.
    bool remove(const K& key)
    {
        const unsigned int bucket = bucketOf(key);

        Node* prev = 0;
        for (Node* n = m_buckets[bucket]; n != 0; n = n->next)
        {
            if (HashFunc::equals(n->key, key))
            {
                if (prev != 0)
                    prev->next = n->next;
                else
                    m_buckets[bucket] = n->next;

                delete n;
                return true;
            }
            prev = n;
        }
        return false;
    }

    unsigned int bucketOf(const K& key) const { return HashFunc()(key) % m_maxSize; }

    // Inserts key/value unless the key is already present; answers false for an existing key.
    // RendDX9.dll copy (StringHashFunc, string value) PE 0x1007B9A0 (`retn 8`): bucket from the
    // hash modulo +0x04, exists, then a 0x3C-byte node whose next is the old head, key and value
    // copy-constructed, stored as the head; true. The engine ELF inlines the same steps into
    // ConsoleObjects::registerTypeConversion. [PLATFORM] the PE engine allocator can return null,
    // and that arm stores the null as the bucket head (dropping the chain) and still answers true;
    // operator new here throws instead, so the arm cannot occur.
    // [INFERRED] name; the member is unreached in RendDX9 (only the dead wrapper PE 0x1007B310).
    bool insertUnique(const K& key, const V& value)
    {
        const unsigned int bucket = bucketOf(key);
        if (exists(key, bucket))
            return false;
        insert(key, value, bucket);
        return true;
    }
    // [CHOSEN, not recovered] Convenience overloads compute the bucket before forwarding.
    bool exists(const K& key) const          { return exists(key, bucketOf(key)); }
    void insert(const K& key, const V& value) { insert(key, value, bucketOf(key)); }

private:
    // [CHOSEN, not recovered] Disable copying because the table exclusively owns its bucket array and nodes.
    Hash(const Hash&);
    Hash& operator=(const Hash&);

public:
    // Public bucket fields support callers that walk entries before deleting their pointed-to values.
    // The third pointer preserves the x86 12-byte layout. Its original meaning is [UNRESOLVED]; it
    // resembles a reference to a stateless temporary hash functor and has no observed reader.
    Node**       m_buckets;    // +0x00  operator new[](maxSize * 4), zero-filled
    unsigned int m_maxSize;    // +0x04
    const void*  m_hashFunc;   // +0x08  [UNRESOLVED] - see above. [CHOSEN] type: void* keeps the
                               //        size right without asserting a reference member.
};

} // namespace hfe
} // namespace dice

#endif // CORE_HASH_H
