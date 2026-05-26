# loadoa — Userspace OS Loader

The small statically-linked userspace binary that boots the Kronos synthesis engine.

| Property | Value |
|---|---|
| Path on device | `/sbin/loadoa` |
| Source path | `dump from kronos/sbin/loadoa` |
| Architecture | x86 LE 32-bit ELF executable (ET_EXEC) |
| Size | ~20 KB |
| Image base | `0x08048000` |
| Functions | 19 (per `nm`) |
| C++ symbols | none mangled — pure C |

---

## Role

`loadoa` is the bridge between the kernel boot and the synth engine. It:

1. Loads `GetPubIdMod.ko` (referenced by string `"/sbin/GetPubIdMod.ko"`)
2. Reads the Atmel chip Public ID
3. Computes the `pairFact` keys used to mount the encrypted `/korg/Eva` and `/korg/Mod`
   loop devices (this part is handled by `loadmod.ko`; `loadoa` orchestrates)
4. Loads `OmapNKS4Module.ko`, `STGEnabler.ko`, `STGGmp.ko`, `loadmod.ko`, then `OA.ko`
5. Hands off to `Eva` for the user-facing UI

It is the **only userspace** part of the integrity chain — once it's running, everything
else is in kernel space or in `Eva`.

---

## Status in this project

| Item | Status |
|---|---|
| Versioned in Ghidra | **Yes** (was checked out as needed) |
| Phase 1 prototypes | Skipped — no C++ mangled symbols to reconstruct |
| Auto-analysis | Done |
| Documented | At the depth this project requires (see `../system_overview.md` for the boot sequence it participates in) |

---

## Patches

`loadoa` IS patched in this project (added 2026-05-26). Two string substitutions redirect
the cryptoloop-located modules to `/sbin/`:

| Stock MD5 | Patched MD5 |
|---|---|
| `8a3d61f3332d7bcf694e8c05845b4754` | `d17c26036fa0f51f5f4c0ef2f6f424bf` |

| File offset | Original (16 B) | Patched | Effect |
|---|---|---|---|
| `0x3696` | `/korg/Mod/OA.ko\0` | `/sbin/OA.ko\0\0\0\0\0` | Loads patched OA from `/sbin/` |
| `0x39c0` | `/korg/Mod/KorgUsbAudioDriver.ko\0` | `/sbin/KorgUsbAudioDriver.ko\0×5` | Loads KorgUsbAudio from `/sbin/` |

Both replacements are shorter than the originals and padded with `\0`. The file size is
unchanged (20 789 bytes), which keeps loadoa's ELF layout valid.

Why we need this: it's the only way to substitute our patched OA.ko at boot time without
rebuilding the cryptoloop image. Without it, stock loadoa loads stock OA from the
cryptoloop and our patched `/sbin/OA.ko` sits unused.

This patch *would* be detected by `loadmod`'s `VerifyCodeIntegrityMd5` (loadoa is in its
file list), so deploying it requires the matching 3-bypass `loadmod.ko` patch — see
[`loadmod.ko.md`](loadmod.ko.md#patches).

Deployment: [`../workflow/deploying_patches.md`](../workflow/deploying_patches.md).

---

## See also

- [`../system_overview.md`](../system_overview.md) — full boot sequence
- [modules/loadmod.ko.md](loadmod.ko.md) — the kernel module `loadoa` loads first
- [modules/GetPubIdMod.ko.md](GetPubIdMod.ko.md) — the Atmel-chip module
