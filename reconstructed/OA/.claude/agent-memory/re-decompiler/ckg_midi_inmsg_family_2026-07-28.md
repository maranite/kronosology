---
name: ckg-midi-inmsg-family-2026-07-28
description: CSKMIDIInMsgHandler and its 5 children (Port/PadByPort/LocalCtrl/Karma/PadByLocal) reconstructed, 72/86 manifest-credited; real call_off table derivation technique; 2 lower-confidence methods flagged
metadata:
  type: project
---

## What this is

Continuation of [[ckg_control_ui_msg_family]]'s `CSKMIDIMsgHandler`-rooted
MIDI-in dispatch family. That batch deliberately deferred
`CSKMIDIInMsgHandler` itself (33 methods, real dying-note-tracking array
arithmetic, a 922-byte ctor) as "the project's own next continuation
target" -- this batch closed it, plus all 5 real children discovered via
a fresh `objdump -r` sweep: `CSKMIDIPortMsgHandler`, `CSKPadNoteByMIDIPortMsgHandler`,
`CSKMIDILocalCtrlMsgHandler`, `CSKMIDIKarmaCtrlMsgHandler`,
`CSKPadNoteByLocalCtrlMsgHandler`.

Files: `include/oa_ckg_midi_msg_handler.h`, `src/engine/ckg_midi_msg_handler.cpp`,
`verify/test_ckg_midi_msg_handler.cpp`. Also touched (minimal, required):
`include/oa_ckg_control_ui_msg.h` (opaque `CSKMIDIInMsgHandler` stand-in ->
forward decl, `CMIDIFlowParamHolder`/`CKGRTCHandler` method additions),
`include/oa_ckg_module_param_msg_handler.h` (`CKGEngine::SendChannelMessage`/
`ShouldForceTimbreZoneBypass`/`ms_poKGEventDisplayManager`),
`src/engine/ckg_control_msg_handler.cpp` (removed a now-duplicate static
definition + its own KAT test needed a new mock), `src/engine/
ckg_module_param_handler.cpp` (1-line static definition).
OA.ko manifest 3040 -> 3112/21,689 (+72, 0 regressions, verified via full
`git stash -u` baseline name-set diff).

## Real class graph (confirmed via objdump -r, corrected the prior summary)

```
CSKMIDIMsgHandler (15 slots, no dtor)
  -> CSKMIDIInMsgHandler (50 slots: 15 inherited + 35 new; 8 stay PURE
     here -- genuinely abstract)
       -> CSKMIDIPortMsgHandler (50 slots, all overrides, 0 new)
            -> CSKPadNoteByMIDIPortMsgHandler (50 slots, overrides only
               dtor + ShouldNotifyToKarmaController + CheckNoteMessageAndTriggerPad)
       -> CSKMIDILocalCtrlMsgHandler (64 slots: 50 inherited + 14 new)
            -> CSKMIDIKarmaCtrlMsgHandler (64 slots, same 3-slot override
               shape as CSKPadNoteByMIDIPortMsgHandler)
            -> CSKPadNoteByLocalCtrlMsgHandler (64 slots, same 3-slot
               override shape)
```

No virtual inheritance anywhere (no VTT/_ZTC symbols) -- plain single
inheritance throughout, unlike the CKGSwitch widget family.

## The call_off table technique (do this FIRST for any deep vtable-dispatch class)

Every `(**(code**)(*this + N))()` in the raw disassembly resolves via
`rodata_offset = N + 8` (established convention, re-confirmed). The
mistake made TWICE in this batch before the fix: recomputing this
mapping ad hoc per-instruction while reading disassembly, which is
extremely error-prone under fatigue (misread `call_off 0x6c` as
targeting `NotifyCCToKarmaController` when it's actually
`ShouldSendChannelMessageToKarmaEngine`; misread `call_off 0x7c` as
`CheckGlobalParameterPreSendToKarmaEngine` when it's actually
`IsEnableViaRPPR`). **Fix that worked**: transcribe the ENTIRE class's
own vtable relocation dump ONE time into a `call_off -> rodata_off ->
name` table (rodata offsets read directly off the `objdump -r` output,
call_off = rodata-8, done once as a straight list, not per-instruction
arithmetic), then look up every call against that table for the rest of
the class. Re-derive the table fresh for each class (base-class slots
keep their own offsets; a derived class's OWN new slots start right
after the last inherited one).

