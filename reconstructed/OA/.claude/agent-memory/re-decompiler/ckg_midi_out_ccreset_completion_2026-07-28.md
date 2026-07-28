---
name: ckg-midi-out-ccreset-completion-2026-07-28
description: CKGMIDIOutMsgHandler (11 own virtuals + KillAllDyingNotes) and CKGCCResetHandler (11 remaining virtuals) both fully completed, commit 69cbe6e, manifest 3182->3204/21689; the dependency web the prior batch discovered is now closed except 2 deliberately-deferred leaves
metadata:
  type: project
---

## What this is

Direct continuation of `ckg_midi_msg_processor_evtdisp_2026-07-28.md`'s
own "Continuation" section: that batch discovered `CKGMIDIOutMsgHandler`
and `CKGCCResetHandler` as brand-new dependency classes (needed by
`CKGMIDIMsgProcessor`'s real ctor) but left everything except `Process()`/
`Initialize()`/3 leaf overrides as inline no-op stubs, explicitly flagging
both classes' remaining slots as "next targets given they're already
partially wired". This batch gave real bodies to all of them except 2
(see Deferred, below).

Files: `include/oa_ckg_midi_msg_handler.h`, `src/engine/
ckg_midi_msg_handler.cpp`, `verify/test_ckg_midi_msg_handler.cpp`. Commit
`69cbe6e`. Manifest 3182 -> 3204/21,689 (+22, 0 regressions, verified via
full `git stash -u` baseline exact-name-set diff).

## Method: full vtable-slot-order table from real relocations, not guessed

Both classes' real vtable slot->method mapping was derived mechanically,
not from naming intuition: for every `call *0xN(%edx)` in the target
functions' disassembly, the real slot is `rodata_offset = N + 8`
(reconfirmed convention, object's own vptr already points AT vtable slot
0). Cross-referencing every such call against the class's own declared
method ORDER (assumed == vtable order, true for these single-inheritance-
only classes) resolved every indirect dispatch with certainty, including
cases where 2 different methods share the exact same 4-byte-apart slot
shape and would be easy to swap by mistake. Direct (non-vtable) calls
were resolved via `objdump -r` against `.text`'s own `R_386_PC32`
relocations at `call_addr+1` (or `+2` for non-EAX `mov reg,[addr]`
loads) -- this is how `CKGBankManager::ms_poInstance[+0x97c744/+0x97c748/
+0x97c749/+0x97c74c/+0x97c7b9/+0x97c7bf]` (6 more fields on that already-
known giant opaque aggregate) and `CKGEngine::ms_poInstance[+0xa0]`
(already known from `CKGUIMsgSender`'s own batch) were identified as the
real targets behind several `mov 0x0,%eax`-shaped relocatable loads.

## CKGMIDIOutMsgHandler: 2 new fields, exact real byte layout confirmed

```
CDyingNoteInfo m_dyingNoteInfo[16];        // real ground truth +0xc
CDyingNoteInfo m_dyingNoteInfoBackup[16];  // real ground truth +0x84c
```
Confirmed by 2 independent sources agreeing: `KillAllDyingNotes()`'s own
16x-unrolled restore-then-reinit sweep (`primary[i] = backup[i];
backup[i].Initialize();`), and `CheckDyingNoteForMIDIPort()`'s own
NoteOn/NoteOff index math (NoteOn always marks the BACKUP slot; NoteOff
checks PRIMARY first, falls back to BACKUP). This project's own
`CSKMIDIMsgHandler` base models 4 bytes fewer than ground truth's +0xc
alignment (already-known gap, see `CSKMIDIInMsgHandler::m_noteDownCount`'s
own `+0xc` comment) -- harmless here since every access is symbolic
through these members, never a raw absolute-offset read that would need
byte-exact ground-truth address matching.

### KillAllDyingNotes() real compiler scheduling artifact (do not copy the raw asm order)

The real 621-byte body's tail (restore primary[i] from backup[i], then
re-`Initialize()` backup[i]) is software-pipelined by the compiler: each
iteration's `Initialize()` call actually fires using a register value
computed ONE ITERATION EARLIER (i.e. the disassembly's own call for
"iteration N" really targets `backup[N-1]`, with the very last call
trailing after the visible loop). Confirmed by manually tracing which
`lea` fed which `call`'s implicit EAX `this`, not by trusting the
visual instruction order -- reconstructed as the semantically equivalent
2 straight-line loops (see comment in the .cpp). Same general caution
already documented for `CKGEventDisplayManager::Initialize()`'s own
out-of-order store emission in the prior batch's memory file.

### CheckDyingNoteForMIDIPort()'s NoteOn/NoteOff asymmetry

NoteOn (status hi-nibble 0x90) ALWAYS marks the BACKUP array and returns
true unconditionally, ignoring `TurnOn()`'s own return value. NoteOff
(0x80) checks the PRIMARY array first: if already on there, fires
`ProcessForDyingNote()` (self, via the message currently sitting in
`m_status`/`m_data1`) then clears it and returns FALSE; only if primary
says off does it check backup, clearing it and returning true only if it
WAS set. Any other status type returns true (default, unchanged from the
old stub). Verified via full register trace, not assumed symmetric with
NoteOn.

### ShouldSendChannelMessageToMIDIPort() / ShouldSendChannelMessageToSTG() / ShouldRecChannelMessageToSequencer(): same CC/PitchBend gate SHAPE, 3 genuinely different bodies

All 3 dispatch on `m_status & 0xf0` (CC=0xb0 / PitchBend=0xe0 / default
true) and reference the same `CKGBankManager` byte at `+0x97c748`, but
each reads DIFFERENT bits of it and gates on DIFFERENT channel values --
verified independently per method, not assumed to generalize:
- `ShouldSendChannelMessageToSTG()`: CC bit `0x1`/`0x10` of `+0x97c748`
  (channel==0 case returns immediately, no filter call), PitchBend bit
  `0x10`(>>4) of the same byte, `+0x97c7b9` for PitchBend channel==4, and
  a `CMIDIFlowParamHolder::GetLocalControlChannel()` comparison for CC/
  PitchBend channel==4 (`channel != localControlChannel`).
- `ShouldRecChannelMessageToSequencer()`: near-identical shape but the CC
  path ALSO calls `SKSTGGate_CheckVJSCCToMIDIPortFilter()` unconditionally
  (even for the cc==0/0x20 special case, where ground truth literally
  jumps INTO the middle of the generic-cc code path to reuse the shared
  filter-call tail -- modeled as inline equivalent logic, not a literal
  goto).
- `ShouldSendChannelMessageToMIDIPort()`: a completely different 4-gate
  chain (channel!=0, `+0x97c749`!=0, base `CheckGlobalFilter()`, then a
  `GetLocalControlChannel()`-match gated `ShouldSendChannelMessageToMIDIPortInEachMode()`
  call) folding into a shared tail keyed off `+0x97c7bf` and a CC-message
  `m_data1>=0x78` cutoff.

### SendChannelMessageOfActiveTimbreToMIDIPort(): clean 3-way dispatcher, fully self-contained

`CMIDIFlowParamHolder::GetVoiceMode()` selects between the class's own 3
remaining out-of-scope leaves: mode 1->`SendExecToMIDIPortInProgram()`
(now real, trivial forward to the base's own
`SendChannelMessageToMIDIPortWithCorrectLength()`), mode 2->
`SendExecToMIDIPortInSong()`, mode 0->`SendExecToMIDIPortInCombi()` --
confirmed by cross-checking each branch's own vtable-slot call target
against the class's declared method order, NOT assumed from mode-number/
method-name adjacency (mode 2 dispatching to "Song" rather than "Combi"
is the kind of off-by-one this cross-check exists to catch).

## CKGCCResetHandler: return-type bug fixed (prior batch's stubs were `void`, real ABI is `int`)

`ConvertToneModifyToCC`, `ProcessNRPN`, `ProcessNRPNIncDec`, `AdjustNRPN`
all genuinely return a value in EAX that real callers use (`ProcessNRPN`'s
own body does `cmp eax,0xff` against `ConvertToneModifyToCC`'s result).
The prior batch's safe-default `void` stubs would have silently produced
wrong codegen once these got real callers -- caught before writing any
body, by tracing each method's own USE of its callees' return values
first. `ProcessNRPN`/`ProcessNRPNIncDec`'s own early-exit path (flag
`+0x6f != 1`) leaves EAX holding a stale `this`-derived value in ground
truth; confirmed every real caller of both methods ignores the return
value entirely, so returning `0` there is a faithful-enough substitute
(documented in the .cpp, not silently assumed).

### HandleNRPNMessage()'s shared "&= ~0x30" tail

4 of the 7 switch cases (0x62/0x63/0x64/0x65) share a REAL jump into a
common tail in ground truth that re-masks the just-written `+0xd8` flags
byte. Modeled as identical inline logic duplicated per case (matching
observable behavior exactly) rather than a literal cross-case goto, since
C++ doesn't have a clean equivalent and the values differ per case going
in.

### StoreValue()'s CMIDIMessage raw-dword-at-offset-0 convention (reconfirmed)

Same real quirk already documented for `CKGMIDIMsgProcessor::
StoreCCMessage()`'s own comment: `CMIDIMessage`'s raw layout is a packed
status/data1/data2/flags dword starting at offset 0, NOT the usual
`CSKMIDIMsgHandler`-family `+4..+7` convention. `StoreValue()` copies the
whole dword directly into `this`'s own `+4..+7` fields via a single
`*(unsigned int*)`.

## Test harness fix required before this batch's KAT could run

`CKGCCResetHandler::ResetKarmaGeneratedValue()`'s new real body reads
`CKGEngine::ms_poInstance[+0xa0]` (already an established field from
`CKGUIMsgSender`'s own batch) -- but this file's own `g_engineBuf` mock
was only `0x40` bytes, an out-of-bounds read the moment the real body
existed. Enlarged to `0x200` (matching `test_ckg_ui_msg_sender.cpp`'s own
`ENGSZ` convention) before running any test. Same "before giving a real
body to a method reachable through an existing singleton, check every
existing mock's own buffer size against the new offset" lesson already
documented in the prior batch's memory file, now confirmed a recurring
concern in this specific file.

## Deferred (documented, not attempted)

`CKGMIDIOutMsgHandler::SendExecToMIDIPortInCombi()`/`SendExecToMIDIPortInSong()`
stay inline no-op stubs. Both are real, substantial, previously fully
disassembled bodies (~140/~300 real instructions each) implementing a
per-timbre scan over `CMIDIFlowParamHolder::GetTimbreStatus/Channel()`
with duplicate-value dedup logic and NoteOn/NoteOff/CC/Aftertouch-specific
sub-dispatch -- genuinely the deepest 2 methods in this class, not
low-risk in the way the rest of the batch was. Both are already reachable
(via `SendChannelMessageOfActiveTimbreToMIDIPort()`'s now-real dispatcher
above) but simply no-op for their 2 respective KARMA voice modes until a
future batch gives them real bodies. Raw disassembly for both was fully
captured this session; a future batch can pick this up directly from
`.text+0x3bbdf0`/`.text+0x3bbc80` in `/home/share/Decomp/OA.ko_Decomp/OA.ko`
without needing to re-derive the call targets (`CMIDIFlowParamHolder`'s
own getters, all already declared in `oa_ckg_control_ui_msg.h`).

## Verification

`make verify`: 92/92 test binaries green (10,570 individual `ok` checks,
0 FAIL, 0 crashes) -- ran the FULL suite, not just this file's own
binary, per this project's standing convention. Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos`: clean link, `OA.ko`
produced (vermagic `2.6.32.11-korg SMP preempt mod_unload ATOM`, matches
the real Kronos target), `nm` confirms all 22 new symbols defined (`T`),
none undefined (`U`), plus the 2 still-deferred stubs correctly emitted as
weak (`W`) inline symbols.

## Continuation

From the prior batch's still-open list, now with `CKGMIDIOutMsgHandler`/
`CKGCCResetHandler` closed except the 2 documented leaves above:
`CKGEngine` (74, still just a growing on-demand `ms_poInstance` stand-in),
`CKGBankManager`'s own remaining surface, `CKGRTCHandler` (27),
`CSKMIDILocalCtrlMsgHandler` (28), `CSKMIDIPortMsgHandler` (12, needs
`CSKMIDIInMsgHandler` as a real base -- already satisfied),
`CKGUIMsgProcessor` (17), `CKGTimerManager` (15), plus
`SendExecToMIDIPortInCombi`/`InSong` themselves as a small, well-scoped
2-method follow-up. Full sweep recipe unchanged: group
`manifest/oa_functions.csv` pending rows by class prefix `CKG`/`CSK`, sort
by count, cross-check `objdump -r` against `.rodata._ZTV*` before trusting
any class's own vtable/inheritance shape.
