# The Kronos `.PCG` File Format

A `.PCG` file is the Kronos's **portable snapshot** format — what "Disk → Save PCG"
writes and "Disk → Load PCG" reads. It packages Programs, Combis, Drum Kits, Wave
Sequences, Set Lists and Global settings into one file for backup/transfer over
USB/FTP. `.SNG` (song) files share the same outer container and header but hold an
independent, unrelated payload (§10) — song data does not live inside a `.PCG`, and
sample audio never lives inside either; both only carry *references* to it (§7).

**Status: verified, not yet exhaustive.** Every offset below is either (a) read
directly from Korg's own official MIDI Implementation SysEx-dump documentation
(`/home/share/SysexInfo/MIDI implementation/*.txt` — Korg's own byte tables), (b)
read from the working, real `PCG Tools` reverse-engineered C# reader/writer
(`/home/share/PCG Tools_enigmahack/PCG-Tools/`, which real Kronos users run against
real files), or (c) independently confirmed by hex-dumping real files pulled from a
live Kronos. Each fact is tagged with its source; where sources disagree, both are
shown.

**2026-08 corpus verification (Phase A):** the entire container framework, all record
sizes, the 4802/24-frame offsets, the checksum algorithm (§12), the bank-id encodings
(§2.4), and the sample-Bank-UUID scheme (§7) were re-verified against a corpus of 32
real Kronos-written `.PCG` files (`Z:/PCG EXAMPLES/DC BANDS/BBPB/`, OS 3.x, byte 7 =
`0x02`), with tooling in `Z:/PCG EXAMPLES/DC BANDS/_WORKING/` (`pcg_truth.py`,
`phase_a_sweep.py`, `phase_a_deep.py`, `PHASE_A_REPORT.md`). Highlights: 2405/2405
chunk checksums matched; 75,591 combi-timbre refs valid at offset 4802 vs 110 at
4806; names decode at record offset 0 (not +4); set-list slot params at +24/+25/+26
(not +12); 0 dangling Combi-timbre or SetList-slot references across the corpus. The
previously-flagged "4-byte marker" ambiguity is resolved: there is no such marker —
the DIY editor's reader computes record starts 4 bytes early and compensates with
+4/+4806/+12 offsets; the correct absolute frame is 4802/0/24 as documented here. See
§2.3a note and §5 note.

---

## 1. How this file relates to the other two Kronos "object" representations

The same in-memory Program/Combi/DrumKit/WaveSequence/SetList/Global objects that
OA.ko's engine runs exist on disk/wire in three different serializations (not
necessarily byte-identical body layout in every case — see the callouts below):

| Representation | Where | Framing | Doc |
|---|---|---|---|
| **Internal committed storage** | `/korg/rw/PRELOAD/PROG*.BIN`, `COMB*.BIN`, `DKIT*.BIN`, `WSEQ*.BIN`, `STLS.BIN`, `GLBL.BIN` | Universal 20-byte `P***` header + flat array of fixed-size records, checksummed | [`../preload/container_format.md`](../preload/container_format.md) and siblings |
| **Portable snapshot** | `.PCG` file (this document) | `KORG` header + nested `TAG` + `size` chunks wrapping the *same* per-object records, per-chunk checksummed (§12) | here |
| **Live wire format** | MIDI SysEx Object Dump (func `0x73`) | No chunk framing — just the raw object body, 8-bit-clean or 7-bit-packed depending on transport | `/home/share/SysexInfo/MIDI implementation/*.txt` (Korg's own docs); consumed live by `KronosScreenRemote`'s librarian |

For the structures checked byte-for-byte (Combi timbre table §4, Set List slot
layout §5), the PCG on-disk record body and the live SysEx wire-format body use
**identical field offsets**. `KronosScreenRemote`'s librarian code independently
arrived at offset 4802 (combi timbre table) and offset 24/25/26 (set-list slot
type/bank/index) from live hardware SysEx traffic; the same offsets were confirmed
here from the on-disk `.pcg` reader C# source (`TimbresByteOffset => 4802` in
`KronosSongFileReader.cs`, `TypeOffset/DefaultBankOffset/DefaultPatchOffset` in
`KronosSetListSlot.cs`) and verified against real bytes in a real `.pcg` file. **For
reference-tracking purposes, PCG body layout and SysEx wire-format body layout can
be treated as the same format.** The one confirmed real difference is Program body
layout: PCG/PRELOAD uses a uniform 4960-byte padded slot for both HD-1 and EXi
engines, while the live SysEx dump is 3706 bytes for HD-1 and 4960 for EXi,
unpadded — see §3.1.

The PRELOAD `program_banks.md` doc states: *"Disk → Save PCG: Reads from PRELOAD
into a `.PCG` file... Disk → Load PCG: Inverse — replaces some/all PRELOAD bytes."*
— i.e. a `.PCG` file's per-object records are, as far as anyone has found, literally
the same bytes as the corresponding PRELOAD `.BIN` record, just re-wrapped.

---

## 2. Container framework

**Source**: `KorgKronosTools/Model/Common/File/{KorgFileReader.cs,PcgFileReader.cs}`,
`KorgKronosTools/Model/KronosOasysSpecific/Pcg/KronosOasysPcgFileReader.cs`,
`KorgKronosTools/Model/KronosSpecific/Pcg/KronosPcgFileReader.cs`. Hex-verified
against `PRELOAD.PCG` (30 019 816 bytes, from `SSD1/FACTORY/PRELOAD.PCG`) and
`SHINEONOPEN-PF.PCG` (a small real user file).

### 2.1 File header (offset 0x00–0x0F)

| Offset | Size | Field | Kronos value | Source |
|---|---|---|---|---|
| `0x00` | 4 | Magic | ASCII `"KORG"` | `KorgFileReader.ReadPcgFile` |
| `0x04` | 1 | Model type | `0x68` = Kronos (Trinity=0x3B, TritonKarma=0x5D, TritonLe=0x63, Oasys=0x70, M3=0x75, M50=0x85, MicroStation=0x8D, Krome=0x95, Kross=0x96, Kross2=0xC9, KromeEx=0xD2, Triton-family=0x50+sub-byte) | `KorgFileReader.GetModelType` |
| `0x05` | 1 | File type | `0x00`=PCG, `0x01`=SNG | `KorgFileReader.GetFileType` |
| `0x06` | 1 | *(unaccounted in reader code — always `0x02` in real files seen so far)* | — | hex-verified, meaning not traced in code; all 32 corpus files read `0x02` here |
| `0x07` | 1 | Checksum/OS-generation flag | `0x00`=OS 1.0/1.1 (no checksum chunk yet), `0x01`=OS 2.x, `0x02`=OS 3.x | `KronosPcgFileReader` ctor; all 32 corpus files read `0x02` |
| `0x08` | 1 | Checksum-present flag | `0x00`=no checksum, `0x01`=checksum present (global gate, §12) | hand notes; all 32 corpus files read `0x01` |

Hex-verified: `PRELOAD.PCG` and `SHINEONOPEN-PF.PCG` both start
`4B 4F 52 47 68 00 02 01` → `KORG`, model=Kronos, filetype=PCG, byte6=0x02,
checksum-flag=0x02 (OS 3.x). **2026-08 corpus**: all 32 BBPB files start
`4B 4F 52 47 68 00 02 02 01` — i.e. byte 7 = `0x02` (OS 3.x) AND byte 8 = `0x01`
(checksum-present flag set). The hand-written notes doc
(`PCG Tools_enigmahack/PCG-Tools/Documentation/PCG Structure Kronos.txt`) reads this
same 8-byte span as "68=Product ID, 00=PCG(01=SNG?), 02=Main Version, 01=Minor
Version" — a plausible alternate gloss of the same bytes (their "Main/Minor Version"
may just be this checksum-generation flag plus an unexplored byte), consistent with
but not as precisely sourced as the code-derived reading above. **Note**: the OS
3.x factory file's byte 8 is `0x00` (no checksum flag) while every user-saved BBPB
file has `0x01` — consistent with byte 8 being a per-save "checksums written" flag.

