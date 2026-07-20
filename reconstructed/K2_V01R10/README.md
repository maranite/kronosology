# KRONOS2S_V01R10.VSB - reconstructed source

Reverse-engineered source for `KRONOS2S_V01R10.VSB`, the Kronos 2 front-panel
firmware - the direct sibling of [`reconstructed/K1_V06R06/`](../K1_V06R06/)
(Kronos 1's `KRONOS_V06R06.VSB`). Same physical role (TI OMAP-L1x,
ARM926EJ-S, USB `0944:1005` front-panel board), same container format, same
load address (`0xC0000000`) - see
[`kronosology/docs/modules/KRONOS2S_V01R10.VSB.md`](../../docs/modules/KRONOS2S_V01R10.VSB.md).

The image is a single `PT_LOAD` wrapped-ELF (`kronos2s_v01r10_panel.elf`),
file offset `0x54` mapping to vaddr `0xC0000000`.

## Architectural relationship to K1

String-anchor comparison (`__FILE__` literals embedded in each image) shows
K2 has a partly redesigned architecture, not just a firmware version bump.

Four K1 files are entirely absent from K2's string table:

- `cpsoc.cpp` - PSoC button/LED scan chip driver
- `cad.cpp` - A/D converter for knobs/pedals
- `ctouchpanel.cpp` - touch panel
- `McAspHandler.cpp`

They are replaced by genuinely new files with no K1 equivalent:
`PanelManager.cpp`, `PanelScanUpdater.cpp`, `SwitchOnChatteringDetector.cpp`,
`SystemInfoHolder.cpp`, plus a new low-level driver
`MCU/Component/OmapL108Syscfg.cpp` that fills `OmapL108Spi.cpp`'s old slot
in the link order.

Every other K1 anchor (`CryptoAt88.cpp`, `I2cByGpio.cpp`, `MCU/OmapL108.cpp`,
`MCU/Component/OmapL137Mcasp.cpp`, `MCU/Component/OmapL137Usbdc.cpp`,
`EvaBoardMain.cpp`, `cobjectmgr.cpp`, `CDix4192.cpp`, `clcdc.cpp`) is still
present in K2's string table - the shared low-level driver layer is
architecturally unchanged and has been migrated into this tree.

## Repository layout

33 reconstructed `.c` files, one-to-one against K1's tree except where
noted below:

```
aintc.c                    cdix4192.c                cdix_autoswitch.c
chan_link_hw.c              chan_param_ctrl.c          chan_slot_dispatch.c
clcdc.c                     cobjectmgr.c               crypto_at88.c
eva_board_main.c            heap_alloc.c               i2c_by_gpio.c
mcasp.c                     midi_chan_status_queues.c  midi_engine.c
omap_gpio.c                 omap_l108.c                omap_l108_syscfg.c
omap_l137_addr_gap_misc.c   omap_l137_usbdc.c          omap_l137_usbdc_ep0.c
omap_l137_usbdc_ext.c       panel_manager.c            panel_scan_updater.c
panelbus_dispatch.c         soc_irq_gate.c             soc_periph.c
switch_chattering_detector.c  system_info_holder.c     task_sched.c
uart1_midi_queue.c           usbdc_midi_status_glue.c  wire_dispatch.c
```

`clcdc_test_dispatch.c` (present in K1) has no K2 counterpart - see
"Files absent in K2" below.

## Panel-scan subsystem (new architecture, replaces cpsoc.cpp/cad.cpp)

### `panel_manager.c`

Consolidates what K1 split across `cpsoc.cpp` (switch/LED names +
diagnostic screen) and `cad.cpp` (knob/slider/pedal names) into one file - a
genuine architectural merge: the 77-entry switch/LED name pointer table
(`0xc0005784-0xc00058b4`, directly comparable to K1 `cpsoc.c`'s own 73-entry
table; the 77th entry is blank padding) and a separate 24-entry
knob/slider/pedal name string group both sit in this file's own `.rodata`
neighborhood.

Functions:

- `panel_manager_set_led_bit()` - single-bit set/clear into a packed 16-bit
  register, addressed via a 73-entry `(word_slot, bit_pos)` lookup table at
  `0xc0027460` (transcribed byte-exact).
- `panel_manager_apply_led_group()` - 16-bit-mask bulk-apply wrapper around
  the former. Callers: a diagnostic LED-list scroller (`0xc00093a4`) and a
  large incoming wire-protocol opcode dispatcher (`0xc0009b54`).
- `panel_manager_encode_scan_event()` (`0xc00061d4`) - switch/knob debounced
  scan-read encoder.
- `panel_manager_dispatch_scan_byte()` (`0xc0006700`, 312 instructions) - a
  stateful message dispatcher; the real caller of
  `switch_chattering_register()`/`switch_chattering_remove()` (see below).

### `panel_scan_updater.c`

