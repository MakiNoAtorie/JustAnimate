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
- Compiler warnings are the lint. Add none (existing ones are ticket #21).

## Tests

- Catch2, in `tests/src/test_<area>.cpp`; register new files in
  `tests/tests.cmake`.
- Every fix ships with a test that fails without it.
- Test at a public interface (`Editor`, `FileManager`, a tool, a manager).
  App-side classes (`app/src/...`) can be compiled into `pencil2d_tests` by
  listing them in `tests/tests.cmake`.
- Tests that touch `QSettings` restore the value they change.

## Code

- Follow the ground rules and lean rules in `ROADMAP.md`.
- File format: new XML attributes are optional; old files must still load.
- Undo: new operations record through the new undo system
  (`UndoRedoManager` commands), not the legacy backup path.
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
