# Brush engine

Rules for Pencil, Brush, Eraser and Smudge dabs, tips and presets. Read
before you edit `dabtool.*`, `dabplacer.*`, `smudgetool.*`, `tiledbuffer.*`,
`stroketool.*`, `toolproperties.h`, `brushpresets.*` or the preset dock and editor.

## Strokes and dabs

- Pencil, Brush and Eraser stroke code lives in `DabTool`. Input smoothing
  lives only in `InputSmoother` and runs on `PointerEvent::timestamp()`,
  never a `QTimer`. Preview-only drawing goes in the tool's `paint()`
  overlay, never the tiled buffer.
- Bitmap dabs are placed only in `DabPlacer` (`dabplacer.*`), a pure module:
  samples, properties and a `QRandomGenerator&` in, dabs out. `DabTool` only
  collects samples and paints the dabs it returns. Width modifiers
  (taper, jitter) multiply `DabPlacer::width`, never replace it. `DabPlacer::add`
  holds back samples for the end taper until a sample that ends the stroke.
- Pen pressure reaches dab width and opacity only through
  `DabPlacer::width`/`opacity`, which apply the `PressureCurve`. Settings
  are read once per press (`DabPlacer::begin`), never once per dab.
- Pen speed and tilt reach dabs only through `StrokeInterpolator::speed()`/
  `tilt()`, stored per `DabSample`.
- Per-dab random values come from the raw output of the placer's
  `QRandomGenerator` (`generate()`), never std distributions; the Dab tool
  owns it and tests seed it with `seedRandom()`. Each draw goes into its own
  named local, in a fixed order: argument evaluation order differs between
  compilers, so two draws in one call place dabs differently on macOS.
  `sizeDynamics()` runs once per placed dab and consumes the RNG, so never
  call it from spacing or prediction.
- Dab alpha: `DabPlacer::opacity` is what the whole stroke reaches;
  `spacedOpacity` is one dab's alpha, `1-(1-a)^(spacing/width)`, so
  spacing does not change darkness; flow per dab with pressure off is full
  flow, as before v0.3. Wash mode is per stroke
  (`TiledBuffer::setWashOpacity` on every press); with wash off the
  SourceOver loop is unchanged.

## Smudge

- The Smudge tool is a `DabTool` preset on bitmap layers (vector layers keep
  vertex moving). Its dabs go only through `TiledBuffer::smudgeDab`: sample
  the mean premultiplied colour within the sample radius, mix it into the
  colour bucket (`mixSmudgeColor`, kept part = smudge length), mix the front
  colour in by colour rate, then `compositeSmudge` moves pixels toward that
  colour, alpha included. Kubelka-Munk mixing (`SMUDGE_MIX_VALUE = 134`,
  "SmudgeMix": 0 light, 1 pigment) runs only inside `mixSmudgeColor`, through
  `spectralMix` (`spectralmix.*`, a port of spectral.js; keep its MIT notice,
  never Mixbox): colour weighted by amount x alpha, alpha mixed linearly.
  Row-band parallelism goes only in `compositeSmudge`
  (dab box >= 256K px, bands >= 64 rows; it fetches tile `bits()` before
  `parallelFor`, since a detach on a worker thread is a data race).
- Smudge reads the canvas every dab, so it never queues dabs for
  `flushDabs`. A smudge stroke seeds each new tile with the layer pixels
  (`setSmudgeSource`) and the buffer replaces the layer
  (`CompositionMode_Source`) in the preview and on commit.

## Settings

- The global pressure curve is the only brush setting in `PreferenceManager`
  (read in `DabTool::pointerPressEvent`). Every other brush setting is a
  per-tool `StrokeToolProperties` value, so presets keep it. Read and write
  one with `StrokeTool::get`/`set` (typed) or `setValue` (by id); every
  write emits `StrokeTool::propertyChanged(id)`. Dynamics, jitter, scatter,
  taper and dabs per step default to 0 (dab count 1), which leaves output
  unchanged.
- New `StrokeToolProperties` ids take the next free number on the
  integration branch tip. A setting that changes how dab alpha adds up is a
  bool, off by default, so a preset can bring back older output.
- A Stroke tool property's key, range, defaults, legacy key and Tool
  settings rows live only in its descriptor in `toolproperties.cpp`.
