---
name: stg-value-getter-family
description: OA.ko's largest known dense accessor family (~2300 pending methods, ~180 STG synth classes) -- STGConvertedParam &Get*(CSTGPatchMessageContext&) "value getter" convention; 6 classes done (CSTGString, CSTGOrganModelPatch, CSTGMS20, CSTGAnalog4PoleBase, CSTGPolysix, CSTGAnalogSyncOsc), 504 methods reconstructed, manifest 1441->1949
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

See [[ckg_bankmanager_class_facts]]/[[ckg_seq_backup_technique]] for the
sibling family this one's decoder was adapted from, and
`HARDWARE_REVIEW_LOG.md`'s "CSTGString value-getter family",
"CSTGOrganModelPatch + CSTGMS20 value-getter families" and
"CSTGAnalog4PoleBase + CSTGPolysix + CSTGAnalogSyncOsc value-getter
families" entries for the full per-batch derivation notes.
