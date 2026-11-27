#!/usr/bin/env python3
"""[TEMP] Compile the production load callback against a heartbeat observer.

Usage: python3 Code/Tests/load_callback_watchdog.py out/build/<dir>/watchdog
Requires a GCC/Clang-style C++ compiler. No engine, threads, sockets or window run.
"""

import argparse
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Code/BF2/Game/Main/BF2Engine.cpp"

FIXTURE = r"""
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

// [TEMP] Observe only the callback's heartbeat dispatch and mode queries.
struct HeartbeatObserver {
    unsigned count = 0;
    void markAsStillRunning() { ++count; }
};
HeartbeatObserver* bf2Engine = nullptr;
bool dedicatedMode = false;
unsigned modeQueries = 0;
bool isDedicated() { ++modeQueries; return dedicatedMode; }

@CALLBACK@

void check(bool passed, const char* message) {
    if (!passed) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main() {
    for (bool mode : {false, true}) {
        dedicatedMode = mode;
        modeQueries = 0;
        bf2Engine = nullptr;
        atIncLoadCallback();
        check(modeQueries == 0, "absent engine skips mode query");

        HeartbeatObserver observer;
        bf2Engine = &observer;
        for (unsigned i = 0; i != 3; ++i)
            atIncLoadCallback();
        check(observer.count == (DEDICATED || mode ? 3u : 0u),
              "only dedicated loading emits every heartbeat");
        check(modeQueries == (DEDICATED ? 0u : 3u),
              "only the client image queries runtime dedicated mode");
    }
    std::printf("load callback watchdog: DEDICATED=%d passed\n", DEDICATED);
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--source", type=Path, default=SOURCE,
                        help="alternate BF2Engine.cpp for regression verification")
    args = parser.parse_args()
    source = args.source.read_text(encoding="utf-8-sig")
    start = source.index("void atIncLoadCallback()\n{")
    end = source.index("\n}", start) + 2
    callback = source[start:end]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    fixture = output / "load_callback_watchdog.cpp"
    fixture.write_text(FIXTURE.replace("@CALLBACK@", callback))
    for image in (0, 1):
        executable = output / ("load_callback_watchdog_" + str(image))
        subprocess.run([
            args.compiler, "-std=c++14",
            "-DDEDICATED=" + str(image), "-include",
            str(ROOT / "Code/BF2/Core/BuildConfig.h"),
            str(fixture), "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
