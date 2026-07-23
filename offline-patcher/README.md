# offline-patcher — Full Offline Firmware Patcher for Korg Kronos

Patches a Korg Kronos entirely on the host machine — no live Kronos access
required during the build step.  Produces a signed Korg UpdateOS USB package
containing the patched binaries pre-embedded in the tar payload.

The package is **self-contained and stock-system compatible**: no prior rooting,
SSH access, or manual module installation is required.

What it patches / installs:
- **OA.ko** — removes the EX-bank authorization requirement (11 patch sites)
- **loadmod.ko** — bypasses the MD5 integrity chain so patched files boot cleanly
- **loadoa** — redirects `/korg/Mod/OA.ko`, `/korg/Mod/KorgUsbAudioDriver.ko`, and
  `/korg/Eva/Eva` to `/sbin/` equivalents so patched copies are used at boot
- **`/sbin/Eva`** — shell wrapper: insmod's `oa_authgen.ko` at boot, then exec's the
  real Eva binary.  This is how `oa_authgen.ko` is loaded on every boot — no
  init script changes required
- **Eva.elf** — patched Eva binary: intercepts the front-panel Authorise button to
  auto-generate auth strings instead of showing the manual-entry dialog
  _(skipped if `Eva.img` is absent; the wrapper still loads `oa_authgen.ko`)_
- **oa_authgen.ko** — kernel module that exposes `/proc/.oaauth`; included in the
  package and deployed to `/sbin/`; loaded at every boot by the Eva wrapper
- **InstallEXs** — replaces `/sbin/InstallEXs` with a wrapper that auto-generates
  and saves the auth string whenever an EX is installed from the front panel

---

## Prerequisites

```bash
pip install cryptography
```

A Korg Kronos OS update folder (the directory that contains `mnt/`, which
contains `korg/ro/Mod.img`, `korg/ro/Eva.img`, `sbin/loadoa`, `sbin/loadmod.ko`).
Download from Korg's site and unzip; the folder is typically named
`KRONOS_Update_3_2_2/` or similar.

`Eva.img` is optional — if absent, the Eva patch is skipped and a warning is
printed.  All other patches still apply.

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
├── kronosology.tar.gz     ← payload:
│                               sbin/OA.ko                  (patched, from Mod.img)
│                               sbin/loadmod.ko             (patched)
│                               sbin/loadoa                 (patched — 3 path redirects)
│                               sbin/KorgUsbAudioDriver.ko  (stock, from Mod.img)
│                               sbin/Eva                    (wrapper script — loads oa_authgen.ko)
│                               sbin/Eva.elf                (patched binary; if Eva.img available)
│                               sbin/oa_authgen.ko          (kernel module; if pre-built)
│                               sbin/InstallEXs             (our wrapper; if built)
├── pretar.sh              ← backs up originals; renames InstallEXs → InstallEXs.real
├── posttar.sh             ← one-shot auth for existing EXs; writes version marker
└── README.txt
```

---

## Building InstallEXs

`patch_firmware_offline.py` will attempt `make` automatically if
`InstallEXs/InstallEXs` is absent.  To build it manually:

```bash
sudo apt install gcc-multilib    # needs 32-bit compiler support
cd offline-patcher/InstallEXs
make
# → InstallEXs  (i386 ELF, stripped, ~10 KB)
```

The `cryptography` Python package is the only other prerequisite:

```bash
pip install cryptography
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
versions we've checked (2014 / 3.2.1 / 3.2.2 / 3.2.3).  The script decrypts it
in pure Python using only the `cryptography` package.

### Patching

**OA.ko** patches are applied as 11 sites total:
- 9 are section-relative offsets (5 in the main `.text` degradation block, 4 in
  their own COMDAT `.text._ZN...` sections) — stable across firmware versions
  because the ELF is an ET_REL (relocatable) object with a fixed section base.
- 2 more (the `CSTGTG92OscBase`/`CPianoOsc` `IsUsingAnyUnauthorizedMultisamples`
  specializations) live directly in the shared main `.text` section rather than
  their own COMDAT section, so a raw section-relative offset for these two
  **does drift** whenever unrelated code earlier in `.text` changes size —
  confirmed happening going 3.2.2 → 3.2.3 (same 19 bytes, same symbols, just
  +0x90 further into `.text`). These two are resolved by `.symtab` symbol name
  instead (`OA_SYMBOL_PATCHES`), the same mechanism `loadmod.ko` already uses,
  so they track the symbol wherever the recompile puts it.

Together the patches:
- Skip the magic-value degradation loop (prevents silent auth bypass removal)
- Force `IsUsingAnyUnauthorizedMultisamples()` to always return 0

