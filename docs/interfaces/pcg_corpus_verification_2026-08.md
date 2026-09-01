# PCG Corpus Verification (2026-08, "Phase A")

Records the results of verifying the `.PCG` format documented in
[`pcg_file_format.md`](pcg_file_format.md) against a corpus of **32 real
Kronos-written `.PCG` files**. The main format doc now carries the inline
confirmation notes; this file is the standalone record of what was tested, how,
and the exact numbers.

## Corpus

- **Location**: `Z:/PCG EXAMPLES/DC BANDS/BBPB/` (top level + `Archive/`)
- **32 files**, each 47,866,616 bytes, all written by a Kronos **OS 3.x**
  (header byte 7 = `0x02`; byte 8 = `0x01` checksum flag)
- Plus `april_2019/` (47 `.KMP` / 251 `.KSF` — unused this phase)
- A 37-file effective set (some byte-identical re-saves) parsed

## Tooling (kept at `Z:/PCG EXAMPLES/DC BANDS/_WORKING/`)

| Script | Purpose |
|---|---|
| `pcg_truth.py` | Golden-truth parser: header, 12-byte chunk walk, bank inventory, record decoders, checksums |
| `phase_a_sweep.py` | Corpus-wide consistency + A/B frame tests + checksum rewrite test |
| `phase_a_deep.py` | Drum-kit / wave-seq / DPI probes + reference-graph invariant |
| `sweep_all.txt`, `deep_all.txt` | Full per-file dumps (regenerable) |

All scripts are read-only over the corpus; the only write is a disposable copy in
`_WORKING/`. Originals untouched.

## Results

### Container framework — confirmed on all 32 files

- 8-byte file header `4B 4F 52 47 68 00 02 02 01` → `KORG`, model `0x68`,
  filetype `0x00`, byte6 `0x02`, **OS byte 7 = `0x02`**, **checksum flag byte 8 = `0x01`**
- **`PCG1` outer container at `0x10`** (size = file size − 0x14), wrapping
  everything from `DIV1` onward
- 12-byte chunk header `[tag][u32 BE size][4-byte dwX]`, payload at +12
- Top-level order (identical in all 32): `DIV1 → SLS1 → PRG1 → CMB1 → DKT1 →
  WSQ1 → GLB1 → DPI1`
- `SLS1` contains `SLD1` then `STL1`; `STL1` contains `SBK1`

### Bank inventories — identical across all 32 files

| Type | Chunks | Counts (Int / user) | Itemsize |
|---|---|---|---|
| Program (`MBK1`/`PBK1`) | 20 | 128 × 20 | 4960 |
| Combi (`CBK1`) | 14 | 128 × 14 | 7810 |
| Drum Kit (`DBK1`) | 15 | 40 + 14×16 | 38424 |
| Wave Seq (`WBK1`) | 15 | 150 + 14×32 | 2216 |
| Set List (`SBK1`) | 1 | 128 | 69416 |

Bank-id encodings verified exactly as documented: Program `0..4`, `0x8000` (I-F),
`0x20000..0x2000D`; Combi `0..6`, `0x20000..0x20006`.

### A/B frame tests (the headline)

| Question | Result |
|---|---|
| Combi timbre table base | **4802**: 75,591 valid refs · **4806**: 110 |
| Record name offset | **+0**: 179,586 printable · **+4**: 43,746 |
| SBK1 slot params | **+24/+25/+26** clean (hist 15972 P / 411 C / 1 Song) · **+12** garbage (hist 3-heavy) |

Conclusion: the correct absolute frame is **4802 / record+0 / slot+24**. The
"4-byte marker" hypothesis is refuted — it was an artifact of the DIY editor's
reader starting records 4 bytes early and compensating with +4/+4806/+12.

### Checksums — 2405/2405 chunks matched

`byte at chunk.offset+11 = sum(payload from +12 to +12+size) & 0xFF`, verified on
every `MBK1/PBK1/CBK1/SBK1/GLB1/WBK1/DBK1` chunk of every file. A rewrite
round-trip (flip byte → recompute → re-read) passed 37/37.

### Set-list slot bank encoding

Slot `Performance Bank` byte (+25) is the **func33 / live-wire** bank code:
`0..5` INT, `6` GM, `7..16` g(1..d), `17..30` USER (`func33 = file_index + 11`
for USER). Combi timbre bank byte (+4803) is the **raw** code: `0..5` INT-A..F,
`17..30` USER-A..GG. With these mappings, **0 dangling references** across all
combi timbres and set-list slots of all files.

### Object types previously un-decoded anywhere

- **Drum Kit**: layout confirmed (24-byte name + 128 notes × 300; 8 zones × 34 +
  7×3 crossfade + 7 trailer). `JazzAmbi Kit Dry` (U-A): 366/1024 zones populated,
  all legacy `KORG\x00×8\x00MS\x00{nn}` UUID with `nn=0` (ROM), sample ids 1..1211.
- **Wave Sequence**: layout confirmed (24-byte name + 16-byte common + 64 steps ×
  34). `19 Orch/Band HITS` (Int): 64/64 Multisample steps, all legacy bank UUID,
  multisample selects 1182.. (LE, clustered).
