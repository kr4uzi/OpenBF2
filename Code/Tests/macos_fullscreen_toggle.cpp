// [TEMP] Run the production BF2Render toggle without a window, device, game or audio.
// Only the window/reset boundary and event sink are fixtures. Private access seeds the same
// stale startup carrier and applied renderer record as a windowed profile start.
#include <string>
#include <vector>
#define private public
#include "../BF2/Game/Main/BF2ClientSeams.h"
#undef private
#include "../BF2/Game/Main/BF2Render.cpp"
#include "../BF2/Menu/MenuViewport.h"

extern "C" void fullscreenHeadlessReset(void*, const renddx9abi::HostDisplayMode&);

namespace {
using namespace dice::hfe;
void requireToggle(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: fullscreen toggle: %s\n", message); std::exit(1); }
}
std::vector<unsigned> sequence;
unsigned hostWidth, hostHeight;
bool resetComplete;

struct WindowFixture : BF2Window
{
    void toggleFullscreenStyle() override { m_fullscreen = !m_fullscreen; sequence.push_back(10); }
    void resizeClient(int w, int h) override
    { hostWidth = w; hostHeight = h; sequence.push_back(20); }
};

struct EventFixture : EventManager
{
    unsigned postEvent(EventCategory category, unsigned id, void* data, float delay) override
    {
        requireToggle(category == static_cast<EventCategory>(11) && delay == 0, "renderer event contract");
        sequence.push_back(id);
        if (id == 2)
        {
            requireToggle(resetComplete, "layout notification follows reset");
            const unsigned packed = *static_cast<const unsigned*>(data);
            const unsigned w = packed >> 16, h = packed & 0xffff;
            requireToggle(w == hostWidth && h == hostHeight, "layout payload matches resized host");
            const auto menu = ui::MenuViewport::fit(w, h);
            requireToggle(menu.x + menu.width <= w && menu.y + menu.height <= h,
                          "complete fitted menu is inside the new back buffer");
        }
        return id;
    }
};
}

// Called through the compiler-typed IRenderer::setDisplayMode fixture slot.
extern "C" bool fullscreenResetFixture(void* renderer, const renddx9abi::HostDisplayMode* mode)
{
    requireToggle(mode->width == hostWidth && mode->height == hostHeight, "host resized before Reset");
    fullscreenHeadlessReset(renderer, *mode);
    resetComplete = true;
    sequence.push_back(30);
    return true; // [TEMP] successful Reset boundary only; no GPU operation is modelled.
}

extern "C" void fullscreenToggleCheck(void* renderer, bool initiallyFullscreen,
                                      unsigned width, unsigned height, unsigned samples)
{
    WindowFixture window;
    EventFixture events;
    SettingsRepostitory repository;
    auto* const savedSettings = settings;
    auto* const savedEvents = eventManager;
    auto* const savedRender = g_bf2Render;
    settings = &repository;
    eventManager = &events;
    // Destruction would shut down the renderer module, which this fixture never initialized.
    // Placement construction exercises the real constructor without taking that unrelated path.
    alignas(BF2Render) unsigned char storage[sizeof(BF2Render)];
    auto* const render = new (storage) BF2Render(nullptr);
    renddx9abi::HostDisplayMode carrier;
    carrier.width = 800; carrier.height = 600; carrier.refreshHz = 85;
    carrier.fullscreen = initiallyFullscreen;
    render->m_window = &window;
    render->m_renderer = static_cast<dx9rend::IRenderer*>(renderer);
    render->m_displayModeCarrier = &carrier;
    for (unsigned i = 0; i != 2; ++i)
    {
        sequence.clear();
        resetComplete = false;
        hostWidth = 1680; hostHeight = 1050;
        requireToggle(render->toggleFullscreen() == 1, "toggle keeps its event result");
        requireToggle(sequence == std::vector<unsigned>({10, 20, 3, 30, 2, 1}),
                      "style, host resize, pre-change, reset, resolution, toggle order");
        requireToggle(carrier.width == width && carrier.height == height && carrier.refreshHz == 60,
                      "applied or preferred mode survives both directions");
        requireToggle(carrier.multiSamples.size() == 1 && carrier.multiSamples[0].type == samples,
                      "applied supported MSAA survives both directions");
        const bool fullscreen = i == 0 ? !initiallyFullscreen : initiallyFullscreen;
        requireToggle(carrier.fullscreen == fullscreen && repository.boolGet("GSFullScreen", false) == fullscreen,
                      "carrier and GSFullScreen agree in both directions");
    }
    settings = savedSettings;
    eventManager = savedEvents;
    g_bf2Render = savedRender;
}
