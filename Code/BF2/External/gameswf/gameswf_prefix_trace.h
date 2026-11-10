// [TEMP] [CHOSEN, not recovered] Owner-requested PREFIX investigation.
// [RETIRE-WHEN: ui-prefix-trace] Remove this header and its call sites after the
// missing login label is explained and verified in initial/Manage/Back captures.
#ifndef GAMESWF_PREFIX_TRACE_H
#define GAMESWF_PREFIX_TRACE_H

#include "gameswf_impl.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace gameswf
{
inline bool prefix_trace_enabled()
{
    static const bool enabled = []() {
        const char* value = std::getenv("BF2_SWF_PREFIX_TRACE");
        return value && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

inline bool prefix_trace_target(movie* object)
{
    if (!prefix_trace_enabled()) return false;
    for (int i = 0; object && i < 32; ++i)
    {
        character* ch = object->cast_to_character();
        if (!ch) break;
        if (ch->get_name() == "prefixField") return true;
        object = ch->get_parent();
    }
    return false;
}

inline void prefix_trace_log(const char* format, ...)
{
    if (!prefix_trace_enabled()) return;
    std::fprintf(stderr, "[ui-prefix] ");
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

// No ActionScript coercion or member lookup: primitive values alone are printed.
inline void prefix_trace_member(movie* object, const tu_stringi& name, const as_value& value)
{
    if (!prefix_trace_target(object)) return;
    const char* text = value.get_type() == as_value::STRING ? value.to_string() : "";
    const double number = value.get_type() == as_value::NUMBER ? value.to_number() :
        value.get_type() == as_value::BOOLEAN ? (value.to_bool() ? 1.0 : 0.0) : 0.0;
    prefix_trace_log("member object=%p key='%s' type=%d string='%.96s' number=%.9g",
        static_cast<void*>(object), name.c_str(), int(value.get_type()), text, number);
}

inline void prefix_trace_geometry(character* ch, const char* event)
{
    if (ch)
    {
        const matrix world = ch->get_world_matrix();
        const cxform color = ch->get_world_cxform();
        prefix_trace_log("%s world object=%p matrix=(%.6g,%.6g,%.6g;%.6g,%.6g,%.6g) alpha=(%.6g,%.6g)",
            event, static_cast<void*>(ch), world.m_[0][0], world.m_[0][1], world.m_[0][2],
            world.m_[1][0], world.m_[1][1], world.m_[1][2], color.m_[3][0], color.m_[3][1]);
    }
    for (int i = 0; ch && i < 32; ++i)
    {
        const matrix& m = ch->get_matrix();
        const cxform& cx = ch->get_cxform();
        prefix_trace_log("%s ancestor=%d object=%p name='%s' id=%d depth=%d visible=%d "
            "alpha=(%.6g,%.6g) matrix=(%.6g,%.6g,%.6g;%.6g,%.6g,%.6g) clip=%d mask=%p maskOnly=%d",
            event, i, static_cast<void*>(ch), ch->get_name().c_str(), ch->get_id(), ch->get_depth(),
            int(ch->get_visible()), cx.m_[3][0], cx.m_[3][1],
            m.m_[0][0], m.m_[0][1], m.m_[0][2], m.m_[1][0], m.m_[1][1], m.m_[1][2],
            ch->get_clip_depth(), static_cast<void*>(ch->m_mask.get_ptr()), int(ch->m_is_mask_only));
        movie* parent = ch->get_parent();
        ch = parent ? parent->cast_to_character() : NULL;
    }
}

// Keep periodic evidence of display-list traversal without logging every frame.
inline bool prefix_trace_display(movie* object, bool text = false)
{
    if (!prefix_trace_target(object)) return false;
    static std::map<movie*, unsigned int> visits[2];
    unsigned int& count = visits[text ? 1 : 0][object];
    ++count;
    return count <= 2 || count % 120 == 0;
}
}
#endif
