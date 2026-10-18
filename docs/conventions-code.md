# Code conventions

Rules for every code change, read before you edit code. Same limits as
`docs/CONVENTIONS.md`: under 8 KB, a line only when it binds future work.

- Follow the ground rules and lean rules in `ROADMAP.md`.
- File format: new XML attributes are optional; old files must still load.
  Homes for new project data: `docs/file-format.md`.
- Undo: open an Edit (`Editor::edit(...)`) before the change and `commit()`
  it on every success path. A gesture that may add a Drawing opens an outer
  `edit(description)`, calls `handleDrawingOnEmptyFrame()`, then the inner
  `edit(layer, frame, ...)`. A held gesture keeps `std::optional<Edit>`.
  Pass a frame only for a Drawing pixel edit. A record the Edit cannot
  infer (layers, attributes, markers) goes through the Edit's own
  `record*` methods. Commands address frames by position and keep removed
  layers alive. Undo tests assert content, never selection state.
- An unrecorded edit still emits `frameModified`, `framesModified` or
  `layerCountChanged`, or calls `object()->modification()`.
- Do not tell the user an action cannot be undone unless no record exists.
- Bitmap pixels are premultiplied: `qUnpremultiply` before use as a `QColor`.
- Untrusted files: reject the whole archive before writing anything; return
  a failed `Status`. A bad value loads as the default; unknown tags are
  skipped; never `Q_ASSERT` on file data or disk state.
- A tool property getter returns the default when it is unregistered.
- CPU-parallel loops use `parallelFor` (at most 8 threads); tasks
  touch only their own item; signals stay on the calling thread. Never
  `waitForDone` a shared pool. Fetch `QImage::bits()` before the loop.
- Layer blending goes through `blendPixels`; its SIMD path stays byte-exact
  to `blendPixelsScalar`. A key's drawing state comes only from
  `Layer::tweenStateAt`; state that varies inside a hold joins `layersKey`
  and skips the `ScribbleArea` frame cache.
- Layer order and group edits: `LayerManager::commitLayerArrangement`;
  timeline rows: `getLayerNumber`/`getLayerY`. Rendering asks
  `Object::isLayerVisible`; group opacity applies to the composite only.
- Saving goes through `FileManager::save` (`<name>.tmp`, renamed only on
  success); movie export through `MovieExporter::writeThenReplace`; ffmpeg
  pipe writes only through `executeFFMpegPipe`, bounded by `bytesToWrite()`.
- Zip and unzip run through `FileManager::runArchiveJob` (worker thread,
  caller emits progress). One `FileManager::save` at a time.
- New preference: enum value plus one `kPrefRows` row in
  `preferencesdef.h` (key in `pencildef.h`). A renamed key is the row's
  `migratedFrom`. New code reads and writes through `get<S>()`/`set<S>()`.
- Read `docs/brush-engine.md` before editing dab, stroke, tiled-buffer,
  tool-property or preset code.
- Bitmap shape and path strokes go through `DabTool::dabPath`. A tool
  subclass skips its base's legacy settings migration. A tool that uses
  Shift or Alt clears `mQuickSizingProperties`. A vector fill from a new
  loop is `BezierCurve(points, false)` plus a `BezierArea`, not
  `VectorImage::fillContour`.
- Dark/light: `Theme::isDark()` only.
- Dock layouts: `app/src/defaultlayout.*`; dock `objectName`s are stable;
  a Layout change raises the version in `dropOutdatedLayouts`.
- New app files go in `app/app.cmake` only. A new `ToolType` goes before
  `TOOL_TYPE_COUNT` with a `BaseTool::TypeName`, a `StatusBar` icon and an
  `initGroups` entry.
- Speed-ups: Release `benchmark` numbers in the commit. Caches match
  `CanvasPainter::paint` pixel for pixel and clear on
  modification signals; reuse `LayerCellCache`/`layersKey`; output-changing
  settings join `invalidateAllCache`. A cache that survives a scrub is keyed
  by content and carries a golden hash.
- Frame pool: frames over 50 from the playhead unload unless modified;
  viewing never marks modified. Playback preloads through
  `Object::preloadFrames`.
- Frame selections span layers. Multi-layer edits: one outer Edit
  with one Edit per layer.
- Snapping: viewport-px guides added in `StrokeTool::prepareInput`; ruler
  state in `OverlayManager`, unsaved.
- Selection transforms change only through `SelectionManager` setters; a
  new field joins `SelectionSaveState` and `TransformCommand`. A pending
  transform is applied in `ScribbleArea::pointerPressEvent` before any
  drawing tool paints.
- Stroke pixels and vector curves go through the Stroke tool's `StrokeBuffer`
  (`begin`, `dab`, `commit`); its composition only in
  `StrokeBuffer::compositionMode()`.
- `duplicateLayer` copies every saved layer attribute.
- Sound: timing via `SoundClip::msAtFrame`/`waveformPeaks`; preview via
  `PlaybackManager::startSound`; tests use `setSamples`. A clip put back in
  a layer gets a player from `SoundManager::processSound`; taking a layer
  out stops its clips. A missing clip file never fails save or export: skip
  and warn.
- Page exports: painters in `paperpainter.*`, files via `writePages`.
- Camera-frame overlays apply `getViewAtFrame(frame)`.
- User-visible strings go through `tr()` (`QCoreApplication::translate`
  outside QObjects).
- Colours come from the palette (Accent = `Highlight`), never hex (drawing
  data: `// palette-exempt:`, checked by `tools/check-new-colours.sh`); icons
  from `Theme::icon`.
- Copy new `KeyFrame` fields that are not attribute rows in its copy
  constructor and `operator=`; key copy copies the rows. Mark filter only from `PreferenceManager::markFilter()`.
- An edit starts with `editor->refuses(layer)` (View tools exempt).
  Refusals and notices go only through `Editor::notice`.
- Cell, layer and easing menus: only `app/src/cellmenus.*`.
- Linked Drawings (ADR 0001): only `linkDrawing` sets a `linkId`; a pixel
  edit outside `KEYFRAME_MODIFY` calls `syncLinkedDrawings`.
- Built-in brush presets are JSON files in `core_lib/data/brushpresets/`,
  never C++. Read a preset by name only through `BrushPresets::read`; with
  a parsed preset in hand, `apply(QJsonObject, tool)`. The preset dock has
  no file watcher: whatever writes a preset refreshes it.
- A developer trace is a `JUSTANIMATE_<NAME>=1` environment switch
  printing through `qDebug()`, never a user preference.
- A tool without settings declares no properties member and no
  `toolProperties()` or `loadSettings()` override.
- A dropped file attribute: stop writing and reading it, no load shim
  (QDom ignores unknown attributes); `docs/file-format.md` says the old
  attribute is ignored.
