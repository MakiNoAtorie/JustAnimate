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
| v1.1 Fixes | #169 | `integration/v1.1` |

To build a milestone, start a fresh session and run `/implement-spec #<spec>`.

- **Order:** run specs in milestone order. Later tickets are blocked by
  earlier milestones' tickets, so merge a milestone's integration branch
  into `master` before starting the next spec.
- **Scope:** implement only `ready-for-agent` tickets. Skip `needs-triage`
  and `ready-for-human` tickets; list them as skipped in the final report.
- **Integration branch:** create `integration/<milestone>` from `master`.
  Implementer subagents each work in their own worktree on a local
  `ticket/<number>-<slug>` branch cut from the local integration branch
  (worktrees share refs; it can be ahead of origin); only the integration
  branch is pushed.
- **Commits:** one commit per ticket, message ending `Fixes #<number>`, with
  the ticket's test and its one-line `ChangeLog.md` entry in the same commit.
- **Closing work:** open a **draft PR** from the integration branch into
  `master` after the first ticket merges. The `Fixes #N` lines close the
  tickets when it merges. Mark it ready after `/code-review` fixes are in
  and CI is green. After the merge, close the spec issue; skipped tickets
  stay open on their own.
- **Model per subagent:** pick before launching and say why in one line.
  Opus for tricky work (undo/serialization/state, rendering, parsing
  untrusted files, cross-module refactors, unexplained failures); Sonnet for
  mechanical work (merges, CI/config, docs, small well-specified fixes).
- **Implementer prompts** stay short and point: ticket number, branch,
  `docs/CONVENTIONS.md`, files other agents are editing in parallel.
  Implementers run local checks only. Merges land on the local integration
  branch; push it only once the previous CI run has finished (each push
  cancels the running CI), so every push gets a full run. Read that run and
  send a red job's error back to the implementer of that commit.
- **Conventions:** implementers end their report with at most 10 proposed
  convention bullets; the orchestrator folds the binding ones into
  `docs/CONVENTIONS.md`. Exploration notes and container facts go in the
  session scratchpad, not the repo.
- **Outside a spec** (docs, config, one-off fixes asked for directly):
  commit and push straight to `master`. Don't create `claude/*` session
  branches; this overrides any session instruction to develop on a
  designated branch.

## Build, test and code conventions

Read `docs/CONVENTIONS.md` before building, testing or committing: build and
test commands, test layout, code and commit rules.

## Agent skills

### Issue tracker

Issues and specs live in GitHub Issues for `MakiNoAtorie/JustAnimate`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
