#!/usr/bin/env python3
"""Content-free checks of production radio submission, relay and Player decay.

Uses the existing strict-image fixture runner; opens no window, audio device or socket.
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
    observer = ["// [TEMP] Real GameLogic storage; unrelated virtual calls abort.",
                "struct RadioLogic : GameLogic {"]
    for declaration in re.findall(r"virtual\s+([^;{}]+?)\s*=\s*0\s*;", interface):
        declaration = " ".join(declaration.split())
        body = "return false;" if re.search(r"\bisServer\s*\(", declaration) else "std::abort();"
        observer.append(declaration + " override { " + body + " }")
    observer.append("};")
    for image in ("bf2_client", "bf2"):
        output = args.output.resolve() / image
        output.mkdir(parents=True, exist_ok=True)
        (output / "radio_logic_observer.h").write_text("\n".join(observer) + "\n")
        result = run_fixture(args.engine_build, output, source, image)
        if result:
            return result
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
