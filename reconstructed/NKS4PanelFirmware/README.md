# NKS4PanelFirmware — reconstructed source

Reverse-engineered source for `KRONOS_V06R06.VSB`, the firmware for the NKS4 front-panel
board itself (USB `0944:1005`) — a physically separate embedded system (TI OMAP-L1x,
ARM926EJ-S) from the Kronos's own x86 host. See
[`kronosology/docs/modules/KRONOS_V06R06.VSB.md`](../../docs/modules/KRONOS_V06R06.VSB.md)
for the container format, load-address derivation, and the full findings this
reconstruction is built from.

This is the **other end of the wire** from
[`kronosology/reconstructed/OmapNKS4Module/`](../OmapNKS4Module/) (the host-side kernel
driver) and from [`KronosNKS4/`](../../../KronosNKS4/) (the userspace protocol port).

## Why this exists

Every prior pass at this firmware (see the `.md` doc's own "Status" table) was scoped to
"what's useful for `KronosScreenRemoteDaemon`" — targeted extraction (the switch/LED name
table, the boot splash location, one AT88 self-test trace), not systematic decompilation.
This project starts the systematic pass: 691 real functions total in the image, worked
through subsystem by subsystem in the same style as `OmapNKS4Module`'s own reconstruction
(ground-truth citations, honest gap-flagging, no guessed opcodes presented as fact).

## SPI-device closure pass, 2026-07-17

Follow-on to the SPI/USB cleanup pass below: went back into the one function this
project's own status recommendation flagged as "the strongest remaining lead" -
`omap_l108_spi.c`'s unattributed third SPI device. Fully resolved: decompiled the
opcode dispatcher's 4 sub-handlers (previously left untouched) and traced them down
to a cluster of LED-bargraph-drive functions whose register encoding
(`cpsoc_led_set`/`cpsoc_led_clear` both collapsing to reg `0x79`/`0x7a`) matches
`cpsoc.c`'s already-documented two-register-bank split exactly - real, concrete
evidence this whole chain is cpsoc.c's own code, not a mystery third chip. Moved the
entire cluster (`cpsoc_analog_poll_task`, `cpsoc_analog_poll_channel`,
`cpsoc_event_opcode_dispatch`, and 4 LED-driving handlers, one of which calls
directly into `clcdc_test_pattern`'s own dispatcher) into `cpsoc.c` and updated
`omap_l108_spi.c`/this README accordingly. Also found and fixed a real mis-naming in
`crypto_at88.c`: `FUN_c0005500` was called `crypto_at88_prepare_fault_screen` but is
actually a generic interrupt-save/disable primitive (`irq_save_and_disable`), only
incidentally used by the fault handler - see `crypto_at88.c`'s own correction note.

## SPI/USB cleanup pass, 2026-07-17

Follow-on to the re-verification pass below: went back into the SPI/USB-adjacent open
items with a working Ghidra bridge and `read_memory` access. Two real results:

- **Falsified the project's own "headline finding"**: `eva_board_main.c`'s init table
  was claimed to explain every "zero static callers" function in the project. Actually
  reading the table's contents (not just its start/end pointers) shows it has exactly
  **one entry**, a lazy-init singleton unrelated to any of them. Corrected in
  `eva_board_main.c`, `cdix4192.c`, and `omap_l108_spi.c` - the real invocation
  mechanism for those functions is still open.
- **Resolved `omap_l108_spi.c`'s two remaining zero-caller functions**: they're a real,
  previously-unidentified THIRD SPI-bus device - a never-returning background polling
  task (`spi_analog_poll_task`) and its edge-triggered read-and-notify routine
  (`spi_analog_poll_channel`), sharing `cad_init`'s own reset primitive and `0x9000`
  SPI config command but driving separate hardware. Not yet attributed to any
  `__FILE__`-anchored source file - the strongest remaining lead in this area.

## Re-verification pass, 2026-07-17

After the first full sweep through every anchored subsystem, four independent agents
adversarially re-checked every "confirmed" claim in all 11 completed files against fresh
Ghidra decompiles/disassembly, specifically hunting for overclaimed-as-confirmed items
and transcription errors. Real bugs found and fixed (each also noted in-file at its
correction site):

- **`crypto_at88.c`** (the most significant finds): `at88_relay_read_result`'s wire-format
  byte order was wrong in two places (word 1 and the trailing payload loop both need
  byte-reversal, not straight copies); a fabricated `at88_usb_tx_ready` gate call that
  doesn't exist in the real function was removed; `at88_frame_command`'s retry step was
  wrongly described as "STOP + START again" (no STOP call exists) and its address-byte
  ACK check had inverted polarity vs. the other three header-byte checks; `at88_delay`
  was missing its `chip` argument throughout.
- **`cdix4192.c`**: `cdix_reg_write`/`cdix_reg_read` don't actually forward their `chip`
  parameter to the underlying I2C primitive (the real code passes an uninitialized stack
  buffer instead) - left honestly unresolved whether this is a genuine firmware quirk.
  Also recovered the actual 3-entry config table contents.
- **`eva_board_main.c`**: `eva_board_watchdog_fault_wrapper` doesn't take a real `handle`
  parameter either - same "phantom forwarded parameter" pattern as `cdix4192.c`, found
  independently by a different verification agent.
- **`clcdc.c`**: a struct-offset/field-name swap in the bitmap font descriptor; a stray
  extra digit in the mirrored-progress-bar-region distance (one row, not two); and a
  "6-entry dispatch table" claim for `clcdc_test_pattern`'s caller that's actually a
  sequential if/else-if chain.
- **`cpsoc.c`**: `cpsoc_diag_menu_input` was missing an entire extra hardware read
  (`cpsoc_read_switch_row_clear` at the pre-movement index) before the already-documented
  post-movement `cpsoc_read_switch_row` call.
- **`cad.c`**: a merged fault call site with a placeholder line number `0` was actually
  two distinct call sites with real line numbers (`0x42`/`0x46`).
- **`mcasp.c`**: an off-by-one in the "21 xrefs, N from one function" count (11, not 10).
- **`omap_l108_spi.c`**: an incomplete caller list for `omap_spi_write` - a 5th caller
  function (~`0xc0011624`) wasn't accounted for.

Everything else checked out exactly, including every previously-flagged high-risk claim
(the SPI driver's two-different-status-bits logic, the USB transfer state machine's
8001-byte threshold and exact comparisons, the tick-timer wraparound direction, every
xref-count and "zero static callers" claim). See each file's own `CORRECTION` comments
for the full detail behind each item above.

## Ghidra setup

Load-bearing detail this doc's own container-format section already worked out: the raw
917504-byte payload (file offset `0x100..EOF`, after the 256-byte "KORG SYSTEM FILE"
header) must be wrapped in a minimal single-`PT_LOAD` ELF32 ARM header
(`e_machine=40`, entry=vaddr=`0xC0000000`) for Ghidra's ELF importer to auto-select
`ARM:LE:32:v8` and place everything correctly — the raw-binary importer path fails
silently through the Ghidra MCP bridge. **Not yet versioned on the shared Ghidra Server**
(currently a throwaway local import) — see `docs/workflow/ghidra_setup.md`'s own
conventions before starting a fresh session's work here.

## Subsystems

| Subsystem | Real source file (per embedded `__FILE__` strings) | Status |
|---|---|---|
| AT88 crypto-chip relay | `../CryptoAt88.cpp` | `crypto_at88.c` — in progress, see below |
| I2C bit-bang primitives | `../I2cByGpio.cpp` | Framing traced (start/ack/stop), not yet reconstructed as its own file |
| LCD controller | `../clcdc.cpp` | `clcdc.c` — done, see below |
| PSoC button/LED scan + resolved 3rd SPI device | `../cpsoc.cpp` | `cpsoc.c` — core done, see below |
| Touch panel | `../ctouchpanel.cpp` | `ctouchpanel.c` — core done, see below |
| Analog (knobs/sliders/pedals) | `../cad.cpp` | `cad.c` — done, full calibration engine, see below |
| Audio serial port | `../McAspHandler.cpp` (0 xrefs - see below) / `../MCU/Component/OmapL137Mcasp.cpp` | `mcasp.c` — core done, see below |
| Digital audio interface | `../CDix4192.cpp` | `cdix4192.c` — done, see below |
| Main entry / board bring-up | `../EvaBoardMain.cpp` | `eva_board_main.c` — core done, see below |
| Object manager | `../cobjectmgr.cpp` | `cobjectmgr.c` — core done, see below |
| SoC tick timer | `../MCU/OmapL108.cpp` | `omap_l108.c` — core done, see below |
| SPI peripheral (shared cad/cpsoc/unknown-3rd-device bus) | `../MCU/Component/OmapL108Spi.cpp` | `omap_l108_spi.c` — done, see below |
| USB device controller | `../MCU/Component/OmapL137Usbdc.cpp` | `omap_l137_usbdc.c` — core done, see below |

## `crypto_at88.c` — status

Ground-truthed so far (fresh Ghidra decompile, 2026-07-17):

- **`FUN_c0001028`** — the self-test: `$B4` zone-0 select → fill a 16-byte scratch buffer
  with `0,1,...,15` → `$B0` write it → **poison the same buffer with `0xa5` repeating**
  (a sentinel to prove the next step actually wrote real data, not stale buffer content) →
  `$B2` read 16 bytes back → byte-for-byte compare against the *original* `0,1,...,15`
  pattern (not the poison value) → any mismatch calls the hard-halt assert handler.
- **`FUN_c000919c`** — the assert/halt handler: draws two lines of text (filename +
  line number, via `FUN_c0015650`, a text-draw primitive taking `(x, y, string, ?)`) at
  fixed screen positions, then `do {} while(true)` — confirmed genuinely unrecoverable,
  matches the `"SYSTEM STARTUP FAILED"` behavior already documented.
- **`FUN_c0005e9c`** — a generic queued AT88 command relay, distinct from the self-test's
  direct calls: pops a command from a **2-deep ring buffer** (`FUN_c0000fc8`, index masked
  `& 1`), dispatches on the command byte's LSB (clear=write via `FUN_c0000ef4`, set=read
  via `FUN_c0000f30`), and for reads, relays the result onward via `FUN_c0005da0`.
- **`FUN_c0005da0`** — builds a wire-format event record for the read result and hands it
  to `FUN_c000acec` (a USB-endpoint transmit-queue submit, gated on buffer availability +
  a 128-byte outstanding-byte budget — this firmware's mirror of the host's own
  `CSTGOmapNKS4Fifos` input-FIFO consumer). **The record has the literal byte `0xe1`
  hardcoded into it** — the exact AtmelRead opcode this project's host-side docs already
  decode (`OmapNKS4Module/driver.cpp`'s `ReceiveEventBuffer`,
  `KronosNKS4/docs/protocol.md`) — and the stack-variable layout (read in true memory
  order, not Ghidra's assignment order) confirms the same **per-dword byte-reversal**
  convention already reverse-engineered from the host side. This is the first time this
  project has traced one specific NKS4 event type's construction on *both* ends of the
  wire and found them to agree exactly.
- **`FUN_c0000ef4`/`FUN_c0000f30`** — thin dispatchers: write → `FUN_c00016e8`,
  read → `FUN_c0001638` (both bottom out in `FUN_c0001588`'s I2C start/ack/stop bit-bang
  framing, per the `.md` doc's own earlier trace).

**Extended and closed out, 2026-07-17 (second pass):**
- **The I2C bit-bang layer is now fully traced**: `at88_i2c_start` (START condition,
  `FUN_c000143c`), `at88_i2c_write_byte` (write-one-byte-MSB-first + read ACK,
  `FUN_c000134c`), and `at88_frame_command` (the 4-byte `{addr, cmd, arg1, arg2}` header
  framer, `FUN_c0001588`) are all reconstructed in `crypto_at88.c`.
- **The queue-relay trigger context is resolved**: `crypto_at88_process_queue`
  (`FUN_c0005e9c`) has exactly one caller, `FUN_c0008b64` - the firmware's central
  interrupt-status dispatch loop. That single function reads one hardware status
  register per invocation and fans out to ~10 subsystem handlers by bit - bit 13
  triggers the AT88 queue processor, bit 8 triggers `clcdc`'s test-pattern generator,
  bit 15 triggers `clcdc_progress_bar` directly. This ties `CryptoAt88.cpp` and
  `clcdc.cpp` together through one shared dispatcher, a genuinely new structural
  finding neither subsystem's own pass surfaced alone.

**Still genuinely open:**
- The write path's own event, if any (does a successful `$B0`/`$B4`/`$B8` ever generate a
  host-visible event the way `$B2` reads do, or is it fire-and-forget?).
- What actually *sets* bit 13 in `FUN_c0008b64`'s status register in the first place -
  almost certainly an ISR reacting to a real AT88 command request (possibly relayed
  from the host), but that ISR itself hasn't been traced.
- `at88_frame_command`'s real retry bound/STOP-and-retry behavior on an address NACK
  (`DAT_c0001634`'s exact value, and `at88_i2c_stop`'s real address) - documented as a
  known simplification in the source rather than guessed at.
- `FUN_c0001638`/`FUN_c00016e8`'s own data-phase transfer logic (built on
  `at88_frame_command`, but not separately traced this pass).
- Whether a `$B8` handshake happens anywhere before `FUN_c0001028`'s zone-0 test, given
  zone 0's documented `AR0=0xd5` crypto-auth requirement — still no static callers found
  for `FUN_c0001028` itself, so it's unclear if/when it actually runs.

## `clcdc.c` — status: done

Every function in the confirmed `../clcdc.cpp` address range (anchored by that literal
filename string, its one xref inside the test-pattern generator's unreachable assert
branch) is reconstructed:

- **Register access** (`clcdc_reg_write`/`_set_bits`/`_clear_bits`) — a plain
  set/or/and trio over a base-plus-offset register file, the base hardware-access layer
  everything else sits on.
- **Drawing cursor** (`clcdc_cursor_set_stride`/`_init`/`_init_from_offset`) — a small
  state struct (x, y, wrap margins, row stride) shared by every higher-level draw
  primitive.
- **`clcdc_draw_edge`** — a scanning fill-to-screen-edge line/box primitive with a
  built-in marching-ants animation counter (cycles a colour index every ~1200 calls) -
  most likely a selection-highlight border, not a one-shot line draw.
- **Bitmap font** (`clcdc_font_glyph`/`_advance`, `clcdc_blit_glyph`) — a proportional
  monospace-storage bitmap font with per-glyph advance widths and a genuinely intricate
  sub-byte-pixel-aligned blitter (1/2/3-source-byte shift-and-mask cases depending on x
  alignment) - the external contract is reconstructed with confidence; the dense
  bit-shift arithmetic itself is documented rather than transcribed, to avoid
  introducing a transcription bug in code this fiddly without a way to verify it
  against real hardware.
- **`clcdc_draw_text`** — drives `clcdc_blit_glyph` per character (which both draws
  and returns the advanced cursor - not a separate measure-then-draw pass), plus a
  second, distinct bitmap-test overlay pass (up to 40 rows, 100-byte stride) painted in
  colour index `0xf` - functionally an underline/highlight layer using a different
  bitmap source than the glyphs themselves.
- **`clcdc_test_pattern`** — the built-in 7-mode factory test-pattern generator (solid
  fills, a screen-centre crosshair, SMPTE-style colour bars, a movable cursor line),
  called from a 6-entry dispatch table that's almost certainly the boot/factory-test
  menu's pattern selector. This is the function whose unreachable default-case assert
  is the literal source of the `"../clcdc.cpp"` string reference that anchored this
  entire subsystem.
- **`clcdc_progress_bar`** — a fixed-point horizontal progress indicator drawn directly
  by the panel firmware, independent of the host-driven progress-bar path. Plausibly
  (not confirmed) the bar under the boot splash — `KRONOS_V06R06.VSB.md`'s own splash
  documentation notes a solid green footer band at a consistent screen position.

**Important correction made during this pass**: the address range immediately after
this subsystem (`0xc0015bf8` onward) was initially assumed to be more of `clcdc.cpp`
by proximity, but turned out to be a generic segregated-free-list heap allocator
(malloc/free/sbrk-style, with coalescing and size-class binning) plus a C++ object
destructor that calls into it - shared firmware-wide runtime code, not LCD-specific.
Deliberately excluded from `clcdc.c` rather than left in under a wrong label.

**Genuinely not resolved, left honest rather than guessed:**
- `FUN_c001e3f8`'s exact fixed-point scaling math (`clcdc_progress_bar`'s
  percent-to-pixel-width conversion) - not traced into.
