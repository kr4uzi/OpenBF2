#!/usr/bin/env python3
"""[TEMP] [CHOSEN, not recovered] Check s1c controls and the 751-library baseline.

[RETIRE-WHEN: macos-effect-cache-input-tests] Keep until equivalent cache gates exist.
Requires a completed --controls --coverage-controls --metal pipeline run. Never
launches Wine, an engine, a server, or a draw; Metal tests compile libraries only.
"""
import csv
import json
import signal
from pathlib import Path
import struct
import subprocess
import sys

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools/macos/d3dxfx"))
from pipeline import BUILD, FIELDS, dump, owned


def object_blocks(blob):
    """Locate object payloads in this test's FX2 fixture for negative mutations."""
    offset = 8 + struct.unpack_from("<I", blob, 4)[0]

    def take(n):
        nonlocal offset
        values = struct.unpack_from("<" + "I" * n, blob, offset)
        offset += 4 * n
        return values

    params, techniques, _, _ = take(4)
    for _ in range(params):
        _, _, _, annotations = take(4)
        offset += 8 * annotations
    for _ in range(techniques):
        _, annotations, passes = take(3)
        offset += 8 * annotations
        for _ in range(passes):
            _, annotations, states = take(3)
            offset += 8 * annotations + 16 * states
    small, large = take(2)
    blocks = []
    for _ in range(small):
        _, length = take(2)
        blocks.append(("small", offset, length))
        offset += (length + 3) & ~3
    for _ in range(large):
        _, _, _, _, kind, length = take(6)
        blocks.append((kind, offset, length))
        offset += (length + 3) & ~3
    assert offset == len(blob)
    return blocks


def main():
    current = owned(Path(sys.argv[1] if len(sys.argv) > 1 else "out/d3dxfx-s1c"))
    baseline = owned(Path(sys.argv[2] if len(sys.argv) > 2 else "out/d3dxfx-s1c-before"))
    output = owned(REPO / "out/s1c-coverage-regression")
    output.mkdir(exist_ok=True)
    runtime = output / "reference-runtime"
    subprocess.run(["clang", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DMOJOSHADER_EFFECT_SUPPORT",
        "-I", str(REPO / "out/third_party/mojoshader"), str(Path(__file__).with_name("reference_runtime.c")),
        str(BUILD / "mojoshader/libmojoshader.a"), "-o", str(runtime)], check=True)
    binary = current / "bin/regression-shader-references.fx.fxo"
    subprocess.run([str(runtime), str(binary)], check=True)
    expression_binary = current / "bin/regression-state-expressions.fx.fxo"
    subprocess.run([str(runtime), str(expression_binary), "expressions"], check=True)
    frontier = subprocess.run([str(runtime), str(expression_binary), "frontier"], capture_output=True, text=True)
    assert frontier.returncode == -signal.SIGABRT
    assert "[FRONTIER] MOJOSHADER_effectBeginPass dynamic state expression evaluator" in frontier.stderr
    (output / "expression-frontier.log").write_text(frontier.stderr)
    expressions = json.loads((current / "msl/regression-state-expressions.fx/result.json").read_text())
    assert expressions["state_expression_count"] == 2
    refs = json.loads((current / "msl/regression-shader-references.fx/result.json").read_text())
    assert refs["shader_reference_count"] == 4 and refs["shader_count"] == 4
    blob = binary.read_bytes()
    blocks = object_blocks(blob)
    for name, original, replacement in (("unknown", b"SelectedVertex", b"Missing_Vertex"),
                                         ("out-of-range", b"VertexChoices[1]", b"VertexChoices[9]"),
                                         ("bad-index", b"VertexChoices[1]", b"VertexChoices[x]")):
        changed = bytearray(blob)
        block = next((o, n) for k, o, n in blocks if k == 1 and original in blob[o:o+n])
        offset = blob.index(original, block[0], block[0] + block[1])
        assert len(original) == len(replacement)
        changed[offset:offset+len(original)] = replacement
        target = output / (name + ".fxo"); target.write_bytes(changed)
        subprocess.run([str(runtime), str(target), "reject"], check=True)
    # Fail the second compiled shader, with both initialized and future slots present.
    shader_blocks = [(o, n) for k, o, n in blocks if k == "small" and blob[o:o+4] in
                     (b"\x01\x01\xfe\xff", b"\x01\x01\xff\xff")]
    assert len(shader_blocks) >= 2
    changed = bytearray(blob); changed[shader_blocks[1][0]:shader_blocks[1][0]+4] = b"BAD!"
    target = output / "bad-shader.fxo"; target.write_bytes(changed)
    subprocess.run([str(runtime), str(target), "reject"], check=True)

    textures = current / "msl/regression-sm1-texture-operations.fx"
    sources = [p.read_text() for p in sorted(textures.glob("*.metal"))]
    assert len(sources) == 3
    assert any("r2.xyz = t1.xyz;" in s and "sample(s1, r2.xy)" in s for s in sources)
    assert any("sample(s0, (t2.xy / t2.w))" in s for s in sources)
    assert any("clamp(input.t0, 0.0, 1.0)" in s for s in sources)
    # Standalone controls cover SM1.1/1.2 TEXCOORD and SM1.4 _dz, in addition
    # to Microsoft's compiled _dw/dependent-read fixture above.
    controls = [(f"texcoord-1{v}", [0xffff0100 | v, 64, 0xb00f0000,
                 1, 0x800f0000, 0xb0e40000, 0xffff]) for v in (1, 2)]
    controls += [("texld-dz", [0xffff0104, 66, 0x800f0000, 0xb9e40001, 0xffff])]
    msl = output / "msl"; msl.mkdir(exist_ok=True)
    rows = []
    for name, tokens in controls:
        shader = output / (name + ".bin"); shader.write_bytes(struct.pack("<"+"I"*len(tokens), *tokens))
        directory = msl / name; directory.mkdir(exist_ok=True)
        subprocess.run([str(BUILD / "effectparse"), "--shader", name.replace("-", "_"),
                        str(shader), str(directory)], check=True, stdout=subprocess.DEVNULL)
        source = (directory / "object_0.metal").read_text()
        assert ("clamp(input.t0, 0.0, 1.0)" if "texcoord" in name else "t1.xy / t1.z") in source
        rows.append(dict(stage="control", job=name, file=name, defines="{}", compile="synthetic_tokens",
                         parse="ok", translate="ok", shader_count=1, msl_count=1))
    with (output / "results.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, FIELDS, lineterminator="\n"); writer.writeheader(); writer.writerows(rows)
    subprocess.run([str(BUILD / "metal-compile"), str(msl), str(output / "results.csv")], check=True)

    old = [r for r in csv.DictReader((baseline / "results.csv").open()) if r["stage"] == "metal"]
    measured = {r["metal_file"]: r["metal"] for r in csv.DictReader((current / "results.csv").open()) if r["stage"] == "metal"}
    assert len(old) == 751 and all(r["metal"] == "ok" for r in old)
    for row in old:
        name = row["metal_file"]
        assert (baseline / "msl" / name).read_bytes() == (current / "msl" / name).read_bytes(), name
        assert measured[name] == "ok", name
    dump(output / "checks.json", dict(previous_libraries_unchanged_and_metal_ok=751,
         live_shader_reference_and_clone=True, malformed_rejections=4, extra_sm1_libraries=3))
    print("751/751 baseline MSL sources unchanged and still Metal-ok; reference/SM1 controls passed.")


if __name__ == "__main__":
    main()
