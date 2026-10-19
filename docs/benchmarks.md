Benchmarks {#benchmarks}
==========

The benchmark (ROADMAP 1.1) is the baseline every optimization is measured
against. For a comparison with Pencil2D, see
[benchmarks-vs-pencil2d.md](benchmarks-vs-pencil2d.md). Keep a speed-up only if it is clearly outside run-to-run noise
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
| timeline | 20 times: `frameModified` on frame 1 (a stroke end), then a paint of a 1280x300 timeline |
| far jump | 10 jumps between the first and the last frame + repaint, caches cleared before each |
| working set | `scrubTo(f)` + repaint for frames 1–500 on a second project with a key every 3 frames, onion skin on both sides; the resident set after it is printed |
| draw top onion | the top-layer pencil strokes with the onion skin on; each release paints only its pending area, and the layer draws and cell renders it causes are printed per release |
| autosave ui | `ProjectSession::autoSave` after one pencil stroke on a saved project: the time the UI is blocked |
| autosave | the same autosave until its archive is written and renamed |
| draw 4K | the pencil strokes on frame 1 of layer 1 of the working set project after a 3840×2160 fill, so each stroke and its undo step land on a 4K Drawing; the resident set after it is printed |
| pan onion | 50 hand-tool pan steps of 7 px on frame 3 with the onion skin on, repaint after each |
| erase 4K | the eraser strokes on the same 4K Drawing right after draw 4K; each stroke ends with an auto-crop of the Drawing |

The table prints median, min and max over the 3 runs, and the median per
frame or stroke. A last line prints the peak RAM (working set, sampled after
every scrub and play frame; Windows and Linux only).

## Baseline

Recorded 2026-10-18 on the v1.8 integration tip (c194fb4). Release
build, Windows 11, AMD Ryzen 5 9600X, MSVC, Qt 6.8.3 offscreen platform,
no other load. `master` before v1.8 read 191.5 MB peak RAM; the frame
cache (#275) now keeps up to 240 viewed frames. Always compare
before/after on the same machine, peak RAM included.

```
JustAnimate benchmark: 500 frames x 10 bitmap layers (+camera), 1280x720 canvas, 20 strokes per draw, 3 reps
scenario      median ms     min ms     max ms     median per
save             5715.9     5697.3     5991.9  5715.93 ms/file
open             1648.9     1553.9     1728.5  1648.86 ms/file
scrub            1319.5     1306.6     1361.9     2.64 ms/frame
play             1210.6     1191.5     1281.3     2.42 ms/frame
draw pencil      1212.6     1208.0     1275.8    60.63 ms/stroke
draw brush       1633.4     1487.9     1675.4    81.67 ms/stroke
draw top         1639.1     1398.8     1639.6    81.96 ms/stroke
scrub onion      1521.4     1519.1     1604.0     3.04 ms/frame
play blend       2773.7     2761.2     2795.1     5.55 ms/frame
loop               18.1       18.0       20.5     0.19 ms/frame
save one          454.3      454.0      484.5   454.29 ms/file
export          10614.8    10314.1    11325.8    21.23 ms/frame
timeline           14.4       13.8       21.7     0.72 ms/stroke
far jump          165.3      161.4      169.7    16.53 ms/jump
working set      1245.2     1213.4     1525.3     2.49 ms/frame
draw top onion     1738.6     1709.8     1761.8    86.93 ms/stroke
autosave ui       260.7      250.8      304.9   260.72 ms/file
autosave          522.7      493.6      625.8   522.68 ms/file
draw 4K          1511.9     1498.5     1628.8    75.59 ms/stroke
pan onion          65.2       63.8       66.1     1.30 ms/event
erase 4K         1993.9     1776.3     2021.8    99.69 ms/stroke
peak RAM during scrub and play: 955.7 MB
working set after the 3s onion scrub: 1003.6 MB
draw top onion, per pen release: 1.0 layer draws, 0.0 cell renders
working set after the draw 4K strokes: 189.2 MB
```

`open` is mostly file extraction: it writes the 5003 archive entries to the
working folder, on up to 8 threads since #147.

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