Direct architectural descendant of a specific K1 feature (PSoC
field-firmware-update, mixed into `cpsoc.cpp` in K1), now split into its own
dedicated file, per its own strings: `"Update panel scan
system."` / `"->Now writing..."` / `"->->Completed. Tern power off."` (a
literal firmware typo, "Tern" for "Turn", transcribed faithfully, the same
convention as K1's own "Tepo"/"Tempo" typo).

`panel_scan_updater_run()` (`0xc0006ee8`) is a 5-parameter top-level
sequencer: prints status text, runs erase/write/verify/apply sub-steps
(each a dense SPI/TWI command-response handshake, fully transcribed), checks
a version/revision byte pair against caller-supplied expected values
(`expect_ver=0xb3`, `expect_rev=0x39`), faults (`panel_fault`, line `0x53`)
on mismatch, and prints completion text as a tail call. Its real caller is
`0xc000685c`. `panel_scan_updater_apply()`'s real signature is
`(const uint8_t *hexstream, int len)` - meaning `run()`'s `b1`/`b2`
parameters are a hex-encoded firmware stream pointer/length pair.

K1's own `"Psoc version error %02x != %02x : Id %03d"` string is not present
anywhere in K2's string table. Unconfirmed whether it was renamed, removed,
or lives outside the address ranges swept so far - to validate, search the
full K2 image (not just the string table window already checked) for a
byte-shifted or partially-changed variant.

Some `panel_fault()` call sites across the four sub-steps have their
line-number arguments elided by the decompiler and were not recoverable;
marked explicitly in the file rather than guessed.

### `switch_chattering_detector.c`

The strongest "genuinely new, not renamed" evidence among these five files:
K1 never had a standalone debounce module. Two functions:
`switch_chattering_register()` (`0xc00070c0`) and
`switch_chattering_remove()` (`0xc0007158`), implementing a real 3-state
(0=idle, 1=actively-debouncing, 2=confirmed) per-switch state machine with a
doubly-linked list for active entries, each asserting via `panel_fault()`
citing this file's own anchor (lines `0x66`/`0x84`). Sole caller of both:
`panel_manager_dispatch_scan_byte()`.

A real, deliberately unreconciled discrepancy: `register()` indexes its
list-node array by a running pool counter (`*(base+4)`); `remove()` indexes
what looks like the same array directly by switch index. This is recorded
rather than resolved one way or the other; a direct decompile of both
functions together would likely settle which side is authoritative.

### `system_info_holder.c`

One function, `system_info_holder_print_status()` (`0xc00097ac`), prints
`"SyncErrorCount %d"` and `"Panel Scanner Version:%02d Revision:%02d"`
(itself a rename of K1's `"Panel Scan System Version:%02d Revision:%02d"`)
to screen, as two genuinely separate steps (format into buffer, then draw
buffer), not one combined 4-argument call.

Caveat: this function's home in `.rodata` sits positionally just after
`cobjectmgr.cpp`'s own anchor and just before `SystemInfoHolder.cpp`'s - by
strict position it would nominally belong to `cobjectmgr.cpp`'s own string
pool. It is placed in this file on functional grounds (a version/health
report fits "SystemInfoHolder" far better than a generic object manager),
not on positional evidence alone.

`0xc000a7c8` (this file's own anchor xref) is a bounds-checked 8-entry table
accessor. `0xc0009b0c` is the "SYSTEM STARTUP FAILED" fault-screen text
cluster, directly paralleling K1's own hard-fault screen.

### `omap_l108_syscfg.c`

Fills the exact link-order slot K1's `OmapL108Spi.cpp` (the shared
cpsoc/cad SPI driver) used to occupy - consistent with the SPI bus's only
two K1 consumers (`cpsoc.cpp`, `cad.cpp`) both being gone in K2. Its anchor
string has a direct real xref at `0xc0001bc8` (an actual
`panel_fault()` call site citing this exact filename and line `0x51`).

Six functions, all byte-verified against live memory contents (no Ghidra
Function objects exist for most of them):

- `omap_syscfg_set_reg154`, `omap_syscfg_set_reg130_a`,
  `omap_syscfg_set_reg130_b`, `omap_syscfg_set_dual_pull_enable` - fixed
  register-write leaves, TI PINMUX-style nibble-per-field encodings.
- `omap_syscfg_reset_and_enable` - a reset/hold/deassert/configure/
  poll-with-timeout peripheral bring-up sequence (999999-iteration timeout),
  faults via this file's own anchor on timeout. Real caller found.
- `omap_syscfg_clear_pull_enable_0xc` - a 7th leaf, immediately following
  the rest of the cluster.

A function originally attributed here as `omap_syscfg_set_reg118` was
removed: it turned out to be a misattribution of `soc_periph.c`'s own
`board_desc_set_pinmux_3word()` tail (same address range, same
`0x54704404` constant at `+0x118`), not an independent function.
`board_desc_set_pinmux_3word()` writes three consecutive words
(`+0x110`/`+0x114`/`+0x118`) in one call - different from K1's three
separate one-word pinmux leaves (`+0x130`/`+0x130`/`+0x154`).

Four of this file's six functions have no caller anywhere in the image
despite being real code. A full-image search for each orphan's raw
address as a literal (the shape a function-pointer dispatch table would
need) came back with zero hits for all four, ruling out that specific
mechanism. The real invocation path - if any - is unresolved; it may be
reached through a computed indirect branch this project's static analysis
cannot trace, or it may genuinely be dead code. The real peripheral base
address/register map has also not been cross-checked against a TI
OMAP-L108/L138 TRM.

## Shared driver / infrastructure layer

The generic-infra and low-level driver files below are either near-identical
ports of K1's own files (re-addressed, re-verified against K2's own
decompiled text and literal pool) or, where noted, structurally different.

### `soc_periph.c`

The peripheral base-address table ports over almost unchanged: 11 of K1's 13
entries carry over with identical or TRM-consistent values (Timer64P0,
SYSCFG0, PSC1, EDMA3CC0/1, EDMA3TC0/1/2, McASP0 base+FIFO, LCDC, eCAP1,
USB0/OTG, AEMIF CS3), plus the `gpio_bank_get_base`/`hw_timer_busy_wait`
pair `i2c_by_gpio.c` already needed as externs - both are now defined here.

Differences from K1:

- **Two brand-new table entries**: an I2C0/I2C1 base selector
  (`i2c0_i2c1_base_select`) and an eCAP2 base getter. The I2C0/I2C1
  selector's real caller is `panelbus_hw_bringup` (see
  `panelbus_dispatch.c` below) - exactly one caller in the whole binary.