## Real field layout (CSKMIDIInMsgHandler, shared struct base for all children)

Confirmed via the 922-byte ctor's own zero-init sequence, cross-checked
against every reader:
```
+0xc    unsigned char m_noteDownCount[128]
+0x8c   int           m_noteOnCount
+0x90   CSKSysExMsgHandler *m_sysExHandler   -- CSTGBankMemory::AllocAligned(0x3c,0x10) + placement-new, own object
+0x94   bool m_bDamperOn
+0x95   bool m_bSostenutoOn
+0x96   bool m_softPedal
+0x98   unsigned int m_lastNotePerChannel[16]     -- init 0xff
+0xd8   CDyingNoteInfo m_dyingNoteMIDIPort[16]    -- own opaque 0x84-byte type
+0x918  CDyingNoteInfo m_dyingNoteSTG[16]
+0x115c unsigned int m_bypassKarmaNoteOnEvent[128]
+0x135c int           m_dyingDamperTicks[16]
+0x139c unsigned char m_dyingDamperFlag[16]
+0x13ac unsigned char m_noteOnHoldCount[128]
+0x142c unsigned char m_extNoteOnChecker[128]     -- CSKMIDILocalCtrlMsgHandler only
+0x14ac int m_perNoteTimbreTranspose[128][16]     -- CSKMIDILocalCtrlMsgHandler only
```
`CDyingNoteInfo` size (0x84=132) confirmed independently 2 ways: the
16-channel array stride in the ctor AND `KillAllDyingNotes()`'s own
`rep movs` (`ecx=0x21` dwords = 0x84 bytes).

