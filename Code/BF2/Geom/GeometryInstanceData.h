// Shared geometry-instance data prefix; this does not define the complete interface.
// [CHOSEN, not recovered] The name is the toolset's std::string on every target (issue #7), so
// the prefix uses native string and pointer sizes; geometry owners must use this prefix.
// The retail x86 prefix was 0x30 bytes, with the two opacity factors at +0x28 and +0x2C.
#ifndef BF2_GEOM_GEOMETRYINSTANCEDATA_H
#define BF2_GEOM_GEOMETRYINSTANCEDATA_H

#include <cstddef>
#if defined(_MSC_VER) && !defined(_WIN64)
#include <string>
#else
#include <string>
#endif

namespace dice { namespace hfe { namespace geom {

struct GeometryInstanceData
{
    explicit GeometryInstanceData(void* const* table)
        : m_vptr(table), m_dword20(0), m_dword24(0), m_float28(1.0f), m_float2C(1.0f) {}

    void* const* m_vptr;
#if defined(_MSC_VER) && !defined(_WIN64)
    std::string m_name;
#else
    std::string m_name;
#endif
    union { unsigned int m_dword20; unsigned int m_drawFrame; };
    union { unsigned int m_dword24; unsigned int m_visibleFrame; };
    float m_float28; // base opacity (Armor's destruction fade)
    float m_float2C; // distance/cull opacity

#if defined(BF2_STATIC_PLUGINS) && !defined(_MSC_VER)
    // [CHOSEN, not recovered] Native geometry owners still publish explicit PE
    // function tables, not compiler-emitted ELF tables. Their opacity entry is
    // ordinal 61. The audit checks every provider and this constant together.
    static constexpr unsigned nativeOpacitySlot = 61;
#endif
    // Sends the combined opacity to the geometry owner's transparency parameter.
    void setOpacity(float value)
    {
#ifdef _MSC_VER
        typedef void (__thiscall* Function)(void*, float);
        reinterpret_cast<Function>(m_vptr[61])(this, value);
#else
        typedef void (*Function)(void*, float);
#if defined(BF2_STATIC_PLUGINS)
        reinterpret_cast<Function>(m_vptr[nativeOpacitySlot])(this, value);
#else
        reinterpret_cast<Function>(m_vptr[62])(this, value);
#endif
#endif
    }
};

static_assert(offsetof(GeometryInstanceData, m_float28) ==
    sizeof(void*) + sizeof(std::string) + 2 * sizeof(unsigned int), "native geometry data prefix");

} } }
#endif
