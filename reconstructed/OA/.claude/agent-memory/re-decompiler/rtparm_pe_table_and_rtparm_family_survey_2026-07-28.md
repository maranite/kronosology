---
name: rtparm-pe-table-and-rtparm-family-survey-2026-07-28
description: "InitializegRTParmFunctionTable_PE reconstructed (GE table's sibling, 5505 bytes, 50 entries), manifest 3472->3473/21689, commit 77cb73f. Also: broad survey identified the ~99-function/~62KB RTParm free-function family (AssignRTParmFunction_*, DoRTParmMultiLogic*, RTParmNameManager, RevertRTSceneBuffers) as the next major dense cluster -- verified tractable, not yet attempted."
metadata:
  type: project
---

## What was done: PE table (the GE table's confirmed sibling)

[[rtparm_ge_table_scripted_decoder_2026-07-28]] flagged
`InitializegRTParmFunctionTable_PE()` as its own strongest open follow-up.
Confirmed and reconstructed this session: real ELF at
`/home/share/docs/ASM Docs/OA.ko/OA.ko` (`nm -S`: entry 0056d6e0, size
0x1581 = 5505 bytes exactly; `gRTParmFunctionTable_PE` .bss size 0x7d0 =
2000 = 50*0x28 exactly) -- both sizes independently confirm this is the
same function the manifest already knew about at a *different* address
numbering (manifest/ADDR_RE uses Ghidra's `0x10000`-based text offset,
`0x583810`; the raw ELF's own nm/objdump addresses are unrelated numbers
from a different tool -- **don't be alarmed when they don't match; trust
size agreement, and always cite the manifest/Ghidra-offset convention
`.text+0x573810` in prose, not the raw-ELF address**, exactly the
established GE-table convention).

