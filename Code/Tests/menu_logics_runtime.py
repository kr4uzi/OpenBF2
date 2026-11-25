#!/usr/bin/env python3
"""[TEMP] Headless menu lifecycle/input checks using the configured client compiler flags.

No application startup, window, content, network or audio device. Unused interface
methods and engine dependencies abort by name. Production binaries are read-only.
Usage: python3 Code/Tests/menu_logics_runtime.py out/build/client-effects
"""
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
out = build / "menu-logics-runtime"
out.mkdir(exist_ok=True)
commands = json.loads((build / "compile_commands.json").read_text())
entry = next(e for e in commands if e["file"].endswith("/Menu/EndOfRoundLogic.cpp"))
flags = shlex.split(entry["command"])
flags = flags[:flags.index("-o")] + ["-I" + str(out)]
headers = ["Menu/ISwiffPlayer.h", "Menu/ISwiffHost.h", "Menu/ISwiffRender.h",
           "Menu/IMenuTeamManager.h", "Game/ControlSettings.h", "IO/Input/Input.h",
           "IO/Console/Console.h"]
includes = out / "interfaces.cpp"
includes.write_text("".join('#include "' + str(root / "Code/BF2" / h) + '"\n' for h in headers))
preprocessed = subprocess.check_output(flags + ["-E", "-P", str(includes)], text=True)

def methods(interface):
    match = re.search(r"(?:class|struct)\s+" + interface + r"\s*(?::\s*public\s+(\w+)\s*)?\{(.*?)\n\};",
                      preprocessed, re.S)
    if not match:
        raise RuntimeError("missing interface " + interface)
    base, body = match.groups()
    inherited = methods(base) if base else []
    return inherited + re.findall(r"virtual\s+([^;{}]+?)\s*=\s*0\s*;", body, re.S)

fakes = []
for interface, namespace in [("ISwiffPlayer", "ui"), ("ISwiffHost", "ui"), ("ISwiffRender", "ui"),
                             ("IMenuTeamManager", ""), ("IControlSettings", ""),
                             ("IControlMap", "io"), ("IControlMapManager", "io"), ("IKeyboardDevice", "io"),
                             ("IAdvancedInputDevice", "io"), ("IInputDeviceManager", "io"), ("Console", "io")]:
    fakes.append("namespace dice { namespace hfe { " + ("namespace " + namespace + " {" if namespace else ""))
    fakes.append("struct Unused" + interface + " : " + interface + " {")
    for method in methods(interface):
        declaration = " ".join(method.split())
        fakes.append(declaration + ' override { unexpected("' + interface + '::' + declaration.replace('"', '\\"') + '"); }')
    fakes.append("}; " + ("}" if namespace else "") + "} }")
(out / "menu_logic_fakes.h").write_text("\n".join(fakes))

objects = []
with (out / "build.log").open("w") as log:
    for source in [root / "Code/Tests/menu_logics_runtime.cpp"] + [root / "Code/BF2" / s for s in
                   ("Menu/EndOfRoundLogic.cpp", "Menu/LoadGameLogic.cpp", "Core/ModuleRegistry.cpp",
                    "Core/ClassManager.cpp", "Core/Mutex.cpp", "Core/StringUtil.cpp")]:
        obj = out / (source.stem + ".o")
        subprocess.run(flags + ["-c", str(source), "-o", str(obj)], check=True, stdout=log, stderr=subprocess.STDOUT)
        objects.append(str(obj))
    stubs = out / "unused.cpp"
    subprocess.run([sys.executable, str(root / "tools/macos/link_stubs.py"), str(stubs)] + objects,
                   check=True, stdout=log, stderr=subprocess.STDOUT)
    stub_obj = out / "unused.o"
    subprocess.run(flags + ["-c", str(stubs), "-o", str(stub_obj)], check=True, stdout=log, stderr=subprocess.STDOUT)
    exe = out / "menu-logics-check"
    subprocess.run([flags[0], "-arch", "x86_64"] + objects + [str(stub_obj), "-o", str(exe)],
                   check=True, stdout=log, stderr=subprocess.STDOUT)
    result = subprocess.run([str(exe), "+noSound", "1"], capture_output=True, text=True)
    (out / "results.log").write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end="")
    result.check_returncode()
    if "menu logic checks passed" not in result.stdout:
        raise RuntimeError("fixture did not complete")
    # Compile the client-only bodies as DEDICATED=1 too; they must publish no methods.
    dedicated_flags = ["-DDEDICATED=1" if f == "-DDEDICATED=0" else f for f in flags]
    for name in ("EndOfRoundLogic", "LoadGameLogic"):
        obj = out / (name + "-dedicated.o")
        subprocess.run(dedicated_flags + ["-c", str(root / "Code/BF2/Menu" / (name + ".cpp")), "-o", str(obj)],
                       check=True, stdout=log, stderr=subprocess.STDOUT)
        if name in subprocess.check_output(["nm", "-C", str(obj)], text=True):
            raise RuntimeError("dedicated object contains " + name)
    symbols = subprocess.check_output(["nm", "-C", str(build / "bin_macos/bf2")], text=True)
    if "EndOfRoundLogic" in symbols or "LoadGameLogic" in symbols:
        raise RuntimeError("dedicated binary contains client menu classes")
print("dedicated objects and bf2 contain neither menu class")
