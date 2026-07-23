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
   - reads the 24-byte Zone0 key material (the same `0x10`/`0x18`/`0x20`
     zones the EX-auth chain uses) via the chip's already-documented
     authenticated read path
   - Blowfish-CFB-64-decrypts the 80-byte blob HOST-SIDE with that key
     material (key=Zone0[0x00:0x10], iv=Zone0[0x10:0x18]) — this is
     `moancjsd82` with `p3=80`, the exact same primitive the EX-auth chain
     uses with `p3=15` (see `oa_crypto.h`'s header comment, and
     `docs/crypto/cryptoloop_keys.md`'s own ".reauth" section below for the
     confirmed 80-byte plaintext layout)

   **Corrected 2026-07-16** — this step used to say "sends the 80-byte blob
   to the stgNV2AC chip... the chip uses its tamper-resistant secret to
   decrypt the blob." That's now confirmed wrong: decrypting the raw file
   contents directly with plain host-side Blowfish (no chip involvement
   beyond the already-solved Zone0 read) reproduces the correct,
   MD5-integrity-checked plaintext on two real devices. Whatever the exact
   original disassembly observation behind the old wording was (possibly a
   misread of the chip call used to obtain Zone0 itself, conflated with
   the blob-decrypt step, or a different function than `bbbbbbbba12` —
   `oa_crypto.h`'s independently-arrived-at finding names `bbbbbbbba12`
   specifically as the `p3=80` `moancjsd82` caller), the *algorithm* is now
   settled by working end-to-end reproduction against real hardware data,
   regardless of that naming detail.

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

**Corrected 2026-07-16 — this whole section was wrong.** Originally claimed
`/.pairFact3` "cannot be decrypted offline — the stgNV2AC chip secret is
tamper-resistant, never exposed to the host CPU." That's backwards. See
[the section below](#reauth-is-pairfact3-and-it-decrypts-with-plain-host-side-blowfish)
for the corrected mechanism — in short, `/.pairFact3` decrypts with the same
plain Blowfish-CFB-64 as everything else in this document, entirely
host-side, using nothing but the target device's own Zone0 data. The one
thing genuinely gated behind tamper-resistant chip access is *obtaining
Zone0* — a completely different, already-solved problem
(`KronosExtract`/`AT88VirtualChip`). This specific file, `pairFact3.bin`, was
never decrypted here for the mundane reason that no matching
`KronosExtract.bin` for *that* device exists in this repo — not because
decryption was impossible in principle.

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

## `.reauth` IS `.pairFact3`, and it decrypts with plain host-side Blowfish

Found 2026-07-16 while investigating (for an unrelated question — whether the
EX-auth chip secret is derivable from the Public ID) captured data that turned
out to include full `KronosExtract.bin` captures **and matching `.reauth`
files** for three real physical units (two Kronos, referred to here as
`947e`/`6630`; one Nautilus, `2D68` — no `.reauth` for the Nautilus one).
(Device identifiers/public IDs and the source file paths intentionally not
reproduced here — private per-device info.)

**`.reauth` and `.pairFact3` are the same file format, byte-for-byte —
confirmed two ways, not assumed:**
1. `KronosExtract/build/kronos.py` (already in this repo) implements both
   `pf3_generate()`/`pf3_decrypt()` under a `# .reauth / .pairFact3
   (bbbbbbbba12 path in loadmod.ko)` comment, one shared code path, no
   format distinction anywhere.
2. Directly confirmed on real hardware: a captured `.reauth` file renamed to
   `.pairFact3` and placed at `/.pairFact3` works as the real thing.

An **earlier version of this section got the mechanism wrong** — it guessed
`.reauth` must be a separate, already-chip-processed artifact, re-encrypted
for storage, because the *original* text above claims `/.pairFact3` "cannot
be decrypted offline." That guess was unnecessary and incorrect. The real
mechanism was already sitting in this repo, just not cross-referenced here:
`reconstructed/OA/include/oa_crypto.h`'s own header comment on `moancjsd82`
(the confirmed, hardware-verified Blowfish-CFB-64 primitive already
documented above for EX-auth strings, called there with `p3=15`) states
plainly:

> `p3` is the expected plaintext length; OA.ko's `ParseAuth` calls it with
> `p3=15` (EXs-style entries), **`loadmod.ko`'s `bbbbbbbba12` calls its own
> separate copy with `p3=80` (`.pairFact3`/`.reauth`)**.

So this was never an open "what wire format does `RetrieveSecurityICKey`
use" question at all (see the correction to "How loadmod recovers these
keys at boot" below) — `/.pairFact3` (or a `.reauth` file used in its
place) decrypts with the **exact same CFB-64 decode** as an EX-auth string,
just a different key source (Zone0 `0x00`-`0x17`, the same 24 bytes
`KronosExtract`/`AT88VirtualChip` already read) and a different length
(80 instead of 15). No chip interaction beyond the already-solved Zone0
read is involved in the decrypt itself.

