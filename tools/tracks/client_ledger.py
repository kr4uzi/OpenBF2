#!/usr/bin/env python3
"""Import and validate the public BF2.exe client review ledger.

    python3 tools/tracks/client_ledger.py --import TABLE.tsv --reviewed COMMIT
    python3 tools/tracks/client_ledger.py [--check]

The import is an allowlist projection of a quoted TSV, not a source scan. Only
FIELDS reach the public CSV. Without --import, read the CSV and regenerate its
summary; --check validates and compares without writing. Standard library only.
"""

import argparse
import csv
import difflib
from pathlib import Path
import re
import sys

REPO = Path(__file__).resolve().parents[2]
FIELDS = ["image", "address", "size", "symbol", "file", "status", "commit", "kind", "server"]
KINDS = {
    "same": "Same as the paired dedicated-server function.",
    "layout": "Differs because of object layout.",
    "minor": "Has a small difference from the paired server function.",
    "diff": "Has a larger difference from the paired server function.",
    "stub": "The paired dedicated-server function is an empty stub; the client function has a body.",
    "ded-extra": "The dedicated-server function does more than the client function.",
    "lib": "Library or compiler code.",
    "unmatched": "No dedicated-server counterpart was found; this does not prove client-only code.",
}
STATUSES = {
    "none": "Nothing client-specific to review (same, layout or lib).",
    "todo": "Not reviewed yet.",
    "present": "The source has the client behaviour.",
    "gap": "Client behaviour is missing or different in the source.",
    "deliberate": "The source differs on purpose, with a citation in the review.",
    "cannot-tell": "The comparison could not establish a verdict.",
    "client-tu": "Taken as covered by a client-only source file, without individual comparison.",
    "skipped": "Left out after a sample of its group; not individually verified.",
}
VERDICTS = frozenset(STATUSES) - {"none", "todo"}
COMPARED = ("present", "gap", "deliberate", "cannot-tell")
ADDRESS = re.compile(r"0x[0-9A-F]{8}\Z")
COMMIT = re.compile(r"[0-9a-f]{7,40}\Z")


def read_table(path, required, delimiter=","):
    """Read a table, rejecting missing columns and malformed rows."""
    with Path(path).open(encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f, delimiter=delimiter)
        fields = reader.fieldnames or []
        if len(fields) != len(set(fields)) or not set(required) <= set(fields):
            raise ValueError(f"{path}: missing or duplicate columns (required: {', '.join(required)})")
        rows = list(reader)
    if any(None in row or any(v is None for v in row.values()) for row in rows):
        raise ValueError(f"{path}: malformed table row")
    return rows, fields


def validate(rows):
    """Validate the public format, address order and review-commit contract."""
    seen = set()
    previous = -1
    for row in rows:
        address = row["address"]
        if row["image"] != "BF2.exe" or not ADDRESS.fullmatch(address):
            raise ValueError(f"client ledger: invalid image/address {address!r}")
        value = int(address, 16)
        if value in seen:
            raise ValueError(f"client ledger: duplicate address {address}")
        if value < previous:
            raise ValueError(f"client ledger: addresses are not sorted at {address}")
        seen.add(value)
        previous = value
        if not re.fullmatch(r"[0-9]+", row["size"]):
            raise ValueError(f"client ledger: invalid size at {address}")
        if row["kind"] not in KINDS or row["status"] not in STATUSES:
            raise ValueError(f"client ledger: unknown kind or status at {address}")
        if row["status"] == "none" and row["kind"] not in ("same", "layout", "lib"):
            raise ValueError(f"client ledger: none requires same/layout/lib at {address}")
        if row["server"] and not ADDRESS.fullmatch(row["server"]):
            raise ValueError(f"client ledger: invalid server address at {address}")
        if ((row["status"] in VERDICTS) != bool(row["commit"]) or
                (row["commit"] and not COMMIT.fullmatch(row["commit"]))):
            raise ValueError(f"client ledger: invalid review commit at {address}")
        file = row["file"]
        if file and (file.startswith(("/", "absent")) or ":" in file or "\\" in file or
                     ".." in file.split("/")):
            raise ValueError(f"client ledger: invalid source path at {address}")


def load_ledger(path):
    rows, fields = read_table(path, FIELDS)
    if fields != FIELDS:
        raise ValueError(f"{path}: public client columns must be exactly {','.join(FIELDS)}")
    validate(rows)
    return rows


def import_table(path, reviewed):
    if not COMMIT.fullmatch(reviewed):
        raise ValueError("--reviewed must be a hexadecimal commit hash (7 to 40 characters)")
    rows, _ = read_table(path, ("pe", "size", "kind", "ded", "elf", "src", "status"), "\t")
    public = []
    for row in rows:
        clean = {key: "" if value == "-" else value for key, value in row.items()}
        source = clean["src"]
        file = "" if source.startswith("absent") else re.sub(r":\d+$", "", source)
        symbol = re.sub(r"^0x[0-9a-fA-F]+\s+", "", clean["elf"])
        public.append(dict(zip(FIELDS, ("BF2.exe", clean["pe"], clean["size"], symbol, file,
                                       clean["status"], reviewed if clean["status"] in VERDICTS else "",
                                       clean["kind"], clean["ded"]))))
    public.sort(key=lambda row: int(row["address"], 16))
    validate(public)
    return public


