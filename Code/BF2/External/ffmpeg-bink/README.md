# FFmpeg Bink decoders (vendored subset)

The macOS client and the Windows x64 client play Battlefield 2's Bink 1 movies
(`mods/*/Movies/*.bik`) with FFmpeg's Bink video and Bink audio (DCT) decoders, linked
statically: into `bf2_client` on macOS, into `RendDX9.dll` on Windows x64
(`Code/Build/renddx9.cmd x64`). Windows x86 keeps RAD Game Tools' `binkw32.dll` and compiles
nothing in this directory. Owner decisions 2026-11-01 (macOS) and 2026-11-07 (x64); the
integration and the validation are in [docs/macos-c-bink.md](../../../../docs/macos-c-bink.md)
and [docs/tracks/bink/README.md](../../../../docs/tracks/bink/README.md).

## Provenance

- FFmpeg 7.1.1, the official release tarball from ffmpeg.org
  (`ffmpeg-7.1.1.tar.xz`, SHA-256
  `733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1`).
- Files taken, byte-for-byte unmodified (SHA-256 of each copy):

| File | SHA-256 |
| --- | --- |
| `libavcodec/bink.c` | `e132ee588345c9f7675c3fa0f58b3b40eb1a355839734188ab2be49e9f63b2d0` |
| `libavcodec/binkaudio.c` | `031808c16afec24009a5b47f74d2ff00eb52406b0bb4e85544b0ad44354258e6` |
| `libavcodec/binkdata.h` | `e549d4f3df3debd0de047593db32ce9d671de210475c76aefd1a741b815b4080` |
| `libavcodec/binkdsp.c` | `2e23cb9d22c5353aa8a1d37dbaf5844cb4a2bac9bee2d42a321ed31db7c3b604` |
| `libavcodec/binkdsp.h` | `f5cc98c60913a070fbd3f4674153b92db271ce1235066ae1228b23e4aad894ff` |
| `libavcodec/wma_freqs.c` | `c8992e806736dd708015c2121bf1773e4c4bbf3201840cbdf7f6c5f3754ee46c` |
| `libavcodec/wma_freqs.h` | `aeef407a065e961f18176fb17918c403f69686e6eb84d717bd688b1511789a56` |
| `COPYING.LGPLv2.1` | `b634ab5640e258563c536e658cad87080553df6f34f62269a21d554844e58bfe` |
| `LICENSE.md` | `cb48bf09a11f5fb576cddb0431c8f5ed0a60157a9ec942adffc13907cbe083f2` |

Local changes to FFmpeg files: none. FFmpeg's build system, configure script, assembly and the
rest of libavutil/libavcodec/libavformat are not included.

## The shim (OpenBF2 port code)

`shim/` is written for OpenBF2, not taken from FFmpeg. It replaces the FFmpeg infrastructure the
two decoders include:

- One-line forwarders with FFmpeg's header names (`avcodec.h`, `get_bits.h`, `libavutil/tx.h`,
  ...) to `shim/ffbink_shim.h`, so the FFmpeg sources compile unmodified.
- `ffbink_shim.h` / `ffbink_shim.c`: a little-endian checked bit reader, single-level LE VLC
  tables, refcounted frames and buffers, the C block/pixel helpers bink.c selects (`clear_block`,
  `fill_block`, `put_pixels8/16`), logging, aligned allocation, and the inverse DCT
  (DCT-III, an FFT-based implementation scaled like FFmpeg's `AV_TX_FLOAT_DCT`).
- `ffbink.h`: a small C interface (open/decode/flush/close for video and audio) used by
  `Code/RendDX9Abi/BinkFFmpeg.cpp`, which implements the Bink API calls the game makes and reads
  the `.bik` container itself.
- Compiler portability (2026-11-07): GCC/clang attributes on macOS; on MSVC `__forceinline`,
  `_BitScanReverse`, `__declspec(align)`, `__declspec(noreturn)`, `_USE_MATH_DEFINES`, and on
  Windows `InitOnceExecuteOnce` for `ff_thread_once` and `_aligned_malloc`/`_aligned_free`.
  MSVC compiles the FFmpeg files as C11 (`/std:c11 /O2`).

Not included: the Bink audio RDFT variant (`binkaudio_rdft`). `av_tx_init` refuses
`AV_TX_FLOAT_RDFT`, so such a track stops at a named `[FRONTIER]` in `BinkOpen`. No shipped
Battlefield 2 movie has one (all audio tracks are `binkaudio_dct`). Bink 2 (`KB2`) files are
refused by `BinkOpen`, as FFmpeg's demuxer also does not decode them.

## Licence

The FFmpeg files are licensed under the GNU Lesser General Public License, version 2.1 or (at
your option) any later version; see `COPYING.LGPLv2.1` and `LICENSE.md` and the notice at the top
of each file. Copyright remains with the FFmpeg authors named there (Konstantin Shishkov, Peter
Ross, Daniel Verkamp and others). None of the files is under one of FFmpeg's GPL-only parts.

The OpenBF2 repository as a whole is GPL-3; LGPL-2.1-or-later code may be combined with it and
linked statically (LGPL 2.1 section 3 lets the code be used under the GPL). This holds for both
images that link these files: the macOS `bf2_client` and the Windows x64 `RendDX9.dll`. Anyone
distributing either binary must also provide the corresponding source for these files (and,
since the whole is then GPL-3, for the program), which this repository does; keep
`COPYING.LGPLv2.1` and `LICENSE.md` with the distribution. The Windows x86 build does not
contain them. The shim files are OpenBF2 code under the repository's licence.

## Updating

Copy the same seven files from a newer FFmpeg release, update the hashes above, and run
`bf2_bink_decode_test` (docs/macos-c-bink.md) and the black-box harness
(docs/tracks/bink/README.md), and build `renddx9.cmd x64` with MSVC. If a decoder starts to use more of libavutil or
libavcodec, extend the shim; do not edit the FFmpeg files.
