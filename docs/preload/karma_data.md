# KARMA Data — `GE.BIN`, `GE.KDF`, `KGEUA.BIN`

KARMA (Korg's Kinetic Arpeggiator / Real-time Manipulation Architecture) is the
generative-music engine that produces phrases, arpeggios, drum patterns, and
chord-strums from a small set of input notes plus a *Generated Effect* (**GE**) — a
multi-parameter algorithm authored by Stephen Kay (KARMA's inventor) and Korg.

| File | Role | Size |
|---|---|---|
| `GE.BIN` | Loaded form of all factory GEs (compact runtime format) | 3 065 760 |
| `GE.KDF` | Korg Data File — the full factory GE library + KARMA template banks (master source) | 7 027 396 |
| `KGEUA.BIN` | **K**arma **GE** **U**ser bank **A** — user-edited / imported GEs | 325 121 |

None of these files use the universal `P***` container; KARMA has its own format
defined by the KARMA SDK.

---

## `GE.KDF` — Korg Data File (master)

Eva's strings reveal the structure:

```
NumOfGEs in GE.KDF %ld
NumOfTemplateBanks in GE.KDF %ld
```

— so `GE.KDF` contains:

1. A header with counts of GEs and template banks
2. The full library of factory **Generated Effects** (~2 000 of them per the Kronos
   manual — the exact count is in the header)
3. Several **Template Banks** — pre-baked KARMA performance setups (combinations of
   GEs + Random Seeds + Module assignments) that the user can load as a starting point

Header (first 16 bytes of GE.KDF observed):
```
00000000: 02 09 12 14 02 00 00 07 00 6b 3a c4 00 00 00 00
```

Likely:
- `02 09 12 14` — magic / version bytes
- `02 00 00 07` — flag word (big-endian?)
- `00 6b 3a c4` — total size or count (big-endian 0x006B3AC4 = 7 027 396 = file size!)

So byte offsets 8..11 hold the total file size. The rest of the header (and subsequent
records) follows the proprietary KDF format documented internally by Korg.

---

## `GE.BIN` — runtime-ready GE pack

A compacted, byte-swapped, internal-format version of the same data that the OA.ko
audio thread can read at audio rate without further parsing. Header (first 16 bytes):

```
00000000: a6 04 00 00 01 00 00 00 00 00 00 00 00 00 00 00
00000010: 41 72 70 20 4d 6f 64 65 ...   "Arp Mode..."
```

The first GE's name "Arp Mode..." starts at offset 0x10. Pattern: each GE record is
prefixed by a fixed-size header (maybe 16 or 32 bytes) and followed by a parameter
blob. KARMA SDK's per-GE size is variable.

---

## `KGEUA.BIN` — User KARMA GE bank

A single user-editable GE bank. Starts directly with the first GE's name:

```
00000000: 41 72 70 20 4d 6f 64 65 6c 20 32 33 20 54 72 69
00000010: 70 6c 65 74 73 00 00 00 00 00 00 00 00 00 00 00
                                                  "Arp Model 23 Triplets"
```

— so user GEs follow the same per-record layout as the factory ones, just with a
different bank index assigned at load time. The single user bank holds ~64 user GEs (the
exact capacity matches what's documented in the Kronos manual's KARMA chapter).

---

## Loader

KARMA data is loaded by Eva at boot via the `K:\PRELOAD\GE.KDF` and `K:\PRELOAD\%s.BIN`
path templates. Eva then feeds the parsed GEs to OA.ko's `CSTGKarmaPerf*` /
`CKarmaPerf1Module` runtime via `/proc/.oacmd`. OA.ko has the runtime side — it can
execute GEs (run the algorithm) but doesn't itself parse the file formats.

Source paths leaked from Eva's strings: `SaveUserGEToPreloadFile`,
`SaveUserKarmaTemplateToPreloadFile` — these are the writers.

---

## Why two files for the factory library (GE.KDF + GE.BIN)?

Common pattern in embedded music gear: the **master source** (`.KDF`) is the canonical
file Korg maintains for distribution and updates. At first boot (or after a factory
reset), an OS routine "compiles" the master file into a **runtime cache** (`.BIN`)
that's faster to memory-map. If the cache exists, the device skips the recompilation.

A factory reset, OS reinstall, or `rm GE.BIN` causes recompilation on next boot —
that's the recovery path if `GE.BIN` ever becomes corrupted.

---

## A KARMA GE itself — what is it?

A GE is a small program written in Stephen Kay's KARMA language. It has:

- **Phase** (chord, arpeggio, drum, dual, …)
- **Pattern Cell array** — the step-by-step note-generation logic
- **Random Seed** — for procedural variation
- **CC# mappings** — how the 8 KARMA sliders modulate the algorithm
- **Note grid template** — what scale/key/octave to play within

When KARMA is engaged in a program or combi, the assigned GE is executed against the
incoming MIDI note(s); the output is a stream of generated MIDI events sent to the
program's oscillators (or to other timbres in a combi).

A program's reference to its GE is by `(bank, index)`. The bank can be `Factory`
(referring to GE.BIN/KDF) or `User` (referring to KGEUA.BIN). Combis can use up to 4
GEs simultaneously, one per KARMA module (A, B, C, D).

---

## See also

- [combi_banks.md](combi_banks.md) — combis assign GEs to KARMA modules
- [program_banks.md](program_banks.md) — programs can use one GE
- [drum_track_patterns.md](drum_track_patterns.md) — alternative rhythm source (pre-recorded vs generative)
- `KRONOS_Op_Guide_E10.pdf`, "KARMA" chapter — concept reference
- `KRONOS_Param_Guide_E10.pdf`, p. 700+ — KARMA parameter reference
- Korg's *KARMA Architectural Reference* (third-party doc, not bundled with the Kronos) —
  the definitive source for GE authoring
