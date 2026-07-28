---
name: stg-value-getter-family
description: OA.ko's largest known dense accessor family (~2300 pending methods, ~180 STG synth classes) -- STGConvertedParam &Get*(CSTGPatchMessageContext&) "value getter" convention; 11 classes done (CSTGString, CSTGOrganModelPatch, CSTGMS20, CSTGAnalog4PoleBase, CSTGPolysix, CSTGAnalogSyncOsc, CPianoOsc, CSTGEPModelPatch, CSTGOrganOsc, CSTGVPMOsc, CSTGMS20ModelPatch), 668 methods reconstructed, manifest 1441->2113
type: project
---

Discovered 2026-07-28 while looking for the next scripted-decoder target
after CKGSeqBackupCommonParam/ModuleParam (see [[ckg_seq_backup_technique]]
and [[ckg_bankmanager_class_facts]]). This family is MUCH bigger than
CKGSeqBackup's 197 methods -- roughly 2300 pending `Get*`/`Set*` methods
across ~180 STG (synth tone generator patch) classes, almost all living in
one contiguous real `.text` range `~0x5a0000`-`0x5c0000`. First batch
(commit after `efa0926`) picked `CSTGString` as the pilot class: 105/107
methods reconstructed, KAT-verified, real Kbuild build clean. Manifest
1441 -> 1550.

**The pattern**: `STGConvertedParam &ClassName::GetFoo(CSTGPatchMessageContext
&ctx)` reads one field of `this` (occasionally index-scaled via `ctx`'s own
+0x4 field, NOT `this`'s), writes it into the shared static
`CSTGParamsOwner::sValueGetterTemp.value` (+0x0) and, for most (not all --
see below) 32-bit fields, ALSO `.displayValue` (+0x18), then returns
`&sValueGetterTemp`. Already partially known before this batch via
`CSTGADSRBase`'s own 20 hand-written methods (`src/engine/adsr_base.cpp`) --
this batch is the first to recognize it as a HUGE, decoder-friendly,
codebase-wide convention rather than a one-off.

**Why these methods are weak/COMDAT `.text._ZN...` per-symbol sections**
(unlike ordinary member functions, which land in the plain merged `.text`):
they're emitted with `inline` linkage (per-class template-like
instantiation from a shared header/macro pattern in ground truth), so each
gets its own tiny section the linker can dedupe. This is exactly what makes
`objdump -dr -j .text._ZN...` per-symbol dumping (rather than one
`--start-address`/`--stop-address` range) necessary -- ground truth does
NOT lay these out contiguously in one class's own address block the way
CKGSeqBackup's two classes were (that one WAS one giant contiguous range).
To pull a whole class's methods in one `objdump` invocation: `nm $KO | awk
'{print $3}' | grep '^_ZN<len><ClassName>[0-9]\+Get'` then pass every
`-j .text.<mangled>` as a separate flag in one `objdump -dr -M intel`
call (objdump accepts many `-j` flags at once).

**Field-shape decoder vocabulary** (superset of CKGSeqBackup's, see
[[ckg_seq_backup_technique]] for the base vocabulary): add `mov edx,[edx+0x4]`
+ `lea edx,[edx+edx*4]` (stride 5, single-modulation-slot pattern) or
`shl edx,N` (stride 2^N, multi-field-block pattern) reading a **dynamic
per-call index from `ctx` itself**, not `this` -- this is the "AMS
modulation source/intensity" sibling-getter sub-family
(`GetXxxAMSSource`/`GetXxxAMSIntensity`), used when a parameter's own
current AMS-slot index isn't fixed at compile time. Whether `.displayValue`
gets the second write is NOT simply "32-bit implies dual-write" -- 5 of 69
CSTGString dword fields (all discrete/enum: PrePost, UseFilter, TableSelect
selectors) are a confirmed real exception that write `.value` only. Always
derive dual-write empirically from whether the `mov ds:0x18,eax` instruction
is actually present, never from width alone.

**3 outlier shapes to expect and exclude, not force**: (1) a different
calling convention entirely (`GetSubComponent`-style `__thiscall` sub-object
accessor, not `__regparm3`) -- check the manifest's `calling_convention`
column, a mismatch is a strong signal it's not part of this family; (2) a
real `fyl2x`-based log2/dB conversion (`GetNoiseSaturation`-style); (3)
genuine DSP math against a runtime value (`GetPluckDelay`-style, uses
`CSTGAudioBusManager`'s live sample rate via `fmul`/`fistp`) -- these last
two are NOT in the weak/COMDAT cluster at all, they're ordinary
global-linkage symbols in the plain merged `.text` (check `nm`'s `T` vs `W`
column and section name before assuming a method is part of the cluster).

**Second batch (2026-07-28, commit after `594e8a2`): `CSTGOrganModelPatch`
(101/103) + `CSTGMS20` (90/90) done**, manifest 1550 -> 1741. Ground truth
binary used for `objdump`/`nm`: `/home/share/Decomp/OA.ko_Decomp/OA.ko`
(not stripped, has full symbols -- this is "OA_real.ko" in older notes).
Reused/extended the CSTGString decoder rather than writing a fresh one;
both new classes' Get* candidates were found via `nm $KO | awk '{print
$2,$3}' | grep -E " _ZN<len><ClassName>[0-9]+Get"` then filtered to `W`
(weak/COMDAT) rows only -- the `T` (global-linkage) rows in that same
grep are ALWAYS a different, non-value-getter mechanism (extra args
beyond `ctx`, e.g. `GetRequiredVoiceInfo`, `GetVoiceLevelEstimate`,
`GetSubRateExtModSourceAddress`) and should be excluded from the
candidate set up front, not fed to the decoder and rejected one by one.

