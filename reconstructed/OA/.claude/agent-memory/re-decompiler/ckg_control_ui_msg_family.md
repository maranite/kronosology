---
name: ckg-control-ui-msg-family
description: CKGControlMsgHandler+CKGUIMsgSender (53), CKGController/Switch/Knob/Pad widget hierarchy (107), and CSKMIDIMsgHandler/CSKSpecialMsgHandler/CSKSysExMsgHandler MIDI dispatch family (50, commit df18ee5) all reconstructed here — manifest 2830->3040/21689; CSKMIDIInMsgHandler is the next continuation target; 4 manifest-generator gotchas found+fixed total, most recent is a DEF_RE "word(" greedy-span bug
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

## UPDATE 2026-07-28 (later batch, commit `a599857`): this hierarchy is now ATTEMPTED

See the new dedicated memory file
`ckg_switch_family_diamond_inheritance_2026-07-28.md` for the full
writeup, including 2 real errors THIS section's own evidence had that
were caught and corrected before writing any C++ (`GetCCValue`'s real
slot, and 2 previously-unidentified CKGController-level pure virtuals).
`include/oa_ckg_switch_family.h` + `src/engine/ckg_switch_family.cpp`.
107 methods, manifest 2883 -> 2990/21,689.

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

## UPDATE 2026-07-28 (later batch, commit `df18ee5`): CSKMIDIMsgHandler family reconstructed

Fresh `nm -C`/manifest sweep of the whole cluster (all pending rows grouped
by class prefix, sorted by count) found a large, previously-untouched
`CSKMIDIMsgHandler`-rooted MIDI-in dispatch family (~176 methods across 13
classes: `CSKMIDIInMsgHandler` 33, `CSKMIDILocalCtrlMsgHandler` 28,
`CSKSysExMsgHandler` 28, `CSKMIDIMsgProcessor` 22, `CSKMIDIMsgHandler` 16,
`CSKMIDIPortMsgHandler` 12, `CSKParameterChangeMessage` 10,
`CSKSpecialMsgHandler` 6, `CSKMIDIKarmaCtrlMsgHandler` 5,
`CSKUIMsgProcessor`/`CSKVoiceControlMessage`/
`CSKPadNoteByLocalCtrlMsgHandler`/`CSKPadNoteByMIDIPortMsgHandler` 4 each).
Real inheritance/vtable graph verified via `objdump -r` against
`.rodata._ZTV*` sections in `/home/share/Decomp/OA.ko_Decomp/OA.ko` (an
unstripped unlinked ground-truth object at a confirmed constant `-0x10000`
offset from the Ghidra static export's own addressing) — NOT trusted from
the Ghidra decompile's naming, which turned out wrong in several places
(see below). Real shape: `CSKMIDIMsgHandler` is a plain (no virtual base,
no VTT) abstract root with 15 real virtuals and no destructor of its own;
`CSKMIDIInMsgHandler` and `CSKSysExMsgHandler` are its two direct children
(siblings, confirmed by each owning a SEPARATE vtable-extension region
starting right after the shared 0x08-0x40 prefix); `CSKMIDIPortMsgHandler`
is `CSKMIDIInMsgHandler`'s own child (not `CSKMIDILocalCtrlMsgHandler`'s,
despite similar naming — table-size comparison settled it);
`CSKSpecialMsgHandler` is a totally separate, unrelated root (own 5-slot
vtable, zero shared slots) that only coincidentally reuses the same
"raw 4-byte MIDI event at offset +4" field convention.

This batch reconstructed `CSKMIDIMsgHandler` (16/16), `CSKSpecialMsgHandler`
(6/6), and `CSKSysExMsgHandler` (28/28) — 50 methods, deliberately scoped to
avoid `CSKMIDIInMsgHandler`'s own body (deeper: 33 methods, real
dying-note-tracking array arithmetic, a 922-byte constructor — next
continuation target, along with its own children `CSKMIDIPortMsgHandler`/
`CSKPadNoteByMIDIPortMsgHandler` which need it as a real base class first).
Files: `include/oa_ckg_midi_msg_handler.h`, `src/engine/
ckg_midi_msg_handler.cpp`, `verify/test_ckg_midi_msg_handler.cpp`. OA.ko
manifest 2990 -> 3040/21,689 (exact 50/50 credited via git-stash-baseline
name-set diff, 0 regressions).

### The "this is EAX, not ECX" + "*this+N is rodata-offset N+8" conventions (re-confirmed, now with a full derivation)

Every method here is `regparm(3)`: `this`=EAX, explicit params in EDX/ECX.
Ghidra's `__thiscall` label is generic ("some register holds `this`"), NOT
the MSVC ECX convention — its decompiled `this` parameter is always dead,
the real body reads an `in_EAX` pseudo-variable instead. Separately, every
`(**(code**)(*this + N))()` indirect call targets absolute
`.rodata._ZTVxxx` offset `N + 8`, because the object's own vptr already
points AT vtable slot 0 (rodata offset 0x08), not at the vtable's own
start (rodata offset 0). Verified against `CSKSpecialMsgHandler::
AnalizeAndProcess()`'s own 3 calls before trusting it for deeper bodies —
its `*this+8`/`+0xc`/`+4` calls resolve to rodata 0x10/0x14/0x0c
(`ProcessProgramChangeMessage`/`ProcessPitchBendMessage`/
`ProcessResetAllControllerMessage`), matching real MIDI status-byte
semantics (0xc0=ProgramChange, 0xe0=PitchBend, CC121=ResetAllControllers)
independently of the offset math itself.

### Three real transcription bugs caught by insisting on raw disassembly (not the Ghidra decompile)

