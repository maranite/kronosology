# PCG Hardware Test Set — load order, mutations & expected outcomes

**Base file**: `JULY-27_2026(FINAL).PCG` (real Kronos OS 3.x, 47,866,616 bytes)
**All 8 files**: full copies, same size; each has ONE controlled mutation.
Load each with **Disk → Load PCG** and report: dialog text / error / silent reject /
partial load / success (and whether the edit is visible).

## Test matrix

| # | File | Mutation | Checksums | What to check |
|---|---|---|---|---|
| T1 | `T1_CONTROL_original.PCG` | none (pristine copy) | all good | Baseline: should load with no errors at all |
| T2 | `T2_diy_rename_NO_checksum.PCG` | Rename **Program I-A:000** → `TEST-A-RENAME!!!` (raw bytes, like DIY editor writes) | **MBK1 checksum stale** | **KEY TEST**: does the Kronos load it? If rejected → checksum byte gates loading. If loads → name shows `TEST-A-RENAME!!!` |
| T3 | `T3_diy_rename_WITH_checksum.PCG` | Same rename | all good (recomputed) | Control for T2: must load, name = `TEST-A-RENAME!!!` |
| T4 | `T4_flip_byte_NO_checksum.PCG` | Flip 1 byte inside Program I-A:000 body (offset +40) | **MBK1 checksum stale** | Does a single corrupted content byte (not a rename) also get rejected? |
| T5 | `T5_repoint_timbre_WITH_checksum.PCG` | Combi I-A:000 timbre 1 → **Program I-B:000** (was U-G:001) | all good (recomputed) | Does the combi now play the I-B:000 program? Validates the timbre bank/number encoding |
| T6 | `T6_rename_AND_repoint_WITH_checksum.PCG` | T3 + T5 combined | all good | Both changes visible together |
| T7 | `T7_glb1_category_NO_checksum.PCG` | Rename **Program Category 0** in GLB1 → `TEST-CATEGORY-RENAM` (offset 12912) | **GLB1 checksum stale** | Does the Kronos show the new category name in Program select? Is GLB1 also checksum-gated? |
| T8 | `T8_glb1_category_WITH_checksum.PCG` | Same GLB1 rename | all good (recomputed) | Control for T7: category name shows, no load error — proves the 12912 offset |

## Design notes

- **T2 vs T3 / T7 vs T8** are the critical pairs: identical edit, one with stale chunk
  checksum and one with recomputed. Whatever difference the Kronos shows between each
  pair isolates the checksum byte's role.
- **T4** isolates "content integrity" from "rename" — a random byte flip vs a deliberate
  name change.
- **T5** validates the reference-encoding (func33/raw bank) that every tool relies on.
- Files with a stale checksum are marked with the **exact chunk** whose checksum is
  wrong (`MBK1` for T2/T4, `GLB1` for T7) — so if the Kronos names the offending chunk
  in an error, we can match it.

## After each load

1. Note the exact screen text (even a generic "file cannot be loaded").
2. If it loads: go to Program I-A:000 / Combi I-A:000 / Global→Category and confirm
   whether the edit is visible.
3. If it fails: try loading a *second* time — some instruments only warn once.
4. Optionally: on the Kronos, **Save PCG again** (re-save) and keep that output — a
   byte-diff against the input tells us exactly which fields the Kronos itself rewrites
   (checksums, DIV1, PCG1 size, etc.).
