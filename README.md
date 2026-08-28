# BitXRay

A cross-platform H.264/H.265 elementary-stream analyzer and visualizer. It
combines NALU-level protocol parsing (NALU list, hex view, SPS/PPS/slice-header
syntax tree) with frame-accurate video playback, keeping raw bytes, syntax
elements, and the decoded picture in sync — bidirectionally, in both
directions: clicking a NALU seeks the player, and playback highlights the
corresponding NALU row.

See `需求与架构设计文档：H.264H.265 Bitstream Analyzer & Visualizer.md` for the
original requirements/architecture spec and `CLAUDE.md` for a contributor-facing
summary of the mandatory three-tier architecture. See `CHANGELOG.md` for the
project's development history.

## Features

- **NALU list**: one row per NALU (offset/length/start-code/type), color-coded
  — SPS/PPS/VPS are each a distinct color, and I/P/B/SP/SI slices are each a
  distinct color. Slices are numbered `#0`, `#1`, `#2`... relative to the most
  recent keyframe, so GOP length is visible at a glance.
- **Hex view**: the selected NALU's raw bytes, hex + ASCII, byte-for-byte.
- **Syntax tree**: full decoded field breakdown for whatever's selected — SPS
  (resolution, profile/level, chroma format, bit depth), PPS (entropy coding
  mode and friends), or a full `slice_header()`/`slice_segment_header()`
  breakdown for any I/P/B slice (reference picture list modification,
  weighted prediction, deblocking, reference-picture-set info, etc.) — not
  just parameter sets.
- **Embedded video preview**: Prev/Play/Next frame-stepping panel synced to
  the NALU table in both directions.
- **Standalone player window**: a real continuous video player (Play/Pause,
  Stop, Prev/Next frame-step, screenshot save, loop playback) modeled on
  H264BSAnalyzer's reference player — deliberately no seek bar/scrubbing,
  since arbitrary seeking on a raw elementary stream (no container index)
  means restarting decode from a keyframe, which a slider invites you to spam.
- Handles both H.264 and H.265, including streams with B-frames (decode order
  vs. display order are kept distinct throughout — see
  `src/ui/VideoDecodeEngine.h`).

## Building

Requires:

- CMake 3.21+
- A C++17 compiler
- Qt6 (Widgets, Svg)
- FFmpeg 6.x (libavcodec, libavutil, libswscale)

```sh
cmake -B build \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/<kit> \
  -DFFMPEG_ROOT=/path/to/ffmpeg-6.x-dev-tree
cmake --build build
ctest --test-dir build
```

`FFMPEG_ROOT` must point at a tree containing `include/libavcodec/avcodec.h`
and `lib/libavcodec.dylib` (or the equivalent for your platform). If FFmpeg is
registered with `pkg-config` or installed system-wide, `FFMPEG_ROOT` can be
omitted.

## Downloads

Pre-built binaries for macOS, Linux, and Windows are attached to each
[GitHub Release](https://github.com/DavidRedVest/BitXRay/releases), built
automatically by CI from the corresponding tag.

- **macOS**: the `.app` bundle is signed with an ad-hoc (unnotarized)
  signature — enough to run locally, but Gatekeeper will still flag it as
  from an unidentified developer on first launch. Right-click (or
  Control-click) the app and choose **Open** to bypass that once, or run:
  ```sh
  xattr -dr com.apple.quarantine BitXRay.app
  ```
- **Linux**: a portable directory bundling Qt/FFmpeg shared libraries next to
  the executable, with a launcher script that points `LD_LIBRARY_PATH` at
  them — not a system package (no `.deb`/`.rpm`/AppImage yet).
- **Windows**: a `.zip` with the executable and its Qt/FFmpeg DLLs
  side-by-side (via `windeployqt`) — unzip and run.

## Running

```sh
./build/src/ui/bitxray_ui                    # then File > Open...
./build/src/ui/bitxray_ui /path/to/clip.h264 # or load a file directly
```

Accepts raw Annex-B elementary streams (`.h264`/`.264`/`.h265`/`.265`/`.hevc`)
— not MP4/MKV/etc. containers. To extract one from a container:

```sh
ffmpeg -i input.mp4 -c:v copy -bsf:v h264_mp4toannexb -f h264 output.h264
```

### Using the app

- **NALU list** (left): select a row to update the hex view and syntax tree.
- **Hex view** / **syntax tree** (right): driven by the current NALU
  selection.
- **Video preview** (bottom left): Prev/Play/Next frame-stepping, synced to
  the NALU list in both directions — selecting a slice row seeks the player
  to that exact decoded frame, and stepping/playing scrolls and highlights
  the matching row.
- **Play button** (toolbar): opens the standalone player window for
  continuous playback of the whole video, with Play/Pause, Stop, Prev/Next,
  screenshot, and loop controls.

## Project layout

- `src/core_parser/` — Layer 1: pure C++ Annex-B/SPS/PPS/slice-header parser,
  zero third-party dependencies. Types: `NaluExtractor`, `BitReader`,
  `ExpGolomb`, `H264SpsPpsParser`, `H265SpsPpsParser`, `SliceHeaderParser`.
- `src/core_player/` — Layer 2: `VideoDecoder`, wrapping libavcodec directly
  (not libavformat, since input is a raw elementary stream, not a
  container). Feeds one NALU at a time so decoded frames can be correlated
  exactly with the NALU list; a `pts` passed through per-NALU is what makes
  that correlation work even with B-frame reordering.
- `src/ui/` — Layer 3: Qt6 interface. `NaluListModel` (table), `HexView`,
  `SyntaxTreeModel` (tree); `VideoDecodeEngine` is the shared decode/seek/
  playback-timer engine (decode-on-demand with a small B-frame decode-ahead
  cache and GOP-keyframe-restart seeking — see the comment at the top of
  `VideoDecodeEngine.h` for why) used by both `VideoPreviewWidget` (the
  embedded frame-stepping panel) and `VideoPlayerWidget`/`PlaybackWindow`
  (the standalone player); `MainWindow` wires the bidirectional sync between
  the table and player.
- `tests/` — GoogleTest unit tests covering `core_parser` (31 cases,
  including SPS/slice-header parsing validated against bytes captured from
  real FFmpeg-encoded streams).
- `.github/workflows/` — CI: unit tests on every push, and a cross-platform
  release build (macOS/Linux/Windows) triggered by pushing a `v*` tag.

## Development history

This project was built iteratively — see `CHANGELOG.md` for what shipped in
each release and the notable bugs found (and fixed) along the way, most of
which only reproduced against real captured `.h264`/`.h265` files rather than
small synthetic test vectors.
