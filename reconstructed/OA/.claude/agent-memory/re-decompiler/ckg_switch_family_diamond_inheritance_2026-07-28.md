---
name: ckg-switch-family-diamond-inheritance-2026-07-28
description: CKGController/CKGSwitch/CKGKnob/CKGPad diamond-multiple-inheritance widget hierarchy reconstructed (107 methods, ~19 classes, commit a599857) -- vtable-relocation cross-referencing technique, 2 real corrections to the standing lead, KAT bugs found and fixed
metadata:
  type: project
---

## What this is

The genuine C++ diamond-multiple-inheritance widget hierarchy scoped (but
not attempted) by the prior `CKGControlMsgHandler`/`CKGUIMsgSender` batch
(see [[ckg_control_ui_msg_family]]). KARMA front-panel real-time
controllers: assignable switches/knobs/pads, tap-tempo, FF/REW, scene
select, drum-track on/off, chord assign/trigger, module control.

Files: `include/oa_ckg_switch_family.h`, `src/engine/ckg_switch_family.cpp`,
`verify/test_ckg_switch_family.cpp`. Also extends `CKGEngine`/
`CKGParamEdit`/`CKGRTCHandler`/`CKGUIMsgProcessor`/`CSKMIDIMsgProcessor`/
`CKarmaGlobal` in the already-existing headers those live in. OA.ko
manifest 2883 -> 2990/21,689. Commit `a599857`.

## Class graph (confirmed via C1/C2 ctors calling `CKGController::CKGController()`
directly, bypassing immediate parents -- Itanium "only most-derived
constructs virtual bases" tell)

```
CKGController (abstract root)
  |        |         |
CKGSwitch CKGKnob  CKGPad
  |  |  |
  |  |  CKGCountUpSwitch -- CKGModuleControlSw
  |  CKGTapSwitch -- CKGFFSw / CKGREWSw / CKGTapTempoSw
  CKGToggleSwitch -- CDrumTrackOnOffSw / CKGChordAssignSw /
                      CKGKarmaAssignableSw / CKGKarmaOnOffSw /
                      CKGLatchSw / CKGPadModSw
  CKGSceneSw (direct CKGSwitch leaf, bypasses all 3 switch tiers)
CKGKnob -- CKGKarmaAssignableKnob / CKGTempoKnob
CKGPad -- CKGChordTrigger
```

## The technique: scripted vtable-relocation cross-referencing, not offset arithmetic

`nm -C "vtable for X"`/`"VTT for X"` -> real section name (e.g.
`.rodata._ZTV13CKGController`) -> `readelf -rW` + `c++filt` dumps every
R_386_32 relocation in that section = the REAL target symbol (or
`__cxa_pure_virtual`) at each byte offset. A throwaway Python script
cross-referenced every derived class's own vtable dump against
CKGController's 16-slot base table by matching the 11 real (non-pure)
symbol names shared by every subclass, found each class's constant
slot-index shift, then read off what occupies the base's 5 remaining
pure slots in EVERY leaf across all 3 branches. **Do NOT try to hand-solve
the raw `call [reg+N]` offset arithmetic instead** -- multiple attempts at
that (before switching to the relocation-dump method) produced
self-contradictory results because the SAME numeric asm offset means
different things depending on which vtable-group (primary-short-block vs
secondary-mirrored-base-region) is being dispatched through, and Itanium's
leading vcall-offset/offset-to-top/RTTI word count differs per class
depth. The relocation-dump + name-matching method sidesteps all of that
by reading confirmed ground truth directly instead of re-deriving layout.

Once the class graph + pure-virtual introduction points were confirmed
this way, the actual C++ was written with ordinary `virtual public`
inheritance (letting g++ regenerate VTT/thunk boilerplate) -- same
precedent as `include/kontakt_parameter_base.h` and Eva's
[[eva-stream-family-diamond-inheritance-2026-07-28]]. Exact vtable BYTE
layout was never reproduced, only which class declares/overrides which
named virtual.