- Which 3 fonts the `DAT_c00157b4` 3-entry table selects between in `clcdc_draw_text`.
- The precise bit-level shift/mask derivation inside `clcdc_blit_glyph` and the exact
  per-direction geometry `clcdc_draw_edge`'s 4 modes set up - both structurally traced
  and behaviorally described, not transcribed as exact arithmetic (see each function's
  own comment for why).

## `cpsoc.c` — status: core done, plus a resolved third SPI device

- **`cpsoc_read_switch_row`/`_read_led_row`/`_read_switch_row_clear`** — confirmed to
  be genuine host-facing wire-protocol entry points, not just internal helpers: they're
  exactly what `FUN_c0007d1c` (the firmware's central command dispatcher, see
  `KRONOS_V06R06.VSB.md`'s new section on it) routes opcode-0 reg `0x50`/`0x51`/`0x52`
  to. All three share the confirmed `0x48` (72) bounds check matching the real 73-entry
  switch/LED table this project already extracted, and dispatch through the same
  `< 0x21` two-register-bank split (`0x79`/`0x7a`) found in the earlier pass.
- **`cpsoc_diag_menu_input`** — the hidden factory diagnostic menu, fully detailed
  (navigation keys, screen layout, a "menu active" latch gating a third action).
- **`cpsoc_event_queue_push`** — a generic 128-entry ring-buffer push primitive with a
  real hard-halt-on-overflow guard, found while chasing this file's own assert call
  sites. **Resolved this pass**: its real, sole caller (`cpsoc_queue_push_validated`,
  see below) gates it by opcode range `0x78`-`0x7b` — genuinely cpsoc-owned, and
  specifically the event queue for the third-device chain below, not the host-facing
  switch/button event queue this section's own earlier guess suggested.

**SPI-device closure pass, 2026-07-17 — a third SPI-bus device, fully resolved:**
what `omap_l108_spi.c` had flagged as an unattributed "third SPI device" (a
never-returning background polling task plus its ADC-read/notify routine) is
cpsoc.c's own code: `cpsoc_analog_poll_task` (never returns; hardware bring-up
sharing `cad_init`'s own reset primitive and `0x9000` SPI config command, then an
infinite loop) drives `cpsoc_analog_poll_channel` (edge-triggered ADC read +
quantize + LED-index notify-on-change) and polls `cpsoc_event_opcode_dispatch` for
opcodes `0x78`/`0x7b` every iteration. That dispatcher logs into a shared history
byte and routes to one of several LED-bargraph handlers — one of which (tag `0x30`)
calls directly into `clcdc_test_pattern`'s own dispatcher, tying this whole chain
into the boot/factory-test menu. The two "set/clear LED" primitives underneath it
all (`cpsoc_led_set`/`cpsoc_led_clear`) collapse to reg `0x79`/`0x7a` — the exact
same two-register-bank split already documented above for
`cpsoc_read_switch_or_led` — the concrete evidence that resolved this attribution.
No `__FILE__` string anchors this address range directly (confirmed via a full
image string search), so — unlike every other subsystem in this project — this
attribution rests on the shared register convention and direct code adjacency to
already-anchored cpsoc.cpp code, not a string-xref proof. Also found along the way:
`crypto_at88.c`'s `FUN_c0005500` was mis-named `crypto_at88_prepare_fault_screen` —
it's actually a generic interrupt-save/disable primitive (`irq_save_and_disable`),
used here as this whole chain's own critical-section entry (paired with
`irq_restore`/`FUN_c0005510`), corrected in that file too.

