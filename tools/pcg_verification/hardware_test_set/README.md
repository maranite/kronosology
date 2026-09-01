# Hardware Test Set — Kronos Load Results (2026-08, Phase B)

8 mutated copies of `JULY-27_2026(FINAL).PCG` (real Kronos OS 3.x, 47,866,616 B),
loaded one at a time on the instrument via Disk → Load PCG.

## Results (as reported by the operator)

| File | Mutation | Checksum | Kronos result |
|---|---|---|---|
| T1 | pristine copy | all good | loads fine |
| T2 | rename Program I-A:000 → TEST-A-RENAME!!! | **MBK1 stale** | loads, **Bank I-A = "File unavailable"** |
| T3 | same rename | recomputed | loads fine, name shows |
| T4 | 1 byte flipped in Program I-A:000 body | **MBK1 stale** | loads, **Bank I-A = "File unavailable"** |
| T5 | Combi I-A:000 timbre1 → Program I-B:000 | recomputed | loads fine, repoint works |
| T6 | T3 + T5 combined | recomputed | loads fine, both visible |
| T7 | GLB1 Program Category 0 name → TEST-CATEGORY-RENAM | **GLB1 stale** | **fails at "Now writing into internal memory"** |
| T8 | same GLB1 rename | recomputed | loads fine, category name shows |

## Conclusions

1. **Chunk checksums are validated by the Kronos, not advisory.**
2. **Failure mode is per-chunk**: a stale bank-chunk (`MBK1`) checksum → only that
   bank is skipped ("File unavailable"); a stale `GLB1` checksum → the whole load
   aborts at commit ("Now writing into internal memory").
3. Every edit with recomputed checksums loaded and was visible on the instrument:
   program rename, timbre repoint, GLB1 category-name @12912 — so the container
   model, the record offsets (4802/0/24), the checksum algorithm (sum&0xFF at
   header+11), and the 12912 category table offset are all **hardware-confirmed**.
4. A single flipped content byte with stale checksum behaves identically to a
   rename with stale checksum — the checksum byte is the gate, not the edit type.

## Files preserved here

- `T2_diy_rename_NO_checksum.PCG` — the exact DIY-editor write path (rename, no
  checksum recompute) → reproduces "Bank I-A File unavailable".
- `T7_glb1_category_NO_checksum.PCG` — GLB1 rename, stale checksum → reproduces the
  fatal "Now writing into internal memory" abort.
- `manifest.md` — the full test matrix + load-order notes (also in
  `Z:/PCG EXAMPLES/DC BANDS/HARDWARE_TEST_SET/`).

Regenerate the full set with `Z:/PCG EXAMPLES/DC BANDS/_WORKING/gen_test_pcgs.py`.
