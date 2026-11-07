// [CHOSEN, not recovered] POSIX UTF-16LE file boundaries (macOSplan.md, S2.10).
#ifndef PLATFORM_UTF16_H
#define PLATFORM_UTF16_H

#include <cstddef>
#include <string>

namespace dice { namespace hfe { namespace platform {

// Reads one little-endian UTF-16 code unit, independent of host width and alignment.
// A short read consumes the available bytes and leaves unit unchanged.
template<class Stream> bool readUtf16LE(Stream& stream, char16_t& unit)
{
    unsigned char bytes[2];
    if (stream.read(bytes, 2) != 2)
        return false;
    unit = static_cast<char16_t>(bytes[0] | (static_cast<unsigned int>(bytes[1]) << 8));
    return true;
}

// Converts a NUL-terminated UTF-16 buffer to native wide text. Valid surrogate pairs become
// one scalar on 32-bit wchar_t hosts; unpaired surrogates retain their Windows code-unit value.
inline std::wstring utf16ToWide(const char16_t* units)
{
    std::wstring text;
    for (std::size_t i = 0; units[i]; ++i)
    {
        unsigned int value = units[i];
        if (sizeof(wchar_t) > 2 && value >= 0xd800 && value <= 0xdbff
            && units[i + 1] >= 0xdc00 && units[i + 1] <= 0xdfff)
        {
            value = 0x10000 + ((value - 0xd800) << 10) + (units[++i] - 0xdc00);
        }
        text.push_back(static_cast<wchar_t>(value));
    }
    return text;
}

} } }
#endif
