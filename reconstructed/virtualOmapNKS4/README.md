# virtualOmapNKS4 - a viewable virtual NKS4 panel

A userspace "virtual panel" that implements the same logical draw API
`COmapNKS4VideoAPI` (`reconstructed/OmapNKS4Module/video.cpp`) exposes
to real callers - `SendPixelDataRegion`, `SendFillData`, `UpdateColorPal`,
`InitLCDRegs`, `XAxisByteSize` - backed by a real in-memory 800x600 8bpp indexed
framebuffer and 256-entry RGB palette, with a dependency-free BMP writer so the
result can actually be viewed (not just asserted correct).

This exists to close the gap `OmapNKS4VirtualDriver/` doesn't: that stub only
satisfies OA.ko's 9 direct exported-symbol dependencies for VM boot testing
(trivial no-op bodies, zero protocol logic, no framebuffer at all). This project
answers a different question - "if a real caller drew a real frame, what would it
actually look like?" - by implementing the drawing itself.

## Files

| file | what |
|---|---|
| `vpanel.h` / `vpanel.c` | the virtual panel: framebuffer + palette + draw ops + BMP writer |
| `demo.c` | exercises every draw op (palette set, pixel-region color bars, fills, a progress-bar readout) and dumps the result |
| `wire_to_vpanel.c` | translates a captured wire-decode log's decoded draw commands into real `vpanel_*` calls - see "Wire capture replay" below |
| `captures/` | a checked-in captured boot console log and the BMP `wire_to_vpanel` rendered from it |

## Scope decision: draw-level API, not raw USB wire bytes

