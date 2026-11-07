"""Scaffolding for ledger source discovery and evidence attribution regressions."""

import importlib.util
import contextlib
import csv
import io
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "build_ledger", ROOT / "tools/ai-reconstruct/build_ledger.py")
ledger = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ledger)


class SourceDiscovery(unittest.TestCase):
    def test_module_table_initializer_is_not_assigned_to_preceding_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            module = "Code/BF2/AI/AIDLL/AI/Main/AIModule.cpp"
            console = "Code/BF2/IO/File/FileManagerConsole.cpp"
            (root / "CMakeLists.txt").write_text(
                'set(bf2_aidll_sources\n "${BF2_ROOT}/AI/AIDLL/AI/Main/AIModule.cpp"\n)\n')
            for rel, source in ((module, "IModule* dllmodules[] = { new ai::AIModule, 0 };\n"),
                                (console, "void precedingConsoleFunction() {}\n")):
                p = root / rel
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text(source)
            symbols = [
                dict(addr=0x100000, size=5, mangled="precedingConsoleFunction",
                     demangled="precedingConsoleFunction()", bind="g", tu=None),
                dict(addr=0x100010, size=86, mangled="_Z41__static_initialization_and_destruction_0ii",
                     demangled="__static_initialization_and_destruction_0(int, int)",
                     bind="l", tu="DllModules.cpp"),
                dict(addr=0x100070, size=31, mangled="_GLOBAL__I__ZN4dice3hfe10dllmodulesE",
                     demangled="_GLOBAL__I__ZN4dice3hfe10dllmodulesE", bind="l", tu="DllModules.cpp")]
            output = root / "ledger.csv"
            with (patch.object(ledger, "REPO", tmp),
                  patch.object(ledger, "LEDGER", str(output)),
                  patch.object(ledger, "SUMMARY", str(root / "summary.md")),
                  patch.object(ledger, "CONSOLE_REPORT", str(root / "absent.json")),
                  patch.object(ledger, "load_symbols", return_value=symbols),
                  patch.object(ledger.subprocess, "run", return_value=SimpleNamespace(stdout="test\n")),
                  patch("sys.argv", ["build_ledger.py", "--ledger", str(output),
                                     "--summary", str(root / "summary.md")]),
                  contextlib.redirect_stdout(io.StringIO())):
                ledger.main()
            with output.open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(rows[0]["file"], console)
            self.assertEqual([r["file"] for r in rows[1:]], [module, module])
            # Attribution is not evidence that the initializer's body is complete.
            self.assertEqual([r["status"] for r in rows[1:]], ["todo", "todo"])

    def test_shared_sources_and_unpaired_inline_headers(self):
        # These shared bodies must be visible even without an aidll manifest entry.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "CMakeLists.txt").write_text("set(bf2_aidll_sources\n)\n")
            paths = (
                "Math/Polynom.h", "Core/SpHandlePool.h", "IO/Input/Input.inl",
                "Math/LocalBoundingBox.cpp", "Core/MemoryPool.cpp", "IO/Stream/MemStream.cpp",
                "AI/Header.h", "AI/Excluded.cpp")
            for rel in paths:
                p = root / "Code/BF2" / rel
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text("class Example { public: int body() { return 3; } };\n")
            with patch.object(ledger, "REPO", tmp):
                cpps, headers = ledger.manifest_files()
                self.assertEqual(set(cpps), {
                    "Code/BF2/Math/LocalBoundingBox.cpp", "Code/BF2/Core/MemoryPool.cpp",
                    "Code/BF2/IO/Stream/MemStream.cpp"})
                self.assertEqual(set(headers), {"Code/BF2/" + rel for rel in paths
                                               if rel.endswith((".h", ".inl"))})
                for rel in headers:
                    definitions, _, _ = ledger.parse_file(rel)
                    self.assertEqual([d["comps"] for d in definitions], [["Example", "body"]])

    def test_address_image_exclusions(self):
        comment = ("ELF 0x1CA310; bf2 0x83AEFE0; PE 0x10012345; "
                   "vtable 0x4BB300; GOT 0x4B30C4; +0x12345")
        self.assertEqual(ledger.comment_addresses(comment), [(0x1CA310, True)])

    def test_danger_object_tu_key(self):
        key = ("_GLOBAL__I__ZNK4dice3hfe2ai12DangerObjecteqERKS2_"
               "AI_AIDLL_AI_AIKnowledge_DangerObject.cpp07kBPb")
        self.assertEqual(ledger.embedded_tu_file(key),
                         "Code/BF2/AI/AIDLL/AI/AIKnowledge/DangerObject.cpp")
        self.assertIsNone(ledger.embedded_tu_file("_GLOBAL__I__ZN4dice3hfe8initMathEv"))

    def test_original_file_belongs_only_to_local_symbols(self):
        table = ("00000000 l    df *ABS*\t00000000 Frustum.cpp\n"
                 "00495d80 l     F .text\t0000001f _GLOBAL__I_example\n"
                 "00000000 l    df *ABS*\t00000000 Vec3.cpp\n"
                 "0049c960 l     F .text\t0000001f _GLOBAL__I_other\n"
                 "00493fd0 g     F .text\t00000150 external\n")
        demangled = "_GLOBAL__I_example\n_GLOBAL__I_other\nexternal\n"
        with patch.object(ledger.subprocess, "run", side_effect=[
                SimpleNamespace(stdout=table), SimpleNamespace(stdout=demangled)]):
            symbols = ledger.load_symbols("unused")
        self.assertEqual([s["tu"] for s in symbols], ["Frustum.cpp", "Vec3.cpp", None])

    def test_local_helper_cannot_complete_external_body(self):
        definition = {"namespaces": ["dice", "hfe", "{anon}"], "comps": ["findIntersection"]}
        external = ledger.parse_demangled("dice::hfe::findIntersection(Plane3 const&, Ray3 const&, Vec3&)")
        local = ledger.parse_demangled("dice::hfe::(anonymous namespace)::findIntersection(Plane3 const&, Ray3 const&, Vec3&)")
        self.assertFalse(ledger.same_linkage_scope(definition, external))
        self.assertTrue(ledger.same_linkage_scope(definition, local))


if __name__ == "__main__":
    unittest.main()