```python
from Crypto.Cipher import Blowfish
key, iv = zone0[0:16], zone0[16:24]           # from KronosExtract.bin
pt = Blowfish.new(key, Blowfish.MODE_CFB, iv=iv, segment_size=64).decrypt(reauth_bytes)
```

Ran this against both Kronos units' `.reauth` files (filenames omitted —
they embed each device's public ID — confirmed **not** the same ciphertext
as this document's own `pairFact3.bin` by MD5, i.e. a genuinely different,
per-device blob, not something already captured). Both decrypt cleanly, and — checked
against `kronos.py`'s own exact plaintext layout (`_pf3_plaintext`), not a
guess this time — every field matches:

| Offset | Field | `947e` | `6630` | Identical across units? |
|---|---|---|---|---|
| `0x00`-`0x0e` | nonce (15 bytes, random per generation) | `3e96a25e591c5b2960b291c6d7d3e1` | `5529663042a7cc5dc07a570298475e` | No — per-device, expected |
| `0x0f`-`0x3f` | **FIXED block (49 bytes)**: `0x03` + Mod key(16) + Eva key(16) + WaveMotion key(16) | `03a336a15cd841ec8926b99e7c3884eaa7342ee59d549c7d329d835537be0540d23e72c0e59fc017a9eb7d7e1168a4cdbe` | (same) | **Yes, byte-exact** |
| `0x40`-`0x4f` | `MD5(plaintext[0x00:0x40])` — integrity check, not secret material | `40712a60928dee54c6308a730ff49e52` | `b804eec221d8dbad90d48068e76af4de` | No — per-device (function of the per-device nonce) |

