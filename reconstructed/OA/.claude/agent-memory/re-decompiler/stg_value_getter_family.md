---
name: stg-value-getter-family
description: OA.ko's largest known dense accessor family (~2300 pending methods, ~180 STG synth classes) -- STGConvertedParam &Get*(some MessageContext-family type&) "value getter" convention; 57 classes done (batch 1-13 list: CSTGString, CSTGOrganModelPatch, CSTGMS20, CSTGAnalog4PoleBase, CSTGPolysix, CSTGAnalogSyncOsc, CPianoOsc, CSTGEPModelPatch, CSTGOrganOsc, CSTGVPMOsc, CSTGMS20ModelPatch, CSTGPolysixModelPatch, CWaveMotionOsc, CSTGPianoModelPatch, CSTGMultiFilter2Pole, CSTGMS20EG, CSTGPolysixMG, CSTGAMSMixerBase, CSTGStepSeq, CSTGPitchMod, CSTGSimple2Pole, CSTGVPMModelPatch, CSTGVPMTG92Osc, CSTGEG, CSTGPanOutputBase, CSTGPianoLPF, CSTGAmp, CSTG3BandEQBase, CSTGEGBase, CSTGVPMOutputMixer, CSTGKeyTrack, CSTGPortamentoBase, CSTGDriver, CSTGVPMNoise, CSTGAnalog4Pole, CSTGPluckedModelPatch, CSTGMOSSAmp, CSTGPitchModOsc; batch 14 adds CSTGSimpleAMSMixer, CSTGPitchModCommon, CSTGPitchModCommonPlusAMS, CSTGVPMEG; batch 15 adds CSTGPitchBase, CSTGVPMMixer, CSTGVPMAudioInput, CSTGStringTrackCommon; batch 16 adds CSTGVPMFilter, CSTGPitchModOscBase, CSTGTG92Osc, CSTGPitchModBase; batch 17 adds CSTGVPMPitchModTG92Osc, CSTGTG01Filter, CSTGStringTrack; batch 18 adds CSTGCombi, CSTGCommonEffectLFO, CSTGCommonLFO, CSTGEffectBalance, CSTGEffectRack, CSTGMetronomeSettings, CSTGToneAdjust), 1103 methods reconstructed, manifest 1441->2526. CSTGPCMModelPatch DEFINITIVELY confirmed NOT part of family (batch 15 root-caused the batch-9-vs-14 contradiction). CSTGPanOutput (distinct from already-modeled CSTGPanOutputBase) also confirmed NOT part of family (batch 16). The old whole-binary weak-linkage/`ER23CSTGPatchMessageContext`-suffix-only sweep was confirmed EXHAUSTED at batch 17 (every one of its 60 visible classes accounted for). **Batch 18 developed and validated a broader discovery method -- a global `objdump -dr` cross-reference of every relocation targeting `CSTGParamsOwner::sValueGetterTemp`, grouped by enclosing function -- which finds real value-getters regardless of linkage (weak OR strong) or context parameter type (the family is NOT exclusively `CSTGPatchMessageContext&`; siblings `CSTGMessageContext&`, `CSTGProgramMessageContext&`, `CSTGToneAdjustMessageContext&` and others also participate). This surfaced 75 classes total (vs 60 via the old sweep) and found 7 genuinely new ones this batch alone; see the batch-18 entry below for the exact recipe. Batch 19 added CSTGHDRTrack, CSTGWaveSequence (49 methods) via a per-context-type follow-up sweep, manifest 2526->2575. **Batch 20 (2026-07-28) reconfirmed BOTH discovery methods -- the sValueGetterTemp sweep AND the per-context-type sweep -- are now fully exhausted (identical 75-class list, no 9th context type exists)**, added CSTGTG92OscBase as a deliberate 1/10 partial class (GetFreqOffset only; the other 9 re-confirmed genuinely pure-virtual via a fresh vtable relocation dump plus a whole-binary search for a concrete override, found none), manifest 2575->2576. CSTGDrumKitData (30-31 candidates, entangled 3D piecewise blob index) is now THE ONLY remaining open item under either established discovery method -- see the batch-20 entry below.
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

**Sixth batch (2026-07-28, commit `8c97b38`): `CSTGPolysixModelPatch`
(48/48) + `CWaveMotionOsc` (23/23) + `CSTGPianoModelPatch` (16/18, plus
2 real accessor helpers) done**, manifest 2113 -> 2202. Same ground
truth binary (`/home/share/Decomp/OA.ko_Decomp/OA.ko`). All three picked
from the fifth batch's own backlog by size after the standard checklist.
`CSTGControllerInfo`/`CSTGVectorMotion` were also on that backlog by raw
pending-method count but correctly SKIPPED this batch too -- both
already have real structs/ctors in `oa_global.h`/`program_ctor.cpp`
respectively, same `CSTGProgramSlot`/`CSTGProgram` already-modeled
precedent, confirmed via the standard word-boundary grep before
starting rather than trusting the raw count alone.

`CSTGPolysixModelPatch` and `CWaveMotionOsc`: zero-outlier, all-fixed-K
dialects, no ctx-dynamic-index methods in either. `CSTGPolysixModelPatch`'s
`GetArpeggiator*` group packs FOUR independent single-bit booleans into
one byte at `+0x4ac` -- Enable bit 0 no shift, KeySync bit 1, MIDITempoSync
bit 2, Latch bit 3, all via `shr al,N` + `and eax,1` -- one more packed
bit than any prior class's own bitfield shape (CSTGVPMOsc/CSTGMS20ModelPatch
each had at most 2 bits in one byte).

`CSTGPianoModelPatch` is this batch's new-shape class -- NOT the same
class as `CPianoOsc`, confirmed distinct via grep, this is the
higher-level acoustic-piano PATCH component that owns a `CPianoOsc`.
Its 8-method SustainPedalDown-/SustainPedalUp-prefixed ctx-indexed group
derives its base pointer NOT from `this` directly but from a
virtual-dispatch call through `this`'s own vtable, ground truth slots
`0x170`/`0x174`. Decompiling both vtable targets directly --
`AccessSustainPedalDownVelocityZones`/`AccessSustainPedalUpVelocityZones`,
both real weak pending 4-byte symbols in the manifest -- showed they are
trivial constant-offset accessors: `lea eax,[eax+0x14]; ret` and
`lea eax,[eax+0x78]; ret`. Both were reconstructed as real member
functions and called directly from the Get* bodies, rather than treating
the whole ctx-indexed group as an outlier. This is a genuinely NEW shape
for the family: a virtual-call-mediated sub-object base pointer whose
target turns out to be mechanically trivial once decompiled --
distinguish this from CPianoOsc's own BankSelect outlier, where the
delegate target (`CSTGMultisampleBankUUIDAndStereoFlag::GetBankIdAndStereoFlag`,
348 bytes) is genuinely non-trivial and remains unreconstructed. This
batch's own 2 excluded outliers, `GetSustainPedalDownMultisampleBank`/
`GetSustainPedalUpMultisampleBank`, hit that exact same still-open
dependency via the same ctx-index arithmetic. `this+0x78 - this+0x14 =
0x64 = 4 * stride(25)` confirmed each velocity-zone array holds exactly
4 records -- a useful independent cross-check when a virtual-call base
pointer is involved: diff the two known base offsets and check it's a
whole multiple of the ctx-index stride.

