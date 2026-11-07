#!/usr/bin/env python3
"""[TEMP] Assemble self-contained Intel/M2 TSan checks, runtimes and APFS-cloned smoke content.

Usage: prepare-tsan.py <x86_64 build> <arm64 build> <existing smoke root>
Never builds or runs a server and never changes the supplied content. Output is under out/m2/tsan.
"""
from pathlib import Path
import shutil
import subprocess
import sys

repo = Path(__file__).resolve().parents[3]
out = repo / "out/m2/tsan"
out.mkdir(parents=True, exist_ok=True)
for name in ("run-tsan.sh", "smoke.tcl"):
    shutil.copy2(Path(__file__).with_name(name), out / name)
(out / "run-tsan.sh").chmod(0o755)
runtime = Path(subprocess.check_output(["xcrun", "clang", "-print-resource-dir"], text=True).strip()) / "lib/darwin/libclang_rt.tsan_osx_dynamic.dylib"
for arch, build in zip(("x86_64", "arm64"), sys.argv[1:3]):
    dest = out / arch
    dest.mkdir(exist_ok=True)
    binaries = ("bf2", "libdice_py.dylib", "libaidll.dylib", "s2_10_boundaries", "s2_10_audio_worker")
    for name in binaries:
        shutil.copy2(Path(build).resolve() / "bin_macos" / name, dest / name)
    shutil.copy2(runtime, dest / runtime.name)
    for name in binaries:
        binary = dest / name
        libraries = subprocess.check_output(["otool", "-L", str(binary)], text=True)
        for line in libraries.splitlines()[1:]:
            old = line.strip().split(" (", 1)[0]
            if old.endswith(runtime.name):
                subprocess.run(["install_name_tool", "-change", old, "@loader_path/" + runtime.name, str(binary)], check=True)
        subprocess.run(["codesign", "--force", "--sign", "-", str(binary)], check=True)
    subprocess.run(["codesign", "--force", "--sign", "-", str(dest / runtime.name)], check=True)
content = Path(sys.argv[3]).resolve()
if not (out / "root").exists():
    subprocess.run(["cp", "-cR", str(content), str(out / "root")], check=True)
# Same offline policy as docs/macos-port.md; reference installation stays read-only.
(out / "offline.sb").write_text('''(version 1)
(allow default)
(deny network-outbound)
(allow network-outbound (remote ip "localhost:*"))
(deny network-outbound (remote unix-socket (path-literal "/private/var/run/mDNSResponder")))
(deny file-write* (subpath "/Users/krauzi/Applications/Battlefield 2.app"))
(deny network-bind (local ip "*:16567"))
''')
(out / "README.txt").write_text("S2.10 [TEMP] TSan bundle. Run ./run-tsan.sh on Intel or M2.\n"
    "Apple /usr/bin/expect is used; no Python or Command Line Tools needed on M2.\n"
    "Return results.txt, server-*.log and tsan-* files. Raw PCs are retained (symbolize=0); keep binaries for symbolication.\n"
    "Contains APFS-cloned retail content: keep private; do not commit/distribute publicly.\n")
print(out)
