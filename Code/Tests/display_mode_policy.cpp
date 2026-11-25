// [TEMP] Content-free checks of the native hosts' display-mode policy (RendMisc/DisplayModePolicy.h):
// the fullscreen start mode and the new-profile resolution. The mode lists are those the hosts
// report (logical sizes on macOS). No device, renderer or profile is modelled.
#include "../BF2/RendMisc/DisplayModePolicy.h"
#include "../BF2/Menu/MenuViewport.h"
#include <cstdio>
#include <vector>
#if defined(__APPLE__)
#include "../BF2/Platform/macOS/DisplayPolicy.h"
#endif

namespace dm = dice::hfe::displaymode;
namespace {
unsigned checks = 0, failures = 0;
void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
}
int pick(const std::vector<dm::Mode>& modes, dm::Mode desktop, unsigned minWidth = 800)
{
    return modes.empty() ? -1 : dm::preferredMode(&modes[0], modes.size(), desktop, minWidth);
}
bool is(const std::vector<dm::Mode>& modes, int index, unsigned w, unsigned h, unsigned hz)
{
    return index >= 0 && modes[index].width == w && modes[index].height == h && modes[index].refresh == hz;
}
}

int main()
{
#if defined(__APPLE__)
    using dice::hfe::host::HostDisplayModeInfo;
    using dice::hfe::macos::DisplayMode;
    using dice::hfe::macos::selectDisplayModes;
    using dice::hfe::macos::fitWindowContent;
    const std::vector<DisplayMode> intelAvailable = {
        {1680,1050,3360,2100,0,true}, {1440,900,2880,1800,0,true},
        {1280,800,2560,1600,0,true}, {1024,640,2048,1280,0,true},
        {825,525,1650,1050,0,true}, {720,450,1440,900,0,true},
        {2880,1800,2880,1800,0,true}, {2560,1600,2560,1600,0,true},
        {2048,1280,2048,1280,0,true}, {1650,1050,1650,1050,0,true},
        {1440,900,1440,900,0,true}, {1280,800,1280,800,0,true},
        {1152,720,1152,720,0,true}, {1024,768,1024,768,0,true},
        {800,600,800,600,0,true}, {840,524,840,524,0,false}, {640,480,640,480,0,false}};
    const auto logicalIntel = selectDisplayModes(intelAvailable, 60);
    const std::vector<HostDisplayModeInfo> expectedIntel = {
        {720,450,60,32}, {800,600,60,32}, {825,525,60,32}, {1024,640,60,32},
        {1024,768,60,32}, {1152,720,60,32}, {1280,800,60,32}, {1440,900,60,32},
        {1650,1050,60,32}, {1680,1050,60,32}};
    const auto sameModes = [](const std::vector<HostDisplayModeInfo>& a,
                              const std::vector<HostDisplayModeInfo>& b) {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
            [](const HostDisplayModeInfo& x, const HostDisplayModeInfo& y) {
                return x.width == y.width && x.height == y.height &&
                       x.refreshHz == y.refreshHz && x.bitsPerPixel == y.bitsPerPixel;
            });
    };
    check(sameModes(logicalIntel, expectedIntel),
          "Intel Retina: sorted logical sizes, deduplicated, GUI-usable, 0 Hz becomes 60");
    const std::vector<DisplayMode> m2Available = {
        {1710,1112,3420,2224,0,true}, {1470,956,2940,1912,0,true},
        {1280,832,2560,1664,0,true}, {1024,665,2048,1330,0,true},
        {2560,1664,2560,1664,0,true}, {2940,1912,2940,1912,0,true}};
    const std::vector<HostDisplayModeInfo> expectedM2 = {
        {1024,665,60,32}, {1280,832,60,32}, {1470,956,60,32}, {1710,1112,60,32}};
    check(sameModes(selectDisplayModes(m2Available, 60), expectedM2),
          "M2 Retina: logical sizes include the desktop and omit oversized 1x modes");
    check(sameModes(selectDisplayModes({{1920,1080,1920,1080,60,true},
                                       {2560,1440,2560,1440,144,true},
                                       {1920,1080,1920,1080,120,true},
                                       {1920,1080,1920,1080,60,true}}, 60),
                    {{1920,1080,60,32}, {1920,1080,120,32}, {2560,1440,144,32}}),
          "1x display: keep all usable sizes and distinct nonzero refresh rates");
    check(selectDisplayModes({{1000,700,2000,1400,0,true},
                              {1100,600,1100,600,0,true},
                              {900,800,900,800,0,true},
                              {2000,1400,4000,2800,0,false}}, 60).size() == 1,
          "HiDPI cap rejects excess width or height; unusable HiDPI does not enlarge it");
    check(selectDisplayModes({}, 60).empty(), "macOS: empty enumeration stays empty");
    const auto fitted = fitWindowContent(1440, 900, 1440, 853);
    check(fitted.width == 1364 && fitted.height == 853,
          "Retina desktop buffer fits around menu/title bars instead of halving");
    const auto smallWindow = fitWindowContent(800, 600, 1440, 853);
    check(smallWindow.width == 800 && smallWindow.height == 600, "fitting buffer keeps one point per pixel");
    const auto largeWindow = fitWindowContent(2880, 1800, 1440, 853);
    check(largeWindow.width == fitted.width && largeWindow.height == fitted.height,
          "larger same-aspect buffer occupies the same available area");
    const auto wideWindow = fitWindowContent(2560, 1080, 1440, 853);
    check(wideWindow.width == 1440 && wideWindow.height == 607, "ultrawide window is width limited");
    const auto portrait = fitWindowContent(900, 1440, 1440, 853);
    check(portrait.width == 533 && portrait.height == 853, "portrait window is height limited");
