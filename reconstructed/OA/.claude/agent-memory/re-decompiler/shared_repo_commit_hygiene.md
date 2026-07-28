---
name: shared-repo-commit-hygiene
description: /home/share is a shared CIFS mount with multiple concurrent agent sessions working in the same git repos (kronosology/OA, kronosology/Eva, etc) -- staged-but-uncommitted changes from another session can get swept into your commit if you don't re-check staging immediately before committing
type: feedback
---

During the CPianoOsc/CSTGEPModelPatch OA.ko batch (2026-07-28), a
`git commit` accidentally bundled in ~1000 lines of unrelated,
unreviewed Eva changes that a concurrent session had staged in the same
shared working tree between my `git add <OA files>` and my
`git commit` call. The tell: `git show --stat HEAD` after the commit
showed 13 files changed across two unrelated projects (`reconstructed/
Eva/*` and `reconstructed/OA/*`) instead of the 7 I intended, and the
commit message only described the OA.ko work -- misleading provenance
for the Eva changes, and a commit message that doesn't match its diff.

**Why**: `/home/share` hosts multiple concurrent Claude Code sessions
(this project's normal mode of operation -- see e.g. `HARDWARE_REVIEW_
LOG.md`'s many same-day parallel OA.ko/Eva batches). Git's staging area
is a single shared piece of repo state; there is no per-session
isolation. A `git add` in one session and a `git add` in another can
interleave with either session's `git commit`, silently mixing their
changes into one commit attributed to whichever session commits first.

**Fix applied**: `git reset --soft HEAD^` (safe -- keeps all changes
staged, no data loss, nothing had been pushed), then `git restore
--staged <the other project's files>` to put them back exactly as the
other session had left them (still staged/modified in their working
tree, untouched), then re-staged and committed ONLY the intended files
as a fresh, isolated `git add <exact file list>` + `git commit` pair
run back-to-back with no other tool calls in between.

**How to apply going forward, in this and any other shared-repo
project on /home/share**: immediately before every `git commit`, run
`git diff --cached --stat` (or `git status --short`) as the LAST step
before the commit call, in the same message/turn if possible, and
verify the file list matches exactly what you intended to stage --
never assume the staging area still matches what you staged several
tool calls ago. If it doesn't match, `git restore --staged` the extra
files before committing, don't just proceed. If a bad commit already
happened and nothing has been pushed, `git reset --soft HEAD^` is the
safe, non-destructive fix (never `--hard`, never on anything already
pushed/shared beyond this repo).

**Reverse-direction instance (2026-07-28, InitializegRTParmFunctionTable_GE
batch)**: this can also happen to YOU as the victim, not just as the
cause. Staged two of my own files (an agent-memory update) with
`git add`, then made several more tool calls (reading status.md,
checking manifest) before running `git commit` -- in that window, a
DIFFERENT concurrent session ran its own `git commit` first, and their
commit (`a5c7f0d`, an unrelated Eva batch) swept up my staged memory
files too (their commit's own `--stat` showed my 2 files alongside their
10). My real OA.ko reconstruction commit itself (`d12aec1`) was made
immediately after staging with no gap, so it was unaffected -- the
exposure window was specifically the SEPARATE, later `git add` for the
memory-file follow-up, which had several tool calls between `add` and
`commit`. Content wasn't lost or wrong, just attributed to a commit
message that doesn't mention it. Did NOT `git reset --soft` the other
session's already-existing commit to fix this -- resetting a commit
another live concurrent session has already made and may be building on
top of is a real risk (they could have already referenced that hash,
pushed, or moved on), unlike resetting your OWN just-made commit which
is always safe. **General rule: minimize the gap between `git add` and
`git commit` to as few tool calls as possible (ideally back-to-back in
one turn) for EVERY commit, not just ones bundling many files -- a
single quiet `git add` followed by unrelated work before committing is
exactly the exposure window this bites in, in either direction.** If it
does happen to you, don't rewrite the other session's commit after the
fact; just note the provenance mismatch (content present, message
attribution imperfect) and move on.
