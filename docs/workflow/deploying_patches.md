# Deploying Patched Binaries to a Live Kronos

The procedure that gets our patched `OA.ko` (EX-bank auth bypass) onto a stock Kronos
and keeps it loaded across reboots. Verified end-to-end on 2026-05-26.

The recipe is encoded as a single self-contained shell script —
**[`patched/kronos_patcher.sh`](../../patcher/kronos_patcher.sh)** — that any
sufficiently-rooted Kronos can run to convert itself from stock to patched, or back.
This document explains *why* each step is there.

---

## Goal

Get the EX-bank authorization bypass live on a stock Korg Kronos so that any EX expansion
can be installed without per-device authorization strings, *while keeping the boot chain
functional*. Survives reboots and OS power cycles.

## The full chain

```
Power on
  └── kernel + busybox init
      └── /etc/inittab.busybox → /etc/OA.clonos.rc start  (NOT /etc/inittab → /etc/OA.rc)
          └── /sbin/loadoa
              ├── insmod rtai_*, STGEnabler, STGGmp, OmapNKS4Module, OmapVideoModule
              ├── insmod /sbin/GetPubIdMod.ko
              ├── insmod /sbin/loadmod.ko
              │     └── inside init_module:
              │         ├── VerifyCodeIntegrityMd5  (check 1; MD5 of 446 files)
              │         ├── RegisterFakeCdromDriver (check 2)
              │         ├── ReadPairFactAndVerify   (check 3; opens /.pairFact3)
              │         ├── RetrieveSecurityICKey   (check 4; Atmel + RSA; ends with BuildCdromCommandStruct)
              │         ├── installs HookedSysMount on _vmalloc syscall slot
              │         └── writes /tmp/stgStatus  (0 on success)
              ├── mount  /korg/ro/Mod.img → /korg/Mod   (cryptoloop, intercepted by loadmod)
              ├── insmod /korg/Mod/USBMidiAccessory.ko          (V2 variant)
              ├── insmod /korg/Mod/OA.ko                        ← PATCHED LOADOA: changes this to /sbin/OA.ko
              ├── insmod /korg/Mod/KorgUsbAudioDriver.ko        ← PATCHED LOADOA: changes this to /sbin/KorgUsbAudioDriver.ko
              ├── umount /korg/Mod
              ├── mount  /korg/ro/Eva.img → /korg/Eva
              ├── mount  /korg/rw/PCM/WaveMotion (cryptoloop)
              └── exec   /korg/Eva/Eva   (Eva spawns as user pocky)
```

Anything that breaks any link in this chain produces the `"System cannot start due to a
file system error"` reauth screen.

---

## The three pieces we touch — and why

### 1. `OA.ko` — the actual EX bypass

