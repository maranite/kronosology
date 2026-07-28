---
name: ckg-msgprocessor-paramchange-2026-07-28
description: CSKMIDIMsgProcessor (22) + CSKParameterChangeMessage (10) reconstructed, commit 060190e, manifest 3112->3144/21689; widget-hierarchy thunk-gap independently re-confirmed as a non-issue; 2 real pre-existing test bugs found+fixed
metadata:
  type: project
---

## What this is

Continuation of the CKG/CSK KARMA cluster sweep after
[[ckg-midi-inmsg-family-2026-07-28]]. Before picking a new target, the
prior batch's own claim ("~194 methods in the widget hierarchy, ~114 real
bodies, rest auto-materializes as compiler thunks") was independently
re-verified rather than trusted: `objdump -dr` on a sample pending row
(`CKGToggleSwitch::AnalizeAndProcessKarmaControllerMessage` at ground
truth `.text+0x3b82d0`) showed a genuine `_ZTv0_n48_...` virtual-thunk
stub (`add eax,[ecx-0xc]; call real_impl`), and `nm OA.o | grep _ZTv0`
confirmed this project's OWN compiled output already emits the matching
thunk (`51` total `_ZTv0_*` symbols) purely from the real `virtual public`
inheritance already written -- the ~93 remaining pending rows in that
family are a confirmed, permanent manifest-heuristic blind spot (thunks
are compiler-generated, never appear as source `Class::Method(...) {`
text), not new work. Don't re-pick this family expecting to close the
gap further.

Picked instead: `CSKMIDIMsgProcessor` (22 methods) + `CSKParameterChangeMessage`
(10 methods), found via a fresh pending-row group-by-class-prefix sweep.
Both were deliberately chosen because they were ALREADY partially wired
into existing real code as opaque stand-ins (see below) -- promoting them
needed zero new cross-header opaque-dependency machinery, unlike a
cold-start class.

Files: `include/oa_ckg_midi_msg_handler.h` (CSKParameterChangeMessage
expanded in place; CSKMIDIMsgProcessor added as a new class at the end,
after its 6 real dependency classes), `include/oa_ckg_control_ui_msg.h`
(CSKMIDIMsgProcessor's old 6-method opaque stub removed, replaced by a
pointer comment -- verified nothing between the removed stub's old
location and this header's own `#include "oa_ckg_midi_msg_handler.h"`
line actually used the type by name before that point), `src/engine/
ckg_midi_msg_handler.cpp`, `verify/test_ckg_midi_msg_handler.cpp`.
Commit `060190e`. Manifest 3112 -> 3144/21,689 (+32, 0 regressions,
verified via full `git stash -u` baseline exact-address-set diff).

## CSKMIDIMsgProcessor real field layout (from the 217-byte ctor's own
`AllocAligned()`+placement-ctor sequence, `objdump -r`)

```
+0x00 CSKMIDIPortMsgHandler          *m_port        AllocAligned(0x142c,0x10)
+0x04 CSKMIDILocalCtrlMsgHandler     *m_localCtrl    AllocAligned(0x34ac,0x10)
+0x08 CSKSpecialMsgHandler           *m_special      AllocAligned(0xc,0x10)
+0x0c CSKMIDIKarmaCtrlMsgHandler     *m_karmaCtrl    AllocAligned(0x34ac,0x10)
+0x10 CSKPadNoteByMIDIPortMsgHandler *m_padByPort    -- see vptr-poke idiom below
+0x14 CSKPadNoteByLocalCtrlMsgHandler *m_padByLocal  -- same idiom
+0x18 int   m_lastMsgKind      -- dispatch selector, exact enum unconfirmed
+0x1c unsigned char *m_activeRawEvent -- points at whichever sub-handler's
      own +4 raw-event byte quad while a dispatch call is in flight
+0x20 int   m_lastMsgSentinel  -- companion selector, unconfirmed
```
`m_special` (+0x8) is deliberately NEVER deleted in the real destructor
(confirmed: the dtor's own field-walk visits +0x14/+0x10/+0xc/+0x4/+0x0,
skipping +0x8 entirely) -- a real, intentional-looking leak, not a
modeling gap.

