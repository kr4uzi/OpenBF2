#pragma once

// [CHOSEN, not recovered] The display-mode choice shared by the renderer's fullscreen start on the
// native hosts (RendDX9/BF2Render.cpp, macos-display-mode-policy) and the new-profile resolution
// (Game/VideoSettings.cpp). Pure functions over the listed modes, so the policy is testable without
// a device (Code/Tests/display_mode_policy.cpp).

#include <cstddef>

namespace dice { namespace hfe { namespace displaymode {

// One listed mode, or the desktop as the adapter reports it (IDirect3D9::GetAdapterDisplayMode:
// pixels on Windows and Linux, points on macOS, where a Retina desktop of 1470x956 points has a
// 2940x1912 pixel backing store).
struct Mode
{
    unsigned width;
    unsigned height;
    unsigned refresh;
};

// Whether `a` is preferred to `b` at equal size: the desktop's refresh rate first, then the higher.
inline bool betterRefresh(unsigned a, unsigned b, unsigned desktopRefresh)
{
    const bool aMatches = a == desktopRefresh, bMatches = b == desktopRefresh;
    if (aMatches != bMatches)
        return aMatches;
    return a > b;
}

// Index of the mode a new profile and a fullscreen start whose requested size the display does not
// list take, or -1 when `count` is 0, the desktop is unknown (0x0) or no mode is at least `minWidth`
// wide (the resolution list's own 800-pixel floor). The same rule on every platform (owner decision
// 2026-11-20, no performance cap):
//   1. the largest mode that fits the desktop. On Windows and Linux the desktop is reported in
//      pixels, so this is the desktop mode itself. macOS lists logical point sizes, so Retina
//      desktops select their point size (1470x956 on a 2940x1912 backing);
//   2. when none fits, the smallest mode that is a whole multiple (2x, 3x) of the desktop: the pixel
//      size of the image the system composites for this screen (the Retina backing store, 2940x1912
//      for a 1470x956-point desktop on the M2), so the drawable reaches it without rescaling,
//      wherever such a mode is listed;
//   3. otherwise the smallest mode larger than the desktop.
// Ties on size take the desktop's refresh rate, then the highest.
inline int preferredMode(const Mode* modes, std::size_t count, const Mode& desktop, unsigned minWidth)
{
    if (!modes || !count || !desktop.width || !desktop.height)
        return -1;
    int fit = -1, multiple = -1, larger = -1;
    for (std::size_t i = 0; i < count; ++i)
    {
        const Mode& m = modes[i];
        if (m.width < minWidth || !m.height)
            continue;
        const unsigned long long area = static_cast<unsigned long long>(m.width) * m.height;
        if (m.width <= desktop.width && m.height <= desktop.height)
        {
            const Mode* best = fit < 0 ? 0 : &modes[fit];
            const unsigned long long bestArea =
                best ? static_cast<unsigned long long>(best->width) * best->height : 0;
            if (!best || area > bestArea ||
                (area == bestArea && betterRefresh(m.refresh, best->refresh, desktop.refresh)))
                fit = static_cast<int>(i);
            continue;
        }
        const unsigned k = m.width / desktop.width;
        if (k >= 2 && m.width == k * desktop.width && m.height == k * desktop.height)
        {
            const Mode* best = multiple < 0 ? 0 : &modes[multiple];
            if (!best || m.width < best->width ||
                (m.width == best->width && betterRefresh(m.refresh, best->refresh, desktop.refresh)))
                multiple = static_cast<int>(i);
        }
        const Mode* best = larger < 0 ? 0 : &modes[larger];
        const unsigned long long bestArea =
            best ? static_cast<unsigned long long>(best->width) * best->height : 0;
        if (!best || area < bestArea ||
            (area == bestArea && betterRefresh(m.refresh, best->refresh, desktop.refresh)))
            larger = static_cast<int>(i);
    }
    return fit >= 0 ? fit : multiple >= 0 ? multiple : larger;
}

// Whether any listed mode has this size (any refresh rate).
inline bool listsSize(const Mode* modes, std::size_t count, unsigned width, unsigned height)
{
    for (std::size_t i = 0; i < count; ++i)
        if (modes[i].width == width && modes[i].height == height)
            return true;
    return false;
}

} } }
