---
name: ckg-engine-per-rtparam-table-2026-07-28
description: "CKGEngine 'per-RTParam table' cluster closed to 10/13 (RefreshPERTParmInfo/SetPERTParmMinMax/SetPERTParmControlModule/SetGERTParmMinMax/RefreshGERTParmInfo/SendChangeGEToEngine/DoInitModule/UpdateEnableDirectPathForVectorCC/DoRandomCaptureExec/SetMIDIFilterForUnusedModules), manifest 3461->3471; plus broad survey findings for the next dense cluster"
metadata:
  type: project
---

## What was done

Direct follow-up to [[ckg_engine_2026-07-28]]'s own "12-method per-RTParam
table family" deferral. First checked whether it was a uniform,
STG-value-getter-style mechanically-scriptable family (see
[[stg_value_getter_family]] for that technique) -- it is NOT: these are
real branchy functions (loops, calls into ~15 different KARMA-library
externs, struct copies), not a single field-extract-and-write-to-a-shared-sink
shape. Reconstructed by hand instead, one function at a time, cross-checking
every register against the raw disassembly (ground truth binary
`/home/share/Decomp/OA.ko_Decomp/OA.ko`, using the already-captured full
`ckgengine_full.asm` dump in the session scratchpad rather than re-running
objdump per function).

**10 of the 13 real candidate methods reconstructed** (the original
"12-method" label from `ckg_engine_2026-07-28.md` was itself an
off-by-one -- the real deferred list for this batch's own theme was 13:
`DoRandomCaptureExec`, `RefreshPERTParmInfo`, `SetPERTParmMinMax`,
`SetPERTParmControlModule`, `SetGERTParmMinMax`, `RefreshGERTParmInfo`,
`SendChangeGEToEngine`, `DoInitModule`, `UpdateEnableDirectPathForVectorCC`,
`ChangePerformance`, `CloseGECategoryPopup`, `UpdateGEInfo`, plus
`SetMIDIFilterForUnusedModules` which the original note didn't include in
the family list at all despite it belonging to the same address range).
`ChangePerformance` (960B, ~30 external calls across CKGRTCHandler/
CSKMIDIMsgProcessor/CKGMIDIMsgProcessor/CKGUIMsgSender/CKGParamEdit/
CKGTimerManager/CKGEventDisplayManager, a `.bss` array write), `CloseGECategoryPopup`
(1072B, largest single method left in the class) and `UpdateGEInfo` (368B,
odd-offset packed-struct writes from a stack buffer) were investigated only
enough to size them up, then correctly left deferred -- not attempted this
batch, still real, scoped follow-ups. `FakeTimbreThru`/
`CheckAndSendTimbreBendRange` also remain deferred (see below).