**Rule of thumb going forward for THIS shape**: when a ctx-indexed
Get*/Set* candidate's disassembly shows `mov edx,[eax]` (loading `this`'s
OWN vtable pointer, not a param) immediately followed by a `call
[edx+K]` BEFORE the usual stride-multiply-and-field-load sequence, don't
default to treating it as an outlier -- first decompile the vtable
target directly (`nm`/`objdump` on the class's own weak symbol list will
usually name it `AccessXxx`/`GetXxxPtr`-shaped). If it's a plain `lea
eax,[eax+K]; ret` or equivalently trivial, inline it as a real,
separately-reconstructed accessor method and proceed with the normal
ctx-index decode against ITS return value as the new base pointer. Only
fall back to excluding the whole group as a genuine outlier if the
vtable target turns out to be non-trivial once actually decompiled --
don't guess from the call site alone.

**New ctx-index field-width variant**: `CSTGPianoModelPatch`'s own
ctx-index field is read as a BYTE -- `movzx ebx, BYTE [ctx+0x4]` --
not the family's usual DWORD read (`mov edx,[edx+0x4]`). Same
conceptual "ctx's own dynamic index" field at the same `+0x4` offset,
just a narrower load -- modeled via a new `CtxIndexByte(ctx, off,
stride)` helper alongside the existing `CtxIndex`, same stride-25
arithmetic as CPianoOsc's own chained-double-`lea` case, just the
initial field read narrowed to `unsigned char` instead of `int`. KAT
convention unaffected -- still set the whole `int` at `ctx+0x4` to 3,
since only the low byte is read.

**New DEF_RE gotcha variant, genuinely NEW class of case**: found a
parenthesis-swallow bug in a `.h` file this batch, previously believed
largely immune since declarations end in `;`. The trigger was a plain
in-comment mention like `` `GetNumSustainPedalVelocityZones()` `` with
nothing meaningful following it in the same sentence -- the runaway
match crossed the `*/` comment-close boundary and reached the file's
own real, immediately-following INLINE FUNCTION DEFINITION
(`CtxIndexByte`, a `static inline` helper with a real `{...}` body right
in the header), whose own `{` closed the match and mis-attributed its
captured name to the comment mention instead of `CtxIndexByte`. Lesson:
`.h`-file immunity from this bug only holds when EVERY real construct
after the trigger paren is a plain `;`-terminated declaration -- it does
NOT hold once a real inline function definition with a `{...}` body
follows, which is common in this family's headers now that `CtxIndex`/
`CtxIndexByte` helper functions live there. Also hit 2 more of the
already-well-known `.cpp`-leading-comment variant this batch (one
DEF_RE-parenthesis, one literal-`*/`), same fixes as always -- see the
running gotcha list above. Fixed all 3 by rewriting to zero literal `(`
characters in the affected spans, the by-now-standard convention;
verified via the same 2-check discipline -- `/*`/`*/` count balance AND
an exact DEF_RE captured-name-set diff, run on every new file before
ever attempting to build.

**Next targets** (same technique, not yet done): ~154 more classes
remain. Re-run the survey query -- next largest fresh candidates as of
this batch: `CKGModuleParamMsgHandler`/`CKGCommonParamMsgHandler`/
`CKGGlobalParamMsgHandler` (130/71/25, still UNCONFIRMED whether part of
this family or a different CKG message-handler convention -- check
the `ER23CSTGPatchMessageContext` mangled-suffix filter and a sample
disassembly before assuming, carried over unconfirmed from the fifth
batch), `CSTGDrumKitData` (44), `CSTGWaveSequence` (39), `CSTGMultisampleBank`
(33 -- SKIP, already modeled, same precedent as CSTGProgramSlot/CSTGProgram/
CSTGControllerInfo/CSTGVectorMotion), `CSTGVPMModelPatch` (29, confirmed
fresh via grep this batch's own survey), `CSTGMultiFilter2Pole` (29,
confirmed fresh), `CMOSSAlgorithm` (29), `CSTGProgramModeDrumTrackSlot`
(28, check for already-modeled status first, same family as
CSTGProgramSlot), `CSTGPolysixMG` (28), `CSPRSeqDataManager` (28),
`CSTGMS20EG` (27). Always do the word-boundary grep + weak-linkage +
ctx-only-suffix `nm` filter BEFORE running the decoder on any of these.

**Seventh batch (2026-07-28, this batch): `CSTGMultiFilter2Pole` (23/23)
+ `CSTGMS20EG` (20/20) + `CSTGPolysixMG` (18/18) done**, manifest
2202 -> 2263, 61 methods. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`). All three picked from the
sixth batch's own backlog; standard checklist run first --
word-boundary `grep -rn '\bClassName\b' src include` (all three zero
hits, genuinely fresh) and the `nm`-derived weak-linkage +
`ER23CSTGPatchMessageContext`-suffix candidate-set filter, which this
batch reconfirmed is essential even when a class's raw pending-method
count looks promising: `CSPRSeqDataManager` (28 pending, next on the
prior batch's list by size) turned out to be ENTIRELY global ('T')
linkage with non-ctx signatures once actually queried (`GetSongSize`,
`GetTrackSize`, etc, all real sequencer-data-area accessors, zero
overlap with this family's own convention) -- correctly skipped without
writing a single file, in favor of the next three genuinely-fresh
candidates on the list.

All three classes turned out FULLY clean or near-clean: `CSTGMS20EG`
and `CSTGPolysixMG` are zero-outlier, zero-ctx-index, all-fixed-K
dialects (matching CSTGEPModelPatch/CSTGPolysixModelPatch's own
simplest-dialect precedent); `CSTGMultiFilter2Pole` has a real
ctx-index sub-family but also zero outliers -- the third batch in a row
with zero excluded outliers across all its classes.

**New ctx-index shape**: `CSTGMultiFilter2Pole`'s
`GetLFOIntensity`/`GetLFOJSminusYIntensity` use `mov edx,[edx+0x4]`
with NO `lea` premultiply at all, going straight into a SIB-scaled
field load `[eax+edx*4+K]` -- effective stride 4. This is genuinely new:
every prior ctx-index shape in the family either used the stride-5 `lea`
premultiply first (with or without an extra SIB scale on top) or, in
CSTGMS20's own `GetInputJack` case, a bare stride-1 load with no SIB
scale either. This is the first confirmed case of a raw per-call index
used directly as a plain array-of-dwords subscript with no slot-record
premultiply involved at all -- i.e. `CtxIndex(ctx, 0x4, 4)` with the
stride passed straight through, same helper, no code change needed.
The class's other ctx-index pair, `GetLFOAMSSource`/`GetLFOAMSIntensity`,
uses the by-now-familiar stride-5 `lea` premultiply with a bare `[+edx*1+K]`
load on top (effective stride 5) -- both shapes coexist in the same class.

**New plain-field width/sign variant**: `CSTGPolysixMG::GetMIDITempoSyncTimes`
is an UNSIGNED byte field (`movzx eax, BYTE [this+K]`, no shift, no mask)
rather than the family's near-universal signed `movsx` byte read on a
plain non-bitfield field -- still single-write, just zero-extended
instead of sign-extended. First confirmed case of this on an ordinary
(non-bitfield) byte field; distinguish carefully from the shift-then-mask
bitfield shape (also `movzx`-based) by checking whether a `shr`/`and`
follows the load -- here there is none, it's the field's own full raw
byte value.

**`CSTGMS20EG`'s own naming quirk, NOT a new shape**: each of its 4
EG-time parameters carries not one but a full 5-method AMS sibling
group -- base value, AMSSource, AMSIntensity, AND a second-level
AMSIntensityAMSSource/AMSIntensityAMSIntensity pair -- suggesting a
possible double-modulation indirection from the naming alone. Confirmed
via direct disassembly that this is NOT real: every one of the 20
fields, including the "second-level" ones, resolves to a single plain
fixed offset off `this`, no extra indirection or nested ctx-index of any
kind. A reminder to always verify field-shape from the actual
disassembly rather than inferring structure from a method's name, even
when the name strongly implies nesting.

**New DEF_RE trigger variant, distinct from every prior one**: a bare
capitalized word immediately followed by a parenthetical aside --
`UNSIGNED (movzx, no shift/mask)` in `stg_polysix_mg_valuegetters.cpp`'s
own leading comment, and independently `field (GetFrequency, GetDelay,
...)` in `oa_stg_polysix_mg.h`'s own prose -- both matched DEF_RE's own
captured-name group on the bare word (`UNSIGNED`, `field`) treating the
parenthetical as a fake parameter list, then ran to the file's own first
real `{` (the `.cpp`'s first function body; the `.h`'s own `struct { `
opening brace) exactly like every prior DEF_RE gotcha instance. This is
the FIRST confirmed case of a `.h` file's runaway match reaching a
`struct Name {` opening brace specifically (as opposed to reaching a
real inline function definition's own `{`, the `CtxIndexByte` case from
batch six) -- reconfirms `.h`-file "immunity" only holds when the struct
body itself has no `{` before the trigger resolves, which is basically
never true once the struct declaration follows in the same file. Both
fixed the same way as always: reworded to remove the literal `(`
immediately after the trigger word (em-dash-delimited clause instead).
Caught via the same 2-check discipline -- comment open/close-count
balance (both files passed this one cleanly, the bug is orthogonal to
unbalanced `/*`/`*/`) AND the exact DEF_RE captured-name-set diff --
run on every new file before ever attempting to build, as always.
Lesson reinforced: run the DEF_RE check on `.h` files too, every batch,
not just `.cpp` files, regardless of how declaration-heavy the header
looks -- a single trigger word anywhere before the struct's own `{` is
enough.

**Eighth batch (2026-07-28, commit `f7f7e5f`): `CSTGAMSMixerBase` (17/19) +
`CSTGStepSeq` (14/14) + `CSTGPitchMod` (12/12) done**, manifest
2263 -> 2306, 43 methods. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`). First act this batch: FINALLY
resolved the `CKGModuleParamMsgHandler`/`CKGCommonParamMsgHandler`/
`CKGGlobalParamMsgHandler` question carried over unconfirmed across
three prior batches -- `nm`-queried all three directly: 130/71/27
methods respectively, ALL global ('T') linkage, ZERO matches against the
`ER23CSTGPatchMessageContext` ctx-only-suffix filter. Their real
signature is `Set*(CKGModuleParamMsg const*)`/`Set*(CKGCommonParamMsg
const*)`/`Set*(CKGGlobalParamMsg*)` -- a different CKG message-dispatch
convention entirely, unrelated to this family despite the superficially
similar "CKG...ParamMsgHandler" naming. Confirmed NOT part of this
family -- remove from all future candidate lists. Also spot-checked and
rejected `CMOSSAlgorithm` (29 pending) this batch: its Get*/Set* methods
are a MOSS DSP-algorithm parameter-descriptor mechanism (`GetParam`/
`SetParam`/`GetMinMax`/`GetDefaultValue`, mixed `__cdecl`/`__regparm1`/
`__regparm3`/`__thiscall`, extra args beyond ctx, zero ctx-only-suffix
matches) -- a different mechanism, not this family. And confirmed
`CSTGDrumKitData`/`CSTGWaveSequence`/`CSTGProgramModeDrumTrackSlot` all
already have real ctors/structs in `oa_global.h` from earlier unrelated
batches (a 17MB raw-blob table, a vtable-only stub, and a real
`CSTGProgramSlot` subclass respectively) -- correctly skipped as
already-modeled, same precedent as `CSTGProgramSlot`/`CSTGProgram`.

All three chosen classes came back clean or near-clean. `CSTGAMSMixerBase`
(STG AMS two-input mixer base) is the simplest dialect yet -- zero
ctx-index methods despite the class's own "mixer" framing -- with 2
outliers, both the familiar `fyl2x` log2-style transform (`GetAttack`/
`GetDecay`, same outlier class as `CSTGString`'s/`CSTGAnalogSyncOsc`'s own
`GetNoiseSaturation`). `CSTGStepSeq` (STG step-sequencer LFO component,
confirmed distinct from the unrelated already-declared `CSTGStepSeqBase`
stub) and `CSTGPitchMod` (STG pitch-modulation component, confirmed
distinct from 5 similarly-prefixed unrelated classes --
`CSTGPitchModBase`/`CSTGPitchModCommon`/`CSTGPitchModCommonPlusAMS`/
`CSTGPitchModOsc`/`CSTGPitchModOscBase`) both came back zero-outlier.
Both had 1-2 pending Get symbols that were global ('T') linkage with a
different signature (`CSTGVoice*` or `(int,int)` args) -- excluded up
front by the standard filter, not fed to the decoder.

**Decoder generalization, not a new field-shape**: the shared scripted
decoder's `load_value` sib-operand branch previously had two separate,
narrower cases (`scaled_index` with `scale==1` only; no `ctxfield`-direct
case at all), which meant the bare-stride-1 and bare-stride-4 shapes
documented in batches 5 and 7 had actually been hand-verified per-class
rather than mechanically decoded end-to-end. Generalized this batch to
one unified rule: `sib` with a `scaled_index` base multiplies the SIB
scale into the existing lea-premultiply stride (covers the family's x1/x2/x4
lea-plus-SIB-scale variants from batches 2-3), and `sib` with a bare
`ctxfield` base uses the SIB scale directly as the stride (covers the
bare x1/x4 no-lea variants from batches 5 and 7). All of `CSTGStepSeq`'s
bare-stride-1 group and `CSTGPitchMod`'s bare-stride-4 pair now decode
through this single formula with no per-class special-casing.

**KAT-evaluator bug found and fixed before shipping, distinct from every
prior gotcha class**: the independent Python KAT oracle's own `eval_fact`
initially treated a dword ('load', width=4) field as signed only when the
decoder's own captured `signed` flag was true -- but the decoder ALWAYS
captures `signed=False` for width-4 loads (there is no signed/unsigned
distinction in a raw `mov eax,[...]` register load), while the C
renderer's `render_expr` unconditionally casts every width-4 load to
plain `int` regardless of that flag, matching the original instruction.
This meant the first draft of all 3 new KATs failed on every dual-write
(dword) field with values differing by exactly 2**32 -- a `got`/`want`
mismatch pattern immediately recognizable as a signed/unsigned encoding
bug, not a real logic error. Fixed by making the evaluator treat every
width-4 load as signed unconditionally, matching the C renderer's own
convention rather than the decoder's internal (irrelevant, at width 4)
signed flag. Re-ran the full 43-check KAT set clean after the fix. Worth
flagging for any future from-scratch KAT generator in this family: the
`signed` field in the decoder's JSON output is only meaningful for
width 1/2 loads, never width 4 -- do not gate a dword evaluator branch on
it.

`make verify`: exit 0, 0 FAIL lines across the whole suite, all 3 new
KATs passing. Real `make ko-clean && make ko KDIR=/home/build/linux-kronos`
Kbuild build: clean link, `OA.ko` produced (488580 bytes), zero warnings
or errors traceable to the 3 new files (confirmed via a build-log grep
scoped to each new filename). `DECOMPILE_ERRORS.md` stays empty -- no
compile/link blocker hit. `manifest/gen_oa_manifest.py` regenerated, OA.ko
manifest 2263->2306/21,689 (10.632%), delta exactly +43 confirmed via a
full before/after reconstructed-name-set diff (0 regressions).

**Next targets** (same technique, not yet done): ~148 more classes
remain. `CKGModuleParamMsgHandler`/`CKGCommonParamMsgHandler`/
`CKGGlobalParamMsgHandler` and `CMOSSAlgorithm` both now CONFIRMED NOT
part of this family -- do not re-add to future candidate lists.
`CSTGDrumKitData`/`CSTGWaveSequence`/`CSTGProgramModeDrumTrackSlot`
confirmed already-modeled -- also skip. Re-run the survey query (group
pending `Set*`/`Get*` methods by class from `manifest/oa_functions.csv`,
sort by count) for the next batch's candidates, still always doing the
word-boundary grep + weak-linkage + ctx-only-suffix `nm` filter BEFORE
running the decoder on any of them -- as of this batch, the next
promising untried candidates by size were `CSTGControllerInfo` (31,
SKIP -- already confirmed modeled in batch 6) and `CSTGVectorMotion` (29,
SKIP -- already confirmed modeled in batch 6); genuinely fresh candidates
still open below those in size include `CSTGVPMModelPatch` (10 weak
ctx-only candidates, confirmed fresh this batch's own survey but not
picked -- smaller than the 3 chosen), `CSTGPluckedModelPatch` (6 weak
ctx-only candidates, confirmed fresh), plus the broader unswept tail of
the pending-count list below `CSTGPatch`/`CSTGCommonLFO`/`CKGBankManager`/
`CSTGEffectRack` (all 4 confirmed to have existing hits/already-modeled
status this batch, re-verify before reusing that verdict if much time
has passed).

**Ninth batch (2026-07-28, commit `d8479c1`): `CSTGSimple2Pole` (13/13) +
`CSTGVPMModelPatch` (10/10) + `CSTGVPMTG92Osc` (9/9) done**, manifest
2306 -> 2338, 32 methods. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`). All three picked from the
prior batch's own "148 more classes remain" backlog via a fresh survey
(group pending `Set*`/`Get*` by class, filter to word-boundary-fresh
classes, then the `nm`-derived weak-linkage + ctx-only-suffix filter).
Confirmed `CSTGPCMModelPatch` (7 raw pending) is NOT part of this
family despite a promising raw count -- `nm` shows only 2 real symbols,
both global ('T') linkage with extra args
(`GetMultisampleIds(unsigned char, unsigned char, bool&, unsigned
int&, unsigned int&)`, `SetupComponentOffsets()`), zero ctx-only-suffix
matches -- a different mechanism, correctly skipped without writing a
file. `CRPPRManager`/`CSPRRecDataMerger`/`CSPRAudioPlayer`/
`CSPRSongControl` (the CSPR/CRPPR-prefixed recorder/player classes on
the same backlog) were NOT investigated this batch -- likely the same
different-mechanism situation as the already-rejected
`CSPRSeqDataManager` from batch 7, but unconfirmed, check before
assuming either way.

All three chosen classes came back fully clean -- zero outliers across
the whole batch, third batch in a row with zero exclusions.
`CSTGSimple2Pole` and `CSTGVPMTG92Osc` are both the family's
by-now-familiar simplest dialect: every candidate a fixed-K field read
directly off `this`, zero ctx-dynamic-index methods despite
`CSTGSimple2Pole`'s own `FreqAMS1IntensityAMSSource`/
`FreqAMS1IntensityAMSIntensity` naming implying a second modulation
level -- reconfirmed via direct disassembly to be plain fixed offsets,
same lesson as `CSTGMS20EG`'s own naming quirk from batch 7. Always
verify field-shape from the actual disassembly, never infer nesting
depth from a method's name alone.

**Genuinely new ctx-index shape, first of its kind in the family**:
`CSTGVPMModelPatch::GetInterMixerLink`/`GetOscMacroClass` load ctx's
own `+0x4` dynamic-index field not as an array/record index at all, but
as a variable SHIFT COUNT: `mov ecx,[edx+0x4]; movzx eax,BYTE
[eax+K]; sar eax,cl; and eax,0x1` -- selecting which single bit of one
fixed byte field to extract, per-call. Every prior ctx-index shape in
the family (CSTGString's stride-5, CSTGMS20's SIB-scale variants,
CPianoOsc's chained double-lea, CSTGMultiFilter2Pole's bare SIB-scale)
computed a per-record BASE OFFSET from the index; this one uses the
index as a BIT POSITION within a single already-fixed field instead.
Since the byte operand is loaded via `movzx` (top 24 bits always zero),
`sar` and a logical `shr` are equivalent here -- modeled as a plain
unsigned right-shift. x86 masks a 32-bit shift count to 5 bits, so the
new helper applies that mask explicitly: `CtxShift(ctx, off)` returns
`*(unsigned int*)(&ctx+off) & 0x1f`, used as `(field >> CtxShift(ctx,
0x4)) & 0x1`. Recognize this shape by `mov ecx,[ctx_reg+K]` feeding
directly into `sar`/`shr` on the SAME register that holds a byte field
value, rather than into an address computation (`lea`) or an SIB index
slot -- the destination of the ctx-derived value is a shift amount, not
a memory operand.

**New plain-field width variant**: `CSTGVPMModelPatch::GetAlgorithm` is
a plain UNSIGNED byte field (`movzx`, no shift/mask, not ctx-indexed)
-- single-write only, same "unsigned non-bitfield byte" variant first
confirmed on `CSTGPolysixMG::GetMIDITempoSyncTimes` in batch 7, now
seen on a second class.

**Tooling note, DEF_RE gotcha hit and fixed before shipping**:
`oa_stg_vpm_model_patch.h`'s first-draft leading comment had "the VPM
(FM/ring-mod/waveshaper) engine's" -- the bare word `VPM` immediately
before a space then `(` matched `DEF_RE`'s NAME group, and because nothing
in the rest of the header before the real `CtxIndex`/`CtxShift`
function definitions contains a literal `;` `{` or `}`, the greedy
`[^;{}]*` capture swallowed everything up to `CtxIndex`'s own closing
`)` and real `{` -- exactly the established parenthesis-swallow
pattern, this time with a SPACE between the trigger word and `(`
(`DEF_RE`'s `\s*` allows this, so "word (" is just as dangerous as
"word("). Caught via the standard exact-name-set diff (`got=={"VPM",
"CtxShift"}` instead of the wanted `{"CtxIndex","CtxShift"}`) before
ever attempting to build; fixed by rewording to "VPM engine's --
FM/ring-mod/waveshaper synthesis -- macro parameters" and removing
every other bare-word-then-paren instance in the same header's prose
(`COUNT (`sar eax,cl`)`, `field (movzx, no shift/mask)`, `movzx (top 24
bits...)` all reworded the same way, em-dash-delimited, zero literal
`(` before the real code). Lesson reinforced: check for `word\s*\(`
(with the optional-whitespace variant, not just `word(`) as the search
pattern when auditing a new header for this bug, not just a bare
substring search for `(`.

**Next targets** (same technique, not yet done): ~145 more classes
remain. Fresh, weak-ctx-only-confirmed candidates smaller than this
batch's three, not yet picked: `CSTGEG` (10 weak ctx-only candidates of
14 total pending, confirmed fresh via word-boundary grep),
`CSTGPanOutputBase` (9 of 11), `CSTGPianoLPF` (9 of 11), `CSTGAmp` (7 of
10), `CSTG3BandEQBase` (6 of 6). `CSTGEGBase` (19 raw pending but only 5
weak-ctx-only -- check disassembly before committing, may be mostly a
different mechanism like `CSTGOrganOsc`'s own per-voice-runtime-state
false positives from batch 5). `CSTGFrontPanelSmoothers`/
`CSTGCommonStepSeq`/`CSTGAudioInput`/`CSTGHDRTrack`/`CSTGHDRMiniModel`/
`CSTGProgramModeProgramSlot` all showed nonzero word-boundary grep hits
this batch's survey -- NOT yet individually verified as already-modeled
or just incidental references, check each with the standard grep before
assuming either way. `CRPPRManager`/`CSPRRecDataMerger`/
`CSPRAudioPlayer`/`CSPRSongControl` also unconfirmed, likely a different
CSPR/recorder-subsystem mechanism per this batch's own
`CSTGPCMModelPatch`/`CSPRSeqDataManager` precedents -- check via `nm`
before assuming. Always do the word-boundary grep + weak-linkage +
ctx-only-suffix `nm` filter BEFORE running the decoder on any of these,
per the by-now-standard checklist.

**Tenth batch (2026-07-28, commit `cc23e63`): `CSTGEG` (10/10) +
`CSTGPanOutputBase` (9/9) + `CSTGPianoLPF` (9/9) done**, manifest
2338 -> 2366, 28 methods. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`). All three picked from the
ninth batch's own priority list; standard checklist run first --
word-boundary `grep -rln <ClassName> src include` (all three zero hits,
genuinely fresh) and the `nm`-derived weak-linkage +
`ER23CSTGPatchMessageContext`-suffix filter, which correctly excluded 4
extra-arg CSTGEG symbols (`GetAMSTimeModSource(unsigned char)` etc, real
weak symbols but with an explicit slot-index arg instead of the
family's plain ctx-only signature), `CSTGPanOutputBase`'s
`GetVoiceLevelEstimate(CSTGVoice const&)` (same per-voice-runtime-state
false-positive shape as `CSTGOrganOsc`'s own precedent, confirmed via
direct disassembly rather than trusting linkage alone) and `SetMute`
(global linkage, extra bool arg), and `CSTGPianoLPF`'s
`GetSubComponent(unsigned short)` (the by-now-familiar sub-object-accessor
outlier) and `SetupComponentOffsets` (global linkage, extra args).

All three classes came back fully clean -- third batch in a row (ninth,
now tenth) with zero excluded outliers among the real ctx-only
candidates. `CSTGEG` (general-purpose envelope-generator patch
component) has a real ctx-index sub-family on its Intensity methods
using the by-now-established bare stride-4 SIB-scale shape (no lea
premultiply, first seen on `CSTGMultiFilter2Pole`); its Source siblings
are plain fixed bytes despite matching AMS-modulation naming.
`CSTGPianoLPF` (acoustic-piano lowpass-filter patch component, distinct
from `CPianoOsc`) is zero-ctx-index, all-fixed-K -- its
`FreqAMS1IntensityAMSSource`/`FreqAMS1IntensityAMSIntensity` naming
implies a second modulation level exactly like `CSTGMS20EG`'s and
`CSTGSimple2Pole`'s own earlier false alarms, reconfirmed via
disassembly to be plain fixed offsets, no real nesting.

**Genuinely new field-shape, first of its kind: hardcoded-constant
getter.** `CSTGPanOutputBase::GetPatchSolo` never dereferences `this`
at all -- its entire body is `mov DWORD PTR
ds:sValueGetterTemp+0x0,0x0` followed by loading `&sValueGetterTemp`
into eax and `ret`. No field read, single-write only (`.value = 0`, no
`.displayValue`), the field-shape decoder now needs to recognize a pure
literal-immediate store as its own case rather than assuming every
candidate reads some `this`-relative memory. Modeled directly as
`CSTGParamsOwner::sValueGetterTemp.value = 0;` with no `base` variable
at all in the generated body. Likely reflects a not-yet-wired-up
patch-level solo feature (solo tracked elsewhere, e.g. per-program) --
distinguish this from a genuine outlier: it IS mechanically decodable,
just with an even simpler-than-usual body, so it belongs in the decoded
set, not the excluded set.

**Tooling note, reconfirmed and extended this batch**: BOTH known DEF_RE
parenthesis-swallow triggers this time were in prose describing OTHER
real function signatures/shapes rather than a bare English word --
`oa_stg_eg.h`'s own "GetAMSTimeModSource(unsigned char)" (a literal,
accurate C-like mention of the excluded overload's real signature) and
`stg_eg_valuegetters.cpp`'s own "shape (no lea premultiply)" -- both hit
the same `word\s*\(` pattern as prior batches, this time the trigger
words were themselves meaningful code-adjacent tokens rather than
incidental prose, which made them easy to overlook at first draft.
Lesson: literal signature mentions in derivation prose are just as
dangerous as ordinary parenthetical asides -- reword using an em-dash or
just drop the parens (e.g. "carrying an extra explicit slot-index
argument" instead of "GetAMSTimeModSource(unsigned char)"). Caught via
the same 2-check discipline -- comment open/close-count balance (both
passed cleanly, orthogonal bug) and an exact DEF_RE captured-name-set
diff -- run on every new file before ever attempting to build, same as
every prior batch.

**Next targets** (same technique, not yet done): ~142 more classes
remain. `CSTGAmp` (7 of 10 pending, confirmed fresh in batch 9's own
survey, not yet picked -- smaller than this batch's three) and
`CSTG3BandEQBase` (6 of 6, confirmed fresh, zero outliers expected given
its 100% ctx-only-suffix hit rate) are the next smallest known-fresh
candidates. `CSTGEGBase` (19 raw pending but only 5 weak-ctx-only --
still unconfirmed whether the other 14 are a different per-voice
mechanism like `CSTGOrganOsc`'s own false positives, check disassembly
before committing). `CSTGFrontPanelSmoothers`/`CSTGCommonStepSeq`/
`CSTGAudioInput`/`CSTGHDRTrack`/`CSTGHDRMiniModel`/
`CSTGProgramModeProgramSlot` still unconfirmed as of batch 9's survey --
each showed nonzero word-boundary grep hits, not yet individually
verified as already-modeled vs incidental reference. `CRPPRManager`/
`CSPRRecDataMerger`/`CSPRAudioPlayer`/`CSPRSongControl` also still
unconfirmed, likely the same different-mechanism situation as the
already-rejected `CSPRSeqDataManager`/`CSTGPCMModelPatch` precedents --
check via `nm` before assuming either way. Always do the word-boundary
grep + weak-linkage + ctx-only-suffix `nm` filter BEFORE running the
decoder on any of these, per the by-now-standard checklist. Re-run the
full survey query (group pending `Set*`/`Get*` methods by class from
`manifest/oa_functions.csv`, sort by count) once the above small
candidates are exhausted, since the list has not been freshly
regenerated since batch 6.

**Eleventh batch (2026-07-28, commit `ddf923e`): `CSTGAmp` (7/10) +
`CSTG3BandEQBase` (6/6) + `CSTGEGBase` (5/19) done**, manifest
2366 -> 2384, 18 methods. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`). All three picked from the
tenth batch's own priority list; standard checklist run first --
word-boundary `grep -rln <ClassName> src include` (all three zero hits,
genuinely fresh) and the `nm`-derived weak-linkage +
`ER23CSTGPatchMessageContext`-suffix filter, which correctly excluded
`CSTGAmp::GetSubComponent(unsigned short)` (the by-now-familiar
sub-object-accessor outlier) and 2 global-linkage extra-arg methods
(`SetupComponentOffsets`, `SetOutputLevelMultiplier`).

`CSTGEGBase` was this batch's headline check -- the ninth/tenth batches'
own notes flagged it as "19 raw pending but only 5 weak-ctx-only,
unconfirmed whether the other 14 are a different mechanism." Confirmed
via direct `nm` query: the other 14 are ALL global ('T') linkage --
ten are per-voice state-machine transition helpers (attack, decay,
sustain, release, hold, slope, free, quick-release, and a generic
normal-state dispatcher, each taking a `STGEGSubRateParamsSlice*` and a
`CSTGVoice*`) plus a filter-setup helper with the same slice-pointer
shape, three more are extra-arg setters (EG type selector, an explicit
control-value setter, a piano half-damper mode flag setter), and the
last is a two-int accumulator query -- zero overlap with this family's
ctx-only convention, same "T linkage = different mechanism" outcome as
`CSTGOrganOsc`'s own per-voice-runtime-state false positives from batch
5. `CSTGEGBase` also confirmed DISTINCT from the already-modeled
`CSTGEG` (a different, unrelated class despite the similar name, same
`CSTGPolysix`/`CSTGPolysixModel` precedent).

All three classes came back fully clean -- zero outliers among the real
ctx-only candidates, fourth batch in a row with a clean sweep.
`CSTGAmp` (STG amplifier patch component) is a mixed dialect: plain
fixed-K dwords/bytes for Level/VelocityAmount/LevelAMSIntensity/
LevelAMSSource, a bare-stride-4 SIB-scaled ctx-index dword for
LFOAmount, and the stride-5 lea-premultiply ctx-index shape (bare byte
and bare dword loads) for the LFOAmountAMSSource/LFOAmountAMSIntensity
pair. `CSTG3BandEQBase` (STG 3-band parametric EQ patch component base)
is the simplest dialect yet -- a 100% ctx-only-suffix hit rate (6 of 6
raw pending symbols all real), zero ctx-index methods, one mask-only
single-bit bitfield (`GetBypassValue`) and five plain fixed-K dwords.

**New confirmed variant, not a new decoder shape**: `CSTGEGBase::GetCurve`
is the first confirmed case in this family of a ctx-indexed UNSIGNED
byte load -- bare stride-1, no lea premultiply, `movzx` instead of
`movsx` -- combined with ctx-indexing. Every prior unsigned-byte
precedent (`CSTGPolysixMG::GetMIDITempoSyncTimes`,
`CSTGVPMModelPatch::GetAlgorithm`) was a plain fixed-K field, never
ctx-indexed. No decoder change needed -- the shared decoder already
tracks sign/width independently of whether the base offset is ctx-scaled,
this is purely a new confirmed data point, not a new code path.
`CSTGEGBase::GetTime` (same bare stride-1 shape, signed byte) sits right
next to it in the same class, letting both be cross-checked against each
other directly.

**Tooling note, DEF_RE gotcha hit and fixed before shipping**: the
first draft of `oa_stg_amp.h`'s leading comment listed all three
excluded symbols with literal signatures in parens
(`GetSubComponent(unsigned short)`, a `SetupComponentOffsets(...)`
argument list, a `SetOutputLevelMultiplier(...)` argument list)
immediately followed by a second paragraph using several more balanced
parenthetical asides -- with NO literal semicolon anywhere in the whole
span between the first trigger paren and the real `CtxIndex` helper's
own `{`, the greedy DEF_RE capture ran all the way through and matched
`CtxIndex`'s own closing paren as if it belonged to `GetSubComponent`,
swallowing the real `CtxIndex` definition entirely (captured name set
was `{"GetSubComponent"}` instead of the wanted `{"CtxIndex"}`). Caught
via the standard exact DEF_RE captured-name-set diff before ever
attempting to build; fixed by rewriting both paragraphs to remove every
literal signature mention and parenthetical aside, following the
established zero-parens-before-real-code convention. Useful negative
control this batch: `oa_stg_eg_base.h`'s own prose had just as many
`word\s*\(` matches but did NOT trigger the bug, because one incidental
real semicolon in its own prose ("single-write; AMSResetThreshold...")
happened to break the runaway before it reached the real code -- do not
treat an absence of DEF_RE-capture symptoms as proof a file's prose is
safe; a single stray semicolon can mask the same underlying hazard by
accident. Keep authoring with zero literal `(` before real code as the
default, not "check whether it happens to still parse."

**Next targets** (same technique, not yet done): ~139 more classes
remain. `CSTGEGBase`'s other 14 raw pending symbols now CONFIRMED NOT
part of this family -- do not re-add to future candidate lists. Still
unconfirmed from the tenth batch's own carryover:
`CSTGFrontPanelSmoothers`/`CSTGCommonStepSeq`/`CSTGAudioInput`/
`CSTGHDRTrack`/`CSTGHDRMiniModel`/`CSTGProgramModeProgramSlot` (nonzero
word-boundary grep hits, not yet individually verified as already-modeled
vs incidental reference) and `CRPPRManager`/`CSPRRecDataMerger`/
`CSPRAudioPlayer`/`CSPRSongControl` (likely a different CSPR/recorder
mechanism per the `CSPRSeqDataManager`/`CSTGPCMModelPatch` precedents,
unconfirmed). The small known-fresh backlog from batch 10 is now
exhausted -- re-run the full survey query (group pending `Set*`/`Get*`
methods by class from `manifest/oa_functions.csv`, sort by count) fresh
for the next batch's candidates, since the list has not been
regenerated since batch 6. Always do the word-boundary grep +
weak-linkage + ctx-only-suffix `nm` filter BEFORE running the decoder on
any of these, per the by-now-standard checklist.

**Twelfth batch (2026-07-28, commit see HARDWARE_REVIEW_LOG.md's own
batch-12 entry): `CSTGVPMOutputMixer` (7/7) + `CSTGKeyTrack` (7/7) +
`CSTGPortamentoBase` (6/6) done**, manifest 2384 -> 2404, 20 methods.
Same ground truth binary (`/home/share/Decomp/OA.ko_Decomp/OA.ko`).

**Spot-check pass first, per this batch's own mandate**: all 10
carryover "unconfirmed" candidates from batches 9-11's own notes were
resolved before picking anything new. `CSTGFrontPanelSmoothers`/
`CSTGCommonStepSeq`/`CSTGAudioInput`/`CSTGHDRTrack`/`CSTGHDRMiniModel`/
`CSTGProgramModeProgramSlot` all CONFIRMED already-modeled (real grep
hits in `oa_global.h`/`oa_engine_init.h` plus multiple ctor/init call
sites, zero real ctx-only weak candidates via `nm` either) -- same
`CSTGProgramSlot`/`CSTGProgram` precedent. `CRPPRManager`/
`CSPRRecDataMerger`/`CSPRAudioPlayer`/`CSPRSongControl` all CONFIRMED
NOT part of this family -- 100% global ('T') linkage, every method
taking extra args beyond ctx, same different-mechanism outcome as
`CSPRSeqDataManager`/`CSTGPCMModelPatch`. Do not re-add any of these 10
to future candidate lists.

**Survey methodology upgrade**: rather than per-class `nm` greps (which
had an easy-to-hit bug this batch -- a length-prefix miscount, e.g.
`CSTGTG92OscBase` is 15 chars not 17, `CSTGMOSSAmp` is 11 not 12, always
verify with `python3 -c "print(len('ClassName'))"` before building the
`_ZN<len><ClassName>` grep pattern rather than counting by eye), dumped
every real ctx-only weak symbol in the WHOLE binary in one pass (`nm
$KO | grep -E '^[0-9a-f]+ W _ZN[0-9]+.*ER23CSTGPatchMessageContext$'`)
and grouped by mangled length-prefixed class name via a small Python
parser. This gives a complete, authoritative per-class candidate count
across the entire family in one shot -- prefer this over re-deriving
per-class pending counts from `manifest/oa_functions.csv` (which mixes
in non-family Get*/Set* methods and can't distinguish real candidates
from T-linkage false positives without a second pass anyway).

**New outlier class, first of its kind: vtable slot resolves to
`__cxa_pure_virtual` in the candidate's OWN class.** Found on
`CSTGTG92OscBase` (10 raw candidates, picked first this batch then
DROPPED after this finding, zero files written for it). 9 of its 10
candidates (all but `GetFreqOffset`, a plain fixed-K dword) show the
same superficial shape as `CSTGPianoModelPatch`'s own batch-6
virtual-call-mediated sub-object base pointer precedent: `mov edx,[eax]`
(this's own vtable pointer) then `call [edx+K]` BEFORE the usual
stride-multiply-and-field-load sequence. Per that batch's own rule of
thumb, decompiled the vtable target directly rather than guessing from
the call site -- but this time the raw `.rodata._ZTV15CSTGTG92OscBase`
relocation at that slot is `__cxa_pure_virtual`, not a concrete
accessor. Verified the raw-offset math (vptr = vtable section base + 8,
so `call [edx+0xd4]` -> raw section offset 0xdc) by cross-checking two
already-known concrete symbols at nearby raw offsets in the SAME vtable
(`GetRestrikeLimitForNote` at raw 0xc8, `GetRequiredVoiceInfo` at raw
0xcc, both real T-linkage members of this exact class) before trusting
the pure-virtual read -- a useful general technique when validating
vtable-pointer-offset assumptions: find two already-known real symbols
in the same vtable blob and confirm their raw-offset-to-call-offset
relationship first. A pure-virtual slot in the candidate's OWN vtable
means the class is genuinely abstract at that method -- real behavior
depends on which concrete subclass overrides it, undeterminable
statically from this class's own disassembly. Correctly treated as a
class-level Tier-B scope deferral (would need the concrete subclass
identified and ITS vtable's slot 0xd4 target decompiled instead), NOT a
per-method outlier to exclude and move on from -- the whole class was
dropped this batch in favor of three smaller clean classes. **Rule of
thumb, extends batch 6's own**: after finding a virtual-call-mediated
base pointer, always check whether the target resolves to
`__cxa_pure_virtual` before assuming it's safely inlineable like
`CSTGPianoModelPatch`'s case -- an identical-looking call site can
resolve to either a trivial concrete accessor or a genuinely abstract
slot, only the actual relocation target distinguishes them.

All three finally-picked classes came back fully clean -- zero outliers,
matching batches 9-11's own clean-sweep streak (the dropped
`CSTGTG92OscBase` doesn't break the streak since no file was ever
written for it). `CSTGKeyTrack` (STG key-tracking/keyboard-scaling
patch component) is the simplest dialect yet -- 7 plain fixed-K byte
fields (3 unsigned key positions, 4 signed ramps), zero ctx-index.
`CSTGPortamentoBase` (STG pitch-glide patch component) packs three
single-bit booleans (Enabled/Fingered/ConstantTime) into one byte at
+0x1d via the established shift-then-mask bitfield shape, plus plain
fixed-K Time/AMSSource/AMSIntensity fields, zero ctx-index.

**New confirmed ctx-index premultiply factor: x9.** `CSTGVPMOutputMixer`
(VPM engine per-operator output mixer) uses `lea edx,[edx+edx*8]` to
premultiply ctx's dynamic-index field by 9 -- every prior lea-premultiply
shape in the family used factor 5. Combined with an explicit x2 SIB
scale on the field load (`[eax+edx*2+K]`), effective stride is 18 --
decoded via the existing batch-8 "SIB scale multiplies into the existing
premultiply stride" generalized rule, no decoder code change needed,
purely a new confirmed stride value. `CSTGVPMOutputMixer::GetPhaseInvert`
also reuses the established `CtxShift` single-bit-boolean shape off a
fixed byte field at +0x78.

**Tooling: 2 fresh `DEF_RE` parenthesis-swallow instances in ONE file**
(`oa_stg_vpm_output_mixer.h`) -- fixing the first trigger ("a new
confirmed stride value (18, distinct from ... variants)") did not fully
clear the file; a SECOND, independent trigger further down ("Field-shape
summary (record base = CtxIndex(ctx, 0x4, 18)):") still had zero
semicolons before it and the real `CtxIndex`/`CtxShift` definitions,
letting the runaway match skip past them again. Also proactively
reworded a batch of `word (+0xNN)` offset-annotation parens in the same
file's field-shape summary list to "word at +0xNN" prose, even though
they weren't individually confirmed as triggers, since leaving
known-risky-shaped parens in a file that had already shown the bug once
is not worth the re-check cycle. **Lesson reinforced**: after fixing one
`DEF_RE` trigger in a file, re-run the check again rather than assuming
one fix clears the whole file -- multiple independent triggers in the
same header are now a confirmed real pattern, not a hypothetical.

`make verify`: exit 0, 0 FAIL lines, all 3 new KATs passing. Real `make
ko-clean && make ko KDIR=/home/build/linux-kronos` Kbuild build: clean
link, `OA.ko` produced (502300 bytes), zero warnings/errors traceable to
the 3 new files. `DECOMPILE_ERRORS.md` unchanged (the `CSTGTG92OscBase`
pure-virtual finding is a scope deferral, logged in
`HARDWARE_REVIEW_LOG.md` instead, not a compile/link failure).
`manifest/gen_oa_manifest.py` regenerated, OA.ko manifest 2384 -> 2404/
21,689 (11.084%), delta exactly +20 (7+7+6, matches all three classes'
real candidate counts, zero regressions).

**Next targets** (same technique, not yet done): ~137 more classes
remain (the whole-binary survey this batch found roughly 60 classes with
at least one real ctx-only candidate still outstanding, most already
individually catalogued across the prior 11 batches' own notes).
`CSTGTG92OscBase` needs its concrete subclass identified before its
`GetFreqOffset`-adjacent 9 methods can be attempted -- do not re-pick it
without that. Fresh, not-yet-individually-verified candidates from this
batch's own whole-binary sweep, roughly by size: `CSTGPitchModOsc` (8,
has an incidental non-triggering mention in `oa_stg_pitch_mod.h`'s own
prose -- confirmed NOT a real reference, still genuinely fresh),
`CSTGDriver` (7, disassembly already captured this batch -- all
fixed-K, zero ctx-index, zero outliers, ready to write up next time with
no further verification needed), `CSTGVPMNoise` (7, disassembly already
captured -- all fixed-K, zero ctx-index, one hardcoded-constant-getter
instance on `GetSaturation` same as `CSTGPanOutputBase::GetPatchSolo`'s
precedent, zero outliers, ready to write up), `CSTGAnalog4Pole` (7,
disassembly already captured -- all fixed-K at large struct offsets,
zero ctx-index, zero outliers, ready to write up -- confirmed DISTINCT
from the already-done `CSTGAnalog4PoleBase`, same
`CSTGPolysix`/`CSTGPolysixModel`-shaped name-collision precedent),
`CSTGPluckedModelPatch` (6, disassembly already captured -- all
fixed-K, zero ctx-index, zero outliers, ready to write up), `CSTGMOSSAmp`
(6, disassembly already captured -- mix of fixed-K and a stride-5
lea-premultiply ctx-index sub-family, zero outliers, ready to write up).
All 5 of these already have verified-clean disassembly on file from this
batch's own investigation (see the batch-12 HARDWARE_REVIEW_LOG.md entry
for full field offsets if the raw objdump needs re-pulling) -- next
session can skip straight to writing the header/cpp/KAT triplets for
whichever 1-3 are picked, no re-verification needed. Below those:
`CSTGPitchModCommon`/`CSTGSimpleAMSMixer`/`CSTGVPMPitchModTG92Osc`/
`CSTGVPMEG` (5 each), and a long tail down to 1-4 candidates per class --
re-run the whole-binary sweep query (see this batch's own methodology
note above) rather than the old per-class-grep approach for anything
below this batch's known-clean list.

**Thirteenth batch (2026-07-28, this batch): `CSTGDriver` (7/7) +
`CSTGVPMNoise` (7/7) + `CSTGAnalog4Pole` (7/7) + `CSTGPluckedModelPatch`
(6/6) + `CSTGMOSSAmp` (6/6) + `CSTGPitchModOsc` (8/8) done**, manifest
2404 -> 2445, 41 methods. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`). All six classes were carried
over from batch 12's own notes, which had already shape-classified all
six as zero-outlier via disassembly captured during that session, but had
NOT recorded exact field offsets in this memory file (only in general
shape terms) -- this batch re-pulled the real disassembly for all 41
candidates from scratch via `nm` + `objdump -dr -M intel
-j .text.<mangled>` rather than trusting the prior summary alone. Every
offset/width/sign/dual-write fact was independently re-derived from the
actual bytes. All six classes' real candidate counts matched the
carried-over counts exactly (8+7+7+7+6+6=41) and all reconfirmed zero
outliers -- a sixth batch in a row with a full clean sweep (batches 9-13,
excepting the deliberately-dropped, never-attempted `CSTGTG92OscBase`).

No new outlier shapes or decoder generalizations were needed -- every
method fit shapes already established in batches 1-12: plain fixed-K
dword/signed-byte fields, the mask-only single-bit bitfield shape
(`CSTGDriver::GetBypass`), the hardcoded-constant-getter shape first seen
on `CSTGPanOutputBase::GetPatchSolo` (`CSTGVPMNoise::GetSaturation`), the
unsigned non-bitfield byte variant first seen on
`CSTGPolysixMG::GetMIDITempoSyncTimes` (`CSTGPitchModOsc::GetEGSelect`),
and the stride-5 lea-premultiply ctx-dynamic-index sub-family
(`CSTGMOSSAmp`'s and `CSTGPitchModOsc`'s own AMSSource/AMSIntensity/
AMSIntensityAMSSource/AMSIntensityAMSIntensity groups) -- the same
naming-implies-second-modulation-level shape that was a false alarm on
`CSTGMS20EG` but confirmed REAL ctx-indexing on both of these two
classes, reinforcing that field-shape must always be verified from
disassembly per class rather than assumed from a prior class's outcome
with similar naming. `CSTGAnalog4Pole` has unusually large field offsets
(up to `+0x12c`) from a big struct layout, otherwise plain fixed-K-only
like `CSTGVPMNoise`/`CSTGDriver`/`CSTGPluckedModelPatch`.

**Hard rule reinforced this batch, worth treating as standard from now
on: NEVER hand-compute a KAT expected constant.** The first hand-typed
draft of `test_stg_driver_valuegetters.cpp`'s expected values (computed
mentally rather than via a script) was wrong in every single constant --
caught immediately by writing and running a standalone Python evaluator
(same deterministic `buf[i]=(i*0x9f+0x37)&0xff` pattern, ctx index fixed
at 3, 32-bit signed dword / 8-bit sign-or-zero-extend field rules) before
ever running `make verify`, then rewriting that one file and generating
every other new file's constants directly from the script's output from
the start. All prior batches' own notes already describe using "a
separate Python evaluator" for the KAT test file's OWN internal
re-derivation logic (verify/test_*.cpp itself), but this is the first
batch to explicitly flag that the same discipline must also apply to
producing the numeric literals that go INTO that test file in the first
place -- do not trust mental arithmetic for 32-bit signed wraparound or
sign-extension, always script it.

`make verify`: exit 0, 0 FAIL lines, all 6 new KATs (41 checks) passing.
Real `make ko-clean && make ko KDIR=/home/build/linux-kronos` Kbuild
build: clean link, `OA.ko` produced (508216 bytes, up from batch 12's
502300), zero warnings/errors traceable to any of the 6 new files.
`DECOMPILE_ERRORS.md` unchanged -- no compile/link blocker, no new
Tier-B deferral either. `manifest/gen_oa_manifest.py` regenerated, OA.ko
manifest 2404 -> 2445/21,689 (11.273%), delta exactly +41, confirmed via
a full reconstructed qualified-name-set diff -- 0 regressions.

**Next targets** (same technique, not yet done): ~131 more classes
remain. `CSTGTG92OscBase`'s pure-virtual deferral from batch 12 is still
open, still needs its concrete subclass identified before revisiting.
No fresh candidates were pre-investigated this batch beyond the six
picked -- re-run the whole-binary sweep (batch 12's own methodology: `nm
$KO | grep -E '^[0-9a-f]+ W _ZN[0-9]+.*ER23CSTGPatchMessageContext$'`
grouped by mangled length-prefixed class name) fresh for the next
batch's candidates, always doing the word-boundary grep +
already-modeled check before picking.

**Fourteenth batch (2026-07-28, commit `44fe4e0`): `CSTGSimpleAMSMixer`
(5/5) + `CSTGPitchModCommon` (5/5) + `CSTGPitchModCommonPlusAMS` (2/2) +
`CSTGVPMEG` (5/5) done**, manifest 2445 -> 2462, 17 methods. Same ground
truth binary (`/home/share/Decomp/OA.ko_Decomp/OA.ko`). Re-ran batch 12's
whole-binary sweep methodology fresh (`nm $KO | grep -E '^[0-9a-f]+ W
_ZN[0-9]+.*ER23CSTGPatchMessageContext$'`, grouped by mangled
length-prefixed class name via a small Python parser) since the class
pool has thinned out considerably -- 61 classes total still had at least
one real ctx-only candidate, the vast majority already individually
catalogued across batches 1-13. Confirmed `CSTGLFO` (21 candidates) and
`CSTGADSRBase` (20 candidates) -- this batch's two largest raw counts --
are BOTH already fully hand-modeled (`src/engine/lfo_component.cpp`,
`src/engine/adsr_base.cpp`, predating this scripted-family effort
entirely) via word-boundary grep, correctly skipped without writing a
file, same `CSTGProgramSlot`/`CSTGProgram` precedent. `CSTGPatch` (4
candidates) also confirmed already-modeled (real `struct CSTGPatch` in
`include/oa_types.h:123` plus stub member functions in
`src/stub/bar2_stubs_auth.cpp`) -- skipped for the same reason. This
left only small (4-5 candidate) genuinely-fresh classes as the batch's
own picks -- `CSTGSimpleAMSMixer`, `CSTGVPMMixer`, `CSTGPitchBase`,
`CSTGVPMAudioInput`, `CSTGStringTrackCommon` (all zero word-boundary grep
hits) plus `CSTGPitchModCommon`/`CSTGPitchModCommonPlusAMS`, which showed
up as only INCIDENTAL prose mentions (in the already-modeled
`CSTGPitchMod`'s and `CSTGPitchModOsc`'s own header comments, listing
sibling class names, not real references) rather than any struct/ctor --
correctly treated as genuinely fresh per the established
incidental-vs-real-reference distinction. `CSTGPitchModCommonPlusAMS`
was picked up as a bonus 4th class this batch because its own 2 real
candidates fell out of the identical `nm` grep used for
`CSTGPitchModCommon` (its name is a superset match) -- both are directly
related sibling classes (the PlusAMS variant adds one extra AMS
modulation leg on top of the Common base), so writing them up together
in the same batch cost no extra survey work.

All four classes came back fully clean -- zero outliers, extending the
clean-sweep streak (batches 9-13, now 9-14 excepting the deliberately-
dropped `CSTGTG92OscBase`). `CSTGSimpleAMSMixer` is the simplest dialect
-- a small two-input AMS mixer (Type selector, SourceA/SourceB,
AmountA/AmountB), zero ctx-index, plain fixed-K bytes and dwords only.
`CSTGPitchModCommonPlusAMS` (AMSSource/AMSIntensity pair) is likewise
zero-ctx-index, plain fixed-K.

**Genuinely new asymmetric ctx-index variant, first of its kind**:
`CSTGVPMEG`'s AMS1LevelModSource/AMS1LevelModIntensity and
AMS1TimeModSource/AMS1TimeModIntensity pairs split the by-now-familiar
bare-stride-4-SIB ctx-index shape (first confirmed on
`CSTGMultiFilter2Pole`, reused on `CSTGEG`) so that ONLY the Intensity
half of each Source/Intensity pair is ctx-indexed -- the Source half is
a plain fixed signed byte read directly off `this`, exactly like every
OTHER class's fixed-field Source siblings, despite sharing the "AMS1"
runtime-slot-implying name with its own ctx-indexed Intensity partner.
Every PRIOR class with this bare-stride-4 shape (`CSTGMultiFilter2Pole`,
`CSTGEG`) had BOTH halves of each Source/Intensity pair ctx-indexed
together -- this is the first confirmed case of the split. Verified
directly from disassembly, not inferred: `GetAMS1LevelModSource` is
`movsx eax, BYTE [eax+0x3e]` (no `edx` load at all) while
`GetAMS1LevelModIntensity` is `mov edx,[edx+0x4]; mov eax,[eax+edx*4+
0x3f]`. No decoder change needed -- the shared decoder already handles
fixed-K and ctx-indexed field loads independently per-method, this is
purely a new confirmed data point reinforcing "verify each method's
actual shape individually, never assume pair symmetry from naming or
from a prior class's own pair-symmetric precedent."

`CSTGVPMEG::GetTriggerAtNoteOn` reuses the established mask-only
single-bit bitfield shape (no shift instruction, bit 0), single-write
only -- no new shape needed.

**Tooling note, DEF_RE gotcha hit and fixed before shipping, in the NEW
class-level derivation prose this time rather than in a signature
mention**: `oa_stg_vpm_eg.h`'s first-draft leading comment used
"stride-4 shape (CSTGMultiFilter2Pole, CSTGEG) had BOTH halves..." and,
independently, "bitfield shape (no shift instruction, bit 0), single-
write only" -- both plain parenthetical asides listing other classes/
qualifiers, with zero semicolons anywhere in the span before the real
`CtxIndex` helper's own `{`, letting the runaway match reach past the
comment close and mis-capture `CtxIndex` as belonging to the word
immediately before the trigger paren ("shape"). Caught via the standard
exact DEF_RE captured-name-set diff (`got=={"shape"}` instead of the
wanted `{"CtxIndex"}` on the first check) before ever attempting to
build; fixed by rewording both to em-dash-delimited clauses, the
by-now-standard convention. All 8 new files (4 classes x header+cpp,
technically the KAT test files weren't independently DEF_RE-checked
since they contain no class-level derivation prose, only the two
`.h`/`.cpp` pairs per class needed the check) passed both the `/*`/`*/`
balance check and the exact-name-set diff before compiling.

`make verify`: exit 0, 0 FAIL lines, all 4 new KATs (17 checks) passing,
43 checks total across the whole suite. Real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` Kbuild build: clean link, `OA.ko`
produced (510736 bytes, up from batch 13's 508216), zero warnings/errors
traceable to any of the 4 new files (confirmed via a build-log grep
scoped to each new filename). `DECOMPILE_ERRORS.md` unchanged -- no
compile/link blocker hit, no new Tier-B deferral either.
`manifest/gen_oa_manifest.py` regenerated, OA.ko manifest 2445 -> 2462/
21,689 (11.351%), delta exactly +17, confirmed via a full reconstructed
qualified-name-set diff -- 0 regressions.

**Shared-repo hygiene note, reconfirmed this batch**: `/home/share`'s
git tree spans BOTH `reconstructed/OA` and `reconstructed/Eva` as one
repo (confirmed via `git log --oneline` showing interleaved "OA.ko:" and
"Eva:" commit messages), and a concurrent session's untracked
`reconstructed/Eva/tools/build_gdbserver.sh` /
`reconstructed/Eva/tools/gdbserver-i386-musl` files were sitting in
`git status` output throughout this batch's work. Per
[[shared_repo_commit_hygiene]], staged ONLY the 13 intended OA files by
exact path (never `git add -A`/`git add .`) and verified `git diff
--cached --stat` showed exactly those 13 paths immediately before
committing -- avoided a repeat of the earlier CPianoOsc/CSTGEPModelPatch
batch's accidental cross-project commit.

**Next targets** (same technique, not yet done): ~127 more classes
remain. `CSTGLFO`/`CSTGADSRBase`/`CSTGPatch` all CONFIRMED
already-modeled this batch -- do not re-add to future candidate lists.
`CSTGTG92OscBase`'s pure-virtual deferral from batch 12 still open.
Fresh, not-yet-individually-verified candidates from this batch's own
whole-binary sweep, by size: `CSTGVPMMixer` (4, confirmed fresh via
word-boundary grep this batch, not yet picked), `CSTGPitchBase` (4,
confirmed fresh), `CSTGVPMAudioInput` (4, confirmed fresh),
`CSTGStringTrackCommon` (4, confirmed fresh). Below those: `CSTGPanOutput`
(3), `CSTGVPMFilter` (3), `CSTGPitchModOscBase` (3), `CSTGTG92Osc` (2),
`CSTGPitchModBase` (2), `CSTGPCMModelPatch` (2 -- note this contradicts
batch 9's own "confirmed NOT part of this family" verdict, which found
only 2 T-linkage symbols; this batch's whole-binary sweep found 2 W
(weak) ctx-only-suffix symbols instead -- UNRECONCILED, re-check via a
direct `nm` query on `CSTGPCMModelPatch` specifically before trusting
either verdict, don't assume the newer sweep is automatically right),
plus singletons `CSTGComponent`/`CSTGTG01Filter`/`CSTGStringTrack`/
`CSTGAnalogSyncModelPatch` (1 each, unverified). Re-run the whole-binary
sweep query fresh once these are exhausted, always doing the
word-boundary grep + already-modeled check before picking.

**Fifteenth batch (2026-07-28, this batch): resolved the CSTGPCMModelPatch
contradiction (Part 1) + `CSTGPitchBase` (3/4) + `CSTGVPMMixer` (4/4) +
`CSTGVPMAudioInput` (4/4) + `CSTGStringTrackCommon` (4/4) done** (Part
2), manifest 2462 -> 2477, 15 methods. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`).

**CSTGPCMModelPatch contradiction, definitively resolved.** Batch 9 said
"not part of family" (only 2 T-linkage extra-arg symbols via a per-class
`nm` grep). Batch 14's later whole-binary sweep (which greps the ENTIRE
binary for `^[0-9a-f]+ W _ZN[0-9]+.*ER23CSTGPatchMessageContext$`, i.e.
weak linkage + the exact ctx-only mangled suffix, with no name-prefix
filter at all) found 2 different, real W-linkage symbols matching that
suffix: `ResetWaveform` and `UpdateSlotPortamento`. Both verdicts were
individually correct on their own narrow evidence, but nobody had opened
either symbol's actual disassembly. Direct `nm -C` full method dump plus
isolated `objdump -dr -M intel -j .text.<mangled>` on both: each is a
5-instruction adjustor thunk -- `lea eax,[eax+K]` (rebasing `this` onto a
DIFFERENT base-class sub-object) immediately followed by a tail `call`
via an `R_386_PC32` relocation to a SAME-NAMED method on a DIFFERENT base
class (`CSTGPCMModelPatch::ResetWaveform` calls
`CSTGTG92OscBase::ResetWaveform`; `CSTGPCMModelPatch::UpdateSlotPortamento`
calls `CSTGPortamentoBase::UpdateSlotPortamento`). Neither body ever
touches `CSTGParamsOwner::sValueGetterTemp` or returns `STGConvertedParam&`
in the family's own convention -- these are C++ multiple-inheritance
covariant/adjustor thunks, a completely different compiler-generated
mechanism, coincidentally weak-linkage and coincidentally ending in the
exact same mangled suffix because they happen to forward a
`CSTGPatchMessageContext&`-taking call. **CSTGPCMModelPatch is
DEFINITIVELY confirmed NOT part of this family** -- batch 9's verdict
stands, batch 14's 2 extra candidates are a real but unrelated
false-positive class, now root-caused. **New standing rule for the
whole-binary sweep methodology**: a bare regex match on weak-linkage +
ctx-only mangled suffix is NOT sufficient on its own when the candidate's
own name doesn't start with `Get`/`Set` (or even when it does, per this
batch's own `CSTGPitchBase` finding below) -- always at least skim the
disassembly's first few bytes before trusting a sweep hit as a real
value-getter candidate, since a same-named-base-class adjustor thunk can
pass every linkage/signature filter used so far without being one. This
also incidentally reveals `CSTGPCMModelPatch` multiply-inherits from (at
least) `CSTGTG92OscBase` and `CSTGPortamentoBase` -- relevant background
for whenever the still-open `CSTGTG92OscBase` pure-virtual deferral from
batch 12 is revisited.

