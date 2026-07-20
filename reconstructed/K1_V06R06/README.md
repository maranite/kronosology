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

## 2026-07-19 — full completion: all 691 functions reconstructed

Following the systematic pass below and many further waves of 1-2-agent (capped by
explicit user instruction, with a hard no-subagent-spawning rule in every dispatch after
one agent was found to have violated an earlier cap) targeted reconstruction, **every one
of the 691 real functions in the image now has a genuine, compilable C definition
somewhere in this directory** (53 files total). This was confirmed directly — not just via
the project's own coverage-tracking script (`coverage_gap2.py`, which itself needed two
real bug fixes during this push: it originally pointed at this directory's pre-rename path
`NKS4PanelFirmware/`, and its citation regex only matched one of the two comment styles
this project's files actually use) — but by manually grepping every remaining flagged
address against the real file contents one at a time before accepting the final count.

The very last gap closed was `FUN_c001eef8` (`softfloat_d2iz` in
`libgcc_softfloat_dcmp.c`), a libgcc `__aeabi_d2iz`-shaped double-to-int32 saturating
truncation helper — added directly (not via a dispatched agent) after a live disassembly
check confirmed it was real code sharing its sole caller with three already-reconstructed
sibling comparison functions in the same file. A transcription bug (the negative-sign
initializer had been mis-derived as `0xffffffff` instead of the correct `0x80000000`) was
caught and fixed before finalizing.

**Coverage is complete; behavioral fidelity is not universally proven.** Full function
coverage means every address Ghidra recognizes as a function boundary has real, honestly
reconstructed C — not that every line is proven bit-identical to the binary's real
runtime behavior. Dozens of individual files still carry their own honest "STILL OPEN"
notes (unresolved `DAT_` constant identities, a handful of functions where the decompiler
itself couldn't distinguish two different-sized bodies, register-level semantics inferred
rather than datasheet-confirmed, etc.) — these are deliberately left as documented
uncertainty rather than smoothed over, per this project's standing discipline. See each
file's own header for its specific caveats.

This directory is also the K1 reference baseline for the sibling
[`K2_V01R10/`](../K2_V01R10/) project (Kronos 2's own panel firmware,
`KRONOS2S_V01R10.VSB`) — see that directory's own README for its port/validation status.

## 2026-07-19 — SPI/I2C0 mis-attribution resolved

Closed the one open item the completion pass above didn't touch: whether `cpsoc.c`'s
own "third device" command-relay primitives (`FUN_c00032f8`/`FUN_c00033f0`, documented
there as `cpsoc_spi_submit_write`/`cpsoc_spi_submit_read`) are genuinely SPI, or - as
`panelbus_dispatch.c`'s own hardware section suspected but didn't chase down - the same
real on-chip I2C0 controller that file's own `panelbus_i2c_read_bytes` uses. This
mattered concretely for a future virtual-board emulator: one shared I2C0 bus controller
to model, or two independent peripherals.

