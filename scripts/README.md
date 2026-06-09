# `scripts/` — offline tools for the encrypted Kronos volumes

Four standalone tools that together let you decrypt, browse, mount, and
*diff* the three encrypted loop-back filesystems that the Kronos uses for its
core binaries — entirely off-device.

Kronos firmware has 3 encrypted volumes, each of which are encrypted using the following keys:

| Image                         | Mount point on device          | Contents | Key (31 chars + `\x00`)                 |
|-------------------------------|--------------------------------|----------|------------------------------------------|
| `/korg/ro/Mod.img`            | `/korg/Mod`                    | `OA.ko` (the synthesis engine) and a couple of small kernel modules | `a336a15cd841ec8926b99e7c3884eaa`        |
| `/korg/ro/Eva.img`            | `/korg/Eva`                    | `Eva` (the main userspace application), `UpdateOS`, network/disk helpers | `342ee59d549c7d329d835537be0540d`        |
| `/korg/ro/WaveMotion.img`     | `/korg/rw/PCM/WaveMotion`      | EP-1 electric-piano physical-model `.wmms` data (Rhodes / Wurlitzer) | `3e72c0e59fc017a9eb7d7e1168a4cdb`        |


Note: The keys are 31 printable ASCII hex chars + one trailing null byte — a
quirk of Korg's `HexEncode` writing 31 chars into a pre-zeroed 32-byte
buffer.
`loadmod.ko` sets these mounts up at boot via Linux cryptoloop. 
Each volume uses **AES-256-CBC with a "plain" IV** (sector number as LE32 in `IV[0..3]`,
zeros in `IV[4..15]`) and a key delivered to the kernel as part of a
`LOOP_SET_STATUS64` ioctl. 

The key originates from `/.pairFact3` (an 80-byte blob the stgNV2AC chip decrypts), 
but the *final* AES keys are universal and **constant** across every Kronos unit and every firmware version. 
You do not need the Kronos' ATMEL security chip to decrypt these volumes.

See [`docs/modules/loadmod.ko.md`](../docs/modules/loadmod.ko.md) for the
boot-time integrity chain that delivers these keys to the kernel, and
[`docs/system_overview.md`](../docs/system_overview.md) for how the three
volumes fit into the wider boot sequence.

---

## What's here

| File                       | Purpose | Needs root? | Needs the Kronos? |
|----------------------------|---------|-------------|--------------------|
| `decrypt_kronos_img.py`    | Decrypt an `.img` into a plaintext ext2 img | No | No |
| `mount_kronos_img.sh`      | Mount an encrypted `.img` file | Yes | No |
| `diff_kronos_versions.sh`  | End-to-end version diff: decrypt + extract + manifest + diff | No | No |
| `patch_oa_ko.py`           | Apply the canonical 11-site "bypass everything" patch set to any stock OA.ko (works across firmware versions because patches are addressed by ELF section + offset, not file offset) | No | No |
| `getloopkey.s`             | i386 assembly source for an on-device tool that prints `LOOP_GET_STATUS64` output (used once, to recover the keys above) | (root on Kronos) | Yes |

---

## `decrypt_kronos_img.py` — the offline decryptor

Pure Python, no `sudo`, no `cryptsetup`. Requires `python3-cryptography`
(`pip install cryptography` or `apt install python3-cryptography`).

```sh
# 1) Check whether a key works (reads only sector 2 — instant):
python3 decrypt_kronos_img.py --check path/to/Mod.img

# 2) Decrypt a single image to a plaintext ext2:
python3 decrypt_kronos_img.py path/to/Eva.img Eva_plain.img

# 3) Bulk-decrypt all three known images from one update directory:
python3 decrypt_kronos_img.py --decrypt-all \
    KRONOS_Update_3_2_2/mnt/korg/ro  ./decrypted_322

# 4) Override the image-type auto-detection (if the file was renamed):
python3 decrypt_kronos_img.py --type eva renamed.img out.img
```

The image type is inferred from the filename
(`Mod.img` / `Eva.img` / `WaveMotion.img`, case-insensitive). If you
renamed the file, pass `--type {mod|eva|wm}` explicitly.

Validation: after decryption the tool re-reads sector 2 and checks the
ext2 magic (`0xEF53` at offset 56). A wrong key produces a non-zero
exit code and a clear error.

### Browsing the decrypted ext2 without mounting

`debugfs` (part of `e2fsprogs`, ships on every Linux box) lets you walk
the ext2 with no kernel involvement at all:

