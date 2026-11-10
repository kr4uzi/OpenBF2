#!/usr/bin/env python3
"""[CHOSEN, not recovered] Source-derived effect inventory and engine input snapshot.

This models a selected device/settings configuration, not a captured running client.
No copyrighted content or generated job list is checked in.
"""
import hashlib
import json
from pathlib import Path
import re

REPO = Path(__file__).resolve().parents[3]
RENDER = REPO / "Code/BF2/RendDX9"


def uncomment(text):
    # Preserve strings: comment delimiters may occur inside a quoted value.
    return re.sub(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*',
                  lambda m: m[0] if m[0].startswith('"') else " " + "\n" * m[0].count("\n"),
                  text, flags=re.S)


def strings(text, name):
    match = re.search(r"\bstring\s+" + re.escape(name) + r"\s*\[\]\s*=\s*\{(.*?)\}\s*;", text, re.S)
    return re.findall(r'"([^"\n]*)"', match[1]) if match else []


def pairs(text):
    return dict(re.findall(r'\{\s*"([^"\n]+)",\s*"([^"\n]+)"\s*\}', text))


def snapshot():
    """Select SM2, non-vendor, high terrain/filter, 16x anisotropy, no AA/debug.

    Dynamic values mirror updateShaderDefines/rebuildShaderDefines. The fixed
    vocabulary and profile pairs are extracted; unsupported source drift refuses.
    """
    legacy = pairs((RENDER / "ShaderManagerDefines.inc").read_text())
    if len(legacy) != 120:
        raise ValueError("ShaderManager vocabulary changed; review engine_inputs.py")
    legacy.update(USEPARTIALPRECISIONTYPES="0", PS2_EXT="ps_2_0", NVIDIA="0", ATI="0",
                  SM30="0", IS_NV4X="0", HASALPHA2MASK="0", Alpha2Mask="PointSize", RAPATH="0",
                  A2MEnable="0", A2MDisable="0", _FORCE_1_4_SHADERS_="0", _FORCE_1_3_SHADERS_="0",
                  FILTER_TRN_DIFF_MIN="ANISOTROPIC", FILTER_TRN_DIFF_MAG="ANISOTROPIC",
                  FILTER_TRN_DIFF_MAX_ANISOTROPY="2", FILTER_ROAD_DIFF_MIN="ANISOTROPIC",
                  FILTER_ROAD_DIFF_MAG="ANISOTROPIC", FILTER_ROAD_DIFF_MAX_ANISOTROPY="4",
                  FILTER_TRN_MIP="LINEAR", FILTER_ROAD_MIP="LINEAR", FILTER_PARTICLE_MIP="LINEAR",
                  HIGHTERRAIN="1", MIDTERRAIN="0")
    ra_source = (RENDER / "Ra/EffectManager.cpp").read_text()

    def table(name):
        match = re.search(r"\b" + name + r"\[\d+\]\[2\]\s*=\s*\{(.*?)\};", ra_source, re.S)
        if not match:
            raise ValueError(f"Missing engine profile table {name}")
        return re.findall(r'\{\s*"([^"]+)",\s*"([^"]+)"\s*\}', match[1])

    profiles = []
    for vs in table("kVsModel"):
        for ps, version in zip(table("kPsModel"), table("kPsVersion")):
            profiles.append(dict([vs, ps, table("kForce14")[0], ("_FORCE_1_3_SHADERS_", "0"),
                                  ("LOWPSMODEL", "ps_1_4"), version]))
    # Virtual include is a different namespace from the D3DXMACRO argument.
    ra_virtual = dict(NVIDIA="0", ATI="0", SM30="0", SM20="1", RAPATH="0", HASALPHA2MASK="0",
                      Alpha2Mask="PointSize", A2MEnable="0x0", A2MDisable="0x0",
                      FILTER_STM_DIFF_MIN="ANISOTROPIC", FILTER_STM_DIFF_MAG="ANISOTROPIC",
                      FILTER_STM_DIFF_MAX_ANISOTROPY="2", FILTER_BM_DIFF_MIN="ANISOTROPIC",
                      FILTER_BM_DIFF_MAG="ANISOTROPIC", FILTER_BM_DIFF_MAX_ANISOTROPY="4",
                      FILTER_STM_NORM_MIP="LINEAR", FILTER_BM_NORM_MIP="LINEAR")
    paths = [RENDER / n for n in ("ShaderManager.cpp", "ShaderManagerDefines.inc", "Effect.cpp",
                                  "Ra/EffectManager.cpp", "Ra/RaShaderPermutator.cpp")]
    minimums = re.search(r"kMinPsVersion\[5\]\s*=\s*\{([^}]+)\}", ra_source)
    if not minimums:
        raise ValueError("Missing Ra pixel-profile capability thresholds")
    thresholds = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]+)u", minimums[1])]
    if len(thresholds) != 5:
        raise ValueError("Changed Ra pixel-profile thresholds")
    supported = [p for i, p in enumerate(profiles) if thresholds[i % 5] <= 0x200]
    return dict(configuration="SM2 non-vendor; quality[0]=3, quality[7]=3; MaxAnisotropy=16; "
                "min/mag anisotropy supported; AA/probes/debug/force/partial precision off",
                legacy_macros=legacy, ra_virtual=ra_virtual, profiles=profiles, supported_profiles=supported,
                selected_profile=profiles[-1], flags=0x100, pool=None,
                source_sha256={str(p.relative_to(REPO)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})


def inventory(source):
    files = sorted(p for p in source.iterdir() if p.suffix.lower() in (".fx", ".dfx", ".mfx"))
    texts = {p.name.lower(): uncomment(p.read_text(encoding="cp1252")) for p in files}
    names = {p.name.lower(): p.name for p in files}
    includes, parents = {}, {n: [] for n in names}
    inactive_templates = {n: [] for n in names}
    for name, text in texts.items():
        includes[name] = re.findall(r'^\s*#\s*include\s+"([^"]+)"', text, re.M)
        for include in includes[name]:
            child = include.replace("\\", "/").split("/")[-1].lower()
            if child not in names:
                raise ValueError(f"Missing corpus include {include} from {names[name]}")
            parents[child].append(names[name])
    # MetaFx::generateShader emits wrappers around its matching .fx master,
    # never an inactive alternate include. Retain that dependency as evidence
    # instead of inventing the alternate's NUM_TEXSETS/literal inputs.
    for p in files:
        if not p.with_suffix(".mfx").is_file():
            continue
        for include in re.findall(r'^\s*//\s*#\s*include\s+"([^\"]+)"', p.read_text(encoding="cp1252"), re.M):
            child = include.replace("\\", "/").split("/")[-1].lower()
            if child in inactive_templates:
                inactive_templates[child].append(p.name)

    def techniques(name, seen):
        if name in seen:
            return False
        return bool(re.search(r"\btechnique\b", texts[name])) or any(
            techniques(n.replace("\\", "/").split("/")[-1].lower(), seen | {name}) for n in includes[name])

    result = []
    for p in files:
        name = p.name.lower()
        meta = p.with_suffix(".mfx").is_file()
        # Ra's directory loader scans all rashader*.fx except BM/SM/STM masters.
        ra = name.startswith("rashader") and p.suffix.lower() == ".fx"
        has_techniques = techniques(name, set())
        if p.suffix.lower() == ".mfx":
            role = "metadata"
        elif meta:
            role = "permutation_template"
        elif inactive_templates[name] and not parents[name] and not ra:
            role = "include_fragment"
        elif not has_techniques:
            role = "pure_include"
        elif p.suffix.lower() == ".dfx" or (parents[name] and not ra):
            role = "include_fragment"
        elif "_" in p.stem and (source / (p.stem.split("_")[0] + ".fx")).is_file() and not ra:
            role = "include_fragment"  # inactive legacy variant; parent includes may be commented
        else:
            role = "effect_candidate"
        result.append(dict(file=p.name, role=role, family="ra" if ra or meta else "legacy",
                           includes=includes[name], included_by=parents[name],
                           has_techniques=has_techniques,
                           selection_evidence=("Inactive include of permutation template " + ", ".join(inactive_templates[name])
                               + "; MetaFx::generateShader wraps only the matching master"
                               if inactive_templates[name] else "Ra directory scan" if ra else "[INFERRED] content include graph")))
    return result


def ra_jobs(source, output, config, all_permutations=False):
    for metadata in sorted(source.glob("*.mfx")):
        family = metadata.stem
        text = uncomment(metadata.read_text(encoding="cp1252"))
        if re.search(r"^\s*#", text, re.M):
            raise ValueError(f"Metadata now needs include/preprocessor expansion; review generator: {metadata}")
        base = strings(text, "GenerateShaderPermutations")
        modifiers = strings(text, "GenerateShaderModifiers")
        permutations = ([v for p in base for v in [p] + [p + "|" + m for m in modifiers]]
                        if all_permutations else base[:3])
        literals = re.findall(r"const\s+(?:dword|int|bool)\s+(\w+)\s*:\s*LITERAL\s*(?:<(.*?)>)?\s*;", text, re.S)
        for index, permutation in enumerate(permutations):
            selected = set(permutation.split("|"))
            if not selected.issubset({name for name, _ in literals}):
                raise ValueError(f"Unknown literal in {permutation}")
            definitions = {name.upper(): str(int(name in selected)) for name, _ in literals}
            lines = ["// [CHOSEN, not recovered] Input following MetaFx::generateShader."]
            lines += [f"#define {key} {value}" for key, value in definitions.items()]
            lines += [f"static const dword {name.lower()} = {definitions[name.upper()]};" for name, _ in literals]
            texsets, arrays = [], {}
            for structure in strings(text, "GenerateStructs"):
                members = strings(text, structure + "Base")
                for literal, annotations in literals:
                    if literal in selected:
                        members += strings(annotations or "", structure)
                members = list(dict.fromkeys(members))
                arrays[structure] = members
                if members:
                    lines.append(f"string {structure}[] = {{" + ",".join(json.dumps(s) for s in members) + "};")
                for member in members:
                    if "2D" in member and member.startswith("T"):
                        texsets.append("Tex" + member[1:-2] + "Ind")
            generated = {"NUM_LIGHTS": "1", "NUM_TEXSETS": str(len(texsets))}
            generated.update({name: str(i) for i, name in enumerate(texsets)})
            lines += [f"#define {key} {value}" for key, value in generated.items()]
            lines.append(f'#include "shaders/{family}.fx"')
            identifier = f"ra-{family}-{index}"
            wrapper = output / "generated" / (identifier + ".fx")
            wrapper.write_text("\n".join(lines) + "\n")
            yield dict(id=identifier, file=family + ".fx", source=str(wrapper), kind="ra",
                       permutation=permutation, arrays=arrays, source_defines={**definitions, **generated},
                       compiler_defines=config["selected_profile"], virtual_defines=config["ra_virtual"],
                       flags=config["flags"], include_mode="ra",
                       metadata_sha256=hashlib.sha256(metadata.read_bytes()).hexdigest())


def jobs(source, output, all_permutations=False, profile_mode="engine"):
    config = snapshot()
    entries = inventory(source)
    (output / "engine-inputs.json").write_text(json.dumps(dict(**config, inputs=entries), indent=2) + "\n")
    virtual_root = output / "virtual"
    virtual_root.mkdir(exist_ok=True)
    (virtual_root / "RaDefines.fx").write_text("".join(f"#define {k} {v}\n" for k, v in config["ra_virtual"].items()))
    candidates = []
    for entry in entries:
        if entry["role"] != "effect_candidate":
            continue
        ra = entry["family"] == "ra"
        candidates.append(dict(id="engine-" + entry["file"], file=entry["file"], source=str(source / entry["file"]),
                   kind="ra_direct" if ra else "legacy", flags=config["flags"],
                   compiler_defines=config["selected_profile"] if ra else config["legacy_macros"],
                   virtual_defines=config["ra_virtual"] if ra else {}, include_mode="ra" if ra else "legacy"))
    candidates += list(ra_jobs(source, output, config, all_permutations))
    for job in candidates:
        job["effect_group"] = job["id"]
        if job["include_mode"] != "ra":
            yield job
            continue
        profiles = config["supported_profiles"] if profile_mode == "engine" else [config["selected_profile"]]
        # The offline compiler has no device/FindNextValidTechnique. Retain every
        # supported attempt, in engine order; do not pretend compile success picks
        # the same first usable technique as a running D3D9 client.
        for rank, profile in enumerate(profiles):
            yield dict(job, id=job["id"] + "-" + profile["VSMODEL"] + "-" + profile["PSMODEL"],
                       compiler_defines=profile, profile_rank=rank)