**Finding: I2C0, definitively — not SPI.** Rebuilt the minimal single-`PT_LOAD` ELF32
ARM wrapper around `Decomp/subsystem/KRONOS_V06R06.VSB` (raw payload at file offset
`0x100`, per this doc's own "Ghidra setup" section) and loaded it fresh into an idle
Ghidra MCP session (no concurrent agents this pass, so live per-call queries were used
directly rather than a pre-fetched static dump). Evidence, strongest first:

1. `FUN_c00033f0` — cpsoc.c's own "SPI read" primitive — is not just structurally
   similar to `panelbus_dispatch.c`'s `panelbus_i2c_read_bytes`. `get_xrefs_to` on
   `0xc00033f0` returns exactly 3 static callers firmware-wide: `FUN_c00073fc`,
   `FUN_c0010b58` (`panelbus_rx_dispatch_loop`, `panelbus_dispatch.c`), and `FUN_c0010f60`
   (`cpsoc_read_event_pair`, `cpsoc.c`). **It is the literal same function**, independently
   transcribed under two names by two different reconstruction passes that didn't cross-
   check addresses against each other.
2. Both sides fetch their register-block handle via `FUN_c0001a00(_, 0)`. Fresh decompile
   confirms `FUN_c0001a00` IS `panelbus_dispatch.c`'s own `panelbus_i2c_base`
   (byte-identical body: `return param_2 ? DAT_c0001a14 : DAT_c0001a18;`). A direct
   `read_memory` on that function's own literal pool this pass reads
   `DAT_c0001a14 = 0x01e28000` (I2C1) and `DAT_c0001a18 = 0x01c22000` (I2C0) — the real
   TI OMAP-L138/AM1808 I2C1/I2C0 peripheral base addresses per the public TRM, exactly as
   `panelbus_dispatch.c` already documented. Every real call site on both the cpsoc.c and
   panelbus_dispatch.c sides passes selector `0` — I2C0.
3. Contrast case, confirming the method actually discriminates rather than rubber-
   stamping everything as I2C0: `cpsoc.c`'s OTHER handle-selector, `cpsoc_get_handle`
   (`FUN_c0001a1c`, immediately adjacent in the image), has its own, DIFFERENT literal
   pool — read this pass at `0xc0001a30`/`0xc0001a34` — holding `0x01f0e000`/`0x01c41000`,
   neither of which matches any OMAP-L138 I2C/TRM peripheral base. That handle feeds
   `omap_spi_write` (`omap_l108_spi.c`, register shape `+0x3c`/`+0x40`, structurally
   distinct from the `+0x08`/`+0x14`/`+0x1c`/`+0x20`/`+0x24` I2C0 shape) for the same
   device's real ADC channel reads — genuinely SPI, unaffected by this finding.

**A second duplicate surfaced chasing this**, flagged but not merged: `0xc0010b58`
(`panelbus_rx_dispatch_loop` in `panelbus_dispatch.c`) is ALSO independently
reconstructed in `cpsoc.c` as `cpsoc_poll_reg_reads` — same address, same real call shape,
two separately-transcribed C bodies. Left as two definitions per this project's standing
convention of flagging cross-file duplicates rather than silently unifying them.

**Files updated**: `cpsoc.c` (renamed `cpsoc_spi_submit_write`/`_read` to
`cpsoc_i2c0_submit_write`/`_read`, corrected every comment asserting SPI for these two
functions and their callers, added the full evidence trail as a file-level correction
note above the "third SPI-bus device" section — the ADC-polling half of that section's
own name is unaffected and remains genuinely SPI); `panelbus_dispatch.c` (upgraded its
own "very likely" flag to CONFIRMED with the address/xref evidence above); this README
(the two mentions above, struck through/marked RESOLVED rather than deleted).

**Confidence: high.** This rests on literal address/constant matches (the same function
address, the same base-address-selector function, the same hex constant read directly
from the image), not on register-offset-shape similarity alone — the ambiguity the
2026-07-18 pass left open is fully closed.

## 2026-07-18 — systematic parallel reconstruction pass

10 parallel agents worked from a pre-fetched static Ghidra dump (`all_decompiled.json`/
`all_data.json`) rather than the live Ghidra MCP bridge — the bridge has a real
concurrency bug under multiple simultaneous sessions, and pre-fetching a static dump per
agent sidesteps it entirely; a genuinely reusable methodology lesson for any future large
parallel pass on this project. Each agent owned a distinct set of files. Net result: **4
new files** (`i2c_by_gpio.c`, `panelbus_dispatch.c`, `heap_alloc.c`, `wire_dispatch.c`)
plus substantial expansion of every one of the 12 pre-existing subsystem files. Full
project compiles clean (16/16 files via `arm-none-eabi-gcc -fsyntax-only` on the build
server — a syntax-correctness gate, not proof of behavioral equivalence with the binary;
see the project `Makefile`'s own header comment).

Corrections made this pass (each also detailed in its own file's section below):
- `ctouchpanel.c`'s anchor range has **23** real functions, not 25 as previously
  documented — the 2 "missing" addresses turned out to be literal-pool data (switch-table
  constants, sentinels), not code.
- `FUN_c0014488` (the shared 5-`0x1d` lookup table) is called from `wire_dispatch_command`
  (`wire_dispatch.c`), **NOT** from `panelbus_dispatch.c`'s `FUN_c0007220` as an earlier
  pass's README note guessed — confirmed independently by both the `ctouchpanel.c` and
  `panelbus_dispatch.c` agents decompiling both candidate callers directly.
- `cpsoc.c`'s address range DOES have a real `__FILE__` anchor after all
  (`DAT_c0023190` resolves to the literal string `"../cpsoc.cpp"`) — the previous "no
  anchor found" conclusion was a false negative of Ghidra's own xref tracking for
  constant-propagated literals, not a real absence of the string reference.
- `clcdc.c`'s real compilation-unit boundary is `0xc0015afc` (end of `clcdc_blit_glyph`),
  not `0xc0015bf8` as previously documented — the gap between them belongs to a different,
  adjacent unit (`heap_alloc.c`'s own neighborhood, plus one `cobjectmgr.c` helper).
- `crypto_at88_self_test`/`FUN_c0001028` is now confirmed to have **zero callers anywhere**
  in the full 691-function xref data — a confirmed (if surprising) fact that this is
  genuinely dead/unreachable code, not an open question about if/when it runs.
- The `FUN_c0009534`/`FUN_c0009540` "ARM/Thumb interworking" mystery (not previously
  written up in this README) is resolved as a mundane artifact: a plain inline literal
  pool sitting after a noreturn tail-call, misread as instructions by linear disassembly
  because nothing marks the fall-through region as data — no Thumb code or interworking
  boundary involved. See `eva_board_main.c`'s own header comment for the full writeup.

**Known cross-file discrepancies surfaced this pass**, deliberately left unresolved
rather than silently reconciled (concurrent agents don't share context mid-pass, and
file content is ground truth over any single agent's guess):
- `ctouchpanel.c` corrected `FUN_c00140d4`'s name/role this pass (from the never-actually-
  decompiled "ctouchpanel_finalize_release" to `ctouchpanel_watch_idle_scalar`, with a
  real `(tp, int16_t new_value)` signature); `panelbus_dispatch.c`'s own concurrently-
  written extern for the same call site still cites the old name and a mismatched
  `(int, short)` signature, and flags this itself rather than being edited to match.
- `clcdc_draw_text` is declared `void` in `clcdc.c`, but at least one real call site
  (`panelbus_dispatch.c`'s own opcode `0x80`-`0x87` branch) captures and returns its
  value — both files note the inconsistency locally rather than settling on one
  canonical signature.
- The shared hard-halt/assert handler (`crypto_at88_fault`/`FUN_c000919c`) is invoked
  with differing visible argument counts (2/3/4) across different files' own call
  sites — each file documents this as an open, unresolved inconsistency at its own
  sites, not unified into one prototype.
- ~~`cpsoc.c` still documents its own third-device submit primitive (`FUN_c00032f8`) as
  SPI; `panelbus_dispatch.c`'s own hardware section this pass found the exact same
  register shape (`+0x08`/`+0x14`/`+0x1c`/`+0x24`) belongs to the real on-chip I2C0/I2C1
  controller, not SPI — flagged as a likely mis-attribution in `cpsoc.c`, not corrected
  there (out of `panelbus_dispatch.c`'s own scope this pass).~~ **RESOLVED 2026-07-19** —
  see the "SPI/I2C0 mis-attribution resolved" section below; struck through rather than
  deleted per this doc's own convention of leaving superseded claims visible.

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
conventions before starting a fresh session's work here. As of the 2026-07-18 pass, this
throwaway import is used heavily via a bulk pre-fetched static dump (rather than live,
per-call MCP queries) specifically to work around the live bridge's own concurrency bug
under multiple simultaneous agent sessions — see that pass's own section above.

## Subsystems

| Subsystem | Real source file (per embedded `__FILE__` strings) | Status |
|---|---|---|
| AT88 crypto-chip relay | `../CryptoAt88.cpp` | `crypto_at88.c` — done, see below |
| I2C bit-bang primitives | `../I2cByGpio.cpp` | `i2c_by_gpio.c` — done, see below |
| LCD controller | `../clcdc.cpp` | `clcdc.c` — done, see below |
| PSoC button/LED scan + resolved 3rd SPI device + queue/dispatch plumbing | `../cpsoc.cpp` | `cpsoc.c` — done, see below |
| Touch panel | `../ctouchpanel.cpp` | `ctouchpanel.c` — done, see below |
| Analog (knobs/sliders/pedals) | `../cad.cpp` | `cad.c` — done, full calibration engine, see below |
| Audio serial port | `../McAspHandler.cpp` (0 xrefs - see below) / `../MCU/Component/OmapL137Mcasp.cpp` | `mcasp.c` — core done, see below |
| Digital audio interface | `../CDix4192.cpp` | `cdix4192.c` — done, see below |
| Main entry / board bring-up | `../EvaBoardMain.cpp` | `eva_board_main.c` — done, see below |
| Object manager | `../cobjectmgr.cpp` | `cobjectmgr.c` — done, see below |
| Shared heap allocator | no `__FILE__` anchor — code-shape evidence | `heap_alloc.c` — core done, see below |
| SoC tick timer | `../MCU/OmapL108.cpp` | `omap_l108.c` — core done, see below |
| SPI peripheral (shared cad/cpsoc bus) | `../MCU/Component/OmapL108Spi.cpp` | `omap_l108_spi.c` — done, see below |
| USB device controller | `../MCU/Component/OmapL137Usbdc.cpp` | `omap_l137_usbdc.c` — done, see below |
| USB device controller — ISR/poll handler, endpoint-event dispatch, low-level register/FIFO/descriptor layer | no `__FILE__` anchor — shared-field-offset evidence with `omap_l137_usbdc.c` | `omap_l137_usbdc_ext.c` — done, see below |
| Second (hardware) I2C bus / internal command channel | no `__FILE__` anchor — code-shape + hardware evidence | `panelbus_dispatch.c` — done, see below |
| Master wire-protocol dispatch loops | no `__FILE__` anchor — code-shape + cross-file evidence | `wire_dispatch.c` — done, see below |

## `crypto_at88.c` — status: done (closure pass, 2026-07-18)

Every item this file's own README status previously left "still genuinely open" is now
resolved:

- **The entire `chip`/`void *` handle argument threaded through every function in this
  file is confirmed dead.** There is exactly one hardwired SDA/SCL GPIO pin pair (bit
  `0x40000`/`0x80000` off two fixed GPIO-object globals); `at88_gpio_set_sda`/`set_scl`/
  `set_sda_dir` and `at88_delay` all ignore whatever pointer they're handed and drive
  that same fixed pair unconditionally. This is the same root cause behind the "phantom
  forwarded parameter" pattern independently found in `cdix4192.c` and
  `eva_board_main.c` - all three are downstream consumers of this one dead-argument
  layer, not three unrelated bugs. The canonical, dedicated reconstruction of this
  shared low-level layer now lives in `i2c_by_gpio.c` (same addresses); this file's own
  copies are kept for continuity with earlier passes' citations, not as a divergent
  duplicate.
- **`at88_frame_command`'s real retry bound**: `DAT_c0001634` = 19999 decimal. No
  `at88_i2c_stop()` call exists anywhere in the retry loop - bus release only happens
  once, in the caller, after the whole transaction completes.
- **`at88_i2c_write`/`at88_i2c_read`'s full data-phase bodies** reconstructed, including
  the `at88_lock`/`at88_unlock` critical section - re-entering while already locked is a
  hard firmware fault citing `"../I2cByGpio.cpp"` (not `"../CryptoAt88.cpp"`), confirming
  this lock genuinely belongs to the I2C bit-bang layer's own translation unit.
- **`crypto_at88_self_test` is now confirmed to have zero static callers anywhere** in
  the full 691-function xref data, including inside `EvaBoardMain`'s own board bring-up.
  Either dead code from a factory-test build, or reached only through a mechanism static
  analysis can't see - the open question "does `$B8` happen before this runs" is moot
  until/unless a caller is found.
- **`crypto_at88_process_queue`'s queue producer resolved**: it's `wire_dispatch_command`
  (`FUN_c0007d1c`, now reconstructed in `wire_dispatch.c`). Host opcode `0xE0`
  (AtmelWrite) and `0xE1` (AtmelRead) each push a queue entry and, on success, set
  status bit 13 via a generic event-flag primitive - `master_dispatch_tick`
  (`wire_dispatch.c`) sees that bit and calls `crypto_at88_process_queue` to drain the
  queue for real over I2C.
- **The write path's own event is confirmed fire-and-forget** - traced both at the
  queue-relay level and one level down inside `at88_i2c_write` itself: neither ever
  calls an event-relay function. A successful `$B0`/`$B4`/`$B8` write produces no
  host-visible completion event.
- **`at88_relay_read_result`'s wire-format byte order**, already corrected once in the
  2026-07-17 re-verification pass (word 1 and the trailing payload loop both need
  byte-reversal), got one further fix this pass: the trailing loop's continuation
  condition was wrong (`i + 4 <= len` instead of the real `i < len`), meaning the real
  firmware processes one more trailing 4-byte group than previously modeled for most
  `len` values. A genuine, documented quirk (not "fixed away"): the queue producer's own
  33-byte length gate exceeds the `struct at88_queue_entry`'s real 28-byte data
  capacity, so for `len` in the high-20s both the real producer and this function walk a
  few bytes past the nominal buffer - transcribed faithfully rather than clamped.

**Still open**: at88_i2c_bus_reset's own caller (`crypto_at88_queue_init`) still passes
an uninitialized stack buffer as `chip` at its one call site (harmless, same dead-chip
pattern); a handful of `at88_delay` unit-value literals whose real timing isn't decoded.

## `i2c_by_gpio.c` — status: done

New file this pass. Reconstructs the genuinely shared GPIO bit-bang I2C driver
(`../I2cByGpio.cpp`, 13 functions), previously only cross-referenced piecemeal from
inside `crypto_at88.c` and `cdix4192.c`, never itself given its own dedicated
reconstruction.

- **Anchor**: the literal string lives at `0xc0022cf8`, reached indirectly through
  `i2c_gpio_busy_guard_enter`'s fault call on a bus-already-busy condition - the same
  single-assert-site pattern already used for `CryptoAt88.cpp`/`clcdc.cpp`/
  `ctouchpanel.cpp`/`CDix4192.cpp` elsewhere in this project.
- **Confirms the whole bus is a hardware singleton**: one fixed GPIO bank (base
  `0x01E26000`, a real TI OMAP-L138/DA850 GPIO controller address), SCL = bank-0 bit 19,
  SDA = bank-0 bit 18. The `chip` parameter every top-level caller (`crypto_at88.c`'s
  write/read wrappers, its queue-init/bus-reset call, `cdix4192.c`'s reg wrappers)
  threads through is dead both at its source (nothing meaningful is ever put into it -
  at least four independent top-level call sites pass an uninitialized local stack
  buffer instead of their own real handle) and at its sink (nothing in this file's own
  GPIO-level functions ever reads it).
- **`i2c_gpio_stop`/`0xc00013cc` resolves `crypto_at88.c`'s previously-unresolved
  `at88_i2c_stop`** - same address, confirmed by call pattern and by being the function
  every transaction-level entry point here calls exactly once, right before clearing the
  busy guard.
- **`i2c_gpio_frame_command`'s retry bound** (19999, `0x4e1f`) matches
  `crypto_at88.c`'s own independently-derived value exactly. The CDIX-side
  `i2c_gpio_addr_start` uses a MUCH smaller 5-retry bound - a real, confirmed asymmetry
  between the two devices' post-write poll behavior, not a transcription inconsistency.
- **`i2c_gpio_read_reg8`** (= `cdix4192.c`'s `cdix_i2c_read_reg`) is confirmed to
  **always return 0**, regardless of whether any internal ACK/NACK phase actually
  succeeded - a genuine firmware quirk, consistent with `cdix_reg_read` (`cdix4192.c`)
  already not checking this return value.
- Two address-adjacent functions (`gpio_bank_get_base`/`FUN_c0001990`,
  `hw_timer_busy_wait`/`FUN_c0001aa0`) are deliberately left `extern` rather than
  reconstructed here - both have far too many callers (66 and 16 respectively) spanning
  the entire firmware image to be private to this driver, despite living at a nearby
  address.

**Still open**: `i2c_gpio_delay`'s own param wiring into the underlying hardware timer
engine (behaviorally confirmed, not independently verified by register-level
disassembly); the real meaning of `icmdr |= 0x800`/bit 11 touched at the end of every
transaction (left as an opaque, undecoded OR).

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
- **`clcdc_dispatch_set_palette_hook`** — found this pass (coverage sweep, 2026-07-18):
  a genuine clcdc.cpp function missed by earlier passes, sitting in a gap-free
  instruction stream between `clcdc_cursor_set_stride` and `clcdc_cursor_init`. A
  one-line wrapper over a shared RGB→RGB565 palette-entry-set primitive; its sole
  caller is `wire_dispatch_command`'s (`wire_dispatch.c`) opcode `0xc5` handler.
- **`clcdc_draw_edge`** — RESOLVED this pass (previously guessed to take a
  caller-supplied `direction` argument): it is a **one-parameter function**. Direction,
  inset distance, and colour index are all read from three self-mutating global
  counters the function itself advances every call - a fully self-driven "marching
  ants" selection border that shrinks from the screen edge to centre (799/2, 599/2)
  over 301 side-cycles, then advances to the next highlight colour and starts over. Zero
  static callers found - almost certainly driven by a timer/tick ISR.
- **Bitmap font** (`clcdc_font_glyph`/`_advance`) — a proportional monospace-storage
  bitmap font with per-glyph advance widths; struct field names corrected in the
  2026-07-17 re-verification pass and again this pass (`+0x05` is confirmed to be a
  per-glyph scanline count, `glyph_rows`, not a `first_char` bound - that bound is a
  hardcoded literal in code, never backed by any struct field).
- **`clcdc_blit_glyph`** — **FULLY TRANSCRIBED this pass** (previously left as a
  documented contract to avoid a transcription bug in code this fiddly). Rasterizes
  each glyph into a shared 1bpp work-bitmap (100-byte row stride), NOT the visible
  16bpp framebuffer directly - the destination plane is strongly suspected (matching
  stride+addressing, not independently confirmed by a live pointer comparison) to be
  the same plane `clcdc_draw_text`'s own second pass reads from. Handles sub-byte x
  alignment via a 1/2/3-destination-byte shift-and-mask scheme.
- **`clcdc_draw_text`** — drives `clcdc_blit_glyph` per character, then a second pass
  compositing the shared 1bpp plane into the real framebuffer. RESOLVED this pass: the
  3-entry font table (`DAT_c00157b4`) is confirmed a genuine `const struct clcdc_font *`
  array; the second pass is corrected from a sparse "highlight only" overlay to a full
  background-fill-plus-highlight composite of the whole text bounding box (every pixel
  in the box is unconditionally repainted).
- **`clcdc_test_pattern`** — the built-in 7-mode factory test-pattern generator, called
  from a sequential if/else-if chain (not a dispatch table, corrected 2026-07-17), with
  the exact key-code-to-mode mapping (`0x1f`-`0x25`) confirmed this pass.
- **`clcdc_progress_bar`** — RESOLVED this pass: `omap_tick_scale`/`FUN_c001e3f8` is
  confirmed a **generic signed-division utility** (shared firmware-wide, also used by
  `omap_l108.c`'s own `omap_tick_elapsed_scaled`), not a percent-specific helper. The
  exact scaling math (`width_px = percent*512/100`, truncating) and the mirrored-row
  offsets (rows 348/349, exactly one row = 800px apart, matching the 2026-07-17
  correction) are both fully confirmed.

**Important boundary correction, this pass (coverage-sweep, 2026-07-18)**: the real
`clcdc.cpp` compilation unit ends at `clcdc_blit_glyph` (`0xc0015afc`), **not**
`0xc0015bf8` as previously documented. A full sweep of the gap (`0xc0015afc`-`0xc0015bf7`)
found 4 more functions, all false proximity: three trivial "zero a global" stubs whose
only callers live deep past `0xc0019000` (nowhere near confirmed `clcdc.cpp` code), and
one self-recursive list-walk helper (`0xc0015bc8`) that turned out to be `cobjectmgr.c`'s
own `cobjectmgr_free_list_recursive`, calling directly into the heap allocator
(`heap_alloc.c`). `0xc0015bf8` itself is `cobjectmgr_object_destroy` — see `cobjectmgr.c`
and `heap_alloc.c`.

**Genuinely not resolved, left honest rather than guessed:**
- Which 3 fonts `clcdc_font_table`'s entries actually point to (a runtime-populated
  pointer array, zeroed in this static dump).
- The precise bit-level shift/mask derivation inside `clcdc_blit_glyph` (transcribed
  faithfully, but not independently verified against real hardware) and the exact
  per-direction geometry `clcdc_draw_edge`'s 4 modes set up.
- `DAT_c0015b00`'s (the blit-clip-bound) actual numeric value.

## `cpsoc.c` — status: done

- **`__FILE__` anchor CORRECTED this pass**: this section previously claimed no
  `"../cpsoc.cpp"` string anchors this address range - the ONE exception to this
  project's usual anchoring standard. That claim is now retracted: once the section's
  `DAT_` fault-call file arguments were actually resolved, every one of them
  (`cpsoc_event_queue_push`, `cpsoc_queue_push_validated`,
  `cpsoc_queue_command_with_retry`, `cpsoc_analog_poll_channel`, `cpsoc_dispatch_tick`'s
  own family) resolves to the literal string `"../cpsoc.cpp"` at `0xc0023190`. The
  earlier "full-image string search found no xref" conclusion was a false negative of
  Ghidra's own xref tracking for constant-propagated literals, not a real absence.
- **`cpsoc_i2c_dispatch`** (the callee behind every host-facing switch/LED-row read,
  opcodes `0x50`/`0x51`/`0x52`) fully re-decompiled: it does **NOT** do a live I2C
  access at all. It enqueues into the SAME 4-instance ring buffer the third-SPI-device
  LED-bargraph chain uses, then posts event-flag bit `0x1000`. This closes the loop
  end-to-end: a host-facing switch/LED-row read is asynchronous, not synchronous
  hardware access.
- **`cpsoc_dispatch_tick`** (`FUN_c0010f08`) — the real, confirmed consumer for BOTH
  the host-facing queue and the third-device queue: called directly from
  `master_dispatch_tick` (`wire_dispatch.c`) on status bit `0x1000`, draining/reading
  every queue instance in one pass. A real, documented asymmetry: reg `0x79` is only
  ever drained (write-queue side), reg `0x7b` is only ever read live - not smoothed into
  a symmetrical pattern that isn't what the disassembly shows.
- **`cpsoc_spi_submit_write`/`_read`** (`FUN_c00032f8`/`FUN_c00033f0`, renamed
  `cpsoc_i2c0_submit_write`/`_read` — **RESOLVED 2026-07-19**, see that section below)
  fully re-decompiled: a bounded busy-wait byte-stream submit/read primitive. Flagged,
  not resolved at the time this entry was written: `panelbus_dispatch.c`'s own hardware
  section this pass found the exact same register shape (`+0x08`/`+0x14`/`+0x1c`/`+0x24`)
  belongs to the real on-chip I2C0/I2C1 controller - this may genuinely be the same I2C
  hardware, not SPI as currently modeled in this file. See that file's own cross-file
  note.
- **The four LED-bargraph tag handlers** (`cpsoc_led_cycle`/`_toggle`/`_ramp`/
  `_quantize`) fully transcribed this pass. Their four separate "DAT_ offset, unresolved"
  constants all resolve to the SAME literal field, `cpsoc+0x821` - not four independent
  struct fields as previously modeled.
- **`cpsoc_event_queue_pop`** newly reconstructed, resolving the full 0x208-byte
  per-instance ring layout (128 slots + write-index + read-index + count).
- **`panel_gpio_reset_pulse`** (formerly unidentified `FUN_c0000ba0`) resolved: a real
  GPIO bank-3/bit-8 assert-hold-deassert pulse (60000-tick hold), independently
  reachable both from the diagnostic menu's key-8 action and from
  `wire_dispatch_command`'s own opcode-9 handler - concrete evidence they're the same
  logical reset action reached two ways.

**Still open**: `cpsoc_analog_poll_task`'s own invocation mechanism (never returns, zero
static callers - two sibling never-returning loops with the identical signature were
also found this pass and left unattributed, a real circumstantial lead for how tasks
like this get started, not a resolved mechanism); most `DAT_` lookup-table contents.
~~whether `cpsoc_spi_submit_write`/`_read` are genuinely SPI or the same I2C0 hardware
`panelbus_dispatch.c` documents (open cross-file question, see this project's 2026-07-18
pass summary above).~~ **RESOLVED 2026-07-19** — I2C0, confirmed by address/xref evidence,
not just register-shape similarity; see the "SPI/I2C0 mis-attribution resolved" section
near the top of this README.

## `ctouchpanel.c` — status: done

**Function count CORRECTED this pass**: the confirmed anchor range
(`0xc0014010-0xc0014f84`) has **23** real Ghidra function objects with no unaccounted
address gaps, not 25 as previously documented - a fresh function-boundary sweep found
the 2 "missing" addresses are literal-pool data (switch-table constants, a global
config-object pointer), not code. All 23 are reconstructed or explicitly characterized
below.

- **`ctouchpanel_sample_raw`** — reads 6 raw ADC channels into a 7-byte record, gated on
  a validity check unless a touch is already active.
- **`ctouchpanel_watch_idle_scalar`** — CORRECTED and renamed this pass (was
  `ctouchpanel_finalize_release`, a name/role that was never actually decompiled and is
  wrong - this function has nothing to do with touch release). Real behavior: reuses
  `cad.c`'s own shared 2-tap smoothing primitive at CAD's own reserved/unused slot
  `0x1e` (30) against ITS OWN local scratch fields, not CAD's 38-slot engine - a
  "borrow a shared primitive, keep the data local" pattern, independently
  cross-confirmed by `cad.c`'s own documentation of slot 30 as excluded from its sweep.
- **`ctouchpanel_check_timeout`** — release-by-timeout debounce, 5 consecutive stale
  ticks triggers a call into `ctouchpanel_watch_idle_scalar` (not a release path, see
  correction above).
- **`ctouchpanel_push_event`** — the confirmed anchor function: a 128-entry ring buffer
  with real ±4 jitter/hysteresis snapping and a hard-fault overflow guard.
- **`ctouchpanel_pop_event`** — NEW this pass: the real dequeue counterpart, previously
  undocumented. Confirmed called every master-dispatcher tick from a touch-event
  consumer outside this file's range that relays events to the host via the same shared
  USB-submit primitive already established across `crypto_at88.c`/`cobjectmgr.c`/`cad.c`.
- **`ctouchpanel_update`** — **FULLY RECONSTRUCTED this pass** (previously only
  structurally described). The central down/move/up debounce state machine: a global
  3-consecutive-sample arming debounce, the exact 6-per-channel-setter call order (this
  IS the real evidence for the channel mapping: ch0/ch1 are raw X/Y, ch2-5 feed the
  calibration-bracket references — NOT a second axis pair or pressure/Z sensing as
  earlier passes guessed), and the real release path (an invalid sample, whether or not
  armed, falls to a shared tail that pushes one final release event and clears state).
- **Four small per-channel bracket setters and two large compute-and-push functions**
  fully transcribed, including a real, confirmed asymmetry: the Y axis's flip is
  conditional on a global config flag, the X axis's own flip is unconditional.

**CORRECTED (cross-file, jointly with `panelbus_dispatch.c`) this pass**:
`FUN_c0014488` (the shared 5-`0x1d` lookup table) is called by `wire_dispatch_command`
(`wire_dispatch.c`), **NOT** by `cobjectmgr.c`'s `FUN_c0007220` as an earlier pass's
README note guessed - independently re-checked directly against fresh decompiles of
both candidate callers this pass.

**Real cross-file finding**: `ctouchpanel_cad_channels_enable`/`_disable` toggle CAD
engine slots `0x20`-`0x25` — the SAME 6 slot numbers `cad.c`'s own
`cad_calibration_select_slot` step-5 override picks from. Concrete, address-confirmed
evidence that CAD's own 38-channel analog engine is what actually captures the touch
panel's 6 raw ADC channels, rather than the touch panel driving its own independent ADC
hardware.

**Still open**: the physical scalar `ctouchpanel_watch_idle_scalar` watches (no reader
of its own "changed" flag found in this address range); a NEEDS LIVE QUERY item at
`ctouchpanel_check_timeout`'s own call site (the real second argument fed to the shared
smoother); the unbound glue code (no Ghidra function boundary) tying `FUN_c0014488` and
a sibling lookup table (`FUN_c00145c4`) together; exactly how CAD's captured
slot-`0x20`-`0x25` samples land in `ctouchpanel_state`'s own `adc_ch` array (same object
vs. a copy - the sharing itself is confirmed, the mechanism isn't).

## `cad.c` — status: done, full calibration engine reconstructed

Anchor: `"../cad.cpp"` has 2 xrefs (`cad_channel_group`'s and `cad_trigger_calibration`'s
own assert calls), confirming the `0xc001335c-0xc0013f5c` range (23 functions) as this
subsystem's real compilation unit.

- **`cad_init`** — subsystem bring-up: hardware handle acquisition, two 38-entry
  calibration/filter state tables reset, 5 expansion-pedal presence probes, a hardware
  reset/config sequence, and - the key finding - registers this subsystem's own opcode
  handlers (`0x78`, `0x79`) with the shared wire-protocol dispatcher.
- **`cad_channel_group`** — a real, disassembly-confirmed 12-channel-to-4-group lookup
  table: `{0,2,3,6,9}->0`, `{1,4,5,7,10}->1`, `{8}->2`, `{0xb}->3`.
- **`cad_channel_eligible`** — a 12-channel enable-bitmask check gating calibration
  eligibility per channel.
- **`cad_trim_adjust`** — a real host-facing entry point, called from
  `panelbus_dispatch.c`'s own `panelbus_cmd_dispatch` for opcode `0x50` (corrected this
  pass from an earlier attribution to `cobjectmgr.cpp`'s `FUN_c0007220` — that function
  is now itself reconstructed as `panelbus_dispatch.c`, see that file). Notably reuses
  register value `0x7a` with a *different* local meaning than `cpsoc.cpp`'s own `0x7a`
  register-bank split - confirms `0x7a` is a per-subsystem-local register number, not a
  single firmware-wide constant.
- **`cad_trigger_calibration`** — the confirmed anchor function: a real host-facing
  calibration trigger with genuine consistency-check asserts and a tick-counter-gated
  calibration sequence.

**Full calibration engine** (all remaining 18 functions decompiled, 2026-07-17 pass):
38-slot parallel state (two smoothed-value caches, a "changed" bitmask, a
"masked/excluded" bitmask, a threshold/cap pair); `cad_calibration_sweep` (the real
per-channel sweep, a 7-step SPI sequence for eligible channels vs. a fixed 3-step
sequence for excluded ones); `cad_calibration_smooth_sample` (a genuine 2-tap debounce
filter); `cad_calibration_pop_changed` (a real rotating bitmask scanner, IRQ-guarded);
the real explanation for `cad_init`'s "5 expansion-pedal presence probes" (seeding the
first 5 bytes of the exclude-mask, one byte per pedal jack); a separate small
pedal-value-encoding trio (`cad_pedal_object_reset/_probe/_init`,
`cad_pedal_send_release`, `cad_pedal_encode_step`) tied to the 38-slot engine only via
the shared presence probe — `cad_pedal_send_release` routes a pedal "off" event through
`cpsoc.c`'s own `cpsoc_i2c_dispatch` primitive rather than cad's own SPI bus.

**Real cross-file tie confirmed this pass (`panelbus_dispatch.c`)**: `cad.c`'s own
registered opcode handlers (`0x78`/`0x79`, set up by `cad_init`) are fed by
`panelbus_dispatch.c`'s own TX-ring-drain loop (`panelbus_tx_drain_channel`), which
calls `cad_trigger_calibration` directly on each drained record — **not** via
`wire_dispatch_command`, which only reaches `cad.c` through the separate
`cad_trim_adjust` (opcode `0x50`) entry point. Two genuinely distinct paths into the
same subsystem.

**Still open:** `cad_calibration_select_slot`/`cad_calibration_advance_group`'s
branch-by-branch real-world meaning; `cad_calibration_init_slot`'s own caller (not
traced); the real contents of `cad_calibration_sweep_table` and several sentinel
constants; whether the `0xe0` message-tag byte `cad_trigger_calibration` checks has any
relation to the AT88 relay's own `0xe0` opcode.

## `mcasp.c` — status: core done, real bugs fixed in a 2026-07-18 re-verification pass

The top-level `"../McAspHandler.cpp"` filename string has **zero xrefs anywhere in the
image**. The real functional driver lives in a different, lower-level compilation unit -
`"../MCU/Component/OmapL137Mcasp.cpp"` (a flat 11 real xrefs, all from `mcasp_init`).

**2026-07-18 re-verification pass** went through `mcasp_init`'s full decompile
field-by-field (previously only spot-checked) and found/fixed several real errors, not
just cosmetic gaps:
- The prior `mcasp_state` struct's field offsets did NOT actually match their own doc
  comments (a real miscompile risk) - replaced with plain byte-offset pointer casts
  throughout, matching this project's own convention for structs with non-contiguous
  confirmed offsets.
- `mcasp_reset_stage`'s shared attempt counter was a placeholder `NULL` - fixed to read
  the counter's address from the real global pointer indirection every stage.
- `mcasp_clock_lock`/`mcasp_clock_unlock` were misnamed and, worse, mis-modeled as a
  critical-section pair: their real bodies do NO locking or memory access at all - pure
  constant selectors, renamed `mcasp_clock_param_select_a`/`_b`, confirmed by a second,
  independent call site that captures their return values into globals (proving they're
  getters, not void-return synchronization primitives).
- The three "step" functions were shown called with no visible arguments in the
  decompile, but their own bodies each take one - restored explicitly this pass as
  ARM's r0-register-reuse (the preceding select-function's return value, still live in
  r0, IS the real argument).
- The TX/RX serializer block was previously called "genuinely symmetric" - false: 2 of 7
  paired fields differ (`+0x70`/`+0xb0`: `0x80` vs `0xc0`; `+0x68`/`+0xa8`: two different
  DAT_ constants).
- All fault-call source line numbers (325-514 decimal) and the file-string pointer
  (confirmed `"../MCU/Component/OmapL137Mcasp.cpp"`) are now resolved, independently
  re-confirming the file-scope correction.

`mcasp_clock_step_c`'s own address-generation shape (a shared global base indexed by a
small integer over a fixed-size record array, matching `mcasp_init`'s own caller's
identical pattern) now reads more like EDMA3 PaRAM descriptor setup for the McASP
TX/RX FIFOs than a PLL divider update - a genuine, evidence-based correction of the
earlier "PLL divider" guess, though not confirmed field-by-field against TI's own PaRAM
layout.

**Still open:** a second, address-adjacent (`0xc0002c60`) but Ghidra-unbounded code
region confirmed via raw disassembly to be a differently-configured second McASP
instance's init sequence (or a reconfigure/deinit path) - not reconstructable as a named
function since Ghidra has no boundary for it; a smaller "reduced reinit" sibling
(`FUN_c0003228`) with no filename-string ownership evidence; the real 9-vs-10
serializer pin-function meaning.

## `cdix4192.c` — status: done

Anchor: `"../CDix4192.cpp"` has exactly one xref, the clean single-assert pattern.

- **`cdix_reg_write`/`cdix_reg_read`** — confirmed to sit on the exact same shared I2C
  bit-bang primitives now dedicated-reconstructed in `i2c_by_gpio.c`. Fixed I2C address
  `0x70` for every access. The `chip` parameter these two wrappers appear to take is NOT
  actually forwarded to the I2C primitive underneath - the real disassembly passes an
  uninitialized local stack buffer instead (re-verification pass, 2026-07-17) -
  `i2c_by_gpio.c`'s own dedicated pass this round independently confirms the whole bus
  is a hardware singleton, so this is a real, harmless firmware quirk rather than an
  unresolved mystery.
- **`cdix_configure_and_verify`** — the confirmed anchor: walks a `{register, value}`
  table twice - write, then read-back-and-verify. **Zero static callers found** - the
  real invocation mechanism remains genuinely unresolved (NOT explained by
  `eva_board_main.c`'s own init table, which is now known to have exactly one, unrelated
  entry).

**Verification pass, 2026-07-18**: `DAT_c000fc44` (the config-verify fault call's `file`
argument) resolved to `0xc0023180` - exactly the address of this file's own
`"../CDix4192.cpp"` string, confirming the anchor a second, independent way. The config
table's own address (`0xc001fb6c`) is directly confirmed; its 3 real entries
(`{0x7f,0x00}`, `{0x03,0x29}`, `{0x04,0x03}`) were already known from the 2026-07-17
pass. The 2 padding bytes per table entry remain unread (needs a live memory query - no
data-segment symbol resolves them in this static dump).

## `eva_board_main.c` — status: done

Anchor: `"../EvaBoardMain.cpp"` has exactly one xref, inside the fault-wrapper function
below - but unlike every other subsystem this project has covered, **Ghidra's
auto-analysis never assigned a containing function boundary to this code**. Everything
here was read directly from raw disassembly.

**New this pass (2026-07-18): the full reset-vector/crt0 chain, from power-on to
`eva_board_main`.** The ARM exception vector table's reset entry (`0xC0000000`) branches
through its own literal pool to `eva_board_reset_handler` (`0xc0009534`), a genuinely
tiny 3-instruction function (load initial SP, install it, tail-call into
`eva_board_crt0`, never returns). `eva_board_crt0` (`0xc00055b8`) is a classic embedded
crt0: zeroes a BSS-style region, then 11 back-to-back per-subsystem/scheduler-table
bring-up calls (one of which populates the task-control-block table
`eva_board_start_task`'s own scheduler depends on), then falls DIRECTLY into the same
scheduler idle/dispatch tail `eva_board_start_task`'s own callee uses, with no return.

**CLOSED this pass**: the `FUN_c0009534`/`FUN_c0009540` "ARM/Thumb interworking" mystery
is resolved as a mundane artifact via a follow-up live disassembly query - a plain
literal pool sitting inline in the code stream right after `eva_board_reset_handler`'s
own noreturn tail-call, misread as instructions by linear disassembly because nothing
marks the fall-through region as data. No Thumb code or interworking boundary involved;
the earlier "FUN_c0009540 calls itself" artifact was purely this misread literal pool.

- **`eva_board_init_table`** — a generic function-pointer-table walker. The table has
  exactly **ONE entry** (confirmed via `read_memory`, SPI/USB cleanup pass 2026-07-17),
  pointing to a lazy-init singleton wrapper - NOT a multi-function driver-probe table.
  `crypto_at88_self_test`, `cdix_configure_and_verify`, and `cobjectmgr_object_destroy`
  are NOT called through this table; their real invocation mechanism remains genuinely
  unresolved.
- **`eva_board_main`** — the actual boot entry point: calls `eva_board_init_table`, then
  `eva_board_final_setup` (now fully reconstructed, see below), then
  `eva_board_start_task(1, 4)` (now fully reconstructed, see below), then an
  unconditional `for(;;)` loop calling `master_dispatch_tick` (now reconstructed in
  `wire_dispatch.c`) forever.
- **`eva_board_watchdog_fault_wrapper`** — the confirmed anchor. Takes NO effective
  parameter (re-verification pass, 2026-07-17 - the same "phantom forwarded parameter"
  pattern independently found in `cdix4192.c`). Calls `cobjectmgr.c`'s own
  `cobjectmgr_hardware_fault_watchdog`, and if that call ever actually returns (which,
  per that function's own confirmed structure, should never happen), raises a separate
  hard fault right here.

**`eva_board_final_setup` — FULLY RECONSTRUCTED this pass.** A per-subsystem hardware
bring-up sequencer: sets three fixed handle fields, runs a hardware compatibility/
self-test gate (`eva_board_compat_check` - the same "draw an error, hang forever"
fail-fast idiom already confirmed for `crypto_at88.c`'s own assert handler), then twelve
further per-subsystem bring-up calls. Three are cross-file CONFIRMED by address:
`cad_init`/`cad_pedal_object_init` (`cad.c`) and `omap_l137_usbdc.c`'s own USB object
bring-up (`omap_usbdc_object_init`) - the CROSS-FILE finding that resolves
`omap_l137_usbdc.c`'s own previously-open "role not traced" question for its own USB
bring-up caller. Three more have suggestive-but-not-confirmed evidence (plausibly
`clcdc.cpp`'s own top-level constructor, `cpsoc.cpp`'s own state-table clear, and a
second USB register-block setup). Finishing touches register four cpsoc register banks
(`0x78`/`0x79`/`0x7b`/`0x7a` - the SAME four banks `eva_board_compat_check`'s own probe
loop cycles through) via `cpsoc_i2c_dispatch`.

**`eva_board_start_task` — FULLY RECONSTRUCTED this pass.** A real primitive of this
firmware's own small priority-based task scheduler, NOT a generic "spawn a task with a
function pointer" call - it operates on a pre-existing task-control-block table
(populated at boot by `eva_board_crt0`'s own init sequence), looking a task up by index
and either marking it ready or updating its priority. `eva_board_main`'s own call,
`(1, 4)`, really does mean "task index 1, priority level 3" (a 1-based-to-0-based
conversion). Its own three scheduler callees (ready/requeue/dispatch) are structurally
confirmed but not transcribed as C (dense pointer/bitmap arithmetic, no independent
verification path).

**Still genuinely open**: whether `eva_board_main` is itself one of the tasks made ready
during `eva_board_crt0`'s own init sequence (the strongest remaining lead tying the
reset-vector section to the rest of this file, not confirmed - the task table's actual
contents weren't read this pass); seven of `eva_board_final_setup`'s twelve callees have
no confirmed cross-file attribution; `eva_board_compat_check`'s own `0x17`/`0x27`
board-id constants' physical meaning; the real invocation mechanism for
`crypto_at88_self_test`/`cdix_configure_and_verify`/`cobjectmgr_object_destroy`.

## `cobjectmgr.c` — status: done

**Anchor-sweep pass, 2026-07-18**: re-swept every literal-pool reference to the
`"../cobjectmgr.cpp"` string address directly, rather than relying on the earlier "6
xrefs" count. Found **NINE** functions with their own local copy of that string
pointer, not six - three not previously catalogued, including `wire_dispatch_command`
itself (see below).

- **`cobjectmgr_tick`** — the confirmed core: called unconditionally every single
  master-dispatcher tick. Polls a single "current object" slot; if occupied, dispatches
  on a one-byte type tag to one of exactly two handlers (any other tag is a hard fault).
  **Genuinely new finding this pass**: the "current object" slot is populated by
  `wire_dispatch_command` (`FUN_c0007d1c`, now reconstructed in `wire_dispatch.c`) - its
  own opcode switch has a case for bytes `0xc4`/`0xc6` (the exact two tag values this
  function dispatches on) that does nothing but hand the queued command's payload
  pointer/length straight into this struct's own fields.
- **`cobjectmgr_handle_type_a`** — fully reconstructed this pass: tag `0xc4`, an 11-byte-
  record solid-colour pixel-run drawer. Confirmed to draw directly into `clcdc.c`'s own
  framebuffer/palette globals - a genuinely new structural finding that `cobjectmgr`
  sits on top of `clcdc` as a renderer for wire-triggered object types, not an
  independent drawing surface.
- **`cobjectmgr_handle_type_b`** — tag `0xc6`, structurally documented (too dense to
  transcribe with confidence): a four-way wraparound pointer walk packing 4 sub-fields
  per source dword into a separate circular output buffer, ALSO confirmed to draw
  through `clcdc.c`'s own palette global.
- **`cobjectmgr_release_object`** — CORRECTED this pass: despite the name this
  project's earlier pass gave it, the real function is NOT object-specific and does
  NOT release anything - it takes no real parameter and unconditionally returns a fixed
  32-bit constant, with 8 call sites across at least 5 completely unrelated functions
  elsewhere in the firmware. A shared, generic constant accessor. The real "release" in
  `cobjectmgr_tick` is entirely the inline `NULL` assignment already there.
- **`cobjectmgr_object_cleanup`** — resolved: a thin, one-line wrapper into an
  unrelated, not-cobjectmgr-owned hardware-descriptor-table writer, always called with
  the same hardcoded indices.
- **`cobjectmgr_notify_host`** — one of a small family of host-notify event senders
  (siblings not individually reconstructed), confirmed the anchor xref for cobjectmgr.c
  that explains most of the "9 vs 6" discrepancy above.
- **`cobjectmgr_hardware_fault_watchdog`** — fully reconstructed and shown to be
  genuinely non-buggy: blocks indefinitely on a real event-group wait primitive for a
  hardware-fault status bit (confirmed via symmetry with `master_dispatch_tick`'s own
  opening event-group poll), and only escalates to the firmware's hard-halt assert once
  that real event actually fires - the bridge from a real hardware fault
  interrupt/event to the software fault path, not vestigial code.
- **`cobjectmgr_free_list_recursive`** — newly identified this pass: a small
  self-recursive singly-linked-list walk-and-free helper, physically found in what an
  earlier pass wrongly attributed to `clcdc.cpp`'s own boundary (see `clcdc.c`'s
  boundary correction above).
- **`cobjectmgr_object_destroy`** — a real C++ virtual destructor (the anchor for
  `heap_alloc.c`). Full struct layout now transcribed as real C, using
  `heap_alloc.c`'s `heap_free` as an opaque, already-characterized dependency (its own
  internals are not transcribed here, consistent with this file's own treatment of
  `cobjectmgr_handle_type_b`). Zero static callers found - consistent with being reached
  only through vtable/virtual dispatch. Has an early-return guard for a fixed
  self-reference sentinel (re-verification pass, 2026-07-17).

**Still open:** `cobjectmgr_handle_type_a`'s own trailing 3 bytes (a "width" field) are
never accounted for in its own stream-position bookkeeping - deliberate fixed trailing
field, or this project's now-familiar "looks used but isn't load-bearing" register
pattern, not resolved; what produces tag-`0xc6` objects and what consumes
`cobjectmgr_handle_type_b`'s own output ring buffer.

## `heap_alloc.c` — status: core done

New file this pass. The firmware's shared, general-purpose heap allocator: a segregated
free-list allocator (small exact-fit bins up to ~504 bytes, indexed logarithmic "tree"
bins beyond that), boundary-tag free chunks with immediate coalescing, a page-granular
trim-back-to-OS path, and an sbrk-style break-pointer primitive underneath it all.

- **NOT anchored by a `__FILE__` string** (a full image string search found none for
  this address range) — attributed on pure code-shape evidence, per the discipline
  `clcdc.c`'s own header comment already established when it first found and excluded
  this code from being mistaken for LCD-specific.
- The code shape (chunk header = size|flags word immediately preceding the user
  pointer, free chunks' fd/bk overlaid into small-bin array storage, a treebin-index
  computation ladder duplicated at three call sites, a dedicated "designated victim"
  slot, a top/"wilderness" chunk grown via sbrk) matches the well-known Doug Lea malloc
  (dlmalloc) family closely enough to almost certainly be a compiled dlmalloc
  derivative — noted as a reading aid, not license to substitute reference source; every
  claim below is grounded in what Ghidra actually shows for this binary.
- **`heap_sbrk`/`heap_trim`** fully transcribed - simple, unambiguous control flow, no
  bin-array walking.
- **`heap_malloc`/`heap_free`** — the allocator core, documented structurally rather
  than transcribed line-for-line (per this project's established practice for code this
  dense with no way to verify against real hardware - same treatment as `clcdc.c`'s own
  `clcdc_draw_edge`/`clcdc_blit_glyph` and `cobjectmgr.c`'s own `cobjectmgr_handle_type_b`).
- **`heap_lock`/`heap_unlock`** confirmed genuinely empty bodies (no observable side
  effect in this decompile) - bracketed around every allocator entry point regardless,
  most plausibly a critical-section pair Ghidra fully collapsed away for a build with no
  real contention.
- Genuinely shared firmware-wide, not cobjectmgr-specific: `heap_malloc`'s own callers
  alone span at least 5 unrelated functions well outside any subsystem this project has
  otherwise anchored.
- The `handle` parameter every function in this file takes is confirmed dead all the
  way through `heap_sbrk`/`heap_trim`/`heap_lock`/`heap_unlock` too - this project's
  now-repeated "phantom forwarded parameter" finding, at scale.

**Still open:** `heap_state`'s exact struct layout beyond the confirmed binmap/
designated-victim/per-bin-sentinel fields; the fixed constants controlling wilderness
growth; the three separate high-water-mark stat globals' individual purposes.

## `omap_l108.c` — status: core done

Anchor: `"../MCU/OmapL108.cpp"` has exactly one xref, inside `omap_tick_init`'s own
init-guard assert.

- **`omap_tick_init`/`omap_tick_read_raw`/`omap_tick_elapsed_scaled`** — a small, generic
  free-running-tick-counter API.
- **Cross-file correction to `clcdc.c`**: `omap_tick_scale`/`FUN_c001e3f8` is confirmed
  a SECOND, unrelated caller of the same function `clcdc_progress_bar` uses (divisor
  `0x96`/150 here vs. `clcdc_progress_bar`'s own 100) - a generic, firmware-wide
  fixed-point scaler, not clcdc-specific.
- **`cad_delay_ticks`** (`cad.c`'s own name) is documented here since it's built almost
  entirely out of this file's tick-timer primitives: genuinely **not a busy-wait** - it
  bounds the requested delay, arms a tick baseline, then loops servicing
  `cobjectmgr_tick` and a cad.c-internal calibration-progress pump until the elapsed
  time reaches the target.
- **`cad_calibration_progress_pump`** (`cad.c`'s own `FUN_c0005a1c`) — RESOLVED and
  reconstructed HERE this pass, rather than in `cad.c`, following the same cross-file-
  attribution precedent already established for `cad_delay_ticks`: genuinely
  `cad.cpp`'s own compiled code (reads/writes CAD's 38-slot calibration state via
  `cad_calibration_pop_changed`/`cad_calibration_slot_is_raw`), documented here because
  its only two real callers (`cad_delay_ticks` here, and `master_dispatch_tick`'s own
  bit-`0x0004` handler in `wire_dispatch.c`) both live here or nearby. Drains CAD's
  changed-slot queue, building a 4-byte wire-format event record per non-"raw" slot
  and, only when a pedal is physically present, drawing live calibration-progress text
  via `clcdc.c`'s own text primitive.

**Still open:** the real identity of the fixed constants `DAT_` references stand in for
(no data-segment symbols resolved); whether the raw tick counter is a hardware register
or a software counter incremented by an ISR.

## `omap_l108_spi.c` — status: done

Anchor: `"../MCU/Component/OmapL108Spi.cpp"` has 2 xrefs, both hard-fault call sites
inside the one function this file amounts to.

- **`omap_spi_write`** — a single SPI transmit primitive: two busy-wait phases against
  the same status register testing two different bits, each independently
  retry-bounded, followed by a read-modify-write of the low 16 bits of a combined
  control/data register.
- **Confirmed shared bus finding**: this exact primitive has 8 real call sites across
  `cad.c` (3x `cad_calibration_sweep`, `cad_trigger_calibration`, `cad_init`) and
  `cpsoc.c` (`cpsoc_analog_poll_channel`, 2x `cpsoc_analog_poll_task`) - the analog
  chip, the PSoC scan chip, and cpsoc.c's own separate third-device analog-polling
  chain all share one physical SPI bus and one low-level transmit primitive.
- The two functions this section originally flagged as an unattributed "third SPI
  device" (`FUN_c0011624`/`FUN_c0011534`) were resolved as `cpsoc.c`'s own
  `cpsoc_analog_poll_task`/`cpsoc_analog_poll_channel` in the 2026-07-17
  SPI-device-closure pass; this file's own scope is now just `omap_spi_write` itself.

**Verification pass, 2026-07-18** (static-dump re-query): re-confirmed `omap_spi_write`'s
body byte-for-byte against a fresh decompile - no corrections needed. Re-confirmed the
8-caller list unchanged. Two `DAT_` constants newly resolved: the fault call's `file`
argument matches this file's own anchor string exactly; the retry-delay argument
resolves to an address well outside this image's own code/data range, plausibly a real
MMIO peripheral/timer register elsewhere in the SoC's address space.

**Still open:** real bit-level meaning of the two status bits (usage-inferred, not
datasheet-confirmed); `omap_spi_retry_delay`'s own internals; whether a corresponding
SPI read function exists elsewhere in the image.

## `omap_l137_usbdc.c` — status: done

Anchor: `"../MCU/Component/OmapL137Usbdc.cpp"` has 3 xrefs — two inside
`omap_usbdc_init_ep0`, one inside `omap_usbdc_poll_transfer`.

**Closure pass, 2026-07-18** re-queried every function/`DAT_` constant this file's own
"still open" list had flagged:
- **`omap_usbdc_init_ep0`'s own final field-setup block was WRONG in the prior draft**:
  every field in that block is written on `dev` (param_1), NOT on `regs` (param_2) as
  previously claimed - corrected this pass.
- Almost every `DAT_` constant in this file now has a real resolved value: retry bounds
  (`0xf423f`/999999, shared by both functions in this file), byte offsets, a shared
  status/flags byte used by BOTH functions here, both hard-fault line numbers/file
  pointers.
- **`omap_usbdc_object_init`** (`FUN_c0009574`, the higher-level USB object bring-up
  function) is now fully decompiled and reconstructed, including a genuinely new
  **cross-file finding**: its own sole caller is `FUN_c00074bc`, which `eva_board_main.c`
  names `eva_board_final_setup` and had listed as one of ITS OWN still-open items - this
  resolves that: `eva_board_main.c`'s post-init-table setup call IS the USB device
  controller bring-up.
- **`FUN_c000acc8` resolved to be more than a no-op-adding wrapper** as previously
  claimed: it silently forwards a real `len` argument that Ghidra's own per-function
  signature analysis failed to recognize as a parameter of `FUN_c000acc8` itself. Its two
  real call sites are now fully traced: `FUN_c000acec` (the shared USB-submit primitive,
  see `crypto_at88.c`) calls it as a pure readiness gate (`len=0`); `master_dispatch_tick`
  (`wire_dispatch.c`) calls it once per tick with a real, computed length - THE call
  that actually arms/advances the transfer-completion state machine.
- **`omap_usbdc_poll_transfer`** — the real bottom of a call chain traced across this
  entire project: a 3-state (idle → in-flight → complete) transfer-completion state
  machine gated on an 8001-byte size threshold (confirmed this pass to be a hardcoded
  literal in the instruction stream, not `DAT_`-sourced).

**Still open (narrowed considerably this pass):** a second, apparently independent set
of 5 register-block pointer derivations in `omap_usbdc_object_init` vs. the 3 inside
`omap_usbdc_init_ep0` itself (overlap not traced); two raw hardware-register constants
and two global-to-global copies in `omap_usbdc_init_ep0` (values now known, functional
meaning not decoded); `dev+0x24`/`dev+0x28`'s real per-tick chunking meaning; whether
the 8001-byte threshold is a real hardware DMA/FIFO limit or a firmware policy choice.

## `omap_l137_usbdc_ext.c` — status: done

New file, closes this project's own last remaining structural gap: `wire_dispatch.c`'s
two real USB-receive-path callers of `wire_dispatch_command` (`FUN_c0003e24`/
`FUN_c000a918`/`FUN_c000aae0`), plus the low-level endpoint-register/FIFO/CPPI-style
descriptor-table layer both are built on. Not part of `omap_l137_usbdc.c`'s own
confirmed `"../MCU/Component/OmapL137Usbdc.cpp"` anchor range, but confirmed the SAME
hardware/software layer by shared field offsets (`dev+0x401`, `dev+0x40e`) with that
file's own `omap_usbdc_init_ep0`/`omap_usbdc_poll_transfer`.

Originally written from a static Ghidra dump only ("no live Ghidra bridge access this
pass"). **RE-VERIFIED 2026-07-19 against the live binary** (`get_disassembly`,
`read_memory`, fresh `decompile_function` calls on all headline functions and their
downstream helpers) rather than trusting the static-dump resolution at face value - see
this file's own header and per-function comments for the full evidence trail. Summary:

- **`usbdc_core_isr`** (`FUN_c0003e24`, 1812 bytes) — the master USB0 core interrupt/poll
  handler: decodes a combined interrupt-status word, handles USB bus reset (full
  endpoint 1-3 re-init + CPPI descriptor table setup), the EP0 control-transfer state
  machine, and per-endpoint TX/RX-ready events, most of which fall through to
  `usbdc_endpoint_event_dispatch`. Sole caller (confirmed via `get_xrefs_to`) is a small
  wrapper at 0xc0009bd8 that looks more like a poll/dispatch wrapper than a raw IRQ
  vector - narrowed, not fully resolved (see that function's own note).
- **`usbdc_endpoint_event_dispatch`** (`FUN_c000aae0`) — switches on event code; case 5
  is `usbdc_ep_recv_bulk`, the real `wire_dispatch_command` call site.
- **`usbdc_ep_recv_bulk`** (`FUN_c000a918`) — drains the bulk-OUT FIFO (clamped to 512
  bytes) and calls `wire_dispatch_command(handle, buffer, len)` directly. THE resolution
  `wire_dispatch.c`'s own README section had been asking for.
- A **second, previously-undocumented `wire_dispatch_command` call site** inside
  `usbdc_core_isr` itself, on the EP0 SETUP-pending path.
- **Two real bugs caught and fixed** by this verification pass (not present in the
  live binary, only in the earlier static-dump-only draft): `usbdc_ep0_notify_tx_complete`
  selecting the wrong endpoint (3, not 0) with a stray `+4` in its CSR-test offset
  (caught via raw disassembly, not decompile text); and `usbdc_endpoint_event_dispatch`
  case `0xb` having `usbdc_ep0_notify_tx_complete`/`usbdc_ep0_notify_rx_complete`
  swapped between its two branches.
- **Cross-file handle sharing confirmed via `read_memory`** rather than inferred:
  `usbdc_default_dev_handle` == `usbdc_bulk_dev_handle`; `usbdc_wire_handle` ==
  `usbdc_setup_dispatch_handle` == the endpoint-notify handle used throughout case
  `0xb` - two shared global objects underlie this entire cluster, not several
  independently-named lookalikes.

**Still open:** `FUN_c0009bfc`'s own caller (no static xref found); individual
TXMAXP/RXMAXP/CSR register field meanings in the bus-reset branch beyond confirmed
TXCSR/RXCSR bit positions; the CPPI-style DMA descriptor table's exact register-level
semantics (no TRM cross-reference done). Full list in the file's own footer comment.

## `panelbus_dispatch.c` — status: done

New file this pass. Reconstructs `FUN_c0007220`, previously flagged in `cobjectmgr.c`'s
own "found but deliberately NOT included" section as "a large secondary protocol
dispatcher, possibly a debug console."

**SETTLED, not a debug console.** Real caller chain: `FUN_c0007220`
(`panelbus_cmd_dispatch`) ← `panelbus_rx_dispatch_loop` ← `panelbus_poll_channels` ←
`master_dispatch_tick` (status bit `0x1000`) - a completely different call path from
`wire_dispatch_command`'s own USB command entry point. Real hardware evidence settles
it decisively: `panelbus_i2c_read_bytes`'s register offsets (`+0x08` status busy/
receive-ready bits, `+0x14` count, `+0x18` RX data, `+0x1c` slave address, `+0x24` mode)
and `panelbus_i2c_base`'s two selectable base addresses (`0x01e28000`, `0x01c22000`) are
an exact match for the TI OMAP-L138/AM1808 on-chip **I2C1 and I2C0** peripheral base
addresses and ICSTR/ICMDR bit positions per the public TRM - a byte-framed command
interpreter fed by real MMIO hardware, polled once per firmware tick, with no ASCII
parsing, no line editing, and no print/echo path anywhere in the chain.

This is a **SEPARATE, real hardware I2C bus** from `i2c_by_gpio.c`'s own bit-banged
`I2cByGpio.cpp` cluster (GPIO toggling, no MMIO peripheral registers, shared by
`CryptoAt88.cpp`/`CDix4192.cpp`) - this file's functions sit outside that address range
entirely and talk to the genuine on-chip I2C0/I2C1 hardware block instead.

10 functions reconstructed:
- **`panelbus_i2c_base`** — selects I2C1 or I2C0 by a (dead, unused) selector; every
  call site inside this file's own family passes selector=0, i.e. this whole dispatcher
  only ever talks to I2C0.
- **`panelbus_i2c_read_bytes`** — blocking I2C read primitive, fully transcribed.
- **`panelbus_opcode_known`**/**`panelbus_rx_dispatch_loop`** — the RX-side opcode
  whitelist and drain loop; this loop is the ONE static caller of
  `panelbus_cmd_dispatch`.
- **`panelbus_tx_queue_pop`**/**`panelbus_tx_send_retry`**/**`panelbus_tx_drain_channel`**
  — the TX-side per-channel ring-buffer drain, feeding real submitted records through
  the shared 4-retry-then-hard-fault pattern.
- **`panelbus_poll_channels`** — the per-tick entry point called from
  `master_dispatch_tick`. Polls ports `0x78`/`0x7a`/`0x7b` for inbound frames and drains
  the TX ring on ports `0x78`/`0x79`/`0x7a` - a real, transcribed-as-found asymmetry
  (port `0x7b` has no TX drain, `0x79` has no RX poll).
- **`panelbus_cmd_dispatch`** — the main event: the full opcode table (`0x30`-`0x32`/
  `0x40`-`0x4f` submit-record family, `0x50` → `cad_trim_adjust`, `0x60`-`0x6f` →
  ctouchpanel-shaped percent scaling, `0x70`/`0x90` → flag posts into a shared RTOS
  event-flag group, `0x80`-`0x87` → a device-ID negotiation handshake with a real,
  disassembly-confirmed hard-fault-with-used-return-value anomaly unlike every other
  call to the same fault handler elsewhere in this project).

**CORRECTED (cross-file, jointly with `ctouchpanel.c`)**: `FUN_c0014488` is **NOT**
called from this file, despite this project's own earlier README note claiming it was.
Its real 2 callers are both inside `wire_dispatch_command` (`wire_dispatch.c`), plus one
call inside `ctouchpanel.c`'s own address range.

**Real cross-file finding**: `panelbus_tx_drain_channel` is the actual feeder for
`cad.c`'s own registered opcode handlers (`0x78`/`0x79`) via `cad_trigger_calibration` -
not called from `wire_dispatch_command` directly (see `cad.c`'s own updated section).

**RESOLVED 2026-07-19** (was: "Flagged, not corrected here"): `cpsoc.c`'s own "third SPI
device" submit primitive (`FUN_c00032f8`) is confirmed - not just shape-suspected - to be
this exact same I2C0 hardware. See the "SPI/I2C0 mis-attribution resolved" section near
the top of this README, and both files' own updated headers, for the full evidence trail
(shared literal handle-selector function `FUN_c0001a00`/`panelbus_i2c_base`, the same
`0x01c22000` I2C0 base constant, and `FUN_c00033f0` turning out to be the literal same
function as this file's own `panelbus_i2c_read_bytes`, not just a lookalike). `cpsoc.c`
has been corrected: `cpsoc_spi_submit_write`/`_read` renamed to
`cpsoc_i2c0_submit_write`/`_read`.

**Still open**: the TX ring's own push/producer side (not found as its own function);
`FUN_c0012de0` (`panelbus_submit_record`'s own target primitive); several real
cross-file signature mismatches for shared symbols this file's own header flags
explicitly (see this project's 2026-07-18 pass summary above) rather than silently
reconciling.

## `wire_dispatch.c` — status: done

New file this pass. Reconstructs the panel firmware's two central dispatch loops,
previously only known via their call sites scattered across other files:

- **`wire_dispatch_command`** (`FUN_c0007d1c`, 1904 bytes) — the single entry point for
  every incoming 32-bit command word from the host, decoding an opcode byte and routing
  to essentially every subsystem this project has reconstructed. The panel-side
  counterpart of the host's own `COmapNKS4Command` wire protocol.
- **`master_dispatch_tick`** (`FUN_c0008b64`, 1240 bytes) — the status-bit dispatcher
  called unconditionally, forever, from `eva_board_main.c`'s own main loop.

Neither has its own `__FILE__` anchor (a full image string search near both addresses
found nothing usable) - identified instead by size, role, and cross-file address
confirmation, per the same no-anchor discipline `cpsoc.c`'s third-device section and
`heap_alloc.c` already established.

- **`wire_dispatch_command`'s real callers resolved**: NOT called directly from
  `eva_board_main`'s main loop with USB data, as `KRONOS_V06R06.VSB.md` previously left
  open. Reached via the USB receive path (a large ISR/poll state machine plus a smaller
  opcode-switch shim), both living in `omap_l137_usbdc.c`'s own address neighborhood
  (not reconstructed here, out of this file's own scope). `master_dispatch_tick` is the
  OTHER, unconditional-tick caller context - the two are siblings, not caller/callee of
  each other.
- **Full opcode table transcribed** and cross-checked against
  `KRONOS_V06R06.VSB.md`'s own independently-derived table (which was itself derived
  from this same function) - every entry confirmed, plus 3 cross-file handle-sharing
  facts confirmed by direct address comparison: the cpsoc register-bank context, the cad
  pedal context, and the cad calibration context this dispatcher reads are each the
  EXACT SAME global `eva_board_final_setup` itself seeds at boot.
- The dense `0xe0` (AT88 relay write) payload-reassembly loop is preserved literally
  rather than simplified, specifically because `crypto_at88.c`'s own re-verification
  pass already found real byte-order bugs in adjacent AT88 relay code - deliberately
  NOT "cleaned up" past what the raw decompile shows.
- **`master_dispatch_tick`'s single most important finding**: its very last call each
  tick drives `omap_l137_usbdc.c`'s own transfer-completion state machine directly (via
  that file's own confirmed thin wrapper), with a length computed from the same handle
  fields the `0x20`-bit and `0x8000`-bit status handlers touch - this dispatcher doesn't
  just trigger other subsystems' USB sends indirectly, it directly pumps a large
  transfer through in ~2000-byte chunks, one attempt per tick, until the USB state
  machine reports completion.

**Still open**: the `0xe0` payload-reassembly loop's exact byte-order past its fixed
header (deliberately not transcribed, see above); most of the generic `eva_wire_`/
`eva_tick_` externs (bare `FUN_`/`DAT_` addresses, no cross-file attribution found this
pass); `master_dispatch_tick`'s own ~80-line USB-adjacent state-machine cluster
(plausibly `omap_l137_usbdc.c`'s own endpoint configure/halt bookkeeping, out of this
file's own scope).

**RESOLVED 2026-07-19** (was: "`FUN_c0003e24`/`FUN_c000a918`/`FUN_c000aae0`
(`wire_dispatch_command`'s two real callers, confirmed as the USB receive path but not
reconstructed here)" - this project's last remaining structural gap in the firmware-side
USB-receive-to-dispatch chain): all three are now fully reconstructed, in
`omap_l137_usbdc_ext.c` (not this file - they sit in `omap_l137_usbdc.c`'s own address
neighborhood/scope). Live Ghidra bridge access was available this pass (the prior pass
that wrote `omap_l137_usbdc_ext.c` had static-dump access only) and was used to
independently re-decompile and disassemble all three functions plus their downstream
helpers rather than trust the earlier static-dump resolution at face value. Findings:

- **`FUN_c0003e24` = `usbdc_core_isr`** - the master USB0 core interrupt/poll handler.
  Decodes a combined interrupt-status word and dispatches per-bit to: a USB bus-reset
  handler (full endpoint 1-3 TXMAXP/RXMAXP/CSR re-init plus CPPI-style DMA descriptor
  table setup), an EP0 control-transfer state machine (which is where wire_dispatch.c's
  own `master_dispatch_tick`/EP0-class-request handlers ultimately connect), and one
  branch per bulk/iso endpoint TX/RX-ready event - each of the latter falls through to
  `usbdc_endpoint_event_dispatch` with a numeric event code. Confirmed sole caller via
  `get_xrefs_to`: one `bl` at 0xc0009bfc, inside a small one-parameter wrapper
  (0xc0009bd8) that looks more like a poll/dispatch wrapper than a raw hardware IRQ
  vector (narrowed, not fully resolved - its own caller has no static xref, plausibly
  reached through an indirect function-pointer table).
- **`FUN_c000aae0` = `usbdc_endpoint_event_dispatch`** - switches on the event code
  `usbdc_core_isr` computes, routing to per-endpoint handlers; case 5 is
  `usbdc_ep_recv_bulk`, the actual `wire_dispatch_command` call site.
- **`FUN_c000a918` = `usbdc_ep_recv_bulk`** - THE resolution: reads the pending bulk-OUT
  byte count, clamps to 512 (USB full-speed bulk max-packet-size), drains the FIFO, and
  calls `wire_dispatch_command(handle, buffer, len)` directly - no queue, no deferred
  processing. Fires once per "endpoint 5 RX ready" USB core interrupt.
- **A second, previously-undocumented `wire_dispatch_command` call site** was found
  inside `usbdc_core_isr` itself, on the EP0 SETUP-pending path (distinct from
  `usbdc_ep_recv_bulk`'s bulk-OUT path) - both operands (handle, buffer) confirmed via
  live `read_memory` to be real, non-null, pre-initialized globals baked into the
  firmware image, not runtime-only values needing hardware capture.
- **Two real bugs were found and fixed** in the earlier static-dump-only draft, both
  caught by cross-checking live decompile/disassembly rather than trusting the earlier
  resolved-value claims: (1) `usbdc_ep0_notify_tx_complete` (`FUN_c00048f8`) was
  documented as selecting EP0 and testing TXCSR at `dev+0x412`, but disassembly
  (`mov r2,#0x3` / `strb r2,[r4,r3]`) shows it actually selects endpoint 3, and the
  CSR-test offset arithmetic had an extra stray `+4`; (2) `usbdc_endpoint_event_dispatch`
  case `0xb` had `usbdc_ep0_notify_tx_complete`/`usbdc_ep0_notify_rx_complete` swapped
  between its two condition branches, with the rx-complete call's own argument order
  wrong as a downstream consequence. Full before/after evidence for both is in
  `omap_l137_usbdc_ext.c`'s own per-function comments.
- **Cross-file handle sharing confirmed via `read_memory`** (not merely inferred from
  naming): `usbdc_default_dev_handle` (`DAT_c000acb8`) and `usbdc_bulk_dev_handle`
  (`DAT_c000a970`) are the literal same global (`0xc01cce50`); `usbdc_wire_handle`
  (`DAT_c000a97c`), `usbdc_setup_dispatch_handle` (`DAT_c0004594`), and the endpoint-
  notify handle used throughout case `0xb` (`DAT_c000acc0`) are all the literal same
  global too (`0xc01cac00`) - i.e. this entire USB-receive cluster, both
  `wire_dispatch_command` call sites, and the endpoint-notify plumbing all ultimately
  operate on two shared global objects, not several independently-named lookalikes.

**Still genuinely open** (not resolved this pass): `FUN_c0009bfc`'s own caller (no static
xref found - plausibly an indirect function-pointer table); individual TXMAXP/RXMAXP/CSR
register field meanings in `usbdc_core_isr`'s bus-reset branch beyond what TXCSR/RXCSR
bit positions already confirm; `usbdc_desc_table_base`'s exact relationship to the CPPI
DMA engine (no TRM cross-reference done). See `omap_l137_usbdc_ext.c`'s own header and
footer for the full list.
