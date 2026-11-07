// Loading and lookup for localization lexicons.

#include "ExcelLexicon.h"
#include "../../Core/Ids.h"                // [INFERRED] header name; the constants are attested
#include "../../Core/StringUtil.h"         // getUpperCase, calcStringHashValueNoCase
#include "../../Core/Debug.h"
#include "../File/File.h"
#if !defined(_WIN32)
#include "../../Platform/Utf16.h"
#endif

namespace dice {
namespace hfe {

namespace io {

// constructor / destructor

// Initializes an empty lexicon with 4711 hash buckets and one owned reference.
ExcelLexicon::ExcelLexicon()
    : m_refCount(1)
    , m_default()               // the shared empty rep
    , m_strings(4711)           // 0x8464AA3 (m_maxSize) / 0x8464AB9 (operator new[](0x499C))
{
}

// Destroys the string table and default string through their member destructors.
ExcelLexicon::~ExcelLexicon()
{
}

// IBase quartet

// Core/IBase.h declares addRef() void.
void ExcelLexicon::addRef()
{
    ++m_refCount;
}


int ExcelLexicon::getRef()
{
    return m_refCount;
}

// Dispatches slot +0x1C - the deleting destructor - at 0x8465743, so
// this hierarchy really does have a virtual destructor, unlike the three Null* managers.
int ExcelLexicon::release()
{
    const int count = m_refCount - 1;
    const bool dead = (m_refCount == 1);
    m_refCount = count;

    if (dead)
        delete this;            // virtual, through slot +0x1C

    return count;
}

// The uniform two-id shape.
void* ExcelLexicon::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<ExcelLexicon*>(this);
    if (iid == IID_ILexicon)
        return const_cast<ExcelLexicon*>(this);
    return 0;
}

// Loads ESC-delimited localization pairs. Empty values are skipped and the first key insertion wins.
// Failures retain earlier entries. Text on disk is UTF-16LE, including the two-byte BOM.
// A final nonspace character at EOF is consumed by trailing-whitespace handling.
bool ExcelLexicon::load(const std::string& filename)
{
    File file;
    if (!file.open(filename, FILEMODE_READ, static_cast<FileAccess>(0), 0))
        return false;
    unsigned char fingerprint[2];
    if (!file.read(fingerprint, 2))
    {
        Debug(DebugTypeError, "IO/Localization/ExcelLexicon.cpp", 48, "")
            << "while reading lexicon file '" << filename << "': couldn't read fingerprint";
        return false;
    }
    if (fingerprint[0] != 0xff || fingerprint[1] != 0xfe)
    {
        Debug(DebugTypeError, "IO/Localization/ExcelLexicon.cpp", 53, "")
            << "while reading lexicon file '" << filename << "': bad unicode fingerprint";
        return false;
    }
    while (file.getAvailableBytes())
    {
        std::wstring wideKey, value;
#if defined(_WIN32)
        if (!file.read(wideKey, wchar_t(27)) || wideKey.empty()
            || !file.seek(static_cast<SeekOrigin>(1), sizeof(wchar_t))
            || !file.read(value, wchar_t(27))
            || !file.seek(static_cast<SeekOrigin>(1), sizeof(wchar_t)))
#else
        // [CHOSEN, not recovered] POSIX seeks count UTF-16 units, not native wide characters.
        if (!file.read(wideKey, wchar_t(27)) || wideKey.empty()
            || !file.seek(static_cast<SeekOrigin>(1), sizeof(char16_t))
            || !file.read(value, wchar_t(27))
            || !file.seek(static_cast<SeekOrigin>(1), sizeof(char16_t)))
#endif
            return false;
        std::string key = makeNarrow(wideKey);
        const std::string::size_type space = key.find(' ');
        if (space != std::string::npos)
            key.erase(space);
        if (key == "no_conversion_exists")
            return false;
        if (!value.empty())
        {
            const std::string upper = getUpperCase(key);
            const unsigned int bucket = m_strings.bucketOf(upper);
            if (!m_strings.exists(upper, bucket))
                m_strings.insert(upper, value, bucket);
        }
#if defined(_WIN32)
        wchar_t character;
        while (file.read(&character, sizeof(wchar_t))
               && (character == L'\n' || character == L'\r' || character == L' ' || character == L'\t'))
        {}
        if (file.getAvailableBytes())
            file.seek(static_cast<SeekOrigin>(1), -static_cast<int>(sizeof(wchar_t)));
#else
        char16_t character;
        while (platform::readUtf16LE(file, character)
               && (character == u'\n' || character == u'\r' || character == u' ' || character == u'\t'))
        {}
        if (file.getAvailableBytes())
            file.seek(static_cast<SeekOrigin>(1), -static_cast<int>(sizeof(char16_t)));
#endif
    }
    return true;
}

// Returns the localized value for key, ignoring case. A missing key returns the original key as wide text.
std::wstring ExcelLexicon::getString(const std::string& key) const
{
    const std::string upper = getUpperCase(key);

    std::wstring found;
    if (m_strings.get(upper, found))                        // the inlined bucket walk, 0x84655C0
        return found;

    return makeWide(key);                                   // the KEY, not empty
}

// Creates the requested lexicon interface with one reference. Aggregation is unsupported.
IBase* ExcelLexicon::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)
        return 0;

    ExcelLexicon* self = new ExcelLexicon();            // 0x850BD10, operator new(0x18u)
    void* itf = self->queryInterface(iid);              // virtual, slot +0x0C
    if (itf == 0)
        self->release();                                // virtual, slot +0x08
    return static_cast<IBase*>(itf);
}

} // namespace io
} // namespace hfe
} // namespace dice