```sh
debugfs -R 'ls -l /'              Mod_plain.img
debugfs -R 'dump /OA.ko OA.ko'    Mod_plain.img
debugfs -R 'rdump / ./extracted'  Mod_plain.img
```

`rdump` is especially useful — it recursively dumps the whole tree to a
host directory. (You'll see harmless `Operation not permitted while
changing ownership` warnings if you run as non-root; the file *contents*
are copied correctly, only the original UID/GID 500 isn't preserved.)

---

## `mount_kronos_img.sh` — direct cryptsetup mount

If you'd rather mount than dump, this wraps `cryptsetup open --type
plain --cipher aes-cbc-plain --key-size 256` with the right keyfile
(31 ASCII chars + a literal `\x00` byte) and then `mount -o ro`.

```sh
sudo apt-get install cryptsetup    # if not already installed

sudo ./mount_kronos_img.sh mod  path/to/Mod.img        /mnt/mod
sudo ./mount_kronos_img.sh eva  path/to/Eva.img        /mnt/eva
sudo ./mount_kronos_img.sh wm   path/to/WaveMotion.img /mnt/wm
sudo ./mount_kronos_img.sh auto path/to/Eva.img        /mnt/eva   # auto-detect type

# unmount cleanly (closes the dm-crypt mapper too):
sudo ./mount_kronos_img.sh mod path/to/Mod.img /mnt/mod unmount
```

`<type>` is `mod | eva | wm | auto`. Use `auto` if the file is named
`Mod.img` / `Eva.img` / `WaveMotion.img`.

The Python path (`decrypt_kronos_img.py` + `debugfs`) is preferred when
you only need to read; this script exists because some workflows
(rebuilding an image, modifying it then re-encrypting, running tools that
need a real filesystem) actually want a live ext2 mount.

---

## `getloopkey.s` — extract a key from a live Kronos

This is the tool used *once* (2026-06-04) to recover the three keys
embedded in `decrypt_kronos_img.py`. It is **not needed for normal
decryption** — the keys are universal and already shipped — but it's
preserved here for two reasons:

1. **Reproducibility.** Anyone can re-verify the keys themselves against
   their own Kronos.
2. **Future-proofing.** If Korg ever rotates the cryptoloop keys in a
   future firmware (we have no evidence they have, but it's possible),
   this is how you recover the new set.

It is i386 Linux assembly, raw syscalls, no libc — so the resulting
binary works on the Kronos's 2.6.32 32-bit x86 kernel (the device has
no `python3`, which is why we couldn't use the `LOOP_GET_STATUS64` ioctl
from a script directly).

### Building

```sh
# On any Linux host that can produce i386 binaries:
as --32 -o getloopkey.o getloopkey.s
ld  -m elf_i386 -static -o getloopkey getloopkey.o
file getloopkey
# → ELF 32-bit LSB executable, Intel 80386, statically linked
```

If your host doesn't have a 32-bit toolchain, install
`gcc-multilib binutils` (Debian/Ubuntu) or use a Docker image.

### Using

```sh
# Push to the running Kronos (root@<ip>, password "kronos" by default):
scp getloopkey root@192.168.0.3:/tmp/

# Probe all loop devices:
ssh root@192.168.0.3 \
    'chmod +x /tmp/getloopkey;
     for n in 0 1 2 3 4 5; do
         echo "=== /dev/loop$n ==="
         /tmp/getloopkey /dev/loop$n 2>&1
     done'
```

Expected output for an encrypted Kronos loop device:

```
Backing file : /korg/ro/Eva.img
Cipher       : aes
Encrypt type : 18
Key size     : 32
Key (hex)    : 3334326565353964353439633764333239643833353533376265303534306400
```

The `Key (hex)` field is the 32 raw bytes of the AES-256 key as 64 hex
chars. Decode those 32 bytes — 31 of them are ASCII hex characters and
the last byte is `\x00`. That 31-char ASCII string is what
`decrypt_kronos_img.py` and `mount_kronos_img.sh` carry as a constant.

### Caveat: loop2 (Mod.img) is transient

`loadmod.ko` mounts Mod.img early in boot, loads `OA.ko` from it,
and then *unmounts* it. So `getloopkey /dev/loop2` typically returns
`Error: ioctl failed` once the system is fully up — Mod.img isn't
attached anymore. The Mod key was originally captured by inspecting
`/sys/...` during the brief boot window. Eva and WaveMotion remain
mounted at runtime, so loop0 and loop1 are easy to probe at any time.

---

## `diff_kronos_versions.sh` — end-to-end firmware-version diff

Walks two Korg-update trees and tells you exactly what changed —
including *inside* the encrypted volumes:

```sh
./diff_kronos_versions.sh  KRONOS_Update_3_2_1  KRONOS_Update_3_2_2/mnt  [workdir]
```

(`workdir` defaults to `/tmp/kronos_diff`; the script caches both
decrypted images and extracted file trees there, so reruns are fast.)

Sample output (a real 3.2.1 → 3.2.2 run):

```
================================================================
  Kronos firmware diff:  KRONOS_Update_3_2_1  →  mnt
================================================================
── Decrypt + extract ──
  [OLD] decrypt Mod.img ...
  [OLD] extract Mod.img ...
  [OLD] decrypt Eva.img ...
  ... (etc) ...

────────────────────────────────────────────────────────
  Mod.img
────────────────────────────────────────────────────────
    files (old/new): 3/3
    changed:
      ~ OA.ko  (14,285,504 → 14,285,512 bytes, +8)

────────────────────────────────────────────────────────
  Eva.img
────────────────────────────────────────────────────────
    files (old/new): 23/23
    changed:
      ~ Eva  (22,930,065 → 22,930,065 bytes, ±0)

────────────────────────────────────────────────────────
  WaveMotion.img
────────────────────────────────────────────────────────
    files (old/new): 7/7
    ✓ identical content

────────────────────────────────────────────────────────
  Unencrypted overlay
────────────────────────────────────────────────────────
    files (old/new): 267/267
    changed:
      ~ korg/VersionInfo  (106 → 106 bytes, ±0)
      ~ sbin/loadmod.ko  (52,384 → 52,384 bytes, ±0)
```

### What this is doing under the hood

| Step | Tool used | Output |
|------|-----------|--------|
| 1. Decrypt each `.img`  | `decrypt_kronos_img.py` | One plaintext ext2 image per volume |
| 2. Extract files        | `debugfs -R "rdump / <out>" <img>` | A regular directory tree per volume |
| 3. Hash files           | Python `hashlib.md5` over file contents | `{path → (size, md5)}` dict per volume |
| 4. Set-difference       | Python set algebra over the dicts | added / removed / changed lists |
| 5. Overlay diff         | Same logic on the unencrypted update tree | Catches non-image changes too |

The MD5 hash is needed because most binary updates preserve file *layout*
(same ELF structure, same string tables) while the content changes — so
size alone often won't catch real changes. Hashing 600 MB of decrypted
firmware completes in under 2 seconds on any modern disk.

### Drilling into a changed binary

Once `diff_kronos_versions.sh` identifies which files changed, the
follow-up technique depends on what you want to learn:

| Question | Technique |
|----------|-----------|
| Where in the file did it change? | Python: `[i for i in range(N) if a[i]!=b[i]]`, report first/last index and a 1 KB-window cluster count |
| What did the change look like? | `python3 -c 'print(open(f,"rb").read()[off-32:off+32].hex())'` or `xxd` a small window around `first_diff` |
| Did any ELF section change size?  | `diff <(readelf -WS old.ko) <(readelf -WS new.ko)` |
| Are there new/removed strings?    | `diff <(strings -n6 old | sort) <(strings -n6 new | sort)` |
| What function changed in a `.ko`? | Ghidra Version Tracking from the annotated old version (see [`docs/workflow/`](../docs/workflow/)) |

For the 3.2.1 → 3.2.2 case, drilling in this way revealed that:

- **Eva**'s 8 differing bytes are the embedded `__DATE__` / `__TIME__`
  rebuild stamp (`Oct  7 2025 08:41:24` → `Apr  9 2026 08:44:16`).
  The Eva binary is functionally identical.
- **OA.ko** has a real but tiny code change (+16 bytes in `.text`) that
  shifted every downstream relocation, producing 2.75 MB of byte drift
  from a single small insertion. Ideal target for Ghidra Version Tracking.
- **loadmod.ko** has 974 differing bytes in one localized 1 KB region
  (0x6cca–0x70a8) — the actual semantic change in the update is here.

---

## `patch_oa_ko.py` — apply the canonical patch set offline

For when you have a new firmware version's stock `OA.ko` and want the same
"bypass everything" patches that the `patcher/kronos_patcher.sh` script
deploys. Pure Python, no Ghidra needed, no root.

```sh
# Apply patches:
python3 patch_oa_ko.py /path/to/stock/OA.ko ./OA.ko.patched

# Check whether a binary is patched or stock:
python3 patch_oa_ko.py --verify /path/to/OA.ko
```

The 11-patch table is embedded in the script and is addressed by
**ELF section name + section-relative offset** (not file offset), so the
same table works across firmware revisions that don't reorganize these
specific functions. The script:

- Recognises known stock and patched MD5s (3.2.1 + 3.2.2 listed inline)
- Verifies the original bytes at each patch site before writing
- Refuses to touch any site whose bytes don't match either the stock or
  the patched values (catches partial/corrupted/version-mismatched files)
- Re-runnable: idempotent on an already-patched binary

Sanity check: applying the table to stock 3.2.1 produces a binary with
MD5 `163550b60b7508b2c0ba1fd314b0b944` — byte-for-byte identical to the
canonical patched 3.2.1 OA.ko produced by the original analyst session.

The Ghidra equivalent — `ApplyKronosOaPatch.java` — lives in
`~/ghidra_scripts/` and applies the same patches inside an open Ghidra
program (see [`docs/workflow/ghidra_patch_application.md`](../docs/workflow/ghidra_patch_application.md)
for the broader in-Ghidra patching workflow).

---

## End-to-end workflow examples

### A) "I want OA.ko from a new firmware version, no Kronos required"

```sh
# 1) Unpack the update tarball (you already have KRONOS_Update_X_Y_Z/mnt/korg/ro/Mod.img)

# 2) Decrypt:
python3 scripts/decrypt_kronos_img.py \
    KRONOS_Update_X_Y_Z/mnt/korg/ro/Mod.img  Mod_plain.img

# 3) Extract OA.ko (no sudo needed):
debugfs -R 'dump /OA.ko OA_new.ko' Mod_plain.img
```

### B) "Show me everything that changed between two firmware versions"

```sh
./scripts/diff_kronos_versions.sh  \
    KRONOS_Update_3_2_1            \
    KRONOS_Update_3_2_2/mnt        \
    /tmp/diff_321_322
```

The output identifies the handful of binaries that actually differ.

### C) "I bought a new Kronos and want to verify the keys"

(You only need to do this if you suspect Korg rotated the keys.)

```sh
# Build the on-device probe (once, on your dev box):
as --32 -o getloopkey.o scripts/getloopkey.s
ld  -m elf_i386 -static -o getloopkey getloopkey.o

# Run on the Kronos:
scp getloopkey root@192.168.0.3:/tmp/
ssh root@192.168.0.3 \
    'chmod +x /tmp/getloopkey; for n in 0 1; do /tmp/getloopkey /dev/loop$n; done'
```

`loop0` is Eva.img, `loop1` is WaveMotion.img, both mounted at runtime.
(Mod.img / `loop2` is unmounted after boot, so you'd need to capture
that one during the brief boot window or by causing a remount.)

The 32 raw bytes from the `Key (hex)` line should match the keys
embedded in `decrypt_kronos_img.py`. If they don't, you've found a
key-rotation in a new firmware version — update the constants in the
two tools and you're good to go again.

---

## Required dependencies

| Tool                       | Needs                                                |
|----------------------------|------------------------------------------------------|
| `decrypt_kronos_img.py`    | `python3`, `python3-cryptography`                    |
| `mount_kronos_img.sh`      | `cryptsetup` (Debian/Ubuntu: `apt install cryptsetup`), root |
| `diff_kronos_versions.sh`  | `python3`, `python3-cryptography`, `debugfs` (in `e2fsprogs`) |
| `getloopkey.s`             | (build) `as` + `ld` with i386 target (`gcc-multilib binutils` on amd64 hosts); (run) root on the Kronos |

---

## Why this exists

The decryption keys for the three loop volumes were the main barrier to
*serious* offline RE work on Kronos firmware. Once we had them, the
in-the-image binaries (`OA.ko`, `Eva`, `loadmod.ko` after a rebuild,
`UpdateOS`, the panel/audio drivers) became as readable as any other
ELF on disk. The diff workflow built on top of that turned firmware
update analysis from "spend a week studying OA.ko 3.2.2" to
"run one shell command and see that only 8 bytes of Eva changed and
they're the build date."

The keys themselves are universal across all Kronos units and every
firmware version we've checked, so once shipped in these tools they
should remain useful indefinitely — unless and until Korg rotates them
in a future release, in which case `getloopkey.s` gives you the recipe
to find the new ones.
