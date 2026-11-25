# Retail fixes

OpenBF2 follows the shipped behavior of Battlefield 2 1.50, quirks and documented defects
included ([Development guide](development.md)). This guide lists every place where it
deliberately corrects behavior that is wrong in retail, and states the rule for adding one.

## What counts as a retail fix

Retail behavior is incorrect when the shipped game does something its own design evidently did
not intend:

- a crash, a hang, a memory error or a leak;
- a security defect, such as trusting an unchecked length from the network;
- a wrong result, or a wrong or misleading message to the player or the operator;
- lost input, lost data, or a setting that does not take effect the way the game says.

A retail fix corrects that behavior and changes nothing else. The following are not retail
fixes and are not listed here:

- improvements and features, including support for systems retail never ran on, such as other
  operating systems, high-DPI or wide displays and newer hardware;
- different defaults and changes of taste;
- every change to an ABI, protocol or file-format contract;
- guards, bounds and initialization in code that has no retail counterpart, such as the ports'
  platform layers.

The first three follow the issue workflow in the [Development guide](development.md). In
recovered code, a guard, bound or initialization that retail lacks is not covered by the last
item: it is a retail fix, falls under the [memory-safety exception](macos-port.md#port-rules)
of the native ports, or follows the issue workflow.

## Rule

- **Client fix.** It needs no separate issue.
- **Server fix.** It needs no separate issue when it stays backwards compatible for retail
  clients: an unmodified retail 1.50 client connects, loads, joins, plays, disconnects and
  reconnects as it does against a retail server, and nothing it sends, receives or relies on
  changes. Any other server fix is discussed in an issue first.
- **Shared code.** A fix in code that the server also runs is a server fix for this rule.
- The pull request that makes the fix adds its entry to this guide.
- Beside the code, mark the fix `[CHOSEN, not recovered]` and name its entry here in place of an
  issue.

## Entry format

Each entry is a heading that names the fix, followed by these fields:

- **Retail:** what 1.50 does, with the evidence: the image and address, or how to reproduce it on
  retail.
- **OpenBF2:** what it does instead.
- **Applies to:** client, server or both, and the platforms.
- **Retail clients:** for a server or shared fix, why an unmodified retail client is unaffected.
- **Source:** the files, and the issue where one exists.

## Client

### Shared zipline ride sound detaches from a destroyed container

- **Retail:** `BfSound::getSound(name)` resolves the name to an ID and returns the stored sound
  pointer without creating an instance or adding a reference (`BF2.exe` `0x7DE1F0`,
  `0x7DF8A0`, `0x7DF7D0`). `addSound` stores one instance per name, releasing a replaced instance
  (`0x7DF980`), and BfSound releases the game sounds on destruction (`0x7E15B0`–`0x7E15D5`).
  Every ZiplineContainer binds this shared `S_ZiplineUsed` sound to its own ObjectSoundNode and
  slide-speed float (`0x5DAD72`–`0x5DADDA`). Its destructor stops playback but leaves those
  bindings (`0x5DA0A3`–`0x5DA0E5`); another container's successful entry plays the same sound
  (`0x5DB361`–`0x5DB37B`), allowing reads through the destroyed container's pointers.
  Stock content does not define `S_ZiplineUsed`; the defect requires content that defines it.
  A retail game reproduction has not been run.
- **OpenBF2:** When the destroyed container is still the sound's parent, clears the parent and
  both evaluator bindings before the container's storage is freed. A successful entry binds the
  sound to the riding container and its speed before playback. The sound remains owned by
  BfSound, and the existing playback and stop conditions are preserved.
- **Applies to:** Client; all platforms. No ABI, protocol or file-format changes.
- **Source:** `Code/BF2/Game/Objects/Game/ZiplineContainer.cpp`,
  `Code/Tests/client_effects_runtime.cpp`

### New profiles get the audio quality the memory check intended

- **Retail:** The audio-defaults initializer (`BF2.exe` 1.50 x86 `0x4B0C10`) calls the
  memory helper at `0x4B0C3F`, compares its result with 768 MB at `0x4B0C44`, and chooses
  "Low" below that threshold or "Medium" otherwise. The helper (`0x4B0B80`) calls
  `GlobalMemoryStatus` at `0x4B0BAC` through import `0x87F0C0`, reads its 32-bit
  `dwTotalPhys`, adds `0x7FFFF` at `0x4B0BB5` and shifts right by 20 at `0x4B0BBA`, then
  rounds up to a multiple of four megabytes. On machines with 4 GB or more the legacy API
  saturates at `0xFFFFFFFF`; the addition wraps and reports 0 MB, selecting "Low".
- **OpenBF2:** Reads the full physical-memory size with `GlobalMemoryStatusEx` (`ullTotalPhys`)
  on Windows, `sysctl hw.memsize` on macOS and `sysinfo` on Linux. Conversion and rounding
  use 64-bit arithmetic and clamp to the `int` range. This memory-read fix preserves the
  rounding and 768 MB threshold, so the low-memory guard no longer mistakes a modern
  machine for a low-memory one. The separate `[CHOSEN, not recovered]`
  [client audio defaults](architecture.md#client-audio-defaults) select hardware and High
  above that guard, replacing retail's software and Medium defaults. Existing profiles
  keep their saved settings when `Audio.con` is loaded; explicitly restoring defaults
  uses the corrected memory check and the new defaults. No quality row, profile format
  or network contract changes.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Game/AudioSettings.cpp`, `Code/Tests/audio_settings_runtime.cpp`

### Unsupported stored video mode falls back to the closest supported one

- **Retail:** `VideoOptions::setDynamicOption` accepts only entries of the enumerated resolution and
  antialiasing lists. A miss, such as a stored refresh rate the display no longer offers, raises the
  Error "Unknown DynamicOption value" (VideoOptions.cpp, line 234) and returns false (`RendDX9.dll`
  `0x1000FBE4`). A reported Error ends in `int 3` and a write to address 0 (`0x10074811`). Per
  [#4](https://github.com/kr4uzi/OpenBF2/issues/4) a fullscreen client then dies without a message.
- **OpenBF2:** A miss selects the closest listed entry. Resolution: the same size with the nearest
  refresh rate, otherwise the nearest size; on a tie the lower rate. A value that cannot be parsed
  uses the desktop mode. Antialiasing keeps the current entry, otherwise the first. One log line
  names the replacement. Retail's stop remains when nothing can be chosen. The fix does not rewrite
  the stored profile value.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/RendDX9/VideoOptions.cpp`; [#4](https://github.com/kr4uzi/OpenBF2/issues/4)

### Depth-stencil surface saved for the multisampled overlay passes is released

- **Retail:** With a multisampled back buffer, `Renderer::renderFrame` saves the depth-stencil
  surface (`GetDepthStencilSurface`), removes it for the overlay passes and restores it, but never
  releases the saved reference (`RendDX9.dll` `0x100306D2` to `0x10030AFA`): one leaked surface
  reference per frame. On the rebuilt macOS client this makes the quit fail with
  "Direct3DDevice release unsuccessful" ([#35](https://github.com/kr4uzi/OpenBF2/issues/35)); on a
  retail install that symptom is unconfirmed.
- **OpenBF2:** Releases the saved surface right after restoring it, so the device's final `Release`
  reaches zero and quitting through the game's QUIT button after a map exits with status 0.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/RendDX9/BF2Render.cpp`, `Code/Tests/macos_metal_device/msaa.inc`;
  [#35](https://github.com/kr4uzi/OpenBF2/issues/35)

### Screenshots are saved opaque instead of with the back buffer's alpha

- **Retail:** `renderer.dumpScreen` (Print Screen) copies the back buffer into an A8R8G8B8 scratch
  surface (`RendDX9.dll` `0x1000521A`, `CreateOffscreenPlainSurface` with format `0x15`) and saves
  it as `screenNNN.png` and `screenNNN.jpg`. A 32-bit fullscreen back buffer is A8R8G8B8 too
  (windowed mode takes the desktop format), and its alpha is render-pass data. On High terrain,
  the low-detail pass for every terrain node above LOD 0 (`Hi_Terrain` pass 3, `Shared_PS_LowDetail` in `Shaders_client.zip`
  `TerrainShader_Shared.fx`) returns the light-accumulation alpha, which is the dynamic-shadow
  term written by pass 7 (`DirectionalLightShadows`). The other branch of the same shader returns
  alpha 1. Shadowed terrain beyond the full-detail nodes therefore has alpha 0. The `.png` keeps
  it: a viewer shows black holes, shaped like the shadows of bushes and palms, in the middle
  distance. The `.jpg` of the same frame is correct.
- **OpenBF2:** The capture stages X8R8G8B8, so the `.png` is opaque and has the same colour as
  the `.jpg`. The rendering and the `.jpg` are unchanged.
- **Applies to:** Client; all platforms (Linux has used this since
  [#36](https://github.com/kr4uzi/OpenBF2/issues/36)).
- **Source:** `Code/BF2/RendDX9/RendDX9Draw.cpp`

### Terrain colour passes no longer leave black patches in the middle distance

- **Retail:** `Terrain::drawMain` writes the terrain's depth in pass 0 (`ZFillLightmap`, ZFUNC LESS)
  and `Terrain::drawFinal` then draws the colour passes (`LowDiffuse`, `FullDetail`,
  `FullDetailMounten`, `FullDetailWithEnvMap`) with ZFUNC LESSEQUAL and no depth writes,
  `FullDetail` additively onto a cleared target. The two kinds of pass use different vertex shaders
  in `TerrainShader_Hi.fx`, and they spell the position differently: only the `FullDetail` family
  adds `vScaleTransY.zw`. Where a colour pass lands a few depth units behind the depth pass 0
  wrote (the exact source of the difference has not been isolated), that pixel fails the test and
  keeps the cleared back buffer: flat, exactly black, ragged patches
  on the ground in the middle distance, which change with every small turn of the view.
  Retail shows them on DX10-class and AMD GPUs ("flickering black hexagons"), and OpenBF2 shows
  them on macOS (Metal). Turning `D3DRS_ZFUNC` to ALWAYS for these draws removes them. On an Intel
  Mac the gap lies between 1e-7 and 1e-6 in D3D9 depth units.
- **OpenBF2:** Each terrain node drawn by `drawFinal` carries `D3DRS_DEPTHBIAS` = -1e-6 within
  60 m of the camera (D3D9 depth units, about 17 steps of a 24-bit depth buffer). The colour passes
  now run immediately after the depth and light pass and the water in
  `Terrain::renderWorldPassBegin`, before world objects, overgrowth and undergrowth.
  Retail runs `drawMain` and `drawWater` in `renderWorldPassBegin` (RendDX9 PE `0x100E0E50`),
  then draws objects before `renderWorldPassEnd` (`0x100E10C0`) calls `drawFinal` and
  `drawSurrounding`. With the bias, additive terrain colour could land on object pixels within
  the bias of the ground, leaving thin bright lines at bush and tree bases and grass that has
  sunk flat at its fade distance. Drawing colour first lets objects cover the finished terrain
  with their normal depth test against the unbiased depth passes; colour passes never write
  depth. The cost is computing terrain colour also where objects later cover it.
  `drawSurrounding` stays after world objects. Beyond 60 m the bias is
  scaled by `(60/d)^2`, where `d` is the distance from the frame's terrain camera position to the
  nearest point of the node's world bounds. A constant depth-buffer bias corresponds to a world
  distance that grows roughly with distance squared. Scaling keeps approximately the world-space
  offset used at 60 m; the draw order prevents terrain colour from adding to world objects.
  Measurements on an Intel Mac: -1e-7 leaves smaller black patches, -5e-7 still leaves some white
  specks, and -1e-6 removes the middle-distance black patches; slope -1 with constant 0 is worse.
  Every colour pass uses the node's bias; lower-LOD batches merge only nodes with equal bias,
  while the debug detail merge uses the first node's value. Redundant state sets are skipped
  across patches and layers for the frame. The bias is reset to the world value 0 after them, so
  pass 0, pass 7, the light passes, the surrounding ring, roads, decals and water are unchanged. The
  Metal backend applies `D3DRS_DEPTHBIAS` as `setDepthBias` scaled by 2^24-1. That is exact for a
  24-bit depth buffer. For Apple silicon's Depth32Float_Stencil8 it is exact near depth 1, where
  mid-range terrain lies, and smaller closer in, so the bias still applies there. A larger bias
  (-1e-5) let the terrain cover coplanar road and track decals.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Geom/TerrainDraw.cpp`, `Code/BF2/Geom/QuadPatch.cpp`,
  `Code/BF2/Geom/TerrainColourPassBias.h`

### The effect parameter snapshot zeroes its description before the query

- **Retail:** `Effect::snapshotParameterValues` calls `GetParameterDesc` into an uninitialized
  `D3DXPARAMETER_DESC`, ignores the result and stores `desc.Bytes` as the pending value's size
  (`RendDX9.dll` `0x1001E59B`, `0x1001E5AA`). When the query fails, for example for an unbound
  handle, the size is whatever the stack held, and the later flush passes it to `SetValue`. The
  snapshot runs when an effect is rebound. A failing query in a retail session was not shown.
- **OpenBF2:** Zeroes the description first, so a failed query records size 0. Successful queries
  behave as in retail.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/RendDX9/Effect.cpp`

### An atlas index (.tai) line with missing numbers reads them as zero

- **Retail:** The texture atlas index reader (`RendDX9.dll` `0x10015970`) extracts each line's two
  offsets and its size into four float locals it never initializes (extractions at `0x10015C45` to
  `0x10015CAF`, copied into the entry from `0x10015CB5`). A failed extraction leaves its target
  unchanged (`msvcp71.dll` `0x7C3C0ABA`), so a malformed line stores stack garbage on the first line
  and the previous line's values afterwards.
- **OpenBF2:** The four numbers start at zero for every line, so a number the line does not supply
  is zero. Well-formed files read identically.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/RendDX9/TextureAtlas/TextureAtlas.cpp`

### 3D water animates continuously instead of jumping at every cycle

- **Retail:** For 3D water `Terrain::updateWaterConstants` writes
  `WaterCycleTime = frac(GlobalTime / waterAnimSpeed)` (`RendDX9.dll` `0x100DBB52`). The stock
  shader `RaShaderWaterBase.fx` offsets the water texture by `WaterScroll * WaterCycleTime` and by
  `WaterCycleTime * 10 + 0.7 u + 1.13 v`, so the surface jumps each time the value wraps.
  [B2BF/OpenBF2 #69](https://github.com/B2BF/OpenBF2/issues/69) records the jump on retail: every 25
  to 30 seconds on the two maps it names.
- **OpenBF2:** The 3D arm divides a double-precision copy of `GlobalTime`, advanced wherever
  `GlobalTime` is, by `waterAnimSpeed` and writes the result modulo 1000 cycles: the same speed,
  with a wrap only after hours, which keeps the float constant precise. The 2D water, which uses the
  value as a 0 to 1 frame blend, stays retail. Special Forces maps are included.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Geom/TerrainDraw.cpp`, `Code/BF2/RendDX9/BF2Render.cpp`;
  [#30](https://github.com/kr4uzi/OpenBF2/issues/30);
  [B2BF/OpenBF2 #69](https://github.com/B2BF/OpenBF2/issues/69)

### Water cells beyond the 256-entry parameter ring are skipped

- **Retail:** `Terrain::drawWaterCell` (`RendDX9.dll` `0x100DC400`) allocates a ring of 256
  object-parameter records of 548 bytes, takes entry `cursor++` for every cell and fills it, with no
  bound on the cursor; only the grid pass sets the cursor back to 0. A pass that draws more than 256
  water cells writes past the allocation. Not reproduced on retail:
  [#33](https://github.com/kr4uzi/OpenBF2/issues/33) expects the limit to be reached at extreme
  views.
- **OpenBF2:** `drawWaterCell` returns before any allocation or ring access once the cursor has
  reached 256. The cells beyond the limit stay undrawn in that pass, so water can be missing at
  extreme views; entries already queued are kept. `drawWaterGrid` resets the cursor as before. The
  guard applies in every mod.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Geom/TerrainDraw.cpp`, `Code/BF2/Geom/Terrain.h`, `docs/widescreen-hud.md`;
  [#33](https://github.com/kr4uzi/OpenBF2/issues/33)

### New rain particles start with cell 0 and group 0

- **Retail:** `RainManager::setParticleCount` (`RendDX9.dll` `0x10159F00`) grows the particle vector
  with a fill value of which only the position is written; cell and group keep whatever the stack
  held. `buildPointBuffer` (`0x10159960`) uses cell as an unchecked index into the 216 per-cell
  counters, and `setRenderMode` reaches it without scattering the particles first. Which call order
  a retail session produces was not shown.
- **OpenBF2:** The fill value has cell 0 and group 0, so a buffer built before the particles are
  scattered counts them in cell 0.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Geom/Rain/RainManager.h`

### A reissued order nametag no longer writes into the callers' stack

- **Retail:** `NametagManager::addTouchItem`, when the order's item already exists, calls
  `OrderNametagItem::setOrder` on the address of its local item pointer instead of the item
  (`RendDX9.dll` `0x10131103`; `0x101311DE` in the overload that takes an object). `setOrder`'s
  stores at `+0xC0` to `+0xFF` then land in the callers' stack frames, and the object overload also
  releases a reference through the stack word at `+0x100`. The item keeps its old order.
- **OpenBF2:** Makes no call on that path. The existing item keeps its old order as in retail, and
  the stack is not written.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Geom/NametagManager.cpp`

### The Swiff line-strip draw is bounded to its scratch array

- **Retail:** The Swiff line-strip draw transforms `count` vertices of 12 bytes each into a static
  scratch array with no bound on `count` (`RendDX9.dll` `0x101547B0`). The array at `0x1025E9E0` is
  at most 0x1000 bytes, 341 vertices: the next static any relocation refers to is at `0x1025F9E0`. A
  longer strip overwrites the statics behind it. The largest count the Flash player passes was not
  determined.
- **OpenBF2:** Uses a scratch array of 10002 vertices and clamps the count to it; a longer strip is
  cut. The capacity is chosen, not recovered, and is larger than the shipped one.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Geom/SwiffRender.cpp`

### The Swiff font key is built without a fixed stack buffer

- **Retail:** `SwiffRender`'s `createFont` slot (`RendDX9.dll` `0x10152EE0`) builds the cache key
  `<name>_<size>` with `sprintf` into a 256-byte stack buffer. A font name of about 250 characters
  or more overruns it. Not reproduced with shipped content.
- **OpenBF2:** Builds the key in a `std::string`; only the number goes through a bounded format.
  Keys for names that fit are byte-identical.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/GeomMisc/FontSystem/SwiffFont.cpp`

### The DifFont file readers start from zeroed values

- **Retail:** The `.dif` font readers extract into locals that are never initialized: `readHeader`
  (`RendDX9.dll` `0x100A34D0`) puts the first name byte back whether or not it was read, and
  `readGlyphs` (`0x100A41B0`) and `readKerning` (`0x100A4120`) insert a row built from the count and
  fields as they stand. A file that ends inside the first glyph or kerning row can yield an entry
  built from stack contents. Not reproduced with a real file.
- **OpenBF2:** The name buffer, the counts and the row values start at zero, so a row that is cut
  short is built from zeros or from the previous row, never from stack contents. Complete files
  parse identically.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/GeomMisc/FontSystem/SwiffFont.cpp`

### MemeTexture::applyCanvas no longer writes before the locked pixels

- **Retail:** `MemeTexture::applyCanvas` (`RendDX9.dll` `0x10088AA0`) walks the pixels shifted by
  one: each of its seven operation loops starts at the last pixel, steps back before it writes and
  runs once per pixel. It therefore writes pixels n-2 down to -1, reads the source byte at -1 and
  never writes the last pixel. The write at -1 lands in the four bytes before the locked texture
  bits.
- **OpenBF2:** Runs the same walk from n-2 down to 0 and omits the step at index -1 in every build.
  The last pixel stays unwritten, as in retail.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/GeomMisc/MenuRend/MemeTexture.cpp`, `docs/macos-port.md`

### Menu natives split list and server-row ids with bounded reads

- **Retail:** `getListEntries` splits its list id with the `sscanf` format `"%s %s %s %s"` into four
  256-byte globals (`SwiffPlayer.dll` `0x10022783`). Five server-row natives split
  `<address> <queryPort> <hostPort>` with the format `"%s %d %d"` into 260-byte stack buffers
  (helper `0x100508D0`; `0x10051B82`, `0x100549D8`, `0x10054C31`) and use the two numbers
  uninitialized when the id holds fewer. An over-long token overruns the buffers. No over-long id is
  known from a retail session.
- **OpenBF2:** The list id is split at spaces by a bounded loop into strings that start empty on
  every call. The row id is read with a width of 259 characters into a zeroed buffer, and missing
  numbers are zero.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayer.cpp`,
  `Code/BF2/External/gameswf/SwiffPlayer/SwiffNativesRecovered.inl`

### PlayerStats.getFavouriteName bounds its name-table index

- **Retail:** `dice.bf2.PlayerStats.getFavouriteName` indexes its kit, vehicle and weapon name
  tables with the favorite index of the fetched stats record. Each arm compares the index with -1
  only and then uses `index * 28 + table` as a string (`SwiffPlayer.dll` `0x1005B382`, `0x1005B40E`,
  `0x1005B49A`), so any other index outside a table reads beyond it. Per the recovered host code the
  index is the stats service's answer; no out-of-range answer was shown.
- **OpenBF2:** An index outside a table, negative or past its end, answers the single space that -1
  answers. In-range indexes behave as in retail.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayer.cpp`

### Leaderboard natives stay inside their cache and request tables

- **Retail:** For kits, vehicles and weapons `SwiffPlayer.dll` indexes the leaderboard cache by
  board with no bound: the row builder (`0x10014230`, `0x10014464`, `0x10014694`) and
  `getNumPlayersInLeaderboard` (`0x1005BEE0`), whose request tables (`0x100DDB58`) run into string
  bytes after 26 entries. A board past the cache reads stack memory as rows or as a count. The
  players list sends an unset profile id when no profile is active (`0x1002C1EA`). Not shown with
  the shipped menu.
- **OpenBF2:** A group above 30 builds no rows and reports 0 players and an empty date. A request
  past the 26 table entries fetches nothing; inside the tables the neighboring entries are still
  used, as in retail. Without a profile the players list starts from -1 and skips that player.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayer.cpp`

### Haptic threshold getters search only the three threshold names

- **Retail:** The 20 `HapticSettings` threshold getters, such as `getPhysicsSOLDIERTransition`
  (`SwiffPlayer.dll` `0x1003A570`), compare the name against 13 slots of 28 bytes (bound `0x16C` at
  `0x1003A5DE`) of a table that holds three strings (`0x100F3870`); the setters stop after three.
  Any other name is compared against ten slots of unrelated globals, and a match there passes an
  index from 3 to 12 to `HapticSettings`. Not shown with the shipped menu.
- **OpenBF2:** Getters and setters search only the three names. Any other name gives -1, the level
  retail passes when nothing matches; the three names behave as in retail.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/External/gameswf/SwiffPlayer/SwiffNativesRecovered.inl`

### A command-line join uses the +password value

- **Retail:** `BF2Engine::joinServer` (`BF2.exe` `0x405C10`) overwrites the `GSPassword` setting,
  which `+password` fills, with the menu's join password (`0x405D80` to `0x405DA7`). Only the menu's
  password dialog sets that: the `joinCommandlineServer` native (`SwiffPlayer.dll` `0x1004D740`) and
  the shipped menu movies never copy the command-line password into it, so `+password` is lost on a
  `+joinServer` start. The refusal by a passworded server was not reproduced on retail.
- **OpenBF2:** The `joinCommandlineServer` native copies the command-line password into the menu's
  join password when that is empty, before it joins. A password entered in the menu still wins.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayer.cpp`,
  `Code/BF2/Game/Main/BF2Engine.cpp`

### Joining without a CD key disconnects instead of crashing

- **Retail:** `GameClient::handleChallenge` (`BF2.exe` `0x4D1CF0`) translates the stored CD key
  before it tests the result. For an empty key `translateCdKey` (`0x59F6A0`) still protects it:
  `protect` allocates no blob for an empty string, and `writeHex` reads through the null blob
  pointer (`0x5B040B`). The client faults at the first challenge and never reaches its own
  disconnect with reason 7. Read from the image; seen on the rebuilt macOS client, not on a retail
  run.
- **OpenBF2:** `translateCdKey` answers an empty string for an empty key, so `handleChallenge`'s
  existing empty-key test runs and the client disconnects with reason 7.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Game/MatchMaking/Client/CdKeyStore.cpp`,
  `Code/Tests/macos_protected_data/protected_data_check.cpp`;
  [#59](https://github.com/kr4uzi/OpenBF2/issues/59)

### Activating a profile without characters does not read an unconstructed value

- **Retail:** `PlayerProfile::activateFirstCharacterProfile` (`BF2.exe` `0x7C0510`) takes the first
  node of the character map and passes its value's name to the lookup with no test for an empty map.
  With no characters that node is the map head, whose value was never constructed. Its caller
  (`0x7BFCA0`, the `Profile.activatePlayerProfile` command in the recovered source) runs it for any
  profile found, also one without characters. Not run on retail.
- **OpenBF2:** With no characters an empty name is passed, which finds nothing and clears the active
  character.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Menu/ProfileConsoleBindings.cpp`

### The spawn map node initializes the flag its update reads first

- **Retail:** `Bf2MenuMapNode`'s constructor (`BF2.exe` `0x780180`) never writes the byte at
  `+0x68D`, but `update` tests it (`0x77C48F`) ahead of its own stores; only the derived mini-map
  node's constructor clears it. The node's block comes from `bf_malloc`, which does not clear it, so
  retail runs on whatever the block held. Seen on the macOS port, not on retail: a non-zero byte
  kept the first spawn screen's map at its mini size without the DONE/SUICIDE row.
- **OpenBF2:** The constructor sets the byte to false, the value the derived mini-map node's
  constructor stores.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Menu/Bf2MenuMapNode.cpp`, `Code/BF2/Menu/Bf2MenuMapNode.h`

### Map icons are drawn for texture handles with the top bit set

- **Retail:** `Bf2MenuMapNode::paintMapIcon` (`BF2.exe` `0x7746E0`) and the mini map's version
  (`0x7CF020`) return without drawing when the texture handle is `<= 0` as a signed 32-bit value. A
  handle is the address of the texture object (`Ref2System::loadTexture`, `0x7AD240`) and `BF2.exe`
  is large-address-aware, so an address above 2 GB counts as no texture. Upstream reports map icons
  that vanish when memory use is high; not reproduced.
- **OpenBF2:** Both functions take the handle as an unsigned pointer-sized value and return only
  when it is 0. Other handle tests, such as the one that collects strategic-object icons, stay
  signed.
- **Applies to:** Client; all platforms. The two tests differ only for a 32-bit handle of 0x80000000
  or above. The x86 client is not linked large-address-aware and 64-bit addresses never have the top
  bit set, so no client built from this tree shows a difference today.
- **Source:** `Code/BF2/Menu/Bf2MenuMapNode.cpp`, `Code/BF2/Menu/Bf2MiniMenuMapNode.cpp`; from pull
  request [B2BF/OpenBF2 #36](https://github.com/B2BF/OpenBF2/pull/36)

### HudChat::addPlayerMessage drops a chat message that has no player

- **Retail:** `HudChat::addPlayerMessage` (`BF2.exe` `0x79F3E0`) tests the player for null only
  around the ignored-player query (`0x79F3EF`) and then calls the player's `getName` unconditionally
  (`0x79F442`), so a message without a player crashes the client. `HudChat::addMessage` passes a
  null player for message kind 1 (`0x79E7D0`); whether that arm or another caller is reached in play
  was not established.
- **OpenBF2:** A message without a player is dropped at the top of the function. With a player the
  function is unchanged.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Menu/HudChat.cpp`; [#30](https://github.com/kr4uzi/OpenBF2/issues/30)

### Incoming voice datagrams shorter than their trailer are rejected

- **Retail:** `BF2VoipClient::handleIncoming` (`BF2.exe` `0x451640`) reads the sender id at
  `packet + length - 2` and the frame stamp two bytes before it; `VoipClient::handleIncoming`
  (`0x736F60`) reads a dword and a word the same way. Neither tests the length, and the receive
  loops hand them every datagram of one byte or more. A datagram shorter than its 4- or 6-byte
  trailer makes them read before the start of the receive buffer, and a zero or negative payload
  length can reach playback.
- **OpenBF2:** Both parsers return false before touching the packet unless the length exceeds the
  trailer and the payload is a whole number of codec frames of a positive size. A rejected datagram
  leaves the sender output untouched and notifies neither playback nor the HUD. Datagrams that carry
  whole frames are parsed as before.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Game/VoipClient/BF2VoipClient.cpp`,
  `Code/BF2/Game/VoipClient/VoipClient.cpp`, `Code/Tests/voip_client_runtime.cpp`;
  [#26](https://github.com/kr4uzi/OpenBF2/issues/26)

### Microphone source selection sizes its value buffer by items and channels

- **Retail:** The mixer code that makes the microphone the recording source (`BF2.exe` `0x735EA0`)
  allocates one 4-byte entry per MUX item, does not test the result, and passes that buffer to
  `mixerGetControlDetails` with `cChannels * cMultipleItems` entries requested; `cChannels` is the
  line's channel count unless the control is uniform. For a non-uniform MUX on a multi-channel line
  the API may write past the allocation. Not reproduced on hardware.
- **OpenBF2:** Allocates items times channels entries for the value query and returns false when the
  allocation fails. The list-text buffer that is queried next keeps retail's size of one entry per
  item.
- **Applies to:** Client; Windows x86 and x64. The macOS mixer backend has no MUX control and Linux
  has no mixer, so the allocation is not reached there.
- **Source:** `Code/BF2/Game/VoipClient/VoipMixer.cpp`

### Server logo URLs cannot leave the logo cache

- **Retail:** `ServerLogoManager::parseUrl` (`BF2.exe` `0x4C3550`) splits a logo URL at `/` and
  takes every part as a cache path component; it does not test for `..` or backslashes.
  `ServerLogoManager::download` (`0x4C3120`) ignores parseUrl's answer (the call at `0x4C3164`),
  deletes `<home>\LogoCache\<directory><file>`, creates the directory chain and downloads the URL
  into it. A server's sponsor or community logo URL can therefore delete and write files outside
  the cache folder, the sponsor logo already when the server is selected in the browser (Luigi
  Auriemma, "Refractor 2 Client URL Directory Traversal", bf2urlz, 2010; reported for 1.50).
- **OpenBF2:** parseUrl refuses a URL with a part that is only dots and spaces, or that contains a
  backslash or a control character. download returns false for a refused URL before it touches a
  file, and getCachedPath refuses it as before, so the logo is not shown. Other URLs are split and
  cached as before; a colon (`host:port`) stays allowed.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Menu/ServerLogoManager.cpp`, `Code/Tests/server_logo_url_runtime.cpp`

### GroundEffectBundle plays the effect of the ground material it hits

- **Retail:** A GroundEffectBundle (stock: `geb_tank`, material 66, on the tank and artillery
  barrels) casts a ray of `maxTriggerDistance` metres below itself when it starts and finds the hit
  object face's material or the terrain's (`BF2.exe` `0x646B60`, ELF `0x8202AB0`), but then asks the material
  manager for the effect of the cell (own material, 0) (`BF2.exe` `0x646D05` to `0x646D1D`, ELF
  `0x8202CF6` to `0x8202D1F`). So it always plays that cell's effect (stock: `e_geb_tank_generic`),
  although `materialManagerSettings.con` gives cells (66, n) separate effects for water, sand,
  gravel, dirt, mud and leaves. Reported for retail by a Project Reality modder in
  [B2BF#93](https://github.com/B2BF/OpenBF2/issues/93).
- **OpenBF2:** Asks the cell (own material, hit material). A Soldier hit leaves the hit material
  at -1, and like any material without its own cell it falls back to (own material, 0), so those
  hits play the effect retail plays.
- **Applies to:** Client; all platforms. The dedicated server never creates the object.
- **Source:** `Code/BF2/Game/Objects/Fx/GroundEffectBundle.cpp`

### The kit shown on the spawn screen is requested with the spawn

- **Retail:** The spawn screen keeps the last selected kit across rounds and servers
  (`HudKitSelection`, whose reset at `BF2.exe` `0x7A1E10` keeps it) and shows it as selected after a
  join, but the server gives a new player kit 2 (`ServerGameLogic::_createPlayer`, Linux ia-32
  server `0x8101400`). The kit is sent only by a click on a kit (`HudKitSelection::selectKit`) or
  after a death (`BF2HudManager::onSpawn`, `0x751560`); the Spawn button (`BF2HudManager::setDone`,
  `0x74F180`) sends only the spawn group.
  The first spawn after a join therefore uses kit 2 while the screen shows another kit.
  Reproduced with a retail client on retail and OpenBF2 servers
  ([B2BF/OpenBF2 #34](https://github.com/B2BF/OpenBF2/issues/34)).
- **OpenBF2:** `setDone` calls `selectKit` with the shown kit before it sends the spawn group. As
  on a click, the request is sent only when the player's kit differs, with the player's chosen
  unlock level for that kit.
- **Applies to:** Client; all platforms.
- **Source:** `Code/BF2/Menu/BF2HudManager.cpp`

## Server

### The dedicated server rejects ghost updates sent by a client

- **Retail:** `GhostManager::processReceivedPacket` is the same on the dedicated server as on the
  client (Linux ia-32 server `0x8098780`, `bf2_w32ded.exe` `0x557F20`): when a packet's ghost bit is
  set it stores the sender's tick, reads a control-object state and decodes up to 255 ghost records,
  which `readData` hands to `NetworkManager::setNetUpdate` and `disableObject`. A modified client
  can thus feed object state to the server. No reproduction is recorded.
- **OpenBF2:** In dedicated builds a set ghost bit makes `processReceivedPacket` fail before
  anything is decoded or applied, and the server disconnects that peer. The one-bit "no ghosts"
  block is accepted as before. Client builds, which also host local games, keep the retail body.
- **Applies to:** Server (dedicated builds); all platforms.
- **Retail clients:** A retail client never sets the ghost bit toward a remote server. In `BF2.exe`,
  `GhostManager::transmit` (`0x5BBB10`) writes it only with a control-object state or queued ghosts,
  and only `GameServer` and `ServerConnection` code supplies those (the callers of `0x5B7390` and
  `0x5BAE70`). The connection a joining client creates gets neither, so it always sends the one-bit
  empty block.
- **Source:** `Code/BF2/Game/Common/GhostManager.cpp`;
  [B2BF/OpenBF2 #1](https://github.com/B2BF/OpenBF2/issues/1)

### The Linux server keeps command-line arguments that contain spaces

- **Retail:** The Linux server's `main` joins `argv[1..]` with single spaces and no quoting (Linux
  ia-32 server `0x8052C70`). The option parser reads an unquoted value up to the next whitespace and
  a quoted one up to the closing quote (`SimpleParser::getKey`, `0x814A5F3`). A shell-quoted path
  with a space is therefore cut at the space; only an argument that itself contains the quotes
  works. Linux images only: the Windows server gets its line from `WinMain`.
- **OpenBF2:** The Linux entry point puts quotes around an argument before the join only where
  retail would cut it: the argument has text after whitespace, contains no option token (a `+`, a
  name, then whitespace or the end of the argument), and neither it nor an earlier argument
  contains a quote character. Under the same quote condition an empty or blank argument, unless it
  is the only one, is written as `""`: it parses as retail's empty value and survives the in-game
  restart of the macOS and Linux clients, which splits the stored line again. Every other argument,
  one whose whitespace is all at its end included, is joined unchanged, so every spelling that
  works on retail, such as an argument with its own quotes or one that holds several options,
  parses as retail's line; whitespace at the end of a value is cut as on retail. A value with
  whitespace is still cut as on retail when it contains an option token or follows an argument
  with a quote character. The restart can still change the parse where an argument holds a quote
  character, which the split reads as `CommandLineToArgvW` does, or where an argument that holds an
  option also holds the whitespace of an empty value, such as `+password  +port 1` passed as one
  argument. A value longer than 255 characters arrives whole, by the fix "Option values longer
  than 255 characters no longer overrun the parser's buffer".
  `bf2_command_line_join_check` compares the parse with retail's line through the recovered parser
  and, on macOS, the restart's round trip; not run on a server or client.
- **Applies to:** Server; Linux, and macOS, whose server uses the same entry point. The helper also
  serves entry points without a retail counterpart: the macOS and Linux clients and the rebuilt
  Windows console entry.
- **Retail clients:** The change is confined to how the server process reads its own command line.
  Nothing on the wire and nothing a client relies on changes.
- **Source:** `Code/BF2/Game/Main/BF2.cpp`, `Code/BF2/Game/Main/CommandLineJoin.h`,
  `Code/Tests/command_line_join_runtime.cpp`

## Client and server

### Windows computer names keep all 15 characters

- **Retail:** `GetComputerNameA` receives capacity 15 (`BF2.exe` `0x004C4717`,
  `0x004C471E`; `bf2_w32ded.exe` `0x004428D0`, capacity store `0x004428EF`). A
  15-character computer name needs 16 bytes including its terminator, so the call fails
  and the cached name becomes `"unknown"` (`BF2.exe` `0x004C4728`–`0x004C4740`).
  With no configured server name, `getServerName` produces `"BF2 server on unknown"`
  (`bf2_w32ded.exe` `0x0054D250`). `SwiffHost::getComputerName` also exposes the cached name.
- **OpenBF2:** Passes the full 255-byte buffer capacity, preserving all 15 characters.
  Query failures still cache `"unknown"`.
- **Applies to:** Client and server; Windows.
- **Retail clients:** The default server-name text contains the actual computer name;
  configured server names, protocol fields, file formats and gameplay behavior are unchanged.
  Retail clients already accept the same text from an explicitly configured server name.
- **Source:** `Code/BF2/Core/SystemInfo.cpp`, `Code/BF2/Game/Utility/BuildNrUtil.cpp`,
  `Code/BF2/Menu/SwiffHost.cpp`

### Distributed lightmap jobs start numbering at zero

- **Retail:** `DistributedLightRenderer`'s constructors leave the job counter at `+0x08`
  unwritten (`BF2.exe` `0x46C350`; Linux ia-32 server `0x080C15A0`, `0x080C1C20`). Its first
  read is in `addObjectToJobQueue` (`BF2.exe` `0x46C20A`; Linux `0x080C2946`): the old value
  becomes the queued work item's id, then the counter advances. The first lightmap job therefore
  gets whatever number the heap held. Not reproduced in a retail lightmap farm.
- **OpenBF2:** Initializes the counter to 0, so jobs are numbered from 0 for each new renderer.
- **Applies to:** Client and server lightmap-farm code in client builds; all platforms. The
  rebuilt dedicated image excludes this implementation; no constructor was identified in
  `bf2_w32ded.exe`.
- **Retail clients:** The counter is internal queue bookkeeping. `assignWorkItem` (`BF2.exe`
  `0x46BA30`; Linux `0x080C3460`) sends the object id or terrain-patch index in data block 4,
  not this job id. The block's format and contents are unchanged.
- **Source:** `Code/BF2/Game/DistributedLightRenderer/DistributedLightRenderer.cpp`

### The material manager's unwritten word starts at zero

- **Retail:** `MaterialManager`'s constructors leave the word at `+0x10` unwritten (Linux ia-32
  server `0x0810E480`, `0x0810E540`). No read of that word was identified in the image's
  `MaterialManager` methods, so no first consumer or gameplay failure is established. The Windows
  layout differs: `BF2.exe` `0x63C340` and `bf2_w32ded.exe` `0x5ED190` initialize a map count at
  `+0x10` (`0x63C379`, `0x5ED1C9`); no corresponding independent scalar was identified there.
- **OpenBF2:** Initializes the retained `m_unknown10` member to 0 instead of leaving its value
  indeterminate. Its purpose remains unknown, and no observable behavior change is established.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** No code in the rebuilt tree reads this member. Material selection, collision
  properties, saved material data and network messages are unchanged.
- **Source:** `Code/BF2/Physics/MaterialManager.cpp`

### The spawn manager's unwritten words start at zero

- **Retail:** `SpawnManager`'s constructors leave the words at `+0x14` and `+0x24` unwritten
  (Linux ia-32 server `0x081124F0`, `0x081126B0`). No reads of either word were identified in
  the image's `SpawnManager` methods, so no first consumers or gameplay failures are established.
  The Windows constructors (`BF2.exe` `0x6599A0`; `bf2_w32ded.exe` `0x611EE0`) use a different
  container layout: the spawn-group map starts at `+0x14`, the mapped-id map at `+0x24`, and the
  group-id counter at `+0x20` starts at 1. No corresponding independent scalars were identified.
- **OpenBF2:** Initializes the retained `m_unknown14` and `m_unknown24` members to 0 instead of
  leaving their values indeterminate. Their purposes remain unknown, and no observable behavior
  change is established. The separate group-id counter still starts at 1.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** No code in the rebuilt tree reads these members. Spawn-group allocation,
  selection and replication are unchanged, including the group ids sent to retail clients.
- **Source:** `Code/BF2/Game/GameLogic/SpawnSystem/SpawnManager.cpp`

### Option values longer than 255 characters no longer overrun the parser's buffer

- **Retail:** `SimpleParser::getKey` copies an option's value with `strncpy` into a 256-byte stack
  buffer with no bound and writes the terminator after the copy, in the unquoted and the quoted arm
  (Linux ia-32 server `0x814A4F0`: buffer at `ebp-0x128`, copies at `0x814A62B` and `0x814A6D0`;
  `BF2.exe` `0x45CB80`: copies at `0x45CD4F` and `0x45CCCF`; `bf2_w32ded.exe` `0x43FB70`: copies
  at `0x43FD6A` and `0x43FCDD`). A value longer than 255 characters writes past the buffer into the
  rest of the frame: in the Linux image and `BF2.exe` into later locals, saved registers and the
  return address; in `bf2_w32ded.exe` first into the stack cookie, which the function compares
  before it returns (`0x75663F`; the mismatch path was not read). The parser reads the process
  command line (`BF2Engine::parseParameters`, Linux ia-32 server `0x8131EC0`, `BF2.exe`
  `0x409CA0`) and, in `BF2.exe`, the text of menu event 5 (`0x40BE55`), which the menu sends and
  writes to `CommandLine.cmd` (`0x493B90`). Not reproduced on retail.
- **OpenBF2:** `getKey` takes the value from the string itself, without the buffer. As on retail it
  ends at the first NUL, so values of up to 255 characters give retail's result; longer values
  arrive whole. The option search, the quoted arm's search for the closing quote and the failure
  path that keeps the found option id are unchanged. `bf2_command_line_join_check` compares the
  function with a model of the retail body for values of up to 255 characters and checks values of
  256, 1000 and 4096 characters in both arms; not run in a server or client.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** Only the process's own reading of its command line, and of the client's
  menu-supplied command line, changes. Nothing on the wire changes.
- **Source:** `Code/BF2/Game/Utility/SimpleParser.cpp`, `Code/Tests/command_line_join_runtime.cpp`

### Player actions received before the connection has a player are dropped (BF2NULL)

- **Retail:** `PlayerActionManager::processRcvdPlayerActions` dereferences its player pointer after
  testing only the count (`bf2_w32ded.exe` `0x616F9F`; Linux ia-32 server `0x809D9BD`), although its
  caller tests the same pointer for null just before (Linux ia-32 server `0x809DAF1`). A client that
  sends an action block while its connection has no player crashes the server: the BF2NULL advisory
  ([B2BF/OpenBF2 #27](https://github.com/B2BF/OpenBF2/issues/27)). The crash was not reproduced.
- **OpenBF2:** With no player bound the received actions are dropped: the function returns false,
  which its caller ignores, and the input high-water mark does not move.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** Nothing on the wire changes: the action block is read from the packet exactly
  as before, and no disconnect follows. The fix acts only where a retail server crashes, and a
  retail client sends no action block while it has no player, so no working session depends on the
  old behavior.
- **Source:** `Code/BF2/Game/Common/PlayerActionManager.cpp`;
  [B2BF/OpenBF2 #27](https://github.com/B2BF/OpenBF2/issues/27)

### Data-block announcements above 1 MiB are refused

- **Retail:** `DataBlockManager::newDataBlock` allocates the 32-bit length a peer announces after
  testing only that no block is in progress (Linux ia-32 server `0x808C7E0`, `new[]` at `0x808C837`;
  the same in `BF2.exe` `0x679C30` and `bf2_w32ded.exe` `0x617B80`). `DataBlockEvent::deSerialize`
  (Linux ia-32 server `0x806F9F0`) reads that length with no range check, so any connected peer can
  make the receiver, a server included, try to allocate up to 4 GB.
- **OpenBF2:** `newDataBlock` refuses a length above `0x100000` the way it refuses while a block is
  in progress: nothing is allocated, the block's chunks are then dropped by `addDataChunk`, and the
  connection stays open. The first refusal is logged once per process.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** The cap applies only to blocks a peer announces to the receiver. A retail
  client sends the server two kinds, its client-info record and remote-console text, both far below
  1 MiB, and what the server sends is unchanged. The senders are read from the recovered client
  source; not measured in a joined session.
- **Source:** `Code/BF2/Game/Common/DataBlockManager.cpp`,
  `Code/BF2/Game/Common/DataBlockManager.h`, `Code/Tests/datablock_cap_runtime.cpp`;
  [#30](https://github.com/kr4uzi/OpenBF2/issues/30)

### Game event readers keep wire lengths and counts inside their buffers

- **Retail:** Seven `deSerialize` functions trust a wire length or count (addresses in the Linux
  ia-32 server): `PostRemoteEvent` `0x8077A70` reads up to 255 bytes into 52; `ChangeSquadNameEvent`
  `0x806B500` and `ChallengeEvent` `0x806A870` up to 255 into 32 and 16, then write a NUL;
  `StringBlockEvent` `0x807D530` and `DataBlockEvent` `0x806F9F0` up to 255 into 63;
  `PythonCommandEvent` `0x8078270` up to 15 values into 4; `ChangePlayerNameEvent` `0x806B0A0`
  forces no terminator. `BF2.exe` does the same.
- **OpenBF2:** Each reader stores at most what its buffer holds, skips the excess bits so that later
  events stay aligned, and terminates the names. Events within the sizes are read exactly as before,
  and the wire format is unchanged.
- **Applies to:** Client and server; all platforms. Whichever side receives an event runs its
  reader.
- **Retail clients:** The readers differ only for events that carry more than the buffers hold. The
  retail constructors cap what is sent: squad and player names at 31 characters plus a terminator,
  chunks at 63 bytes, four command values (read in `BF2.exe` and the Linux server). The
  `PostRemoteEvent` constructors have no cap, but a payload above 52 bytes would overwrite the
  sender's own count byte. A retail client therefore never sends such an event, and what it receives
  is unchanged. Not run against a retail client.
- **Source:** `Code/BF2/Game/Common/GameEvent.cpp`,
  `Code/BF2/Game/GameEvents/ChangeSquadNameEvent.cpp`,
  `Code/BF2/Game/GameEvents/StringBlockEvent.cpp`,
  `Code/BF2/Game/GameEvents/ChangePlayerNameEvent.cpp`,
  `Code/BF2/Game/GameEvents/ChallengeEvent.cpp`, `Code/BF2/Game/GameEvents/DataBlockEvent.cpp`,
  `Code/BF2/Game/GameEvents/PythonCommandEvent.cpp`

### The server refuses a kit the player's team does not have

- **Retail:** `GameServer::handleNetworkEvent` masks a client's kit request to 0..15 (Linux ia-32
  server `0x80AC894`), and `ServerGameLogic::selectKitAndUnlockLevel` (`0x80FBDA0`) stores it
  unchecked; stock levels register kits 0 to 6 per team. At the next spawn
  `ServerGameLogic::spawnPlayer` finds no kit for the team and index and raises the Error "TBM:
  Unable to create the requested kit!" (`0x8100370`); an emitted Error ends the process. Not
  reproduced. `bf2_w32ded.exe` stores the kit with no check as well (`selectKitAndUnlockLevel`
  `0x4543E0`, the vtable slot after `selectTeam` as in the Linux image); its request masking and
  `spawnPlayer` were not read.
- **OpenBF2:** `selectKitAndUnlockLevel` stores nothing and returns false unless the player's team
  is 0 to 2 and the kit is registered for that team, in the same kit map `spawnPlayer` reads; the
  previous kit and unlock level stay. Accepted requests are stored as in retail.
- **Applies to:** Client and server, wherever a game is hosted; all platforms.
- **Retail clients:** Nothing on the wire changes. An unmodified client sends only kits its HUD
  offers for its team, 0 to 6 on both teams in 1.50, and the server checks them against the team it
  already holds, so they are accepted as before. Only a kit the player's team lacks, which takes a
  modified client or a mod whose teams have different kit lists, is refused where retail would end
  the process at the next spawn.
- **Source:** `Code/BF2/Game/GameLogic/ServerGameLogic.cpp`; from [B2BF/OpenBF2
  #9](https://github.com/B2BF/OpenBF2/issues/9)

### The server refuses a team outside 0 to 2

- **Retail:** `GameServer::handleNetworkEvent` passes the 32-bit team a client requests (Linux ia-32
  server `0x80AC7F8`) to `ServerGameLogic::selectTeam` (`0x80FBC40`; `bf2_w32ded.exe` `0x455900`),
  and `Player::setTeam` (`0x80F3410`) stores any value. The player update sends the team as team + 1
  in 0 to 3; for any other team the writer raises the Error "(fatal error) Value out of bounds"
  (IO/BitStream.inl, line 59; `Player::getNetUpdate` `0x80EEA74`, `0x80F02AA`). Team -1 passes the
  update, but a spawn attempt, such as one with the spawn screen's reset group -1, asks
  `ScoreManager::getTeamScore` (`0x8107890`) for it, which raises the Error "TBM: Incorrect team
  number" for every team but 1 and 2. An emitted Error ends the process. Not reproduced. The
  Windows server's update writer was not read.
- **OpenBF2:** `selectTeam` returns false for a team below 0 or above 2 before anything changes: no
  team change, no Python notification, no correction event. Teams 0 to 2, team 0 included, are
  handled as in retail.
- **Applies to:** Client and server, wherever a game is hosted; all platforms.
- **Retail clients:** Nothing on the wire changes. A retail client requests a team only through
  `HudKitSelection::selectTeam` (`BF2.exe` `0x7A2180`), which sends neither -1 nor the current
  team, and the stock spawn screen asks for teams 1 and 2 (`spawnManager.setPlayerTeam 1` and `2`
  in `HudElementsSpawn.con`). Another value takes a modified client or that console command typed
  with it. Every refused value makes a retail server raise one of the Errors above at the next
  player update or spawn attempt; the Error ends the process only when it is emitted
  (`Debug::~Debug`, Linux ia-32 server `0x83C0580`, aborts only on the paths that emit the
  message), and whether a retail server's default filter state emits these Errors was not checked.
  Whether a player can enter that command in the retail console was not checked.
- **Source:** `Code/BF2/Game/GameLogic/ServerGameLogic.cpp`,
  `Code/Tests/gamelogic_registries_runtime.cpp`

### The server refuses a squad request for another team

- **Retail:** `JoinSquadEvent::executeServer` passes the team byte from the packet to
  `SquadManager::addToSquad` (Linux ia-32 server `0x8075270`), and `addToSquad` (`0x811CC20`)
  looks the squad up in that team without comparing it with the player's team.
  `ChangeSquadNameEvent::executeServer` (`0x806B7A0`) likewise renames the packet's team's squad
  and has `setSquadName` add the named player to it. A request for the other team's squad thus
  puts the player into that squad's member set, while `Squad::addToSquad` (`0x81198F0`) sets only
  the player's squad number, so the clients show the player in the same-numbered squad of their own
  team, and makes them its leader when that squad has no players.
  The player is later removed from their own team's squad, where `removeFromSquad` (`0x811CF50`)
  does not find them and returns, so the other team's squad keeps a member who is not in it, even
  after that player leaves the server.
  `setSquadName` refuses a squad with members, and the round reset keeps members, so that squad can
  no longer be created until the server restarts, and the client offers the same squad, the lowest
  one without players, every time. This is a candidate cause of "cannot create squads after (some)
  round changes" ([B2BF/OpenBF2 #37](https://github.com/B2BF/OpenBF2/issues/37)): a client sends
  its own team as it knows it, which is stale while a team change, such as an autobalance move, has
  not reached it yet. Not reproduced on retail.
- **OpenBF2:** Both handlers ignore the request when the player it would add exists and is on a
  different team than the packet names. Requests for the player's own team are handled as in
  retail.
- **Applies to:** Client and server, wherever a game is hosted; all platforms.
- **Retail clients:** Nothing on the wire changes. An unmodified client names its own team
  (`HudSquad::joinSquad`, `BF2HudManager::createSquad`), so its requests are handled as before.
  Only a request made with a stale or foreign team is dropped; the client then stays outside the
  squad, as after any refused join, and can ask again.
- **Source:** `Code/BF2/Game/GameEvents/JoinSquadEvent.cpp`,
  `Code/BF2/Game/GameEvents/ChangeSquadNameEvent.cpp`

### Only the squad leader is told that a member left

- **Retail:** When a player leaves a squad, `Squad::removeFromSquad` sends HUD event 21 to every
  member, the leaver included (Linux ia-32 server `0x8119D00`). The client's handler shows
  "#PLAYERNAME# has left your squad" to any receiver on the same team who is in a squad, except the
  leaver (`BF2.exe` `0x75B0FE`..`0x75B176`), so every remaining member gets it although only the
  squad leader needs it ([B2BF/OpenBF2 #80](https://github.com/B2BF/OpenBF2/issues/80)); BF2142
  tells only the leader.
- **OpenBF2:** The server sends event 21 only to the leaver and the squad leader. The client shows
  the text only when the local player is a squad leader, so it also behaves this way on a retail
  server. When the leader leaves, nobody else gets event 21; the members then get the new-leader
  event (HUD 24) as before. The kick event (HUD 20) still goes to every member.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** Event 21 does nothing else on a member's client: `BF2HudManager` is its only
  receiver, and for another player it only adds the chat line (squad lists are built from the
  players' own squad fields). The leaver, whose client closes its squad view on its own id, and the
  leader still receive it. A retail client on an OpenBF2 server therefore loses only the extra
  message.
- **Source:** `Code/BF2/Game/GameLogic/SquadManager/Squad.cpp`,
  `Code/BF2/Menu/BF2HudManagerHudEvents.cpp`, `Code/Tests/hud_hud_event_runtime.cpp`

### Player names keep up to 27 characters

- **Retail:** The server builds a joining player's name as clan tag, a space and the nick, and
  `GameServer::createPlayerWithValidName` keeps only its first 23 characters (Linux ia-32 server
  `0x80AFCA0`, constant `0x17`); `GameServer::changePlayerName` does the same (`0x80B3460`). The
  client caps the tag at six characters (`GlobalSettings::getNamePrefix`, `BF2.exe` `0x4B0A00`) and
  a GameSpy unique nick has up to 20 (`GP_UNIQUENICK_LEN` 21), so a full name has up to 27
  characters and long names lose their end in the game, the scoreboard and server queries
  ([B2BF/OpenBF2 #59](https://github.com/B2BF/OpenBF2/issues/59)).
- **OpenBF2:** Both functions keep up to 27 characters. With a name collision the retry suffix
  `_<id>` (at most `_255`) still fits in the 31 characters the player events carry.
- **Applies to:** Client and server, wherever a game is hosted; all platforms.
- **Retail clients:** The wire format is unchanged: `CreatePlayerEvent` carries the name in a
  32-byte field (31 characters and a terminator, Linux ia-32 server `CreatePlayerEvent` serialize
  and deSerialize) and `ChangePlayerNameEvent` up to 31, so names of 24 to 27 characters arrive
  whole. On the receiving side the recovered client code keeps player names in `std::string`
  (player, scoreboard, squad list, kill and chat messages, nametags, whose format buffer has 4096
  bytes), the end-of-round reader uses a 256-byte buffer, and the PunkBuster client copies at most
  32 bytes into a 33-byte field; no fixed buffer smaller than 28 bytes holds a player name. A
  longer name can be drawn wider in the scoreboard and nametags; that was not checked in a retail
  client.
- **Source:** `Code/BF2/Game/GameServer/GameServer.cpp`

### Content errors are logged without ending the game

- **Retail:** Four content checks emit Error (Debug type 6), which ends the process when
  enabled: `BundleTemplate::addBundleChilds` reports a missing child other than an optional
  `S_` sound on a non-dedicated client (`BF2.exe` `0x62892C`–`0x6289DA`);
  `KitTemplate::addBundleChilds` reports any missing child (`BF2.exe` `0x594A10`,
  `bf2_w32ded.exe` `0x55CA70`, Linux ia-32 server `0x8185CE0`);
  the client `EventManager::postEvent` overloads and node `postEventNextUpdate` report
  "Attempted use of unregistred event" for an unregistered category/ID, subject to their
  existing exemptions (`BF2.exe` `0x68DB0E`, `0x68DCCB`, `0x68DE0B`); and
  `SoldierTemplate::makeScript` reports "SoldierTemplate doesn't have makeScript implemented.
  Save failed..." (`BF2.exe` `0x5F4870`, Linux ia-32 server `0x81B8C60`). The control flow
  following these diagnostics already skips the missing child, dispatches or queues the event,
  or returns false without writing a save. Termination prevents those failure paths from
  completing. No retail game reproduction was run.
- **OpenBF2:** These checks write their original text as Error through the normal typed log
  and console sinks without invoking fatal diagnostic handling. Before the console exists,
  the message goes to stderr. Repeats are suppressed for the process lifetime: one message
  per parent/missing-child pair at each template site, per event category/ID across the three
  event checks, and per soldier template name. The existing runtime dedicated guards,
  optional-sound exemption, event exemptions, child IDs, dispatch order and failed-save
  result remain. Other Error diagnostics and all frontiers keep their termination policy.
- **Applies to:** Client on all platforms for the bundle and event checks; client and server
  on all platforms for kit children and soldier saves. No platform-specific selection.
- **Retail clients:** Only local content diagnostics change. Valid children, event payloads,
  registrations, packet formats and save formats are unchanged. A host skips an unavailable
  kit child as the existing failure path specifies, and a refused soldier save still returns
  false with no output. No client message or protocol transition changes.
- **Source:** `Code/BF2/Core/ContentError.h`, `Code/BF2/Core/EventManager.cpp`,
  `Code/BF2/Game/Objects/Base/BundleTemplate.cpp`,
  `Code/BF2/Game/Objects/Game/KitTemplate.cpp`,
  `Code/BF2/Game/Objects/Game/SoldierTemplate.cpp`,
  `Code/Tests/content_errors_runtime.cpp`, `Code/Tests/content_errors_runtime.py`.

### Wrong-length voice challenge datagram is dropped instead of ending the server

- **Retail:** `VoipServerConnection::HandlePacketChallengeString` (Linux ia-32 server `0x8127600`,
  `bf2_w32ded.exe` `0x54AEA0`) answers any length other than 10 with the Error diagnostic
  "BFVoipChallengeString packet length is invalid", and an emitted Error ends the process
  (`Debug::~Debug`, Linux ia-32 server `0x83C0580`). The voice channel is a bound UDP socket read
  from any sender, so one datagram whose last byte is 104 can end the server. Not reproduced on a
  retail server.
- **OpenBF2:** A length other than 10 returns silently, without a log line, since a line per
  malformed datagram would itself flood the log. A 10-byte challenge is handled as in retail.
- **Applies to:** Client and server, wherever a game is hosted; all platforms.
- **Retail clients:** The challenge travels between the voice server and the game server, not to or
  from game clients, and a 10-byte challenge takes the retail path. Only a malformed datagram is
  treated differently: it is ignored.
- **Source:** `Code/BF2/Game/GameServer/VoipServerConnection.cpp`

### The server prints the internet-backend failure text instead of a pointer

- **Retail:** When the matchmaking services cannot be initialized, `GameServer::init` builds the
  text "FATAL ERROR: Your server cannot communicate with the required internet backends..." in an
  `ostringstream` and then streams the stream object itself into both diagnostics through
  `operator<<(const void*)` (Linux ia-32 server `0x80A72E1`, `0x80A73A3`; `bf2_w32ded.exe`
  `0x44AEF3`, `0x44AF74`; `BF2.exe` `0x4CFB03`, `0x4CFB84`). The server operator sees a pointer
  value instead of the text.
- **OpenBF2:** Both diagnostics carry the text itself (`message.str()`).
- **Applies to:** Client and server, wherever a game is hosted; all platforms.
- **Retail clients:** Only console and log text of the hosting process changes. Nothing is sent to
  clients.
- **Source:** `Code/BF2/Game/GameServer/GameServer.cpp`

### FireComp::requestFire tolerates a firearm without a player

- **Retail:** When the weapon template names an ignored fire button, `FireComp::requestFire` calls
  `getCurrentFiringButton` on the firearm's player without a null test (Linux ia-32 server
  `0x82630C0`, dereference at `0x82630F2`; `BF2.exe` `0x6202A0`, dereference at `0x6202C0`), so a
  firearm without a player crashes there. With no ignored button the player is never read. When a
  firearm has no player in play was not established.
- **OpenBF2:** A null player skips only the ignored-button comparison. The rest of the function runs
  as in retail, on the path retail takes when no button is ignored.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** Nothing a client sends or receives changes. With a player the function is
  retail's; without one the server keeps running where retail crashes.
- **Source:** `Code/BF2/Game/Objects/Weapons/FireComponents/FireComp.cpp`;
  [#30](https://github.com/kr4uzi/OpenBF2/issues/30)

### AIPlayer::getPlayerControlObjectId tests the vehicle for null before using it

- **Retail:** `AIPlayer::getPlayerControlObjectId` fetches the player's vehicle and reads the
  vehicle's template before any null test. The Linux image tests the pointer only afterwards,
  answering -1 (Linux ia-32 server `0x83FC6C8`, test at `0x83FC6D3`); the Windows images have no
  test at all (`bf2_w32ded.exe` `0x752F7B`, `BF2.exe` `0x82625B`). A player without a vehicle
  therefore faults. Whether a bot's player can be without a vehicle when this runs was not
  established.
- **OpenBF2:** Tests the vehicle first and answers -1 for null, the answer the Linux image's late
  test names; otherwise identical.
- **Applies to:** Client and server, wherever bots run; all platforms.
- **Retail clients:** AI-interface code inside the hosting process. No message changes; where retail
  would fault, the host carries on.
- **Source:** `Code/BF2/AI/AIInterface/BFKnowledge/AIPlayer.cpp`

### SAI::releaseUnitFromObject skips the second free at the end of the set

- **Retail:** After `BotInfo::removeAssigned` has erased the unit and advanced the set iterator,
  `SAI::releaseUnitFromObject` reads the iterator's value again and calls `free` on that bot without
  an end test (Linux ia-32 `libaidll.so` `0x32D6D0`: reload at `0x32D733`, call at `0x32D74F`). When
  the released unit was the last one, the value comes from the set's header node and is used as a
  bot index. Whether the Windows AI libraries do the same is not known.
- **OpenBF2:** Calls `free` only when the iterator is not at the end of the set. Retail's quirk of
  freeing the next assigned unit, not the released one, is kept.
- **Applies to:** Client and server, wherever bots run; all platforms.
- **Retail clients:** The change is bot strategy bookkeeping inside the hosting process; no protocol
  data changes.
- **Source:** `Code/BF2/AI/AIDLL/AI/AIStrategic/SAI.cpp`

### The ClusterNodeManager destructor reads the chain link before deleting the record

- **Retail:** The destructor walks the hashed path-finding records, deletes each record and then
  reads the deleted record's next link at `+8` to continue the walk: a use after free at AI teardown
  (Linux ia-32 `libaidll.so` `0x3201E0`, delete at `0x32025B`, read at `0x320269`; the same order in
  `AIDLL_w32ded.dll` `0x10171350` and in `AIDLL.dll` at `0x100DA4D2`).
- **OpenBF2:** Reads the link first, then deletes the node and the record: the same walk, the same
  frees, the same counters.
- **Applies to:** Client and server, wherever the AI library is loaded; all platforms.
- **Retail clients:** Only the order of a read and a free during AI teardown inside the process
  changes; nothing on the wire changes.
- **Source:** `Code/BF2/AI/AIDLL/AI/AIPathFinding/Cluster/ClusterNodeManager.cpp`

### AI control-point queries reject missing IDs

- **Retail:** `BFGame::getOwnerOfControlPoint`, `getControlPointTakeOverCounter` and
  `getControlPointInfo` dereference the result of the control-point map's `find` without an
  end check (`BF2.exe` `0x8164E0`, `0x816520`, `0x816560`; Linux ia-32 `bf2` `0x83FF220`,
  `0x83FF270`, `0x83FF2C0`). `AIStrategicArea::postLoadInit` passes its scripted ID to the
  info query and discards its return (`AIDLL.dll` `0x1006BB30`, call at `0x1006BB81`;
  Linux ia-32 `libaidll.so` `0x337E20`). An absent ID reads past the map's end node and can
  crash during level loading. Highway Tampa's installed `server.zip` has an empty
  `GameModes/gpm_cq/16/AI/StrategicAreas.ai` and control points 101, 103, 104 and 109 in
  that layer's `GamePlayObjects.con`. Its `GameModes/sp1/16/AI/StrategicAreas.ai` instead
  names 602 (`CPNAME_MC_SP16_hotel`), 604, 603 and 601; none belongs to the conquest layer,
  and the sp1/16 gameplay file defines no control points at all. The hotel is the first area
  in name order, so loading those strategic areas reaches missing ID 602 first.
  `AIStrategicArea::init` itself reads no control-point object: AIDLL.dll `0x10065DEF`
  loads the cached `IAITerrain` pointer from `0x10160E60`, dereferences its vtable at
  `0x10065DF5` and calls terrain altitude at `0x10065E04`. It then reads the pathfinding
  singleton (`0x10065E1E`) and queries used maps/polygons. Post-load's position and radius
  values cannot make a null terrain pointer safe. The owner's native crash report
  (`bf2_client-2026-11-24-235450.ips`, `init()+46`, `rdi=0`) identifies that terrain
  dereference. Retail also requires a live terrain adapter here; surviving an unchecked
  end-node read is heap-dependent undefined behavior, not a retail missing-area policy.
- **OpenBF2:** Tests for the end iterator in all three queries. A missing ID returns neutral
  owner 0, capture progress 0, or false with position, radius and active outputs zeroed.
  Existing IDs keep their live queries, including position and radius for points that cannot
  change teams. No map entry is inserted, and content files are unchanged. Defined failure
  outputs also cover AI callers that discard the info result. The info query's Boolean now
  reports ID existence, while `active` still reports whether the point can change teams.
  Retail returns `active`; every existing production caller discarded that Boolean. The
  area position/radius helper now propagates existence so a fixed-team main base remains valid.
  This is an intentional internal return-contract change between rebuilt images, with no
  signature, virtual-slot or object-layout change; rebuild the host and AIDLL together.

  Before any post-load area initialization, the strategic-object manager logs and removes
  control-point areas whose IDs are absent. It unlinks both neighbor vectors, name tables,
  active selection and category entries, deletes their waypoints and areas, and compacts
  the object/category indices and surviving waypoint keys together. Deferred array pointers
  are initialized to null so rejected areas can be destroyed without running their deferred
  constructor work. Debug primitives are cleared using their old indices before compaction.
  Bounded areas are retained. An empty roster skips `SAI::update` before strategy/planning
  work; respawn/position notifications and owned-point tests also reject a null closest area.
  This avoids publishing a phantom neutral target or ordering bots toward the zero position.

  The AI-side consumer audit covers area owner/capture updates, takeability, radius/order
  positioning, SAI owned/takeable queries, `VehicleUrgencyCalculator`, all three
  `BBPFireInfantery` flag branches, squad taking-flag checks, and `BotSpawner`'s spawn-group
  control-point IDs. They use surviving areas, the guarded BFGame queries, or already reject
  a null closest object. Strategies store type/status modifiers rather than control-point IDs;
  their area lists and bot orders are populated from the filtered manager roster. This does
  not initialize a missing terrain adapter: a valid area still requires terrain and pathfinding.
- **Applies to:** Client and server, all platforms. This is a shared retail fix under the rule
  above, rather than an omission limited to a native platform arm.
- **Retail clients:** The change handles invalid local AI lookups. It changes no network interface,
  control-point ID, packet, state transition or file format; valid geometry, ownership,
  capture eligibility and retail-client communication are unchanged. It does not supply
  missing AI/navigation content.
- **Source:** `Code/BF2/AI/AIInterface/BFKnowledge/BFGame.cpp`;
  `Code/BF2/AI/AIDLL/AI/AIStrategicEntity/AIStrategicArea.cpp`,
  `AIStrategicObjectManager.cpp`; `Code/BF2/AI/AIDLL/AI/AIStrategic/SAI.cpp`;
  `Code/Tests/aidll/control_point_lookup_runtime.cpp`, `control_point_area_runtime.cpp` (headless regression check,
  `bf2_control_point_lookup_check`).

### BoneAnimation::load keeps channel samples inside the declared sample block

- **Retail:** The `.baf` loader allocates the declared number of 16-bit samples per bone and then,
  for each of seven channels, reads a count and copies that many samples to the running offset.
  Nothing compares offset + count with the declared number, so a malformed animation file overruns
  the heap block (Linux ia-32 server `0x8338820`, copy at `0x8338D1F`; the same loop in `BF2.exe`
  `0x7FD1F0`). Not reproduced with a real file.
- **OpenBF2:** Clamps each channel's count to what is left of the bone's sample block and advances
  the file cursor by the clamped amount. Files whose channels fit their declared block load
  identically.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** Only the parsing of a local animation file whose channels exceed their
  declared block changes; files that fit load identically, and nothing on the wire changes.
- **Source:** `Code/BF2/Animation/BoneAnimation/BoneAnimation.cpp`

### The skeleton loader keeps a bone name inside its 256-byte buffer

- **Retail:** The version-2 skeleton loader reads a 16-bit name length per bone and then reads that
  many bytes into a 256-byte stack buffer with no bound (Linux ia-32 server `0x834AE10`: length read
  at `0x834B0BB`, name read at `0x834B0DB`). A length above 256 overruns the stack, and the name is
  terminated only by the file's own zero byte. Whether the Windows images do the same is not known.
- **OpenBF2:** Reads at most 255 bytes into the buffer, skips the rest of the name in the stream and
  terminates the string.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** A local file is parsed. The 20 shipped skeletons (names up to 19 bytes, each
  with its zero byte) load identically, and no protocol or file format changes.
- **Source:** `Code/BF2/Animation/BoneAnimation/Skeleton.cpp`

### Console run and include free the script buffer on a short read

- **Retail:** `OldConsole::run` and `OldConsole::include` allocate the file size plus 2 bytes with
  `new[]`. When `File::read` returns fewer bytes than the file size they set
  "Could not read file: ...!" and return false without `delete[]`, so the buffer leaks (Linux ia-32
  server: `run` `0x844D980` from `0x844DB96`, `include` `0x844E9E0` from `0x844ED31`; `BF2.exe`:
  `run` `0x6A11F0`, `include` `0x69DBA0`).
- **OpenBF2:** Both failure paths free the buffer before they return false. The message, the
  restored access level and the return value are unchanged.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** Only process memory changes; nothing is sent or received differently.
- **Source:** `Code/BF2/IO/Console/Console.cpp`

### tool::getAsString no longer writes past its string

- **Retail:** `tool::getAsString` builds `std::string(getPtr())`, whose length comes from `strlen`
  and not from the stream size, and then writes a NUL at `data + getSize()` (Linux ia-32 server
  `0x8329DE5`; `bf2_w32ded.exe` `0x6EF6F0`; `BF2.exe` `0x730D40`). When the stream size exceeds the
  C-string length, that write lands past the string's storage. It serves `Object.printScript` and
  `ObjectTemplate.printScript`, among others. No such stream was shown on retail.
- **OpenBF2:** Returns `std::string(getPtr(), getSize())`: exactly the stream's bytes, the same text
  for scripts without an embedded NUL, and no write outside the string.
- **Applies to:** Client and server; all platforms.
- **Retail clients:** Only the text of a console command's answer is built here, and it is the same
  text for every script without an embedded NUL. No message changes.
- **Source:** `Code/BF2/Scene/Object/ToolObject/ObjectTemplateAdm.cpp`

### socket.inet_ntop no longer writes one byte past its buffer

- **Retail:** CPython 2.3.4's `socket_inet_ntop` zeroes `sizeof(ip) + 1` bytes. In the Linux ia-32
  server's `libdice_py.so` (`0xA9557`) the 17-byte buffer is cleared with 18 bytes, so the last zero
  lands on the low byte of the stack-protector value that the function checks before it returns
  (`0xA961D`); where that byte is not already zero, the process aborts. The amd-64 build only writes
  padding, and the Windows `dice_py.dll` has no `inet_ntop`. Reached only when a script calls
  `socket.inet_ntop`; the stock server scripts do not.
- **OpenBF2:** The POSIX branch zeroes exactly `sizeof(ip)` bytes. Results are unchanged:
  `inet_ntop` is given `sizeof(ip)` as its limit, terminator included.
- **Applies to:** Client and server, wherever a game is hosted; Linux and macOS.
- **Retail clients:** Only the Python socket module inside the hosting process changes; no game
  message does.
- **Source:** `Code/BF2/External/python-embedded/py-src-2.3.4/Modules/socketmodule.c`,
  `Code/BF2/External/python-embedded/linux/README.md`

### POSIX mutexes are recursive

- **Retail:** The Linux image creates `dice::hfe::Mutex` with `pthread_mutex_init(p, NULL)`, a
  non-recursive mutex (Linux ia-32 server `0x83D2CC0`), while shared code relocks on one thread:
  `CBF2VoipServer::Shutdown` locks its mutex (`0x84EC920`) and calls `ReleaseAllInstances`, which
  locks it again (`0x84EC6CD`). That path deadlocks on itself. The Windows voice server uses
  critical sections, which allow it. No retail hang is known; on quit the Linux server's `main`
  never destroys the game.
- **OpenBF2:** POSIX mutexes are created with `PTHREAD_MUTEX_RECURSIVE`, which matches the Windows
  critical sections. A thread may lock a mutex it already holds; other threads are excluded as
  before. The hang this prevents was seen at local-host shutdown on macOS.
- **Applies to:** Client and server; POSIX builds only (Linux and macOS).
- **Retail clients:** Lock behavior inside the process only. Nothing on the wire changes.
- **Source:** `Code/BF2/Core/Mutex.cpp`, `Code/Tests/mutex_runtime.cpp`,
  `Code/Tests/voip_server_shutdown_runtime.cpp`

### Win32 crash dumps are written: no doubled separator in the dump name

- **Retail:** The crash handler names the dump `<directory>` + `\bf2_v<build>.dmp` (`BF2.exe`
  `0x402000`), although the directory already ends in the separator after `dmp` (`0x6C6400`), so the
  name has a doubled separator. The writer (`0x433A90`) opens it as `\\?\<name>`, a prefix that
  turns off path normalization, and skips the dump when `CreateFileW` fails. A probe on Windows 11
  gets `ERROR_INVALID_NAME` for such a name; a retail crash was not observed. `bf2_w32ded.exe` has
  the same code (`0x401410`).
- **OpenBF2:** `crashDumpFileName` leaves out its separator when the directory already ends in one,
  so the dump is written into the `dmp` folder below `<Documents>\Battlefield 2`.
- **Applies to:** Client and server; Windows x86 and x64.
- **Retail clients:** The fix changes only the name of a local file that a crashing process writes;
  nothing on the wire changes.
- **Source:** `Code/BF2/Game/Main/BF2.cpp`, `Code/Tests/w64_crash_dump_runtime.cpp`;
  [#58](https://github.com/kr4uzi/OpenBF2/issues/58)
