#!/usr/bin/env python3
"""[CHOSEN, not recovered] Does an Ra group's preprocessed effect depend on the profile attempt
beyond the profile names? Compares attempt 0 with the engine's ps_2_a attempt (rank 2)."""
import json, re, subprocess, sys, collections, tempfile, os
from pathlib import Path
root = Path(__file__).resolve().parents[3]
tool = root / "out/build/nativefx/tool/nativefx"
groups = json.load(open(sys.argv[1]))
jobs = {j["id"]: j for j in json.load(open(sys.argv[2]))}
state = json.load(open(sys.argv[3]))["results"]
reqdir = Path(sys.argv[4])
prof = re.compile(r"\b(vs_[123]_[01x]|ps_[123]_[0-4abx])\b")
work = root / "out/nativefx/profile-paths"; work.mkdir(parents=True, exist_ok=True)
def pp(key, defines, name):
    out = work / name
    args = [str(tool), "preprocess", str(reqdir / f"{key}.request"), str(out)] + [f"{k}={v}" for k, v in defines.items()]
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t = prof.sub("PROFILE", out.read_text()); out.unlink(); return t
res = collections.Counter(); rows = []
for g in groups:
    if g["kind"] not in ("ra_direct", "ra_generated") or g["d3dx_first"] is None: continue
    a0 = g["attempts"][0]; key = state[a0]["key"]
    first = g["d3dx_first"]
    t0 = pp(key, jobs[a0]["compiler_defines"], "a0.fx")
    t2 = pp(key, jobs[g["attempts"][2]]["compiler_defines"], "a2.fx")
    same = t0 == t2
    res[(first, same)] += 1
    rows.append((g["effect"], first, same))
for k, v in sorted(res.items(), key=str): print(f"d3dx first attempt {k[0]}  attempt0==ps_2_a text {k[1]}: {v}")
json.dump(rows, open(work / "result.json", "w"))
