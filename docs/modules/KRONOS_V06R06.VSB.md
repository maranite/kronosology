# KRONOS_V06R06.VSB — NKS4 Panel Sub-System Firmware

Source: `Decomp/subsystem/KRONOS_V06R06.VSB` (917 760 bytes, from the official
`KRONOS_Updater.exe` v06r06 Windows updater package). This is **not** code that runs
on the Kronos's x86 Atom/Celeron host — it is the firmware for the physically
separate front-panel controller board (USB `0944:1005`), the thing
[`OmapNKS4Module.ko.md`](OmapNKS4Module.ko.md) talks to and that has historically
been treated as an opaque "chip." It turns out to be a full standalone embedded
system with its own CPU, LCD controller, touch panel, PSoC scan chip, and (per one
referenced source file) its own path to the AT88 crypto chip. This doc records the
container format and what was learned from a raw disassembly pass; no Korg binary
or copyrighted art asset is reproduced here — only factual structure, offsets, and
short UI/debug strings needed for interop, in keeping with this repo's existing
documentation style (see e.g. [`OmapNKS4Module.ko_chip_wedge.md`](OmapNKS4Module.ko_chip_wedge.md)
quoting literal dmesg text).

---

## Container format ("KORG SYSTEM FILE")

256-byte header followed by a raw payload:

| Offset | Size | Content |
|---|---|---|
| `0x00` | 16 | `"KORG SYSTEM FILE"` magic (NUL-padded) |
| `0x10` | 16 | `"KRONOS"` product tag (NUL-padded) |
| `0x20` | 8 | `"Sub-sys"` file-type tag (NUL-padded) |
| `0x28` | 4 | `00 01 06 06` — subtype `0001`, version `06.06` → matches filename `V06R06` |
| `0x2c` | 4 | `80 02 03 ff` — unidentified (flags/checksum-seed?) |
| `0x30` | 4 | `00 00 00 00` — reserved |
| `0x34` | 4 | `00 00 0e 00` (LE `0x000e0000` = 917 504) — **payload length**, exactly `file_size - 256` |
| `0x38` | 4 | `00 00 00 00` — reserved |
| `0x3c` | 4 | `00 00 0e 00` — payload length repeated (redundancy/checksum companion?) |
| `0x40` | 2 | `02 00` (LE `2`) — unidentified, possibly chunk/section count |
| `0x42..0xff` | — | `0xff` padding to end of header |
| `0x100` | 917 504 | Raw ARM firmware image (see below) |

No compression signature (gzip/xz/lz4/squashfs/uImage) appears anywhere in the
payload — it's a flat, uncompressed, statically-linked image.

---

## Target hardware

String evidence (`../MCU/OmapL108.cpp`, `../MCU/Component/OmapL137Mcasp.cpp`,
`../MCU/Component/OmapL108Spi.cpp`, `../MCU/Component/OmapL137Usbdc.cpp`) identifies
the CPU as a **TI OMAP-L1x** (DaVinci-class ARM926EJ-S + C674x DSP SoC, OMAP-L108/L137
family — the McASP audio serial port and USB device controller drivers are TI-specific
peripheral names unique to that family). Boot strings distinguish two board revisions:

```
Running on barack board...
Running on Proto2...
```

(consistently spelled "barack", not "barrack", throughout the strings table).

The firmware links flat at **`0xC0000000`** — confirmed by the ARM exception vector
table at payload offset `0x000` (`0xE59FF018` = `LDR PC,[PC,#0x18]` ×5, i.e. the
classic all-vectors-branch-through-a-literal-pool layout) whose literal pool at
`+0x20` holds the reset-handler target `0xC0009534`, plus ~2000 other `0xC000xxxx`–
`0xC01dxxxx` pointer-looking words scattered through the image, none below
`0xC0000000`. `0xC0000000` is TI OMAP-L1x's standard DDR2 EMIF base address — this
firmware is loaded straight into the SoC's own RAM, not relocated/PIC.

Reconstructing a loadable image for Ghidra: wrap the 917 504-byte payload (file
offset `0x100..EOF`) in a minimal single-`PT_LOAD` ELF32 ARM header (`e_machine=40`,
entry = vaddr = `0xC0000000`) — Ghidra's ELF importer then auto-selects
`ARM:LE:32:v8` and places everything at the right addresses with no manual language/
base-address wizardry needed. (The raw-binary importer path failed silently through
the Ghidra MCP bridge; the ELF-wrapper trick sidesteps that.)