- **SPI survives** - a notable finding given `cpsoc.cpp`/`cad.cpp` (SPI's
  only two K1 consumers) are both gone in K2: the SPI0 base accessor is not
  dead code. Its real callers are `FUN_c0004f70` and
  `panel_scan_updater_run()` - SPI's consumer changed, not its presence.
  K1's own SPI1 selector arm has no K2 counterpart (only a single-value SPI0
  getter exists).
- **UART shrinks** - K1's 3-way `uart_base_select` has no K2 counterpart;
  K2 has only a single, no-parameter UART1-only accessor. Whether K2 dropped
  the other two instances' bring-up entirely or moved it elsewhere is
  unconfirmed.
- **Pinmux writes merged**: K1's three separate one-word pinmux leaves
  become one K2 function writing three consecutive words at different
  offsets (see `omap_l108_syscfg.c` above), not just a mechanical merge.
- **Two `psc_module_enable`-shaped functions** are present, matching K1's
  own architecture exactly (a zero-caller one, plus an extern-only sibling,
  `gpio_psc_enable`, now defined for the first time in `omap_gpio.c`).
- `timer64p0_enable_ch15` (`0xc0000040`) - lazy-init guard,
  `timer64p0_base_get()` cache, `board_desc_init_type5()` call, AINTC
  channel `0x15` EISR enable.

