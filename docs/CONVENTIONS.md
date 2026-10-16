# Conventions

Rules every change follows. Keep under 8 KB: add a line only when it binds
future work; delete lines that stop being true.

## Build and test

The SessionStart hook (`.claude/hooks/session-start.sh`) installs Qt 6,
Ninja and ccache in cloud sessions and sets `QT_QPA_PLATFORM=offscreen`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2 --target pencil2d_tests 2>&1 | grep -E 'error|warning:|FAILED' | head -40
./build/pencil2d_tests 2>&1 | tail -5             # full suite, summary only
./build/pencil2d_tests "[ToolSettings]"           # one tag or test name
```

- Build with `-j2`, one build at a time per worktree. Stop only processes
  you started (by PID).
- Re-run `cmake -S . -B build` after a change to any `*.cmake` file.
- Warnings are the lint: CI builds with `-DJUSTANIMATE_WERROR=ON`, Qt 6.4
  (Linux) and 6.5.3 on GCC, Clang and MSVC: no Qt 6.5+ API, no
  `QT_VERSION < 6` branches.
- CI runs the suite with ASan, UBSan and LSan (`-DJUSTANIMATE_SANITIZE=ON`).
- Windows: `tools\build-test.cmd ["exact test name" | [tag]]` runs the CI
  source guards, builds both targets and runs the suite offscreen.
- Edit files with the Edit tool or a byte-level script: keep each file's
  line endings (`git ls-files --eol`). Git Bash `sed -i` drops CRLF.
  PowerShell: commit with `git commit -F <scratch file>` written as UTF-8
  without BOM (`[IO.File]::WriteAllText`).

## Tests

- Catch2, in `tests/src/test_<area>.cpp`; register new files in
  `tests/tests.cmake`.
- Every fix ships with a test you ran red on the old code (a compile
  failure counts for new API). These rules beat Ponytail's; its markers
  are `// shortcut:`.
- Test at a public interface (`Editor`, `FileManager`, a tool, a manager;
  `MainWindow2` only for whole-UI sweeps such as `[Accessibility]`).
- Settings go through `pencilSettings()` (`util/pencilsettings.h`) only,
  never `QSettings(PENCIL2D, PENCIL2D)`; it is one shared `QSettings&`.
  Tests run on an empty temporary INI store, restore what they change and
  set up what they read.
- Tests do not link QtTest: count signals with a lambda `connect`, not
  `QSignalSpy`.
- Tests write files only under their own `QTemporaryDir`: agents run suites in
  parallel on one machine.
- Fixtures are stack objects or `unique_ptr`, and every `new` QObject in app
  code gets a parent: only CI's LSan job sees leaks (none on Windows).
- No modal dialog in a test path: put the logic in helpers the test can
  call. Command-line paths never prompt.
- A test that crashes a child process on purpose faults for real (null
  write in a function marked `no_sanitize`), sets `SEM_NOGPFAULTERRORBOX` on
  Windows and forwards the child's stderr.
- Golden values change only in a commit that means to change output.
- Catch2 test names have no commas: CLI filters split on them.

## Code

- Follow the ground rules and lean rules in `ROADMAP.md`.
- File format: new XML attributes are optional; old files must still load.
  Decided homes for new project data: `docs/file-format.md`.
- Undo: every edit records through `UndoRedoManager` (`createState` after
  `handleDrawingOnEmptyFrame()`, then `record` on every path; layer
  add/remove through `recordLayerAdd/Remove`). Commands address frames by
  stored position and keep removed layers alive, never recreate them.
  A gesture records once, at its end; a state it never records goes to
  `UndoRedoManager::discardState()`.
- A missing sound clip file never fails save or export: skip the clip and
  warn, naming it `<layer> frame <n>`.
- Undo tests assert content after undo and redo, never selection state.
- An edit that is not recorded for undo still emits `frameModified`,
  `framesModified` or `layerCountChanged`, or calls `object()->modification()`.
- Do not tell the user an action cannot be undone unless no record exists.
- Bitmap pixels are premultiplied: `qUnpremultiply` before use as a `QColor`.
- Untrusted files: reject the whole archive before writing anything; return
  a failed `Status`. A bad value loads as the default; unknown tags are
  skipped; never `Q_ASSERT` on file data.
- A tool property a test may leave unregistered gets a getter that returns
  the default when it is missing.