---

## Firmware source-file inventory (from embedded `__FILE__`-style strings)

```
../McAspHandler.cpp          - audio serial port (McASP) driver
../CryptoAt88.cpp            - talks to the AT88 crypto chip directly (see note below)
../I2cByGpio.cpp             - bit-banged I2C over GPIO
../MCU/OmapL108.cpp          - SoC bring-up
../MCU/Component/OmapL137Mcasp.cpp
../MCU/Component/OmapL108Spi.cpp
../MCU/Component/OmapL137Usbdc.cpp
../EvaBoardMain.cpp          - main entry / board bring-up ("Eva" = same name as the
                                host-side GUI app studied in modules/Eva.md — the
                                panel board and the host GUI share a codename)
../cobjectmgr.cpp            - object manager
../CDix4192.cpp              - AKM/DIX-style digital audio interface chip driver
../cpsoc.cpp                 - PSoC (Cypress Programmable SoC) button/LED scan-chip driver
../ctouchpanel.cpp           - resistive/capacitive touch panel driver
../clcdc.cpp                 - LCD controller driver
../cad.cpp                   - A/D converter driver (knobs, sliders, pedals — the
                                analog controls)
```

**`../CryptoAt88.cpp` — followed up. The panel board has its own direct AT88
command path, independent of the host's `OA.ko`/`OmapNKS4Module.ko`/
`GetPubIdMod.ko`.** The single xref to the filename string is an assertion inside
`FUN_c0001028`, a self-test routine that:

1. `$B4` zone-select → **zone 0** (`{0xb4, 0x03, 0x00, 0x00}`)
2. `$B0` write of a 16-byte known pattern (`0,1,2,…,15`) to address 0
3. `$B2` read of 16 bytes back from address 0
4. Byte-by-byte compare; any mismatch calls `FUN_c000919c(0, "../CryptoAt88.cpp", 0xbd)`

Traced down to the byte level (`FUN_c0000ef4`/`FUN_c0000f30` → `FUN_c00016e8`/
`FUN_c0001638` → `FUN_c0001588` building the actual I2C-bit-banged header —
`I2cByGpio.cpp`-style start/ack/stop framing throughout), the on-wire command
format is **exactly** `{cmd, arg1, arg2, length}` — the same 4-byte framing this
repo's own [`atmel_nv2ac.md`](../crypto/atmel_nv2ac.md) and the top-level
`CLAUDE.md` document for the host-side protocol (`{0xb4, 0x03, zone, 0x00}` for
zone-select, `{0xb2, 0x00, addr, len}` for read). Same protocol, same opcodes,
issued from a second, independent master.

**Open question, not resolved:** zone 0 is documented (`CLAUDE.md`'s AT88SC
summary) as `AR0=0xd5` crypto-auth mode, requiring a `$B8` handshake before reads
decrypt correctly — but no `$B8` call appears anywhere in `FUN_c0001028` itself,
and Ghidra found **no static callers of `FUN_c0001028` at all** (likely invoked
indirectly, e.g. through a POST/self-test dispatch table) — so it's not established
whether this runs on every boot or only in a factory-test build, or whether a `$B8`
handshake happens earlier in `EvaBoardMain`'s bring-up before this test executes.
Either way, `FUN_c000919c` (the assert handler) is a **hard halt**: it draws an
error screen and then loops forever (`do {} while(true)`) — this is not a soft
warning, it's the same code path behind the `"SYSTEM STARTUP FAILED"` string.

`FUN_c0005e9c` (a second caller of the same write/read primitives) is a generic
queued-command relay dispatching on the command byte's LSB (odd/even) rather than
the fixed `$B0`/`$B2`/`$B4`/`$B8` opcode set — meaning the panel firmware's AT88
command surface may be broader than the four opcodes ground-truthed here. Not
chased further.

