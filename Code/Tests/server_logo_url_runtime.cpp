// [TEMP] [CHOSEN, not recovered] Retail fix "Server logo URLs cannot leave the logo cache"
// (docs/retail-fixes.md, B2BF #38). Links the actual ServerLogoManager translation unit; the
// HTTPRequest base and the file manager are stand-ins. parseUrl and getCachedPath must keep the
// retail split for plain URLs and refuse traversal components; download must refuse such a URL
// before it reaches the file manager (left null, so any use crashes the check) or the transfer.
#include "../BF2/Menu/ServerLogoManager.h"
#include "../BF2/IO/File/FileManager.h"

#include <cstdio>
#include <string>

namespace {
int failures = 0;
int transfers = 0;

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++failures;
    }
}
}

namespace dice { namespace hfe {
namespace io { FileManager* fileManager = nullptr; }

HTTPRequest::HTTPRequest()
    : m_getPending(false), m_postPending(false), m_ghttpHandleA(-1), m_postData(nullptr),
      m_ghttpHandleB(-1), m_revision(0) {}
HTTPRequest::~HTTPRequest() {}
void HTTPRequest::onGetComplete(const std::string&) {}
void HTTPRequest::onGetFailure(const std::string&) {}
void HTTPRequest::onGetFileComplete() {}
void HTTPRequest::onPostComplete(const std::string&) {}
void HTTPRequest::onPostFailure(const std::string&) {}
void HTTPRequest::onProgress(float) {}
bool HTTPRequest::isUpdating() { return false; }
bool HTTPRequest::getFile(const std::string&, const std::string&, bool) { ++transfers; return true; }
} }

using dice::hfe::ServerLogoManager;

static void expectSplit(ServerLogoManager& logos, const char* url, const char* directory, const char* file)
{
    std::string d("unchanged"), f("unchanged");
    const bool ok = logos.parseUrl(url, d, f);
    require(ok, url);
    require(ok && d == directory, url);
    require(ok && f == file, url);
}

static void expectRefused(ServerLogoManager& logos, const char* url)
{
    std::string d("unchanged"), f("unchanged");
    require(!logos.parseUrl(url, d, f), url);
    require(d == "unchanged" && f == "unchanged", url);
    std::string cached("unchanged");
    require(!logos.getCachedPath(url, cached) && cached == "unchanged", url);
    const int before = transfers;
    require(!logos.download(url), url);          // returns before the null file manager is used
    require(transfers == before, url);
}

int main()
{
    ServerLogoManager* logos = new ServerLogoManager;

    // The retail split: everything after the first "//", '/' tokens, directory tokens joined by '\'.
    expectSplit(*logos, "http://www.example.com/logo.png", "www.example.com\\", "logo.png");
    expectSplit(*logos, "http://host/a/b/sponsor.png", "host\\a\\b\\", "sponsor.png");
    expectSplit(*logos, "http://host:8080/img/logo.png", "host:8080\\img\\", "logo.png");
    expectSplit(*logos, "http://host//double//logo.png", "host\\double\\", "logo.png");
    expectSplit(*logos, "http://host/v1.2/..logo.png", "host\\v1.2\\", "..logo.png");
    expectSplit(*logos, "http://host", "", "host");

    // Components that leave the cache folder.
    expectRefused(*logos, "http://host/../../evil.png");
    expectRefused(*logos, "http://host/a/../evil.png");
    expectRefused(*logos, "http://host/..");
    expectRefused(*logos, "http://../evil.png");
    expectRefused(*logos, "http://host/.../evil.png");
    expectRefused(*logos, "http://host/.. /evil.png");
    expectRefused(*logos, "http://host/./evil.png");
    expectRefused(*logos, "http://host/..\\..\\evil.png");
    expectRefused(*logos, "http://host/a\\b.png");
    expectRefused(*logos, "http://host/a\tb.png");
    expectRefused(*logos, "http://");

    logos->release();
    if (failures) {
        std::fprintf(stderr, "server_logo_url_runtime: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("server_logo_url_runtime: ok\n");
    return 0;
}
