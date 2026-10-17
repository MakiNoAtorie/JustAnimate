# Conventions

Rules every change follows. Keep under 8 KB: add a line only when it binds
future work; delete lines that stop being true.

## Build and test

Cloud sessions: `.claude/hooks/session-start.sh` sets up Qt 6.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2 --target pencil2d_tests 2>&1 | grep -E 'error|warning:|FAILED' | head -40
./build/pencil2d_tests 2>&1 | tail -5    # full suite, or "[tag]"
```

- Build with `-j2`, one build per worktree; stop only processes you
  started. Re-run cmake after a `*.cmake` change.
- Warnings are the lint: CI builds with `-DJUSTANIMATE_WERROR=ON` on Qt 6.4
  (Linux) and 6.5.3, GCC, Clang and MSVC: no Qt 6.5+ API.
- CI runs the suite in random order (rerun: `--order rand --rng-seed
  <run number>`) under ASan, UBSan and LSan; third-party leaks go in
  `tools/lsan.supp`.
- Windows: `tools\build-test.cmd ["exact test name" | [tag]]` (from
  PowerShell) runs the CI source guards, builds and runs the suite.
- Keep each file's line endings (`git ls-files --eol`); Git Bash `sed -i`
  drops CRLF. Commit with `git commit -F <file>` (UTF-8, no BOM).

## Tests

- Catch2, `tests/src/test_<area>.cpp`, registered in `tests/tests.cmake`.
  Test names have no commas.
- Every fix ships with a test you ran red on the old code (a compile
  failure counts for new API). These rules beat Ponytail's; its markers
  are `// shortcut:`.
- Test at a public interface (`Editor`, `FileManager`, a tool, a manager;
  `MainWindow2` only for whole-UI sweeps).
- Settings go through `pencilSettings()` (`util/pencilsettings.h`), one
  shared `QSettings&`. Tests run on an empty temporary INI store, restore
  what they change and set up what they read.
- No QtTest: count signals with a lambda `connect`. Tool tests send
  `QTabletEvent`s at `view()->mapCanvasToScreen()`; paint checks call
  `grab()` on a shown area; sizes compare to a `sizeHint` (styles differ
  by OS). Timing: `QEvent::setTimestamp`, no sleeps. `Status` in `((...))`.
- Tests write files only under their own `QTemporaryDir`.
- Fixtures are stack objects or `unique_ptr`; every `new` QObject in app
  code gets a parent.
- No modal dialog in a test path: put the logic in helpers the test can
  call. Command-line paths never prompt.
- A test that crashes a child process on purpose faults for real (null
  write in a `no_sanitize` function), sets `SEM_NOGPFAULTERRORBOX` on
  Windows and forwards the child's stderr.
- Golden values change only in a commit that means to change output.

## Code

- Follow the ground rules and lean rules in `ROADMAP.md`.
- File format: new XML attributes are optional; old files must still load.
  Homes for new project data: `docs/file-format.md`.
- Undo: every edit records through `UndoRedoManager` (`createState` after
  `handleDrawingOnEmptyFrame()`, `record` on every path; layers through
  `recordLayerAdd/Remove`). A gesture records once, at its end; a state it
  never records goes to `discardState()`. A gesture that may add a Drawing
  wraps both in `beginMacro`/`endMacroWith(state)`. Commands address frames by
  position and keep removed layers alive. Camera key edits record as
  `CAMERA_MODIFY`. Undo tests assert content, never selection state.
- An unrecorded edit still emits `frameModified`, `framesModified` or
  `layerCountChanged`, or calls `object()->modification()`.
- Do not tell the user an action cannot be undone unless no record exists.
- Bitmap pixels are premultiplied: `qUnpremultiply` before use as a `QColor`.
- Untrusted files: reject the whole archive before writing anything; return
  a failed `Status`. A bad value loads as the default; unknown tags are
  skipped; never `Q_ASSERT` on file data or disk state.
- A tool property a test may leave unregistered gets a getter that returns
  the default when it is missing.
- CPU-parallel loops use `parallelFor` (`util.h`, at most 8 threads); tasks
  touch only their own item; signals stay on the calling thread. Never
  `waitForDone` a shared pool. Fetch `QImage::bits()` before the loop.
- Layer blending goes through `blendPixels`; its SIMD path stays byte-exact
  to `blendPixelsScalar`. A key's drawing state comes only from
  `Layer::tweenStateAt`; state that varies inside a hold joins `layersKey`
  and skips the `ScribbleArea` frame cache.
- Layer order and group edits: `LayerManager::commitLayerArrangement`;
  timeline rows: `getLayerNumber`/`getLayerY`. Rendering asks
  `Object::isLayerVisible`; group opacity applies to the composite only.
- Crash-handler code uses static buffers only.
- Saving goes through `FileManager::save` (`<name>.tmp`, renamed only on
  success); movie export through `MovieExporter::writeThenReplace`; ffmpeg
  pipe writes only through `executeFFMpegPipe`, bounded by `bytesToWrite()`.
- Zip and unzip run through `FileManager::runArchiveJob` (worker thread;
  the caller emits progress). One `FileManager::save` at a time.
