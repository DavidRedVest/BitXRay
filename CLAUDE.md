# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

BitXRay is a cross-platform H.264/H.265 elementary-stream analyzer and visualizer. It combines NALU-level
protocol parsing (NALU list, hex view, SPS/PPS syntax tree) with frame-accurate video playback, keeping
raw bytes, syntax elements, and the decoded picture in sync (bidirectionally — clicking a NALU seeks the
player, and playback highlights the corresponding NALU row).

The original Chinese requirements/architecture doc is
`需求与架构设计文档：H.264H.265 Bitstream Analyzer & Visualizer.md` — this file supersedes it for
day-to-day implementation guidance, but the doc is the source of truth for *why* the constraints below
exist.

## Commands

```sh
# Configure (Qt6/FFmpeg paths are typically not on a system default search path)
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/<kit> -DFFMPEG_ROOT=/path/to/ffmpeg-6.x-dev-tree

# Build everything
cmake --build build -j 8

# Run all unit tests (core_parser only currently has tests)
ctest --test-dir build
# or, for verbose per-test output:
cd build && ctest --output-on-failure

# Run a single test by name (GoogleTest filter, works via ctest -R too)
./build/tests/core_parser/core_parser_tests --gtest_filter='H264SpsPpsParser.*'
ctest --test-dir build -R SliceHeaderParser

# Run the app
./build/src/ui/bitxray_ui                    # then File > Open...
./build/src/ui/bitxray_ui /path/to/clip.h264 # or load a file directly on launch
```

`FFMPEG_ROOT` must contain `include/libavcodec/avcodec.h` and `lib/libavcodec.dylib` (or platform
equivalent); omit it if FFmpeg is discoverable via `pkg-config` or a system install. `core_player`'s
`CMakeLists.txt` falls back to a default `FFMPEG_ROOT` — check it if configure fails to find FFmpeg.

Test input files are raw Annex-B elementary streams (`.h264`/`.h265`), not MP4/MKV containers:
`ffmpeg -i input.mp4 -c:v copy -bsf:v h264_mp4toannexb -f h264 output.h264`.

## Architecture: Three-Tier Decoupling (must not be violated)

- **Layer 1 — Parser Core (`src/core_parser/`)**: pure C++, zero third-party dependencies (enforced by
  its `CMakeLists.txt` having no `find_package`/`target_link_libraries` beyond the standard library).
  Owns byte-offset ground truth for NALUs. Never add an FFmpeg or Qt include here.
- **Layer 2 — Player Engine (`src/core_player/`)**: wraps libavcodec directly (not libavformat — the
  input is a raw elementary stream, not a container). Its public API (`VideoDecoder`) is deliberately
  free of any `core_parser` type — it only takes raw byte spans (`feedNalu(data, size, pts)`) and a `pts`
  the caller assigns, never a `NaluInfo`. Never add a Qt include here.
- **Layer 3 — UI Layer (`src/ui/`)**: Qt6 Widgets. The only layer allowed to depend on both Layer 1 and
  Layer 2, and where all correlation between "a NALU" and "a decoded frame" happens (see
  `VideoPreviewWidget`).

## Layer 1 — `core_parser`

- `NaluExtractor` — scans an Annex-B buffer for `00 00 01`/`00 00 00 01` start codes, returns
  `std::vector<NaluInfo>` (offset/length/startCodeLen/codec/naluType). Codec-agnostic scan, codec-specific
  NAL-header interpretation (`detectCodec()` sniffs from the first VPS/SPS/PPS-shaped header found).
- `BitReader` / `ExpGolomb` — MSB-first bit reader and `ue(v)`/`se(v)` decoding. `unescapeRbsp()` strips
  emulation-prevention bytes (`00 00 03` → `00 00`) — **always required before parsing SPS/PPS/slice-header
  payloads**, and NAL header bytes must be stripped first (1 byte for H.264, 2 for H.265).
- `H264SpsPpsParser` / `H265SpsPpsParser` — parse SPS/PPS RBSP into plain structs (resolution derived from
  raw dimensions minus conformance-window/frame-cropping, profile/level, chroma format, bit depth). Each
  stops parsing once it has the fields the UI displays — full-spec completeness (e.g. scaling lists'
  values, VUI) isn't needed and isn't implemented.