1. **`ProcessPitchBendMessage()`'s real SetBendRange call is 3-arg, not
   1-arg.** Ghidra's decompile showed a single bogus argument (computed as
   if it were `this`). Raw disassembly showed: `this`=CKGEngine singleton,
   arg1(EDX)=`m_status & 0xf` (the PitchBend status byte's own low nibble,
   NOT `m_flags & 0xf` like every other channel read in this file),
   arg2(ECX)=a signed 6-bit-with-forced-sign transform of `m_data1`,
   arg3(stack)=the same transform of `m_data2`.
2. **`ProcessProgramChangeMessage()`'s bankId/index parameter order, and
   its 0xfe sentinel's real target field.** Confirmed EDX=bankId(param1),
   ECX=index(param2) by cross-checking the CALLEE's own prologue
   (`CKGBankManager::ChangeKarmaPerfForCombi`, same 2-param shape): it
   bounds-checks ECX against 0x7f (index-shaped) and stores EDX unchecked
   (bankId-shaped) — an independent check on a class NOT even in this
   batch's own reconstruction scope. `bankId` is always `m_data1&0x3f`
   (never touched by the 0xfe/>0x80 sentinel condition); the sentinel
   instead forces `index` to the literal `0xfffe`, a deliberate
   out-of-range value that (per the callee's own bounds-check branch)
   selects a different fallback path in code this batch doesn't own. A
   first-draft transcription got this backwards (mapped bankId onto
   `data1&0x3f`-carries-sentinel and index onto the raw byte) purely by
   trusting the naming instinct instead of the register evidence — caught
   by the KAT test, not by re-reading the disassembly a second time.
3. **`ShouldRecThisParameterChange()`'s bar/beat wraparound source
   register and missing +1.** The decompile conflated which of
   `CSPRClockHandler::Get{Current,Precount}Location()`'s 4 by-reference
   outputs feeds the wraparound add, and dropped a real `+1` to the "bar"
   comparison value entirely (`jmp`-back-and-reuse pattern in the raw
   asm, invisible in the naive decompile's apparent straight-line flow).

### Reconfirmed manifest-generator gotcha: a comment `word(` immediately before a real definition

Same bug class already documented in `ckg_module_param_msg_handler_family.md`
("DEF_RE gotcha... ANY unterminated `(` in a comment before the next real
definition triggers it, since comments essentially never contain a literal
`;`") — recorded here anyway because this batch hit it independently (8
instances, more than that batch's 6) before remembering to check for it,
and because it's the reason this file's own earlier "3 new manifest
gotchas" framing needed correcting: this is the SAME class as gotcha #3
there (trailing-comment-breaks-DEF_RE), not a new one. `gen_oa_manifest.py`'s
`DEF_RE` parameter-capture group is `[^;{}]*` — excludes only literal
`;`/`{`/`}`, NOT `(`/`)`. A prose comment containing "someword
(parenthesized text)" or "someword(args)" directly above a real out-of-line
function definition, with no `;`/`{`/`}` anywhere in between, lets the
regex's greedy param-capture span from that earlier "someword(" all the
way to the REAL function's own closing `)` + `{`, mis-attributing the
captured name as "someword" and silently NOT crediting the real method.
**Faster detection for next time**: run `DEF_RE.finditer()` directly
against new files with a match-span-length filter (>150 chars = suspect)
before ever reaching the manifest-diff step — a technique already noted in
the Module/Common/Global batch's own memory, not used here until after the
diff already caught the shortfall. Cost 8 real, correctly-implemented
methods (5 out-of-line bodies
whose leading comment happened to contain this shape, plus all 3 trivial
constructors — which ALSO need to move out-of-line from `ClassName() {}`
inline-in-header to `ClassName::ClassName() {}` in the .cpp, since an
in-class inline definition never produces the qualified `Class::Method`
name this heuristic requires) a false "pending" classification, caught
only by the exact-name-set diff showing 43 additions instead of the
expected 50. Fixed by rewording the affected comments (no
`identifier(`/`identifier (` adjacency) rather than touching the shared
regex — a project-wide regex fix felt too large a blast radius to audit
against the full 21,689-function corpus in one sitting. Also independently
re-confirmed the established `.text+0xXXXXXX` convention IS the
Decomp-object-relative offset (`ghidra_address - 0x10000`), not the raw
Ghidra address — this batch's comments briefly used the wrong (Ghidra-raw)
value throughout before a scripted sweep fixed all ~50 occurrences at
once; harmless to manifest crediting (name-matching alone already
succeeded) but would have misled a future cross-reference against
`OA.ko_Decomp`.

## Continuation

Remaining in the broader ~845-method CKG/CSK family, roughly by size:
`CKGEngine` (74, own methods not yet reconstructed — currently only an
opaque `ms_poInstance`/`ms_poKGParamEdit` stand-in with a handful of methods
added on-demand by callers), `CKGBankManager`'s own remaining surface,
`CKGRTCHandler` (27 pending beyond the methods declared here),
`CSKMIDIInMsgHandler` (33, next continuation target — see above),
`CSKMIDILocalCtrlMsgHandler` (28), `CSKMIDIPortMsgHandler` (12, needs
`CSKMIDIInMsgHandler` as a real base first), `CKGUIMsgProcessor` (17, more
complex parsing logic — `AnalizeAndProcessParamterChangeMessage` alone is
567 bytes), `CSKMIDIMsgProcessor` (22), `CKGTimerManager` (15),
`CKGEventDisplayManager` (15), and many more small `CKG*Sw`/`CKG*Knob`
classes covered by the widget hierarchy above. Full sweep recipe: group
`manifest/oa_functions.csv` pending rows by class prefix `CKG`/`CSK`, sort
by count.