## 2 real corrections to the PRIOR batch's own standing lead

The task instructions explicitly said "verify before trusting a prior
batch's evidence" -- this paid off:

1. **`GetCCValue`'s pure-virtual slot was wrong.** The earlier note
   guessed "N=0xc" for it. Real relocations place it at CKGController's
   own vtable byte offset 0x18 (confirmed independently via CKGKnob's AND
   CKGPad's own thunk targets landing on the identical relative slot).
   The "N=0xc" slot is actually `AnalizeAndProcessCCMessage`.
2. **2 CKGController-level pure virtuals weren't identified at all**:
   `GetCCNumber()` (base offset 0x14) and `Process()` (base offset 0x38).
   Confirmed the same way -- every one of the ~14 concrete leaves across
   all 3 branches overrides exactly these two names at the matching
   relative slot.

Also confirmed (not previously stated either way): CKGController has NO
virtual destructor at all -- no `~CKGController` symbol anywhere in the
binary, and the vtable's 16 slots are fully accounted for by 11 real + 5
pure methods with none left over for a dtor pair. Nothing in this whole
tree is ever `delete`d through a base pointer.

## Real field layout (all confirmed from ctor/method disassembly)

- `CKGController::m_value` (int, +0x4 in the CKGController subobject) --
  a generic "last thing processed" slot. Ground-truth ctor does NOT zero
  it (single `mov [this],vtbl+8; ret`) -- every real
  AnalizeAndProcessXxx() override writes it before any use. Zero-init'd
  in this reconstruction's own ctor for KAT determinism, a documented
  behavior-preserving deviation.
- `CKGSwitch::m_bOn` (byte, own +0x4, own-object offset, NOT inside the
  vbase subobject -- confirmed distinct storage by tracing which `this`
  each write targets).
- `CKGPad::m_bOn`/`m_lastPositive` (bytes, +0x4/+0x5) + `m_lastValue`
  (int, +0x8).
- Leaf-specific: `CKGChordTrigger::m_index` (+0xc), `CKGKarmaAssignableSw`/
  `CKGKarmaAssignableKnob::m_id` (+0x8), `CKGModuleControlSw::m_enabled`
  (+0x4), `CKGSceneSw::m_scene` (+0x8).
- `CKGKnob` has ZERO fields of its own -- `GetCCValue()`/`GetResetValue()`
  both just return the inherited `CKGController::m_value` directly.

## `CKGBankManager` double-dereference gotcha (real, confirmed, NOT a
   modeling inconsistency)

Every real access to the shared module/status byte
(`ms_poInstance[2]`-equivalent) dereferences `CKGBankManager::ms_poInstance`
TWICE (`mov eax,ms_poInstance; mov eax,[eax]; movzx eax,BYTE[eax+2]`) --
confirmed identically in `CKGController::GetDestinationModule()` and every
`GetCurrentValue()` bit-flag reader. The large `0x97c7xx`-range per-switch
config tables use a SINGLE dereference in the SAME functions. `ms_poInstance`
itself is the large table object; its own `+0` field is a SEPARATE pointer
to the small live "current status" sub-object. Modeled via a helper
`OA_CKGBankMgrState()`. **A test-mock bug from getting this wrong**: an
early version of the KAT harness stored a self-referencing pointer INSIDE
the same buffer whose byte offset+2 the test also wrote to as the "state
byte" -- writing the state byte corrupted byte 2 of the 8-byte host
pointer sitting at offset 0-7 of the SAME buffer, causing a segfault only
on the SECOND test case (first case's pointer write hadn't been corrupted
yet). Fixed by using two genuinely separate buffers, matching what real
ground truth actually does (a separate small sub-object, not a
self-reference). If a KAT test involving a double-deref singleton
segfaults on a LATER test case but not the first, suspect exactly this.

## `ProcessRTControllersValue` overload-selection gotcha

