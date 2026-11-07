#!/usr/bin/expect -f
# [TEMP] [CHOSEN, not recovered] Drain the ncurses PTY for 60 s, then request shutdown.
set bundle [lindex $argv 0]
set reference [lindex $argv 1]
set env(TERM) xterm
set env(LINES) 40
set env(COLUMNS) 120
set env(BF2_PORT) 16890
cd "$bundle/root"
set timeout 60
spawn -noecho /usr/bin/sandbox-exec -D "REFERENCE=$reference" -f "$bundle/offline.sb" \
    "$bundle/bin/bf2" +modPath mods/bf2 +config smoke_cfg/ServerSettings.con \
    +mapList smoke_cfg/maplist.con
set server_pid [exp_pid]
set early 0
expect {
    -re {.+} { exp_continue -continue_timer }
    eof { set early 1 }
    timeout { puts "\n60 s elapsed; sending quit"; send -- "quit\r" }
}
if {!$early} {
    set timeout 30
    expect {
        -re {.+} { exp_continue -continue_timer }
        eof {}
        timeout {
            puts "\nFAIL: server did not exit within 30 s of quit; terminating owned process"
            catch {exec /bin/kill -TERM $server_pid}
            close
            wait
            exit 124
        }
    }
}
set result [wait]
puts "\nserver wait result: $result"
if {$early} { puts "FAIL: server exited before the 60 s smoke finished"; exit 1 }
if {[lindex $result 2] != 0 || [llength $result] > 4} { exit 1 }
exit [lindex $result 3]