**Compared against `kronosology/reconstructed/AT88VirtualChip`** (the project's
software AT88 emulator, built for host-side `OA.ko`/`loadmod.ko` VM boot-testing):
this self-test would have failed outright against it — `AT88VirtualChip` had no
`$B0` (write) opcode at all (silent no-op on any unrecognized `stgNV2AC_sync_cmd`
opcode), and its `$B2` read was hardcoded to always mean the DEAX-encrypted zone-0
path regardless of session state. **Fixed 2026-07-16** (same day): `$B0` now
writes raw bytes into `zone0[]`, and `$B2` gates on `chip->b8RoundsAccepted` —
raw passthrough pre-auth (which is what makes this exact self-test's write-then-
read round-trip correctly now), DEAX-encrypted post-auth as before, preserving
every existing OA.ko-facing behavior. Full writeup and new KAT coverage
reproducing this self-test end to end:
[`AT88VirtualChip/README.md`](../../reconstructed/AT88VirtualChip/README.md) Open
Item #5, cross-linked from [`KronosNKS4/docs/gaps.md`](../../../KronosNKS4/docs/gaps.md).
Still open there: `$B2`'s dispatch remains unconditional on zone 0 regardless of
what `$B4` selected — the panel firmware's zone-select call is explicit and
structural, not vestigial, so this is a known simplification, not a fix of the
"only zone 0 is ever emulated" scope itself.

**Do not** attempt live `$B8` probing against real hardware based on any of this;
the chip lockout risk documented in memory (`at88_b8_lockout_safety`) applies
regardless of which side initiates it.

---

## Two-tier control system: Main + Panel Scan

Boot/status strings reveal a two-microcontroller architecture on the panel board
itself:

```
Main System Version:%02d Revision:%02d
Panel Scan System Version:%02d Revision:%02d
Psoc version error %02x != %02x : Id %03d
Updating the panel scan system...
Completed!
Cannot update it.
```