Also hex-verified on all 32 corpus files: a **`PCG1` outer container chunk at file
offset `0x10`** (`50 43 47 31`, size = file size − 0x10 − 4) wraps everything from
`0x1C` onward. PCG-Tools' reader skips it (starts at `DIV1`); the hand-notes doc
lists it as the first chunk. Its payload is the `DIV1` chunk plus the rest of the
file — a container-of-containers, not decoded further.

### 2.2 Chunk framework

Fixed-offset **`DIV1`** chunk at file offset **`0x1C`** (`Div1Offset => 0x1C`,
`KronosOasysPcgFileReader`) kicks off a flat, sequential top-level chunk walk
(`PcgFileReader.ReadContent`): each chunk is `[4-byte ASCII tag][4-byte big-endian
size][4-byte unknown "dwX"][...size bytes of payload]` — i.e. a **12-byte chunk
header**, then payload. The reader advances `Index += chunkSize +
BetweenChunkGapSize` (gap = 12 bytes for Kronos/Oasys) for any tag it doesn't
specifically handle. **The 12-byte header (tag+size+dwX, content at +12) was
independently confirmed on all 32 corpus files** (and matches the Synthify
spreadsheet's `TAG1, size, dwX, Data` description). Nesting is by convention, not a
generic recursive framework — each specific top-level chunk type (`PRG1`, `CMB1`,
`DKT1`, `WSQ1`, `SLS1`, `GLB1`, `DPI1`) has its own hand-written descent logic that
knows its own internal sub-chunk shape (`MBK1`/`PBK1` inside `PRG1`, `CBK1` inside
`CMB1`, `SBK1` inside `STL1` inside `SLS1`, etc.) — `PcgFileReader.ReadChunk`
dispatch table:

```
INI2, INI3   → checksum-era chunks (Kronos OS 1.5/1.6), 12-byte skip + size
CMB1, CMB2   → Combis            (CMB2 = OS 1.5/1.6-only secondary chunk, see §4.4)
PRG1, PRG2   → Programs          (PRG2 = OS 1.5/1.6-only secondary chunk)
SLS1         → Set Lists
STL2         → OS 1.5/1.6-only secondary Set List chunk
WSQ1         → Wave Sequences
DKT1         → Drum Kits
GLB1         → Global settings
DPI1         → Drum Track Patterns (nested DPN1/DPD1/DPS1/DPV1 — not covered here,
               out of scope)
(anything else) → skipped: Index += chunkSize + 12
```

### 2.3 Verified top-level chunk order (real OS 3.x file)

Hex-walked directly against `PRELOAD.PCG` in Python, mirroring the C# reader's own
offset arithmetic exactly:

```
@0x0000001C  DIV1  size=0x2C
@0x00000054  SLS1  size=0x8EA248   (Set Lists)
@0x008EA2A8  PRG1  size=0xA4B198   (Programs)
@0x0133544C  CMB1  size=0x5B8690   (Combis)
@0x018EDAE8  DKT1  size=0x3398A0   (Drum Kits)
@0x01C27394  WSQ1  size=0x73CB8    (Wave Sequences)
@0x01C9B058  GLB1  size=0x6084     (Global)
```

**2026-08 corpus resolution**: the discrepancy is settled — all 32 real OS 3.x
BBPB files show the same top-level order as `PRELOAD.PCG` (`SLS1` at `0x54`, `PRG1`
after it), with the additional `DPI1` chunk at the end (present in all 32):

```
@0x0000001C  DIV1  size=0x2C
@0x00000054  SLS1  size=0x8EA248   (Set Lists)
@0x008EA2A8  PRG1  size=0xC1C1E0   (Programs)
@0x01506494  CMB1  size=0xD58F50   (Combis)
@0x0225F3F0  DKT1  size=0x9ACA28   (Drum Kits)
@0x02C0BE24  WSQ1  size=0x1439D8   (Wave Sequences)
@0x02D4F808  GLB1  size=0x6084     (Global)
@0x02D55898  DPI1  size=0x50A54    (Drum Track Patterns)
```

The hand-notes "PRG1 with SLS1 nested inside" reading is refuted for OS 3.x files.
Note the sizes differ from `PRELOAD.PCG` (user file has more content in each bank) but
the order, and the `SLS1`-contains-`SLD1`-then-`STL1` (which contains `SBK1`)
nesting, are identical. Re-verify against an OS 1.x/2.x sample before relying on
chunk order for anything OS-version-sensitive.

`DIV1`'s own payload (0x2C = 44 bytes) is never decoded by the C# reader — it's
skipped like any unrecognized chunk. The real reader discovers which banks exist
purely by which `CBK1`/`PBK1`/`MBK1`/`DBK1`/`WBK1` sub-chunks it encounters while
descending `PRG1`/`CMB1`/`DKT1`/`WSQ1` — `DIV1` is a redundant table-of-contents the
loader doesn't need. Its payload is decoded below anyway (§2.3a).

### 2.3a `DIV1` payload

For each object type, `DIV1` holds a byte-pair "wordA" immediately followed by a
2-byte count, then a byte-pair "wordB" immediately followed by the same count again:

```
[wordA: u16 BE][count: u16 BE][wordB: u16 BE][count: u16 BE]

wordA/wordB together form a 32-bit, LSB-first bitmask over a flat slot list:
  slot N is present  <=>  bit N of wordA is set (N = 0..15)
                      <=>  bit (N-16) of wordB is set (N = 16..31)

slot list = [internal banks, in natural letter order] ++ [user banks, in natural letter order]
```

| Object | Internal slots (bits) | User slots (bits) | Count field | Confirmed via |
|---|---|---|---|---|
| Program | 0–5 = I-A..I-F | 6–19 = U-A..U-N (14) | 21 (=6+14+1 virtual GM, GM has no slot bit — never stored on disk, see §3.2a) | `PRELOAD` wordA `0xEFFF` (all set except bit12=U-G, matching real `MBK1`/`PBK1` scan exactly), `SHINEONOPEN` wordA `0x0000`/wordB `0x0006` (only bits17,18 = U-L,U-M set, matching its real 2 program banks exactly) |
| Combi | 0–6 = I-A..I-G (7, genuinely more than Program's 6 — matches §2.4) | 7–13 = U-A..U-G (7) | 14 | `PRELOAD` wordA `0x003F` (bits0-5 set, bit6=I-G clear, matching real `CBK1` scan — I-G has no content in this file), `SHINEONOPEN` wordA `0x0800` (bit11 set = slot11 = U-E, matching its one real combi bank `0x20004` exactly) |
| Drum Kit | 0 = Int | 1–14 = U-A..U-N (14) | 15 | `PRELOAD` wordA `0x000F` (bits0-3 set), matching real `DBK1` scan `[Int, U-A, U-B, U-C]` (slots 0,1,2,3) exactly |
| Wave Sequence | 0 = Int | 1–14 = U-A..U-N (14) | 15 | `PRELOAD` wordA `0b1000011` (bits0,1,6 set), matching real `WBK1` scan `[Int, U-A, U-F]` (slots 0,1,6) exactly |

The count field is the total number of possible slots for that object type, not how
many are actually populated — it doesn't change between a nearly-empty file and a
nearly-full one (confirmed: both `PRELOAD` and `SHINEONOPEN` report count=21 for
Program, count=14 for Combi, despite very different bitmask contents).

**Global/SetList/DPI flags** (payload offset `0x28`, i.e. the dword at file offset
`0x4C`–`0x4F`) — 3 of 4 bytes confirmed via real chunk presence/absence: byte0 =
has-`DPI1`-chunk (0 in both real files — neither has drum-track patterns), byte1 =
has-`SLS1`-chunk (1 in `PRELOAD` which has real set lists, 0 in `SHINEONOPEN` which
has none), byte3 = has-`GLB1`-chunk (1 in `PRELOAD`, 0 in `SHINEONOPEN`, matching
real chunk presence exactly both times). Byte2 is unconfirmed — `0x00` in `PRELOAD`
but `0x4C` in `SHINEONOPEN`, not a clean boolean; the hand notes' "reserved" label
for this byte doesn't hold up against real data, and no better explanation is
confirmed (possibly a leftover/uninitialized value from the save routine).

The two leading dwords at payload offset `0x00`–`0x07` (file offset `0x24`–`0x2B`)
are byte-identical (`00 01 00 00 00 00 00 00`) in both real files — plausibly a
fixed chunk-internal version number (`1`) + reserved dword, not per-file data. Not
decoded further; low value.

The container-framework stepping formula is confirmed empirically:
`next_chunk_tag_offset = this_chunk_tag_offset + 12 + declared_payload_length` — every
top-level chunk's true on-disk footprint is an 8-byte header (tag+len) + declared
payload + a 4-byte trailer whose contents were not investigated (both real files show
`FF FF 00 00` immediately after `DIV1`'s own 44-byte payload, before the next chunk's
tag). This formula reproduces the master chunk-order list in §2.3 exactly for all 7
real top-level chunks checked.

### 2.4 Bank identity encoding (`PcgId` / "func33" bank number)

Both Program and Combi banks are addressed on-disk by a **bank ID** distinct from
their PCG in-file array position. `PcgFileReader.ProgramBankId2ProgramIndex` /
`CombiBankId2CombiIndex` decode it:

**Program** (`ProgramBankId2ProgramIndex`, code comment table, `PcgFileReader.cs:446-464`):

| Raw bank ID | Bank | Array index |
|---|---|---|
| `0x00000` | I-A | 0 |
| `0x00001`–`0x00004` | I-B..I-E | 1–4 |
| `0x08000` | I-F | 5 |
| `0x20000` | U-A | 6 |
| `0x20001`–`0x20006` | U-B..U-G | 7–12 |
| (virtual/extended banks, ≥ `ProgramBanks.FirstVirtualBankId`) | U-H.. | 20+ |

**Program has only 6 internal banks (I-A..I-F)**, not 7 — confirmed three ways: (1)
this bank-ID table has no I-G entry, (2) the live SysEx object-dump reference bytes
decoded by `KronosScreenRemote`'s librarian (`ObjBankToFunc33`/`Func33ToObjBank`,
pinned against real hardware Combi-timbre reference bytes), and (3)
`CPcgSaveInfo::setsaveprogbank(b0..b5)` in the real Eva firmware
(`kronosology/reconstructed/Eva/include/pcg_save_info.h`) takes exactly **6** bank
arguments. The 14 writable *user* banks similarly line up:
`setsaveprogbank_exb(b0..b13)` = 14 arguments, matching
`ObjectTypeRegistry.ProgramDescriptor.EditableBanks() =>
Enumerable.Range(0x00,6).Concat(Enumerable.Range(0x40,14))` in `KronosScreenRemote`.

**Combi** (`CombiBankId2CombiIndex`, `PcgFileReader.cs:260-271`): non-virtual IDs
`< 0x20000` map straight through (0–6, **7 internal banks I-A..I-G** — genuinely 7,
unlike Program), virtual-bank IDs (`≥ CombiBanks.FirstVirtualBankId`) map to indices
14+. `CPcgSaveInfo::setsavecombibank(b0..b6)` = 7 arguments confirms 7 internal
banks; `setsavecombibank_exb(b0..b6)` = 7 more, confirming 7 user banks — matching
`CombiDescriptor.EditableBanks() => Range(0x00,7).Concat(Range(0x40,7))`.

**Drum Kit / Wave Sequence** (`WaveSequenceBankId2WaveSequenceIndex`,
`DrumKitBankId2DrumKitIndex`, both identical formula): ID `0` = Int (index 0), ID
`≥ 0x20000` = `id - 0x20000 + 1` (User A=1, B=2, ...). `CPcgSaveInfo` confirms via
`setsavedkitbank(enable)` (1 flag = the single Int bank) +
`setsavedkitbank_exb(b0..b13)` (14 args = 14 user banks) → **15 drum kit banks
total**, and identically for wave sequences (`setsavewseqbank`/`_exb`) → **15 wave
sequence banks total**. Matches PRELOAD's own file inventory exactly: `DKITA.BIN`
(Int, 40 slots) + `DKITB..H.BIN`+`DKITAA..GG.BIN` (14 user files, 16 slots each) +
the odd `DKITI.BIN` (9 slots, smaller record) — see
[`../preload/drum_kit_banks.md`](../preload/drum_kit_banks.md) for the full file
inventory.

---

## 3. Programs

**Source**: `KorgKronosTools/Model/KronosSpecific/Synth/KronosProgram.cs`,
`KronosOasysSpecific/Synth/KronosOasysProgram.cs`, Korg's own
`Prog_HD-1.txt` / `Prog_EXi.txt` / `Prog_EXi_Common.txt` (SysEx dump docs),
hex-verified against `PRELOAD.PCG`.

### 3.1 Size — the one confirmed PCG/wire-format divergence

| Representation | HD-1 size | EXi size |
|---|---|---|
| Live SysEx Object Dump | **3706 bytes** (`Prog_HD-1.txt` header, Object Version 5) | **4960 bytes** (`Prog_EXi.txt` header, Object Version 5) |
| PCG file / PRELOAD `PROG*.BIN` record | **4960 bytes** (both engines — padded to a uniform slot) | **4960 bytes** |

Confirmed on-disk via hex-walk: `PRELOAD.PCG`'s first `MBK1` sub-chunk (EXi bank
I-A) reports `sizeOfAProgram = 4960` directly from the file's own embedded size
field. `PROG*.BIN`'s own container header (`../preload/container_format.md`) states
the same 4960-byte record size for *every* program bank file regardless of
`PMOS`/`PPCM` magic, noting Korg pads both engines' record to the same size, which
is what lets the same loader handle either kind. **2026-08 corpus: all 20 Program
banks in all 32 BBPB files declare `itemsize=4960` — both HD-1 (`PBK1`) and EXi
(`MBK1`) — with count=128 each.** So: **HD-1 programs are NOT padded
on the wire (3706B) but ARE padded on disk (4960B, same as EXi)** — anything that
converts between the two representations (as `KronosScreenRemote`'s
`ProgramFormatConverter.WireBodyFromPcgEntry` does) must account for this, at least
for HD-1.