`CKGUIMsgProcessor::ProcessRTControllersValue` has 2 real overloads
(mangled `Eiiib` = 4 params, `Eiiiib` = 5 params). Several leaf `Process()`
bodies use the 4-arg form with a middle `0` argument that's easy to drop
by mistake when transcribing from a disassembly trace that reads `edx=4
(msgId); ecx=0 (unused)` as "just pass msgId" -- the `ecx=0` IS a real
argument slot, not padding. Always count regparm(3) args explicand
against the REAL mangled signature's arity, not against what "looks
right" semantically.

## `CKGTempoKnob` MSB/LSB self-comparison bug (real bug this batch caught via KAT)

Initial transcription of `AnalizeAndProcessKarmaControllerMessage`'s "send
if the combined value differs from X" comparison used `GetCCValue()` as
X -- but `GetCCValue()` for a `CKGKnob` just returns `m_value`, which the
SAME function had already overwritten with the combined value one line
above, making the comparison always-false (a silent no-op that would
never have fired a UI update). The KAT test failure (`got=0 want=13000`)
caught this immediately. Re-checked the real vtable-slot cross-reference
for that specific call site: it's actually `GetCurrentValue()` (CKGBank
Manager's own LIVE tempo word), read BEFORE the store, not `GetCCValue()`.
**Lesson: when a "combine then compare against getter" idiom produces a
self-comparison, that's a strong signal the getter identification is
wrong, not that the real code has a no-op quirk** -- re-verify the vtable
slot rather than accepting the self-comparison as ground truth.

## Best-effort (not byte-exact) methods, flagged as such in the header/source

- `CKGModuleControlSw::AnalizeAndProcessKarmaControllerMessage` -- a
  genuinely branchy module-scope-change handler (2 RTC flag bytes, a
  bounded re-derivation walk). Reproduced as a behaviorally-faithful
  shape, not a byte-exact CFG transcription.
- `CKGChordTrigger::SendNoteOrCC`/`SendNoteOrCCInExternalMode`/
  `SetStatusAndPadsAssign` -- real, deep KARMA chord-note MIDI dispatch.
  The exact 0x7f/0x80 MIDI-status-byte boundary classification and byte-
  packing order are transcribed as read but not independently
  cross-checked against a live MIDI capture.

## Verification

`make verify`: all green, including new `verify/test_ckg_switch_family.cpp`
-- 2 kinds of check: (1) structural diamond-dispatch through
`CKGController*` for a representative leaf per branch (proves the virtual-
inheritance ABI is correct, not just that individual getters return the
right constant), (2) field-offset oracle checks where the expected value
is derived directly from the ground-truth byte offset documented in the
header (independent of the C++ implementation under test), plus an
independently hand-computed MSB/LSB combine formula for `CKGTempoKnob`.
Real `make ko KDIR=/home/build/linux-kronos` Kbuild build: clean link,
only expected external ("Unknown symbol" at insmod time) undefined refs,
all with correctly-mangled real symbol names (confirmed via `nm -C OA.o`,
including the 2 `asm()`-linkage-name-override symbols matching byte-for-
byte).

## Cross-header circular-dependency technique: `asm()` linkage-name override

`CKGParamEdit::SendChordMemory`'s real 3rd parameter type is
`CKGController::EChangeSource`, and `CSKMIDIMsgProcessor::
ProcessKarmaControllerGeneratedChannelMessage`'s real 1st parameter type
is `CMIDIMessage::EStatus` -- both enums declared in
`oa_ckg_switch_family.h`, a header that (transitively) INCLUDES the
headers `CKGParamEdit`/`CSKMIDIMsgProcessor` themselves live in. Rather
than restructure the include graph, both declarations use plain-int
surface types with an explicit `asm("_ZN...")` linkage-name override
matching the real ground-truth mangled symbol exactly (read off the
binary's own relocation, not guessed) -- GCC links against the real
symbol regardless of the locally-visible parameter types. Reusable
pattern for any future cross-header enum-in-signature circular
dependency in this project.