- `.gitignore` ignores `*.rc`; add a `!` exception to commit one.
- New preference: enum in `preferencesdef.h`, key in `pencildef.h`,
  `loadPrefs` and `set()` in `preferencemanager.cpp`. A renamed setting
  migrates the old key once, only while the new key is absent.
- Read `docs/brush-engine.md` before you edit `dabtool.*`, `tiledbuffer.*`,
  `stroketool.*`, `toolproperties.h`, `brushpresets.*` or the preset UI.
- Bitmap shape and path strokes go through `DabTool::dabPath`. A tool
  subclass skips its base's legacy settings migration. A tool that uses
  Shift or Alt clears `mQuickSizingProperties`. A vector fill from a new
  loop is `BezierCurve(points, false)` plus a `BezierArea`, not
  `VectorImage::fillContour`.
- Dark/light: `Theme::isDark()`, never `PlatformHandler::isDarkMode()`.
- Dock layouts: `app/src/defaultlayout.*`; dock `objectName`s are stable;
  a Layout change raises the version in `dropOutdatedLayouts`.
- New app files go in `app/app.cmake` only. New tools append to `ToolType`
  before `TOOL_TYPE_COUNT`.
- Speed-ups: Release `benchmark`, before/after on one machine, numbers in
  the commit; noise-level wins go to `docs/rejected-optimizations.md`.
  Caches match `CanvasPainter::paint` pixel for pixel and clear on
  modification signals; reuse `LayerCellCache`/`layersKey`; output-changing
  settings join `invalidateAllCache`. A cache that survives a scrub is keyed
  by content and carries a golden hash.
- Frame pool: frames over 50 from the playhead unload unless modified;
  viewing never marks modified. Playback preloads through
  `Object::preloadFrames`. Memory claims use the benchmark's peak RAM line.
- Frame selections span layers. Multi-layer edits: one `createState` per
  layer in one `UndoMacro`; a re-time ends in one `moveKeyFrames`.
- Snapping: viewport-px guides added in `StrokeTool::prepareInput`; ruler
  state in `OverlayManager`, unsaved.
- Selection transforms change only through `SelectionManager` setters; a
  new field joins `SelectionSaveState` and `TransformCommand`. A pending
  transform is applied in `ScribbleArea::pointerPressEvent` before any
  drawing tool paints.
- Stroke buffer composition: only `ScribbleArea::bufferCompositionMode()`.
  `duplicateLayer` copies every saved layer attribute.
- Sound: timing via `SoundClip::msAtFrame`/`waveformPeaks`; preview via
  `PlaybackManager::startSound`; tests use
  `setSamples`, or a valid WAV when they need a player. A clip put back in
  a layer gets a player from `SoundManager::processSound`; taking a layer
  out stops its clips. A missing clip file never fails save or export: skip
  and warn, naming it with `LayerSound::clipLabel()`.
- User-visible strings go through `tr()`.
- Colours come from the palette (Accent = `Highlight`), never hex; icons
  from `Theme::icon`.
- Copy new `KeyFrame` fields in its copy constructor and `operator=`. Mark filter only from `PreferenceManager::markFilter()`.

## Commits

- One commit per ticket: code, test and a one-line `ChangeLog.md` entry at
  the top of "JustAnimate - Unreleased".
- The message ends `Fixes #<ticket>` (`Refs #<n>` for follow-ups), then the
  attribution trailers.
- `ChangeLog.md` merges with the union driver; keep both lines when a hand
  merge still conflicts.


## v1.3 homes

- X-sheet: `app/src/xsheet.{h,cpp}`, dock `objectName` `XSheet`, tabbed
  with the timeline; layout version 4. No data of its own. (#193)
- Linked Drawings: `<image linkedTo="<src of the followed image>">` on
  bitmap and vector keys; copies share one image in memory; row in
  `docs/file-format.md`. (#194)
- Lock-all: `Layer::lockAll()`/`setLockAll(bool)`, `<layer lockAll="1">`
  on every layer type; row in `docs/file-format.md`. Refusals and other
  short notices go through a new `Editor::statusMessage(QString)` signal
  that `MainWindow2` shows in the status bar. (#199; #196 reuses it)
- Solo saved visibility lives in `LayerManager`, unsaved, no undo. (#199)
- Shift and Trace: `CMD_SHIFT_AND_TRACE` "CmdShiftAndTrace", default
  `Shift+T`; offsets are View state next to the onion painter, never in
  `Object`. (#195)
- Create inbetweens: `CMD_CREATE_INBETWEENS` "CmdCreateInbetweens", no
  default key, Animation menu; core in `LayerManager`, returns `Status`;
  easing is `CameraEasingType`. (#196)
- Transform key: a Move tool property; tween fields in Tool settings;
  writes through `LayerManager::setKeyTween`; the tween dialog in
  `TimeLineCells::showTweenMenu` goes. (#197)
- Tool groups: last tool per group in `pencilSettings()` key
  `ToolGroup/<first tool's CMD_TOOL_* name>`, value a `ToolType` int. (#198)
- `CMD_TOGGLE_XSHEET` "CmdToggleXSheet", no default key. (#193)