**2026-08 hardware confirmation (Phase B)**: a real 128-program SysEx Object-Dump
capture of the Kronos's U-FF bank (`HD-1_SysexDumpU-FF.txt`) decodes (via KSR's
8-to-7 codec: `Decode8to7` — every 8 SysEx bytes → 7 binary bytes, first byte
carries the 7 MSBs) to **exactly 3706 bytes per program**, and **127/128 programs
are byte-for-byte identical to the first 3706 bytes of their 4960-byte PCG record**
(verified against `JULY-27_2026(FINAL).PCG` U-FF bank). The one mismatch (index
127, an empty/init program) differs only in the trailing padding. The SysEx header
is `F0 42 30 68 73 [obj] [bank] [index MSB<<7|LSB] [version=5] [8-to-7 body] F7`
with the 7-bit payload starting at byte 10. **KSR's `ProgramFormatConverter`
(wire HD-1 = PCG prefix 3706) is hardware-confirmed.**

### 3.2 Name & category

| Field | Offset | Size | Source |
|---|---|---|---|
| Name | `0x000` | 24 bytes ASCII, **space**-padded | `KronosOasysProgram.MaxNameLength => 24`; hex-verified `PRELOAD.PCG` program 0 = `"Berlin Grand SW2 U.C.   "` (trailing spaces); **2026-08 corpus: all 20 banks' records decode names at +0** |
| Category | `+2568` | 4 bits | `KronosProgram.GetParam(Category)` — `IntParameter(...,2568,4,0,...)` |
| Sub-category | `+2568` | bits 7–5 (3 bits, same byte) | `KronosProgram.GetParam(SubCategory)` — packed in the same byte as Category |
| Favorite | `+2558` | bit 5 | `KronosProgram.GetParam(Favorite)` |
| Osc Mode | `+2558` | 2 bits | `Single/Double/Drums/-(EXi)/-(Unused)/Double Drums` enum, `KronosProgram.GetParam(OscMode)` |

