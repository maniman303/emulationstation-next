---
name: sync-branches
description: Update local master from origin and merge it into the current local branch, resolving clear conflicts and asking about conflicts that would change behavior.
---

# Sync Branches

Use this skill when asked to bring the newest `origin/master` into local `master` and then into the branch the user is working on.

## Workflow

1. Inspect the repository root, current branch, worktree status, remotes, and local `master` before changing anything. Preserve the user's current branch as the integration target.
2. If there are uncommitted changes, do not stash, discard, or move them. Report the status and stop before switching branches. If the current branch is `master`, update it once and finish without a self-merge.
3. Fetch from `origin`. If fetching fails, stop and report the error; do not use stale remote refs as if they were current.
4. Update local `master` to `origin/master` with a fast-forward. If local `master` has diverged or cannot fast-forward, stop and describe the divergence. Never reset, force-update, or discard commits to make it fit.
5. Return to the saved integration branch and merge local `master` with a regular merge commit (`git merge --no-edit master`). Do not rebase or push unless the user separately asks.
6. If the merge conflicts, inspect every conflict and the surrounding code before resolving. Resolve automatically only when the intended result is unambiguous and preserves the behavior of both sides, such as combining independent additions or retaining both non-overlapping changes. Do not choose a side wholesale just to make the merge complete.
7. Before editing any conflict whose resolution could substantially change behavior, control flow, data interpretation, user settings, persistence, or public interfaces, pause and ask the user a focused question. Describe the alternatives in plain language, the consequence of each, and any recommendation with its rationale. Ask before taking the consequential action; do not stage a guessed resolution or continue the merge while waiting. Continue with independently resolvable conflicts only if doing so does not obscure the pending choice.
8. For clear conflicts, edit the files, inspect the resulting diff, and verify all conflict markers are gone. Stage only files involved in the merge and complete the merge. If a safe resolution is not clear, ask rather than guess.
9. Confirm the final branch, worktree status, merge commit, and relationship to its origin tracking branch. Report the fetch/update, merge, conflict resolutions or questions, and whether the branch was pushed. Leave the result local unless push was requested.

## Conflict questions

Ask one compact, file-specific question per substantive decision. State:

- what each branch changed and why the conflict exists;
- the behavior each plausible resolution would produce;
- a recommended option when the code provides enough evidence, and why;
- whether multiple changes can be combined without changing either behavior.

Do not ask about routine textual mechanics or independent changes with an obvious combined result. Do ask when preserving both sides requires choosing precedence, changing logic, dropping a behavior, or inventing behavior not supported by the code or request.
