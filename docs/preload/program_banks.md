# Program Banks — `PROG*.BIN`

The Kronos's **Program** mode stores a single instrument patch per slot: oscillator(s),
filter, amp envelope, LFO, effects bypass settings, and the per-program effect routing.
Programs are organised into **banks**, and each bank file lives in `/korg/rw/PRELOAD/`.

| Property | Value |
|---|---|
| File naming (internal banks) | `PROG<A..P>.BIN` — 16 files, banks I-A through I-P |
| File naming (user banks) | `PROG<AA..GG>.BIN` — 7 files, banks USER-A through USER-G |
| Total banks possible | **23** (hardcoded — see [extension_points.md](extension_points.md)) |
| File size | 634 900 bytes (every bank) |
| Header magic | `PMOS` (MOSS/EXi physical-model programs) **or** `PPCM` (HD-1 PCM programs) |
| Programs per bank | **128** (header `record_count = 0x80`) |
| Record size | **4 960 bytes** per program (header `record_size = 0x1360`) |
| Container | See [container_format.md](container_format.md) |

---

## Per-file magic-and-bank mapping (observed)

| Bank file | Magic | Bank in UI | Engine |
|---|---|---|---|
| `PROGA.BIN` | `PMOS` | I-A | EXi (factory) |
| `PROGB.BIN` | `PPCM` | I-B | HD-1 PCM (factory) |
| `PROGC.BIN` | `PMOS` | I-C | EXi (factory) |
| `PROGD.BIN` | `PMOS` | I-D | EXi (factory) |
| `PROGE.BIN` | `PMOS` | I-E | EXi (factory) |
| `PROGF.BIN` | `PMOS` | I-F | EXi (factory) |
| `PROGG.BIN` | `PMOS` | I-G | EXi (factory) |
| `PROGH.BIN` | `PMOS` | I-H | EXi (factory) |
| `PROGI..PROGP.BIN` | mostly `PMOS` | I-I..I-P | EXi (factory) |
| `PROGAA.BIN`..`PROGGG.BIN` | `PPCM` | USER-A..USER-G | HD-1 PCM (user) |

The magic in the header determines what **engine** OA.ko expects in each record's
parameter blob. PMOS records hold EXi-engine parameters; PPCM records hold HD-1 PCM
engine parameters. The two record layouts differ internally, but their *total* size is
the same (4 960 bytes) — Korg pads both to fit the same slot layout, which is what
lets the same loader handle either kind.

---

## Loader

`CSTGGlobal::InitializePerformances` (`0x00006770` in OA.ko) walks all 23 program banks
on engine start-up:

```c
for (iVar7 = 0; iVar7 < 0x17; iVar7++) {            // 0x17 = 23 banks total
    char buf[12];
    if (iVar7 < 0x10) {                              // 0x10 = 16 single-letter banks
        snprintf(buf, 11, "PROG%c.BIN", 'A' + iVar7);
    } else {                                          // 7 double-letter user banks
        char l = 'A' + (iVar7 - 16);
        snprintf(buf, 11, "PROG%c%c.BIN", l, l);
    }
    CKorgProgBankFile bank(buf);                      // ctor builds "/korg/rw/PRELOAD/" + buf
    int rc = bank.Load();
    if (rc == 0) {
        kBankInfo[iVar7 * 2] = bank.bank_type;        // stash PMOS vs PPCM type
        // also flips a bit in STGAPIFrontPanelStatus → 0x294f8 marking the bank loaded
    } else {
        // bit flipped off → bank shows as "empty" in front panel UI
    }
    CSTGProgramBank::Initialize(<per-bank memory area>, iVar7, type, …);
    // memory stride: 0x67603 bytes per bank slot in the giant CSTGGlobal data area
}
// Then a second nested loop runs 14 times for combi banks — see combi_banks.md
```

`CKorgProgBankFile` is a thin subclass of `CKorgPreloadFile` (see
[../modules/OA.ko.md](../modules/OA.ko.md#crypto-primitives-present) for the address
table). The base class handles open / read / size-check / byte-swap; the subclass adds
the bank-type field (`PMOS` vs `PPCM`) detection.

---

## Per-record layout (4 960 bytes per program)

The exact field-by-field layout is documented in **Korg's KRONOS Parameter Guide**, but
the broad structure is:

```
Offset      Size       Field
-------------------------------------------------------------
0x000       24         Program name (ASCII, null-padded)
0x018        4         Category/sub-category code
0x01C       ...        Common parameters (volume, key zones, KARMA assignments, …)
…           …          Engine-specific parameter block (varies by PMOS/PPCM)
0x1340     ~32         Effect routing / IFX/MFX/TFX bypass flags
0x135C        4        Padding / reserved
-------------------------------------------------------------
Total: 4960 bytes
```

We have not field-mapped every byte by hand — Korg ships the Parameter Guide PDF
(`KRONOS_Param_Guide_E10.pdf`) for the user-visible parameters, and Eva's `CForm*`
classes do the user-edit-to-bytes binding. For RE purposes, the meaningful entry
points in OA.ko are:

| Function | Address | Purpose |
|---|---|---|
| `CKorgPreloadFile::Load` (in vtable) | called from `0x00006879` | Reads the file, validates, calls per-record byte-swap |
| `CSTGProgramBank::Initialize` | called per bank | Builds in-memory `CSTGProgramBank` from raw record data |
| `CSTGProgram::Initialize` | called per program | Unpacks one record into the running `CSTGProgram` object |

Eva's writer side uses `CFormDlogGlobalDrumKitWrite`, `CFormDlogProgramWrite`, and the
`CPcgSaveInfo` family to serialise back. `CPcgSaveInfo::ClearSaveCombiBankInfo` and
`EPcgCombiBank` show the same shape on the program side.

---

## What the UI lets you do with these

| UI flow | Touches |
|---|---|
| **WRITE** a program to a slot | The program record at `(bank_index, prog_index)` |
| **Bank Edit → Initialize Bank** | All 128 records of a single bank |
| **Disk → Save PCG** | Reads from PRELOAD into a `.PCG` file (Korg's portable format) |
| **Disk → Load PCG** | Inverse — replaces some/all PRELOAD bytes from `.PCG` |
| **Reset → Load Factory Programs** | Restores the PRELOAD files from the FACTORY image |

The on-disk `PROG*.BIN` file is rewritten **only** on the user's WRITE action (or a
PCG load). It is not touched on every Program-mode parameter edit — those changes
live in the engine's running RAM until WRITE.

---

## See also

- [container_format.md](container_format.md) — the 20-byte header these files share
- [combi_banks.md](combi_banks.md) — the next-step container (combis reference programs)
- [extension_points.md](extension_points.md) — adding a 24th, 25th, … bank
- [../modules/OA.ko.md](../modules/OA.ko.md) — host of the bank loader
- `KRONOS_Param_Guide_E10.pdf`, p. 25–110 — the user-visible parameter list
