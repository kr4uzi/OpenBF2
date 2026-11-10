#!/usr/bin/env python3
"""[TEMP] [CHOSEN, not recovered] Run the browser fixture with macOS SDK objects.

Usage: python3 Code/Tests/server_info_bots.py out/build/srv-botinfo
Build bf2 and bf2_client first. Default checks open no sockets or game content.
Inside the loopback client sandbox, add --query --expect 50 16 50 to check a
running co-op server at 127.0.0.1:29911. This does not verify the movie display.
"""
import argparse
import json
import math
from pathlib import Path
import shlex
import socket
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("--query", action="store_true", help="query only loopback UDP 29911")
parser.add_argument("--expect", nargs=3, metavar=("RATIO", "COUNT", "DIFFICULTY"), type=float)
options = parser.parse_args()
if options.query != (options.expect is not None):
    parser.error("--query and --expect RATIO COUNT DIFFICULTY must be used together")
if options.expect is not None and not options.expect[1].is_integer():
    parser.error("expected bot count must be an integer")
if options.expect is not None and not all(math.isfinite(value) for value in options.expect):
    parser.error("expected bot values must be finite")

root = Path(__file__).resolve().parents[2]
build = options.build.resolve()
commands = json.loads((build / "compile_commands.json").read_text())
for target in ("bf2_engine_objects", "bf2_client_objects"):
    cache = next(c for c in commands if c["file"].endswith("/ServerQueryCache.cpp")
                 and (target + ".dir/") in c.get("output", ""))
    cache_args = shlex.split(cache["command"])
    for flag in ("-o", "-c"):
        index = cache_args.index(flag)
        del cache_args[index:index + 2]
    subprocess.run(cache_args + ["-fsyntax-only", str(root / "Code/Tests/server_info_bots.cpp")],
                   cwd=build, check=True)
print("server_info_bots: cache protocol indices pass for dedicated and client images", flush=True)

entry = next(c for c in commands if c["file"].endswith("/serverbrowsing/sb_server.c"))
args = shlex.split(entry["command"])
for flag in ("-o", "-c"):
    index = args.index(flag)
    del args[index:index + 2]
out = build / "server-info-bots-check"
out.mkdir(exist_ok=True)
obj = out / "fixture.o"
subprocess.run(args + ["-DDEDICATED=0", "-include", str(root / "Code/BF2/Core/BuildConfig.h"),
                      "-c", str(root / "Code/Tests/server_info_bots.c"), "-o", str(obj)],
               cwd=build, check=True)
objects = []
for target in ("bf2_gamespy_objects", "bf2_gamespy_sb_objects"):
    sources = [c for c in commands if (target + ".dir/") in c.get("output", "")]
    for source in sources:
        path = build / source["output"]
        if not path.exists():
            raise SystemExit("Missing SDK object; build default targets first: " + str(path))
        objects.append(str(path))
binary = out / "server_info_bots"
subprocess.run([args[0], "-arch", "x86_64", str(obj), *objects, "-Wl,-dead_strip",
                "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True)

# Check the queried-value path without sockets as well: empty publication must
# fail even for expected zeros, and stale defaults must fail a changed config.
cases = [
    (["50.000000", "16", "50.000000"], ["50", "16", "50"], True),
    (["30.500000", "12", "70.250000"], ["30.5", "12", "70.25"], True),
    (["0.000000", "0", "0.000000"], ["0", "0", "0"], True),
    (["", "", ""], ["0", "0", "0"], False),
    (["50.000000", "16", "50.000000"], ["30", "12", "70"], False),
    (["invalid", "0", "0"], ["0", "0", "0"], False),
    (["30.500000", "12.5", "70.250000"], ["30.5", "12", "70.25"], False),
]
for actual, expected, should_pass in cases:
    result = subprocess.run([str(binary), "--published", *actual, *expected],
                            capture_output=True, text=True)
    if result.returncode != (0 if should_pass else 1):
        raise SystemExit("Publication fixture unexpected result:\n" + result.stdout + result.stderr)
print("server_info_bots: 7 publication acceptance/rejection cases passed", flush=True)


def bot_query_values(reply):
    fields = reply[5:].split(b"\0")
    if reply[:5] != b"\x00\x01\x02\x03\x04" or len(fields) != 4 or fields[-1] != b"":
        raise ValueError("Malformed or unexpected bot query reply: " + repr(reply))
    return [field.decode("ascii") for field in fields[:3]]


valid_reply = b"\x00\x01\x02\x03\x04" + b"30.500000\0" + b"12\0" + b"70.250000\0"
if bot_query_values(valid_reply) != ["30.500000", "12", "70.250000"]:
    raise SystemExit("Query reply lost or reordered values")
for invalid in (valid_reply[:4], b"\x01" + valid_reply[1:],
                valid_reply[:1] + b"\x05" + valid_reply[2:], valid_reply[:-1], valid_reply + b"\0"):
    try:
        bot_query_values(invalid)
    except ValueError:
        continue
    raise SystemExit("Accepted malformed query reply: " + repr(invalid))
print("server_info_bots: query reply framing passes; 5 malformed replies rejected", flush=True)

if options.query:
    # Non-split partial query: keys 79/80/81, no player/team keys. The server
    # echoes the four-byte request ID before three NUL-terminated value strings.
    request_id = b"\x01\x02\x03\x04"
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.settimeout(3)
        probe.connect(("127.0.0.1", 29911))
        probe.send(b"\xfe\xfd\x00" + request_id + bytes([3, 79, 80, 81, 0, 0]))
        reply = probe.recv(4096)
    actual = bot_query_values(reply)
    ratio, count, difficulty = options.expect
    print("Loopback published ratio/count/difficulty: " + repr(actual), flush=True)
    subprocess.run([str(binary), "--published", *actual, str(ratio), str(int(count)),
                    str(difficulty)], check=True)
