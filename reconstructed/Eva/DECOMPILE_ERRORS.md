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
format()`'s own still-open item needs.

## CFsConverterNormal::Process()/CFsCwInterpolation::Process() (deferred,
## not a compile/link failure -- CFsConverterNormal::SetFilterCoeffs(int,int,
## int,int,int)/CFsCwInterpolation::SetFilterCoeffs(int,int,float,int,int),
## originally deferred alongside these, were RECONSTRUCTED 2026-07-28, see
## below)

`.text+0x08304500` (1446 bytes) / `.text+0x08305380` (1643 bytes) -- see
`include/fs_converter.h`'s own header comment for full detail. These 2
methods are the real polyphase-FIR ring-buffer convolution core of the
`CFsConverterNormal`/`CFsCwInterpolation` cluster (kaiser_window.h/
fs_converter.h/pcm_filter.h, found 2026-07-28). Every other method in the
cluster (46 addresses total, verified via `grep -c` against
`manifest/eva_functions.csv` -- corrects the original pass's own "58"
estimate, `CKaiserWindowCoeffs`/`CDecimationFilterCoeffs`/
`COversamplingFilterCoeffs` fully, plus `CFsConverterNormal`'s/
`CFsCwInterpolation`'s own ctor/dtor/`Reset()`/delay-offset/bessel-length/
sidelobe-attenuation/`SetFilterCoeffs` (both the 1-arg dispatcher AND, as of
this same-day follow-up pass, the real 5-arg int-only and float-arg
overloads) methods) IS reconstructed and passes its own known-answer test
(`verify/test_fs_converter.cpp`).

Follow-up pass, same day: `CFsConverterNormal::SetFilterCoeffs(int,int,int,
int,int)` (0x08304cd0, internally named `BuildFilterCoeffTable`) and
`CFsCwInterpolation::SetFilterCoeffs(int,int,float,int,int)` (0x083059f0)
turned out fully tractable once decoded end to end via `objdump -dr -M
intel` -- both are now real (see fs_converter.cpp) and marked
`reconstructed` in `manifest/eva_functions.csv`. That same pass also
corrected the header: `BuildFilterCoeffTable()` is actually VIRTUAL (vtable
slot 5, confirmed via a direct `.rodata` dump of both classes' vtables,
`.rodata+0x08f31280`/`.rodata+0x08f312e0`) rather than the plain method
first assumed, and both `SetSideLobeAttenuation(double)`/
`SetBesselFunctionLength(int)` on `CFsConverterNormal` and `Reset()` are
likewise virtual thin wrappers, not compiler-inlined convenience calls --
see the header's own updated comment for the full 9-slot vtable layout.

Why `Process()` itself is STILL deferred: the ring-buffer field layout
(`SRingBufState`, fs_converter.h) was fully recovered from the ctor/dtor/
`Reset()`/(now also) `SetFilterCoeffs()` bodies, and this follow-up pass
traced `Process()`'s outer structure precisely (per-channel outer loop;
per-channel ring-fill inner loop bounded by `mState->mCarry`, 8-way-Duff's-
device-unrolled; a second per-output-sample FIR-sum inner loop doing
`mPhaseCoeffs[phase][tap] * mChannelRing[ch][(idx-tap)&mRingMask]` via
chained x87 `fmul`/`faddp` -- NOT SSE-vectorized on closer inspection,
correcting an earlier overstatement; `mCarry`/`mRingWritePos` written back
once at the end for all channels; a real, separate `mNumChannels==0`
early-out). See `include/fs_converter.h`'s own header comment for the full
outline. But the exact tap-index/ring-wraparound arithmetic across those 3
nested loops is still high-risk to transcribe -- a single off-by-one
produces plausible-sounding but wrong audio, and this project has no golden
reference PCM stream to check against, unlike the `SetFilterCoeffs()` pair
above whose correctness was verifiable purely from already-tested
`mFilterCoeffs`/field-layout state. Stays out of scope for a single pass.

Stub bodies: `Process()` (both overrides) still always reports 0 samples
produced (never fabricates output) -- these 2 addresses (0x08304500,
0x08305380) stay `pending` in `manifest/eva_functions.csv`.

What would unblock it: a dedicated follow-up with a synthesized
known-answer input (e.g. an impulse or a single sinusoid at a known
frequency) run through both `Process()` and a from-scratch reference
polyphase resampler implementation and compared numerically -- not just
re-deriving the assembly by inspection, which is how every other method in
this cluster (including the now-real `SetFilterCoeffs()` pair) was
verified, but isn't sufficient on its own for a stateful per-sample DSP
inner loop this dense.