- `SliceHeaderParser` — reads just enough of a slice header to classify I/P/B (H.264: `slice_type % 5`;
  H.265: needs `numExtraSliceHeaderBits` from the active PPS, defaults to 0 if unknown, and only handles
  the first-slice-segment-in-picture case).
- Every parse function returns `std::optional<T>` and internally catches `BitstreamOverrunError` —
  malformed/truncated input degrades to "no data" rather than throwing across the API boundary.

## Layer 2 — `core_player`

- `VideoDecoder::feedNalu(data, size, pts)` takes one Annex-B NALU **including its start code**. `pts` (if
  non-negative) is attached to the AVPacket and echoed back on `DecodedFrame::pts` — this is the only way
  to correlate a decoded frame with its source NALU once the decoder reorders for B-frames.
- `feedNaluPtsOnly`/`flushPtsOnly` are a lean variant that skip the RGB24/`sws_scale` conversion, used for
  cheaply learning decode-order-vs-display-order without materializing pixels (see below).
- Decoder threading is explicitly enabled (`thread_count = 0`, frame+slice threading) — without it,
  decoding a real multi-thousand-frame file is an order of magnitude slower than FFmpeg's own CLI.
- Colorspace conversion mirrors ffmpeg's own default heuristic (BT.601 for ≤576px height else BT.709,
  limited→full range) so decoded frames are pixel-comparable with `ffmpeg -i ... -frames:v 1 out.png` —
  this was load-bearing for validating correctness (see `VideoPreviewWidget` below).

## Layer 3 — `ui`

- `NaluListModel : QAbstractTableModel` — one row per `NaluInfo`, computed once in `load()` (not
  re-parsed per paint). Row index == NALU index used everywhere else in the UI (no separate ID mapping).
  Caches parsed SPS/PPS structs per row for the syntax tree.
- `HexView`, `SyntaxTreeModel` (a `QTreeWidget`, despite the name — a full `QAbstractItemModel` wasn't
  warranted for a flat field list) — driven by `MainWindow::onCurrentRowChanged`.
- `VideoPreviewWidget` — the trickiest piece; read the block comment at the top of `VideoPreviewWidget.h`
  before touching it. Two orderings matter and must not be conflated:
  - **NALU index** = bitstream/decode order = table row order.
  - **display position** = playback order ("Frame N" in the UI) — differs from decode order whenever
    B-frames are present (encoders emit e.g. `I0 P3 B1 B2 P6 B4 B5...`).
  `loadStream()` runs one lightweight pass (`feedNaluPtsOnly`) up front to learn the decode→display
  mapping from the decoder's own output order, since that's exactly what B-frame reordering already
  computes — re-deriving it from POC in slice headers would just duplicate that logic. Actual pixel
  decoding is on-demand: only a small decode-ahead cache is kept (real files can be too large to hold
  every decoded frame as a `QImage` simultaneously), and seeking restarts from the nearest preceding
  keyframe when the forward-only decoder can't reach the target. All decoder calls are wrapped in
  try/catch so a stream `core_parser` can list but FFmpeg can't actually decode degrades to "video
  preview unavailable" rather than crashing the app.
- `MainWindow` wires the bidirectional sync: `onCurrentRowChanged` → `videoPreview_->goToNalu(row)` (only
  for slice rows), and `VideoPreviewWidget::frameChanged` → `onVideoFrameChanged` selects/scrolls the
  table. No reentrancy guard is needed — Qt doesn't re-emit `currentRowChanged` when the index is already
  current, which naturally breaks the cycle.

## Testing conventions

- `tests/core_parser/*Test.cpp`, GoogleTest via `FetchContent`. Several tests embed real byte sequences
  captured from local FFmpeg encodes (see comments citing `ffprobe`-confirmed ground truth like
  profile/resolution) rather than hand-derived bit patterns — prefer that pattern when adding parser
  tests for anything beyond trivial bit-twiddling, since hand-deriving Exp-Golomb bit sequences is
  error-prone.
- There is no UI test suite. Verifying `VideoPreviewWidget`/`MainWindow` changes means actually running
  the app (see Commands above) — for headless/scriptable checks, compare a dumped frame against
  `ffmpeg -vf select=eq(n\,N) -frames:v 1` output with `ffmpeg -lavfi psnr` (near-45dB+ or `inf` indicates
  a correct decode; anything in the 15-25dB range usually means an off-by-N frame-order bug, not a
  colorspace issue).