**Still open:** the underlying SPI submit primitive inside
`cpsoc_queue_command_with_retry` (`FUN_c00032f8`) and its conditional-fault flag's
meaning; most `DAT_` constants in the new section (lookup-table contents, cache-field
addresses); how/where `cpsoc_analog_poll_task` itself gets started as a background
task; the actual I2C-level register semantics behind `0x50`/`0x51`/`0x52`/`0x79`/
`0x7a` (confirmed as a real dispatch pattern, not yet tied to a specific meaning
like "switch pressed" vs "released" vs "LED state"); and whether this section's
lack of a `__FILE__` anchor means it's genuinely part of cpsoc.cpp's own
translation unit (most likely) or a separate one placed adjacently by the linker.

## `ctouchpanel.c` — status: core done

Anchor: `"../ctouchpanel.cpp"` has exactly one xref (like `CryptoAt88.cpp`/`clcdc.cpp` -
a single assert call site), confirming the `0xc0014010-0xc0014f84` address range
(25 functions, first noticed unattributed while mapping `clcdc.cpp`'s boundary in an
earlier pass) as this subsystem's real compilation unit.

- **`ctouchpanel_sample_raw`** — reads 6 raw ADC channels (10-bit readings truncated
  to 8 bits), gated on a validity check against a fixed "no contact" sentinel unless a
  touch is already known active. Six channels is a genuinely higher count than a
  simple 4-wire resistive panel needs - not confirmed which physical signal each maps
  to (pressure/Z-axis sensing is one plausible guess for the extra two, not asserted).