**Real "construct via parent, then poke vptr" idiom**: ground truth
builds `m_padByPort`/`m_padByLocal` by calling `CSKMIDIPortMsgHandler`'s/
`CSKMIDILocalCtrlMsgHandler`'s OWN ctor (same-sized allocation as their
non-pad siblings), then overwrites the constructed object's vptr slot IN
PLACE with `vtable for CSKPadNoteByMIDIPortMsgHandler`'s own address
(confirmed via a `R_386_32` relocation directly on a `mov [esi], <vtable
symbol>` instruction right after the parent ctor call, before storing the
pointer into the field). This reconstruction does NOT hand-poke a vptr --
placement-construct directly through the derived type
(`new (buf) CSKPadNoteByMIDIPortMsgHandler()`) produces the identical end
state with real C++. Worth remembering as a general pattern: when ground
truth "constructs via the base ctor then patches the vtable pointer",
that's just an object-file-level optimization for two classes with
identical layout/size and no new fields -- always safe to replace with
direct construction through the derived type.

## The call_off table for this class (verified via `objdump -r` against
`.rodata._ZTV19CSKMIDIInMsgHandler`/`.rodata._ZTV26CSKMIDILocalCtrlMsgHandler`/
`.rodata._ZTV20CSKSpecialMsgHandler`, `rodata_offset = call_off + 8`)

```
0x00 -> 0x08  CSKSpecialMsgHandler::AnalizeAndProcess(unsigned char*)
0x40 -> 0x48  ~ClassName() [deleting dtor, D0]
0x44 -> 0x4c  AnalizeAndProcess(unsigned char*, int)
0x48 -> 0x50  AnalizeAndProcessNoteOffWhilePerformanceChange(unsigned char*, int)
0x4c -> 0x54  Process()
0x50 -> 0x58  IsDamperOn()
0x54 -> 0x5c  IsSostenutoOn()
0xc8 -> 0xd0  InitializeExtNoteOnChecker()      [LocalCtrl-only new slot]
0xcc -> 0xd4  CopyNoteOnStatus(unsigned char*)  [LocalCtrl-only new slot]
0xd0 -> 0xd8  IsKeyboardAllOff()                [LocalCtrl-only new slot]
0xd4 -> 0xdc  ClearNoteStatus()                 [LocalCtrl-only new slot]
```
Two call_off values (0x44 vs 0x4c, 0x48 vs 0x50) look confusingly similar
-- got these swapped once mid-batch before cross-checking against the
table a second time. Always look the call_off up in the table, never
trust a half-remembered mapping from a few instructions ago.

## Real transcription bug caught: `status + channel`, not `status` alone

All 5 `Process*ChannelMessage()` overloads
(Local/MIDIPort/KarmaControllerGenerated/PadByLocal/PadByPort) share an
IDENTICAL opening instruction: `lea edx,[ecx+edx*1]` -- i.e. `edx(status)
+ ecx(channel)` computed once, stored as the sub-object's `m_status`.
First draft stored `status` alone and discarded `channel` entirely with
`(void)channel;` (an easy mistake since the 5 functions otherwise look
like simple "copy 2 params into 2 fields" forwarders) -- caught by
re-reading each function's own prologue a second time before writing the
KAT, not by the KAT itself. `m_flags` for each is a FIXED literal (0x01
for the 3 "internal" sources, 0x00 for the 2 raw MIDI-port-shaped ones),
confirmed independently, not derived from either parameter.

## CSKParameterChangeMessage: 14-byte SysEx-shaped layout

