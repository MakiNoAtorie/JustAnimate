Benchmarks {#benchmarks}
==========

The benchmark (ROADMAP 1.1) is the baseline every optimization is measured
against. Keep a speed-up only if it is clearly outside run-to-run noise
(about ±10 %), and put the before/after numbers in the commit message.

## Run it

Use a Release build; Debug numbers are not comparable.

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target benchmark
```

The `benchmark` target builds `pencil2d_tests` and runs its hidden
`[benchmark]` test case (`./build-release/pencil2d_tests "[benchmark]"`
does the same). In cloud sessions `QT_QPA_PLATFORM=offscreen` is set; on a
desktop the canvas window briefly appears. A full run takes a few minutes.
The `[BenchmarkSmoke]` case runs the same code on a 3-frame project in the
normal test suite, so the benchmark can't silently break.

## What it measures

A project is generated first (not timed): 500 frames × 10 bitmap layers
(plus the camera layer), one 100 px ellipse per keyframe, saved as `.pclx`.
Then, 3 times over, each in a fresh editor and 1280×720 canvas:

| Scenario | What is timed |
|---|---|
| open | `Editor::openObject` on the `.pclx` |
| scrub | `scrubTo(f)` + canvas repaint for frames 1–500, caches cleared first (frames load from disk on first use) |
| play | playback on, `scrubForward()` + repaint for 500 frames back to back (no frame-rate timer) |
| draw pencil | 20 strokes of 100 mouse moves on frame 1, repaint after every move |
| draw brush | the same with the brush tool |

The table prints median, min and max over the 3 runs, and the median per
frame or stroke.

## Baseline

Recorded 2026-10-13 on the v0.1 integration tip (after ticket #37). Release
build, Windows 11, AMD Ryzen 5 9600X, MSVC 19.51, Qt 6.8.3 offscreen
platform, no other load. Always compare before/after on the same machine.

```
JustAnimate benchmark: 500 frames x 10 bitmap layers (+camera), 1280x720 canvas, 20 strokes per draw, 3 reps
scenario      median ms     min ms     max ms     median per
open             8562.6     8434.2     8599.7  8562.57 ms/file
scrub            1956.4     1890.0     2053.6     3.91 ms/frame
play              954.8      878.5      956.4     1.91 ms/frame
draw pencil      1104.0     1096.1     1175.0    55.20 ms/stroke
draw brush       1280.0     1218.4     1280.3    64.00 ms/stroke
```

`open` was 6186 ms on the same machine before the zip-slip check (ticket #3):
the check resolves every archive entry on disk before extracting.

## Brush replay

Replays seeded pen strokes (6 strokes of 150 samples, with pressure) through Pencil,
Brush and Eraser at sizes 4, 24, 96 and 200, on a fixed canvas, then hashes (SHA-1)
the painted layer and the composited frame. It is the safety net for brush engine
work: a change that must keep the pixels keeps the hashes.

- `[BrushReplay]` runs in the normal suite. It checks that replaying the same
  strokes twice gives the same hashes, and that today's output matches `kGolden`
  in `tests/src/test_benchmark.cpp`.
- Qt's raster engine is not bit-exact across OS, CPU and Qt version, so `kGolden`
  has one row per platform (`<kernelType> <cpu>`, e.g. `winnt x86_64`). On a
  platform with no rows the test prints a warning with the lines to paste. On a
  mismatch the failure shows the actual hash. To add a platform, paste its lines.
- Change `kGolden` only in a commit that means to change brush output.

Timing and before/after comparison (Release build, quiet machine):

```bash
cmake --build build-release --target replay > before.txt   # or: pencil2d_tests "[.replay]"
# ... change the brush engine, rebuild ...
cmake --build build-release --target replay > after.txt
python util/compare_replay.py before.txt after.txt
```

It prints before/after ms, the speedup, and whether the layer and frame hashes are
identical for every tool and size; exit code 1 if any differ.

Stage profile: set `JUSTANIMATE_STAGE_PROFILE=1` to print time and call counts per
stage (input, dab mask, composite, buffer paste, canvas update) after each replay case,
and at no extra cost when unset (one branch per stage). Times are inclusive: input
contains dab mask, canvas update contains composite. The replay sends no paint events,
so composite and canvas update are rare there; run the app to see them in use.

Replay timing baseline (same machine and build as above), ms per case:

| Size | Pencil | Brush | Eraser |
|---|---|---|---|
| 4 | 28.3 | 27.8 | 27.4 |
| 24 | 14.9 | 14.2 | 13.8 |
| 96 | 16.5 | 15.8 | 22.4 |
| 200 | 23.1 | 22.6 | 35.2 |
