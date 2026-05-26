# STGEnabler.ko — Tiny Helper Module

A very small kernel module loaded early in the boot sequence. Its role is purely
infrastructural — it appears to enable kernel features needed by the rest of the
Korg synthesis stack.

| Property | Value |
|---|---|
| Path on device | `/sbin/STGEnabler.ko` |
| Source path | `dump from kronos/sbin/STGEnabler.ko` |
| Architecture | x86 LE 32-bit kernel module (ET_REL) |
| Size | **~5 KB** (the smallest .ko in the system) |
| Functions | 16 (per `nm`) |
| C++ symbols | none — pure C |

---

## Role

Best inferred guess: STGEnabler enables specific privileged kernel paths needed by
subsequent module loads (e.g. enabling NLFSR, granting `iopl` to a daemon, or unlocking
specific MSRs). Without source it is difficult to be more specific, and the module is
small enough that any conclusion drawn from a single Ghidra session is suspect.

It loads **before** `OmapNKS4Module.ko`, `STGGmp.ko`, `loadmod.ko`, and `OA.ko` in the
boot sequence (per `loadoa`).

---

## Status

| Item | Status |
|---|---|
| Phase 1 prototypes | N/A — no C++ mangled symbols |
| Deep RE | Not pursued |
| Documented | Stub — no project goal requires deeper analysis |

If you find yourself needing to understand STGEnabler in detail, it is a one-afternoon
job — the module is tiny and the function names exposed via `nm` will quickly point at
the role of each function.
