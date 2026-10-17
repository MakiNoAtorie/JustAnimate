# Implementer rules

Copy this file into your scratchpad, fill the two placeholders, and point every implementer prompt at the copy.

- Scratchpad: `<scratchpad path>`; ticket texts in `tickets/<n>.md`, the spec in `spec.md`.
- Integration branch: `<integration branch>`.

## Rules

- Read the ticket, then its comments (`gh issue view <n> --comments`): a "Decision" or "Agent Brief" there wins over the body. Read `CLAUDE.md` and the conventions doc (with its milestone homes section) before you code.
- Start the branch on the integration tip: `git checkout -B <branch> <integration branch>`. Use PowerShell for git commands.
- Build with the Skill tool `tdd`. Run local checks only; never push.
- Run every new test red on the old code before you write the fix; copy its failing line (`file(line)` and the assertion, or the compile error) for the report.
- One commit: code, test and the changelog line. The message ends `Fixes #<n>`, a blank line, then the attribution trailer.
- A step that needs a human (real hardware, an installer on a real machine, listening) is left for them: do the rest, end the message `Refs #<n>` so the ticket stays open, and say what is left.
- A crash or failure in the full suite is a finding even when a rerun passes: name the test and the line in your report.
- Before you report: `git rebase <integration branch>`, rebuild, rerun the full suite.

## Report

At most 15 lines: branch, sha, each test added by its `TEST_CASE` name with its red failing line, the suite's result line, what is left, then at most 10 proposed convention bullets.