New files: `include/oa_rtparm_pe_table.h`, `src/engine/rtparm_pe_table.cpp`,
`verify/test_rtparm_pe_table.cpp`, `verify/rtparm_pe_func_stubs.cpp`.
Commit `77cb73f`. `make verify`: full suite green (exit 0). Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos`: clean link,
`gRTParmFunctionTable_PE` .bss size matches ground truth exactly, all 46
non-null RT_* symbols show the expected `*UND*` pattern. Manifest 3472 ->
3473/21689 (+1, `address+name` match, delta verified exact via a grep for
the new `.text+0x` citations in the added files -- all 3 correctly cite
only the reconstructed function's own address, `0x573810`, and the one
`BirthOfKarma` citation was deliberately phrased as "ground-truth offset
0x507d0c" to avoid the [[rtparm_ge_table_scripted_decoder_2026-07-28]]
false-positive gotcha).

### Same record shape, DIFFERENT observed content (don't assume GE's
### invariants transfer)

Both tables share the exact 0x28-byte/12-field record layout, but the PE
table's actual field VALUES break two of GE's "always true" invariants:

- **4 entries (indices 5,6,7,8) have a literal NULL funcPtr** -- a single
  `R_386_32` relocation only (the base-array symbol), no second relocation
  for a function-pointer target, confirmed independently by both decoders
  and by direct raw-disasm inspection (not a parse artifact). GE's own
  313/313 entries were all real, distinct function pointers -- **do not
  assume a sibling table's funcPtr is always non-null just because the
  first one you find is.** Handled in the decoder by checking reloc COUNT
  (1 vs 2) rather than the printed immediate (which is 0 either way, since
  it's always relocation-filled at link time).
- **field20 == field24 for all 50 entries** (confirmed), but NEITHER is
  uniformly 0 (unlike GE, where field1c/field20/field24 were all 0 in
  every one of 313 entries) -- and field1c is NOT equal to field20/field24
  here despite the numeric closeness in some entries. Exact semantic
  relationship among field1c/20/24 remains unconfirmed -- flagged as TODO,
  not asserted.
- **field0c is boolean-shaped over {0,2}** (not GE's {0,1}) -- 47/50 read
  2; only indices 2,3,4 (the `RT_crb_xpose`/`RT_crb_xpose_oct`/
  `RT_crb_xpose_oct_5th` trio) read 0. A real, minority-case KAT check
  (`count(field0c==2)==47`, not a round number like 50) -- **when writing
  a structural-invariant test for a table like this, compute the expected
  count from the actual decoded data, don't guess "probably all of them"
  by analogy to the sibling table.** Caught by an initial wrong `want=50`
  assertion that failed on first test run (got=47) before the true
  distribution was checked.

### Same decoder gotcha as GE, worth re-flagging: raw text continuation
### lines carry an address prefix

The GE-table decoder script's continuation-byte regex (`^\s+[0-9a-f]{2}...`)
does NOT match a `mov`'s trailing-byte continuation line in this project's
`objdump -dr -M intel` output, because that line is NOT bare hex bytes --
it's `  ADDR:\tXX XX XX` (address + colon prefix, same as any other
disassembly line). Using the bare-hex regex silently drops the
continuation line's relocations from being associated with the parent
`mov`, which manifests as "funcPtr write missing expected reloc count" on
the very FIRST write in the function (the one immediately after
`push ebp; mov ebp,esp`, before any entry's real fields even start) --
a fast, obvious failure that caught the bug on first decoder run here.
Fixed regex: `^\s*[0-9a-f]+:\s+[0-9a-f]{2}( [0-9a-f]{2})*\s*$`. **If
starting a fresh scripted decoder for a THIRD sibling table (or any new
mechanical-table function), copy the corrected regex from
`src/engine/rtparm_pe_table.cpp`'s own decoder script (not preserved in
the repo, but reproducible from this note) rather than re-deriving it from
GE's write-up, which didn't call this pitfall out explicitly.**

### `*/`-in-prose gotcha recurred, same fix

Header-comment prose listing several RT_* name-prefixes ("...RT_pe_*/
RT_run/RT_qtz_*...") accidentally contained the literal 2-character
sequence `*/`, prematurely closing the block comment and producing a
cascade of "stray backtick"/"missing terminating '" errors starting several
lines later, at the NEXT real apostrophe in the doc-comment prose. Caught
immediately by the very first standalone compile of the generated header
(never reached the project tree with this bug). **General rule, worth
permanently generalizing beyond the one gotcha already documented for
CKontaktXml: before writing prose that describes multiple `xxx_*`-style
identifier prefixes inside a C/C++ block comment, grep the generated text
for the literal substring `*/` and rephrase (e.g. `xxx_...,` instead of
`xxx_*/`) if a hit lands anywhere before the comment's real closing `*/`.**

## Broad survey: next major dense cluster identified (NOT attempted this pass)

Per this session's own standing instruction to survey after finishing the
sibling table. Method used: `awk` over `manifest/oa_functions.csv` sorted
by `size_bytes` descending, filtered to `status==pending`. Top result is
`__static_initialization_and_destruction_0` (501 pending instances, GCC's
per-TU static-constructor-ordering boilerplate) -- **not a good cluster
target**: each instance calls real C++ constructors for that translation
unit's global objects, so reconstructing them usefully requires the
REFERENCED classes to already be modeled first, not the other way around;
skipped as a cluster (noted for awareness, not investigated further).

The next real finding, immediately visible once the size-sorted list is
filtered/grepped for name families rather than read as flat function-by-
function: **the RTParm free-function family**, `AssignRTParmFunction_Drm`
(8094B), `AssignRTParmFunction_Rpt` (1995B), `AssignRTParmFunction_Phs`
(1605B), `AssignRTParmFunction_Rif` (1073B), `AssignRTParmFunction_Rhy`
(4115B), `DoRTParmMultiLogicPE` (4556B), `IsRTParmFunctionSameGE` (3907B),
`RevertRTSceneBuffers` (8095B), `RTParmNameManager` class (ctor 1437B +
`Initialize` 767B + `GetRTParmNameString` 3643B), `LimitRTParmPairGE`
(1266B), `AssignRTParmGE`/`AssignRTParmPE` (936B/898B), plus ~85 more
smaller functions -- **99 functions total, ~62,318 bytes, 100% pending**,
confirmed via
`awk -F',' 'NR>1 && $6=="pending" && ($2~/RTParm/||$3~/RTParm/){c++;s+=$4} END{print c,s}'`.

This is a real, coherent, previously-unflagged-anywhere family (grepped
`HARDWARE_REVIEW_LOG.md` for `AssignRTParmFunction`/`DoRTParmMultiLogic`/
`RTParmNameManager`/`RevertRTSceneBuffers` -- zero hits, i.e. never marked
a dead end or deferred). Operates on `GenEffect*`/`Performance*`/`RTParm*`
pointer types (mangled param types confirmed via a handful of samples,
e.g. `GetRTParmIDFromGE(GenEffect*, RTParm*)`), matching the pattern
already established by `CKGEngine`'s own already-reconstructed
`StoreGERTParmMinMaxToBank`/`SetGERTParmMinMax`/etc. (a C++-method view of
adjacent territory) -- this family is the plain-C/`regparm(3)`-function
side of the same domain, natural continuation from today's GE/PE table
work.

**Tractability spot-check** (4 samples, real `objdump -dr -M intel`
disassembly, not assumed): `ResetDynRTParmWindow` (8B, single
`mov BYTE PTR [eax],0` -- trivial setter), `GetRTParmIDFromGE`/
`GetRTParmIDFromPE` (17B each, pure pointer-difference-and-shift index
math, no struct field dereference at all), `AdjustRTParmFunctionGE` (83B,
short, has real branches -- not yet fully traced). None of the 4 sampled
hit anything exotic (no unmodeled external class, no floating point, no
weird calling convention) -- consistent with this being a tractable,
well-scoped cluster similar in spirit to `CKGEngine`'s PERT/GERT work, NOT
yet independently confirmed at the scale of the largest members
(`RevertRTSceneBuffers` at 8095B, `AssignRTParmFunction_Drm` at 8094B,
`DoRTParmMultiLogicPE` at 4556B are unsampled and could still hide real
depth -- spot-check those specifically before committing to the whole
cluster in a future session, per this project's own "verify tractability
on a sample before committing to a whole cluster" standing rule).

**Left for a dedicated future session** -- not attempted this pass beyond
the tractability spot-check above. `GenEffect`/`Performance`/`RTParm`
struct layouts will need establishing or reusing (a `GenEffect_pub` opaque
forward-decl and a `GetGenEffect()` returning `unsigned char*` already
exist in `include/oa_ckg_module_param_msg_handler.h`/`oa_engine_init.h`
from prior CKGEngine work -- check those first before inventing new
layouts).
