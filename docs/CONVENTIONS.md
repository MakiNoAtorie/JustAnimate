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
- Windows (MSVC): `toolsuild-test.cmd [filter]` builds both targets and
  runs the suite (set `VCVARS`/`QTDIR` for another install). It runs Qt
  offscreen when the plugin is installed. For the `benchmark` target set
  `QT_QPA_PLATFORM=offscreen` and `QT_QPA_PLATFORM_PLUGIN_PATH` to
  `%QTDIR%\plugins\platforms` so no window opens on the user's desktop.
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
- Tests write files only under their own `QTemporaryDir`: several agents run
  suites at once on one machine.
- Fixtures are stack objects or `unique_ptr`; a raw `new` fails the leak job.
- No modal dialog in a test path: put confirm and cleanup logic in helpers
  or callbacks the test can call. Command-line paths never prompt.
- A test that crashes a child process on purpose faults for real (null
  write in a function marked `no_sanitize`), sets `SEM_NOGPFAULTERRORBOX` on
  Windows and forwards the child's stderr.
- Golden values change only in a commit that means to change output.
- Catch2 test names contain no commas (the CLI filter splits on them).

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
- Brush engine, tips and presets: read `docs/brush-engine.md` before you
  edit `dabtool.*`, `tiledbuffer.*`, `stroketool.*`, `toolproperties.h`,
  `brushpresets.*` or the preset dock and editor.
- Dark/light decisions use `Theme::isDark()`, never
  `PlatformHandler::isDarkMode()`.
- Dock layouts and workspaces: free functions in `app/src/defaultlayout.*`
  taking widget pointers and `QSettings&`. Dock `objectName`s are stable keys.
- App files compiled into a test are listed in both `app/app.cmake` and
  `tests/tests.cmake`.

- Tool property ids: bucket `FILLCLOSEGAPS_VALUE = 307` ("FillCloseGaps",
  px, 0 = off) and `FILLBEHINDLINES_ENABLED = 308` ("FillBehindLines", off).
  New tools append to `ToolType` before `TOOL_TYPE_COUNT`: `SHAPE`, then
  `LASSOFILL`. `ShapeToolProperties` ids 600–699 (`SHAPEMODE_VALUE = 600`,
  "ShapeMode": 0 line, 1 rectangle, 2 ellipse, 3 polygon); lasso fill 700–799.
- Speed-ups: Release `benchmark` target, before/after back to back on the
  same machine, numbers in the commit message; a kept attempt that is not
  outside noise goes to `docs/rejected-optimizations.md` instead. Caches
  are checked pixel-identical against an uncached render and are cleared
  only from the existing modification signals.
- Opt-in fast mode: `SETTING::FAST_RENDER_LAYERS_ABOVE`, key
  `"FastRenderLayersAbove"`, default off.
- Project thumbnails (welcome screen): app side, written after a successful
  save to `QStandardPaths::CacheLocation/thumbnails/<sha1 of canonical
  path>.png`. No file-format change. Recent projects reuse the existing
  recent-files settings; templates are a fixed list in app code.

## Commits

- One commit per ticket: code, test and a one-line `ChangeLog.md` entry at
  the top of "JustAnimate - Unreleased".
- The message ends `Fixes #<ticket>` (`Refs #<n>` for follow-ups such as CI
  fixes), then the attribution trailers.
- `ChangeLog.md` merges with the union driver (`.gitattributes`); keep both
  lines when a hand merge still conflicts.
