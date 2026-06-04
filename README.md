# Kronosology

> A knowledge base, patching tool and update-builder for the
> [Korg Kronos](https://www.korg.com/products/synthesizers/kronos2/) workstation.

The Korg Kronos is a remarkable instrument — a Linux box, a digital signal processor,
and a music workstation rolled into one — but its inner workings are essentially
undocumented outside Korg's engineering team. **Kronosology** is what came out of
several intensive weeks of studying: a corpus of analysis notes, a working
tool to install patched binaries on a stock unit, and a builder for custom Korg-format
OS update packages.

Most of the learnings are recorded in [`docs/`](docs/) (forewarning: it's a pretty dry read!).


The two executable parts are [`patcher/`](patcher/README.md) and [`update-builder/`](update-builder/README.md).

---

## What this repo will do for you

| You want to... | Go to |
|---|---|
| **Patch a stock Kronos via SSH** (you have root access — uprooting tarball etc.) | [patcher/](patcher/) |
| **Patch a stock Kronos via USB stick** (no root needed — uses Korg's OS-update flow) | [updater-package/](updater-package/) |
| **Build your own custom OS-update package** | [update-builder/](update-builder/) |
| **Decrypt the encrypted Mod / Eva / WaveMotion images offline** | [scripts/](scripts/) |
| **Diff two firmware versions across the encrypted volumes** | [scripts/](scripts/) — `diff_kronos_versions.sh` |
| **Install or Remove EXs banks via SSH** | [InstallEXs](docs/modules/InstallEXs.md) |
| Understand the Kronos software architecture end-to-end | [docs/system_overview.md](docs/system_overview.md) |
| Browse module-by-module reverse-engineering notes | [docs/modules/](docs/modules/) |
| Understand the boot integrity chain (loadoa → loadmod → cryptoloop → OA → Eva) | [docs/system_overview.md](docs/system_overview.md), [docs/modules/loadoa.md](docs/modules/loadoa.md), [docs/modules/loadmod.ko.md](docs/modules/loadmod.ko.md) |
| Understand the Atmel NV2AC security IC protocol (GPA stream cipher) | [docs/crypto/atmel_nv2ac.md](docs/crypto/atmel_nv2ac.md) |
| Understand the OS-update signature algorithm (SHA-1 + `UpdaterScriptsKey`) | [docs/crypto/update_signature.md](docs/crypto/update_signature.md) |
| Understand the on-disk format of programs / combis / drum kits / wave sequences / etc. | [docs/preload/](docs/preload/) |
| Understand the EX-bank authorization algorithm (Base32 + Blowfish-CFB + MD5 + chip secret) | [docs/crypto/auth_string_algorithm.md](docs/crypto/auth_string_algorithm.md) |
| Set up Ghidra to follow along with the analysis | [docs/workflow/ghidra_setup.md](docs/workflow/ghidra_setup.md) |
| Export a patched .ko from a Ghidra session (reloc-aware diff) | [docs/workflow/export_patched_ko.md](docs/workflow/export_patched_ko.md) |
| Add a new external-symbol import to a Linux 2.6 .ko by ELF surgery | [tools/README.md](tools/README.md) |

The full doc index, including the per-binary deep-dives and the workflow notes, is in
[`docs/README.md`](docs/README.md).

---

## Repository layout

```
kronosology/
├── README.md                       this file
├── docs/                           the reverse-engineering corpus
│   ├── README.md                   doc index
│   ├── system_overview.md          how the whole thing hangs together
│   ├── modules/                    per-binary deep-dives (OA.ko, loadmod, loadoa, etc.)
│   ├── crypto/                     auth-string algorithm, NV2AC protocol, update signature
│   ├── interfaces/                 /proc/.oacmd, file formats
│   ├── preload/                    on-disk preset memory formats
│   └── workflow/                   Ghidra setup, patch deployment, analysis methodology
├── patcher/                        live-Kronos patcher (for rooted Kronos via SSH)
│   ├── README.md                   how to use it
│   └── kronos_patcher.sh           the script (busybox-compatible)
├── updater-package/                USB-stick installer for NON-rooted Kronos
│   ├── README.md                   how to build + use
│   ├── build_updater.sh            produces output/kronosology-installer/
│   └── output/kronosology-installer/  (built by the build script)
├── update-builder/                 OS-update package builder (low-level)
│   ├── README.md                   internals + usage
│   └── update_builder.py
├── tools/                          reusable techniques
│   ├── README.md
│   └── patch_omapnks4_cleanup.py   demo of adding new symbol imports to a .ko
└── scripts/                        offline crypto tools for the encrypted volumes
    ├── README.md                   complete usage + how the encryption works
    ├── decrypt_kronos_img.py       AES-256-CBC + plain IV decryptor (Mod/Eva/WaveMotion), pure Python
    ├── mount_kronos_img.sh         cryptsetup mount helper (alternative to Python path)
    ├── diff_kronos_versions.sh     end-to-end version diff across encrypted volumes
    └── getloopkey.s                i386 asm — on-device LOOP_GET_STATUS64 probe (key recovery)
```

---

## Patching a stock Kronos

Two install paths — pick whichever matches your access level:

### Path A — USB-stick installer (no rooting required)

If you have a stock, non-rooted Kronos, the easiest path is the USB-stick installer
under [`updater-package/`](updater-package/). It plugs into Korg's own OS-update
flow: build the package once, copy the contents to a FAT-formatted USB stick, and
trigger an OS update from the Kronos front panel. The Kronos itself runs our
patcher as the update's PRETAR script. No SSH, no command line, no rooting tarball
needed by the end user.

```sh
cd updater-package && sh build_updater.sh
cp -r output/kronosology-installer/* /media/your-usb-stick/
# then plug into Kronos and use the front-panel "OS update" menu
```

Full instructions: [`updater-package/README.md`](updater-package/README.md).

### Path B — direct SSH patcher (for already-rooted Kronos)

If you already have root SSH access (e.g. via [uprooting/kronos_rooting](https://github.com/uprooting/kronos_rooting)),
the patcher in [`patcher/`](patcher/) runs directly:

```sh
scp patcher/kronos_patcher.sh root@<kronos-ip>:/tmp/
ssh root@<kronos-ip>
sh /tmp/kronos_patcher.sh --verify   # diagnostic; no changes
sh /tmp/kronos_patcher.sh            # apply; backs up originals first
```

Both paths use the **same script** internally (`kronos_patcher.sh`) with the same
verification, the same backups, and the same MD5 checks — they differ only in how
the script is delivered to the Kronos.

### What the patcher actually does

It copies `OA.ko` out of the cryptoloop-encrypted `/korg/Mod` image, patches it in
56 places to bypass EX-bank authorization, patches `loadoa` so that on next boot it
loads that file instead of the cryptoloop one, and patches `loadmod.ko` so that the
integrity-check chain still believes the system is unmodified. After applying,
**always power-cycle** (not soft-reboot — see below).

Full details under [`patcher/README.md`](patcher/README.md) and the design rationale
under [`docs/workflow/deploying_patches.md`](docs/workflow/deploying_patches.md).

### **Operational warning: soft reboots can wedge the panel controller**

The front-panel chip (USB `0944:1005`) can get into a firmware-state wedge that survives
soft reboots and software USB resets. Only a **physical power-off of ~60 seconds**
reliably clears it. After applying the patcher, *always* power-cycle — don't `reboot`.
See [docs/modules/OmapNKS4Module.ko_chip_wedge.md](docs/modules/OmapNKS4Module.ko_chip_wedge.md).

---

## Decrypting and diffing the firmware images

The three loop-back images on the Kronos (`Mod.img`, `Eva.img`,
`WaveMotion.img`) are AES-256-CBC encrypted by the kernel's cryptoloop
driver. The keys are derived at boot from `/.pairFact3` via the stgNV2AC
security chip, but the *final* AES keys turn out to be universal across
every Kronos unit and every firmware version we've checked
(2014 / 3.2.1 / 3.2.2 update packages + live-device dumps). The tools in
[`scripts/`](scripts/) ship those keys inline, so you can decrypt and
inspect the encrypted volumes offline with no hardware involvement:

```sh
# Decrypt one image to a plaintext ext2 (pure Python, no root):
python3 scripts/decrypt_kronos_img.py KRONOS_Update_3_2_2/mnt/korg/ro/Mod.img  Mod_plain.img

# Browse without mounting (no root):
debugfs -R 'ls -l /'              Mod_plain.img
debugfs -R 'dump /OA.ko OA.ko'    Mod_plain.img

# Full version diff — decrypts both, extracts everything, reports what changed:
sh scripts/diff_kronos_versions.sh  KRONOS_Update_3_2_1  KRONOS_Update_3_2_2/mnt
```

The diff script answers questions like "what actually changed between
3.2.1 and 3.2.2?" in one shell command. (Answer for that specific pair:
`OA.ko` had a real ~16-byte `.text` change; `loadmod.ko` had 974 bytes
in one localized region; everything else was either byte-identical or
just a rebuild-date stamp.)

If a future Kronos firmware rotates the cryptoloop keys, the same folder
contains `getloopkey.s` — an i386-assembly on-device probe that
recovers the new keys via the `LOOP_GET_STATUS64` ioctl. Full method
notes, build instructions, and the per-tool usage are in
[`scripts/README.md`](scripts/README.md).

---

## Building a custom OS update package

`update-builder/update_builder.py` produces installable `.tar.gz` update packages in
the format the stock Kronos `UpdateOS` binary accepts. This includes the correct
`install.info` file with a valid SHA-1 + `UpdaterScriptsKey` signature.

Use cases:

- Ship a patched system image (e.g. one containing our pre-patched `OA.ko` and the
  modified `loadoa`/`loadmod`) as a friendly USB-stick install
- Apply system-wide configuration tweaks during a maintenance update
- Distribute additional executables or libraries to multiple Kronoses with one tool

Usage:

```sh
python3 update-builder/update_builder.py --root /path/to/new/files --output kronos_update.tar.gz
```

Full internals, signature algorithm, and command reference in
[`update-builder/README.md`](update-builder/README.md).

---

## Status & scope

| Layer | Status |
|---|---|
| `OA.ko` (synthesis engine) — top-level RE | Comprehensive |
| `loadmod.ko` (boot integrity) — analysis | Comprehensive |
| `loadmod.ko` — both MD5 checks bypassed | ✅ working |
| `loadoa` — patches deployed | ✅ working |
| `OA.ko` — EX-bank auth bypass | ✅ working (56-byte-run patch) |
| `OmapNKS4Module.ko` — chip protocol RE | Partial (enough to understand the dongle handshake) |
| `OmapNKS4Module.ko` — hot-fix for chip wedge | Investigated; not feasible (firmware-level, not software) |
| `Eva` — top-level RE | Light (we mostly need to know what it talks to) |
| `UpdateOS` — update package format | Comprehensive |
| `InstallEXs` — EX install flow | Comprehensive |
| `update_builder.py` — produces signed packages | ✅ working |
| Cryptoloop key derivation chain (pairFact + dongle) | Documented, intercepted at hook level |
| Cryptoloop keys (Mod / Eva / WaveMotion) — extracted, validated, embedded in tools | ✅ all 3 keys, universal across units/versions |
| Offline decrypt + mount + version-diff toolchain (`scripts/`) | ✅ working — no Kronos, no `cryptsetup`, no `sudo` for the Python path |
| Per-binary patches, tested live on hardware | ✅ verified end-to-end via patcher script |

---

## How this came to exist

Long-running analysis sessions using [Ghidra](https://ghidra-sre.org/) plus the
[GhidraMCP](https://github.com/LaurieWired/GhidraMCP) bridge, with much trial,
much breaking-the-Kronos-and-fixing-it-via-SSH, and a memorable detour where I
attempted to clean up a USB chip wedge by adding new symbol imports to a kernel
module via ELF surgery — which worked perfectly but didn't actually solve the
problem because the wedge is at the chip's firmware level, not in the host driver.

The `tools/patch_omapnks4_cleanup.py` script preserves that surgical technique
because it generalises well to any time you want to call a kernel function the
stock `.ko` doesn't already import.

The bumps along the way produced concrete operational lessons that are documented
alongside the binary they belong to — see for example
[docs/modules/loadmod.ko_inner_md5_check.md](docs/modules/loadmod.ko_inner_md5_check.md),
[docs/modules/OA.ko_hot_swap_bug.md](docs/modules/OA.ko_hot_swap_bug.md), and
[docs/modules/OmapNKS4Module.ko_chip_wedge.md](docs/modules/OmapNKS4Module.ko_chip_wedge.md).

---

## Caveats & legal

- This is reverse-engineering work for **interoperability and personal-use research**
  purposes. Korg owns the Kronos firmware; nothing in this repo redistributes any
  Korg binary. The patcher works against the user's own already-licensed install.
- The OS-update mechanism uses cryptographic signatures that this repo includes the
  necessary key for. **Do not use this to distribute pirated content.** The point is
  to let owners customise their own instruments.
- Tested against Kronos OS v3.2.1. Other versions will likely need adjusted file
  offsets and MD5s; the patcher refuses to run on unrecognised MD5s, which is the
  intended behaviour — adapt the patcher's MD5 constants and offset tables, don't
  bypass the safety check.
- **There is no warranty.** A Kronos in the wrong state can be bricked enough to
  require a CD-ROM reinstall. The patcher's rollback-on-failure logic is designed
  to minimise this risk but cannot eliminate it.

---

## Contributing

This is research notes more than a "project". If you've used this work, found a
bug, extended it to a different Kronos OS version, or want to discuss any aspect
of the studying, please open an issue or a PR.

If you're starting your own Kronos exploration, [`docs/workflow/`](docs/workflow/)
has the methodology notes.

## License

The studied analysis (documentation, scripts, RE notes) in this repository
is released into the public domain — use, modify, distribute as you wish.

The patched Korg binaries themselves are not distributed here; the patcher produces
them from the user's own stock files at apply time.
