# KRONOS2S_V01R10.VSB — Kronos 2 NKS4 Panel Sub-System Firmware

Source: `Decomp/subsystem/KRONOS2S_V01R10.VSB` (917 760 bytes — byte-identical *size* to
`KRONOS_V06R06.VSB`). This is the **Kronos 2** front-panel controller board firmware,
the sibling image to [`KRONOS_V06R06.VSB.md`](KRONOS_V06R06.VSB.md) (Kronos 1). Same
physical role as its K1 counterpart — a standalone embedded system (TI OMAP-L1x,
ARM926EJ-S) behind USB `0944:1005`, separate from the Kronos's own x86 host — but a
**partly redesigned codebase**, not a simple recompile/patch of K1.

Read `KRONOS_V06R06.VSB.md` first for the container-format background and Ghidra-setup
method, both of which transfer to K2 unchanged except where noted below. The
reconstructed C source lives at
[`kronosology/reconstructed/K2_V01R10/`](../../reconstructed/K2_V01R10/) (see that
directory's own `README.md` for per-file status); the K1 baseline it's diffed against
lives at [`kronosology/reconstructed/K1_V06R06/`](../../reconstructed/K1_V06R06/)
(read-only, untouched by this work).

---

## Container format ("KORG SYSTEM FILE") — diffed against K1

Same 256-byte header layout as K1 (see `KRONOS_V06R06.VSB.md`'s own table for the full
field list); only the fields that differ are called out here, confirmed by a direct
byte-for-byte read of both raw `.VSB` files:

| Offset | Size | K1 (`V06R06`) | K2 (`V01R10`) | Notes |
|---|---|---|---|---|
| `0x10` | 16 | `"KRONOS"` | `"KRONOS II"` | product tag, NUL-padded in both |
| `0x28` | 4 | `00 01 06 06` | `00 01 01 0a` | subtype `0001` unchanged; version `06.06` → `01.10`, matches filename `V01R10` |
| `0x2c` | 1 (of the 4-byte field logged as `80 02 03 ff`) | `80` | `00` | First byte of this still-unidentified 4-byte field changed; remaining 3 bytes (`02 03 ff`) unchanged. Not decoded (flags/checksum-seed guess unconfirmed either way). |
| `0x41` | 1 (of the 2-byte field logged as `02 00`) | `00` | `01` | Second byte of this still-unidentified field changed. Guessed as "possibly chunk/section count" — a K1→K2 delta of exactly 1 is consistent with that guess (one more chunk/section in K2), but not confirmed. |
| `0x34`/`0x3c` | 4+4 | `00 00 0e 00` (917 504) | `00 00 0e 00` (917 504) | payload length identical — the payload is the same size in both images, only its contents differ |

**Whole-file byte diff**: 257 559 of 917 760 bytes differ (28.06%); restricted to the
payload only (skipping the 256-byte header): 257 552 of 917 504 bytes differ (28.07%)
— essentially all of the header-vs-payload difference is in the payload. This is
consistent with a meaningfully redesigned image, not just a few patched addresses: a
simple hot-fix/relink would show a much smaller diff fraction concentrated in a handful
of runs, while ~28% spread across the image is compiler-output-scale change (new
functions, removed functions, shifted layout cascading through every subsequent
address-embedding literal pool).

---

## Target hardware — same base load address, same reset-vector shape

Confirmed from raw payload bytes: K2 still links flat at **`0xC0000000`**, with the
same 5×`LDR PC,[PC,#0x18]`-style ARM exception vector table at payload offset `0x000`
as K1, though the non-reset vectors 6/7/8 use slightly different literal-pool offsets
in K2 (`E5 9F F0 04` / `E5 9F F0 14` / `E5 9F F0 14` vs. K1's — not independently
re-checked). The reset vector's own literal-pool target differs, as expected from any
address-shifted rebuild: `0xC0009534` (K1) → **`0xC000A860`** (K2), confirmed both by
the raw literal-pool bytes at payload offset `0x20` and by decompiling `FUN_c000a860`
in the K2 Ghidra dump — same shape as K1's reset handler (a 2-call stub tail-calling
into a noreturn crt0 function).

---

## Firmware source-file inventory — anchor-string comparison

Every `../*.cpp`-style `__FILE__` literal was enumerated in both images' string tables
and diffed.

**Present in both K1 and K2** (the shared low-level driver layer):

| Source file | K1 anchor | K2 anchor |
|---|---|---|
| `../CryptoAt88.cpp` | `0xc0022ce4` | `0xc002a70c` |
| `../I2cByGpio.cpp` | `0xc0022cf8` | `0xc002a720` |
| `../MCU/OmapL108.cpp` | `0xc0022d0c` | `0xc002a734` |
| `../MCU/Component/OmapL137Mcasp.cpp` | `0xc0022d20` | `0xc002a76c` |
| `../MCU/Component/OmapL137Usbdc.cpp` | `0xc0022d68` | `0xc002a790` |
| `../EvaBoardMain.cpp` | `0xc0022d8c` | `0xc002b218` |
| `../cobjectmgr.cpp` | `0xc0022dcc` | `0xc002b2d4` |
| `../CDix4192.cpp` | `0xc0023180` | `0xc002b5e8` |
| `../clcdc.cpp` | `0xc0023bac` | `0xc002b5f8` |

