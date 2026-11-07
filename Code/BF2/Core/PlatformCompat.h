// [CHOSEN, not recovered] Compiler/CRT spelling adapters for non-MSVC engine translation units.
// Force-include this header for C++ engine code; vendored C libraries provide their own portability.
// Only equivalent names belong here. OS operations require explicit platform implementations.
// Do not change serialized widths or introduce Win32 typedefs that hide width mismatches.
// Existing definitions take precedence through the local #ifndef guards.

#ifndef CORE_PLATFORMCOMPAT_H          // [CHOSEN] guard style, matching the tree's other Core headers
#define CORE_PLATFORMCOMPAT_H

#if !defined(_MSC_VER)

#include <strings.h>    // strcasecmp, strncasecmp
#include <string.h>     // strdup
#include <stdio.h>      // snprintf, vsnprintf
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>      // tolower, toupper - the _strlwr/_strupr helpers below
#include <wchar.h>      // wcscasecmp
#include <alloca.h>     // alloca
#include <cmath>        // std::isfinite, std::isnan

#if !defined(_WIN32)
// [CHOSEN, not recovered] POSIX Interlocked adapters (macOSplan.md, S2.10).
// Targets must be naturally aligned native words, never fields in packed file/packet data.
// Increment/decrement/add return the new value; exchange/add-exchange/CAS return the old value.
template<class T> inline T InterlockedIncrement(volatile T* target)
{ return __atomic_add_fetch(target, 1, __ATOMIC_SEQ_CST); }
template<class T> inline T InterlockedDecrement(volatile T* target)
{ return __atomic_sub_fetch(target, 1, __ATOMIC_SEQ_CST); }
template<class T, class V> inline T InterlockedAdd(volatile T* target, V value)
{ return __atomic_add_fetch(target, static_cast<T>(value), __ATOMIC_SEQ_CST); }
template<class T, class V> inline T InterlockedExchangeAdd(volatile T* target, V value)
{ return __atomic_fetch_add(target, static_cast<T>(value), __ATOMIC_SEQ_CST); }
template<class T, class V> inline T InterlockedExchange(volatile T* target, V value)
{ return __atomic_exchange_n(target, static_cast<T>(value), __ATOMIC_SEQ_CST); }
template<class T, class V, class C>
inline T InterlockedCompareExchange(volatile T* target, V value, C comparand)
{
    T previous = static_cast<T>(comparand);
    __atomic_compare_exchange_n(target, &previous, static_cast<T>(value), false,
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return previous;
}
#endif

// Win32 calling-convention spellings are empty for this non-MSVC native port.
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif
#ifndef __thiscall
#define __thiscall
#endif

// MSVC fixed-width integer spelling. `long long` is 64 bits on every toolchain this tree targets,
// and `unsigned __int64` must keep working, which is why this is a macro and not a typedef.
#ifndef __int64
#define __int64 long long
#endif
#ifndef __int32
#define __int32 int
#endif
#ifndef __int16
#define __int16 short
#endif
#ifndef __int8
#define __int8 char
#endif

// MSVC C-runtime names that glibc spells differently. Same semantics, different name.
#ifndef _stricmp
#define _stricmp   strcasecmp
#endif
#ifndef stricmp
#define stricmp    strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp  strncasecmp
#endif
#ifndef strnicmp
#define strnicmp   strncasecmp
#endif
#ifndef _wcsicmp
#define _wcsicmp   wcscasecmp
#endif
#ifndef _wcsnicmp
#define _wcsnicmp  wcsncasecmp
#endif
#ifndef _snprintf
#define _snprintf  snprintf
#endif
#ifndef _vsnprintf
#define _vsnprintf vsnprintf
#endif
#ifndef _strdup
#define _strdup    strdup
#endif
#ifndef _alloca
#define _alloca    alloca
#endif
#ifndef _finite
#define _finite(x) (std::isfinite(x) ? 1 : 0)
#endif
#ifndef _isnan
#define _isnan(x)  (std::isnan(x) ? 1 : 0)
#endif
#ifndef _atoi64
#define _atoi64(s) strtoll((s), 0, 10)
#endif

// _strlwr / _strupr have no glibc counterpart. The vendored GameSpy SDK carries them for every
// non-Windows platform (External/GameSpy/common/gsPlatformUtil.c, `#if !defined(_WIN32)`, linked
// into every image through bf2_gamespy_objects) and declares them with C linkage in gsPlatform.h;
// these declarations match that one exactly, so a TU that includes both sees a single function.
// The MSVC semantics (in place, returns its argument) are what that SDK implementation has.
extern "C" char* _strlwr(char* string);
extern "C" char* _strupr(char* string);
#ifndef strlwr
#define strlwr  _strlwr
#endif
#ifndef strupr
#define strupr  _strupr
#endif

// The GameSpy SDK's gsPlatformUtil.h defines function-like `min(a,b)` / `max(a,b)` macros unless a
// macro of that name already exists - the same pair <windows.h> defines on MSVC, where the MSVC STL
// is written to survive them. libstdc++ is not: every `std::min(` / `std::max(` inside its own
// headers is mangled by that macro. Defining each name to itself first makes the SDK skip its
// definitions (`#ifndef max`) while leaving every `min` / `max` token exactly as written.
#ifndef min
#define min min
#endif
#ifndef max
#define max max
#endif

// Map the diagnostic break to a process trap on the native port.
#ifndef __debugbreak
#define __debugbreak() __builtin_trap()
#endif

#endif // !defined(_MSC_VER)

#endif // CORE_PLATFORMCOMPAT_H
