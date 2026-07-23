# Eva — drop-in source reconstruction: plan

`Eva` (`/korg/Eva/Eva`) is the Kronos userspace GUI/front-panel application: the Peg
widget toolkit, `CForm*` mode UIs, the `CSK*` model layer, and everything that isn't
synthesis DSP (that's all in `OA.ko`, already substantially reconstructed in
`reconstructed/OA/`). It is a normal dynamically-linked x86-32 ELF (`ET_EXEC`,
GCC 4.5.0, glibc-era), **not** a freestanding kernel module — architecturally very
different from OA.ko, so several OA conventions carry over unchanged and several don't
(see "What's different from OA.ko" below).

**Scale: 38,048 Ghidra-analyzed functions (41,986 nm-defined), ~22 MB, not stripped.**
Same marathon-not-sprint shape as OA.ko. This plan follows the same staged, prioritized,
manifest-tracked, per-function-verified structure that got OA.ko to a real `insmod`
success (`kronosology/reconstructed/OA/PLAN.md`) — adapted for a userspace boot target
instead of a kernel-module load target.

## Concrete goal (per user direction, 2026-07-22)

Get Eva's **exterior framework** — the boot path from `_start` through to the point
where the app is genuinely running (event loop live, not crashed, not stuck) — reachable
in `kronos_vm`, the same way OA.ko's `init_module()` reaching `return 0` was the concrete
milestone for that project. Full UI/CForm/Peg fidelity is explicitly **not** the goal of
this pass, same spirit as OA.ko's "audio DSP fidelity is out of scope" policy
([[oa_ko_rtai_virtualization_policy]]). Once Eva "reasonably boots" in the VM, the user
wants to move to validating dependencies against real hardware.

## What's different from OA.ko

1. **Userspace ELF, not a kernel module.** No Kbuild, no kernel headers, no `-mregparm=3`
   freestanding ABI, no RTAI. Ordinary `-m32` cross g++ against a *period-correct*
   glibc-family ABI. Ground truth links against: `libpthread.so.0`, `librt.so.1`,
   `libssl.so.6`, `libxml2.so.2`, `libz.so.1`, `libuuid.so.1`, `libstdc++.so.6`,
   `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`, `libcrypto.so.6`.
