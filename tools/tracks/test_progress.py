#!/usr/bin/env python3
"""Offline contracts for the client ledger and progress page (standard library only).

    python3 tools/tracks/test_progress.py
"""

import contextlib
import csv
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import client_ledger as client
import progress


def row(address="0x00401000", status="present", kind="diff", **changes):
    result = dict(zip(client.FIELDS, ("BF2.exe", address, "10", "f()", "Code/BF2/Game/F.cpp",
                                      status, "e976b82b" if status in client.VERDICTS else "", kind, "")))
    return {**result, **changes}


def write_csv(path, fields, rows, delimiter=","):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fields, delimiter=delimiter, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def markdown_rows(page, heading):
    section = page.split("## " + heading + "\n", 1)[1].split("\n## ", 1)[0]
    first_table = section[section.index("| "):].split("\n\n", 1)[0]
    return [[c.strip() for c in line.strip("|").split("|")]
            for line in first_table.splitlines()]


class ClientLedger(unittest.TestCase):
    def test_import_projects_only_public_columns(self):
        fields = "pe size kind ded match delta cluster elf src basis srchash tier status cite flag finding run note".split()
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "review.tsv"
            cases = []
            for i, status in enumerate(client.STATUSES):
                case = {key: f"private {key}\tquoted\ntext" for key in fields}
                case.update(pe=f"0x0040{1000 + i:04X}", size="12", kind="same" if status == "none" else "diff",
                            ded="0x00402530" if i == 0 else "-", elf="0x08012345 dice::f(int, char const*)" if i == 0 else "-",
                            src="Code/BF2/Game/F.cpp:502" if i == 0 else ("absent: no source" if i == 1 else "-"),
                            status=status)
                cases.append(case)
            write_csv(path, fields, reversed(cases), "\t")
            public = client.import_table(path, "e976b82b")
            self.assertEqual(len(public), len(cases))
            self.assertEqual(list(public[0]), client.FIELDS)
            self.assertEqual(public[0]["symbol"], "dice::f(int, char const*)")
            self.assertEqual(public[0]["file"], "Code/BF2/Game/F.cpp")
            self.assertEqual(public[0]["server"], "0x00402530")
            for imported in public:
                self.assertEqual(imported["commit"], "e976b82b" if imported["status"] in client.VERDICTS else "")
                self.assertNotIn("private", " ".join(imported.values()))
            self.assertEqual(public[1]["file"], "")
            self.assertEqual(public[2]["file"], "")
            self.assertEqual(public[1]["symbol"], "")
            self.assertEqual(public[1]["server"], "")
            output = Path(tmp) / "ledger.csv"
            client.write_ledger(output, public)
            self.assertNotIn(b"\r", output.read_bytes())
            self.assertEqual(client.load_ledger(output), public)

    def test_invalid_client_rows(self):
        cases = [([row(), row()], "duplicate"),
                 ([row(status="unknown")], "unknown"),
                 ([row(status="none", kind="diff")], "none requires"),
                 ([row("0x00402000"), row()], "not sorted"),
                 ([row(commit="")], "commit"),
                 ([row(status="todo", commit="e976b82b")], "commit"),
                 ([row(server="not-an-address")], "server"),
                 ([row(size="-1")], "size"),
                 ([row(kind="unknown")], "unknown"),
                 ([row(file="/private/source.cpp")], "path")]
        for rows, message in cases:
            with self.subTest(message=message, rows=rows), self.assertRaisesRegex(ValueError, message):
                client.validate(rows)

    def test_summary_check_is_read_only_and_detects_changes(self):
        with tempfile.TemporaryDirectory() as tmp:
            ledger, summary = Path(tmp) / "ledger.csv", Path(tmp) / "summary.md"
            client.write_ledger(ledger, [row()])
            args = ["--ledger", str(ledger), "--summary", str(summary)]
            self.assertEqual(client.main(args), 0)
            self.assertEqual(client.main([*args, "--check"]), 0)
            before = summary.read_bytes()
            client.write_ledger(ledger, [row(status="gap")])
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(client.main([*args, "--check"]), 1)
            self.assertEqual(summary.read_bytes(), before)
            self.assertEqual(client.main(args), 0)
            self.assertEqual(client.main([*args, "--check"]), 0)