- **`ctouchpanel_check_timeout`** — release-by-timeout debounce, called every
  master-dispatcher tick (same shared `FUN_c0008b64` dispatcher every other subsystem
  ties into): 5 consecutive stale ticks with no fresh sample triggers a forced release.
- **`ctouchpanel_push_event`** — the confirmed anchor function. Genuine coordinate
  jitter/hysteresis filtering: non-touch-down events within +/-4 of the current
  reference position are snapped back to that exact reference before queuing, into a
  128-entry ring buffer with a real hard-fault overflow guard (same shape as, but a
  distinct instance from, `cobjectmgr.c`'s/`crypto_at88.c`'s own ring buffers).
- **`ctouchpanel_update`** — the central down/move/up debounce state machine tying the
  raw sampler to the event queue. Structurally confirmed (debounce counter, 6
  per-channel setter calls on a real transition, release-path call to
  `ctouchpanel_push_event` gated on three validity flags); the six setter calls
  themselves are cited, not individually decompiled - same treatment this project has
  already given to similarly dense per-channel logic elsewhere.

**Still open:** the six per-channel setter functions' own roles (calibration curve?
raw passthrough?); which of the 6 ADC channels correspond to which physical touch-panel
signal; and `FUN_c0014488` (a pure lookup table, code range 5-0x1d) found in this same
address range but also referenced from `cobjectmgr.cpp`'s `FUN_c0007220` - possibly a
shared key-code/index remapping table rather than touch-panel-specific, left
unattributed rather than guessed at.

## `cad.c` — status: done, full calibration engine reconstructed

