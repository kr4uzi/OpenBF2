#!/usr/bin/env python3
"""Content-free production AI material-effect queue checks; no engine startup or audio.

Runs client and dedicated bodies from completed macOS builds using typed observers.
"""
import argparse
from pathlib import Path
import re
from console_bindings_runtime import run_fixture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).with_suffix(".cpp")
    interface = (source.parents[1] / "BF2/Game/IGameLogic.h").read_text()
    interface = re.sub(r"//[^\n]*", "", interface)
    observer = ["// [TEMP] Typed observer; unused pure virtual calls abort.",
                "struct ObservedGameLogic : dice::hfe::IGameLogic {"]
    for declaration in re.findall(r"virtual\s+([^;{}]+?)\s*=\s*0\s*;", interface):
        declaration = " ".join(declaration.split())
        if re.search(r"\bplayEffect\s*\(", declaration):
            observer.append("void playEffect(Vec3 p, const world::IObjectTemplate* e, Vec3 up) override { recordEffect(p, e, up); }")
        else:
            observer.append(declaration + " override { std::abort(); }")
    observer.append("};")
    for image in ("bf2_client", "bf2"):
        output = args.output.resolve() / image
        output.mkdir(parents=True, exist_ok=True)
        (output / "ai_effect_observer.h").write_text("\n".join(observer) + "\n")
        result = run_fixture(args.engine_build, output, source, image)
        if result:
            return result
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