2. **Do not reuse the project's `musl-i386` toolchain** ([[kronos_i386_musl_toolchain]]).
   That precedent exists for *newly written* static tools that have no matching `.so` on
   the target and would otherwise trip glibc's kernel-version floor. Eva is the opposite
   case: the real `kronos.img` rootfs the VM boots already ships the exact old
   glibc/libstdc++/libssl/libxml2/libuuid this binary was built against (the real Eva
   runs there today). A reconstructed Eva should link dynamically against **those same
   on-image libraries**, not a fresh static musl build — mismatching would trade one ABI
   problem for a worse one (C++ exceptions/RTTI/iostream state across a libstdc++ version
   the rest of the image doesn't share). Confirming the exact on-image glibc version and
   getting a matching (or compatible-enough) cross-toolchain together is Stage 0 work,
   not assumed solved.
3. **No hardware-substitute virtual-driver pattern needed for the boot path itself.**
   Eva's only real coupling to anything hardware/kernel-shaped is IPC: RTAI FIFOs
   (`/dev/rtf0`, `/dev/rtf1`), `/dev/dmsg0` (STG direct-message device), and
   `/proc/.oacmd`. All three already exist as real, working interfaces once OA.ko is
   loaded in the VM (confirmed reachable — OA.ko's own `init_module` creates them; see
   [[oa_ko_rtai_virtualization_policy]]'s milestone note). Eva's IPC client code
   (`USTGUserAPI`) just needs to open real device nodes and speak the existing protocol
   — no new virtual endpoint to build, unlike OA.ko's AT88/NKS4/USB dependencies.
4. **Almost the entire binary is genuinely out of scope.** OA.ko's breadth (voice
   models, DSP) is Stage 4/5, deferred but still "the point" of that project. For Eva,
   the Peg UI toolkit and the ~150+ `CForm`/`CSK` mode classes are **not** the point of
   *this* effort — they're explicitly deferred indefinitely per the existing project
   scope note ([[kronos_project_scope_boundaries]]: "DSP/engine software completeness...
   is not" in scope; the analogous read for Eva is "UI feature completeness is not in
   scope, boot reliability is"). The manifest will show a very long tail of `pending`
   Peg/CForm functions that this plan does not intend to close.

## Guiding principles (carried over from OA.ko unchanged)

1. **Boot-path priority.** Same rule OA.ko settled into
   ([[oa_ko_rtai_virtualization_policy]]'s "init/boot-path reachability" follow-on):
   before picking any general/breadth reconstruction target, finish whatever `main()`'s
   own transitive call graph still needs. Only fall back to general sweeping once the
   known boot-path graph is stub-free.
2. **Bottom-up within that scope.** Reconstruct leaves first, then callers whose callees
   are already done, same as OA.ko's call-graph layering.
3. **Faithfulness is verified, not asserted.** Every function gets checked against the
   real decompile/disassembly before being marked done — same three-layer methodology as
   OA.ko (below), with layer B's BSim comparison against `Eva` instead of `OA_322.ko`.
4. **Always compilable.** New units only enter the build once they compile against the
   recovered headers, same as OA.ko.
5. **RTAI/audio-DSP virtualization policy applies here too.** Eva has zero audio/DSP
   code of its own (confirmed — [[oa_ko_rtai_virtualization_policy]] applies by
   inheritance, not by restating), but if any boot-path function turns out to block on
   something environment-specific and non-essential (a real touchscreen/backlight ioctl,
   a real front-panel LED write), stub it rather than block the boot milestone on it —
   same spirit as the "virtual class" substitution precedent.

## Verification methodology

Unchanged in shape from OA.ko ([[oa_ko_rtai_virtualization_policy]] /
`reconstructed/OA/PLAN.md`'s methodology section — read that for the full rationale, not
duplicated here):

- **A. Completeness** — `manifest/eva_functions.csv`, all 38,048 rows, same
  `pending → reconstructed → compiles → verified` states, regenerated from
  `Decomp/EVA_Decomp/eva_export/functions.csv` (already exists — see
  [[oa_ghidra_decomp_export]]'s sibling export) plus `symbols.csv` for demangled
  class::method names (`functions.csv` alone does not carry `::`-qualified names — confirm
  against `symbols.csv` before trusting a bare function-file name).
- **B. Structural faithfulness** — recompile + BSim against the real `Eva` binary
  (already loaded in the Ghidra Server `OA_Timeout_Test` repo per
  [[eva_oa_ghidra_coordination]], under `/EVA/Eva`).
- **C. Behavioural faithfulness** — KAT/emulation for pure functions; for Eva this
  matters far less than it did for OA.ko's crypto/auth path (Eva holds no secrets — see
  `docs/modules/Eva.md`'s security-model section) — expect layer C to be used sparingly,
  mostly for the `USTGUserAPI`/`/proc/.oacmd` wire-format code where getting a byte
  wrong would desync the IPC protocol.

## Staged construction order

| Stage | Scope | Why |
|---|---|---|
| **0. Foundations** | source tree (this layout), manifest generator, host build harness, resolve the linking-ABI question above, confirm real on-image shared libs are reachable/stageable in `kronos_vm` | makes later stages measurable + compilable |
| **1. Boot path** | `_start`→`main`→ CPU-affinity pin → `USTGUserAPI::Connect` (opens `/dev/rtf0`/`/dev/rtf1`/`/dev/dmsg0`) → `USTGAPILCDControl::LoadStoredSettings` → app-mode detection (`argv[0]` basename) → `CCommDriver::getInstance` → `COmegaInterface::Init` (spawns 6 `OmegaSchedulingThread`s + `SetConfigInfo`/`CKernel::InitSystemLayer`/`Mains`/`OmegaInitThread`/`OmegaTimingThread`) → SIGINT handler (`Ouch`) → `COmegaInterface::Close` | **the actual milestone target** — already traced this session, real named symbols throughout, no struct-layout guessing needed for most of it |
| **2. IPC/message substrate** | `USTGUserAPI`'s remaining send/receive methods, `CSTGHandle` (shared-memory handle), the STG message wire format Eva sends/receives — needed for the boot path to do anything beyond "not crash" (e.g. actually exchange one real message with OA.ko) | closes the gap between "process alive" and "process doing something real" |
| **3. CKernel / threading substrate** | `CKernel::CKernel`, `InitSystemLayer`, `OmegaSchedulingThread`/`OmegaInitThread`/`OmegaTimingThread` bodies, `SetConfigInfo`, `Mains()` | what `COmegaInterface::Init` actually calls into; scope each as encountered, deferring genuinely UI-only branches |
| **4. Peg toolkit substrate** | only if Stage 3 shows the boot path actually blocks on real UI bring-up (framebuffer open, font/resource load) rather than just spawning threads that idle | not assumed necessary yet — Eva.md's own note is that Peg is UI-only and vast; don't start here speculatively |
| **5. Breadth sweep** | everything else (CForm/CSK mode classes, dialogs) | explicitly out of scope for this effort; listed only so the manifest stays honest about total coverage |

## Layout

```
reconstructed/Eva/
  PLAN.md            this document
  README.md          architecture overview + boot-path findings (grows per stage)
  include/           recovered type model (per-class / per-subsystem headers)
  src/<subsystem>/   reconstructed .cpp, in staged order (boot path first)
  manifest/          eva_functions.csv (coverage), gitignored/regenerable
  verify/            host-side known-answer test harness (KAT + BSim results)
  Makefile           host compile-check (-m32 g++) + eventual real target link
```

## Honest expectation

38,048 functions is larger than OA.ko's 22,195, and this pass's scope is deliberately
much narrower than OA.ko's (boot path only, not "the whole engine eventually"). Progress
here is measured by "does the VM show Eva's process alive, past `COmegaInterface::Init`,
without a crash/hang" — a binary yes/no milestone, not a manifest percentage. The
manifest still tracks all 38,048 rows for honesty about total coverage, but stages 4-5
are not expected to move in this effort.
