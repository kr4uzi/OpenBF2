// [TEMP] Headless renderer/options regression fixture. Included instead of VideoOptions.cpp in
// a private test copy of the renderer archive so its real list builder needs no public test API.
// No engine initialization, window, device creation, content, or audio. Run via macos_resolution_list.py.
#include "../BF2/RendDX9/VideoOptions.cpp"
#include <cstdlib>

namespace {
void require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

// [TEMP] Only adapter metadata/capabilities are synthetic. The renderer list builder, option
// formatting, selection and preset bodies are production code. Unused adapter calls abort.
struct MetadataAdapter : IDirect3D9
{
    std::vector<dice::hfe::host::HostDisplayModeInfo> modes;
    dice::hfe::host::HostDisplayModeInfo desktop = {1680, 1050, 60, 32};
    MetadataAdapter()
    {
        require(std::sscanf(std::getenv("BF2_TEST_DESKTOP_MODE"), "%dx%d@%d", &desktop.width,
                            &desktop.height, &desktop.refreshHz) == 3, "fixture desktop syntax");
        const char* p = std::getenv("BF2_TEST_DISPLAY_MODES");
        while (p && *p) {
            int w, h, hz, used = 0;
            require(std::sscanf(p, "%dx%d@%d%n", &w, &h, &hz, &used) == 3 && used > 0, "fixture mode syntax");
            modes.push_back({w, h, hz, 32});
            p += used;
            if (*p == ',') ++p;
        }
    }
    UINT GetAdapterModeCount(UINT, D3DFORMAT format) override
    { require(format == D3DFMT_X8R8G8B8, "renderer enumerates 32-bit modes"); return modes.size(); }
    HRESULT EnumAdapterModes(UINT, D3DFORMAT, UINT i, D3DDISPLAYMODE* out) override
    { const auto& m = modes.at(i); *out = {UINT(m.width), UINT(m.height), UINT(m.refreshHz), D3DFMT_X8R8G8B8}; return D3D_OK; }
    HRESULT GetAdapterDisplayMode(UINT, D3DDISPLAYMODE* out) override
    { *out = {UINT(desktop.width), UINT(desktop.height), UINT(desktop.refreshHz), D3DFMT_X8R8G8B8}; return D3D_OK; }
    HRESULT CheckDeviceFormat(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) override
    { return D3D_OK; }
    HRESULT CheckDeviceMultiSampleType(UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE sample, DWORD* quality) override
    { *quality = 1; return sample == D3DMULTISAMPLE_NONE ? D3D_OK : D3DERR_NOTAVAILABLE; }
    HRESULT QueryInterface(REFIID, void**) override { std::abort(); }
    ULONG AddRef() override { std::abort(); }
    ULONG Release() override { std::abort(); }
    HRESULT RegisterSoftwareDevice(void*) override { std::abort(); }
    UINT GetAdapterCount() override { std::abort(); }
    HRESULT GetAdapterIdentifier(UINT, DWORD, D3DADAPTER_IDENTIFIER9*) override { std::abort(); }
    HRESULT CheckDeviceType(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL) override { std::abort(); }
    HRESULT CheckDepthStencilMatch(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) override { std::abort(); }
    HRESULT CheckDeviceFormatConversion(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT) override { std::abort(); }
    HRESULT GetDeviceCaps(UINT, D3DDEVTYPE, D3DCAPS9*) override { std::abort(); }
    HMONITOR GetAdapterMonitor(UINT) override { std::abort(); }
    HRESULT CreateDevice(UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**) override { std::abort(); }
};
}

// The test driver selects this entry point instead of the linked engine's main.
extern "C" int resolutionListCheck(int, const char**)
{
    using namespace dice::hfe;
    using namespace dice::hfe::dx9rend;
    CapsView caps = {};  // [TEMP] Unknown GPU: the preset's shipped non-whitelist flag is 1.
    g_deviceCaps = &caps;
    MetadataAdapter adapter;
    RendDX9Base base = {};
    base.d3d9 = &adapter;
    base.deviceType = D3DDEVTYPE_HAL;
    base.pp.Windowed = TRUE;
    base.depthFormats.push_back(D3DFMT_D24S8);
    VideoOptions& vo = base.videoOptions;
    vo.vptr = const_cast<void**>(kVideoOptionsVtable);
    vo.minusOneDC = vo.minusOneD8 = -1;
    for (int i = 0; i < 8; ++i) { vo.rowC[i] = 0; vo.maxQuality[i] = 3; }
    buildResolutionList(&vo);
    const auto& modes = videoOptions_getDynamicOptionList(&vo, 1);
    const std::vector<std::string> expected = {
        "800x600@60Hz", "825x525@60Hz", "1024x640@60Hz", "1024x768@60Hz", "1152x720@60Hz",
        "1280x800@60Hz", "1440x900@60Hz", "1650x1050@60Hz", "1680x1050@60Hz"};
    require(modes == expected, "logical Intel list reaches options in order, excluding widths under 800");
    for (const auto& mode : modes)
    {
        videoOptions_setDynamicOption(&vo, 1, &mode);
        require(vo.resList[vo.minusOneDC] == mode, "every offered mode selects itself without fallback");
        std::printf("mode: %s\n", mode.c_str());
    }
    const std::string desktop("1680x1050@60Hz");
    videoOptions_setDynamicOption(&vo, 1, &desktop);
    videoOptions_applyQualityPreset(&vo, 1);
    require(vo.resList[vo.minusOneDC] == "800x600@60Hz", "Medium explicitly selects retail 800x600");
    require(vo.quality[0] == 2 && vo.quality[7] == 2 && vo.dynFloat == 0.9f,
            "Medium stages level 2 and view distance 0.9");
    videoOptions_setDynamicOption(&vo, 1, &desktop);
    require(vo.resList[vo.minusOneDC] == desktop, "desktop can be selected after Medium");
    g_deviceCaps = nullptr;
    std::puts("PASS: renderer mode list -> VideoOptions list/selection, Medium preset, desktop reselection");
    return 0;
}