**Discrepancy, unresolved**: `kronosology/docs/preload/program_banks.md` (derived
from OA.ko disassembly, explicitly marked as not field-mapped byte-by-byte) puts
Category/sub-category at PRELOAD record offset `0x018` (=24 decimal, right after the
name). That does not match the PCG-Tools-verified, empirically-working offset 2568
above. Trust 2568 — it's what a real, working PCG editor actually reads/writes —
over the PRELOAD doc's own explicitly-marked guess. (2026-08 corpus: program I-A:000
category byte at +2568 reads a valid in-range value; no corpus evidence for 0x018.)

### 3.2a GM Program bank

`KronosGmProgram.cs`/`KronosGmProgramBank.cs`: the GM bank is not a distinct on-disk
record format — it's an ordinary Program bank (same 4960-byte record, same field
layout as above) whose patches are forced to synthesis type HD-1
(`DefaultSampledSynthesisType => Hd1`, `DefaultModeledSynthesisType` throws
`NotSupportedException` — GM patches can't be MOSS/modeled) and display-named
`"GM" + (index+1)` (`IndexOffset => 1`, so array index 0 shows as **GM1**, not GM0).
No new structural knowledge beyond §3.2/§3.3 — noted separately only because it's a
distinct bank identity in the `ProgramBankId2ProgramIndex` table (§2.4) and a
distinct `ObjectTypeRegistry`/librarian bank category (§9).

### 3.3 Oscillator / multisample (sample) reference — where "which sample this program plays" lives

This is the field the librarian's sample-reference tracking needs.
`KronosProgram.GetZoneMsByteOffset(osc, zone) = ByteOffset + 2774 + osc*466 + zone*22`
(8 zones per oscillator, up to 2 oscillators depending on Osc Mode), each zone
holding a 1-byte mode selector (`Off`/`Sample`/`WaveSequence`) at its own offset
`+0`, then:

- **If mode = Wave Sequence**: Korg's own doc (`Prog_HD-1.txt`) confirms an
  **"MS Bank UUID"** field starting at zone-offset `+1` (e.g. OSC1 Zone1 at absolute
  offset **2775**, OSC2 Zone1 at **3241** — `3241-2775 = 466`, matching
  `osc*(3240-2774)` in the C# code exactly, off-by-one because the code's zone base
  includes the 1-byte mode field the doc's field table starts *after*), followed by
  a numeric multisample/wave-sequence index.
- **If mode = Sample** (`KronosOasysProgram`, line ~113/138): index at zone-offset
  `+2`, 2 bytes, "bank unused, always 0" per the code comment — i.e. a direct
  numeric multisample index, no separate bank selector at the PCG-body level for
  this case (see §7 for what actually resolves a bare index into real sample data).

Both cases ultimately resolve through the same **Bank UUID + numeric ID** reference
mechanism documented authoritatively for Drum Kits and Wave Sequences in §6/§7 below
— Korg's SysEx docs mark the "MS Bank UUID" field explicitly as *"use binary param
change (function 0x44)"*, meaning it's too large/opaque to show as a simple hex
range in their own tables (consistent with a 16-byte UUID, matching `CKorgKsc`'s own
64-byte `mUUID` field format family — see §7).

---

## 4. Combis & Timbres

**Source**: Korg's own `CombiAndSongTimbreSet.txt` (authoritative — object size and
every offset below is read directly from Korg's own table), cross-checked against
`KorgKronosTools/Model/KronosSpecific/Synth/{KronosCombi.cs,KronosTimbre.cs}` and
`Model/KronosSpecific/Song/KronosSongFileReader.cs`, hex-verified against real combi
bodies in `PRELOAD.PCG`.

### 4.1 Size & name

**Combi body = 7810 bytes** (Korg doc header: `"Combination Size: 7810 byte"`,
Object Version 3). Hex-verified: `PRELOAD.PCG`'s first `CBK1` reports
`sizeOfACombi = 7810` directly. Name at offset `0`, 24 bytes ASCII — but **null**-padded
on disk, not space-padded like Program names (hex-verified: combi 0 =
`"K-Lab: Katja's House\x00\x00\x00\x00"`).

### 4.2 Timbre table — the reference structure a future librarian needs

Byte-identical across all three representations (PCG on-disk, live SysEx wire
format, Korg's own docs):

| Field | Offset (Timbre 1 / index 0) | Size | Source |
|---|---|---|---|
| Program Number | **4802** | 1 byte, 0x00–0x7F | Korg doc; `KronosSongFileReader.TimbresByteOffset => 4802`; `KronosScreenRemote`'s `LibRefs.Timbre0Num` |
| Bank Number | **4803** | 1 byte, 0x00–0x1E (INT-A..U-GG) | Korg doc; `KronosTimbre.UsedProgramBankId` (`TimbresOffset + 1`); `KronosScreenRemote`'s `LibRefs.Timbre0Bank` |
| MIDI Channel | 4804 | bits 4–0 | Korg doc |
| Status (Off/Int/Ext/EX2) | 4804 | bits 7–5 | Korg doc: `00~04` = `Off~External2` |
| **Timbre stride** | — | **188 bytes** | Korg doc (offset deltas between successive `TimbreN` blocks); `KronosTimbre.TimbresSizeConstant => 188`; `KronosScreenRemote`'s `LibRefs.TimbreStride` |
| Timbre count | — | 16 | Korg doc field indices (`4|0|0`..) run 0–15 across the Combi |

Hex-verified against `PRELOAD.PCG` combi 0 ("K-Lab: Katja's House"): timbre
0/1/2/3 at `combi.ByteOffset+4802+t*188` read as (number=65,bank=22),
(number=120,bank=22), (number=112,bank=22), (number=9,bank=24) — all in-range
values, bank IDs plausible U-bank indices. The `Status` byte-value pattern found in
the older hand notes doc ("0x20=off, 0x21=Int, 0x62=Ext, 0x83=EX2") is consistent
with Korg's own bit layout: `(byte & 0xE0) >> 5` giving the 3-bit status enum in
bits 7-5 (their low nibble was miscounting the adjoining MIDI Channel field, but the
shift/mask matches).

### 4.3 Category / Favorite

`KronosCombi.GetParam`: Category at `+4790` (4 bits), Sub-Category at `+4790`
bits 7–5, Favorite at `+4791` bit 0 — immediately before the timbre table starts
at 4802, consistent with "combi-common-header, then 16×188-byte timbre array"
being the combi body's overall shape.

### 4.4 OS 1.5/1.6 legacy secondary chunk (`CMB2`/`CBK2`)

Only relevant for files saved under Kronos OS 1.5/1.6 specifically: a *second*,
separate `CMB2`/`CBK2` top-level chunk stores an alternate/extended program
reference per timbre (`KronosTimbre.GetProgramOffset`/`UsedProgramBankId`
branch on `PcgRoot.Model.OsVersion == EOsVersionKronos15_16`), used when a timbre
points at a "UserExtended" program bank the original 1-byte/7-bit `CMB1` bank field
couldn't address (sentinel value `127` written into `CMB1`, real bank/index
written into the parallel `CBK2` table instead). Not relevant to OS 2.x/3.x files.

---

## 5. Set Lists

**Source**: Korg's own `SetList.txt` (complete, authoritative — every offset below
is Korg's own table), cross-checked against
`KorgKronosTools/Model/KronosSpecific/Synth/KronosSetListSlot.cs`.

**Set List object = 69416 bytes** (Korg doc header, Object Version 0) — this is the
*entire* set-list array (all 128 set lists as ONE object on the wire/in-PCG, not one
object per set list — confirmed by `PcgFileReader.ReadSetList`, which computes
`sizeOfASetListSlot = chunkSize / numberOfSetLists` from a single `SBK1`
sub-chunk). One Set List = 24-byte name + 128 slots.

**2026-08 corpus confirmation**: on all 32 BBPB files, `SBK1` declares `count=128,
itemsize=69416` (per-slot 542), and the set-list name + slot names + params all
decode at the offsets below. The type histogram across set list 0 of the FINAL file
was 118 Program / 9 Combi / 1 Song — the first real **Song-type slot (type=2)**
observed anywhere in this ecosystem.

| Field | Offset (relative to slot start) | Size | Source |
|---|---|---|---|
| Slot Name | `0` | 24 bytes ASCII | Korg doc; `KronosSetListSlot.MaxNameLength => 24` |
| Performance Type (0=Combi,1=Program,2=Song) | `24` | bits 1–0 | Korg doc; `KronosSetListSlot.TypeOffset => ByteOffset+24` |
| Color | `24` | bits 5–2 | Korg doc |
| Font (LSB 2 bits) | `24` | bits 7–6 | Korg doc |
| Performance Bank (0x00–0x1E, INT-A..U-GG) | `25` | bits 4–0 | Korg doc; `KronosSetListSlot.DefaultBankOffset => ByteOffset+25` |
| Transpose (MSB 3 bits) | `25` | bits 7–5 | Korg doc |
| Performance Index | `26` | full byte, 0–199 | Korg doc; `KronosSetListSlot.DefaultPatchOffset => ByteOffset+26` |
| Hold Time | `27` | 1 byte, 0–60s | Korg doc |
| Volume | `28` | 1 byte | Korg doc; matches `KronosSetListSlot.cs`'s own inline comment `"28 = volume"` |
| Keyboard Track | `29` | bits 3–0 | Korg doc |
| Font MSB | `29` | bit 4 | Korg doc |
| Transpose (LSB 3 bits) | `29` | bits 7–5 | Korg doc |
| Comments/Description | `30` | 512 bytes ASCII | Korg doc; `KronosSetListSlot.MaxDescriptionLength => 512` |

**Slot stride = 542 bytes** (Korg doc: slot 1 starts at absolute offset 566, slot 0
at 24 → `566-24=542`; `KronosScreenRemote`'s independently-derived live-wire
`SlStride` constant agrees exactly). 128 slots × 542 = 69376, + 24-byte set-list
name = 69400, + a trailing per-set-list-EQ/control-surface footer (offsets
69400–69415, 16 bytes) not modeled by `KronosSetListSlot.cs` at all (PCG-Tools
doesn't expose per-set-list EQ editing) = **69416 total** — this figure is for
**one Set List** (name+128 slots+footer); the file-level object (`ReadSetList` in
`PcgFileReader.cs`) holds *all 128* set lists back-to-back inside one `SBK1`/`STL1`
payload. Note a terminology collision: "Set List slot" (one of 128
performance-reference rows inside a set list) vs. the reader's own `SetListSlot`
variable naming for a whole Set List — read the code, not the variable names, when
working in this area.

**Slot bank encoding (2026-08, corpus-verified)**: the `Performance Bank` byte at
+25 is the **func33 / live-wire bank code**, not a PCG file index — `0..5` = INT-A..F,
`6` = GM, `7..16` = g(1..d), `17..30` = USER-A..GG (USER-A=17 → file index 6, i.e.
`func33 = file_index + 11` for USER banks). Every slot reference in the corpus
resolves to a real Program/Combi bank with this mapping (0 dangling).

Note (from Korg's own doc footer): *"the param value of sys/ex param PID 18 (set
list slot performance) is packed like this: `type << 16 | bank << 8 | index`"* —
useful when working with the live parameter-change protocol instead of the object
dump.

---

## 6. Drum Kits — the sample-reference-heavy structure

**Source**: Korg's own `DrumKit.txt` (complete, authoritative). PCG-Tools' own C#
model (`KronosDrumKit.cs`, `KronosOasysDrumKit.cs`, base `DrumKit.cs`) does **not**
decode this structure at all — `DrumKit.ChangeReferences` throws
`NotImplementedException`, and the only fields exposed are Name (offset 0, 24 bytes)
and an empty/init-name heuristic. This is a genuine gap in the tool real users run
for Kronos patch management: it has never reverse-engineered per-key drum sample
assignments. Korg's own SysEx documentation fills that gap completely below.

**Drum Kit object = 38424 bytes** (Object Version 3). Structure: 24-byte kit name,
then **128 Notes** (one per MIDI key) × **300 bytes each** (`128×300 + 24 =
38424` — exact match). Each Note holds **8 Zones** (velocity/round-robin layers,
`DS` = "Drum Sample") of 34 bytes each (`8×34=272`), 7 inter-zone
crossfade-control blocks of 3 bytes each (21 bytes, for zone-pairs 1-2..7-8), and
a 7-byte "Note Common" trailer — `272+21+7=300`, matching.

**2026-08 corpus confirmation**: `DBK1` banks on all 32 BBPB files declare
`itemsize=38424` (Int bank count=40, each of 14 user banks count=16). First-record
probe of user kit `JazzAmbi Kit Dry` (U-A): 366/1024 zones populated (`Sample
On/Off`=1), every populated zone carrying the legacy `KORG\x00×8\x00MS\x00{nn}`
bank UUID with `nn=0` (ROM), and sample ids 1..1211 (tight sequential clusters) —
exactly the pattern §7 predicts. Note 0's zone 0 (`on=1`, uuid `KORG...MS\x00\x14`,
sample id 0) is the factory-typical "silent kick" placeholder.

### Per-zone layout (34 bytes, relative to zone start)

| Offset | Field | Notes |
|---|---|---|
| `0` | Sample On/Off | 1 byte |
| `1`–`~15` | **Sample Bank UUID** | *"use binary param change (function 0x54)"* — opaque in the simple table, see §7 |
| `18` | **Sample Id** | 2 bytes, `0000~FFFE` = 0–65534 — the actual sample reference: which multisample inside the bank named by the UUID |
| `20` | Level | -99..+99 |
| `21` | Start Offset (4 bits) / Sample Reverse (bit 7) | |
| `22` | Transpose | -64..+63 |
| `23` | Tune | -99..+99 |
| `24` | Attack | -64..+63 |
| `25` | Decay | -64..+63 |
| `26` | Cutoff | -64..+63 |
| `27` | Resonance | -64..+63 |
| `28` | EQ High Gain | -72..+72 dB |
| `29` | EQ Mid Gain | -72..+72 dB |
| `30` | EQ Low Gain | -72..+72 dB |
| `31` | Drive | -99..+99 |
| `32` | Low Boost | -99..+99 |

(Offsets above are relative to each zone's own start per Korg's table; absolute
file offsets for Note 1 Zone 1 start at 24 — mechanically identical, just `+300` per
note and `+34` per zone.)

### Note Common trailer (7 bytes, after the 8th zone + crossfade blocks)

Pan, Bus Select (5 bits) + FX Control Bus (2 bits) packed in one byte, Send 1/2
Level, Exclusive Group, and a bitfield byte (Assign / Single Trigger / Receive
Note On / Receive Note Off).

**The reference mechanism**: a Drum Kit does not embed sample audio or even a
simple linear sample index — each of up to 1024 zones (128 keys × 8 layers)
independently references a sample via **(Sample Bank UUID, Sample Id)**, the same
two-part addressing scheme used by Wave Sequence steps (§7) and Program oscillator
zones (§3.3). See §7 for what that UUID identifies and how it resolves to real
audio.

---

## 7. Wave Sequences

**Source**: Korg's own `WaveSequence.txt` (complete, authoritative).

**Wave Sequence object = 2216 bytes** (Object Version 1). 24-byte name, 16-byte
Common block (offsets 24–39: Run Sequence / Time-Tempo Mode / Note-On Advance /
Swing Resolution / Start-End Step / Start-Step AMS Source+Intensity /
Position-AMS Source+Intensity / Duration-AMS Source+Intensity / Loop
Start/End/Direction/Repeat), then **64 Steps** of 34 bytes each (`64×34=2176`,
`+40=2216`, matching exactly).

### Per-step layout (34 bytes, relative to step start)

| Offset | Field | Notes |
|---|---|---|
| `0` | Step Type (bits 1–0) | `00`=Multisample, `01`=Rest, `02`=Tie |
| `1`–`~15` | **Bank Select UUID** | *"use binary param change (function 0x56)"* — same opaque-UUID pattern as Drum Kit |
| `18` | **Multisample Select** | 2 bytes, `0000~3FFE` = 0–16382 — the actual sample/multisample reference |
| `20` | Level | 0–127 |
| `21` | Start Offset / Reverse | |
| `22` | Tune | -1200..+1200 cents |
| `24` | Transpose | -24..+24 |
| `25` | AMS1 Output | |
| `26` | AMS2 Output | |
| `27` | Duration | 0–146 |
| `29` | Tempo Base Note | |
| `30` | Tempo Multiplier | x1–x32 |
| `31` | Crossfade | |
| `32` | Fade-In Shape | -128..+127 |
| `33` | Fade-Out Shape | -128..+127 |

Same **(Bank UUID, numeric Select/Id)** reference pattern as Drum Kit zones and
Program oscillator zones (§3.3, §6). This is a Kronos-wide convention, not
per-object-type — worth designing the future librarian's reference model around
that single shared pattern rather than three separate ones.

**2026-08 corpus confirmation**: `WBK1` banks on all 32 BBPB files declare
`itemsize=2216` (Int bank count=150, each of 14 user banks count=32). First-record
probe of `19 Orch/Band HITS` (Int): all 64 steps type 0 (Multisample), all carrying
the legacy `KORG\x00×8\x00MS\x00{nn}` bank UUID, multisample select values 1182..
(LE, tight sequential cluster).

### What the UUID actually is

Korg's own `KRONOS_MIDI_SysEx.txt` documents the Bank UUID format in footnote `*10`
("Multisample Bank UUIDs", lines 1245–1355). Combined with direct hex-decoding of
real `PRELOAD.PCG` bytes and 10+ real `/korg/rw/Options/Sxxx` files pulled live from
the Kronos, every part of the format is cross-confirmed three independent ways (doc
text, real on-disk bytes, real option-file content) with no contradictions:

**1. It's a standard 128-bit UUID**, stored as 16 raw bytes, with one special bit: byte
15 bit 0 is a mono(0)/stereo(1) flag, not part of the bank's identity — so a bank's mono
and stereo members share the same UUID except for that one bit.

**2. "Legacy" banks (ROM, old-RAM, and factory EXs 1–126) use a fixed, derivable
pseudo-UUID**, per Korg's own C array: `{ 'K','O','R','G', 0,0, 0,0, 0,0, 0,0, 'M','S',
0, nn }` = bytes `4B 4F 52 47 00 00 00 00 00 00 00 00 4D 53 00 nn` (`"KORG"` +
8 zero bytes + `"MS"` + 1 zero byte + the legacy ID byte `nn`). `nn = (legacy_bank_number
<< 1) | stereo_flag`, where `legacy_bank_number` is 0=ROM, 1="Smp: Old RAM", 2=EXs1,
3=EXs2, ... i.e. **`legacy_bank_number = EXs_number + 1`** for any factory EXs bank
numbered 1–126.

  **Hex-verified**: scanning the `DKT1` (Drum Kit) chunk of `PRELOAD.PCG` for this exact
  16-byte pattern found **90,017 occurrences** with a clean, small histogram of `nn`
  values (0, 1, 4, 5, 12, 13, 18, 19, 20, 21, ...) — decoding via the formula above gives
  ROM mono/stereo (`nn`=0/1) and EXs1/EXs5/EXs8/EXs9 mono/stereo, all plausible factory
  content for drum samples. Populated zones (`Sample On/Off`=1, non-zero Sample Id) show
  tightly clustered, sequential-looking Sample Id values within each bank — exactly what
  real consecutive multisample indices in a factory PCM bank should look like. The
  identical pattern also appears in the `PRG1` (Program) chunk's oscillator zones (21
  distinct `nn` values found), confirming this is the same field/convention Korg's docs
  describe for Program MS Bank (func `0x44`), Drum Kit Sample Bank (func `0x54`), and
  Wave Seq Bank Select (func `0x56`) — one shared scheme, not three different ones.

  **Cross-checked against real EXs option files**: `/korg/rw/Options/S016` through
  `S047` pulled live from the Kronos — every populated file's line-4 `<id>` field
  equals its own EXs number **+1**, with zero exceptions (EXs16→id 17, EXs17→id 18,
  EXs18→id 19, EXs22→id 23, EXs23→id 24, EXs25→id 26, EXs40→id 41, EXs41→id 42,
  EXs42→id 43, EXs47→id 48). This confirms the option file's `<id>` field **is**
  `legacy_bank_number` directly — so for any factory EXs bank, its Bank-UUID `nn` byte is
  computable from its option file alone: `nn = (<id> << 1) | stereo_flag`.

**3. EXs127+ and all 3rd-party/user content use a real generated UUID, stored
verbatim** — no `KORG`/`MS` prefix, no derivation, just the raw 16 bytes (byte 15 bit 0
still the mono/stereo flag). Hex-verified two ways: (a) `/korg/rw/Options/S191`
and `S206` (both real 3rd-party packs — Soundiron, A2D) have `<id>` values
`uuid:5124fc23-145e-4fe4-af12-308e1c17b5b4` and `uuid:8e7ab882-4abf-4317-b095-874bc9627802`
respectively, confirming Korg's doc claim that ids ≥127 switch to the `uuid:` form; (b) the
second of those UUIDs was found byte-for-byte, unmodified, live inside `PRELOAD.PCG`
at file offset `0x1266deb` — `8e 7a b8 82 4a bf 43 17 b0 95 87 4b c9 62 78 02`, an exact
match to the option file's UUID string with no prefix, no transformation, last byte's bit
0 already 0 (mono) so stored as-is.

**4. Sample Id / Multisample Select (the 2-byte field right after the Bank UUID) is
little-endian.** Not explicitly stated for this specific field in Korg's docs, but
(a) empirically the real `PRELOAD.PCG` values only make sense as sequential factory
multisample indices when read little-endian (e.g. bytes `ee 01`/`ef 01`/`e0 01` decode to
494/495/480 — a tight, plausible cluster — vs. nonsensical 61185/61441/57345 read
big-endian), and (b) Korg's own doc explicitly flags the one 4-byte field elsewhere in
these same object-dump tables that *is* big-endian (`Prog_HD-1.txt`, KARMA Start Seed:
`"* Big Endian"`) as a called-out exception — implying unmarked multi-byte fields,
including this one, default to little-endian.

**Formula for a librarian/editor**:
```
given an EXs option file's line-4 id field:
  if id is numeric (< 127):   bank_uuid = KORG_MS_PREFIX + bytes([(id << 1) | stereo])
  if id is "uuid:<uuid>":     bank_uuid = raw 16 bytes of <uuid>, with byte[15] bit0 set/cleared for stereo/mono
KORG_MS_PREFIX = 4B 4F 52 47 00 00 00 00 00 00 00 00 4D 53 00   (15 bytes)
```
This is solved well enough to write a working (Bank UUID) → (EXs bank / option file)
resolver without further reverse engineering. Not yet independently confirmed for
`.KSC` user-collection UUIDs specifically — see Open Questions — and the *live wire*
(binary param-change, func `0x44`/`0x54`/`0x56`) byte layout was not directly
captured, only inferred from this document's established PCG-body/wire-body parity
pattern (§1, §4.2, §5).

User-created sample collections use the parallel `.KSC` file format (`CKorgKsc`,
`korg_ksc.h`), which carries its own 64-byte `mUUID` field (`GetUUID`/`SetUUID`,
`.text+0x089ce100`/`0x089ce150`, ctor-verified offsets `0x250`/`0x251` for two
adjacent flag bytes). `.KMP` (`korg_kmp.h`, multisample-map: which samples cover
which key/velocity zones) and `.KSF` (`korg_ksf.h`, single raw sample + minimal
header) round out the on-disk sample family — fully mapped in
[`ksc_kmp_ksf_file_format.md`](ksc_kmp_ksf_file_format.md).

**In short**: `.PCG` files never embed sample audio. Every sample reference is a
two-part **(Bank UUID, numeric ID)** pointer that resolves either into a factory
EXs PCM bank (identity tied to that EXs's option file) or a user `.KSC` sample
collection (identity = its own embedded UUID) — both stored *outside* the `.pcg`,
on the Kronos's own filesystem or on removable media. **A `.pcg` file is therefore
only self-contained for Program/Combi/DrumKit/WaveSequence/SetList/Global
*structure and cross-references between those six things* — never for the sample
audio those structures point at.** Moving a `.pcg` to a machine without the
matching EXs/KSC content installed will load fine but produce silent/wrong-sample
playback; a future librarian that "moves sound data safely" must treat
(Bank UUID, ID) pairs as first-class external references, exactly like it already
must treat Combi-timbre→Program and SetList-slot→Program/Combi references (§9).

---

## 8. Global settings

**Source**: Korg's own `Global.txt` (25414 lines — only spot-checked, not read in
full; treat this section as a starting point, not exhaustive), cross-checked
against `KronosGlobal.cs` (thin — most Global logic lives in the shared/base
`Program.cs`'s `CategoryAsName`/`SubCategoryAsName`, which delegates to
`global.GetCategoryName(this)`/`GetSubCategoryName(this)`).

- **Global object = 24708 bytes** (Object Version 1, Korg doc header) — matches
  PRELOAD's own `GLBL.BIN` record size (`../preload/container_format.md`:
  `PGLB` magic, 1 record of 24708 bytes) exactly, confirming Global really is a
  **singleton** (bank 0, index 0 — not an array of banks like every other object
  type), consistent with `LibObj.Global` in `KronosScreenRemote` being explicitly
  excluded from the Librarian's move/reference-tracking model as "the instrument's
  single Global settings object... never catalogued, moved, placed or pushed."
- **Program Category name table** confirmed at absolute offset **12912**, 24-byte
  ASCII names, one entry per category (`"Program Category 00"`, `"Program Category
  01"`, ... stride 24 bytes — not yet counted how many total categories exist;
  the Kronos UI has a fixed category list, so this is almost certainly a small,
  bounded table). A parallel Sub-Category name table almost certainly exists
  nearby, not yet located precisely. **2026-08 corpus: all four tables confirmed
  on every BBPB file — Program category @12912, Program sub-category @13344,
  Combi category @16800, Combi sub-category @17232, all 24-byte ASCII (read
  `Keyboard`, `A.Piano`, ... on the FINAL file). `KronosScreenRemote`'s
  `GlobalBody.cs` uses exactly these offsets (12912/13344/16800/17232), read off
  Korg's own `Global.txt` offset column.** Note: the GLB1 record has NO name at
  offset +0 (bytes start `00 00 08 02...`) — Global's record base is not
  name-first like the other object types; the exact payload base (payload+0 vs
  +12 vs +16) is the one frame not yet pinned (deferred to Phase B).
