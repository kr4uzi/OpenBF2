// [CHOSEN, not recovered] <dxerr9.h> of the no-SDK D3DX build. Only on the include path when
// Code/Build/dxsdk.cmd selected the fallback. The two functions the rebuilt DLLs call came from the
// SDK's DxErr9.lib; without it renddx9.cmd and textureatlasbuilder.cmd compile
// ../DxErr9StandIn.cpp, which formats the HRESULT instead of naming or describing it.
#ifndef RENDDX9ABI_DXERR9_FALLBACK_H
#define RENDDX9ABI_DXERR9_FALLBACK_H
#include <windows.h>
extern "C" const char* WINAPI DXGetErrorString9A(HRESULT hr);
extern "C" const char* WINAPI DXGetErrorDescription9A(HRESULT hr);
#endif