**loadmod.ko** patches are applied via `.symtab` symbol lookup — the code finds
`init_module` and the obfuscated `bbbbbbbba12` symbol and patches at fixed
offsets from each.  This bypasses both MD5 check stages so the integrity chain
passes even with modified files.

**loadoa** patches are three null-padded string replacements (same byte length,
always safe):
- `/korg/Mod/OA.ko` → `/sbin/OA.ko` — loads patched OA from `/sbin/`
- `/korg/Mod/KorgUsbAudioDriver.ko` → `/sbin/KorgUsbAudioDriver.ko`
- `/korg/Eva/Eva` → `/sbin/Eva` — executes `/sbin/Eva` (our wrapper) at startup

**`/sbin/Eva` wrapper** is a small shell script deployed by the tar:
```sh
#!/bin/sh
insmod /sbin/oa_authgen.ko 2>/dev/null || true
exec /sbin/Eva.elf "$@"      # or /korg/Eva/Eva if Eva.img was absent
```
loadoa exec's `/sbin/Eva` as its final startup step, after `OmapNKS4Module.ko`
is already loaded — which is exactly when `oa_authgen.ko` (which depends on
`OmapNKS4Module.ko` exports) can safely be insmod'd.  No init script changes
are required.

**Eva.elf** is patched with a 168-byte code cave written into a 206-byte region
in `.rodata` that's all-zero on 3.2.1/3.2.2 (see [Compatibility](#compatibility)
for what changed on 3.2.3).  When the front-panel Authorise button is pressed,
instead of opening the `CAuthKeyboard` dialog for manual code entry, the cave:
1. Calls `GetProductOptionFileName` to get the option file name (e.g. `S023`)
2. Opens `/proc/.oaauth` (exposed by `oa_authgen.ko`)
3. Writes `GEN:S023` to generate the device-specific auth string
4. Reads back the 24-character result
5. Calls `SendCommandAuthorizeOption` to submit it to OA.ko

If `Eva.img` was absent, `Eva.elf` is not deployed and the wrapper exec's the
stock Eva from the cryptoloop mount instead.  The `oa_authgen.ko` loading still
happens, so the `InstallEXs` wrapper works regardless.

### Signing

The UpdateOS package signature is `SHA1(pretar_content + posttar_content +
UpdaterScriptsKey)`.  The key was extracted from the `UpdateOS` binary data
section.  Full algorithm: [`../docs/crypto/update_signature.md`](../docs/crypto/update_signature.md).

---

## Compatibility

Patch strategies are version-agnostic where possible:
- **OA.ko** — 9 of 11 sites are section-relative offsets (same for any ET_REL
  with stable section layout); 2 sites are `.symtab` symbol lookup (with a
  3.2.1/3.2.2-only fallback offset if the module is ever shipped stripped)
- **loadmod.ko** — `.symtab` symbol lookup with byte-pattern fallback
- **loadoa** — fixed-length string replacement; works on any version containing those paths
- **Eva** — VMA-absolute constants verified against 3.2.1, 3.2.2, and 3.2.3.
  On 3.2.3 the 206-byte `.rodata` code cave used for the auto-auth injection
  is no longer all-zero (22 bytes of linker string-merge-section leftover —
  a `\r`-byte staircase, not code — now sit in it). `patch_eva()`'s safety
  check no longer requires the cave to be literally all-zero; instead it
  scans the whole binary for any absolute pointer landing inside the cave
  range before proceeding, and only refuses if it finds one (i.e. the region
  is still referenced by something live). Confirmed zero such references on
  3.2.3, and confirmed the built package's `Eva.elf` has the cave correctly
  overwritten with the auto-auth code.

The script reports the detected firmware version from known stock MD5s and
**proceeds on unrecognised versions with a warning** (rather than refusing).
Run `--verify` first against an unfamiliar firmware version to check that all
patch sites match before committing to a build.

To add a new firmware version to the known-MD5 table, extract the stock
binaries, compute their MD5s, and add an entry to `KNOWN_STOCK_MD5` in
`patch_firmware_offline.py`.

---

## Further reading

- [`../docs/crypto/auth_string_algorithm.md`](../docs/crypto/auth_string_algorithm.md) — EX auth algorithm
- [`../docs/modules/loadmod.ko_inner_md5_check.md`](../docs/modules/loadmod.ko_inner_md5_check.md) — loadmod dual MD5 check
- [`../docs/workflow/deploying_patches.md`](../docs/workflow/deploying_patches.md) — deployment design rationale
- [`../docs/system_overview.md`](../docs/system_overview.md) — full boot chain
