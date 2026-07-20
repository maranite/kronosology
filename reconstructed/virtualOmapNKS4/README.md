# virtualOmapNKS4 — a viewable virtual NKS4 panel

A userspace "virtual panel" that implements the same logical draw API
`COmapNKS4VideoAPI` (`kronosology/reconstructed/OmapNKS4Module/video.cpp`) exposes
to real callers — `SendPixelDataRegion`, `SendFillData`, `UpdateColorPal`,
`InitLCDRegs`, `XAxisByteSize` — backed by a real in-memory 800×600 8bpp indexed
framebuffer and 256-entry RGB palette, with a dependency-free BMP writer so the
result can actually be viewed (not just asserted correct).

This exists to close the gap `OmapNKS4VirtualDriver/` doesn't: that stub only
satisfies OA.ko's 9 direct exported-symbol dependencies for VM boot testing
(trivial no-op bodies, zero protocol logic, no framebuffer at all). This project
answers a different question — "if a real caller drew a real frame, what would it
actually look like?" — by implementing the drawing itself.

## Files

| file | what |
|---|---|
| `vpanel.h` / `vpanel.c` | the virtual panel: framebuffer + palette + draw ops + BMP writer |
| `demo.c` | exercises every draw op (palette set, pixel-region colour bars, fills, a progress-bar readout) and dumps the result |
| `wire_to_vpanel.c` | translates a captured `vm_virtual_probe` dmesg log's real wire-decode lines into real `vpanel_*` calls - see "Live wire capture → real render" below |
| `captures/` | real, checked-in evidence: a captured boot console log + the BMP `wire_to_vpanel` rendered from it |

## Scope decision: draw-level API, not raw USB wire bytes