`CSTGOrganModelPatch` dialect: SIMPLER than CSTGString's -- `this` stays
in eax throughout, zero ctx-dynamic-index methods (every AMS* sibling is
a fixed-offset field, confirmed empirically). New shape: boolean NOT
(`movzx eax,BYTE[eax+K]` + `xor eax,0x1` + `movzx eax,al`, single
.value-only write) on `GetPercLevelSwitch`. 2 outliers: `GetRotaryHornMic
Distance`/`GetRotaryRotorMicDistance` compute real `1.0f - field` via x87
(`fld1`/`fsub`/`fst`/`fstp`) -- same class of outlier as CSTGString's
`GetNoiseSaturation`. Also reconfirmed the "32-bit does not always imply
dual-write" quirk on 3 more discrete/enum selector fields (`GetVCType`,
`GetRotaryHornStopPhase`, `GetRotaryRotorStopPhase`).

`CSTGMS20` dialect: MIXED -- most methods match CSTGOrganModelPatch's
simple eax shape, but also has a real ctx-dynamic-index sub-family
(Standard*/Mixer* AMSSource/AMSIntensity siblings) requiring 2 NEW
decoder shapes beyond CSTGString's own stride-5/stride-32 cases: (1) the
usual `mov edx,[edx+0x4]` + `lea edx,[edx+edx*4]` (stride 5) premultiply,
but then the field load ITSELF carries an extra SIB scale factor
(`[eax+edx*2+K]`), so the effective per-index stride is `5 * SIB_scale`
(10 here) -- the second multiply is folded into the addressing mode, not
a separate instruction; (2) a BARE unscaled ctx-index load with no `lea`
at all (`[eax+edx*1+K]`, stride 1) -- `GetInputJack` alone. Modeled both
via the same `CtxIndex(ctx, off, stride)` helper, passing the
already-fully-reduced stride. Zero outliers in this class -- a first for
the family.

**Tooling gotcha found and fixed this batch** (distinct from the known
"literal `*/` in a comment ends the block comment early" issue):
`manifest/gen_oa_manifest.py`'s NAME-heuristic `DEF_RE` regex does not
balance parentheses in its captured params group (`[^;{}]*`), so an
unbalanced-looking `(` triggered by ordinary prose -- a contraction like
"didn't (Foo", or even a plain "pending (see ...)" aside with NO
semicolon anywhere between it and the next real function -- makes the
regex swallow everything up to and including the FIRST real function's
opening brace as bogus "parameter list" text, silently hiding that one
function from the reconstructed count (no compile error, just an
undercount). Cost exactly 1 method per new `.cpp` file this batch
(`CSTGOrganModelPatch::GetAmpGain`, `CSTGMS20::GetAnalog`) until fixed by
rewriting both files' leading header comments to avoid parenthetical
asides before the first function (use `--` em-dashes instead of `(...)`
for asides in that specific span). Verify by running `DEF_RE` directly
against the `.cpp` file and diffing the captured name set against every
declared method name -- do this as a standard post-generation check for
every future class in this family, not just when something looks wrong.
`.h` files are immune (declarations end in `;`, which resets the regex's
runaway match immediately) -- only `.cpp` leading comments need this
check.

