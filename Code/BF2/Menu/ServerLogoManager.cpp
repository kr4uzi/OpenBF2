// Client server-logo management. [INFERRED] Source placement.
#include "ServerLogoManager.h"
#if !DEDICATED
#include "../Core/Ids.h"
#include "../Game/Persistence/SimpleTokenizer.h"
#include "../IO/File/FileManager.h"
#include <new>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace dice { namespace hfe {

//4C34A0 (48 B): the HTTPRequest base first (59C390), the two tables, then a refcount of 1.
ServerLogoManager::ServerLogoManager() : m_refCount(1) {}

//4C34D0 (32 B): the tables reset, the base's 59C3C0, and the deleting flag frees the 32 bytes.
ServerLogoManager::~ServerLogoManager() {}

//410784 (54 B): the settings-factory shape - aggregation refused, 32 retail bytes, construction
// only on a non-null allocation, unconditional queryInterface (the shipped null defect is kept,
// exactly as DemoLibrary::create keeps it).
IBase* ServerLogoManager::create(unsigned int iid, IBase* outer)
{
    if (outer) return nullptr;
    void* allocation = ::operator new(sizeof(ServerLogoManager));
    ServerLogoManager* owner = allocation ? new (allocation) ServerLogoManager : nullptr;
    IBase* result = static_cast<IBase*>(owner->queryInterface(iid));
    if (!result) owner->release();
    return result;
}
void ServerLogoManager::addRef() { ++m_refCount; }
int  ServerLogoManager::getRef() { return m_refCount; }
//479830 (26 B): decrement, destroy at zero.
int  ServerLogoManager::release()
{
    const int references = --m_refCount;
    if (!references) delete this;
    return references;
}
//4C30E0 (25 B): IBase (1), then IServerLogoManager (121067); anything else null.
void* ServerLogoManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IServerLogoManager)
        return const_cast<ServerLogoManager*>(this);
    return nullptr;
}
bool ServerLogoManager::init() { return true; }                    //the constant-true stub several client singletons share
void ServerLogoManager::shutdown() {}                              //nullsub

//4C3040 (73 B): the text after the first slash plus two. It is find_first_of("//"), a character
// set, so it finds a single '/' and skips the two of "http://host/x" to answer "host/x"; a url
// with no slash is answered whole. A url whose only slash is its last character assigns from one
// byte past its terminator, as shipped.
static std::string afterScheme(const std::string& url)
{
    std::string out;
    const std::string::size_type at = url.find_first_of("//", 0, 2);
    if (at == std::string::npos)
        out = url;
    else
        out = url.c_str() + at + 2;
    return out;
}

// [CHOSEN, not recovered] Retail fix "Server logo URLs cannot leave the logo cache"
// (docs/retail-fixes.md). A path component that the file system could read as anything but a
// plain name inside the cache: empty, only dots and spaces (".." and the forms Windows trims to
// it), or containing a backslash (Windows' separator, which the '/' split leaves in place; the
// POSIX file systems map it to '/') or a control character. A colon stays allowed: it is not a
// drive inside the cache path, and a "host:port" component works on the POSIX ports.
static bool isUnsafeCacheComponent(const std::string& component)
{
    if (component.empty())
        return true;
    if (component.find_first_not_of(". ") == std::string::npos)
        return true;
    for (std::string::size_type i = 0; i < component.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(component[i]);
        if (c == '\\' || c < 0x20 || c == 0x7F)
            return true;
    }
    return false;
}

