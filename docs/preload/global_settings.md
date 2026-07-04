# Global Settings — `GLBL.BIN`

The single record that holds everything the Kronos remembers about *how it should
behave globally* — irrespective of which program, combi, or set list is currently
loaded.

| Property | Value |
|---|---|
| File | `GLBL.BIN` |
| File size | 24 728 bytes |
| Header magic | `PGLB` |
| Records | **1** (header `record_count = 0x01`) |
| Record size | **24 708 bytes** (header `record_size = 0x6084`) |
| Container | See [container_format.md](container_format.md) |

---

## What the 24 708-byte record holds

The Global parameters cover (per the Korg Operation Guide, "Global Mode" chapter):

| Section | Approx. size | What's in it |
|---|---|---|
| **System** | ~200 bytes | Master tune, transpose, footswitch polarity, damper polarity, audio output assignments |
| **MIDI** | ~500 bytes | MIDI channel assignments (Local control, Global channel, all 16 per-channel filters), routing matrix, SysEx device ID |
| **Effect Global** | ~200 bytes | MFX/TFX bypass defaults, IFX defaults |
| **Velocity / Aftertouch curves** | ~300 bytes | User curve definitions for velocity, AT, KARMA velocity |
| **User scales** | ~50 bytes × 16 user scales | 16 user-definable scale tunings |
| **User octave scales** | ~12 × 16 | Per-octave tuning offsets |
| **Foot-switch assigns** | ~80 bytes | Damper polarity, sustain mode, assignable switch behaviour |
| **Pad / Joystick assigns** | ~200 bytes | Default control routings |
| **PSU / Calibration** | ~100 bytes | Front-panel calibration |
| **Display / UI** | ~100 bytes | Brightness, contrast, language, page-preference |
| **Disk / SSD options** | ~50 bytes | Auto-load behaviour, file-browser defaults |
| **PRELOAD volumes** | ~200 bytes | Per-PCM-volume mounts and Auto-Load list (references `KSCLIST.BIN`) |
| ... | rest | Auto-Power-Off setting, KARMA-Module-Lock defaults, drum-track defaults, etc. |
| **Reserved / padding** | ~remainder | Filler to align to 24 708 bytes |

The Korg Parameter Guide (p. 530+, "Global Mode") has the byte-exact list. Most of it
maps 1-to-1 onto user-visible parameters in Global mode pages.

---

## Loader

`GLBL.BIN` is loaded by Eva at startup (via `CPreloadFile` infrastructure) and the
parsed parameters are pushed into OA.ko via `/proc/.oacmd`. The OA.ko side holds the
global state in `CSTGGlobal::sInstance` (a singleton — see
[../modules/OA.ko.md](../modules/OA.ko.md)).

When the user **WRITE**s in Global mode, Eva re-serialises the singleton state and
overwrites `GLBL.BIN`.

---

## Why it's one big record

There's no need to slot Globals into "banks" — every Kronos has exactly one Global
configuration. Korg made it a single-record file with the standard `PGLB` magic for
uniformity with the other PRELOAD files.

The 24 708-byte payload is mostly **fixed-position parameters** (each parameter at a
known offset), not a tag-value soup. That makes it simple to read but also means the
layout is permanently frozen at compile time — any new global parameter Korg adds in a
future OS would require a layout migration or a separate file.

---

## MIDI Filter flags (confirmed 2026-06-24)

The MIDI filter byte is at **record offset 24578, file offset 24598 (0x6016)**.

| Bit | Flag | Default |
|---|---|---|
| 0 | Program Change Enable | OFF |
| 1 | Bank Change Enable | OFF |
| 2 | Combi Change Enable | OFF |
| 3 | Aftertouch Enable | OFF |
| 4 | Control Change Enable | OFF |
| 5 | **SysEx Enable (Exclusive)** | OFF |
| 6 | Start/Stop Out Enable | OFF |

To enable all MIDI filters: set byte at file offset 24598 to `0x3F` (bits 0–5)
or `0x7F` (all 7 bits). **Must also update both header checksums** — see
[container_format.md](container_format.md) for the algorithm.

Confirmed by binary diff of two `GLBL.BIN` files captured with Exclusive ON vs OFF:
only file offset 24598 changed in the record data (0x20 vs 0x00, bit 5).

---

## Things you can extract directly from the file

You can `dd` out specific bytes if you know the offset. For example:

```bash
# Master Tune (Global > Basic > Master Tune) — 16-bit signed, big-endian
# offset ≈ 0x14 (first parameter after the 20-byte header)
xxd -s 0x14 -l 2 GLBL.BIN
```

(The exact offsets aren't documented here byte-for-byte; consult Parameter Guide
section "Global P0: Basic Setup" for the field list, then add 20 for the file header.)

---

## See also

- [container_format.md](container_format.md)
- [misc_files.md](misc_files.md) — `KSCLIST.BIN` (referenced from Global's auto-load section)
- `KRONOS_Op_Guide_E10.pdf`, ch. on Global mode — concept reference
- `KRONOS_Param_Guide_E10.pdf`, p. 530+ — exhaustive parameter list
