// [TEMP] [CHOSEN, not recovered] Link-time GameSpy request seam for the production
// HTTPRequest base. Only file-request refusal, offline failure and cancellation
// are modeled. No GameSpy objects, sockets, DNS or file writes are linked here.
// This prepares P4 validation; it does not exercise the still-frontiered DemoLibrary.
#include "../../BF2/Game/Persistence/HTTPRequest.h"
#include "../../BF2/External/GameSpy/ghttp/ghttp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
int checks = 0, failures = 0, startups = 0, saves = 0, thinks = 0, cancels = 0;
bool refuseStart = false;
ghttpCompletedCallback completion = nullptr;
void* callbackData = nullptr;
const int requestHandle = 37;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
[[noreturn]] void unexpected() { std::abort(); }
struct Request : dice::hfe::HTTPRequest {
    int failed = 0, completed = 0;
    std::string error;
    void onGetFailure(const std::string& message) override
    {
        ++failed;
        error = message;
        HTTPRequest::onGetFailure(message);
    }
    void onGetFileComplete() override { ++completed; HTTPRequest::onGetFileComplete(); }
};
}
extern "C" {
void ghttpStartup() { ++startups; }
void ghttpCleanup() { --startups; }
GHTTPRequest ghttpSaveExA(const char* url, const char* file, const char* headers,
    GHTTPPost post, GHTTPBool throttle, GHTTPBool blocking,
    ghttpProgressCallback, ghttpCompletedCallback callback, void* data)
{
    ++saves;
    check(std::strcmp(url, "http://offline.invalid/demo.bf2demo") == 0,
          "request URL crosses the seam unchanged");
    check(std::strcmp(file, "unused-demo.bf2demo") == 0, "target crosses the seam unchanged");
    check(!*headers && !post && !throttle && !blocking, "asynchronous file-request options");
    if (refuseStart) return -1;
    completion = callback;
    callbackData = data;
    return requestHandle;
}
GHTTPBool ghttpRequestThink(GHTTPRequest handle)
{
    ++thinks;
    check(handle == requestHandle, "update drives the accepted request");
    if (completion) {
        auto callback = completion;
        completion = nullptr;
        check(callback(handle, GHTTPHostLookupFailed, nullptr, 0, callbackData) == GHTTPTrue,
              "failure callback releases SDK response storage");
    }
    return GHTTPTrue;
}
void ghttpCancelRequest(GHTTPRequest handle)
{
    check(handle == requestHandle, "cancel receives the accepted handle");
    ++cancels;
    completion = nullptr;
}
GHTTPBool ghttpSetProxy(const char*) { unexpected(); }
GHTTPPost ghttpNewPost() { unexpected(); }
GHTTPBool ghttpPostAddStringA(GHTTPPost, const char*, const char*) { unexpected(); }
GHTTPRequest ghttpGetA(const char*, GHTTPBool, ghttpCompletedCallback, void*) { unexpected(); }
GHTTPRequest ghttpPostA(const char*, GHTTPPost, GHTTPBool, ghttpCompletedCallback, void*) { unexpected(); }
}
int main()
{
    {
        Request request;
        check(startups == 1 && !request.isUpdating(), "new request is idle");
        request.update();
        check(thinks == 0 && !request.cancel(), "idle update and cancel do no SDK work");
        refuseStart = true;
        check(!request.getFile("http://offline.invalid/demo.bf2demo", "unused-demo.bf2demo", true),
              "SDK refusal propagates false");
        check(!request.isUpdating() && request.getRevision() == 1, "refused start clears pending state");
        refuseStart = false;
        check(request.getFile("http://offline.invalid/demo.bf2demo", "unused-demo.bf2demo", true),
              "asynchronous request accepted by fixture");
        check(request.isUpdating(), "accepted request remains pending before update");
        const int previousSaves = saves;
        check(!request.getFile("ignored", "ignored", true) && saves == previousSaves,
              "pending request refuses a second transfer before SDK entry");
        request.update();
        check(request.failed == 1 && request.completed == 0 && !request.isUpdating(),
              "offline lookup failure takes failure callback and clears pending state");
        check(request.getRevision() == 2 && request.error == "<Failed looking up the hostname>",
              "failure publishes recovered revision and error text");
        check(request.cancel() && request.getRevision() == 1, "completed handle can be cancelled");
        check(!request.cancel(), "repeated cancel refuses");
        check(request.getFile("http://offline.invalid/demo.bf2demo", "unused-demo.bf2demo", true),
              "request can start again after cancellation");
        check(request.cancel() && !request.isUpdating() && !completion,
              "pending cancellation clears request and suppresses callback");
        request.update();
        check(thinks == 1 && cancels == 2, "cancelled request is not driven again");
    }
    check(startups == 0, "request destruction balances SDK startup");
    std::printf("HTTPRequest offline seam: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
