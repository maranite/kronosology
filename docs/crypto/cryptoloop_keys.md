# The Kronos cryptoloop keys

The Korg Kronos stores three of its core volumes as AES-encrypted ext2
images, attached at boot via the Linux 2.6 **cryptoloop** driver. This
document records the end-to-end key chain: how `loadmod.ko` recovers
the keys from a chip-encrypted blob, what the kernel does with them,
why the *final* keys turn out to be universal across every Kronos unit
we've tested, and what each key actually is.

The operational decryption tools (one Python decryptor + one cryptsetup
mount helper + one version-diff script + the on-device key probe) live
under [`scripts/`](../../scripts/README.md).

---

## The encrypted volumes

| Image                         | Mount on device              | Contents |
|-------------------------------|------------------------------|----------|
| `/korg/ro/Mod.img`            | `/korg/Mod`                  | `OA.ko` plus a couple of small drivers |
| `/korg/ro/Eva.img`            | `/korg/Eva`                  | `Eva` (the userspace app), `UpdateOS`, network/disk helpers |
| `/korg/ro/WaveMotion.img`     | `/korg/rw/PCM/WaveMotion`    | EP-1 Rhodes / Wurlitzer physical-model `.wmms` data |

All three are ext2 filesystems wrapped in a cryptoloop using the same
parameters; only the AES key differs per volume.

---

## Encryption parameters

Derived from binary RE of `loadmod.ko` (the values it writes into the
`loop_info64` struct before `LOOP_SET_STATUS64`) and confirmed against
Korg's published kernel `drivers/block/cryptoloop.c`:

