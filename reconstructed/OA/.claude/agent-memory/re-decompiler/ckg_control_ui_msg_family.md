---
name: ckg-control-ui-msg-family
description: CKGControlMsgHandler + CKGUIMsgSender reconstructed (53 methods, commit pending 2026-07-28) — the next dense cluster after STG value-getter + CKG*ParamMsgHandler closure; includes the CKGSwitch/Knob/Pad widget-hierarchy investigation and 2 rejected candidates, plus 3 new manifest-generator gotchas
metadata:
  type: project
---

## What this is

After the STG value-getter family (~2300 methods) and all three
`CKG*ParamMsgHandler` classes (230 methods) were both confirmed fully closed
(see [[stg-value-getter-family]] and [[ckg-module-param-msg-handler-family]]),
a fresh broad survey (group all pending manifest rows by class, sort by
count) found a large, previously-untouched KARMA UI/MIDI cluster: ~64 classes,
~845 pending methods, spanning `CKG*` and `CSK*` prefixes. This batch closed
the first slice of it: `CKGControlMsgHandler` (24/26 methods, 2 deferred) +
`CKGUIMsgSender` (29/29 methods, fully done).

Files: `include/oa_ckg_control_ui_msg.h`, `src/engine/ckg_control_msg_handler.cpp`,
`src/engine/ckg_ui_msg_sender.cpp`, `verify/test_ckg_control_msg_handler.cpp`,
`verify/test_ckg_ui_msg_sender.cpp`. OA.ko manifest 2830 -> 2883/21,689.

## The convention (3rd distinct one in this codebase, after STG value-getters and CKG checked-writes)

`CKGControlMsgHandler::Xxx(const CKGControlMsg*)` — plain UI-action
dispatch, forwards into `CKGEngine`/`CKGBankManager`/`CKGRTCHandler`/
`CKGParamEdit`. No CSPREngine gate, no KARMA-perf record lookup, no
checked-write skeleton at all — genuinely different from both prior
families.

`CKGUIMsgSender::Xxx(...)` — builds a fixed-shape `CSKMessage` on the
stack, hands it to `KGOutGate_SendMessageToUI(msg, immediate)`. Two shapes:
- **(a) ParameterChangeMessage shape** (11 methods: Send/Update/SetMax/
  SetMin/Refresh/Dim x {Common,Module}): `+0x00` u16 class (0x24 Common /
  0x28 Module), `+0x02` u16 subtype=5, `+0x04` i32 constant (2 Common / 3
  Module), `+0x08` i32 msgId, `+0x0c` void* = `CKGEngine::ms_poInstance[+0xa0]`
  (a pointer read, not dereferenced further), `+0x10`=0, `+0x14`=0xffff,
  then value/operation/trailing-arg at offsets that **differ between
  sub-families** — see gotcha below.
- **(b) "UI action" shape** (18 methods): `+0x00`=0x14, `+0x02`=5, `+0x04`=0,
  `+0x08` sub-opcode literal, then `+0xc`/`+0x10` hold EITHER the
  `CKGEngine` engine-field pointer + the method's single param, OR the
  method's own 1-2 params directly with no engine pointer at all — confirmed
  individually per method, not assumed from naming. 2 methods
  (`UpdateKarmaInitialInfo`, `UpdateRTCModelName`) use a further variant
  with no engine pointer AND no real param (both `+0xc`/`+0x10` literal 0).
  Always sent unconditionally with `immediate=false`.

Send-gate for shape (a): `operation==5` (Dim) → always
`KGOutGate_SendMessageToUI(msg, true)` unconditionally, bypassing every
guard. Any other operation → sent with `immediate=false` ONLY if
`CSKSpecialMsgHandler::m_NowHandlingSamplingPerformanceChange` is clear AND
none of `CKGControlMsgHandler::ms_bIsNowDumping{Song,Combi,Prog}` are set;
otherwise silently dropped (no send at all).

## Real gotcha: two visually-identical field layouts that aren't

`SendModuleParamMessage` (5 real params) and the
`{SetModuleParamMax,SetModuleParamMin,UpdateModuleParam,DimOnModuleParam}`
quartet (4 real params each) BOTH use the Module shape's `class=0x28`
header, but place their "value" field at **different absolute offsets**:
`+0x24` for `SendModuleParamMessage` (leaving `+0x1c` an unwritten gap),
`+0x1c` for the other four (with `+0x24` instead holding the 4th/trailing
real param — `arg4` for Max/Min/Update, `dim` for DimOnModuleParam). This
was NOT discoverable by transcribing one method and assuming the rest match
— each of the 5 methods needed its own full disassembly. Don't reuse a
"looks the same" field-offset helper across sibling methods without
verifying each one's own instruction trace.

