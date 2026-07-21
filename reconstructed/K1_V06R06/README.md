# NKS4 Panel Firmware - reconstructed source

Reverse-engineered source for `KRONOS_V06R06.VSB`, the firmware for the NKS4 front-panel
board itself (USB `0944:1005`) - a physically separate embedded system (TI OMAP-L1x,
ARM926EJ-S) from the Kronos's own x86 host. See
[`kronosology/docs/modules/KRONOS_V06R06.VSB.md`](../../docs/modules/KRONOS_V06R06.VSB.md)
for the container format and load-address derivation this reconstruction is built from.

This is the other end of the wire from
[`kronosology/reconstructed/OmapNKS4Module/`](../OmapNKS4Module/), the host-side kernel
driver that talks to this firmware over USB. A separate userspace client outside this
repository also implements the same wire protocol.

## Scope

Earlier passes at this firmware extracted only what was needed for specific downstream
consumers - a switch/LED name table, the boot splash location, one AT88 self-test trace -
rather than attempting systematic decompilation. This reconstruction instead covers the
image as a whole: all 691 real functions Ghidra identifies in the binary have a genuine,
compilable C definition somewhere in this directory (53 files total).

Full function coverage means every address Ghidra recognizes as a function boundary has
real, reconstructed C - it does not mean every line is proven bit-identical to the
binary's runtime behavior. Many files carry their own "still open" notes (unresolved
`DAT_` constant identities, a handful of functions where the decompiler could not
distinguish two different-sized bodies, register-level semantics inferred rather than
datasheet-confirmed). These are documented per file below and in each file's own header
comment rather than smoothed over.

This directory is also the K1 reference baseline for the sibling
[`K2_V01R10/`](../K2_V01R10/) project (Kronos 2's own panel firmware,
`KRONOS2S_V01R10.VSB`) - see that directory's own README for its port/validation status.

## Ghidra analysis setup

