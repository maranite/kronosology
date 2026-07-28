---
name: ckg-engine-2026-07-28
description: "CKGEngine (KARMA performance-editing engine) reconstruction, 55/74 methods, commit 018ce7e; 2 manifest address-citation bugs found+fixed, ToU32/FromU32 host-test gotcha"
metadata:
  type: project
---

## What was done

CKGEngine (oa_ckg_module_param_msg_handler.h / src/engine/ckg_engine.cpp)
reconstructed: 55/74 real ground-truth methods, `.text`+0x3a96e0..0x3aee80.
Manifest 3406 -> 3461/21,689. Commits `018ce7e` (code) + `f1bf6cf` (docs).
Full writeup in `HARDWARE_REVIEW_LOG.md`'s own "CKGEngine" entry — this
memory is the condensed, reusable-technique version.

Found via a fresh `nm -C -S --size-sort`-style per-class survey after the
STG value-getter family and all three `CKG*ParamMsgHandler` classes were
closed. `CKGEngine::ms_poInstance`/`ms_poKGParamEdit` were already read by
dozens of previously-real CKG*/CSK* methods — it was the obvious next
target, previously a 24-declaration opaque stand-in.

19 methods deliberately deferred (declared, not defined): `IsEditedPerf()`
(9458 bytes, huge outlier), the "per-RTParam table" family
(FakeTimbreThru/RefreshPERTParmInfo/SetPERTParmMinMax/
SetPERTParmControlModule/SetGERTParmMinMax/RefreshGERTParmInfo/
SendChangeGEToEngine/DoInitModule/DoRandomCaptureExec/
UpdateEnableDirectPathForVectorCC/ChangePerformance/CloseGECategoryPopup/
UpdateGEInfo), and ChangeValuesInBackupWhenChangingGE (both overloads) +
ProcessForSeqWhenChangingGE (dense struct-copy, traced but not
byte-exact-confirmed). See [[ckg_module_param_msg_handler_family]] for the
sibling classes this one completes the picture around.

## Reusable techniques / gotchas

**1. Manifest ADDR_RE false-credit, new trigger: DEFERRED-method address
citations.** Already known from a prior batch (see
[[ckg_control_ui_msg_family]]) that citing a real address in prose for a
NOT-YET-implemented method falsely credits whatever unrelated ground-truth
function shares that manifest address. I re-triggered this myself via
every "DEFERRED -- .text+0xNNNNNN" comment in this batch (5 collisions
caught via baseline diff: CSPRHDRManager::SetErrorCode/
ShouldPlayCheckingAutoInput, CSPRAudioPlayer::WaitUntilPlayStandby/
SetStandbyNextEventBeforeRunning). **Fix, now the established pattern for
this project going forward**: for any DEFERRED/not-yet-implemented method,
write the real address as "ground-truth offset 0xNNNNNN" — NEVER the
literal substring ".text+0x" — so `ADDR_RE` can't match it. Only write
".text+0x" for methods that actually HAVE a real body in the same file.

