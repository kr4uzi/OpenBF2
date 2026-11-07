// [CHOSEN, not recovered] Run-time binding of the real d3d9.dll / d3dx9_25.dll for the G2w
// forwarding backend. The forwarding objects define Direct3DCreate9 and the D3DX functions under
// their SDK names, so the real DLLs cannot be import-linked; they are bound by name here.
#if defined(_WIN32) && defined(BF2_GFX_FORWARD_TU)
#include <windows.h>
#include <cstdio>
#include <cstdlib>

extern "C" void* bf2fwd_GetProc(const char* module, const char* name) {
    static bool announced = false;
    if (!announced) {   // one line in the startup log proves the seam build is the one running
        announced = true;
        std::fprintf(stderr, "[bf2fwd] BF2_GFX_SEAM forwarding backend active (d3d9.dll and D3DX bound at run time)\n");
        std::fflush(stderr);
    }
    HMODULE h = LoadLibraryA(module);
    void* p = h ? reinterpret_cast<void*>(GetProcAddress(h, name)) : nullptr;
    if (!p) {
        std::fprintf(stderr, "[FRONTIER] bf2fwd (BF2_GFX_SEAM): cannot resolve %s!%s (error %lu)\n",
                     module, name, GetLastError());
        std::fflush(stderr);
        std::abort();
    }
    return p;
}
#endif
