#!/usr/bin/env python3
"""[TEMP] macOS x86_64 fixtures with target flags; no game or audio device."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("build_dir", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = args.build_dir.resolve()
out = build / "client-effects-runtime"
out.mkdir(exist_ok=True)
commands = json.loads((build / "compile_commands.json").read_text())

# Generate aborting implementations for unused pure interface methods. Used methods
# are overridden in the fixture; a newly reached dependency fails by name.
def fake(header, interface, name):
    text = (root / "Code/BF2" / header).read_text()
    body = text.split("class " + interface + " :", 1)[1].split("\n};", 1)[0]
    body = re.sub(r"//[^\n]*", "", body)
    methods = re.findall(r"virtual\s+([^;{}]+?)\s*=\s*0\s*;", body, re.S)
    result = ["struct " + name + " : " + interface + " {"]
    result += ["void addRef() override { unexpected(\"addRef\"); }",
               "int getRef() override { unexpected(\"getRef\"); }",
               "int release() override { unexpected(\"release\"); }",
               "void* queryInterface(unsigned int) const override { unexpected(\"queryInterface\"); }"]
    for method in methods:
        declaration = " ".join(method.split())
        result.append(declaration + ' override { unexpected("' + interface + '"); }')
    for slot in re.findall(r"BF2_GTM_RESERVED\((\w+)\)", body):
        result.append('void reserved_' + slot + '() override { unexpected("reserved"); }')
    return "\n".join(result + ["};"])

(out / "client_effects_fakes.h").write_text(
    fake("Geom/IGeometryTemplate.h", "IGeometryTemplate", "UnusedTemplate") + "\n" +
    fake("Geom/GeometryTemplateManager.h", "IGeometryTemplateManager", "UnusedManager") + "\n" +
    fake("Game/Objects/IPlayer.h", "IPlayer", "UnusedPlayer") + "\n" +
    fake("Game/Objects/ICameraObject.h", "ICameraObject", "UnusedCamera") + "\n")

with (out / "results.log").open("w") as log:
    for dedicated in (0, 1):
        entry = next(e for e in commands if e["file"].endswith("/Game/Main/BF2Log.cpp")
                     and f"-DDEDICATED={dedicated}" in e["command"])
        flags = shlex.split(entry["command"])
        flags = flags[:flags.index("-o")]
        flags += ["-DBF2_STATIC_PLUGINS=1", "-D_ITERATOR_DEBUG_LEVEL=0", "-I" + str(out)]
        sources = [root / "Code/Tests/client_effects_runtime.cpp"] + [
            root / "Code/BF2" / source for source in (
                "Game/Main/BF2Log.cpp", "Game/Objects/Game/ZiplineContainer.cpp",
                "Game/Objects/Game/DestroyableWindowsBundle.cpp", "Sound/ObjectSoundNode.cpp")]
        objects = []
        for source in sources:
            obj = out / (source.stem + f"-{dedicated}.o")
            subprocess.run(flags + ["-c", str(source), "-o", str(obj)], check=True,
                           stdout=log, stderr=subprocess.STDOUT)
            objects.append(str(obj))
        stubs = out / f"stubs-{dedicated}.cpp"
        subprocess.run(["python3", str(root / "tools/macos/link_stubs.py"), str(stubs)] + objects,
                       check=True, stdout=log, stderr=subprocess.STDOUT)
        stub_obj = out / f"stubs-{dedicated}.o"
        subprocess.run(flags + ["-c", str(stubs), "-o", str(stub_obj)], check=True,
                       stdout=log, stderr=subprocess.STDOUT)
        executable = out / f"check-{dedicated}"
        subprocess.run([flags[0], "-arch", "x86_64"] + objects + [str(stub_obj), "-o", str(executable)],
                       check=True, stdout=log, stderr=subprocess.STDOUT)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        log.write(result.stdout + result.stderr)
        log.flush()
        result.check_returncode()
        if f"client effects DEDICATED={dedicated}:" not in result.stdout:
            raise RuntimeError("fixture exited before completing its checks")
print((out / "results.log").read_text())
