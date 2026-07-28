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
