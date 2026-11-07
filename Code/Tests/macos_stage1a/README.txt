OpenBF2 stage 1a M2 validation [TEMP] [CHOSEN, not recovered]

1. Copy this whole stage1a folder to a writable APFS location on the M2,
   outside the Battlefield 2 installation. No Command Line Tools are needed.
2. Open Terminal, cd to the copied folder, and run:
   chmod +x run.sh
   /usr/bin/arch -arm64 /bin/bash ./run.sh "$HOME/Applications/Battlefield 2.app/Contents/SharedSupport/prefix/drive_c/Battlefield 2"
   cat results.txt
3. Send results.txt, python.log, aidll.log and server.log (if created) to the
   coordinator. Keep the full log when a frontier or failure occurs.

Output goes directly to results.txt. Cloning and the smoke can take a while;
server.log contains the full live terminal output once the server starts.

The first argument is the read-only retail content root. mods/, python/ and
pylib-2.3.4.zip are APFS-cloned with cp -Rc under this folder's root/. A failed
clone stays in clone.XXXXXX and is not used. No original game files are written.
Only the clone and this bundle's logs/configuration are modified. Keep the bundle
on APFS; a filesystem without clone support causes a visible failure.

The script records uname -m, hw.model, artifact slices/dependencies, each check's
exit code and the last 50 lines of each log. It runs the dice_py_check_run payload
with the staged copy_reg.py/types.py files, then the real AI loader check (exports
and private globals only: initDll is not called). It refuses to start a server if
sandbox-exec cannot install the profile or UDP 16890/29911 is occupied.

The server uses the milestone 1 sandbox profile: outbound loopback only, no DNS,
read-only original content and a hard denial on binding port 16567. Its config
explicitly uses offline loopback port 16890, query port 29911, no VoIP, no ranked
services, no demos and dalian_plant/gpm_cq/16. No AI/bot test is requested.
expect supplies the terminal required by ncurses, drains output for 60 seconds,
then types quit. A premature exit or failure to quit within 30 seconds fails the
check. A timeout terminates only the process started by this run and is reported.
Inspect server.log for map-load/playing reach; an exit code is not gameplay proof.

Do not remove the sandbox wrapper or run the server directly if isolation fails.
For an Intel slice rehearsal, run:
   /usr/bin/arch -x86_64 /bin/bash ./run.sh '/path/to/Battlefield 2'
Keep the Intel results separate from the owner's native arm64 results.

To reproduce this bundle from the repository after building macos-universal:
   bash Code/Tests/macos_stage1a/package.sh out/build/s1a-arm64
No proprietary game content is included in the staged bundle.