The FIXED block matches the already-known 31-char ASCII keys ("The
recovered keys" above) exactly on their first 15 bytes each, on **two
independent, unrelated physical units**, and matches byte-for-byte between
the two units including the 16th byte of each key — which the ASCII
`LOOP_GET_STATUS64` capture could never reveal (`HexEncode` only ever emits
31 of 32 hex chars, see "Key format" above). **The true, complete 16-byte
raw keys are therefore now known for the first time:**

| Volume | Full 16-byte key (previously: 15 bytes + unknown nibble) |
|---|---|
| Mod | `a336a15cd841ec8926b99e7c3884eaa7` → 16 bytes: `a3 36 a1 5c d8 41 ec 89 26 b9 9e 7c 38 84 ea a7` |
| Eva | `342ee59d549c7d329d835537be0540d2` → 16 bytes: `34 2e e5 9d 54 9c 7d 32 9d 83 55 37 be 05 40 d2` |
| WaveMotion | `3e72c0e59fc017a9eb7d7e1168a4cdbe` → 16 bytes: `3e 72 c0 e5 9f c0 17 a9 eb 7d 7e 11 68 a4 cd be` |

(`AT88VirtualChip/pairfact_fixture.cpp` previously guessed `0xa0`/`0xd0`/`0xb0`
for that unknown 16th byte, arbitrarily — turns out the real values are
`0xa7`/`0xd2`/`0xbe`. Corrected there too, same day, alongside a new general
`pf3_decrypt()` implementing this exact mechanism using
`reconstructed/OA`'s already-verified `moancjsd82`/MD5 — see
`AT88VirtualChip/pairfact_decrypt.h`.)

**What this does and doesn't mean:**
- `/.pairFact3` is **not** tamper-resistant or chip-processed at rest. The
  only genuinely tamper-resistant step in this whole chain is obtaining
  Zone0 in the first place (the AT88 `$B8` authenticated handshake) — a
  completely different, already-fully-solved problem
  (`KronosExtract`/`AT88VirtualChip`). Once you have a device's Zone0, its
  `/.pairFact3`/`.reauth` is exactly as decryptable as an EX-auth string,
  using the identical primitive.
- It's an independent, second confirmation of "The recovered keys" above,
  via a completely different method (direct decrypt vs. the
  `LOOP_GET_STATUS64` side-channel), on two different physical units.
- These are universal keys, confirmed identical on two independent physical
  units once decrypted — the outer ciphertext is per-device (confirmed
  different by MD5, since the nonce differs each time a `.reauth` is
  generated), but what it decrypts to is fixed content, not secret
  key-derivation material.
- **Only 2 of 3 available units have a matching `.reauth` file** (`2D68`, the
  Nautilus, doesn't). If a Nautilus `.reauth` ever turns up, decrypting it
  would additionally confirm these keys are universal *across product lines*,
  not just across Kronos units — plausible given the identical universal
  config-zone constants already observed across all three units' config
  zones (see [`atmel_nv2ac.md`](atmel_nv2ac.md)'s "Config-zone cross-check
  across 3 real units"), but not yet directly tested for this specific
  artifact.

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
produced by `scripts/diff_kronos_versions.sh` (that script no longer exists —
removed in the `auto-auth and offline-patcher` reorg commit — but the same
methodology is described in
[`workflow/firmware_version_diff.md`](../workflow/firmware_version_diff.md)
and is what the 3.2.2 → 3.2.3 comparison below repeats manually: decrypt with
`offline-patcher/patch_firmware_offline.py`'s `_decrypt_img`, `debugfs -R
"rdump / <dir>" <img>` to extract, `diff -rq` + per-function symbol-table
diffing to localize changes):

| Volume          | Files changed |
|-----------------|----------------|
| `Mod.img`       | 1 of 3 — `OA.ko` (real code change: +16 bytes in `.text`) |
| `Eva.img`       | 1 of 23 — `Eva` (8 bytes: `__DATE__` and `__TIME__` only) |
| `WaveMotion.img`| 0 of 7 |

The keys themselves did not change. Korg's firmware-update process
does not appear to touch `/.pairFact3` — consistent with the file's
factory-provisioning mtime not advancing across firmware updates on
the unit we examined.

### 3.2.2 → 3.2.3 (2026-07-23)

Same universal keys, same methodology, decrypted and diffed both `Mod.img`
and `Eva.img` in full:

| Volume          | Files changed |
|-----------------|----------------|
| `Mod.img`       | 1 of 3 — `OA.ko` (+240 bytes: `.text` +176B, `.rodata` +64B) |
| `Eva.img`       | 3 of 23 — `Eva` (−22 bytes), `NAND/EDITRES/ENG/TSTENG.PEG` (+69B), `NAND/STARTUP/ALL/EDITRES/BITMAPS.PEG` (+76,784B) |
| `WaveMotion.img`| content changed (same file, +26.3MB in the outer encrypted image; see below) |

`OA.ko` has the same function/symbol count as 3.2.2 (0 added, 0 removed) — only
5 functions changed size, all identified by disassembling and tracing
relocations rather than trusting the raw byte diff (which is 10 of 14.3MB,
almost entirely address-drift noise from the 176-byte `.text` growth, not
actual content change):

| Function | Δ size | Finding |
|---|---|---|
| `CSTGKLMManager::AuthorizeProduct` | +141B | New block compares an installed product's ID against literal string `"S043"` (at `.rodata.str1.1+0x520`); on match, auto-authorizes it directly via a call to itself recursively-adjacent-looking `AuthorizeProduct`, ahead of the normal per-user `AuthorizationStrings` flow. Reads as Korg auto-whitelisting one new factory-bundled product, not an anti-tamper measure. |
| `CSTGInstalledEXProducts::Initialize` | +147B | Gained the `"S043"` auto-authorize loop above (confirmed via relocations: `CountProductCallback`, `LoadProductCallback`, `kAuthFileName`, `ParseAuths`, `CSTGFile_ReadFile`/`FreeReadFile`). |
| `CSTGInstalledEXProducts::ReInitialize` | −160B | Lost an ~40-instruction block that is (by relocation signature) the same code that appeared in `Initialize` above — a refactor/move, not new logic. |
| `CSTGEPModel::GetTotalWaveMotionDataSize` | +8B | Consistent with the WaveMotion content growth below. |
| `CSTGEPModel::Initialize` | +16B | Consistent with the WaveMotion content growth below. |

`AuthorizeProduct`'s growth is **not** auth-bypass hardening either: the new
comparison there is against `CSTGMultisampleBankUUIDBase::sLegacyBankPrefix`,
followed by a call to `CSTGMultisampleBankManager::AccessBankWithLegacyRAMAlias`
— legacy multisample-bank-UUID addressing compatibility, unrelated to license
checking.

**Practical fallout for the offline patcher:** 9 of `OA.ko`'s 11 known bypass
patch sites still match byte-for-byte at their old offsets. The other 2
(`CSTGTG92OscBase`/`CPianoOsc`'s `IsUsingAnyUnauthorizedMultisamples`) contain
the *exact same unpatched bytes* as 3.2.2, just shifted +0x90 (144 bytes)
further into `.text` by the unrelated growth above — not deliberately moved to
dodge the patch table. Fixed in `patch_firmware_offline.py` by resolving those
two via `.symtab` symbol name instead of a raw section offset (see
`OA_SYMBOL_PATCHES` — same mechanism `loadmod.ko`'s patches already use).

`loadmod.ko` (outside the encrypted volumes): `.text`/`.init.text`/`.rodata`
byte-identical to 3.2.2 — only a 990-byte `.data` blob changed (some constant
table, not code). All 3 `loadmod.ko` patch sites unaffected.

`Eva`'s −22 bytes turned out to matter for the patcher: the 206-byte `.rodata`
code cave the auto-auth injection uses, all-zero on 3.2.1/3.2.2, now contains
22 non-zero bytes on 3.2.3 (a staircase of stray `\r` fragments — leftover
from `SHF_MERGE|STRINGS` section deduplication shifting, not real code).
**Fixed**: confirmed those 22 bytes are genuinely dead by scanning the entire
22.9MB `Eva` binary for any 4-byte absolute pointer landing inside the
206-byte cave range — zero hits, meaning nothing still references that space
(Eva is a fixed-base, non-PIE ET_EXEC, so a live reference has to appear this
way; there's no relocation table left to consult in a linked binary).
`patch_eva()`'s safety check now performs that same scan instead of requiring
literal all-zero bytes, and only refuses if it finds a reference. Verified
end-to-end: a full build against a real 3.2.3 tree produces an `Eva.elf` with
the cave correctly overwritten (`/proc/.oaauth\0` + the auto-auth code) and
the call-site redirect and NOP sled both applied.

No new integrity/anti-tamper/anti-debug checks were found anywhere in this
diff. The one functional addition (`"S043"` auto-authorization) correlates
with `WaveMotion.img` growing +26.3MB and `CSTGEPModel` (the EP-1
Rhodes/Wurlitzer physical model) gaining code in the same release — most
likely a new factory-bundled expansion that ships pre-authorized, not a
response to any patching activity.

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
