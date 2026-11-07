// [CHOSEN, not recovered] Statically linked client plugin images for the macOS client.
//
// Retail BF2.exe loads RendDX9.dll, SwiffPlayer.dll and BF2Audio.dll at run time and resolves their
// exports by name. Retail-DLL swapping is retired (https://github.com/B2BF/OpenBF2/issues/7), so the
// macOS client links the three images into the executable instead (docs/macos-port.md). This table
// lets the engine's existing LoadLibrary/GetProcAddress call sites find them unchanged: the POSIX
// loader wrappers in Core/PluginDll.cpp consult it before dlopen. Only images built with
// BF2_STATIC_PLUGINS use it.

#ifndef STATICPLUGINS_STATICPLUGINS_H
#define STATICPLUGINS_STATICPLUGINS_H

namespace bf2staticplugins {

// Returns an opaque handle for the statically linked image whose file stem (the library name
// without directory or extension, compared case-insensitively, e.g. "RendDX9") is `stem`, or null
// when no such image is linked in.
void* find(const char* stem);

// True when `handle` was returned by find().
bool isStaticHandle(const void* handle);

// Returns the address of the export `name` of the image behind `handle`, or null when that image
// has no such export. `handle` must satisfy isStaticHandle().
void* symbol(const void* handle, const char* name);

// Menu re-entry. Retail BF2.exe frees SwiffPlayer.dll on every menu exit (FreeLibraryBf 0x446CE0:
// deinitDll, then FreeLibrary) and loads it again on the next entry, so each menu gets a fresh
// image: new module objects and a fresh module table for initDll to register. A linked image
// cannot be unmapped. For an image that is unloaded per use, unloadsWithDeinit() is true: the
// POSIX FreeLibraryBf then calls its deinitDll like the Win32 body, release() marks it unloaded,
// and the next find() rebuilds its module table before handing out the handle again. RendDX9,
// which BF2.exe frees once at engine shutdown (~BF2Render 0x4548B0 -> FreeLibraryBf 0x54CD80),
// also answers true on macOS, so its deinitDll closes the renderer modules there; it is never
// loaded again and needs no reload. BF2Audio (not freed through FreeLibraryBf) keeps the plain
// "stays mapped" behaviour.
bool unloadsWithDeinit(const void* handle);
void release(const void* handle);

} // namespace bf2staticplugins

#endif // STATICPLUGINS_STATICPLUGINS_H
