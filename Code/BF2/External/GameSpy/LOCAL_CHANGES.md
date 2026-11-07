# GameSpy SDK local changes

This tree uses the December 2007 GameSpy SDK; qr2 identifies itself as version
3.04.00 in [its changelog](qr2/changelog.txt). Preserve the upstream notices and
keep integration changes identifiable in the source.

| File and function | Local change | Reason |
| --- | --- | --- |
| `serverbrowsing/sb_serverbrowsing.c`: `ServerBrowserAuxUpdateIP` | Check for an existing server address before starting and appending a new server. | Avoid rejecting the newly appended server as its own duplicate before the synchronous wait. |
| `qr2/qr2.c`: `qr2_init_socketA` | Initialize `denyresp2_ip_callback` to `NULL` with the other callbacks. | The enabled query-parser hook must not call an uninitialized function pointer. |
| `common/gsPlatformSocket.h`: `_MACOSX` socket-call macros | Apply the `socklen_t` casting macros to C translation units only. | Macro definitions such as `connect(s,a,al)` rewrite C++ member functions of the same name in engine sources that include the SDK; the engine already passes `socklen_t`. Affects only the macOS port. |

Both client and dedicated-server integrations can use these shared SDK files.
S2.10 adds these POSIX-only changes, all [CHOSEN, not recovered]:

- `common/{linux,macosx}/gsThread*.c`: sequentially consistent atomic increment/decrement,
  returning the new value. The SDK previously asserted and returned 1.
- `common/gsCore.c`: sequentially consistent atomic loads/stores for initialization/shutdown
  flags and the reference-count read; the C record layout and task-list critical section stay intact.
- `natneg/natneg.c`, `NNProcessData`: reject input shorter than the base header before reading
  its magic/type. Packed payloads already copy with `memcpy`.

Changing the SDK revision requires checking the interfaces and behavior used by
those integrations.
