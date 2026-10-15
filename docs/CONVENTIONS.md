# Conventions

Build, test and code rules every change follows. Keep this file under 8 KB:
add a line only when a rule binds future work, delete lines that stop being
true. Agents propose new lines in their report; the orchestrator folds them in.

## Build and test

The SessionStart hook (`.claude/hooks/session-start.sh`) installs Qt 6,
Ninja and ccache in cloud sessions and sets `QT_QPA_PLATFORM=offscreen`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2 --target pencil2d_tests 2>&1 | grep -E 'error|warning:|FAILED' | head -40
./build/pencil2d_tests 2>&1 | tail -5             # full suite, summary only
./build/pencil2d_tests "[ToolSettings]"           # one tag or test name
```

- Keep output quiet: filter builds to errors/warnings and tests to the
  summary or failures. Read a full log only to diagnose a specific failure.
- Build with `-j2` when several agents share a machine; run one build at a
  time per worktree. Stop only processes you started (by PID).
- Re-run `cmake -S . -B build` after a change to `tests/tests.cmake` or any
  `*.cmake` file.
- Compiler warnings are the lint: CI builds with `-DJUSTANIMATE_WERROR=ON`.
- CMake is the only build system. CI builds with Qt 6.4 (Linux) and 6.5.3 on GCC, Clang and
  MSVC: use no Qt 6.5+ API and no `QT_VERSION < 6` branches.
- `pencil2d_tests` does not compile all of `app/src`: build target `pencil2d`
  too after an edit there.
- Sanitizers: `-DJUSTANIMATE_SANITIZE=ON` (GCC/Clang, off by default); CI
  runs the suite with ASan, UBSan and LeakSanitizer.
- Windows (MSVC): run `vcvars64.bat`, configure with `-G Ninja
  -DCMAKE_PREFIX_PATH=<Qt>/msvc2022_64`; put Qt's `bin` on `PATH` to run tests.
- Edit files with the Edit tool or a byte-level script: keep each file's
  line endings (`git ls-files --eol`). Git Bash `sed -i` drops CRLF.

## Tests

- Catch2, in `tests/src/test_<area>.cpp`; register new files in
  `tests/tests.cmake`.
- Every fix ships with a test that fails without it.
- Test at a public interface (`Editor`, `FileManager`, a tool, a manager).
  App-side classes (`app/src/...`) can be compiled into `pencil2d_tests` by
  listing them in `tests/tests.cmake`.
- Tests that touch `QSettings` restore the value they change.
- Fixtures are stack objects or `unique_ptr`; a raw `new` fails the leak job.
- No modal dialog in a test path: put confirm and cleanup logic in helpers
  or callbacks the test can call. Command-line paths never prompt.
- A test that crashes a child process on purpose faults for real (null
  write in a function marked `no_sanitize`), sets `SEM_NOGPFAULTERRORBOX` on
  Windows and forwards the child's stderr.
- Golden values are keyed per platform; change them only in a commit that
  means to change output.
- A commit that changes brush output regenerates the rows it can run and
  drops the rest; CI prints the missing rows to paste in a `Refs` commit.
- Catch2 test names contain no commas (the CLI filter splits on them).
- Count dabs with `StageProfiler::calls(DabMask)`, not pixels. Brush timing:
  Release build, interleaved before/after runs, ranges in the commit message.

## Code

- Follow the ground rules and lean rules in `ROADMAP.md`.
- File format: new XML attributes are optional; old files must still load.
- Undo: every edit records through `UndoRedoManager` (`createState` after
  `handleDrawingOnEmptyFrame()`, then `record` on every path; layer
  add/remove through `recordLayerAdd/Remove`). Commands address frames by
  stored position and keep removed layers alive, never recreate them.
- Undo tests assert image or structure content after undo and after redo,
  never selection state.
- An edit that is not recorded for undo still emits `frameModified`,
  `framesModified` or `layerCountChanged`, or calls `object()->modification()`.
- Do not tell the user an action cannot be undone unless no record exists.
- Bitmap pixels are premultiplied: `qUnpremultiply` before use as a `QColor`.
- Untrusted files: reject the whole archive before writing anything; return
  a failed `Status`, never `Q_ASSERT` on it.
- Code that runs in a crash handler uses static buffers only.
- Saving goes through `FileManager::save`, which writes `<name>.tmp` and
  renames it over the target only after a successful write.
- `.gitignore` ignores `*.rc`; add a `!` exception for a resource file you
  mean to commit.
- New preference: enum in `preferencesdef.h`, key in `pencildef.h`,
  `loadPrefs` and `set()` in `preferencemanager.cpp`. A renamed setting
  migrates the old key once, only while the new key is absent.
- Brush engine: Pencil, Brush and Eraser stroke code lives in `DabTool`.
  Input smoothing lives only in `InputSmoother` and runs on
  `PointerEvent::timestamp()`, never a `QTimer`. Preview-only drawing goes in
  the tool's `paint()` overlay, never the tiled buffer.
- Pen pressure reaches dab width and opacity only through
  `DabTool::dabWidth`/`dabOpacity`, which apply the global `PressureCurve`.
  Read preferences once per press, never once per dab.
- A structured preference is a string read through a parser (such as
  `PressureCurve::fromString`); an empty or invalid value gives the neutral
  default.
- Dab shape parameters live in `BrushTip` (`tiledbuffer.h`); each adds a
  quantized field to the `DabMask` cache key. The default tip keeps the old
  fast path so golden hashes do not change. `DabTool::drawDab` takes the tip
  from `dabTip(direction)`; subclasses pass it through unchanged.
- Tip angles are degrees, counter-clockwise on screen (canvas y points down).
- Stroke settings shared by Pencil, Brush and Eraser get defaults in
  `StrokeTool::loadSettings`; each tool only adds `mPropertyUsed`.
- Bitmap dabs are placed only in `DabTool::dabSegment`. Width modifiers
  (taper, jitter) multiply `dabWidth`, never replace it. `drawStroke` may hold
  back pen points until release; do not assume `mLastBrushPoint` is the
  current point.
- Global stroke shaping outside a preset (pressure curve, taper) is a
  `PreferenceManager` setting read in `DabTool::pointerPressEvent`. Any other
  brush setting is a `StrokeToolProperties` value, so presets keep it.
- Dab alpha: `DabTool::dabOpacity` is what the whole stroke reaches;
  `spacedDabOpacity` is one dab's alpha, `1-(1-a)^(spacing/width)`, so
  spacing does not change darkness. Wash mode is per stroke
  (`ScribbleArea::setWashOpacity` on every press); with wash off the
  SourceOver loop is unchanged.
- Tests that check exact brush alpha call `setFeather(1)`.
- Brush tip files (.gbr, PNG) are untrusted: bound every header field and
  check data length against file size before reading. A tip image is a
  `Format_Alpha8` coverage mask (255 = full paint).
- Per-dab random values come from the raw output of `DabTool::mRandom`
  (`mt19937`), never std distributions; tests seed it with `seedRandom()`.
  `sizeDynamics()` runs once per placed dab and consumes the RNG, so never
  call it from spacing or prediction.
- Pen speed and tilt reach dabs only through `StrokeInterpolator::speed()`/
  `tilt()`. Dynamics, jitter, scatter and dabs per step are per-tool
  `StrokeToolProperties` (set through `StrokeTool::setDynamic`, shown in the
  stroke options widget). They default to 0 (dab count 1), which leaves
  output unchanged.
- Brush presets go only through `BrushPresets` (`brushpresets.*`): one
  `<name>.json` per preset, name checked by `isValidName`. A preset holds
  every `ToolProperties` value keyed by identifier, plus `tipImage`
  (`StrokeTool::tipImagePath()`, empty = generated tip). The only global
  setting a preset may override is the optional `pressureCurve` (empty =
  global curve). Per-preset state outside `ToolProperties` (tip path, curve)
  lives on `StrokeTool`, never in QSettings; tests reset it at scope exit.
  Saving never overwrites another preset. New keys are optional; preset JSON is untrusted
  (wrong type or missing gives the default, numbers clamped, unknown keys
  ignored, corrupt file skipped with `qWarning`).
- New `StrokeToolProperties` ids take the next free number on the
  integration branch tip. A setting that changes how dab alpha adds up is a
  bool, off by default, so a preset can bring back older output.
- Built-in presets list only keys that differ from tool defaults, and are
  seeded at GUI startup by `BrushPresets` methods that write a file only if
  it is missing. Optional string `description` per preset.
- A .jabrush is a zip with exactly `preset.json` plus at most one
  `tip.<suffix>`. Import untrusted zips in memory
  (`mz_zip_reader_extract_to_mem`, sizes bounded), write only after every
  check passes, never overwrite an existing preset.
- Offscreen brush rendering uses a hidden Editor/ScribbleArea driven by
  tablet events (`PresetSampler`, `brushpreseteditor.h`). The preset editor
  builds controls from `ToolProperties` ranges (`objectName` = identifier),
  so new properties need no editor code. Code that changes tool values outside the
  options widget emits `ToolManager::toolsReset`.
- Tiles are `QImage` ARGB32_Premultiplied. Dabs write `scanLine()` with
  branchless loops, no `QPainter`; mask math uses float and `sqrt` only, so
  platforms hash alike. Read pixels only via `TiledBuffer::tiles()`; call
  `endCoverage()`/`flushDabs()` before any other tile write. Workers write
  tile pixels only inside `flushDabs`; tile creation and the pool stay on
  the main thread.
- Dark/light decisions use `Theme::isDark()`, never
  `PlatformHandler::isDarkMode()`.
- Dock layouts and workspaces: free functions in `app/src/defaultlayout.*`
  taking widget pointers and `QSettings&`. Dock `objectName`s are stable keys.
- App files compiled into a test are listed in both `app/app.cmake` and
  `tests/tests.cmake`.

## Commits

- One commit per ticket: code, test and a one-line `ChangeLog.md` entry at
  the top of "JustAnimate - Unreleased".
- The message ends `Fixes #<ticket>` (`Refs #<n>` for follow-ups such as CI
  fixes), then the attribution trailers.
- `ChangeLog.md` conflicts: take the integration bytes and re-add your
  line; `git diff --stat` against the integration branch shows +1.
