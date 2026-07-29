# RTParm family round 43 (2026-07-29, solo — no subagents)

Context: session hit its 200-subagent cap mid-cycle; user chose "continue
solo" for the standing decompile-everything goal. This batch was done
directly in the main assistant loop with Bash/Read/Edit/Write, no Agent
tool.

## What landed (commit follows this memory write)

Reconstructed the last 3 RTParm-family "pending" callees flagged by the
round-42 agent: `Do_KM_rtp_val_out_pe` (29B, trivial gate+tailcall),
`CountOnBits` (35B, trivial popcount), and `IsRTParmFunctionSameGE`
(3907B — the family's 5th-largest member).

## The `IsRTParmFunctionSameGE` native-execution-harness approach

This function has **zero relocations** (confirmed via `objdump -dr`) —
pure logic over `(kind, idx, b, c)`, no gKS/global touches at all. That
makes it a perfect candidate for direct native execution rather than
manual disassembly tracing:

1. Extracted the raw machine-code bytes for the function's address range
   directly from the saved `objdump -dr -M intel` dump (parsed the hex
   byte columns per instruction — no ELF file-offset math needed since
   there's nothing to relocate).
2. `gcc -m32` harness: `mmap(PROT_EXEC)`, memcpy the bytes in, cast to a
   `regparm(3)` function pointer, call directly.
3. Brute-forced **all** 16 kind values (0 always false; dispatch only
   handles 2,3,4,5,6,7,8,9,0xa,0xb,0xc,0xe) × 256 idx × 256 c, with
   `b==kind` (the only case that can ever return true — confirmed `ecx`/
   `b` is never touched past the initial `kind!=b` gate) = 1,048,576
   real executions in under a second.
4. Derived the group/range structure per kind from the resulting dump,
   then **verified every derived formula against the full dump
   computationally** (Python set-equality over all 256×256 pairs per
   kind) before writing a single line of the real C++ reconstruction.

Result: kinds 2,4,5,6,7,8,9,0xa,0xc,0xe are genuine symmetric
equivalence-class groups (cliques — any two members of the same listed
set are mutually "same"). Kind 3 is a "hub" shape (36 connects to
everything in [20,40]; 37/38/39/40 each ALSO connect to their own private
4-value sub-range). **Kind 0xb (11) is genuinely NON-symmetric in ground
truth** — e.g. `(idx=20,c=6)` is true but `(idx=6,c=20)` is false, a real
property of the compiled table (re-verified via direct harness
re-invocation on that exact pair both ways, not a dump artifact). Kind
11 is also the reason for the function's size: it's a FURTHER 22-way
sub-dispatch on `idx` inside the kind==0xb arm, each with its own
hand-authored compatible-value list — traced enough of the real
disassembly to confirm this shape, then let the exhaustive dump do the
rest rather than hand-transcribing all 22 sub-blocks. Reproduced as a
literal directed-pair static table (190 entries) taken verbatim from the
dump.

**Lesson for future sessions**: when a target function has zero
relocations, check that FIRST — it's the gate for whether the
`x86_direct_execution_oracle_technique.md` approach applies without any
relocation-patching complexity. This one had none, making the harness a
~15-line C file instead of the fuller ELF-relocation-aware version
described in that memory file.

## Test infrastructure fix required

The prior round's stub file (`rtparm_family_stubs.cpp`) had placeholder
bodies for these 3 now-real functions — had to delete them (link
collision: "multiple definition") and relocate the
`g_do_km_rtp_val_out_pe_calls` test counter from `Do_KM_rtp_val_out_pe`'s
(now-removed) stub into `KM_rtp_val_out_pe`'s stub (still `pending`,
still stubbed — `Do_KM_rtp_val_out_pe` now really tail-calls it). Also
had to re-verify (not just re-word) the `DoRTParmMultiEnableGE` test's
"every ge self-writes 0xff" assumption, previously justified by "stubbed
false" — now real, but the test's own setup memsets gKS to all-zero
first, meaning every slot's `kind` field reads as 0, which is
unconditionally false in ground truth too, so the expected values
happened to still be correct; only the comment needed updating.

## Verification

`make verify` full suite green (0 failures, all new checks added).
Real `make ko-clean && make ko KDIR=/home/build/linux-kronos` build
green. `nm OA.ko | c++filt` confirms all 3 new symbols' mangled names
match ground truth exactly. Manifest 3536 -> 3539/21689 (+3, 0
regressions, confirmed via before/after `gen_oa_manifest.py` count diff).

## Process note

Unlike the last 2 rounds' agents (one of which built+verified everything
but never ran `git commit`), this batch's commit is being made directly
by the main-loop assistant as part of the same turn that did the work —
no separate "did it actually commit" verification step needed here since
there's no agent hand-off.
