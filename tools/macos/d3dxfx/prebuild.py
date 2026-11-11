#!/usr/bin/env python3
"""[CHOSEN, not recovered] Prebuild the engine-reachable request set, round by round.

For each (config, effect) group from enumerate_requests.py, the engine tries its profile
attempts in order and stops at the first created effect with a valid technique
(Ra/EffectManager.cpp compileEffect). Round r builds attempt r of every group that has
no published attempt yet, through build-cache.py (Wine + D3DX9_25, MojoShader, cache
publication). The attempts actually built are the requests the engine would form once
the cache is warm; later attempts of a resolved group are never requested.

Offline there is no device: a published effect counts as "valid technique". The
macOS validator (G4) can still reject a technique and move the engine on; those
requests would then miss and be recorded at run time.
"""
import argparse
import csv
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

REPO = Path(__file__).resolve().parents[3]


DETAIL = {"X4532": "cannot map expression to pixel shader instruction set",
                  "X4520": "texcoord read plus sampling (ps_1_x)", "X4507": "constant register limit",
                  "X4523": "dependent texture read (ps_1_x)", "X4516": "texcoord shared by samplers (ps_1_x)",
                  "X4502": "texcoord/register reference exceeds shader model range", "X4510": "sampler count exceeds shader model",
                  "X4512": "texcoord w component in general expression (ps_1_x)", "X4521": "out of temporary registers", "X4500": "program too complex for ps_1_4",
                  "X3501": "entrypoint not found", "X3000": "syntax error", "X3004": "undeclared identifier"}


def classify(log):
    text = log.read_bytes().decode("latin-1") if log.is_file() else ""
    if "There were no techniques" in text:
        return "no techniques (metadata compiler only)"
    codes = re.findall(r"error (X\d+)", text)
    if codes:
        return name(codes[0])
    if "CompileEffect HRESULT=0x8007000e" in text:
        return "E_OUTOFMEMORY in CompileEffect"
    if "CompileEffect HRESULT=0x0" in text or "parse=" not in text:
        return "post-compile (FX2 parse/translation/publication)"
    return "compiler refusal (unclassified)"


def name(cls):
    code = cls.split()[0]
    return code + " " + DETAIL[code] if code in DETAIL else cls