```
+0x00 0xf0 (SOX, fixed)              +0x08 param4 (5-byte ovl) / param5 (7-int ovl)
+0x01 0x42 (Korg mfr ID, fixed)      +0x09 param5 (5-byte ovl) / param6 (7-int ovl)
+0x02 globalChannel|0x30 (or a       +0x0a value bits 20:14 (7b)
      source-seq tag override)       +0x0b value bits 13:7 (7b)
+0x03 0x68 (fixed function ID)       +0x0c value bits 6:0 (7b)
+0x04 kind byte ('m'/'C'/'n'/'A')    +0x0d 0xf7 (EOX, fixed)
+0x05 param2
+0x06 param3
+0x07 0 (5-byte SetParameters overload) / param4 (7-int overload)
```
The 2 `SetParameters` overloads share almost everything but disagree on
where the "4th param" lands (+0x7 stays 0 for the 5-byte-param version;
the 7-int version's 4th param genuinely occupies +0x7) -- confirmed by
tracing BOTH overloads' own register sequences independently, not
assumed from the first one. `GetValue()` sign-extends the 21-bit combined
value based on bit 6 of the MSB byte (`msb & 0x40` -> `value |=
0xffe00000u`), confirmed via the real `cmp/sbb/not/and` sequence, not
guessed from the name.

**Real, confirmed quirk in `IsThisParamChage()`**: the +0x2 comparison
sign-extends `CKGBankManager::ms_poInstance[0x97c747]` (a `signed char`)
to a full int, ORs in `0x30`, then compares against a ZERO-extended
byte read of `this->m_bytes[0x2]`. If the bank-manager byte is negative,
the sign-extended comparand can never equal a zero-extended byte, so the
whole method silently always returns false for that case -- transcribed
byte-exact, not "fixed" into a symmetrical comparison.

## 2 real pre-existing bugs found+fixed while verifying (not introduced
by this batch, but only surfaced by it)