**Next targets** (same technique, not yet done): `CSTGProgramSlot` --
SKIP for the value-getter treatment, it is NOT a fresh opaque class like
the others: it already has a large, heavily-annotated real struct
definition with a modeled vtable in `include/oa_global.h` (base class of
`CSTGProgramModeProgramSlot`/`CSTGProgramModeDrumTrackSlot`), so adding
~85 Get* members there is a much bigger, riskier undertaking than
declaring a fresh minimal opaque struct -- re-scope or hand off
separately if tackled. `CSTGAnalog4PoleBase`/`CSTGPolysix`/
`CSTGAnalogSyncOsc` DONE this batch (see below). Remaining
fresh-opaque-class candidates: `CSTGProgram` (65 -- verify this one isn't
also already-modeled like CSTGProgramSlot before starting), `CPianoOsc`
(59), `CSTGEPModelPatch` (58), and ~162 more classes -- see
`manifest/oa_functions.csv` filtered for `qualified_name` matching
`^(Set|Get)[A-Z0-9_]` grouped by class, or rerun the survey query (group
pending Set*/Get* methods by class, sort by count). Always check
`grep -rln <ClassName> src include` FIRST before picking a class -- and
use a WORD-BOUNDARY grep (`\bClassName\b`), not a bare substring, since a
similarly-named-but-unrelated class (e.g. `CSTGPolysixModel` vs
`CSTGPolysix`) can produce a false "already referenced" positive with a
naive substring match. Same lesson as CSTGProgramSlot above.

