# JustAnimate Improvement Plan

## Ground rules
- Every change ships with tests where testable (`tests/src`) and must keep
  `pencil2d_tests` green.
- Project files stay compatible: new data goes in new optional XML
  attributes, so older JustAnimate and Pencil2D still open the files
  (and ignore what they don't know).
- One feature per branch/PR; core logic in `core_lib/`, UI in `app/`.
- Keep internal names as-is so upstream Pencil2D fixes can still be merged.

Sizes: **S** ≈ a day, **M** ≈ a few days, **L** ≈ a week or more.

---

## Phase 0 — Foundation (do first; everything else depends on it)

| # | Task | Where | Size |
|---|------|-------|------|
| 0.1 | Rewrite CI for this repo: Linux/Windows/macOS build + tests on every push/PR; drop Pencil2D notarization/upload steps | `.github/workflows/ci.yml`, `.github/actions/*` | M |
| 0.2 | Fix the 3 "control reaches end of non-void function" warnings (real crash risk) | build log locations | S |
| 0.3 | Replace deprecated Qt 6 calls (`localPos()`, `posF()`, `x()/y()` → `position()`), then turn on `-Werror` in CI | tool/canvas event code | M |
| 0.4 | Add sanitizer CI job (ASan/UBSan) running the test suite | CI | S |
| 0.5 | Retire the old undo system: make `NEW_UNDO_REDO_SYSTEM_ON` always on, remove the legacy path | `undoredomanager`, 4 call sites | M |
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
| 1.3 | Canvas rendering | Profile `CanvasPainter`; extend the existing layer pixmap cache (`mPreLayers/PostLayersPixmapCacheValid`) so only dirty tiles redraw | L |
| 1.4 | Onion skin cost | Cache tinted onion frames instead of re-tinting every paint | M |
| 1.5 | Memory | Audit frame pool (`FRAME_POOL_SIZE`): unload far-away keyframes, measure peak RAM on large projects | M |
| 1.6 | Async save/load | Move zip compression in `FileManager` to a worker thread with a progress bar; UI stays responsive | M |
| 1.7 | Crash safety | Write autosave to a temp file and atomically rename it; strengthen recovery of unsaved projects at startup | M |
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
| B0 | Test & measurement harness | Record/replay pointer streams (JSON) → render headlessly → compare against golden PNGs with a tolerance. Benchmark: dabs/s and ms per stroke for brush sizes 4/32/256 px. Lock the **current** look in as a baseline first | — | M |
| B1 | Correct spacing (quick win) | Extract one `DabSpacer` used by Pencil/Brush/Eraser; carry the remainder between events; interpolate pressure per dab; expose a "Spacing %" setting (default matches today's 12.5 %); fix the tile range (radius, not width) | F1, F2, F3, F6 | M |
| B2 | Fast dab rendering | `QImage` (ARGB32_Premultiplied) tiles; `DabMaskCache`; direct mask blending with SIMD-friendly loops; batch all dabs of one event per tile; dirty-rect updates to the canvas. **Target: ≥5× dabs/s vs the B0 baseline at 256 px** | F5, F6 | L |
| B3 | Dynamics system | Add tilt + timestamps to `PointerEvent` (from `QTabletEvent::xTilt/yTilt`, `timestamp()`); `BrushInputs` + `DynamicsCurve`; pressure → size/opacity/flow; speed → size; tilt → angle/size; random jitter; start/end taper (also "simulated pressure" for mouse users); **flow vs opacity** with build-up/wash modes; opacity linearize. Replaces the hard-coded `pressure * 0.5` | F3, F4, F9 | L |
| B4 | Smoothing rewrite | `InputSmoother` with None / 1€ / Weighted / Stabilizer modes; 0–100 strength slider; catch-up on pen lift; run on input timestamps instead of a 5 ms `QTimer`; old NONE/SIMPLE/STRONG settings map to equivalent presets | F8 | M |
| B5 | Brush tips | Generated tips (hardness, aspect, angle, follow-stroke-direction rotation); image tips (PNG, GIMP `.gbr`); scatter/jitter of position, size, angle, count per dab | F7 | M |
| B6 | Grain / texture | Grain texture multiplied into the dab mask; "fixed to canvas" (paper) or "moving with the stroke"; scale/contrast/brightness; a small built-in paper texture set | F7 | M |
| B7 | Smudge & color mixing | Smudge built into the engine (bucket, length, radius, color rate); retire the separate smudge loop; optional Kubelka–Munk mixing via a C++ port of spectral.js (MIT) for "wet" brushes. Do not use Mixbox (CC BY-NC) | F10 | L |
| B8 | Presets & UI | Brush preset dock with live stroke thumbnails; preset editor with curve editor and preview; import/export `.jabrush`; ship a default pack (HB pencil, 2B pencil, G-pen ink, marker, soft airbrush, chalk, watercolor-ish, hard/soft eraser) plus **"Classic" presets that reproduce today's Pencil/Brush/Eraser** so existing users aren't surprised | — | L |
| B9 | Optional: MyPaint brushes | Embed libmypaint (ISC) as a second engine behind the same `DabCompositor` surface interface, to load `.myb` brushes | — | L |
| B10 | Optional: GPU path | GPU dab compositing via Qt RHI for very large brushes/canvases; CPU path stays the reference implementation | — | L |

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
| 3.8 | Layer blend modes | Multiply, screen, overlay, add — stored per layer | M |
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

**Done when:** a first-time user can start a project, draw, animate, and
export without the manual.

---

## Suggested order / milestones

1. **v0.1 "Foundation"**: Phase 0 (all), 1.1, 1.7, **B0**
2. **v0.2 "Feel"**: **B1, B2, B4**, 3.1, 4.1, 4.2
3. **v0.3 "Brushes"**: **B3, B5, B8** (with Classic + default packs)
4. **v0.4 "Speed & tools"**: 1.2–1.6, 2.3, 2.4–2.6, 3.4, 3.5, 4.3, 4.4
5. **v0.5 "Paint & animate"**: **B6, B7**, 3.3, 3.6–3.8, 3.11, 3.12
6. **v1.0**: 2.8–2.11, 3.9, 3.10, remaining Phase 4, bug sweep
7. **After 1.0 (optional)**: **B9** (MyPaint brushes), **B10** (GPU)

## Risks
- **File format drift:** guard with round-trip tests (save → load →
  compare) and keep new fields optional.
- **Brush engine changes the look of existing drawings' new strokes:**
  ship "Classic" presets and golden-image tests from B0 before changing
  anything.
- **Performance work breaking rendering:** add pixel-comparison tests for
  `CanvasPainter` and the brush engine before optimizing.
- **Licensing:** only use GPL-compatible code (libmypaint = ISC ✔,
  spectral.js = MIT ✔, Mixbox = CC BY-NC ✘).
- **Upstream divergence:** periodically merge Pencil2D fixes; keep
  refactors small.
- **Art assets:** the icon, tool icons, brush tips, and paper textures need
  a designer or CC0 sources.

## References
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
