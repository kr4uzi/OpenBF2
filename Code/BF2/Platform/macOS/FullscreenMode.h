#pragma once

namespace renddx9abi { struct HostDisplayMode; }

namespace dice { namespace hfe { namespace dx9rend {

// [CHOSEN, not recovered] Cocoa full screen keeps the renderer's applied video settings.
// Entering with an unlisted mode selects DisplayModePolicy's preferred mode. The requested
// fullscreen flag is retained; supported MSAA, depth, vsync and window ownership are copied.
// Requires a live renderer and adapter; stops if no usable full-screen mode is available.
void prepareMacFullscreenMode(void* renderer, renddx9abi::HostDisplayMode& mode);

} } }
