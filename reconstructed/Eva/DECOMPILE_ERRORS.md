# Decompile Errors / Revisit List — Eva

Per the standing goal (2026-07-27, updated): "Continue decompiling both OA.ko
and EVA until told to stop. Note the decompiled files that error or do not
compile properly, we can revisit later. Only stop once the entirety of OA.ko
and EVA are decompiled."

This file tracks functions/files where a reconstruction attempt hit a real
blocker — compile error, link error, or a transcription that couldn't be
made to match ground truth with reasonable effort — so later passes can
revisit them without re-discovering the same blocker from scratch.

Format: `## <Class::method or file>` + real ground-truth address/size +
what went wrong + what would be needed to unblock it.

Do NOT log routine Tier-B scope deferrals here (a class correctly judged
"needs an unmodeled dependency, precisely documented, no attempt forced") —
those belong in `HARDWARE_REVIEW_LOG.md`'s deferred registry as before. This
file is specifically for attempts that were MADE and failed to compile/link,
or where the reconstruction doesn't match ground truth after a real attempt.

---

## verify/test_client_comm_server (KNOWN pre-existing flake, NOT caused by this pass)

Recurred 2026-07-28 while running the full host `verify/` suite as part of an
unrelated `storage_format_converters` batch: `objs/verify/test_client_comm_server`
failed the same 6 known-answer checks (`OnRxSexWhenInWAIT` type=2/3/4,
`OnRxSexWhenInSENT` type=3/4/9/default) it has failed intermittently before.
Reproduced identically via `git stash` back to the pre-batch commit and
rebuilding/rerunning just this one binary in isolation, confirming nothing in
`storage_format_converters.*`/`storage_converter_ext_stubs.h` caused it.

This is NOT a fresh finding -- it is the SAME already-root-caused, still-open
issue documented at length in
`kronosology/.claude/agent-memory/re-decompiler/
eva_client_comm_server_6fail_closed_not_a_bug_2026-07-26.md`: a genuine
ASLR-dependent uninitialized-value read (confirmed via ASan/UBSan rebuilds
showing zero diagnostics and `setarch -R` reruns showing zero failures --
rules out both memory corruption and a deterministic source bug), still
un-localized because doing so needs MSan/valgrind (neither available on this
host). That same memory also documents a DISTINCT, structural contamination
risk worth restating here given this session hit a live git-index race with a
concurrent OA.ko agent working in the same repo: `Makefile`'s
`objs/verify/%: verify/%.cpp $(OBJ)` rule links every `verify/test_*` binary
against the ENTIRE object set, so another agent's uncommitted, mid-edit WIP
anywhere in `src/` at build time can transiently fail totally unrelated test
binaries. Do not re-diagnose this signature from scratch on a future
recurrence -- read that memory file first, and control for both the
ASLR/uninit-value flake AND any other agent's concurrent uncommitted state
before concluding it's a real regression.