**Present in K1, confirmed absent from K2's string table** (genuinely gone):

| K1-only source file | K1 anchor | Reconstructed as (K1) |
|---|---|---|
| `../cpsoc.cpp` | — | `K1_V06R06/cpsoc.c` |
| `../ctouchpanel.cpp` | — | `K1_V06R06/ctouchpanel.c` |
| `../McAspHandler.cpp` | — (0 xrefs even in K1 — dead string already in K1) | not reconstructed separately in K1 either |
| `../MCU/Component/OmapL108Spi.cpp` | `0xc0022d44` | `K1_V06R06/omap_l108_spi.c` |

`OmapL108Spi.cpp` has no literal anywhere in K2's string table; it's replaced
positionally (same slot in the literal pool's address ordering, between `OmapL108.cpp`
and `OmapL137Mcasp.cpp`) by a new `../MCU/Component/OmapL108Syscfg.cpp` string. Whether
K2's SPI peripheral driver was folded into a broader "Syscfg" unit, replaced by a
different bus, or simply renamed is not established.

**New in K2, no K1 counterpart**:

| K2-only source file | K2 anchor |
|---|---|
| `../MCU/Component/OmapL108Syscfg.cpp` | `0xc002a748` |
| `../PanelManager.cpp` | `0xc002af98` |
| `../PanelScanUpdater.cpp` | `0xc002b18c` |
| `../SwitchOnChatteringDetector.cpp` | `0xc002b1f4` |
| `../SystemInfoHolder.cpp` | `0xc002b5d0` |

