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
- Fixtures are stack objects or `unique_ptr`; every `new` QObject in app
  code gets a parent.
- No modal dialog in a test path or on the command line; a UI
  confirmation is an injectable `std::function<bool()>`. A modal left open
  fails its test after 2 s (watchdog in `tests/src/main.cpp`).
- A test that crashes a child on purpose sets `SEM_NOGPFAULTERRORBOX`
  on Windows.
- Golden values change only in a commit that means to change output.
- Saved XML: compare attributes sorted (Qt 6.4/6.5 QDom writes them in
  hash order). Render golden hashes keep a macOS column.
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

## v1.8 homes

Temporary; folded after v1.8. Source: `docs/reviews/performance-2026-10.md`
(untracked; a copy sits in the orchestrator's scratchpad).

- Benchmark: a new scenario is a row appended at the end of the `sc` table
  in `tests/src/test_benchmark.cpp` and of the scenario table in
  `docs/benchmarks.md`; it also runs in `[BenchmarkSmoke]`. Names: `loop`
  (#275), `draw top onion` (#278), `far jump` and `working set` (#279).
  New rows record through `row("<name>")`, never an `sc[<index>]`; the
  smoke check's `sc.size()` gets one more per row.
  `save` (#282) goes into the printed baseline table. Release before /
  after numbers go in the commit message, never into the baseline.
- New counters are `StageProfiler` stages appended before `StageCount`
  (name in `names[]`, same order); tests read `StageProfiler::calls`.
- Frame cache (#275): a `ScribbleArea` member cache keyed by frame,
  replacing `mPixmapCacheKeys`; no `QPixmapCache::setCacheLimit`. #280 and
  #287 build on it. `QPixmapCache` stays only for `Theme` icons.
- Onion cache: `CanvasPainter::mOnionCache` is the one onion cache. #278
  adds onion image keys to the below-layers cell key; #280 re-keys by view
  scale/rotation; #289 adds vector entries keyed by the `VectorImage`
  modification counter. These three land in that order.
- Frame pool (#279): `ActiveFramePool` keeps its memory total as a running
  sum; every Drawing load reaches it through `KeyFrameEventListener`.
  Preload stays `Object::preloadFrames`.
- Undo rectangles (#281) live inside the Edit's bitmap state; no new public
  API on `Editor`.
- Save (#282): stored PNG entries and the copy of unchanged entries live in
  `FileManager`/`qminiz`; #283 runs that same `FileManager::save` path from
  `ProjectSession` on the worker, never a second writer.
- Export (#284): encoding goes through `parallelFor` inside `Exporter`;
  rendering stays on the calling thread.
- Stroke settings (#277): read at press into `StrokeTool` members from the
  tool property registry; #288 keeps that.

