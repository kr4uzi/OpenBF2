// [CHOSEN, not recovered] Headless check of a wait for a native effect build with a caller
// service (Platform/Gfx/Effect/Native.h: Service, wait, compile). No game, window, GPU or effect
// cache: one small effect is compiled repeatedly. Target bf2_fx_wait_check
// (Platform/Gfx/Effect/CMakeLists.txt); exit status 0 when every check passes.
#include "Native.h"
#include "Wire.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#if defined(__APPLE__)
#include <pthread.h>
#include <pthread/qos.h>
#endif

using namespace bf2fx;

namespace {
using Clock = std::chrono::steady_clock;
const std::chrono::seconds kLimit(60); // No step waits longer; the watchdog ends a run that hangs.

unsigned checks = 0, failures = 0;
void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", what.c_str()); }
}

struct Gate {
    std::mutex mutex;
    std::condition_variable changed;
    bool open = false;
    void release() { { std::lock_guard<std::mutex> lock(mutex); open = true; } changed.notify_all(); }
    bool wait() { std::unique_lock<std::mutex> lock(mutex); return changed.wait_for(lock, kLimit, [this] { return open; }); }
};
// What a thread that runs a build had.
struct Ran {
    std::thread::id thread;
    unsigned long qos = 0;
    size_t stack = 0;
    void record() {
        thread = std::this_thread::get_id();
#if defined(__APPLE__)
        qos = qos_class_self();
        stack = pthread_get_stacksize_np(pthread_self());
#endif
    }
};
// A caller's service: counts its runs, notes the thread and the time of each.
struct Counter {
    std::mutex mutex;
    std::condition_variable changed;
    unsigned runs = 0, offCaller = 0;
    std::vector<Clock::time_point> at;
    std::thread::id caller = std::this_thread::get_id();
    void run() {
        { std::lock_guard<std::mutex> lock(mutex);
          ++runs; at.push_back(Clock::now()); if (std::this_thread::get_id() != caller) ++offCaller; }
        changed.notify_all();
    }
    bool reached(unsigned count) { std::unique_lock<std::mutex> lock(mutex); return changed.wait_for(lock, kLimit, [&] { return runs >= count; }); }
    unsigned count() { std::lock_guard<std::mutex> lock(mutex); return runs; }
};
const unsigned kCompeting = 24;
// Shared with the threads that run the builds, and never destroyed: a callback may outlive main.
struct State {
    Gate workerBusy, workerHeld, abandoned;
    Ran worker, plain, serviced;
    Counter building, held, finished, throwing, compiling, competing, interrupted;
    std::atomic<unsigned> servicedDone{0}, duplicateDone{0}, interruptedDone{0};
    std::mutex competingMutex;
    unsigned built[kCompeting] = {}, onWorker = 0;
};

const char source[] = R"fx(
float4x4 Transform;
float4 Tint = float4(1, 1, 1, 1);
struct Vertex { float4 position : POSITION0; float2 uv : TEXCOORD0; };
Vertex vertexMain(Vertex input) {
    Vertex result;
    result.position = mul(input.position, Transform);
    result.uv = input.uv;
    return result;
}
float4 pixelMain(float2 uv : TEXCOORD0) : COLOR0 { return float4(uv, 0, 1) * Tint; }
technique Fixture {
    pass Draw {
        VertexShader = compile vs_2_0 vertexMain();
        PixelShader = compile ps_2_0 pixelMain();
    }
}
)fx";

bool throwsDoneFailed(const std::function<void()>& call) {
    try { call(); } catch (const std::runtime_error& e) { return std::strcmp(e.what(), "done failed") == 0; }
    return false;
}

