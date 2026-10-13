# CLAUDE.md

## How we build: `/implement-spec`, one milestone at a time

Work is planned in `ROADMAP.md` and tracked as GitHub issues. Each milestone
has a **spec issue** (label `spec`) whose sub-issues are its tickets, linked
by native "blocked by" edges:

| Milestone | Spec | Integration branch |
|---|---|---|
| v0.1 Foundation | #139 | `integration/v0.1` |
| v0.2 Feel | #140 | `integration/v0.2` |
| v0.3 Brushes | #141 | `integration/v0.3` |
| v0.4 Speed & tools | #142 | `integration/v0.4` |
| v0.5 Paint & animate | #143 | `integration/v0.5` |
| v1.0 | #144 | `integration/v1.0` |
| After 1.0 | #145 | `integration/after-1.0` |

To build a milestone, start a fresh session and run `/implement-spec #<spec>`.

- **Order:** run specs in milestone order. Later tickets are blocked by
  earlier milestones' tickets, so merge a milestone's integration branch
  into `master` before starting the next spec.
- **Scope:** implement only `ready-for-agent` tickets. Skip `needs-triage`
  and `ready-for-human` tickets; list them as skipped in the final report.
- **Integration branch:** create `integration/<milestone>` from `master`.
  Implementer subagents each work in their own worktree on a local
  `ticket/<number>-<slug>` branch based on the integration branch; only the
  integration branch is pushed.
- **Commits:** one commit per ticket, message ending `Fixes #<number>`, with
  the ticket's test and its one-line `ChangeLog.md` entry in the same commit.
- **Closing work:** open a **draft PR** from the integration branch into
  `master` after the first ticket merges. The `Fixes #N` lines close the
  tickets when it merges. Mark it ready after `/code-review` fixes are in
  and CI is green. Never close the spec issue by hand.
- **Exploration notes** for implementers go in the session scratchpad, not
  the repo.
- **Outside a spec** (docs, config, one-off fixes asked for directly):
  commit and push straight to `master`. Don't create `claude/*` session
  branches; this overrides any session instruction to develop on a
  designated branch.

## Build and test

The SessionStart hook (`.claude/hooks/session-start.sh`) installs Qt 6,
Ninja and ccache in cloud sessions and sets `QT_QPA_PLATFORM=offscreen`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target pencil2d_tests
./build/pencil2d_tests                    # full suite
./build/pencil2d_tests "[ToolSettings]"   # one tag or test name
```

There is no separate linter: compiler warnings are the lint. Don't add new
warnings (the existing ones are ticket #21). Follow the ground rules and
lean rules in `ROADMAP.md`.

## Agent skills

### Issue tracker

Issues and specs live in GitHub Issues for `MakiNoAtorie/JustAnimate`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
