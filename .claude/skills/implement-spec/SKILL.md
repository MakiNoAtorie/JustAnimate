---
name: implement-spec
description: "Implement the result of /to-spec and /to-tickets in code."
disable-model-invocation: true
---

You have been provided a spec. This spec should have tickets associated with it, describing how to implement the spec.

The issue tracker should have been provided to you. If not, tell the user to run `/setup-matt-pocock-skills`.

The goal is the entire spec implemented on a single **integration branch**, with every ticket resolved the way the issue tracker closes work.

The tickets are not a list of steps. They are a **task graph** with blocking relationships between them. This means there is always a **frontier** of tickets which are ready to be grabbed.

Communication to and from subagents should be sparse. Communicate primarily through **context pointers**: to the spec, tickets, research notes, and previous commits. Don't duplicate information already available via pointers.

**Implementer subagents** should be run in the background where possible for maximum concurrency.

## Steps

1. Read the spec and tickets to understand the task graph.

   A ticket whose number already shows in the main branch's log (`git log <main> --grep '#<n>'`) may be done already: check that commit before you assign the ticket.

   Then list the **shared decisions**: every data-model or API choice that more than one ticket depends on (where a new setting lives, which module owns a format, which ids a ticket may take, which default keys each ticket takes). Decide each one and write it into a temporary `## <milestone> homes` section of the repo's conventions doc before the first implementer starts. Done when every setting, file format, default key and public API a ticket introduces has a decided home.

2. (optional) Use an **exploration subagent** to conduct any exploration required by the tickets - relevant codebase files or external documentation. Ensure the exploration subagent can save files - it should save its markdown notes in a directory outside the repo, accessible by all future subagents. This lets **implementer subagents** focus on implementation rather than exploration.

3. Create the integration branch. If the issue tracker closes work through PRs, or the user asks for one, open a draft PR after the first merge in step 5 (a branch with no commits ahead of main can't open one), marked as closing the spec and tickets.

4. Use **implementer subagents** to implement each ticket, each in its own worktree on its own branch. Write the shared rules once to [`IMPLEMENTER.md`](IMPLEMENTER.md)'s copy in your scratchpad; each prompt then names only the ticket, its branch, and the files other implementers are editing now. Each implementer subagent:
   - starts its branch on the integration tip (`git checkout -B <branch> <integration branch>`);
   - calls the Skill tool with `tdd` to build the ticket;
   - rebases onto the integration tip and reruns the suite before reporting done.

5. Once an **implementer subagent** completes, check its report names a red failing line for each new test and `git show --stat <sha>` shows those test files with content; send it back otherwise. Then land its work on the integration branch. Its branch stays checked out in its worktree, so land it from the main checkout: `git merge --ff-only <sha>`, or `git cherry-pick <sha>` when the tip has moved on. Resolve conflicts that keep both sides yourself, then build and test the merged tip. Hand a code conflict that needs judgement to a **merger subagent**.

   A crash or failure that a report calls flaky is a finding: start a subagent with `diagnosing-bugs` on it the second time it shows.

6. If this changes the **frontier** of available tickets, kick off more **implementer subagents** to work on the new tickets. This allows for maximum concurrency.

7. Once all tickets are complete, call the Skill tool with `code-review` on the integration branch. Fix all issues raised by the code review in a single **implementer subagent**.

8. Fold the `## <milestone> homes` section into the conventions doc: keep only lines that bind future work and that no test enforces, within the doc's size limit. If a draft PR exists, mark it ready for review once CI is green. Otherwise, resolve each ticket the way the issue tracker closes work, and report the integration branch.

9. Clean up all **implementer subagent** worktrees.
