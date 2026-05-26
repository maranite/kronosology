# Combi Banks — `COMB*.BIN`

A **Combination** ("Combi") is a multi-program performance: up to 16 *timbres*, each
referencing a program from any bank, with independent key range, velocity range,
transpose, output routing, and per-timbre KARMA module assignment. Combis are how the
Kronos creates splits, layers, drum-and-bass-layered-with-pad, and complex performances.

| Property | Value |
|---|---|
| File naming | `COMB<A..N>.BIN` — 14 files, banks I-A through I-N |
| User combi banks | **none in PRELOAD** — user combis are written to the same A..N files (no separate `COMBAA..GG`) |
| Total banks | **14** (hardcoded — see [extension_points.md](extension_points.md)) |
| File size | 999 700 bytes per bank |
| Header magic | `PCMB` ("**P**reload **C**o**MB**ination") |
| Combis per bank | **128** (header `record_count = 0x80`) |
| Record size | **7 810 bytes** per combi (header `record_size = 0x1E82`) |
| Container | See [container_format.md](container_format.md) |
| Total combi slots | 14 × 128 = **1 792** |

---

## Why combis are bigger than programs

A program is one engine instance (~4 960 bytes). A combi must hold:

- 16 timbres, each with: target program (bank + index), key zone, velocity zone, MIDI
  channel, KARMA module, transpose, send levels — roughly 350–400 bytes per timbre
- Common parameters: name, category, tempo, KARMA-global settings
- A full effect-routing matrix (IFX × 12, MFX × 2, TFX × 2 — all the bypass / send
  matrix that programs each have just one of)

7 810 bytes per combi works out to roughly: 24 (name) + ~100 (common) + 16 × 350
(timbres) + ~2 100 (effect routing) + padding. The exact field map is in the Korg
Parameter Guide PDF.

---

## Loader

Combi banks are loaded by the **second half** of `CSTGGlobal::InitializePerformances`
(after the 23-bank program loop ends). The pattern mirrors the program loader:

```c
for (combi_bank = 0; combi_bank < 0xE; combi_bank++) {     // 0x0E = 14 banks
    char buf[12];
    snprintf(buf, 11, "COMB%c.BIN", 'A' + combi_bank);
    CKorgCombiBankFile bank(buf);                           // similar shape to ProgBankFile
    int rc = bank.Load();
    // ... per-combi initialisation, identical pattern to program ...
}
```

The loop bound (`0xE` = 14) is what limits combi banks to I-A through I-N. Doubled-letter
combi banks (`COMBAA.BIN` etc.) would simply be ignored even if present, because the
loader's `snprintf` template is `"COMB%c.BIN"` only — see
[extension_points.md](extension_points.md).

The in-memory home for each combi bank is `CSTGCombiBank` (the sibling of
`CSTGProgramBank`). Bank stride in CSTGGlobal's data area is roughly 0xCF381 = 849 793
bytes per combi bank (visible in the inner loop of InitializePerformances).

---

## Per-record layout (7 810 bytes per combi)

```
Offset       Size                     Field
-----------------------------------------------------------------
0x000        24                       Combi name (ASCII, null-padded)
0x018         4                       Category / sub-category
0x01C       ~100                      Common: tempo, KARMA global, scale, etc.
0x080      16 × 350-ish               Timbre 1..16 — each with:
                                          • target bank (1 byte)
                                          • target program (1 byte)
                                          • MIDI channel (1 byte)
                                          • status (INT/OFF/EX2/etc.)
                                          • key range (low/high)
                                          • velocity range (low/high)
                                          • transpose (semitones, cents)
                                          • KARMA module (A..D / OFF)
                                          • send levels, output bus
0x1A50    ~2 200                       Effect routing + IFX/MFX/TFX bypass
0x1E00       ~30                       Padding / reserved
-----------------------------------------------------------------
Total: 7 810 bytes
```

Per-timbre size varies slightly because of zone-curve tables; the layout above is
approximate. The Korg Parameter Guide PDF (p. 130+ in the English edition) is the
authoritative reference.

---

## Save path

When the user presses **WRITE** in Combi mode, Eva calls into OA.ko via `/proc/.oacmd`
to:

1. Stop the audio engine briefly
2. Take the in-memory `CSTGCombi` for the current edit buffer
3. Apply the inverse of the boot-time byte-swap
4. Write back to `COMB<bank>.BIN` at the correct record offset
5. Update an in-RAM "dirty bit" so the boot loader doesn't need to re-scan

The user can also write a full bank via the **Disk → Save PCG** flow, which copies the
relevant slice of the file to the PCG output. The internal `CPcgSaveInfo`
infrastructure in Eva (`CPcgSaveInfoCombi`, `EPcgCombiBank`) handles the format
translation.

---

## Why there are no user combi banks in PRELOAD

The Kronos's UI does **not** distinguish "internal" vs "user" combi banks the way it
does for programs. All 14 combi banks (I-A through I-N) are user-writable. Korg ships
banks I-A and I-B with content and the rest empty; you fill them yourself. That's why
the PRELOAD directory only contains 14 `COMB*.BIN` files — there's no `COMBAA.BIN` etc.

Compare to programs, where Korg explicitly separates "factory-write-protected" banks
(I-A..I-P that ship with EXi instruments and HD-1 content) from "always-user-writable"
banks (USER-A..USER-G = `PROGAA..PROGGG.BIN`).

---

## See also

- [container_format.md](container_format.md)
- [program_banks.md](program_banks.md) — combis reference programs by (bank, index)
- [extension_points.md](extension_points.md) — adding banks O, P, … or AA, BB, …
- [../modules/OA.ko.md](../modules/OA.ko.md) — host of the bank loader
- `KRONOS_Param_Guide_E10.pdf`, p. 130–200 — combi parameter reference
