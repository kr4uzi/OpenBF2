#!/usr/bin/env python3
"""[CHOSEN, not recovered] Apply shared native-source zlib MojoShader patches, offline."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    source, report = map(Path, sys.argv[1:])
    records = []
    for patch in sorted((Path(__file__).resolve().parents[1] / "d3dxfx/patches").glob("*.patch")):
        command = ["patch", "-p1", "--batch", "--fuzz=0", "-i", str(patch.resolve())]
        forward = subprocess.run(command + ["--forward", "--dry-run"], cwd=source,
                                 capture_output=True, text=True)
        if forward.returncode == 0:
            subprocess.run(command + ["--forward"], cwd=source, check=True)
        else:
            reverse = subprocess.run(command + ["--reverse", "--dry-run"], cwd=source,
                                     capture_output=True, text=True)
            if reverse.returncode:
                raise RuntimeError(f"MojoShader patch drift: {patch.name}\n"
                                   f"{forward.stdout}{forward.stderr}{reverse.stdout}{reverse.stderr}")
        records.append(dict(patch=patch.name, sha256=hashlib.sha256(patch.read_bytes()).hexdigest()))
    paths = [source / n for n in ("mojoshader.c", "mojoshader.h", "mojoshader_effects.c",
                                  "profiles/mojoshader_profile_metal.c")]
    report.write_text(json.dumps(dict(patches=records, source_sha256={str(p.relative_to(source)):
                                    hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}), indent=2) + "\n")


if __name__ == "__main__":
    main()