Anchor: `"../cad.cpp"` has 2 xrefs (`cad_channel_group`'s and `cad_trigger_calibration`'s
own assert calls), confirming the `0xc001335c-0xc0013f5c` range (23 functions) as this
subsystem's real compilation unit - the same "found unattributed while mapping a
neighbor, resolved on this file's own pass" pattern as `ctouchpanel.cpp`.

- **`cad_init`** — subsystem bring-up: hardware handle acquisition, two 38-entry
  calibration/filter state tables reset, 5 expansion-pedal presence probes, a hardware
  reset/config sequence, and - the key finding - **registers this subsystem's own
  opcode handlers (`0x78`, `0x79`) with the shared wire-protocol dispatcher**. This
  closes a loop `cobjectmgr.cpp` and `ctouchpanel.cpp` both left open: both of those
  files' own code already referenced opcodes `0x78`/`0x79` without this project having
  traced where they came from - now confirmed as `cad.cpp`'s own registered handlers.
- **`cad_channel_group`** — a real, disassembly-confirmed 12-channel-to-4-group lookup
  table (not a guessed linear scheme): `{0,2,3,6,9}->0`, `{1,4,5,7,10}->1`, `{8}->2`,
  `{0xb}->3`. Two 5-channel groups (likely knobs and sliders, order not confirmed) plus
  two single-channel groups (likely dedicated pedal jacks).
- **`cad_channel_eligible`** — a 12-channel enable-bitmask check gating calibration
  eligibility per channel.
- **`cad_trim_adjust`** — a real host-facing entry point (called directly from
  `cobjectmgr.cpp`'s own secondary dispatcher for opcode `0x50`), confirming this
  subsystem's wire surface extends beyond the two handlers registered in `cad_init`.
  Notably reuses register value `0x7a` with a *different* local meaning than
  `cpsoc.cpp`'s own `0x7a` register-bank split - confirms `0x7a` is a
  per-subsystem-local register number, not a single firmware-wide constant, worth
  keeping in mind for any future subsystem that also happens to use it.
- **`cad_trigger_calibration`** — the confirmed anchor function: a real host-facing
  calibration trigger with genuine consistency-check asserts (cached-vs-incoming
  register value mismatches are hard faults, not just logged) and a tick-counter-gated
  calibration sequence.

**Calibration-engine pass, 2026-07-17 — all remaining 18 functions decompiled.**
Resolved the full engine `cad_trigger_calibration`'s `0xd0` flag and `cad_channel_eligible`'s
0-vs-4 state selection actually drive:

- **38-slot parallel state** (already partially seeded by `cad_init`, now shown
  coherent): two smoothed-value caches (`+0xe0`/`+0x178`), a "changed" bitmask
  (`+0x4c`), a "masked/excluded" bitmask (`+0x51`), and a threshold/cap pair
  (`+0x80`/`+0xa6`, current cap `+0x5a`).
- **`cad_calibration_sweep`** (`FUN_c0013e50`) — the real per-channel sweep: a 7-step
  SPI sequence for eligible channels, a distinct fixed 3-step sequence for excluded
  ones, each step capturing a sample, finishing with a group-advance + reconfigure.
- **`cad_calibration_smooth_sample`** — a genuine 2-tap debounce filter: keeps
  whichever of two reference caches is closer to the new sample, unconditionally
  updating the other - real hysteresis logic, not a guessed placeholder.
- **`cad_calibration_pop_changed`** — resolves what `omap_l108.c`'s own
  `cad_delay_ticks` note had left as an unconfirmed guess ("pops pending entries"):
  a real rotating bitmask scanner over the `+0x4c` changed-mask, critical-section
  guarded by the same `irq_save_and_disable`/`irq_restore` pair used elsewhere.
- **A real explanation for `cad_init`'s "5 expansion-pedal presence probes" loop**:
  it isn't 5 independent pedal flags - it's seeding the first 5 bytes (40 bits) of
  the `+0x51` exclude-mask, one byte per pedal jack. A channel group whose physical
  expansion pedal isn't plugged in has its whole byte masked out, so
  `cad_calibration_mark_changed`'s own gate silently drops it from the engine
  entirely rather than reporting spurious floating-input readings.
- **A separate small pedal-value-encoding trio** (`cad_pedal_object_reset/_probe/_init`,
  `cad_pedal_send_release`, `cad_pedal_encode_step`) - a distinct, smaller object from
  the 38-slot engine, tied to it only via the shared `cad_pedal_present` probe.
  `cad_pedal_send_release` is a genuine cross-subsystem call: it routes a pedal "off"
  event through `cpsoc.c`'s own `cpsoc_i2c_dispatch` primitive rather than cad's own
  SPI bus.

**Still open:** `cad_calibration_select_slot`/`cad_calibration_advance_group`'s
branch-by-branch real-world meaning (mode 1 vs 2, the 6 step-5 override targets) -
both are dense state machines, structurally cited rather than transcribed, since
the physical calibration semantics behind each branch aren't independently
confirmed; `cad_calibration_init_slot`'s own caller (not traced - `cad_init` itself
hardcodes the same fields directly rather than calling it); the real contents of
`cad_calibration_sweep_table` and several sentinel constants (no data-segment
symbols resolved); and whether the `0xe0` message-tag byte `cad_trigger_calibration`
checks has any relation to the AT88 relay's own `0xe0` opcode or is purely
coincidental reuse.

## `mcasp.c` — status: core done, with a real scope correction

**First genuine anomaly in this project's per-subsystem anchor method**: the top-level
`"../McAspHandler.cpp"` filename string has **zero xrefs anywhere in the image** - every
other subsystem so far (`CryptoAt88`, `clcdc`, `cpsoc`, `cobjectmgr`, `ctouchpanel`,
`cad`) had at least one real assert call site anchoring it. Not resolved which of two
explanations is correct: a genuinely assert-free thin wrapper, or dead code this build
doesn't actually link in. The real functional driver lives in a different, lower-level
compilation unit instead - `"../MCU/Component/OmapL137Mcasp.cpp"` - which has 21 xrefs,
11 of them from one function (`mcasp_init`; corrected from an earlier off-by-one count
during a 2026-07-17 re-verification pass).

