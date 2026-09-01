# Phase A Report — PCG Verification Against Kronos-Written Files

**Corpus**: `Z:/PCG EXAMPLES/DC BANDS/BBPB/` — 32 `.PCG` files (plus 5 dedup variants = 37 unique parses),
all written by a Kronos OS 3.x (header byte 7 = `0x02`; `checksum_flag` byte 8 = `0x01`),
each 47,866,616 bytes. Plus `april_2019/` (47 `.KMP` / 251 `.KSF` sample files — unused this phase).

**Tooling** (all in `_WORKING/`, read-only except explicit copy-writes):
- `pcg_truth.py` — golden-truth model: header, chunk walk, bank inventory, record decoders, checksums.
- `phase_a_sweep.py` — corpus-wide consistency + A/B frame tests + checksum rewrite test.
- `phase_a_deep.py` — drum-kit / wave-seq / DPI probes + reference-graph invariant.
- Outputs: `sweep_all.txt`, `deep_all.txt` (full per-file dumps).

---

## 1. Headline result: every disputed offset is settled

| Disputed point | Result (corpus-wide) | Verdict |
|---|---|---|
| Combi timbre table base | **4802**: 75,591 valid refs; **4806**: 110 | **4802 confirmed** (DIY's 4806 refuted) |
| Name offset | **+0**: 179,586/192,918 printable; **+4**: 43,746 | **+0 confirmed** (DIY's +4 refuted) |
| SBK1 slot params | **+24**: type hist {1:15972, 0:411, 2:1} clean; **+12**: garbage {3:102, 2:51, 1:149} | **+24 confirmed** (DIY's +12 refuted) |
| Chunk order | `PCG1 → DIV1 → SLS1 → PRG1 → CMB1 → DKT1 → WSQ1 → GLB1 → DPI1` (identical in all 37) | kronosology's hex-walk confirmed; hand-notes "SLS1-in-PRG1" refuted |
| `PCG1` container | present, size 0x2da62dc, wraps everything | real (PCG-Tools code ignores it) |
| Checksum byte | byte at header+11 = sum(payload)&0xFF — **2405/2405 chunks OK** | PCG-Tools `FixChecksumValues` confirmed byte-exact |
| Program record size | 4960 everywhere (all 20 banks, HD-1 and EXi) | confirmed |
| Bank inventories | 20 Program / 14 Combi / 15 DK / 15 WSQ / 1 SBK, counts 128/128/128/40+16×14/150+32×14 | confirmed |
| Global category tables | 12912/13344/16800/17232 all read `Keyboard`/`A.Piano`… | confirmed |
| Drum-kit sample UUID | 100% of populated zones use `KORG 00×8 MS 00 nn` legacy prefix | kronosology §6/§7 confirmed |
| Wave-seq bank UUID | 100% of steps use same legacy prefix | kronosology §7 confirmed |
| Reference graph | **0 dangling** Combi-timbre and SetList-slot refs | cross-reference encoding confirmed |

## 2. What the A/B tests actually proved

### 2.1 The "4-byte marker" is a systematic misparse in DIY, not a real format feature
Every disputed DIY offset is **exactly 4 bytes off from the ground truth**, in the same direction:
- Combi name at +0 (DIY: +4) — reads `"SEXYBACK MAIN"` at +0, `"BACK MAIN..."` at +4.
- Program name at +0 (DIY: +4) — `"Berlin Grand SW2 U.C."` at +0.
- Timbre table at 4802 (DIY: 4806) — 4806 reads `(0,127),(0,67),(0,109)` garbage.
- SBK1 slot params at +24/+25/+26 (DIY: +12/+13/+14) — +12 reads type=3 (invalid), bank=0, num=0.

Root cause identified from DIY's own code: `PcgFile.cpp` computes
`recordsStart = chunk.contentStart + 8` where `contentStart = tag_offset + 12`, so records
start **4 bytes early** (inside the bank-id field), and it compensates by reading names at
`+4` and timbres at `+6`-relative. Its *reads* may coincidentally land on correct bytes for
names (because the 4-byte early start + +4 name offset = correct absolute), but the
timbre/params frames and any *write* path are off. **DIY must adopt the 4802/0/24 frame.**
(Note: this also explains why DIY's own tests passed — they were self-consistent.)

### 2.2 Set-list slots use func33 bank codes; combi timbres use raw bank codes
- Set-list slot bank byte = **func33/live-wire encoding**: 0..5 INT, 6=GM, 7..16 g(1..d), 17..30 USER
  (USER-A=17 → file idx 6, USER-GG=30 → file idx 19, i.e. `func33 = file_idx + 11` for USER).
- Combi timbre bank byte = **raw internal code**: 0..5 INT-A..F, 17..30 USER-A..GG (same as DIY's
  `kConfirmedTimbreBanks`; `USER code - 11 = file idx`).
- Both resolve 100% of refs to real banks with zero dangling.

### 2.3 Checksums are real, verified, and required
Every one of the 7 chunk types (MBK1/PBK1/CBK1/SBK1/GLB1/WBK1/DBK1) carries a checksum
byte at `header+11` = `sum(payload from +12 to +12+size) & 0xFF`. All 2405 chunks across
all 37 files match. The rewrite test (flip a byte, recompute, re-read) passes 37/37.
**DIY's editor writes files without recomputing this — its saved files will be rejected or
misread by the Kronos.** This is the single highest-priority fix.

### 2.4 Song-type set-list slots exist in real files
The FINAL file's set list 0 slot 0 is **type 2 (Song)** — the first real Song-type slot
observed anywhere in this ecosystem. (DIY's docs said "never observed in a real file".)

## 3. Deep structural results (previously unmapped anywhere)

### Drum Kit (`DBK1`, 38424 B)
- Layout confirmed: 24-byte name, then 128 notes × 300 B; note = 8 zones × 34 + 7×3 crossfade + 7-byte trailer.
- `JazzAmbi Kit Dry` (U-A): 366/1024 zones populated (on=1), all `KORG...MS\x00{nn}` with **nn=0** (ROM bank), sample ids 1..1211 (sequential clusters). `Trance kit` (Int) is the factory kit.
- The legacy `nn` byte and the LE sample-id at +18/+19 decode exactly as kronosology §6 documented.

### Wave Sequence (`WBK1`, 2216 B)
- Layout confirmed: 24-byte name, 16-byte common, 64 steps × 34 B.
- `19 Orch/Band HITS` (Int): all 64 steps type 0 (Multisample), all legacy bank UUID, multisample select 1182.. (LE, clustered).

### Drum Track Patterns (`DPI1`)
- Skeleton parses: `DPN1` (476 B) → `DPD1` (7436 B) → `DPS1` (322,376 B, containing DPV1s).
- Content not decoded (out of scope, matches PCG-Tools' `NotImplementedException`).

### Global (`GLB1`, 24708 B)
- Category tables at 12912 (18 Prog cat × 24), 13344 (18×8 Prog sub), 16800 (Combi cat), 17232 (Combi sub) — all read real names (`Keyboard`, `A.Piano`, …).
- GLB1 record has **no name at +0** (starts `00 00 08 02…`) — Global's record base is not name-first like the others; **the exact record base (payload+0 vs +12 vs +16) is the one remaining frame to pin** (Phase B B5 / a small follow-up).

## 4. Verification summary across the corpus

| Check | Result |
|---|---|
| Files parsed | 37 (32 unique) |
| Banks found | 65/file (20 prog + 14 combi + 15 DK + 15 WSQ + 1 SBK) |
| Chunk checksums | 2405/2405 OK |
| Rewrite round-trip | 37/37 OK |
| Timbre A/B (4802) | 75,591 valid vs 110 @4806 |
| Name A/B (+0) | 179,586 printable vs 43,746 @+4 |
| Slot A/B (+24) | clean type hist vs garbage @+12 |
| Dangling combi refs | 0 |
| Dangling set-list refs | 0 |
| DK zone bank-UUIDs | 296/296 legacy `KORG...MS` |
| WSQ step bank-UUIDs | 2368/2368 legacy `KORG...MS` |

## 5. Findings requiring Phase B (on-Kronos) confirmation

1. **Checksum enforcement**: are the chunk checksums *validated* on load (reject bad file) or *advisory*? The rewrite test proves we can produce valid files, but only the Kronos can tell us if a deliberately-broken checksum is rejected. (B2/B6.)
2. **Kronos re-save byte-equality**: save a corpus file on the Kronos and diff — proves the whole container model on ground truth (B1.2).
3. **3706-byte HD-1 wire truncation**: KSR claims wire = first 3706 of 4960. Needs live dump comparison (B4).
4. **GLB1 record base**: pin the exact payload offset (payload+0 vs +12) using the category tables (B5 / A5).
5. **DIV1 byte 2 / partial-save behavior**: this corpus's DIV1 flag dword reads `01 01 00 01` (byte2=0). A partial save (only some categories) should change it — test on hardware (B7).
6. **`PCG1` outer size**: DIY's walker needs the +0x10 `PCG1` container handling if it's to match PCG-Tools structurally.

## 6. Actionable fixes for the four solutions (from this phase alone)

- **DIY-KORG-KRONOS-EDITOR** (highest priority):
  1. Fix `recordsStart` to `chunk.offset + 24` (not `contentStart + 8`), names at +0, timbre base 4802, slot params at +24/+25/+26.
  2. Port PCG-Tools' `FixChecksumValues` (sum&0xFF at header+11 for the 7 types) and call it on `save()`.
  3. Add `PCG1` container handling in `collectChunks` (walk from 0x10, or at least skip the wrapper).
- **KronosScreenRemote**: already correct on all confirmed frames (4802/24/4960). Optionally: teach the PCG importer to *write* (it currently never writes .pcg), and add the checksum write path if it ever does.
- **PCG Tools**: already the reference implementation. Its docs (`PCG Structure Kronos.txt`) should be corrected to remove the "SLS1 nested in PRG1" and "sometimes 10" notes — the corpus refutes both.
- **kronosology**: docs confirmed almost 100% (chunk order, checksums, UUIDs, record sizes, bank counts, category tables). Update §1's open note about the "4-byte marker" uncertainty (resolved: no such feature; DIY misparse) and note the corpus-verified `PCG1` container + `checksum_flag=0x01` in real OS 3.x files.

## 7. Reproducibility

```
python _WORKING/pcg_truth.py BBPB/*.PCG          # per-file bank/checksum dump
python _WORKING/phase_a_sweep.py                  # full corpus sweep + A/B + rewrite test
python _WORKING/phase_a_deep.py                   # drum/wave/dpi + reference graph
```

All three scripts are read-only over the corpus; the only write is `_WORKING/csum_test.PCG`
(disposable copy). Original files in `BBPB/` and `Archive/` are untouched.