#endif
    // Logical sizes from the macOS host include each Retina desktop point size.
    const std::vector<dm::Mode> intel = {
        {720,450,60}, {800,600,60}, {825,525,60}, {1024,640,60}, {1024,768,60},
        {1152,720,60}, {1280,800,60}, {1440,900,60}, {1650,1050,60}, {1680,1050,60}};
    check(is(intel, pick(intel, {1680, 1050, 60}), 1680, 1050, 60), "Intel Retina: desktop point size");
    check(dm::listsSize(&intel[0], intel.size(), 800, 600), "Intel lists 800x600: the start keeps it");

    // MacBook Air M2 (2560x1664 panel, desktop 1470x956 points).
    const std::vector<dm::Mode> m2 = {
        {1024,665,60}, {1280,832,60}, {1470,956,60}, {1710,1112,60}};
    check(!dm::listsSize(&m2[0], m2.size(), 800, 600), "M2 has no 800x600");
    check(is(m2, pick(m2, {1470, 956, 60}), 1470, 956, 60), "M2: desktop point size");
    check(is(m2, pick(m2, {1280, 832, 60}), 1280, 832, 60), "M2 at 'larger text': desktop point size");
    // A desktop with no whole multiple listed: the smallest larger mode.
    const std::vector<dm::Mode> backing = {{2048,1330,60}, {2560,1664,60}, {2940,1912,60}, {3420,2224,60}};
    check(is(backing, pick(backing, {1600, 1000, 60}), 2048, 1330, 60), "no multiple: smallest larger mode");
    // A 3x multiple counts when 2x is missing.
    const std::vector<dm::Mode> notMultiple = {{3000, 2000, 60}, {2400, 1500, 60}};
    check(is(notMultiple, pick(notMultiple, {1000, 600, 60}), 2400, 1500, 60),
          "3000x2000 is no multiple of 1000x600: smallest larger mode");
    const std::vector<dm::Mode> triple2 = {{3000, 1800, 60}, {2600, 1600, 60}};
    check(is(triple2, pick(triple2, {1000, 600, 60}), 3000, 1800, 60), "3x backing when 2x is absent");

    // Refresh ties: the desktop's rate first, then the highest.
    const std::vector<dm::Mode> rates = {{1920, 1080, 60}, {1920, 1080, 144}, {1920, 1080, 120}};
    check(is(rates, pick(rates, {1920, 1080, 120}), 1920, 1080, 120), "desktop refresh wins a tie");
    check(is(rates, pick(rates, {1920, 1080, 75}), 1920, 1080, 144), "otherwise the highest refresh");
    const std::vector<dm::Mode> pro = {{3024, 1964, 60}, {3024, 1964, 120}, {2048, 1330, 120}};
    check(is(pro, pick(pro, {1512, 982, 120}), 3024, 1964, 120), "ProMotion backing at 120 Hz");

    // Windows/Linux: pixel desktop, the desktop's own mode fits.
    const std::vector<dm::Mode> deck = {{1280, 800, 60}, {800, 600, 60}, {640, 400, 60}};
    check(is(deck, pick(deck, {1280, 800, 60}), 1280, 800, 60), "Deck: the panel mode");

    // The 800-pixel floor of the resolution list, and the empty cases.
    const std::vector<dm::Mode> small = {{640, 480, 60}, {720, 450, 60}};
    check(pick(small, {1440, 900, 60}) < 0, "nothing at least 800 wide");
    check(pick(small, {1440, 900, 60}, 0) == 1, "without the floor the largest fitting");
    check(pick(m2, {0, 0, 0}) < 0, "unknown desktop");
    check(pick(std::vector<dm::Mode>(), {1440, 900, 60}) < 0, "empty list");

    // The HiDPI cursor factor: the menu viewport height over 600,
    // rounded down, so neither looks larger against the UI than in the 800x600 menu.
    using dice::hfe::ui::MenuViewport;
    check(MenuViewport::bitmapScale(800, 600) == 1, "800x600: 1");
    check(MenuViewport::bitmapScale(1024, 768) == 1, "1024x768: 1");
    check(MenuViewport::bitmapScale(1440, 900) == 1, "1440x900: 1");
    check(MenuViewport::bitmapScale(1650, 1050) == 1, "1650x1050: 1 (retail size)");
    check(MenuViewport::bitmapScale(1920, 1080) == 1, "1920x1080: 1");
    check(MenuViewport::bitmapScale(1920, 1200) == 2, "1920x1200: 2");
    check(MenuViewport::bitmapScale(2560, 1600) == 2, "2560x1600: 2");
    check(MenuViewport::bitmapScale(2560, 1664) == 2, "2560x1664: 2");
    check(MenuViewport::bitmapScale(2940, 1912) == 3, "2940x1912: 3");
    check(MenuViewport::bitmapScale(3840, 2160) == 3, "3840x2160: 3");
    check(MenuViewport::bitmapScale(2560, 1080) == 1, "21:9 2560x1080: height decides, 1");
    check(MenuViewport::bitmapScale(1200, 1600) == 1, "portrait 1200x1600: 4:3 width-limited (900), 1");
    check(MenuViewport::bitmapScale(0, 0) == 1, "degenerate: 1");

    std::printf("display_mode_policy: %u/%u passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