- **DPI1** skeleton: `DPN1` (476 B) → `DPD1` (7436 B) → `DPS1` (322,376 B).
- **Global**: category tables at 12912 / 13344 / 16800 / 17232 all read real
  names. GLB1 record has **no name at +0** (base not yet pinned — Phase B).

### Reference-graph invariants

- 0 dangling Combi-timbre refs, 0 dangling SetList-slot refs (with the correct
  bank encodings above) — a Kronos-written file has no broken internal
  references.

## What this settles vs what Phase B must settle

**Settled (documented in `pcg_file_format.md`):** container + chunk order +
`PCG1`; all record sizes; 4802/0/24 frames; bank-id + func33 encodings; checksum
algorithm; DIV1 byte2 = 0 on this corpus; Global category tables; drum/wave-seq
structure + bank-UUID scheme; first real Song-type set-list slot observed.

**Still open (hardware, Phase B):** is a Kronos re-save of an unchanged file
byte-identical? Is the HD-1 SysEx dump exactly the first 3706 bytes of the
4960-byte PCG record? What is the exact GLB1 record base? Does a partial save
change DIV1 byte 2?

## Phase B hardware results (2026-08, real Kronos OS 3.x)

A set of 8 mutated copies (`Z:/PCG EXAMPLES/DC BANDS/HARDWARE_TEST_SET/`, see
`manifest.md`) was loaded on the instrument. Results:

| File | Mutation | Checksum | Kronos result |
|---|---|---|---|
| T1 | pristine copy | all good | loads fine |
| T2 | rename Program I-A:000 | **MBK1 stale** | loads, **Bank I-A = "File unavailable"** |
| T3 | same rename | recomputed | loads fine, name shows |
| T4 | 1 byte flipped in Program I-A:000 | **MBK1 stale** | loads, **Bank I-A = "File unavailable"** |
| T5 | Combi I-A:000 timbre1 → I-B:000 | recomputed | loads fine, repoint works |
| T6 | T3+T5 combined | recomputed | loads fine, both visible |
| T7 | GLB1 Program Category 0 rename | **GLB1 stale** | **fails at "Now writing into internal memory"** |
| T8 | same GLB1 rename | recomputed | loads fine, category name shows |

**Conclusions (hardware-confirmed):**
- The chunk checksum byte **is validated** by the Kronos (not advisory).
- Failure mode is **per-chunk**: a stale bank-chunk (`MBK1`) checksum → only that
  bank is skipped ("File unavailable"); a stale `GLB1` checksum → whole load
  aborts at commit (Global is required, not skippable).
- All edits with recomputed checksums loaded and were visible on the instrument:
  rename, timbre repoint, GLB1 category-name @12912. The container/record model,
  the checksum algorithm, and the 12912 offset are now hardware-confirmed.
- A single flipped content byte with stale checksum (T4) behaves identically to a
  rename with stale checksum (T2) — the checksum is the gate, not the edit type.

## Phase B follow-ups (re-save + SysEx, 2026-08)

### Kronos re-save is NOT byte-identical — DPI1 grows

`T8_RESAVE.PCG` (the Kronos's own re-save of T8, fetched over FTP from
`SSD2/TEST/`) is **170,614 bytes larger** than T8 (48,037,230 vs 47,866,616).
Diff analysis:
- **Every object chunk is preserved byte-identical**: SLS1/PRG1/CMB1/DKT1/WSQ1/GLB1
  all at the same offsets, same sizes, same payloads. The GLB1 category edit
  (`TEST-CATEGORY-RENAM`) survived and its checksum was recomputed correctly by the
  Kronos (0x96/0x96).
- **DPI1 grew 330,324 → 500,938** (+170,614 = the whole size increase). Inside:
  DPN1 476→834, DPD1 7436→13164, DPS1 322,376→486,904. DPD1's header shows the
  drum-pattern count expanded **232 → 411** (pattern size 32 bytes, names starting
  `STAY`). The Kronos filled in/added factory drum-track patterns on re-save.
- **DIV1 byte 2 (0x4e) changed 0x00 → 0xB6** and the trailing `0x50-51` changed
  `ff 00` → `00 01` — DIV1's byte-2 mystery (§2.3a) is resolved: it's **DPI state
  the Kronos recomputes on save**, not a boolean. The `PCG1` size field at 0x10
  updated to match.

### HD-1 SysEx dump = exactly the PCG record's first 3706 bytes

`HD-1_SysexDumpU-FF.txt` (a 128-program Object-Dump capture of bank U-FF) decodes
via KSR's 8-to-7 codec to **3706 bytes per program**, and **127/128 match the
first 3706 bytes of their PCG U-FF records byte-for-byte**. This is the definitive
confirmation of §3.1: HD-1 wire = 3706 (the PCG record prefix); EXi wire = 4960
(identical). KSR's `ProgramFormatConverter` is hardware-confirmed.

### Still open
- Exact GLB1 record base (payload+0 vs +12 vs +16).
- What precisely DIV1 byte-2 encodes (pattern count fragment? sub-chunk checksum?).