`board_desc_init_type4` (K1's caller-length descriptor-constructor sibling)
and the Timer64P1 lazy-init singleton have no located K2 counterpart.

### `omap_gpio.c`

K1's generic pair-indexed GPIO register layer
(`gpio_reg_read_in`/`_set_bit`/`_clear_bit`, `bank_base + pair*0x28 +
offset`) is gone in K2. A full-image search of every K2 function's
decompiled text for that `* 0x28` stride pattern turns up one unrelated
false positive and zero real hits. Every GPIO-touching K2 leaf instead
hardcodes one fixed bit at one fixed byte offset directly, with no
pair-index indirection - a real simplification, not a coverage gap.

`gpio_bank_set_dir_bit`/`gpio_bank_read_sda_bit` (both already declared
`extern` by `i2c_by_gpio.c`) still exist in role, but read/write their fixed
SDA bit directly rather than through a pair-indexed primitive.
`omap_psc_enable_module_0x10` and its callee `gpio_psc_enable` (K1's own
never-defined `extern`) port over with identical mechanism.
`gpio_bank_hw_init`'s K2 counterpart is a genuine subset of K1's own: it
writes BINTEN and one bit-5 edge-trigger pair (numerically identical to K1)
but omits all of K1's DIR/OUT_DATA pair-1/3/4 default writes.

All 30 real callers of `gpio_bank_get_base` were individually examined; none
writes to the pair-1/3/4 absolute DIR/OUT_DATA offsets K1 used (`+0x38`,
`+0x88`/`+0x8c`, `+0xb0`/`+0xb4`). A supplementary full-image search for
K1's three DIR default constants as raw literal-pool bytes found zero hits
for all three. This is reasonable-confidence evidence, not proof: an ARM
MVN-immediate encoding could hide an equivalent constant from a plain byte
search, so whether those defaults exist somewhere in K2 under a different
encoding remains open. Several further thin bit-twiddle leaves (matched-pair
field selectors at `+0x90`/`+0x94` and `+0xb8`/`+0xbc`) have no K1
counterpart at all.

### `heap_alloc.c`

The dlmalloc-derivative shape carries over essentially unchanged.
`heap_sbrk`/`heap_trim` are statement-for-statement identical to K1 (same
rounding, same ENOMEM path, same sbrk-of-negative-amount fallback).
`heap_lock`/`heap_unlock` are empty bodies, same as K1. `heap_malloc`'s
opening rounding/small-bin-fast-path/treebin-ladder logic (the only part
checked) is identical down to the same shift constants (9/6/12/15/18) and
additive offsets (0x38/0x5b/0x6e/0x77/0x7c/0x7e) K1's own file catalogues.
`heap_free` is `heap_trim`'s sole call site, matching K1.

`heap_state+8`'s "is this the top chunk" role is the top-chunk pointer's own
address, not a separate sentinel object - a standard dlmalloc idiom.
`heap_trim`'s real trigger threshold (`0xC00A0E78`) and pad argument
(`0xC01CE1EC`) addresses are established. K2's `heap_errno` global is also
independently zeroed by three unrelated K2 functions elsewhere in the
image, outside this file's own scope.

Open: `heap_malloc`'s treebin/designated-victim slot layout beyond the
opening logic was not established. Whether K2's heap base/end constants
imply a differently-sized heap region than K1's is unconfirmed - K1's own
file never records its own constants numerically either, so this needs a
direct read of both images' heap base/end symbols to settle.

### `task_sched.c`

K2 keeps the same task-scheduler architecture as K1 - TCB table + priority
ready-queue/bitmap dispatcher, the kernel-object (event-flag-group) table
and its set/clear/wait API, and the tick-ordered delay/timeout min-heap -
all present at new addresses, with (where checked field-by-field) identical
branch shapes, error codes (`-25`/`-18`/`-17`/`-28`), and struct-offset
conventions to K1.

**`eva_board_main` does not exist as a single K2 function.** K2's ROM
autostart table (count at `0xC002A6F8`, task-id array at `0xC002A68C`,
cfg-record array at `0xC002A698`, `0x20`-byte stride) has exactly 3 tasks:

- task id=1, priority 0 (most urgent): runs `eva_board_init_table`'s own
  walk-and-dispatch loop, then `eva_board_final_setup`, then loops forever
  calling `eva_board_boot_status_dispatch` (a genuine 2-instruction infinite
  loop, per hand disassembly).
- task id=2, priority 2: a separate stub calling `eva_board_main_loop` (the
  real `master_dispatch_tick` forever-loop).
- task id=3, priority 4 (lowest, structurally unreachable in normal
  operation since task id=2's own main loop never blocks): an immediate,
  unconditional `crypto_at88_fault` call citing `"../EvaBoardMain.cpp"` line
  `0x70` - this is the K2 counterpart of
  `eva_board_watchdog_fault_wrapper`.

This is architecturally different from K1's own `eva_board_main`
reconstruction, which currently models these same call sites as one linear
function body (built from call-site addresses rather than one continuous
disassembly). The distinction has not been folded back into
`eva_board_main.c` itself and remains a candidate for a future
consolidation pass.

Which of a task's two ROM-pushed stack words is the real jump target is
settled as `cfg+8`, not `cfg+4`: every observed `cfg+8` value is a code
address, every `cfg+4` value is a small integer. A transcription bug in
`sched_task_create_and_ready` (an erroneous extra `cfg +` pointer addition,
present in neither the real decompile nor K1's own version) was found and
fixed.

The delay-heap sift-up/sift-down real addresses are known
(`FUN_c001aae8`/`FUN_c001a7d0`) but not transcribed to C. A previously
uncharacterized function near `0xC001AA98` removes the current task from
ready then resets its TCB - it accounts for call-site `0xC001AAB4`, which
K1's own file cited but never traced, though its shape matches neither
`eva_board_sched_ready` nor `eva_board_sched_requeue` as K1 describes them;
both remain unlocated in K2.

A real behavioral difference from K1: K2's `sched_wait_list_insert`
unconditionally calls `sched_remove_from_ready` on the blocking task before
inserting it into the wait list; K1's version does not (only its separate
TIMED variant did). Whether K2 merged the timed/untimed paths, or this K2
function is actually the timed variant under an unexpected name, is
unresolved.

A second difference: the general-purpose `sched_dispatch` (called
from `kobj_eventflag_wait`) differs from `eva_board_crt0`'s own inlined copy
of the same ready-scan/WFI tail - the general-purpose version has an extra
"save the outgoing task's context" header the crt0-inlined copy omits (crt0
has no outgoing task on cold boot). K1's own file describes these two tails
as byte-for-byte identical without drawing this distinction; whether that
description also needs correcting on the K1 side is an open question for
that file.

### `soc_irq_gate.c`

Located by a full-image sweep for every K2 function calling `aintc_base()`
directly: 11 genuine AINTC channel-gate leaves, plus both "group"
enable/disable dispatchers (`soc_irq_gate_group_a_enable`/
`_group_b_disable`), all cluster-matched 1:1 against K1's file. The shared
bookkeeping table these leaves reference resolves to the same fixed
address as K1's own table, `0xC00E0000` - independently re-derived from
K2's own literal-pool arithmetic, strong evidence it is a fixed
OMAP-L138/DA850 physical SRAM location rather than firmware-relative data
(K2's own table is smaller, `0xC00E0000-0xC00E004C` vs K1's `-0x0068`,
because K2 genuinely dropped slots rather than shrinking offsets).

Differences from K1:

- Timer64P1 (channel `0x17`) and channel `0x36` IRQ gating are gone
  entirely - a full-image literal sweep for either channel number written
  to EISR/EICR found zero matches.
- Channel `0x2a` now has a real enable side, inlined in the group-A
  dispatcher's own tail rather than as a standalone leaf (K1 had none).
- Channel `0x32`'s disable path re-arms its own enable-side guard on every
  call (clears the same cached-pointer slot the enable side uses as its
  lazy-init guard) - K1's version never touched that guard on the disable
  side.
- Channel `0x2a`'s disable-side GPIO acknowledgment write is inlined rather
  than routed through a dedicated `omap_gpio.c` helper the way K1's
  `gpio_pair0_intstat_ack_bit5` was.
- `soc_irq_gate_mcasp2_bringup`'s body is a real subset of K1's own
  11-callee sequence (a different second base-getter call, one final GPIO
  write instead of three) - this also confirms `FUN_c00019e0`/
  `FUN_c00019e8` are McASP-instance base getters (now `mcasp0_base_get`/
  `mcasp0_fifo_base_get` in `soc_periph.c`) and that `mcasp_reinit_reduced`
  is this function's sole caller.
- `midi_hw_write16`/`midi_hw_read16`'s own caller counts (130/53) are
  essentially identical to K1's own counts (129/53) - this pair is genuine
  shared, firmware-wide MMIO plumbing, not MIDI-exclusive.

`timer64p0_enable_ch15` (`0xc0000040`) is defined in `soc_periph.c` (see
above). A self-correction recorded in this file:
`soc_irq_gate_timer0_quiesce`'s cached-handle literal (`DAT_c0000124`)
reads `0xC00E0000` (table+0x00), not `0xC00E0008` (table+0x08) as an
earlier pass claimed - direct byte evidence. That slot is distinct from
CLUSTER 2's own `mcasp_param_b_cache` (`DAT_c00001a8`, genuinely
`0xC00E0008`) - the two were never the same slot.

A previously undocumented 72-byte code region (`0xc0000098-0xc00000df`)
opens with the same "read table+0x00, OR bit `0x2` into `+0x44`" idiom as
`soc_irq_gate_timer0_quiesce`'s own opening, writes AINTC channel `0x15`'s
SICR via a hardcoded literal address instead of a live `aintc_base()` call,
calls a tiny report-code gate function, then tail-branches into a large,
separately-prologued, IRQ-disable-guarded routine. Its only reference in
the whole image is a parameter-type (not call-type) read from
`task_sched.c`'s own ROM-table-walk subsystem-bring-up code, suggesting
this region is itself a `task_sched.c` ROM-autostart-table entry - flagged
here, not claimed or defined in this file.

Open: `usbdc_gap_config_slot`'s real K2 address remains unlocated. A
candidate address (`0xc0002d80`) is instead
`panelbus_dispatch.c`'s own `panelbus_i2c_mode_config` - an address
misattribution bug carried in `omap_l137_addr_gap_misc.c`, not yet fixed
there. `soc_irq_gate_slot0x00_get`/`_ring3_state_reset` (K1's own
cluster-11 tail items) also remain unlocated; a full-image byte sweep for
the `table+0x00` literal finds only the 4 already-known real consumers,
none shaped like a bare getter wrapper.

### `wire_dispatch.c`

The distinguishing 2nd argument between opcode register `0x50` and `0x52`
(in `eva_wire_pedal_send` and `wire_dispatch_command`) is a real 3rd
parameter (`1`="set"/`0`="clear" into a `(word_slot, bit_pos)`-table bit,
the same table-lookup idiom `panel_manager_set_led_bit` uses) - both call
sites now pass it explicitly.

`FUN_c0008d24` (the opcode `0xc6` continuation resolver) is a byte-per-entry,
256-entry LUT (at `0xC001B814`) expanding an 8-bit-indexed byte stream into
16-bit values written into a `0x321`-halfword circular ring buffer -
not the same ring/cursor object as the opcode `0xc4` continuation
(`FUN_c0009158`, which targets an 800-wide framebuffer with a
palette-indexed pixel write). Not transcribed statement-for-statement
(dense, roughly 140 real statements with heavily duplicated wraparound-check
inlining across two near-identical loop shapes).

### `panelbus_dispatch.c`

K1's own per-tick I2C0/I2C1 opcode dispatcher (RX poll loop, TX ring drain,
opcode-table command interpreter, roughly 7 functions) was built entirely
on top of `cad.cpp` (calibration) and `cpsoc.cpp`'s "third SPI device" -
both absent from K2's string table. The underlying I2C0/I2C1 base
selector survives structurally unchanged (`i2c0_i2c1_base_select`, defined
in `soc_periph.c`), but has exactly one caller in the whole binary:
`panelbus_hw_bringup`, a single crt0 hardware bring-up stub. There is no
RX/TX/opcode dispatcher cluster anywhere in K2's covered code.

