# Drum-Track Patterns — `PPAT.BIN`, `DPAT.BIN`

The Kronos's **Drum Track** is a built-in rhythm pattern player that runs in Program,
Combi, and Sequencer modes alongside KARMA. It plays one *pattern* per program/combi —
either selected from a factory library or a user-recorded pattern.

| File | Role | Size | Format |
|---|---|---|---|
| `PPAT.BIN` | **Preset** patterns (factory rhythm library) | 1 346 361 | Custom (no `P***` magic) |
| `DPAT.BIN` | User-recorded **drum** patterns | 672 025 | Custom (no `P***` magic) |

These two files do **not** use the universal `P***` container — they have their own
header + name-table + variable-length pattern-data layout.

---

## File header (both files)

Both files start with a 24-byte header of six little-endian `u32`s:

```
Offset  Size    Field                           Example (PPAT.BIN)        (DPAT.BIN)
-------------------------------------------------------------------------------------
0x00    4       Version / flags                 0x000002cf (719?)         0x00000001
0x04    4       Total file size or index size   0x00020E93 (134803)       0x0000006c (108?)
0x08    4       Pattern count                   0x00000337 (823)          0x000003e8 (1000)
0x0C    4       Data table offset               0x00028488 (165000)       0x00013880 (80000)
0x10    4       Per-entry name size             0x00000018 (24)           0x00000018 (24)
0x14    4       ??? (per-entry data offset?)    0x000066f8 (26360)        0x00007d18 (32024)
-------------------------------------------------------------------------------------
```

Then comes a **name table** of `pattern_count` entries, each 24 bytes (name + null
pad) + 4 bytes (flags) + 4 bytes (pointer/offset into the data section). Then the
actual pattern data (variable-length per pattern).

### `PPAT.BIN` — 823 preset patterns

The first preset patterns observed in the dump are named like:

```
Pop & Ballad 1 [All]
Pop & Ballad 2 [All]
Pop & Ballad 3 [All]
Pop & Ballad 4 [All]
…
```

— rhythm names organised by genre, with `[All]` / `[Variation N]` / `[Fill]` suffixes.

Korg's Operation Guide lists ~825 preset Drum Track patterns, which matches our count
of 823 entries (the small discrepancy could be 2 reserved/system slots).

### `DPAT.BIN` — 1000 user patterns

The user file ships with all 1 000 slots set to `"Unused Pattern "` with no data
attached (the data-offset field reads `0xFFFFFFFF` for unused slots). Users can record
their own drum patterns via the Sequencer mode → Drum Track Pattern recording flow,
and those get saved here.

---

## Per-pattern data layout

Each pattern is a sequence of MIDI-like events with note-on / note-off pairs, velocity,
timestamp, plus a header containing tempo, time-signature, bar count, and length. The
exact event encoding is the Korg sequencer-event format (similar to MIDI Standard MIDI
File chunks but with Korg's extension byte for KARMA events and drum-mute events).

Field map for each pattern data record (approximate):

```
Offset       Size       Field
---------------------------------------------------------------
0x00         24         Pattern name (duplicate of name table)
0x18         4          Length in ticks
0x1C         2          Time signature numerator
0x1E         2          Time signature denominator
0x20         4          Tempo (BPM × 100 — int)
0x24         var        MIDI event list (note on, note off, velocity, channel)
...          ...        End-of-pattern marker
```

---

## Loader

Drum-track patterns are loaded by Eva. The OA.ko side (`CSTGDrumTrackBankManager`,
`CSTGDrumTrackPatternDataHolder`) holds the parsed patterns in memory and provides the
sequencer with timing/note data on demand.

Both files are mmap'd or read-fully into a `CSTGDrumTrackPatternDataHolder` (one for
each file — preset vs user). The on-disk format and in-memory format are nearly
identical, so deserialisation is mostly pointer fix-up.

---

## Save path

When the user records a pattern via Sequencer → Drum Track Pattern (record mode):

1. The sequencer captures MIDI events into a temp buffer
2. On WRITE, Eva serialises the buffer into the next-free `DPAT.BIN` slot
3. The slot's data-offset field gets updated to point at the new data
4. The name string gets written into the name table

`PPAT.BIN` is **read-only** under normal user operation — the factory presets cannot
be overwritten via the UI. (You could `cp` over the file from a shell, but the UI
won't let you.)

---

## See also

- [combi_banks.md](combi_banks.md) — combis can reference a drum-track pattern by index
- [karma_data.md](karma_data.md) — KARMA generates patterns dynamically; drum-track patterns are pre-recorded alternatives
- `KRONOS_Op_Guide_E10.pdf`, "Drum Track" chapter — concept reference
- `KRONOS_Param_Guide_E10.pdf`, p. 580+ — drum-track parameter reference