The K2 literal-pool address *ordering* of the shared files exactly mirrors K1's own
ordering (CryptoAt88 → I2cByGpio → OmapL108 → [Spi/Syscfg] → OmapL137Mcasp →
OmapL137Usbdc → … → EvaBoardMain → cobjectmgr → … → CDix4192 → clcdc), with the new
K2-only files' literals inserted at the two points where K2 gained whole new
subsystems (between `OmapL137Usbdc.cpp`/`EvaBoardMain.cpp`, and between
`cobjectmgr.cpp`/`CDix4192.cpp`) — consistent with an incremental link-order-preserving
addition of new translation units rather than a full reshuffle. Each shared file's
neighbors in the string table are the same neighbors in both images, useful for
locating a file's approximate K2 code region before decompile-shape matching for exact
function boundaries. `../cad.cpp` (K1's analog/knob driver) was never checked against
K2's string table — status unknown.

---

## Shared low-level driver layer — reconstruction status

| K1 file | K2 file | Status |
|---|---|---|
| `aintc.c` | `aintc.c` | Byte-for-byte identical logic, re-addressed; K2 has 20 vs K1's 24 `aintc_base` callers (fewer early-bringup stubs, consistent with cpsoc/cad removal). |
| `omap_l108.c` | `omap_l108.c` | Structurally identical tick-service API; `cad_delay_ticks`/`cad_calibration_progress_pump` (K1's own callers) have no confirmed K2 mapping. |
| `i2c_by_gpio.c` | `i2c_by_gpio.c` | Done; corrected a real K1 signature bug in the process (`write_block`/`read_block` are 5-arg, not K1's documented 6-arg shape). |
| `mcasp.c` | `mcasp.c` | Byte-for-byte structurally identical to K1, including embedded fault-call line numbers — a straight recompile/relink, not a hardware-generation port. `0xc0002738` confirmed real code: the predicted `mcasp_configure_clock`/`mcasp_configure_pins` call pair plus further literal-pool constant writes. `mcasp_reinit_reduced` and `mcasp_clock_step_a` are cited from `omap_l137_addr_gap_misc.c` (both confirmed there — see below). |
| `crypto_at88.c` | `crypto_at88.c` | Migrated, anchor confirmed. |
| `cdix4192.c` | `cdix4192.c` | Structurally identical port of K1. All 5 clustered CDIX register tables (`cdix_config_table`, `cdix_mode0_table`, `cdix_mode1_table`, `cdix_reset_table_a`, `cdix_reset_table_b`) confirmed byte-exact and contiguous in `.rodata`. `mode0`/`mode1` tables differ in exactly 2 of 13 entries between K1/K2; `reset_table_a`'s 12 entries reconstruct as `config_table`'s opening pair plus `mode1_table`'s own tail, bit-for-bit. |
| `clcdc.c` | `clcdc.c` | Migrated. |
| `cobjectmgr.c` | `cobjectmgr.c` | Migrated. `cobjectmgr_hardware_fault_watchdog` is confirmed to be none of K2's 3 ROM-autostarted tasks (see Task scheduler below) — the watchdog mechanism it uses in K2 is unidentified. |
| `eva_board_main.c` | `eva_board_main.c` | Migrated, but the reconstruction's control flow is known-wrong — see Task scheduler below: `eva_board_main` does not exist as one K2 function, and this file's straight-line four-step reconstruction has not been corrected to match. |
| `omap_l137_usbdc.c` / `_ep0.c` / `_ext.c` | same | Migrated. `_ep0.c`: `FUN_c000a7dc` is `cad_pedal_present`'s K2 replacement, expanded from a boolean to a 3-valued pedal-type classifier while keeping K1's bit-0x17 gate, confirmed caller `FUN_c0009838`. `_ext.c`: `usbdc_core_isr` fully resolved — see below; all 7 of Section 3's DMA/descriptor-table functions now have confirmed K2 addresses (`usbdc_dma_engine_reset@0xc00035b4`, `usbdc_desc_table_global_init@0xc00036c8`, `usbdc_desc_arm_slot@0xc000379c`, `usbdc_ep_arm_rx@0xc00037c4`, `usbdc_ep_arm_tx@0xc00037f4`, `usbdc_desc_set_length@0xc0003680`, `usbdc_desc_get_length@0xc00036a8`); Section 4 (`usbdc_ep0_notify_tx_complete`/`_rx_complete`) remains fully open — not called directly by `usbdc_core_isr`. |
| — (new file) | `omap_l108_syscfg.c` | New K2-only low-level driver — see New panel-scan architecture, below. |
| `soc_periph.c` | `soc_periph.c` | Peripheral base-address table structurally near-identical to K1 (11 of 13 K1 entries carry over with identical or TRM-consistent values); 2 new table entries (I2C0/I2C1 selector, eCAP2 base); SPI0 base survives despite cpsoc/cad removal — now consumed by `panel_scan_updater.c`'s firmware-update sequencer instead; UART selector shrank from K1's 3-way to a single UART1-only accessor; pinmux writes merged from 3 one-word K1 leaves into 1 three-word K2 leaf. `FUN_c00019e0`/`FUN_c00019e8` confirmed as McASP-instance base getters, named `mcasp0_base_get`/`mcasp0_fifo_base_get`. `timer64p0_enable_ch15` (ch-0x15/Timer64P0 lazy-init AINTC-enable singleton) confirmed defined here, not in `soc_irq_gate.c`. `board_desc_set_pinmux_3word()` is the real owner of what was briefly misattributed to `omap_l108_syscfg.c` as `omap_syscfg_set_reg118` (removed there — see New panel-scan architecture). |
| `omap_gpio.c` | `omap_gpio.c` | K1's generic pair-indexed GPIO register primitives (`gpio_reg_read_in/set_bit/clear_bit`, `bank_base + pair*0x28 + offset`) are gone in K2 (confirmed via full-image pattern search, zero matches) — every K2 GPIO-touching leaf hardcodes one fixed bit at one fixed offset directly. `gpio_bank_hw_init`'s K2 counterpart writes a genuinely smaller subset of K1's registers (no DIR/OUT_DATA defaults for pairs 1/3/4). Whether those pair-1/3/4 defaults exist elsewhere in K2 is closed, with reasonable confidence, in the negative: all 30 confirmed callers of `gpio_bank_get_base` were individually examined and none writes to the pair-1/3/4 offsets (`+0x38`, `+0x88`/`+0x8c`, `+0xb0`/`+0xb4`); a full-image search for K1's three DIR default constants as raw little-endian literal-pool bytes found zero hits (an ARM MVN-immediate encoding could still hide an equivalent constant from a byte search — not ruled out). |
| `heap_alloc.c` | `heap_alloc.c` | Structurally identical to K1 (dlmalloc-derivative shape unchanged) — `heap_sbrk`/`heap_trim` statement-for-statement identical, `heap_malloc`'s opening rounding/bin-selection logic identical including the same shift/offset constants. `heap_free` fully decompiled, adding real detail on `heap_state`'s bitmap/top-chunk/bin-index layout and `heap_trim`'s real trigger threshold and pad argument. |
| `task_sched.c` | `task_sched.c` | See Task scheduler / `eva_board_main`, below. |
| `soc_irq_gate.c` | `soc_irq_gate.c` | See IRQ-gate leaf cluster / second I2C bus, below. |
| `panelbus_dispatch.c` | `panelbus_dispatch.c` | See IRQ-gate leaf cluster / second I2C bus, below. |

---

## New panel-scan architecture

Five genuinely-new K2-only files: `PanelManager.cpp`, `PanelScanUpdater.cpp`,
`SwitchOnChatteringDetector.cpp`, `SystemInfoHolder.cpp`, and
`../MCU/Component/OmapL108Syscfg.cpp` (a low-level driver, not panel-scan logic, but new
and directly filling `OmapL108Spi.cpp`'s old link-order slot). Note: the pre-fetched
static Ghidra dump for K2 does not include any of these files' functions as decompiled
Function objects, despite the addresses sitting inside the dump's nominal coverage
range; some were recovered via manual capstone ARM32 disassembly before live Ghidra
access became available, others were then confirmed/completed live.

| File | What it does | Confidence |
|---|---|---|
| `PanelManager.cpp` | Consolidates K1's `cpsoc.cpp` (switch/LED naming) + `cad.cpp` (knob/slider/pedal naming) into one file. 77-entry switch/LED name table (vs. K1's 73, byte-exact, no `0xff` sentinel present), single-bit LED-state set/clear via a `(word_slot, bit_pos)` lookup table, 16-bit bulk-apply wrapper. Both of the file's own anchor xrefs are resolved: one is a switch/knob scan-event encoder, the other a 312-instruction message dispatcher that is the real caller of `SwitchOnChatteringDetector.cpp`'s two functions. Real callers for the LED-bit functions: a diagnostic scroller and a wire-protocol opcode dispatcher (`wire_dispatch.c`). | High |
| `PanelScanUpdater.cpp` | Direct split-out of K1's PSoC field-firmware-update feature (previously mixed into `cpsoc.cpp`). Top-level update sequencer (status text, erase/write/verify/apply steps, version/revision check, fault-on-mismatch) and all four sub-steps are transcribed as real SPI/TWI handshakes, with a real caller found; `apply()`'s signature is a (hex-stream pointer, length) pair. | High |
| `SwitchOnChatteringDetector.cpp` | A standalone debounce module with no K1 equivalent — a 3-state (idle/debouncing/confirmed) per-switch FSM with a doubly-linked active-entry list; register/remove functions transcribed. Its register/remove indexing discrepancy is confirmed real (decompile-verified), not a transcription artifact. | High |
| `SystemInfoHolder.cpp` | Version/health-status display singleton — prints `SyncErrorCount` and a renamed `"Panel Scanner Version:%02d Revision:%02d"` (K1: `"Panel Scan System Version..."`). Has a bounds-checked table accessor and the "SYSTEM STARTUP FAILED" fault screen, full text recovered. | High |
| `../MCU/Component/OmapL108Syscfg.cpp` | SoC pin-mux / module reset-and-enable peripheral driver, filling `OmapL108Spi.cpp`'s old link-order slot — consistent with the SPI bus's only two K1 consumers (`cpsoc.cpp`/`cad.cpp`) both being gone. Real reset/hold/deassert/configure/poll-with-timeout sequence plus several PINMUX-shaped register writes. Gained real callers for two functions, three more byte-verified against live memory, one brand-new 7th leaf found; one previously-attributed function (`omap_syscfg_set_reg118`) was removed as a cross-file misattribution — it's actually `soc_periph.c`'s own `board_desc_set_pinmux_3word()`. | Highest — the only one of the 5 files with a directly-confirmed anchor-string xref |

K1's `"Psoc version error %02x != %02x : Id %03d"` string is absent from K2's string
table entirely — status (renamed, dropped) not established.

---

## IRQ-gate leaf cluster / second I2C bus

**`soc_irq_gate.c`** (the AINTC channel enable/disable leaf cluster sitting right after
the ARM exception vector table): structurally near-identical to K1 for 10 of 11 AINTC
channel-gate leaves plus both "group" enable/disable dispatchers (found by a full-image
sweep for every K2 function calling `aintc_base()` directly — 14 hits: 11 real
channel-gate leaves, `aintc_base` self-referencing, `eva_board_crt0`, and the group-A
dispatcher). Confirmed differences from K1: Timer64P1 (ch 0x17) and channel 0x36 gating
are dropped entirely (zero matches in a full-image literal sweep); channel 0x2a now has
a real enable side, inlined into the group-A dispatcher's own tail (K1 had no matching
enable stub); channel 0x32's disable path re-arms its own enable guard on every call
(K1's never did); channel 0x2a's disable-side GPIO ack write is inlined rather than
routed through a dedicated `omap_gpio.c` helper the way K1 did it. The shared ~0x4C-byte
bookkeeping table these leaves reference resolves to **0xC00E0000** — the identical
fixed physical address K1's own file found for its larger (0xC00E0000-0xC00E0068)
version of the same table, strong evidence it's a fixed OMAP-L138/DA850 SRAM/scratch
location on the silicon itself, not firmware-relative data (it survived the K1→K2
rewrite at the same address). `soc_irq_gate_timer0_quiesce`'s own cached-handle literal
is confirmed to be `table+0x00` (`0xC00E0000`), not `table+0x08` — CLUSTER 2's own
`0xC00E0008` (the McASP param cache) is a separate, independently-verified slot, not the
same one. `FUN_c0000040` (the ch-0x15/Timer64P0 lazy-init AINTC-enable singleton this
cluster cites) is defined in `soc_periph.c` as `timer64p0_enable_ch15`, not in this
file. A 72-byte code region at `0xc0000098`-`0xc00000df` (no Ghidra Function object)
sits in the gap between two of this cluster's functions, sharing CLUSTER 1's bit-set
idiom plus a report-code call; reached only via a `PARAM`-type reference from
`task_sched.c`'s `FUN_c00199dc`, suggesting it's a `task_sched.c` ROM-autostart-table
entry (not claimed as characterized here). `gap_slot_bringup`
(`omap_l137_addr_gap_misc.c`'s claimed `usbdc_gap_config_slot` at `0xc0002d80`) is a
real cross-file misattribution: that address is actually `panelbus_dispatch.c`'s
`panelbus_i2c_mode_config` (an I2C ICMDR-shaped config write, not USB/gap-shaped).
`usbdc_gap_config_slot`'s real K2 address remains unlocated, as do `slot0x00_get`/
`ring3_state_reset` (a full-image search for the `table+0x00` literal found only the 4
already-known real consumers, none a bare getter wrapper).

**`panelbus_dispatch.c`** (K1's on-chip I2C0/I2C1 hardware controller dispatcher): K1's
elaborate per-tick RX/TX opcode dispatcher — built on `cad.cpp`'s calibration handlers
and `cpsoc.cpp`'s "third SPI device," both confirmed absent from K2's string table — has
**no confirmed K2 counterpart**. The underlying hardware primitive (the I2C0/I2C1 base
selector, `soc_periph.c`'s `i2c0_i2c1_base_select`) survives unchanged, but has exactly
one caller in the whole binary: `panelbus_hw_bringup` (a crt0 hardware bring-up stub,
resolving one of `eva_board_main.c`'s "not individually traced" crt0 calls). No RX poll
loop, TX ring, or opcode table exists anywhere in the covered K2 dump.
`panelbus_hw_bringup`'s body repeats the same idx=1/I2C1 setup a second time via a tail
branch — not a mirrored I2C0 pass, correcting an earlier "symmetric I2C0+I2C1 pair"
reading. Whether the real K2 panel-scan architecture uses this hardware block at all is
open — its confirmed consumer, `PanelScanUpdater.cpp`, uses SPI per `soc_periph.c`'s
finding that SPI survives, making an I2C-based path unlikely but not ruled out.

---

## MIDI subsystem cluster

K1's USB-MIDI transport cluster — `midi_engine.c`, `chan_link_hw.c`,
`chan_param_ctrl.c`, `chan_slot_dispatch.c`, `usbdc_midi_status_glue.c`,
`uart1_midi_queue.c`, `midi_chan_status_queues.c` (7 files, 4466 K1 source lines) — is
migrated into K2. This whole MIDI transport stack — hardware register layer, USB-MIDI
CIN framer/event state machine, the link watchdog/queue-drain cluster, the per-channel
parameter engine, the UART1 16550-shaped driver, and the TX/RX status-queue pair — is
essentially **unchanged between K1 and K2**, just re-addressed: across the 5 files
ported with high confidence (`chan_link_hw.c`/`chan_slot_dispatch.c`/
`chan_param_ctrl.c`/`midi_engine.c`/`midi_chan_status_queues.c`/`uart1_midi_queue.c`, 66
of K1's ~76 functions), the overwhelming majority match at an exact Ghidra-reported byte
size, and every numeric register constant independently re-resolved from K2's own
`DAT_` pool is bit-for-bit identical to K1's values.

**Confirmed differences from K1**:
- Three K1 globals merge into one in K2: `midi_hw_mode_flags` (`midi_engine.c`),
  `chan_port_hwctx_global` (`chan_slot_dispatch.c`), and `chan_global_hi_mode_flags`
  (`chan_param_ctrl.c`) are three separate addresses in K1 but resolve to the identical
  literal `0xC01CCD10` in K2.
- `uart1_midi_queue.c` and `midi_chan_status_queues.c` merge into one contiguous K2
  address run (`0xc0011010`-`0xc0011a80`, no gap) — K1 kept the UART1 driver and the
  TX/RX status-queue cluster in two separate files; K2 built them as one compilation
  unit.
- `chan_link_rt_queue_push`'s K2 role is expanded, not a 1:1 port. K1's version is a
  small, self-contained "push one realtime byte into a 64-slot ring" primitive taking an
  incoming byte parameter. K2's counterpart (`chan_param_ctrl.c`'s `FUN_c000ea68`, 356
  bytes vs K1's 144) takes no incoming byte — it pops its own internal realtime ring (a
  new read-index field at `link+0x139`, immediately before the pending-count field at
  `link+0x13a` K1 already had), packages the popped byte as a 4-byte USB-MIDI Realtime
  CIN frame (`{0x0f, byte, 0, 0}`) and transmits it via `chan_link_tx`, then separately
  drains ring 1 when it has pending data (optionally poking `midi_hw_set_reg_f6` first)
  and arms/re-arms two IRQ-enable lines (3 and 4) based on two independent readiness
  checks, finally conditionally acknowledging the link via `chan_link_ack` — a combined
  "realtime service tick" that subsumes the old push primitive's role entirely. Two of
  `chan_irq_toggle`'s own `which`/`enable` arguments at this call site could not be
  recovered from the caller's decompile (only 2 of 4 arguments visible) — left as
  explicitly-flagged unverified placeholders.
- `midi_ring1_push_zeros` (`midi_engine.c`'s `FUN_c000e7cc`) is a new K2 function with no
  K1 counterpart — a "push N zero-filled words into ring 1" loop sharing K1's ring-1
  field layout exactly.
- `uart1_tx_byte` (`uart1_midi_queue.c`) gained a real side effect in K2: it now also
  sets a flag global whenever the transmitted byte isn't `0xFE` (Active Sensing) — the
  same condition K1's drop-filter tests, but K1's version never touched that flag
  itself.
- `midi_hw_flush_alt` (`midi_engine.c`) is a bare, unconditional one-line forwarder to
  `chan_link_ack` (`chan_param_ctrl.c`'s `FUN_c000d564`) — no independent flush logic
  survives in K2, whatever K1's own (never-decompiled) version may have done
  independently. `midi_hw_flush_notify` was not found: `chan_link_ack`'s 6 real callers
  are all accounted for by other functions, none a bare thin-forwarder the way
  `midi_hw_flush_alt` is.

**`usbdc_midi_status_glue.c`**: 12 of its 16 K1 functions are located and ported —
`chan_ring2_relay_and_status`, `usbdc_ep_regblock_ptr_a`/`_b`, `chan_irq_toggle` (K2
`FUN_c00087d8`, confirmed caller `usbdc_ep_state7_handler` in `omap_l137_usbdc_ext.c`),
`chan_ring_drain_pack` (K2 `FUN_c0008b04`, callee of `chan_slot_dispatch.c`'s
port-interrupt dispatcher `FUN_c000d6a0`; brackets its ring-counter read/decrement in
IRQ-guard calls `FUN_c0004f40`/`FUN_c0004f50` that K1's reconstruction shows no trace
of), `chan_maybe_enable_irq4` (K2 `FUN_c0008b98`), `chan_status_notify` (K2
`FUN_c0008be8`), `chan_status_byte_msb` (K2 `FUN_c0008c14`, boundary test simplifies to
`b >= 0x7f`, a one-value shift from K1's `(b & 0x80) != 0`), and the `chan_ring_entry_
clear_0..3` quartet (K2 `FUN_c0008c3c`/`_c48`/`_c54`/`_c60`; `clear_1`/`_2`/`_3` are
bracketed in the same IRQ-guard pair `chan_ring_drain_pack` uses, while `clear_0` stays
unguarded — an asymmetric addition K1 never had; caller grouping by offset parity
(entries 0/2 from one caller, 1/3 from another) is identical in shape to K1's own
quartet, at new K2 addresses `FUN_c000b760`/`FUN_c000f880`). Only
`chan_status_promote_on_flag` remains unlocated — its K1 body uses a different, unrelated
handle from the two objects (`chan_status_obj`/`chan_ring_obj`) whose literals were used
to find the rest, so it couldn't be found the same way; `chan_dispatch_probe`'s K2 body
(`FUN_c00117c8`, called by `chan_maybe_enable_irq4`) is the concrete lead for locating
it.

**`usbdc_core_isr`** (`omap_l137_usbdc_ext.c`, Section 6): resolved. A genuine
1940-byte hole in K2's function list (`0xc0003840`-`0xc0003fd4`) that Ghidra's own
auto-analysis never bounds as a function — manually transcribed from the raw
instruction listing. Near-exact structural and semantic match to K1's own
`usbdc_core_isr` (same interrupt-status decode, same bus-reset EP1-4 re-init sequence
at byte-identical offsets/values, same per-endpoint event dispatch chain, same tail)
with six confirmed differences from K1:
1. The SETUP-pending branch's `usbdc_desc_arm_slot` call uses confirmed register values
   `(dev, 1, 1)` — K1's own file could only guess `(dev, 0, 0)` here due to a
   phantom-forwarded-argument decompiler artifact obscuring the real values in K1.
2. The EP1-ready branch's CSR-like field sits at byte offset `0x516` in K2 (matching the
   direct/flat RXCSR-window formula for endpoint 1) vs. K1's `0x462` for the
   structurally identical branch.
3. The three boot-flag writes each compile through a functionally dead intermediate
   dereference of `dev` before the actual fixed-global write — harmless unless that
   dev-relative address is itself a volatile hardware register with a read side effect
   (not checked).
4. The EP0 context nibble-clear operations (`&= 0xf0`) compile to full 32-bit `ldr`/`str`
   in K2, not the byte `ldrb`/`strb` K1's byte-pointer-typed reconstruction implies.
5. `usbdc_setup_dispatch_buf`'s own indirection collapses to a single literal-pool load
   in K2 rather than K1's implied two-hop dereference.
6. Three "write event code, no dispatch call, goto tail" cases (masked bits `0x20000`,
   `0x2`, `0x8`) are compiler-tail-merged into one shared physical instruction sequence
   in K2's compiled code — a codegen artifact, not a behavioral difference; represented
   as three separate cases in the reconstructed C, matching K1's structure.

---

## `cpsoc.cpp`-adjacent stragglers

**`clcdc_test_dispatch.c` — confirmed absent from K2, no file created.** K1's factory
test-menu keypress dispatcher (`clcdc_test_pattern_dispatch`, keyed off a key-code byte
at `cpsoc`'s own scratch offset `0x821`, driving `clcdc_test_pattern` and two
`cad`-adjacent analog-arg setters) has no surviving K2 counterpart. `clcdc_test_pattern`'s
own K2 port (in `clcdc.c`) has exactly one call site in the entire static dump
(`FUN_c000a4bc`), and that function is tick/counter-driven (a `param+0x30 > 2000`
timeout gate, structurally a debounce/watchdog state) — not a keypress dispatcher.
`PanelManager.cpp`'s two anchor xrefs (`0xc00061cc`, a switch/knob scan-event encoder;
`0xc00066f8`, a MIDI-status-byte-shaped dispatcher comparing against `0x80`/`0x90`/
`0xa0`/`0xa4`) independently rule out both as candidates. Consistent with `cpsoc.cpp`
itself being confirmed entirely absent from K2's string table: the factory test-menu
system this file exposed appears genuinely gone in K2, not relocated. `clcdc_test_
pattern` itself does survive, already ported in `clcdc.c`, with a different
(tick-driven, not keypress-driven) caller.

**`cdix_autoswitch.c` — ported cleanly.** All three functions
(`cdix_set_format_reg`/`cdix_apply_mode_table`/`cdix_reset_and_configure`) are exact
byte-size matches against K1 (20/20, 76/76, 120/120), structurally identical
operation-for-operation. K2's own outer CDIX auto-switch state machine
(`FUN_c0010380`/`FUN_c0010258`, K2 counterparts of K1's `FUN_c000f01c`/`FUN_c000f0c8`)
directly toggles `omap_l137_addr_gap_misc.c`'s McASP2 bit-flag helpers
(`mcasp2_set_bit25`/`_bit15`) around the CDIX reset call — a previously-undocumented
hardware coupling between the digital-audio-interface reset path and the second McASP
instance, most plausibly a shared clock/reset domain. The shared context-handle
constant this cluster, `mcasp_init` (`mcasp.c`), and `clcdc_display_object_init`
(`omap_l137_addr_gap_misc.c`) all key off, `0xC00E004C`, sits inside the same fixed
`0xC00E0000`-based SRAM page `soc_irq_gate.c`'s port already confirmed as real
OMAP-L138/DA850 hardware — three otherwise-unrelated subsystems share one physical
bookkeeping page.

**`omap_l137_addr_gap_misc.c` — 5 of 6 K1 clusters ported, 1 confirmed genuinely
absent.** (1) McASP2 reduced-reinit — exact size match ×3, confirmed by `mcasp.c`'s own
K2 port, which cites this cluster's top function (`mcasp_reinit_reduced`) but leaves it
bodyless here (the same collision-avoidance split K1 used between its own two files).
(2) `usbdc_gap_config_slot` — structurally identical, subsystem still unattributed, same
as K1. (3) The UART-shaped register-pair cluster — confirmed absent, not a coverage gap:
a full-image raw byte search (`0xC0000000`-`0xC00E0000`) for all four of K1's
distinctive literal immediates (`0xe00`/`0xe01`/`0xf01` mode selectors,
`0x4a10`/`0x14a10` baud/divisor pair) found zero occurrences anywhere in K2's binary,
code or data — whatever peripheral K1 drove through this register shape is either
dropped in K2 or reconfigured through a different, unsearched register layout. (4) Two
tiny bit-extraction helpers — both found, plus one K2-only addition
(`gap_store_low_byte`, the write-side counterpart K1's file never documented). (5) The
default 256-entry RGB565 palette loader — found, ported. K1's own suspicious
`0x752ff`-iteration remap-loop bound (documented as "cannot be correct firmware
behaviour" but left untouched rather than silently corrected) resolves to the exact same
value in K2's independently-compiled image — strong evidence it's a genuine (if still
unexplained) compiled constant, not a per-build Ghidra data-type misinference. (6) The
large struct-zero-init function — exact size match, confirmed by `mcasp.c`'s own K2 port
(cited there as `mcasp_clock_step_a`, left bodyless there).

---

## Task scheduler / `eva_board_main`

`task_sched.c` kept the same RTOS-shaped task scheduler as K1 (TCB table, priority
ready-queue/bitmap, kernel-object event-flag API, delay/timeout min-heap), just
re-addressed. Boot tasks are auto-started via a ROM table walked in crt0
(`sched_task_create_and_ready`, called from `sched_tcb_table_init_and_autostart`), not
via an explicit call from `eva_board_main` the way K1's reconstruction implies.

**K2's ROM autostart table has exactly 3 tasks** (ids 1/2/3, priorities 0/2/4), dumped
byte-exact via live memory read and hand-disassembled (Ghidra has no Function object in
this address range):
- **Task id=1 (priority 0, most urgent)**: runs `eva_board_init_table`'s
  walk-and-dispatch loop, then `eva_board_final_setup`, then loops forever calling
  `eva_board_boot_status_dispatch` (a genuine 2-instruction infinite loop — confirmed by
  hand-decoding the branch back).
- **Task id=2 (priority 2)**: a separate stub that calls `eva_board_main_loop` (the real
  `master_dispatch_tick` forever-loop).
- **Task id=3 (priority 4, lowest — structurally unreachable in normal operation since
  neither of the other two tasks ever both block)**: an immediate, unconditional
  `crypto_at88_fault` call citing `"../EvaBoardMain.cpp"` line `0x70` — this is the K2
  counterpart of `eva_board_watchdog_fault_wrapper`.

**`eva_board_main` does not exist as a single K2 function** — it is split across tasks
1 and 2 above. `eva_board_main.c`'s current reconstruction presents these as one
straight-line function calling all four steps in sequence; that reconstruction was built
purely from call-site addresses, not from linear disassembly, and byte-level tracing
shows the boot-status-dispatch loop never falls through to the main-loop call. **The
file has not been corrected to match** — flagged here for a future consolidation pass.

Other confirmed findings: a real transcription bug in `sched_task_create_and_ready` (an
erroneous extra pointer addition) was found and fixed, which also settles K1's own
long-open ambiguity about which of a task's two ROM-pushed stack words is the real jump
target — it's `cfg+8`, not `cfg+4` (all of the ROM table's real `cfg+8` values are code
addresses). The delay-heap sift-up/sift-down internals' K2 addresses are confirmed via
`sched_delay_heap_extract_min`'s call graph. A function near `sched_remove_from_ready`'s
cluster (~`0xC001AA98`) resets the current task's TCB after removing it from ready —
resolves an address K1's file cited but never traced, though it does not match either
`eva_board_sched_ready`'s or `eva_board_sched_requeue`'s K1 shape, so neither is claimed
found.

---

## `wire_dispatch.c` — two corrections

`eva_wire_pedal_send`'s real signature is 3-parameter, not 2: the long-flagged
"distinguishing 2nd argument" between opcode reg `0x50` and `0x52` is a real 3rd
parameter (`1`="set" / `0`="clear" into a `(word_slot, bit_pos)`-table-addressed bit,
the same idiom `PanelManager.cpp`'s `panel_manager_set_led_bit` uses) — both call sites
corrected. `FUN_c0008d24` (the opcode `0xc6` continuation resolver) is an
8-bit-indexed-pixel-to-16-bit LUT expansion (a 256-entry table at `0xC001B814`) feeding
a `0x321`-halfword circular ring buffer — a different mechanism from the opcode `0xc4`
continuation's 800-wide framebuffer cursor, not the same ring. Not transcribed
statement-for-statement (dense, ~140 real statements).

---

## Status

Every K1 file *not* one of the four confirmed-obsolete `cpsoc.cpp`/`cad.cpp`/
`cpsoc_issp.cpp`/`ctouchpanel.cpp`-adjacent files (none of which have, or are expected
to have, a K2 counterpart) has now been either ported into `K2_V01R10/` or explicitly
confirmed absent with documented evidence. The shared low-level driver layer, the new
panel-scan architecture, the IRQ-gate/second-I2C-bus infrastructure, the full MIDI
transport cluster, and the remaining `cpsoc.cpp`-adjacent stragglers are all accounted
for. All 33 reconstructed files compile cleanly (`arm-none-eabi-gcc -fsyntax-only`).

Remaining work is depth, not breadth. Open items:

- `eva_board_main.c`'s reconstruction still needs correcting to match the real 3-task
  split described above.
- `chan_status_promote_on_flag` (`usbdc_midi_status_glue.c`) and `midi_hw_flush_notify`
  (`midi_engine.c`) are unlocated.
- `usbdc_gap_config_slot`'s real K2 address, `slot0x00_get`, and `ring3_state_reset`
  (all `soc_irq_gate.c`/`omap_l137_addr_gap_misc.c`-adjacent) are unlocated.
- `omap_l108_syscfg.c`'s 4 orphan functions (`reg130_a`/`reg130_b`/`dual_pull_enable`/
  `clear_pull_enable_0xc`) have zero confirmed callers; a function-pointer-table
  hypothesis (in the style of `eva_board_init_table`/`task_sched.c`'s ROM task table) was
  tested and ruled out (a full-binary search for each orphan's exact address found zero
  occurrences). Ghidra has never disassembled `0xc0001c04`-`0xc0001c4c` as Instructions
  at all, which is why no xref exists — there is no Instruction/Function object for the
  xref engine to compute against.
- `cad.cpp`'s status in K2 (folded into `PanelManager.cpp`'s naming role, per the new
  panel-scan architecture section, but a standalone equivalent driver file's fate is
  unconfirmed) and the switch/LED name table, boot splash, wire-protocol opcode table,
  and PSoC protocol documented in `KRONOS_V06R06.VSB.md` from K1 evidence — none of that
  should be assumed to carry over to K2 unchanged (K2's own switch/LED name table, 77
  entries grown from K1's 73, is located in `PanelManager.cpp` — see above).
- The two still-undecoded container-header fields (`0x2c` first byte, `0x41` second
  byte — see table above).

See `K2_V01R10/README.md` for the complete running per-file status.
