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

## v1.7 homes

Temporary: decided before the v1.7 tickets start; folded at the end.

- New core files go in `core_lib/core_lib.cmake`; each module's test is
  `tests/src/test_<module>.cpp` (names below).
- Attribute table (#252-#255): `core_lib/src/structure/attributes.*` holds
  `enum class AttributeId`, `AttributeScope { Layer, Drawing, Project }`
  and the descriptor rows; values are `QVariant`. One `AttributeCommand`
  in `undoredocommand.*`, recorded by `UndoRedoManager::recordAttribute`.
  Setters on `LayerManager`: `setAttribute(layer, id, value)`,
  `setKeyAttribute(layer, frame, id, value)`, `setProjectAttribute(id,
  value)`. Rows write XML in today's order. Markers, camera size and key
  opacities keep their commands. Test: `test_attributes.cpp`.
- Edit (#256-#258): `core_lib/src/interface/edit.*`, a move-only
  `[[nodiscard]] Edit` from `Editor::edit(layer, description)`,
  `Editor::edit(layer, frame, description)`, and `Editor::edit(description)`
  for an outer multi-layer Edit; `commit()`. Nesting depth lives in
  `UndoRedoManager`. Attribute commands inside an open Edit join its step.
  Until #256 lands, new code uses today's protocol; #256/#257 migrate it.
  Test: `test_edit.cpp`.
- Frame renderer (#259-#261): `core_lib/src/framerenderer.*` holds the
  layer rules and the export adapter. `CanvasPainter` is the screen
  adapter. Test: `test_framerenderer.cpp`.
- Exporter (#273-#274): `core_lib/src/exporter.*`, `struct ExportRequest`,
  `Exporter::run(request, progress) -> Status`. `MovieExporter` stays the
  movie/GIF encoder behind it. Test: `test_exporter.cpp`.
- ProjectSession (#262-#263): `core_lib/src/projectsession.*`, a QObject
  built with `Editor*`, parented to `MainWindow2`; prompts are
  `std::function` members. Dirty = `UndoRedoManager` not at its clean
  index, or the unrecorded-change flag (`Object::modification()`), read
  only through `ProjectSession::isDirty()`. `RecentFileMenu` only displays
  the session's list. Test: `test_projectsession.cpp`.
- FrameEditGesture (#264-#265): `core_lib/src/interface/frameeditgesture.*`;
  the hit is `enum class FrameHit { Empty, Cell, ExposureEdge, Link }`.
  Test: `test_frameeditgesture.cpp`.
- Tool property registry (#266-#267): descriptors in
  `core_lib/src/tool/toolproperties.*`; ids are the existing
  `StrokeToolProperties::Type` values. Tool settings keys stay out of the
  preference table. Test: `test_toolpropertyregistry.cpp`.
- StrokeBuffer (#268-#269): `core_lib/src/tool/strokebuffer.*`, owned by
  `StrokeTool`. Test: `test_strokebuffer.cpp`.
- DabPlacer (#270): `core_lib/src/tool/dabplacer.*`; the random source is
  a `QRandomGenerator&`. Test: `test_dabplacer.cpp`.
- Preference table (#271-#272): rows in `core_lib/src/managers/
  preferencetable.*`; row ids are the existing `SETTING` values; keys and
  stored values unchanged. Test: `test_preferencetable.cpp`.
- A design-check ticket (gesture, stroke buffer, dab placer, preference
  table, exporter) that finds the deeper module does not pay stops and
  reports why, with no commit.
