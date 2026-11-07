# Local MojoShader changes

[CHOSEN, not recovered] Owner-requested macOS tooling. Patches apply only to the
worktree-local upstream copy, never to the engine or the supplied upstream source.
Based on MojoShader ad5dff84830c2863c841f4b1f4e3df78c705b383, under its zlib
license (`LICENSE.txt` in that source). These alterations are marked in source;
the upstream copyright and license remain intact. Local patch contributions use
the same zlib license.

`0001-metal-sm1-texture-inputs.patch` adds implicit pre-1.4 pixel texture registers
to the attribute list. The existing emitter already initializes mutable `tN`
locals from `input.tN`, but ordinary TEX does not declare that attribute. This
produces missing members or a missing stage input and dangling sampler comma.
The patch lets the existing attribute emitter generate `user(texcoordN)` and the
existing finalizer generate the stage input. No shader instructions change.

`0002-effect-shader-references.patch` handles FX2 large shader objects whose type
is 1: a parameter name, optionally with a fixed array subscript. It retains the
parameter/element and resolves the live shader on every CommitChanges. This
avoids feeding a name to the bytecode parser. It also skips uninitialized shader
slots during error cleanup/clone and fixes the zero-symbol selector loop. The
existing `is_preshader` field uses 2 for a parameter reference (1 remains an
actual selector preshader); consumers must distinguish them when inspecting.
Controls: `shader-references.fx`, `reference_runtime.c`, negative mutations in
`check-coverage.py`; the real BundledMesh, StaticMesh and SkinnedMesh effects.

`0003-metal-sm1-texture-operations.patch` implements TEXCRD/TEXCOORD and SM1.4
TEXLD, including implicit coordinates/samplers, destination-selected texture
stages, dependent temporary coordinates, source swizzles, `_dz`/`_dw` division,
and destination masks. Pre-1.4 TEXCOORD reloads and clamps the interpolator;
SM1.4 TEXCRD copies without that clamp. Sampler maps can still select cube/volume
types; the default remains 2D. Controls: `sm1-texture-operations.fx` and three
standalone SM1.1/1.2/projection controls in `check-coverage.py`. All original 751
MSL sources remain byte-for-byte identical and pass Metal again.

`0004-effect-state-expressions.patch` fixes the invalid object-index read for
nonliteral render/sampler states (including empty sampler references). It keeps
the FX2 expression type and owned payload in `effectValue`, deep-copies it on
clone, and frees it on deletion. This changes the local MojoShader C structure
layout: rebuild the helper and library together; no engine ABI changes.
`effectparse` reports these values as requiring an evaluator. BeginPass or
CommitChanges at an affected active state emits a qualified `[FRONTIER]`, flushes
stderr and aborts; it never publishes a made-up literal value. Retire that stop
only when typed expression evaluation and its runtime validation exist.
Controls: `state-expressions.fx`, payload/clone checks and an expected frontier
abort in `check-coverage.py`; ASan parsing of all 222 successful fixture/corpus
effects. Retaining a graph is not state execution.

The build applies patches with zero fuzz, accepts an already applied patch only
after a reverse dry run, and fails on other source drift. `patches.json` in the
tool build records all four altered source files and patch hashes. No generated
MSL is rewritten. These patches cover the default corpus snapshot, not every
possible Direct3D shader feature. The upstream effect parser is still not a
general untrusted-input parser.

`0005-metal-array-uniform-define.patch` drops the trailing `;` from the Metal
profile's `#define cN uniforms.uniforms_float4[i]` for float constants that fall
inside a relatively addressed array. Those macros are used inside expressions
(`r0.w = r0.y + -c1.w;`), where the semicolon ended the statement early: Metal
reported "assigning to 'float' from incompatible type 'float4'" plus "expected
expression". It broke 464 of 3,227 BundledMesh libraries in the c-fx-permutations
prebuild (skinned-bone arrays at `ARRAYBASE_0`). The non-array branch already
emitted the macro without a semicolon. No instruction emission changes.

`0006-metal-log-abs.patch` (from the native-compile package, c-native-fx): Direct3D 9 `log`
is log2(|src|), as `rsq` uses |src|. The Metal profile emitted log2(src), which turns
pow(x, y) lowered to exp(y * log(x)) into NaN for negative x. D3DX's vs_1_1 code and vkd3d's
pow lowering rely on the D3D9 semantics.

`0007-metal-vs1-expp.patch` (c-native-fx): vs_1_x `expp` writes x = 2^floor(s),
y = s - floor(s), z = 2^s, w = 1. The Metal profile emitted exp2(s) everywhere, but D3DX
compiles `frac()` in vs_1_1 to `expp .y` (Decals' instance index, SwiffMenu's sin/cos range
reduction), so those shaders computed garbage. From vs_2_0 on expp stays a scalar exp2.