The raw 917504-byte payload (file offset `0x100..EOF`, after the 256-byte "KORG SYSTEM
FILE" header) must be wrapped in a minimal single-`PT_LOAD` ELF32 ARM header
(`e_machine=40`, entry=vaddr=`0xC0000000`) for Ghidra's ELF importer to auto-select
`ARM:LE:32:v8` and place everything correctly - the raw-binary importer path fails
silently. See `kronosology/docs/workflow/ghidra_setup.md` for general session conventions.

When running many analysis passes over this image at once, prefer a pre-fetched static
decompile/data dump per pass over live per-call queries against one shared Ghidra
session - concurrent live queries against a single session have produced incorrect
results.

## Architecture

**Boot chain.** The ARM exception vector table's reset entry (`0xC0000000`) branches
through its own literal pool to `eva_board_reset_handler` (`0xc0009534`), a 3-instruction
function (load initial SP, install it, tail-call into `eva_board_crt0`, never returns).
`eva_board_crt0` (`0xc00055b8`) is a classic embedded crt0: zeroes a BSS-style region,
runs 11 back-to-back per-subsystem/scheduler-table bring-up calls (one of which populates
the task-control-block table the scheduler depends on), then falls directly into the
scheduler idle/dispatch tail with no return.

`eva_board_main` is the actual boot entry point: it calls `eva_board_init_table` (a
generic function-pointer-table walker with exactly one real entry, a lazy-init singleton -
not a multi-function driver-probe table), then `eva_board_final_setup` (a per-subsystem
hardware bring-up sequencer covering twelve further subsystems, gated by a hardware
compatibility/self-test check), then `eva_board_start_task(1, 4)` (a real scheduler
primitive - marks task index 1, 1-based, ready at priority level 3; not a generic
spawn-with-function-pointer call), then an unconditional `for(;;)` loop calling
`master_dispatch_tick` forever.

**Two central dispatch entry points**, reached via independent paths and neither anchored
by its own `__FILE__` string (both were identified by size, role, and cross-file address
confirmation instead):

- `wire_dispatch_command` (`FUN_c0007d1c`, 1904 bytes) - the single entry point for every
  incoming 32-bit command word from the host over USB. Decodes an opcode byte and routes
  to nearly every subsystem in this reconstruction. Reached via the USB bulk-OUT receive
  path (`usbdc_ep_recv_bulk` in `omap_l137_usbdc_ext.c`) and via a second call site inside
  the core USB ISR (`usbdc_core_isr`) on the EP0 SETUP-pending path - not called directly
  from `eva_board_main`'s main loop.
- `master_dispatch_tick` (`FUN_c0008b64`, 1240 bytes) - the status-bit dispatcher called
  unconditionally, forever, from `eva_board_main`'s main loop. Polls an event-flag group
  and, per set bit, drains various subsystem queues (AT88 relay, cpsoc, panel I2C bus, cad
  calibration progress) and pumps the USB transfer-completion state machine forward by
  around 2000 bytes per tick until complete.

## Subsystems

| Subsystem | Real source file (per embedded `__FILE__` strings) | Reconstructed as |
|---|---|---|
| AT88 crypto-chip relay | `../CryptoAt88.cpp` | `crypto_at88.c` |
| I2C bit-bang primitives | `../I2cByGpio.cpp` | `i2c_by_gpio.c` |
| LCD controller | `../clcdc.cpp` | `clcdc.c` |
| PSoC button/LED scan + I2C0 submit primitives + queue/dispatch plumbing | `../cpsoc.cpp` | `cpsoc.c` |
| Touch panel | `../ctouchpanel.cpp` | `ctouchpanel.c` |
| Analog (knobs/sliders/pedals) | `../cad.cpp` | `cad.c` |
| Audio serial port | `../McAspHandler.cpp` (no xrefs) / `../MCU/Component/OmapL137Mcasp.cpp` | `mcasp.c` |
| Digital audio interface | `../CDix4192.cpp` | `cdix4192.c` |
| Main entry / board bring-up | `../EvaBoardMain.cpp` | `eva_board_main.c` |
| Object manager | `../cobjectmgr.cpp` | `cobjectmgr.c` |
| Shared heap allocator | no `__FILE__` anchor - code-shape evidence | `heap_alloc.c` |
| SoC tick timer | `../MCU/OmapL108.cpp` | `omap_l108.c` |
| SPI peripheral (shared cad/cpsoc bus) | `../MCU/Component/OmapL108Spi.cpp` | `omap_l108_spi.c` |
| USB device controller | `../MCU/Component/OmapL137Usbdc.cpp` | `omap_l137_usbdc.c` |
| USB ISR/poll handler, endpoint-event dispatch, low-level register/FIFO/descriptor layer | no `__FILE__` anchor - shared field-offset evidence with `omap_l137_usbdc.c` | `omap_l137_usbdc_ext.c` |
| Second (hardware) I2C bus / internal command channel | no `__FILE__` anchor - code-shape + hardware evidence | `panelbus_dispatch.c` |
| Master wire-protocol dispatch loops | no `__FILE__` anchor - code-shape + cross-file evidence | `wire_dispatch.c` |

## `crypto_at88.c` - AT88 crypto-chip relay

- The `chip`/`void *` handle argument threaded through every function in this file is
  dead. There is exactly one hardwired SDA/SCL GPIO pin pair (bit `0x40000`/`0x80000` off
  two fixed GPIO-object globals); `at88_gpio_set_sda`/`set_scl`/`set_sda_dir` and
  `at88_delay` all ignore whatever pointer they are handed and drive that same fixed pair
  unconditionally. `cdix4192.c` and `eva_board_main.c` show the same "phantom forwarded
  parameter" pattern - all three are downstream consumers of one dead-argument layer. The
  canonical reconstruction of that shared low-level layer lives in `i2c_by_gpio.c` (same
  addresses); this file's own copies exist for citation continuity, not as a divergent
  duplicate.
- `at88_frame_command`'s retry bound is 19999 (`DAT_c0001634`). No `at88_i2c_stop()` call
  exists anywhere in the retry loop - bus release happens once, in the caller, after the
  whole transaction completes.
- `at88_i2c_write`/`at88_i2c_read` implement the full data-phase transaction, including an
  `at88_lock`/`at88_unlock` critical section - re-entering while already locked raises a
  hard firmware fault citing `"../I2cByGpio.cpp"` (not `"../CryptoAt88.cpp"`), showing this
  lock belongs to the I2C bit-bang layer's own translation unit, not this one.
- `crypto_at88_self_test` has zero static callers anywhere in the full 691-function
  cross-reference data, including inside `eva_board_main`'s own board bring-up path. It is
  either dead code left over from a factory-test build, or reached only through a
  mechanism static analysis cannot see. *Open item; to validate: search for a
  function-pointer table entry or vtable slot resolving to this address, or trigger the
  self-test on real hardware and trace back to a caller.*
- `crypto_at88_process_queue`'s producer is `wire_dispatch_command` (`wire_dispatch.c`):
  host opcode `0xE0` (AtmelWrite) and `0xE1` (AtmelRead) each push a queue entry and, on
  success, set status bit 13 via a generic event-flag primitive; `master_dispatch_tick`
  sees that bit and calls `crypto_at88_process_queue` to drain the queue over I2C.
- The write path's completion notification is fire-and-forget at both the queue-relay
  level and one level down inside `at88_i2c_write` itself - neither ever calls an
  event-relay function. A successful `$B0`/`$B4`/`$B8` write produces no host-visible
  completion event.
- `at88_relay_read_result`'s wire format needs byte-reversal on word 1 and on the trailing
  payload loop (not a straight copy). The trailing loop's real continuation condition is
  `i < len`, not `i + 4 <= len`: the queue producer's own 33-byte length gate exceeds
  `struct at88_queue_entry`'s real 28-byte data capacity, so for `len` in the high-20s both
  the real producer and this function walk a few bytes past the nominal buffer - a genuine
  firmware quirk, transcribed faithfully rather than clamped.

**Open items:** `at88_i2c_bus_reset`'s caller (`crypto_at88_queue_init`) passes an
uninitialized stack buffer as `chip` at its one call site (harmless, same dead-chip
pattern as above); a handful of `at88_delay` unit-value literals whose real timing is not
decoded.

## `i2c_by_gpio.c` - shared GPIO bit-bang I2C driver

Reconstructs the shared GPIO bit-bang I2C driver (`../I2cByGpio.cpp`, 13 functions), the
primitive layer `crypto_at88.c` and `cdix4192.c` both sit on.

- Anchor: the literal string at `0xc0022cf8`, reached through
  `i2c_gpio_busy_guard_enter`'s fault call on a bus-already-busy condition.
- The whole bus is a hardware singleton: one fixed GPIO bank (base `0x01E26000`, a TI
  OMAP-L138/DA850 GPIO controller address), SCL = bank-0 bit 19, SDA = bank-0 bit 18. The
  `chip` parameter every top-level caller threads through is dead both at its source (at
  least four independent top-level call sites pass an uninitialized local stack buffer
  instead of a real handle) and at its sink (nothing in this file's own GPIO-level
  functions reads it).
- `i2c_gpio_stop` (`0xc00013cc`) is `crypto_at88.c`'s `at88_i2c_stop` - same address,
  called exactly once by every transaction-level entry point here, right before clearing
  the busy guard.
- `i2c_gpio_frame_command`'s retry bound (19999, `0x4e1f`) matches `crypto_at88.c`'s
  independently-derived value exactly. The CDIX-side `i2c_gpio_addr_start` uses a much
  smaller 5-retry bound - a real asymmetry between the two devices' post-write poll
  behavior, not a transcription inconsistency.
- `i2c_gpio_read_reg8` (= `cdix4192.c`'s `cdix_i2c_read_reg`) always returns 0, regardless
  of whether any internal ACK/NACK phase actually succeeded - consistent with
  `cdix_reg_read` (`cdix4192.c`) never checking this return value.
- `gpio_bank_get_base` (`FUN_c0001990`) and `hw_timer_busy_wait` (`FUN_c0001aa0`) are left
  `extern` rather than reconstructed here - both have far more callers (66 and 16
  respectively) spanning the entire firmware image than would fit a driver private to this
  file.

**Open items:** `i2c_gpio_delay`'s parameter wiring into the underlying hardware timer
engine is behaviorally plausible but not verified by register-level disassembly; bit 11
(`icmdr |= 0x800`), touched at the end of every transaction, has undecoded meaning. *To
validate: cross-reference against the OMAP-L138 I2C controller TRM for ICMDR bit 11.*

## `clcdc.c` - LCD controller

Anchored by `"../clcdc.cpp"`'s single xref inside the test-pattern generator's
unreachable assert branch. The real compilation-unit boundary ends at `clcdc_blit_glyph`
(`0xc0015afc`); the address range up to `0xc0015bf8` belongs to `cobjectmgr.c`
(`cobjectmgr_free_list_recursive`, `cobjectmgr_object_destroy`) and three trivial
"zero a global" stubs called from elsewhere, not to this file.

- **Register access** (`clcdc_reg_write`/`_set_bits`/`_clear_bits`) - a plain set/or/and
  trio over a base-plus-offset register file.
- **Drawing cursor** (`clcdc_cursor_set_stride`/`_init`/`_init_from_offset`) - a small
  state struct (x, y, wrap margins, row stride) shared by every higher-level draw
  primitive.
- **`clcdc_dispatch_set_palette_hook`** - a one-line wrapper over a shared RGB->RGB565
  palette-entry-set primitive; its sole caller is `wire_dispatch_command`'s opcode `0xc5`
  handler.
- **`clcdc_draw_edge`** is a one-parameter function. Direction, inset distance, and colour
  index are all read from three self-mutating global counters the function itself
  advances every call - a fully self-driven "marching ants" selection border that shrinks
  from the screen edge to centre (799/2, 599/2) over 301 side-cycles, then advances to the
  next highlight colour and starts over. It has zero static callers - almost certainly
  driven by a timer/tick ISR. *To validate: locate the ISR or timer callback table entry
  that invokes it.*
- **Bitmap font** (`clcdc_font_glyph`/`_advance`) - a proportional monospace-storage
  bitmap font with per-glyph advance widths. Offset `+0x05` is a per-glyph scanline count
  (`glyph_rows`), not a `first_char` bound (that bound is a hardcoded literal in code,
  never backed by a struct field).
- **`clcdc_blit_glyph`** rasterizes each glyph into a shared 1bpp work-bitmap (100-byte
  row stride), not the visible 16bpp framebuffer directly. The destination plane is
  strongly suspected, by matching stride and addressing, to be the same plane
  `clcdc_draw_text`'s second pass reads from, though this is not independently confirmed
  by a live pointer comparison. Handles sub-byte x alignment via a 1/2/3-destination-byte
  shift-and-mask scheme.
- **`clcdc_draw_text`** drives `clcdc_blit_glyph` per character, then a second pass
  composites the shared 1bpp plane into the real framebuffer. The 3-entry font table
  (`DAT_c00157b4`) is a genuine `const struct clcdc_font *` array. The second pass is a
  full background-fill-plus-highlight composite of the whole text bounding box (every
  pixel in the box is unconditionally repainted), not a sparse highlight-only overlay.
- **`clcdc_test_pattern`** - the built-in 7-mode factory test-pattern generator, called
  from a sequential if/else-if chain (not a dispatch table), with an exact
  key-code-to-mode mapping (`0x1f`-`0x25`).
- **`clcdc_progress_bar`** - `omap_tick_scale`/`FUN_c001e3f8` is a generic
  signed-division utility shared firmware-wide (also used by `omap_l108.c`'s own
  `omap_tick_elapsed_scaled`), not percent-specific. Scaling math is
  `width_px = percent*512/100`, truncating; the mirrored-row offsets are rows 348/349,
  exactly one row (800px) apart.

**Open items:** which 3 fonts `clcdc_font_table`'s entries actually point to (a
runtime-populated pointer array, zeroed in a static image dump - *to validate: read the
table live at runtime, or from a full memory snapshot rather than the static firmware
image*); the precise bit-level shift/mask derivation inside `clcdc_blit_glyph` and the
exact per-direction geometry `clcdc_draw_edge`'s 4 modes set up, both transcribed
faithfully but not independently verified against real hardware; `DAT_c0015b00`'s (the
blit-clip-bound) actual numeric value.

