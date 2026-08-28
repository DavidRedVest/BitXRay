# Changelog

All notable changes to BitXRay are documented here. Format loosely follows
[Keep a Changelog](https://keepachangelog.com/).

## [0.1.0] - 2026-08-29

Initial public release. Built iteratively (see below) against three real
Annex-B captures (`possible_ip.h264`, `possible_ip.h265`, `possible_ipb.h264`)
rather than only synthetic test vectors, per the project's own testing
convention — several of the bugs listed under Fixed only reproduced on real
files (large size, real B-frame patterns, real weighted-prediction flags).

### Added

- **Layer 1 (`core_parser`)**: Annex-B start-code scanning with codec
  auto-detection (`NaluExtractor`); MSB-first bit reader and `ue(v)`/`se(v)`
  Exp-Golomb decoding (`BitReader`, `ExpGolomb`); RBSP emulation-prevention
  unescaping; full H.264 and H.265 SPS/PPS parsing (resolution, profile/level,
  chroma format, bit depth, entropy coding mode, and everything
  `slice_header()`/`slice_segment_header()` parsing needs from them); slice
  classification (I/P/B/SP/SI) and a full slice-header field breakdown for
  both codecs, including `ref_pic_list_modification()`, `pred_weight_table()`,
  `dec_ref_pic_marking()` (H.264) and `profile_tier_level()`,
  `st_ref_pic_set()`, long-term reference pics (H.265).
- **Layer 2 (`core_player`)**: `VideoDecoder`, wrapping libavcodec directly
  (not libavformat, since the input is a raw elementary stream, not a
  container). Per-NALU `pts` echoing lets the UI layer correlate a decoded
  frame back to the exact NALU that produced it even after B-frame reorder.
  Frame+slice decoder threading enabled for real-time decode speed.
  Colorspace conversion tuned to match ffmpeg's own default heuristic so
  decoded output is pixel-comparable with `ffmpeg -frames:v 1`.
- **Layer 3 (`ui`)**: NALU table with offset/length/type and color-coded rows
  (SPS/PPS/VPS distinguished; I/P/B/SP/SI slices distinguished); GOP-relative
  frame numbering (`#0`, `#1`, `#2`...) in the Info column; synchronized hex
  view and syntax tree (full slice-header detail for *any* selected slice
  row, not just SPS/PPS); bidirectional sync between the NALU table and video
  playback in both directions; an embedded frame-stepping preview panel; a
  standalone Play window (Play/Pause, Stop, Prev/Next frame-step, screenshot
  save, loop) modeled on H264BSAnalyzer's player, deliberately without a seek
  bar; an About dialog.
- 31 GoogleTest cases for `core_parser`, several validated against
  `ffprobe`-confirmed ground truth or hand-checked against H264BSAnalyzer's
  own field-level output.

### Fixed

- H.264 P-slices showing no detailed NAL info: `pred_weight_table()` was
  originally an unconditional bail-out, so any P-slice with
  `weighted_pred_flag` set (true of real encoder output, not just contrived
  test cases) silently produced no detail.
- H.265 slices showing only a generic one-line description instead of a full
  breakdown: SPS/PPS parsing hadn't been extended deep enough to reach the
  reference-picture-set/tile/deblocking-default fields that
  `slice_segment_header()` parsing depends on.
- Standalone playback feeling like manual frame-by-frame stepping instead of
  real-time video: the decoder-restart decision was comparing raw
  bitstream/decode-order NALU indices, which triggered a full decoder restart
  on almost every B-frame transition (decode order and display order diverge
  whenever B-frames are present).
- A seek bar in the standalone player that could hang the UI on arbitrary
  jumps; replaced with the fixed control set above (no seeking), matching how
  H264BSAnalyzer's reference player behaves.
- The app freezing for multiple seconds (up to the length of the whole
  remaining file) when a user selected a NALU far ahead of wherever the
  decoder had already fed up to — e.g. clicking near the end of a long H.265
  file. Large forward jumps now restart the decoder directly at the target's
  own GOP and skip RGB conversion for the intermediate frames that
  necessarily get decoded but never shown, rather than synchronously decoding
  every intervening NALU in bitstream order.
- A reentrancy crash in the bidirectional NALU-table/frame sync: a direct
  (synchronous) Qt signal let the table-selection-sync handler call back into
  the seek path for the frame already being displayed, invalidating an
  iterator the outer call still held.
- macOS/Qt auto-relocating an "About"-titled action into the native app menu
  by text-matching heuristics, which left the custom menu empty and hidden.

### Performance

- `extractNalus()` no longer scans the whole file twice (once via an internal
  `detectCodec()` call, once itself) when codec auto-detection is requested —
  now a single shared scan. ~2x faster NALU extraction on a 33 MB capture
  (47 ms → 24 ms).
- `NaluListModel::load()` no longer emulation-unescapes every slice NALU's
  full payload twice (once to classify I/P/B, once more to parse the full
  slice header) — the second pass now reuses the first's output. Since slice
  payloads make up the bulk of a file's bytes, this roughly halved total
  parse time on the same 33 MB capture (1310 ms → 656 ms).
