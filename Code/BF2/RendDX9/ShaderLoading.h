// Renderer loading-screen progress shared by the two effect compilers.
#ifndef BF2_RENDDX9_SHADERLOADING_H
#define BF2_RENDDX9_SHADERLOADING_H

#include "../Core/LoadStat.h"
#include "../Core/EventManager.h"
#if !defined(_MSC_VER)
#include "../Platform/X87Compat.h"
#endif

namespace dice { namespace hfe { namespace dx9rend {

// PE 0x10022310; also inlined at 0x10035836..0x10035891.
inline float shaderLoadingPercent(LoadStat* stat)
{
    if (stat->m_unknown0B)
        return 100.0f;
    const unsigned int total = static_cast<unsigned int>(stat->m_unknown24);
    if (total && static_cast<unsigned int>(stat->m_loadCounter) >= total)
        stat->m_loadCounter = stat->m_unknown24;
#if defined(_MSC_VER)
    if (!total)
        return stat->m_unknown38 * 100.0f;
    return (static_cast<float>(static_cast<unsigned int>(stat->m_loadCounter)) /
            static_cast<float>(total) * (stat->m_unknown3C - stat->m_unknown38) +
            stat->m_unknown38) * 100.0f;
#else
    // [CHOSEN, not recovered] Retain the PE's x87 intermediates on native processors.
    using namespace bf2x87;
    if (!total)
        return static_cast<float>(mul(wide(stat->m_unknown38), wide(100.0f)));
    return static_cast<float>(mul(add(mul(
        div(wide(static_cast<unsigned int>(stat->m_loadCounter)), wide(total)),
        sub(wide(stat->m_unknown3C), wide(stat->m_unknown38))),
        wide(stat->m_unknown38)), wide(100.0f)));
#endif
}

// PE 0x1002308D..0x100230B2 and 0x1003582C..0x100358A6.
inline void shaderLoadingEvent()
{
    if (loadStat->m_loadCallback)
    {
        float percent = shaderLoadingPercent(loadStat);
        eventManager->postEvent(static_cast<EventCategory>(10), 4, &percent, 0.0f);
    }
}

} } }
#endif
