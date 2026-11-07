// C-like scanner and token cursor. Line and column numbers are one-based.
// Token indices are unsigned; m_tokens[m_count] is the end token.
// The selected Windows build places bodies in RendDX9. [CHOSEN, not recovered] build placement
// and native STL containers keep these objects local to their image; PE and ELF sizes differ.
// Member names are [INFERRED] where the ELF carries no symbol.
#ifndef BF2_IO_TOKENIZER_H
#define BF2_IO_TOKENIZER_H

#include <string>
#include <vector>

namespace dice {
namespace hfe {

class Tokenizer;

class Token
{
public:
    // 0x8438A60 (out of line in the ELF; RendDX9 inlines it at clean() and at the scan):
    // owner 0, index -1, text empty, line and column 0xFFFF, type 0.
    Token();

    // Advances to the next token, stopping at the end token, and returns this cursor.
    Token& operator++();
    // Steps back one token, stopping at the first, and returns this cursor.
    Token& operator--();
    // Advances by count tokens, stopping at the end token, and returns this cursor.
    Token& operator+=(unsigned int count);
    // The token count positions ahead of this cursor, clamped to the end token.
    const Token& operator[](unsigned int count) const;

    Tokenizer*     owner;     // RendDX9 +0x00
    unsigned int   index;     // RendDX9 +0x04
    std::string    text;      // RendDX9 +0x08
    unsigned short line;      // RendDX9 +0x24
    unsigned short column;    // RendDX9 +0x26
    int            type;      // RendDX9 +0x28
};

// [INFERRED] Token equality compares line and column only.
inline bool operator==(const Token& a, const Token& b)
{
    return a.line == b.line && a.column == b.column;
}
inline bool operator!=(const Token& a, const Token& b)
{
    return !(a == b);
}

class Tokenizer
{
public:
    // Starts with empty storage. init() establishes the token count before cursor operations.
    Tokenizer();
    // 0x8438DA0 / sub_100768E0 - clean(), then the compiler's member teardown (the name, then the
    // vector).
    ~Tokenizer();

    // Reads and scans fileName. Returns false on a file or scan error; a scan error still records the filename.
    bool init(const char* fileName);
    // 0x8439080 / sub_10076900 - see Tokenizer.cpp for the scanner's rules and quirks.
    bool init(const char* data, unsigned int size);
    // 0x8439530 / sub_10076880 - the name to one NUL character, no tokens, count 0.
    void clean();

    // The integer starting at token, consuming its tokens: a "-" or "+" token is glued to the next
    // token, then the text is converted with _atoi64.
    static long long getInt64Number(Token& token);

    // [INFERRED] accessors: the shipped users inline these reads (the water loader reads
    // m_tokens[0] and m_tokens[m_count]); the ELF carries no out-of-line accessor.
    const Token& token(unsigned int i) const { return m_tokens[i]; }
    unsigned int count() const               { return m_count; }

private:
    friend class Token;   // operator++ reads the owner's tokens and count

    std::vector<Token> m_tokens;     // RendDX9 +0x00
    unsigned int       m_count;      // RendDX9 +0x10
    std::string        m_fileName;   // RendDX9 +0x14
};

} // namespace hfe
} // namespace dice

#endif // BF2_IO_TOKENIZER_H