- A structured preference is a string read through a parser (such as
  `PressureCurve::fromString`); an empty or invalid value gives the neutral
  default.

## Tips and tiles

- Dab shape parameters live in `BrushTip` (`tiledbuffer.h`); each adds a
  quantized field to the `DabMask` cache key. The default tip keeps the old
  fast path so golden hashes do not change. `DabTool::drawDab` takes the tip
  read at press with the dab's angle; subclasses pass it through unchanged.
- Grain (`BrushTip::grainMode`) is not part of the `DabMask` key:
  `TiledBuffer::grainMask` multiplies a copy of the cached mask per dab.
  Paper grain samples canvas pixels; moving grain samples pixels from the
  dab's integer origin. `GrainTexture` 0 is generated with integer math
  only; 1.. are the 256x256 PNGs in `core_lib/data/textures` (sources and
  CC0 licence in `SOURCES.md`). A new texture is a file, a qrc line, a name in
  `kGrainNames` and a bigger `grainTextureCount`.
- Tip angles are degrees, counter-clockwise on screen (canvas y points down).
- Brush tip files (.gbr, PNG) are untrusted: bound every header field and
  check data length against file size before reading. A tip image is a
  `Format_Alpha8` coverage mask (255 = full paint).
- Tiles are `QImage` ARGB32_Premultiplied. Dabs write `scanLine()` with
  branchless loops, no `QPainter`; mask math uses float and `sqrt` only, so
  platforms hash alike. Read pixels only via `TiledBuffer::tiles()`; call
  `endCoverage()`/`flushDabs()` before any other tile write. Workers write
  tile pixels only inside `flushDabs`; tile creation and the pool stay on
  the main thread.

## Presets

- Brush presets go only through `BrushPresets` (`brushpresets.*`): one
  `<name>.json` per preset, name checked by `isValidName`. A preset holds
  every `ToolProperties` value keyed by identifier, plus `tipImage`
  (`StrokeTool::tipImagePath()`, empty = generated tip). The only global
  setting a preset may override is the optional `pressureCurve` (empty =
  global curve). Per-preset state outside `ToolProperties` (tip path, curve)
  lives on `StrokeTool`, never in QSettings; tests reset it at scope exit.
- A preset's optional `tool` (`BrushPresets::toolKey`: "eraser" or "brush";
  missing = "brush") picks the tool group it lists and applies for. The dock
  and the radial picker list only `names(toolKey(current tool))`.
- Preset JSON is untrusted: wrong type or missing gives the default, numbers
  are clamped, unknown keys are ignored, a corrupt file is skipped with
  `qWarning`. New keys are optional. Saving never overwrites another preset.
- Built-in presets list only keys that differ from tool defaults, and are
  seeded at GUI startup through `BrushPresets::seed`: once per name (the
  `.seeded` file in the preset folder), never over an existing file, so a
  deleted built-in stays deleted. Optional string `description` per preset.
- Only `remove` deletes tips: under `<presets>/tips/`, unused by other
  presets. Import never overwrites a tip. Callers use `exists()`/`filePath()`.
- A .jabrush is a zip with exactly `preset.json` plus at most one
  `tip.<suffix>`. Import untrusted zips in memory
  (`mz_zip_reader_extract_to_mem`, sizes bounded), write only after every
  check passes, never overwrite an existing preset.
- Offscreen brush rendering uses a hidden Editor/ScribbleArea driven by
  tablet events (`PresetSampler`, `brushpreseteditor.h`). The Tool settings
  Panel and the preset editor build their rows from the descriptors
  (label, range, combo items; editor `objectName` = key): a new property is
  one descriptor row, plus a Panel control only when it needs its own place.
  Code that changes tool values outside the options widget emits
  `ToolManager::toolsReset`.

## Tests

- Tests that check exact brush alpha call `setFeather(1)`.
- Count dabs with `StageProfiler::calls(DabMask)`, not pixels. Brush timing:
  Release build, interleaved before/after runs, ranges in the commit message.
- Golden rows use platform `""` (any platform), since platforms hash alike;
  CI fails on a mismatch. Add a platform-specific row only when a platform
  really differs. Change golden values only in a commit that means to change
  brush output, and say so.