## Real vtable-pointer-in-an-int-field idiom (why 3 methods were deferred)

`CKGControlMsgHandler::SharedMemProgramDump/CombiDump/SongDump` call
`CKGProgramDownloader::HandleProgramDownload`/`CKGCombiDownloader::
HandleCombiDownload` with `this` reinterpreted directly from
`CKGControlMsg::m_mode` (an int field!) rather than through any
`ms_poInstance`-style singleton — the caller smuggles a real pointer value
through the message's own field. Confirmed via disassembly (`mov eax,[eax]`
reading `msg->m_mode` directly into the register used as the call's `this`).
Combi/Song variants also pass a 3rd `eSTGMsgPerfType` argument whose
register/stack slot could not be resolved with confidence (regparm(3) only
has 3 GP registers total, already consumed by `this`+2 explicit args, no
stack spill visible in either observed call site). Deferred rather than
guessed — logged in `HARDWARE_REVIEW_LOG.md`, not `DECOMPILE_ERRORS.md`
(that file is for compile/link failures on an attempted reconstruction, not
scope deferrals).

`HandleMessage(CSKMessage*)` itself (1181 bytes, ~40-case jump table over
`msg[+0x8]`) is a clean, self-contained, fully-disassembled standalone
deferral — its case bodies mostly INLINE logic duplicating (not calling)
this class's own separately-addressed methods, so it has zero dependents
among what's already done.

## Rejected candidates from this batch's survey (don't re-pick without new evidence)

- **`CKGParamEdit`** (133 pending) — the checked-write family's own
  `SendXxx()` call target, not itself decoder-friendly (already flagged in
  an earlier CKG*ParamMsgHandler batch).
- **The whole `CSPR`/`CRPPR`-prefixed sequencer-record family**
  (`CSPRRecorder`, `CRPPRManager`, `CSPRControlMsgHandler`, `CSPRAudioPlayer`,
  etc.) — sample disassembly at several of their own manifest-CSV addresses
  landed MID-FUNCTION inside `CSingleRPPR::settrkno`/`CSPRRecorder::
  RenewSongPlayParamAfterRec`. The Ghidra static export's function-boundary
  detection is unreliable across this whole family — verify with a fresh
  `objdump`/`nm` cross-check before ever attempting it, don't trust the CSV
  addresses at face value for this specific prefix group.

## Next target: CKGSwitch/CKGKnob/CKGPad widget hierarchy (investigated, NOT attempted)

A real, genuine diamond-multiple-inheritance C++ class tree — same
technique precedent as Eva's `CStream` family (see Eva's
`eva_stream_family_diamond_inheritance_2026-07-28` memory: "write real C++
virtual bases, let g++ regen VTT/thunk boilerplate", don't hand-solve
Itanium ABI vcall-offset machinery).

Root: `CKGController` (abstract, 12 real methods, several pure virtual —
`GetCCValue` at vtable slot N=0xc, another unnamed pure-virtual getter at
N=0x10, `AnalizeAndProcessNoteMessage` real at N=0x8, `GetChannel`/
`ResetMIDI`/`Reset`/`StartBuffering`/`FlashBufferdValue`/`SendCC`/`Change`/
`GetDestinationModule`/`IsEnableResetSwitch`/`ShouldProcess` all real).
Field: `int m_value` at controller-subobject+4 (a generic "last processed
value" DWORD, reset to 0 at the start of every `AnalizeAndProcessXxx`
override across the whole tree).

Three `virtual public CKGController` intermediate classes: `CKGSwitch` (2
methods, own field: 1-byte `m_bOn`-style toggle at CKGSwitch-object+4,
separate memory from CKGController's own +4 field — verified by tracing
which `this` pointer each write targets), `CKGKnob` (7 methods),
`CKGPad` (16 methods).

`CKGSwitch` has 3 more `virtual public` intermediate levels:
`CKGToggleSwitch` (8), `CKGTapSwitch` (6), `CKGCountUpSwitch` (6) — each
overrides `GetCCValue`/`AnalizeAndProcessCCMessage`/
`AnalizeAndProcessKarmaControllerMessage`.