The real wire-level pixel-streaming protocol (`ContinueProcessingEvent`'s 0xc6
512-byte chunks + 0x83 end marker, dword-halfword byte-swapped) has one
still-unresolved sizing detail: the decompiled packet buffer is declared 544 bytes
(`unsigned int packet[0x88]`) against a documented/expected 512-byte USB chunk size
(see `video.cpp`'s own comment on `ContinueProcessingEvent`). Building a byte-exact
wire replay device on top of that ambiguity risked a "screen" that was subtly wrong
in a way that wouldn't be obvious just by looking at it.

The draw-level API carries no such ambiguity — it's exactly what this project's own
`KronosDoom/doomgeneric_kronos.c` driver and the real `OA.ko`/`Eva` callers already
use (confirmed via the 2026-07-13 OA.ko call-inventory audit, `KronosNKS4/docs/
protocol.md`). Building on that layer means every pixel this virtual panel renders
is provably what the real `COmapNKS4VideoAPI` would have done with the same calls -
no wire-framing guesswork in between.

A future pass can add a wire-level front end (a real USB gadget, matching
`KronosNKS4/tests/gadget-sim`'s existing infrastructure, or a simple pipe-fed
byte-stream replay) that decodes onto this same `struct vpanel` - the rendering
core doesn't need to change either way.

## Build & run

```sh
gcc -Wall -Wextra -O2 -o demo demo.c vpanel.c
./demo
# -> /tmp/vpanel_demo.bmp (viewable in any image tool/browser)
```

## Status

- [x] Framebuffer + palette + all five draw ops implemented
- [x] BMP output, verified via a real render (colour bars + fill + progress bar,
      confirmed correct 800×600 24-bit BMP, published as a viewable artifact)
- [ ] Wire-level (USB) front end - not yet built, see scope note above
- [x] Wired up to the real reconstructed `OmapNKS4Module` submit path, via a captured-log
      replay (not a live pipe - see "Live wire capture → real render" below for the honest
      scope of what this does and doesn't prove)

## Live wire capture → real render, 2026-07-19: what a real boot actually draws

Closes the loop between this tool and `OmapNKS4Module`'s own `vm_virtual_probe` VM-testing
mode (`../OmapNKS4Module/README.md`, search "Bulk-video/URB-pool path confirmed working"):
that work proved a real, unmodified caller's wire-protocol draw commands decode correctly on
the synthetic board; this pass takes those actual captured commands and drives this file's
real `vpanel_*` draw API with them, producing an actual viewable image - not a description of
one.

**What was built:**

- **`wire_to_vpanel.c`** (this directory) - a small, honest translator. It parses a captured
  dmesg/console log for `OmapNKS4: vm_virtual_probe: DIAG <opcode> wire decode: ...` lines (all
  five opcodes - `usb.cpp`'s `vm_usb_submit_urb()` diagnostic, already generalized from the
  original `0xc4`-only version before this pass started) and calls the matching real
  `vpanel_send_fill()` / `vpanel_update_color_pal()` / `vpanel_init_lcd_regs()` /
  `vpanel_x_axis_bytesize()` function with the exact captured values, in the exact captured
  order. It deliberately does **not** draw `SendPixelDataRegion` - that DIAG line only captures
  the 12-byte header (width/offset/rowBytes); the real pixel bytes ride a separate, uncaptured
  streaming path (`ContinueProcessingEvent`'s `0xc6`/`0x83` chunks) - so the tool counts that
  header as "seen" but never fabricates pixel content for it. Build: `gcc -Wall -Wextra -O2 -o
  wire_to_vpanel wire_to_vpanel.c vpanel.c`.
- **A real, freshly captured `vm_virtual_probe=1` boot**, `kronosvm` (192.168.3.87), via the
  existing `../OmapNKS4Module/tools/run_vm_virtual_probe_test.sh -t 200` (run
  `run_20260719_144946`) - **verdict PASS** (the module chain loaded, `OmapNKS4Module.ko`'s own
  `insmod` returned, `loadoa` ran to completion; the previously-documented 4+ minute
  post-return-0 hang did not occur this run). Full console log and the resulting BMP are
  checked in under `captures/vm_virtual_probe_run_20260719_144946_boot_console.log` /
  `.bmp` as real evidence, not just an inline paste.

**The real captured wire log** (`grep 'vm_virtual_probe: DIAG' boot_console.log`, this run):

```
OmapNKS4: vm_virtual_probe: DIAG SendFillData wire decode: width=553 base=172 color=192 height=553 (raw cmd=0xc4290200 len=12)
OmapNKS4: vm_virtual_probe: DIAG SendFillData wire decode: width=553 base=172 color=192 height=553 (raw cmd=0xc4290200 len=12)
OmapNKS4: vm_virtual_probe: DIAG SendFillData wire decode: width=97 base=75 color=9 height=97 (raw cmd=0xc4610000 len=12)
OmapNKS4: vm_virtual_probe: DIAG SendFillData wire decode: width=97 base=75 color=1 height=97 (raw cmd=0xc4610000 len=12)
```

Only `0xc4` (`SendFillData`) appears - exactly the 4 calls `SetProgressBarPercent()`
(`../OmapNKS4Module/driver.cpp:441-445`) makes, matching the earlier `pbtest_20260719_141603`
capture's own values field-for-field (same progress-bar geometry, confirmed reproducible
across independent runs, not a fluke of one boot). No `0xc0`/`0x81`/`0xc2`/`0xc5` lines appear
in this or any real boot to date - consistent with this project's own documented limitation
that `Eva` (the process that owns palette setup and the fuller LCD draw repertoire) has never
been run against this virtual board.

**Running the translator against that real log:**

```
$ ./wire_to_vpanel captures/vm_virtual_probe_run_20260719_144946_boot_console.log /tmp/out.bmp
wire_to_vpanel: parsed 4 draw-command line(s) from captures/vm_virtual_probe_run_20260719_144946_boot_console.log
  SendFillData:         4 (drawn)
  UpdateColorPal:       0 (drawn)
  InitLCDRegs:          0 (no-op on this panel model, see vpanel.c)
  XAxisByteSize:        0 (no-op on this panel model, see vpanel.c)
  SendPixelDataRegion:  0 header(s) seen, 0 drawn (no pixel payload captured)
vpanel draw ops actually applied to the framebuffer: 4
wrote /tmp/out.bmp
```

**The render is real, not blank/uniform** - independently verified by decoding the BMP's own
pixel data (not just trusting the tool's exit message): 800×600, 3 distinct colors
(`#000000` background 34.3%, `#c0c0c0` "track" fill 63.7%, `#010101` final-value fill 2.0%,
where the `color=9` fill from the log above is fully covered by the very next `color=1` fill at
the identical rectangle - both really happened, only the second is visible, exactly matching
two sequential fills to the same region rather than a decode bug). `file` confirms `PC bitmap,
Windows 3.x format, 800 x 600 x 24`. Checked-in copy: `captures/vm_virtual_probe_run_20260719_144946.bmp`
(md5 `f66221d26f52e49d9b418aed04536c87`).

**Honest scope of what this proves, and what it doesn't:** this is a genuine, non-fabricated
rendering of the *only* draw traffic a real `vm_virtual_probe=1` boot produces today - a small
progress-bar rectangle on an otherwise-untouched black screen, not a Kronos boot logo or UI.
That's the correct, expected result given the known limitation documented in
`../OmapNKS4Module/README.md` (`SetProgressBarPercent` is the only real caller exercised;
`Eva`'s fuller LCD repertoire is out of scope). This pass does not add a wire-level USB
front end, does not implement `SendPixelDataRegion`'s multi-chunk pixel streaming, and does
not run `Eva` against this board - all three remain open, as noted in the Status list above
and in `../OmapNKS4Module/README.md`'s own "Still open, not exercised by this test" note.