## `cpsoc.c` - PSoC button/LED scan + I2C0 submit primitives + queue/dispatch plumbing

Anchored by `"../cpsoc.cpp"` at `0xc0023190`, referenced from several `DAT_` fault-call
file arguments (`cpsoc_event_queue_push`, `cpsoc_queue_push_validated`,
`cpsoc_queue_command_with_retry`, `cpsoc_analog_poll_channel`, and
`cpsoc_dispatch_tick`'s own family).

- **`cpsoc_i2c_dispatch`** (the callee behind every host-facing switch/LED-row read,
  opcodes `0x50`/`0x51`/`0x52`) does not perform a live I2C access. It enqueues into the
  same 4-instance ring buffer the LED-bargraph chain uses, then posts event-flag bit
  `0x1000` - a host-facing switch/LED-row read is asynchronous, not synchronous hardware
  access.
- **`cpsoc_dispatch_tick`** (`FUN_c0010f08`) is the consumer for both the host-facing
  queue and the LED-bargraph queue: called directly from `master_dispatch_tick` on status
  bit `0x1000`, draining/reading every queue instance in one pass. Register `0x79` is
  only ever drained (write-queue side); register `0x7b` is only ever read live - a real,
  documented asymmetry.
- **`cpsoc_i2c0_submit_write`/`_read`** (`FUN_c00032f8`/`FUN_c00033f0`) are a bounded
  busy-wait byte-stream submit/read primitive over the real on-chip I2C0 controller, not
  a SPI device as an earlier pass had them named and documented. `FUN_c00033f0` and
  `panelbus_dispatch.c`'s `panelbus_i2c_read_bytes` are the literal same function at the
  same address, independently transcribed twice under two names. Both sides fetch their
  register-block handle via `FUN_c0001a00` (== `panelbus_dispatch.c`'s
  `panelbus_i2c_base`), whose literal pool resolves to `DAT_c0001a14 = 0x01e28000` (I2C1)
  and `DAT_c0001a18 = 0x01c22000` (I2C0) - the real TI OMAP-L138/AM1808 I2C1/I2C0
  peripheral base addresses; every real call site on both sides passes selector `0`, i.e.
  I2C0. By contrast, `cpsoc.c`'s other handle selector, `cpsoc_get_handle`
  (`FUN_c0001a1c`), has its own different literal pool (`0x01f0e000`/`0x01c41000`,
  matching neither OMAP-L138 I2C base) feeding `omap_spi_write` for the same device's ADC
  channel reads - genuinely SPI, unaffected by the above.
- The four LED-bargraph tag handlers (`cpsoc_led_cycle`/`_toggle`/`_ramp`/`_quantize`)
  all resolve to the same literal field, `cpsoc+0x821`, not four independent struct
  fields.
- **`cpsoc_event_queue_pop`** resolves the full 0x208-byte per-instance ring layout (128
  slots + write-index + read-index + count).
- **`panel_gpio_reset_pulse`** (`FUN_c0000ba0`) is a real GPIO bank-3/bit-8
  assert-hold-deassert pulse (60000-tick hold), independently reachable both from the
  diagnostic menu's key-8 action and from `wire_dispatch_command`'s own opcode-9 handler -
  the same logical reset action reached two ways.
- `panelbus_rx_dispatch_loop` (`0xc0010b58`, `panelbus_dispatch.c`) is also independently
  reconstructed here as `cpsoc_poll_reg_reads` - same address, same real call shape, two
  separately-transcribed C bodies, left as two definitions rather than silently unified
  per this project's convention of flagging cross-file duplicates.

**Open items:** `cpsoc_analog_poll_task`'s own invocation mechanism (never returns, zero
static callers; two sibling never-returning loops with the identical signature exist and
are similarly unattributed - a circumstantial lead, not a resolved mechanism. *To
validate: look for a task-table or scheduler entry pointing at one of these three
addresses.*); most `DAT_` lookup-table contents.

## `ctouchpanel.c` - touch panel

The anchored range `0xc0014010-0xc0014f84` has 23 real function objects with no
unaccounted address gaps (2 addresses once thought to be functions are literal-pool
data: switch-table constants and a global config-object pointer).

- **`ctouchpanel_sample_raw`** - reads 6 raw ADC channels into a 7-byte record, gated on
  a validity check unless a touch is already active.
- **`ctouchpanel_watch_idle_scalar`** (previously misnamed `ctouchpanel_finalize_release`)
  reuses `cad.c`'s own shared 2-tap smoothing primitive at CAD's own reserved/unused slot
  `0x1e` (30) against its own local scratch fields, not CAD's 38-slot engine - a "borrow a
  shared primitive, keep the data local" pattern; `cad.c`'s own documentation of slot 30
  as excluded from its sweep is independent corroboration.
- **`ctouchpanel_check_timeout`** - release-by-timeout debounce, 5 consecutive stale
  ticks triggers a call into `ctouchpanel_watch_idle_scalar`.
- **`ctouchpanel_push_event`** - the anchor function: a 128-entry ring buffer with +-4
  jitter/hysteresis snapping and a hard-fault overflow guard.
- **`ctouchpanel_pop_event`** - the dequeue counterpart, called every master-dispatcher
  tick from a touch-event consumer outside this file's range that relays events to the
  host via the same shared USB-submit primitive used elsewhere in this project.
- **`ctouchpanel_update`** - the central down/move/up debounce state machine: a global
  3-consecutive-sample arming debounce, a 6-per-channel-setter call order (ch0/ch1 are raw
  X/Y, ch2-5 feed the calibration-bracket references - not a second axis pair or
  pressure/Z sensing), and a release path where an invalid sample, whether or not armed,
  falls to a shared tail that pushes one final release event and clears state.
- Four small per-channel bracket setters and two large compute-and-push functions are
  fully transcribed, including a real asymmetry: the Y axis's flip is conditional on a
  global config flag, the X axis's own flip is unconditional.
- `FUN_c0014488` (the shared 5-entry `0x1d` lookup table) is called by
  `wire_dispatch_command` (`wire_dispatch.c`), not by any function in this file or in
  `panelbus_dispatch.c`.
- `ctouchpanel_cad_channels_enable`/`_disable` toggle CAD engine slots `0x20`-`0x25` - the
  same 6 slot numbers `cad.c`'s own `cad_calibration_select_slot` step-5 override picks
  from, showing CAD's own 38-channel analog engine is what actually captures the touch
  panel's 6 raw ADC channels, rather than the touch panel driving its own independent ADC
  hardware.