Part 2's four classes were the smallest known-fresh candidates carried
over from batch 14's own list, all reconfirmed fresh via word-boundary
grep before starting. `CSTGVPMMixer`/`CSTGVPMAudioInput`/
`CSTGStringTrackCommon` came back clean; `CSTGPitchBase` had 1 excluded
outlier out of its 4 real ctx-only-suffix candidates --
`HandleVoiceKeyDownTuningOffsetChanged`, whose entire body is a bare
`ret` with NOTHING else -- no field read, no `sValueGetterTemp` write, no
meaningful return value. A genuinely new outlier shape: a real no-op
stub, distinct from the earlier hardcoded-constant-getter shape
(`CSTGPanOutputBase::GetPatchSolo`, which DOES write a literal 0 into
`.value`) -- this one writes nothing at all. Also excluded because its
name doesn't fit the `Get`/`Set` convention, giving two independent
reasons to drop it. This is the SAME general false-positive risk class as
the `CSTGPCMModelPatch` thunks above (a ctx-only-suffix match whose body
isn't a real getter) -- reinforces treating a body-check as mandatory,
not optional, for any candidate whose name doesn't start with `Get`.

**Notable finding, not a bug**: `CSTGPitchBase::GetBendUp` and
`GetBendRange` are byte-identical bodies, both reading `this+0xc` --
confirmed by re-dumping each in complete isolation (not a copy-paste
mixup from a shared multi-`-j` objdump invocation reordering sections).
Modeled faithfully as two separate member functions with identical
bodies, matching ground truth's own apparent field-aliasing rather than
having one call the other or sharing an implementation -- the family's
established practice throughout has been "transcribe what the
disassembly shows," and this is exactly that, even when the result looks
redundant.

No new decoder shapes needed this batch -- `CSTGVPMMixer` reuses the
stride-10 lea-premultiply-plus-x2-SIB-scale ctx-index shape first seen on
CSTGMS20's own Standard/Mixer AMS group; `CSTGStringTrackCommon`'s
`GetStringNoteValue` reuses the bare stride-1 ctx-index shape first seen
on CSTGMS20's own `GetInputJack`; `CSTGVPMAudioInput` is zero-ctx-index,
all fixed-K, byte-identical field layout offsets to `CSTGVPMMixer`'s own
per-operator record despite NOT being ctx-indexed itself (confirmed via
disassembly, not assumed from the offset coincidence).

**KAT discipline note**: caught my own hand-typed draft constants in the
first version of `test_stg_pitch_base_valuegetters.cpp` (accidentally
reused two numbers from a DIFFERENT class's own KAT, a copy-paste-style
slip rather than mental arithmetic) BEFORE running `make verify`, by
writing the standalone Python evaluator FIRST for every class this batch
(not retrofitted after a failure) and diffing its output against what had
already been typed -- caught the mismatch immediately. Reinforces batch
13's own "never hand-compute a KAT constant" rule: the risk isn't just
mental-arithmetic errors, copy-paste from a neighboring class's own
already-computed values is an equally real failure mode, and the
one Python-script-first discipline catches both.

**Tooling note, 3 fresh DEF_RE/comment-balance gotchas hit and fixed
before shipping**: `oa_stg_pitch_base.h` and `oa_stg_vpm_mixer.h` each
hit ANOTHER fresh instance of the literal-`*/`-in-prose bug -- "Get*/Set*
convention" (the exact by-now-well-known trigger phrase, still recurring
despite being flagged in multiple prior batches) and "Standard*/\nMixer*
AMS group" respectively. `oa_stg_string_track_common.h` hit the DEF_RE
parenthesis-swallow bug TWICE in succession on two independent
triggers -- first "CSTGString (pilot class, batch 1)" (a real class-name
mention immediately followed by a parenthetical aside), then even after
fixing that, a second trigger "GetFretPosition (unsigned byte, movzx, no
shift/mask --" (a literal method-name-then-signature-detail mention) was
still present and had to be caught by RE-RUNNING the check, not assumed
fixed after the first pass -- same "one fix doesn't clear a whole file"
lesson as batch 12's own `CSTGVPMOutputMixer` finding. All fixed via the
established zero-parens-before-real-code convention; final state
verified via both checks on all 8 new `.h`/`.cpp` files before ever
compiling.

