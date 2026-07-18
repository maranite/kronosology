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
- [ ] Wired up to the real reconstructed `OmapNKS4Module` submit path or a
      `gadget-sim`-style USB transport for genuine end-to-end testing
