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

## Commits

- One commit per ticket: code, test and a one-line `ChangeLog.md` entry at
  the top of "JustAnimate - Unreleased".
- The message ends `Fixes #<ticket>` (`Refs #<n>` for follow-ups such as CI
  fixes), then the attribution trailers.
- `ChangeLog.md` conflicts: keep every line.
