# Diffing two Kronos firmware versions

Now that the cryptoloop keys are known (see
[`docs/crypto/cryptoloop_keys.md`](../crypto/cryptoloop_keys.md)),
"what actually changed between two firmware versions?" is a
one-shell-command question.

## Tool

```sh
scripts/diff_kronos_versions.sh  <old_update_dir>  <new_update_dir>  [workdir]
```

Each `<update_dir>` is the unpacked Korg update tree — i.e. a
directory that contains the usual `korg/ro/Mod.img`,
`korg/ro/Eva.img`, `korg/ro/WaveMotion.img` plus the unencrypted
overlay (`sbin/loadmod.ko`, `korg/VersionInfo`, etc.). `workdir`
defaults to `/tmp/kronos_diff` and is reused on repeat invocations.

## What it does, end to end

| Step | Tool used | Output |
|------|-----------|--------|
| 1. Decrypt each `.img`  | `decrypt_kronos_img.py` (AES-256-CBC + plain IV) | Plaintext ext2 images |
| 2. Extract file trees   | `debugfs -R "rdump / <out>" <img>`              | One directory per volume |
| 3. Hash every file      | Python `hashlib.md5` over file contents         | `{path → (size, md5)}` per volume |
| 4. Set-diff manifests   | Python set algebra                              | added / removed / changed lists |
| 5. Overlay diff         | Same logic on the unencrypted overlay tree      | Catches non-image changes too |

The MD5 step is necessary because most binary updates preserve file
*layout* (same ELF structure, same string tables) while changing the
content — size alone often doesn't catch real changes. Hashing
600 MB of decrypted firmware takes <2 seconds on any modern disk.

## Example: the real 3.2.1 → 3.2.2 result

```
================================================================
  Kronos firmware diff:  KRONOS_Update_3_2_1  →  KRONOS_Update_3_2_2/mnt
================================================================

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

Just **four files** of substantive change, out of ~300 in the
update.

## Drilling into a changed binary

Once `diff_kronos_versions.sh` identifies which files changed, the
follow-up technique depends on what you want to learn:

| Question | Technique |
|----------|-----------|
| Where in the file did it change? | Python: `[i for i in range(N) if a[i]!=b[i]]`, report first/last index and a 1 KB-window cluster count |
| What did the change look like? | `xxd` a small window around `first_diff`, or `python3 -c 'print(open(f,"rb").read()[off-32:off+32].hex())'` |
| Did any ELF section change size? | `diff <(readelf -WS old.ko) <(readelf -WS new.ko)` |
| Are there new/removed strings?   | `diff <(strings -n6 old | sort) <(strings -n6 new | sort)` |
| What function changed in a `.ko`? | Ghidra Version Tracking from the annotated old version — see [`ghidra_setup.md`](ghidra_setup.md) |

### Worked example — Eva.img/Eva

`diff_kronos_versions.sh` reports `Eva` as changed (size ±0). Drilling:

```python
a = open("k321/Eva/Eva","rb").read()
b = open("k322/Eva/Eva","rb").read()
diff_idx = [i for i in range(len(a)) if a[i]!=b[i]]
# → 8 differing bytes, all between 0x00e319e1 and 0x00e319f4
```

A 64-byte hex window around the diff:

```
3.2.1  0x00e319e0:  ... 00 'Oct  7 2025' 00 '08:41:24' 00 ...
3.2.2  0x00e319e0:  ... 00 'Apr  9 2026' 00 '08:44:16' 00 ...
```

So Eva's only change between 3.2.1 and 3.2.2 is the embedded
`__DATE__` and `__TIME__` from the rebuild. The Eva binary is
functionally identical.

### Worked example — Mod.img/OA.ko

`diff_kronos_versions.sh` reports OA.ko as changed (+8 bytes overall).
Drilling:

```python
# 2,755,164 differing bytes (19.3% of file)
# First diff @ 0x0034929f (24.1% into file)
# Last  diff @ 0x00d9fab8 (100% into file — runs to EOF)

# ELF section sizes:
#                3.2.1                3.2.2
#   .text       0x592071             0x592081     (+16)
#   .rel.text   0x1e46c8             0x1e46d0     (+8 = +1 reloc)
```

So OA.ko has a real but tiny code change (+16 bytes inserted into
`.text`) that shifts every downstream relocation by 16 bytes, which
is what produces the 2.75 MB of drift through the rest of the file.
The *substantive* change is small — the right tool to identify
exactly which function gained 16 bytes is Ghidra Version Tracking.

### Worked example — sbin/loadmod.ko

`diff_kronos_versions.sh` reports loadmod.ko changed (size ±0).
Drilling:

```
974 differing bytes
First diff @ 0x00006cca
Last  diff @ 0x000070a8
Touched 1 KB windows: 2 of 51   ← very localized
```

Same total size, 974 bytes differ, all confined to a ~1 KB region.
A single targeted code change inside one function. The substantive
content of the 3.2.2 update for the boot-integrity chain probably
lives here.

## Caching

The workdir holds both the decrypted images and the extracted file
trees. Re-running with the same workdir skips both steps:

```sh
scripts/diff_kronos_versions.sh \
    KRONOS_Update_3_2_1 \
    KRONOS_Update_3_2_2/mnt \
    /tmp/diff_321_322

# Later: instant re-run, useful while you're iterating on drill-down analysis
ls /tmp/diff_321_322/
```

The same cache supports any "what changed in <new>?" query — extract
once, diff against any later version.

## Dependencies

- `python3` + `python3-cryptography` (for the decrypt step)
- `debugfs` (Debian/Ubuntu: `apt install e2fsprogs`)

No root, no `cryptsetup`, no Kronos hardware required.

## Related

- [`docs/crypto/cryptoloop_keys.md`](../crypto/cryptoloop_keys.md) — how the keys were recovered
- [`scripts/README.md`](../../scripts/README.md) — usage reference for all the offline crypto tools
- [`docs/workflow/ghidra_setup.md`](ghidra_setup.md) — how to use Ghidra Version Tracking on a changed `.ko`