`panelbus_hw_bringup` selects I2C1 (`idx=1`) - the opposite selector value
from every one of K1's own call sites, which exclusively used I2C0
(`idx=0`). Whether this reflects a genuinely different physical bus
assignment on the K2 board, or an unrelated hardware consumer of this
shared selector, is unconfirmed; resolving it would need board-level
schematics or a live I2C bus trace. `panelbus_hw_bringup`'s full body
(`0xc00008d4-0xc0000904`) repeats the same I2C1 setup twice via a tail
branch, not a symmetric I2C0+I2C1 setup as an earlier characterization
suggested.

`panelbus_hw_bringup_unreached` (`0xc00008cc`, 52 bytes) is a structural
sibling of three other single-caller functions in the same cluster, but has
zero callers anywhere in the image - an exhaustive dead end.

K2's real panel-scan traffic most likely uses SPI instead (per
`soc_periph.c`'s "SPI survives" finding, above), not this I2C hardware.
Whether any further, unswept consumer of the I2C0/I2C1 hardware exists is
unconfirmed.

### `cdix_autoswitch.c`

Ports cleanly: all 3 functions are exact byte-size matches against K1
(20/20, 76/76, 120/120 bytes), no logic differences. K2's own outer CDIX
auto-switch state machine also toggles `omap_l137_addr_gap_misc.c`'s own
McASP2 bit-flag helpers around the CDIX reset call - a real hardware
coupling not present in K1's documentation. A shared `0xC00E004C`
context-handle constant is used by this cluster, `mcasp_init` (`mcasp.c`),
and `clcdc_display_object_init` (`omap_l137_addr_gap_misc.c`) - inside the
same fixed SRAM page `soc_irq_gate.c`'s table occupies.

