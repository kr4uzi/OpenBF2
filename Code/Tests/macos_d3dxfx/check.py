#!/usr/bin/env python3
"""[TEMP] [CHOSEN, not recovered] Offline effect-input and SM1 emitter regression.

[RETIRE-WHEN: macos-effect-cache-input-tests] Retire when the production effect
cache has equivalent input/translation gates.
Optional arguments are the coordinator baseline CSV and patched replay MSL root.
No Wine, Metal device, game or server is launched.
"""
import csv
from pathlib import Path
import re
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools/macos/d3dxfx"))
import engine_inputs as inputs


def main():
    config = inputs.snapshot()
    legacy, virtual = config["legacy_macros"], config["ra_virtual"]
    assert legacy["DLHigh"] == "4" and legacy["D3DDECLTYPE_FLOAT3"] == "2"
    assert config["flags"] == 0x100 and config["pool"] is None
    assert "CEXP" not in legacy and "PSMODEL" not in legacy
    assert "DLHigh" not in virtual and "FILTER_PARTICLE_MIP" not in virtual
    assert [(p["VSMODEL"], p["PSMODEL"]) for p in config["supported_profiles"]] == [
        (vs, ps) for vs in ("vs_1_1", "vs_2_0") for ps in ("ps_1_3", "ps_1_4", "ps_2_0")]
    assert all(len(p) == 6 for p in config["supported_profiles"])
    # A synthetic archive with a metadata permutation and a shared fragment.
    out = REPO / "out"
    out.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="s1b-input-check-", dir=out) as directory:
        root = Path(directory)
        source, output = root / "src", root / "output"
        source.mkdir(); output.mkdir(); (output / "generated").mkdir()
        (source / "types.fx").write_text("typedef float scalar;\n")
        (source / "Foo.fx").write_text('#include "shaders/types.fx"\ntechnique Draw { pass P {} }\n')
        (source / "Foo_variant.fx").write_text('technique Partial { pass P {} }\n')
        (source / "RaShaderTest.fx").write_text('// #include "shaders/OldTemplate.fx"\ntechnique Variable { pass P {} }\n')
        (source / "OldTemplate.fx").write_text('float4 tex[NUM_TEXSETS]; technique Old { pass P {} }\n')
        (source / "RaShaderTest.mfx").write_text('''
string GenerateStructs[] = {"reqVertexElement"};
string reqVertexElementBase[] = {"TBase2D", "Normal"};
const dword _base_ : LITERAL;
const bool _detail_ : LITERAL < string reqVertexElement[] = {"Normal", "TDetail2D"}; >;
string GenerateShaderPermutations[] = {"_base_"};
string GenerateShaderModifiers[] = {"_detail_"};
''')
        entries = {e["file"]: e for e in inputs.inventory(source)}
        assert entries["types.fx"]["role"] == "pure_include"
        assert entries["Foo_variant.fx"]["role"] == "include_fragment"
        assert entries["RaShaderTest.fx"]["role"] == "permutation_template"
        assert entries["OldTemplate.fx"]["role"] == "include_fragment"
        assert "MetaFx::generateShader" in entries["OldTemplate.fx"]["selection_evidence"]
        jobs = list(inputs.jobs(source, output, True))
        assert len(jobs) == 13  # one legacy + two permutations x six profiles
        detail = next(j for j in jobs if j.get("permutation") == "_base_|_detail_")
        assert detail["arrays"]["reqVertexElement"] == ["TBase2D", "Normal", "TDetail2D"]
        assert detail["source_defines"]["NUM_TEXSETS"] == "2"
        assert detail["source_defines"]["TexDetailInd"] == "1"
        assert detail["compiler_defines"]["PSVERSION"] == "13"
        assert "_DETAIL_" not in detail["compiler_defines"]
        assert inputs.uncomment('"http://example" // remove\n/* gone */') == '"http://example"  \n '
        assert inputs.uncomment('float/* spacer */value') == 'float value'
    # The real coordinator failures exercise the translator, not synthetic
    # replacements for renderer state. Replay using run-all.sh --stage2-only.
    if len(sys.argv) in (2, 3):
        baseline = Path(sys.argv[1]).resolve()
        rows = list(csv.DictReader(baseline.open(newline="")))
        if len(sys.argv) == 3:
            replay = Path(sys.argv[2])
        else:
            replay = out / "s1b-regression/msl"
            parser = REPO / "out/build/s1c-fx-coverage/tools/effectparse"
            for name in sorted({r["job"] for r in rows if r["stage"] == "metal"}):
                target = replay / name
                target.mkdir(parents=True, exist_ok=True)
                with (target / "replay.log").open("w") as log:
                    result = subprocess.run([str(parser), str(baseline.parent / "bin" / (name + ".fxo")),
                                             str(target)], stdout=log, stderr=log, timeout=120)
                assert result.returncode in (0, 1), target
        repaired = unchanged = 0
        for row in rows:
            if row["stage"] != "metal":
                continue
            path = replay / row["metal_file"]
            if row["metal"] == "fail":
                text = path.read_text()
                refs = set(re.findall(r"input\.(t\d+)", text))
                fields = set(re.findall(r"float4 (t\d+) \[\[user\(texcoord", text))
                assert refs and refs <= fields, path
                assert "input [[stage_in]]" in text and not re.search(r",\s*\)\s*\{", text), path
                repaired += 1
            elif row["metal"] == "ok":
                assert path.read_bytes() == (baseline.parent / "msl" / row["metal_file"]).read_bytes(), path
                unchanged += 1
        assert (repaired, unchanged) == (18, 48)
        print("18/18 reported emission defects removed; 48/48 passing MSL sources unchanged (no Metal run).")
    print("Input vocabulary, profile order, include roles, modifiers and texture-set checks passed.")


if __name__ == "__main__":
    main()