- Everything else in this 24708-byte object (MIDI routing, global tuning, KARMA
  global settings, per-song-mode defaults, control-surface assignment, etc.) is
  not yet catalogued — Korg's own doc has the full byte table if a specific Global
  field is ever needed; grep it directly rather than re-deriving.

---

## 9. Existing reference-tracking prior art (for the future librarian)

`KronosScreenRemote`'s existing Librarian (`Core/LibrarianModel.cs` and
`Core/LocalLibrary/*`) is **live-MIDI-transport-based**, not `.pcg`-file-based — it
mirrors instrument state to a local cache and pushes/pulls over SysEx object-dump
func `0x73`/Store func `0x76`, not by reading/writing `.pcg` files as its native
storage. It does, however, read `.pcg` files as an **import source**
(`DependencyScanner.RepointPcgReferences`, via a `PcgLibraryView` +
`ProgramFormatConverter.WireBodyFromPcgEntry` bridge — worth reading directly, likely
the richest additional source of empirically-verified PCG↔wire body layout knowledge
in this ecosystem, not yet mined here).

What's directly reusable for a future **offline, `.pcg`-native** librarian:

- **Reference model**: `ObjLoc(ObjType, Bank, Number)` addresses; a `ReferrerSite`
  record for each reference *site* (e.g. "Combi X timbre 3 → Program Y"). Combi
  timbres and Set List slots are the only referrer types currently modeled — Song
  Timbre Sets are explicitly out of scope there, and no sample/multisample/EXi
  reference is walked at all (confirmed: `ObjectReferenceWalker.Walk` only decodes
  Combi-timbre→Program and SetList-slot→Program/Combi). §3.3/§6/§7's Bank-UUID+ID
  sample references have no prior art anywhere in this codebase ecosystem — a
  future librarian's sample-reference tracking must be designed fresh from this
  document, not ported from the existing librarian.
