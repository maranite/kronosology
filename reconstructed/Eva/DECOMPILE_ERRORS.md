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

## verify/test_client_comm_server (pre-existing, NOT caused by this pass)

Found 2026-07-28 while running the full host `verify/` suite as part of an
unrelated `storage_format_converters` batch. `objs/verify/test_client_comm_server`
fails 6 known-answer checks, all in the `OnRxSexWhenInWAIT`/`OnRxSexWhenInSENT`
dispatch tests (client_comm_server.cpp):

  - `OnRxSexWhenInWAIT(type=2/3/4)` expected to dispatch into `Error()`
    (mState -> 0, no `TransmitSysEx()` call) -- actual behavior differs.
  - `OnRxSexWhenInSENT(type=3)` expected to resend + bump `mState0d` on a
    matching retry -- actual behavior differs.
  - `OnRxSexWhenInSENT(type=4)` and `(type=9, default)` expected to be
    genuine no-ops -- actual behavior differs.

Confirmed PRE-EXISTING and unrelated to this batch: reproduced identically
(same 6 failures) by `git stash`-ing back to commit `2e423b1` (the last commit
before this batch's changes) and rebuilding/rerunning just this one binary.
Nothing in `storage_format_converters.*`/`storage_converter_ext_stubs.h` is
included by or related to `client_comm_server.cpp`. This is either a stale
known-answer assumption in the test itself or a genuine gap in the
`OnRxSexWhenInWAIT`/`OnRxSexWhenInSENT` reconstruction (client_comm_server.cpp,
last touched by commit `8271d30`) -- needs a fresh disassembly pass against
those 2 methods' real dispatch tables to determine which. Logged here rather
than silently ignored so `make verify`'s exit code (currently 1, from this one
binary) isn't mistaken for a regression introduced by a later, unrelated batch.
