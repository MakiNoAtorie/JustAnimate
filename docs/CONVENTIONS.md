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
- A red CI run: `bash tools/ci-failures.sh [run id]` prints the failed
  checks, the seed and, after a crash, the test that ran before it.
- Manual: `cmake --build build --target manual` writes the HTML pages,
  `JustAnimate-manual.md` and `.pdf` into `build/manual` (Pandoc; the PDF
  also needs Typst on `PATH` at configure time). `docs/manual/links.lua`
  fails the build on a broken chapter link, a missing image or a duplicate
  heading ID. Link to a chapter as `[Text](05-timeline-and-xsheet.md#exposure)`.
- Keep each file's line endings (`git ls-files --eol`); Git Bash `sed -i`
  drops CRLF. Commit with `git commit -F <file>` (UTF-8, no BOM).
  Stage files by name: the tree can hold the user's untracked files.

## Tests

- Catch2, `tests/src/test_<area>.cpp`, registered in `tests/tests.cmake`.
  Test names have no commas.
- Every fix ships with a test you ran red on the old code (a compile
  failure counts for new API). These rules beat Ponytail's. Restore old
  code with `git stash` or `git checkout`: a `Copy-Item` restore keeps the
  old modification time, so Ninja skips the rebuild.
- Test at a public interface (`Editor`, `FileManager`, a tool, a manager;
  `MainWindow2` only for whole-UI sweeps).
- Settings go through `pencilSettings()`, one shared `QSettings&`. Tests run on an empty temporary INI store, restore
  what they change and set up what they read.
- No QtTest: count signals with a lambda `connect`. Tool tests send
  `QTabletEvent`s at `view()->mapCanvasToScreen()`; paint checks call
  `grab()` on a shown area (offscreen glyphs are boxes); sizes compare
  to a `sizeHint` (styles differ by OS). Timing: `QEvent::setTimestamp`, no sleeps. `Status` in `((...))`.
- Tests write files only under their own `QTemporaryDir`.
- A test that parks pool workers uses `PoolHold` (`testfixture.h`).
- A skip-if-unchanged path (cache, archive copy) is tested with an input
  whose content changes while its size and time stay the same.
- Fixtures are stack objects or `unique_ptr`; every `new` QObject in app
  code gets a parent.
- No modal dialog in a test path or on the command line; a UI
  confirmation is an injectable `std::function<bool()>`. A modal left open
  fails its test after 2 s (watchdog in `tests/src/main.cpp`).
- A test that crashes a child on purpose sets `SEM_NOGPFAULTERRORBOX`
  on Windows.
- Speed-ups carry Release `benchmark` numbers: in the commit for a lone
  ticket; for a parallel milestone, one quiet run of `master` against the
  integration tip, in its PR.
- Benchmark rows append to the `sc` table and `docs/benchmarks.md`,
  record through `row("<name>")` and run in `[BenchmarkSmoke]`. Counters
  are `StageProfiler` stages appended before `StageCount`; tests read
  `StageProfiler::calls`.
- Golden values change only in a commit that means to change output.
- Saved XML: compare attributes sorted (Qt 6.4/6.5 QDom writes them in
  hash order). Render golden hashes keep a column per
  platform or Qt version that rasterises differently; for a version you
  cannot build (CI runs Qt 6.4 and 6.5.3), take the hash CI prints with
  `CAPTURE`.
- A test that needs the app's connections builds `TestSession`, never
  its own session struct; event filters go on after `start()`.

## Code

Read `docs/conventions-code.md` before you edit code.

## Commits

- One commit per ticket (`CLAUDE.md`); its `ChangeLog.md` line goes at the
  top of the list under "## JustAnimate - Unreleased"
  (`tools/check-changelog.sh` guards it). Follow-ups end `Refs #<n>`.
- `ChangeLog.md` merges with the union driver; keep both lines when a hand
  merge still conflicts.

## v1.9 homes

- Manual source: `docs/manual/`. Chapters, one per ticket:
  `00-index.md` (#295), `01-getting-started.md` (#298),
  `02-drawing-tools.md` (#299), `03-brushes-and-tablet.md` (#300),
  `04-layers.md` (#301), `05-timeline-and-xsheet.md` (#302),
  `06-animating.md` (#303), `07-sound-and-lip-sync.md` (#304),
  `08-anime-workflow.md` (#305), `09-saving-and-safety.md` (#306),
  `10-exporting.md` (#307), `11-preferences-and-reference.md` (#308).
  #295 creates `00-index.md` and a skeleton `01-getting-started.md`.
- Chapter link: `[Exposure](05-timeline-and-xsheet.md#exposure)`. Heading
  IDs are unique across all chapters; prefix a generic heading with an
  explicit ID, such as `## Overview {#layers-overview}`.
- Shots: one file per chapter, `tests/src/screenshots/shots_<slug>.cpp`
  (`shots_getting_started.cpp` ... `shots_preferences_and_reference.cpp`).
  #296 creates and registers all eleven, empty except Getting started.
  A chapter ticket edits only its own shot file.
  Add a shot: one `SECTION("<shot>")` after `ShotWindow shot;` (the main window,
  demo project, Light theme; `screenshots.h`) that sets up the scene and calls
  `saveShot(widget, "<chapter slug>-<shot>")`, plus one image reference.
  `cmake --build build --target screenshots` writes them to `build/manual/images`.
- Shot PNG name: `<chapter slug>-<shot>.png`, such as
  `getting-started-main-window.png`; chapters reference `images/<name>`.
- Online manual URL (#297): `APP_REPO_URL "/blob/master/docs/manual/00-index.md"`.
