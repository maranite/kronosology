# Effect Presets — `FXPR.BIN`

A library of factory + user effect presets, each holding a snapshot of one IFX, MFX, or
TFX slot's parameter settings. When the user picks "Load Preset" on any effect page,
the choices come from here.

| Property | Value |
|---|---|
| File | `FXPR.BIN` |
| File size | 523 777 bytes |
| Header | No `P***` magic — starts directly with first preset name |
| Format | Sequence of fixed-size preset records, each prefixed by a 24-byte ASCII name |

---

## Observed structure

First 256 bytes:

```
00000000: 49 6e 69 74 69 61 6c 20 53 65 74 20 20 20 20 20     "Initial Set     "
00000010: 20 20 20 20 20 20 20 20 00 00 00 00 00 00 00 00     "        " + null padding
...
00000050: 00 00 00 00 00 00 00 00 65 6d 70 74 79 20 20 20             "empty   "
00000060: 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20
00000070: 00 00 00 00 ...
...
000000B0: 65 6d 70 74 79 20 20 20 20 20 20 20 20 20 20 20     "empty           "
```

Pattern: each preset record is roughly 96 bytes (or some multiple), with:
- 24-byte name (ASCII, space-padded)
- ~72 bytes of parameter blob

If preset size is 96 bytes: 523 777 / 96 ≈ 5 456 presets — too many. If 192 bytes:
~2 728. If 384 bytes: ~1 364. Without further analysis of the Eva `CLoadAllEffectPresetsDlg`
code path the exact count and size aren't pinned, but the **"Initial Set"** first entry
followed by many **"empty"** placeholders suggests Korg ships a small number of named
factory presets at the start and leaves the rest empty for user fill.

---

## Conceptual structure

Each effect preset belongs to a specific **effect type** (e.g. Stereo Compressor, Hall
Reverb, Multi-Tap Delay). The preset blob contains:

- Effect type ID
- All parameter values for that effect type
- Bypass / wet-dry-mix defaults
- Tempo-sync settings

When the user picks "Load Preset" in an effect slot, Eva filters the preset list to
only those compatible with the slot's chosen effect type, then pastes the parameter
values into the running effect.

---

## Loader

Eva loads `FXPR.BIN` via its `CLoadAllEffectPresetsDlg` / `CLoadAllEffectPresetsIn1FxDlg`
forms. The loaded presets are held in an Eva-side cache; OA.ko receives the parameter
values only when the user actually picks one (via `/proc/.oacmd`).

`FXPR.BIN` is mutated by Eva when the user does **WRITE preset** in any effect editor
— the in-edit effect's parameters get written to the next free slot (or overwrite an
existing named slot).

---

## See also

- [program_banks.md](program_banks.md) — each program has one IFX, one MFX, one TFX slot
- [combi_banks.md](combi_banks.md) — combis have 12 IFX, 2 MFX, 2 TFX slots
- `KRONOS_Op_Guide_E10.pdf`, "Effects" chapter — concept reference
- `KRONOS_Param_Guide_E10.pdf`, ch. "Effect Guide" — per-effect parameter reference (>500 pages)