- CPU-parallel loops use `parallelFor` (`util.h`, at most 8 threads). Tasks
  touch only their own item; signals and progress stay on the calling thread.
  Never call `QThreadPool::waitForDone` on a shared pool: it deletes threads.
  Fetch `QImage::bits()` before the loop: a detach on a worker is a race.
- Layer blending goes through `blendPixels` (`util/blendmode.*`); its SIMD
  path stays byte-exact to `blendPixelsScalar`. A key's drawing state at a
  frame comes only from `Layer::tweenStateAt`; state that varies inside a
  hold joins `layersKey` and skips the `ScribbleArea` frame cache.
- Layer order and group edits record through
  `LayerManager::commitLayerArrangement`; timeline rows map through
  `getLayerNumber`/`getLayerY`. Rendering asks `Object::isLayerVisible`;
  group opacity applies to the group composite only.
- Crash-handler code uses static buffers only.
- Saving goes through `FileManager::save`, which writes `<name>.tmp` and
  renames it over the target only after a successful write. Movie export
  goes through `MovieExporter::writeThenReplace`; ffmpeg pipe writes only
  through `executeFFMpegPipe`, bounded by `bytesToWrite()`.
- Zip and unzip run through `FileManager::runArchiveJob` (worker thread;
  the caller emits progress). One `FileManager::save` at a time.
- `.gitignore` ignores `*.rc`; add a `!` exception for a resource file you
  mean to commit.
- New preference: enum in `preferencesdef.h`, key in `pencildef.h`,
  `loadPrefs` and `set()` in `preferencemanager.cpp`. A renamed setting
  migrates the old key once, only while the new key is absent.
- Brush engine, tips and presets: read `docs/brush-engine.md` before you
  edit `dabtool.*`, `tiledbuffer.*`, `stroketool.*`, `toolproperties.h`,
  `brushpresets.*` or the preset dock and editor.
- Shape and path strokes on bitmap layers go through `DabTool::dabPath`.
  A tool subclass skips its base tool's legacy settings migration. A tool
  that uses Shift or Alt clears `mQuickSizingProperties`. A vector fill
  from a new loop is `BezierCurve(points, false)` plus a `BezierArea` on all
  its vertices, not `VectorImage::fillContour`.
- Dark/light decisions use `Theme::isDark()`, never
  `PlatformHandler::isDarkMode()`.
- Dock layouts: free functions in `app/src/defaultlayout.*`; dock
  `objectName`s are stable keys.
- New app files go in `app/app.cmake` only; `tests/tests.cmake` takes them
  from there.
- New tools append to `ToolType` before `TOOL_TYPE_COUNT`.
- Speed-ups: Release `benchmark`, before/after back to back on one
  machine, numbers in the commit; noise-level wins go to
  `docs/rejected-optimizations.md`. Caches match `CanvasPainter::paint`
  pixel for pixel and clear on modification signals; only what those can't
  scope to a layer is keyed by content. Reuse `LayerCellCache`/`layersKey`;
  output-changing settings join `invalidateAllCache`.
- Frame pool: frames over 50 from the playhead unload unless modified;
  viewing never marks modified. Playback preloads through
  `Object::preloadFrames` (look-ahead under 50). Memory claims use the
  benchmark's peak RAM line.
- Frame selections span layers (`LayerManager::deselectAllFrames`).
  Multi-layer edits: one `createState(type, layer)` per layer in one
  `UndoMacro`; a re-time computes all targets, then one `moveKeyFrames`.
- A cache that survives a scrub is keyed by content only; pixel-identical
  caches carry a golden hash recorded before the change.
- Snapping: guides are viewport-px lines added in
  `StrokeTool::prepareInput` (`strokesnapper.*`); ruler state in
  `OverlayManager`, unsaved.
- Selection transforms change only through `SelectionManager` setters; a
  new field joins `SelectionSaveState` and `TransformCommand`.
- Stroke buffer composition: only `ScribbleArea::bufferCompositionMode()`.
  `duplicateLayer` copies every saved layer attribute.
- Sound timing: `SoundClip::msAtFrame`/`waveformPeaks`; tests use
  `setSamples`.
- Preference tooltips: `PreferencesDialog::addTooltips()`.
- User-visible strings go through `tr()`.

## Commits

- One commit per ticket: code, test and a one-line `ChangeLog.md` entry at
  the top of "JustAnimate - Unreleased".
- The message ends `Fixes #<ticket>` (`Refs #<n>` for follow-ups such as CI
  fixes), then the attribution trailers.
- `ChangeLog.md` merges with the union driver (`.gitattributes`); keep both
  lines when a hand merge still conflicts.
