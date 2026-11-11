#!/usr/bin/env python3
"""[CHOSEN, not recovered] Check the parser using s0's original synthetic binaries.

These bypass Microsoft compilation. They are never added to the production sweep.
"""
import csv
import hashlib
import json
from pathlib import Path
import subprocess

from pipeline import BUILD, FIELDS, REPO, S0, dump

output = REPO / "out/d3dxfx-controls"
output.mkdir(parents=True, exist_ok=True)
samples = [("metadata", S0 / "out/shaderproto/controls/effects/metadata.fx/effect.fxo", None)]
samples += [(name, S0 / "out/shaderproto" / name / "shader.bin", entry) for name, entry in
            (("control-vertex", "Vertex"), ("control-pixel", "Pixel"),
             ("control-vertex-sm3", "Vertex"), ("control-pixel-sm3", "Pixel"))]
rows, results, jobs = [], {}, []
for name, binary, entry in samples:
    directory = output / "msl" / name
    directory.mkdir(parents=True, exist_ok=True)
    for path in directory.iterdir():
        if path.is_file() and path.name != "command.json":
            path.unlink()
    command = [str(BUILD / "effectparse")]
    if entry:
        command += ["--shader", entry]
    command += [str(binary), str(directory)]
    dump(directory / "command.json", command)
    with (directory / "stdout.log").open("w") as stdout, (directory / "stderr.log").open("w") as stderr:
        child = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=30)
    if child.returncode:
        raise RuntimeError(f"{name}: parser exit {child.returncode}; see {directory}")
    result = json.loads((directory / "result.json").read_text())
    assert result["parse_ok"], name
    if entry:
        assert result["translate_ok"] and result["shader_count"] == result["msl_count"] == 1, name
        assert len(list(directory.glob("*.metal"))) == 1, name
    else:
        assert not result["translate_ok"] and result["translate_status"] == "no_shaders", name
        assert result["technique_count"] == result["pass_count"] == 1, name
        inventory = (directory / "inventory.txt").read_text()
        assert "name=Tint semantic=DIFFUSE" in inventory and "states=3" in inventory, name
    results[name] = dict(source=str(binary), sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), **result)
    rows.append(dict(stage="control", file=name, defines="{}", compile="provided_s0", parse="ok",
                     translate=result["translate_status"], error_summary="", job=name,
                     shader_count=result["shader_count"], msl_count=result["msl_count"], metal_file="", metal="not_run"))
    jobs.append(dict(id=name, file=name, defines={}))

# The parser must not accept a non-effect as a successful effect. Isolate upstream assertions.
negative = output / "negative"
negative.mkdir(exist_ok=True)
binary = negative / "invalid.fxo"
binary.write_bytes(b"not an effect\0" + bytes(64))
with (negative / "stderr.log").open("w") as stderr:
    child = subprocess.run([str(BUILD / "effectparse"), str(binary), str(negative)],
                           stdout=subprocess.DEVNULL, stderr=stderr, timeout=30)
assert child.returncode != 0, "Invalid effect accepted"
dump(output / "control-results.json", dict(positive=results, invalid_effect_exit=child.returncode))
dump(output / "jobs.json", jobs)
with (output / "results.csv").open("w", newline="") as file:
    writer = csv.DictWriter(file, fieldnames=FIELDS, lineterminator="\n")
    writer.writeheader(); writer.writerows(rows)
print(f"Controls: 1/1 metadata parse; 4/4 shader parse + MSL; invalid effect rejected ({child.returncode}).")
print(f"Artifacts: {output}")
