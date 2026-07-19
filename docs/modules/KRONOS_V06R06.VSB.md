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
../I2cByGpio.cpp             - bit-banged I2C over GPIO (reconstructed 2026-07-18 as
                                NKS4PanelFirmware/i2c_by_gpio.c - the genuinely shared
                                driver underneath both CryptoAt88.cpp and CDix4192.cpp;
                                previously only framing-traced/cross-referenced, not
                                itself a real file in this repo)
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
command surface may be broader than the four opcodes ground-truthed here.

**Chased further, 2026-07-17 — traces all the way to the wire, and closes a loop
with the host-side protocol docs.** `FUN_c0005e9c` pops a 32-byte queue entry
(`FUN_c0000fc8` — a 2-deep ring buffer, index masked `& 1`, at queue-handle offsets
`+0x41`/`+0x42`) and dispatches on the low bit of the command byte: LSB clear →
write path (`FUN_c0000ef4`, the same primitive `$B0`'s trace already used); LSB set
→ read path (`FUN_c0000f30`, same as `$B2`'s trace), **then relays the read result
onward via `FUN_c0005da0`.**

`FUN_c0005da0` builds a wire-format event record and hands it to `FUN_c000acec`
(a USB-endpoint transmit-queue submit, gated on a "buffer available" check +
128-byte outstanding-byte budget — the firmware-side mirror of the host's own
`CSTGOmapNKS4Fifos` input-FIFO consumer). The record it builds has the literal byte
**`0xe1`** hardcoded into it — the exact same **AtmelRead** opcode this repo's own
host-side docs (`OmapNKS4Module.ko`'s `driver.cpp`/`ReceiveEventBuffer`,
`KronosNKS4/docs/protocol.md`) already decode on the *receiving* end. Reading
`FUN_c0005da0`'s stack-variable layout in address order (not assignment order —
Ghidra's `local_XX` naming decreases toward the frame pointer, so `&local_70`'s
first 4 bytes in memory are `[local_70, local_6f, local_6e, local_6d]`) gives the
literal first wire word as `[cmd_data[2], cmd_data[1], cmd_data[0], 0xe1]` — the
same **per-dword byte-reversal** convention already reverse-engineered from the
host side (`ContinueProcessingEvent`'s pixel-chunk swap, `KronosNKS4`'s
"pairwise-halfword swap" documentation for `AtmelRead` decode). This is the first
time this project has traced a specific NKS4 event type's construction on *both*
ends of the wire and found them to agree exactly — the two independent RE efforts
(host-side decompile, firmware-side decompile) corroborate each other, which is
about as strong a confirmation as this protocol work gets without a live capture.

Not chased further this pass: `FUN_c0000ef4`'s write-path equivalent event
(presumably a similar record for whatever the write acknowledges, if anything —
`$B0`/`$B4`/`$B8` writes may not generate a host-visible event at all, unlike
`$B2` reads), and the exact command-byte encoding the *host* side uses to enqueue
into this 2-deep queue in the first place (i.e., what writes into the ring buffer
`FUN_c0000fc8` dequeues from — not yet traced back to its producer/caller).

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

**Extended, 2026-07-17** (reconstructed as `NKS4PanelFirmware/cpsoc.c`): navigation
key codes confirmed - `0x28` moves down the list (index++, capped at `0x48`), `0x27`
moves up (index--, floored at `0`). Screen layout confirmed from the real draw-call
y-coordinates: `y=0x208` (520) is the idle header line, `y=0x21c` (540) is the live
switch/LED readout for the current index, `y=0x230` (560) is a third, distinct
action triggered by key code `8` while a separate "menu active" latch (set/cleared
by key code `0x17`) is held - that third action's own effect (`FUN_c0000ba0`) isn't
yet identified, plausibly an exit/reset of the diagnostic menu.

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
`0x25235` (VSB file offset `0x25335`; runtime VA `0xC0025235`, since the firmware
links flat at `0xC0000000` — see "Target hardware" above). Content: 68px of black margin, then the
"KORG" wordmark centered, a centered row of 9 Kronos EXi-engine badge icons (left
to right: SGX-1, EP-1, CX-3, AL-1, MS-20ex, Polysix ex, MOD-7, STR-1, HD-1), the
centered "KRONOS / MUSIC WORKSTATION" title lockup, and a solid green footer band
starting around row 527-540 (palette index `0xFF`). This is almost certainly the
boot/startup splash rendered directly by `clcdc.cpp` while the rest of the system
comes up — **not reproduced in this repo** (it's Korg's copyrighted artwork, not a
protocol fact), but useful to know it's there and exactly where.

**Offset correction (2026-07-19, two passes):** earlier revisions of this doc gave
`0x32800` (VSB file offset `0x32900`; runtime VA `0xC0032800`). That offset decodes to a real image, but a
horizontally-*rolled* one — every row starts 331 bytes into its true content and
wraps, so the badge row and title lockup each appear as two swapped half-width
copies side by side (e.g. `AL-1` wrapping around to sit next to `MS-20ex` instead
of ending against a black margin) and the whole splash reads as off-center to the
right. Caught by cross-referencing `KronosScreenRemoteDaemon`'s own boot-splash
compositing work against a real device photo
(`KronosScreenRemoteDaemon/../BootScreen/Main.png`): `0x32800 - 331 = 0x326b5`
renders as a single clean copy with the KORG wordmark, badge row, and title lockup
all landing within a pixel of dead-center — matching the photo's horizontal
centering. (An intermediate guess of `0x32800 - 420 = 0x3265c` fixed the
wraparound but was still ~89px off-center; the wraparound-free byte range is much
wider than the range that's actually centered, so "doesn't wrap" alone isn't
sufficient to confirm the true offset — check centering against a real reference
image too.)

That first pass fixed column alignment but not row alignment: `0x326b5` places the
top of the KORG lettering flush against row 0, but on the real screen it sits 68px
down. Column and row alignment are independent for a flat row-major image (a shift
of a whole number of rows, i.e. a multiple of 800 bytes, changes only which row
content lands in, not where within the row) - shifting 68 rows (`68*800` bytes)
earlier moves the whole image down by exactly 68px on output without disturbing
the already-confirmed horizontal centering: `0x326b5 - 68*800 = 0x25235`, and the
KORG glyph's first non-black row does land at exactly row 68 of the result. The
palette offset (`0x1ef80` payload / `0x1f080` file) is unaffected by either
correction — cross-checked independently below and not sensitive to the same
row-roll failure mode.

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

## The wire-protocol command dispatcher — ties every subsystem together

**Found 2026-07-17** while chasing `cpsoc.cpp`'s callers: `FUN_c0007d1c` is the
firmware's single entry point for every incoming 32-bit command word from the host -
the direct counterpart to the host-side `COmapNKS4Command` wire protocol
(`kronosology/reconstructed/OmapNKS4Module/command.cpp`). **Fully reconstructed
2026-07-18** as `wire_dispatch_command` in
[`NKS4PanelFirmware/wire_dispatch.c`](../../reconstructed/NKS4PanelFirmware/wire_dispatch.c)
— that file also reconstructs its sibling, the master per-tick status-bit dispatcher
`FUN_c0008b64` (`master_dispatch_tick`), and resolves this dispatcher's own real
callers (the USB receive path in `omap_l137_usbdc.c`'s own address neighborhood, not
`eva_board_main.c`'s main loop directly, as this doc previously left open — see
`wire_dispatch.c`'s own header for the full trace). One function, dispatching
on the opcode byte, routes to essentially every subsystem reconstructed so far:

| Opcode | Routes to | Matches |
|---|---|---|
| `0xc0` | `clcdc_reg_write`/`_set_bits`/`_clear_bits` (via a reg-byte sub-select) | `InitLCDRegs` |
| `0x81` | `clcdc_cursor_set_stride` | `XAxisByteSize` |
| `0xc2` | sets up a pixel-region transfer context | `SendPixelDataRegion` |
| `0x83` | clears the pixel-region transfer context | region-end marker |
| `0xc5` | palette update (`FUN_c0015018`) | `UpdateColorPal` |
| `0xc6`/`0xc4` | returns early, deferred to a streaming continuation (the panel-side mirror of `ContinueProcessingEvent`) | pixel-data chunk / fill |
| `0xc7` | LCD brightness (`FUN_c0007ccc`) | `SetLCDBrightness` |
| `0xe0` | AT88 relay write path, reassembles a variable-length payload | `stgNV2AC_sync_cmd` |
| `0xe1` | AT88 relay read path | `stgNV2AC_sync_read_cmd` / AtmelRead |
| `0x50`/`0x51`/`0x52` (op-byte `0`) | `cpsoc`'s three register-bank read wrappers | switch/LED state reads |
| `0xee` | comm-check | `CommunicationCheck` |
| `0xf0` | version query (asserts via the same hard-halt handler if a state byte is `-1`) | `GetVersion` |
| `0xd0`/`0xd1`/`0x80` | not yet attributed to a known host-side command | - |

This single finding retroactively explains *why* `clcdc.cpp` and `cpsoc.cpp` (and by
extension `CryptoAt88.cpp`'s queue-relay bit-13 trigger from `FUN_c0008b64`, a
sibling dispatcher one level up) all showed up as separately-reconstructed
subsystems with no obvious common caller before now - they share this one dispatch
point, not because they call each other, but because they're each addressed by a
distinct opcode range of the same wire protocol. `EvaBoardMain.cpp`'s own bring-up
almost certainly wires this dispatcher to the USB receive path; that connection
itself hasn't been traced yet (see `EvaBoardMain.cpp`'s own "not started" status).

## Status

| Item | Status |
|---|---|
| Container header | Parsed, mostly understood (2 fields unidentified — `0x2c` and `0x40`) |
| Load address / architecture | Confirmed: ARM, `0xC0000000`, TI OMAP-L1x |
| Ghidra project | Still not versioned on the shared Ghidra Server — still a throwaway local import; redo via `workflow/ghidra_setup.md` conventions before deep work. As of the 2026-07-18 systematic pass, this throwaway import is now used heavily via a bulk pre-fetched static dump (`all_decompiled.json`/`all_data.json`) rather than live per-call MCP queries, specifically to work around a real concurrency bug in the live Ghidra MCP bridge under multiple simultaneous agent sessions — see `NKS4PanelFirmware/README.md`'s own 2026-07-18 pass section for the full methodology note |
| Switch/LED name table | Fully extracted (73 entries) |
| Boot splash + palette | Located, dimensions/offsets documented; image itself not extracted into the repo |
| `CryptoAt88.cpp` | Fully reconstructed as `NKS4PanelFirmware/crypto_at88.c` (self-test opcodes/framing/halt behavior, the full I2C bit-bang layer, the queued-command relay and its host-side producer/consumer, byte-order-corrected wire events) — see that file's own status section. The self-test's zone-0-without-visible-`$B8` question is now moot: `crypto_at88_self_test` is confirmed to have zero static callers anywhere in the full 691-function xref data, i.e. it does not run in this build's normal boot path |
| PSoC protocol details | Substantially investigated beyond string evidence — see `NKS4PanelFirmware/cpsoc.c` (register-bank dispatch, event queues, LED-bargraph handlers) and `NKS4PanelFirmware/panelbus_dispatch.c` (the second, genuine hardware I2C0/I2C1 command channel feeding it) |
| Deep RE (full function coverage) | **COMPLETE as of 2026-07-19**: all 691 real functions in the image now have a genuine, compilable C definition, verified by direct address-by-address grep against the real file contents (not just the project's own coverage-tracking script, which needed its own bug fixes along the way). 53 `K1_V06R06/*.c` files (renamed from `NKS4PanelFirmware/` when the sibling Kronos 2 port project, `K2_V01R10/`, was started). Coverage-complete does not mean behaviorally proven — individual files still carry their own honest "STILL OPEN" notes for unconfirmed register semantics, unresolved constant identities, and a couple of cases the decompiler itself couldn't fully disambiguate; see `K1_V06R06/README.md`'s own 2026-07-19 section and each file's own header |
