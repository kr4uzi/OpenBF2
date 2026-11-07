// [CHOSEN, not recovered] MSVC 7.1 CRT functions that client code calls, rebuilt for the native
// POSIX ports with the MSVC observable semantics (return values, truncation, wide-format
// conversions). See docs/macos-port.md, "Win32 and CRT items (S2.5/S2.8)".
//
// Include it only where one of these names is used. On MSVC it is empty: the real CRT is used.
#ifndef CORE_MSVCCRTCOMPAT_H
#define CORE_MSVCCRTCOMPAT_H

#if !defined(_MSC_VER)

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <string>
#include <vector>

namespace bf2crt {

// Rewrites an MSVC wide-printf format for the C library. In MSVC's wide family `%s`/`%c` take
// wide arguments and `%S`/`%C` narrow ones (an `h` prefix forces narrow, `l`/`w` wide); C99 uses
// `%ls`/`%lc` for wide and `%s`/`%c` for narrow. `I64` becomes `ll`, `I32` is dropped and `I`
// becomes `z`. Everything else is copied unchanged.
inline std::wstring translateWideFormat(const wchar_t* fmt)
{
    std::wstring out;
    if (!fmt)
        return out;
    for (const wchar_t* p = fmt; *p; ) {
        if (*p != L'%') {
            out += *p++;
            continue;
        }
        out += *p++;                                   // '%'
        if (*p == L'%') {
            out += *p++;
            continue;
        }
        while (*p && std::wcschr(L"-+ #0", *p))        // flags
            out += *p++;
        while (*p == L'*' || (*p >= L'0' && *p <= L'9'))  // width
            out += *p++;
        if (*p == L'.') {                              // precision
            out += *p++;
            while (*p == L'*' || (*p >= L'0' && *p <= L'9'))
                out += *p++;
        }
        // Length prefix: 0 none, 'h' narrow, 'l' wide/long, 'w' wide.
        wchar_t size = 0;
        std::wstring lengthText;
        for (;;) {
            if (p[0] == L'I' && p[1] == L'6' && p[2] == L'4') { lengthText += L"ll"; p += 3; continue; }
            if (p[0] == L'I' && p[1] == L'3' && p[2] == L'2') { p += 3; continue; }
            if (p[0] == L'I') { lengthText += L"z"; ++p; continue; }
            if (*p == L'h') { size = L'h'; lengthText += *p++; continue; }
            if (*p == L'l') { if (size == 0) size = L'l'; lengthText += *p++; continue; }
            if (*p == L'w') { size = L'w'; ++p; continue; }
            if (*p == L'L' || *p == L'j' || *p == L'z' || *p == L't') { lengthText += *p++; continue; }
            break;
        }
        const wchar_t conv = *p;
        if (!conv)
            break;
        ++p;
        switch (conv) {
        case L's':
            out += (size == L'h') ? L"s" : L"ls";
            break;
        case L'S':
            out += (size == L'l' || size == L'w') ? L"ls" : L"s";
            break;
        case L'c':
            out += (size == L'h') ? L"c" : L"lc";
            break;
        case L'C':
            out += (size == L'l' || size == L'w') ? L"lc" : L"c";
            break;
        default:
            out += lengthText;
            out += conv;
            break;
        }
    }
    return out;
}

// Formats the whole output of an MSVC wide format. Returns false when the C library refuses
// (an encoding error or more than 16M characters).
inline bool formatWideFully(std::vector<wchar_t>& out, int& length, const wchar_t* fmt, va_list args)
{
    const std::wstring format = translateWideFormat(fmt);
    std::size_t capacity = 256;
    for (;;) {
        out.assign(capacity, L'\0');
        va_list copy;
        va_copy(copy, args);
        const int n = std::vswprintf(out.data(), capacity, format.c_str(), copy);
        va_end(copy);
        if (n >= 0) {
            length = n;
            return true;
        }
        if (capacity >= (std::size_t(1) << 24))
            return false;
        capacity *= 4;
    }
}

} // namespace bf2crt

// _vsnwprintf (MSVC 7.1): at most `count` characters are written. A shorter result is
// terminated and its length returned; a result of exactly `count` characters is written without
// a terminator and `count` returned; a longer one writes its first `count` characters, no
// terminator, and returns -1. A format the C library refuses returns -1 and writes nothing.
inline int _vsnwprintf(wchar_t* buffer, std::size_t count, const wchar_t* fmt, va_list args)
{
    std::vector<wchar_t> full;
    int length = 0;
    if (!bf2crt::formatWideFully(full, length, fmt, args))
        return -1;
    const std::size_t len = static_cast<std::size_t>(length);
    if (len < count) {
        std::wmemcpy(buffer, full.data(), len + 1);
        return length;
    }
    std::wmemcpy(buffer, full.data(), count);
    return len == count ? length : -1;
}

// _swprintf (MSVC 7.1 legacy, no size argument): the whole result and its terminator are written.
// Returns the length, or -1 for a format the C library refuses.
inline int _swprintf(wchar_t* buffer, const wchar_t* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::vector<wchar_t> full;
    int length = 0;
    const bool ok = bf2crt::formatWideFully(full, length, fmt, args);
    va_end(args);
    if (!ok)
        return -1;
    std::wmemcpy(buffer, full.data(), static_cast<std::size_t>(length) + 1);
    return length;
}

