#ifndef BF2_RENDDX9_SHADERSOURCE_H
#define BF2_RENDDX9_SHADERSOURCE_H

#include <cstddef>
#include <cstring>

namespace dice { namespace hfe { namespace dx9rend {

namespace shaderSourceDetail {

inline bool identifier(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_';
}

inline bool terrainHiFile(const char* path)
{
    const char* base = path;
    for (const char* p = path; *p; ++p)
        if (*p == '/' || *p == '\\') base = p + 1;
    const char name[] = "terrainshader_hi.fx";
    std::size_t i = 0;
    for (; base[i] && i < sizeof(name) - 1; ++i)
    {
        const char c = base[i] >= 'A' && base[i] <= 'Z'
            ? static_cast<char>(base[i] + ('a' - 'A')) : base[i];
        if (c != name[i]) return false;
    }
    return i == sizeof(name) - 1 && base[i] == 0;
}

inline unsigned int heightAssignmentEnd(const char* source, unsigned int size, unsigned int at)
{
    const char* const tokens[] = {
        "wPos", ".", "yw", "=", "(", "indata", ".", "Pos1", ".", "xw", "*",
        "vScaleTransY", ".", "xy", ")", "+", "vScaleTransY", ".", "zw", ";"
    };
    for (const char* token : tokens)
    {
        while (at < size && (source[at] == ' ' || source[at] == '\t')) ++at;
        const unsigned int length = static_cast<unsigned int>(std::strlen(token));
        if (size - at < length || std::memcmp(source + at, token, length) != 0) return 0;
        at += length;
        if (identifier(token[length - 1]) && at < size && identifier(source[at])) return 0;
    }
    return at;
}

} // namespace shaderSourceDetail

// [CHOSEN, not recovered] Retail fix "Terrain colour passes no longer leave black patches in
// the middle distance" (docs/retail-fixes.md). Removes the zero height addend so colour and
// depth passes expose the same operations to multiply-add contraction on every client platform.
// Rewrites only TerrainShader_Hi.fx in place, updates size, and returns the replacement count.
// The buffer need not be terminated; bytes outside matching assignments are preserved.
inline unsigned int correctTerrainShaderHeight(const char* path, char* source, unsigned int& size)
{
    if (!shaderSourceDetail::terrainHiFile(path)) return 0;
    const char replacement[] = "wPos.yw = indata.Pos1.xw * vScaleTransY.xy;";
    const unsigned int length = sizeof(replacement) - 1;
    unsigned int count = 0;
    for (unsigned int at = 0; at < size; )
    {
        // Comments and quoted text are not shader tokens.
        if (at + 1 < size && source[at] == '/' && source[at + 1] == '/')
        {
            at += 2;
            while (at < size && source[at] != '\n' && source[at] != '\r') ++at;
        }
        else if (at + 1 < size && source[at] == '/' && source[at + 1] == '*')
        {
            at += 2;
            while (at + 1 < size && !(source[at] == '*' && source[at + 1] == '/')) ++at;
            at = at + 1 < size ? at + 2 : size;
        }
        else if (source[at] == '"' || source[at] == '\'')
        {
            const char quote = source[at++];
            while (at < size)
            {
                const char c = source[at++];
                if (c == '\\' && at < size) ++at;
                else if (c == quote) break;
            }
        }
        else if (shaderSourceDetail::identifier(source[at]))
        {
            const unsigned int end = source[at] == 'w'
                ? shaderSourceDetail::heightAssignmentEnd(source, size, at) : 0;
            if (end)
            {
                std::memcpy(source + at, replacement, length);
                std::memmove(source + at + length, source + end, size - end);
                size -= end - at - length;
                at += length;
                ++count;
            }
            else
                do { ++at; } while (at < size && shaderSourceDetail::identifier(source[at]));
        }
        else
            ++at;
    }
    return count;
}

void deobfuscateShader(unsigned char* source, unsigned int size);

// Prepares root and included shader bytes before either effect compiler or request capture sees
// them. Callers retain their decoding policy and ownership, and must use the returned byte size.
// [CHOSEN, not recovered] Shared source preparation for the terrain height retail fix above.
inline void prepareShaderSource(const char* path, unsigned char* source, unsigned int& size,
                                bool obfuscated)
{
    if (obfuscated) deobfuscateShader(source, size);
    correctTerrainShaderHeight(path, reinterpret_cast<char*>(source), size);
}

} } }
#endif
