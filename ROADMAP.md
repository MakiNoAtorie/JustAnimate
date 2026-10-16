# JustAnimate Improvement Plan

## Ground rules
- Every change ships with tests where testable (`tests/src`) and must keep
  `pencil2d_tests` green.
- Project files stay compatible: new data goes in new optional XML
  attributes, so older JustAnimate and Pencil2D still open the files
  (and ignore what they don't know).
- One commit per ticket (`Fixes #N`); one integration branch and PR per
  milestone (see `CLAUDE.md`); core logic in `core_lib/`, UI in `app/`.
- Keep internal names as-is so upstream Pencil2D fixes can still be merged.
- **Every change gets a one-line entry in `ChangeLog.md`** under
  "JustAnimate - Unreleased" (newest first), in the same commit as the
  change. Plain words, what changed for the user or developer, e.g.
  `- Fix zip-slip: opening a .pclx can no longer write outside its temp folder.`
  At release, rename "Unreleased" to the version.
- Performance rules (proven in JustPaint, see below): measure before and
  after; keep a change only if it is clearly outside run-to-run noise
  (≈ ±10 %); output must stay **pixel-identical** (checked by image hashes)
  unless the change is an opt-in "fast mode"; one change per commit with
  the numbers in the commit message; record rejected attempts in
  `docs/rejected-optimizations.md` so they are not retried blindly.

Sizes: **S** ≈ a day, **M** ≈ a few days, **L** ≈ a week or more.

## How we build: lean rules (adapted from ponytail)

Source: the `ponytail` skill set (MIT), https://github.com/dietrichgebert/ponytail.
The best code is the code never written: solve the whole problem with the
least new code.

**Before writing**
- Read the task and the code it touches. List every place the change must
  reach: callers, tests, `.ui` files, settings keys, file format, both
  build files while qmake still exists. That is scope; extra features are not.
- Check what the change could break for users: project files it could
  damage, settings it could reset, shortcuts that stop working.

**Pick the first option that fully works**
1. Does it need to exist? Skip options and flexibility nobody asked for,
   and say so in one line.
2. Already in this codebase? Reuse it the way the surrounding code does
   (e.g. `TiledBuffer`, `BlitRect`, `PreferenceManager`, the new undo
   system, existing camera easing for tweening).
3. Qt or the C++ standard library already does it? Use it.
4. An existing dependency (Qt, miniz)? Use it. Never add a dependency for
   a few lines.
5. Can it be one clear line? One line.
6. Otherwise: the minimum code that works.

**While writing**
- No abstraction, wrapper, option, config or "for later" code nobody asked
  for. An interface with one implementation, or a factory with one product,
  is not allowed. Deletion beats addition.
- Keep the structure the code already has (`core_lib` engine, `app` UI,
  managers on `Editor`).
- Bug fix: grep every caller first, then fix the root cause once in the
  shared code, not in one caller.
- Code that is moved or merged keeps its error handling and validation.
- Comment only the *why* the code can't show, in one line.
- New non-trivial logic (a branch, a loop, a parser, file I/O, an
  algorithm such as fill or spacing) ships with one small test that fails
  when it breaks. Trivial changes need none.
- A deliberate shortcut gets a marker comment:
  `// shortcut: <the limit>, <when to upgrade>`.
- **Never cut:** validation of data from files and the command line,
  error handling that prevents losing a user's work, security,
  accessibility, tablet calibration, anything the user asked for.

**Checks we run** (the ponytail commands, used as process even without the plugin)
- `/code-review` on every integration PR (its Standards axis applies these
  rules); rank findings in ponytail-review order: correct → safe
  → holds under load → tested → fast → lean; each finding needs a concrete
  failing case and the smallest fix; verdict "Ship" or "fix N first".
- `ponytail-audit` of the whole repo before each milestone (first one: see
  **Code audit** below).
- `ponytail-debt` before each release: grep `shortcut:` markers into a
  ledger; any marker without an upgrade trigger gets fixed or removed.

**Applying it to this roadmap (the "ultra" pass: does each item need to exist?)**
- Build the brush engine as **one** module with plain classes; no plugin
  system, no paintop registry, no engine interface until B9 (MyPaint)
  actually needs a second engine.
- Start presets as a plain JSON file per preset; a `.jabrush` zip only when
  tips or textures need packaging (B5/B6).
- Fast paths only after B0 measurements show the bottleneck.
- Deferred until users ask: X-sheet (3.9), command palette (4.4), shortcut
  presets for other apps (4.5), GPU path (B10), segment fast path (B11),
  MyPaint engine (B9).

---

## Code audit (ponytail-audit, 2026-10-13)

**What this repo does:** JustAnimate is a desktop 2D animation app
(Pencil2D fork, C++/Qt 6). Projects are `.pclx` zips holding XML plus one
PNG/vector file per keyframe; a command-line mode exports movies and
image sequences. **Assumed load:** one artist at a time, with a pen
tablet, on projects with hundreds of frames and canvases up to 4K; plus
scripted command-line exports.

Method: four parallel read-throughs (save/load, undo and timeline,
drawing tools and canvas, import/export and app shell); the top findings
were re-checked against the code by hand. Fix numbers below are referenced
from Phase 0 and the milestones. Each fix must ship with a test that
fails without it, and a `ChangeLog.md` line.

### Must fix (bug, crash, data loss, security)

| # | Problem (concrete case) | Where | Smallest fix |
|---|---|---|---|
| A1 | **A failed save marks the project as saved.** Save to a full disk or read-only folder → error dialog, but the undo stack is already "clean"; closing then quits without asking and the temp folder is deleted: work lost | `mainwindow2.cpp:803`, `undoredomanager.cpp:86-94` | Mark clean only after `fm.save()` succeeds |
| A2 | **Many edits never trigger "save changes?".** Default (legacy) undo doesn't record layer add/delete/rename, frame drag, paste frames, reverse, exposure; `Object::isModified()` is never read. Drag frames, quit → edit lost | `undoredomanager.cpp:159-169`, `mainwindow2.cpp:881` | `hasUnsavedChanges()` also returns `object()->isModified()`; set it from the existing frame/layer change signals |
| A3 | **Zip-slip:** a `.pclx` entry named `../../.bashrc` or an absolute path is written outside the temp folder on open | `qminiz.cpp:205-241` | Reject entries whose cleaned absolute path is outside the folder (reuse `validateDataPath`) |
| A4 | **Save overwrites the original before the new file is complete.** Disk full → backup copy fails silently, then the original is truncated: the user's only copy is corrupt. Backup naming also breaks on names like `shot.v2.pclx` | `filemanager.cpp:347-379`, `587-636` | Write to `<name>.tmp`, rename over the original on success; delete the backup helpers (less code) |
| A5 | **Undo after "Remove frame" on a vector layer crashes** (passes a layer *index* where an *id* is expected) | `undoredomanager.cpp:661` | Look up by `layerId` in all three branches |
| A6 | **Bucket (bitmap) or eraser (vector) crashes on a frame before the layer's first keyframe** | `bitmapbucket.cpp:55`, `erasertool.cpp:270` | Return early when the keyframe is null |
| A7 | **Tablet: a keyboard shortcut mid-stroke ruins the stroke.** Pen down + `E` → the brush dabs are committed as an erase; pen down + `.` → stroke lands on another frame. The shortcut filter only checks the mouse | `shortcutfilter.cpp:28`, `mainwindow2.cpp:1350` | Check `isPointerInUse()`; install the filter on frame/layer navigation actions too |
| A8 | **"Move frame forward" can drop a keyframe:** the swap leaves the selection list unsorted; a following drag inserts onto an occupied key and loses it (medium confidence) | `layer.cpp:248-267`, `657` | Sort the selection list after the swap |
| A9 | **Command-line export always exits 0 and prints "Done."**, even when ffmpeg is missing, the width is odd, or the project is empty; and it shows a blocking "already open" dialog if the GUI is running | `commandlineexporter.cpp:114-174`, `pencil2d.cpp:78-122` | Return the export `Status` and exit non-zero; do the single-instance check only for GUI launches |
| A10 | **Peg-bar align crashes on a held frame** (button enabled from the last keyframe, code reads the exact frame), and it has no undo | `pegbaraligner.cpp:40`, `pegbaralignmentdialog.cpp:137` | Use the last keyframe at the position; wrap in a backup |
| A11 | **Import Layers crashes on a corrupt or unreadable project** (null object dereferenced) | `importlayersdialog.cpp:160` | Null check + show the load error |

### Should fix (risky, wrong output, real slowness)

| # | Problem | Where | Smallest fix |
|---|---|---|---|
| A12 | Write errors ignored: a truncated `main.xml` / `.vec` is reported as saved | `filemanager.cpp:737`, `vectorimage.cpp:134` | Check stream/file status after flush |
| A13 | Save As on a nearly full disk: failed frame copy is ignored and the frame is saved blank | `layerbitmap.cpp:132-155` | Fail the save when copy/rename fails |
| A14 | Loader: an XML comment in `<projectdata>` hangs forever; an unknown layer type or `frame <= 0` crashes | `filemanager.cpp:424`, `object.cpp:99`, layer loaders | Advance the loop; skip unknown/invalid elements |
| A15 | Deleting a layer applies a pending selection move to the neighbouring layer | `layermanager.cpp:338` | Emit `currentLayerWillChange` before deleting |
| A16 | Importing a vector image or animated GIF into a hidden layer crashes | `editor.cpp:650`, `770` | Copy the visibility guard from `importBitmapImage` |
| A17 | A tap (dot) uses the pressure from the end of the previous stroke: dots vanish or come out fat | `stroketool.cpp:156` | Set pressure at stroke start |
| A18 | Vector copy/paste duplicates the whole frame (condition `< -1` can never be true) | `vectorimage.cpp:1055` | Paste only selected curves when there is a selection |
| A19 | Eyedropper picks premultiplied colour: soft edges give a darker colour | `eyedroppertool.cpp:152` | `qUnpremultiply()` |
| A20 | Export overwrites an existing file without asking; cancelling leaves a truncated movie | `importexportdialog.cpp:76`, `movieexporter.cpp:684` | Ask before overwrite; delete the partial file on cancel/fail |

**21 more confirmed, smaller findings** (track as issues): new-system frame-move undo moves wrong frames; Reposition Frames undo stores the wrong layer after the change; undo states clone the full image even when unused and leak on no-op bucket clicks; vector copy leaks a clone; autosave counter counts each edit up to 3×; bucket click outside the image fills from its top-left corner; vector fill skips vertex 0 of curve 0; tablet eraser end fires `toolChanged` on every event; smudge copies the whole image per move and is quadratic per stroke; canvas cache key mismatch makes held frames re-render on every hover; eraser never shrinks image bounds; camera "reset scaling" also resets rotation; camera tool hover clears all caches; pen double-tap needs a triple tap; Import Layers remaps vector colours in a chain; Check for Updates spins forever / null `deleteLater` on dev builds; CI packaging still expects Pencil2D names and builds qmake, not CMake; sound preview one frame off from export; image-sequence import not in numeric order; GIF/4K export queues all frames in RAM; exposure on long selections is O(n²).

### Missing tests (one good test each, not coverage)
`MiniZ::uncompressFolder` with `..` entries; a failed save leaves the
original intact; loading malformed XML; `UndoRedoManager` (legacy + new)
undo/redo of remove-frame and move-frames; `moveKeyFrame` then
`moveSelectedFrames`; `BitmapImage::floodFill`; `VectorImage::fillSelectedPath`
and `paste`; `CommandLineExporter` exit status; `CommandLineParser`
bad arguments.

### Lean (code that should not exist)
| Item | Lines |
|---|---|
| Finish the undo migration (0.5): legacy system + toggle; the new system first needs records for paste, flip, clear, delete selection, import, remove/reverse/exposure frames, layer add/delete | ≈ −500 net |
| qmake build files (after CI moves to CMake) | −729 |
| Dead CI for this fork: AppVeyor, docs, Transifex workflows and scripts | ≈ −500 |
| Qt 5 compatibility branches (CMake already requires Qt 6) | ≈ −40 branches in 21 files |
| Dead code: ~280 commented-out lines + unused functions in `vectorimage.cpp`, unused `BitmapImage::paintImage` overload, `FileManager::loadPaletteFile`, unused sound/export helpers and 3 unused setting keys, duplicate filter install | ≈ −400 |
| Pencil/Brush/Eraser/Pen duplicate press/release/`paintAt` code (handled by brush engine B1) | ≈ −300 |

**Verdict:** fix A1–A4 first (they lose or expose user data), then A5–A11.
`Lean: about -2,700 lines and -0 dependencies possible.`
**Not checked:** `miniz.c`, most of `timelinecells.cpp`, overlay and
onion-skin painters, platform-specific code (Windows/macOS), and nothing
was run under a tablet or with real ffmpeg.

---

## Phase 0 — Foundation (do first; everything else depends on it)

| # | Task | Where | Size |
|---|------|-------|------|
| 0.1 | Rewrite CI for this repo: Linux/Windows/macOS build + tests on every push/PR; drop Pencil2D notarization/upload steps | `.github/workflows/ci.yml`, `.github/actions/*` | M |
| 0.2 | Fix the "control reaches end of non-void function" warning (one header, `toolproperties.h:95`, counted three times; every enum case already returns, so low risk; one line) and the Must-fix items from the **Code audit** | see audit | S–M |
| 0.3 | Replace deprecated Qt 6 calls (`localPos()`, `posF()`, `x()/y()` → `position()`), then turn on `-Werror` in CI | tool/canvas event code | M |
| 0.4 | Add sanitizer CI job (ASan/UBSan) running the test suite | CI | S |
| 0.5 | Retire the old undo system. The legacy one is still the **default** and has crash bugs (A5); the new one lacks records for paste, flip, clear, delete selection, import, frame remove/reverse/exposure and layer add/delete. Add those records, fix new-system frame-move undo, make it the only system, delete the legacy path (≈ −500 lines net) | `undoredomanager`, `legacybackupelement`, callers | L |
| 0.6 | Update installer scripts (`util/installer`, `win-deploy.ps1`) and the CI packaging step for the new name | `util/`, `.github/` | M |
| 0.7 | New app + file icons (replace Pencil2D logo) | `app/data/icons`, `.ico/.icns/.png` | S (needs art) |
| 0.8 | Remove the qmake build (`*.pro`), keep CMake only | root, `app/`, `core_lib/` | S |

**Done when:** CI is green on all three OSes, there are no compiler warnings,
and builds have the new icon and name.

---

## Phase 1 — Performance & stability

| # | Task | Details | Size |
|---|------|---------|------|
| 1.1 | Benchmarks | Add a benchmark target: open a 500-frame/10-layer project, scrub, play, draw strokes. Record the numbers before optimizing anything | M |
| 1.2 | Playback frame cache | Pre-render composited frames ahead of the playhead during playback; invalidate on edit | L |
| 1.3 | Canvas rendering | Profile `CanvasPainter`; turn the existing "layers below" pixmap cache (`mPreLayersPixmapCacheValid`) into JustPaint's below-composite cache: cached per painted layer, filled in 64 px cells, invalidated by any change outside the painted layer, with a test that fails if invalidation is removed. Caching layers *above* is not pixel-exact (rounding), so it's an opt-in fast mode | L |
| 1.4 | Onion skin cost | Cache tinted onion frames instead of re-tinting every paint | M |
| 1.5 | Memory | Audit frame pool (`FRAME_POOL_SIZE`): unload far-away keyframes, measure peak RAM on large projects | M |
| 1.6 | Async + parallel save/load | `.pclx` stores every bitmap keyframe as a PNG, so encode/decode frames on all cores (JustPaint: parallel PNG encoding cut cold startup 22.7 %, byte-identical pixels) and zip on a worker thread with a progress bar; cap worker count (JustPaint found 4-per-worker was slower) | M |
| 1.7 | Crash safety | Write autosave to a temp file and atomically rename it; strengthen recovery of unsaved projects at startup; add an emergency-save hook on fatal signals that only publishes the last *completed* autosave (JustPaint `KisEmergencySave` pattern: allocation-free, lock-free, re-entry guarded; still unverified there, so write the crash test first) | M |
| 1.8 | Fuzz the loader | Fuzz `.pclx`/`.pcl` parsing (malformed XML/zip) to catch crashes | M |
| 1.9 | Upstream bug sweep | Triage open Pencil2D crash/data-loss issues, fix the reproducible ones | ongoing |

Stroke rendering speed is covered by the Brush Engine plan below (B1–B2).

**Done when:** documented speed-ups against the 1.1 baseline, and no known
crash in load/save/undo.

---

## Phase 2 — Drawing tools

The brush-related items (stabilizer, pressure curves, brush presets) are now
part of the **Brush Engine** plan below. Remaining tool work:

| # | Task | Details | Size |
|---|------|---------|------|
| 2.1 | → see **B4** (smoothing & stabilizer) | | |
| 2.2 | → see **B3** (dynamics & pressure curves) | | |
| 2.3 | Shape tool (new `ToolType`) | Line, rectangle, ellipse, polygon; Shift = constrain, Alt = from center; bitmap strokes go through the brush engine so shapes use the current brush | L |
| 2.4 | Bucket gap closing | "Close gaps" option: treat line gaps up to N px as closed (dilate line art in a mask before flood fill) in `BitmapBucket` | L |
| 2.5 | Lasso fill | Draw a freehand region → fill with current color (bitmap + vector) | M |
| 2.6 | Fill behind lines | Fill option that paints under the line art so fill doesn't eat into antialiased edges | M |
| 2.7 | → see **B8** (brush presets) | | |
| 2.8 | Lock transparency | Per-layer "only paint on existing pixels" toggle (implemented as a compositor mode in **B2**) | S |
| 2.9 | Ruler/guides | Straight-line ruler and perspective snapping using the existing perspective overlays; feeds snapped points into the brush engine | L |
| 2.10 | Transform improvements | Free-transform handles with skew; numeric input for move/scale/rotate | M |
| 2.11 | Vector cleanup | Fix known vector-tool rough edges (curve smoothing, merge/close tolerance) | M |

**Done when:** each tool has unit tests for its core algorithm (fill, shape
geometry) and works on bitmap and vector layers where it applies.

---

## Brush Engine (new)

### Current state in JustAnimate (findings from the code)

| # | Finding | Where | Effect for artists |
|---|---------|-------|--------------------|
| F1 | The same dab-placement loop is copy-pasted in three tools | `brushtool.cpp`, `penciltool.cpp`, `erasertool.cpp` (`drawStroke`) | Fixes must be made three times; tools drift apart |
| F2 | Spacing is hard-coded at width/8 (`brushStep = 0.5*width`, distance ×4) and uses `qRound(steps)`; leftover distance is thrown away because `mLastBrushPoint` jumps to the current point | same | Uneven spacing at different speeds ("beading"/clumping); no spacing control |
| F3 | One pressure value per input event is used for every dab in that segment; no interpolation | same | Visible width/opacity stair-steps on fast strokes |
| F4 | Opacity is hard-coded to `pressure * 0.5`; dabs pile up with SourceOver in the buffer | same + `ScribbleArea::paintBitmapBuffer` | No "flow vs opacity" control; strokes darken where they overlap themselves |
| F5 | Each dab builds a `QRadialGradient` and opens a new `QPainter` per tile it touches | `ScribbleArea::drawBrush`, `TiledBuffer::drawBrush` | Slow with large brushes and dense spacing |
| F6 | Tiles are `QPixmap`; the tile range is computed from `brushWidth` instead of the radius, so twice as many tiles are touched as needed | `tile.h`, `tiledbuffer.cpp` | Extra work and memory per dab |
| F7 | Only one tip: a round Gaussian-ish gradient (`setGaussianGradient`) | `scribblearea.cpp` | No textured, shaped, or image brushes |
| F8 | Smoothing has three fixed levels; STRONG is a 5 ms `QTimer` average; bitmap dabs follow straight segments between smoothed points (only vector gets cubic curves) | `strokeinterpolator.cpp` | Laggy or jagged lines; little control |
| F9 | `PointerEvent` has pressure, rotation, and tangential pressure, but no tilt and no timestamps | `pointerevent.h` | Tilt shading and speed-based dynamics are impossible |
| F10 | Smudge is a separate tool with a fixed 2 px step and its own logic | `smudgetool.cpp` | No wet/blending brushes |

### What the best engines do (research summary)

- **Dab ("stamp") model.** Krita's pixel engine, MyPaint, Procreate, and
  Photoshop all build a stroke from many small stamps ("dabs") of a brush
  tip placed along the path. Each dab can vary in size, opacity, angle,
  and color.
- **Spacing done right (libmypaint).** The number of dabs to place is
  `distance / actual_radius * dabs_per_actual_radius + distance /
  base_radius * dabs_per_basic_radius + dt * dabs_per_second`. The
  fractional leftover is kept (`PARTIAL_DABS`) and carried into the next
  input event, and **every input (position, pressure, tilt, time) is
  interpolated for each dab**. This gives even spacing at any speed and an
  airbrush mode based on time.
- **Inputs → outputs with curves.** MyPaint maps inputs (pressure, fine
  speed, gross speed, random, stroke progress, direction, tilt declination
  and ascension, custom) through editable curves onto outputs (radius,
  opacity, hardness, offset, color, smudge, …). Speeds are low-pass
  filtered with `1 - exp(-dt/T)` and log-mapped. The radius is stored on a
  log scale, which makes size curves feel natural.
- **Opacity linearize (libmypaint).** Per-dab alpha is corrected so dense
  spacing doesn't make strokes darker:
  `alpha_dab = 1 - (1 - alpha)^(1 / dabs_per_pixel)`.
- **Flow vs opacity (Krita, Photoshop).** Flow is per dab and builds up
  where dabs overlap. Opacity caps the whole stroke. Krita multiplies them
  and also offers a "wash" mode where dabs can't exceed the stroke opacity.
- **Smoothing (Krita).** Krita offers several modes:
  - *Weighted*: a Gaussian-weighted average of recent points by distance,
    with a speed-dependent radius and a "tail aggressiveness" option.
  - *Bezier interpolation*: segments are drawn using velocity tangents.
  - *Stabilizer*: a delayed average over N samples, with a dead zone and a
    "finish line" so the stroke catches up to the pen on release.
  - Other approaches: Lazy Nezumi and Paintstorm use **pulled string**
    smoothing. The **1€ filter** is a speed-adaptive low-pass filter:
    strong smoothing when slow, little lag when fast, with only two
    parameters.
- **Shape + grain (Procreate).** A brush is a *shape* (the tip) holding a
  *grain* (a texture). The grain can be fixed to the canvas, like paper, or
  move with the stroke. Other settings: spacing, jitter/scatter,
  StreamLine (smoothing), and start/end **taper**, including "touch taper"
  for input without pressure (mouse or finger).
- **Smudge (libmypaint, Krita color smudge).** The brush keeps a "bucket"
  of picked-up color, sampled from the canvas around the dab
  (`smudge_radius`). `smudge_length` controls how fast the bucket updates,
  and the bucket color is mixed with the paint color. Krita's "color rate"
  sets how much of the brush color is added.
- **Pigment mixing.** Kubelka–Munk mixing makes blue + yellow → green, as
  with real paint. **Mixbox is CC BY-NC, so it can't be used in a GPL
  app.** **spectral.js is MIT** and uses a 7-channel Kubelka–Munk model,
  so it can be ported to C++.
- **GPU stamping.** Research such as Ciallo (SIGGRAPH) and newer engines
  such as darkly and Kala Paint render dabs on the GPU. This is worth it
  later for very large brushes and canvases.
- **libmypaint itself is ISC-licensed**, which is GPL-compatible. Krita
  embeds it as an extra engine, and we could do the same to load the large
  library of existing MyPaint brushes.

### Target design

New module `core_lib/src/brush/`; tools become thin wrappers around it.

```
PointerEvent (+tilt, +timestamp)
      │
      ▼
 InputSmoother ──► DabSpacer ──► DynamicsEvaluator ──► DabMaskCache ──► DabCompositor ──► TiledBuffer (QImage tiles)
 (B4)              (B1)          (B3)                  (B2/B5/B6)       (B2/B7)              │
                                                                                        paste ▼ (opacity / wash / erase / lock alpha)
                                                                                     BitmapImage (layer)
```

| Class | Responsibility |
|-------|----------------|
| `BrushSample` | One input sample: canvas pos, pressure, tilt X/Y, rotation, tangential pressure, timestamp (ms) |
| `InputSmoother` | Modes: None, 1€ filter, Weighted (Gaussian by distance), Stabilizer (pulled string + dead zone + catch-up on release). Each exposes a 0–100 "strength". Replaces `StrokeInterpolator` |
| `DabSpacer` | libmypaint-style dab counting (relative to actual radius, base radius, and time), carries the fractional remainder, interpolates every input per dab along a Catmull-Rom/Bezier path (not straight segments) |
| `BrushInputs` / `DynamicsEvaluator` | Computes per-dab inputs (pressure, fine/gross speed with exp-decay filter, direction, tilt, rotation, random, stroke progress, distance) and applies per-output curves |
| `DynamicsCurve` | Monotone cubic curve with a control-point editor, shared by all dynamics and the global pressure curve |
| `BrushTip` | Generated tip (round/square, hardness, aspect ratio, angle, softness profile) or an image tip (PNG/GIMP `.gbr`), plus a grain texture |
| `DabMaskCache` | Caches alpha masks by quantized size/angle/hardness, with sub-pixel variants (or bilinear sampling) so small brushes stay smooth |
| `DabCompositor` | Writes masks directly into premultiplied `QImage` tiles (no per-dab `QPainter`). Modes: normal build-up (flow), wash (capped at stroke opacity), erase, lock-alpha, smudge |
| `SmudgeState` | Color bucket(s), smudge length, sample radius, RGB or Kubelka–Munk mixing |
| `BrushPreset` | All settings + curves + tip/grain images; saved as `.jabrush` (zip: JSON + PNGs) in the app data folder |
| `BrushEngine` | Wires it all together per stroke; called by Pencil/Brush/Eraser/Smudge tools and later the shape tool and ruler |

Vector layers keep `BezierCurve`. The engine is used only for bitmap layers
and the live bitmap preview. The `.pclx` format doesn't change, because
bitmap layers still store pixels.

### Implementation steps

| # | Step | Details | Fixes | Size |
|---|------|---------|-------|------|
| B0 | Test & measurement harness | Port JustPaint's setup: a stage profiler (time + call counts per stage: input, dab mask, composite, buffer paste, canvas update; off unless `JUSTANIMATE_STAGE_PROFILE=1`, one branch per stage when off), a `BrushStrokeReplayBenchmark` that replays deterministic or recorded strokes per preset/brush size and prints wall time + SHA-1 of the painted layer and the composited frame, and a compare script (speedup + "pixels identical?"). Seed all randomness in the benchmark so replays are deterministic. Lock the **current** look in as the baseline first | — | M |
| B1 | Correct spacing (quick win) | Extract one `DabSpacer` used by Pencil/Brush/Eraser; carry the remainder between events; interpolate pressure per dab; expose a "Spacing %" setting (default matches today's 12.5 %); fix the tile range (radius, not width). This step intentionally changes output: new golden hashes after review | F1, F2, F3, F6 | M |
| B2 | Fast dab rendering | `QImage` (ARGB32_Premultiplied) tiles; `DabMaskCache`; masks from r² with a precomputed **float** 1D profile table (no sqrt/erf/exp per pixel; JustPaint: float instead of double tables halved SIMD gathers, 1.35–1.45× on soft tips, bit-identical); 8-fold symmetry for round tips; tight dab bounds where mask ≥ 1/255; integer fixed-point mask × opacity × flow for 8-bit; a size-ordered dab-buffer pool reused across dabs and strokes with a memory cap (JustPaint: 128 MiB); **coverage-only accumulation**: for constant-color strokes accumulate a 1-channel alpha buffer and colorize once at paste. When multithreading big dabs, render dabs whose rolling mean time is < ~50 µs inline instead of via a thread pool (JustPaint: 2.8× on small brushes, which got *slower* with more threads before). Skip tile-binned compositing at first (JustPaint: no gain while batches hold 1–2 dabs). **Target: ≥5× dabs/s vs the B0 baseline at 256 px** | F5, F6 | L |
| B3 | Dynamics system | Add tilt + timestamps to `PointerEvent` (from `QTabletEvent::xTilt/yTilt`, `timestamp()`); `BrushInputs` + `DynamicsCurve`; pressure → size/opacity/flow; speed → size; tilt → angle/size; random jitter; start/end taper (also "simulated pressure" for mouse users); **flow vs opacity** with build-up/wash modes; opacity linearize. Replaces the hard-coded `pressure * 0.5` | F3, F4, F9 | L |
| B4 | Smoothing & latency | `InputSmoother` with None / 1€ / Weighted / Stabilizer modes; 0–100 strength slider; catch-up on pen lift; run on input timestamps instead of a 5 ms `QTimer`; old NONE/SIMPLE/STRONG settings map to equivalent presets. Perceived latency (JustPaint spec §7): short-horizon pen prediction drawn only in a throwaway preview overlay, and input event compression | F8 | M |
| B5 | Brush tips | Generated tips (hardness, aspect, angle, follow-stroke-direction rotation); image tips (PNG, GIMP `.gbr`); scatter/jitter of position, size, angle, count per dab | F7 | M |
| B6 | Grain / texture | Grain texture multiplied into the dab mask; "fixed to canvas" (paper) or "moving with the stroke"; scale/contrast/brightness; a small built-in paper texture set | F7 | M |
| B7 | Smudge & color mixing | Smudge built into the engine (bucket, length, radius, color rate); retire the separate smudge loop; optional Kubelka–Munk mixing via a C++ port of spectral.js (MIT) for "wet" brushes. Do not use Mixbox (CC BY-NC). Smudge reads the canvas every dab, so it can't use the paint-ahead pipeline; speed it up like JustPaint did: composite large dabs (≥ 256K px) in row bands of ≥ 64 rows on all cores (1.16×, bit-identical), keep small dabs serial; parallel source reads and buffer fills were measured as no gain, so skip them | F10 | L |
| B8 | Presets & UI | Brush preset dock whose rows show a rendered sample stroke, display name and description (as in JustPaint `bd1ca4c`); preset editor with curve editor and preview; import/export `.jabrush`; ship a default pack (HB pencil, 2B pencil, G-pen ink, marker, soft airbrush, chalk, watercolor-ish, hard/soft eraser) plus **"Classic" presets that reproduce today's Pencil/Brush/Eraser** so existing users aren't surprised | — | L |
| B9 | Optional: MyPaint brushes | Embed libmypaint (ISC) as a second engine behind the same `DabCompositor` surface interface, to load `.myb` brushes | — | L |
| B10 | Optional: GPU path | Low priority: JustPaint's research found Photoshop, Clip Studio and Krita all compute strokes on the CPU and use the GPU for display, and Krita's 2018 OpenCL brush test was ~30 % *slower* because of per-dab transfers. If done: a GPU-resident stroke buffer, dabs as instanced quads, readback only at stroke end, GPU result used as live preview while the CPU commits the authoritative pixels. CPU stays the reference | — | L |
| B11 | Optional: segment fast path | For round tips, compute coverage per stroke segment from distance-to-segment (capsule SDF) instead of stamping dabs; big win at tight spacing; opt-in fast mode with the dab path as fallback (JustPaint spec §9) | — | M |

**Brush engine acceptance criteria**
- Replayed strokes match their golden images; spacing is constant (±5 %)
  across slow and fast replays of the same path.
- No visible pressure stair-steps on a fast pressure ramp.
- Input-to-pixel latency stays under one frame (16 ms) for a 256 px brush
  on a 4K canvas on a mid-range laptop.
- The "Classic" presets look the same as the B0 baseline (within the image
  tolerance).
- Brush presets survive an app restart and can be exported and
  re-imported.

### Lessons from JustPaint (Krita fork, `MakiNoAtorie/justpaint`)

JustPaint already went through a measured brush and performance pass on
Krita. What carries over to JustAnimate:

| JustPaint result | What JustAnimate takes from it | Plan item |
|---|---|---|
| Stage profiler + stroke replay benchmark with layer/projection hashes; ±10 % noise rule | Same harness and rules from day one | B0, ground rules |
| Float curve table for soft tips: 1.35–1.45×, bit-identical | Float 1D profile tables in `DabMaskCache` | B2 |
| Small dabs inline instead of thread pool: 2.8× | Threshold on rolling mean dab time when threading is added | B2 |
| Size-ordered dab-buffer pool, 128 MiB cap | Reuse dab buffers across dabs and strokes | B2 |
| Below-composite cache: up to 1.43× with many layers | Upgrade `CanvasPainter`'s "layers below" cache | 1.3 |
| Parallel color-smudge compositing for big dabs: 1.16× | Row-band parallelism in engine smudge | B7 |
| Vectorized 8-bit blend modes (Multiply, Screen, …) with integer math matching the scalar rounding byte for byte: 1.4× on blended replays | Layer blend modes must ship with a SIMD path and a byte-exact equivalence test | 3.8 |
| Parallel PNG encode: −22.7 % cold startup | Parallel frame PNG encode/decode in save/load | 1.6 |
| Emergency save on fatal signals (unverified) | Same pattern, crash test first | 1.7 |
| Preset rows with rendered stroke + description | Preset dock design | B8 |
| **Rejected:** LTO (slower), PGO (≤2 % on brushes), `-O3` (only ~2 %), U16 SIMD blends (slower), tile-binned compositing with tiny batches, parallel smudge reads/fills | Don't spend time on these; Release default flags are enough | — |
| GPU research: big apps keep stroke math on CPU | GPU brush is last (B10); GPU display first if ever | B10 |

**Reusing JustPaint/Krita code:** JustAnimate is GPLv2-only (inherited
from Pencil2D). Krita files marked `GPL-2.0-or-later` or `LGPL-*` can be
ported (keep their copyright headers). Files marked `GPL-3.0-*` cannot be
copied (238 such files in `libs/` + `plugins/`, e.g.
`kis_spacing_information.h`); reimplement those ideas from scratch. Check
the SPDX line of every file before porting. Most of the brush code
(`kis_circle_mask_generator`, `kis_curve_circle_mask_generator`,
`kis_tool_freehand_helper`, `kis_distance_information`, `kis_cubic_curve`,
`KisStageProfiler`, `BrushStrokeReplayBenchmark`, `KisEmergencySave`) is
GPL-2.0-or-later ✔, but Krita's architecture (paint devices, paintops,
strokes framework) is far bigger than JustAnimate needs, so port
**algorithms and tests**, not whole classes.

---

## Phase 3 — Animation workflow

| # | Task | Details | Size |
|---|------|---------|------|
| 3.1 | Onion skin upgrade | Per-frame opacity falloff curve, custom colors, "keyframes only" vs "every frame", onion on selected layers only | M |
| 3.2 | Light table | Show any chosen reference frames (not just neighbors) as a light-table overlay | M |
| 3.3 | Transform tweening | Tween position/rotation/scale/opacity of a drawing between two keyframes, reusing the camera easing code (`cameraeasingtype`) | L |
| 3.4 | Timeline multi-select | Box-select across layers; drag to move/retime many frames at once; ripple insert/delete | M |
| 3.5 | Exposure editing | Set hold length per frame (1s/2s/3s) with one command; "Clean exposure" | S |
| 3.6 | Frame labels & markers | Named markers/colors on frames (e.g. "key", "breakdown"); saved in project | M |
| 3.7 | Layer groups/folders | Collapsible groups with group visibility/opacity | L |
| 3.8 | Layer blend modes | Multiply, screen, overlay, add — stored per layer; vectorized 8-bit path with integer math matching the scalar rounding, plus a byte-for-byte equivalence test over all 256 alpha values (JustPaint `3d3a368`) | M |
| 3.9 | X-sheet view | Optional exposure-sheet view of the timeline (traditional workflow) | L |
| 3.10 | Audio | Waveform display on sound layers; scrubbing improvements; lip-sync markers | M |
| 3.11 | Playback | Loop/ping-pong range, playback speed (0.5×/2×), frame-by-frame flipping improvements | S |
| 3.12 | Export | MP4/WebM quality presets, PNG sequence with alpha, sprite-sheet export, GIF palette options | M |

**Done when:** new data saves and loads in `.pclx` without breaking
Pencil2D compatibility, and there are timeline operation tests.

---

## Phase 4 — UI / UX polish

| # | Task | Details | Size |
|---|------|---------|------|
| 4.1 | Theme system | In-app Light / Dark / System choice; one central `QPalette` + stylesheet; hook into existing dark icon support (`IconResource`) | M |
| 4.2 | Default layout | Better default dock arrangement at small and large screen sizes; "Reset layout"; saved workspaces (Draw / Animate / Ink) | M |
| 4.3 | Welcome screen | Replace the preset dialog with a start screen: recent projects with thumbnails, templates (sizes/fps), open/new | M |
| 4.4 | Command palette | Ctrl+Shift+P searchable list of all actions | S |
| 4.5 | Shortcuts | Conflict detection, presets (Pencil2D / Krita / Toon Boom style) | M |
| 4.6 | Canvas HUD | Brush-size ring, rotation/zoom indicator, quick tool radial menu, quick brush-preset picker (tablet-friendly) | M |
| 4.7 | High-DPI pass | Check all icons/widgets at 150%/200% scaling | S |
| 4.8 | Accessibility | Keyboard navigation in timeline/panels, tooltips everywhere, contrast check for themes | M |
| 4.9 | Preferences cleanup | Reorganize preferences pages, search box, explain each option | M |
| 4.10 | Translations | Re-sync `.ts` files with new strings; set up a translation workflow for the fork | S |
| 4.11 | Optional: ideas from JustPaint's Photoshop-style UI | Toolbox groups behind one button with long-press menu; Alt+click on a layer's eye to show only that layer; layer lock row (pixels / all); proportional scaling by default in transform (Shift unlocks); Alt+wheel / Ctrl+wheel navigation; a monochrome icon set for light and dark themes | M |

**Done when:** a first-time user can start a project, draw, animate, and
export without the manual.

---

## Suggested order / milestones

1. **v0.1 "Foundation"**: audit **A1–A4**, then **A5–A11**, then A12–A20 (each with its test); Phase 0 (all), 1.1, 1.7, **B0**
2. **v0.2 "Feel"**: **B1, B2, B4**, 3.1, 4.1, 4.2
3. **v0.3 "Brushes"**: **B3, B5, B8** (with Classic + default packs)
4. **v0.4 "Speed & tools"**: 1.2–1.6, 2.3, 2.4–2.6, 3.4, 3.5, 4.3, 4.4
5. **v0.5 "Paint & animate"**: **B6, B7**, 3.3, 3.6–3.8, 3.11, 3.12
6. **v1.0**: 2.8–2.11, 3.9, 3.10, remaining Phase 4, bug sweep
7. **After 1.0 (optional)**: **B9** (MyPaint brushes), **B11** (segment fast path), **B10** (GPU, last)

## Risks
- **File format drift:** guard with round-trip tests (save → load →
  compare) and keep new fields optional.
- **Brush engine changes the look of existing drawings' new strokes:**
  ship "Classic" presets and golden-image tests from B0 before changing
  anything.
- **Performance work breaking rendering:** add pixel-comparison tests for
  `CanvasPainter` and the brush engine before optimizing.
- **Licensing:** only use GPLv2-compatible code (libmypaint = ISC ✔,
  spectral.js = MIT ✔, Krita/JustPaint `GPL-2.0-or-later`/`LGPL` files ✔,
  Krita `GPL-3.0-*` files ✘, Mixbox = CC BY-NC ✘).
- **Unverified work:** JustPaint's "heavy end" commits (emergency save,
  out-of-memory guard, PSD adjustment layers) compile but are not tested;
  don't port them as-is.
