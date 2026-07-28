---
name: rtparm-ge-table-scripted-decoder-2026-07-28
description: "InitializegRTParmFunctionTable_GE reconstructed (34,435-byte mechanical table init, 313 entries), manifest 3471->3472/21689, commit d12aec1. Scripted-decoder technique for large static-table-initializer functions, at 10x the scale of the prior STG value-getter/CKG seq-backup decoders."
metadata:
  type: project
---

## What was done

`InitializegRTParmFunctionTable_GE()` (.text+0x56b18d, 34435 bytes, cdecl,
no args) -- the "strongest next candidate" flagged by
[[ckg_engine_per_rtparam_table_2026-07-28]]'s own survey. Populates
`gRTParmFunctionTable_GE` (.bss+0x630f40, size 0x30e8 = 12520 bytes) with
313 40-byte (0x28) records via 3756 individual `mov [table+off], value`
stores. Zero branches, zero calls anywhere in the function -- purely
mechanical.

New files: `include/oa_rtparm_ge_table.h`, `src/engine/rtparm_ge_table.cpp`,
`verify/test_rtparm_ge_table.cpp`, `verify/rtparm_ge_func_stubs.cpp`.
Commit `d12aec1`. `make verify`: 218/218 green. Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos`: clean link,
`OA.ko` 787900 bytes, `gRTParmFunctionTable_GE` .bss size matches ground
truth exactly, all 313 RT_* symbols show the expected `*UND*` pattern.
Manifest 3471 -> 3472/21689 (+1, `address+name` match).

## Entry record layout (0x28 = 40 bytes, 313 entries)

```
+0x00 dword  funcPtr   -- always a real, unique RT_* function pointer (313/313 distinct)
+0x04 dword  field04   -- plain int, 0..45 observed, meaning unconfirmed
+0x08 word   index     -- SELF-INDEX: index == entry position, for ALL 313 entries
+0x0a byte   field0a   -- 1..15 observed
+0x0b byte   field0b   -- 0..45 observed
+0x0c byte   field0c   -- boolean-shaped, only 0 or 1 observed (43/313 are 1)
+0x0d..0x0f  (3 bytes implicit padding, never written)
+0x10 dword  field10   -- 10..27786 observed
+0x14 dword  field14   -- 0..27435 observed, 169/313 nonzero
+0x18 dword  field18   -- 0..27643 observed, 72/313 nonzero (a contiguous block starting ~index 171, wav-parameter-adjacent)
+0x1c dword  field1c   -- 0 in ALL 313 entries
+0x20 dword  field20   -- 0 in ALL 313 entries
+0x24 dword  field24   -- 0 in ALL 313 entries
```

Field semantics beyond funcPtr/index are NOT independently confirmed --
consistent with an RT-parameter-descriptor table (min/max/default/UI-
resource-ID, same family as [[ckg_engine_per_rtparam_table_2026-07-28]]'s
PERT/GERT tables) but flagged as TODO in the header, not asserted.

The 313 callees are heterogeneous C++ signatures (2-4 args, mixing
`unsigned char`/`char`/`short`/an `RTParmBufferSelect` enum) -- confirmed
via `c++filt` on all 313 mangled names before writing anything, NOT
assumed uniform. Stored as `void *`, cast at declaration time via each
function's own real prototype (`(void *)&RT_xxx`), matching how the
compiler itself emits these as raw relocated addresses regardless of
callee signature.

## Technique: scripted decoder for a 34KB mechanical table (10x scale-up from prior decoders)

1. `objdump -dr -M intel --start-address=X --stop-address=Y` over the
   EXACT function range (confirmed via the next function's own start
   address -- `InitializegRTParmFunctionTable_PE` began at exactly
   `0x56b18d + 0x8683`, a clean boundary with zero overlap, itself a
   good sanity check that the size in the manifest is trustworthy).
2. Regex-parse every `mov (BYTE|WORD|DWORD) PTR ds:0xOFF,0xIMM` line +
   any following `R_386_32` relocation line(s). A dword write with a
   SECOND relocation (destination symbol + a distinct function symbol)
   is a function-pointer field; one relocation only (destination) is a
   plain immediate, even if the immediate happens to be 0.
3. `entry_index, field_offset = divmod(printed_offset, ENTRY_SIZE)` maps
   every write to its slot. Zero parse errors and zero missing fields
   across all 3756 writes was itself the first correctness signal --
   a genuine off-by-one in entry size or field layout would show up
   immediately as duplicate-field or misaligned-offset errors, not
   silently.
4. Generate the header (struct + extern decls, mechanically derived from
   `c++filt` demangled signatures) and source (`static const` array
   literal + a 2-line runtime copy loop into the real `.bss` array) via
   a Python codegen script -- never hand-transcribe entries at this
   scale (3756 individual field values).

## Verification, layered (do this whenever a table this large is claimed "reconstructed")

1. **Byte-count cross-check** (cheapest, do this FIRST): sum each write
   opcode's real encoded instruction length (BYTE mov = 7B, WORD mov =
   9B, DWORD mov = 10B on x86-32 with a 32-bit relocated
   displacement) times entry count, plus prologue/epilogue, and compare
   against the function's own manifest-listed size. Exact match (34435)
   before touching a single field value confirmed the entry SHAPE
   (2 dword + 1 word + 3 byte + 6 dword per entry) independently of the
   parser.
2. **A second, independently-implemented decoder** with a genuinely
   different derivation path -- not just a second run of the same
   script. Here: decoder #1 trusts objdump's printed `ds:0xOFF` text and
   computes entry/field via arithmetic on it; decoder #2 ignores that
   text, walks instructions in pure POSITIONAL order (12 fixed slots
   per entry, one entry at a time) and only then ASSERTS the offset it
   independently computed matches what decoder #1 read from text. If a
   parsing bug in either had swallowed/duplicated/reordered an
   instruction, this would diverge. Confirmed 0 discrepancies across all
   3756 values -- this is the single strongest piece of evidence for a
   table this size, stronger than any number of spot checks alone.
3. **Manual spot checks straight from the raw disassembly TEXT** (not
   decoder JSON output) for a handful of scattered entries (0, 17, 171
   here) -- catches a bug that could exist in BOTH decoders if they
   shared a subtle misunderstanding (they don't share code, but they DO
   share the same reading of "what a relocation pair means").
4. **Mangling round-trip**: compile the generated extern declarations
   standalone and diff the resulting object's own R_386_32 symbol table
   against ground truth's mangled name list. An EXACT match across all
   313 is strong, cheap, automatic evidence that every declared
   signature (arg count, arg types, arg order) is correct -- a single
   wrong type anywhere would mangle differently and show up as a diff,
   not a link error (nothing is being linked yet at this stage).
5. **KAT test**: spot-checked entries (funcPtr identity + all fields) +
   full-313-entry structural invariants (redundant self-index, known-
   always-zero fields, known nonzero-counts for boolean-shaped fields)
   + a full-table checksum (weighted sum over all portable/non-pointer
   fields, independently precomputed in Python and cross-checked against
   the C++ runtime recomputation) + pairwise funcPtr distinctness. The
   checksum step is what gives FULL 313-entry coverage beyond the spot
   checks without embedding 313 literal entries twice.

## Design choice: don't replay 3756 statements verbatim

Ground truth's own fully-unrolled form (one `mov` per field, no loop) is
almost certainly the compiled-and-unrolled result of either a hand-
written per-parameter registration macro list, or a `for` loop over a
`const` template array with a small-enough trip count that some GCC
version fully unrolled it -- either way, NOT a shape worth replaying
literally in reconstructed source. Instead: `static const` 313-entry
struct-literal array (one line per entry, still git-diffable/reviewable)
+ a genuine 2-line runtime `for` loop copying it into the real `.bss`
array. This produces bit-identical final table contents (verified by the
KAT) while being reviewable; flagged explicitly in the header comment as
a deliberate source-shape choice, not a claim about the original
source's own structure.

## Gotcha: citing a NOT-reconstructed sibling/caller's address in prose false-triggers the manifest's ADDRESS heuristic

`manifest/gen_oa_manifest.py`'s address heuristic matches ANY
`\.text\+0x[0-9a-f]{4,8}` text anywhere under src/include, unconditional
on which function that address actually belongs to. Writing
"`InitializegRTParmFunctionTable_PE (.text+0x573810, ...) -- NOT
reconstructed by this pass`" in a doc comment -- explaining
non-reconstruction -- is exactly the string shape the heuristic credits
as evidence FOR reconstruction. Caught by the delta-diff step (regenerate
manifest, diff old vs new CSV, expect EXACTLY the intended entries to
flip) -- initially showed a false +2 (the sibling table function AND the
caller `BirthOfKarma()`, both cited by real address to document
provenance). Fixed by rephrasing those two citations as "ground-truth
offset 0x..." instead of "`.text+0x...`", leaving the real target
function's own genuine `.text+0x56b18d` citation untouched (correctly
credited). **General rule: before regenerating the manifest, grep your
new files for `.text+0x` and confirm every hit is either (a) the actual
function you reconstructed, or (b) rephrased to avoid the literal
pattern if it's citing a sibling/caller/callee you did NOT reconstruct.**
Always diff the manifest CSV before/after and confirm the delta is
EXACTLY the expected entry set -- this is what caught it here.

## Gotcha: a caller-search regex with a stray `\b` produces a false "no caller found" negative

First relocation-table grep for callers of
`InitializegRTParmFunctionTable_GE` used
`grep "InitializegRTParmFunctionTable_GE\b"` -- but the real mangled
symbol is `_Z33InitializegRTParmFunctionTable_GEv`, so the `\b` sits
right before the trailing `v` (a word character), which is NOT a word
boundary, so the pattern never matched anything and the search silently
returned zero hits. Almost wrote up "no caller found in ground truth" as
a real finding before re-running a background full-disassembly grep
without the `\b` and finding the real caller (`BirthOfKarma()`,
`.text+0x507d0c`, calls this immediately before
`InitializegRTParmFunctionTable_PE()` under a one-time-init guard).
**General rule: when grepping mangled C++ symbol names for a plain
identifier substring, don't anchor with `\b` on the end that abuts the
mangled suffix (type-encoding letters right after the name) -- verify a
"zero hits" result isn't a regex artifact before treating it as a
finding, especially before writing a "confirmed dead/unreachable" claim
into a memory file.**

## Open follow-ups

- `InitializegRTParmFunctionTable_PE()` (.text+0x573810, 5505 bytes) --
  the sibling table, SAME exact record shape at 50 entries instead of
  313. Should be a near-mechanical re-run of this same decoder script
  with the address range changed. Strong next candidate.
- The 313 RT_* callees themselves -- a large, real, separate family
  (KARMA RTParm "GE" domain parameter handlers), all confirmed real and
  distinct via symtab, none reconstructed. `BirthOfKarma()` itself
  (7177 bytes, the real caller of both Initialize*FunctionTable*
  functions) is also still pending.
- `ConvertPerfKarmaToX2100()` (12127 bytes) -- the OTHER candidate this
  session's survey flagged, not yet investigated beyond size/name.
