# SGX-2 Piano Types — `PianoTypes/PianoType<NN>`

The **SGX-2** EXi engine — the acoustic-piano model that ships with the Kronos — uses
a set of "piano type" configuration files, one per piano model (German Grand, Japanese
Grand, Bösendorfer, etc.). Each `PianoType<NN>` file holds the per-note tuning, voicing
balance, hammer characteristics, string-resonance settings, and noise-table references
that define one piano.

| Property | Value |
|---|---|
| Directory | `/korg/rw/PRELOAD/PianoTypes/` |
| Files | `PianoType00`, `PianoType01`, …, `PianoType<NN>` |
| File size | 1 626 bytes each (every file) |
| Naming | `PianoType%02lu` — two-digit zero-padded index |
| Companion files | `PianoTypeInfo<NN>` — info/sidecar files (text or small binary) |

---

## Loader

OA.ko has dedicated `CSTGPianoTypes` and `CSTGPianoModel` classes that handle these
files directly (not via `CKorgPreloadFile`):

| Function | Address | Purpose |
|---|---|---|
| `CSTGPianoModel::RescanPianoTypes` | called from `ProcessOACmd` (`PT:` command) | Re-enumerate files in the `PianoTypes/` directory |
| `CSTGPianoTypes::AddPianoType(idx, name)` | (vtable method) | Register a piano-type index + display name |
| `CSTGPianoTypes::SetNthPianoType(slot, idx, name)` | (vtable method) | Bind a piano type to a slot |
| `CSTGPianoTypes::ReadPianoTypeInfo(idx, *info_out)` | (vtable method) | Open `PianoTypeInfo<NN>`, read metadata |
| `CSTGPianoTypes::WritePianoTypeInfo(idx, *info_in)` | (vtable method) | Write back metadata for a piano type |
| `CSTGPianoTypes::GetPianoTypeFileName(buf, idx)` | (vtable method) | `snprintf(buf, sz, "%s/PianoType%02lu", PRELOAD_PIANO_PATH, idx)` |
| `CSTGPianoTypes::GetPianoTypeInfoFileName(buf, idx)` | (vtable method) | `snprintf(buf, sz, "%s/PianoTypeInfo%02lu", PRELOAD_PIANO_PATH, idx)` |
| `CSTGPianoModelPatch::GetPianoType(ctx)` | program-parameter getter | Return the piano-type index for the current SGX-2 program |
| `CSTGPianoModelPatch::LoadPianoType(idx, …)` | runtime loader | Open `PianoType<NN>` and inject into the SGX-2 model |
| `CSTGPianoModelPatch::UpdatePianoType(ctx, val)` | program-parameter setter | Change which piano type a program uses |
| `CPianoOsc::CopyMultisampleInfo(piano_types_obj, idx)` | runtime helper | Wire the multisamples (from `DKITI.BIN`) to the piano model |

The `RescanPianoTypes` flow is triggered by the `PT:` command on `/proc/.oacmd`
(see [../interfaces/proc_oacmd.md](../interfaces/proc_oacmd.md)) — when Eva imports new
piano types (e.g. from an SGX-2 sound library), it sends `PT:` and OA.ko re-walks the
directory.

---

## Per-file structure (1 626 bytes)

The contents are SGX-2-internal — Korg's piano-model designers maintain the layout.
Broadly:

```
Offset    Size    Field
----------------------------------------------------------
0x000      24     Piano type name (ASCII, null-padded)
0x018       8     Sample-set reference (which DKITI entry)
0x020     ~88    Per-key tuning offsets (1 byte per key × ~88 keys)
0x078    ~256    String / hammer character coefficients
0x178    ~512    Velocity-to-loudness curve
0x378    ~256    Sympathetic-resonance coefficients
0x478    ~256    Per-note noise table references
0x578    ~ rest  Misc voicing, damper-pedal noise, pedal-down noise references
----------------------------------------------------------
Total: 1 626 bytes
```

The exact byte map is in the SGX-2 internal documentation Korg maintains for piano-model
authoring. The user-facing parameters (the ones in the Kronos UI under "Piano Type
Edit") are only a subset of what the file holds.

---

## How piano types are selected at runtime

1. A program built on SGX-2 has a `piano_type_index` field in its parameter record (in
   `PROG?.BIN`)
2. When the program loads, `CSTGPianoModelPatch::LoadPianoType(idx)` opens the
   corresponding `PianoType<NN>` file
3. The 1 626 bytes get parsed into the running `CSTGPianoModel` instance
4. Combined with `DKITI.BIN`'s noise-sample data, the model is ready to play

---

## Adding new piano types

If Korg releases an SGX-2 piano expansion (or you author your own piano types via
third-party tools), the new files drop into `/korg/rw/PRELOAD/PianoTypes/`. Eva
triggers `PT:` on `/proc/.oacmd` and OA.ko's `RescanPianoTypes` picks them up. No
hardcoded limit on the number of piano types — the loader iterates the directory
listing dynamically.

This is **one of the few places** in the Kronos PRELOAD design that uses a
"directory-enumerate" rather than a hardcoded file list — which makes piano types
trivially extensible compared to programs, combis, drum kits, etc. See
[extension_points.md](extension_points.md) for a comparison.

---

## See also

- [drum_kit_banks.md](drum_kit_banks.md) — `DKITI.BIN` holds piano noise samples
- [program_banks.md](program_banks.md) — SGX-2 programs reference piano types
- [extension_points.md](extension_points.md) — piano-types extensibility vs everything else
- `KRONOS_Op_Guide_E10.pdf`, "SGX-2" section — concept reference
- `KRONOS_Param_Guide_E10.pdf`, "SGX-2 EXi" chapter — parameter list