Concrete leaves (all confirmed via real vtable relocation dumps +
construction-vtable inheritance evidence, not guessed):
`CDrumTrackOnOffSw`/`CKGChordAssignSw`/`CKGKarmaAssignableSw`/
`CKGKarmaOnOffSw`/`CKGLatchSw`/`CKGPadModSw` : `CKGToggleSwitch`;
`CKGFFSw`/`CKGREWSw`/`CKGTapTempoSw` : `CKGTapSwitch`;
`CKGModuleControlSw` : `CKGCountUpSwitch`; `CKGSceneSw` : `CKGSwitch`
directly (NOT via CKGCountUpSwitch, despite similar method shape — verified
via its own construction-vtable list only naming `CKGSwitch`, not
`CKGCountUpSwitch`); `CKGKarmaAssignableKnob`/`CKGTempoKnob` : `CKGKnob`;
`CKGChordTrigger` : `CKGPad`.

~194 methods total across ~19 classes; ~114 are real bodies (rest are
compiler-generated `virtual thunk to X` + duplicate C1/C2 ctor pairs that
should auto-materialize once the real bodies are written with correct
inheritance — same leverage ratio as Eva's CStream family). NOT attempted
this batch: each method needs individual semantic tracing (real branchy
vtable-dispatch logic, e.g. `CKGController::ShouldProcess()` reads a global
`CKGBankManager` flag byte AND 2 other classes' static `sm_bNowReset`
flags), not a mechanical field-offset transcription — genuinely deeper than
either STG value-getters or CKG checked-writes, closer in effort-per-method
to `CSTGControllerInfo` (which took 10+ batches).

## Manifest-generator gotchas (3 new ones, all fixed before committing)

See `gen_oa_manifest.py`'s `ADDR_RE`/`DEF_RE`. All 3 confirmed via a full
`git stash -u` baseline diff before AND after each fix (never trust the
on-disk manifest CSV — untracked, can be stale).

1. **`ADDR_RE` matches `.text+0xXXXXXX` literals inside prose, including
   comments describing a DEFERRED (unimplemented) function's address for
   documentation purposes.** Writing `HandleMessage`'s real offset in a
   header comment (to help a future session find it) false-credited it as
   done, purely from the mention — no `DEF_RE` match needed at all. Fix:
   describe deferred functions' locations without a literal `.text+0x`
   citation (e.g. "see re-decompiler agent memory for the real offset").
2. **Same mechanism, different trigger**: a class-region-END address
   comment (`` `.text+0x3c84e0`..`.text+0x3c90e0`. Stateless... `` for
   `CKGUIMsgSender`) coincidentally matched a DIFFERENT class's (unrelated,
   untouched) ctor address, false-crediting `CKGModuleParamMsgHandler::
   CKGModuleParamMsgHandler`. Fix: cite an address you actually implemented
   (e.g. the class's own last real method) instead of "one past the end."
3. **NEW: a trailing same-line comment between `)` and `{` breaks `DEF_RE`.**
   `void Foo(...)	/* .text+0x... */\n{` does NOT match, because `DEF_RE`'s
   tail is `\)\s*(?:const\s*)?(?::\s*[^;{}]*)?\{` — a `/*...*/` comment is
   not whitespace to the regex, so the match fails entirely and the method
   is silently NOT credited even though its body is 100% correct and
   present. Cost 14 real, correctly-implemented `CKGUIMsgSender` methods a
   false "pending" classification until caught by the exact-name-set diff
   showing fewer additions than expected. Fix: always put address/derivation
   comments on their OWN line immediately ABOVE the signature, never
   trailing after the closing paren — the convention already used
   everywhere else in this project, which is why only one late-written
   block (family (b) of `ckg_ui_msg_sender.cpp`) was affected.

## Continuation

Remaining in the broader ~845-method CKG/CSK family, roughly by size:
`CKGEngine` (74, own methods not yet reconstructed — currently only an
opaque `ms_poInstance`/`ms_poKGParamEdit` stand-in with a handful of methods
added on-demand by callers), `CKGBankManager`'s own remaining surface,
`CKGRTCHandler` (27 pending beyond the 2 methods declared here),
`CSKMIDIInMsgHandler` (33), `CSKMIDILocalCtrlMsgHandler` (28),
`CSKSysExMsgHandler` (28), `CKGUIMsgProcessor` (17, more complex parsing
logic — `AnalizeAndProcessParamterChangeMessage` alone is 567 bytes),
`CSKMIDIMsgProcessor` (22), `CKGTimerManager` (15), `CKGEventDisplayManager`
(15), and many more small `CKG*Sw`/`CKG*Knob` classes covered by the widget
hierarchy above. Full sweep recipe: group `manifest/oa_functions.csv`
pending rows by class prefix `CKG`/`CSK`, sort by count.