| Field                  | Value                              | Source |
|------------------------|------------------------------------|--------|
| `lo_encrypt_type`      | 18 (`LO_CRYPT_CRYPTOAPI`)          | loadmod writes constant at `loop_info64 + 0x2c` |
| `lo_crypt_name`        | `"aes"` → kernel resolves to `cbc(aes)` | cryptoloop.c uses crypto-API name lookup |
| `lo_encrypt_key_size`  | 32                                 | loadmod writes constant at `+0x30` |
| `lo_encrypt_key`       | 32 bytes — see [Key format](#key-format) below | loadmod calls `HexEncode(pairFact, 16, key_buf)` |
| Sector size            | 512 bytes                          | `LOOP_IV_SECTOR_SIZE` |
| IV mode                | `plain` — `IV[0..3] = sector_num LE32, IV[4..15] = 0` | `cryptoloop_transfer` in cryptoloop.c |

So a sector at file offset `N*512` is decrypted with:

```
key = lo_encrypt_key                              (32 bytes)
iv  = struct.pack("<I", N) + b'\x00' * 12         (16 bytes)
plaintext_sector = AES-256-CBC-decrypt(key, iv, ciphertext_sector)
```

This is `cryptsetup`'s `aes-cbc-plain` mode exactly. The
[`scripts/decrypt_kronos_img.py`](../../scripts/decrypt_kronos_img.py)
implementation is 200 lines of plain Python around `cryptography.hazmat`.

---

## Key format

`lo_encrypt_key` is **32 bytes total**, of which:

- The first 31 bytes are printable ASCII hex characters (`0`–`9`, `a`–`f`)
- The 32nd byte is `\x00` (null)

This is a quirk of Korg's `HexEncode` function — it takes 16 binary
pairFact bytes and writes 31 hex characters into a buffer that was
pre-zeroed to 32 bytes. The 32nd byte remains the original null
padding. All 32 bytes (including the null) are then passed to AES-256
as the key.

This caught us out for a while: the kronoshacker blog quoted the Mod
key as `a336a15cd841ec8926b99e7c3884eaa1` (32 hex chars), but the real
key is `a336a15cd841ec8926b99e7c3884eaa` + `\x00` — same first 31 chars
but a null at position 31 instead of the expected `1`. AES-256 with the
"wrong" 32nd byte produces unrelated output; the blog's key fails to
decrypt the image even though it is one byte away from correct.

---

## The recovered keys

Extracted 2026-06-04 from a live Kronos at 192.168.0.3 via the
`LOOP_GET_STATUS64` ioctl (using [`scripts/getloopkey.s`](../../scripts/getloopkey.s)
as an i386 static probe). 31 ASCII hex chars + a literal `\x00`:

| Volume          | Loop dev at runtime | 31-char ASCII key                       |
|-----------------|---------------------|-----------------------------------------|
| `Mod.img`       | `loop2` (transient) | `a336a15cd841ec8926b99e7c3884eaa`       |
| `Eva.img`       | `loop0`             | `342ee59d549c7d329d835537be0540d`       |
| `WaveMotion.img`| `loop1`             | `3e72c0e59fc017a9eb7d7e1168a4cdb`       |

**These keys are universal across firmware versions and Kronos units.**
We verified by running [`scripts/decrypt_kronos_img.py --check`](../../scripts/decrypt_kronos_img.py)
against every image we have:

```
Image source              Mod  Eva  WaveMotion
KRONOS_Update_3_2_2/...    ✓    ✓    ✓
KRONOS_Update_3_2_1/...    ✓    ✓    ✓
dump from kronos/...       ✓    ✓    ✓
```

The same ciphertext appears byte-for-byte at the same offset in
every version of every image, which is consistent with: same key
applied to identical plaintext (in the case of WaveMotion, the
plaintext also never changed; in the case of Mod and Eva, only
specific files inside the ext2 changed between versions).

Why "Mod.img is loop2 (transient)": `loadmod.ko` mounts Mod.img
early in boot to insmod `OA.ko`, then unmounts the loop device.
Once the system is fully up, `getloopkey /dev/loop2` returns
`ENXIO` — Mod.img isn't attached anymore. So the Mod key has to
be captured during the brief boot window. Eva and WaveMotion stay
mounted at runtime and can be probed at any time.

---

## How loadmod recovers these keys at boot

End-to-end flow inside `init_module`:

```
1. ReadPairFactAndVerify (at loadmod offset 0x4e90):
   - Assembles the path "/.pairFact3" byte-by-byte on the stack:
       [esp+4..0xf] = '/' '.' 'p' 'a' 'i' 'r' 'F' 'a' 'c' 't' '3' '\0'
   - filp_open("/.pairFact3", O_RDONLY)  → kernel struct file *
   - reads exactly 0x50 (80) bytes into a stack buffer
   - this 80-byte blob is the chip-encrypted pairFact data

2. RetrieveSecurityICKey (at loadmod offset 0x3e10):
   - sends the 80-byte blob to the stgNV2AC chip via
     OmapNKS4Module.ko's stgNV2AC_sync_read_cmd
   - the chip uses its tamper-resistant secret to decrypt the blob
   - response contains 16 binary bytes of raw pairFact key material
     for each of the 3 volumes (plus, plausibly, some integrity bytes
     — we have not had occasion to RE the exact plaintext layout)

3. For each of the 3 volumes:
   - HexEncode the 16 binary bytes → 31 hex chars (writing into a
     pre-zeroed 32-byte buffer, so a null sits in byte 31)
   - copy the 32 bytes into loop_info64.lo_encrypt_key
   - set lo_encrypt_type=18, lo_encrypt_key_size=32, lo_crypt_name="aes"
   - issue LOOP_SET_STATUS64 on the appropriate /dev/loopN

4. cryptoloop.c picks up the configured loop and from then on
   handles every sector read/write transparently with AES-256-CBC
   plus the plain IV.
```

The kernel modifications and the symbol-naming for this path are
in [`docs/modules/loadmod.ko_analysis.md`](../modules/loadmod.ko_analysis.md).

---

## `/.pairFact3` — the chip-encrypted source blob

A captured copy of `/.pairFact3` from a 2.6.32 Kronos running 3.2.2 is
included here as binary evidence: [`pairFact3.bin`](pairFact3.bin)
(80 bytes, MD5 `817956d550647905828e115f9eae7a0e`, device mtime
2021-06-11 — the factory provisioning timestamp; never rewritten on
this unit).

Hex dump of the 80 bytes:

```
0000  0a e6 e2 57 39 78 4e ca 47 50 7e 83 30 0d 06 fa
0010  b9 6f e5 62 0b d5 d0 2b 83 95 6e 0a bf dc 6c 06
0020  37 03 c8 ed 11 d3 c2 e1 95 61 63 3f 95 18 b8 ba
0030  74 29 eb 35 c9 70 9f be 9b d4 36 8a 1d b3 f0 94
0040  ca 9d ab b7 23 9e 3d 53 8f f7 1d 5e af 98 04 a2
```

Statistical profile:

| Metric | Observed | Uniform-random expectation |
|--------|----------|-----------------------------|
| Mean byte | 127.1 | 127.5 |
| Stdev      | 74.3  | 73.9 |

Indistinguishable from random — content is fully encrypted, no
plaintext header, no visible structure. Plausible plaintext layout
(80 bytes = 5 × 16-byte AES blocks): `[IV/header] [Mod key 16]
[Eva key 16] [WaveMotion key 16] [MAC/check]`. Educated guess only;
we can't confirm without observing the chip's decrypted output.

### What this file does NOT give us

Cannot be decrypted offline — the stgNV2AC chip secret is tamper-
resistant, never exposed to the host CPU. So `/.pairFact3` does not
let us recover the raw 16-byte pairFact bytes; we only learned the
*final* ASCII keys because the kernel briefly stores them in
`loop_info64.lo_encrypt_key` and exposes them to root via
`LOOP_GET_STATUS64`.

### Why we keep it anyway

- **Insurance against firmware key rotation.** If a future Korg
  firmware revs the chip's pairFact ciphertext, having this factory
  blob preserved means we can detect the change.
- **Comparison across units.** Comparing this blob's bytes against
  `/.pairFact3` from a different Kronos would prove whether the
  blob is per-device (chip-specific) or factory-uniform. The
  *output* keys are universal across units, but the input blob
  could still be device-bound if each chip's secret is different
  and the blob is encrypted-to-that-chip.
- **Future RE.** If anyone instruments loadmod with kprobes on
  `stgNV2AC_sync_read_cmd` to dump the chip's response, this blob
  is the corresponding input.

---

## Why the blog said `/proc/iFactc3` and what's actually true

The kronoshacker blog (Korg Kronos boot-process series, 2016)
documented this path as a proc entry `/proc/iFactc3` provided by a
kernel patch. That's wrong in the version we have evidence for:

| Evidence | Finding |
|----------|---------|
| `ls /proc/iFactc3` on live 3.2.2 Kronos | `No such file or directory` |
| Binary RE of `ReadPairFactAndVerify` | The string `/.pairFact3` is assembled byte-by-byte on the stack (offsets 0x4e6d..0x4e9f) before `filp_open`. No proc-fs lookup function involved. |
| `stat /.pairFact3` on live device | 80 bytes, regular file, ext2 root, factory mtime |

So the operative path is **`/.pairFact3`** — a regular file at the
ext2 root, *not* a `/proc/` entry. (It is possible Korg used a proc
path in an earlier firmware and the blog was correct for that era;
we have no way to test, but every 2014, 3.2.1, and 3.2.2 sample
we have shows the regular-file layout.)

`loadmod`'s MD5 filesystem-integrity check filters out filenames
starting with `.pairFact`, so the file is *present* on the ext2 root
but invisible to the integrity hash, consistent with it being a
device-resident credential separate from the system files.

---

## How firmware updates change (or don't change) the keys

Comparison of every encrypted volume across the 3.2.1 → 3.2.2 update,
produced by [`scripts/diff_kronos_versions.sh`](../../scripts/diff_kronos_versions.sh)
(see [`workflow/firmware_version_diff.md`](../workflow/firmware_version_diff.md)
for the methodology):

| Volume          | Files changed |
|-----------------|----------------|
| `Mod.img`       | 1 of 3 — `OA.ko` (real code change: +16 bytes in `.text`) |
| `Eva.img`       | 1 of 23 — `Eva` (8 bytes: `__DATE__` and `__TIME__` only) |
| `WaveMotion.img`| 0 of 7 |

The keys themselves did not change. Korg's firmware-update process
does not appear to touch `/.pairFact3` — consistent with the file's
factory-provisioning mtime not advancing across firmware updates on
the unit we examined.

---

## Related files

- [`scripts/README.md`](../../scripts/README.md) — operational usage of the tools
- [`scripts/decrypt_kronos_img.py`](../../scripts/decrypt_kronos_img.py) — the offline decryptor (keys embedded)
- [`scripts/mount_kronos_img.sh`](../../scripts/mount_kronos_img.sh) — cryptsetup mount path
- [`scripts/diff_kronos_versions.sh`](../../scripts/diff_kronos_versions.sh) — end-to-end version diff
- [`scripts/getloopkey.s`](../../scripts/getloopkey.s) — on-device key-recovery probe
- [`docs/modules/loadmod.ko_analysis.md`](../modules/loadmod.ko_analysis.md) — full RE of the boot-time integrity chain
- [`docs/crypto/atmel_nv2ac.md`](atmel_nv2ac.md) — the chip itself (stgNV2AC, panel-side protocol)
- `pairFact3.bin` (alongside this file) — captured factory blob, 80 bytes