### `omap_l137_addr_gap_misc.c`

Five of K1's six clusters port over:

1. McASP2 reduced-reinit - exact size match, per `mcasp.c`'s own citations
   (`mcasp_reinit_reduced`/`mcasp_clock_step_a`).
2. `usbdc_gap_config_slot` - structurally identical, real address still
   unattributed (the candidate address `0xc0002d80` is a misattribution -
   see `soc_irq_gate.c` above).
3. The UART-shaped register pair - absent. A full-image raw byte
   search for all four of K1's distinctive literal immediates found zero
   occurrences anywhere in K2's binary, code or data.
4. Two tiny bit-extraction helpers - port over, plus one real K2-only
   addition, `gap_store_low_byte`.
5. Default RGB565 palette loader - ports over. The `0x752ff`-iteration
   remap-loop bound resolves to the identical value in K2's independently
   compiled image, evidence it is a genuine compiled constant rather than a
   per-build artifact.
6. Struct-zero-init - exact size match, per `mcasp.c`'s citations.

Two previously "uncontained call site" candidates are now resolved:

- Cluster 2's second caller (`0xc00008fc`) is a real, 52-byte function
  (`0xc00008cc`), structurally a sibling of three other functions
  (`FUN_c0000800`/`FUN_c0000864`/`FUN_c000094c`) that each have exactly one
  real caller - but this one has zero callers or raw-byte literal hits
  anywhere in the image, an exhaustive dead end. Named
  `panelbus_hw_bringup_unreached` in `panelbus_dispatch.c`.
- Cluster 4's sole caller (`0xc00005b8`) had never been disassembled by
  Ghidra at all. Once bounded (`0xc0000594`, `0x84` bytes), it decompiles as
  a real state-machine handler with a real caller: a small
  function-pointer/state-id table at `0xc002a648` (`{ptr, state_id}` rows).

A cross-file bug was found and fixed during this investigation:
`panelbus_dispatch.c` had mis-transcribed `0xc00008cc`'s body as extra
statements appended onto `panelbus_hw_bringup`'s own, and had separately
undercounted `i2c0_i2c1_base_select`'s callers. Both are corrected in that
file now. The overall conclusion - no live K2 dispatcher consumes this I2C
hardware beyond boot-time bring-up - is unchanged; the previously
miscounted second caller turned out to be dead code, not a hidden runtime
consumer.

## Files absent in K2

**`clcdc_test_dispatch.c`** has no K2 file. K1's factory test-menu keypress
dispatcher depended entirely on `cpsoc.cpp`'s own scratch-struct offset
(`0x821`) and `cpsoc_led_set`/`_clear` - both gone along with the
rest of `cpsoc.cpp`. This is positive evidence, not just absence of an
anchor string: `clcdc_test_pattern`'s own K2 port (`clcdc.c`) has exactly
one caller in the whole image, and it is tick/counter-driven, not
keypress-driven; both of `PanelManager.cpp`'s anchor xrefs
(`0xc00061cc`/`0xc00066f8`) independently resolve to
`panel_manager_encode_scan_event` (a switch/knob scan-event encoder) and
`panel_manager_dispatch_scan_byte` (a message dispatcher) respectively -
neither shaped anything like K1's key-range switch.

## MIDI subsystem cluster

Seven files: `midi_engine.c`, `chan_link_hw.c`, `chan_param_ctrl.c`,
`chan_slot_dispatch.c`, `usbdc_midi_status_glue.c`, `uart1_midi_queue.c`,
`midi_chan_status_queues.c`.

The whole USB-MIDI transport stack is essentially unchanged between K1 and
K2, just re-addressed: `chan_link_hw.c` (17/17), `chan_slot_dispatch.c`
(14/14), `chan_param_ctrl.c` (17/18), `midi_engine.c` (24/26),
`uart1_midi_queue.c` (5/5), and `midi_chan_status_queues.c` (9/9) match at
an exact Ghidra-reported byte size, and every numeric register/mask
constant independently re-resolved from K2's own literal pool is
bit-for-bit identical to K1's own values.

`usbdc_midi_status_glue.c` has 12 of 16 K1 functions located:

- `chan_irq_toggle` (K2 `FUN_c00087d8`)
- `chan_ring_drain_pack` (K2 `FUN_c0008b04`) - a real K2-only addition: its
  ring-counter read/decrement are now bracketed in IRQ-guard calls
  (`irq_guard_enter_k2`/`_exit_k2`) K1's reconstructed C shows no trace of.
- `chan_maybe_enable_irq4` (K2 `FUN_c0008b98`)
- `chan_status_notify` (K2 `FUN_c0008be8`)
- `chan_status_byte_msb` (K2 `FUN_c0008c14`) - its boundary test collapses
  to `b >= 0x7f`, not K1's `(b & 0x80) != 0` (`b >= 0x80`) - a genuine
  one-value threshold shift, verified by hand enumeration.
