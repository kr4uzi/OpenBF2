#!/usr/bin/env python3
"""[CHOSEN, not recovered] Profile-attempt acceptance: native compile vs D3DX, per request and
per engine group (the engine uses the first attempt whose effect compiles)."""
import collections, csv, json, sys

def accepted(path):
    reqs = collections.defaultdict(list)
    for r in csv.DictReader(open(path), delimiter="\t"):
        reqs[r["id"]].append(r)
    ok = {}
    for rid, rows in reqs.items():
        if rows[0]["stage"] == "-": ok[rid] = rows[0]["status"] in ("no_techniques",) and False
        else: ok[rid] = all(x["status"] in ("compiled", "promoted", "assembled") for x in rows)
    d3dx = {rid: rows[0]["d3dx_ok"] == "1" for rid, rows in reqs.items()}
    return ok, d3dx

def main(measure, groups_path, label):
    ok, d3dx = accepted(measure)
    m = collections.Counter((d3dx[r], ok[r]) for r in ok)
    print(f"{label}: requests {len(ok)}")
    print(f"  D3DX accept & native accept {m[(True, True)]}, D3DX accept & native refuse {m[(True, False)]}, "
          f"D3DX refuse & native accept {m[(False, True)]}, both refuse {m[(False, False)]}")
    groups = json.load(open(groups_path))
    g = collections.Counter()
    for grp in groups:
        att = grp["attempts"]
        nat = next((i for i, a in enumerate(att) if a in ok and ok[a]), None)
        # Native attempts beyond the D3DX-formed prefix were never formed as requests:
        # a refusal within the prefix is the only evidence available.
        ref = grp["d3dx_first"]
        formed = [a for a in att if a in ok]
        if nat is None and ref is not None and len(formed) <= ref: nat = "unformed"
        g[(grp["kind"], ref == nat if nat != "unformed" else "unknown")] += 1
        if ref != nat and nat != "unformed": g[("first-attempt pairs", f"d3dx {ref} native {nat}")] += 1
    for k, v in sorted(g.items(), key=str): print(f"  {k}: {v}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else sys.argv[1])

def rule_r(measure, groups_path):
    """Rule R [CHOSEN]: Ra attempts below ps_2_0 (ranks 0, 1, 5, 6) are refused natively; every
    other request is accepted when all of its shaders compile with the profile ladder."""
    ok, d3dx = accepted(measure)
    def native(rid):
        kind = rid.split("-")[1]; rank = int(rid.rsplit("-", 1)[1])
        if kind in ("ra_direct", "ra_generated") and rank % 5 in (0, 1): return False
        return ok[rid]
    groups = json.load(open(groups_path))
    g = collections.Counter()
    for grp in groups:
        att = grp["attempts"]
        nat = next((i for i, a in enumerate(att) if a in ok and native(a)), None)
        ref = grp["d3dx_first"]
        if nat is None and ref is not None: nat = "beyond formed requests"
        g[(grp["kind"], "same" if ref == nat else f"d3dx {ref} native {nat}")] += 1
    print("Rule R (refuse Ra ps_1_x attempts, ladder elsewhere):")
    for k, v in sorted(g.items(), key=str): print(f"  {k}: {v}")

if __name__ == "__main__" and len(sys.argv) > 4: rule_r(sys.argv[1], sys.argv[2])
