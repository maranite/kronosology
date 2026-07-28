---
name: ckg-module-param-msg-handler-family
description: OA.ko's KARMA "checked write" message-dispatch family -- CKGModuleParamMsgHandler/CKGCommonParamMsgHandler/CKGGlobalParamMsgHandler. ALL THREE NOW COMPLETE (2026-07-28). Module 131/131 (commit 3c451f6). Common 72/72 + Global 27/27 (commit 0f24e5e) -- Common shares Module's checked-write skeleton (cross-referenced against CKGSeqBackupCommonParam); Global is a THIRD, structurally distinct convention with no shared skeleton at all.
type: project
---

**UPDATE 2026-07-28 (Common + Global batch, commit 0f24e5e)**: closed out
the family's last two siblings in one sitting.

CKGCommonParamMsgHandler (72/72, src/engine/ckg_common_param_handler.cpp,
include/oa_ckg_common_param_msg_handler.h): confirmed structurally
identical to Module's own checked-write skeleton via direct disassembly
(same ShouldAttemptSysExShadowWrite/SysExShadowWriteIsNeeded gate shape,
same single-vs-dual-shadow rule -- dual iff the field's read-side
counterpart is m_default-based, no longer gated on ctx-indexing too;
Common's own fixed-offset SetScene field is m_default-based and DOES get
dual shadow, showing the "AND ctx-indexed" qualifier in Module's own rule
was incidental to that batch's field mix, not a real requirement). Field
offsets cross-referenced against the already-reconstructed READ-side
sibling CKGSeqBackupCommonParam (same file as CKGSeqBackupModuleParam,
karma_seq_backup.cpp) -- confirmed the standing prediction from Module's
own batch-1 note that this sibling would exist and the same technique
would apply directly. 66 methods were fully mechanical (reused the
Module-batch's own classify/gen_cpp Python pipeline, adapted for Common's
own message-struct layout and Send-call convention); 6 were hand-traced
standalones: GetKarmaPerfCommon (3-way switch on m_kind, but with a REAL
explicit NULL fallback for any kind outside {0,1,2} -- unlike Module's own
GetKarmaModule, which treats "anything else" as Combi), GetKarmaPerfCommon
ForSeqBackup, ShouldStoreToBackup (all near-identical to their Module
counterparts), SetChordMemVelocity (confirmed real no-op, 1-byte ret
body), SetTempo (the one non-const message pointer in the whole 3-class
family -- a real caller-visible mutation of msg->m_value to report back
the OLD tempo on 2 of its 3 real branches; also the only method using a
2-arg NotifyAfterEdit(bool,int) overload instead of the universal 0-arg
one), and SetScene (dual-shadow on a fixed-offset field, then a real
4-module linked-scene-id broadcast loop via CKGUIMsgProcessor::
SendModuleSceneMessage consuming the SAME packed-nibble idiom
CKGModuleParamMsgHandler::SetLinkedSceneId writes, just read here
instead).

Genuine Send-call convention deviation from Module (documented in the new
header's own comment): Common's message struct has only ONE index field
(m_index, +0xc) that does double duty as both the ctx-array index AND
every Send call's own "index" argument -- no separate m_deviceIndex/
GetRTParmBufferSelectId() indirection exists at all. Arity-driven Send-arg
shape: 1 arg = value only (matches every non-ctx-indexed field), 2 args =
(index, value), 3 args = (index, GROUP_CONST, value) where GROUP_CONST is
a real confirmed 0-based ordinal (SendDynModule's A/B/C/D/Last quintet =
0..4, SendRTPModule's A/B/C/D quartet = 0..3, SendChordMemNote/
SendChordMemVelocity's Note1..8[Vel] octets = 0..7 -- every constant
spot-checked against its own disassembly, not inferred from name
ordering). SetKnob1-8Value/SetSw1-8Value are still the 5-arg SendKnob/
SendAssignableSwitch shape but with a fixed constant 0 in arg0 (no RTParm
indirection) and msg->m_index directly in arg1 -- no
SKSTGGate_NotifyKarmaSliderPosition() tail call for either group here
(Module's own Knob-only addition, absent throughout).

CKGGlobalParamMsgHandler (27/27, src/engine/ckg_global_param_handler.cpp,
include/oa_ckg_global_param_msg_handler.h): a THIRD, structurally
distinct convention -- confirmed via direct disassembly of every single
method that there is NO CSPREngine gate, NO m_liveRecord/m_defaultRecordA/
m_defaultRecordB triple, NO KARMA-perf record lookup, and NO shared
control-flow skeleton to factor out at all (unlike Module/Common, no
ShouldAttemptSysExShadowWrite-style helper exists here). Global settings
are session-wide (MIDI channel, velocity curve, external pad config...),
not per-KARMA-module/per-Combi records. A single m_globalData pointer at
+0x4 is written directly at small fixed byte offsets (or a per-call
msg->m_index-indexed byte/dword array within it). Shapes, all confirmed
individually via disassembly, no cross-reference sibling exists (no
CKGSeqBackupGlobalParam -- this class's own params are apparently not
part of the KARMA seq-backup mechanism at all):
  - plain field write only (8 methods)
  - field write + one real CKGParamEdit::SendXxx()/free-function call,
    with NO suppression check of any kind -- this class never reads
    CKGEngine::ms_poInstance[0xb0] at all (6 methods)
  - setne-style plain boolean field write only (7 methods)
  - 2 confirmed real no-ops (SetLocalOn/SetNoteReceive, 1-byte ret
    bodies each, same convention as Common's SetChordMemVelocity)
  - SetLocalControllerMIDICh: no field write at all, just the same
    2-call tail SetMIDIChannel makes
  - SetMIDIFilter: real per-bit set/clear on one byte keyed by
    msg->m_index, ground truth's own rol-of-0xfffffffe clear-side idiom
    rendered as plain bit ops (semantically identical, not a distinct
    behavior)
  - SetMIDIClockSource: the ONE method with a real conditional gate -- a
    nonzero clock-source value that maps to an unavailable MIDI port
    (SKSTGGate_IsMidiPortAvailable) makes the entire method a no-op, no
    field write, no Send, at all
  - SetEnableMIDIInToKarmaModule: no field write, one real call to a
    confirmed genuine naming exception -- CKGParamEdit::
    SetEnableMIDIInToKarmaModule(), NOT prefixed "Send" like literally
    every other CKGParamEdit target across this whole 3-class family
    (verified directly via the real mangled symbol, not a typo introduced
    here)
HandleMessage()/ctor/dtor: deferred, same convention as every sibling.

DEF_RE gotcha instances found+fixed this batch (6 total, 3 per file): the
by-now-standard "bare word immediately before a (, with no ; before the
next real function's own {" trigger (broader than the originally-
documented "then a :: downstream" framing -- direct regex analysis this
batch showed the :: isn't actually required, ANY unterminated ( in a
comment before the next real definition triggers it, since comments
essentially never contain a literal ;). Caught via the standard
captured-name-set diff PLUS, this batch, direct experimental
DEF_RE.finditer() runs against the new files with a match-span-length
filter (>150 chars = suspect) -- a faster, more systematic detection
method than eyeballing prose, worth reusing going forward. One instance
(ckg_common_param_handler.cpp's own SetScene comment) genuinely cost
SetScene its own manifest credit before being caught and fixed via the
pre-build DEF_RE scan (not build/verify -- both stayed green throughout,
this bug class is invisible to both). Two more in
ckg_global_param_handler.cpp were caught the same way before ever
reaching the manifest-diff step -- notably a bare `~(1<<n)`: the
destructor-shaped leading `~` counts as a valid DEF_RE name char. A
.h-file-only instance (oa_ckg_common_param_msg_handler.h's own struct-
body-reaching runaway) was found but left unfixed since .h files in this
project never contain a real {}-bodied function definition of their own
(only ;-terminated declarations plus the two struct openings, neither
DEF_RE-trackable) -- confirmed zero manifest impact, pure cosmetic noise.

KAT technique: verify/test_ckg_common_param_handler.cpp (139 checks, 3
parts mirroring Module's own convention) and
verify/test_ckg_global_param_handler.cpp (60 checks, one block per method
given the lack of a shared skeleton to isolate). Both computed expected
values independently from the same ground-truth offset/shift/mask/arity
facts the generator used, not from the generated C source itself. Caught
2 real test-authoring bugs during this batch (both in the Common Part-2
hand-written block, not the source): a missing ctx-index*stride term in a
manually-computed byte address, and a scene-broadcast mock that
overwrote its own participation-gate byte -- both documented as a general
caution: hand-written KAT blocks for a representative-subset skeleton
exercise need the SAME arithmetic care as the mechanically-generated
Part-1 block, they are not exempt from the "derive from ground truth,
don't eyeball it" rule just because there are fewer of them.

Manifest quirk reconfirmed: CKGGlobalParamMsgHandler's own ctor picked up
"reconstructed (address)" credit despite never being implemented -- the
header's own opening .text+0x3c7660..text+0x3c79ba range citation
happens to start exactly at the ctor's real address, which ADDR_RE
doesn't distinguish from a real implementation. Same class of harmless
false-credit already documented for Module's own
SKSTGGate_NotifyKarmaSliderPosition stub-credit -- accepted per
established precedent, not corrected (removing the range citation would
lose real documentation value for zero benefit).

Manifest: 2730 -> 2830/21,689 (13.048%), delta +100 (72 Common + 27
Global real methods + 1 incidental Global-ctor address-credit), confirmed
via full before/after reconstructed-name-set diff, 0 regressions. make
verify: exit 0, 0 FAIL across the whole suite (10,245 checks total
project-wide). Real make ko-clean && make ko KDIR=/home/build/linux-kronos
Kbuild build: clean link, OA.ko produced (618056 bytes), zero errors,
zero undefined references.

This closes the whole CKG*ParamMsgHandler family -- all three siblings
(Module 131, Common 72, Global 27 = 230 methods total across the family)
are now fully reconstructed. No further targets within this specific
family; next dense-cluster survey should start fresh.


**UPDATE 2026-07-28 (batch 2, commit `3c451f6`)**: finished the 18
deliberately-deferred methods from batch 1 -- `SetKnob1Value`..`SetKnob8Value`/
`SetSw1Value`..`SetSw8Value` (Shape C/D: same Shape-B skeleton plus a
`CKGParamEdit::GetRTParmBufferSelectId(msg->m_deviceIndex)` indirection
feeding the Send call's own first arg; Knob-only adds a conditional
`SKSTGGate_NotifyKarmaSliderPosition(0)` tail call gated on UI mode != 1)
and `SetScene`/`SetLinkedSceneId` (real idx-dependent packed-nibble
outliers, traced from raw disassembly, NOT mechanically derivable from the
skeleton -- see the header comment in `oa_ckg_module_param_msg_handler.h`
for the full step-ordering trace; the two invert which of their two real
fields is suppression-gated vs unconditional). `CKGModuleParamMsgHandler`
is now 131/131, fully closed.

**Bug found + fixed in the process (affects ALL 85 batch-1 Shape-B
methods)**: `ShouldAttemptSysExShadowWrite()`'s range check was INVERTED.
Batch 1 wrote `return (unsigned)(mode-8) <= 2u; /* mode in {8,9,10} */` --
re-deriving the same gate while tracing `SetKnob1Value`/`SetSw1Value`
against 3 independent ground-truth disassemblies (`SetValue`
@.text+0x3cd650, `SetKnob1Value` @.text+0x3cf930, `SetGenCC`
@.text+0x3cb1a0, all byte-identical in this block) showed the `ja
<sysex-lookup-block>` branch is actually taken when `(mode-8)` is
unsigned `> 2`, i.e. mode is OUTSIDE {8,9,10} -- the shadow-write attempt
happens when mode is NOT in that range, not when it is. Fixed by flipping
the comparison to `> 2u`. The batch-1 KAT test had encoded the SAME
inverted assumption (`mode=9` "in {8,9,10}" expecting the shadow branch)
so it was internally self-consistent but not ground-truth-faithful; fixed
by changing the test's mode value to something outside the range (20)
instead. No other assertions needed to change since the test was
comparing against its own (also-inverted) expectations either way. This
is a good general lesson: a KAT passing cleanly only proves internal
self-consistency between the source and its own test, not ground-truth
fidelity -- worth an independent disassembly spot-check even on
"already-verified, already-shipped" shared helpers when a later batch
gives a natural opportunity (here: needing to re-derive the same gate for
2 more instances anyway).

**New field discovered**: `CKGModuleParamMsgHandler::m_pendingSceneSendGuard`
(`+0x14`, `void*`) -- `SetScene`-only, non-NULL suppresses its own
`SendScene()`/`NotifyAfterEdit()` pair entirely. Semantics beyond that
unconfirmed (never touched by any other method in this 131-method class).

**Manifest gotcha reconfirmed**: the SKSTGGate_NotifyKarmaSliderPosition
no-op stub (added to `src/stub/bar2_stubs.cpp` purely to link-satisfy the
Knob tail call, real body a separate out-of-scope KARMA-slider-UI
subsystem) counts as "reconstructed" in the manifest tool's own
name-match convention despite being an empty stub -- same as every other
deliberately-stubbed confirmed-real dependency project-wide. Manifest
delta was +19 (18 real methods + this 1 stub-credit), confirmed harmless
via the standard full before/after name-set diff (0 regressions).

Discovered 2026-07-28 during a fresh broad survey for the next dense
cluster after [[stg_value_getter_family]] was confirmed exhausted (both its
discovery methods, at 75 classes / manifest 2576). Standard survey
technique used: group ALL pending `manifest/oa_functions.csv` methods by
class, sort by count, then investigate the top unexplored entries via
`nm -C -S` + targeted `objdump -dr -M intel` sampling.

**The family**: `CKGModuleParamMsgHandler::Set*(CKGModuleParamMsg const*)`
(and its two siblings `CKGCommonParamMsgHandler`/`CKGGlobalParamMsgHandler`,
same convention, ~76/~27 more methods, NOT yet reconstructed) -- a
"validate a live KARMA-perf record write, notify the UI, and offer a SysEx
record-buffer override" dispatch convention. Previously misidentified (5th
STG batch) as possibly part of the STG value-getter family due to the
superficial naming similarity "CKG...ParamMsgHandler" -- confirmed NOT
related: different signature (`Set*(CKGModuleParamMsg const*)` returning
`void`/`bool`, not `STGConvertedParam&Get*(CSTGPatchMessageContext&)`),
different call skeleton entirely, ALL global (`T`) linkage (never weak/
COMDAT). `CKGModuleParamMsgHandler` alone: 131 real methods (`.text+
0x3c90f0`..`.text+0x3d05ac`, ground truth `/home/share/Decomp/
OA.ko_Decomp/OA.ko`), of which 113 done this first batch.

**Critical enabler**: this family's WRITE-side field layout is the exact
mirror of the already-reconstructed READ-side sibling class
`CKGSeqBackupModuleParam` (`src/engine/karma_seq_backup.cpp`,
`include/oa_karma_seq_backup.h`, from an much earlier, unrelated batch --
[[ckg_seq_backup_technique]]/[[ckg_bankmanager_class_facts]]). Both classes
access the SAME real per-module KARMA-perf record: `CKGSeqBackupModuleParam
::SetXxx()` READS a field at some offset/shift/mask into `m_value`;
`CKGModuleParamMsgHandler::SetXxx()` WRITES `msg->m_value` to the identical
offset/shift/mask. Every field parameter used in this batch (base register,
ctx-index stride, byte offset, bitfield shift/mask, signedness) was
extracted via a Python regex parser run directly against
`CKGSeqBackupModuleParam`'s own already-verified C source
(`src/engine/karma_seq_backup.cpp` lines 602-1588), NOT re-derived from raw
disassembly by hand -- this cross-reference is what made a from-scratch,
fully mechanical 113-method batch tractable in one sitting. **When
starting the Common/Global siblings' own batches, check whether
`CKGSeqBackupCommonParam`/an equivalent GLOBAL-scope read-side sibling
exists first** -- if so, reuse this exact cross-reference technique rather
than re-deriving fields from scratch.

**The shared control-flow skeleton** (confirmed byte-identical, register
allocation aside, across 9+ independently disassembled instances --
SetTranspose/SetOutputCh/SetKeyTop/SetModPercent/SetGenCC/SetSeed/SetValue/
SetSwName/SetSceneIsLinked/SetKnob):
```
field_write(m_liveRecord, K, msg->m_value)   // unconditional, always first

if (CSPREngine::ms_poInstance[0xa] != 0
    && this->m_defaultRecordA != 0
    && CKGUIMsgProcessor::ms_poInstance->mode(+0x6c) != 4
    && (unsigned)(mode - 8) <= 2)             // mode in {8,9,10}
{
    if (CSPRMIDIMsgProcessor::ms_poSysExPlayBuf->GetValue(0x6d, cfgByte, 5,
            msg->m_deviceIndex, 0, this->m_moduleIndex, msg->m_index,
            &local) == 0)                     // miss
    {
        CKGUIMsgProcessor::ms_poInstance->flag(+0x74) = 1;
        field_write(m_defaultRecordA, K, msg->m_value);
        if (<field is ctx-indexed AND its read-side counterpart uses
             m_default>)
            field_write(m_defaultRecordB, K, msg->m_value);  // dual-shadow
    }
}

if (CKGEngine::ms_poInstance[0xb0] == 0) {    // edits not suppressed
    CKGEngine::ms_poKGParamEdit->SendXxx(msg->m_deviceIndex[, msg->m_index],
                                          msg->m_value);
    CKGUIMsgProcessor::ms_poInstance->NotifyAfterEdit();
}
```
Steps 2 and 3 ALWAYS both run in sequence -- confirmed via every fallback
block's own trailing `jmp` landing on step 3's entry, not on `ret`. This is
NOT an if/else.

**Single- vs dual-shadow-write rule** (the one genuinely new piece of logic
beyond a straight cross-reference, derived empirically from 6+ instances,
independently reconfirmed by `oa_karma_seq_backup.h`'s own pre-existing
comment: "m_default consulted unconditionally... by SetSwName/SetKnobName,
the Sw/Knob-assignment value groups, and the RTParam group's A/B/C/D/Min/
Max/Value fields"): a field gets a SECOND shadow write (to
`m_defaultRecordB`, `this+0xc`) if and only if its `CKGSeqBackupModuleParam`
read-side counterpart is BOTH ctx-indexed (`m_index`-based) AND reads
through `m_default` (not `m_source`). Every other field -- fixed-offset,
or ctx-indexed-but-`m_source`-based -- gets exactly one shadow write (to
`m_defaultRecordA`, `this+0x8`).

**Send-call argument convention**: every `CKGParamEdit::SendXxx()` target
takes either 2 args (`msg->m_deviceIndex`, `msg->m_value`) or 3 args
(`msg->m_deviceIndex`, `msg->m_index`, `msg->m_value`), argument count
matching the target's own real demangled arity exactly -- no case observed
where the 2 vs 3 choice differs from arity, so it can be driven purely off
`argtypes.length` without inspecting the call site's own register/stack
setup per-function. A handful of fields (`SetGE`/`SetTZoneBypass`/
`SetSwName`/`SetKnobName`) skip the Send call entirely but still call
`NotifyAfterEdit()` when unsuppressed -- detect via an empty
`send_targets` list, don't assume every field has one.

**Batch 1 scope** (113/131 methods, manifest 2598->2711, commit pending):
included the trivial Shape-A ctx-indexed writes (24: `SetModifiedKnob1-8`/
`SetModifiedSw1-8Value`/`SetModifiedSw1-8Status`, no check/notify/shadow at
all -- matches `CKGSeqBackupModuleParam::SetModified*`'s own stride-10
array exactly), the full Shape-B checked-write skeleton (85, covering
fixed-offset, ctx-indexed stride 1/2/8, and bitfield variants), plus 4
special-shaped standalones: `SetSolo` (no CSPREngine/shadow gate at all,
just the CKGEngine suppression check), `ShouldStoreToBackup` (reuses the
exact same gate+GetValue call, returns bool instead of writing),
`GetKarmaModule` (3-way switch on `msg->m_kind`, dispatches to
`CKGBankManager::Get{Prog,Seq,Combi}KarmaPerfModule`, added 2 new
declarations + 2 minimal placeholder enums `eSTGCombiBankId`/
`eSTGProgramBankId` to `oa_engine_init.h`'s existing `CKGBankManager`
struct for correct mangling), and `GetKarmaPerfModuleForSeqBackup` (near-
identical logic to the already-reconstructed
`CKGSeqBackupModuleParam::GetKarmaPerfModuleForSeqBackup`, cross-checked
against it directly).

**[RESOLVED in batch 2, 2026-07-28 -- see the UPDATE note at the top of
this file] Was: deliberately excluded, next-batch targets** (18/131, already fully
traced, NOT yet written up as C):
- `SetKnob1Value`..`SetKnob8Value` (8) and `SetSw1Value`..`SetSw8Value` (8):
  same Shape-B skeleton PLUS a `CKGParamEdit::GetRTParmBufferSelectId()`
  indirection before the Send call, and (Knob only) a conditional
  `SKSTGGate_NotifyKarmaSliderPosition()` tail call gated on
  `CKGUIMsgProcessor::ms_poInstance`'s own mode being exactly 1. Full
  disassembly already captured in this batch's own working notes
  (`SetKnob1Value`/`SetSw1Value` fully traced) -- straightforward
  extension, not a new discovery.
- `SetScene`/`SetLinkedSceneId` (2): real idx-dependent packed-nibble/
  multi-branch logic, ~0x2c1/0x24d bytes, same complexity class as the STG
  family's own SetLinkedSceneId-style outliers.
  `SetLinkedSceneId`'s READ-side semantics are already known verbatim via
  `CKGSeqBackupModuleParam::SetLinkedSceneId`'s own hand-reconstructed body
  (2-scenes-per-byte, 3-bit low/high-nibble pack keyed on `idx&1`) --  only
  the WRITE-side's own dual-branch shadow-copy logic needs tracing, the
  field math itself does not.

**Decoder technique used**: NOT a from-scratch instruction-pattern
scripted decoder like [[ckg_seq_backup_technique]] or the STG family's own
`CtxIndex` helper -- instead, a `classify.py`/`gen_cpp.py` Python pipeline
that (1) enumerates candidates via `nm -C -S` filtered to
`CKGModuleParamMsgHandler::(Set|GetKarma|ShouldStore)`, (2) fact-extracts
per-function landmark booleans (CSPREngine-check presence, stride-9 `lea`
presence, `SendAssignableSwitch`/`GetRTParmBufferSelectId`/`SendKnob`
presence) via regex over `objdump -dr -M intel` text to bucket into shapes,
(3) for the Shape-B bucket, looks up the exact field descriptor (base,
stride, offset, type, shift, mask) from the independently-parsed
`CKGSeqBackupModuleParam` source rather than re-deriving it, (4)
code-generates all 113 bodies from those two fact tables via string
templates. Two shared `static inline`-equivalent member-function helpers
(`ShouldAttemptSysExShadowWrite()`/`SysExShadowWriteIsNeeded()`) factor out
the skeleton's own common blocks, same "shared helper, thin per-method
body" convention as the STG family's `CtxIndex()`.

**KAT technique**: `verify/test_ckg_module_param_handler.cpp`, 3 parts --
(1) primary-write-only check for all 113 methods with both gates held
closed (CSPREngine gate shut, CKGEngine suppressed) so only the
unconditional `m_liveRecord` write executes, expected values computed
independently in the generator script (not re-using the source's own C
expressions); (2) full gate+shadow-write+Send/Notify skeleton exercise for
a representative subset (`SetOutputCh` single-shadow, `SetKnob`
dual-shadow, `SetGenCC` single-shadow-ctx-indexed, `SetSolo`,
`ShouldStoreToBackup`); (3) `GetKarmaModule`/`GetKarmaPerfModuleForSeqBackup`
against predictable fake-address mocks (`0x1000+idx`/`0x2000+idx`/
`0x3000+idx`, never dereferenced, only their integer value checked).

**New DEF_RE gotcha class, genuinely distinct from every prior variant
logged in [[stg_value_getter_family]]**: a bare `Name()` (even EMPTY
parens) or a balanced `(...)` parenthetical aside in a comment, followed
ANYWHERE later (even much later, across multiple sentences) by a
`ClassName::Method(...)` real definition with NO intervening `;`, is
enough to trigger a runaway match -- the trigger is NOT the aside's own
un-closed paren (it IS balanced) but the FIRST `::` after it, which
satisfies the DEF_RE's `(?::\s*[^;{}]*)?` "trailing init-list" optional
group (a single literal `:`, and `::` supplies one). This is a strictly
larger risk surface than the previously-documented "parenthesis left open"
or "literal `*/`" triggers: it fires even when every paren in the comment
is individually well-formed. Caught 8 separate instances across this one
file (`kind (stride 10...)`, `SysExShadowWriteIsNeeded (confirmed...)`,
`m_kind (1=Program...)`, `GetKarmaPerfModuleForSeqBackup(int) (karma_seq_
backup.cpp:611)`, a bare `Send()` in prose, two more `(Seq only)`/`(0xffff
is the sentinel)` asides). Manifested as exactly 4 real methods silently
NOT counted as reconstructed by `manifest/gen_oa_manifest.py` despite
`make verify`/`make ko` both passing clean (the swallowed methods still
compile fine -- DEF_RE is a name-detection heuristic, not a compiler check,
so this class of bug is INVISIBLE to every build/test signal and can only
be caught by the DEF_RE captured-name-set diff itself). **New standing
rule**: after writing any header-comment prose in this project, treat
EVERY literal `(` (empty-paren "Name()" mentions included) as suspect
regardless of whether its own matching `)` is present later in the same
sentence -- the real danger is a `::` anywhere downstream before the next
real function, not paren balance. Reword to em-dash/comma-delimited
clauses with zero literal `(` in header-comment prose, the same
established convention, just now understood to be necessary even when the
parens Look balanced.

**Tooling note**: `git stash` without `-u` does NOT touch untracked files
-- when comparing before/after manifest state for a batch that ADDS new
files, either use `git stash -u` or manually move the new untracked files
out of the tree before the "before" manifest run (mirrors [[shared_repo_
commit_hygiene]]'s general caution, but for manifest baselines
specifically, not concurrent-session staging). **When moving files aside
for this, NEVER pass multiple directory arguments to one `mv` invocation**
(`mv holdout/* src/engine/ include/ verify/` moves EVERYTHING, including
the `src/engine/` and `include/` directories themselves, into the LAST
argument -- `mv` with N>2 args always takes the last as the sole
destination). This cost a real, if fully recovered, incident this batch:
`src/engine/`'s entire ~161-file tracked tree got relocated into
`verify/engine/` and then accidentally `rm -rf`'d while trying to fix the
mv mistake. Recovered cleanly via `git checkout -- src/engine` (nothing
was staged/committed, so this was a zero-risk full restore) plus manually
moving `include/` back from `verify/include/`. Move exactly one holdout
file (or one directory) per `mv` invocation when doing this kind of
temporary relocation, never batch multiple sources against multiple
plausible destinations in one command.

**Next targets**: `CKGModuleParamMsgHandler` itself is now fully closed
(131/131, batch 2, 2026-07-28). Start `CKGCommonParamMsgHandler` (~76 pending) and `CKGGlobalParamMsgHandler`
(~27 pending) -- check first whether a `CKGSeqBackupCommonParam`-equivalent
read-side sibling exists for field cross-reference (very likely, given
`CKGSeqBackupCommonParam` already exists per [[ckg_seq_backup_technique]]'s
own batch), same technique should apply directly.
