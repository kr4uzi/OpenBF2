// Expansion-light grouping helper owned by the Xp light coordinator.
// Placement follows the attested _r file Code/BF2/Geom/LightMerger.cpp; member and method
// spellings are [INFERRED].
#ifndef GEOM_LIGHTMERGER_H
#define GEOM_LIGHTMERGER_H

#ifdef BF2_IMAGE_RENDDX9

#include "../Math/Vec3.h"

#include <map>
#include <vector>

namespace dice {
namespace hfe {
namespace geom {

struct XpLight;

// A merged light group's two colours, pooled for the frame.
struct LightMergerRecord
{
    LightMergerRecord();                                   // PE 0x101938A0: all zero
    explicit LightMergerRecord(const XpLight* light);      // source colour, twice
    LightMergerRecord(const LightMergerRecord& other);     // member-wise copy

    // PE 0x10193940: first = the light manager's sun colour, second = its sky colour.
    // [INFERRED] name; unreachable in 1.50: no code or data references.
    void captureLightManagerColors();
    // PE 0x10193990: publishes second to the Ra constants (+0x1F8), then the manager's sun colour
    // (first) and sky colour (second). [INFERRED] name; unreachable in 1.50.
    void applyToLightManager() const;

    /* +0x00 */ Vec3 first;
    /* +0x0C */ Vec3 second;
};

// Groups the input lights by key and caches one pooled record per group. The coordinator clears
// it at the start of a light frame and finalizes it after the frame's geometry pairs are built.
// In the shipped image nothing inserts into the input map (its only inserter, PE 0x10193EB0, has no
// caller), so finalize finds it empty and the output cache and record pool stay empty.
class LightMerger
{
public:
    LightMerger();
    ~LightMerger();

    // Empties the input map and the output cache; the record pool is released separately.
    void clearTransient();
    // Walks the input map in key order, publishing each finished group, then clears the dirty
    // flag. The final group is never flushed, as in retail.
    void finalize();

    // Destroys every pooled record and empties the pool.
    static void releaseRecords();

    // [UNRESOLVED] The key's identity: its producer, PE 0x10193EB0, is unreferenced. Keys compare
    // as unsigned pointers.
    typedef const void* Key;

    // PE 0x10193EB0: marks the merger dirty and adds a light to the input map under a key.
    // [INFERRED] name; unreachable in 1.50: no code or data references.
    void addLight(Key key, XpLight* light);
    // PE 0x10194020: the record cached for a key, or null. [INFERRED] name; unreachable in 1.50.
    LightMergerRecord* findRecord(Key key);

private:
    LightMerger(const LightMerger&);
    LightMerger& operator=(const LightMerger&);

    /* +0x00 */ std::vector<XpLight*>*             m_group;
    /* +0x04 */ std::multimap<Key, XpLight*>        m_input;
    /* +0x10 */ std::map<Key, LightMergerRecord*>   m_output;
    /* +0x1C */ bool                                m_dirty;
};

// Turns the accumulated group into a pooled record, then empties the group.
LightMergerRecord* flushLightGroup(std::vector<XpLight*>& group);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9

#endif // GEOM_LIGHTMERGER_H
