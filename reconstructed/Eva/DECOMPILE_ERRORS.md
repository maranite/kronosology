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

## CFileIoDos::format(EDevice_Id, int, EFatType) (deferred, not a compile/link failure)

`.text+0x0831afc0`, 0xbd8 = 3032 bytes -- the single largest method in the
2026-07-28 CFileIoAkai/CFileIoDos/CFileIoIso9660 storage-cluster batch (see
`HARDWARE_REVIEW_LOG.md`'s own entry for the batch as a whole). Fully
disassembled and traced (`objdump -dr -M intel`, ~680 lines) before concluding
it is not a good candidate for this pass -- logged here per this file's own
"attempted and found intractable" criterion, not a routine scope deferral.

What it is: a genuine, resumable, multi-call FAT-format state machine driven
by the static `CFileIoDos::iStage` global (`.bss+0x93b0fe0`, real symbol name
via `nm -C -S`) -- the function re-enters itself across multiple calls (driven
by the caller polling/retrying), dispatching on `iStage` (0..4) to: build a
boot-sector/volume-label string (`strncpy`/`strncat`/`str2upper`, several real
`.rodata` literals), select a cluster size via a real division-by-constant
loop cross-checked against `pc_fat_size(unsigned short, unsigned short,
unsigned short, unsigned short, unsigned long, unsigned short)`, zero-write
FAT/data-area sectors in a real progress-bar-driven loop
(`CDDriverIO::rw()`/`CDDriverIO::InitProcessBar()`/`SetProcessBar()`/
`EnableProgress()`), stamp a real system date/time
(`rtfs_getsysdate(_Dos_Date*)`), and finally call `pc_mkfs(short,
fmtparms*)` -- the embedded `fmtparms` struct IS this class's own object
storage from `+0x108` onward (at least 0x158 bytes' worth of named fields
touched: `+0x108` volume-label buffer, `+0x118` computed cluster-size-related
dword, `+0x134`..`+0x15e` an 11-byte-ish OEM/volume-label copy with 12
individual byte-flag checks driving a computed-goto-shaped dispatch to error
codes 1/2/3/4/5/6/7/8/9/0xb/0xc/0xe, `+0x140`/`+0x141`/`+0x148`/`+0x14c`/
`+0x150` various stage-transition/retry counters).

Why deferred: every individual piece above is, in isolation, tractable
(matches this project's own established idioms -- opaque `CDDriverIO`/
`fmtparms` stand-ins, real literal transcription, the same divide-by-100-via-
multiply idiom already reconstructed byte-for-byte in all 3 sibling classes'
own `dir()` methods) -- but the SHEER LENGTH (3032 bytes, ~680 disassembly
lines, a 5-stage state machine with ~15 named+numbered fmtparms fields and a
12-way byte-flag dispatch near the end) made it a poor fit for this batch's
time budget alongside the other 68 real entry points across 3 classes. Same
"surveyed and explicitly passed over" treatment `CFileIoCdda`
(41 overrides)/`CFileIoUdf` (35 overrides, `format()` alone 4791 bytes) got
in the batch before this one.

What would unblock it: a dedicated follow-up pass with the disassembly
already captured (this session's own `objdump` output, not re-captured from
scratch) -- primarily needs the `fmtparms` struct's own ~15 fields named with
confidence (most are already positionally identified above, just not yet
verified against a real DOS `fmtparms` header if one exists in any reference
RTFS SDK), and the 12-way byte-flag dispatch's real per-flag meaning (each
flag currently only known by its jump target's error-code literal, not its
semantic name).

## CFileIoCdda::getcurpos(EDevice_Id, EAudioStatusMMC*, unsigned char*, unsigned char*, int, int) (deferred, not a compile/link failure)

`.text+0x08318f30`, 0x74a=1866 bytes -- the largest method in the 2026-07-28
`CFileIoCdda`/`CFileIoUdf` storage-cluster batch (the last 2 concrete
`CFileIoBase` siblings, previously flagged "less tractable" in the
`CScsiDriverBase` batch -- see `HARDWARE_REVIEW_LOG.md`'s entry for the full
re-survey). Fully disassembled (`objdump -dr -M intel`, ~250 lines) before
concluding it is not a good candidate for this pass.

What it is: a genuine CD-DA track/index binary-search over a track
descriptor array embedded in the shared `cddadstat` search-state object
(`this->m_pStat`), walking successive `+0x10`-strided records comparing a
target LBA (from a preceding `USTGAPICDAudio::GetCurrentPosition()` call, a
real 64-bit-shaped output) against each record's own `+0x24`/`+0x28`
cumulative-length fields, then refining the result via a real,
multi-iteration `cdda_getidxstart()` loop (up to 4 calls per invocation,
comparing each returned start-LBA against the target and adjusting an
index/count pair). Also involves a real `CDDriverIO::scsi_get_event()`
probe and 2 real "last known position" cache-global writes (`.bss+0x93b0fb0`
and `.rodata`-adjacent `.bss+0x91b94a0`, both also written by the
already-reconstructed `play()`).

Why deferred: unlike `CFileIoDos::format()`'s deferral (a long but
essentially linear, stage-driven state machine), `getcurpos()`'s complexity
is a genuine multi-level nested-branch binary-search-plus-refinement
algorithm with real arithmetic over opaque track-descriptor fields --
transcribing it faithfully would require either full field-name recovery of
the `cddadstat` record layout (not otherwise needed by any other
already-reconstructed method in this class) or accepting a
structurally-plausible-but-unverifiable guess, neither of which met this
batch's bar for the other 40 tractable methods in the same class. NOT
declared as a virtual override in `file_io_cdda.h` (same convention as
`CFileIoDos`'s own deferred `format()`) -- the reconstructed vtable falls
back to `CFileIoBase::getcurpos()`'s own inherited stub (`return 0;`) rather
than leaving a declared-but-undefined symbol that would fail to link.

What would unblock it: a dedicated follow-up pass with this session's own
`objdump` output already captured, primarily needing the `cddadstat` record
layout's `+0x24`/`+0x26`/`+0x28`/`+0x30` fields named with confidence (all
currently only known by their role in the length-accumulation walk, not
their real CD-DA-TOC semantic names).