**Open items:** the physical scalar `ctouchpanel_watch_idle_scalar` watches has no reader
of its own "changed" flag found in this address range; the real second argument fed to
the shared smoother at `ctouchpanel_check_timeout`'s own call site needs a live
memory/register query to resolve; the unbound glue code (no Ghidra function boundary)
tying `FUN_c0014488` and a sibling lookup table (`FUN_c00145c4`) together; exactly how
CAD's captured slot-`0x20`-`0x25` samples land in `ctouchpanel_state`'s own `adc_ch`
array (same object vs. a copy - the sharing itself is established, the mechanism is not).

## `cad.c` - analog (knobs/sliders/pedals)

Anchor: `"../cad.cpp"` has 2 xrefs (`cad_channel_group`'s and `cad_trigger_calibration`'s
own assert calls), confirming the `0xc001335c-0xc0013f5c` range (23 functions) as this
subsystem's real compilation unit.

- **`cad_init`** - subsystem bring-up: hardware handle acquisition, two 38-entry
  calibration/filter state tables reset, 5 expansion-pedal presence probes, a hardware
  reset/config sequence, and registration of this subsystem's own opcode handlers (`0x78`,
  `0x79`) with the shared wire-protocol dispatcher.
- **`cad_channel_group`** - a 12-channel-to-4-group lookup table:
  `{0,2,3,6,9}->0`, `{1,4,5,7,10}->1`, `{8}->2`, `{0xb}->3`.
- **`cad_channel_eligible`** - a 12-channel enable-bitmask check gating calibration
  eligibility per channel.
- **`cad_trim_adjust`** - a host-facing entry point, called from `panelbus_dispatch.c`'s
  `panelbus_cmd_dispatch` for opcode `0x50`. Reuses register value `0x7a` with a
  different local meaning than `cpsoc.c`'s own `0x7a` register-bank split - confirming
  `0x7a` is a per-subsystem-local register number, not a single firmware-wide constant.
- **`cad_trigger_calibration`** - the anchor function: a host-facing calibration trigger
  with consistency-check asserts and a tick-counter-gated calibration sequence.
- **Calibration engine** (the remaining 18 functions): 38-slot parallel state (two
  smoothed-value caches, a "changed" bitmask, a "masked/excluded" bitmask, a
  threshold/cap pair); `cad_calibration_sweep` (a 7-step SPI sequence for eligible
  channels vs. a fixed 3-step sequence for excluded ones); `cad_calibration_smooth_sample`
  (a 2-tap debounce filter); `cad_calibration_pop_changed` (a rotating bitmask scanner,
  IRQ-guarded); `cad_init`'s 5 expansion-pedal presence probes seed the first 5 bytes of
  the exclude-mask, one byte per pedal jack; a separate small pedal-value-encoding trio
  (`cad_pedal_object_reset/_probe/_init`, `cad_pedal_send_release`,
  `cad_pedal_encode_step`) is tied to the 38-slot engine only via the shared presence
  probe - `cad_pedal_send_release` routes a pedal "off" event through `cpsoc.c`'s own
  `cpsoc_i2c_dispatch` primitive rather than cad's own SPI bus.
- `cad.c`'s own registered opcode handlers (`0x78`/`0x79`, set up by `cad_init`) are fed
  by `panelbus_dispatch.c`'s TX-ring-drain loop (`panelbus_tx_drain_channel`), which
  calls `cad_trigger_calibration` directly on each drained record - not via
  `wire_dispatch_command`, which only reaches `cad.c` through the separate
  `cad_trim_adjust` (opcode `0x50`) entry point. Two genuinely distinct paths into the
  same subsystem.
- **`cad_delay_ticks`** is not a busy-wait: it bounds the requested delay, arms a tick
  baseline, then loops servicing `cobjectmgr_tick` and a cad.c-internal
  calibration-progress pump until the elapsed time reaches the target.
- **`cad_calibration_progress_pump`** (`cad.c`'s own `FUN_c0005a1c`, reconstructed in
  `omap_l108.c` alongside the tick-timer primitives it is built from): reads/writes CAD's
  38-slot calibration state via `cad_calibration_pop_changed`/`cad_calibration_slot_is_raw`.
  Its only two real callers are `cad_delay_ticks` and `master_dispatch_tick`'s own
  bit-`0x0004` handler. Drains CAD's changed-slot queue, building a 4-byte wire-format
  event record per non-"raw" slot and, only when a pedal is physically present, drawing
  live calibration-progress text via `clcdc.c`'s own text primitive.

**Open items:** `cad_calibration_select_slot`/`cad_calibration_advance_group`'s
branch-by-branch real-world meaning; `cad_calibration_init_slot`'s own caller (not
traced); the real contents of `cad_calibration_sweep_table` and several sentinel
constants; whether the `0xe0` message-tag byte `cad_trigger_calibration` checks has any
relation to the AT88 relay's own `0xe0` opcode. *To validate: capture a real
`0xe0`-tagged message and compare against the AT88 relay opcode's own framing.*

## `mcasp.c` - audio serial port

The top-level `"../McAspHandler.cpp"` filename string has zero xrefs anywhere in the
image; the real functional driver lives in a different, lower-level compilation unit,
`"../MCU/Component/OmapL137Mcasp.cpp"` (11 xrefs, all from `mcasp_init`).

- The `mcasp_state` struct is addressed via plain byte-offset pointer casts throughout
  (its field offsets do not line up with contiguous struct-field doc comments, so offsets
  are used directly rather than named fields).
- `mcasp_reset_stage` reads its shared attempt counter's address from a real global
  pointer indirection at every stage.
- `mcasp_clock_lock`/`mcasp_clock_unlock` (renamed `mcasp_clock_param_select_a`/`_b`) do
  no locking or memory access - they are pure constant selectors, confirmed by a second,
  independent call site that captures their return values into globals.
- The three "step" functions each take one argument passed via ARM's r0-register reuse
  (the preceding select-function's return value, still live in r0, is the real argument),
  not called with no arguments as an early decompile suggested.
- The TX/RX serializer block is not fully symmetric: 2 of 7 paired fields differ
  (`+0x70`/`+0xb0`: `0x80` vs `0xc0`; `+0x68`/`+0xa8`: two different `DAT_` constants).
- `mcasp_clock_step_c`'s address-generation shape (a shared global base indexed by a small
  integer over a fixed-size record array, matching `mcasp_init`'s own caller's identical
  pattern) reads more like EDMA3 PaRAM descriptor setup for the McASP TX/RX FIFOs than a
  PLL divider update, though this is not confirmed field-by-field against TI's own PaRAM
  layout.

**Open items:** a second, address-adjacent (`0xc0002c60`) but Ghidra-unbounded code
region, confirmed via raw disassembly to be a differently-configured second McASP
instance's init sequence (or a reconfigure/deinit path), is not reconstructable as a
named function since Ghidra assigned it no boundary; a smaller "reduced reinit" sibling
(`FUN_c0003228`) has no filename-string ownership evidence; the real 9-vs-10 serializer
pin-function meaning is unresolved.

## `cdix4192.c` - digital audio interface

Anchor: `"../CDix4192.cpp"` has exactly one xref, the clean single-assert pattern;
independently confirmed by `DAT_c000fc44` (the config-verify fault call's `file`
argument) resolving to `0xc0023180`, the exact address of this file's own string.

- **`cdix_reg_write`/`cdix_reg_read`** sit on the same shared I2C bit-bang primitives
  reconstructed in `i2c_by_gpio.c`. Fixed I2C address `0x70` for every access. The `chip`
  parameter these two wrappers appear to take is not actually forwarded to the I2C
  primitive underneath - the real disassembly passes an uninitialized local stack buffer
  instead. Since `i2c_by_gpio.c` independently confirms the whole bus is a hardware
  singleton, this is a harmless firmware quirk rather than an unresolved mystery.