// _TRUNCATE and _snprintf_s (MSVC 8 secure CRT): with count == _TRUNCATE, as much as fits in
// sizeInBytes - 1 characters is written and terminated; the answer is the length, or -1 when the
// output was truncated. A finite count also limits the characters written, with the same -1
// answer when the limit cut the output.
#ifndef _TRUNCATE
#define _TRUNCATE (static_cast<std::size_t>(-1))
#endif
inline int _vsnprintf_s(char* buffer, std::size_t sizeInBytes, std::size_t count, const char* fmt, va_list args)
{
    if (!buffer || sizeInBytes == 0)
        return -1;
    std::size_t limit = sizeInBytes - 1;
    if (count != _TRUNCATE && count < limit)
        limit = count;
    va_list copy;
    va_copy(copy, args);
    const int n = std::vsnprintf(buffer, limit + 1, fmt, copy);
    va_end(copy);
    if (n < 0) {
        buffer[0] = '\0';
        return -1;
    }
    return static_cast<std::size_t>(n) > limit ? -1 : n;
}
inline int _snprintf_s(char* buffer, std::size_t sizeInBytes, std::size_t count, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int n = _vsnprintf_s(buffer, sizeInBytes, count, fmt, args);
    va_end(args);
    return n;
}

// sprintf_s, the array form (MSVC 8 secure CRT): the formatted text with its terminator, or, when
// it does not fit, the invalid-parameter handler, whose default ends the process. That stop is
// kept: the text is reported on stderr and the process aborts.
template <std::size_t N>
inline int sprintf_s(char (&buffer)[N], const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int n = std::vsnprintf(buffer, N, fmt, args);
    va_end(args);
    if (n < 0 || static_cast<std::size_t>(n) >= N) {
        buffer[0] = '\0';
        std::fprintf(stderr, "sprintf_s: buffer of %u bytes too small for \"%s\" (invalid parameter)\n",
                     static_cast<unsigned>(N), fmt);
        std::fflush(stderr);
        std::abort();
    }
    return n;
}

// __time32_t / _time32: seconds since the epoch as a 32-bit value; -1 once that no longer fits
// (from 2038), as the MSVC function answers.
typedef std::int32_t __time32_t;
inline __time32_t _time32(__time32_t* out)
{
    const std::time_t now = std::time(nullptr);
    const __time32_t value = (now < 0 || now > static_cast<std::time_t>(INT32_MAX))
                                 ? static_cast<__time32_t>(-1) : static_cast<__time32_t>(now);
    if (out)
        *out = value;
    return value;
}

// _time64: 64-bit seconds since the epoch.
inline long long _time64(long long* out)
{
    const long long value = static_cast<long long>(std::time(nullptr));
    if (out)
        *out = value;
    return value;
}

// _splitpath (MSVC 7.1 splitpath.c, single-byte): a drive is the first two characters when the
// second is ':'; the directory runs to the last '/' or '\\' inclusive; the extension starts at
// the last '.' after it. Each part is truncated to the MSVC limits (_MAX_DRIVE 3, _MAX_DIR 256,
// _MAX_FNAME 256, _MAX_EXT 256, terminator included) and every non-null output is terminated.
// As in MSVC, the caller's buffers must hold those limits.
inline void _splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
    const std::size_t maxDrive = 3, maxDir = 256, maxFname = 256, maxExt = 256;
    if (std::strlen(path) >= maxDrive - 2 && path[maxDrive - 2] == ':') {
        if (drive) {
            std::strncpy(drive, path, maxDrive - 1);
            drive[maxDrive - 1] = '\0';
        }
        path += maxDrive - 1;
    } else if (drive) {
        *drive = '\0';
    }
    const char* lastSlash = nullptr;
    const char* dot = nullptr;
    const char* p = path;
    for (; *p; ++p) {
        if (*p == '/' || *p == '\\')
            lastSlash = p + 1;
        else if (*p == '.')
            dot = p;
    }
    if (lastSlash) {
        if (dir) {
            std::size_t len = static_cast<std::size_t>(lastSlash - path);
            if (len > maxDir - 1) len = maxDir - 1;
            std::strncpy(dir, path, len);
            dir[len] = '\0';
        }
        path = lastSlash;
    } else if (dir) {
        *dir = '\0';
    }
    if (dot && dot >= path) {
        if (fname) {
            std::size_t len = static_cast<std::size_t>(dot - path);
            if (len > maxFname - 1) len = maxFname - 1;
            std::strncpy(fname, path, len);
            fname[len] = '\0';
        }
        if (ext) {
            std::size_t len = static_cast<std::size_t>(p - dot);
            if (len > maxExt - 1) len = maxExt - 1;
            std::strncpy(ext, dot, len);
            ext[len] = '\0';
        }
    } else {
        if (fname) {
            std::size_t len = static_cast<std::size_t>(p - path);
            if (len > maxFname - 1) len = maxFname - 1;
            std::strncpy(fname, path, len);
            fname[len] = '\0';
        }
        if (ext)
            *ext = '\0';
    }
}

#endif // !_MSC_VER
#endif // CORE_MSVCCRTCOMPAT_H
