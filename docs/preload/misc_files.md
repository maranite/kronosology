# Miscellaneous PRELOAD Files

The small, single-purpose files in `/korg/rw/PRELOAD/` that hold one specific piece of
device state — not banks, not big libraries, just one settings record each.

---

## `KSCLIST.BIN` (240 bytes) — KSC Auto-Load List

Plain ASCII (with `\r\n` Windows-style line endings) describing which KSC
("Korg Sample Collection") files to auto-load when the device boots. Loaded by Eva
into the Global mode's "PCM Auto Load" page.

Observed contents:

```
#KORG Auto Load List Version 1.0
#KSC
Generic
IDE HDD1                                                            
\x00\x01
\x00\x02
\x00\x02
\x1a\x00A:\FACTORY old\PRELOAD.KSC
```

Format:

- Lines starting with `#` are headers/comments
- A volume label, then a series of `\x00\x0N` "include" markers
- The last line is the file path to load (`A:\` is the legacy DOS-letter prefix Korg
  uses internally — at runtime it resolves to `/korg/ftp/SSD1/` or similar)

Used by OA.ko's `CSTGPCMPrecacheManager` (via Eva → `/proc/.oacmd PC:`) to choose
which PCM data to keep cached across reboots.

---

## `FANCTRL.BIN` (16 bytes) — Fan Controller Settings

Exactly 16 bytes:

```
00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 02
```

Likely: 2-byte fields for fan threshold, duty cycle, min RPM, max RPM, etc. (totalling
8 × u16 = 16 bytes). The Kronos has a small CPU fan plus the SSD-area airflow; this
file lets users (or service techs) tweak the fan behaviour to balance noise vs cooling.

Reader: the on-board OMAP-side fan controller driver — not OA.ko, not Eva. Likely a
small kernel module or systemd-equivalent boot script reads it.

---

## `PONMEM.BIN` (16 bytes) — Power-On Memory

Exactly 16 bytes:

```
05 5c 00 00 00 00 00 00 00 00 00 00 00 00 00 61
```

Stores what the device should restore on the next power-on:

- `05 5c` — last program / combi / set-list slot (0x5C = 92)
- last byte `0x61` — last mode (Program = `0x60`, Combi = `0x61`, Sequencer = `0x62`,
  Set List = `0x63`, etc. — based on Eva's mode enum)
- Middle bytes — flags (autoload-last-state, autoplay, etc.)

Updated by Eva every time the user changes mode or selects a new program/combi.

---

## `SFCCMAP.BIN` (80 bytes) — Switch & Fader CC Map

Exactly 80 bytes, structured as 40 × 2-byte entries:

```
10 ff   10 ff   10 ff   10 ff   10 ff   …(× 39)…   01 4a
```

Each entry pairs a CC#/state byte with a value (mostly `0x10 0xFF` defaults). The 40
entries map onto the Kronos's assignable hardware controls:

- 16 sliders
- 16 knobs (8 on top, 8 on bottom)
- 8 buttons (4 KARMA scene + 4 mod)

For each control, the file stores the *default* MIDI CC# emitted when the control isn't
overridden by the current program/combi/setlist. The last `0x01 0x4A` entry is the
default for the joystick or ribbon controller.

Loaded by Eva at boot and pushed to OA.ko's `CSTGControllerInfo` runtime.

---

## `FILESORT.BIN` (2 bytes) — File-Browser Sort Preference

The two bytes:

```
01 01
```

— a per-bit flag set for the Disk page's file-browser sort preferences:

- Sort by Name / Date / Size / Type
- Ascending / Descending

Read by Eva when opening the Disk page; written when the user picks a different sort
order in the file browser.

---

## Summary table

| File | Size | Purpose | Reader |
|---|---|---|---|
| `KSCLIST.BIN` | 240 | KSC auto-load list (plain ASCII) | Eva → OA.ko (`PC:`) |
| `FANCTRL.BIN` | 16 | Fan controller settings | Boot script / kernel driver |
| `PONMEM.BIN` | 16 | Power-on restore state | Eva |
| `SFCCMAP.BIN` | 80 | Default switch/fader CC# mappings | Eva → OA.ko |
| `FILESORT.BIN` | 2 | Disk-browser sort preference | Eva |

---

## See also

- [global_settings.md](global_settings.md) — many global parameters that *could* live here but instead live in `GLBL.BIN`
- [../interfaces/proc_oacmd.md](../interfaces/proc_oacmd.md) — `PC:` and `PT:` commands relevant to KSC loading
