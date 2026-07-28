---
name: send-exec-midiport-combi-song-2026-07-28
description: CKGMIDIOutMsgHandler::SendExecToMIDIPortInCombi()/InSong() reconstructed from pre-captured disassembly (manifest 3204->3206/21689, commit 186873e); CKGKarmaAssignableSw::GetId() missing-override bug found+fixed in a follow-up sweep (commit fb691d3); widget family's manifest "pending" counts largely a generator blind spot, not real missing work
metadata:
  type: project
---

## What this is

Direct continuation of `ckg_midi_out_ccreset_completion_2026-07-28.md`'s
own "Deferred" section: that batch fully captured the raw disassembly for
`CKGMIDIOutMsgHandler::SendExecToMIDIPortInCombi()`
(`.text+0x3bbdf0`, 752 bytes) and `SendExecToMIDIPortInSong()`
(`.text+0x3bbc80`, 368 bytes) in `/home/share/Decomp/OA.ko_Decomp/OA.ko`
but deferred writing bodies. This batch reconstructed both directly from
that disassembly (re-verified fresh, not trusted from the prior summary).

Files: `include/oa_ckg_midi_msg_handler.h`, `src/engine/
ckg_midi_msg_handler.cpp`, `verify/test_ckg_midi_msg_handler.cpp`. Commit
`186873e`. Manifest 3204 -> 3206/21,689 (exact 2/2 credited via a
git-stash-baseline name-set diff SCOPED to only this batch's 3 files via
`git stash push -u -- <paths>`, not a full-repo `git stash -u` -- see
"shared-repo gotcha" below for why that scoping mattered today
specifically, 0 regressions).

## Song vs Combi: same overall shape, 3 real behavioral differences

Both are per-timbre (0-15) scans dispatching on the raw event's status
type (`m_status & 0xf0`: 0x90 NoteOn / 0x80 NoteOff / 0xb0 CC / 0xd0
Aftertouch / anything else = unconditional send), gated per-type by
`CheckZoneOfNoteOn()`/`CheckZoneOfNoteOff()` (this class's own virtuals,
already real) or `CMIDIFlowParamHolder::IsEnableTimbreCC()`/
`IsEnableTimbreAftertouch()`. Channel match is against `m_status & 0xf`
(the SAME raw combined status+channel byte convention already established
for this class by `CheckDyingNoteForMIDIPort()`, re-confirmed here, NOT
`m_flags & 0xf` which some SIBLING methods in this same class use for a
different purpose -- verified per-method, not assumed to generalize
class-wide).

Three real differences, all confirmed from raw disassembly, not guessed:

1. **Channel-match fallback (Combi only).** Combi compares
   `GetTimbreChannel(i)` directly only when it's a real channel number
   (`<=0xf`); a sentinel `>0xf` means "follow the current Local Control
   channel" and falls back to `GetLocalControlChannel()` instead
   (`cmp eax,0xf; jle <direct-compare>` vs the fallback branch at
   `.text+0x3bbe46`). Song has no such fallback at all -- a bare
   `GetTimbreChannel(i) != eventChannel` check.
