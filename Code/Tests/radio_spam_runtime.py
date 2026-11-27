#!/usr/bin/env python3
"""Content-free checks of radio charges, revive refusals, relay and Player decay.

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
    # [TEMP] Compile the production notification branch verbatim without constructing
    # ServerGameLogic's round, Python and network owners. Health/revive eligibility is
    # outside this fixture; the actual submission, HUD dispatch and relay bodies run.
    revive = (source.parents[1] / "BF2/Game/GameLogic/ServerGameLogic.cpp").read_text()
    start = revive.index('        char voiceMessage[36];', revive.index('bool ServerGameLogic::resurrect('))
    end = revive.index('\n        if (occupant)', start)
    callout = ('// [TEMP] Production successful-revive notification branch, copied verbatim.\n'
               'void reviveCallout(world::IPlayer* player)\n{\n' + revive[start:end] + '\n}\n')
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
        (output / "radio_revive_callout.h").write_text(callout)
        result = run_fixture(args.engine_build, output, source, image)
        if result:
            return result
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