//4C3550 (293 B): the url after its scheme, split at '/' by the persistence tokenizer (59E190 /
// SimpleTokenizer); nothing to split is a refusal. The last token is the file;
// the ones before it, each followed by a backslash, are the directory. Both outputs are assigned
// only on success, directory first.
// Retail takes every token as it comes (the loop at 0x4C35C4 only appends), so a server's sponsor
// or community logo URL with ".." or backslash components names a file outside "<home>\LogoCache\"
// that download() then writes (Luigi Auriemma, "bf2urlz", 2010; B2BF #38).
// [CHOSEN, not recovered] Retail fix "Server logo URLs cannot leave the logo cache"
// (docs/retail-fixes.md): such a URL is refused like one with nothing to split.
bool ServerLogoManager::parseUrl(const std::string& url, std::string& directory, std::string& file)
{
    const std::string stripped = afterScheme(url);
    SimpleTokenizer tokenizer(stripped, std::string("/"));
    std::vector<std::string> tokens;
    std::string token;
    if (!tokenizer.next(token))
        return false;
    do
        tokens.push_back(token);
    while (tokenizer.next(token));
    if (tokens.empty())                     // unreachable after the loop above; the shipped test
        return false;
    for (std::size_t i = 0; i < tokens.size(); ++i)
        if (isUnsafeCacheComponent(tokens[i]))
            return false;                   // [CHOSEN, not recovered] see isUnsafeCacheComponent
    const std::string last(tokens.back());
    tokens.pop_back();
    std::string joined;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        joined += tokens[i];
        joined += "\\";                     // word_893B38
    }
    directory = joined;
    file = last;
    return true;
}

//4C33D0 (205 B): the home directory's "\LogoCache\" plus the parsed directory and file - the
// on-disk path of a logo. A url that does not parse is a refusal with `out` untouched.
bool ServerLogoManager::getCachedPath(const std::string& url, std::string& out)
{
    std::string directory;
    std::string file;
    if (!parseUrl(url, directory, file))
        return false;
    std::string cache(io::fileManager->getHomeSubstitution());     // +0x48
    cache += "\\LogoCache\\";
    out = cache;
    out += directory;
    out += file;
    return true;
}

//4C3120 (680 B): one transfer at a time. The url is parsed (its result unread), the cache
// directory "<home>\LogoCache\<directory>" and the logical "@HOME@\LogoCache\<directory><file>"
// are built, and a cached copy younger than 24 hours - whole hours of its last-write filetime
// against the clock now - is kept as it is. Otherwise the cached file is deleted, the directory
// chain is created by writing and deleting an empty "/dummy.txt" beside it, and the ghttp transfer
// runs blocking into the cache path; only a refused transfer is a failure.
// [CHOSEN, not recovered] Retail fix "Server logo URLs cannot leave the logo cache"
// (docs/retail-fixes.md): retail ignores parseUrl's answer; a refused URL is now a failed download,
// so nothing is deleted, created or written. SwiffRender's logo arm then has no texture (-1).
bool ServerLogoManager::download(const std::string& url)
{
    if (isUpdating())                                              // a get or a post pending
        return false;
    std::string directory;
    std::string file;
    if (!parseUrl(url, directory, file))
        return false;
    std::string cacheDirectory(io::fileManager->getHomeSubstitution());
    cacheDirectory += "\\LogoCache\\";
    cacheDirectory += directory;
    std::string logical("@HOME@\\LogoCache\\");
    logical += directory;
    logical += file;
    std::string cached(cacheDirectory);
    cached += file;
    io::FileInfo info;
    if (io::fileManager->getFileInfo(logical, info))               // +0x38
    {
#if defined(_WIN32)
        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        const unsigned long long ticksPerHour = 36000000000ULL;   // 0x861C46800 in 100 ns units
        const unsigned long long nowHours =
            ((static_cast<unsigned long long>(now.dwHighDateTime) << 32) | now.dwLowDateTime) / ticksPerHour;
        const unsigned long long writtenHours =
            ((static_cast<unsigned long long>(info.lastWriteTimeHigh) << 32) | info.lastWriteTimeLow) / ticksPerHour;
        if (nowHours - writtenHours < 24)
            return true;
#endif
    }
    io::fileManager->deleteFile(logical);                          // +0x20
    std::string dummy(cacheDirectory);
    dummy += "/dummy.txt";
    io::fileManager->writeFile(dummy, 0, 0, true);                 // +0x30: the directory chain
    io::fileManager->deleteFile(dummy);
    if (!getFile(url, cached, false))                              // 59C5B0, asynchronous false
        return false;
    return true;
}

} }
#endif // !dedicated
