#!/usr/bin/env python3
"""[CHOSEN, not recovered] Classify measured Metal diagnostics without hiding failures."""
import csv
import json
from pathlib import Path
import re
import sys
from collections import Counter


def classify(message):
    if re.search(r"no member named 't\d+'", message):
        return "mojoshader_sm1_missing_texcoord_member"
    if "expected parameter declarator" in message and "undeclared identifier 'input'" in message:
        return "mojoshader_sm1_missing_stage_input_and_trailing_comma"
    if "undeclared identifier 'input'" in message:
        return "mojoshader_sm1_missing_stage_input"
    if re.search(r"reserved (?:keyword|name)|keyword.*identifier", message, re.I):
        return "reserved_identifier_needs_token_rename"
    if re.search(r"unsupported|not supported|requires.*(?:version|language)", message, re.I):
        return "unsupported_feature_needs_semantic_review"
    return "unclassified_needs_review"


def main():
    path = Path(sys.argv[1])
    with path.open(newline="") as file:
        # Metal appends runs. Latest measurement per shader is the current result.
        measured = {(r["job"], r["metal_file"]): r for r in csv.DictReader(file) if r["stage"] == "metal"}
    statuses = Counter(r["metal"] for r in measured.values())
    failures = [dict(job=r["job"], metal_file=r["metal_file"], cause=classify(r["error_summary"]),
                     diagnostic=r["error_summary"]) for r in measured.values() if r["metal"] == "fail"]
    report = dict(libraries=len(measured), statuses=dict(statuses),
                  failure_classes=dict(Counter(r["cause"] for r in failures)), failures=failures)
    path.with_name("metal-summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "failures"}, indent=2))


if __name__ == "__main__":
    main()