- **Coherent-move pattern** (`Librarian.PlanMove`): catalog every referrer of both
  a move's source and destination first, patch all of them as part of one atomic
  write plan with pre-images kept for rollback, refuse the whole move rather than
  leave a dangling reference. This is the right *shape* for a future
  cross-reference-aware `.pcg` editor, even though the current implementation is
  wired to live SysEx writes, not file bytes.
- **Bank encoding**: the "func33"↔"object bank" translation layer
  (`KronosBanks.Func33ToObjBank`/`ObjBankToFunc33`) already correctly encodes the
  6-internal-bank Program / 7-internal-bank Combi asymmetry confirmed independently
  in §2.4 above — reuse it rather than re-deriving.

---

## 10. SNG files (brief)

`.SNG` files share the outer `KORG` header + chunk framework (file type byte = 1
instead of 0) but hold Song data — sequencer tracks, MIDI/audio/automation events,
patterns — independent of `.PCG` content; out of scope for this document. One
overlap worth knowing: a Song's mixer/timbre set (`CTimbreSetSong`) is derived from
`CCombi` in Korg's own internal naming (per the hand notes doc's SNG structure
notes) and `KronosSongFileReader.TimbresByteOffset => 4802` — the exact same offset
as a Combi's timbre table (§4.2) — confirming a Song's timbre set reuses the Combi
body layout wholesale.