| | |
|---|---|
| Stock MD5 | `955636c2b11a70a1dbecefaaa7bd4f80` (always in `/korg/Mod/OA.ko` only) |
| Patched MD5 | `163550b60b7508b2c0ba1fd314b0b944` |
| Patch sites | 56 byte runs at file offsets `0x0bc44` – `0x5b52f0` (the 6 `IsUsingAnyUnauthorizedMultisamples` variants — see [OA.ko.md](../modules/OA.ko.md#patches)) |
| Where it lives | We copy `OA.ko` out of the cryptoloop (where it normally lives) into `/sbin/OA.ko` and patch it there. |

`/sbin/OA.ko` is **not** in loadmod's MD5 integrity list — so patching it here is invisible
to loadmod. The reason it lives in `/sbin/` is so our patched `loadoa` can find it without
needing the cryptoloop mounted at insmod time.

### 2. `loadoa` — the path redirect

| | |
|---|---|
| Stock MD5 | `8a3d61f3332d7bcf694e8c05845b4754` |
| Patched MD5 | `d17c26036fa0f51f5f4c0ef2f6f424bf` |
| Patches | 2 string substitutions (39 bytes total): `/korg/Mod/OA.ko` → `/sbin/OA.ko` and `/korg/Mod/KorgUsbAudioDriver.ko` → `/sbin/KorgUsbAudioDriver.ko` |
| Same-size? | Yes — the replacement strings are shorter, padded with `\0` to fit the original allocation. |

This is what makes the patched `OA.ko` actually get loaded. Without this, stock `loadoa`
still loads stock `OA.ko` from the cryptoloop.

### 3. `loadmod.ko` — three bypasses

| | |
|---|---|
| Stock MD5 | `d1697c9b1c478c0dcdfaef71516fe5f2` |
| Patched MD5 | `28d1cec16f1d893f1d78241b62a989d9` |
| Patches | 16 bytes total at 3 file offsets |

Why we have to patch loadmod even though we're not really touching its functionality:
`loadmod`'s `VerifyCodeIntegrityMd5` hashes **`/sbin/loadoa`** as part of its 446-file
integrity check (see [loadmod_md5_check_files.md](../modules/loadmod.ko_md5_check_files.md) for
the full list). Patched `loadoa` ⇒ MD5 mismatch ⇒ stock loadmod fails with status 1 ⇒
boot dies.

The three patches:

| # | File offset | Bytes (orig → new) | Purpose |
|---|---|---|---|
| 1 | `0x572d` (8B) | `85 c0 0f 85 a3 00 00 00` → `90×8` | NOP `TEST EAX,EAX; JNE` after `VerifyCodeIntegrityMd5`. Makes the outer MD5 gate inert (init no longer returns error code 1 on file mismatch). |
| 2 | `0x57b1` (2B) | `75 47` → `90 90` | NOP `JNE` after `RetrieveSecurityICKey`. Makes the dongle-check gate inert. |
| 3 | `0x3fb0` (6B) | `0f 85 e7 fe ff ff` → `e9 1e 01 00 00 90` | Replace first JNE *inside* `RetrieveSecurityICKey` with `JMP +0x11e` past all 16 byte-comparisons. Lands at the start of the success path (`GetRandomBytesWrapper(pbVar9, 8)` call); the rest of the function runs naturally and reaches `BuildCdromCommandStruct()`. |

**Patch #3 is the non-obvious one and the bug we hit first.** The same 16-byte hash check
that `VerifyCodeIntegrityMd5` does is repeated inside `RetrieveSecurityICKey`, right before
`BuildCdromCommandStruct()` — the function that populates the cryptoloop key globals. If
we don't bypass it too, `BuildCdromCommandStruct` is never called, status is still 0 (we
NOPed the gate at #2), and then `mount` of `/korg/Mod` fails with `VFS: Can't find an
ext2 filesystem on dev loopN`. See [loadmod_inner_md5_check.md](../modules/loadmod.ko_inner_md5_check.md).

---

## The `USBMidiAccessory.ko` gotcha

The stock Kronos has **two different** `USBMidiAccessory.ko` files:

| Path | MD5 | Size |
|---|---|---|
| `/sbin/USBMidiAccessory.ko` | `fae9ff96711b86791a83272e5affb334` | 102 741 B |
| `/korg/Mod/USBMidiAccessory.ko` | `e6b16f79b4216d4f7e734fd1d8bacdfd` | 102 931 B |

These are **different binaries**. `loadmod`'s MD5 check hashes the V1 (`/sbin/` variant).
If anyone overwrites `/sbin/USBMidiAccessory.ko` with the `/korg/Mod/` variant (easy
to do if you're naively copying files), stock loadmod will fail with status 1 even with
no other changes.

The patcher script refuses to proceed if it detects this state and tells you to restore
the V1 stock first.

---

## Operational warnings

### The OmapNKS4 panel chip wedge

Soft reboots (`reboot`, `reboot -f`) **do not** reliably reset the front-panel NKS4
controller (USB `0944:1005`). Once it wedges, its USB enumeration succeeds but the
proprietary comm-check times out, so `OmapNKS4Module.ko`'s `OmapNKS4Init` returns
failure and `loadoa` bails. Only a **full power-off of ~60 s with the unit unplugged**
clears it. USB-level resets via sysfs or `usb_reset_device(udev)` from a module patch do
not help — it's a deep firmware-state issue. See [kronos_chip_wedge.md](../modules/OmapNKS4Module.ko_chip_wedge.md).

**Implication:** never test a new boot-path patch with `reboot -f` if you can avoid it.
Either apply via hot-load (with the caveat below) or do a clean power cycle.

### The `/proc/.shm` hot-swap bug

Stock `OA.ko`'s `cleanup_module` calls `remove_proc_entry(".shm", NULL)` correctly, but
because `Eva` (and anything else that opened the entry) holds open fds on it, the proc
entry's refcount stays > 0 — the kernel keeps the `proc_dir_entry` around. When the
module text is freed by `rmmod`, those fds now point to freed memory. The next process
exit that closes its fds (often just an SSH `sh` exiting) triggers
`do_exit → filp_close → fops->release` on freed memory ⇒ **kernel oops** with `Fixing
recursive fault but reboot is needed!`. See [oa_ko_hot_swap_bug.md](../modules/OA.ko_hot_swap_bug.md).

**Implication:** do not `rmmod OA` while Eva is alive (or while *anything* has
`/proc/.shm` open). For permanent deployment, install the patched binaries and reboot —
don't try to hot-swap a running `OA`.

---

## The deployment script

[`patched/kronos_patcher.sh`](../../patcher/kronos_patcher.sh) is a self-contained
busybox-compatible shell script. It:

1. Defaults to `apply` mode (alt: `--verify`, `--revert`)
2. Checks stock binary MD5s — refuses to proceed on mismatch
3. Mounts `/korg/Mod` cryptoloop if not already mounted
4. Backs up originals to `/korg/rw/kronos_patcher_backup/`
5. Copies `OA.ko` and `KorgUsbAudioDriver.ko` from `/korg/Mod/` to `/sbin/`
6. Applies 3 byte-patches to `/sbin/loadmod.ko`
7. Applies 7 byte-patches to `/sbin/loadoa`
8. Applies 56 byte-patches to `/sbin/OA.ko`
9. Re-verifies all patched MD5s
10. Rolls back from backups if any step fails

On success, instructs the user to power-cycle (not soft-reboot, per the wedge issue).

### Constraints the script respects

| Tool | Status on Kronos | Used? |
|---|---|---|
| `bash`, `sh` | ✓ | yes (shebang `#!/bin/sh`) |
| `dd`, `cp`, `mv`, `rm`, `mkdir`, `mount`, `umount`, `sync` | ✓ | yes |
| `md5sum`, `cut`, `cat`, `grep`, `printf` (builtin) | ✓ | yes |
| `awk`, `sed`, `tar`, `gzip` | ✓ | not needed |
| `od`, `xxd`, `hexdump`, `wc`, `head`, `tail`, `cmp` | ✗ — **not available** | avoided |
| `python`, `perl` | ✗ | avoided |

Byte verification at a file offset uses `md5sum` of a `dd`-extracted range, compared
against `md5sum` of the expected bytes hex-decoded via `printf '%b'` — no reliance on
`od` or friends.

### Usage

```sh
# Inspect current state without changing anything
sh /tmp/kronos_patcher.sh --verify

# Apply (default mode)
sh /tmp/kronos_patcher.sh

# Roll back to stock (uses /korg/rw/kronos_patcher_backup)
sh /tmp/kronos_patcher.sh --revert
```

Re-running `apply` on an already-patched system detects it and exits 0.

---

## What didn't work, for future searches

These approaches were tried and failed during the development of this recipe. Documented
so we don't try them again:

1. **Patched OA only, stock everything else** — works in principle (`/sbin/OA.ko` is not
   in the MD5 list), but stock loadoa still loads `/korg/Mod/OA.ko`, so the patched OA
   sits unused. You need patched `loadoa` too.
2. **2-bypass loadmod (MD5 + dongle JNE only)** — Sets status=0 but cryptoloop mount
   still fails because the inner MD5 check in `RetrieveSecurityICKey` blocks
   `BuildCdromCommandStruct`. Resulting `VFS: Can't find an ext2 filesystem on dev loopN`
   in dmesg led us down a long blind alley about cryptoloop key derivation before we
   realised the second check existed.
3. **Hot-swap stock→patched OA via `rmmod OA && insmod /sbin/OA.ko`** — kernel oops on
   next file-fd close because `/proc/.shm` has stale fops (see hot-swap bug above).
4. **Modifying `OmapNKS4Module.ko` cleanup to call `usb_reset_device`** — patch works but
   triggers re-enumeration with `sDeviceInstance` still set, producing a
   `"DANGER! 2nd OmapNKS4 detected"` log entry. The USB-level reset also does *not* clear
   the wedge — that's a firmware-state issue. Patch removed.
5. **Patching `OmapNKS4Module.ko` with `usb_reset_device` import via ELF surgery** —
   complete; works without kernel oops; doesn't solve the actual wedge problem. Source
   in [`patch_omapnks4_cleanup.py`](../../tools/patch_omapnks4_cleanup.py) for the next time
   someone wants to add new imports to a Kronos kernel module.
