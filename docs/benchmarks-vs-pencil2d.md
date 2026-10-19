JustAnimate vs Pencil2D {#benchmarks-vs-pencil2d}
=======================

A black-box comparison through the command line, which both apps share:
open a `.pclx`, render it and export it. Each run is a fresh process. The
in-app scenarios (draw, scrub, play, save) are in [benchmarks.md](benchmarks.md);
Pencil2D cannot run them, because they link against JustAnimate's code.

## Run it

```bash
python tools/bench-vs-pencil2d.py <justanimate.exe> <pencil2d.exe> <work dir> [reps]
```

Needs Python with Pillow, on Windows (the peak RAM reading uses `psapi`).
Each exe needs `plugins/ffmpeg.exe` next to it; give both the same
`ffmpeg.exe`. Deploy JustAnimate from a Release build with `windeployqt
--release`. The script writes the test project into the work dir, does one
warm-up run, then runs the apps in turn for each rep, so drift hits both.
It fails if an export gives the wrong number of files.

## What it measures

The project has the same shape as the in-repo benchmark: 500 frames × 10
bitmap layers (plus the camera layer), one 100 px ellipse per keyframe at a
seeded random position. Pencil2D and JustAnimate open the same file.

| Scenario | Command line |
|---|---|
| startup | open a project with only a camera layer, export 1 frame |
| open + 1 frame | open the 500-frame project, export frame 1 at 1920×1080 |
| PNG sequence | export frames 1–500 at 1920×1080 as PNG |
| MP4 | export frames 1–500 at 1920×1080 as MP4 (ffmpeg) |
| GIF 720p | export frames 1–500 at 1280×720 as GIF (ffmpeg) |

Time is wall clock from process start to exit. Peak MB is the peak working
set of the app process; the ffmpeg child process is not counted.

## Results

Recorded 2026-10-19. JustAnimate at 3eac704 (Release, MSVC, Qt 6.8.3);
Pencil2D v0.7.2 official Windows portable (Qt 5.15.2). Same `ffmpeg.exe`
(from the Pencil2D portable). Windows 11, AMD Ryzen 5 9600X, 32 GB RAM, SSD,
no other load.

```
500 frames x 10 bitmap layers, 3 reps after one warm-up, fresh process per run
scenario        app            median s    min s    max s  peak MB
startup         JustAnimate        0.34     0.32     0.35       60
startup         Pencil2D           0.34     0.33     0.37       46
open + 1 frame  JustAnimate        2.35     2.30     2.44       77
open + 1 frame  Pencil2D           4.79     4.75     4.95       51
PNG sequence    JustAnimate        6.73     6.72     6.89      211
PNG sequence    Pencil2D          27.53    27.26    28.50      243
MP4             JustAnimate        5.45     5.36     5.47      120
MP4             Pencil2D          13.70    13.62    13.75     1192
GIF 720p        JustAnimate        8.54     8.33     8.77       98
GIF 720p        Pencil2D          18.77    18.74    18.90     1993
```

| Scenario | Speed-up | Peak RAM |
|---|---|---|
| startup | same | +14 MB |
| open + 1 frame | 2.0× | +26 MB |
| PNG sequence | 4.1× | −13 % |
| MP4 | 2.5× | 10× less |
| GIF 720p | 2.2× | 20× less |

- Run-to-run spread is under 5 %, so every speed-up is far outside noise.
- Most of `open` is archive extraction, which JustAnimate runs on up to 8
  threads (#147). It keeps a little more in memory after the open.
- Pencil2D's movie export peaks at 1.2 GB for MP4 and 2 GB for GIF on this
  project. The likely cause: it renders frames faster than ffmpeg reads
  them, and the unread frames wait in the pipe buffer. JustAnimate caps the frames that
  wait (`MAX_QUEUED_FRAMES` in `movieexporter.cpp`), so its peak stays
  near 100 MB.
- Output is the same: the 500 PNG frames of both apps are identical pixel
  for pixel.

## Not covered

- Drawing latency, scrubbing, playback and saving: these need the UI, and
  Pencil2D has no command line or test hook for them.
- Vector layers, sound and camera motion: the project has none.
- macOS and Linux: the RAM reading is Windows only.