## CFileIoUdf::format(EDevice_Id, int) (deferred, not a compile/link failure)

`.text+0x0831d610`, 0x12b7=4791 bytes -- BY FAR the largest method in the
entire storage-driver cluster to date, larger even than
`CFileIoDos::format()`'s own already-deferred 3032 bytes. Fully disassembled
(`objdump -dr -M intel`, ~1050 lines) before concluding it is not a good
candidate for this pass.

What it is: a genuine, resumable, multi-call UDF-format state machine driven
by the static `CFileIoUdf::iStage` global (`.bss+0x93b1240`, confirmed via
`nm -C -S` -- every `ds:0x93b1240` read/write in the whole binary is
confined to this one method's own address range, independently verified via
a full-function grep, so no other reconstructed method in this class
secretly depends on it). Stages 2 through 0xb build real UDF volume/
partition/logical-volume descriptors, call `CFileIoUdf::formatsub()` twice
(already reconstructed -- a self-contained, non-iStage-touching leaf
routine), and interleave real `CDDriverIO::rw()`-shaped progress-bar-driven
sector writes.

Why deferred: same "every individual piece is tractable in isolation, but
the sheer length made it a poor fit for this batch's time budget" reasoning
as `CFileIoDos::format()`'s own deferral -- this method is over 50% larger
still. `formatsub()` and `setfmtparam()`, format()'s own 2 helper functions,
were successfully extracted and reconstructed independently since neither
touches `iStage` itself. NOT declared as a virtual override in
`file_io_udf.h` (same convention as `CFileIoDos`'s own deferred format())
-- the reconstructed vtable falls back to `CFileIoBase::format(EDevice_Id,
int)`'s own inherited stub (assert-log + `return -1;`) rather than leaving a
declared-but-undefined symbol that would fail to link.

What would unblock it: a dedicated follow-up pass with this session's own
`objdump` output already captured -- primarily needs each of the ~10
`iStage` values' own real UDF descriptor field layouts named with
confidence (positionally identified during this survey but not yet
cross-checked against a reference UDF/ECMA-167 descriptor header), and the
same kind of `fmtparms`-shaped local-buffer field naming `CFileIoDos::

## CFsConverterNormal::Process()/CFsCwInterpolation::Process()/
## CFsConverterNormal::BuildFilterCoeffTable()/CFsCwInterpolation::SetFilterCoeffs()
## (deferred, not a compile/link failure)

`.text+0x08304500` (1446 bytes), `.text+0x08305380` (1643 bytes),
`.text+0x08304cd0` (549 bytes), `.text+0x083059f0` (171 bytes) -- see
`include/fs_converter.h`'s own header comment for full detail. These 4
methods are the real polyphase-FIR ring-buffer sample-rate-conversion core
of the `CFsConverterNormal`/`CFsCwInterpolation` cluster (kaiser_window.h/
fs_converter.h/pcm_filter.h, found+reconstructed 2026-07-28). Every other
method in the cluster (58 addresses total, `CKaiserWindowCoeffs`/
`CDecimationFilterCoeffs`/`COversamplingFilterCoeffs` fully, plus
`CFsConverterNormal`'s/`CFsCwInterpolation`'s own ctor/dtor/`Reset()`/
delay-offset/bessel-length/sidelobe-attenuation/1-arg-`SetFilterCoeffs`
dispatcher methods) IS reconstructed and passes its own known-answer test
(`verify/test_fs_converter.cpp`).

Why deferred: the ring-buffer field layout (`SRingBufState`, fs_converter.h)
WAS fully recovered from the ctor/dtor/`Reset()` bodies (which ARE real and
exercise every field by name), but the actual convolution/ring-index
arithmetic inside `Process()` is a real, GCC 8-way-Duff's-device-unrolled
AND partially SSE-auto-vectorized per-channel circular-buffer FIR filter --
high risk of a subtle off-by-one or ring-wrap transcription error, and a
large enough chunk of novel DSP logic to warrant its own dedicated pass
rather than rushing it into this batch. `BuildFilterCoeffTable()`/
`CFsCwInterpolation::SetFilterCoeffs()` are deferred alongside it since they
only matter once `Process()` itself is real (they populate the coefficient
tables `Process()` consumes).

Stub bodies: `Process()` (both overrides) always reports 0 samples produced
(never fabricates output); `BuildFilterCoeffTable()` is a true no-op;
`CFsCwInterpolation::SetFilterCoeffs()` preserves the real
`mOversamplingRate` shift/un-shift bookkeeping (needed for
`CFsConverterNormal`'s own destructor to free the right number of
`mPhaseCoeffs[]` entries) without building the coefficient table itself.
None of these 4 addresses are marked `reconstructed` in
`manifest/eva_functions.csv` -- they stay `pending`.

What would unblock it: a dedicated follow-up pass tracing `Process()`'s
real ring-buffer write/read-and-convolve loop against the already-recovered
`SRingBufState` field layout (this pass's own `objdump -d -C` output for
both `Process()` overrides is a good starting point), plus
`BuildFilterCoeffTable()`'s per-phase coefficient-array allocation loop.
format()`'s own still-open item needs.