- `chan_ring_entry_clear_0..3` quartet (K2 `FUN_c0008c3c`/`_c48`/`_c54`/
  `_c60`) - `clear_1`/`_2`/`_3` (offsets `+0x10`/`+0x1c`/`+0x28`) are
  IRQ-guarded with the same pair `chan_ring_drain_pack` uses, while
  `clear_0` (`+4`) stays bare, an asymmetric addition K1 never had. Caller
  grouping by offset parity (0/2 from one caller, 1/3 from another) matches
  K1's own shape.

Still open: `chan_status_promote_on_flag` - its K1 body never touches
either of the two objects (`chan_status_obj`, `chan_ring_obj`) that could
be used as byte-search anchors, so it could not be located by that
technique. A concrete lead is `chan_dispatch_probe`'s own K2 body
(`FUN_c00117c8`), not yet followed up.

Other K2-side differences (not transcription artifacts):

- Three separate K1 globals (`midi_hw_mode_flags`/`chan_port_hwctx_global`/
  `chan_global_hi_mode_flags`) merge into one K2 address (`0xC01CCD10`).
- `uart1_midi_queue.c` and `midi_chan_status_queues.c` merge into one
  contiguous K2 address run (two files in K1, one compilation unit in K2;
  kept as two separate reconstructed files here to match K1's
  file-per-subsystem convention).
- `chan_link_rt_queue_push` (`chan_param_ctrl.c`, K2 `FUN_c000ea68`, 356
  bytes) is a real K2-side restructuring, not a 1:1 port. K1's version is a
  small, self-contained "push one realtime byte into a 64-slot ring"
  primitive taking one incoming byte parameter. K2's version takes no
  incoming byte at all: it pops its own internal realtime ring (a new
  read-index field at `link+0x139`, immediately before the pending-count
  field at `link+0x13a` K1 already had), packages the popped byte as a
  4-byte USB-MIDI Realtime CIN frame (`{0x0f, byte, 0, 0}`) and transmits
  it, then separately drains ring 1 when it has pending data and
  arms/re-arms two IRQ-enable lines (3 and 4) based on two independent
  readiness checks, then conditionally acknowledges the link - a combined
  "realtime service tick" that subsumes the old push primitive's role. Two
  of its own callees' `which`/`enable` arguments to `chan_irq_toggle` could
  not be recovered from this call site's own decompile (only 2 of 4
  arguments visible) - transcribed as unverified placeholders rather than
  invented.
- `midi_hw_flush_alt` (`midi_engine.c`) is a bare, unconditional one-line
  forwarder to `chan_link_ack` (`chan_param_ctrl.c`) - no independent flush
  logic of its own survives in K2. `midi_hw_flush_notify` was not found:
  `chan_link_ack` has exactly 6 real callers, and the two not otherwise
  accounted for (`FUN_c000a308`/`FUN_c000ef8c`) both have real callers of
  their own (neither is a bare thin-forwarder the way `midi_hw_flush_alt`
  is) - left unresolved rather than guessed.
- `midi_ring1_push_zeros` - a brand-new function with no K1 counterpart.
- `uart1_tx_byte` gained a real new side effect (sets a drop-flag-adjacent
  global) K1's own version never had.

### `omap_l137_usbdc_ext.c` - `usbdc_core_isr`

This was the single largest data gap in the shared-driver-layer
migration: a real, 1940-byte hole in K2's function coverage
(`0xc0003840-0xc0003fd4`) that Ghidra never bounded as a Function object -
manually transcribed instruction-by-instruction from raw disassembly.

The function is a near-exact structural/semantic match to K1's own
`usbdc_core_isr`, with 6 differences:

- a real `usbdc_desc_arm_slot(dev,1,1)` call resolving one of K1's own
  guessed phantom-forward arguments
- a genuinely different register offset for the EP1-ready branch's CSR
  field, `0x516` vs K1's `0x462`
- a harmless dead intermediate dereference ahead of the 3 boot-flag writes
- 32-bit vs byte access on the EP0 context nibble-clear field
- a collapsed single-literal load for `usbdc_setup_dispatch_buf`
- compiler tail-merging of 3 no-dispatch event cases into one shared code
  fragment

This also established real K2 addresses for 5 of Section 3's 7
previously-unbodied DMA/descriptor-table functions, via their own direct
call targets inside `usbdc_core_isr`'s bus-reset branch (bodies not
transcribed). Section 3's last two functions
(`usbdc_desc_set_length`/`usbdc_desc_get_length`) are now fully addressed,
bodied, and have real callers.

`omap_l137_usbdc_ep0.c`'s `FUN_c000a7dc` is, on the best available evidence,
`cad_pedal_present`'s K2 replacement, expanded to a 3-valued classifier.

### `cobjectmgr.c`

`cobjectmgr_hardware_fault_watchdog`'s search came back negative from two
independent angles: the ack primitive (`kobj_eventflag_clear`, K2
`FUN_c001a01c`) has exactly one caller in the whole image, and the
3-task ROM autostart table (see `task_sched.c` above) accounts
for every autostarted task in K2, none of them cobjectmgr-shaped. The
function's real K2 address remains unresolved; a further xref search would
need a different anchor than either of these two.

