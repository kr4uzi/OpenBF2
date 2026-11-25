#pragma once

// [CHOSEN, not recovered] macOS render sizes and window content sizes in logical points.
// No display mode is switched by the Metal host.
#include "../Host/ClientHost.h"
#include <algorithm>
#include <cmath>

namespace dice { namespace hfe { namespace macos {

struct DisplayMode
{
    int width, height, pixelWidth, pixelHeight, refreshHz;
    bool usable;
};

// Offer GUI-usable logical sizes, bounded in each dimension by the largest HiDPI logical
// size when present. A 1x display keeps all usable modes. Unknown rates use the panel rate.
inline std::vector<host::HostDisplayModeInfo> selectDisplayModes(
    const std::vector<DisplayMode>& available, int panelRefreshHz)
{
    int maxWidth = 0, maxHeight = 0;
    for (const auto& mode : available)
        if (mode.usable && (mode.pixelWidth > mode.width || mode.pixelHeight > mode.height))
        {
            maxWidth = std::max(maxWidth, mode.width);
            maxHeight = std::max(maxHeight, mode.height);
        }
    std::vector<host::HostDisplayModeInfo> modes;
    for (const auto& mode : available)
        if (mode.usable && (!maxWidth || (mode.width <= maxWidth && mode.height <= maxHeight)))
            modes.push_back({mode.width, mode.height,
                             mode.refreshHz ? mode.refreshHz : panelRefreshHz, 32});
    std::sort(modes.begin(), modes.end(), [](const host::HostDisplayModeInfo& a,
                                           const host::HostDisplayModeInfo& b) {
        if (a.width != b.width) return a.width < b.width;
        if (a.height != b.height) return a.height < b.height;
        return a.refreshHz < b.refreshHz;
    });
    modes.erase(std::unique(modes.begin(), modes.end(), [](const host::HostDisplayModeInfo& a,
                                                         const host::HostDisplayModeInfo& b) {
        return a.width == b.width && a.height == b.height && a.refreshHz == b.refreshHz;
    }), modes.end());
    return modes;
}

struct WindowContentSize { double width, height; };

// Keep one point per back-buffer pixel when it fits, otherwise use the largest proportional
// content size that fits the visible screen after window decorations have been subtracted.
inline WindowContentSize fitWindowContent(double width, double height, double maxWidth, double maxHeight)
{
    if (width <= 0 || height <= 0 || maxWidth <= 0 || maxHeight <= 0)
        return {width, height};
    const double scale = std::min(1.0, std::min(maxWidth / width, maxHeight / height));
    return {std::floor(width * scale), std::floor(height * scale)};
}

} } }