`make verify`: exit 0, 0 FAIL lines, all 15 new checks passing. Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` Kbuild build:
clean link, `OA.ko` produced (513040 bytes, up from batch 14's 510736),
zero warnings/errors traceable to any of the 4 new files.
`DECOMPILE_ERRORS.md` unchanged -- no compile/link blocker.
`manifest/gen_oa_manifest.py` regenerated, OA.ko manifest 2462 -> 2477/
21,689 (11.421%), delta exactly +15, confirmed via a full reconstructed
qualified-name-set diff -- 0 regressions. Committed as 13 files (4
headers + 4 .cpp + 4 test .cpp + Makefile); `git diff --cached --stat`
verified immediately before commit per [[shared_repo_commit_hygiene]] --
2 concurrent-session Eva files (`build_gdbserver.sh`,
`gdbserver-i386-musl`) were sitting untracked in `git status` throughout
and correctly left untouched.

**Next targets** (same technique, not yet done): ~123 more classes
remain. `CSTGPCMModelPatch` now DEFINITIVELY confirmed NOT part of this
family -- do not re-add to any future candidate list, the contradiction
is closed. `CSTGTG92OscBase`'s pure-virtual deferral from batch 12 still
open, now with a confirmed second sibling subclass
(`CSTGPCMModelPatch`, via the adjustor-thunk finding above) worth
checking if that subclass's own concrete override is ever needed.
Fresh, not-yet-individually-verified candidates from batch 14's own
whole-binary sweep, by size, still not picked: `CSTGPanOutput` (3),
`CSTGVPMFilter` (3), `CSTGPitchModOscBase` (3), `CSTGTG92Osc` (2),
`CSTGPitchModBase` (2), plus singletons `CSTGComponent`/
`CSTGTG01Filter`/`CSTGStringTrack`/`CSTGAnalogSyncModelPatch` (1 each,
unverified -- note `CSTGStringTrack` is NOT the same class as this
batch's own `CSTGStringTrackCommon`, check via word-boundary grep before
assuming either way). Re-run the whole-binary sweep query fresh (batch
12's own methodology: `nm $KO | grep -E '^[0-9a-f]+ W
_ZN[0-9]+.*ER23CSTGPatchMessageContext$'` grouped by mangled
length-prefixed class name) once these are exhausted, always doing the
word-boundary grep + already-modeled check AND a disassembly body-check
on any non-`Get`/`Set`-prefixed candidate before picking.