def write_ledger(path, rows):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def table(headers, rows):
    """Render a GitHub Markdown table; numeric values use thousands separators."""
    def cell(value):
        text = f"{value:,}" if isinstance(value, int) else str(value)
        return text.replace("|", "\\|").replace("\n", " ")
    return ["| " + " | ".join(map(cell, headers)) + " |",
            "| " + " | ".join("---" for _ in headers) + " |",
            *["| " + " | ".join(map(cell, row)) + " |" for row in rows], ""]


def totals(rows):
    return len(rows), sum(int(row["size"]) for row in rows)


def breakdown(rows, key, values):
    return [(value, *totals([r for r in rows if r[key] == value])) for value in values]


def matrix(rows, first, values, second, columns):
    """Rows and bytes in each cell, including zero cells and marginal totals."""
    def count(subset):
        n, size = totals(subset)
        return f"{n:,} / {size:,}"
    out = []
    for value in values:
        subset = [r for r in rows if r[first] == value]
        out.append((value, *[count([r for r in subset if r[second] == c]) for c in columns], count(subset)))
    out.append(("Total", *[count([r for r in rows if r[second] == c]) for c in columns], count(rows)))
    return table([first, *columns, "Total"], out)


def review_commits(rows):
    return ", ".join(f"`{commit}`" for commit in sorted({r["commit"] for r in rows if r["commit"]})) or "unknown"


def summary(rows):
    lines = ["# Client program ledger", "",
             "Generated by `python3 tools/tracks/client_ledger.py`; do not edit by hand. "
             "One row per BF2.exe function in the imported client review table.", "",
             "`kind` compares the retail client function with its paired bf2_w32ded.exe function. "
             "`status` records the source review, not merely whether a source body exists. "
             f"The verdicts describe the source at the `commit` value: {review_commits(rows)}. "
             "Later fixes are not reflected yet. Bytes are retail function sizes, not source-code sizes.", "",
             "## Columns", ""]
    lines += table(["Column", "Meaning"], [
        ("image", "Retail image, always BF2.exe."), ("address", "Client function entry, hexadecimal."),
        ("size", "Retail function size in bytes, decimal."),
        ("symbol", "Demangled Linux server name, when known; its address is omitted."),
        ("file", "Repository source path without a line number; empty when unknown or absent."),
        ("status", "Source review verdict, defined below."),
        ("commit", "Source revision for a verdict; empty for none and todo."),
        ("kind", "Client-versus-dedicated comparison, defined below."),
        ("server", "Paired bf2_w32ded.exe entry, hexadecimal; empty when unknown.")])
    lines += ["## Statuses", ""] + table(["Status", "Meaning"], STATUSES.items())
    lines += ["## Kinds", ""] + table(["Kind", "Meaning"], KINDS.items())
    lines += ["## Rows and bytes by status", ""]
    lines += table(["Status", "Rows", "Bytes"], [*breakdown(rows, "status", STATUSES), ("Total", *totals(rows))])
    lines += ["## Rows and bytes by kind", ""]
    lines += table(["Kind", "Rows", "Bytes"], [*breakdown(rows, "kind", KINDS), ("Total", *totals(rows))])
    lines += ["## Status by kind", "", "Each cell is rows / bytes.", ""]
    lines += matrix(rows, "status", STATUSES, "kind", KINDS)
    return "\n".join(lines)


def check_text(path, expected):
    """Return false and print a short diff if a generated file is missing or stale."""
    path = Path(path)
    actual = path.read_text(encoding="utf-8") if path.exists() else ""
    if actual == expected:
        return True
    diff = list(difflib.unified_diff(actual.splitlines(), expected.splitlines(),
                                   fromfile=path.name, tofile="regenerated", lineterm=""))
    print("\n".join(diff[:35]), file=sys.stderr)
    if len(diff) > 35:
        print("... (diff truncated)", file=sys.stderr)
    return False


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--import", dest="import_path", type=Path)
    parser.add_argument("--reviewed")
    parser.add_argument("--ledger", type=Path, default=REPO / "docs/tracks/client/ledger.csv")
    parser.add_argument("--summary", type=Path, default=REPO / "docs/tracks/client/ledger-summary.md")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    if bool(args.import_path) != bool(args.reviewed) or (args.check and args.import_path):
        parser.error("--import and --reviewed must be used together, without --check")
    try:
        rows = import_table(args.import_path, args.reviewed) if args.import_path else load_ledger(args.ledger)
        expected = summary(rows)
        if args.check:
            return 0 if check_text(args.summary, expected) else 1
        if args.import_path:
            write_ledger(args.ledger, rows)
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(expected, encoding="utf-8")
        return 0
    except (OSError, ValueError) as error:
        print(f"client_ledger.py: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
