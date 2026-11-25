#!/usr/bin/env python3
"""[TEMP] Check the compiled client/server registration surface without starting either image."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CLASSES = ("RankManager", "ClanManager", "NewsManager", "AutoPatch", "Persistence")


def preprocess(entry):
    args = shlex.split(entry["command"]) if "command" in entry else entry["arguments"]
    clean = []
    i = 0
    while i < len(args):
        if args[i] in ("-o", "-MF", "-MT", "-MQ"):
            i += 2
            continue
        if args[i] not in ("-c", "-MD", "-MMD"):
            clean.append(args[i])
        i += 1
    return subprocess.check_output(clean + ["-E"], cwd=entry["directory"], text=True)


def body(text, method):
    start = text.index("void ModuleGame::" + method)
    return text[start:text.index("\n}", start) + 2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--server", type=Path)
    args = parser.parse_args()
    db = json.loads((args.build_dir / "compile_commands.json").read_text())
    entries = [e for e in db if e["file"].endswith("/Core/ModuleRegistry.cpp")]
    texts = {}
    for entry in entries:
        command = entry.get("command", " ".join(entry.get("arguments", [])))
        match = re.search(r"-DDEDICATED=([01])", command)
        if match:
            texts[int(match[1])] = preprocess(entry)
    assert set(texts) == {0, 1}, "both production registration configurations required"
    client = body(texts[0], "getClassList")
    server = body(texts[1], "getClassList")
    rows = re.findall(r'ClassRegInfo\{CID_(\w+),\s*"([^"]+)",\s*&(\w+)::create\}', client)
    names = [row[0] for row in rows]
    start = names.index("ProfileManager") + 1
    assert names[start:start + 6] == list(CLASSES) + ["ServerBrowser"], "retail class order"
    assert client.index("if (!isDedicated())") < client.index("CID_RankManager"), "runtime dedicated guard"
    singletons = body(texts[0], "getSingletonList")
    for index, cls in enumerate(CLASSES):
        assert (cls, "dice.hfe." + cls, cls) in rows, cls + " factory identity"
        assert "CID_" + cls not in server, cls + " absent from dedicated registration"
        flag = 2 if cls == "RankManager" else 1
        pattern = (r'SingletonRegInfo\{"' + cls + r'",\s*&g_\w+,\s*IID_I' + cls
                   + r',\s*CID_' + cls + r',\s*' + str(flag) + r'\}')
        assert re.search(pattern, singletons), cls + " singleton interface and timing"
        assert re.search(r'CID_' + cls + r'\s*=\s*' + str(121021 + index * 2) + r';', texts[0]), cls + " CID"
        assert re.search(r'IID_I' + cls + r'\s*=\s*' + str(121020 + index * 2) + r';', texts[0]), cls + " IID"
        sources = [e for e in db if e["file"].endswith("/" + cls + ".cpp")]
        assert sources, cls + " in source manifest"
        assert not any("-DDEDICATED=1" in e.get("command", " ".join(e.get("arguments", [])))
                       for e in sources), cls + " excluded from dedicated sources"
    if args.server:
        symbols = subprocess.check_output(["nm", "-C", str(args.server.resolve())], text=True)
        for cls in CLASSES:
            assert "dice::hfe::" + cls + "::" not in symbols, cls + " absent from dedicated binary"
            assert "vtable for dice::hfe::" + cls + "\n" not in symbols, cls + " table absent"
    print("PASS online service registrations: client order, factories, ids, singleton timing, dedicated exclusion")


if __name__ == "__main__":
    main()