**Third batch (2026-07-28, commit `e59300a`): `CSTGAnalog4PoleBase`
(74/76) + `CSTGPolysix` (71/71) + `CSTGAnalogSyncOsc` (63/65) done**,
manifest 1741 -> 1949. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`). All three picked from the
prior batch's own priority list; `grep -rln <ClassName> src include`
confirmed all three genuinely fresh (zero pre-existing references) before
starting -- note `CSTGPolysix` needs a *word-boundary* grep
(`grep -rn 'CSTGPolysix\b' | grep -v CSTGPolysixModel`), a naive substring
grep falsely flags it as already-referenced because of the unrelated,
already-modeled `CSTGPolysixModel` wrapper class in `oa_engine_init.h`
(voice-model factory, not the STG patch class).

**New signature-outlier check, should now be standard**: filtering the
`nm`-derived weak-symbol candidate list to mangled names ending in
`ER23CSTGPatchMessageContext` (i.e. the real "takes a
`CSTGPatchMessageContext&`" signature) BEFORE running the decoder caught
`CSTGAnalog4PoleBase::GetSubComponent(unsigned short)` up front -- same
outlier class as `CSTGString`'s own `GetSubComponent`, but this one is
weak/COMDAT ('W'), not global-linkage ('T'), so the old "check the W/T
column" heuristic alone would NOT have caught it. Do the mangled-suffix
signature check as a first-class step, not just the W/T check.

**3 new field-shapes this batch**:
1. Ctx-index field load with an explicit ×4 SIB scale on top of the
   usual stride-5 `lea edx,[edx+edx*4]` premultiply --
   `[eax+edx*4+K]` -- effective stride 20 (`CSTGPolysix`'s
   `ExtMod*Intensity`/`ExtModSource` group). Third distinct SIB-scale
   value confirmed for this sub-family now (×1 bare, ×2 from CSTGMS20,
   ×4 here) -- always derive the effective stride as
   `premultiply_stride * SIB_scale` from the actual addressing mode,
   never assume a fixed value.
2. Boolean nonzero/zero integer test: `mov reg,[this+K]` (dword);
   `test reg,reg`; `setne al` or `sete al`; `movzx eax,al`; single write.
   Distinct from the earlier boolean-NOT shape (`xor eax,0x1` on an
   already-loaded 0/1 value) -- this one derives the 0/1 result from an
   arbitrary dword's truth value, not from flipping an existing bit.
   `CSTGAnalogSyncOsc`'s `GetRingModModulatorSelect`/
   `GetRingModCarrierSelect` (setne, "is nonzero") and
   `GetSubOscAudioInModeSelect` (sete, "is exactly zero") all confirmed
   real and mechanically decodable -- included in the batch, not treated
   as outliers, since a truth-value test is not a numeric transform.
3. (Not a new *decoder* shape, but a new outlier variant worth logging)
   A real x87 ordered-equal-to-1.0 float compare via
   `fld field; fld1; fucomip st,st(1); fstp st(0); setnp dl;
   cmove eax,edx; xor eax,0x1; movzx eax,al` -- effectively
   "field is not exactly 1.0" as a boolean.
   `CSTGAnalog4PoleBase`'s `GetFilterALeakage`/`GetFilterBLeakage`, both
   excluded. Also a NEW numeric-transform outlier variant: SSE `sqrtss`
   (real square root, not the earlier classes' `fyl2x`/`fmul`+`fistp`
   examples) on `CSTGAnalogSyncOsc::GetNoiseCutoff`, excluded for the
   same "genuine DSP computation, out of scope" reason as
   `CSTGString`'s `GetPluckDelay` pair.

`CSTGPolysix` was the first class in this family with genuinely ZERO
outliers of any kind across its full candidate set (matching
`CSTGMS20`'s earlier zero-outlier result) -- 71 candidates, 71 decoded.

Reused the exact same KAT-generation discipline (separate Python
evaluator over the same parsed shape facts, deterministic
`buf[i]=(i*0x9f+0x37)&0xff` pattern, ctx index fixed at 3) and the same
DEF_RE parenthesis-balance post-generation check. Caught ANOTHER fresh
instance of the "*/ literal in prose" gotcha this batch (distinct bug
from the DEF_RE parenthesis one, previously only documented in
[[ckg_seq_backup_technique]] for OA.ko's own family): adjacent
`FilterA*/FilterB*` prose in `oa_stg_analog4pole_base.h`'s header comment
formed a literal `*/` that silently closed the block comment early,
turning the rest of the derivation prose into raw (uncompilable) source
text -- caught immediately via an open-vs-close `/*`/`*/` count check
(`text.count("/*") == text.count("*/")`) run on every new file right
after writing it, before ever attempting to build. Recommend running
BOTH the DEF_RE-match-count check AND this brace-balance count check as
one standard post-generation step from now on, not just the DEF_RE one.

**Fourth batch (2026-07-28, commit `515830e`): `CPianoOsc` (46/53) +
`CSTGEPModelPatch` (42/42) done**, manifest 1949 -> 2037. Same ground
truth binary (`/home/share/Decomp/OA.ko_Decomp/OA.ko`). `CSTGProgram`
(65 pending Get*/Set*, next on the prior batch's own priority list) was
spot-checked FIRST via `grep -rn 'CSTGProgram' include/oa_global.h |
wc -l` (97 hits) and confirmed already heavily modeled -- real ctor,
`Initialize`/`Copy`, vtable, full struct at `oa_global.h:2227` -- same
situation as `CSTGProgramSlot`, correctly skipped again in favor of
`CPianoOsc`/`CSTGEPModelPatch`, the next two genuinely-fresh classes on
the list. `CPianoOsc` is notable as the first class in this family that
is NOT itself `CSTG`-prefixed (it's the acoustic-piano voice-model patch
component) yet fully participates in the same convention -- same
`sValueGetterTemp` sink, same `CSTGPatchMessageContext&` signature, same
weak/COMDAT per-symbol sections. Its 61 total `Get*` symbols split as: 53
real `ER23CSTGPatchMessageContext`-suffixed weak candidates (46 decoded +
7 outliers), plus 8 pre-excluded up front (`GetRequiredVoiceInfo` T
linkage/extra args, `GetTransposedNote` `__thiscall` unrelated helper,
and 6 metadata/factory-table `__cdecl` stubs -- `GetId`/`GetName`/
`GetNumParams`/`GetParamDescriptors`/`GetMessageHandlers`/
`GetValueGetters`, all 6-byte trivial accessors, a NEW excluded-family
shape worth recognizing on sight in future classes: `__cdecl`,
tiny fixed size, name doesn't fit `Get<ParamName>`). No `Set*` methods
exist for this class at all.

**New ctx-dynamic-index shape**: `CPianoOsc`'s Level/MultisampleNum/
BankType/BottomVelocity group (7 named parameter categories x 4 fields =
28 methods) uses effective stride 25 -- `movzx edx,[edx+0x4]` then TWO
back-to-back `lea edx,[edx+edx*4]` premultiplies (5*5), with a bare
`[eax+edx*1+K]` addressing mode (no additional SIB scale). This is
distinct from every prior stride variant in the family (CSTGString's
bare x1, CSTGMS20's x2-via-SIB giving 10, CSTGPolysix's x4-via-SIB
giving 20) -- all of which were a single `lea` premultiply plus an extra
SIB scale factor on the final load. Modeled via the same `CtxIndex`
helper, just passing 25 directly as the already-fully-reduced stride --
no helper code changes needed, only the decoder's shape-recognition
needed extending (chained double-`lea`, still no SIB scale, as its own
distinct case alongside the existing single-`lea`-plus-SIB-scale cases).

**New outlier class**: `CPianoOsc`'s 7 `Get*BankSelect` methods
(`GetBankSelect`, `GetResonanceBankSelect`,
`GetUnaCordaResonanceBankSelect`, `GetKeyOffNoiseBankSelect`,
`GetReleaseSampleBankSelect`, `GetUnaCordaBankSelect`,
`GetUnaCordaReleaseBankSelect`) compute a sub-object pointer via the same
ctx-index arithmetic as the group above, but then make a REAL call
(`call`, not a field load) into
`CSTGMultisampleBankUUIDAndStereoFlag::GetBankIdAndStereoFlag()` --
itself still unreconstructed (348 bytes, `.text+0x57be0`), which itself
calls the also-unreconstructed `FindBankUUID` (503 bytes). Confirmed via
`grep CSTGMultisampleBankUUIDAndStereoFlag manifest/oa_functions.csv` --
both still `pending`. This is a genuinely NEW outlier class for the
family: every prior outlier (CSTGString's `GetPluckDelay`/
`GetNoiseSaturation`, CSTGOrganModelPatch's rotary-mic-distance pair,
CSTGAnalog4PoleBase's leakage pair, CSTGAnalogSyncOsc's `GetNoiseCutoff`)
was real DSP/math computation (fyl2x/sqrtss/x87 compare) against the
object's OWN data; this is the first "delegates to another undecoded
real member function on a different class" case. Recognize this shape
by: ctx-index arithmetic feeding into `lea eax,[...]` (building a
pointer, not dereferencing it) immediately followed by a `call` with an
`R_386_PC32` relocation to a real mangled symbol, rather than a
`mov`/`movzx`/`movsx` load.

**New confirmed "32-bit does not always imply dual-write" exception**:
`CPianoOsc::GetKeybedSize` -- fixed dword field, single `.value`-only
write, no `.displayValue` -- same class of exception as prior batches'
discrete/enum dword fields (CSTGString's PrePost/UseFilter/TableSelect,
CSTGOrganModelPatch's VCType/RotaryHornStopPhase/RotaryRotorStopPhase).

`CSTGEPModelPatch` dialect: the SIMPLEST yet in this family -- every one
of its 42 real candidates is a fixed-K field read directly off `this`,
zero ctx-dynamic-index methods of any kind (no AMS-slot-array
sub-family at all, unlike CPianoOsc/CSTGPolysix/CSTGMS20). Zero outliers
-- only the third class in the family with a full clean sweep (after
CSTGPolysix and CSTGMS20). No exceptions to the width-vs-dual-write rule
found here either -- every dword field dual-writes, every byte field
(signed or unsigned) single-writes, no surprises.

**Tooling note, reconfirmed and hardened this batch**: both new files'
leading header comments hit BOTH known gotchas on the first draft --
the DEF_RE parenthesis-swallowing bug (a plain "derivation notes (46 of
53 ... see header)." aside with no semicolon before the next real
function) AND the literal-`*/`-in-prose bug ("Tine*/Reed*" in
`CSTGEPModelPatch`'s header). Caught BOTH via script before ever
attempting to compile: (1) `text.count("/*") == text.count("*/")` on
every new file, (2) an exact DEF_RE captured-name-set diff (`got == want`
where `want` is `{f"{cls}::{name}" for name in declared_methods}`) rather
than just checking the match COUNT -- a count-only check would have
still passed at 46/42 even with the bug, since the swallowed real
function's match got silently replaced by a same-count bogus "notes"/
"kind" match with the wrong name, only a set-equality check catches this
reliably. The eventual fix in both files was the SAME approach as the
established convention: rewrite every parenthetical aside as an em-dash-
or colon-delimited clause instead, so ZERO literal `(` characters appear
in prose before the first real function/struct in either the `.h` or
the `.cpp`. Recommend treating "zero parens in leading comments before
the first code construct" as the default authoring style for all future
batches' file headers, rather than writing parenthetical asides first
and fixing them after the fact -- cheaper than the catch-and-fix cycle.

**Next targets** (same technique, not yet done): ~160 more classes
remain. `CSTGProgram` and `CSTGProgramSlot` both confirmed
already-heavily-modeled, skip. Re-run the survey query (group pending
`Set*`/`Get*` methods by class from `manifest/oa_functions.csv`, sort by
count, then for each candidate: (1) word-boundary
`grep -rn '\bClassName\b' src include` to rule out an already-modeled
class or a name collision with an unrelated similarly-named class (the
`CSTGPolysix`/`CSTGPolysixModel` and `CSTGProgramSlot` precedents), (2)
`nm $KO | awk '{print $2,$3}' | grep -E ' _ZN<len><ClassName>[0-9]+(Get|
Set)' | grep 'ER23CSTGPatchMessageContext$'` to get the exact real
candidate set up front, filtered to weak linkage AND the exact ctx-only
signature suffix before ever running the decoder.