**Sixteenth batch (2026-07-28, commit `a19d894`): `CSTGVPMFilter` (3/3) +
`CSTGPitchModOscBase` (3/3) + `CSTGTG92Osc` (2/2) + `CSTGPitchModBase`
(2/2) done**, manifest 2477 -> 2487, 10 methods. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`). Picked from the fifteenth
batch's own carried-over backlog (`CSTGPanOutput`/`CSTGVPMFilter`/
`CSTGPitchModOscBase`/`CSTGTG92Osc`/`CSTGPitchModBase`, all singletons/
small classes "unverified" as of that batch). Standard checklist first
-- word-boundary grep confirmed all 5 genuinely fresh (the 3
PitchMod-prefixed names showed only incidental sibling-class-name prose
mentions in already-modeled classes' own header comments, same pattern
as batch 14's own `CSTGPitchModCommon` precedent, not real references),
whole-binary `nm` weak-linkage + ctx-only-suffix sweep gave the exact
real candidate set per class.

**`CSTGPanOutput` investigated and excluded -- NOT part of this
family.** Its 3 ctx-only-suffix candidates are all named
`UpdateHDRBus`/`UpdateOutputBus`/`UpdateFXControlBus`, none starting
with `Get`/`Set` -- per the standing rule from batch 15 (any sweep hit
whose name doesn't fit the convention needs an actual body/context check
before being trusted, not just linkage), dumped the whole class's `nm`
output rather than just the 3 candidates. It shows a real, already
partially-modeled runtime bus-routing/message-handler mechanism:
`sValueGetters`/`sMessageHandlers` static tables (the same
`CKGModuleParamMsgHandler`-style dispatch convention rejected back in
batch 8, not this family's own), `PrecomputeData`, `ProcessSubRate`, and
a dozen more real T-linkage `UpdateXxx(ctx[, STGConvertedParam&])`
runtime methods (`UpdatePatchLevel`, `UpdateSend1Level`,
`UpdatePanAMSSource`, etc). Correctly excluded without ever running the
decoder or writing a file -- distinct from `CSTGPanOutputBase`
(already-modeled since batch 10), a different, unrelated class despite
the shared prefix, same `CSTGPolysix`/`CSTGPolysixModel`-shaped
name-collision precedent as always.

The 4 remaining classes came back fully clean -- zero outliers, seventh
batch in a row with a full clean sweep (batches 9-16, excepting the
still-open, deliberately-dropped `CSTGTG92OscBase` pure-virtual
deferral from batch 12; note `CSTGTG92Osc`, this batch's own pick, is a
small, separate, already-fully-tractable subclass -- confirmed distinct
from `CSTGTG92OscBase` via its own 2-candidate real set, unrelated to
the still-open deferral). All 4 are the family's simplest dialect --
every candidate a fixed-K byte/dword field read directly off `this`,
zero ctx-dynamic-index methods across the whole batch.

**New field-shape: byte-equals-literal-constant boolean.**
`CSTGVPMFilter::GetRoutingValue` is `cmp BYTE[this+0x1f],0x3; sete al;
movzx eax,al` -- single-write only. Every prior truth-value-test shape
in the family (`CSTGAnalogSyncOsc`'s own `GetRingModModulatorSelect`/
`GetSubOscAudioInModeSelect`, both dword `test reg,reg; setne/sete`)
compared a field against zero; this is the first confirmed case of a
byte compared against a nonzero literal constant. Modeled directly as
`(*(unsigned char*)(base+K) == LITERAL) ? 1 : 0` -- no decoder
infrastructure change needed, purely a new confirmed comparison operand
shape alongside the existing zero-test cases.

**Reconfirmed variant, not new**: `CSTGTG92Osc::GetOscVelocityZoneLow`/
`GetOscVelocityZoneHigh` are both the "unsigned non-bitfield byte"
variant (`movzx`, no shift/mask) -- the 4th and 5th confirmed instances
in the family after `CSTGPolysixMG::GetMIDITempoSyncTimes`,
`CSTGVPMModelPatch::GetAlgorithm`, and `CSTGPitchModOsc::GetEGSelect`.

**Tooling note, negative result worth logging**: all 8 new `.h`/`.cpp`
files passed both standard post-generation checks (comment
open/close-count balance, exact DEF_RE captured-name-set diff) clean on
the FIRST draft -- the first batch since roughly batch 8 to hit neither
of the two well-known gotcha classes at all. Not a sign the bugs are
fixed (the tooling itself is unchanged) -- simply this batch's own
derivation prose happened to avoid both trigger patterns (no
parenthetical asides before real code, no adjacent `X*/Y*`-shaped
wording). Keep running both checks on every future file regardless.

`make verify`: exit 0, 0 FAIL lines, all 10 new checks passing. Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` Kbuild build:
clean link, `OA.ko` produced (514656 bytes, up from batch 15's 513040),
zero warnings/errors traceable to any of the 4 new files (the only
`grep -i error` hits in the build log for these filenames were the
`-Werror-implicit-function-declaration` substring inside the compile
invocation line itself, not a real diagnostic -- worth remembering
before trusting a naive log grep). `DECOMPILE_ERRORS.md` unchanged -- no
compile/link blocker, no new Tier-B deferral. `manifest/gen_oa_manifest.py`
regenerated, OA.ko manifest 2477 -> 2487/21,689 (11.467%), delta exactly
+10 (3+3+2+2), all 10 spot-checked present in the CSV with correct
`__regparm3`/`reconstructed`/byte-size-matching-disassembly rows (sizes
cross-checked 1:1 against the real objdump byte counts).

**Next targets** (same technique, not yet done): ~119 more classes
remain. `CSTGPanOutput` now CONFIRMED NOT part of this family -- do not
re-add to future candidate lists (distinct from the already-modeled
`CSTGPanOutputBase`, don't confuse the two). `CSTGTG92OscBase`'s
pure-virtual deferral from batch 12 still open. No fresh candidates were
pre-investigated beyond the four picked this batch -- re-run the
whole-binary sweep query fresh (batch 12's own methodology: `nm $KO |
grep -E '^[0-9a-f]+ W _ZN[0-9]+.*ER23CSTGPatchMessageContext$'` grouped
by mangled length-prefixed class name) for the next batch's candidates,
always doing the word-boundary grep + already-modeled check AND a
disassembly/body check on any non-`Get`/`Set`-prefixed candidate before
picking.

**Seventeenth batch (2026-07-28, commit `ae766cb`): `CSTGVPMPitchModTG92Osc`
(5/5) + `CSTGTG01Filter` (1/1) + `CSTGStringTrack` (1/1) done**, manifest
2487 -> 2494, 7 methods. Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`). Re-ran batch 12's whole-binary
sweep methodology fresh (`nm $KO | grep -E '^[0-9a-f]+ W
_ZN[0-9]+.*ER23CSTGPatchMessageContext$'`, grouped by mangled
length-prefixed class name) since batch 16 left no pre-investigated
carryover -- 60 classes total showed at least one sweep hit, and cross-
referencing every one against this file's own done/excluded/already-
modeled lists found only 5 candidates genuinely untouched:
`CSTGVPMPitchModTG92Osc` (5), plus 4 singletons --
`CSTGVPMPitchModTG92Osc` aside, `CSTGComponent` (1),
`CSTGTG01Filter` (1), `CSTGStringTrack` (1), `CSTGAnalogSyncModelPatch`
(1).

**Major finding: the whole-binary sweep is now EXHAUSTED.** Every one of
the 60 sweep-visible classes is accounted for -- reconstructed (49
classes across batches 1-17, one of them `CSTGEGBase` at a real 5/5),
confirmed NOT part of the family (`CSTGPCMModelPatch`, `CSTGPanOutput`,
and as of this batch `CSTGComponent`/`CSTGAnalogSyncModelPatch`,
below), confirmed already-modeled elsewhere (`CSTGProgramSlot`,
`CSTGProgram`, `CSTGControllerInfo`, `CSTGVectorMotion`,
`CSTGDrumKitData`, `CSTGWaveSequence`, `CSTGProgramModeDrumTrackSlot`,
`CSTGMultisampleBank`, `CSTGFrontPanelSmoothers`, `CSTGCommonStepSeq`,
`CSTGAudioInput`, `CSTGHDRTrack`, `CSTGHDRMiniModel`,
`CSTGProgramModeProgramSlot`, `CSTGLFO`, `CSTGADSRBase`, `CSTGPatch`),
or the one still-open `CSTGTG92OscBase` pure-virtual deferral from batch
12. **This means the family's remaining ~120 classes (of the original
~180-class estimate) do NOT show up in this sweep at all** -- either
their `Get*`/`Set*` methods got fully inlined into their callers with no
standalone symbol left, or they use a differently-shaped mangled suffix
(e.g. an extra `const` qualifier, a different context-parameter type, a
namespace wrapper) that this sweep's exact-suffix regex doesn't match,
or they're simply not yet emitted as COMDAT weak symbols for some other
reason. **Next session's first task should be figuring out which of
these is true** -- e.g. try a looser sweep regex (weak linkage without
the exact suffix requirement, or grep for `sValueGetterTemp` cross-refs
directly via `nm`/relocation-based `objdump -dr` on the whole `.text`
section rather than per-symbol section names) before assuming the
convention-based scripted-decoder technique has run its course on this
codebase.

`CSTGVPMPitchModTG92Osc` (VPM engine TG92-oscillator pitch-modulation
component) came back fully clean -- zero outliers. Reuses the
`CSTGVPMEG`-precedent asymmetric AMS split (only the Intensity half of
an AMSSource/AMSIntensity pair is ctx-indexed, the Source half stays a
plain fixed byte) on its own AMSSource/AMSIntensity pair, PLUS a second,
independent example of the SAME asymmetry one level down on
AMSIntensityAMSSource/AMSIntensityAMSIntensity (both plain fixed fields,
no ctx-index at all) -- confirming the asymmetric-split shape isn't a
one-off quirk of `CSTGVPMEG` specifically. `GetUseCommonMod` reuses the
established mask-only single-bit bitfield shape.

`CSTGTG01Filter` and `CSTGStringTrack`: each class's ENTIRE real
ctx-only-suffix candidate set turned out to be exactly one method --
`GetRouting` (plain fixed signed byte, single-write) and
`GetStringValue` (bare stride-4 SIB-scaled ctx-index dword, no lea
premultiply, dual-write -- the same shape first confirmed on
`CSTGMultiFilter2Pole`) respectively. `CSTGStringTrack` confirmed
genuinely distinct from the already-modeled `CSTGStringTrackCommon` via
word-boundary grep -- same `CSTGPolysix`/`CSTGPolysixModel` name-
collision precedent as always.

**2 new sweep hits investigated and confirmed NOT part of this family,
2 genuinely new outlier/exclusion shapes**:
1. `CSTGComponent::PrecomputeData` -- entire body is a bare `ret`, zero
   instructions before it. A real no-op stub, same shape as
   `CSTGPitchBase::HandleVoiceKeyDownTuningOffsetChanged` from batch 15
   (no field read, no `sValueGetterTemp` write at all -- distinct from
   the hardcoded-constant-getter shape, which DOES write a literal into
   `.value`). Also: `CSTGComponent` itself is independently characterized
   in this project's own `oa_adsr_base.h` header comments (predating this
   family's own batches) as "out-of-scope, giant" -- the universal STG
   component base class -- reinforcing this exclusion rather than
   contradicting it. `PrecomputeData` doesn't fit the `Get`/`Set` naming
   convention either, a second independent reason to exclude per the
   batch-15/16 standing rule.
2. `CSTGAnalogSyncModelPatch::UpdateSlotPortamento` -- a genuine C++
   multiple-inheritance adjustor thunk: `push ebp; lea eax,[eax+0x3fe];
   mov ebp,esp; and esp,0xfffffff0; call
   CSTGPortamentoBase::UpdateSlotPortamento; ...; ret` -- rebases `this`
   onto a different base-class sub-object then tail-calls the SAME-NAMED
   method on that base class. Identical mechanism to
   `CSTGPCMModelPatch`'s own two thunks found in batch 15
   (`ResetWaveform`/`UpdateSlotPortamento`, forwarding to
   `CSTGTG92OscBase`/`CSTGPortamentoBase` respectively) -- this is now
   the THIRD confirmed adjustor-thunk instance in the family, and the
   second one specifically forwarding to `CSTGPortamentoBase`'s own
   `UpdateSlotPortamento`. Also doesn't fit `Get`/`Set` naming, same
   dual-reason exclusion as above. Worth noting for whenever
   `CSTGTG92OscBase`'s pure-virtual deferral is revisited:
   `CSTGAnalogSyncModelPatch` is now a THIRD confirmed class that
   multiply-inherits from `CSTGPortamentoBase` (or `CSTGTG92OscBase`),
   alongside `CSTGPCMModelPatch`'s own two.

**Tooling note, DEF_RE gotcha hit and fixed before shipping, one file
needed 2 separate rounds**: `oa_stg_string_track.h`'s first-draft leading
comment had TWO independent triggers in sequence -- "before starting
(distinct from the already-modeled CSTGStringTrackCommon..." (a real
parenthetical aside) and, even after fixing that one, a second trigger
"sweep (batch 12's methodology)." further down, both hit via the
standard exact DEF_RE captured-name-set diff (`got=={"starting"}` then
`got=={"sweep"}`, wanted `{"CtxIndex"}` both times) -- reconfirms batch
12's and 15's own "one fix doesn't clear a whole file, always re-run the
check" lesson. `oa_stg_tg01_filter.h` independently hit the by-now
well-known literal-`*/`-in-prose trigger ("no other Get*/\nSet*
symbols"), same fix as always (reworded to "Get*- or Set*-prefixed").
All 6 new files passed both checks (comment balance, exact DEF_RE
name-set diff) before ever compiling.

`make verify`: exit 0, 0 FAIL lines, all 7 new checks passing, 54 test
suites total. Real `make ko-clean && make ko KDIR=/home/build/linux-kronos`
Kbuild build: clean link, `OA.ko` produced (515908 bytes, up from batch
16's 514656), zero warnings/errors traceable to any of the 3 new files.
`DECOMPILE_ERRORS.md` unchanged -- no compile/link blocker, no new
Tier-B deferral. `manifest/gen_oa_manifest.py` regenerated, OA.ko
manifest 2487 -> 2494/21,689 (11.499%), delta exactly +7, confirmed via
a full reconstructed qualified-name-set diff -- 0 regressions. Committed
as 10 files (3 headers + 3 `.cpp` + 3 test `.cpp` + Makefile);
`git diff --cached --stat` verified immediately before commit per
[[shared_repo_commit_hygiene]] -- the same 2 concurrent-session Eva
files plus a newly-modified `../Eva/include/stream_family.h` were
sitting in `git status` throughout and correctly left untouched.

**Next targets (superseded by batch 18 below)**: `CSTGTG92OscBase`'s
pure-virtual deferral from batch 12 still open, now with 3 confirmed
sibling/related classes via the adjustor-thunk trail
(`CSTGPCMModelPatch` x2, `CSTGAnalogSyncModelPatch` x1).

---

## Batch 18 (2026-07-28, commit `e43c010`): broader discovery method + 7 new classes

**The broader discovery method, exact recipe** (validated this batch,
reuse directly for future batches once the current backlog is
exhausted again):

1. Find `CSTGParamsOwner::sValueGetterTemp`'s mangled name and address
   via `nm -C $KO | grep sValueGetterTemp` (ground truth binary is
   still `/home/share/Decomp/OA.ko_Decomp/OA.ko`).
2. `objdump -dr $KO > full_dis.txt` -- a full disassembly of every
   section, weak/COMDAT `.text.<mangled>` sections AND the plain merged
   `.text` section together, WITH relocation annotations (`-r`).
3. Walk `full_dis.txt` with awk: track the most recent `<function>:`
   label line, and every time a line contains `sValueGetterTemp`
   (a relocation annotation line), record the CURRENT enclosing
   function name. `sort -u` the result.
4. This batch: 3608 raw relocation hits collapsed to 1462 UNIQUE
   enclosing functions -- `c++filt` each mangled name, extract the
   class name (text before `::`), `sort | uniq -c | sort -rn` to rank
   by method count. 75 unique classes total (vs 60 via the old
   `ER23CSTGPatchMessageContext$` sweep).
5. `comm -23` the new class list against a hand-maintained list of
   every class already done/excluded/already-modeled/deferred (79
   entries as of batch 17) -- this batch found exactly 7 genuinely new
   classes this way: `CSTGCombi`, `CSTGCommonEffectLFO`,
   `CSTGCommonLFO`, `CSTGEffectBalance`, `CSTGEffectRack`,
   `CSTGMetronomeSettings`, `CSTGToneAdjust` (plus 2 harmless false
   positives to filter out by hand: `CSTGParamsOwner` itself, whose own
   `DummyGetter` trivially references its own static, and the
   "global constructors keyed to ..." static-initializer stub).

**Why this catches classes the old sweep could never see -- confirmed
on real examples, not just theorized**: the old sweep's regex required
the EXACT mangled suffix `ER23CSTGPatchMessageContext$` -- reference to
`CSTGPatchMessageContext`, weak linkage only. This batch's 7 new
classes fail that filter on BOTH axes simultaneously in different
combinations:
  - Different context type entirely: `CSTGCombi`/`CSTGEffectRack`/
    `CSTGMetronomeSettings`/`CSTGCommonEffectLFO`/`CSTGEffectBalance`
    take `CSTGMessageContext&` -- a real, already-modeled SIBLING
    context class (`oa_global.h`, with a confirmed real `index` field
    at +0x4, the same offset the rest of the family reads its own
    per-call dynamic index from). `CSTGCommonLFO` takes a
    `CSTGProgramMessageContext&` and `CSTGToneAdjust` a
    `CSTGToneAdjustMessageContext&` -- both BRAND NEW context types,
    declared for the first time this batch (see below).
  - Mixed/strong linkage: `CSTGEffectBalance`'s entire candidate set
    and roughly half of `CSTGEffectRack`'s are `T` (global), not `W`
    (weak/COMDAT) -- invisible to any weak-only sweep no matter how the
    suffix regex is loosened.
  A grep across the whole binary for other `*MessageContext` class
  names (`nm -C $KO | grep -oE '[A-Za-z_][A-Za-z0-9_]*MessageContext'`)
  turns up MORE siblings not yet investigated: `CSTGDrumkitMessageContext`,
  `CSTGEffectMessageContext`, `CSTGHDRTrackMessageContext`,
  `CSTGProgramSlotMessageContext`, `CSTGWaveSeqDataMessageContext` --
  strong evidence the family's real context-type surface is much wider
  than `CSTGPatchMessageContext` alone, confirming this discovery
  method's premise rather than being a one-off.

**Two brand-new minimal context structs declared this batch**, same
"only the fields this cluster's own methods read" treatment as
`CSTGPatchMessageContext` (`oa_adsr_base.h`):
  - `CSTGProgramMessageContext` (`include/oa_engine_init.h`, right
    before `CSTGCommonLFO`): just the shared `index` field at +0x4 --
    confirmed UNUSED by every one of `CSTGCommonLFO`'s own 17
    candidates (see below), kept only for structural consistency with
    every sibling context type.
  - `CSTGToneAdjustMessageContext` (`include/oa_global.h`, right before
    `CSTGToneAdjust`): `index` at +0x4 (used by all 7 candidates) PLUS
    a real OUTPUT field, `changedFlag` at +0x1c -- see the new
    side-effect shape below.

**4 of the 7 new classes already had minimal real structs elsewhere in
this project** (ctor and/or `Initialize()` only, from earlier UNRELATED
batches reconstructing `CSTGProgram`'s/`CSTGCombi`'s own constructor
chains) -- `CSTGCombi`, `CSTGEffectRack`, `CSTGToneAdjust`,
`CSTGCommonLFO`. Extended IN PLACE (new method declarations added to
the existing `struct` in `oa_global.h`/`oa_engine_init.h`, new `.cpp`
files for the definitions) rather than skipped -- this is DIFFERENT
from the established `CSTGProgramSlot`/`CSTGProgram`/
`CSTGControllerInfo`-class "already-modeled, SKIP" precedent, because
none of these 4 structs carry that precedent's actual risk factor (a
LARGE, heavily cross-referenced struct with a real modeled vtable and
dispatch machinery) -- all 4 are tiny (1-2 pre-existing methods, no
named fields, no vtable dispatch modeled). Always check the ACTUAL
existing struct's size/complexity before defaulting to "already
modeled = skip" -- the real risk factor is entanglement, not mere
prior existence. The other 3 new classes
(`CSTGCommonEffectLFO`/`CSTGEffectBalance`/`CSTGMetronomeSettings`)
were genuinely fresh -- referenced only in OTHER classes' own header
comments as inlined sub-objects (no out-of-line ctor in ground truth at
all, confirmed via a whole-symbol-table grep), got brand new headers
following the family's usual one-class-one-header convention.

**3 genuinely new shapes for the whole family, all found in this single
batch**:

1. **Piecewise multi-bank record resolver** (`CSTGEffectRack::
   GetValueAlgorithm`/`GetValueDModMIDIRouting`): a SINGLE unified
   `ctx.index` (0..15) selects across THREE different fixed-base,
   different-stride record arrays -- IFX (0..11, stride 0xa8, base
   `this+4`), MFX (12..13, stride 0x9c, base `this+0x7e4`), TFX (14..15,
   stride 0x98, base `this+0x91c`) -- reproduced as a small file-local
   `ResolveEffectSlotRecord()` helper matching ground truth's own
   branch structure exactly, including its own unguarded NULL fallback
   for `idx>15` (dead code in practice, the real slot index is always
   0..15, kept for exact behavioral fidelity anyway). Every OTHER
   ctx-indexed getter in this same class uses a much simpler single-bank
   `this + idx*stride + K` shape, matching every prior batch's
   established `CtxIndex`-family convention -- this piecewise shape is
   exclusive to the 2 "algorithm"/"routing" parameters that apply
   uniformly across effect types.
2. **Global-singleton indirection, ignoring `this` and `ctx` entirely**
   (`CSTGEffectBalance`'s all 3 methods): resolves the process-wide
   ACTIVE `CSTGPerformanceVarsManager` via the exact same raw
   `sInstance[8]`-selector lookup this project's own
   `CSTGGlobal::ResolveActivePerformanceVarsManager()` (`global.cpp`)
   already establishes, confirmed via direct disassembly that ground
   truth INLINES this sequence at each of the 3 call sites rather than
   emitting a real `call` -- and since that helper is a PRIVATE
   `CSTGGlobal` member, a real cross-class call wouldn't even compile,
   so this file's own small static local reimplements the same raw
   sequence rather than reusing the existing one. First confirmed case
   in this whole family of a value-getter that reads neither the
   object's own data NOR the passed context at all.
3. **Side effect on the context object itself**
   (`CSTGToneAdjust::GetValueAssignSlider`/`AssignKnob`/`AssignSwitch`):
   the returned value is the upper 7 bits (`>>1`) of a ctx-indexed
   packed byte, but bit 0 of that SAME byte is separately written back
   into `ctx.changedFlag` (+0x1c) as a genuine mutation of the caller's
   context object -- every prior batch's Get* methods were pure reads
   of `this`/`ctx`, never writers. A related but distinct 4th shape,
   `GetValueSwitchValue`, uses `ctx.index` as a variable BIT-SHIFT
   AMOUNT (`(fixed_word >> ctx.index) & 1`) against a FIXED word field,
   rather than as an array/record index at all -- the family's first
   confirmed "index selects a bit position, not a record" case.

**`CSTGCommonLFO`'s own naming trap, NOT a new shape**: despite 8 of its
17 methods carrying AMSSource/AMSIntensity/nested-AMSIntensity naming
that in EVERY other class so far has meant a real ctx-dynamic-indexed
modulation-slot array, direct disassembly confirms ALL 17 of
`CSTGCommonLFO`'s own candidates are plain fixed-K reads off `this` --
`ctx` is never dereferenced by any of them. Third confirmed instance of
"verify empirically, don't infer structure from a method's name" in
this family (after `CSTGMS20EG` and `CSTGOrganOsc`'s own precedents).

**Tooling note, 2 fresh DEF_RE gotcha instances this batch, both the
already-known "bare paren in prose" class, one with a NEW trigger
shape**: `stg_common_lfo_valuegetters.cpp`'s own leading comment hit
the standard bare-word-plus-paren trigger ("methods (ctx is never
dereferenced..."), swallowing `GetWaveform` (the file's own first real
function) exactly as documented in every prior batch's version of this
bug. `stg_effect_balance_valuegetters.cpp` hit a NEW variant of the
SAME underlying regex flaw: a fully BALANCED, empty pair of parens in
prose -- `` `ResolveActivePerformanceVarsManager()` is itself... `` --
still triggers it, because `DEF_RE`'s captured-params group
(`[^;{}]*`) doesn't stop at any `)` character, only at `;`/`{`/`}` --
so a matched pair of parens offers ZERO protection, the runaway match
just keeps extending through the literal `)` and everything after it
until the next real brace. Updated standing rule: "zero literal `(`
characters in prose before the first real code construct" applies to
EVERY `(`, balanced or not -- there is no such thing as a "safe"
parenthetical in a file DEF_RE will scan. Both fixed by rewording to
remove the parens entirely (em-dash/comma-delimited clauses), caught
via the standard 2-check discipline (comment open/close-count balance,
which both files passed cleanly since the bug is orthogonal to
brace/comment balance -- only the exact DEF_RE captured-name-set diff
catches this class of bug) before ever attempting to build.

**KAT-oracle gotcha found and fixed before shipping, distinct from any
prior code bug**: the independent Python evaluator's own helper for
reading dword fields (`buf[base+off:base+off+4]`) silently returns a
TRUNCATED (or empty) slice when `base+off+4` exceeds the oracle
script's own `BUFSZ`, and `int.from_bytes()` of a short/empty slice
just returns a smaller/zero integer with NO error -- this produced
`CSTGEffectBalance`'s draft expected values as all-zero (offsets
0x2128/0x212c/0x2130, `BUFSZ` was only 0x2000) before being caught by
inspection (all-zero KAT expectations are themselves a red flag worth
checking, not just trusting) and fixed by widening `BUFSZ` to safely
exceed every offset used. Worth flagging for any future oracle script
in this family that computes offsets much larger than earlier classes'
typical range (`CSTGEffectBalance` and `CSTGEffectRack` both index tens
of KB into the fixture buffer) -- always size the oracle's own buffer
from the MAXIMUM offset actually used, not a reused constant from a
smaller prior class.

`make verify`: exit 0, 0 FAIL lines, 188 test binaries total (up from
54 at some earlier point in this project's own history -- the delta
reflects OA.ko's many other unrelated families too, not just this
one), all 7 new suites (54 checks) passing, including 2 explicit
branch-coverage checks beyond the family's usual single-fixed-index
convention: `CSTGEffectRack::GetValueAlgorithm` re-exercised at
`ctx.index` = 3/12/14 to independently hit all 3 of
`ResolveEffectSlotRecord`'s banks, and `CSTGToneAdjust::
GetValueSwitchValue` re-exercised at 2 indices landing on a clear vs
set bit of the same fixed word field. Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos`: clean link,
`OA.ko` 523852 bytes (up from batch 17's 515908), zero warnings/errors
traceable to any of the 7 new files. `DECOMPILE_ERRORS.md` unchanged --
no compile/link blocker hit. `manifest/gen_oa_manifest.py` regenerated,
reconstructed-status count 2472 -> 2526, delta exactly +54, confirmed
via a full reconstructed qualified-name-set diff -- 0 regressions.
Committed as 20 files (Makefile + 2 edited mega-headers + 3 new headers
+ 7 new `.cpp` + 7 new test `.cpp`); `git diff --cached --stat`
verified immediately before commit per
[[shared_repo_commit_hygiene]] -- 2 concurrent-session Eva files
(`../Eva/tools/build_gdbserver.sh`, `../Eva/tools/gdbserver-i386-musl`)
were sitting untracked in `git status` throughout and correctly left
untouched.

**Next targets**: re-run the batch-18 broader-discovery recipe fresh
for the next batch -- 75 sweep-visible classes total, 68 now
accounted for (57 done across all batches + `CSTGPCMModelPatch`/
`CSTGPanOutput`/`CSTGComponent`/`CSTGAnalogSyncModelPatch` excluded +
17 already-modeled + `CSTGTG92OscBase` deferred), so roughly 7 more
sweep-visible classes may remain unexamined -- re-run the full recipe
rather than assuming the count above is exhaustive, this batch did not
exhaustively cross-check every one of the 75 by name. Also worth a
follow-up sweep pass specifically for the OTHER sibling context types
spotted but not yet investigated this batch:
`CSTGDrumkitMessageContext`, `CSTGEffectMessageContext`,
`CSTGHDRTrackMessageContext`, `CSTGProgramSlotMessageContext`,
`CSTGWaveSeqDataMessageContext` -- grep
`nm -C $KO | grep '::Get.*<ContextType>&)'` for each directly rather
than relying on the sValueGetterTemp sweep alone, since a class using
one of these might route through a different shared scratch temp or
skip the convention's sink entirely (unconfirmed either way).

## Batch 19 (2026-07-28, commit `739d72a`): CSTGHDRTrack + CSTGWaveSequence, 49 methods

**Confirmed the batch-18 sweep is exhausted for real this time.** Re-ran
the exact batch-18 recipe (`objdump -dr` cross-reference of every
relocation targeting `CSTGParamsOwner::sValueGetterTemp`, grouped by
enclosing function, classes ranked by method count) fresh against the
same ground truth binary and got the identical 75-class list, byte-for-
byte the same class names and counts as batch 18's own table -- every
one of the 75 already accounted for (done/excluded/already-modeled/
deferred) except the still-open `CSTGTG92OscBase` pure-virtual deferral
from batch 12. Zero new classes from this specific method this batch.

**Follow-up per batch 18's own open item, and the real find this
batch**: batch 18 flagged 5 sibling `*MessageContext` types spotted but
never individually checked (`nm -C $KO | grep -oE
'[A-Za-z_][A-Za-z0-9_]*MessageContext'` lists them all). Checked each
directly this batch via `nm -C $KO | grep -oE
'[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*\(CSTG<Type>MessageContext
&?\)$'` (anchored at the closing paren so extra-arg methods don't match)
rather than relying on the `sValueGetterTemp` sweep at all -- this is a
genuinely INDEPENDENT discovery axis, useful whenever the
`sValueGetterTemp` sweep itself goes quiet, since some real getters
apparently route through no shared sink at all (see the piecewise/
UUID-copy shapes below) yet still keep this exact context-typed
signature shape:
  - `CSTGDrumkitMessageContext` -> `CSTGDrumKitData` (30 real weak
    candidates, `Getter*` naming). Investigated but NOT picked this
    batch -- see below.
  - `CSTGEffectMessageContext` -> ~250 real weak/strong hits across
    ~70 DIFFERENT effect-DSP classes (`CSTGStereoCompressor`,
    `CSTGReverbGate`, dozens more), but ALL named `InitMeters`/
    `RefreshMeters`/`ResetMeterVars`/`UpdateMeterData`/`UpdateXxxCoeff` --
    a completely different, real-time audio-meter/coefficient-update
    mechanism, not this family's value-getter convention at all (no
    `Get`/`Set` prefix on any of them). Confirmed NOT part of this
    family -- do not re-investigate, this is a large unrelated
    subsystem.
  - `CSTGHDRTrackMessageContext` -> `CSTGHDRTrack` (15 real weak
    candidates, all `GetValue*`). Reconstructed this batch, see below.
  - `CSTGProgramSlotMessageContext` -> `CSTGProgramSlot` (~75 real weak
    `GetValue*`/`GetChord*` candidates). `CSTGProgramSlot` is the
    already-established "SKIP -- large, heavily-annotated real struct
    with a modeled vtable" class from the very first batches of this
    family; this just reconfirms that verdict via a different discovery
    path, still correctly out of scope.
  - `CSTGWaveSeqDataMessageContext` -> `CSTGWaveSequence` (34 real weak
    `Getter*` candidates). Reconstructed this batch, see below.

**`CSTGDrumKitData` investigated, deliberately SKIPPED (not excluded --
a real future target)**: sampled `GetterDrumkitLevel`'s own disassembly
directly. It's a genuinely complex PIECEWISE multi-dimensional index
computation -- combines a bank byte (`this+0x1c`) scaled by 0x200
(`shl esi,9`) with a note-index dword (`this+0x4`) premultiplied by 25
(`lea`/`lea` chain) plus a further `note*2` term, THEN adds a separate
velocity-zone term computed via `imul ecx,[this+0x18],0x10302`, sums
all three, and finally indexes into the 17.3MB `_unrecovered[0x1143530]`
blob already declared in `oa_global.h` (see that class's own header
comment -- a confirmed 273 x 129 x 8 legacy multisample-bank UUID
table). This is real entanglement, not mere prior existence -- the
established "always check the actual struct's size/complexity before
defaulting to skip" rule (batch 18) cuts the OTHER way here: the class
genuinely IS the giant already-opaque blob the rule warns about, unlike
batch 18's own small ctor-stub classes. Left as a documented, scoped-out
future target requiring a dedicated 3-dimensional-index decoder
extension, not attempted this batch.

**`CSTGHDRTrack` (15/15) reconstructed, zero outliers.** Was previously
a FULLY opaque, raw-offset-only embedded sub-object (16 instances at a
confirmed 0x2c stride inside `CSTGSequence`, see that class's own header
comment in `oa_global.h`) -- no standalone struct existed anywhere in
this project before this batch, a genuinely fresh class same as
`CSTGCommonEffectLFO`/`CSTGEffectBalance`/`CSTGMetronomeSettings` from
batch 18. New header `include/oa_stg_hdr_track.h` (matching the
"genuinely fresh -> own header" convention, as opposed to extending an
existing in-place stub).

Field layout independently cross-checks against `CSTGSequence`'s own
already-confirmed ctor: it zeros exactly 3 bytes per HDRTrack slot at
`+0x4`/`+0x5`/`+0x6` -- this batch's own field discovery independently
lands `GetValueOutputBus`/`GetValueFXCtrlBus`/`GetValueHDRBus` (all
signed bytes) at those SAME three offsets. A real, useful cross-check
technique: when a class's OWN embedding ctor already documents which of
its bytes get zeroed, check whether your own newly-discovered field
offsets land on exactly those bytes before trusting either independently.

**Genuinely new combined shape**: `GetValueSolo` reads NEITHER `this`
nor `ctx.index` -- `mov eax,ds:CSTGControllerRTData::sInstance` (a REAL,
already-declared external singleton pointer, `oa_global.h`'s own
`CSTGControllerRTData::sInstance`, not a private raw-selector
reimplementation like `CSTGEffectBalance`'s own precedent), then
`movzx ecx,[edx+0x18]` (ctx's own per-call byte, NOT the family's usual
`+0x4` slot), then `movzx eax,WORD[eax+0x24]` (a WORD read off the
GLOBAL object, not `this`), `sar eax,cl` (variable shift by the
ctx-derived amount, x86's own 5-bit shift-count masking applied
explicitly since C++ doesn't guarantee it), `and eax,1`. This combines
TWO shapes the family has each seen SEPARATELY before -- global-
singleton indirection ignoring `this` (`CSTGEffectBalance`, batch 18)
and a per-call ctx-derived variable bit-shift amount (`CtxShift`,
`CSTGVPMModelPatch`, earlier batch) -- into one method, and is the first
confirmed case of EITHER shape reading its shifted data through a REAL,
already-declared external symbol rather than a private lookup or a
fixed field on `this`. No new C++ struct/global declaration was needed
since `CSTGControllerRTData::sInstance` already existed from earlier,
unrelated project work.

**`CSTGWaveSequence` (34/34) reconstructed, zero outliers.** This class
already had a real, hand-confirmed ctor (`waveseq_setlist_init.cpp`,
predating this family entirely) but a totally EMPTY struct body
(`struct CSTGWaveSequence { CSTGWaveSequence(); };`, zero named fields,
zero vtable dispatch modeled) -- extended IN PLACE in `oa_global.h`,
matching batch 18's own precedent for small pre-existing stubs
(`CSTGCombi`/`CSTGEffectRack`/`CSTGToneAdjust`/`CSTGCommonLFO`) rather
than moved to a dedicated header.

Two groups: 17 methods index a per-step record array whose element 0
begins at `this` itself (`this + ctx.index*0x34 + K`); 17 more never
touch `ctx` at all, plain fixed-K fields at `this+0x4..0x13` (three of
them -- `RunSequence`/`NoteOnAdvance`/`TimeTempoMode` -- packed as
independent single-bit booleans into byte `0x4`, the established
shift-then-mask shape).

**New instruction FORM for the ctx-index premultiply, not a new
effective stride**: `imul edx,[edx+0x4],0x34` -- a direct 3-operand
`imul` immediate multiply. Every PRIOR ctx-index premultiply in this
family used either a `lea`-chain (stride 5/9/25 etc) or a bare SIB
scale on the final load (stride 1/4) -- this is the first confirmed
case of the compiler emitting a literal 3-operand immediate `imul`
instead, for effective stride 0x34 (52). Purely a new confirmed
instruction encoding, not a new conceptual shape -- modeled with the
same `this + idx*stride + K` arithmetic as every other ctx-indexed
class, no decoder generalization needed (this class's own `.cpp` just
inlines `int idx = (int)ctx.index;` directly rather than using a shared
`CtxIndex` helper, since `CSTGWaveSeqDataMessageContext` -- like
`CSTGMessageContext`, `CSTGEffectRack`'s own context type -- declares
`index` as a real NAMED field rather than requiring a raw byte-offset
read).

**Reconfirmed the CSTGDrumKitData-style "field genuinely overflows the
nominal per-record stride" quirk on a SECOND class.** Several of
`CSTGWaveSequence`'s own confirmed per-step field offsets (up to
`+0x47`) exceed the 0x34-byte (52) nominal stride between two adjacent
step records -- i.e. one step's own confirmed field set genuinely
overlaps into the next step's own leading bytes. Reproduced verbatim
via raw per-method address arithmetic exactly as disassembled, NOT
"fixed" into a padded record type -- same treatment as
`CSTGDrumKitData`'s own already-documented instance of this exact
quirk class (see that class's own header comment). Worth treating as a
recognized, expected pattern in per-step/per-record ctx-indexed classes
generally now that it's been seen twice independently, not a red flag
to "fix."

**Genuinely new shape, first raw multi-dword struct-copy in the
family**: `GetterBankSelect`/`GetterBankSelectUUID` -- byte-identical
bodies, confirmed via two independent isolated re-dumps, not a
copy-paste mixup -- copy a 16-byte UUID (4 sequential dwords at record
offset `0x14`/`0x18`/`0x1c`/`0x20`) directly into `sValueGetterTemp`'s
own `+0x0`/`+0x4`/`+0x8`/`+0xc` bytes, in place of the usual
`.value`/`.displayValue` write. No `STGConvertedParam` struct change was
needed: that struct's own already-declared `_unrecovered_a[0x0c]` gap
(`+0x04..+0x0f`) already covers exactly those 3 extra dword slots, so
this is a raw pointer-cast 4-dword copy against bytes the struct already
declares as writable, not a new named field. Same conceptual family as
`CPianoOsc`'s own still-open `GetBankIdAndStereoFlag` outlier from
batch 4 (both are "bank select returns a UUID, not a scalar") but a
DIFFERENT, much simpler mechanism here -- this class's own UUID lives
directly in ITS OWN per-step record, no delegate call into another
still-unreconstructed class needed, so it was decodable in place rather
than excluded as an outlier.

**2 fresh DEF_RE parenthesis-swallow gotchas, both caught before
compiling via the standard exact-name-set diff**: `stg_wave_sequence_
valuegetters.cpp`'s own leading comment for the BankSelect pair had
"16-byte record UUID (4 sequential dwords at record offset..." --
swallowed `GetterBankSelect` ENTIRELY into a bogus capture named `UUID`
(confirmed via `got=={"UUID", ...}` missing `GetterBankSelect` from the
expected 34-name set). `stg_hdr_track_valuegetters.cpp`'s own
`GetValueSolo` comment had "per-call bit index read from ctx's own
+0x18 byte (masked to 5 bits..." -- same trigger shape, swallowed
`GetValueSolo`. Both fixed the same established way, removing the
literal `(` (em-dash/comma-delimited rewording). Also ran the same
exact-name-set diff directly against the `oa_global.h` EDIT itself this
batch (not just the new standalone files) via a before/after DEF_RE
capture-set diff on the whole file -- confirmed 0 added, 0 removed
captures, i.e. the large new comment block inserted into that
already-huge, heavily-cross-referenced file introduced no new gotcha
despite the file's own size and density. Worth doing this "diff DEF_RE
captures across the whole file before/after" check specifically
whenever editing an ALREADY-EXISTING shared header in place (as opposed
to a fresh file), since the count-based per-file check used for new
files doesn't directly apply when a file already has other legitimate
captures in it.

**Manifest counting convention clarified this batch** (a real point of
confusion worth documenting): `manifest/gen_oa_manifest.py`'s own
printed summary line counts RAW REconstructed ROWS (some `qualified_name`
values repeat across overloads/duplicate address entries), while this
memory file's own running batch-to-batch narrative -- and
`PROJECT_BRAIN/status.md`'s own convention -- counts UNIQUE
`qualified_name` strings (a strictly smaller number, since it collapses
duplicates via `set()`). This batch's own delta was clean either way
(+49) but the baseline shifts depending which convention is used:
2548->2597 by raw row count, 2526->2575 by unique-name count. Use the
UNIQUE-name count (matching `status.md`'s own historical numbers) when
reporting the family's running total, but don't be surprised if a raw
`gen_oa_manifest.py` run reports a different, larger number for the
same state -- both are internally consistent, they're just counting
different things.

`make verify`: exit 0, 0 FAIL lines, 208 test suites total (up from 188
at batch 18 -- other unrelated project work happened in between,
confirmed via `oa_global.h`'s own comments referencing "batch 45"/
"batch 55" work on `CSTGCombi`/`CSTGSequence` ctors, a DIFFERENT,
project-wide batch counter than this family's own local batch
numbering -- don't conflate the two when reading header comments). Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` Kbuild build:
clean link, `OA.ko` 530640 bytes, zero warnings/errors traceable to
either of the 2 new files (only the harmless
`-Werror-implicit-function-declaration`-in-the-invocation-line false
positive, same as every prior batch's own build-log grep). Manifest
delta exactly +49 (15+34), 0 regressions -- confirmed via `git stash`
(stashing just this batch's own changed/new files), regenerating the
manifest against the TRUE prior committed state, then `git stash pop`
and regenerating again, rather than trusting whatever `manifest/
oa_functions.csv` happened to already contain on disk (which can be
stale/uncommitted leftover state from a different, unrelated prior
session -- this generated CSV is untracked by git, so there is no
`git diff` to fall back on for it the way there is for real source
files).

**Next targets**: `CSTGDrumKitData` (30 real candidates) is the
best-documented next target -- full piecewise-index derivation already
captured in this batch's own entry above, ready to decode once a
3-dimensional ctx-index helper shape is designed (bank term `<<9`, note
term via `this+0x4` premultiplied by 25 plus `note*2`, velocity-zone
term via `[this+0x18]*0x10302`, all summed then added to a fixed base
`+0x14e`). `CSTGTG92OscBase`'s pure-virtual deferral from batch 12
still open. The `sValueGetterTemp` whole-binary sweep is confirmed
re-exhausted (75/75 accounted for) -- future batches should default to
the per-context-type `nm` sweep demonstrated this batch (grep each
`*MessageContext` type's own `Get*(Type&)`-shaped mangled suffix
directly) rather than re-running the `sValueGetterTemp` sweep again
without a specific reason to expect it's changed.

See [[ckg_bankmanager_class_facts]]/[[ckg_seq_backup_technique]] for the
sibling family this one's decoder was adapted from, and
`HARDWARE_REVIEW_LOG.md`'s "CSTGString value-getter family",
"CSTGOrganModelPatch + CSTGMS20 value-getter families",
"CSTGAnalog4PoleBase + CSTGPolysix + CSTGAnalogSyncOsc value-getter
families", "CPianoOsc + CSTGEPModelPatch value-getter families",
"CSTGOrganOsc + CSTGVPMOsc + CSTGMS20ModelPatch value-getter families",
"CSTGPolysixModelPatch + CWaveMotionOsc + CSTGPianoModelPatch
value-getter families", "CSTGMultiFilter2Pole + CSTGMS20EG +
CSTGPolysixMG value-getter families", "CSTGAMSMixerBase + CSTGStepSeq +
CSTGPitchMod value-getter families", "CSTGSimple2Pole +
CSTGVPMModelPatch + CSTGVPMTG92Osc value-getter families",
"CSTGEG + CSTGPanOutputBase + CSTGPianoLPF value-getter families",
"CSTGAmp + CSTG3BandEQBase + CSTGEGBase value-getter families",
"CSTGVPMOutputMixer + CSTGKeyTrack + CSTGPortamentoBase value-getter
families", "CSTGDriver + CSTGVPMNoise + CSTGAnalog4Pole +
CSTGPluckedModelPatch + CSTGMOSSAmp + CSTGPitchModOsc value-getter
families", "CSTGSimpleAMSMixer + CSTGPitchModCommon +
CSTGPitchModCommonPlusAMS + CSTGVPMEG value-getter families", and
"CSTGVPMFilter + CSTGPitchModOscBase + CSTGTG92Osc + CSTGPitchModBase
value-getter families" entries for the full per-batch derivation notes.

## Batch 20 (2026-07-28, commit `3b5fd69`): CSTGTG92OscBase partial (1/10), family growth confirmed exhausted

**Both established discovery methods reconfirmed fully exhausted, with
fresh independent evidence, not just re-trusting batch 18/19's own
verdict.** Re-ran the whole `sValueGetterTemp`-relocation sweep from
scratch against the same ground-truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`): `objdump -dr` (104 seconds),
awk-scanned for the most recent enclosing `<function>:` label at every
line referencing `sValueGetterTemp`, `sort -u` -> 1462 unique enclosing
functions, `c++filt` + group-by-class-name -> the IDENTICAL 75-class list
batches 18/19 already found, same names, same per-class hit counts
byte-for-byte. Cross-referenced all 75 against this memory file's own
accumulated done/excluded/already-modeled/deferred registry: 62 done, 9
already-modeled elsewhere, 2 harmless false positives
(`CSTGParamsOwner` itself via its own `DummyGetter` self-reference, the
static-initializer stub), and exactly 2 genuinely open --
`CSTGTG92OscBase` and `CSTGDrumKitData`. Separately reconfirmed the
per-context-type axis (batch 19's own method) is ALSO exhausted: `nm -C
$KO | grep -oE '[A-Za-z_][A-Za-z0-9_]*Context\b'` finds exactly the same
8 real `*MessageContext` types batches 18-19 already investigated
(`CSTGPatchMessageContext`, `CSTGMessageContext`,
`CSTGProgramMessageContext`, `CSTGToneAdjustMessageContext`,
`CSTGDrumkitMessageContext`, `CSTGEffectMessageContext`,
`CSTGHDRTrackMessageContext`, `CSTGProgramSlotMessageContext`,
`CSTGWaveSeqDataMessageContext`) plus 2 unrelated non-class matches
(`GetDefaultContext`/`PrepareContext`-family function names, `PrepareEffectMessageContext`
turned out to be a real function name not a class) -- no 9th context
type exists anywhere in the binary. **This is the first batch to
establish, with fresh cross-checked evidence rather than an assumption
carried forward, that NEITHER established discovery method can yield any
further class from this ground-truth binary.**

Per this batch's own task-level exclusion guidance (entangled classes
needing large opaque blobs, `CSTGDrumKitData` given as the standing
example), `CSTGDrumKitData` was deliberately NOT attempted this batch --
left exactly as batch 19 documented it (full 3-dimensional
piecewise-index derivation already captured in that entry), still the
one real, attemptable next target whenever it's picked up.

**`CSTGTG92OscBase` revisited with fresh, independent evidence rather
than trusting the batch-12 writeup verbatim.** `objdump -r -j
.rodata._ZTV15CSTGTG92OscBase $KO` dumps the class's own vtable's
relocations directly: raw offset 0xd4 (the slot all 9 of the class's
ctx-indexed candidates dispatch through via `mov edx,[eax]; call
[edx+0xd4]` before their own stride-25 ctx-index field read) resolves to
`__cxa_pure_virtual`, confirming batch 12's finding stands. Went one step
further than batch 12: grepped the WHOLE symbol table for every one of
the 9 blocked candidates' own method names --
`GetReverse`/`GetBankType`/`GetStartOffset`/`GetBankSelectUUID`/
`GetBottomVelocity`/`GetCrossfadeCurve`/`GetCrossfadeRange`/
`GetMultisampleNum`/`GetLevel` -- across every OTHER class in the binary.
Zero concrete overrides found anywhere (the only same-named hits were
`CPianoOsc`'s own unrelated `GetBankType`/`GetBottomVelocity`/
`GetMultisampleNum` and a handful of other classes' own unrelated
`GetLevel`, none of them subclasses of `CSTGTG92OscBase`). This binary
genuinely never instantiates a concrete subclass of `CSTGTG92OscBase`
through any symbol visible to static analysis -- the deferral is not
merely "not yet resolved," it is provably unresolvable from THIS
binary's own evidence alone; would need either a different ground-truth
binary revision with the concrete subclass linked in, or external
knowledge of which STG oscillator type owns the override.

The class's 10th real candidate, `GetFreqOffset`, does NOT touch the
vtable at all -- confirmed via direct disassembly to be the family's
single most common shape: a plain signed dword field read straight off
`this+0xc`, dual-write (both `.value` and `.displayValue`). Reconstructed
as a deliberate 1-of-10 PARTIAL class
(`include/oa_stg_tg92_osc_base.h`, `src/engine/
stg_tg92_osc_base_valuegetters.cpp`) -- same established precedent as
every other partial class in this family (`CSTGEGBase` 5/19, `CSTGAmp`
7/10, `CPianoOsc` 46/53, `CSTGOrganOsc` 13/36), not a new pattern.

**Tooling note, DEF_RE gotcha caught and avoided BEFORE it happened this
time** (not caught-and-fixed after a failed check, but pre-emptively
avoided by drafting with zero literal `(` before real code from the
first pass, then verifying with the standard exact-name-set diff to
confirm): the header's leading comment initially listed the 9
pure-virtual method names in a parenthetical aside, which would have
been a certain DEF_RE trigger per the by-now well-established
bare-word-then-paren pattern -- rewritten with em-dashes before ever
running the check, confirmed clean on the first real verification pass
(`got=set()` for the `.h`, since it's declaration-only; `got=={"CSTGTG92OscBase::GetFreqOffset"}`
matching exactly for the `.cpp`). Comment open/close-count balance also
clean (2/2 for the `.h`, 1/1 for the `.cpp`).

`make verify`: exit 0, 0 FAIL lines, the 1 new KAT check passing. Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` Kbuild build:
clean link, `OA.ko` produced (530868 bytes, up from batch 19's 530640),
zero warnings/errors traceable to the new file (only the by-now-standard
harmless `-Werror-implicit-function-declaration`-in-the-invocation-line
false positive, same as every prior batch's own build-log grep).
`DECOMPILE_ERRORS.md` unchanged -- no compile/link blocker hit, and this
is a RECONFIRMATION of an already-logged Tier-B deferral (batch 12), not
a new one. `manifest/gen_oa_manifest.py` regenerated against the TRUE
prior committed state via `git stash -u` / `git stash pop` (not trusting
the on-disk CSV, which is untracked and can be stale) -- 2575 -> 2576
unique reconstructed qualified names (raw-row convention: 2597 -> 2598),
delta exactly +1, confirmed via a full before/after set diff showing
exactly one added row (`CSTGTG92OscBase::GetFreqOffset`) and 0
regressions.

Committed as 5 files (`Makefile`, `HARDWARE_REVIEW_LOG.md`, 1 new header,
1 new `.cpp`, 1 new test `.cpp`); `git diff --cached --stat` verified
immediately before commit per [[shared_repo_commit_hygiene]] -- the same
2 concurrent-session Eva files (`../Eva/tools/build_gdbserver.sh`,
`../Eva/tools/gdbserver-i386-musl`) were sitting untracked in `git
status` throughout, still untouched by this or any prior batch of this
family.

**Next targets**: `CSTGDrumKitData` (30-31 real candidates depending on
sweep variant) is now THE ONLY remaining open item under either
established discovery method -- full piecewise-index derivation already
captured in batch 19's own entry above (bank term `<<9`, note term via
`this+0x4` premultiplied by 25 plus `note*2`, velocity-zone term via
`[this+0x18]*0x10302`, all summed then added to a fixed base `+0x14e`,
indexing into the already-declared 17.3MB `_unrecovered[0x1143530]` blob
in `oa_global.h`). If a future batch decides to take it on, design a
3-dimensional ctx-index helper shape rather than trying to force it
through the existing 1-D `CtxIndex`/`CtxShift` helpers. Beyond that class,
the 75-class `sValueGetterTemp`-sweep list is now fully accounted for (62
done, 9 already-modeled elsewhere, 2 harmless false positives,
`CSTGTG92OscBase` at 1/10 done with the rest provably blocked, and
`CSTGDrumKitData` pending) -- finding any FURTHER class beyond these 75
requires a genuinely new discovery axis (e.g. hunting for fully-inlined
value-getters with no standalone symbol at all, per batch 17's own
speculation, never attempted), not another pass over either method used
so far. The remaining gap against the original ~180-class estimate for
the whole family (roughly 100+ classes never surfaced by either sweep)
may simply not be recoverable from this specific ground-truth binary via
any symbol-table-based method -- most likely fully inlined into their
callers with no trace left in the symbol table.