**Real, confirmed dual-use gotcha**: `m_extNoteOnChecker[128]` is used
BOTH note-indexed (0-127, by `RegistExtNoteOn`/`UnRegistExtNoteOn`/
`IsSendingNoteOnToExt`) AND channel-indexed (0-15, by
`CheckDuplicateMessage()`'s ChannelAftertouch dup-suppression) -- same
storage, two different index spaces, confirmed by tracing both
consumers' own disassembly independently, not assumed from either alone.

## Real transcription bugs caught by insisting on a second read

1. **`CSKMIDIInMsgHandler::Process()`'s `edi`/`consumed` local is NOT a
   simple "handled" bool.** For a real Note-Off whose per-note hold
   counter (`m_noteOnHoldCount`) was already nonzero, `edi` becomes the
   `CKGBankManager::ms_poInstance[0x97c749]` gate-flag VALUE itself (0
   or 1), preserved into the shared tail (a jump target that SKIPS the
   `xor edi,edi` the other 2 paths hit) -- gating a later
   `SKSTGGate_EndMonitorSTGQueue()` call. Missing the 2-target-address
   distinction (`343ee0` resets, `343ee2` two bytes later doesn't) would
   have silently dropped this whole STG-monitor mechanism.
2. **`ShouldSendChannelMessageToMIDIPort()`'s (LocalCtrl) `mov
   edi,[eax+0xd8]` is a vtable READ, not a random field.** `eax` at that
   point is the object's own vptr (rodata_base+8), so `[eax+0xd8]` =
   rodata_offset 0xe0 = `IsNotThruKarma`'s own slot -- i.e. the whole
   sequence is just `this->IsNotThruKarma(GetLocalControlChannel())`
   compiled through an explicit function-pointer load instead of the
   usual `call [edx+N]` shape. Same +8 rule, just easy to miss when the
   read isn't inside an obvious `call [reg+N]` instruction.
3. **`AnalizeAndSetParameter`'s field-store order differs between Port
   and LocalCtrl in a way that matters**: Port masks the incoming flags
   byte to its high nibble only (`m_flags = buf[3] & 0xf0`) and accepts
   a WIDER status-type set (0x80/0x90/0xa0/0xb0/0xe0 as 2-data-byte,
   0xc0/0xd0 as 1-data-byte); LocalCtrl stores `m_flags` verbatim and
   instead rejects on `(m_flags & 0xf) == 5`. Confirmed independently
   per class, not assumed shared.

## 2 lower-confidence methods (flagged in source, not silently presented as solid)

`CSKMIDILocalCtrlMsgHandler::SendChannelMessageInCombiOtherTimbreToMIDIPort()`
(0-arg, 444 bytes) and its 2-arg overload (371 bytes) are real,
implemented, and build/link/pass their own KAT coverage -- but their
per-branch register mapping was NOT independently re-verified with a
second disassembly pass the way everything else in this batch was
(time-boxed). What IS solid: the real
`m_perNoteTimbreTranspose[note][timbre]` addressing math (`this + 4*note*16
+ 4*timbre + 0x14ac`, confirmed independently via the ctor's own 128-row
zero-init loop) and the overall shape (apply+remember a
`GetTimbreTranspose()` value on Note On, recall -- not recompute -- the
SAME value on the matching Note Off, so a held note survives a live
transpose-setting change). These were kept IN the class, not stubbed to
a no-op, because both are real vtable slots (rodata 0xe8/0xec) shared by
`CSKMIDIKarmaCtrlMsgHandler` and `CSKPadNoteByLocalCtrlMsgHandler` --
leaving either pure would make all 3 classes non-instantiable. If this
pair ever needs revisiting, start from the ctor's zero-init loop (128
rows x 16 cols, confirms the addressing) and re-trace both overloads
fresh rather than trusting this batch's own control-flow notes at face
value.

## Manifest-generator gotcha (same class as before, WORSE here, root cause finally isolated)

Same `DEF_RE` greedy-span bug documented in
[[ckg_module_param_msg_handler_family]] and
[[ckg_control_ui_msg_family]] ("word(" adjacency swallowing a real
definition) -- but this batch found the SPECIFIC, extremely common
trigger that cost the most credit: **the project's own
`.text+0xNNN, N bytes, regparm(3).` comment convention is itself a
self-inflicted trap.** `regparm(3)` is a literal `identifier(params)`
shape; when it's the LAST thing in the comment before the real
function's own signature (nothing else with a `;`/`{`/`}` in between),
`DEF_RE`'s greedy `[^;{}]*` param-capture swallows straight through the
comment closer and the real function's own name+params, credited
instead to the bogus name `"regparm"`. Cost 4 real methods in this
batch alone (`AnalizeAndProcessNoteOffWhilePerformanceChange`,
`AnalizeAndProcess`, both `AnalizeAndSetParameter` overloads) before
being caught by a scripted `DEF_RE.finditer()` span-length sweep (>150
chars) run proactively this time, BEFORE the manifest diff, per the
prior batch's own recommendation. Fix applied locally: reworded
`regparm(3).` to `uses regparm 3 calling convention.` in the 4 affected
comments (not a project-wide regex change -- same blast-radius caution
as prior batches). **Any future batch should run the span-length sweep
on its own new files BEFORE trusting the manifest diff**, not after --
this alone found and let-fix 9 of the ~14 total instances hit this
batch, all caught and fixed before committing (final delta: +72
credited, 0 regressions, only the compiler-auto-generated dtors and 2
genuinely-inline 1-byte stubs -- `StoreDyingNoteInfoForSTG`/
`StoreDyingNoteInfoForMIDPort`, deliberately left inline since they're
truly trivial no-ops -- remain uncredited, an accepted, understood gap
matching the widget-hierarchy batch's own precedent).

## Real 3-way TU coordination needed (shared-repo hygiene note)

Moving a static's DEFINITION from one class's own `.cpp` (where it had
lived as a 1-field opaque-stand-in leftover) to its real owning class's
new `.cpp` breaks EVERY other already-linked TU that was relying on the
old location -- caught 2 separate instances here: the real
`ckg_control_msg_handler.cpp` (fixed by deleting its now-duplicate
definition) AND that class's OWN KAT test file,
`verify/test_ckg_control_msg_handler.cpp` (which links
`ckg_control_msg_handler.cpp` but NOT `ckg_midi_msg_handler.cpp`, so
needed its own new mock definition). `make verify`'s full 94-binary
sweep is what caught the second one -- `make objs` and the standalone
target's own KAT did NOT, since neither links that specific other
test binary. Always run the FULL `make verify` (not just the new
target) after moving any static's definition across files.
