# OA.ko (OA_322.ko) — reconstructed source

Drop-in source reconstruction of the Korg Kronos firmware-3.2.2 **`OA_322.ko`** — the
STG synthesis engine + copy-protection layer (Linux 2.6.32.11 + RTAI, x86-32, C++).

**22,195 functions.** This is a staged, verified, loop-driven effort — see
[`PLAN.md`](PLAN.md) for the full strategy and the per-function verification
methodology (coverage manifest + recompile/BSim + emulation/known-answer tests).

## Layout

- `PLAN.md` — the plan + verification methodology
- `include/` — recovered type model (class structs → headers)
- `src/<subsystem>/` — reconstructed `.cpp`, in staged order (auth first)
- `manifest/oa_functions.csv` — every function + `status` (regenerated from Ghidra; gitignored, regenerable)
- `verify/` — BSim/emulation harness + crypto known-answer vectors

## Progress

| Stage | Status |
|---|---|
| 0. Foundations (tree, manifest, plan, verify harness) | manifest ✅ · plan ✅ · harness ⏳ |
| 1. Copy-protection / auth | **cluster mapped** ⏳ (reconstruction next) |
| 2. Shared utilities | pending |
| 3. Engine core | pending |
| 4. Voice models & DSP | pending |
| 5. Breadth sweep | pending |

### Stage-1 auth cluster (mapped)

The copy-protection core, recovered from the symbol table + string references:

- **`CSTGKLMManager`** (Korg license manager) @ 0x2de10 — `AuthorizeProduct`,
  `AuthorizeVoiceModel/Effect/MultisampleBank/Builtins`, `IsAuthorizedVoiceModel/
  MultisampleBank/Effect`, `GetKLMAddressForPatch`, `RunKLM`.
- **`ProcessOACmd`** @ 0xa0c0 — the `/proc/.oacmd` command dispatcher
  (globals `sXCmd`, `sCdromCommand`, `sOACmdStatus`, `sOACmdResult`).
- **`SetupAtmelForAuthorizations`** @ 0x207a50 — Atmel NV2AC chip auth setup.
- **`CSTGInstalledEXProducts::AuthorizeProductByFilename`** @ 0x481d0,
  **`AuthorizeProductCallback`** @ 0x47fa0.
- **`InitCdromSupport`** @ 0x40 — cdrom anti-tamper (magic `0x22fb39cc`).
- Auth source: `/korg/rw/Startup/AuthorizationStrings`.
- Per-patch checks: `CSTG*ModelPatch::IsUsingAnyUnauthorizedMultisamples`.

This subsystem talks to the Atmel **NV2AC** security chip (via `stgNV2AC_*` in
`OmapNKS4Module.ko`) and uses **GMP** big-integer maths (`__gmpz_powm`/`invert` from
`STGGmp.ko`) — i.e. it ties together the two already-reconstructed modules.

## Driven by `/loop`

A 4-hourly session loop advances this: each tick reconstructs + verifies a bounded
batch of functions (auth first), marks them `verified` in the manifest, and commits.
