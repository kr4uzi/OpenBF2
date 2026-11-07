#!/usr/bin/env python3
"""[CHOSEN, not recovered] Parallel driver for `nativefx measure` and its summary tables."""
import argparse, collections, csv, os, subprocess, sys
from pathlib import Path

def run(args):
    rows = [l for l in Path(args.list).read_text().splitlines() if l.strip()]
    out = Path(args.output); out.mkdir(parents=True, exist_ok=True)
    n = args.jobs
    procs = []
    for i in range(n):
        chunk = out / f"list.{i}.tsv"
        chunk.write_text("".join(r + "\n" for r in rows[i::n]))
        cmd = ([args.tool, "measure", str(chunk), args.requests, str(out / f"measure.{i}.tsv")] if not args.entries else
               [args.tool, "structure", str(chunk), args.requests, args.entries, str(out / f"part.{i}")])
        procs.append(subprocess.Popen(cmd,
                                      stdout=subprocess.DEVNULL, stderr=open(out / f"measure.{i}.log", "w")))
    codes = [p.wait() for p in procs]
    if args.entries:
        header, body = None, []
        for i in range(n):
            part = out / f"part.{i}"
            lines = (part / "structure.tsv").read_text().splitlines()
            header = lines[0]; body += lines[1:]
            for f in part.iterdir():
                if f.name != "structure.tsv": f.rename(out / f.name)
            (part / "structure.tsv").unlink(); part.rmdir(); (out / f"list.{i}.tsv").unlink()
        (out / "structure.tsv").write_text(header + "\n" + "\n".join(body) + "\n")
        return
    header, body = None, []
    for i in range(n):
        lines = (out / f"measure.{i}.tsv").read_text().splitlines()
        header = lines[0]; body += lines[1:]
    (out / "measure.tsv").write_text(header + "\n" + "\n".join(body) + "\n")
    for i in range(n):
        for f in (f"list.{i}.tsv", f"measure.{i}.tsv"): (out / f).unlink()
    if any(codes): print("exit codes", codes, file=sys.stderr)

def summarize(path, accepted_only):
    rows = list(csv.DictReader(open(path), delimiter="\t"))
    if accepted_only: rows = [r for r in rows if r["d3dx_ok"] == "1"]
    reqs = collections.OrderedDict()
    for r in rows: reqs.setdefault(r["id"], []).append(r)
    req_fail = collections.Counter(v[0]["status"] for v in reqs.values() if v[0]["stage"] == "-")
    stage = collections.defaultdict(collections.Counter)
    for r in rows:
        if r["stage"] == "-": continue
        cls = r["status"]
        if cls == "vkd3d_error": cls = "vkd3d " + r["code"] + " " + classify(r["message"])
        elif cls == "mojo_error": cls = "mojo " + r["message"][:60]
        stage[r["stage"]][cls] += 1
    full = sum(1 for v in reqs.values() if v[0]["stage"] != "-" and all(x["status"] in ("compiled", "promoted", "assembled") for x in v))
    print(f"requests {len(reqs)}; all shaders compiled+translated: {full}; request-level failures: {dict(req_fail)}")
    for s in ("vs", "ps"):
        tot = sum(stage[s].values())
        print(f"\n{s}: {tot} shader jobs")
        for k, v in stage[s].most_common(): print(f"  {v:6d}  {k}")

def classify(m):
    if "exceeds limits" in m: return "register limit " + m.split("for shader type")[1].strip() if "for shader type" in m else m
    if m.startswith("Instruction"): return m.split("(")[0].strip()
    if m.startswith("Invalid source parameter count"): return "texld src count"
    return m[:70]

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--list"); ap.add_argument("--requests"); ap.add_argument("--output")
    ap.add_argument("--tool", default="out/build/nativefx/tool/nativefx"); ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--summary"); ap.add_argument("--accepted", action="store_true")
    ap.add_argument("--entries", help="reference cache entries: run the structure comparison")
    a = ap.parse_args()
    if a.summary: summarize(a.summary, a.accepted)
    else: run(a)