**Fifth batch (2026-07-28, commit `0863921`): `CSTGOrganOsc` (13/36) +
`CSTGVPMOsc` (44/44) + `CSTGMS20ModelPatch` (19/19) done**, manifest
2037 -> 2113. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`). All three picked from the
prior batch's own "~160 more classes remain" backlog by size after the
usual pre-checks: word-boundary `grep -rn '\bClassName\b' src include`
(both `CSTGVPMOsc`/`CSTGOrganOsc` genuinely fresh -- zero hits;
`CSTGMS20ModelPatch` also fresh and explicitly confirmed DISTINCT from
the already-done `CSTGMS20`, same precedent as
`CSTGPolysix`/`CSTGPolysixModel`) and the `nm`-derived weak-linkage +
`ER23CSTGPatchMessageContext`-suffix candidate-set filter before ever
running the decoder.

`CSTGOrganOsc` (tonewheel-organ oscillator patch) is notable for how
FEW of its 36 pending symbols turned out to be real candidates: only 13.
The other 23 are global ('T') linkage. Most of those are an obvious
different shape (Set* with an extra bool/enum argument beyond ctx), but
8 of them -- `GetLowerNoteCount`, `GetUpperNoteCount`,
`GetLowerDrawbarSum`, `GetUpperDrawbarSum`, `GetEXPercDrawbarSum`,
`GetLowerNoteCountCompression`, `GetUpperNoteCountCompression`,
`GetVoiceLevelEstimate` -- superficially match the family's own
`Get*(CSTGPatchMessageContext&)`-shaped mangled suffix. Spot-checked
`GetLowerNoteCount`'s own disassembly rather than trusting the linkage
check blind: it's `mov eax,[eax+8]; mov edx,[edx+0x28]; movsx
eax,[eax+8]; mov eax,[edx+eax+0xd8]; ret` -- a plain per-voice runtime
note-count read returned directly in eax, ZERO `sValueGetterTemp` write,
ZERO `STGConvertedParam&` return. Real per-voice runtime state, not a
static patch-value accessor, despite the matching mangled suffix. This
reconfirms "T linkage = different mechanism" is trustworthy even when a
candidate's signature alone looks like a match -- linkage beats
signature shape when they disagree.

`CSTGVPMOsc` (FM/ring-mod/waveshaper phase-modulation oscillator patch):
44/44 real ctx-only candidates, 1 pre-excluded outlier
(`GetSubComponent(unsigned short)`, the same different-signature
sub-object-accessor shape as CSTGString's/CSTGAnalog4PoleBase's own).
SIMPLEST-dialect class again -- zero ctx-dynamic-index methods, every
candidate a fixed-offset field read directly off `this`.

`CSTGMS20ModelPatch` (the model-generator/voice-allocator-EG patch
component that OWNS a `CSTGMS20`, not the same class): 19/19, zero
outliers, also zero ctx-dynamic-index methods.

**New field-shape this batch**: shift-then-mask bitfield extraction --
`movzx eax,[base+K]; shr al,N; and eax,1` (N > 0), modeled directly as
`(*(unsigned char *)(base+K) >> N) & 0x1`. This extends the family's
existing MASK-ONLY bitfield shape (no shift instruction at all, e.g.
CPianoOsc's `& 0x3` two-bit field) with an explicit shift for bit
positions other than 0. Confirmed on two separate classes this batch:
`CSTGVPMOsc` packs FOUR independent single-bit booleans into byte 0x1f
(`GetOscOnOff` bit 0 -- no shift instruction, `GetUseCommonPitchMod` bit
1, `GetWaveshaperDriveKeySlopeHighOnly` bit 2, `GetFeedbackPrePost` bit
3); `CSTGMS20ModelPatch` packs two into offset 0x6f7 (`GetMGKeySync` bit
0, `GetMGMIDITempoSync` bit 1). No new helper needed -- same `CtxIndex`
helper still covers the (unrelated) ctx-index cases, this is purely a
field-read-expression change.

Reused the exact same KAT-generation discipline (separate Python
evaluator over the same parsed shape facts, deterministic
`buf[i]=(i*0x9f+0x37)&0xff` pattern, ctx index fixed at 3) plus BOTH
established post-generation checks -- comment open/close-count balance
and an exact DEF_RE captured-name-set diff (`got == want`, not just a
count match) -- run on every new `.cpp` BEFORE any build attempt. Caught
3 fresh instances of the literal-`*/`-in-prose gotcha this batch, a new
variant not seen before: plain "Get*/Set*" prose (used in all 3 new
headers' derivation comments to describe the pending-symbol split) forms
a literal `*/` all on its own, no adjacent word pairing needed like the
prior batches' "FilterA*/FilterB*"/"Tine*/Reed*" examples -- ANY
`X*/Y*`-shaped prose is now a known trigger, not just two similarly-named
fields. Fixed by rewording to "Get*- and Set*-prefixed" throughout (also
fixes the same pattern for future headers describing a Get*/Set* split).
Also caught 2 fresh instances of the DEF_RE parenthesis-swallowing bug in
2 of the 3 new `.cpp` leading comments (a "13 of 36 pending candidates
(the other 23 are ...)." aside and a "(see header for the ... detail)."
aside, both with no semicolon before the next real function) -- fixed by
rewriting both as em-dash-delimited clauses, same established convention.

**Next targets** (same technique, not yet done): ~157 more classes
remain. Re-run the survey query (group pending `Set*`/`Get*` methods by
class from `manifest/oa_functions.csv`, sort by count) -- the next
largest fresh candidates as of this batch, by raw pending count, were
`CKGModuleParamMsgHandler`/`CKGCommonParamMsgHandler`/
`CKGGlobalParamMsgHandler` (130/71/25 -- NOTE: unconfirmed whether these
are actually part of THIS family or a different CKG message-handler
convention; check the `ER23CSTGPatchMessageContext` mangled-suffix filter
and a sample disassembly before assuming), `CSTGPolysixModelPatch` (62),
`CSTGOrganOsc`/`CSTGMS20ModelPatch` already done this batch,
`CSTGPianoModelPatch` (34, confirmed fresh via grep -- only appears in an
unrelated stub-file comment, no real struct), `CSTGMultisampleBank` (33
-- SKIP, already a real heavily-referenced class per `CPianoOsc`'s own
`GetBankIdAndStereoFlag` outlier note, same "already modeled" situation
as `CSTGProgramSlot`/`CSTGProgram`), `CWaveMotionOsc` (33, confirmed
fresh), `CSTGControllerInfo` (31), `CSTGVectorMotion` (29),
`CSTGMultiFilter2Pole` (29). Always do the word-boundary grep +
weak-linkage + ctx-only-suffix `nm` filter BEFORE running the decoder on
any of these, per the by-now-standard checklist.

See [[ckg_bankmanager_class_facts]]/[[ckg_seq_backup_technique]] for the
sibling family this one's decoder was adapted from, and
`HARDWARE_REVIEW_LOG.md`'s "CSTGString value-getter family",
"CSTGOrganModelPatch + CSTGMS20 value-getter families",
"CSTGAnalog4PoleBase + CSTGPolysix + CSTGAnalogSyncOsc value-getter
families", "CPianoOsc + CSTGEPModelPatch value-getter families" and
"CSTGOrganOsc + CSTGVPMOsc + CSTGMS20ModelPatch value-getter families"
entries for the full per-batch derivation notes.
