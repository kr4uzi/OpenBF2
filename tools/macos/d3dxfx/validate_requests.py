#!/usr/bin/env python3
"""[CHOSEN, not recovered] Check enumerate_requests.py's grammar against earlier inputs.

1. s2 loader requests (requests/<key>.request written on a cache miss): unpack each,
   find the enumerated request with the same effect, recompute its key with the
   production capture (cache-tool) and compare key, root, macros and include closure.
2. s1b/s1c jobs.json manifests: compare macro sets, profile attempts, RaDefines bytes
   and generated wrapper text for the effects both describe.
Run enumerate_requests.py with --caps s1b-sm2 first: s1b modeled an SM2 device.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

REPO = Path(__file__).resolve().parents[3]
TOOL = REPO / "out/build/s2-fx-runtime/Code/BF2/Platform/Gfx/Effect/bf2_fx_cache_tool"


def semantics(text):
    """Macro lines (whitespace-normalized), dword literals, string arrays and the include."""
    import re
    t = text.decode("cp1252")
    lines = sorted(" ".join(l.split()) for l in t.splitlines() if l.startswith(("#define", "static", "#include")))
    arrays = {m[1]: re.findall(r'"([^"]*)"', m[2]) for m in re.finditer(r"string\s+(\w+)\[\]\s*=\s*\{(.*?)\};", t, re.S)}
    return lines, arrays


def run(*args):
    return subprocess.check_output([str(TOOL), *map(str, args)], text=True).strip()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--requests", type=Path, required=True, help="enumerate_requests.py output directory")
    p.add_argument("--config", required=True, help="enumerated config ID to compare against")
    p.add_argument("--corpus", type=Path, required=True, help="flat include directory (extracted under out/)")
    p.add_argument("--s2", type=Path, action="append", default=[], help="s2 cache directory with requests/")
    p.add_argument("--s1b", type=Path, action="append", default=[], help="s1b/s1c jobs.json")
    p.add_argument("--work", type=Path, default=REPO / "out/c-fx/validate")
    a = p.parse_args()
    a.work.mkdir(parents=True, exist_ok=True)
    reqs = [r for r in json.loads((a.requests / "requests.json").read_text()) if a.config in r["configs"]]
    by_effect = {}
    for r in reqs:
        by_effect.setdefault(r["effect"].split("/")[-1].lower(), []).append(r)
    report = dict(s2=[], s1b=[])
    for cache in a.s2:
        for request in sorted((cache / "requests").glob("*.request")):
            d = a.work / ("s2-" + request.stem[:12])
            run("unpack", request, d)
            flags, dll, translator = (d / "info.txt").read_text().split()
            source = (d / "source.fx").read_bytes()
            theirs = dict(l.split("=", 1) for l in (d / "defines.txt").read_text().splitlines())
            match = None
            for r in reqs:
                if Path(r["source"]).read_bytes() == source and dict(r["macros"]) == theirs:
                    match = r
            row = dict(request=request.name, root_bytes=len(source))
            if not match:
                row["effect_in_root"] = source[:40].decode("latin-1")
                cands = [r for r in reqs if Path(r["source"]).read_bytes() == source]
                row.update(result="no enumerated request with identical root and macros",
                           root_matches=len(cands),
                           macro_diffs=[{k: (theirs.get(k), dict(c["macros"]).get(k)) for k in set(theirs) | set(dict(c["macros"]))
                                         if theirs.get(k) != dict(c["macros"]).get(k)} for c in cands[:3]])
            else:
                defs = d / "mine-defines.txt"
                defs.write_text("".join(f"{k}={v}\n" for k, v in sorted(dict(match["macros"]).items())))
                # Flags are recorded separately: s2 also stored explicit flags-zero controls.
                key = run("capture", match["source"], defs, flags, dll, translator, a.corpus,
                          match["include_mode"], match["virtual_root"], d / "mine.request", d / "mine.key")
                row.update(result=("key identical" if key == request.stem else "key differs")
                           + ("" if int(flags) == match["flags"] else " (request flags %s, engine %s)" % (flags, match["flags"])),
                           enumerated=match["id"], key=key)
                if key != request.stem:
                    run("unpack", d / "mine.request", d / "mine")
                    theirs_inc = (d / "includes.tsv").read_text().splitlines()
                    ours_inc = (d / "mine" / "includes.tsv").read_text().splitlines()
                    row["include_list_identical"] = theirs_inc == ours_inc
                    row["include_bytes_differ"] = [t.split("\t")[3] for t in theirs_inc if (d / t.split("\t")[4]).read_bytes()
                                                   != (d / "mine" / t.split("\t")[4]).read_bytes()] if theirs_inc == ours_inc else None
            report["s2"].append(row)
    for manifest in a.s1b:
        jobs = json.loads(manifest.read_text())
        for job in jobs:
            if job.get("kind") == "metadata":
                continue
            name = job["file"].lower()
            mine = by_effect.get(name) if job["kind"] != "ra" else None
            row = dict(manifest=str(manifest), job=job["id"], kind=job["kind"])
            if job["kind"] == "ra":
                fam = job["file"][:-3]
                cands = [r for r in reqs if r.get("family") == fam and r.get("permutation") == job["permutation"]]
                row["permutation"] = job["permutation"]
                if not cands:
                    row["result"] = "permutation not registered in this config"
                    report["s1b"].append(row)
                    continue
                gen = Path(job["source"])
                if not gen.is_file():
                    gen = manifest.parent / "generated" / gen.name
                theirs, ours = gen.read_bytes(), Path(cands[0]["source"]).read_bytes()
                row["wrapper_identical"] = theirs == ours
                row["wrapper_semantics_identical"] = semantics(theirs) == semantics(ours)
                mine = cands
            if not mine:
                row["result"] = "not requested by the engine walk"
                report["s1b"].append(row)
                continue
            profiles = [r for r in mine if dict(r["macros"]) == job["compiler_defines"]]
            row["macros_identical"] = bool(profiles)
            if not profiles:
                m = dict(mine[0]["macros"])
                row["macro_diffs"] = {k: (job["compiler_defines"].get(k), m.get(k)) for k in set(m) | set(job["compiler_defines"])
                                      if job["compiler_defines"].get(k) != m.get(k)}
            vfile = Path(job.get("virtual_root") or manifest.parent / "virtual") / "RaDefines.fx"
            if job.get("include_mode") == "ra" and vfile.is_file():
                row["radefines_identical"] = vfile.read_bytes() == (Path(mine[0]["virtual_root"]) / "RaDefines.fx").read_bytes()
            row["flags_identical"] = job.get("flags") == mine[0]["flags"]
            report["s1b"].append(row)
    (a.work / "validation.json").write_text(json.dumps(report, indent=1) + "\n")
    summary = dict(s2={}, s1b={})
    for row in report["s2"]:
        summary["s2"][row["result"]] = summary["s2"].get(row["result"], 0) + 1
    for row in report["s1b"]:
        k = row["kind"] + ": " + ", ".join(f"{n}={row[n]}" for n in ("result", "macros_identical", "flags_identical",
                                                                    "radefines_identical", "wrapper_identical",
                                                                    "wrapper_defines_identical") if n in row)
        summary["s1b"][k] = summary["s1b"].get(k, 0) + 1
    print(json.dumps(summary, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
