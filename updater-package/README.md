# Kronosology USB-stick updater

A drop-in Korg-format update package that applies the Kronosology patches via the
Kronos's built-in OS-update mechanism. **Works on a stock, non-rooted Kronos** — no
SSH, no rooting tarball, no command line required by the end user.

## The Easy Way

1. Insert a USB stick into your Kronos, formmat it
2. Insert the stick into your PC and copy the files in [output](output/kronosology-installer/) into the root folder of the USB stick.
3. Eject the stick from your PC (so that all files are flushed)
4. Insert the stick into your Kronos
5. Run an Update (`Global->[Page Menu]->Update System Software)
6. Once completed, power cycle your Kronos


## The Other Way

[`build_updater.sh`](build_updater.sh) generates a folder called
`output/kronosology-installer/` containing:

```
install.info          ← signed package metadata (UpdateOS reads this first)
kronosology.tar.gz    ← minimal payload (drops a marker file in /tmp/)
pretar.sh             ← the actual patcher (it's kronos_patcher.sh + a banner)
README.txt            ← these instructions, for the inquisitive
```

The `pretar.sh` is the same byte-level patcher that lives in
[`../patcher/kronos_patcher.sh`](../patcher/kronos_patcher.sh) — same MD5 checks, same
rollback safety, same backup directory. UpdateOS runs it as root before extracting the
(deliberately trivial) tarball.

`install.info` is signed with the `UPDATER_KEY` that `UpdateOS` verifies — same key
the `kronos_rooting` tarball uses, and the same key Korg's own factory updates use.

## How to build the package

From this directory:

```sh
sh build_updater.sh
```

That produces `output/kronosology-installer/`. Copy its contents to a
**FAT-formatted USB stick** (or ext2 — UpdateOS reads both):

```sh
cp -r output/kronosology-installer/* /media/your-usb-stick/
sync
```

## How an end-user installs it on their Kronos

1. **Back up your Kronos data** (programs, combis, set lists) via the front-panel
   `MEDIA → Save All` menu. This patcher won't touch user data, but a good backup
   before any system-level change is sensible.
2. Power on the Kronos normally and wait for it to fully boot.
3. Insert the USB stick prepared above.
4. From the front panel: **GLOBAL → SystemUpdate** (or equivalent — the exact menu
   wording varies by OS version).
5. The Kronos will display "Installing OS update — please wait" and the update flow
   starts.
6. **What's happening under the hood:**
   - UpdateOS reads `install.info` from the USB
   - Verifies the SHA-1 signature over `pretar.sh + UPDATER_KEY`
   - Runs `pretar.sh` as root: it mounts the cryptoloop, verifies stock MD5s, backs up
     originals to `/korg/rw/kronos_patcher_backup/`, patches three binaries (~6 seconds)
   - Extracts `kronosology.tar.gz` (a no-op for the user — just a marker file)
   - Displays success
7. When the Kronos says the update completed: **POWER-CYCLE THE INSTRUMENT** (turn the
   power switch off, wait ~60 seconds, turn it back on). **Do not "soft reboot"** via
   the front panel — that can wedge the front-panel chip and require a longer
   power-cycle to recover. See [the chip wedge note](../docs/modules/OmapNKS4Module.ko_chip_wedge.md).

After power-cycle, the Kronos boots normally with the patched OA active; any EX
expansion installs without per-device authorization strings.

## What's installed where

After successful install, the following live on the Kronos's internal disk:

| Path | Stock MD5 | After install |
|---|---|---|
| `/sbin/loadmod.ko` | `d1697c9b…` (52 384 B) | `28d1cec1…` (52 384 B, 3 bypass patches) |
| `/sbin/loadoa` | `8a3d61f3…` (20 789 B) | `d17c2603…` (20 789 B, 7 path-redirect patches) |
| `/sbin/OA.ko` | (not present) | `163550b6…` (14 285 504 B, copied from /korg/Mod/ + 56 EX-bypass patches) |
| `/sbin/KorgUsbAudioDriver.ko` | (not present) | `29fbd20c…` (32 184 B, stock — copied from /korg/Mod/) |
| `/sbin/USBMidiAccessory.ko` | `fae9ff96…` (102 741 B) | unchanged |
| `/korg/rw/kronos_patcher_backup/loadmod.ko` | — | backup of stock loadmod.ko |
| `/korg/rw/kronos_patcher_backup/loadoa` | — | backup of stock loadoa |
| `/korg/rw/kronos_patcher_backup/USBMidiAccessory.ko` | — | backup of stock USBMidi |
| `/tmp/kronosology_installed` | — | marker file with version + install time |

The marker in `/tmp/` is gone after the next reboot (it's just so UpdateOS has a
non-trivial extract to do — anything in `/tmp/` is volatile by design). The
backups under `/korg/rw/` are persistent.

## How to uninstall

There is no front-panel uninstall path (UpdateOS doesn't speak that). You have two
options:

**Option A — reinstall the official Korg OS update.** The factory installer ships
its own copies of `loadmod.ko`, `loadoa`, and `USBMidiAccessory.ko`, which will
overwrite ours. Get the official update from [Korg's website](https://www.korg.com/support/download/product/0/152/).
Note that the factory installer will NOT remove `/sbin/OA.ko` or `/sbin/KorgUsbAudioDriver.ko`
(those are files Korg doesn't ship at `/sbin/`), but their presence is harmless once
the stock `loadoa` is back — it'll load OA from `/korg/Mod/` as before.

**Option B — use SSH** (requires the same root access the rooting tarball provides):

```sh
ssh root@<kronos-ip>
sh /korg/rw/kronos_patcher_backup/../../kronos_patcher.sh --revert
```

(or wherever you placed the patcher script — see [../patcher/README.md](../patcher/README.md))

## Failure modes

| Symptom | Cause | Fix |
|---|---|---|
| "Bad signature" or "install.info invalid" | USB stick built incorrectly, or `install.info` modified after signing | Re-run `build_updater.sh`, re-copy to stick |
| Patcher exits with FATAL | Stock binary MD5 doesn't match what the patcher expects (different OS version, prior third-party patches) | Manual investigation; the patcher's verification design is intentional |
| Kronos shows "System cannot start" after install + power-cycle | Front-panel chip wedge from a soft reboot, OR genuine patch failure | Power-cycle again for ≥ 60 s with unit unplugged; if persists, get SSH access and run `--revert` |
| Update completes, but EX install still asks for auth | Stock OA loaded instead of patched (loadoa didn't get patched correctly) | SSH in, run `sh kronos_patcher.sh --verify` to diagnose |

## Safety notes

- The patcher is **idempotent** — re-running on an already-patched system is a no-op
- The patcher **verifies stock MD5s before making any change** — if your Kronos isn't
  a v3.2.1 stock install, the patcher refuses to run and leaves everything as-is
- The patcher **backs up originals** before patching, so SSH-based rollback is one
  command (`sh kronos_patcher.sh --revert`)
- The official Korg OS reinstall will undo this cleanly

## What this is NOT

- This is not a Korg-endorsed update. The signature is valid only because we
  studied the same key the rooting tarball already uses; it does not
  imply any cooperation with Korg.
- This does not redistribute any Korg binary. It ships only the byte-patcher; the
  binaries themselves come from the user's own Kronos at install time.
- This will not survive a Korg-issued OS update — when Korg ships a new v3.2.2 (or
  whatever), the factory installer will overwrite our patches. You'll need an
  updated build of this package for the new MD5s. Adapting it is just a matter of
  updating the MD5 constants and patch tables in `kronos_patcher.sh`.

## See also

- [`../patcher/README.md`](../patcher/README.md) — the patcher itself, for SSH users
- [`../docs/workflow/deploying_patches.md`](../docs/workflow/deploying_patches.md) —
  why each patch is necessary, with the full design rationale
- [`../update-builder/README.md`](../update-builder/README.md) — the UpdateOS package
  format and signature algorithm
