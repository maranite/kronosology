---
name: rtparm-family-bottom-up-batch-2026-07-29
description: "56 of the ~99-function RTParm free-function family reconstructed bottom-up (8..402 bytes); 7 deliberately deferred with documented reasons; found+worked-around a real cross-header RT_run linkage conflict; 3 real KAT-caught bugs; manifest 3473->3529/21689, commits pending (this session)."
metadata:
  type: project
---

## What was done

Continuation of [[rtparm_pe_table_and_rtparm_family_survey_2026-07-28]]'s
own standing follow-up: the 99-function/~62,318-byte RTParm free-function
family that pass surveyed but didn't attempt. Per this project's "verify
tractability before committing to a whole cluster" rule, sampled the
family's LARGEST members first via real `objdump -dr -M intel`
disassembly (not just `nm -S` size) before committing to a bottom-up
approach: `AssignRTParmFunction_Drm` (real nm size 8294 bytes, NOT the
8094 the earlier survey's `awk` pass estimated -- always re-derive size
from `nm -C -S`, don't trust a size column copied from a different
pass's own summary) is a real jump-table dispatcher over
`GenMod`/`GenEffect`-relative `gKS` writes, referencing already-declared
`RT_*`/`GetFirstOnBit`/`AssignRTParmGE` symbols -- confirmed in-scope and
tractable in SHAPE, but genuinely too dense (8+ KB, many interleaved
per-case writes) to attempt in this pass's own budget. Worked bottom-up
from the family's smallest member (`ResetDynRTParmWindow`, 8 bytes)
instead, landing 56 total before diminishing returns / rising real risk
(see deferral list below) made stopping the right call.

New files: `include/oa_rtparm_family.h` (the shared header -- documents
the reverse-engineered data model: `GenEffect`/`Performance` own a
32-entry 8-byte-stride `RTParm` array at fixed offsets, `GenMod` owns a
`GenEffect*` plus two 0x28-stride `RTParmFunctionTable`-shaped
current/compare buffers, `gKS` -- the `.bss` KARMA-scene blob, 0x402a8
bytes -- holds per-module-slot buffers at `module*0x9d10 + {0x1f73c,
0x1fc3c}` plus several smaller fixed-offset tables; `RTParm` is
`{type,subId}` at +0/+1; `RTParmFunction` is `{valueSrc,handler,kindTag}`
at +0/+4/+8; `RTParmEdit` is 8 bytes with 3 real 16-bit fields at
+0/+4/+6), `src/engine/rtparm_family.cpp` (48 of the 56 members),
`src/engine/rtparm_ckgparamedit.cpp` (the other 3 CKG-class members, kept
separate -- see the linkage-conflict note below), `verify/
test_rtparm_family.cpp` (94 checks), `verify/test_rtparm_ckgparamedit.cpp`,
`verify/rtparm_family_stubs.cpp`.

## The 7 deliberately deferred members (all real disasm read, none guessed)

- `LimitRTParmEditValues`(130B)/`LimitRTParmEditValuesRow`(421B):
  interleaved TWO index computations (an edit-value array index AND a
  separate `GetRTParmDescriptorGE`-driven min/max-table index) feeding a
  branchy clamp with several early-exit paths that share labels across
  both the "sel==0" and "sel!=0" halves of the function. Needs a
  dedicated re-read with more time than this pass had left.
- `UpdateRTParmIfSame_GE`(238B): the single most confusing control flow
  sampled this pass -- a loop that checks BOTH pointer-identity ("does
  this `RTParm*` literally equal the current array slot's address") AND,
  on mismatch, falls back to a 3-byte content compare, with the
  content-compare's OWN failure path re-entering the loop at a DIFFERENT
  label than the pointer-match success path re-enters at. Real risk of
  silently swapping which path increments the loop counter.
- `GetRTParmModAndID`(264B): a dynamic linear/binary-ish range search
  against `gKS+0x1f40` with per-branch bound computations; this pass's own
  disassembly capture was ALSO incomplete at session end (tail not
  independently re-verified) -- doubly not safe to claim.
- `RTParmShortNameGroup::GetRTParmShortNameStringPtr`(235B): nested
  string-length/scan loops with several interacting cursors. The OTHER 3
  members of this class (ctor, `SetRTParmShortNameStringPtr`,
  `SetProductArrays`) ARE reconstructed -- this one stays declared-only.
- `DoRTParmMultiEnablePE`(291B)/`DoRTParmMultiEnableGE`(409B): a nested
  8-modules-x-up-to-8-rows loop with byte inversion (`not bl`), bit
  accumulation ACROSS TWO separate per-module `gKS` tables, and an
  early-break-on-unsigned-overflow condition buried mid-inner-loop
  (`cmp bx,si; ja <outer-continue>`). Densest control flow sampled this
  pass short of the still-untouched giants
  (`DoRTParmMultiLogic{GE,PE}`).

All 7 stay `pending`; ground-truth addresses/sizes are in
`include/oa_rtparm_family.h`'s own header comment for a focused
follow-up session to pick up directly (no re-discovery needed).

## A real, pre-existing latent header conflict (found + worked around, not fixed)

`include/oa_ckg_module_param_msg_handler.h` declares
`RT_run(unsigned char, unsigned char)` `extern "C"` (a DELIBERATE choice
for that file's own ~50-function block of enum-widened KARMA externs,
documented in its own header comment: enum params were widened to plain
`int` there, which would otherwise change the mangled name vs ground
truth, so the block sidesteps mangling entirely via C linkage).
`include/oa_rtparm_pe_table.h` declares the SAME symbol `extern "C++"`
with `regparm(3)` -- the real, GE/PE-table-verified mangled linkage
(`_Z6RT_runhh`). Including both headers in one TU is a hard compile
error (conflicting linkage). No prior file in this project ever needed
BOTH headers together (one file uses `CKGParamEdit`'s full class
declaration, the other uses `gRTParmFunctionTable_PE`), so this never
surfaced until this pass needed exactly that combination for
`CKGParamEdit::GetRTParmBufferSelectId`.

**Workaround, not a fix**: `CKGParamEdit::GetRTParmBufferSelectId` (and
the 2 `CKGSysExBuffer` methods, which have no conflict themselves but
travel with it since they're the same family) live in their own
translation unit, `src/engine/rtparm_ckgparamedit.cpp`, which only ever
includes `oa_ckg_module_param_msg_handler.h`. Own KAT binary
(`test_rtparm_ckgparamedit`) for the same reason. **Real fix would be**:
audit every actual call site of that ~50-function `extern "C"` block
(mainly `ckg_engine.cpp`) to confirm none depend on the literal
unmangled linker symbol resolving, then switch the whole block to
`extern "C++"`/`regparm(3)` matching real ground truth -- not attempted
this pass (touches a large, separately-verified file, out of this
batch's own scope). Logged in `DECOMPILE_ERRORS.md` too. **Worth a
project-wide audit** for other `RT_*`/`KS_*` externs with the same
extern-"C"-here/extern-"C++"-there split before the next cluster needs
both worlds again.

## 3 real bugs caught by this batch's own independent-oracle KAT (before landing)

1. **C++ integer-promotion pitfall, `~flag >> 7`**: ground truth's real
   op is an 8-bit-register `not bl; shr bl,7` (in
   `IsRTParmPairAssigned{GE,PE}`). Writing `(unsigned char)((~flag) >> 7)`
   in C++ is WRONG -- `~` promotes `flag` (an `unsigned char`) to `int`
   first, so the shift operates on a 32-bit-wide complement, not the real
   8-bit one (`flag==0` should give `~flag==0xff`, `>>7==1`; the
   unfixed C++ gives `~flag==0xffffffff`, `>>7==0x1ffffff`, truncated to
   `0xff`). Fix: truncate to `unsigned char` explicitly BEFORE shifting.
   **General rule for this whole codebase**: any time ground truth does
   `not <8-bit-reg>; shr <8-bit-reg>,N`, the C++ translation needs an
   explicit `(unsigned char)~x` (or equivalent width-correct cast)
   BEFORE the shift, not after -- C++'s own promotion rules will
   otherwise silently shift the wrong width.
2. **Packed-32-bit-field-vs-native-pointer, same class as `CKGBankManager::
   ms_poInstance`** (see `ckg_bankmanager_class_facts.md` /
   `send_exec_midiport_combi_song_2026-07-28.md`'s own prior instances of
   this exact bug class): `RTParmShortNameGroup::
   SetRTParmShortNameStringPtr`'s real body is `mov DWORD PTR[eax],edx` --
   a 4-byte pointer store on the real -m32 target. Writing
   `*(const void**)p = str;` in C++ stores a native 8-byte pointer on a
   64-bit host build, clobbering the NEXT field's own bytes too. Fixed
   via this project's established convention: `*(unsigned int*)p =
   (unsigned int)(unsigned long)str;`. **Recurring lesson**: any field
   this project's own disassembly shows as a plain `DWORD`-width store of
   what is semantically a pointer needs this exact treatment -- a native
   C++ pointer type on the field is ALWAYS wrong for a host KAT build
   here, regardless of how obvious/small the function looks.
3. **Wrong TEST assumption, not a source bug**: assumed
   `gRTParmFunctionTable_GE[0].funcPtr == &RT_bnd_amt` because that's the
   first `extern` LISTED in `oa_rtparm_ge_table.h`'s own header -- but
   that header's listing order is alphabetical/scan order from the
   ORIGINAL scripted-decoder work, NOT the real table's own initializer
   population order. Fixed by searching for entry `[5]`'s own real
   `funcPtr` self-consistently instead of asserting which named function
   occupies which index. **General rule**: never assume a table's
   `extern` DECLARATION order in a header matches its real runtime
   population order, even when that header itself is fully verified --
   the declaration list and the initializer are two independently-derived
   things.

## Verification

`make verify` full suite green (incl. 2 new binaries), and a real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build green.
`nm OA.ko | c++filt` spot-check confirms every new symbol's real mangled
name matches ground truth exactly (e.g.
`CKGSysExBuffer::SendParamsDependOnRTParm(CSKParameterChangeMessage*)`),
and the deliberately-`extern`-only siblings (`AssignRTParmGE`/
`AssignRTParmPE`/`GetFirstOnBit`/`KM_rtp_val_out_pe`/
`Do_KM_rtp_update_name`/`Do_KM_rtp_update_all_names`/`ScaleRTParmValue`)
show up as the expected `U` (undefined) entries with their own correct
real mangled names too -- confirming the linkage is right even where the
body isn't reconstructed yet. Manifest 3473 -> 3529/21,689 (+56, 0
regressions, verified via a full before/after name-set diff, not just a
raw count).

## Also worth knowing for the NEXT RTParm-family session

`GetRTParmFunctionTableEntry_{GE,PE}`'s real `kind==1` search compares
record offset **+0xa** (a packed `field0a`+`field0b` word) against the
search key, NOT the already-independently-verified `index` field at
+0x08 (`RTParmFunctionTableEntry_{GE,PE}`, cross-checked by two decoders
plus real KAT in the earlier GE/PE table passes -- high confidence that
offset is right). This pass implemented the +0xa read literally (raw
offset, not asserting it's secretly the same field) rather than trying
to reconcile the two -- worth a dedicated look at what +0xa/+0xb
(`field0a`/`field0b`) actually mean before the next RTParm session, since
this is the ONLY way the real "search by index" callers of these two
functions can ever succeed.
