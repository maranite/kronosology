# offline-patcher — Full Offline Firmware Patcher for Korg Kronos

Patches a Korg Kronos entirely on the host machine — no live Kronos access
required during the build step.  Produces a signed Korg UpdateOS USB package
containing the patched binaries pre-embedded in the tar payload.

What it patches:
- **OA.ko** — removes the EX-bank authorization requirement (11 patch sites)
- **loadmod.ko** — bypasses the MD5 integrity chain so patched files boot cleanly
- **loadoa** — redirects binary paths from `/korg/Mod/` to `/sbin/` so the
  patched copies are loaded instead of the originals from the encrypted image

Stock-system compatible: no prior rooting or SSH access required.

---

## Prerequisites

```bash
pip install cryptography
```

A Korg Kronos OS update folder (the directory that contains `mnt/`, which
contains `korg/ro/Mod.img`, `korg/ro/loadoa`, `korg/ro/loadmod.ko`).
Download from Korg's site and unzip; the folder is typically named
`KRONOS_Update_3_2_2/` or similar.

---

## Usage

```bash
# Build the patched USB package
python3 patch_firmware_offline.py /path/to/KRONOS_Update_3_2_2/mnt

# Check patch-site compatibility only (no output written)
python3 patch_firmware_offline.py /path/to/KRONOS_Update_3_2_2/mnt --verify

# Custom output directory
python3 patch_firmware_offline.py /path/to/KRONOS_Update_3_2_2/mnt \
    --output /tmp/my-kronos-package
```

Output directory (default: `offline-patcher/output/kronosology-offline-patched/`):

```
output/kronosology-offline-patched/
├── install.info           ← signed package metadata
├── kronosology.tar.gz     ← payload: patched sbin/OA.ko, sbin/loadmod.ko, sbin/loadoa,
│                             sbin/KorgUsbAudioDriver.ko (stock copy at new path)
├── pretar.sh              ← pre-flight: verify stock MD5s, back up originals
└── posttar.sh             ← post-install: set permissions, log results
```

---

## Deploying to the Kronos

Copy the output directory contents to a FAT-formatted USB stick and trigger
**Global → OS Update** from the Kronos front panel:

```bash
cp -r output/kronosology-offline-patched/* /media/your-usb-stick/
sync
# Insert USB into Kronos, then: Menu → Global → OS Update
```

After the update completes, **power-cycle** the Kronos (full power-off ≥ 60 s,
then on).  Do **not** use the front-panel soft-reboot — it can wedge the
OmapNKS4 panel chip permanently until a long power cycle.
See [`../docs/modules/OmapNKS4Module.ko_chip_wedge.md`](../docs/modules/OmapNKS4Module.ko_chip_wedge.md).

---

## How it works

### Decryption

The Kronos stores its main binaries in an AES-256-CBC cryptoloop image
(`Mod.img`).  The key is universal across all Kronos units and all firmware
versions we've checked (2014 / 3.2.1 / 3.2.2).  The script decrypts it in
pure Python using only the `cryptography` package.

### Patching

**OA.ko** patches are applied as 11 section-relative offsets within the `.text`
section — the same offsets work for any firmware version because the ELF is
an ET_REL (relocatable) object with a stable `.text` base.  The patches:
- Skip the magic-value degradation loop (prevents silent auth bypass removal)
- Force `IsUsingAnyUnauthorizedMultisamples()` to always return 0

**loadmod.ko** patches are applied via `.symtab` symbol lookup — the code finds
`init_module` and the obfuscated `bbbbbbbba12` symbol and patches at fixed
offsets from each.  This bypasses both MD5 check stages so the integrity chain
passes even with modified files.

**loadoa** patches are two string replacements: `/korg/Mod/OA.ko` → `/sbin/OA.ko`
and `/korg/Mod/KorgUsbAudioDriver.ko` → `/sbin/KorgUsbAudioDriver.ko`.  This
redirects the loader to the patched copies extracted to `/sbin/` by the UpdateOS
tar payload.

### Signing

The UpdateOS package signature is `SHA1(pretar_content + posttar_content +
UpdaterScriptsKey)`.  The key was extracted from the `UpdateOS` binary data
section.  Full algorithm: [`../docs/crypto/update_signature.md`](../docs/crypto/update_signature.md).

---

## Compatibility

Tested against Kronos OS 3.2.1 and 3.2.2.  The `--verify` flag checks all
patch sites against known stock MD5s and exits non-zero if anything is
unexpected — run it against a new firmware version before committing to a build.

Known stock MD5s are embedded in the script for both 3.2.1 and 3.2.2.  Adding
a new version requires extracting the stock binaries, computing their MD5s, and
adding an entry to `KNOWN_STOCK_MD5` in `patch_firmware_offline.py`.

---

## Further reading

- [`../docs/crypto/auth_string_algorithm.md`](../docs/crypto/auth_string_algorithm.md) — EX auth algorithm
- [`../docs/modules/loadmod.ko_inner_md5_check.md`](../docs/modules/loadmod.ko_inner_md5_check.md) — loadmod dual MD5 check
- [`../docs/workflow/deploying_patches.md`](../docs/workflow/deploying_patches.md) — deployment design rationale
- [`../docs/system_overview.md`](../docs/system_overview.md) — full boot chain
