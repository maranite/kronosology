# STGGmp.ko — GMP (GNU Multiple Precision) Wrapper

A kernel-mode wrapper around (or a stripped re-implementation of) parts of the GMP
big-integer library, used by `loadmod.ko` for RSA-style public-key arithmetic during
boot integrity verification.

| Property | Value |
|---|---|
| Path on device | `/sbin/STGGmp.ko` |
| Source path | `dump from kronos/sbin/STGGmp.ko` |
| Architecture | x86 LE 32-bit kernel module (ET_REL) |
| Size | ~75 KB |
| Functions | 71 (per `nm`) |
| C++ symbols | none — pure C |

---

## Role

Exports `__gmpz_*` symbols (`__gmpz_init`, `__gmpz_add_ui`, `__gmpz_mul`, `__gmpz_powm`,
`__gmpz_tdiv_r`, `__gmpz_clear`, `__gmpz_init_set_str`, `__gmpz_init_set_ui`,
`__gmpz_mul_2exp`) — i.e. the standard GMP integer API for arbitrary-precision arithmetic.

These are consumed by `loadmod.ko` to perform RSA operations (`m^e mod n` is exactly
`__gmpz_powm`). This is how loadmod verifies kernel-module signatures and computes
parts of the `pairFact` derivation.

---

## Why it's its own module

Linux kernel modules can't be linked against userspace `libgmp.so` — the entire GMP
arithmetic core had to be wrapped (or re-implemented) in a loadable kernel module. Korg
clearly chose to bundle it as its own .ko so that multiple consumers (`loadmod.ko`, and
potentially others) can share one copy.

---

## Status

| Item | Status |
|---|---|
| Phase 1 prototypes | N/A — no C++ mangled symbols |
| Deep RE | Not pursued (it's standard GMP; the algorithms are public) |
| Documented | Stub |

We don't need to look inside `STGGmp.ko` for our project goals — anything we'd want to
do with the keys (e.g. forging an RSA signature for a custom kernel module) needs the
*private* key, which is not present on the device.