- **Upstream divergence:** periodically merge Pencil2D fixes; keep
  refactors small.
- **Art assets:** the icon, tool icons, brush tips, and paper textures need
  a designer or CC0 sources.

## References
- JustPaint brush/performance record: `MakiNoAtorie/justpaint` → `PERFORMANCE.md`, `NEXT_STEPS.md`, `base-doc/krita-brush-speedup-spec.md`, `base-doc/krita-brush-gpu-research.md`, `dev-docs/rejected-optimizations.md`
- libmypaint source (spacing, dynamics, smudge): https://github.com/mypaint/libmypaint/blob/master/mypaint-brush.c
- MyPaint brush settings: https://docs.drawpile.net/help/draw/mypaint
- Krita MyPaint engine docs: https://docs.krita.org/en/reference_manual/brushes/brush_engines/mypaint_engine.html
- Krita pixel brush engine: https://docs.krita.org/en/_sources/reference_manual/brushes/brush_engines/pixel_brush_engine.rst.txt
- Krita smoothing implementation: https://github.com/KDE/krita/blob/master/libs/ui/tool/kis_tool_freehand_helper.cpp
- Krita opacity and flow: https://docs.krita.org/en/reference_manual/brushes/brush_settings/opacity_and_flow.html
- Krita color smudge engine: https://docs.krita.org/Color_Smudge
- Procreate Brush Studio settings: https://help.procreate.com/procreate/handbook/brushes/brush-studio-settings
- 1€ filter: https://cristal.univ-lille.fr/~casiez/1euro
- Microsoft Research paint model (smudge/pickup): https://microsoft.com/en-us/research/wp-content/uploads/2010/06/PaintModel_NPAR_2010.pdf
- Ciallo, GPU brush strokes: https://cislab.hkust-gz.edu.cn/publications/ciallo-gpu-accelerated-rendering-of-vector-brush-strokes/
- spectral.js (MIT, Kubelka–Munk): https://npmjs.com/package/spectral.js
- Mixbox (pigment mixing, CC BY-NC): https://pub.dev/documentation/mixbox
