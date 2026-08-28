# BitXRay

A cross-platform H.264/H.265 elementary-stream analyzer and visualizer. It
combines NALU-level protocol parsing (NALU list, hex view, SPS/PPS syntax
tree) with frame-accurate video playback, keeping raw bytes, syntax elements,
and the decoded picture in sync.

See `需求与架构设计文档：H.264H.265 Bitstream Analyzer & Visualizer.md` for the
full requirements/architecture spec and `CLAUDE.md` for a contributor-facing
summary of the mandatory three-tier architecture.

## Building

Requires:

- CMake 3.21+
- A C++17 compiler
- Qt6 (Widgets)
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

- **NALU list** (left): one row per NALU with its offset/length/type; I/P/B
  slices and parameter sets are color-coded. Selecting a row updates the hex
  view and (for SPS/PPS rows) the syntax tree.
- **Hex view** (top right): the selected NALU's bytes, hex + ASCII.
- **Syntax tree** (bottom right): decoded SPS/PPS fields (resolution,
  profile/level, chroma format, entropy coding mode, etc.) for the selected
  parameter-set row.
- **Video preview** (bottom left): Play/Pause and Prev/Next frame-step
  controls. Clicking an I/P/B slice row seeks the player to that exact
  decoded frame; conversely, playing/stepping through video auto-scrolls and
  highlights the corresponding NALU row — both directions stay in sync.

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
  `SyntaxTreeModel` (tree), `VideoPreviewWidget` (decode-on-demand playback
  with a small B-frame decode-ahead cache and GOP-keyframe-restart seeking —
  see the comment at the top of `VideoPreviewWidget.h` for why), and
  `MainWindow` wiring the bidirectional sync between the table and player.
- `tests/` — GoogleTest unit tests covering `core_parser` (31 cases,
  including SPS/slice-header parsing validated against bytes captured from
  real FFmpeg-encoded streams).
