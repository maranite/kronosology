# `kronos_patcher.sh` — apply Kronosology patches to a live Kronos

A self-contained shell script that converts a stock Korg Kronos v3.2.1 into
one running our patched `OA.ko` (EX-bank authorization bypass) — and reverses the
process cleanly on demand.

The script is **idempotent**, **rollback-safe**, **busybox-compatible**, and **does
all its verification before touching anything**. If anything looks off, it refuses
to run.

For clarity, you can run the Korg factory update at any time to undo the changes this script makes.


## What it does

1. Verifies the MD5 of every stock binary it's about to touch. If it's not from v3.2.1, it will stop.
2. Mounts the `/korg/Mod` cryptoloop (needed to copy `OA.ko` out of it).
3. Backs up `/sbin/loadmod.ko`, `/sbin/loadoa`, `/sbin/USBMidiAccessory.ko` to
   `/korg/rw/kronos_patcher_backup/`.
4. Copies `OA.ko` and `KorgUsbAudioDriver.ko` from `/korg/Mod/` to `/sbin/`.
5. Applies 3 byte-patches to `/sbin/loadmod.ko` (bypassing the 3 MD5/dongle gates).
6. Applies 7 byte-patches to `/sbin/loadoa` (to load `OA` and `KorgUsbAudo` from `/sbin/` instead of the cyptoloop images mounted at `/korg/Mod/`).
7. Applies 56 byte-patches to `/sbin/OA.ko` (to bypass authenticity and authorization checks).
8. Verifies every resulting file's MD5 matches the expected patched value.
9. On any error, rolls back to the originals via an `EXIT` trap.


## Prerequisites

You need:
- A stock Kronos v3.2.1 (the script refuses to run on unrecognised binaries).
- About 30 MB of free space on `/korg/rw` for the backups.
- Root SSH access (see [kronos_rooting](https://github.com/uprooting/kronos_rooting) ).


## Quick start

Copy the script to the Kronos and run it as root:

```sh
# from your workstation
scp kronos_patcher.sh root@<kronos-ip>:/tmp/

# on the Kronos
ssh root@<kronos-ip>
sh /tmp/kronos_patcher.sh --verify   # diagnostic pass, no changes
sh /tmp/kronos_patcher.sh            # apply
```

After it reports success, **power-cycle the Kronos** (full power-off ≥ 60 s). Do not
`reboot` — a soft reboot can wedge the front-panel chip and leave the Kronos stuck on
the "System cannot start" reauth screen until another power-cycle (see
[../docs/modules/OmapNKS4Module.ko_chip_wedge.md](../docs/modules/OmapNKS4Module.ko_chip_wedge.md)).

## Usage

```
sh kronos_patcher.sh           # apply (default)
sh kronos_patcher.sh --verify  # show current state, no changes
sh kronos_patcher.sh --revert  # restore /korg/rw/kronos_patcher_backup/ → /sbin/
```

`--verify` always exits 0 even on a stock system — it's a diagnostic, not a precondition
check.

`--revert` requires `/korg/rw/kronos_patcher_backup/` to exist (created automatically
by a previous successful `apply`).

Re-running `apply` on an already-patched system is a no-op.

## Target version

Targets v3.2.1 binaries with these MD5s:

| File | Stock MD5 | Patched MD5 |
|---|---|---|
| `/sbin/loadmod.ko` | `d1697c9b1c478c0dcdfaef71516fe5f2` | `28d1cec16f1d893f1d78241b62a989d9` |
| `/sbin/loadoa` | `8a3d61f3332d7bcf694e8c05845b4754` | `d17c26036fa0f51f5f4c0ef2f6f424bf` |
| `/sbin/USBMidiAccessory.ko` (V1, *not* the /korg/Mod V2) | `fae9ff96711b86791a83272e5affb334` | unchanged |
| `/korg/Mod/OA.ko` (copied to `/sbin/OA.ko`) | `955636c2b11a70a1dbecefaaa7bd4f80` | `163550b60b7508b2c0ba1fd314b0b944` |
| `/korg/Mod/KorgUsbAudioDriver.ko` (copied to `/sbin/`) | `29fbd20cf729980e1cffd670391256b5` | unchanged |

If any MD5 doesn't match, the script aborts before making any changes. Adapting it to a
different OS version is a matter of updating the MD5 constants and the patch byte tables
at the top of the script.


## What if the patcher refuses to run?

The most common reason is that something has overwritten `/sbin/USBMidiAccessory.ko`
with the wrong variant. The Kronos ships **two** `USBMidiAccessory.ko` files of
different sizes — the one under `/sbin/` (V1, 102 741 B) is what `loadmod`'s MD5
integrity check expects. If you (or an earlier tool) copied the `/korg/Mod/` variant
(V2, 102 931 B) on top of it, `loadmod`'s check fails before our patches can help.
Restore the V1 stock variant from a backup, OS reinstall, or the
[uprooting tarball](https://github.com/uprooting/kronos_rooting).

If the chip wedge has set in, no software fix helps until you power-cycle for ≥ 60 s.

## Background

The patches are explained in detail under [../docs/workflow/deploying_patches.md](../docs/workflow/deploying_patches.md)
(why each one is needed and how it was discovered). The reverse-engineering writeups for
each binary touched live under [../docs/modules/](../docs/modules/).