2. **Early-return shape.** Song's CC/Aftertouch/default paths send once
   and return immediately; its NoteOn/NoteOff ALSO return immediately
   after a single send (`jmp 3bbcee` -> shared "test al,al; jne
   <send+ret>" tail). Combi's CC/Aftertouch/default do the identical
   send-and-return; but Combi's NoteOn/NoteOff NEVER return early -- they
   always fall through to `jmp 3bbe1c` (continue the scan) regardless of
   whether they sent or not, confirmed by tracing every exit edge from
   both the 0x90 and 0x80 blocks back to the loop-increment label rather
   than the epilogue.
3. **Transpose + dedup (Combi NoteOn/NoteOff only).** Combi's NoteOn
   computes `newNote = origNote + GetTimbreTranspose(i)`, drops it
   silently if outside `0..0x7f` (ground truth does the identical
   unbounded add-then-range-check with no separate guard, transcribed
   as-is), then temporarily overwrites `m_data1` with the transposed
   value for the send (ALWAYS overwritten before the dedup check, even on
   a duplicate -- restored to the original right after regardless of
   outcome; matches ground truth's exact instruction order, confirmed by
   tracing `mov [esi+0x5],al` landing BEFORE the dedup-search loop in both
   the 0x90 and 0x80 blocks). A per-call (stack-local, reset every call)
   dedup list of already-sent post-transpose notes prevents 2 timbres
   transposing to the same physical note from double-firing. NoteOff does
   the mirror computation but reads the transpose from a NEW persistent
   field, `m_noteTransposeCache`, instead of calling
   `GetTimbreTranspose(i)` again -- see below.

## New field: `int m_noteTransposeCache[128][16]` (real ground-truth +0x1090)

Confirmed via TWO independent pieces of evidence, not just the formula:

1. **The formula itself**, present at both the write site (NoteOn:
   `shl edx,4; lea edx,[ebx+edx*1+0x424]; mov [esi+edx*4],eax` -- unit
   index `i + note*16 + 0x424`) and the read site (NoteOff: identical
   unit-index arithmetic, but `add eax,[esi+edx*4]` reads instead of
   writes -- deliberately NOT a fresh `GetTimbreTranspose()` call, so a
   NoteOff turns off the exact transposed note that was actually turned
   on even if the timbre's live transpose setting changed in between).
2. **The real ctor's own zero-init loop** (`.text+0x3bc255`..`0x3bc30c`):
   `mov eax,0x80` down to 0, zeroing 16 consecutive DWORDs (one full
   `[timbre]` row) per iteration starting at `ebx+0x1090` -- independently
   confirms both the exact byte offset (`0x424*4 = 0x1090`) and the
   `[128][16]` shape without needing the formula at all.

This project's own `CKGMIDIOutMsgHandler()` ctor stays deliberately
minimal (matching established precedent -- it still doesn't call
`CDyingNoteInfo::Initialize()` 32x like the real 565-byte ctor does), but
`m_noteTransposeCache` specifically IS zero-initialized via a mem-
initializer (`: m_noteTransposeCache() {}`), because unlike the
dying-note arrays (whose existing tests only ever toggle-then-read a
single index, tolerating stack-garbage elsewhere), a "NoteOff with no
prior matching NoteOn in this same test" scenario would otherwise read
genuine uninitialized stack garbage as a transpose value -- real ground
truth avoids this via its own explicit zero-init loop, so this
reconstruction does too for the one field a NEW method actually depends
on, without touching the older out-of-scope ctor gaps.

## Test harness change: SKSTGGate_SendToMIDIPort mock now logs sent bytes

The existing mock only counted calls and recorded length -- insufficient
to verify WHICH note value was actually in `m_data1` at send time (the
whole point of the transpose/dedup KAT). Extended (backward-compatible,
existing call-count/length assertions untouched) with a
`g_sentNoteLog[32]`/`g_sentNoteLogCount` pair that snapshots `buf[1]`
(`m_data1`, since every caller passes `&m_status` and the class's own
layout is `status,data1,data2,flags` at `+0..+3`) per call, reset in
`reset_all_mocks()`.

## Independent oracle

`oracle_send_exec.py` (scratchpad, not committed) reimplements both
methods' logic directly from the disassembly notes above in plain Python
(dict-based cache, list-based dedup) and was run BEFORE writing any C++
KAT assertion to get every expected `(timbre, sentNote)` tuple
independently. Caught one scenario-design mistake before it became a
wrong assertion: an initial "Combi local-control-channel fallback" test
used a NoteOn-type message, which the oracle correctly returned empty
for (since `IsEnableTimbreNoteOn` also gates NoteOn and wasn't set in
that scenario) -- reworked to use a CC message instead to isolate the
channel-fallback logic being tested from NoteOn's own extra gating.

## Follow-up: CKGKarmaAssignableSw::GetId() missing-override bug (commit fb691d3)

The task's fallback instruction ("do a fresh manifest sweep... for the
next tractable subset") led to grouping remaining `CKG*`/`CSK*` pending
rows by class and picking the CKGSwitch/CKGKnob/CKGPad widget leaf family
(`CKGKarmaOnOffSw`, `CKGLatchSw`, `CKGPadModSw`, `CKGTapTempoSw`,
`CKGTempoKnob`, `CKGKarmaAssignableKnob`, `CKGKarmaAssignableSw`, etc --
see `ckg_switch_family_diamond_inheritance_2026-07-28.md`'s own "107
methods" claim) as the next candidate, since its base/inheritance
infrastructure was already established in a prior batch.

**Real finding: most of this family's manifest "pending" rows are a
manifest-generator BLIND SPOT, not real missing work.** Cross-checking
`nm -C OA.ko` (after a real `make ko` build) against every manifest-
pending qualified name in this family found the overwhelming majority
ALREADY present as real, correct, non-trivial weak (`W`) symbols in the
compiled module -- e.g. `CKGTapSwitch::GetCCValue/
AnalizeAndProcessCCMessage/AnalizeAndProcessKarmaControllerMessage`,
`CKGKnob::AnalizeAndProcessCCMessage`, every leaf's `GetCCNumber()` --
all already written as real bodies, just declared INLINE-IN-CLASS rather
than out-of-line `ClassName::Method() {`. `gen_oa_manifest.py`'s `DEF_RE`
regex only matches the out-of-line shape (same root mechanism as the
already-documented "keep deliberately-out-of-scope stubs inline to dodge
the heuristic" technique from `ckg_midi_out_ccreset_completion_2026-07-28.md`
-- turns out the SAME mechanism also hides genuinely-DONE inline work,
not just deliberate stubs, which the earlier note didn't call out).
**Practical implication: don't trust this family's raw "pending" COUNT as
a work-remaining estimate; always spot-check with `nm -C` against a real
`make ko` build before assuming a "pending" row is actually unwritten.**

Constructor symbols specifically (`CKGFFSw::CKGFFSw` etc) showed up as
"MISSING" from `nm -C` even for classes with fully-real, already-read
source bodies -- this is a SEPARATE, unrelated effect (ODR-use: an inline
function never emitted at all if nothing in the translation unit actually
constructs/calls it, standard C++ dead-code elision, not a correctness
signal) -- don't conflate the two when doing this kind of nm-based audit.

**The one genuine gap found**: `CKGKarmaAssignableSw::GetId()` was
silently falling through to `CKGToggleSwitch::GetId()`'s inherited
`return 0` default instead of having its own override, even though
`nm -C` against ground truth confirms a real, distinct
`_ZN20CKGKarmaAssignableSw5GetIdEv` weak symbol exists there (proof a
real override was written in ground truth, not just inherited). Every
one of this class's other 5 accessors (`GetCurrentValue`/`GetCCNumber`/
`GetCommonMsgId`/`GetModuleMsgId`/`GetResetValue`) already indexes by its
own `m_id`, and `GetId()` feeds `ResetKRTCSwitch(GetId())` call sites
elsewhere in this family (see `CKGChordAssignSw::
AnalizeAndProcessKarmaControllerMessage()`) -- the inherited
0-for-everyone default would make every per-slot instance of this class
(there are up to 8 real slots, `OA_CKG_BANKMGR_KARMAASSIGNSW_CCNUM_OFF`-
indexed) behave identically there, contradicting the whole per-slot
design already present in its OTHER 5 real accessors. Fixed to
`return m_id;`. Exact ground-truth byte encoding not directly confirmed
(the symbol resolves to address 0 pre-link in the unlinked ground-truth
object -- a COMDAT/weak-inline artifact, not a disassemblable address in
the static `objdump` view used elsewhere in this project) -- flagged
honestly in the fix's own comment rather than presented as byte-exact.

Files: `include/oa_ckg_switch_family.h`, `verify/
test_ckg_switch_family.cpp`. No manifest count change (expected -- the
fix is itself an inline one-liner, subject to the exact blind spot just
documented).

## Shared-repo gotcha: scope `git stash` to your own files, not the whole tree

Another concurrent session had unrelated, uncommitted Eva changes in the
same working tree throughout this batch (`Eva/include/
long_binary_file.h` modified, several new untracked Eva files, and
mid-session `Eva/src/base/file_operation_stub.cpp` ALSO changed --
confirming a live concurrent session, not stale leftovers). A full
`git stash -u` for the manifest-baseline diff would have swept up that
other session's in-progress work along with mine. Used
`git stash push -u -m <label> -- <exact OA paths>` instead (git >=2.13,
confirmed available: 2.39.5) to scope the stash to only this batch's 3
files, leaving the Eva changes completely untouched in the working tree
throughout. Combined with the already-documented practice
(`shared_repo_commit_hygiene.md`) of checking `git status --short`
immediately before every `git add`/`git commit`, both commits this batch
(`186873e`, `fb691d3`) contain exactly their intended files, confirmed via
`git diff --cached --stat` before each commit.

## Continuation

`CKGEngine` (74, still just a growing on-demand `ms_poInstance` stand-in),
`CKGBankManager`'s own remaining surface, `CKGRTCHandler` (27),
`CSKMIDILocalCtrlMsgHandler` (28), `CSKMIDIPortMsgHandler` (12),
`CKGUIMsgProcessor` (17), `CKGTimerManager` (15), `CKGSysExBuffer` (12).
`CKGParamEdit` (133) stays a confirmed-not-tractable rejection (see
`ckg_control_ui_msg_family.md`). Before picking up any more of the
switch/knob/pad widget family's remaining "pending" rows, re-run the
`nm -C` cross-check from this batch first -- most of what looks pending
there is likely already done.
