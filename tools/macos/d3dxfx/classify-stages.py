#!/usr/bin/env python3
"""[CHOSEN, not recovered] Account for every unsuccessful pipeline stage/job."""
import csv
from collections import Counter
import json
from pathlib import Path
import re
import sys


def compile_class(text):
    if "undeclared identifier 'NUM_TEXSETS'" in text:
        return "input_generation_inactive_permutation_fragment"
    for needle, category in (
        ("tex2Dproj is not supported on ps_1_3", "profile_refusal_projected_sample"),
        ("error X4520", "profile_refusal_texcoord_read_and_sample"),
        ("error X4507", "profile_refusal_constant_limit"),
        ("error X4523", "profile_refusal_dependent_sample"),
        ("error X4516", "profile_refusal_shared_texcoord"),
        ("cannot map expression to pixel shader instruction set", "profile_refusal_instruction_set"),
    ):
        if needle in text:
            return category
    return "unclassified_needs_review"


def main():
    root = Path(sys.argv[1]).resolve()
    rows = list(csv.DictReader((root / "results.csv").open(newline="")))
    failures = {stage: [] for stage in ("compile", "parse", "translate", "metal")}
    runtime_frontiers = []
    for row in rows:
        if row["stage"] != "effect":
            continue
        job = row["job"]
        result_path = root / "msl" / job / "result.json"
        graph = json.loads(result_path.read_text()) if result_path.exists() else {}
        if graph.get("state_expression_count"):
            runtime_frontiers.append(dict(job=job, frontier="fx-state-expression-evaluator",
                                          occurrences=graph["state_expression_count"]))
        if row["compile"] != "ok":
            log = root / "logs" / (job + ".compile.log")
            message = log.read_text(errors="replace").replace("\0", "") if log.exists() else row["error_summary"]
            failures["compile"].append(dict(job=job, status=row["compile"], category=compile_class(message),
                diagnostic=message, errors=re.findall(r"[^\r\n]*error X\d+[^\r\n]*", message)))
        elif row["parse"] != "ok":
            failures["parse"].append(dict(job=job, category="fx_shader_reference_or_partial_cleanup"
                if "Unsupported shader type" in row["error_summary"] else "unclassified_needs_review",
                diagnostic=row["error_summary"]))
        elif row["translate"] == "fail":
            result = json.loads((root / "msl" / job / "result.json").read_text())
            for shader in result["shaders"]:
                if shader["translate_ok"]:
                    continue
                prefix = "object_" + str(shader["object"]) + " "
                errors = [s for s in result["errors"] if s.startswith(prefix)]
                categories = set()
                for message in errors:
                    if "TEXLD == Shader Model 1.4" in message:
                        categories.add("sm14_texld")
                    elif "TEXCRD unimplemented" in message:
                        categories.add("sm1_texcrd")
                    else:
                        categories.add("unclassified_needs_review")
                failures["translate"].append(dict(job=job, object=shader["object"],
                    category="+".join(sorted(categories)), diagnostic=errors))
    metal = {(r["job"], r["metal_file"]): r for r in rows if r["stage"] == "metal"}
    for (job, name), row in metal.items():
        if row["metal"] != "ok":
            failures["metal"].append(dict(job=job, metal_file=name, status=row["metal"], diagnostic=row["error_summary"]))
    report = dict(counts={stage: len(items) for stage, items in failures.items()},
        classes={stage: dict(Counter(item.get("category", item.get("status")) for item in items))
                 for stage, items in failures.items()}, failures=failures,
        runtime_frontiers=runtime_frontiers,
        note="Profile refusals remain attempted jobs: EffectManager::getEffect retries after compile failure. "
             "No runtime technique selection or device validation is inferred.")
    (root / "stage-failures.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in ("counts", "classes")}, indent=2))


if __name__ == "__main__":
    main()
