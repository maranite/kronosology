# The Universal `P***` Container Format

The Kronos uses a single container layout for all the "primary" preload files —
programs, combis, drum kits, wave sequences, set lists, global settings, and set-list
templates. Every file of these types starts with a fixed 20-byte header followed by an
array of equal-size records.

---

## The 20-byte header

| Offset | Size | Field | Encoding | Notes |
|---|---|---|---|---|
| `0x00` | 4 bytes | **Magic** | ASCII | `PMOS`, `PPCM`, `PCMB`, `PDKT`, `PWSQ`, `PSTL`, `PGLB`, `TMPU`. Always 4 ASCII chars |
| `0x04` | 4 bytes | **Record count** | **big-endian u32** | Number of records following the header |
| `0x08` | 4 bytes | **Record size** | **big-endian u32** | Size in bytes of each record |
| `0x0C` | 2 bytes | **Type / version (high)** | big-endian u16 | Subtype of the file (e.g. `0x0003` for drum-kit format 3, `0x0005` for the current program format) |
| `0x0E` | 2 bytes | **Type / version (low)** or **checksum/seq** | big-endian u16 | Varies per file; appears to be a small monotonically-incrementing counter or a per-bank ID. For PROG: low byte changes from bank to bank; for DKIT: low byte often `0x0040` for user banks (= 64) |
| `0x10` | 4 bytes | **Record-table base offset** or **first record start** | big-endian u32 | Always `0x00000040` followed by the first record's first 4 bytes — i.e. records start immediately at offset `0x14` (= 20). In practice this field reads as the leading bytes of record 0 |

**Total header size: 20 bytes.** File size = `20 + record_count × record_size`. Every
P-magic file in PRELOAD obeys this exactly.

### Worked example — `WSEQB.BIN`

```
Offset  Hex                                           ASCII   Meaning
0x00    50 57 53 51                                   PWSQ    magic
0x04    00 00 00 20                                           record count = 32
0x08    00 00 08 a8                                           record size  = 2216
0x0C    00 01                                                 type   = 1
0x0E    00 6b                                                 subval = 0x6b
0x10    ...                                                   ← record 0 starts here
```

File size: `20 + 32 × 2216 = 70 932 bytes` — matches `ls -l WSEQB.BIN` exactly.

---

## Which files use this format

| Magic | Family | File(s) | Record count | Record size |
|---|---|---|---|---|
| `PGLB` | Global settings | `GLBL.BIN` | 1 | 24 708 |
| `PWSQ` | Wave Sequence | `WSEQA.BIN` | 150 | 2 216 |
| `PWSQ` | Wave Sequence | `WSEQB.BIN`..`WSEQH.BIN`, `WSEQAA.BIN`..`WSEQGG.BIN` (14 files) | 32 | 2 216 |
| `PDKT` | Drum Kit | `DKITA.BIN` | 40 | 38 424 |
| `PDKT` | Drum Kit | `DKITB.BIN`..`DKITH.BIN`, `DKITAA.BIN`..`DKITGG.BIN` (14 files) | 16 | 38 424 |
| `PDKT` | Drum Kit | `DKITI.BIN` (special, smaller record) | 9 | 22 040 |
| `PMOS` | Program — MOSS/EXi | `PROGA.BIN`, `PROGC.BIN`, `PROGD.BIN`, `PROGE.BIN`, `PROGF.BIN`, `PROGG.BIN`, `PROGH.BIN`, `PROGI.BIN`, `PROGJ.BIN`, `PROGK.BIN`, `PROGL.BIN`, `PROGM.BIN`, `PROGN.BIN`, `PROGO.BIN`, `PROGP.BIN` | 128 | 4 960 |
| `PPCM` | Program — HD-1 PCM | `PROGB.BIN`, `PROGAA.BIN`..`PROGGG.BIN` | 128 | 4 960 |
| `PCMB` | Combination | `COMBA.BIN`..`COMBN.BIN` (14 files) | 128 | 7 810 |
| `PSTL` | Set List | `STLS.BIN` | 128 | 69 416 |
| `TMPU` | Set-list Template | `STMPP.BIN` (preset templates) | 18 | 13 080 |

Files **not** using this format (each documented in their own file):

| File | Why |
|---|---|
| `PPAT.BIN`, `DPAT.BIN` | Drum-track patterns — own header format (raw u32 LE counts + name table) |
| `GE.BIN`, `GE.KDF` | KARMA Generated Effect data — Korg KARMA SDK file format |
| `KGEUA.BIN` | User KARMA GE bank — starts directly with first GE's name (no magic) |
| `FXPR.BIN` | Effects preset bank — starts directly with first preset's name |
| `STMPU.BIN` | User set-list templates — starts directly with first template's name |
| `KSCLIST.BIN` | KSC auto-load list — plain ASCII with `\r\n` line endings |
| `FANCTRL.BIN`, `PONMEM.BIN`, `SFCCMAP.BIN`, `FILESORT.BIN` | Tiny config files — just a handful of bytes each |
| `PianoTypes/PianoType<NN>` | Per-piano-type config — own format |

---

## Endian-ness

The header is **big-endian** for the count, size, and type fields — confirmed across
every file family by checking the resulting file size. This is unusual for an x86 Linux
system but consistent with Korg's other data formats (KORG, like Yamaha and Roland,
ships big-endian on-disk formats from the M-series / Wavestation era).

The record body inside each file is typically a mix: parameter blocks tend to be
**little-endian** (the running CPU is x86) but a few fields are big-endian for legacy
reasons. Specific endian-ness per record type is documented in each family's MD file.

---

## Validation observations

The Kronos's loader (`CKorgPreloadFile::Load`) does the following on every preload file:

1. Open the file at `/korg/rw/PRELOAD/<name>.BIN`
2. Read the 20-byte header
3. Validate the magic (rejects file if not the expected 4 chars)
4. Read `record_count × record_size` bytes
5. Optionally byte-swap fields it knows are big-endian
6. Wire records into the in-memory `CSTG…Bank` object

There is **no checksum or CRC** in the standard P-magic header. Korg relies on the
filesystem (ext2 on `/korg/rw`) for integrity, plus per-record byte swaps that would
explode visibly if the data were corrupted.

---

## Why this matters

Knowing the universal header format means:

- You can write a **bulk-rename / bulk-restore tool** that operates on any preload file
  by reading just 20 bytes to discover what it is
- You can **inject extra records** (e.g. more programs in a bank) by patching the
  `record_count` field AND ensuring the corresponding in-memory `CSTG…Bank` accepts the
  higher count (which requires patches to OA.ko — see [extension_points.md](extension_points.md))
- You can **back up and restore individual banks** via byte-perfect file copies — no
  Eva, no SysEx, no PCG file dance