---

## 11. Open questions

- ~~Chase the `PRELOAD.PCG` chunk-order discrepancy (§2.3) against an OS 1.x/2.x
  `.pcg` sample~~ — **RESOLVED for OS 3.x** by the 32-file corpus (§2.3): the
  hand-notes "SLS1-nested-in-PRG1" reading is refuted; `SLS1` is top-level. Still
  open only for OS 1.x/2.x files.
- Field-map Program's remaining ~2500 bytes of common/effects parameters (§3) and
  Global's remaining ~24000 bytes (§8) from Korg's own docs on demand, rather than
  up front — grep `Prog_HD-1.txt`/`Prog_EXi.txt`/`Global.txt` directly for whatever
  specific field is needed next; don't re-derive what's already sitting in those
  files.
- Whether `.KSC`'s `#`-comment lines are functionally parsed by the Kronos or purely
  informational beyond what [`ksc_kmp_ksf_file_format.md`](ksc_kmp_ksf_file_format.md)
  already establishes, and whether the Bank-UUID formula above (§7) matches `.KSC`
  UUIDs byte-for-byte (not yet cross-checked).
- Read `ProgramFormatConverter.WireBodyFromPcgEntry` in `KronosScreenRemote` directly
  (§9) — likely resolves the HD-1 3706↔4960 padding question (§3.1) exactly, and may
  already document more PCG body fields than captured here.
- Byte2 of the `DIV1` Global/SetList/DPI flags dword (§2.3a) is not a clean boolean
  in the two sample files checked; meaning unconfirmed. **2026-08 corpus: all 32
  BBPB files read byte2 = `0x00` (DIV1 flag dword `01 01 00 01`), matching
  `PRELOAD.PCG` — the `0x4C` value seen once in `SHINEONOPEN-PF.PCG` remains a
  lone anomaly.** **RESOLVED (Phase B, re-save diff 2026-08):** the Kronos's own
  re-save of an edited file changed DIV1 byte2 from `0x00` to `0xB6` while
  expanding the DPI1 drum-pattern count 232→411. Byte2 is therefore **DPI state the
  Kronos recomputes on save** (not a boolean flag) — likely a DPI1 sub-chunk size or
  pattern-count fragment. The trailing `0x50-51` also changed `ff 00` → `00 01` on
  re-save, so those two bytes are likewise Kronos-written state, not a static trailer.