## Building and testing

```bash
make
```

Syntax-checks every file in this tree with `arm-none-eabi-gcc -fsyntax-only`
(the same gate `K1_V06R06/` uses). All 33 files pass clean.

## Known limitations

Each item below is a genuinely open question - either no K2 counterpart has
been located, or a finding rests on evidence short of certainty. Where a
concrete next step exists, it is noted.

- **`omap_l108_syscfg.c`: 4 of 6 functions have no located caller.** A
  function-pointer-table dispatch mechanism has been ruled out (a raw-byte
  search for each orphan's address found zero hits). *To validate:* trace
  computed indirect branches in the surrounding code, or confirm the
  functions are unreachable dead code carried over from a build
  configuration K2 no longer uses.
- **`omap_gpio.c`: whether K1's DIR/OUT_DATA pair-1/3/4 defaults exist
  anywhere in K2 is not fully closed.** All 30 real callers of
  `gpio_bank_get_base` were checked and none matches, and a literal-constant
  byte search also came back empty, but an ARM MVN-immediate encoding could
  hide an equivalent constant from that search. *To validate:* decode any
  remaining unexamined GPIO-adjacent leaf for an MVN-based bit pattern
  matching K1's DIR/OUT_DATA values.
- **`usbdc_gap_config_slot`'s real K2 address is unlocated**, and
  `omap_l137_addr_gap_misc.c` currently carries a misattributed address
  (`0xc0002d80`, actually `panelbus_dispatch.c`'s `panelbus_i2c_mode_config`)
  for it. *To validate:* fix the misattribution in
  `omap_l137_addr_gap_misc.c`, then search for the real function by its
  USB/gap-configuration register signature rather than by address.
- **`soc_irq_gate_slot0x00_get`/`_ring3_state_reset`** (K1's cluster-11 tail
  items) have no located K2 counterpart. *To validate:* a byte sweep for the
  `table+0x00` literal only finds the 4 already-known consumers; a broader
  sweep for a bare getter-wrapper shape referencing that address would be
  the next step.
- **The 72-byte code region at `0xc0000098-0xc00000df`** in
  `soc_irq_gate.c` is undocumented and not bound by any Ghidra Function
  object. Its single reference (a parameter-type read from `task_sched.c`'s
  ROM-table-walk code) suggests it is itself a ROM-autostart-table entry.
  *To validate:* confirm against `task_sched.c`'s own autostart table
  whether a 4th task entry exists beyond the 3 already found.
- **`panelbus_hw_bringup` selects I2C1, not I2C0** (the value every K1 call
  site used). Whether this is a genuinely different physical bus assignment
  on the K2 board, or an unrelated hardware consumer of a shared selector,
  is unresolved. *To validate:* check K2 board schematics or trace the I2C1
  bus directly against known peripheral addresses.
- **`task_sched.c`: `eva_board_sched_ready`/`eva_board_sched_requeue`** (K1's
  richer "already-active, reprioritize" ready-insert functions) have no
  located K2 counterpart. *To validate:* would need a future pass focused
  on K2's reprioritize-while-waiting behavior specifically.
- **`task_sched.c`: `sched_wait_list_insert`'s unconditional
  `sched_remove_from_ready` call** is a real behavioral difference from
  K1, but whether it means K2 merged the timed/untimed wait-insert paths, or
  this function is actually the timed variant under an unexpected name, is
  unresolved. *To validate:* locate a second, K2-side wait-insert function
  (if one exists) to compare against.
- **`heap_alloc.c`: `heap_malloc`'s treebin/designated-victim slot layout**
  beyond the opening rounding/fast-path logic has not been established, nor
  has whether K2's heap base/end constants imply a different heap size than
  K1's. *To validate:* a direct read of both images' heap base/end symbol
  values would settle the sizing question directly.
- **`usbdc_midi_status_glue.c`: `chan_status_promote_on_flag`** and
  **`midi_engine.c`: `midi_hw_flush_notify`** remain unlocated. Byte-search
  anchors keyed on known related objects/functions do not reach either.
  *To validate:* for the former, follow the `chan_dispatch_probe`
  (`FUN_c00117c8`) lead; for the latter, no positive lead currently exists
  beyond the 6 known callers of `chan_link_ack`, none of which fit the
  expected shape.
- **`cobjectmgr.c`: `cobjectmgr_hardware_fault_watchdog`'s real K2 address**
  is unresolved. Two independent negative checks (its expected ack
  primitive's caller count, and the 3-task ROM autostart table)
  rule out the leads tried so far. *To validate:* would need a new anchor,
  independent of both approaches already exhausted.
- **`panel_scan_updater.c`: several `panel_fault()` call sites** across the
  four sub-steps (erase/write/verify/apply) have line-number arguments the
  decompiler elided; these are marked explicitly in the file rather than
  guessed. *To validate:* a raw disassembly pass targeting just those
  literal-load instructions would recover the real line numbers.
- **A handful of uncharacterized callees** remain across this tree:
  `FUN_c0005284`, `FUN_c001ac94`, `FUN_c000704c`, and
  `FUN_c00068d4`'s own fixed-global arguments. *To validate:* no specific
  lead exists yet for any of these; each would need to be picked up as a
  standalone investigation.
