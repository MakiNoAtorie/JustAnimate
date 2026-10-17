# Conventions

Rules every change follows. Keep under 8 KB: add a line only when it binds
future work; delete lines that stop being true.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2 --target pencil2d_tests 2>&1 | grep -E 'error|warning:|FAILED' | head -40
./build/pencil2d_tests 2>&1 | tail -5    # full suite, or "[tag]"
```

- Build with `-j2`, one build per worktree; stop only processes you
  started, by path (a kill by name ends other worktrees' suites). Re-run
  cmake after a `*.cmake` change.
- Warnings are the lint: CI builds with `-DJUSTANIMATE_WERROR=ON` on Qt 6.4
  (Linux) and 6.5.3, GCC, Clang and MSVC: no Qt 6.5+ API.
- CI runs the suite in random order (rerun: `--order rand --rng-seed
  <run number>`) under ASan, UBSan and LSan; third-party leaks go in
  `tools/lsan.supp`.
- Windows: `tools\build-test.cmd ["test name" | [tag]]` runs the CI
  source guards and the suite offscreen.
- Keep each file's line endings (`git ls-files --eol`); Git Bash `sed -i`
  drops CRLF. Commit with `git commit -F <file>` (UTF-8, no BOM).

## Tests

- Catch2, `tests/src/test_<area>.cpp`, registered in `tests/tests.cmake`.
  Test names have no commas.
- Every fix ships with a test you ran red on the old code (a compile
  failure counts for new API). These rules beat Ponytail's.
- Test at a public interface (`Editor`, `FileManager`, a tool, a manager;
  `MainWindow2` only for whole-UI sweeps).
- Settings go through `pencilSettings()`, one shared `QSettings&`. Tests run on an empty temporary INI store, restore
  what they change and set up what they read.
- No QtTest: count signals with a lambda `connect`. Tool tests send
  `QTabletEvent`s at `view()->mapCanvasToScreen()`; paint checks call
  `grab()` on a shown area (offscreen glyphs are boxes); sizes compare
  to a `sizeHint` (styles differ by OS). Timing: `QEvent::setTimestamp`, no sleeps. `Status` in `((...))`.
- Tests write files only under their own `QTemporaryDir`.
- Fixtures are stack objects or `unique_ptr`; every `new` QObject in app
  code gets a parent.
- No modal dialog in a test path or on the command line; a UI
  confirmation is an injectable `std::function<bool()>`. A modal left open
  fails its test after 2 s (watchdog in `tests/src/main.cpp`).
- A test that crashes a child on purpose sets `SEM_NOGPFAULTERRORBOX`
  on Windows.
- Golden values change only in a commit that means to change output.
- A test that needs the app's connections builds `TestSession`, never
  its own session struct; event filters go on after `start()`.

## Code

Read `docs/conventions-code.md` before you edit code.

## Commits

- One commit per ticket (`CLAUDE.md`); its `ChangeLog.md` line goes at the
  top of the list under "## JustAnimate - Unreleased", never in the
  heading; write it as UTF-8 without BOM (PowerShell `Get-Content`
  mangles it). Follow-ups end `Refs #<n>`.
- `ChangeLog.md` merges with the union driver; keep both lines when a hand
  merge still conflicts.
