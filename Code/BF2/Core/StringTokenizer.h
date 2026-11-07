// A character-set string tokenizer. BF2.exe 0x59E110 (destructor), 0x59E120 (assign), 0x59E1B0
// (constructor), 0x59E1F0 (nextToken). [INFERRED] class, member and header names and placement: no
// symbol or __FILE__ string names this class. SwiffHost::parseResolution (0x489210) is the menu caller.
//
// Retail layout (x86, 0x40 bytes): +0x00 the text (a VC7.1 std::string), +0x1C a std::bitset<256>
// of delimiter characters, +0x3C the scan position. The native object keeps the same members with
// the native STL; nothing outside the owner reads them.
#ifndef CORE_STRINGTOKENIZER_H
#define CORE_STRINGTOKENIZER_H

#include <bitset>
#include <string>

namespace dice {
namespace hfe {

class StringTokenizer
{
public:
    // 0x59E1B0: the bitset is zeroed (eight words), the position cleared, then assign().
    StringTokenizer(const std::string& text, const std::string& delimiters)
        : m_position(0)
    {
        assign(text, delimiters);
    }

    // 0x59E120: position 0, the text copied, then one bit set per delimiter byte. Bits already
    // set by an earlier assign() are kept (the shipped body does not clear the set).
    void assign(const std::string& text, const std::string& delimiters)
    {
        m_position = 0;
        m_text = text;
        for (std::string::size_type i = 0; i < delimiters.size(); ++i)
            m_delimiters.set(static_cast<unsigned char>(delimiters[i]));
    }

    // 0x59E1F0: `token` is cleared first (std::string::clear). Delimiters are skipped; at the end of the text the answer
    // is false and `token` stays empty. Otherwise the run up to the next delimiter (or the end) is
    // copied out with substr and the answer is true. The position is left on the delimiter that
    // ended the token, so the next call skips it.
    bool nextToken(std::string& token)
    {
        token.clear();
        while (m_position < m_text.size()
               && m_delimiters.test(static_cast<unsigned char>(m_text[m_position])))
            ++m_position;
        if (m_position >= m_text.size())
            return false;
        const std::string::size_type start = m_position;
        while (m_position < m_text.size()
               && !m_delimiters.test(static_cast<unsigned char>(m_text[m_position])))
            ++m_position;
        token = m_text.substr(start, m_position - start);
        return true;
    }

private:
    std::string             m_text;        // retail +0x00
    std::bitset<256>        m_delimiters;  // retail +0x1C
    std::string::size_type  m_position;    // retail +0x3C
};

} // namespace hfe
} // namespace dice

#endif // CORE_STRINGTOKENIZER_H
