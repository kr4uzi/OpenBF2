#!/usr/bin/env python3
"""[TEMP] [CHOSEN, not recovered] Read-only decoder check of installed content.

Usage: content.py <CPU harness executable> <mods/bf2 directory>
Streams image bytes directly to the harness; no content is extracted or modified.
"""
import collections
import pathlib
import re
import struct
import subprocess
import sys
import tempfile
import zipfile

exe, root = pathlib.Path(sys.argv[1]).resolve(), pathlib.Path(sys.argv[2]).resolve()
extensions = {'.dds', '.tga', '.png', '.bmp', '.jpg', '.jpeg'}
names = []
counts = collections.Counter()
# This stock level entry has a .png suffix but contains no image signature.
# Keep the negative result visible; never accept it as a decoded texture.
expected_rejections = {'Levels/Midnight_Sun/Info/aurora.png'}
with tempfile.TemporaryFile() as output:
    process = subprocess.Popen([str(exe), '--corpus'], stdin=subprocess.PIPE, stdout=output)

    def send(name, data):
        names.append(name)
        counts[pathlib.Path(name).suffix.lower()] += 1
        process.stdin.write(struct.pack('<I', len(data)))
        process.stdin.write(data)

    for path in sorted(root.rglob('*')):
        if path.suffix.lower() == '.zip':
            with zipfile.ZipFile(path) as archive:
                for entry in archive.infolist():
                    if pathlib.Path(entry.filename).suffix.lower() in extensions:
                        send(str(path.relative_to(root)) + ':' + entry.filename, archive.read(entry))
        elif path.is_file() and path.suffix.lower() in extensions:
            send(str(path.relative_to(root)), path.read_bytes())
    process.stdin.close()
    result = process.wait()
    output.seek(0)
    report = output.read().decode()
    summary = re.search(r'^(?:PASS|FAIL): (\d+)/(\d+) corpus images parsed$', report, re.M)
    if not summary or int(summary[2]) != len(names):
        sys.exit('Decoder did not report processing every submitted file: ' + report)
    unexpected = []
    rejected = 0
    for line in report.splitlines():
        match = re.match(r'FAIL image (\d+) size (\d+)', line)
        if match:
            name = names[int(match[1])]
            rejected += 1
            if name in expected_rejections:
                print('Expected non-image rejection:', name)
            else:
                unexpected.append(name)
                print(line, name)
    print(f'{"FAIL" if unexpected else "PASS"}: {len(names)-rejected} images parsed, '
          f'{rejected} rejected, {len(unexpected)} unexpected rejections')
    print(dict(counts))
    sys.exit(1 if unexpected or result not in (0,1)
             or int(summary[1]) != len(names)-rejected else 0)
