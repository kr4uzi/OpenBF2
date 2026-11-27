#!/usr/bin/env python3
"""Content-free template saves using a completed macOS renderer-enabled client build.

python3 Code/Tests/template_save_runtime.py --engine-build out/build/local --output out/template-save
Only fixture objects and an executable are written; the engine build is reused unchanged.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess


def descriptors(root, output):
    """Typed scaffolding: store authored descriptor values without renderer initialization."""
    header = (root / "Code/BF2/Geom/ParticleSystem/ParticleSystemManager.h").read_text()
    classes = {}
    for match in re.finditer(r"class (IParticle\w+)\s*(?:: public (\w+))?\s*\{(.*?)\n\};", header, re.S):
        methods = re.findall(r"virtual\s+([^;{}]+?)\s+(\w+)\(([^()]*)\)\s*(const)?\s*= 0;", match[3])
        classes[match[1]] = (match[2], methods)

    def inherited(name):
        base, methods = classes[name]
        return (inherited(base) if base in classes else []) + methods

    names = ["IParticleSystemTemplateType0", "IParticleSystemTemplateType1", "IParticleSystemTemplateType2",
             "IParticleSystemEmitterTemplate", "IParticleSystemTemplateType4"]
    code = ['// [TEMP] Generated typed descriptor inputs; only the save fixture consumes them.',
            'namespace dice { namespace hfe { namespace geom {']
    for index, name in enumerate(names):
        methods = inherited(name)
        fields = {}
        for result, method, params, const in methods:
            if method.startswith("get"):
                fields[method[3:].removesuffix("Ref")] = result.replace("const ", "").replace("&", "").strip()
        code.append(f"struct SaveDescriptor{index} : {name} {{")
        code += [f"    {type} value{field}{{}};" for field, type in fields.items()]
        code += [f"    mutable unsigned reads{method} = 0;" for _, method, _, _ in methods if method.startswith("get")]
        for result, method, params, const in methods:
            if method.startswith("get"):
                body = f"++reads{method}; return value{method[3:].removesuffix('Ref')};"
            elif method.startswith("set") and method[3:] in fields:
                arg = re.search(r"(\w+)$", params)[1]
                body = f"value{method[3:]} = {arg};"
            elif method == "refreshDerivedState":
                body = ""  # The fixture models already-authored getter inputs only.
            else:
                body = "std::abort();"
            code.append(f"    {result} {method}({params}) {const} override {{ {body} }}")
        code.append("};")
    code.append("} } }")
    (output / "template_save_descriptors.h").write_text("\n".join(code) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-build", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    build, output = args.engine_build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    descriptors(Path(__file__).resolve().parents[2], output)
    entries = json.loads((build / "compile_commands.json").read_text())
    entry = next(row for row in entries if row["file"].endswith("/Game/Main/BF2.cpp")
                 and "bf2_client_objects.dir" in row["command"])
    command = shlex.split(entry["command"])
    original = (build / command[command.index("-o") + 1]).resolve()
    link = shlex.split((build / "CMakeFiles/bf2_client.dir/link.txt").read_text())
    if any("undefined" in arg or "multiply_defined" in arg for arg in link):
        parser.error("The fixture requires a strict input link")
    main_object = output / "BF2.renamed-main.o"
    replaced = 0
    for i, arg in enumerate(link):
        if arg.endswith("/Game/Main/BF2.cpp.o"):
            assert (build / arg).resolve() == original
            link[i] = str(main_object)
            replaced += 1
    assert replaced == 1
    exe = output / "template_save_runtime"
    link[link.index("-o") + 1] = str(exe)
    link.append(str(output / "fixture.o"))
    with (output / "build.log").open("w") as log:
        renamed = command[:]
        renamed[renamed.index("-o") + 1] = str(main_object)
        renamed.append("-Dmain=bf2_fixture_unused_main")
        subprocess.run(renamed, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
        fixture = command[:]
        fixture[fixture.index("-o") + 1] = str(output / "fixture.o")
        fixture[fixture.index("-c") + 1] = str(Path(__file__).with_suffix(".cpp").resolve())
        fixture.append("-I" + str(output))
        subprocess.run(fixture, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run(link, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
    env = os.environ.copy()
    env["DYLD_LIBRARY_PATH"] = str(build / "bin_macos")
    with (output / "runtime.log").open("w") as log:
        result = subprocess.run([str(exe)], cwd=output, env=env, stdout=log, stderr=subprocess.STDOUT)
    print((output / "runtime.log").read_text(), end="")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
