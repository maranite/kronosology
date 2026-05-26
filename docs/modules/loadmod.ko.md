# loadmod.ko — Boot-Integrity Kernel Module

The kernel module that establishes the Kronos's trust chain. It hooks `register_cdrom`,
`init_cdrom_command`, and several syscalls so that anything that "looks at the CD-ROM
the wrong way" returns either magic values (when integrity holds) or junk (when it doesn't).

| Property | Value |
|---|---|
| Path on device | `/sbin/loadmod.ko` |
| Source path | `dump from kronos/sbin/loadmod.ko` |
| Architecture | x86 LE 32-bit kernel module (ET_REL) |
| Size | ~51 KB |
| Functions | 63 (per `nm`) |
| C++ symbols | none — pure C with deliberately C-style API |
| Versioned in Ghidra | Yes (was checked out for analysis in earlier sessions) |

See **[`../modules/loadmod.ko_analysis.md`](../modules/loadmod.ko_analysis.md)** for the detailed function-by-function
inventory, error codes, and crypto primitives (NLFSR / Blowfish / MD5 / RSA / LCG). The
brief summary below is for context within the broader docs.

---

## Role

`loadmod.ko` is the *root of trust*. It:

1. Validates the Korg-patched Linux kernel itself (modified `register_cdrom`/`init_cdrom_command`
   that return magic values)
2. Computes the `pairFact` keys from the Public ID + an internal RSA computation, used to
   decrypt the `/korg/Eva` and `/korg/Mod` loop devices
3. Stores the magic value `0x22FB39CC` at the dword `g_pCdromDrvInfo + 5` so that anything
   subsequently asking "are we OK?" sees it
4. Hooks `init_cdrom_command` so that commands with code `0xA0F3` return `-42` (the magic
   return value) instead of normal CD behaviour
5. Once installed, makes the system appear to be a genuine, integrity-verified Kronos

`OA.ko::InitCdromSupport` reads back both magic values to confirm `loadmod.ko` is healthy.
See [`../modules/OA.ko_auth.md`](../modules/OA.ko_auth.md) for that handshake.

---

## Magic values

| Value | Type | Where set | Where read |
|---|---|---|---|
| `-42` (`0xFFFFFFD6`) | i32 return value | `loadmod.ko`'s hooked `init_cdrom_command` for command `0xA0F3` | `OA.ko::InitCdromSupport` |
| `0x22FB39CC` | dword | `loadmod.ko` writes at `g_pCdromDrvInfo + 5` after integrity | `OA.ko::InitCdromSupport`, `CSTGEngine::Initialize`'s degradation gate |

If either is absent or wrong, `OA.ko` enters audio-degradation mode (see [`OA.ko.md`](OA.ko.md#patches)).

---

## Crypto inside loadmod.ko

| Primitive | Why |
|---|---|
| NLFSR | Used in `pairFact` derivation (custom Korg construction) |
| Blowfish | Encrypts/decrypts the loop-device key |
| MD5 | File-integrity sub-checks |
| RSA (via GMP — see [`STGGmp.ko.md`](STGGmp.ko.md)) | Public-key verification of the kernel module signature |
| LCG | PRNG for masking |

Details in [`../modules/loadmod.ko_analysis.md`](../modules/loadmod.ko_analysis.md).

---

## Patches

There **is** a 3-bypass patch to `loadmod.ko` in this project (added 2026-05-26 when we
needed to ship a patched `loadoa`). Patched MD5: `28d1cec16f1d893f1d78241b62a989d9`.

The patched loadmod doesn't change *functionality* — it bypasses the integrity gates that
would otherwise reject any modification to `/sbin/loadoa` (which **is** in the MD5 file
list). The cryptoloop key derivation still runs naturally and `BuildCdromCommandStruct()`
still gets called, so cryptoloop mounts work normally.

| # | File offset | Bytes (orig → new) | What it bypasses |
|---|---|---|---|
| 1 | `0x572d` (8B) | `85 c0 0f 85 a3 00 00 00` → `90 × 8` | Outer `TEST EAX,EAX; JNE` after `VerifyCodeIntegrityMd5` — error code 1 path |
| 2 | `0x57b1` (2B) | `75 47` → `90 90` | Outer `JNE` after `RetrieveSecurityICKey` — error code 5 path |
| 3 | `0x3fb0` (6B) | `0f 85 e7 fe ff ff` → `e9 1e 01 00 00 90` | First `JNE` *inside* `RetrieveSecurityICKey`'s 16-byte hash check — replaced with `JMP +0x11e` to start of success path (`GetRandomBytesWrapper` call) |

**Patch #3 is the non-obvious one.** The same 16-byte hash check that
`VerifyCodeIntegrityMd5` does is repeated *inside* `RetrieveSecurityICKey` right before
`BuildCdromCommandStruct()` is called. Without bypassing it too, `BuildCdromCommandStruct`
is never called, so cryptoloop key globals stay empty and `mount` fails with
`VFS: Can't find an ext2 filesystem on dev loopN`. The status file lies — it reads `0`
because we NOPed the outer gate at #2, but the boot dies later. See
[`../modules/loadmod.ko_inner_md5_check.md`](../modules/loadmod.ko_inner_md5_check.md).

Why we need to patch loadmod at all: `/sbin/loadoa` is one of 446 paths in loadmod's MD5
list. Our patched loadoa (which redirects `/korg/Mod/OA.ko` → `/sbin/OA.ko`) would
otherwise fail loadmod's check 1. See [`../modules/loadmod.ko_md5_check_files.md`](../modules/loadmod.ko_md5_check_files.md)
for the full list and the LCG-decode script.

Deployment: see [`../workflow/deploying_patches.md`](../workflow/deploying_patches.md).

See also [`../workflow/patch_guide.md`](../workflow/patch_guide.md) and [`../modules/loadmod.ko_ground_truth.md`](../modules/loadmod.ko_ground_truth.md)
(ground-truth notes from kronoshacker.blogspot.com).