- **`cdix_configure_and_verify`** - the anchor function: walks a `{register, value}`
  table twice, write then read-back-and-verify. It has zero static callers found anywhere
  in the image; the real invocation mechanism is genuinely unresolved (not explained by
  `eva_board_main.c`'s own init table, which has exactly one, unrelated entry). *To
  validate: search for an indirect/table-based call site a static xref sweep cannot see,
  or trigger a config-verify failure on real hardware and trace back to a caller.*
- The config table's address is `0xc001fb6c`; its 3 real entries are `{0x7f,0x00}`,
  `{0x03,0x29}`, `{0x04,0x03}`. The 2 padding bytes per table entry remain unread - no
  data-segment symbol resolves them in a static image dump. *To validate: a live memory
  read of the table at runtime.*

## `eva_board_main.c` - main entry / board bring-up

Anchor: `"../EvaBoardMain.cpp"` has exactly one xref, inside the fault-wrapper function
below - but unlike every other subsystem in this project, Ghidra's auto-analysis never
assigned a containing function boundary to this code; everything here was read directly
from raw disassembly.

- The full reset-vector/crt0 chain, from power-on to `eva_board_main`, is described under
  Architecture above.
- The `FUN_c0009534`/`FUN_c0009540` code that once looked like an ARM/Thumb interworking
  boundary is a mundane artifact: a plain literal pool sitting inline in the code stream
  right after `eva_board_reset_handler`'s own noreturn tail-call, misread as instructions
  by linear disassembly because nothing marks the fall-through region as data. No Thumb
  code or interworking boundary is actually involved.
- **`eva_board_init_table`** - a generic function-pointer-table walker. The table has
  exactly one entry, pointing to a lazy-init singleton wrapper - not a multi-function
  driver-probe table. `crypto_at88_self_test`, `cdix_configure_and_verify`, and
  `cobjectmgr_object_destroy` are not called through this table; their real invocation
  mechanism remains genuinely unresolved.
- **`eva_board_main`** - the boot entry point: calls `eva_board_init_table`, then
  `eva_board_final_setup`, then `eva_board_start_task(1, 4)`, then an unconditional
  `for(;;)` loop calling `master_dispatch_tick` forever.
- **`eva_board_watchdog_fault_wrapper`** (the anchor) takes no effective parameter (the
  same "phantom forwarded parameter" pattern found in `cdix4192.c`). It calls
  `cobjectmgr.c`'s own `cobjectmgr_hardware_fault_watchdog`, and if that call ever
  actually returns (which, per that function's own confirmed structure, should never
  happen), raises a separate hard fault right here.
- **`eva_board_final_setup`** - a per-subsystem hardware bring-up sequencer: sets three
  fixed handle fields, runs a hardware compatibility/self-test gate
  (`eva_board_compat_check`, the same "draw an error, hang forever" fail-fast idiom
  confirmed for `crypto_at88.c`'s own assert handler), then twelve further per-subsystem
  bring-up calls. Three are confirmed by address: `cad_init`/`cad_pedal_object_init`
  (`cad.c`) and `omap_l137_usbdc.c`'s own USB object bring-up
  (`omap_usbdc_object_init`). Three more have suggestive-but-not-confirmed evidence
  (plausibly `clcdc.cpp`'s own top-level constructor, `cpsoc.cpp`'s own state-table
  clear, and a second USB register-block setup). The final calls register four cpsoc
  register banks (`0x78`/`0x79`/`0x7b`/`0x7a` - the same four banks
  `eva_board_compat_check`'s own probe loop cycles through) via `cpsoc_i2c_dispatch`.
- **`eva_board_start_task`** is a real primitive of this firmware's own small
  priority-based task scheduler, not a generic "spawn a task with a function pointer"
  call - it operates on a pre-existing task-control-block table (populated at boot by
  `eva_board_crt0`'s own init sequence), looking a task up by index and either marking it
  ready or updating its priority. `eva_board_main`'s own call, `(1, 4)`, means "task
  index 1, priority level 3" (a 1-based-to-0-based conversion). Its own three scheduler
  callees (ready/requeue/dispatch) are structurally confirmed but not transcribed as C -
  dense pointer/bitmap arithmetic with no independent verification path.

**Open items:** whether `eva_board_main` is itself one of the tasks made ready during
`eva_board_crt0`'s own init sequence is the strongest remaining lead tying the
reset-vector section to the rest of this file, but is not confirmed - the task table's
actual contents have not been read; seven of `eva_board_final_setup`'s twelve callees
have no confirmed cross-file attribution; `eva_board_compat_check`'s own `0x17`/`0x27`
board-id constants' physical meaning is unresolved; the real invocation mechanism for
`crypto_at88_self_test`/`cdix_configure_and_verify`/`cobjectmgr_object_destroy` (see
those files) remains open.

## `cobjectmgr.c` - object manager

Nine functions carry their own local copy of the `"../cobjectmgr.cpp"` string pointer.

- **`cobjectmgr_tick`** - the core: called unconditionally every master-dispatcher tick.
  Polls a single "current object" slot; if occupied, dispatches on a one-byte type tag to
  one of exactly two handlers (any other tag is a hard fault). The "current object" slot
  is populated by `wire_dispatch_command`'s own opcode switch, which has a case for bytes
  `0xc4`/`0xc6` (the exact two tag values this function dispatches on) that does nothing
  but hand the queued command's payload pointer/length straight into this struct's own
  fields.
- **`cobjectmgr_handle_type_a`** - tag `0xc4`, an 11-byte-record solid-colour pixel-run
  drawer, drawing directly into `clcdc.c`'s own framebuffer/palette globals -
  `cobjectmgr` sits on top of `clcdc` as a renderer for wire-triggered object types, not
  an independent drawing surface.
- **`cobjectmgr_handle_type_b`** - tag `0xc6`, structurally documented rather than
  transcribed (too dense to transcribe with confidence): a four-way wraparound pointer
  walk packing 4 sub-fields per source dword into a separate circular output buffer, also
  drawing through `clcdc.c`'s own palette global.
- **`cobjectmgr_release_object`**, despite its name, is not object-specific and does not
  release anything - it takes no real parameter and unconditionally returns a fixed
  32-bit constant, with 8 call sites across at least 5 unrelated functions elsewhere in
  the firmware. The real "release" in `cobjectmgr_tick` is the inline `NULL` assignment
  already there.
- **`cobjectmgr_object_cleanup`** - a thin, one-line wrapper into an unrelated,
  not-cobjectmgr-owned hardware-descriptor-table writer, always called with the same
  hardcoded indices.
- **`cobjectmgr_notify_host`** - one of a small family of host-notify event senders
  (siblings not individually reconstructed).
- **`cobjectmgr_hardware_fault_watchdog`** blocks indefinitely on a real event-group wait
  primitive for a hardware-fault status bit, and only escalates to the firmware's
  hard-halt assert once that event actually fires - the bridge from a real hardware fault
  interrupt/event to the software fault path, not vestigial code.
- **`cobjectmgr_free_list_recursive`** - a small self-recursive singly-linked-list
  walk-and-free helper (physically located in the address range once misattributed to
  `clcdc.cpp` - see `clcdc.c`'s boundary note above).
- **`cobjectmgr_object_destroy`** - a real C++ virtual destructor (the anchor for
  `heap_alloc.c`). Its full struct layout is transcribed as real C, using
  `heap_alloc.c`'s `heap_free` as an opaque, already-characterized dependency. It has
  zero static callers, consistent with being reached only through vtable/virtual
  dispatch, and has an early-return guard for a fixed self-reference sentinel.

**Open items:** `cobjectmgr_handle_type_a`'s own trailing 3 bytes (a "width" field) are
never accounted for in its own stream-position bookkeeping - unclear whether this is a
deliberate fixed trailing field or an unused-but-present register value; what produces
tag-`0xc6` objects and what consumes `cobjectmgr_handle_type_b`'s own output ring buffer
is unresolved. *To validate: trace producers/consumers of the type-b output ring buffer
via its address.*

## `heap_alloc.c` - shared heap allocator

The firmware's shared, general-purpose heap allocator: a segregated free-list allocator
(small exact-fit bins up to ~504 bytes, indexed logarithmic "tree" bins beyond that),
boundary-tag free chunks with immediate coalescing, a page-granular trim-back-to-OS path,
and an sbrk-style break-pointer primitive underneath it all.

- Not anchored by a `__FILE__` string (a full image string search found none for this
  address range) - attributed on code-shape evidence alone.
- The code shape (chunk header = size|flags word immediately preceding the user pointer,
  free chunks' fd/bk overlaid into small-bin array storage, a treebin-index computation
  ladder duplicated at three call sites, a dedicated "designated victim" slot, a
  top/"wilderness" chunk grown via sbrk) closely matches the well-known Doug Lea malloc
  (dlmalloc) family - noted as a reading aid, not license to substitute reference source;
  every claim here is grounded in what Ghidra shows for this binary.
- **`heap_sbrk`/`heap_trim`** are fully transcribed - simple, unambiguous control flow,
  no bin-array walking.
- **`heap_malloc`/`heap_free`** (the allocator core) are documented structurally rather
  than transcribed line-for-line, per this project's practice for code this dense with no
  way to verify against real hardware.
- **`heap_lock`/`heap_unlock`** have genuinely empty bodies (no observable side effect in
  this decompile) - bracketed around every allocator entry point regardless, most
  plausibly a critical-section pair fully collapsed away by the compiler for a build with
  no real contention.
- This allocator is shared firmware-wide, not cobjectmgr-specific: `heap_malloc`'s own
  callers span at least 5 unrelated functions well outside any other subsystem in this
  project.
- The `handle` parameter every function in this file takes is dead all the way through
  `heap_sbrk`/`heap_trim`/`heap_lock`/`heap_unlock` too - the same "phantom forwarded
  parameter" pattern found elsewhere in this project, at scale.

**Open items:** `heap_state`'s exact struct layout beyond the confirmed
binmap/designated-victim/per-bin-sentinel fields; the fixed constants controlling
wilderness growth; the three separate high-water-mark stat globals' individual purposes.

## `omap_l108.c` - SoC tick timer

Anchor: `"../MCU/OmapL108.cpp"` has exactly one xref, inside `omap_tick_init`'s own
init-guard assert.

- **`omap_tick_init`/`omap_tick_read_raw`/`omap_tick_elapsed_scaled`** - a small, generic
  free-running-tick-counter API.
- `omap_tick_scale` (`FUN_c001e3f8`) is a second, unrelated caller of the same function
  `clcdc_progress_bar` uses (divisor `0x96`/150 here vs. `clcdc_progress_bar`'s own 100) -
  a generic, firmware-wide fixed-point scaler, not clcdc-specific.
- `cad_delay_ticks` (`cad.c`'s own name) is documented here since it is built almost
  entirely out of this file's tick-timer primitives - see `cad.c` above.
- `cad_calibration_progress_pump` (`cad.c`'s own `FUN_c0005a1c`) is reconstructed here
  rather than in `cad.c`, since its only two real callers (`cad_delay_ticks`, here, and
  `master_dispatch_tick`'s own bit-`0x0004` handler in `wire_dispatch.c`) both live near
  this file's own code - see `cad.c` above for its behavior.

**Open items:** the real identity of the fixed constants `DAT_` references stand in for
(no data-segment symbols resolved); whether the raw tick counter is a hardware register
or a software counter incremented by an ISR.

## `omap_l108_spi.c` - SPI peripheral (shared cad/cpsoc bus)

Anchor: `"../MCU/Component/OmapL108Spi.cpp"` has 2 xrefs, both hard-fault call sites
inside the one function this file amounts to.

- **`omap_spi_write`** - a single SPI transmit primitive: two busy-wait phases against
  the same status register testing two different bits, each independently retry-bounded,
  followed by a read-modify-write of the low 16 bits of a combined control/data register.
- This exact primitive has 8 real call sites across `cad.c` (3x `cad_calibration_sweep`,
  `cad_trigger_calibration`, `cad_init`) and `cpsoc.c` (`cpsoc_analog_poll_channel`, 2x
  `cpsoc_analog_poll_task`) - the analog chip, the PSoC scan chip, and cpsoc.c's own
  separate analog-polling chain all share one physical SPI bus and one low-level
  transmit primitive.
- The retry-delay argument resolves to an address well outside this image's own
  code/data range, plausibly a real MMIO peripheral/timer register elsewhere in the
  SoC's address space.

**Open items:** the real bit-level meaning of the two status bits is usage-inferred, not
datasheet-confirmed; `omap_spi_retry_delay`'s own internals are unresolved; whether a
corresponding SPI read function exists elsewhere in the image is unknown.

## `omap_l137_usbdc.c` - USB device controller

Anchor: `"../MCU/Component/OmapL137Usbdc.cpp"` has 3 xrefs - two inside
`omap_usbdc_init_ep0`, one inside `omap_usbdc_poll_transfer`.

- `omap_usbdc_init_ep0`'s final field-setup block writes every field on `dev` (param_1),
  not on `regs` (param_2).
- Retry bounds (`0xf423f`/999999, shared by both functions in this file), byte offsets, a
  shared status/flags byte used by both functions here, and both hard-fault line
  numbers/file pointers are all resolved.
- **`omap_usbdc_object_init`** (`FUN_c0009574`, the higher-level USB object bring-up
  function) is fully reconstructed. Its sole caller is `FUN_c00074bc`, which
  `eva_board_main.c` names `eva_board_final_setup` - `eva_board_main.c`'s post-init-table
  setup call is the USB device controller bring-up.
- `FUN_c000acc8` is more than a no-op-adding wrapper: it silently forwards a real `len`
  argument that Ghidra's own per-function signature analysis failed to recognize as a
  parameter. Its two real call sites: `FUN_c000acec` (the shared USB-submit primitive,
  see `crypto_at88.c`) calls it as a pure readiness gate (`len=0`); `master_dispatch_tick`
  calls it once per tick with a real, computed length - the call that actually
  arms/advances the transfer-completion state machine.
- **`omap_usbdc_poll_transfer`** is the bottom of a call chain traced across this entire
  project: a 3-state (idle -> in-flight -> complete) transfer-completion state machine
  gated on a hardcoded 8001-byte size threshold.

**Open items:** a second, apparently independent set of 5 register-block pointer
derivations in `omap_usbdc_object_init` vs. the 3 inside `omap_usbdc_init_ep0` itself -
overlap not traced; two raw hardware-register constants and two global-to-global copies
in `omap_usbdc_init_ep0` have known values but undecoded functional meaning;
`dev+0x24`/`dev+0x28`'s real per-tick chunking meaning is unresolved; whether the
8001-byte threshold is a real hardware DMA/FIFO limit or a firmware policy choice is
unknown.

## `omap_l137_usbdc_ext.c` - USB ISR/poll handler, endpoint-event dispatch, low-level layer

Not part of `omap_l137_usbdc.c`'s own confirmed anchor range, but the same
hardware/software layer by shared field offsets (`dev+0x401`, `dev+0x40e`) with that
file's own `omap_usbdc_init_ep0`/`omap_usbdc_poll_transfer`.

- **`usbdc_core_isr`** (`FUN_c0003e24`, 1812 bytes) - the master USB0 core interrupt/poll
  handler. Decodes a combined interrupt-status word, handles USB bus reset (full endpoint
  1-3 re-init + CPPI descriptor table setup), the EP0 control-transfer state machine, and
  per-endpoint TX/RX-ready events, most of which fall through to
  `usbdc_endpoint_event_dispatch`. Its sole caller is a small wrapper at `0xc0009bd8` that
  looks more like a poll/dispatch wrapper than a raw IRQ vector; that wrapper's own caller
  has no static xref (plausibly reached through an indirect function-pointer table).
- **`usbdc_endpoint_event_dispatch`** (`FUN_c000aae0`) - switches on event code; case 5 is
  `usbdc_ep_recv_bulk`, the real `wire_dispatch_command` call site.
- **`usbdc_ep_recv_bulk`** (`FUN_c000a918`) - reads the pending bulk-OUT byte count,
  clamps to 512 (USB full-speed bulk max-packet-size), drains the FIFO, and calls
  `wire_dispatch_command(handle, buffer, len)` directly - no queue, no deferred
  processing. Fires once per "endpoint 5 RX ready" USB core interrupt.
- A second `wire_dispatch_command` call site exists inside `usbdc_core_isr` itself, on the
  EP0 SETUP-pending path (distinct from `usbdc_ep_recv_bulk`'s bulk-OUT path).
- `usbdc_ep0_notify_tx_complete` (`FUN_c00048f8`) selects endpoint 3 (not 0) and tests
  TXCSR with a CSR-test offset that does not include a stray `+4` some earlier
  transcriptions carried; `usbdc_endpoint_event_dispatch` case `0xb` calls
  `usbdc_ep0_notify_tx_complete` and `usbdc_ep0_notify_rx_complete` in the correct (not
  swapped) branch/argument order.
- `usbdc_default_dev_handle` and `usbdc_bulk_dev_handle` are the literal same global
  (`0xc01cce50`); `usbdc_wire_handle`, `usbdc_setup_dispatch_handle`, and the
  endpoint-notify handle used throughout case `0xb` are all the literal same global too
  (`0xc01cac00`) - this entire USB-receive cluster, both `wire_dispatch_command` call
  sites, and the endpoint-notify plumbing all ultimately operate on two shared global
  objects, not several independently-named lookalikes.

**Open items:** `FUN_c0009bfc`'s own caller has no static xref (plausibly an indirect
function-pointer table - *to validate: search the image for a function-pointer table
containing this address*); individual TXMAXP/RXMAXP/CSR register field meanings in the
bus-reset branch beyond confirmed TXCSR/RXCSR bit positions are undecoded; the
CPPI-style DMA descriptor table's exact register-level semantics have no TRM
cross-reference done.

## `panelbus_dispatch.c` - second (hardware) I2C bus / internal command channel

Reconstructs `FUN_c0007220` (`panelbus_cmd_dispatch`), a large secondary protocol
dispatcher. Not a debug console: its real caller chain is `panelbus_cmd_dispatch` <-
`panelbus_rx_dispatch_loop` <- `panelbus_poll_channels` <- `master_dispatch_tick`
(status bit `0x1000`) - a completely different call path from `wire_dispatch_command`'s
own USB command entry point.

- `panelbus_i2c_read_bytes`'s register offsets (`+0x08` status busy/receive-ready bits,
  `+0x14` count, `+0x18` RX data, `+0x1c` slave address, `+0x24` mode) and
  `panelbus_i2c_base`'s two selectable base addresses (`0x01e28000`, `0x01c22000`) are an
  exact match for the TI OMAP-L138/AM1808 on-chip I2C1 and I2C0 peripheral base addresses
  and ICSTR/ICMDR bit positions - a byte-framed command interpreter fed by real MMIO
  hardware, polled once per firmware tick, with no ASCII parsing, no line editing, and no
  print/echo path anywhere in the chain.
- This is a separate, real hardware I2C bus from `i2c_by_gpio.c`'s own bit-banged
  `I2cByGpio.cpp` cluster (GPIO toggling, no MMIO peripheral registers, shared by
  `CryptoAt88.cpp`/`CDix4192.cpp`) - this file's functions sit outside that address range
  and talk to the genuine on-chip I2C0/I2C1 hardware block instead.
- **`panelbus_i2c_base`** selects I2C1 or I2C0 by a selector that is dead in practice -
  every call site inside this file's own family passes selector=0, i.e. this whole
  dispatcher only ever talks to I2C0.
- **`panelbus_i2c_read_bytes`** - blocking I2C read primitive, fully transcribed.
- **`panelbus_opcode_known`**/**`panelbus_rx_dispatch_loop`** - the RX-side opcode
  whitelist and drain loop; this loop is the one static caller of
  `panelbus_cmd_dispatch`.
- **`panelbus_tx_queue_pop`**/**`panelbus_tx_send_retry`**/**`panelbus_tx_drain_channel`**
  - the TX-side per-channel ring-buffer drain, feeding real submitted records through the
  shared 4-retry-then-hard-fault pattern.
- **`panelbus_poll_channels`** - the per-tick entry point called from
  `master_dispatch_tick`. Polls ports `0x78`/`0x7a`/`0x7b` for inbound frames and drains
  the TX ring on ports `0x78`/`0x79`/`0x7a` - a real asymmetry (port `0x7b` has no TX
  drain, `0x79` has no RX poll).
- **`panelbus_cmd_dispatch`** - the full opcode table (`0x30`-`0x32`/`0x40`-`0x4f`
  submit-record family, `0x50` -> `cad_trim_adjust`, `0x60`-`0x6f` -> ctouchpanel-shaped
  percent scaling, `0x70`/`0x90` -> flag posts into a shared RTOS event-flag group,
  `0x80`-`0x87` -> a device-ID negotiation handshake with a hard-fault-with-used-return-
  value pattern unlike every other call to the same fault handler elsewhere in this
  project).
- `FUN_c0014488` is not called from this file. Its real 2 callers are both inside
  `wire_dispatch_command` (`wire_dispatch.c`), plus one call inside `ctouchpanel.c`'s own
  address range.
- `panelbus_tx_drain_channel` is the actual feeder for `cad.c`'s own registered opcode
  handlers (`0x78`/`0x79`) via `cad_trigger_calibration` - not called from
  `wire_dispatch_command` directly.
- `cpsoc.c`'s own I2C0 submit primitive (`FUN_c00032f8`) is this exact same I2C0
  hardware, not a separate SPI device - see the `cpsoc.c` section above for the
  address/xref evidence (shared literal handle-selector function
  `FUN_c0001a00`/`panelbus_i2c_base`, the same `0x01c22000` I2C0 base constant, and
  `FUN_c00033f0` being the literal same function as this file's own
  `panelbus_i2c_read_bytes`).

**Open items:** the TX ring's own push/producer side has not been found as its own
function; `FUN_c0012de0` (`panelbus_submit_record`'s own target primitive) is
unresolved; several cross-file signature mismatches for shared symbols are flagged
rather than silently reconciled (see Known limitations below).

## `wire_dispatch.c` - master wire-protocol dispatch loops

Reconstructs the panel firmware's two central dispatch loops (see Architecture above for
their roles): `wire_dispatch_command` (`FUN_c0007d1c`, 1904 bytes) and
`master_dispatch_tick` (`FUN_c0008b64`, 1240 bytes). Neither has its own `__FILE__`
anchor; both are identified by size, role, and cross-file address confirmation.

- `wire_dispatch_command` is reached via the USB receive path - a large ISR/poll state
  machine (`usbdc_core_isr`) plus a smaller opcode-switch shim
  (`usbdc_endpoint_event_dispatch`/`usbdc_ep_recv_bulk`), both living in
  `omap_l137_usbdc_ext.c` - not called directly from `eva_board_main`'s main loop with USB
  data. `master_dispatch_tick` is the other, unconditional-tick caller context; the two
  are siblings, not caller/callee of each other.
- The full opcode table is transcribed, cross-checked against
  `kronosology/docs/modules/KRONOS_V06R06.VSB.md`'s own independently-derived table, and
  confirms 3 cross-file handle-sharing facts by direct address comparison: the cpsoc
  register-bank context, the cad pedal context, and the cad calibration context this
  dispatcher reads are each the exact same global `eva_board_final_setup` itself seeds at
  boot.
- The dense `0xe0` (AT88 relay write) payload-reassembly loop is preserved literally
  rather than simplified, since `crypto_at88.c`'s own byte-order handling nearby is known
  to be error-prone - deliberately not "cleaned up" past what the raw decompile shows.
- `master_dispatch_tick`'s very last call each tick drives `omap_l137_usbdc.c`'s own
  transfer-completion state machine directly (via that file's own thin wrapper), with a
  length computed from the same handle fields the `0x20`-bit and `0x8000`-bit status
  handlers touch - this dispatcher does not just trigger other subsystems' USB sends
  indirectly, it directly pumps a large transfer through in ~2000-byte chunks, one attempt
  per tick, until the USB state machine reports completion.
- `FUN_c0003e24`/`FUN_c000a918`/`FUN_c000aae0` (`wire_dispatch_command`'s two real
  USB-receive-path callers) are reconstructed in `omap_l137_usbdc_ext.c`, not this file -
  see that file's section above for the full detail (`usbdc_core_isr`,
  `usbdc_endpoint_event_dispatch`, `usbdc_ep_recv_bulk`).

**Open items:** the `0xe0` payload-reassembly loop's exact byte-order past its fixed
header is deliberately not transcribed further (see above); most of the generic
`eva_wire_`/`eva_tick_` externs (bare `FUN_`/`DAT_` addresses) have no cross-file
attribution; `master_dispatch_tick`'s own ~80-line USB-adjacent state-machine cluster is
plausibly `omap_l137_usbdc.c`'s own endpoint configure/halt bookkeeping, but out of this
file's own scope to resolve.

## Building and testing

The reconstructed C compiles cleanly under `arm-none-eabi-gcc -fsyntax-only` (a
syntax-correctness check only, not proof of behavioral equivalence with the binary) - see
this project's own `Makefile` header comment for build details.

## Known limitations

Beyond the per-subsystem open items listed above, a few findings cut across files and are
deliberately left as open, unreconciled inconsistencies rather than silently resolved one
way or the other - concurrent reconstruction passes over different files do not always
agree, and the file contents are the ground truth over any single pass's guess:

- [x] ~~**Signature mismatch**~~ - RESOLVED 2026-07-20. Had it backwards:
  `ctouchpanel.c`'s current `int ctouchpanel_watch_idle_scalar(struct
  ctouchpanel_state *tp, int16_t new_value)` IS ground truth, confirmed via
  direct Ghidra decompile of both `FUN_c00140d4` itself (`undefined4
  FUN_c00140d4(int param_1, short param_2)`) and its real caller
  `FUN_c0007220` (`uVar2 = FUN_c00140d4(iVar6); return uVar2 & 0xff;`).
  `panelbus_dispatch.c`'s own extern was the stale one - predating
  `ctouchpanel.c`'s own 2026-07-18 correction pass, still using the old name
  (`ctouchpanel_finalize_release`) and a wrong `void` return. Fixed to match
  (kept as `void *` rather than pulling in `ctouchpanel.c`'s file-local
  struct, per this file's own established cross-file-decoupling convention).
- [x] ~~**Return-value disagreement**~~ - CONFIRMED CORRECT AS DOCUMENTED,
  not actually a bug. `clcdc_draw_text` (`FUN_c0015650`) genuinely has no
  meaningful return (Ghidra's own decompile ends in a bare `return;`, no
  value) - but its real caller `FUN_c0007220`'s own decompile, independently
  re-checked 2026-07-20, DOES show `uVar2 = FUN_c0015650(0x14,0x50,uVar3,0);
  return uVar2;` at that one specific opcode-`0x80`-`0x87` call site: real
  ARM/compiler-generated code that happens to read r0 immediately after this
  particular call and propagate it, a genuine hardware/compiler quirk, not a
  reconstruction inconsistency. `panelbus_dispatch.c`'s dual naming
  (`clcdc_draw_text` for the ordinary void call sites, `clcdc_draw_text_ret`
  for this one) was already the right way to transcribe it faithfully; no
  change needed.
- [x] ~~**Argument-count disagreement**~~ - CONFIRMED REAL, same category as
  the `clcdc_draw_text` finding above, not a reconstruction error. The
  shared hard-halt handler (`crypto_at88_fault`/`FUN_c000919c`) genuinely
  takes 3 parameters in its own body (confirmed via direct decompile:
  `void FUN_c000919c(undefined4 param_1, undefined4 param_2, undefined4
  param_3)`, ends in an infinite loop, never returns) - but Ghidra's own
  independent per-call-site analysis of `FUN_c0007220` shows a real 4-arg
  call (`FUN_c000919c(0, DAT_c00073e0, DAT_c00073e4, param_2)`) at one site.
  This is real ARM ABI behavior (some call sites set up an extra
  argument register the callee simply never reads), not something to force
  into one artificial canonical signature. Spot-checked rather than
  exhaustively re-verified all 27 call sites - the underlying mechanism is
  now understood and demonstrated, which is the actionable finding.
- [x] ~~**Unresolved invocation mechanisms**~~ - STRENGTHENED, still
  genuinely unresolved but now conclusively so. `crypto_at88_self_test`,
  `cdix_configure_and_verify`, and `cobjectmgr_object_destroy` were
  re-checked 2026-07-20 with `mcp__ghidra__get_xrefs_to` (Ghidra's own
  analysis engine, independent of the project's text-based 691-function
  sweep - would catch anything Ghidra itself recognizes as a reference,
  including vtable/jump-table entries) AND an exhaustive raw 4-byte
  little-endian address search across the ENTIRE firmware image
  (`mcp__ghidra__search_bytes` for each function's own address bytes) - both
  came back with **zero hits** for all three functions. This is stronger
  evidence than "no caller found by a static sweep": these addresses do not
  appear ANYWHERE in the 917KB image, as code or data, direct or indirect.
  Genuinely, provably dead code within this image - or reached by a
  mechanism entirely external to it (JTAG/debug probe, a separate
  configuration blob not part of this firmware image). *To validate further
  if ever needed:* trigger on real hardware and trace the call back, since
  static analysis of this image alone cannot go further.
- [x] ~~**Duplicate transcriptions**~~ - CONFIRMED already correctly
  handled, no action needed. Re-checked 2026-07-20:
  `panelbus_rx_dispatch_loop` (`panelbus_dispatch.c`) and
  `cpsoc_poll_reg_reads` (`cpsoc.c`) are already thoroughly cross-referenced
  in both directions with detailed header comments explaining the shared
  address and why they're kept as two independently-named views rather than
  merged - exactly matching this project's own stated convention. Not a
  gap; the TODO item itself over-stated this as open.

## Related documentation

- `kronosology/docs/modules/KRONOS_V06R06.VSB.md` - the container format, load-address
  derivation, and independently-derived opcode table this reconstruction is built from
  and cross-checked against.
- `kronosology/docs/workflow/ghidra_setup.md` - general Ghidra session conventions used
  for this project.
- `kronosology/reconstructed/OmapNKS4Module/` - the host-side kernel driver, the other
  end of the USB wire protocol this firmware implements.
- `kronosology/reconstructed/K2_V01R10/` - the Kronos 2 panel firmware port that uses
  this directory as its K1 reference baseline.