class Progress(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.addCleanup(self.tmp.cleanup)
        self.data = []
        for index, (_, directory, image, _) in enumerate(progress.PARTS):
            if directory == "tracks/client":
                rows = [row(f"0x0040{1000 + i:04X}", status=status, kind="lib" if status == "none" else "diff")
                        for i, status in enumerate(client.STATUSES)]
                fields = client.FIELDS
            else:
                statuses = [("done", "skip-lib", "skip-debug"),
                            ("done", "chosen", "stop", "skip", "todo", "todo"),
                            ("done", "stop", "chosen", "unattributed"), ("done", "todo")][index]
                rows = [{k: v for k, v in row(f"0x1000{1000 + i:04X}", status=status).items()
                         if k in progress.CONTRACT} for i, status in enumerate(statuses)]
                for r in rows:
                    r["image"] = image
                fields = list(progress.CONTRACT)
                if index == 0:
                    fields.remove("image")
                    rows = [{k: v for k, v in r.items() if k != "image"} for r in rows]
                if index == 1:
                    rows.append({**rows[0], "image": "BF2.exe"})  # same address in a different image is valid
            self.data.append(rows)
            write_csv(self.root / "docs" / directory / "ledger.csv", fields, rows)
        write_csv(self.root / "docs/tracks/rend/attribution.csv", ["address", "class"],
                  [{"address": self.data[1][4]["address"], "class": "compiler"}])
        write_csv(self.root / "docs/tracks/markers.csv", ["kind", "file", "line", "function", "note"],
                  [{"kind": "abort", "file": "Code/BF2/Game/F.cpp", "line": "1", "function": "f", "note": ""}])

    def cli(self, *args):
        return subprocess.run([sys.executable, str(Path(progress.__file__)), "--root", str(self.root), *args],
                              capture_output=True, text=True)

    def test_totals_equal_rows_and_bytes(self):
        page = progress.build_page(self.root)
        table = markdown_rows(page, "Ledger summary")
        for i, cells in enumerate(table[2:]):
            name, directory, image, _ = progress.PARTS[i]
            included = [r for r in self.data[i] if r.get("image", image) == image]
            self.assertEqual(cells[:3], [name, image, str(len(included))])
            if i < 4:
                counts = [int(cells[j]) for j in range(3, 8)]
                self.assertEqual(sum(counts), len(included))
        attribution = markdown_rows(page, "Renderer open rows by attribution")
        self.assertIn(["compiler", "1", "10"], attribution)
        self.assertIn(["No attribution class", "1", "10"], attribution)
        self.assertEqual(attribution[-1], ["Total", "2", "20"])
        groups = markdown_rows(page, "Client program")[:8]
        self.assertEqual(groups[-1], ["Total", "8", "80"])
        self.assertEqual(sum(int(r[1]) for r in groups[2:-1]), 8)
        self.assertEqual(sum(int(r[2]) for r in groups[2:-1]), 80)
        self.assertIn("| Total | 1 | Source marker findings |", page)

    def test_check_fails_after_change_then_passes_after_regeneration(self):
        self.assertEqual(self.cli().returncode, 0)
        self.assertEqual(self.cli("--check").returncode, 0)
        path = self.root / "docs/tracks/srv/ledger.csv"
        rows, fields = client.read_table(path, progress.CONTRACT)
        rows[1]["status"] = "done"
        write_csv(path, fields, rows)
        page_before = (self.root / "docs/progress.md").read_bytes()
        result = self.cli("--check")
        self.assertEqual(result.returncode, 1)
        self.assertIn("regenerated", result.stderr)
        self.assertEqual((self.root / "docs/progress.md").read_bytes(), page_before)
        self.assertEqual(self.cli().returncode, 0)
        self.assertEqual(self.cli("--check").returncode, 0)

    def test_summary_totals_are_never_used(self):
        path = self.root / "docs/tracks/rend/ledger-summary.md"
        path.write_text("Generated by `tool.py` at abcdef12 from inputs.\n999 done / 777 todo\n")
        expected = progress.build_page(self.root)
        path.write_text("Generated by `tool.py` at abcdef12 from inputs.\n0 done / 0 todo\n")
        (self.root / "docs/tracks/swf/ledger-summary.md").write_text("1916 done / 67 todo\n")
        self.assertEqual(progress.build_page(self.root), expected)
        self.assertIn("`abcdef12`", expected)
        self.assertIn("| Menu player | unknown |", expected)

    def test_validates_every_ledger_including_excluded_renderer_rows(self):
        for _, directory, _, _ in progress.PARTS:
            path = self.root / "docs" / directory / "ledger.csv"
            original = path.read_bytes()
            rows, fields = client.read_table(path, ("status",))
            for mutation in ("duplicate", "status", "columns"):
                with self.subTest(directory=directory, mutation=mutation):
                    changed = [dict(r) for r in rows]
                    if mutation == "duplicate":
                        changed.append(dict(changed[-1]))
                    elif mutation == "status":
                        changed[-1]["status"] = "invalid"
                    selected = [f for f in fields if f != "size"] if mutation == "columns" else fields
                    write_csv(path, selected, [{k: r[k] for k in selected} for r in changed])
                    with self.assertRaises(ValueError):
                        progress.build_page(self.root)
                    path.write_bytes(original)
            path.unlink()
            with self.assertRaises(OSError):
                progress.build_page(self.root)
            path.write_bytes(original)

    def test_marker_refresh_uses_declared_scope(self):
        def scan(command, **_):
            # The scan writes tab-separated rows; an empty note leaves a trailing tab.
            Path(command[command.index("--tsv") + 1]).write_text(
                "kind\tfile\tline\tfunction\tnote\n"
                "partial\tCode/BF2/Menu/G.cpp\t7\tg\t\n", encoding="utf-8")

        with mock.patch.object(progress.subprocess, "run", side_effect=scan) as run:
            self.assertEqual(progress.main(["--root", str(self.root), "--refresh-markers"]), 0)
        command = run.call_args.args[0]
        self.assertEqual(command[2:2 + len(progress.MARKER_PATHS)], list(progress.MARKER_PATHS))
        saved = (self.root / "docs/tracks/markers.csv").read_text(encoding="utf-8")
        self.assertEqual(saved, "kind,file,line,function,note\npartial,Code/BF2/Menu/G.cpp,7,g,\n")
        self.assertFalse(any(line != line.rstrip() for line in saved.splitlines()))
        self.assertIn("| partial | 1 |", (self.root / "docs/progress.md").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
