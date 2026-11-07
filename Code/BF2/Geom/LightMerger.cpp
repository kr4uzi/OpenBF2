// Expansion-light grouping helper owned by the Xp light coordinator (+0xB0).
// Placement follows the attested _r file Code/BF2/Geom/LightMerger.cpp.

#ifdef BF2_IMAGE_RENDDX9

#include "LightMerger.h"
#include "XpLightManager.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../Game/Objects/Visual/LightSource.h"
#include "../RendDX9/RendDX9Base.h"

#include <new>

namespace dice {
namespace hfe {
namespace geom {

extern LightManager* g_LightManager;                    // ModuleGeom.cpp, dword_1024B914

namespace {

// PE 0x10264104: the frame's pooled group records, released by LightMerger::releaseRecords.
std::vector<LightMergerRecord*> s_recordPool;

} // anonymous namespace

// PE 0x101938A0.
LightMergerRecord::LightMergerRecord()
{
    first.x = first.y = first.z = 0.0f;
    second.x = second.y = second.z = 0.0f;
}

// PE 0x101938C0. Both colours start as the light source's colour (LightSource +0x198).
LightMergerRecord::LightMergerRecord(const XpLight* light)
{
    first.x = first.y = first.z = 0.0f;
    second.x = second.y = second.z = 0.0f;
    second = xpLightSource(light)->getColor();
    first = second;
}

// PE 0x10193910. Unreachable in 1.50: no code or data references.
LightMergerRecord::LightMergerRecord(const LightMergerRecord& other)
{
    first.x = other.first.x; first.y = other.first.y; first.z = other.first.z;
    second.x = other.second.x; second.y = other.second.y; second.z = other.second.z;
}

// PE 0x10193940. The sun colour (manager slot +0x1AC) is read before the sky colour (slot +0x30).
// Unreachable in 1.50: no code or data references.
void LightMergerRecord::captureLightManagerColors()
{
    first = g_LightManager->getSunColor();
    second = g_LightManager->getSkyColor();
}

// PE 0x10193990. Unreachable in 1.50: no code or data references.
void LightMergerRecord::applyToLightManager() const
{
    float* const constants = reinterpret_cast<float*>(dx9rend::raShaderConstantsAt(0x1F8));
    constants[0] = second.x;
    constants[1] = second.y;
    constants[2] = second.z;
    g_LightManager->setSunColor(first);                // slot +0x1A8
    g_LightManager->setSkyColor(second);               // slot +0x2C
}

// PE 0x10193EB0. Unreachable in 1.50: no code or data references.
void LightMerger::addLight(Key key, XpLight* light)
{
    m_dirty = true;
    m_input.insert(std::make_pair(key, light));
}

// PE 0x10194020. Unreachable in 1.50: no code or data references.
LightMergerRecord* LightMerger::findRecord(Key key)
{
    if (m_output.count(key) != 0)
        return m_output[key];
    return 0;
}

// PE 0x10194080. The group vector is heap-owned (PE 0x101947B0 zeroes its three pointers).
LightMerger::LightMerger()
    : m_group(0), m_input(), m_output(), m_dirty(false)
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(std::vector<XpLight*>));
    m_group = storage ? new (storage) std::vector<XpLight*>() : 0;
}

// PE 0x10193EE0. Destroys the output cache, then the input map. Shipped defect: the +0x00 group
// vector is never released; its destructor, PE 0x101947C0, has no caller.
LightMerger::~LightMerger()
{
}

// PE 0x10193E40.
void LightMerger::clearTransient()
{
    m_input.clear();
    m_output.clear();
}

// PE 0x10193F40. A key change flushes the group built so far and caches that record under the
// NEW key; the lights of the last key are left in the group without a flush. Both are shipped
// behaviour.
void LightMerger::finalize()
{
    if (m_input.size() != 0)
    {
        Key previous = m_input.begin()->first;
        for (std::multimap<Key, XpLight*>::const_iterator it = m_input.begin();
             it != m_input.end(); ++it)
        {
            const Key key = it->first;
            XpLight* const light = it->second;
            if (key != previous)
            {
                LightMergerRecord* const record = flushLightGroup(*m_group);
                previous = key;
                m_output[key] = record;
            }
            m_group->push_back(light);
        }
    }
    m_dirty = false;
}

// PE 0x101948F0. The record destructor, PE 0x10193890, is empty.
void LightMerger::releaseRecords()
{
    for (std::vector<LightMergerRecord*>::iterator it = s_recordPool.begin();
         it != s_recordPool.end(); ++it)
    {
        LightMergerRecord* const record = *it;
        if (record)
        {
            record->~LightMergerRecord();
            ::renddx9abi::engineFree(record);
        }
    }
    s_recordPool.resize(0);
}

// PE 0x10194840. A nonempty group allocates a record from its first light and pools it; an empty
// group reuses the first pooled record (retail writes through a null record when the pool is also
// empty; finalize never flushes an empty group). The group is emptied and the record's colours
// are then overwritten with (0, 1, 0), replacing the constructor's source colour.
LightMergerRecord* flushLightGroup(std::vector<XpLight*>& group)
{
    LightMergerRecord* record = 0;
    if (group.size() != 0)
    {
        XpLight* const light = group[0];
        void* const storage = ::renddx9abi::engineAlloc(sizeof(LightMergerRecord));
        record = storage ? new (storage) LightMergerRecord(light) : 0;
        s_recordPool.push_back(record);
    }
    else if (s_recordPool.size() != 0)
    {
        record = s_recordPool[0];
    }
    group.resize(0);
    record->second.y = 1.0f;
    record->second.x = 0.0f;
    record->second.z = 0.0f;
    record->first.y = 1.0f;
    record->first.x = 0.0f;
    record->first.z = 0.0f;
    return record;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
