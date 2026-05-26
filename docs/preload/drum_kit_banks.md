# Drum-Kit Banks — `DKIT*.BIN`

A **Drum Kit** maps each of 88 keys (the Kronos's MIDI note range) to one or more
multisamples, with per-key tuning, level, pan, effect routing, and exclusive-group
information. The HD-1 PCM engine, and any program using a percussion-style oscillator,
plays through a drum kit.

| Property | Value |
|---|---|
| File naming (internal banks) | `DKIT<A..H>.BIN` — 8 files |
| File naming (user banks) | `DKIT<AA..GG>.BIN` — 7 files |
| Special file | `DKITI.BIN` — smaller record size, 9 kits |
| Total banks | **16** distinct files |
| Total drum-kit slots | ~265 (see breakdown below) |
| Header magic | `PDKT` |
| Container | See [container_format.md](container_format.md) |

---

## Per-file breakdown

| File | Size | Records | Record size | Subtype | Total kits |
|---|---|---|---|---|---|
| `DKITA.BIN` | 1 536 980 | 40 | 38 424 | `0x0003` | 40 |
| `DKITB.BIN`..`DKITH.BIN` | 614 804 each | 16 | 38 424 | `0x0003` | 7 × 16 = 112 |
| `DKITAA.BIN`..`DKITGG.BIN` | 614 804 each | 16 | 38 424 | `0x0003` | 7 × 16 = 112 |
| `DKITI.BIN` | 198 380 | 9 | 22 040 | `0x0002` | 9 (special) |
| **Total** | | | | | **273 drum kits** |

Notes:
- The "A" bank is larger (40 kits) because Korg packs more factory drum kits there
- The "I" file is unique: 9 records of a *different* size (22 040 bytes — `type=2` instead
  of `type=3`). This corresponds to the SGX-2 reduced-form drum kits used by the piano
  models for sympathetic-resonance noise tables — not user-editable in the conventional sense
- The double-letter files (AA..GG) are user drum-kit banks

---

## Per-record layout (38 424 bytes per kit — type 3)

```
Offset       Size       Field
-------------------------------------------------------------------
0x000        24         Drum-kit name (ASCII, null-padded)
0x018        ~16        Common: bank type, exclusive-group default, etc.
0x028     88 × 437      Per-key data (MIDI 9..96 = 88 keys), each:
                          • assigned multisample (bank + index, low/high velocity)
                          • level, pan, transpose, tuning
                          • exclusive group
                          • effect send levels (IFX 1..12 send amount)
                          • velocity-range curves
                          • drum/note flag
0x9610       ~16        Reserved / padding
-------------------------------------------------------------------
Total: 38 424 bytes (88 × 437 = 38 456 — slight discrepancy; exact field map TBD)
```

The 88 keys span MIDI notes C-1 through C9, but only ~88 are user-mappable in the UI
(C2 to C9 typically); the rest hold default mappings.

### `DKITI.BIN` (type 2 — reduced kit, 22 040 bytes/record)

Per Korg manual structure documentation, the SGX-2 piano model carries its own set of
"drum kits" containing the noise samples used for sympathetic resonance simulation.
These are not editable drum kits in the conventional UI sense — they're support data for
the piano model. The 9 records correspond to the 9 piano types Korg ships.

---

## Loader

Drum-kit loading is **not** done by `CSTGGlobal::InitializePerformances` in OA.ko —
it's done by **Eva** at startup via its `CPreloadFile` framework (see
[../modules/Eva.md](../modules/Eva.md)). Eva walks the PRELOAD directory, opens each
`DKIT*.BIN`, and uses `/proc/.oacmd` to feed the parsed kits to OA.ko's
`CSTGDrumKitBank` instance for runtime use.

The exception is `DKITI.BIN`, which is loaded by OA.ko's `CSTGPianoModel` /
`CPianoOsc::CopyMultisampleInfo` chain along with the corresponding piano type files
in `PianoTypes/`.

---

## Drum kit ↔ Program relationship

A program using a drum-kit oscillator (the HD-1 has a "Drum Kit" multisample mode)
references one drum kit by (bank, index). The reference is two bytes inside the
oscillator parameter block of the program record (offsets ~0x40+ in the oscillator
section). When the program plays, each note looks up the corresponding key in the
drum kit and forwards to that multisample.

This is the same mechanism the GM (General MIDI) bank uses to play "Drum Channel" on
MIDI channel 10 — a single GM program references the GM drum kit, which maps each
MIDI note to a percussion sample.

---

## See also

- [container_format.md](container_format.md)
- [program_banks.md](program_banks.md) — programs reference drum kits
- [wave_sequence_banks.md](wave_sequence_banks.md) — sibling resource type
- `KRONOS_Param_Guide_E10.pdf`, p. 470+ — drum-kit parameter reference
