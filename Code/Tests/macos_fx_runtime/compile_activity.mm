// [TEMP] [CHOSEN, not recovered] Headless scheduling checks; no game, audio, window or GPU.
// Observe real Foundation begin/end calls while running the production scope guard.
#include "../../BF2/Platform/macOS/ShaderCompileActivity.h"
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

namespace {
using Begin = id (*)(id, SEL, NSActivityOptions, NSString*);
using End = void (*)(id, SEL, id);
Begin originalBegin;
End originalEnd;
std::mutex mutex;
std::set<void*> live;
unsigned begins = 0, ends = 0, failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", what); }
}
id begin(id self, SEL selector, NSActivityOptions options, NSString* reason)
{
    id token = originalBegin(self, selector, options, reason);
    std::lock_guard<std::mutex> lock(mutex);
    check(options == NSActivityUserInitiatedAllowingIdleSystemSleep, "finite user work allows system/display sleep");
    check([reason isEqualToString:@"OpenBF2 shader compilation"], "activity names shader compilation");
    check(token && live.insert((__bridge void*)token).second, "each begin owns a distinct live token");
    ++begins;
    return token;
}
void end(id self, SEL selector, id token)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        check(live.erase((__bridge void*)token) == 1, "end matches one live begin");
        ++ends;
    }
    originalEnd(self, selector, token);
}
std::size_t active()
{
    std::lock_guard<std::mutex> lock(mutex);
    return live.size();
}
}

int main()
{
    @autoreleasepool {
        const Class cls = [NSProcessInfo.processInfo class];
        const Method beginMethod = class_getInstanceMethod(cls, @selector(beginActivityWithOptions:reason:));
        const Method endMethod = class_getInstanceMethod(cls, @selector(endActivity:));
        originalBegin = reinterpret_cast<Begin>(method_setImplementation(beginMethod, reinterpret_cast<IMP>(begin)));
        originalEnd = reinterpret_cast<End>(method_setImplementation(endMethod, reinterpret_cast<IMP>(end)));
        check(active() == 0, "idle process holds no compile activity");
        {
            bf2macos::ShaderCompileActivity outer;
            check(active() == 1, "one build owns one activity");
            try {
                bf2macos::ShaderCompileActivity inner;
                check(active() == 2, "nested builds own independent activities");
                throw std::runtime_error("compiler refusal");
            } catch (const std::runtime_error&) {}
            check(active() == 1, "exception ends only its own activity");
            std::atomic<bool> started{false}, release{false};
            std::thread worker([&] {
                bf2macos::ShaderCompileActivity work;
                started = true;
                while (!release) std::this_thread::yield();
            });
            while (!started) std::this_thread::yield();
            check(active() == 2, "overlapping threads retain independent activities");
            release = true;
            worker.join();
            check(active() == 1, "worker completion retains the other build's activity");
        }
        check(active() == 0 && begins == 3 && ends == begins, "all activities end before idle, exactly once");
        method_setImplementation(beginMethod, reinterpret_cast<IMP>(originalBegin));
        method_setImplementation(endMethod, reinterpret_cast<IMP>(originalEnd));

        for (qos_class_t qos : {QOS_CLASS_UNSPECIFIED, QOS_CLASS_BACKGROUND, QOS_CLASS_UTILITY,
                                QOS_CLASS_DEFAULT, QOS_CLASS_USER_INITIATED})
            check(bf2macos::shaderCompileQos(qos) == QOS_CLASS_USER_INITIATED, "compile QoS has a User Initiated floor");
        check(bf2macos::shaderCompileQos(QOS_CLASS_USER_INTERACTIVE) == QOS_CLASS_USER_INTERACTIVE,
              "compile QoS preserves User Interactive callers");
    }
    std::printf("shader compilation scheduling: %u failures\n", failures);
    return failures ? 1 : 0;
}
