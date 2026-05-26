# `/korg/rw/PRELOAD/` — Committed Preset Memory

This directory holds the Kronos's persistent "committed memory" — every program, combi,
drum kit, wave sequence, KARMA GE, drum-track pattern, set list, effect preset, and
piece of global state that survives a power cycle. They are read at boot by `OA.ko`
and `Eva` and written back on user-initiated "Write" operations.

The Kronos UI calls this "Internal Memory" or "Saved Memory". When you press the **WRITE**
button on the front panel, you are ultimately mutating one of these files.

---

## Quick file inventory

| File family | Magic | Count | Role | Doc |
|---|---|---|---|---|
| `PROG?.BIN` (16) + `PROG??.BIN` (7) | `PMOS`/`PPCM` | 23 banks × 128 programs | Program (HD-1 PCM and MOSS/EXi) banks | [program_banks.md](program_banks.md) |
| `COMB?.BIN` (14) | `PCMB` | 14 banks × 128 combis | Combination (combi) banks | [combi_banks.md](combi_banks.md) |
| `DKIT?.BIN` (8) + `DKIT??.BIN` (7) + `DKITI.BIN` | `PDKT` | 23+ banks × 16 (or 9) drum kits | Drum-kit banks | [drum_kit_banks.md](drum_kit_banks.md) |
| `WSEQ?.BIN` (8) + `WSEQ??.BIN` (7) | `PWSQ` | 15 banks × 32 (or 150) wave seqs | Wave-sequence banks | [wave_sequence_banks.md](wave_sequence_banks.md) |
| `STLS.BIN` | `PSTL` | 128 set lists | Set lists | [set_list.md](set_list.md) |
| `STMPP.BIN`, `STMPU.BIN` | `TMPU` / (no magic) | 18 templates each | Set-list templates (preset/user) | [set_list.md](set_list.md) |
| `GLBL.BIN` | `PGLB` | 1 record (24 708 bytes) | Global parameters | [global_settings.md](global_settings.md) |
| `PPAT.BIN`, `DPAT.BIN` | — | ~823 preset + ~1 000 user | Drum-track patterns | [drum_track_patterns.md](drum_track_patterns.md) |
| `GE.BIN`, `GE.KDF`, `KGEUA.BIN` | — | thousands of GEs + 1 user bank | KARMA Generated Effects | [karma_data.md](karma_data.md) |
| `FXPR.BIN` | — | ~128 effect presets | Effect/EQ presets | [effect_presets.md](effect_presets.md) |
| `PianoTypes/PianoType??` | — | 16 × 1626-byte records | SGX-2 piano types | [piano_types.md](piano_types.md) |
| `FANCTRL.BIN`, `PONMEM.BIN`, `SFCCMAP.BIN`, `FILESORT.BIN`, `KSCLIST.BIN` | — | various tiny | Misc state | [misc_files.md](misc_files.md) |

---

## What you'll find in the per-file docs

For each family, each doc covers:

- **Filename convention** (e.g. `PROG<letter>.BIN` / `PROG<letter><letter>.BIN`)
- **File sizes and bank count** observed on the factory dump
- **Container header layout** (magic, record count, record size, type)
- **Per-record structure** at the granularity we have so far
- **Loader and saver functions** in `OA.ko` and/or `Eva`
- **References to the Korg Operation Guide / Parameter Guide** for the conceptual meaning

---

## See also

| Document | Topic |
|---|---|
| [container_format.md](container_format.md) | The **universal 20-byte header** that 8 of the file families share |
| [extension_points.md](extension_points.md) | **Adding more program/combi/drum-kit/wave-seq banks** — every hardcoded limit, every loop bound, what it would take to patch |
| [../modules/OA.ko_analysis.md](../modules/OA.ko_analysis.md) | Where the engine init runs and what it calls |
| [../modules/OA.ko.md](../modules/OA.ko.md) | The kernel module that does most of this loading |
| [../modules/Eva.md](../modules/Eva.md) | The GUI app that does the rest (and handles all user-initiated writes) |
| `KRONOS_Op_Guide_E10.pdf` | Korg's Operation Guide — concept reference for banks, modes, write workflow |
| `KRONOS_Param_Guide_E10.pdf` | Korg's Parameter Guide — definitive list of every parameter in every record |

Both PDFs live under `KRONOS_Updater_3_2_1/Manuals/English/`.

---

## How OA.ko learns about these files

Boot sequence (relevant slice):

```
loadoa  →  loads kernel modules in order  →  OA.ko gets loaded
                                                  ↓
                                         module_init hook fires
                                                  ↓
                                         CSTGEngine::Initialize  (0x000001b0)
                                                  ↓
                                         CSTGGlobal::Initialize  (0x00008340)
                                                  ↓
                          ┌──────────────────────┼─────────────────────────────────┐
                          ↓                      ↓                                 ↓
              CSTGWaveSeqData::Initialize  InitializePerformances           CSetListBank::Initialize
                          ↓                      ↓                                 ↓
              opens WSEQ*.BIN files     opens PROG*.BIN (23 banks)           opens STLS.BIN
                                        then COMB*.BIN (14 banks)
```

DKIT, GE, KGEU, FXPR, PPAT, DPAT, PianoTypes etc. are loaded by **Eva** (the userspace
GUI), which `mmap`s or `read`s them via the path template `K:\PRELOAD\%s.BIN` (the `K:`
is a Windows-style prefix left over from cross-compilation — at runtime it resolves to
`/korg/rw/PRELOAD/`). See [../modules/Eva.md](../modules/Eva.md) for the `CPreloadFile`
class hierarchy.
