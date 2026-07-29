# Decompile Errors / Revisit List — OA.ko

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

## `oa_ckg_module_param_msg_handler.h` vs `oa_rtparm_pe_table.h` — conflicting `RT_run` linkage (2026-07-29)

**What happened**: both headers declare `RT_run(unsigned char, unsigned
char)` — `oa_ckg_module_param_msg_handler.h` as `extern "C"` (a
deliberate choice for that file's block of ~50 enum-widened KARMA
externs, documented in its own header comment), `oa_rtparm_pe_table.h` as
`extern "C++"` with `regparm(3)` (the real, GE/PE-table-verified mangled
linkage, `_Z6RT_runhh`). Including both headers in the same translation
unit is a hard compile error (`conflicting declaration ... with 'C++'
linkage`). Hit while reconstructing `CKGParamEdit::
GetRTParmBufferSelectId` (needs `CKGParamEdit`'s full class declaration,
only in the first header) alongside the rest of the RTParm free-function
family (needs `gRTParmFunctionTable_PE`, only in the second) — see
`HARDWARE_REVIEW_LOG.md`'s "RTParm free-function family, bottom-up batch"
entry for the full context. No prior file ever needed both headers
together, so this is a real, pre-existing latent inconsistency, not
something this batch introduced.

**Workaround applied (not a fix)**: `CKGParamEdit::
GetRTParmBufferSelectId`'s real body lives in its own translation unit,
`src/engine/rtparm_ckgparamedit.cpp`, which only ever includes
`oa_ckg_module_param_msg_handler.h`. It has its own KAT binary,
`verify/test_rtparm_ckgparamedit.cpp` / `verify/test_rtparm_ckgparamedit`
(wired into `make verify` alongside `test_rtparm_family`).

**What would unblock a real fix**: audit every real call site of the
~50-function `extern "C"` block in `oa_ckg_module_param_msg_handler.h`
(mainly `ckg_engine.cpp`) to confirm none of them actually depend on the
literal unmangled `"RT_run"` (etc.) linker symbol resolving — if none do,
that whole block can likely switch to `extern "C++"` with `regparm(3)`,
matching the real mangled names the GE/PE table work already verified,
and the two headers would stop conflicting. Not attempted this pass
(out of scope, touches a large previously-verified file under time
pressure) — a real repository-wide audit for other real KARMA `RT_*`/
`KS_*` externs with this exact same latent conflict (declared `extern
"C"` somewhere, `extern "C++"` somewhere else) would also be worthwhile
before the next cluster that needs both worlds.