def metal_gate(a, keys):
    """metal-compile over copies of each published entry's object_*.metal (newLibraryWithSource)."""
    gate = a.work / "metal"
    if gate.exists():
        shutil.rmtree(gate)
    (gate / "msl").mkdir(parents=True)
    count = 0
    for key in keys:
        for metal in (a.cache / "entries" / key).glob("object_*.metal"):
            (gate / "msl" / key).mkdir(exist_ok=True)
            shutil.copyfile(metal, gate / "msl" / key / metal.name)
            count += 1
    (gate / "jobs.json").write_text("[]\n")
    (gate / "results.csv").write_text("stage,file,defines,compile,parse,translate,error_summary,job,shader_count,"
                                      "msl_count,metal_file,metal\n")
    start = time.time()
    rc = subprocess.run([str(REPO / "tools/macos/d3dxfx/metal-compile"), str(gate / "msl"), str(gate / "results.csv")],
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    with (gate / "results.csv").open(newline="") as f:
        rows = list(csv.DictReader(f))
    ok = sum(r["metal"] == "ok" for r in rows)
    warned = sum(r["metal"] == "ok" and bool(r["error_summary"]) for r in rows)
    return dict(entries=len(keys), libraries=count, compiled=ok, compiled_with_warnings=warned,
                failed=len(rows) - ok, exit=rc.returncode,
                seconds=round(time.time() - start, 1), device=next((l for l in rc.stdout.splitlines() if l.startswith("Metal device")), None))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--requests", type=Path, default=REPO / "out/c-fx/requests")
    p.add_argument("--config", action="append", default=[], help="config IDs; default: every enumerated config")
    p.add_argument("--corpus", type=Path, default=REPO / "out/c-fx/corpus")
    p.add_argument("--cache", type=Path, default=REPO / "out/c-fx/cache")
    p.add_argument("--work", type=Path, default=REPO / "out/c-fx/prebuild")
    p.add_argument("--kinds", default="ra_metadata,ra_direct,ra_generated,legacy")
    p.add_argument("--max-rounds", type=int, default=10)
    p.add_argument("--timeout", type=int, default=36000)
    p.add_argument("--parallel", type=int, default=4, help="concurrent fxcompile/cache-tool processes")
    p.add_argument("--metal", action="store_true", help="compile every published entry's MSL with Metal")
    a = p.parse_args()
    for path in (a.cache, a.work):
        if not path.resolve().is_relative_to(REPO / "out"):
            raise ValueError("Writes must be below out/")
    a.work.mkdir(parents=True, exist_ok=True)
    jobs = {j["id"]: j for j in json.loads((a.requests / "jobs.json").read_text())}
    kinds = set(a.kinds.split(","))
    every = json.loads((a.requests / "groups.json").read_text())
    if not a.config:
        a.config = list(dict.fromkeys(g["config"] for g in every))
    groups = [g for g in every if g["config"] in a.config and g["kind"] in kinds]
    state_path = a.work / "state.json"
    state = json.loads(state_path.read_text()) if state_path.is_file() else dict(results={}, rounds=[])
    results = state["results"]
    for rank in range(a.max_rounds):
        pending = []
        for g in groups:
            done = any(results.get(x, {}).get("ok") for x in g["attempts"])
            if not done and rank < len(g["attempts"]) and g["attempts"][rank] not in results:
                pending.append(g["attempts"][rank])
        pending = list(dict.fromkeys(pending))
        if not pending:
            continue
        manifest = a.work / f"jobs-r{rank}.json"
        manifest.write_text(json.dumps([jobs[x] for x in pending], indent=1) + "\n")
        work = a.work / f"r{rank}"
        start = time.time()
        rc = subprocess.run([sys.executable, str(Path(__file__).with_name("build-cache.py")), "--manifest", str(manifest),
                             "--source", str(a.corpus), "--cache", str(a.cache), "--work", str(work),
                             "--timeout", str(a.timeout), "--parallel", str(a.parallel)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        seconds = time.time() - start
        out = json.loads((work / "results.json").read_text())
        for e in out["entries"]:
            rec = dict(ok=e["ok"], key=e["key"], rank=rank, result=e["result"])
            if not e["ok"]:
                rec["class"] = classify(Path(e["directory"]) / "compile.log")
                rec["directory"] = e["directory"]
            for job_id in e.get("aliases", [e["job"]]):
                results[job_id] = rec
        state["rounds"].append(dict(rank=rank, jobs=len(pending), seconds=round(seconds, 1), wine_exit=out["wine_exit"],
                                    published=sum(e["ok"] for e in out["entries"]), tail=rc.stdout[-300:]))
        state_path.write_text(json.dumps(state, indent=1) + "\n")
        print(f"round {rank}: {len(pending)} jobs, {state['rounds'][-1]['published']} published, {seconds:.0f} s", flush=True)
    # Per-config summary: requests the engine forms with this cache, and unresolved groups.
    summary = dict(rounds=state["rounds"], configs={})
    for cfg in a.config:
        mine = [g for g in groups if g["config"] == cfg]
        formed, refused, unresolved, ranks = set(), {}, [], {}
        for g in mine:
            for x in g["attempts"]:
                if x not in results:
                    break
                formed.add(x)
                if results[x]["ok"]:
                    ranks[results[x]["rank"]] = ranks.get(results[x]["rank"], 0) + 1
                    break
                c = name(results[x]["class"])
                refused[c] = refused.get(c, 0) + 1
            else:
                unresolved.append(g["effect"])
        summary["configs"][cfg] = dict(groups=len(mine), requests_formed=len(formed),
                                       published=sum(results[x]["ok"] for x in formed),
                                       refused_by_class=refused, resolved_at_rank=ranks,
                                       unresolved=sorted(set(unresolved)))
    if a.metal:
        summary["metal"] = metal_gate(a, sorted({r["key"] for r in results.values() if r["ok"]}))
    (a.work / "summary.json").write_text(json.dumps(summary, indent=1) + "\n")
    print(json.dumps(summary["configs"], indent=1))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(2)