Commit: (see git log for this session's OA.ko commit). Manifest
3461 -> 3471/21,689 (+10, confirmed via a `git stash push -u`-based
before/after full name-set diff -- exact match, 0 regressions).
`make verify`: 0 FAIL, 10747 checks. Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos`: clean link,
`OA.ko` 759384 bytes, only expected external undefined KARMA-library
symbols (`KS_get_rtp_min_pe`, `KS_get_rtd_max_ge`, `RT_ge_select`,
`KGOutGate_NotifyEnableDirectPathForVectorCCToSoundEngine`, etc).
`DECOMPILE_ERRORS.md` untouched -- no genuine compile/link blocker hit.

## Record layouts confirmed (new)

- **"PERT" table**: `CKGBankManager::ms_poInstance[+8]` (== `SharedMemBase()`)
  `+ idx*0x12` (18-byte records, idx 0..7). Fields: `+0x0/+0x2` sorted
  (min,max) word pair from `KS_get_rtd_min_pe`/`KS_get_rtd_max_pe`;
  `+0x4/+0x6` sorted (min,max) word pair from `KS_get_rtp_min_pe`/
  `KS_get_rtp_max_pe`; `+0xa..+0xd` 4 "control" bytes and `+0xe..+0x11`
  4 "enabled" bytes, one pair per bit (0..3) of `KS_get_rtp_enabled_bits(idx)`:
  bit clear -> control=0,enabled=1; bit set -> control=`KS_get_rtp_multi_id_pe(idx)`'s
  own same bit ? 1:0, enabled=0 -- UNLESS `KS_get_rtp_bank_menu_pe(idx)==1`,
  which instead force-sets all 4 enabled bytes to 1 and leaves control
  untouched.
- **"GERT"/GE table**: `SharedMemBase() + ge*0x3c + module*0x780` (32
  ge-slots per module, `0x780 == 0x20*0x3c`). `+0xc0+8/+4/+6` =
  `KS_get_rte_val_ge/min_ge/max_ge(module,display=0,ge)`; `+0x1ec0+8/+4/+6`
  = same 3 with `display=1`; `+0xc0+0/+2` = sorted (min,max) word pair
  from `KS_get_rtd_min_ge`/`KS_get_rtd_max_ge(module,ge)` (display-0 block
  only -- display-1 has no rtd pair).
- **module record** (`m_currentModule + module*0x2e8`, 0x2e8=744 bytes
  total, confirmed via `DoInitModule`'s own whole-record `rep movs` copy):
  `+0x0` typeId (short), `+0x2` byte (low 5 bits = "program number",
  compared against `0x10` sentinel in several places -> remaps to
  `m_globalChannel`), `+0x3` voiceModelType byte (also `0x10`-sentinel
  remapped), `+0x14` bit `0x20` = a per-module "direct path" enable flag,
  `+0x20/+0x22/+0x24` and `+0x196/+0x198/+0x19a` = a 32-slot (stride 8)
  "velocity zone" word array DoInitModule preserves verbatim across a
  template reset, `+0x126` bit `0x20` = another preserved flag,
  `+0x294` = the 0x50-byte region `SendChangeGEToEngine`'s
  `m_geCategoryPopupOpen` backup snapshot copies from.
- **"effective channel" remap idiom**, confirmed shared by THREE methods
  (`UpdateEnableDirectPathForVectorCC` now real, `FakeTimbreThru`/
  `CheckAndSendTimbreBendRange` still deferred): a module's own
  voiceModelType byte (`+0x3`), remapped to `m_globalChannel` when `==0x10`,
  compared against either the module's own program-number-low-5-bits
  (`+0x2 & 0x1f`, itself remapped to `m_globalChannel` when `==0x10`) OR,
  if that fails, a fallback (`-1` if `m_perfType==1` or the raw `+0x2` byte
  is non-negative as SIGNED, else `m_globalChannel`).

## Reusable techniques / gotchas

**1. A prior batch's own deferred-method-count label can be off by one --
verify from the header's own `DEFERRED` comment count, not the prose.**
The "12-method" family label in `ckg_engine_2026-07-28.md` (and repeated
in this task's own briefing) was actually 13 real deferred methods once
counted directly from `oa_ckg_module_param_msg_handler.h`'s own
`DEFERRED` comment markers (`awk '/class CKGEngine/,/^};/' ... | grep -c
DEFERRED` gave 19 total, minus `IsEditedPerf` minus the 3-method
struct-copy pair = 15, not the 12 implied). Always recompute the real
count from the header before trusting a prose summary.

**2. "Not amenable to the STG-style scripted decoder" doesn't mean "not
tractable" -- it means a different reconstruction MODE.** The STG
value-getter family ([[stg_value_getter_family]]) is a genuine
mechanical field-extract-and-write pattern; THIS family is real branchy
control flow with external calls, but it's still highly hand-tractable
because the same few idioms (sorted-min-max-pair write, enabled-bits-gated
control/enabled byte pair, the "effective channel" remap) repeat across
every method -- once decoded once, each subsequent method is mostly
"which idiom, at which offset, with which extra wrapper logic".

**3. Existing tests that mock a soon-to-be-real method must be updated
BEFORE the new body ships, not after.** `test_ckg_engine.cpp` already had
its own `CKGEngine::SendChangeGEToEngine()` mock (from the earlier batch
that deferred it) -- giving the real class a real body created an
immediate multiple-definition link error, caught immediately by `g++`
during the very first compile attempt. Removed the mock, replaced it with
a mock for `ChangeValuesInBackupWhenChangingGE()` (the NEW dependency the
real `SendChangeGEToEngine()` body introduces, itself still deferred).
**General rule for this project going forward**: before writing a real
body for any method previously mocked in its own class's test file, grep
that test file for `ClassName::MethodName` and remove/repurpose the mock
FIRST, then write the real production body, then rebuild the test.

**4. A newly-real method can expose a genuine test-fixture gap in an
EARLIER, unrelated test block.** `UpdateUserGE()`'s own existing test
never set `m_currentCommon` (harmless while `SendChangeGEToEngine()` was
mocked and never touched it) -- once real, `SendChangeGEToEngine()` calls
`CopyCurrentParameterToSharedMemory()`, which dereferences `m_currentCommon`
unconditionally, segfaulting the WHOLE test binary. Caught via `gdb -batch
-ex run -ex bt` on the segfaulting binary (stdout buffering hid the exact
crash point when just piping to `head`/`tail` -- `stdbuf -oL` or a
redirect-to-file-then-`wc -l` sanity check is needed to trust "where did
it stop printing" as a diagnostic signal). **General rule**: whenever a
method that was previously mocked in a test file becomes real, re-run
EVERY existing test block that transitively calls it (not just the new
KAT blocks for the method itself) and treat any NEW segfault as a
fixture gap to fix, not a regression in the reconstructed code itself --
check what NEW field/dependency the real body touches that the mock never
did.

## Broad survey for the next dense cluster (per this session's own task)

Ran `manifest/oa_functions.csv`-grouped-by-class pending-count survey plus
a targeted `grep -rln`/`nm -S` check on the top non-STG-value-getter-family
candidates (the STG value-getter effort itself is a separate, already very
actively worked, well-documented ongoing project -- see
[[stg_value_getter_family]], which as of this session already spans 20
batches same-day; not re-litigated here).

**Top recommendation, NOT yet attempted: `InitializegRTParmFunctionTable_GE()`**
-- a single **34,435-byte** free function at ground-truth offset
`0x56b18d` (`.bss` symbol `gRTParmFunctionTable_GE` it populates is at
`0x630f40`). Directly thematically continuous with this session's own
"per-RTParam table" work -- confirmed via a raw disassembly sample
(`objdump -dr -M intel --start-address=0x56b18d --stop-address=+0x400`)
to be a **massively repetitive mechanical static-table initializer**:
a long flat sequence of `mov [gRTParmFunctionTable_GE+OFFSET], VALUE`
instructions (byte/word/dword literal writes, occasional
`R_386_32`-relocated function-pointer writes like `_Z10RT_ge_modehh`/
`_Z15RT_ge_gate_typehh`), no branches visible in the first 400 bytes
sampled. This is an EXCELLENT scriptable-decoder candidate in the exact
same spirit as the STG value-getter family's own script, just parsing
`mov ds:OFFSET, VALUE`/`R_386_32 SYMBOL` pairs instead of field loads --
likely tractable as ONE script pass over the whole 34KB function rather
than a class-by-class manual effort. A sibling free function
`ConvertPerfKarmaToX2100()` (12,127 bytes, `0x5629c8`) is also a strong,
smaller candidate, not yet investigated beyond its size/name.
`CSTGMonitorMixer::HasFeedbackLoop` (25,039 bytes) and
`CSTGVoiceAllocator::GetSlotVoiceRequirementsForPatches` (10,543 bytes)
are the next-largest PENDING single methods after those two, but are real
class methods (audio-routing graph traversal / voice allocation) likely
requiring genuine algorithmic understanding, not mechanical transcription
-- lower priority than the two free-function table initializers above.

**Class-level survey** (`manifest/oa_functions.csv` pending count grouped
by class, `grep -rln '\bClassName\b' include src` + `manifest` cross-check
for already-claimed/already-modeled status): `CSTGProgramSlot` (275),
`CSTGProgram` (156) confirmed ALREADY heavily modeled (skip, established
precedent from [[stg_value_getter_family]]). `CKGParamEdit` (133) and
`CSTGControllerInfo` (123) both CONFIRMED already investigated+correctly
rejected in an earlier CKG survey (see status.md's 2026-07-28
`CKGControlMsgHandler` entry) -- do not re-pick. The bulk of the
remaining top-20-by-pending-count list is still `CSTG*ModelPatch`/`CSTG*Osc`
-prefixed classes squarely inside the STG value-getter family's own
active scope, not a "fresh" cluster. Genuinely fresh, zero-reference,
real classes found via `grep -rln`: **`CSPRAudioPlayer`** (59 pending,
`.text+0x3ac100`..range, ctor/dtor/`AdvanceCurrentTick`/`StopCurrentEvent`/
`WaitUntilPlayStandby`/etc -- a real sequencer-audio-playback state
machine, entirely untouched), `CSTGProgramDownloader` (81), `CSPRRecDataMerger`
(62), `CSPRRecorder` (58), `CSPRParamEdit` (57), `CSPRStatusManager` (60),
`CSeqRPPR` (85) -- all 0 references anywhere in `include`/`src`, genuinely
fresh, but ALL are `CSPR`/`CSeq`-prefixed sequencer/recorder classes, the
same family [[stg_value_getter_family]]'s own batch-7/9 notes already
flagged as "likely a different, real-per-class mechanism, not investigated
individually" (`CSPRSeqDataManager` was the one instance actually checked
and confirmed a different, real, per-class mechanism with no shared
skeleton) -- worth a dedicated per-class investigation pass before
committing to a reconstruction batch, not assumed tractable from the
pending count alone.

## Open follow-ups

- `FakeTimbreThru()`/`CheckAndSendTimbreBendRange()` -- both real, both
  confirmed to share `UpdateEnableDirectPathForVectorCC()`'s own
  "effective channel" remap idiom (now a known, reusable formula) plus a
  per-module leader/dedup bitmap build not yet independently confirmed to
  byte-exact confidence. A real, scoped next continuation for THIS class.
- `ChangePerformance()` (960B), `CloseGECategoryPopup()` (1072B),
  `UpdateGEInfo()` (368B) -- all real, all sized-up but not attempted.
- `InitializegRTParmFunctionTable_GE()` (34,435 bytes) -- see survey
  above, the strongest concrete "next dense cluster" candidate found this
  session, likely scriptable.