**2. `comment_offset == raw nm address`, NOT `raw - 0x10000`.** The
established convention (`gen_oa_manifest.py`'s own docstring) is
`real_manifest_address = TEXT_BASE(0x10000) + comment_offset`. This means
`comment_offset` must be the function's RAW nm/objdump address, UNMODIFIED
— Ghidra's own `0x10000` image-base assignment is added ON TOP of that,
not derived by subtracting it first. I made exactly this mistake (computed
`offset = raw - 0x10000`, effectively double-subtracting), which put every
single address citation in this batch at the wrong manifest address —
caught only because the post-batch baseline diff showed unrelated
ground-truth names being credited instead of `CKGEngine::*`. **Always
sanity-check**: pick one already-known-correct citation elsewhere in the
project (e.g. grep the manifest CSV for a function you know is real), confirm
`manifest_address == raw_nm_address` directly (no arithmetic needed) before
writing a batch of new citations, not after.

**3. `ToU32()`/`FromU32()` packed-32-bit-pointer convention needed for
raw-offset pointer FIELDS, not just struct members.** Already documented
in `oa_engine_init.h`/`oa_engine.h` for STRUCT FIELDS ("packed 32-bit
pointer... 4 bytes on the real target"), but I hit the SAME issue reading
`CKGBankManager::ms_poInstance[+0]`/`[+4]`/`[+8]` as 3 independent 4-byte
pointer slots via raw offset casts (`*(unsigned char **)(bankMgr+N)`) —
on this project's 64-bit verify-test host, an 8-byte pointer write/read at
offset N silently overlaps offset N+4's own slot. Symptom: a segfault or
(worse) a SILENT wrong value deep inside a called function, not at the
read site itself — cost real debugging time via gdb watchpoints before the
root cause was clear. **Fix pattern**: use `ToU32(void*)`/`FromU32(u32)`
(4-byte-truncate / zero-extend) for ANY raw-offset pointer-sized field
read, whether it's your own struct's field or an external raw-offset
blob's field, and compile the test with `-fno-pie -no-pie` (same fix as
`test_tone_adjust_descriptors`, see its own Makefile comment) so static
mock buffers land in the low 4GB and the round-trip doesn't lose the
pointer's high bits. **Same fix used twice in ONE test session** (once in
production `ckg_engine.cpp` itself, once in the test's own mocks) — check
for this pattern EARLY (before writing any raw-offset pointer read) on any
future class touching another class's blob via `+N` pointer offsets.

**4. GCC inline-memcpy decoding.** `CopyCurrentParameterToSharedMemory()`'s
3 segments were each a `rep movsd` + conditional `movsw`/`movsb` tail —
the tail encoding is the REAL total byte count's own low 2 bits (`count &
2` / `count & 1`), not something to hand-transcribe instruction-by-
instruction; cross-check the visible dword count (`ecx` before `rep`)
against the visible `mov eax,N` (used for the tail test) to recover N
exactly, then just call `__builtin_memcpy(dst, src, N)` in the
reconstruction — behaviorally identical, far less error-prone than
reproducing the tail dance.

**5. Real logic bug SendChannelMessage caught only by KAT, not by
re-reading disassembly a second time**: the `m_field0` gate direction was
backwards on the first draft (assumed "do the RT_channel_in/
ProcessTimbreThruChannelMessage dispatch when m_field0==0", actual real
body is "do NOTHING AT ALL when m_field0!=0, else dispatch on a DIFFERENT
field, bankMgr[0x20]"). Two completely different fields conflated during a
fast read. Independent KAT with an explicit "no calls happened" assertion
for the early-return case is what caught it — a bug like this can hide
even after "re-reading the disassembly" if you re-read your OWN prior
(wrong) mental model instead of the raw bytes again.

## Open follow-ups

- `IsEditedPerf()` (9458 bytes) — needs a dedicated pass, likely scriptable
  (huge repetitive per-RTParam comparison table, same shape as the
  "per-RTParam table" family below it).
- The "per-RTParam table" family (12 methods, see deferred list above) —
  proven mechanical but lengthy; a scripted decoder (same idea as
  [[ckg_seq_backup_technique]]) would likely make this tractable in one
  more session.
- `ChangeValuesInBackupWhenChangingGE`/`ProcessForSeqWhenChangingGE` — the
  dense live-to-backup struct copy, traced far enough to know the shape
  but needs a slower, more careful pass (or a scripted field-offset
  extractor) before committing to a transcription.
- `CKGTimerManager`'s remaining 9 methods (AdvanceClock/IncElapsedTick/
  SetCurrentTempo/SetTempoPercent/GetIntervalClock/ReceiveMIDIClock/
  ShouldTempoLEDFlash/GetTicksUntilTheBeat/GetKarmaIntervalClock/
  SetTempo) — a real, self-contained, well-scoped next cluster (only 5 of
  14 done, the ones CKGEngine itself calls).