1. **`CSKMIDIMsgProcessor::ms_poInstance` was never wired to a valid
   instance in the KAT harness.** A PRIOR batch's `CSKMIDIMsgHandler::
   StoreDyingNoteInfoForSTG()` (see `ckg_midi_msg_handler.cpp` around
   line 138) already casts `CSKMIDIMsgProcessor::ms_poInstance` and calls
   through it unconditionally -- harmless while the target method was an
   empty `{}` test mock (calling a no-op member function on a null `this`
   never dereferences anything), SIGSEGV (near-null READ) the moment this
   batch gave it a REAL body that dereferences `m_localCtrl`/`m_port`.
   Root-caused via a `-fsanitize=address` rebuild of just the one test
   binary (`g++ ... -fsanitize=address ...`, not the project's own
   Makefile flags) -- ASan's own backtrace pointed straight at the
   dereference, no manual bisection needed. Fixed with a real singleton
   test double (`OA_TestSKMIDIMsgProcessorSingleton()`, a function-local
   static wiring 6 real stack-constructed sub-objects, set into
   `CSKMIDIMsgProcessor::ms_poInstance` inside `reset_all_mocks()`) --
   matches what the real ctor always does in ground truth, not a
   workaround. **Lesson for future batches**: before giving a real body
   to any method whose OLD empty-mock version was silently reached
   through an uninitialized/null singleton elsewhere in an
   already-committed TU, grep the WHOLE file for other call sites of that
   exact singleton, not just the test cases for the class under test.
2. **The mock note-display buffer was ~400x undersized against real
   ground-truth offsets.** `CKGBankManager::ms_poInstance[+8]` is itself
   a pointer to a "note display" sub-object; several ALREADY-COMMITTED
   methods from a PRIOR batch (`NotifyNoteCountToUI()`,
   `NotifyDamperStatusToUI()`, `NotifySostenutoStatusToUI()`,
   `NotifyNoteEventToUI()`) write through it at real offsets up to
   `0x147a7`, but the test's own mock buffer was only `0x200` bytes --
   an out-of-bounds write that had apparently been silently corrupting
   unrelated static memory for at least 2 prior batches without ever
   crashing, until THIS batch's own new static globals happened to shift
   the memory layout enough to hit something load-bearing (confirmed via
   ASan pointing directly at the OOB write, not inferred). Fixed by
   enlarging to `0x14800` bytes. **Lesson**: an OOB write into unused
   static memory can pass `make verify` silently for an arbitrary number
   of unrelated batches before a completely unrelated future change
   exposes it as a crash -- if a KAT binary ever segfaults after a batch
   that didn't touch the crashing function's own file, suspect a
   pre-existing OOB write whose "landing zone" just moved, and reach for
   ASan immediately rather than bisecting by hand.

## Verification

`make verify`: 94/94 binaries green, 0 failures. New KAT coverage added:
`CSKParameterChangeMessage` full round-trip (SetParameters x2, GetValue's
sign-extension, SetValue(int), IsThisParamChage's magic-byte gate,
SetSourceSeq/Restore/Reset's nibble-preserving rewrites) against values
independently hand-derived from the bit-math itself (not copy-pasted from
the implementation under test); `GetNowProcessingNoteOffVelocity()`'s
null/NoteOn/NoteOff 3-way branch; the `status+channel` combine fix and
`m_flags` literal for 3 of the 5 `Process*ChannelMessage()` overloads;
`IsKeyboardAllOff()`'s 4-gate short-circuit (both sub-handlers' own
`IsKeyboardAllOff()`, then `IsDamperOn()`, then `!IsSostenutoOn()`).

Real `make ko-clean && make ko KDIR=/home/build/linux-kronos` Kbuild
build: clean link, `OA.ko` produced, `nm OA.o` confirms every new method
defined (`T`, not `U`); the only remaining undefined reference touching
this area is the pre-existing, out-of-scope
`CSPRSysExBufManager::SetValue`.

## Continuation

Remaining sizeable pending clusters in the CKG/CSK sweep (by count,
`CKGParamEdit` still excluded as previously rejected -- see
[[ckg_control_ui_msg_family]]): `CKGEngine` (74, its own methods --
currently only an opaque `ms_poInstance` stand-in with handful of methods
added on demand), `CKGBankManager` (38, same situation), `CKGRTCHandler`
(27), `CSKMIDIMsgProcessor`/`CKGMIDIMsgProcessor` -- wait, NOTE:
`CKGMIDIMsgProcessor` (13 pending, a DIFFERENT class from
`CSKMIDIMsgProcessor` reconstructed here, own singleton, own vtable-less
plain-class shape, KARMA-generated-CC-value tracker, currently a
4-method opaque stand-in in `oa_ckg_control_ui_msg.h`) is a natural
next-batch companion given the naming similarity and that it's already
partially wired in the same way this batch's 2 targets were.
`CKGUIMsgProcessor` (17, more complex parsing -- `AnalizeAndProcess
ParamterChangeMessage` alone is 567 bytes), `CKGTimerManager` (15,
no vtable, clean self-contained tempo/clock class),
`CKGEventDisplayManager` (15, no vtable, ALREADY has a 2-method opaque
stub in `oa_ckg_midi_msg_handler.h` used by
`CSKMIDIInMsgHandler::NotifyNoteEventToUI()` -- same "already partially
wired" opportunity as this batch's own targets), `CKGMIDIOutMsgHandler`
(15, has a vtable, own inheritance not yet checked), `CKGCCResetHandler`
(13, has a vtable), `CKGSysExBuffer` (12, has a vtable). Full sweep
recipe unchanged: group `manifest/oa_functions.csv` pending rows by class
prefix `CKG`/`CSK`, sort by count -- but ALSO grep the existing headers
for "own class layout out of scope" opaque stand-ins matching those class
names first, since (as this batch and the prior CSKMIDIInMsgHandler batch
both found) a class that's already a partial opaque stand-in used by
real, working code is lower-risk to promote than a cold-start class with
no existing integration.
