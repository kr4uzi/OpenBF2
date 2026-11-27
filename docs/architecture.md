# Engine architecture

## Images and boundaries

The repository contains several images built from overlapping source sets. The root
`CMakeLists.txt` defines the engine and client manifests; the directory tree alone does
not determine what links into an image.

`DEDICATED=1` selects the dedicated engine. `DEDICATED=0` selects the client and client
DLL code. Every engine compilation force-includes `Core/BuildConfig.h`, which rejects a
missing image define. A runtime `isDedicated()` check still matters in the client image:
the retail client supports a dedicated mode, so a runtime branch cannot automatically be
replaced by conditional compilation.

The client uses separate renderer (`RendDX9`), sound (`BF2Audio`), movie (`SwiffPlayer`),
and Python (`dice_py`) components. Rebuilt renderer, sound, and movie plugins target the
rebuilt host's native ABI and share its dynamic CRT and release STL layout. Mixing these
plugins with the retail executable, or retail engine plugins with the rebuilt executable,
is unsupported. The AI side-DLL has its own manifest and build target.

This is an intentional policy change under [issue #7](https://github.com/B2BF/OpenBF2/issues/7),
`[CHOSEN, not recovered]`. Gameplay compatibility with retail clients and existing content
remains a requirement. The first removal pass retires compatibility plugin builds and
the `Memory.dll`/MSVCR71 heap adapters, and stages a rebuilt TextureAtlasBuilder with the
client. TextureAtlasBuilder retains its static CRT and legacy float formatting because
those choices preserve `.tai` output, independently of executable compatibility.

The VC7.1 container and string adapters (`vc71::*`, `Msvcr71Heap`) are removed. The renderer,
movie and audio plugins, the client and the editor hold this toolset's `std::string`,
`std::vector`, `std::map`, `std::set` and `std::list` directly, and pass them across the image
boundaries by reference or by value. Objects that embed them are sized with `sizeof` and
constructed in place; their `// +0xNN` comments keep the retail offsets as evidence only.
Container element types that no recovered code reads stay marked `[UNRESOLVED]`.
`RendDX9Abi/PluginAbi.h` enforces the shared contract: `/MD` and `_ITERATOR_DEBUG_LEVEL=0` in
every image. Recovered `__fastcall` factories are still published through stateless thunks.
Dedicated/non-x86 alternatives and unresolved native frontiers retain their existing behavior.

### Native x64 client

The client, its four DLLs and the editor also build as native x64 images
(`[CHOSEN, not recovered]`, [issue #7](https://github.com/B2BF/OpenBF2/issues/7)). The x86
images are unchanged and stay the fidelity reference. The rules that keep both correct:

- **Hand-built vtables.** Thunks are declared `RD9_THUNK name(void* self RD9_THUNK_EDX, ...)`
  and called directly as `name(obj RD9_THUNK_ARG, ...)` (`Code/RendDX9Abi/ThunkAbi.h`). On x86
  this is the `__fastcall` form with its dummy EDX parameter; on x64 the dummy is absent, because
  it would shift every later argument. Calls through raw function pointers use `__thiscall`
  pointer types without the dummy, which is the same ABI on x86 and correct on x64.
- **Raw slots.** A vtable slot named by its retail byte offset is indexed `offset / 4`, never
  `offset / sizeof(void*)`: MSVC keeps the slot order on x64.
- **Layouts.** Retail size and offset asserts are gated on `_M_IX86`. Where code reached a field by
  its retail byte offset, it now uses a named member, `offsetof`, or an accessor keyed by the
  retail offset that maps to the native field and stops with `[FRONTIER]` on an unmapped offset:
  `rendererTail*`, `rendererRenderSwitch` and the other Renderer accessors in
  `RendDX9/RendDX9Base.h`, `raShaderConstantsAt` for the shader-constants block
  (`RendDX9/RenderQueues.cpp`), and `Geom/ParticleSystem/ParticleRawLayout.h` for the particle
  descriptors, instances, pools and renderers.
- **Handles.** Values that carry a pointer are pointer-sized on x64 and keep `int` on x86: the meme
  font and texture handles (`FontHandle`/`TextureHandle` in `External/BFMeme/Object.h`), the HUD
  icon handles, and the SwiffPlayer render-capability argument (`intptr_t`).
- **Platform selection.** A `[PLATFORM]` choice between the PE and ELF behaviour is gated on
  `_MSC_VER` (Windows images), not `_M_IX86`; `_M_IX86` gates only x86 inline assembly and
  x86 layouts.

The Windows x64 client and dedicated server use the same native `aidll.dll`, built from
the AI manifest with the host's CRT and STL ABI (`[CHOSEN, not recovered]`,
[issue #11](https://github.com/kr4uzi/OpenBF2/issues/11)). The DLL exports only `initDll`
and `deinitDll` by name; x86 retail ordinals are not an x64 contract. The server's former
empty-name policy is retired. Windows link and runtime validation remain pending.
Other x64 client limitations are listed under [Native x64 client](building.md#native-x64-client).

SwiffPlayer's wrapper now contains only its native host paths; the retail-host conditional
arms have been removed under the same issue. The native ActionScript callback whitelist and
the `swiff-native-as-host` and `swiff-host-bodies` frontiers remain, along with the RW-88
callback/tracing, RW-89 crash-tracing, and RW-49 module-consolidation boundaries. `voipStopTest`
and the other VoIP rows reach the recovered voice client (Game/VoipClient, 2026-11-13); their
report-and-continue exception is retired. Its strings, player-stat records and
queued-mouse storage are std containers. This removal does not change movie formats or callback
behavior.

## Source map

All engine paths below are relative to [Code/BF2](../Code/BF2/).

| Area | Responsibilities and starting points |
| --- | --- |
| `Game/Main/` | Process startup, `BF2`, `BF2Engine`, module assembly, and the main loop. |
| `Core/` | Class and singleton registration, interfaces, events, settings, platform services, and diagnostics. |
| `IO/Console/`, `IO/File/` | Console commands and scripts, filesystem access, archive mounting, and resource lookup. |
| `IO/Network/` | Sockets, packets, connection transport, and networkable descriptions. `ProtocolLayer/` holds `NetServer` and `SWConnection`. |
| `Game/GameServer/`, `Game/GameClient/` | Session ownership, connection handling, loading, and server/client update paths. |
| `Game/GameLogic/`, `Game/Python/` | Level loading, game rules, and the embedded Python interface. |
| `Game/Objects/`, `Scene/` | Object templates, live world objects, components, and world registration. |
| `Game/GameEvents/`, `Game/Common/` | Serialized game events, shared game state, and ghost replication. |
| `Physics/`, `Math/`, `Animation/` | Simulation, collision, transforms, skeletons, and animation. |
| `Geom/`, `RendDX9/`, `RendMisc/` | Geometry resources, terrain, rendering, and renderer support. |
| `Menu/`, `Sound/` | Client interface and audio integration. |
| `External/` | Upstream dependencies and their local integration notes. |

[Code/Tools/BF2Editor](../Code/Tools/BF2Editor/) contains the editor application.
[Code/Tests](../Code/Tests/) contains focused fixtures and compile checks;
[Code/Build](../Code/Build/) contains build helpers and diagnostic harnesses.
These harnesses are scaffolding, not alternate production engine implementations.

## From startup to a running server

The production entry in `Game/Main/BF2.cpp` creates `BF2`, calls `init`, runs `BF2::run`
until it returns false, then destroys the application. `BF2Engine` owns engine startup,
command-line settings, the main loop, and shutdown. `Core/Module` and `ClassManager`
provide registered factories and named singletons used during assembly.

For a dedicated session, `BF2::init` reaches `BF2Engine::startGame`, which assembles the
game and invokes `GameServer::startServer`. The server constructs and initializes its
`NetServer` before selecting and validating a map. A no-bot session still opens a real
network listener.

The selected map reaches `GameServer::loadPath`, then `GameLogic::load`. Loading mounts
content, executes the level's console scripts, initializes game-mode Python, and publishes
world state through the engine's event and object systems. The order is observable: script
commands refer to registered types and resources, while synchronous load events can invoke
other subsystems before loading returns.

On a client, `GameLogic::beginLoadStats` publishes the loading flag, fades main-menu
or between-rounds music through `MusicPlayer`, and releases the old menu before
posting the loading-screen events (BF2.exe `0x4EC030`). Only runtime dedicated mode
sets the callback interval to 1/30 second; a client keeps the configured interval
(initially 0.5 seconds). The client load callback yields with `System::sleep(0)`
and services `BF2::handleInGameWinMessages` when the application exists. That method
keeps each platform's event pump and quit handling. Process working-set housekeeping
remains the `loadcallback-client-preamble` frontier.

Successful demo playback disables HUD paint and update through
`BF2HudManager::setPaint(false)` (BF2.exe `0x40653A`). A lightmap worker accepts a
nonnull player's spawn-group selection without posting a gameplay request
(`0x4E1C42`); ordinary clients retain the normal request path.

`BF2::run` delegates each iteration to `BF2Engine::mainLoop`. Timing supplies the server's
simulation step count and timestep; `GameServer::update` coordinates connection work,
simulation, events, and outgoing state. Loading also pumps a special update path, so seeing
an update call alone does not prove that normal simulation ran.

Shutdown includes game unloading, owner and reference release, listener shutdown, and
receive-thread completion. A successful startup test therefore also checks clean shutdown,
immediate port reuse, and reload. Record the configuration and paths exercised with every
test result; the presence of these methods does not establish runtime coverage.

## Radio spam accounting

The radio and chat allowance belongs to each `Player`. Submission charges ordinary
radio messages; `AUTO_` kit use messages bypass that charge. The server relay compares
the low byte of the player ID with `0xFF` before charging: the hosted human has ID 255
and shares the submission counter, while remote players have a separate server counter.
Preserve this byte comparison on native ports. Comparing the full integer with `-1`
charges the host again and charges it for bypassed and bot-generated server messages.
The original checks are BF2.exe `0x4C8E8B`–`0x4C8E9D`, Windows dedicated
`0x445F2B`–`0x445F3D` and Linux ia-32 `0x080AEBCB`–`0x080AEBE4`.

The medic's automatic `get_up` line after a successful revive is charged as in retail:
at the host submission (BF2.exe `0x4E1381`–`0x4E1393`), at the client's submission through
HUD event 51 (`0x75A7E0`–`0x75A7FA`), and at the server relay under the player-ID rule
above. HUD event 51 carries the message name in 32 bytes followed by the channel at
offset `0x20`. A blocked player's revive line is refused; the local human receives
`spam` on channel 14 and `HUD_HUD_MENU_CHAT_SPAM_MESSAGE`, with the host notice skipped
on a dedicated server. This line is distinct from the defibrillator's bypassed
`AUTO_MOODGP_usingshockpaddles` use message. The
[AI radio fix](retail-fixes.md#ai-radio-messages-consume-one-allowance) keeps AI from-player
messages charged once at relay. The limiter resets the entire interval
window, rather than removing one message at a time. Windows images retain the double
world-clock result during comparisons; the Linux dedicated image rounds each read to
float. World time is in seconds. `Code/Tests/radio_spam_runtime.py --engine-build
<build-directory> --output <check-directory>` uses the completed macOS client and
dedicated objects to check submission, remote HUD dispatch, relay, clock boundaries
and kit use without starting a game or opening devices or sockets. It compiles the
successful-revive notification branch from source into the fixture; health changes,
revive eligibility and production gameplay are not exercised.

## Debugger detection at startup

`[CHOSEN, not recovered]` `Core/SystemInfo.cpp` returns false from
`PosixSystemInfo::getIsDebuggerInstalled` on every platform, retaining the Linux server's
answer (ia-32 ELF `0x083D2C80`) on Windows too. Retail Windows (`BF2.exe` `0x004C47A0`,
`bf2_w32ded.exe` `0x00442990`) reads `SOFTWARE\Dice`'s `forceNoDebugger` override
(`BF2.exe` `0x004C47C8`, `0x004C47E5`, `0x004C4809`), tests the Visual Studio 7.0/7.1
Visual C++ registry keys (`0x004C48DE`, `0x004C4900`), and reads AeDebug's `Debugger`
value (`0x004C4922`, `0x004C4945`, `0x004C4967`). It excludes commands containing
`drwtsn32` or `drwatson` (`0x004C4992`, `0x004C49B8`) and returns a Boolean
(`0x004C4A14`). This queries debugger registrations.

When retail would report a registered debugger, OpenBF2 still takes the no-debugger
startup choices: `BF2Engine::init` clears `hack-ignore-asserts` to `0` for non-dedicated
clients, and the Windows `bf2WinMain` wrapper in `Game/Main/BF2.cpp` keeps its SEH
crash-dump scope unless `+seh 0` is supplied. Retail's true answer skips that flag reset
and bypasses the SEH scope. Dedicated mode queries the answer without resetting the flag.

## Squad asset requests

Asset requests travel through `IssueSquadOrderEvent` (24), order type 2, to
`SquadManager::issueRequest`. After storing and broadcasting the request, a squad with a
leader and initialized AI posts ECAI/15: the complete `SquadOrder`, team, squad and first
point. `AIEventListener` forwards artillery, UAV, vehicle-drop and supply requests to the
strategic AI. The leader's ECHud/30 notification follows locally, except that a remote
co-op leader receives a `PostRemoteEvent`. With no active AI, only leader ID 255 uses local
delivery. An assigned commander's ECHud/29 notification follows the leader's notification.
This ordering is shared by client and dedicated images (`BF2.exe` 0x4E60F0, dedicated PE
0x45E710, Linux x86 ELF 0x0811DAE0).

The retail asset-authorization rule still compares the sender's low byte with the full
commander ID. The AI's local sender -1 becomes 255 and fails against an empty commander
slot (-1). Restoring request delivery therefore restores AI handling and HUD feedback,
but does not authorize assets without a commander. The AI rearms UAV requests for 30 seconds
and other requests for 60 seconds, queues cooldown refusals for 2–5 seconds later, requires
surviving artillery, and suspends automatic handling while a human commands the team.

After configuring a macOS build, run
`python3 Code/Tests/commander_requests_runtime.py out/build/<directory>` for headless checks
with both image defines. The fixture executes production request, listener, courier, policy
and asset-event code with synthetic squads, transport, clock and asset observers; it does
not start a game or validate asset creation in a level.

## Client audio defaults

`[CHOSEN, not recovered]` New profiles and an explicit restore of audio defaults use
provider `hardware` and quality `High` on every platform. The rounded physical-memory
check retains `Low` below 768 MB. Software, Low and Medium remain selectable, and loading
an existing `Audio.con` applies its saved values through the same setters as before.
The [full-width memory read](retail-fixes.md#new-profiles-get-the-audio-quality-the-memory-check-intended)
is a retail fix; preferring hardware and High is a separate port policy. Software with
Low or Medium can lose single-fire weapon sounds on macOS; hardware with High avoids
that path. No profile syntax, provider index, quality row or network contract changes.

Retail `BF2.exe` 1.50 x86 `0x4B0C10` assigns `software` and selects Low/Medium around
768 MB (`0x4B0C3F` calls the memory helper; `0x4B0C44` compares). Its apply helper
`0x4B1420` maps hardware to BF2Audio provider 1 without EAX or 2 with EAX, both requesting
`DirectSound3D`. BF2Audio builds its provider table at `0x100112A0`; `OpenALProvider::open`
at `0x100118F0` loads `BF2OpenAL.dll` and passes that device name to `alcOpenDevice`
at `0x10011A49`. A failed open closes the provider and returns error 4; it does not try
software. `BfSoundEngine::openProvider` (`0x10005B70`) clears the active provider on
failure, and `initialize` (`0x10006FA0`) returns that error. The settings wrapper
`0x4B1750` retries the previous record (`0x4B179B`) and restarts music (`0x4B17AF`),
then returns the original failure. Before the first successful apply, the constructor
(`0x4B1B20`) leaves the current provider empty with EAX enabled: name lookup
(`0x4B1240`) returns -1, plus EAX gives index 0 into `{6, 0, 1, 2, 3, 4}`
(`0x8A3538`). Retrying that record therefore opens software row 6 at the current quality
(now High on normal-memory machines). A successful retry clears status but still returns
the original failure and leaves the requested settings pending. After a successful apply,
failure instead retries the previously applied provider and quality. These are shared
recovered paths.

The native OpenAL backends already select a working default output when no physical
hardware acceleration is available:

| Platform | Hardware device selection |
| --- | --- |
| macOS | `OpenAL.framework` accepts `DirectSound3D` and opens the default output, as measured in [Audio (S2.5)](macos-port.md#audio-s25). Both BF2OpenAL and OpenAL32 names load that framework. |
| Windows | The [bundled OpenAL Soft 1.25.2](ci-releases.md#openal-soft-in-the-windows-clients) treats `DirectSound3D`, `DirectSound` and `MMSYSTEM` as default-device aliases in [`alcOpenDevice`](https://github.com/kcat/openal-soft/blob/1.25.2/alc/alc.cpp#L2770-L2798). It mixes in software without requiring a hardware sound card. |
| Linux | `OpenALProvider::open` passes null to OpenAL Soft (`libopenal.so.1`) for non-system provider rows, selecting the native default output (issue #36). |

No additional software retry is needed on these backends: the hardware and software rows
open the same output, and the initial-apply path already retries software. Missing libraries,
absent output devices and context failures still report initialization failure; changing
the provider label cannot supply an absent device.
EAX detection is optional for the hardware row, so lack of EAX does not refuse it. High
is valid for every provider: `applyRecord` maps it to engine quality 0 and 32 voices;
BF2Audio loads samples at 44100 Hz. Only UltraHigh requires creative and is downgraded
to High on other providers. High is never downgraded.

`Code/Tests/audio_settings_runtime.cpp` checks the memory guard, exact saved lines,
existing software/Low/Medium settings, and High on every provider through the real
settings owner and typed sound observers. It also checks an initial hardware-open failure
followed by a successful software retry, retaining High and the wrapper's original result.
Run its `.cmd` runner on Windows x86, or
`sh Code/Tests/audio_settings_runtime.sh <build-directory>` on macOS x86_64.
This fixture opens no audio device.

## Client menu fonts

The main, pause and between-rounds Flash menus use a centered 4:3 viewport.
The menu renderer clears unused pixels to black, and the player translates pointer
coordinates into the same rectangle. The selected display mode still supplies
the full backbuffer for the game world and its HUD.
At a font scale other than 1 (any menu width but 800 and 1024, such as 1066 on a
1280x800 panel), RendDX9 keeps the shipped glyph layout and line measurement but puts
each glyph quad on a whole pixel. Inside a word, equal font-unit steps get equal pixel
steps, and the word stays centred on its shipped extent. Retail scales the font-unit
positions to fractional pixels, so the gaps between letters vary
(`GeomMisc/FontSystem/GlyphPixelSnap.h`).
See [issue #45](https://github.com/kr4uzi/OpenBF2/issues/45).

The rebuilt clients automatically draw menu text at the output resolution. When a
bitmap font is enlarged, RendDX9 matches its shipped atlas against glyph outlines
from the loaded SWF movies. A match supplies a higher-resolution texture with the
original glyph positions, advances and kerning. Unmatched fonts and glyphs retain
their bitmap appearance. The spawn and commander HUD font pages use the same matcher.
SwiffPlayer and RendDX9 exchange outlines through the engine's named singleton
registry, which also works when Windows loads the two plugins as separate DLLs.
SWF curve precision follows the ratio of output pixels to movie pixels.
A menu font whose best fit is weak (error above 0.05) is fitted again when another movie
loads, and a better fit replaces its texture. Which typefaces are available depends on the
order the movies load in: through the main menu, `DynamicText_13` first fitted TradeGothic
(error 0.096). A start straight into the loading screen fits Helvetica Condensed Bold
(error 0.011).
See [issue #42](https://github.com/kr4uzi/OpenBF2/issues/42).

## Client display modes and HiDPI UI

`[CHOSEN, not recovered]` policy shared by the rebuilt clients
(`RendMisc/DisplayModePolicy.h`, `Menu/MenuViewport.h`):

- A new profile's resolution uses the preferred mode, the same rule on every
  platform: the largest listed mode that fits the desktop. Windows and Linux list
  pixel sizes; macOS lists GUI-usable logical point sizes, capped at the largest
  HiDPI logical size when present. New Apple-silicon profiles therefore find and
  select their desktop point size (1470x956 on an M2 at that desktop setting),
  instead of its 2940x1912 backing size. When none fits, the rule selects a listed
  whole multiple of the desktop, otherwise the smallest larger mode. On macOS and Linux a
  fullscreen start without `+szx/+szy`, and the engine's own default sizes (the
  presets' 800x600 and 1024x768, the empty profile's 800x600@60Hz), take the same
  mode when the display does not list them. Windowed starts and explicit
  `+szx/+szy` keep their size.
- Any other profile resolution the display does not list runs as the closest listed
  mode ([issue #4](https://github.com/kr4uzi/OpenBF2/issues/4)); `Video.con` keeps
  the stored value until a resolution is set again.
- Every successful device reset logs the back-buffer size, mode, refresh rate,
  fullscreen, vsync and multisampling.
- From 1350 lines the console is drawn at a whole-number scale, the back-buffer
  height over 900 rounded (1600 and 1800 lines: 2, 2250: 3), with its bitmap font
  as point-sampled blocks. It shows as many lines, at most the shipped 20, as cover
  the share of the screen retail's console has at 1440x900 (1600 lines: 18 lines).
  Below 1350 lines it is the shipped console.
- macOS: the menu's `.ani` cursors are enlarged by `MenuViewport::bitmapScale`, the
  4:3 menu viewport's height over 600 rounded down (1 below 1200 lines, 2 at 1600
  and 1664, 3 at 1912), so they look no larger against the UI than in the 800x600
  menu. On screen a cursor pixel covers that many back-buffer pixels, kept between
  0.5 and 4 points per cursor pixel (`Platform/macOS/CursorScale.h`): 1.125 points
  for 2560x1600 full screen on a 1440x900-point desktop.
- macOS: Core Animation enlarges the drawable with nearest-neighbour filtering only
  at a whole-number factor (linear otherwise, so 800x600 or 1650x1050 full screen on
  a Retina panel is not jagged).

## Client menu mouse queue

`[CHOSEN, not recovered]`: `SwiffPlayer::update` hands the movie one queued mouse sample
per frame, as recovered, after dropping queued samples that the next one repeats
(`External/gameswf/SwiffPlayer/QueuedMouse.h`). The loading and end-of-round screen
(`BetweenRoundsLogic`) queues the pointer state once per 30 Hz input tick, so below 30
frames a second each frame queues the same state more than once. Without the rule the
queue grows on every frame and the movie falls behind the pointer: at 17 frames a second
a click on JOIN GAME a minute after the level loaded takes most of a minute to reach
the button. The movie still receives every distinct pointer state and button change, in
order and one per frame. A frame that holds one input tick at most, the normal case
above 30 frames a second, queues nothing the rule would drop.
`bf2_swiff_queued_mouse_check` models both rates. A press and its release that arrive
within one frame are still not seen by that screen, as recovered.

## Anti-aliasing change without a restart

`[CHOSEN, not recovered]`, issue #60 (https://github.com/kr4uzi/OpenBF2/issues/60):
retail reports "restart needed" for every anti-aliasing change (`VideoSettings::needsRestart`),
and `VideoSettings::applyToOptions` pushes the level only while the settings repository's
`GSLoadLevel` is empty, so after a match the edit is dropped by the read-back. Here both
follow one rule (`RendMisc/AntialiasingPolicy.h`): while no game is loaded, Apply pushes
the level and no restart is announced; while a game is loaded nothing is pushed and the
restart notice stays. The renderer switches the level the way every start does
(`videoOptionsChangeResolution`: device reset, multisample probes, define rebuild). Two
renderer changes keep nothing stale: the probe's force and disable bytes, which only ever
became set, are cleared before the probe re-derives them, and a switch that changes the
`HASALPHA2MASK` define marks the effects for the reload the next level load runs
(`dirty116`..`dirty118`, as a texture-filtering change does). Other multisample users
(terrain, vegetation, post-production, render targets) read the state at draw time or are
rebuilt by the device reset. `bf2_antialiasing_policy_check` covers the rule; the
renderer side is verified by reading only.

## Client menu disconnect

The stock `mainMenu.swf` disconnect page queues `dice.bf2.Logic.disconnectGame`
through its UpdateManager, then polls `Logic.isGameRunning` while displaying the
disconnect modal. A missing native callback leaves the game running and the page
waiting indefinitely. `SwiffPlayer.cpp` registers the callback in the rebuilt plugin.
It requests shutdown on the player's host while the separately looked-up registry
host owns the busy cursor. The registry host may be absent; the player and its host
are required. Arguments are ignored and the ActionScript result is left untouched.

This bridge follows the original x86 BF2 1.50 `SwiffPlayer.dll` (MD5
`b4ef39939007a555f23829824c021e35`): registration at `0x1004DFC3` binds the
callback at `0x1004CB10`; `0x10013520` establishes the cursor/busy scope. The callback
invokes host slot `+0xB4`. In BF2.exe, `SwiffHost::requestShutdownGame` at `0x491860`
queues event 17, and `BF2Engine::renderMenu` at `0x40BD10` consumes it through
`shutdownGame(false)`. `SwiffHost::isGameRunning` at `0x486C90` reports whether
the global game still exists. The movie clears its modal after that becomes false.
The movie library is client-only; there is no dedicated-server ELF counterpart to
this callback. No host layout, event ID, or network protocol changes are needed.

`Code/Tests/swiff_disconnect_runtime.cmd` resolves the registered ActionScript
member instead of calling `SwiffHost::requestShutdownGame` directly. It takes a
fresh native plugin object directory and an isolated test output directory. The optional
`native` argument remains accepted. Distinct registry and player hosts expose
incorrect cursor ownership; the callback must preserve call order and the
caller's result even when the host returns false. This boundary check does
not establish joined-client teardown or reconnect stability. The existing
`swiff-native-as-host`, `swiff-host-bodies`, and `RW-88` frontiers outside this
callback remain subject to their existing recovery and validation requirements.

## End-of-round and load-game menus

`EndOfRoundLogic` (class ID 121015) and `LoadGameLogic` (121038) implement only
`IMenuLogic`. Their factories follow `BetweenRoundsLogic` and precede
`LocalProfileManager` in the client module list (BF2.exe 1.50 x86
`0x430BDA` / `0x430C19`). They are absent from dedicated builds and from the
client's runtime dedicated class list.

The retail allocations are 0x44 and 0x38 bytes, with primary tables at
`0x8A2AF8` and `0x8A2C08`. Both contain the reference count, enabled/frame flags,
borrowed player/host/renderer/console pointers and the saved control-map name.
EndOfRoundLogic additionally stores a mouse-button byte and two coordinates.
Native pointers and STL members follow the rebuilt host ABI; macOS x86_64 sizes
are 88 and 72 bytes. These native sizes do not validate the retail x86 layout.

EndOfRoundLogic loads `EndOfRound.swf`, disables the mouse device and forwards its
cached pointer state. Its Escape-key input queues menu event 15 while a game exists
and no host request is pending. `BF2Engine::activateMenu` accepts its class ID
(`0x4033B0`, selection `0x403426`, creation call `0x40344C`). LoadGameLogic loads
`loadGame.swf`, activates all input devices, samples unscaled mouse axes multiplied
by 30 with truncation, and forwards console input when the console is open. Its
Escape-key input queues event 9 while a game exists. No direct creation caller for
LoadGameLogic was found in the retail executable; publishing its factory does not
add a new menu transition.

Both logics enable the menu before looking up the movie services, preserve partial
state when a required service is missing, and disable/unload it during teardown.
Their resize and window Escape callbacks are shipped empty bodies. Menu input has
no return value; the engine discards it. The separate `BetweenRoundsLogic` remains
the loading and between-round screen selected by ModuleGame events 3 and 9
(`0x410865`); events 7 and 8 do not create either of these classes.

`Code/Tests/menu_logics_runtime.py <build-directory>` checks the production class
list, factory lookup, native fields, lifecycle and input paths with headless
collaborators. It opens no window and initializes no game, content or audio device.
It also checks that the dedicated executable and dedicated compilations of the two
source files contain neither class. Movie appearance and live menu transitions
require separate client validation.

## Object-owned sounds

Weapon components request `Fire1P`, `Fire1P_Outdoor`, `Fire3P`, and their other
samples through `SimpleObjectTemplate::createSoundFromTemplate`. The client factory
looks up `S_<object-template-name>_<sound-name>` with sound class ID 9986001,
requires an engine template and a nonempty filename, then passes that template and
the owning object to `BfSound::createSound`. That facade assigns the sound's owner
and obstruction evaluator. The same factory serves vehicle, projectile, soldier,
and ambient object sounds.

The original BF2 1.50 x86 BF2.exe client implementation is `0x624A20` (image MD5
`4c843dcee79388341f72c3a7adb1ace6`); weapon initialization at `0x608920` calls it for
all eleven weapon sound slots. Its four `impl()` calls and
two filename reads are preserved, as are the runtime dedicated-mode and missing
sound-facade guards. The symbolized dedicated ELF implementation at `0x081F2760`
returns null without looking anything up. The shared source now selects that
server implementation only for `DEDICATED=1`. Previously, the server body was also
used in the client, leaving every weapon sound slot empty before firing reached
the audio engine. This recovery changes no interface layout or sound-file format.

`Code/Tests/weapon_sound_runtime.cmd <isolated-output-directory> client` checks the
factory guards, owner forwarding, all eleven weapon sound slots, single and
automatic third-person firing, stopping, and sound release. Run it again with a
different output directory and `dedicated` to check the server's null factory.
The fixture copies the selected production bodies verbatim under their real
headers and compiles the actual weapon sound component; its surrounding observers
provide no audio device or game world. `BF2_TEST_FACTORY_SOURCE` can select a
pre-fix source snapshot to verify that the client regression fails before the fix.

Factory and weapon-component checks establish sound creation and playback dispatch;
they do not establish audible output from a live joined client. Device playback,
first/third-person mixing, and repeated gameplay still need runtime validation.
Existing `bfsound-debug-draw` frontiers are outside this factory and remain unchanged.

Soldier templates create their `Death`, `Revive`, `Swim`, `Swim_Still`, and
`Swim_Sprint` sound children through `SoldierTemplate::templateInitialize`
(BF2.exe `0x5F4830`, ELF `0x081B8C20`) and the static
`SoldierSound::initTemplates` helper (BF2.exe `0x7E2AD0`, ELF `0x083F2EC0`).
This runs after the manager assigns the soldier template's name and before its
`.tweak` file activates and configures those children. Without this hook, stock
`ObjectTemplate.active S_<soldier>_Death` selects a missing template and the live
soldier's Death sound lookup returns null. Both images create the children;
dedicated sound creation still returns null. The death dispatch and first-person
damage gate remain those of retail (`Soldier::handleMessage`, BF2.exe `0x5A7570`,
ELF `0x081ACD90`; `SoldierSound::die`, BF2.exe `0x7E2CB0`, ELF `0x083F35E0`).

Root vehicle initialization also creates a `GroundContactSound` when the client is
not in dedicated mode and the template's `groundContactVolume` exceeds float epsilon
(BF2.exe `0x5FA887..0x5FA93A`, constructor `0x5F4CB0`). Its fixed sound node borrows
the vehicle; the vehicle owns the carrier and stops/releases its sound before deletion.
Wheel discovery follows allocation. The existing tick and material-change paths
(`0x5FBCDB..0x5FBD67`, `0x5F4DD0`, `0x5F4D20`) drive the retained null sound or
the material-pair sound through the proportional controller. Dedicated images leave
the carrier null.

## Client material effects

`Soldier::triggerFootStepEffect` follows BF2.exe `0x5A7A50..0x5A7C08`: standing
soldiers with an occupant outside first-person view create the material-pair effect
within the camera distance limit (20 units by default). The transform includes the
configured vertical offset and velocity lead; effect bundles receive a collision plane
before transform assignment, initialization and message 0. Dedicated images retain
the empty body. `EffectBundleTemplate::preCache` also warms child mesh-particle
geometries after the base call on its first client precache (`0x642250`).

Aircraft downwash remains missing: `AirDraftEffectBundleTemplate` is reconstructed,
but its client runtime `AirDraftEffectBundle` is absent. The retail factory at
`0x646880` constructs a 0x1D0-byte x86 object through `0x646400`; its runtime
layout, virtual overrides and material/effect/sound ownership must be recovered
together before enabling this factory. The current dedicated null factory does
not establish client downwash support.

## Content and the console

Aircraft downwash is created by the client `AirDraftEffectBundleTemplate` factory;
dedicated factories return null. Its runtime selects material-cell effects and
sound using the aircraft engine and a downward object/terrain query. See the
[air-draft recovery contract](recovery-notes/Game/air-draft-effect-bundle.md)
for the client vtable, lifecycle, stock aircraft references and validation limits.

`.con` files are executable engine configuration. The console binds command names such as
`ObjectTemplate.*`, `Object.*`, and `sv.*` to typed engine operations. Templates describe
shared object definitions; instantiated world objects carry live state. Registration and
load order determine which names and resources a script can use.

`FileManager` resolves loose paths and mounted archives. The working directory, mod path,
archive order, and level directory all affect lookup. Diagnose a missing command or resource
at its registration or loading boundary before adding fallback behavior. Python game modes
use the embedded interpreter and native bindings; a fixture that bypasses those scripts
does not validate production content loading.

The following inspection and administration commands are registered in both client and
dedicated images. Game-logic commands require a live game logic; message commands also
require its voice-message owner. Player edits require an active player.

| Commands | Arguments and result |
| --- | --- |
| `pythonHost.sendCommand` | One command string and up to eight optional strings; dispatches Python event `ConsoleSendCommand` (33) as `(command, argsTuple)`. Omissions are empty strings; only trailing empty arguments are trimmed. Access level 2; returns Boolean true even when no handler runs. |
| `squadManager.changeSquadName` | Squad name and Boolean; returns void. Renames the local human player's squad; an absent local player or squad manager does nothing. The Boolean is accepted but ignored; the manager receives false. A refused rename posts HUD event 15. Access level 2, category 0; requires the global game and a live player manager. |
| `squadManager.listSquadMembers` | Team and squad integers; returns members in player-manager order as `id:<id> <name>` lines, each ending in a newline. Access level 2, category 0; requires the global game and a live player manager. |
| `squadManager.listSquads` | Team integer; returns existing squad IDs 0 through 7 as `id:<id> <name> is private.` or `id:<id> <name> is public.` lines, each ending in a newline. An absent squad manager returns an empty string. Access level 2, category 0; requires the global game. |
| `gameLogic.getTeamLanguage`, `gameLogic.getTeamName` | Team number; returns the configured string. |
| `gameLogic.defaultLevel` | No argument reads the name; one string sets it. |
| `gameLogic.setKit` | Team, kit type, kit template, soldier template. Types accept `KTMedic`, `Medic`, or `5` (the seven stock types); unknown tokens select the no-kit sentinel 13. |
| `gameLogic.teleportTo` | Slash-separated position; moves the local human player's controlled vehicle and resets its cached physics node when present. Requires `GSIsServer` and live game logic. |
| `gameLogic.teleportCameraTo` | Slash-separated position; moves the local human player's current camera object. Requires `GSIsServer` and live game logic. |
| `gameLogic.getNumberOfTickets` | Unsigned team number; returns current tickets for team 1 or 2, or zero for another team. Requires `GSIsServer` and live game logic. |
| `gameLogic.messages.playRadioMessage` | Team, message name, voice name, filter name; returns the message result integer. |
| `gameLogic.messages.playHelpMessage` | Help key; returns the message result integer. |
| `player.pickup` | Item template name; adds it to the controlled soldier on a server. |
| `player.moveTo` | Slash-separated position; moves the root vehicle on a server, or returns a refusal string. |
| `Player.create` | Player class name and player name; creates with automatic ID `-1`, returning the player pointer. |
| `Object.template` | Read-only template pointer of the active object. |
| `ObjectTemplate.writeBinary`, `ObjectTemplate.readBinary` | One filename; accepted no-ops, as in the shipped game. |
| `kitTemplate.getType` | No arguments; returns the active AI kit's `KTSpecops` through `KTExtended6` token. Out-of-range values print no token. |
| `reservedSlots.configFile`, `reservedSlots.load`, `reservedSlots.save` | Read/set the filename, load its console script, or save the current list; load/save return Boolean results. |
| `reservedSlots.addNick`, `reservedSlots.removeNick`, `reservedSlots.clear`, `reservedSlots.list` | Add/remove one nickname, clear, or return newline-separated nicknames. Administrator access level 1. |
| `sv.maxConnectionType` | No argument reads the connection-class name; one name sets the ceiling. Names are case-sensitive: `CTConnecting`, `CTModem28Kbps`, `CTModem56Kbps`, `CTCable128Kbps`, `CTCable256Kbps`, `CTLanT1` (values 0 through 5). Unknown names, including numeric tokens, select 6 and print `CTNumTypes`. Administrator access level 1. |

For example, `sv.maxConnectionType CTCable256Kbps` in `serversettings.con` sets the
ceiling to class 4. Query `sv.maxConnectionType` to read the name back. Teleport commands
need a local human player; a headless server normally has none, so they leave the world
unchanged there. Camera teleport uses the current camera, including a vehicle camera.

`pythonHost.sendCommand` requires the host pointer and an already running interpreter.
The host's normal initialization and round-state gates still decide which handlers run;
its Boolean reply does not report whether a Python command succeeded. The stock
`bf2.stats.fragalyzer_log` handler accepts `pythonHost.sendCommand falog 1` to enable
Fragalyzer logging for the current round, or `falog 0` to disable it. With that handler
initialized and the round playing, the Python output reports the logging state and
announces its chosen log filename when enabling.

Reserved nicknames load during `GameServer::init` from
`<GSModDirectory>/settings/reservedSlots.con` (normally `mods/bf2/settings/reservedSlots.con`),
resolved by the virtual filesystem. `+rsconfig <file>` overrides that filename. Each file
line uses `reservedSlots.addNick "Nickname"`; `reservedSlots.save` writes that same syntax.
After initialization, an administrator can read `reservedSlots.configFile` and
`reservedSlots.list` to verify the selected file and loaded names. Loading merges with the
current list; clear it explicitly before reloading if replacement is intended. Addition and
reservation checks strip everything through the first space as a clan prefix; removal uses
the stored spelling exactly. Matching and duplicate suppression are case-sensitive.

The shared console catalogue includes these settings and inspection commands in both game images:

| Object | Commands and properties |
| --- | --- |
| `Console` | `workingPath`, `echo`, `echoErrors`, `maxLineSize`, `maxHistorySize`, `maxCommandHistorySize`, `useRelativePaths`, `allowMultipleFileLoad`; `listVariables [prefix]` (default `v_`), `listConstants [prefix]` (default `c_`), `run <file> [arg1..arg4]`, `bindKeyToConsoleScript <1..12> <script>`; registry reflection through `getArgumentTypes`, `getEnumName`, `getRange`, `getReturnType`, `getDelta`, `hasBundledType`, `getTypeDescription`, `isDynamic`, `isSetGet`, `hasRange`, `getCategory`, `getActiveMethods`. |
| `FileManager` | `addPath <path>` makes the path the first search/write path; `fileExists`, `deleteFile`, `moveFile <from> <to>`, `copyFile <from> <to>`, `mountArchive`. Copy forwards the shipped `true` third argument; the POSIX filesystem refuses an existing destination and creates a hard link. |
| `InputDevices` | `setInvertAxis <device> <axis> <bool>`, `setAxisScale <device> <axis> <float>`; unavailable devices return false. |
| `ObjectTemplate` | Particle `nearCullingEnable` (ordinary sprites only) and `isStaticSystem` (sprite, mesh, trail and NSA systems), access 2/category Emitter. `setToolTipType <name>` uses full `TT*` or short case-sensitive names; unknown tokens, including numeric tokens, select 22. `vehicleCameraShake` accepts `CstNoShake`, `CstExplosion`, `CstMachinegun`, `CstHitShake`, `CstDieShake` and decimal -1..3. `grip` accepts eight `PGF*` bit names, their `c_` forms and numeric normalization; its published range remains 0..7 without clamping. `zoom.changeCameraViewMode` and `zoom.startCameraViewMode` accept `CVMFirstPerson`, `CVMInside`, `CVMNose`, `CVMChase`, `CVMFrontChase`, `CVMFlyBy`, `CVMTrace`, `CVMExternTrace` and single-digit numeric normalization. `listTemplateClasses` lists registered classes. `getTemplate [index]` returns a bundle child's template name, defaulting to the child selected by `addTemplate`/`setActiveTemplate`; invalid indices return empty text. `getHeatBarType` requires an engine template and advertises an int result, but retail publishes no value. |
| Inspection | `physics.listProperties`; `CollisionManager.getNameOfObjectInFocus`, `identityRotateObjectInFocus`, `buildDebugCollisionMeshes`; `Object.setAsSkyLight` (lights only), `Object.getLightSourceMask`, `Object.addAbsolutePoint` (area templates only, no arguments; appends negative absolute x/z and creates a marker when `DirectionArrow` exists); `Player.listPlayerClasses`; `Terrain.refreshTerrainSampling`; `Editor.getActiveObjectTemplate` and `Editor.getActiveObject` return borrowed name/pointer proxies. |

Special Forces templates also expose `ObjectTemplate.MaxZiplineLength`, `EntryRadius`,
`collisionStartRadius`, `heightOffset` and `distanceCannotEnter` on zipline templates;
`ropeScaleFactor` on zipline-rope templates; and `MaxRopeLength` plus integer `NumberOfLinks`
on grappling-rope templates. These shared properties feed the existing placement, entry,
link scaling and rope initialization paths. Content's `setMaxZiplineLength`, `setEntryRadius`,
`setMaxRopeLength` and `setNumberOfLinks` spellings use the console's existing `set` aliases.
`ObjectTemplate.shakeFactor` writes the player-control template value read by
`PlayerControlObject::getShakeFactor`. These properties have access 2/category 15.
`ZiplineTemplate::setZiplineSoldierHeight` also updates the shared Vars handle
`zipline-soldier-height` before its own member, so existing riders see the new height.

Player-control templates expose `ObjectTemplate.seatPosition` and `seatRotation` as
Vec3 properties (access 2/category 15). Rotation writes preserve translation; the rotation
getter retains the shipped cache initialized by its first call, even across templates.
The same category exposes `listenerObstruction`, `groundContactVolume` and
`damagedAmbientSoundLimit`. All three clamp writes to 0..1; PE maps NaN to zero,
while ELF retains it, and both preserve signed zero. Listener obstruction is read by
the local occupant's environment and soldier sound; the damaged limit selects the
damaged ambient sample by the root object's health fraction. In the client image,
a root vehicle outside runtime dedicated mode allocates its ground-contact sound
carrier only above `FLT_EPSILON`, then discovers its drive wheels. The dedicated image
does not allocate that carrier. Stock HMMWV and Vodnik templates author ground volume
1 and obstruction 0.45/0.4; the AH1Z authors obstruction 0.5 and damaged limit 0.15.

`Object.setTeam <int>` is an access-2/category-0 static method for the active object
only when its template is an object-spawner template. It dispatches the live spawner's
`setTeam`; the separate `Object.team` property initializes both its start and live team.
`LocalPlayer.firstCommanderSelectDelay` and `commanderSelectDelay` are access-2/category-0
dynamic float properties, available only with `GSIsServer` true and a published squad
manager. They keep the first/subsequent election defaults 15/10 and accept raw values
without clamping. These bindings are shared by both game images.

`ObjectTemplate.respawnDelay` has separate receivers for destroyable objects, dynamic bundles
and destroyable-window bundles. `GameLogic` copies their values into spawners; the dynamic
bundle's default `-1` selects no respawn. Window state updates also read their template delay.
The dynamic-bundle binding has category 14; the window binding has category 0.

`ObjectTemplate.LightAffectionFactor` accepts sprite, mesh, trail and non-screen-aligned
particle templates, excluding emitters. It forwards to the renderer descriptor and then
refreshes derived state, which selects the authored factor or the light manager's default
for negative values. Its access is 2/category 19. Special Forces tear-gas effects
author `0.2`; underwater effects author `0`. The setter tolerates an absent renderer
descriptor; the getter requires one, including in a dedicated image.

`Console.run` returns the script-open status as text (`0`/`1`); script output is discarded.
The particle flags forward to renderer descriptors: near culling drives the sorted sprite
collector's distance/age fade and alpha rejection; static systems enable initial simulation
prewarming. Additive collection keeps its separate shipped behavior. The flags do not create
renderer descriptors in a dedicated image. `HeightmapCluster.compileWaterMeshesIntoHeightmap`
remains unpublished while its absorbed-water-object state update is unresolved.

`Code/Tests/console_bindings_runtime.py` exercises the registry, real console, loose-file
operations, enum conversions, template selectors and forwarding into typed descriptor observers.
It covers rope content aliases, shared rider height, seat transforms and their cached getter,
sound property clamps, vehicle shake reads, independent respawn receivers, spawner team dispatch,
election-delay availability and particle light write/refresh ordering across all four accepted
classes without a window or game startup. The ground-contact check copies the production
allocation tail verbatim and uses a retained null-sound observer; it exercises the guards and
real carrier constructor, not the preceding world initialization or audible output.
It requires a completed macOS renderer-enabled client build;
particle-manager construction, particle collection, rendered pixels and a joined player are untested.

The client `BFGameEffector` keeps the recovered deferred material-effect consumer
(BF2.exe `0x81FC40`). The append of a copied position and two material IDs
(`0x81FCF6..0x81FD25`) is withheld at the `ai-material-effect-drain-caller` frontier
until the frame caller is recovered and validated: without a drain, production records
would accumulate for the entire session. This is an owner-requested
`[CHOSEN, not recovered]` omission; immediate effect dispatch remains live.
The consumer uses the AI update clock, checks its budget after each dispatch,
and clears the entire queue even when the budget stops the walk. Both dedicated images
have empty queue/consumer bodies. A per-frame caller has not been established in the
inspected BF2.exe or AIDLL.dll; ordinary bot reach and visible effects remain unverified.
`ServerGameLogic::uPlaying` skips ticket processing on client lightmap-rendering nodes
(`0x4E0A24`); the dedicated images retain their state/enable gate without that node test.
The rendering-node startup option remains unregistered.
`Code/Tests/ai_effect_queue_runtime.py` checks the linked client and dedicated effector
with typed material, game-logic and clock observers, including budget exhaustion,
equality, unordered clock values and copied queue records seeded through a test-only
friend. It also checks that repeated immediate effects do not allocate or grow the
queue. It calls the consumer directly and does not establish a frame-scheduling
path or audible output.

### The `openbf2` console extension

`openbf2.*` is an OpenBF2 extension, not a recovered command; no retail image defines it. It is
`[CHOSEN, not recovered]` under [issue #39](https://github.com/B2BF/OpenBF2/issues/39).
`Game/Main/OpenBF2Console.cpp` registers it from `ModuleBF2::init`.

`openbf2.addModDll <file>` loads a native module at run time: either an OpenBF2 plugin that exports
`initDll(IClassManager*)` (initialized through the same handshake as `Core/PluginDll.cpp`, a false
return unloads it) or a module that does its work in `DllMain` and exports no `initDll`, which is
kept loaded. A bare name resolves to `.\<file>` on Windows and `lib<stem>.so` on Linux. The command:

- exists only in the dedicated-server image; it compiles out of the client and editor, so a
  downloaded map cannot reach it on a player's machine;
- refuses any call dispatched from a mounted-archive script (`OldConsole::isDispatchingArchiveScript`),
  so bundled map or mod `.con` files cannot load native code. Operator callers — a loose server
  `.con`, rcon, Python, or the server console — are allowed.

A module's `DllMain` runs on load, so an integration that patches the host from `DllMain` takes
effect without an `initDll`. bf2hub is such a module: its `DllMain` patches the engine's `dice_py`
import slots (`Py_InitModule4`, `PyImport_ImportModule`) so that its `bf2hublib` and `bf2.bf2hub`
Python modules are injected when the engine registers its `host` module and imports `bf2`. This only
takes effect when the module is loaded **before** Python initialization — load it from
`ServerSettings.con` or `Init.con`, which run before `GameServer::init` calls
`pythonHost()->initialize()`, not from a map script that runs afterward.

This loader does not itself reroute sockets. bf2hub's GameSpy/master-server redirection relied on an
install-time patch of the host's import table that a runtime load does not reproduce; select the
provider with `+onlineProviderConfig` instead (see [Online services](building.md#online-services)).

## The editor

The editor targets this engine's 1.50 content contract. Its application, UI, command history,
and editing modules are new code. It shares engine code with the client and isolates
editor-specific changes in shared translation units with `BF2_IMAGE_EDITOR`.

Lightmap generation is shared renderer code recovered from the 1.50 RendDX9 image: the terrain's
light sampler (`Geom/TerrainLightSampling.cpp`), the static-mesh light sampler
(`Geom/LightmapGeneration/MeshLightSampling.cpp`) and the generator (`Geom/LightmapGeneration/`).
1.50 never creates the generator. Only the editor's renderer flavour (`BF2_RENDDX9_EDITOR=1`)
publishes the 2006 editor's `lightmapGeneration` console object over it, using the 2006 binding
names (`init`, `traceTerrain`, the settings and `light.*` rows). This is chosen behaviour, not
recovered 1.50 behaviour. Both traces are recovered: the intensity trace (the shipped default) and
the colour trace (`enableIntensityLightmaps 0`), which writes RGB lightmaps from what each light's
views see over the ambient colour. The colour trace has not yet been compared with a retail-editor
golden.

An object is traced from its mesh's precomputed sample grid: `<template path>/Meshes/<template
name>.samples` for geom 0, lod 0, otherwise `.samp_<geom><lod>` (the SMP1 and SMP2 layouts and an
older one without a magic word). No stock archive ships these files; the art exporters write them.
A template with `lightmapOverride` caps the grid at its `setLightmapSizeAll` size. The sampler writes
the object's lightmap, dilated and converted to DXT1 (DXT5 when `lightmapFormat` is not 1) with mips,
as `<Editor_LevelSavePath>/<GSLevelDirectory><GSLoadLevel>/Lightmaps/Objects/<template>` plus the
position suffix the game's loader expects. Outside the renderer's editor mode the game reads object
lightmaps only from the level's atlas (`Lightmaps/Objects/LightmapAtlas.tai`). The 1.50 rows `StaticMeshRenderer.loadAllLightMapFilesDLL
<directory> <extension>` (one atlas, named `LightmapAtlas`) and `StaticMeshRenderer.loadAllLightMapFiles
<directory> <extension>` (numbered batches `LightmapAtlas0`, `LightmapAtlas1`, ...) pack a directory's
lightmaps through TextureAtlasBuilder.dll, reading and writing under `Editor_LevelSavePath`; the
directory is the level's object-lightmap directory relative to that root
(`<GSLevelDirectory><GSLoadLevel>/Lightmaps/Objects/`) and the extension `.dds`. Both
bind in the native and editor flavours, as do `GeometryTemplate.setLightmapSizeAll` and
`Object.geometry.GenerateLightmapForThisInstance`; the retail-compatible flavour publishes no console
rows. The editor mounts `bf2editor/LightmapSamples*.zip` at `Objects` when the object archives are
mounted, as the retail editor's setup did. Its object trace starts from the level's own object
lightmaps - the level's atlas cut back into single lightmaps (`Framework/LightmapAtlas`), then the
single lightmaps the editor kept in `Editor/Lightmaps/Objects` - so the re-packed atlas keeps every
object the trace does not reach; stock levels ship only their atlas. The re-packed atlas goes into
`Lightmaps/Objects` and the traced single lightmaps into `Editor/Lightmaps/Objects`.
The game's 1.50 `StaticMesh.fx` comments out its include of the lightmap-generation techniques the
trace draws static meshes with as occluders; the editor's flavour compiles it with that include
(`RendDX9/Effect.cpp`), as the retail editor's own shader did, so terrain lightmaps carry the
objects' shadows. This is chosen behaviour for the editor; the game images are unchanged.

Editable roads are shared renderer code recovered from the 1.50 RendDX9 image (`Geom/RoadEditable.cpp`,
`Geom/RoadTemplate.cpp`, `Geom/RoadEditableRenderer.cpp`, `GeomMisc/EditorSpline.cpp`,
`GeomMisc/PerlinNoise.cpp`); 1.50 binds no console rows for them. Their interfaces are declared in
headers the renderer and the editor both include (`Geom/IRoadEditable.h`, `Geom/IRoadTemplate.h`,
`Geom/IRoadEditableRenderer.h`, `GeomMisc/IEditorSpline.h`, `GeomMisc/IPerlinNoise.h`), whose strings
are the renderer's host string in its image and `std::string` in the editor's. The editor's road
module (`App/RoadNetwork`) makes the objects through the class manager, keeps them alive (a road holds
no reference to its spline, template or noise) and implements the retail editor's road rows over them.
In the editor's renderer flavour `RoadEditable` takes its buffers out of the buffer manager when it
drops them: the shipped body releases a dropped vertex buffer a second time on its next rebuild, which
the retail Memory.dll heap absorbed. This is chosen behaviour for the editor; the game images keep the
shipped body.

The editor's `App/` layer owns the window, engine host, editing session, and renderer
integration. `Framework/` supplies command history, selection, settings, file readers and
writers, and source-control interfaces. Editing tools live with the application. Its framework
checks run without starting the engine, while viewport and content tests need a game
installation and the rebuilt editor renderer.

The editor uses Dear ImGui and ImGuizmo for its interface. Its renderer and texture atlas
DLLs are built separately from the client's plugin outputs. The source-control interface
registers `NullSourceControl` by default; editing does not require Perforce. Console
bindings expose editing operations through the same session used by the UI; see the
[editor console](editor-console.md) for commands and replay behavior.

Retail-compatible files and `.con` semantics are the editor's compatibility boundary.
Compare output with retail-editor golden files. An unfinished operation reports its failure;
it must not claim to have saved or compiled content. Follow the [editor build steps](building.md#editor)
for its dependencies, checks, and output directories.


Every editor save is planned before anything is written (`Framework/SavePlan`): each part of the
level - its object and settings files, the gameplay layers, templates, materials, animation systems,
descriptions, painted maps, compiled terrain, overgrowth, roads, lightmaps - adds the files it would
write and the step that marks it saved, without writing. The plan can be shown first (File > Save,
`EditorLevel.preview`, with a line diff of each text file). Writing it puts each new file beside its
target, copies every file it replaces into the level's backup folder
(`bf2editor/Backups/<mod>/<level>/<date and time>/`, with a manifest), and only then replaces the
targets; if one cannot be replaced, the ones already replaced are put back, so a save never leaves the
level half written. A part that cannot be saved (a file from a mounted archive, one source control
refuses) is reported and left unsaved; the other parts are written. A backup can be put back with
every save after it (`EditorLevel.restore`), which is itself a backed-up save. The retail editor wrote
each file as it went and kept no backup; this is chosen behaviour for the editor.

The HUD editor is an addition; the retail editor left HUD scripts to a separate tool. Unless started with
`+hud 0`, the editor runs the client's own HUD start-up: the HUD's modules, the `HudManager` singleton the renderer
takes its paint hook from, and `BF2HudManager::init`. The renderer then updates and paints the HUD
itself. `App/HudEditor` decides whether the HUD is drawn through the renderer's retail
`renderer.drawHud` row: in soldier mode, or while the panel previews it. A preview holds the
selected part's show variables and runs the HUD's widget update, which the game runs only for a player
in a vehicle. `Framework/HudSetup` keeps the scripts `HudSetupMain.con` runs as lossless lines and
tracks hudBuilder's selection across them, so every row belongs to its node. An edit is a line edit
through the undo history. A row the builder can safely take again is replayed on the live HUD; a
failing builder row ends the process, so the editor checks the live object and the row's words first.
`Bf2TransformNode` reports where each node is updated, so the view's outline follows the game's own
layout; that hook is compiled only into the editor's copy of the file. The save rewrites the menu
archives that hold the scripts (`Framework/ArchiveRewrite`). The engine keeps its archives open, so a
save part may unmount them just before the save plan replaces its targets and mount them again
afterwards.