The OMAP-L1x ("Main System") is the one this VSB image is for for; it in turn talks
to a **PSoC** ("Panel Scan System") over what `cpsoc.cpp`/`I2cByGpio.cpp` suggest is
bit-banged I2C, and can itself field-update the PSoC's firmware (`Fail to send data
to psoc : %d` in `cpsoc.cpp`). The PSoC is the thing actually scanning the button/
key matrix and driving LEDs; the OMAP-L1x is the thing driving the LCD/touch panel
and talking USB back to the host.

---

## Factory self-test menu — the authoritative switch/LED name table

`FUN_c0008618` (xrefs to the two format strings below) implements a hidden
diagnostic screen: an up/down-scrollable list, index `0..0x48` (0-72, 73 entries),
printing:

```
Switch : %15s
LED    : %15s
```

for the currently-selected index, with `FUN_c0012750`/`FUN_c00127e0` reading live
switch/LED hardware state for that index (`I2C`-style register selects `0x50`/`0x52`/
`0x79`/`0x7a`, `< 0x21` vs `>= 0x21` splitting into two register banks — plausibly two
8-bit-wide GPIO expanders/shift registers on the scan matrix). The name for each index
comes from a flat 73-entry pointer table (`0xc0012c04..0xc0012d24`), decoded below —
**this is Korg's own ground-truth ordering and spelling for every physical switch/LED
on the panel**, extracted directly rather than inferred from live capture:

| Idx | Name | Idx | Name | Idx | Name |
|---|---|---|---|---|---|
| 0 | Combi | 25 | Seq Red | 50 | KARMA Module All |
| 1 | Prog | 26 | Pause | 51 | KARMA Module A |
| 2 | Seq | 27 | Rew | 52 | KARMA Module B |
| 3 | Sampling | 28 | FF | 53 | KARMA Module C |
| 4 | Global | 29 | Tepo *(sic)* | 54 | KARMA Module D |
| 5 | Disk | 30 | Help | 55 | DrumTrack Linked |
| 6 | Live | 31 | Compare | 56 | DrumTrack On/Off |
| 7 | Bank I-A | 32 | Play/Mute 1 | 57 | Timbre (Button) |
| 8 | Bank I-B | 33 | Play/Mute 2 | 58 | Timbre 1-8 |
| 9 | Bank I-C | 34 | Play/Mute 3 | 59 | Timbre 9-16 |
| 10 | Bank I-D | 35 | Play/Mute 4 | 60 | Audio (Button) |
| 11 | Bank I-E | 36 | Play/Mute 5 | 61 | Audio Input |
| 12 | Bank I-F | 37 | Play/Mute 6 | 62 | Audio 1-8 |
| 13 | Bank I-G | 38 | Play/Mute 7 | 63 | Audio 9-16 |
| 14 | Bank U-A | 39 | Play/Mute 8 | 64 | External (Button) |
| 15 | Bank U-B | 40 | Select 1 | 65 | Knobs/KARMA(Button) |
| 16 | Bank U-C | 41 | Select 2 | 66 | ToneAdjust (Button) |
| 17 | Bank U-D | 42 | Select 3 | 67 | ToneAdjust |
| 18 | Bank U-E | 43 | Select 4 | 68 | EQ |
| 19 | Bank U-F | 44 | Select 5 | 69 | Channel Strip |
| 20 | Bank U-G | 45 | Select 6 | 70 | Individual Pan |
| 21 | Sampling Green | 46 | Select 7 | 71 | Solo |
| 22 | Sampling Rec | 47 | Select 8 | 72 | *(blank / out-of-range default)* |
| 23 | Seq Rec | 48 | KARMA On/Off | | |
| 24 | Seq Green | 49 | KARMA Latch | | |

Note **"Sampling Green"/"Sampling Rec" and "Seq Green"/"Seq Red" are separate table
entries** for what are single physical buttons (Sampling Start/Stop, Seq Start/Stop) —
i.e. those two buttons have **bi-color (2-independently-driven) LEDs**, not a simple
on/off LED, confirmed structurally rather than guessed.

---

## Boot splash resource

A raw, uncompressed, 8bpp palette-indexed 800×600 framebuffer image (no container/
header — literally row-major pixel bytes) is embedded starting at payload offset
`0x32800` (VSB file offset `0x32900`). Content: the "KORG" wordmark top-left, a row
of Kronos EXi-engine badge icons (MS-20ex, Polysix ex, MOD-7, STR-1, HD-1, SGX-1,
EP-1, CX-3, AL-1), the "KRONOS / MUSIC WORKSTATION" title lockup, and a solid green
footer band starting around row 527-540 (palette index `0xFF`). This is almost
certainly the boot/startup splash rendered directly by `clcdc.cpp` while the rest of
the system comes up — **not reproduced in this repo** (it's Korg's copyrighted
artwork, not a protocol fact), but useful to know it's there and exactly where.

### Palette cross-check against KronosScreenRemoteDaemon

A 768-byte (256×RGB) palette table sits immediately before the splash bitmap, at
payload offset `0x1ef80` (VSB file offset `0x1f080`). Diffed byte-for-byte against
`KronosScreenRemoteDaemon/source/palette_data.h` (which the daemon captured live off
a running Kronos's `/dev/fb1`): **229 of 256 entries are byte-identical.** The 27
mismatches are concentrated at index 0 (near-black rounding) and indices 125-126 and
232-255, where the daemon's live-captured table has real gradient/skin-tone colors
and the firmware's static table has placeholder black. Read together, this confirms:

1. The daemon's palette is correct for the ~230 "fixed" UI-chrome colors.
2. Indices ~232-255 (and 125-126) are a dynamically-remapped range — content that
   uses photos/gradients (album art, VU-style gradients) gets its palette entries
   written at runtime rather than fixed at boot, so a **static** capture of those
   slots is inherently screen-content-dependent and will drift. Anything in
   `palette_data.h` reading black in that range is not a bug, it's just whatever
   happened to be on-screen when it was captured.
3. Architecturally this confirms the display pipeline both sides assume: the host
   renders an 8bpp palette-indexed 800×600 framebuffer (`/dev/fb1`), and
   `OmapNKS4Module.ko` streams *that* — indexed pixels, not RGB — over USB to this
   OMAP-L1x board, which pushes it to the physical LCD through its own `clcdc.cpp`
   LCD controller using (mostly) this same fixed palette. Sending indices instead of
   RGB24 is a 3:1 USB bandwidth win, which is presumably why Korg did it that way —
   and it's exactly the framebuffer model `screenremote.c` already assumes for
   `/dev/fb1`, now confirmed from the other end of the wire.

---

## Status

| Item | Status |
|---|---|
| Container header | Parsed, mostly understood (2 fields unidentified — `0x2c` and `0x40`) |
| Load address / architecture | Confirmed: ARM, `0xC0000000`, TI OMAP-L1x |
| Ghidra project | Not versioned yet — analyzed via a throwaway ELF-wrapped import; redo via `workflow/ghidra_setup.md` conventions before deep work |
| Switch/LED name table | Fully extracted (73 entries) |
| Boot splash + palette | Located, dimensions/offsets documented; image itself not extracted into the repo |
| `CryptoAt88.cpp` | Self-test routine fully traced (opcodes, framing, halt behavior); zone-0-without-visible-`$B8` question and the broader queued-command relay left open |
| PSoC protocol details | Not investigated beyond string evidence |
| Deep RE (full function coverage) | Not pursued — this pass was scoped to "what's useful for KronosScreenRemoteDaemon" |
