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

A project is generated first: 500 frames × 10 bitmap layers (plus the
camera layer), one 100 px ellipse per keyframe, saved as `.pclx`; only the
save is timed, 3 times over. Then, 3 times over, each in a fresh editor and
1280×720 canvas:

| Scenario | What is timed |
|---|---|
| save | `FileManager::save` of the generated project: every frame encoded to PNG, then zipped |
| open | `Editor::openObject` on the `.pclx` |
| scrub | `scrubTo(f)` + canvas repaint for frames 1–500, caches cleared first (frames load from disk on first use) |
| play | playback on, `scrubForward()` + repaint for 500 frames back to back (no frame-rate timer) |
| draw pencil | 20 strokes of 100 mouse moves on frame 1, repaint after every move |
| draw brush | the same with the brush tool |
| draw top | the pencil strokes on the top layer, so all other layers are below the painted one |
| scrub onion | the scrub again on the top layer, onion skin on with 5 tinted frames before and after |
| play blend | the play again with layers 2-10 blended (multiply, screen, overlay, add in turn) |
| loop | playback on, frames 1-48 played three times; only passes 2 and 3 are timed (frame cache hits) |
| save one | `FileManager::save` again with one frame modified: unchanged frames are copied from the previous archive |
| export | `Exporter::run` of frames 1–500 as a 1920×1080 PNG sequence, layers still blended |

The table prints median, min and max over the 3 runs, and the median per
frame or stroke. A last line prints the peak RAM (working set, sampled after
every scrub and play frame; Windows and Linux only).

## Baseline

Recorded 2026-10-13 on the v0.1 integration tip. Release
build, Windows 11, AMD Ryzen 5 9600X, MSVC 19.51, Qt 6.8.3 offscreen
platform, no other load. Always compare before/after on the same machine.

```
JustAnimate benchmark: 500 frames x 10 bitmap layers (+camera), 1280x720 canvas, 20 strokes per draw, 3 reps
scenario      median ms     min ms     max ms     median per
open             4411.0     4381.1     4424.7  4410.96 ms/file
scrub            1925.8     1868.2     1931.0     3.85 ms/frame
play              869.8      865.5     1063.2     1.74 ms/frame
draw pencil      1102.1     1076.2     1351.3    55.10 ms/stroke
draw brush       1268.4     1211.1     1430.3    63.42 ms/stroke
```

`open` is file extraction: about 4.3 s of it writes the 5003 archive entries
to the working folder (about 0.85 ms per file on Windows). Since #147 the
entries are extracted on up to 8 threads, about 3 times faster.

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