- **`mcasp_init`** — the confirmed hardware bring-up sequence for the McASP peripheral
  itself: TX/RX serializer configuration (two parallel, symmetric register blocks),
  6 per-serializer pin-function assignments, and an 11-stage reset/enable sequence
  (set a control bit, poll the matching status bit with a bounded retry, hard-fault on
  real timeout) plus a clock-reconfiguration critical section in the middle. This
  staged bit-set-then-poll pattern matches TI's own documented McASP reset procedure
  closely enough to be confident this is genuinely that sequence, not a coincidental
  resemblance - real hardware IP of this kind requires exactly this kind of
  settling-time polling between reset stages, not a blind fixed delay.
- **`mcasp_reset_stage`** — factored out of the real function's own 11-times-repeated
  inline pattern as a named helper (parameterized by register, bit, and polarity) -
  behaviorally identical to the real disassembly, not a simplification; one stage
  genuinely has inverted polarity (waits for a *different* register's busy bit to
  clear, not set) and is modeled as such rather than forced into the common case.

**Still open:** `mcasp_configure_clock`/`_configure_pins`/`_apply_pin_config`'s own
logic (the sub-config parameter's real structure isn't traced); the three
`mcasp_clock_step_*` calls inside the reset sequence's critical section (plausibly a
PLL divider update, not confirmed); and the real meaning of the 9-vs-10
serializer pin-function split. Also unexplored: `McAspHandler.cpp`'s own likely role as
a higher-level wrapper around this driver, if it exists at all in this build.

## `cdix4192.c` — status: done

Anchor: `"../CDix4192.cpp"` has exactly one xref, the clean single-assert pattern.

