#!/usr/bin/env python3
"""[TEMP] Scaffolding: compile checks from the configured image manifests.

Run after configuring macos-x64 and a separate -DCMAKE_OSX_ARCHITECTURES=arm64
directory. Logs and JSON summaries go in the supplied build directory.
"""
import argparse
import concurrent.futures
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--all-client', action='store_true')
    parser.add_argument('--summarize', action='store_true', help='only summarize existing per-TU logs')
    args = parser.parse_args()
    build = args.build.resolve()
    database = json.loads((build / 'compile_commands.json').read_text())
    changed = subprocess.check_output(['git', 'diff', '--name-only'], cwd=ROOT, text=True).splitlines()
    touched = {str(ROOT / f) for f in changed if f.endswith('.cpp')}
    selected = {}
    for row in database:
        command = row['command']
        if '-DDEDICATED=0' not in command:
            continue
        if row['file'] in touched or (args.all_client and '/Code/BF2/' in row['file']):
            # Keep distinct plugin and host compilations of shared sources.
            selected[(row['file'], row['output'].split('/')[1])] = row
    logs = build / 'portable_x87_syntax'
    logs.mkdir(exist_ok=True)

    def run(item):
        (file, image), row = item
        flags = shlex.split(row['command'])
        del flags[flags.index('-o'):flags.index('-o')+2]
        flags.remove('-c')
        flags += ['-fsyntax-only', '-ferror-limit=0']
        log = logs / (str(Path(file).relative_to(ROOT)).replace('/', '_') + '.' + image + '.log')
        if args.summarize:
            output = log.read_text()
        else:
            result = subprocess.run(flags, cwd=row['directory'], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = result.stdout
            log.write_text(output)
        errors = [line for line in output.splitlines() if re.search(r'(fatal )?error:', line)]
        status = int(bool(errors)) if args.summarize else result.returncode
        return dict(file=str(Path(file).relative_to(ROOT)), image=image, status=status,
                    errors=errors, asm_errors=[s for s in errors if re.search(
                        r'error:.*(?:\b__asm\b|inline assembly|MS-style inline|asm blocks)', s)])

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(run, selected.items()))
    summary = 'summary-all-client.json' if args.all_client else 'summary-touched.json'
    (logs / summary).write_text(json.dumps(results, indent=2) + '\n')
    print(f'{len(results)} image/TU checks; {sum(r["status"] == 0 for r in results)} passed; '
          f'{sum(bool(r["asm_errors"]) for r in results)} TUs with asm errors')
    for result in results:
        if result['status'] and str(ROOT / result['file']) in touched:
            print(result['file'], result['image'], '\n  ' + '\n  '.join(result['errors'][:3]))
    # Existing independent blockers are recorded, not hidden as a passing compile.
    return int(any(r['asm_errors'] for r in results))


if __name__ == '__main__':
    raise SystemExit(main())
