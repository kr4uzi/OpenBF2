# Reconstruction progress

This page is generated from the repository's five function ledgers by `python3 tools/tracks/progress.py`. The first four count retail functions with source bodies; the client ledger records client-behaviour review verdicts. These are inventories, not proof of equivalent behaviour, working gameplay or a percentage of the whole engine.

## Ledger summary

| Part | Retail image | Functions | Functions with a source body | Deliberate replacements | Stops | Library or compiler code | Open functions | Open share of retail bytes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Bot AI | libaidll.so | 12,170 | 10,961 | 0 | 0 | 1,209 | 0 | 0.0% |
| Renderer | RendDX9.dll | 9,762 | 5,678 | 88 | 3 | 76 | 3,917 | 25.8% |
| Menu player | SwiffPlayer.dll | 2,168 | 1,940 | 85 | 3 | 0 | 140 | 1.0% |
| Server network, files, console | bf2 | 1,882 | 1,434 | 4 | 2 | 0 | 442 | 18.6% |
| Client program | BF2.exe | 39,197 | Not measured | 29 | Not measured | 2,671 | 19,234 | 44.1% |

Key (the ledgers' own words):

- Bot AI: source body = `done`; stops = `stop`; library/compiler = `skip-lib`; open = `todo` + `skip-debug` (debug-drawing code set aside).
- Renderer and server: source body = `done`; deliberate replacements = `chosen`; stops = `stop`; library/compiler = `skip`; open = `todo`.
- Menu player: source body = `done`; deliberate replacements = `chosen`; stops = `stop`; open = `todo` + `unattributed` (no established attribution). No separate library/compiler status exists.
- Client: deliberate replacements = `deliberate`; library/compiler = kind `lib`; open review work = `todo` + `gap` + `cannot-tell` + `skipped`. These are review categories; source-body and stop counts are not measured. `present` and `client-tu` do not establish a whole-program source-body count.

Open counts exclude the separate stop column in the first four ledgers. Library/compiler counts use only the listed skip statuses, not renderer attribution classes. Bytes are the sum of retail function sizes, not unique image coverage; zero-size functions still count as rows.

The renderer row counts only RendDX9.dll; its BF2.exe rows overlap the client ledger and are excluded.

Recorded ledger revisions (generation hashes, not a claim of current-source coverage):

| Part | Recorded revision |
| --- | --- |
| Bot AI | `6f2fb0c1` |
| Renderer | `6f2fb0c1` |
| Menu player | unknown |
| Server network, files, console | `6f2fb0c1` |
| Client program | `e976b82b` |

## Renderer open rows by attribution

Only current RendDX9.dll `todo` rows are joined to the attribution table by address. Attribution does not change their ledger status.

| Attribution class | Open rows | Retail bytes |
| --- | --- | --- |
| cited | 0 | 0 |
| matched | 6 | 1,252 |
| inlined | 0 | 0 |
| stl | 227 | 43,093 |
| crt | 5 | 362 |
| compiler | 1,322 | 24,779 |
| editor | 0 | 0 |
| missing | 2,353 | 362,342 |
| No attribution class | 4 | 1,626 |
| Total | 3,917 | 433,454 |

`missing` means unexplained so far; it has not been shown to be unwritten code.

## Client program

Review commit(s): `e976b82b`. Verdicts describe source at each row's `commit` value; later fixes are not reflected yet. `none` and `todo` carry no review commit. The comparison pairs BF2.exe with bf2_w32ded.exe; Linux names supply symbols where known.

| Review group | Functions | Retail bytes |
| --- | --- | --- |
| Nothing client-specific to review | 15,487 | 1,662,438 |
| Compared one by one | 5,395 | 1,026,955 |
| Left out after a sample | 638 | 101,623 |
| Not reviewed yet | 17,628 | 1,709,557 |
| Client-only file taken as covered | 49 | 45,267 |
| Total | 39,197 | 4,545,840 |

The client-only-file group (`client-tu`) was taken as covered without individual comparison; it is shown separately so every row belongs to exactly one group.

Verdicts of rows compared one by one:

| Status | Meaning | Functions | Retail bytes |
| --- | --- | --- | --- |
| present | The source has the client behaviour. | 4,398 | 812,467 |
| gap | Client behaviour is missing or different in the source. | 710 | 133,664 |
| deliberate | The source differs on purpose, with a citation in the review. | 29 | 19,938 |
| cannot-tell | The comparison could not establish a verdict. | 258 | 60,886 |
| Total | Compared one by one | 5,395 | 1,026,955 |

Kind by status for all client rows. Each cell is functions / retail bytes; the [client ledger summary](tracks/client/ledger-summary.md) defines every kind and status.

| kind | none | todo | present | gap | deliberate | cannot-tell | client-tu | skipped | Total |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| same | 12,817 / 1,603,886 | 0 / 0 | 6 / 2,301 | 1 / 33 | 0 / 0 | 3 / 3,886 | 0 / 0 | 2 / 79 | 12,829 / 1,610,185 |
| layout | 1 / 180 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 1 / 180 |
| minor | 0 / 0 | 0 / 0 | 210 / 76,181 | 1 / 31 | 0 / 0 | 3 / 442 | 0 / 0 | 112 / 12,330 | 326 / 88,984 |
| diff | 0 / 0 | 0 / 0 | 4,043 / 704,989 | 695 / 121,936 | 28 / 19,857 | 142 / 55,744 | 0 / 0 | 523 / 89,193 | 5,431 / 991,719 |
| stub | 0 / 0 | 0 / 0 | 21 / 4,195 | 6 / 2,523 | 0 / 0 | 1 / 132 | 0 / 0 | 0 / 0 | 28 / 6,850 |
| ded-extra | 0 / 0 | 0 / 0 | 5 / 62 | 1 / 91 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 6 / 153 |
| lib | 2,669 / 58,372 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 1 / 21 | 0 / 0 | 1 / 21 | 2,671 / 58,414 |
| unmatched | 0 / 0 | 17,628 / 1,709,557 | 113 / 24,739 | 6 / 9,050 | 1 / 81 | 108 / 661 | 49 / 45,267 | 0 / 0 | 17,905 / 1,789,355 |
| Total | 15,487 / 1,662,438 | 17,628 / 1,709,557 | 4,398 / 812,467 | 710 / 133,664 | 29 / 19,938 | 258 / 60,886 | 49 / 45,267 | 638 / 101,623 | 39,197 / 4,545,840 |

`unmatched` means no dedicated-server counterpart was found; it does not prove the function is client-only.

`skipped` means left out after a sample of its group; those rows were not individually verified.

## Declared holes

These are source marker findings from the committed [marker table](tracks/markers.csv), not additional retail functions to add to the ledgers. Findings can overlap ledger rows or describe the same function more than once. The scan does not evaluate conditions; a conditional stop is still counted. The table is a saved scan, not a scan run by `--check`.

| Marker kind | Findings | Meaning |
| --- | --- | --- |
| abort | 81 | A definition or namespace-scope macro that stops at a live frontier. |
| free | 39 | A frontier comment attached to no function (for example a field or file header). |
| decl | 31 | A frontier-tagged declaration with a matching non-stopping definition in the scanned files. |
| partial | 28 | A non-stopping definition with a frontier comment inside its body; execution continues past a documented gap. |
| stale | 2 | A frontier-tagged definition with neither a stop nor frontier text in its body; it may be a stale tag, a cross-reference or a silent partial body. |
| seam | 0 | A definition calling a report-and-continue seam helper. |
| Total | 181 | Source marker findings |

| Directory under Code/BF2 | Findings |
| --- | --- |
| Animation | 15 |
| Core | 5 |
| Game | 100 |
| IO | 5 |
| Menu | 11 |
| Physics | 24 |
| Scene | 20 |
| Sound | 1 |
| Total | 181 |

## Not measured yet

Shared game code (objects, game logic, physics and animation) has no ledger of its own. Sound (BF2Audio and BF2OpenAL), the voice server and the Python host have none either. Some shared functions appear in the existing image ledgers; that does not make a complete inventory of these subsystems.

## How these numbers are produced

The page and its check read only committed tables and recorded revision lines. Regenerating the original function ledgers needs the inputs below. Retail images and local binary-analysis exports are not included in the repository.

| Part | Ledger command | Inputs needed to regenerate the ledger |
| --- | --- | --- |
| Bot AI | `python3 tools/ai-reconstruct/build_ledger.py --binary RETAIL_LIBAIDLL` | Symbolized retail Linux ia-32 libaidll.so, source/manifests, console bindings; objdump, c++filt and git. |
| Renderer | `python3 tools/tracks/rend_ledger.py --export out/rend-ghidra` | Ghidra function exports from retail RendDX9.dll and BF2.exe, source; git. Attribution is a separate committed input (tools/tracks/attrib/). |
| Menu player | `python3 tools/tracks/swf_align.py --export SWF_EXPORT --history GAMESWF_HISTORY --binary RETAIL_SWIFFPLAYER`, then `python3 tools/tracks/swf_ledger.py` | Retail SwiffPlayer.dll, its Ghidra exports, gameswf svn-r586 and tu-testbed rel-2004-05-12 checkouts, source; objdump. The second command needs out/swf-align tables/provenance plus committed completion overrides. The ledger CSV alone cannot regenerate its summary. |
| Server network, files, console | `python3 tools/tracks/srv_ledger.py --binary RETAIL_BF2` | Symbolized retail Linux ia-32 bf2 and source; objdump, c++filt and git. |
| Client program | `python3 tools/tracks/client_ledger.py --import TABLE.tsv --reviewed COMMIT` | Quoted client review TSV and its reviewed source commit. That working table is not published; producing reviews requires retail BF2.exe and bf2_w32ded.exe comparisons. The public CSV suffices for validation and its summary. |

Uppercase command arguments are placeholders for locally supplied inputs.

Refresh the declared-hole table explicitly (the source scan is slower):

```sh
python3 tools/tracks/progress.py --refresh-markers
```

This runs `tools/tracks/marker_check.py` over `Code/BF2/Game`, `Code/BF2/Menu`, `Code/BF2/Core`, `Code/BF2/Scene`, `Code/BF2/Physics`, `Code/BF2/Sound`, `Code/BF2/Animation`, `Code/BF2/IO`.

Regenerate and check the page and client summary without retail images or network access:

```sh
python3 tools/tracks/client_ledger.py
python3 tools/tracks/progress.py
python3 tools/tracks/client_ledger.py --check
python3 tools/tracks/progress.py --check
```

The checks validate table contracts and generated text. They do not compare source with retail instructions, update review verdicts, refresh source markers, or test a build or gameplay.