- **`cdix_reg_write`/`cdix_reg_read`** — confirmed to sit on the exact same shared
  I2C bit-bang primitives already reconstructed for the AT88 chip in `crypto_at88.c`
  (same low address range as `at88_frame_command`'s own callees) - real evidence
  `I2cByGpio.cpp` is a genuinely shared bus driver, not private to either chip. Fixed
  I2C address `0x70` for every access.
  **Correction (re-verification pass, 2026-07-17)**: the `chip` parameter these two
  wrappers appear to take is NOT actually forwarded to the I2C primitive underneath -
  the real disassembly overwrites it before use and passes an uninitialized local
  stack buffer instead. See `cdix4192.c`'s own updated comment for the full note;
  left genuinely unresolved whether this is a real firmware quirk or a sign the
  functions never took a chip/context parameter at the source level at all.
- **`cdix_configure_and_verify`** — the confirmed anchor: walks a `{register, value}`
  table twice - once writing every entry, once reading every entry back and
  hard-faulting on the first mismatch. A real write-then-verify configuration
  sequence, not a blind init. **Zero static callers found** - the same "reached only
  via indirect dispatch" pattern already seen for `cobjectmgr.c`'s object destructor
  and `CryptoAt88.cpp`'s own self-test, suggesting `EvaBoardMain.cpp`'s own bring-up
  walks some kind of driver probe/init table rather than calling each subsystem's
  entry point directly - now directly confirmed by `eva_board_main.c`'s own
  init-table-walker finding.

**Still open, partially resolved (re-verification pass, 2026-07-17):** the config
table (`DAT_c000fc40`, real address `0xc001fb6c`) is confirmed 4-byte-stride
`{reg, value, pad, pad}`, `0xff`-terminated, with exactly **3 real entries**:
`{0x7f, 0x00}`, `{0x03, 0x29}`, `{0x04, 0x03}` - genuine register numbers/values now
known, though which physical DIX4192 function each register controls (sample rate,
format, clock source) is still not decoded; the two padding bytes' meaning also still
open.

## `cobjectmgr.c` — status: core done

Anchor: `"../cobjectmgr.cpp"` has 6 real xrefs - a less clean boundary than
`CryptoAt88.cpp`'s or `clcdc.cpp`'s single-assert anchors. Only the functions with a
genuinely confirmed "object manager" role are reconstructed in `cobjectmgr.c` itself;
the other three anchor xrefs are recorded here rather than forced into that file under
a label they may not deserve.

- **`cobjectmgr_tick`** — the confirmed core: called unconditionally every single
  master-dispatcher tick (`FUN_c0008b64`, not gated by any status bit, unlike most of
  that dispatcher's other fan-out targets). Polls a single "current object" slot; if
  occupied, dispatches on a one-byte type tag to one of exactly two handlers (any other
  tag is a hard fault), then clears the slot and releases the object. A genuinely
  minimal "poll the one active managed object, process it, release it" pattern, not a
  general allocator or registry. Independently of the object-slot handling, also polls
  a hardware-ready status bit every tick and increments a counter when set.
- **`cobjectmgr_object_destroy`** — a real C++ virtual destructor (walks a 15-bucket
  child-widget hash table and a second linked list, freeing every entry via the shared
  heap allocator found at `clcdc.cpp`'s own boundary, then conditionally calls a
  vtable-style function pointer before possibly recursing). **Zero static callers
  found** - now resolved: see `eva_board_main.c` below, this is very likely one of the
  entries `eva_board_init_table` walks indirectly, exactly the mechanism that explains
  every other "zero static callers" finding in this project. Documented as a confirmed
  structural finding, not transcribed as executable C (needs the still-unresolved heap
  allocator's exact struct layout to express faithfully).

**Three other anchor xrefs found but deliberately NOT included in `cobjectmgr.c`,
each its own honestly-scoped finding:**
- `FUN_c0005c50` — a host-notify event sender; confirmed to share `FUN_c000acec` (the
  generic USB-send/event-submit primitive) with `crypto_at88.c`'s own AtmelRead event
  path - real evidence `FUN_c000acec` is a shared, generic primitive, not AT88-specific.
- `FUN_c00090b8` — a hardware-fault watchdog task: an infinite loop whose only exit
  path is its own internal assert call. Now load-bearing: `eva_board_main.c`'s
  `eva_board_watchdog_fault_wrapper` calls this directly and treats any return from it
  as itself a fault condition, consistent with this function's own structure.
- `FUN_c0007220` — a large secondary protocol dispatcher, possibly a debug console;
  confirmed as the real call site for `cad.c`'s own `cad_trim_adjust` (opcode `0x50`),
  and referencing the same lookup table (`FUN_c0014488`) `ctouchpanel.c` also uses -
  not yet reconstructed as its own file.

**Still open:** `cobjectmgr_handle_type_a`/`_type_b` (the two tag-dispatched handlers
themselves); `cobjectmgr_release_object`/`_object_cleanup`'s own logic; and
`FUN_c0007220`'s full scope (a real candidate for its own future subsystem file rather
than a `cobjectmgr.cpp` addendum).

**Re-verification pass, 2026-07-17**: every structural claim in this file checked out
exactly against fresh decompiles (the 6-xref anchor count, `cobjectmgr_tick`'s
unconditional call from the master dispatcher, `cobjectmgr_object_destroy`'s zero
callers). One minor omission found, now noted: `cobjectmgr_object_destroy` has an
early-return guard (`if (obj == a fixed self-reference constant) return;`) not
mentioned in this file's own structural description - not a wrong claim, just an
incomplete one.

## `eva_board_main.c` — status: core done, ties the whole project together

Anchor: `"../EvaBoardMain.cpp"` has exactly one xref, inside the fault-wrapper function
below - but unlike every other subsystem this project has covered, **Ghidra's
auto-analysis never assigned a containing function boundary to any of this code**
(`get_function_info` on `0xc0005670` returns "No function found"). Everything here was
read directly from raw disassembly (`0xc0005610`-`0xc0005698`), not through
`decompile_function`.

- **`eva_board_init_table`** — a generic function-pointer-table walker: iterates a
  table from a fixed start pointer to a fixed end pointer (`DAT_c0005664`/
  `DAT_c0005668`), skipping NULL entries, calling every non-NULL entry via a manual
  ARM call sequence (`mov lr,pc; cpy pc,r3`).
  **CORRECTION (SPI/USB cleanup pass, 2026-07-17)**: this file originally claimed the
  table explained every "zero static callers" finding across the whole project
  (`crypto_at88_self_test`, `cdix_configure_and_verify`, `cobjectmgr_object_destroy`).
  Having now actually READ the table's contents via `read_memory` rather than just its
  start/end pointers, this is wrong: the table has exactly **ONE entry**
  (`eva_board_init_table_start` holds `0xc0098f54`, `eva_board_init_table_end` holds
  `0xc0098f58` - a 4-byte, i.e. one-entry, span), pointing to
  `eva_board_init_table_entry_0` (`FUN_c0009168`), a lazy-init singleton wrapper
  around a byte-zero (`FUN_c0005720`) and a genuine 4-byte no-op stub (`FUN_c0011814`,
  confirmed via disassembly to be a bare `mov pc,lr`). None of the three
  previously-guessed zero-caller functions are called through this table. Their real
  invocation mechanism remains genuinely unresolved - the "ties the whole project
  together" framing overreached, and is retracted.
- **`eva_board_main`** — the function containing the table walker, immediately followed
  by the real main loop: after the init table finishes, one more setup call
  (`eva_board_final_setup`, role not traced) and a call with argument pair `(1, 4)`
  (`eva_board_start_task` - plausibly spawning a background task, the watchdog task
  found in `cobjectmgr.c` is the leading candidate, not confirmed), then an unconditional
  `for(;;)` loop calling the master wire-protocol dispatcher (`FUN_c0008b64`, already
  reconstructed via its call sites in `crypto_at88.c`/`clcdc.c`/`cobjectmgr.c`) forever.
  This is, as far as this project has traced, the actual loop the entire rest of the
  firmware runs inside of after boot.
- **`eva_board_watchdog_fault_wrapper`** — the confirmed anchor. Calls the hardware-fault
  watchdog task (`cobjectmgr.c`'s `FUN_c00090b8`), and if that call ever actually
  returns - which, per that function's own confirmed structure, should never happen
  under normal operation - unconditionally raises a separate hard fault right here,
  with a fixed line number (`0x6d` = 109). Defensive, belt-and-suspenders code, not a
  normal code path.
  **Correction (re-verification pass, 2026-07-17)**: originally documented as taking
  and forwarding a `handle` parameter - re-verified against fresh disassembly and
  found WRONG. The function takes no effective parameter; the argument to
  `FUN_c00090b8` is loaded from a fixed literal (`DAT_c0005698`) that happens to hold
  the same runtime value as `eva_board_handle`, not from an incoming register. The
  exact same "phantom forwarded parameter" pattern was independently found the same
  pass in `cdix4192.c`'s register wrappers - worth treating as a real, recurring
  transcription risk in this project rather than a one-off.

**Still genuinely open:** `eva_board_final_setup` (`FUN_c00074bc`) and
`eva_board_start_task` (`FUN_c001cfd8`, args `(1, 4)`)'s own real roles; the real
invocation mechanism for `crypto_at88_self_test`/`cdix_configure_and_verify`/
`cobjectmgr_object_destroy` (now confirmed NOT the init table); and why Ghidra's
auto-analysis failed to bound this region as functions at all - worth understanding
before trusting `decompile_function` output elsewhere in low addresses near
`0xc0005600`-`0xc00056a0`.

## `omap_l108.c` — status: core done

Anchor: `"../MCU/OmapL108.cpp"` has exactly one xref, inside `omap_tick_init`'s own
init-guard assert.

- **`omap_tick_init`/`omap_tick_read_raw`/`omap_tick_elapsed_scaled`** — a small, generic
  free-running-tick-counter API: init-once guard + baseline snapshot, a raw counter
  read, and an elapsed-time query with wraparound correction that converts through a
  shared fixed-point scaler.
- **Cross-file correction to `clcdc.c`**: `FUN_c001e3f8`, previously flagged there as
  *"`clcdc_progress_bar`'s exact fixed-point scaling math, not traced into"*, is
  confirmed here as a SECOND, unrelated caller of the same function
  (`omap_tick_elapsed_scaled` uses it too, divisor `0x96`/150) — it's a generic,
  firmware-wide tick-to-unit scaler, not clcdc-specific. `clcdc.c`'s own note should be
  read with this correction in mind.
- **`cad_delay_ticks`** (`cad.c`'s own name, `FUN_c00085a8`) is documented here instead
  of in `cad.c`, since it's built almost entirely out of this file's tick-timer
  primitives, with one cad.c-specific addition: it's genuinely **not a busy-wait** — it
  bounds the requested delay, arms a tick baseline, then loops servicing
  `cobjectmgr_tick` (already reconstructed) and a cad.c-internal calibration-progress
  pump (`FUN_c0005a1c`, itself confirmed to route a 4-byte record through the same
  shared `FUN_c000acec` USB-submit primitive `crypto_at88.c`/`cobjectmgr.c` already
  established, and to draw progress text via `clcdc.c`'s own text primitive) until the
  elapsed time reaches the target. A cooperatively-scheduled "delay without missing
  dispatch events" pattern, not a hardware sleep.

**Still open:** the real identity of the fixed constants this file's `DAT_` references
stand in for (no data-segment symbols resolved in this ELF-wrapper import); whether the
raw tick counter is a hardware register or a software counter incremented by an ISR;
and `cad_calibration_progress_pump` (`FUN_c0005a1c`) itself, deliberately left
unreconstructed as a future `cad.c` addendum rather than pulled into this file.

## `omap_l108_spi.c` — status: done, plus a resolved third SPI device

Anchor: `"../MCU/Component/OmapL108Spi.cpp"` has 2 xrefs, both hard-fault call sites
inside the one function this file amounts to.

- **`omap_spi_write`** — a single SPI transmit primitive: two busy-wait phases against
  the same status register testing **two different bits**, not the same condition
  twice (bit `0x80000000` must SET, then a separate bit `0x20000000` must CLEAR),
  each independently retry-bounded and hard-faulting on real timeout, followed by a
  read-modify-write of the low 16 bits of a combined control/data register.
- **Confirmed shared bus finding**: this exact primitive has 8 real call sites across
  `cad.c` (`cad_trigger_calibration`, `cad_init`, and an undecompiled per-channel
  function, `FUN_c0013e50`) and two functions in the cpsoc.c address range — the
  analog chip, the PSoC scan chip, and cpsoc.c's own separate third-device
  analog-polling chain all share one physical SPI bus and one low-level transmit
  primitive, the SPI counterpart of `I2cByGpio.cpp`'s already-established
  shared-I2C-bus role.

**SPI-device closure pass, 2026-07-17 — fully resolved and moved to `cpsoc.c`**: the
two functions this section used to flag as an unattributed "third SPI device"
(`FUN_c0011624`/`FUN_c0011534`) are cpsoc.c's own code —
`cpsoc_analog_poll_task`/`cpsoc_analog_poll_channel`. See that file's own new
section for the full writeup (the opcode router's 4 sub-handlers, the LED-bargraph
handlers underneath, and the register-encoding evidence that resolved the
attribution). This file's own scope is back to just `omap_spi_write` itself.

**Still open:** real bit-level meaning of the two status bits (usage-inferred, not
datasheet-confirmed); the retry-delay primitive itself; whether a corresponding SPI
read function exists elsewhere in the image (not searched for this pass).

## `omap_l137_usbdc.c` — status: core done

Anchor: `"../MCU/Component/OmapL137Usbdc.cpp"` has 3 xrefs — two inside
`omap_usbdc_init_ep0`, one inside `omap_usbdc_poll_transfer`.

- **`omap_usbdc_init_ep0`** — control-endpoint (endpoint 0) bring-up: three
  register-block pointers derived via a shared relocation helper, a mode-field write,
  a bounded ready-wait, a real hardware reset sequence (set/settle/clear/reconfigure a
  control register, bounded wait on a completion bit with a hard-fault on timeout), and
  endpoint-0 max-packet-size/transfer-type field setup. Called from a higher-level USB
  object bring-up function (`FUN_c0009574`, only partially traced) that allocates the
  same register-block offsets before calling in.
- **`omap_usbdc_poll_transfer`** — the real bottom of a call chain traced across this
  entire project: a 3-state (idle → in-flight → complete) transfer-completion state
  machine gated on an 8001-byte size threshold, reached via a thin wrapper
  (`FUN_c000acc8`) sitting **directly adjacent to** `FUN_c000acec` — the generic
  USB-submit primitive already confirmed shared by `crypto_at88.c`'s AtmelRead event,
  `cobjectmgr.c`'s host-notify event, and `cad.c`'s calibration-progress pump. This
  closes the loop: every subsystem's "send an event to the host" call, for large-enough
  payloads, ultimately drives this exact hardware endpoint state machine.

**Still open:** most `DAT_` constants (no data-segment symbols available to resolve
fixed register offsets/thresholds further); `FUN_c0009574`'s own full scope; whether the
8001-byte threshold is a real hardware DMA/FIFO limit or a firmware policy choice; and
the exact code path between `FUN_c000acc8` and `FUN_c000acec` (confirmed adjacent and
part of the same call chain, not fully mapped instruction-by-instruction).
