#!/usr/bin/expect -f
# [TEMP] [CHOSEN, not recovered] Sandboxed TSan map-load/tick/quit and immediate rebind.
# [RETIRE-WHEN: s2-10-tsan-smoke] Replace with the native lifecycle test on both Macs.
set bundle [lindex $argv 0]
set architecture [lindex $argv 1]
set run [lindex $argv 2]
set timeout 180
set env(TERM) xterm
set env(LINES) 40
set env(COLUMNS) 120
set env(BF2_PORT) 16890
# [CHOSEN, not recovered] The shipped CheckServerAliveThread watchdog is never joined, so a leak
# report at exit is expected; it would make TSan abort in finalize and fail the clean quit.
set env(TSAN_OPTIONS) "halt_on_error=0:exitcode=66:symbolize=0:report_thread_leaks=0:log_path=$bundle/tsan-server-$architecture-$run"
cd "$bundle/root"
log_file -noappend "$bundle/server-$architecture-$run.log"
spawn /usr/bin/sandbox-exec -f "$bundle/offline.sb" "$bundle/$architecture/bf2" +modPath mods/bf2 +config smoke_cfg/ServerSettings.con +mapList smoke_cfg/maplist.con +port 16890
set child [exp_pid]
proc stopFailed {message} {
    global child
    puts stderr $message
    catch {exec /bin/kill -TERM $child}
    exit 1
}
expect {
    -re {Status:.*playing} { puts "Reached playing; observing 60 seconds of ticks" }
    eof { stopFailed "Server exited before playing" }
    timeout { stopFailed "Map load timed out" }
}
# Keep draining the pty so the console cannot block the server while it ticks.
set timeout 60
expect {
    eof { stopFailed "Server exited during tick observation" }
    timeout {}
}
send -- "quit\r"
set timeout 30
expect {
    eof {}
    timeout { stopFailed "Server did not quit within 30 seconds" }
}
set result [wait]
if {[lindex $result 2] != 0 || [lindex $result 3] != 0 || [llength $result] > 4} {
    puts stderr "Server quit failed: $result"
    exit 1
}
puts "PASS map-load/ticks/quit run $run ($architecture); inspect TSan reports"