- **Phase B (hardware) — checksum validation CONFIRMED 2026-08**: on the real
  Kronos (OS 3.x), a stale per-chunk checksum causes a **per-bank skip** for bank
  chunks (`Bank I-A = "File unavailable"` — the rest of the file loads) but a
  **fatal abort** for the `GLB1` chunk (load fails at "Now writing into internal
  memory"). See §12. Also resolved (Phase B): (b) **re-save is NOT byte-identical**
  — the Kronos preserves every object chunk (Program/Combi/DrumKit/WaveSeq/SetList/
  Global payloads byte-identical, my GLB1 edit + checksum survived) but **expands
  the DPI1 chunk** (drum patterns 232→411, DPN1/DPD1/DPS1 all grow) and updates the
  `PCG1`/`DIV1` size fields; (c) **HD-1 SysEx dump = exactly the first 3706 bytes
  of the 4960-byte PCG record** — confirmed byte-for-byte on 127/128 programs of a
  real U-FF bank dump (§3.1); (d) GLB1 record base still open.
  vs +16)?

---

## 12. Checksums — required for any externally-edited `.PCG` to load again

**This is the load-bearing fact for making changes outside the Kronos and having the
file remain loadable.** Every bank-container chunk carries a checksum byte that the
Kronos verifies on load; editing chunk payload bytes without recomputing it produces
a file that may be silently rejected or misread on the real instrument. Algorithm
confirmed directly from `KronosPcgMemory.FixChecksumValues`
(`KorgKronosTools/Model/KronosSpecific/Pcg/KronosPcgMemory.cs:40-86`) **and now
verified byte-for-byte on every chunk of all 32 real corpus files (2405/2405
matched) and confirmed by real hardware (Phase B, 2026-08)**:

- **Which chunks**: every `PBK1`, `MBK1`, `CBK1`, `SBK1`, `GLB1`, `WBK1`, `DBK1` chunk in the file
  — i.e. every Program/GM, Combi, Set List, Global, Wave Sequence and Drum Kit bank chunk (§2.2's
  dispatch table) — regardless of whether this project has decoded that chunk's *contents* (Drum
  Kit and Wave Sequence banks are still opaque per §6/§7, but their checksum still has to be fixed
  after any edit).
- **Algorithm**: for each such chunk, sum every payload byte from `chunk.Offset+12` (first byte
  after the 12-byte chunk header: 4-byte tag + 4-byte size + 4 more header bytes) through
  `chunk.Offset+chunk.Size+12` (exclusive), **mod 256**. Write that single byte to
  `Content[chunk.Offset+11]` — the last byte of the 12-byte chunk header. In the generic
  chunk-header diagram (§2.2) this byte sits where "flags" would generically go; **for these seven
  chunk types specifically it is a checksum, not a flags byte.**
- **OS 1.5/1.6 only**: the same checksum is *also* duplicated into a lookup table inside the `INI2`
  chunk, at `FindIni2Or3Offset(chunk.Name, occurrenceIndex) + 54` (scanned as 64-byte records, 16
  bytes into `INI2`, matched by chunk name + occurrence index). Irrelevant for any OS 2.x/3.x file
  — the header byte at `0x07` (§2.1) selects the checksum scheme and OS 2.x/3.x skips this path
  entirely.
- The top-level `KORG` header's byte `0x08` (hand notes: `00`=No checksum, `01`=Checksum) is a
  **global gate** for whether checksumming applies to the file at all — independent of, and
  checked before, everything above. **2026-08 corpus: all 32 user-saved BBPB files have byte
  8 = `0x01`; the OS 3.x factory `PRELOAD.PCG` has byte 8 = `0x00`. Consistent with "checksums
  written" per save.**

**Practical rule for any external editor**: after changing bytes inside a Program/Combi/SetList/
DrumKit/WaveSequence/Global bank chunk — including moving a patch, renaming it, or repointing a
reference — recompute that chunk's checksum with this exact algorithm before writing the file
back. This is a straightforward, already-solved procedure (`FixChecksumValues` can be ported
directly), not something requiring further reverse engineering. **2026-08: the Phase A tooling
(`pcg_truth.py`) implements this and a rewrite round-trip (flip a byte, recompute, re-read)
passed 37/37 files.** Note: `DIY-KORG-KRONOS-EDITOR` currently writes files without recomputing
this checksum — its saved output will be **rejected per-bank** (or, if GLB1 is touched,
**abort the whole load**) until it ports this algorithm.

**Hardware-verified failure modes (Phase B, 2026-08, real Kronos OS 3.x)**: a test set
of 8 mutated files was loaded on the instrument with these exact results:

| File | Mutation | Checksum | Kronos result |
|---|---|---|---|
| T1 | pristine copy | all good | loads fine |
| T2 | rename Program I-A:000 | **MBK1 stale** | loads, but **Bank I-A = "File unavailable"** |
| T3 | same rename | recomputed | loads fine, name shows |
| T4 | 1 byte flipped in Program I-A:000 | **MBK1 stale** | loads, but **Bank I-A = "File unavailable"** |
| T5 | Combi I-A:000 timbre1 → I-B:000 | recomputed | loads fine, repoint works |
| T6 | T3+T5 combined | recomputed | loads fine, both changes visible |
| T7 | GLB1 Program Category 0 rename | **GLB1 stale** | **fails at "Now writing into internal memory"** |
| T8 | same GLB1 rename | recomputed | loads fine, category name shows |

**Interpretation**: (1) the chunk checksum byte IS validated by the Kronos, not
advisory; (2) the failure mode is **per-chunk**: a stale bank-chunk (`MBK1`) checksum
causes only that bank to be skipped ("File unavailable"), while a stale `GLB1`
checksum aborts the whole write-to-internal-memory (Global is not skippable — it's
required for the load to commit); (3) every edit with recomputed checksums loaded and
was visible (rename, timbre repoint, GLB1 category name @12912) — so the entire
container/record model, the checksum algorithm, and the 12912 offset are now
hardware-confirmed; (4) a single flipped content byte (T4) with a stale checksum is
treated the same as a rename with a stale checksum (both = that bank unavailable),
so the checksum is the gate, not the content-change type.

---

## Sample files used for verification

Pulled live via FTP from a real Kronos:

| File | Source path | Size | Used for |
|---|---|---|---|
| `PRELOAD.PCG` | `SSD1/FACTORY/PRELOAD.PCG` | 30 019 816 bytes | Full-factory-set container/chunk-order/program/combi hex verification (§2.3, §3.1, §4.1, §4.2) |
| `PRELOAD.SNG` | `SSD1/FACTORY/PRELOAD.SNG` | 1 293 906 bytes | Header-only spot check |
| `SHINEONOPEN-PF.PCG` | `SSD2/ANDRE_K2_73/SHINEONOPEN-PF.PCG` | 2 269 620 bytes | Header spot check (small real user file) |
| `BBPB` corpus (32 files) | `Z:/PCG EXAMPLES/DC BANDS/BBPB/` + `Archive/` | 47 866 616 bytes each | **2026-08 Phase A**: full corpus verification (checksums 2405/2405, chunk order, bank inventories, 4802/0/24 frames, func33 slot encoding, drum-kit/wave-seq/UUID structures, reference-graph invariants) |
| `usersample.KSC` | `SSD2/ANDRE_K2_73/usersample.KSC` | 379 839 bytes | See [`ksc_kmp_ksf_file_format.md`](ksc_kmp_ksf_file_format.md) |
| `usersample_UserBank.KSC` | `SSD2/ANDRE_K2_73/usersample_UserBank.KSC` | 407 380 bytes | See [`ksc_kmp_ksf_file_format.md`](ksc_kmp_ksf_file_format.md) |
