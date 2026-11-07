OpenBF2 arm64 bundle for the M2 [TEMP] [CHOSEN, not recovered]

What it is: the renderer-ON macOS client (bf2_client), the dedicated server (bf2), libdice_py, the
AI image (libaidll), the arm64 builds of the CPU and GPU checks, and a copy of the loopback harness
that runs a sandboxed local server and client against each other. Everything was cross-compiled on
an Intel Mac. No game content is included: the bundle reads your Battlefield 2 folder and never
writes to it (each run clones it under out/loopback/run-*/).

Full instructions, the hand-play checklist and what to look for: docs/macos-c-arm64-client.md in
the repository ("Owner instructions (M2)"). Short form:

1. Unpack into a writable folder on the same volume as the game, outside the game folder and
   outside Desktop/Documents/Downloads, for example ~/OpenBF2-m2:
      mkdir -p ~/OpenBF2-m2 && tar -xzf openbf2-arm64-*.tgz -C ~/OpenBF2-m2
      xattr -dr com.apple.quarantine ~/OpenBF2-m2/openbf2-arm64
   (run.sh also clears the quarantine flag and re-signs any binary whose signature fails.)
   The harness runs on the bundle's own Python (python/, a portable CPython build), so neither
   Xcode nor the Command Line Tools are needed. Bundles staged without it fall back to the
   Command Line Tools' python3 ("xcode-select --install") or BF2_PYTHON=/path/to/python3.
2. In Terminal (logged-in desktop session; windows open):
      cd ~/OpenBF2-m2/openbf2-arm64
      /usr/bin/arch -arm64 /bin/bash ./run.sh --content "<Battlefield 2 folder>" gate    # FIRST, ~15 min
   gate runs the three standard scenarios (infantry-range, render-view-dalian, render-view-ocs) and
   the map2 reload run, and compares the render views with the Intel reference views. A view over
   the Intel limit is listed as information only (different GPU). Do not touch mouse or keyboard
   while it runs. Then AirDrop results.tgz back. The other modes:
      /usr/bin/arch -arm64 /bin/bash ./run.sh preflight
      /usr/bin/arch -arm64 /bin/bash ./run.sh all          # checks + two scripted runs, ~15 min
      /usr/bin/arch -arm64 /bin/bash ./run.sh play 10      # you play, about 10 minutes
   If the game is not in "~/Applications/Battlefield 2.app/Contents/drive_c/Battlefield 2", add
   --content '/path/to/Battlefield 2' (the folder with mods/, python/, pylib-2.3.4.zip).
3. AirDrop back results.tgz from the bundle folder (every mode rewrites it with everything collected
   so far; it holds no game content).

Modes (./run.sh --help):
  gate       the arm64 checks, then the Intel track gate: infantry-range, render-view-dalian, render-view-ocs, map2 reload;
             results.txt lists per run the [FRONTIER] count, crash reports, script finished, and the
             per-view numbers against reference/baselines/ with compare PNGs (results/compare/)
  preflight  quarantine, signatures (codesign -v; ad-hoc re-sign on failure), arm64 check,
             MANIFEST.sha256, Python 3.7+ (the bundled python/), expect/sandbox-exec, content folder
  checks     dice_py_check, bf2_aidll_loader_check, bf2_native_vtable_check and
             bf2_meme_dispatch_check (the runtime half of the vtable audit), macos_gfx_forward,
             macos_metal_device (CPU, GPU with MTL_DEBUG_LAYER=1, three frontier modes that must
             abort with exit 134)
  smoke      server + client, 16 bots on Dalian (co-op), input script play-combat, 200 s, bots
             spawn-only: the track's gate run. PASS = client still running at 200 s, clean
             server exit, no crash report, no [FRONTIER] the Intel gate run did not reach.
  smoke-ai   the same with the full bot AI ticking (bots move, so aiming may miss)
  play [MIN] server + client with 16 bots and no input script; you play. Screenshots about once
             a minute (at most 20). Leave through the game menu or let the time run out.
  collect    rebuild results.tgz only
Extra harness options go after --, for example: ./run.sh play 10 -- --video-high --fullscreen

The scripted and hand-play runs use loopback UDP 127.0.0.1:16890 and 29911 only, under
sandbox-exec (no other network, port 16567 denied, writes only inside this folder's out/). The
first run compiles every effect natively into out/fx-native-cache (this is part of the test); the
later runs reuse it. Delete old out/loopback/run-* folders when you like.

Files: bin/ (checks and their fixtures), out/build/arm64/bin_macos/ (game images), tools/macos/loopback/ (harness copy; reference/loopback.py.diff shows its one change),
python/ (portable CPython from python-build-standalone, its licence in python/lib/python3.*/LICENSE.txt),
reference/ (the Intel gate run's frontier set), artifacts.txt (architectures, hashes, dependencies),
MANIFEST.sha256.
