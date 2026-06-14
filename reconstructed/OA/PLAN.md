# OA.ko (OA_322.ko) — drop-in source reconstruction: plan

OA_322.ko is the Korg Kronos firmware-3.2.2 "OA" module: the **entire STG synthesis
engine plus the copy-protection / authorization layer**, compiled as one C++ kernel
module (Linux 2.6.32.11 + RTAI, x86-32, `g++ -mregparm=3 -fno-exceptions -fno-rtti`).

**Scale: 22,195 functions, ~20 MB, hundreds of C++ classes.** Producing a *compilable,
faithful, drop-in* source base for all of it is a marathon, not a sprint. This plan
makes the marathon tractable: it is **staged**, **prioritised by value**, **provably
complete** (a manifest tracks every one of the 22,195 functions), and **verified per
function** (each reconstructed function is mechanically checked against the original).
A `/loop` advances it every 4 hours.

## Guiding principles

1. **Build on the annotation already done.** Earlier passes restored `this` on ~16,700
   instance methods, rebuilt `CSTGEngine`/`CSTGGlobal` structs, cleaned the ABS-equate
   symbol pollution, and added module-wide name-derived comments. Reconstruction
   transcribes from *that* improved decompiler state, not from raw bytes.
2. **Bottom-up.** Reconstruct leaves (no defined callees) first, then functions whose
   callees are all done. The call-graph layering (computed earlier: 12,530 leaves,
   layers 0..36) drives the order so every function is written with its callees already
   typed.
3. **Value first.** Within the bottom-up order, pull the **copy-protection/auth**
   subsystem forward — it is bounded (dozens of functions) and is the single
   highest-value target. The engine breadth follows.
4. **Faithfulness is verified, not asserted.** Every function gets a mechanical
   equivalence check (below). "Done" means "verified", not "transcribed".
5. **Always compilable.** The tree builds at every checkpoint; new units are added to
   the build only once they compile against the recovered headers.

## Verification methodology (how we *prove* faithful capture)

Three layers, from cheapest/broadest to strongest:

### A. Completeness — the coverage manifest
`manifest/oa_functions.csv` lists **all 22,195 functions**: address, name, class/
namespace, size, call-graph layer, in-cycle flag, and `status`
(`pending → reconstructed → compiles → verified`). Generated from Ghidra and
regenerated each fire. This is the ground truth that *nothing is skipped* — "every
function and class" is a checkbox, not a hope. A function is not finished until its row
reads `verified`.

### B. Structural faithfulness — recompile + BSim (scales to all 22k)
1. Compile the reconstructed source with the **same toolchain and flags** as the
   original (matching gcc, `-mregparm=3 -fno-exceptions -fno-rtti -O?`, the Kronos
   2.6.32.11 kernel headers) → `OA_recon.ko`.
2. Import `OA_recon.ko` into Ghidra; for each function compute its **BSim** feature
   vector and compare to the original OA_322.ko function's vector (the project already
   runs a BSim Postgres + `archive_ingest_*`). Score ≥ ~0.95 ⇒ structurally faithful;
   lower scores flag a function for rework.
3. Cross-check with `diff_functions` (basic-block count, edges, call targets) and a
   side-by-side decompile diff for any function below threshold.

### C. Behavioural faithfulness — emulation + known-answer tests (for the functions that matter most)
- **Pure / leaf functions** (math, DSP kernels, table lookups, the crypto primitives):
  drive identical inputs through (a) the original via Ghidra `emulate_function` and
  (b) the reconstructed function compiled to a host harness, and assert identical
  outputs over a fuzzed/boundary input set.
- **Crypto/auth**: Blowfish-CFB, MD5, base32, the GMP `powm`/`invert` maths, and the
  NV2AC chip transcript are validated against **known-answer vectors** and against the
  original's emulated behaviour — this proves the authorization path bit-exactly, which
  is the part that must be perfect.

A function reaches `verified` when: it compiles (A), its BSim score clears threshold
(B), and — if it is pure or security-critical — its emulation/KAT check passes (C).

## Staged construction order

| Stage | Scope | Why / verification emphasis |
|---|---|---|
| **0. Foundations** | source tree, build harness, **manifest generator**, BSim/emulation **verify harness**, export the recovered **type model** (all class structs → headers) | makes every later stage measurable + compilable |
| **1. Copy-protection / auth** | `InitCdromSupport` (magic `0x22fb39cc`), `/proc/.oacmd` handler, `AuthorizationStrings` (base32 + Blowfish-CFB + MD5 + chip secret), NV2AC chip I/O, GMP RSA-ish maths | **highest value, bounded**; verified by KAT + emulation (layer C) |
| **2. Shared utilities** | leaf math/tables, `CSTGQuad`/list primitives, `CSTGBankMemory` heap, TMP allocators | the substrate everything else calls; verified by BSim + emulation |
| **3. Engine core** | `CSTGEngine`, `CSTGGlobal`, the managers (audio/voice/MIDI/effect/HDR/file/streaming) | already partly annotated; BSim |
| **4. Voice models & DSP** | PCM, Organ, Plucked, MS20, Polysix, VPM, Piano, EP models; effects; FFT | the bulk; BSim, with emulation on the DSP kernels |
| **5. Breadth sweep** | every remaining manifest row until 100 % `verified` | closes the long tail |

## Layout

```
reconstructed/OA/
  PLAN.md            this document
  README.md          architecture overview (grows per stage)
  include/           recovered type model (per-class / per-subsystem headers)
  src/<subsystem>/   reconstructed .cpp, mirroring the staged order
  manifest/          oa_functions.csv (coverage) + per-stage progress
  verify/            BSim/emulation harness + KAT vectors + results
  Makefile / Kbuild  builds OA_recon.ko against the Kronos kernel tree
```

## Per-fire cadence (what each 4h `/loop` tick does)

1. Re-open OA_322.ko; regenerate/refresh the manifest; read this plan + the campaign
   memory note for current position.
2. Advance the current stage by a bounded batch of functions (decompile → transcribe →
   compile → verify → mark `verified` in the manifest), checkpointing into git under
   `kronosology/`.
3. Update README/manifest progress; commit. Never leave the tree non-compiling.

## Honest expectation

This is a very large effort; a single fire produces a *verified increment*, not the
whole engine. Progress is measured by the manifest's `verified` count climbing toward
22,195, auth first. The verification harness guarantees that whatever is marked done is
genuinely faithful — correctness is never traded for coverage.