void run() {
    State& s = *new State;
    const auto self = std::this_thread::get_id();
    Request request;
    const auto captured = capture(source, std::strlen(source), {}, 0, native::identityHash(), native::identity(), nullptr, request);
    check(bool(captured), "the fixture request is captured: " + captured.detail);
    const Entry direct = native::compile(request);
    check(direct.kind == EffectEntry && direct.description.shaders.size() == 2, "the fixture compiles to two shaders: " + direct.log);
    const Bytes expected = encode(direct);

    // 1. The queue's only worker starts a job and is held in its done callback: that build is
    //    running on a worker, and nothing that is queued after it can start on the queue.
    const auto first = native::submit("held", request, std::string(), [&s](const Entry&) {
        s.worker.record();
        s.workerBusy.release();
        s.workerHeld.wait();
    });
    check(s.workerBusy.wait(), "the queue's worker starts the first job");
    check(s.worker.thread != self, "a queued job that nobody waits for runs on the worker");

    // 2. Without a service nothing changes: the calling thread compiles a job that no worker has started.
    const auto second = native::submit("plain", request, std::string(), [&s](const Entry&) { s.plain.record(); });
    const Entry plain = native::wait("plain", second);
    check(s.plain.thread == self, "without a service the waiting thread compiles the job itself");
    check(encode(plain) == expected, "without a service the wait returns the compiled entry");

    // 3. With a service the same job runs at once on a thread of its own (the worker is still
    //    held), the service runs on the calling thread while the job lasts (its completion callback
    //    holds it until the fifth service), and the entry is the same. A duplicate submission and
    //    another waiter must share that job, without running either callback twice.
    const auto third = native::submit("serviced", request, std::string(), [&s](const Entry&) {
        s.serviced.record();
        ++s.servicedDone;
        s.building.reached(5);
    });
    const auto duplicate = native::submit("serviced", request, std::string(), [&s](const Entry&) { ++s.duplicateDone; });
    std::future<Entry> otherWaiter;
    const auto began = Clock::now();
    const Entry serviced = native::wait("serviced", third, [&] {
        s.building.run();
        if (!otherWaiter.valid()) otherWaiter = std::async(std::launch::async, [duplicate] {
            return native::wait("serviced", duplicate);
        });
    });
    check(s.serviced.thread != std::thread::id() && s.serviced.thread != self, "with a service the build runs off the waiting thread");
    check(s.serviced.thread != s.worker.thread, "with a service the build does not wait for the queue's busy worker");
    check(s.building.count() >= 5, "the service runs repeatedly while the build runs");
    check(s.building.offCaller == 0, "the service runs on the waiting thread");
    check(encode(serviced) == expected, "with a service the wait returns the same entry as without one");
    check(otherWaiter.valid() && encode(otherWaiter.get()) == expected, "a concurrent plain waiter receives the same entry");
    check(s.servicedDone == 1 && s.duplicateDone == 0, "duplicate submission and concurrent waits complete the job once");
    {
        std::vector<double> gaps;
        auto last = began;
        for (const auto& t : s.building.at) { gaps.push_back(std::chrono::duration<double, std::milli>(t - last).count()); last = t; }
        std::sort(gaps.begin(), gaps.end());
        const double shortest = gaps.empty() ? 0.0 : gaps.front(), median = gaps.empty() ? 0.0 : gaps[gaps.size() / 2];
        std::printf("service runs: %zu, gaps %.0f ms shortest, %.0f ms median, %.0f ms longest\n",
                    gaps.size(), shortest, median, gaps.empty() ? 0.0 : gaps.back());
        check(shortest >= 40.0, "the waiting thread sleeps between two runs of the service (about 50 ms)");
        check(median <= 250.0, "the service runs about every 50 ms");
    }
#if defined(__APPLE__)
    std::printf("build threads: worker qos 0x%lx stack %zu KiB, with a service qos 0x%lx stack %zu KiB\n",
                s.worker.qos, s.worker.stack / 1024, s.serviced.qos, s.serviced.stack / 1024);
    check(s.worker.qos == QOS_CLASS_USER_INITIATED, "the queue worker uses User Initiated QoS despite a Utility caller");
    check(s.serviced.qos == QOS_CLASS_USER_INITIATED, "the serviced build uses User Initiated QoS despite a Utility caller");
    check(s.serviced.stack >= (size_t(8) << 20), "the build's thread has the main thread's stack size");
#endif

    // 4. What the build throws reaches the waiting thread, with a service as without one.
    const auto fourth = native::submit("throws", request, std::string(), [](const Entry&) { throw std::runtime_error("done failed"); });
    check(throwsDoneFailed([&] { native::wait("throws", fourth, [&s] { s.throwing.run(); }); }),
          "with a service the wait throws what the build threw");
    check(fourth.wait_for(std::chrono::seconds(0)) == std::future_status::ready,
          "a completion exception settles the shared job future");
    check(throwsDoneFailed([&] { native::wait("throws", fourth); }), "another waiter receives the same completion exception");
    std::promise<Entry> failed;
    failed.set_exception(std::make_exception_ptr(std::runtime_error("done failed")));
    check(throwsDoneFailed([&] { native::wait("already-failed", failed.get_future().share()); }),
          "a plain wait preserves a future's exception");

    // 5. A build that a worker is running (the first job): the waiting thread runs the service
    //    until the worker finishes; here the third run lets the worker go on.
    const Entry held = native::wait("held", first, [&s] { s.held.run(); if (s.held.count() == 3) s.workerHeld.release(); });
    check(s.held.count() >= 3, "the service runs repeatedly while a worker runs the build");
    check(s.held.offCaller == 0, "the service runs on the waiting thread while a worker runs the build");
    check(encode(held) == expected, "the wait for a worker's build returns the same entry");
    s.workerHeld.release();

    // 6. A finished build: no wait, the service does not run.
    const Entry again = native::wait("serviced", third, [&s] { s.finished.run(); });
    check(s.finished.count() == 0, "the service does not run for a finished build");
    check(encode(again) == expected, "a finished build's entry is returned again");

    // 7. compile() with a service: the same entry, from the request and from preprocessed text.
    check(encode(native::compile(request, nullptr, [&s] { s.compiling.run(); })) == expected,
          "compile with a service returns the same entry");
    check(s.compiling.offCaller == 0, "compile's service runs on the calling thread");
    check(encode(native::compile(request, nullptr, native::Service())) == expected, "compile with an empty service returns the same entry");
    std::string text, log;
    check(native::preprocess(request, text, log), "the fixture is preprocessed: " + log);
    check(encode(native::compile(request, &text, [&s] { s.compiling.run(); })) == encode(native::compile(request, &text)),
          "compile of preprocessed text with a service returns the same entry");

    // A service exception must not destroy/cancel a job that has already started.
    const auto interrupted = native::submit("interrupted", request, std::string(), [&s](const Entry&) {
        s.abandoned.wait();
        ++s.interruptedDone;
    });
    bool serviceThrew = false;
    try {
        native::wait("interrupted", interrupted, [&s] {
            s.interrupted.run();
            if (s.interrupted.count() == 3) throw std::runtime_error("service failed");
        });
    } catch (const std::runtime_error& e) { serviceThrew = std::strcmp(e.what(), "service failed") == 0; }
    check(serviceThrew, "the caller receives a service exception");
    s.abandoned.release();
    check(encode(native::wait("interrupted", interrupted)) == expected && s.interruptedDone == 1,
          "a job survives a throwing service and another waiter receives its result");

    // Invalid input still returns the same diagnostic/refusal rather than a success or throw.
    Request invalid = request;
    const char broken[] = "float4 invalid = @;";
    invalid.source.assign(broken, broken + std::strlen(broken));
    const Entry refused = native::compile(invalid);
    check(refused.kind == RefusedEntry && !refused.log.empty(), "invalid source is refused with a diagnostic");
    check(encode(native::compile(invalid, nullptr, [&s] { s.compiling.run(); })) == encode(refused),
          "compile with a service preserves refusal fields and diagnostics");
    const auto bad = native::submit("invalid", invalid, std::string(), {});
    check(encode(native::wait("invalid", bad, [&s] { s.compiling.run(); })) == encode(refused),
          "a serviced queue job preserves refusal fields and diagnostics");

    // 8. Jobs that the queue's worker and a waiting thread with a service compete for (the worker
    //    takes them from the front, the waits from the back): each is built once, each wait
    //    returns the entry.
    std::vector<std::shared_future<Entry>> futures;
    for (unsigned i = 0; i < kCompeting; ++i)
        futures.push_back(native::submit("competing-" + std::to_string(i), request, std::string(), [&s, i](const Entry&) {
            std::lock_guard<std::mutex> lock(s.competingMutex);
            ++s.built[i];
            if (std::this_thread::get_id() == s.worker.thread) ++s.onWorker;
        }));
    unsigned same = 0;
    for (unsigned i = kCompeting; i-- > 0;)
        if (encode(native::wait("competing-" + std::to_string(i), futures[i], [&s] { s.competing.run(); })) == expected) ++same;
    unsigned once = 0, onWorker = 0;
    {
        std::lock_guard<std::mutex> lock(s.competingMutex);
        for (unsigned count : s.built) if (count == 1) ++once;
        onWorker = s.onWorker;
    }
    std::printf("competing jobs: %u on the queue's worker, %u on threads of their own\n", onWorker, kCompeting - onWorker);
    check(once == kCompeting, "every job the worker and the waiting thread compete for is built exactly once");
    check(same == kCompeting, "every wait for a competing job returns the same entry");
    check(s.competing.offCaller == 0, "the service of those waits runs on the waiting thread");
    // Retirement follows future completion, so allow the final job time to leave the map.
    size_t left = native::pending();
    for (const auto until = Clock::now() + std::chrono::seconds(5); left && Clock::now() < until; left = native::pending())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(left == 0, "every serviced, plain, refused and throwing job leaves the queue");
}
} // namespace

int main() {
    // Ends a run in which a wait never returns.
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(240));
        std::fprintf(stderr, "FAIL: no result within 240 s\n");
        std::_Exit(2);
    }).detach();
    setenv("BF2_FX_NATIVE_THREADS", "1", 1); // One queue worker, which step 1 occupies.
#if defined(__APPLE__)
    // [TEMP] A background caller must not consign required effect builds to Utility QoS.
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif
    try {
        run();
    } catch (const std::exception& e) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", e.what());
    }
    if (failures) { std::fprintf(stderr, "%u of %u checks failed\n", failures, checks); return 1; }
    std::printf("%u checks passed\n", checks);
    return 0;
}