The real wire-level pixel-streaming protocol (`ContinueProcessingEvent`'s 0xc6
512-byte chunks + 0x83 end marker, dword-halfword byte-swapped) has one
unresolved sizing detail: the decompiled packet buffer is declared 544 bytes
(`unsigned int packet[0x88]`) against a documented/expected 512-byte USB chunk size
(see `video.cpp`'s own comment on `ContinueProcessingEvent`). Building a byte-exact
wire replay device on top of that ambiguity risks a "screen" that is subtly wrong
in a way that would not be obvious just by looking at it.

The draw-level API carries no such ambiguity - it's exactly what this project's
DOOM port and the real `OA.ko`/`Eva` callers already use. Building on that layer
means every pixel this virtual panel renders is provably what the real
`COmapNKS4VideoAPI` would have done with the same calls - no wire-framing
guesswork in between.

A future pass can add a wire-level front end (a real USB gadget or a simple
pipe-fed byte-stream replay) that decodes onto this same `struct vpanel` - the
rendering core doesn't need to change either way.

## Build & run

```sh
gcc -Wall -Wextra -O2 -o demo demo.c vpanel.c
./demo
# -> /tmp/vpanel_demo.bmp (viewable in any image tool/browser)
```

## Status

- Framebuffer, palette, and all five draw ops are implemented.
- BMP output has been verified against a real render (color bars, a fill, and
  a progress bar produce a correct 800x600 24-bit BMP).
- A wire-level (USB) front end has not been built - see the scope decision above.
- The panel has been wired up to the real reconstructed `OmapNKS4Module` submit
  path via a captured-log replay (not a live pipe) - see "Wire capture replay"
  below for the exact scope of what that does and doesn't prove.

## Wire capture replay

`wire_to_vpanel.c` is a small, honest translator. It parses a captured
dmesg/console log for `OmapNKS4: vm_virtual_probe: DIAG <opcode> wire decode: ...`
lines - all five opcodes that `usb.cpp`'s `vm_usb_submit_urb()` diagnostic
emits - and calls the matching real `vpanel_send_fill()` /
`vpanel_update_color_pal()` / `vpanel_init_lcd_regs()` / `vpanel_x_axis_bytesize()`
function with the exact captured values, in the exact captured order. It
deliberately does not draw `SendPixelDataRegion`: that DIAG line only captures
the 12-byte header (width/offset/rowBytes), while the real pixel bytes ride a
separate, uncaptured streaming path (`ContinueProcessingEvent`'s `0xc6`/`0x83`
chunks). The tool counts that header as "seen" but never fabricates pixel
content for it.

Build:

```sh
gcc -Wall -Wextra -O2 -o wire_to_vpanel wire_to_vpanel.c vpanel.c
```

Usage:

```sh
./wire_to_vpanel <captured_console_log> <output.bmp>
```

### What a real boot draws today

A real boot with `vm_virtual_probe=1` produces wire traffic that decodes to
lines like:

```
OmapNKS4: vm_virtual_probe: DIAG SendFillData wire decode: width=553 base=172 color=192 height=553 (raw cmd=0xc4290200 len=12)
OmapNKS4: vm_virtual_probe: DIAG SendFillData wire decode: width=553 base=172 color=192 height=553 (raw cmd=0xc4290200 len=12)
OmapNKS4: vm_virtual_probe: DIAG SendFillData wire decode: width=97 base=75 color=9 height=97 (raw cmd=0xc4610000 len=12)
OmapNKS4: vm_virtual_probe: DIAG SendFillData wire decode: width=97 base=75 color=1 height=97 (raw cmd=0xc4610000 len=12)
```

Only `0xc4` (`SendFillData`) traffic appears - exactly the 4 calls
`SetProgressBarPercent()` (`reconstructed/OmapNKS4Module/driver.cpp:441-445`)
makes, with values that reproduce across independent captures. No
`0xc0`/`0x81`/`0xc2`/`0xc5` traffic has been observed on any boot to date,
consistent with `Eva` (the process that owns palette setup and the fuller LCD
draw repertoire) never having been run against this virtual board.

Running the translator against a log like the one above produces:

```
wire_to_vpanel: parsed 4 draw-command line(s) from <log>
  SendFillData:         4 (drawn)
  UpdateColorPal:       0 (drawn)
  InitLCDRegs:          0 (no-op on this panel model, see vpanel.c)
  XAxisByteSize:        0 (no-op on this panel model, see vpanel.c)
  SendPixelDataRegion:  0 header(s) seen, 0 drawn (no pixel payload captured)
vpanel draw ops actually applied to the framebuffer: 4
wrote <output.bmp>
```

The render is not blank or uniform: decoding the BMP's own pixel data (rather
than just trusting the tool's exit message) shows 800x600 with 3 distinct
colors - a black background, a light-gray "track" fill, and a small dark
final-value fill, where the `color=9` fill from the log above is fully covered
by the very next `color=1` fill at the identical rectangle. Both draws really
happened; only the second is visible on top - matching two sequential fills to
the same region rather than a decode bug.

### Honest scope of what this proves

This is a genuine, non-fabricated rendering of the only draw traffic a real
`vm_virtual_probe=1` boot produces today: a small progress-bar rectangle on an
otherwise-untouched black screen, not a Kronos boot logo or UI. That is the
correct, expected result given that `SetProgressBarPercent` is the only real
caller exercised so far; `Eva`'s fuller LCD repertoire is out of scope of this
capture. This tool does not add a wire-level USB front end, does not implement
`SendPixelDataRegion`'s multi-chunk pixel streaming, and has not been exercised
against `Eva`'s draw traffic - all three remain open (see Known limitations).

## Known limitations

- **No wire-level (USB) front end.** Only the draw-level API is implemented;
  see the scope decision above for why, and the `ContinueProcessingEvent`
  packet-buffer sizing ambiguity that would need resolving first. *To
  validate:* determine whether the decompiled 544-byte packet buffer
  (`unsigned int packet[0x88]`) is a real oversize or a decompiler artifact
  before building a byte-exact wire replay on top of it.
- **`SendPixelDataRegion` is not rendered**, even by the wire capture replay -
  only its 12-byte header is captured by the diagnostic log; the actual pixel
  bytes stream through a separate, currently-uncaptured path
  (`ContinueProcessingEvent`'s `0xc6`/`0x83` chunks). *To validate:* extend the
  wire-decode diagnostic (or add a new capture point) to also log the streamed
  pixel payload, then extend `wire_to_vpanel.c` to draw it.
- **Only `SendFillData` traffic has been observed from a real boot.** No
  `UpdateColorPal`, `InitLCDRegs`, or `XAxisByteSize` calls have been captured,
  because `Eva` (the process that owns palette setup and the fuller LCD draw
  repertoire) has not been run against this virtual board. The rendering
  pipeline for those opcodes is implemented and exercised by `demo.c`, but not
  yet validated against real captured traffic. *To validate:* run `Eva` against
  the virtual board, capture its wire-decode log, and replay it through
  `wire_to_vpanel`.

## Related documentation

- `reconstructed/OmapNKS4Module/README.md` - the bulk-video/URB-pool submit
  path this project's captures come from.
- `reconstructed/OmapNKS4Module/video.cpp` - `COmapNKS4VideoAPI` and the
  `ContinueProcessingEvent` packet-size comment referenced above.
- `reconstructed/OmapNKS4Module/driver.cpp` - `SetProgressBarPercent`, the
  only real caller exercised in the captures under `captures/`.
- `reconstructed/OmapNKS4VirtualDriver/` - the lighter-weight exported-symbol
  stub this project extends beyond (no framebuffer, no draw logic).
