# KRONOS2S_V01R10.VSB — reconstructed source

Reverse-engineered source for `KRONOS2S_V01R10.VSB`, the Kronos 2 front-panel firmware -
the direct sibling of [`reconstructed/K1_V06R06/`](../K1_V06R06/) (Kronos 1's
`KRONOS_V06R06.VSB`). Same physical role (TI OMAP-L1x, ARM926EJ-S, USB `0944:1005`
front-panel board), same container format, same load address (`0xC0000000`) - see
[`kronosology/docs/modules/KRONOS2S_V01R10.VSB.md`](../../docs/modules/KRONOS2S_V01R10.VSB.md).

## Why this is a separate tree, not a K1_V06R06 patch

String-anchor comparison (`../*.cpp` `__FILE__` literals embedded in each image) shows
K2 has a **partly redesigned architecture**, not just a firmware version bump. Four K1
files are entirely absent from K2's string table - `cpsoc.cpp` (PSoC button/LED scan
chip driver), `cad.cpp` (A/D converter for knobs/pedals), `ctouchpanel.cpp` (touch
panel), `McAspHandler.cpp` - replaced by genuinely new files with no K1 equivalent:
`PanelManager.cpp`, `PanelScanUpdater.cpp`, `SwitchOnChatteringDetector.cpp`,
`SystemInfoHolder.cpp`, plus a new low-level driver `MCU/Component/OmapL108Syscfg.cpp`
that fills `OmapL108Spi.cpp`'s old slot in the link order.

Every other K1 anchor (`CryptoAt88.cpp`, `I2cByGpio.cpp`, `MCU/OmapL108.cpp`,
`MCU/Component/OmapL137Mcasp.cpp`, `MCU/Component/OmapL137Usbdc.cpp`,
`EvaBoardMain.cpp`, `cobjectmgr.cpp`, `CDix4192.cpp`, `clcdc.cpp`) is still present in
K2's string table - the shared low-level driver layer is architecturally unchanged, and
is being validated/migrated into this tree separately (see those files' own headers).

## 2026-07-18 — new panel-scan architecture, first pass

Reconstructed the 5 genuinely-new files. **Real methodology change from K1's own
2026-07-18 pass, flagged prominently in every file below**: the pre-fetched Ghidra
static dump for K2 (`all_decompiled_k2.json`, 581 functions) does not cover any of the
address ranges these 5 files' code actually lives in, despite those addresses (e.g.
`0xc0005784-0xc0005978`, `0xc0006eec-0xc0007018`, `0xc0007158-0xc00071d8`,
`0xc00097ac-0xc0009830`, `0xc0001b40-0xc0001c3c`) sitting well inside the dump's own
nominal `0xc0000000-0xc001b794` coverage span. Ghidra's auto-analysis for this
throwaway K2 import evidently never turned that code into "Function" objects - a
different failure mode from K1's own "constant-propagation false negative" anchor
issue (cpsoc.cpp's case, see K1_V06R06/README.md) - this time the code just isn't
function-ified at all in the export, not merely mis-attributed.

Worked around it with a genuinely different technique: `capstone` (ARM32, no Thumb
observed anywhere in this cluster) disassembly of the raw wrapped-ELF image
(`kronos2s_v01r10_panel.elf`, single `PT_LOAD`, file offset `0x54` = vaddr
`0xC0000000`), located by byte-pattern search for the 4-byte-aligned string-pointer
values rather than Ghidra's own function/xref graph. This recovers real, faithful
register-level transcriptions, but WITHOUT a decompiler's type inference, SSA, or
control-flow cleanup - every function below is hand-derived from raw ARM disassembly,
not a Ghidra decompile. Flagged as lower-confidence than every prior file in this
project's history; each file's own header repeats this note.

| Subsystem | Real source file (per embedded `__FILE__` strings) | Status |
|---|---|---|
| Switch/knob/LED naming + LED bit-level state | `../PanelManager.cpp` | `panel_manager.c` — anchor + 77-entry switch-name table + 2 real functions (LED bit set/clear, 16-bit bulk-apply), see below |
| Panel-scan-system (PSoC-successor) firmware update | `../PanelScanUpdater.cpp` | `panel_scan_updater.c` — anchor + top-level update sequencer transcribed, sub-steps cited as extern, see below |
| Switch debounce state machine | `../SwitchOnChatteringDetector.cpp` | `switch_chattering_detector.c` — anchor + register/remove functions transcribed, one real cross-function discrepancy flagged, see below |
| System version/health-status display | `../SystemInfoHolder.cpp` | `system_info_holder.c` — anchor + one status-display function transcribed (attribution partly content-based, not purely positional — see file header), see below |
| SoC pin-mux / module reset-enable (shared driver tier, grouped here because it's new) | `../MCU/Component/OmapL108Syscfg.cpp` | `omap_l108_syscfg.c` — anchor with a REAL confirmed xref (highest-confidence file this pass) + 6 real functions, see below |

Not touched this pass, out of this pass's lane (see `CLAUDE.md`/task-level scoping):
the shared low-level driver layer (`crypto_at88.c`, `i2c_by_gpio.c`, `omap_l108.c` (not
yet present), `mcasp.c`, `omap_l137_usbdc.c`, `eva_board_main.c` (not yet present),
`cobjectmgr.c` (not yet present), `cdix4192.c`, `clcdc.c` (not yet present)) — being
migrated/validated from K1_V06R06 into this tree by a concurrent pass. Files already
present from that pass as of this writing: `mcasp.c`, `crypto_at88.c`, `i2c_by_gpio.c`,
`omap_l137_usbdc.c`, `cdix4192.c`.

### `panel_manager.c` — status: first pass done, real functions, deep coverage open

Consolidates what K1 split across `cpsoc.cpp` (switch/LED names + diagnostic screen)
and `cad.cpp` (knob/slider/pedal names) into one file — a genuine, confirmed
architectural merge, not a guess: the 77-entry switch/LED name pointer table
(`0xc0005784-0xc00058b4`, directly comparable to K1 cpsoc.c's own 73-entry table) and
a *separate*, not-yet-table-located 24-entry knob/slider/pedal name string group both
sit in this file's own `.rodata` neighborhood. Two real functions transcribed:
`panel_manager_set_led_bit()` (single-bit set/clear into a packed 16-bit register,
addressed via a 73-entry `(word_slot, bit_pos)` lookup table at `0xc0027460`) and
`panel_manager_apply_led_group()` (a 16-bit-mask bulk-apply wrapper around the former).
**Still open**: no caller located for either function; the full 73-entry bit-position
table wasn't transcribed byte-for-byte (only its first 10 entries, establishing the
pattern); two more `PanelManager.cpp` anchor xrefs (`0xc00061cc`, `0xc00066f8`) weren't
disassembled — very likely the diagnostic-menu screen (K1 cpsoc.c's `FUN_c0008618`
equivalent). NEEDS LIVE QUERY: `0xc0027460` full table, `0xc00061cc`, `0xc00066f8`.

### `panel_scan_updater.c` — status: first pass done, top-level sequencer transcribed

Direct architectural descendant of a specific K1 *feature* (PSoC field-firmware-update,
mixed into `cpsoc.cpp` in K1 - see `docs/modules/KRONOS_V06R06.VSB.md`'s "Two-tier
control system" section), now split into its own dedicated file — confirmed by its own
strings: `"Update panel scan system."` / `"->Now writing..."` / `"->->Completed. Tern
power off."` (a literal firmware typo, "Tern" for "Turn" — transcribed faithfully, same
as K1's own `"Tepo"`/"Tempo" typo). `panel_scan_updater_run()` (`0xc0006ee8`) fully
transcribed as a 5-parameter top-level sequencer: prints status text, runs
erase/write/verify/apply sub-steps (all four cited `extern`, not transcribed — too
dense for hand disassembly this pass), checks a version/revision byte pair against
caller-supplied expected values, faults (`panel_fault`, line `0x53`) on mismatch, prints
completion text as a tail call. **Still open**: the four sub-step callees; no caller
located; K1's own `"Psoc version error %02x != %02x : Id %03d"` string is NOT present
in K2's string table at all — either renamed/removed or outside the ranges swept this
pass, itself worth a future look. NEEDS LIVE QUERY: `0xc0006dc4`, `0xc0006d28`,
`0xc0006c5c`, `0xc0006aa8` (the 4 sub-steps), and this function's real caller.

### `switch_chattering_detector.c` — status: first pass done, real FSM, one open discrepancy

The strongest "genuinely new, not just renamed" evidence among the 5 files: K1 never
had a standalone debounce module. Two functions transcribed —
`switch_chattering_register()` (`0xc00070c0`) and `switch_chattering_remove()`
(`0xc0007158`) — implementing a real 3-state (0=idle, 1=actively-debouncing,
2=confirmed) per-switch state machine with a doubly-linked list for active entries,
each with its own `panel_fault()` assert call citing this file's own anchor (lines
`0x66`/`0x84`). **Confirmed, deliberately unreconciled discrepancy**: `register()`
indexes its list-node array by a running pool counter (`*(base+4)`); `remove()` indexes
what looks like the same array directly by switch index — flagged rather than smoothed
over, per this project's established convention of recording cross-function
inconsistencies rather than guessing which side is "right." NEEDS LIVE QUERY: a real
Ghidra decompile of both functions would likely resolve this immediately; also no
caller located for either function, and `0xc0006fb0` (a plausible init/constructor
candidate, field-offset-compatible with both functions here) is unattributed between
this file and `panel_scan_updater.c` — flagged identically in both.

### `system_info_holder.c` — status: first pass done, one function, attribution caveat

One function transcribed, `system_info_holder_print_status()` (`0xc00097ac`) — prints
`"SyncErrorCount %d"` and `"Panel Scanner Version:%02d Revision:%02d"` (itself a rename
of K1's `"Panel Scan System Version:%02d Revision:%02d"`) to screen. **Important
caveat, stated up front rather than buried**: this function's home in `.rodata` sits
positionally just AFTER `cobjectmgr.cpp`'s own anchor and just BEFORE
`SystemInfoHolder.cpp`'s — per the K1-established "strings after a file's anchor belong
to that file" convention, this content is nominally `cobjectmgr.cpp`'s own string pool,
not `SystemInfoHolder.cpp`'s. Placed in this file on functional/semantic grounds (a
version/health-status report fits "SystemInfoHolder" far better than a generic object
manager), not positional proof. Also open: the printf-style callee's real 4-argument
signature wasn't fully modeled (a simplified stand-in was used, flagged in-file); the
`"SYSTEM STARTUP FAILED"` fault-screen text cluster in the same neighborhood
(`0xc0009b0c`) wasn't disassembled. NEEDS LIVE QUERY: `0xc0009b0c`,
`0xc000a7c8` (this file's own anchor xref, also not disassembled).

### `omap_l108_syscfg.c` — status: highest confidence this pass, real confirmed anchor xref

Unlike the other 4 files, this one's anchor string has a directly-confirmed real xref
(`0xc0001bc8`, an actual `panel_fault()` call site citing this exact filename and line
`0x51`) — not inferred from content or neighborhood. Six real functions transcribed:
four fixed-constant register-write leaves (`omap_syscfg_set_reg118/154/130_a/130_b`,
all recognizable TI PINMUX-style nibble-per-field encodings), one genuine
reset/hold/deassert/configure/poll-with-timeout peripheral bring-up sequence
(`omap_syscfg_reset_and_enable`, 999999-iteration timeout, faults via this file's own
anchor on timeout), and one dual-register bit-set leaf
(`omap_syscfg_set_dual_pull_enable`). Architecturally, this file fills the exact link-
order slot K1's `OmapL108Spi.cpp` (the shared cpsoc/cad SPI driver) used to occupy —
consistent with the SPI bus's only two K1 consumers (`cpsoc.cpp`, `cad.cpp`) both being
gone in K2. **Still open**: no caller located; the real peripheral base address/register
map wasn't cross-checked against a TI OMAP-L108/L138 TRM; only a `0xc0001b40-0xc0001c3c`
cluster was swept — the file's true compilation-unit boundary (where `OmapL108.cpp`
ends and `OmapL137Mcasp.cpp` begins) wasn't independently established.

## 2026-07-19 — generic infra migration: `soc_periph.c`, `omap_gpio.c`, `heap_alloc.c`, `task_sched.c`

Migrated K1's four remaining generic-infra files into K2. Ground truth: static Ghidra
decompile dump of `KRONOS2S_V01R10.VSB` (`all_decompiled_k2.json`/`all_data_k2.json`,
`query_dump_k2.py`) — no live Ghidra MCP calls this pass. Every function below was
independently re-matched against K2's own decompile (not assumed to sit at the same
address) and every cited constant/address was independently re-resolved from K2's own
`DAT_` literal pool, not copied from K1's citations.

### `soc_periph.c` — status: done, near-identical table + real, documented differences

The peripheral base-address table ports over almost unchanged: 11 of K1's 13 table
entries carry over with identical or TRM-consistent values (Timer64P0, SYSCFG0, PSC1,
EDMA3CC0/1, EDMA3TC0/1/2, McASP0 base+FIFO, LCDC, eCAP1, USB0/OTG, AEMIF CS3), plus the
`gpio_bank_get_base`/`hw_timer_busy_wait` pair `i2c_by_gpio.c` already cited `extern` —
both now defined here, closing that file's own forward reference.

**Real, confirmed differences from K1, not transcription artifacts:**
- **Two brand-new table entries** with no K1 counterpart: an I2C0/I2C1 base selector
  (`i2c0_i2c1_base_select`, real caller confirmed) and an eCAP2 base getter.
- **"SPI survives"** — a genuinely interesting finding given cpsoc.cpp/cad.cpp (SPI's
  only two K1 consumers) are both gone in K2: the SPI0 base accessor is NOT dead code.
  Its real callers are `FUN_c0004f70` and, concretely, `panel_scan_updater_run()`
  (`panel_scan_updater.c`) — SPI's *consumer* changed, not its presence. K1's own SPI1
  selector arm has no confirmed K2 counterpart (only a single-value SPI0 getter exists).
- **"UART shrinks"** — K1's 3-way `uart_base_select` has no confirmed K2 counterpart;
  K2 has only a single, no-parameter UART1-only accessor. Whether K2 dropped the other
  two instances' own bring-up or moved it elsewhere is open.
- **Pinmux writes merged**: K1's three separate one-word pinmux leaves
  (`+0x130`/`+0x130`/`+0x154`) become ONE K2 function writing three consecutive words
  (`+0x110`/`+0x114`/`+0x118`) in a single call — different offsets too, not just merged.
- **Two `psc_module_enable`-shaped functions confirmed present**, matching K1's own
  architecture exactly (K1 had a zero-caller one it reconstructed, plus an `extern`-only
  sibling — `gpio_psc_enable` — it never actually defined). K2's own dump happens to
  cover BOTH as real function objects; the sibling is now defined for the first time in
  this project, in `omap_gpio.c`.
- `board_desc_init_type4` (K1's caller-length descriptor-constructor sibling) and the
  Timer64P1 lazy-init singleton have no confirmed K2 counterpart — open, not forced.

**Still open**: NEEDS LIVE QUERY on K2's UART0/UART2 base story if it matters later;
`board_desc_set_pinmux_3word`'s own field semantics; `psc_module_enable`'s zero-caller
status (same open shape as every other "zero callers" finding in this project).

### `omap_gpio.c` — status: done, real architectural difference from K1

**Headline finding**: K1's generic pair-indexed GPIO register layer
(`gpio_reg_read_in`/`_set_bit`/`_clear_bit`, `bank_base + pair*0x28 + offset`) is GONE
in K2 — confirmed by a full-image search of every K2 function's decompiled text for
that `* 0x28` stride pattern: one unrelated false positive, zero real hits. Every
GPIO-touching K2 leaf found in the equivalent address cluster instead hardcodes one
fixed bit at one fixed byte offset directly, with no pair-index indirection at all —
a real, confirmed simplification/inlining, not a coverage gap.

Two functions are still confirmed identical in ROLE to K1 (`gpio_bank_set_dir_bit`/
`gpio_bank_read_sda_bit`, both already declared `extern` by `i2c_by_gpio.c`) but even
these read/write their fixed SDA bit directly rather than through any pair-indexed
primitive. `omap_psc_enable_module_0x10` and its callee (`gpio_psc_enable`, K1's own
never-defined `extern`) both port over with identical mechanism to K1.
`gpio_bank_hw_init`'s K2 counterpart is real but a genuine SUBSET of K1's own — it
writes BINTEN and one bit-5 edge-trigger pair (both numerically identical to K1) but
omits all of K1's DIR/OUT_DATA pair-1/3/4 default writes entirely; whether those moved
elsewhere or were dropped (consistent with cpsoc/cad's own removal freeing up those
pins) is not resolved. Several further thin bit-twiddle leaves (matched-pair field
selectors at `+0x90`/`+0x94` and `+0xb8`/`+0xbc`) have no K1 counterpart at all.

**Still open**: no caller was traced past one level for any function in this file
(explicit time-budget tradeoff, prioritizing breadth across all 4 files this pass);
whether K1's missing DIR/OUT_DATA defaults exist elsewhere in K2 — NEEDS LIVE QUERY.

### `heap_alloc.c` — status: done, confirmed essentially unchanged

The dlmalloc-derivative shape carries over completely unchanged. `heap_sbrk`/`heap_trim`
are confirmed statement-for-statement identical to K1 (same rounding, same ENOMEM path,
same "sbrk-of-negative-amount failed, re-derive top-chunk size from the real break"
fallback). `heap_lock`/`heap_unlock` are confirmed empty bodies, same as K1.
`heap_malloc`'s opening rounding/small-bin-fast-path/treebin-ladder logic (the only part
spot-checked, per this project's own established practice for this function) is
identical down to the same shift constants (9/6/12/15/18) and additive offsets
(0x38/0x5b/0x6e/0x77/0x7c/0x7e) K1's own header catalogues. `heap_free` is confirmed to
be `heap_trim`'s sole call site, exactly matching K1's own claim.

**One open, explicitly out-of-scope cross-reference**: K2's `heap_errno` global address
is ALSO independently zeroed by three unrelated K2 functions elsewhere in the image —
not folded into this file's own scope on weak evidence, flagged honestly instead.

**Still open**: `heap_state`'s full struct layout beyond what `heap_trim`'s own body
confirms; whether K2's heap base/end constants imply a differently-sized heap region
than K1's (NEEDS LIVE QUERY if it matters).

### `task_sched.c` — status: done, HEADLINE FINDING: same RTOS architecture as K1

K2 kept the exact same task-scheduler architecture as K1 — TCB table + priority
ready-queue/bitmap dispatcher, the kernel-object (event-flag-group) table and its
set/clear/wait API, and the tick-ordered delay/timeout min-heap those primitives block
into — all present, at new addresses, with (where checked field-by-field) IDENTICAL
branch shapes, error codes (`-25`/`-18`/`-17`/`-28`), and struct-offset conventions to
K1. Located via known-good-anchor evidence rather than a raw address sweep: started
from `eva_board_main.c`'s own already-confirmed WFI/dispatch-tail globals inside
`eva_board_crt0` (that file's own header explicitly left the tail "not transcribed
further" and `eva_board_start_task`'s own K2 fate "no confirmed counterpart at all"),
then swept the whole K2 image for every other function referencing those same resolved
data addresses — found the entire rest of the cluster in one pass.

**Resolves `eva_board_main.c`'s own open question, without editing that file**: K2 auto-
starts boot-time tasks via a ROM table walk in `eva_board_crt0`
(`sched_tcb_table_init_and_autostart` → `sched_task_create_and_ready` →
`sched_make_ready`), exactly like K1's own headline finding for its own scheduler — NOT
via an `eva_board_start_task`-shaped explicit call from `eva_board_main`'s own body.
That's the concrete, confirmed reason no K2 counterpart to that specific call shape was
ever going to be found there.

**A second, real headline finding**: the general-purpose `sched_dispatch` (found at a
separate, dedicated address, called from `kobj_eventflag_wait`) is CONFIRMED to differ
from `eva_board_crt0`'s own inlined copy of the same ready-scan/WFI tail — the
general-purpose version has an extra "save the outgoing task's context" header the
crt0-inlined copy omits (crt0 has no real "outgoing" task on cold boot). K1's own
`task_sched.c` asserted these two tails were "byte-for-byte identical" without drawing
this distinction — worth a note back to K1's own file in a future consolidation pass
(not done here, out of this pass's own file-scope boundaries).

**One real, confirmed behavioral difference from K1, transcribed rather than smoothed
over**: K2's `sched_wait_list_insert` unconditionally calls `sched_remove_from_ready` on
the blocking task before inserting it into the wait list — K1's own version of this
function does NOT do this (only the separate TIMED variant did, in K1's own
architecture). Left genuinely open which of two explanations is correct (K2 merged the
timed/untimed paths, or this K2 function is actually the timed variant under an
unexpected name) — not forced into either story.

**Still open**: `eva_board_sched_ready` (K1's own "already-active, reprioritize" richer
ready-insert) and `eva_board_sched_requeue` have no confirmed K2 counterpart — NEEDS
LIVE QUERY if a future pass needs K2's reprioritize-while-waiting behavior. The delay-
heap sift-up/sift-down internals were not individually located, only the extract-min
entry point. `sched_tcb_table_init_and_autostart`'s own ROM table contents (whether
`eva_board_main` itself is one of the auto-started entries) would need a live
`read_memory` query, same open item K1's own file left for its own equivalent table.

## 2026-07-19 — IRQ-gate leaf cluster + second I2C bus: `soc_irq_gate.c`, `panelbus_dispatch.c`

Migrated K1's remaining two infra files. Ground truth: `all_decompiled_k2.json`/
`all_data_k2.json` via `query_dump_k2.py`, no live Ghidra MCP calls this pass. Full
detail (per-cluster K1/K2 address citations, DAT_ resolution, cross-file naming
reconciliation with `mcasp.c`'s `mcasp_reinit_reduced` and `omap_gpio.c`'s
`gpio_bank_hw_init`) lives in each file's own header comment — see
[`docs/modules/KRONOS2S_V01R10.VSB.md`](../../docs/modules/KRONOS2S_V01R10.VSB.md)'s
own "IRQ-gate and second-I2C-bus infrastructure" section for a narrative summary.

### `soc_irq_gate.c` — status: near-identical port, real documented differences

Located by a full-image sweep for every K2 function calling `aintc_base()` directly (14
hits, 11 of which are genuine AINTC channel-gate leaves). All 11 leaves, plus both
"group" enable/disable dispatchers (`soc_irq_gate_group_a_enable`/`_group_b_disable`,
found the same way K1's own file found its own "group A"/"group B"), are ported and
cluster-matched 1:1 against K1's file. **The shared bookkeeping table these leaves
reference resolves to the SAME fixed address as K1's own table, 0xC00E0000** —
independently re-derived from K2's own literal-pool arithmetic, not assumed — strong
evidence it's a fixed OMAP-L138/DA850 physical SRAM location, not firmware-relative
data (K2's own table is smaller, 0xC00E0000-0xC00E004C vs K1's -0x0068, because K2
genuinely dropped slots rather than shrinking offsets).

**Real, confirmed differences from K1:**
- Timer64P1 (channel 0x17) and channel 0x36 IRQ gating are **gone entirely** — a
  full-image literal sweep for either channel number written to EISR/EICR found zero
  matches, not a coverage gap.
- Channel 0x2a **now has a real enable side** — K1's own file explicitly left this
  "STILL OPEN" ("no matching enable stub found anywhere"). K2's group-A dispatcher
  inlines a genuine ch-0x2a EISR-enable in its own tail rather than as a standalone
  leaf — resolves K1's own open question, in the negative.
- Channel 0x32's disable path **re-arms its own enable-side guard** on every call
  (clears the same cached-pointer slot the enable side uses as its lazy-init guard) —
  K1's own version never touched that guard on the disable side.
- Channel 0x2a's disable-side GPIO acknowledgment write is **inlined** rather than
  routed through a dedicated `omap_gpio.c` helper the way K1's
  `gpio_pair0_intstat_ack_bit5` was.
- `soc_irq_gate_mcasp2_bringup`'s body is a **real subset** of K1's own 11-callee
  sequence (a different second base-getter call, one final GPIO write instead of
  three) — also independently confirms two items K1's own file left open:
  `FUN_c00019e0`/`FUN_c00019e8` really are McASP-instance base getters (now named
  `mcasp0_base_get`/`mcasp0_fifo_base_get` in `soc_periph.c`), and `mcasp_reinit_
  reduced` (`mcasp.c`'s own name for K1's `FUN_c0003228`) is this function's real
  sole caller.
- `midi_hw_write16`/`midi_hw_read16`'s own caller counts (130/53) are essentially
  identical to K1's independently-documented counts (129/53) — reconfirms this pair
  is genuinely shared, firmware-wide MMIO plumbing on both boards, not MIDI-exclusive.

**Still open**: `FUN_c0000040` (ch-0x15 enable) remains undefined anywhere in K2's
tree — belongs to `soc_periph.c`'s own territory per K1's convention, not filled there
yet either; K1's own "gap_slot_bringup" cluster (twice-called `usbdc_gap_config_slot`-
shaped bring-up) has no located K2 candidate this pass (no `omap_l137_addr_gap_misc.c`-
equivalent K2 file exists yet to cross-check against); `soc_irq_gate_slot0x00_get`/
`soc_irq_gate_ring3_state_reset` (K1's own cluster-11 tail items) not independently
re-located in K2 this pass. NEEDS LIVE QUERY: the gap_slot_bringup cluster if a future
pass wants to close it.

### `panelbus_dispatch.c` — status: HEADLINE FINDING, higher-level dispatcher absent in K2

Real, evidence-based architecture finding, not a coverage gap: K1's own per-tick I2C0/
I2C1 opcode dispatcher (RX poll loop, TX ring drain, the opcode-table command
interpreter — ~7 functions) is built entirely on top of two K1 subsystems, `cad.cpp`
(calibration) and `cpsoc.cpp`'s "third SPI device", BOTH of which this project's own
2026-07-18 anchor-string pass already confirmed are absent from K2's string table
entirely (replaced by the new `PanelManager.cpp`/`PanelScanUpdater.cpp`/
`SwitchOnChatteringDetector.cpp`/`SystemInfoHolder.cpp` architecture). This pass
independently confirms the hardware-level consequence: the underlying I2C0/I2C1 base
selector survives structurally unchanged, but it's **already fully defined in
`soc_periph.c`** as `i2c0_i2c1_base_select` — not redefined here. A full xref sweep of
its own `callers` list finds exactly **two call sites total**, both inside a single
crt0 hardware bring-up stub (`panelbus_hw_bringup`, reconstructed here for the first
time, resolving one of `eva_board_main.c`'s own 11 "not individually traced" crt0
calls) — no RX/TX/opcode dispatcher cluster anywhere in the covered dump. This closes
`soc_periph.c`'s own previously-open item ("`i2c0_i2c1_base_select`'s real consumption
beyond the one traced caller — not fully swept") in the negative.

**Real, confirmed detail**: `panelbus_hw_bringup` selects **I2C1** (idx=1) — the
OPPOSITE selector value from every one of K1's own panelbus_dispatch.c call sites,
which exclusively used I2C0 (idx=0). Whether this reflects a genuinely different
physical bus assignment on the K2 board or just a different, unrelated hardware
consumer of this shared selector is not resolved. `panelbus_hw_bringup`'s own real
decompile continues past its own Ghidra-assigned function boundary into an unbounded
code region (0xc00008d4-0xc0000904) — the same "Ghidra never resolved this boundary"
artifact this project has repeatedly documented elsewhere — not transcribed (no raw
disassembly access this pass).

**Still open**: whether K2's real panel-scan architecture uses this I2C0/I2C1 hardware
block at all for its own runtime traffic (most likely not — `PanelScanUpdater.cpp`'s
own confirmed bus is SPI, per `soc_periph.c`'s own "SPI SURVIVES" finding) or whether a
further consumer exists outside this static dump's own function-boundary coverage
(same known gap the panel-scan files themselves hit). NEEDS LIVE QUERY:
`panelbus_hw_bringup`'s own unbounded continuation (0xc00008d4-0xc0000904 raw
disassembly); a live full-image xref re-search for any other caller of
`i2c0_i2c1_base_select` beyond this one bring-up stub, which would settle the "does a
K2 dispatcher exist somewhere unswept" question definitively.

## 2026-07-19 — MIDI subsystem cluster: 7 files

Migrated K1's own USB-MIDI transport cluster (`midi_engine.c`, `chan_link_hw.c`,
`chan_param_ctrl.c`, `chan_slot_dispatch.c`, `usbdc_midi_status_glue.c`,
`uart1_midi_queue.c`, `midi_chan_status_queues.c`) into K2 — new files, all 7 added this
pass. Ground truth: `all_decompiled_k2.json`/`all_data_k2.json` via `query_dump_k2.py`,
no live Ghidra MCP calls. Full detail (per-file K1↔K2 function/address maps, every
independently-verified `DAT_` constant, per-file STILL OPEN lists) lives in each file's
own header comment; see `docs/modules/KRONOS2S_V01R10.VSB.md`'s own "MIDI subsystem
cluster" section for the narrative summary.

Located by walking outward from `soc_irq_gate.c`'s own already-committed CLUSTER 10
(`midi_hw_write16`/`_read16`/`_fifo_write`/`_fifo_read`) through every one of its own K2
xrefs — the whole 7-file cluster hangs off that one anchor.

**Headline finding**: this entire MIDI transport stack is essentially unchanged between
K1 and K2, just re-addressed — the overwhelming majority of functions in
`chan_link_hw.c` (17/17), `chan_slot_dispatch.c` (14/14), `chan_param_ctrl.c` (17/18),
`midi_engine.c` (24/26), `uart1_midi_queue.c` (5/5), and `midi_chan_status_queues.c`
(9/9) match at an *exact* Ghidra-reported byte size, and every numeric register/mask
constant independently re-resolved from K2's own `DAT_` pool is bit-for-bit identical to
K1's own values. `usbdc_midi_status_glue.c` is the one incomplete file (3/16 K1
functions located, honestly flagged rather than guessed) — a breadth-over-completeness
tradeoff this pass, with concrete leads left in that file's own header for closing it.

Real, confirmed K2-side differences (not transcription artifacts): three separate K1
globals (`midi_hw_mode_flags`/`chan_port_hwctx_global`/`chan_global_hi_mode_flags`)
merge into one K2 address (`0xC01CCD10`); `uart1_midi_queue.c` and
`midi_chan_status_queues.c` merge into one contiguous K2 address run (two files in K1,
one compilation unit in K2, though kept as two separate reconstructed files here to
match K1's own file-per-subsystem convention); `chan_link_rt_queue_push`'s own K2 role
is genuinely expanded/restructured (left as an unfilled `extern` rather than a
possibly-wrong 1:1 port); a brand-new function, `midi_ring1_push_zeros`, has no K1
counterpart at all; `uart1_tx_byte` gained a real new side effect (sets a drop-flag-
adjacent global) K1's own version never had.

## 2026-07-19 — `usbdc_core_isr` resolved + `usbdc_midi_status_glue.c` improved (live pass)

Follow-on same-day pass, using a dedicated single-session live Ghidra MCP bridge
connection (`get_disassembly`/`decompile_function`/`get_xrefs_to`/`read_memory`) rather
than the static-dump-only methodology every other pass in this file uses — authorized
once for this specific task under this project's own "2-agent cap, no further fan-out"
constraint (see `CLAUDE.md`). Two real gaps closed, zero Agent-tool subagent calls made.

**`omap_l137_usbdc_ext.c`'s own Section 6 (`usbdc_core_isr`) — RESOLVED.** This was the
single largest confirmed data gap left anywhere in the K2 shared-driver-layer migration:
a real, 1940-byte hole in K2's function list (`0xc0003840`-`0xc0003fd4`) that neither the
static dump nor a live `decompile_function`/`get_function_info` call could bound as a
Ghidra Function object (same "no function found" artifact this project has hit before,
e.g. K1's own `eva_board_main.c` crt0 chain) — manually transcribed instruction-by-
instruction from `get_disassembly`, per this project's own established technique for
similarly unbounded regions. The function is a near-exact structural/semantic match to
K1's own `usbdc_core_isr`, with 6 real, independently-confirmed differences (a real
`usbdc_desc_arm_slot(dev,1,1)` call resolving one of K1's own guessed phantom-forward
arguments; a genuinely different confirmed register offset for the EP1-ready branch's CSR
field, 0x516 vs K1's 0x462; a harmless dead intermediate dereference ahead of the 3 boot-
flag writes; 32-bit vs byte access on the EP0 context nibble-clear field; a collapsed
single-literal load for `usbdc_setup_dispatch_buf`; and compiler tail-merging of 3
no-dispatch event cases into one shared code fragment) — full detail in the function's
own header comment. Incidentally also confirmed real K2 addresses for 5 of Section 3's 7
still-unbodied DMA/descriptor-table functions via their own direct `bl` targets inside
`usbdc_core_isr`'s bus-reset branch (bodies still not transcribed, out of this pass's own
scope).

**`usbdc_midi_status_glue.c` improved from 3/16 to 5/16.** `chan_irq_toggle` (K2
`FUN_c00087d8`) found via `get_xrefs_to` on the already-known `usbdc_ep_irqmask_set`/
`_clear` pair; `chan_ring_drain_pack` (K2 `FUN_c0008b04`, exact caller-count match to K1's
own single caller) found by following `chan_slot_dispatch.c`'s own already-identified K2
port-interrupt dispatcher one hop to its callee. A real, confirmed K2-only addition found
in `chan_ring_drain_pack`: its ring-counter read/decrement are now bracketed in IRQ-guard-
shaped calls K1's own reconstructed C shows no trace of. A candidate false lead
(`FUN_c00084f4`) was checked and explicitly rejected in the file rather than forced.
Still genuinely open (5 items, unchanged search leads from the prior pass's header):
`chan_status_promote_on_flag`, `chan_maybe_enable_irq4`, `chan_status_notify`,
`chan_status_byte_msb`, `chan_ring_entry_clear_0..3` quartet.

See `docs/modules/KRONOS2S_V01R10.VSB.md`'s own "`usbdc_core_isr` resolved +
`usbdc_midi_status_glue.c` improved" section for the narrative summary, and each file's
own header comment for full citation detail.

## 2026-07-19 — MIDI cluster deep-dive: `usbdc_midi_status_glue.c` to 12/16, `chan_link_rt_queue_push` and `midi_hw_flush_alt` resolved (live pass)

Second same-day follow-on live Ghidra MCP pass (same dedicated single-session
bridge, same "2-agent cap, no further fan-out" authorization), targeting the
three concrete open items the MIDI-subsystem cluster pass and its first
live follow-on both left behind. Zero Agent-tool subagent calls made.

**`usbdc_midi_status_glue.c` improved from 5/16 to 12/16.** `search_bytes` on
`chan_status_obj`'s (`0xC01CB344`) and `chan_ring_obj`'s (`0xC01CBA5C`) own
resolved literal byte patterns located 7 more functions: `chan_maybe_enable_
irq4` (K2 `FUN_c0008b98`), `chan_status_notify` (K2 `FUN_c0008be8`),
`chan_status_byte_msb` (K2 `FUN_c0008c14`), and the full `chan_ring_entry_
clear_0..3` quartet (K2 `FUN_c0008c3c`/`_c48`/`_c54`/`_c60`), the last three
found sitting immediately after `chan_ring_drain_pack`'s own body and before
`usbdc_ep_regblock_ptr_a`. Real, confirmed differences from K1: `chan_status_
byte_msb`'s boundary test collapses to `b >= 0x7f`, not K1's `(b & 0x80) !=
0` (`b >= 0x80`) — a genuine one-value threshold shift, verified by hand
enumeration; `chan_ring_entry_clear_1/_2/_3` (offsets `+0x10`/`+0x1c`/`+0x28`)
are now IRQ-guarded (same `irq_guard_enter_k2`/`_exit_k2` pair
`chan_ring_drain_pack` already uses) while `clear_0` (`+4`) stays bare, an
asymmetric addition K1 never had; caller grouping by offset parity (0/2 from
one caller, 1/3 from another) is confirmed IDENTICAL in shape to K1's own,
just at new K2 caller addresses. Only `chan_status_promote_on_flag` remains
open — its own K1 body never touches either object this pass's two sweeps
searched for, so a byte-search couldn't have found it by construction; a
concrete lead (`chan_dispatch_probe`'s own K2 body, `FUN_c00117c8`) is left
for a future pass. This pass's own header also documents two live-bridge tool
quirks worth knowing for future sessions: `search_bytes` returns an opaque
error rather than a real zero-match result when a pattern has no hits at all
(caught via a digit-transposition typo), and `decompile_function` on an
unbounded/non-function address can silently return a DIFFERENT, unrelated
real function's body instead of erroring.

**`chan_param_ctrl.c`'s `chan_link_rt_queue_push` RESOLVED as a real K2-side
restructuring, not a 1:1 port.** `decompile_function` directly on `FUN_c000ea68`
(356 bytes) succeeded on the first attempt. K1's own version is a small,
self-contained "push one realtime byte into a 64-slot ring" primitive
(one incoming byte parameter). K2's version takes no incoming byte at all —
it now POPS its own internal realtime ring (a new read-index field at
`link+0x139`, immediately before the pending-count field at `link+0x13a`
K1 already had), packages the popped byte as a 4-byte USB-MIDI Realtime CIN
frame (`{0x0f, byte, 0, 0}`) and transmits it, THEN separately drains ring 1
when it has pending data and arms/re-arms two IRQ-enable lines (3 and 4)
based on two independent readiness checks, finally conditionally
acknowledging the link — a combined "realtime service tick" that subsumes
the old push primitive's role. Full real body now transcribed in the file
(previously left as an unfilled `extern`). Two of its own callees' `which`/
`enable` arguments to `chan_irq_toggle` could not be recovered from this
call site's own decompile (only 2 of 4 arguments visible) — transcribed
honestly as unverified placeholders rather than invented, per this project's
own convention.

**`midi_engine.c`'s `midi_hw_flush_alt` RESOLVED; `midi_hw_flush_notify`
still open.** `get_disassembly` on `midi_stream_decode_step`'s own
"maybe_flush" tail found the real `bl` target (`0xc000d5b4`) for a call this
file already knew about but had never resolved an address for.
`decompile_function` on that address shows a bare, unconditional one-line
forwarder to `chan_link_ack` (`chan_param_ctrl.c`'s `FUN_c000d564`) — no
independent flush logic of its own survives in K2. `midi_hw_flush_notify`
was NOT found: `get_xrefs_to` on `chan_link_ack` returns exactly 6 real
callers, and the two not otherwise already accounted for
(`FUN_c000a308`/`FUN_c000ef8c`) both have real callers of their own (i.e.
neither is a bare thin-forwarder the way `midi_hw_flush_alt` is), so there's
no positive evidence pointing at either — left open rather than guessed.

Compile status re-verified clean, 33/33, after all edits above.

## 2026-07-19 — `cpsoc.cpp`-adjacent stragglers: `cdix_autoswitch.c`, `omap_l137_addr_gap_misc.c`

Migrated K1's last three `cpsoc.cpp`-adjacent files (`clcdc_test_dispatch.c`,
`cdix_autoswitch.c`, `omap_l137_addr_gap_misc.c`). Ground truth: `all_decompiled_k2.json`/
`all_data_k2.json` via `query_dump_k2.py`, cross-checked with a raw capstone/byte-pattern
static search of `kronos2s_v01r10_panel.elf` (reusing the panel-scan-cluster pass's own
`k2disasm.py` helper) for two items the function-only dump doesn't cover — no live Ghidra
MCP calls this pass. Full detail (per-cluster K1↔K2 address maps, every independently
re-resolved `DAT_` constant, per-file STILL OPEN lists) lives in each file's own header
comment; see `docs/modules/KRONOS2S_V01R10.VSB.md`'s own "`cpsoc.cpp`-adjacent stragglers"
section for the narrative summary.

**`clcdc_test_dispatch.c` — CONFIRMED ABSENT, no K2 file created.** K1's factory
test-menu keypress dispatcher depended entirely on `cpsoc.cpp`'s own scratch-struct
offset (`0x821`) and `cpsoc_led_set`/`_clear` — both confirmed gone along with the rest
of `cpsoc.cpp`. Positive evidence, not just absence-of-anchor: `clcdc_test_pattern`'s own
K2 port (`clcdc.c`) has exactly one caller in the whole static dump, and it's
tick/counter-driven, not keypress-driven; raw disassembly of `PanelManager.cpp`'s own two
previously-uninvestigated anchor xrefs (`0xc00061cc`/`0xc00066f8`, left open by the
2026-07-18 panel-scan pass) independently rules both out as a relocated dispatcher — one
is a switch/knob scan-event encoder, the other a MIDI-status-byte dispatcher, neither
shaped anything like K1's key-range switch.

**`cdix_autoswitch.c` — ported cleanly**, all 3 functions EXACT byte-size matches
against K1 (20/20, 76/76, 120/120 bytes), no logic differences. New finding: K2's own
outer CDIX auto-switch state machine (directly covered in K2's dump, unlike K1's own,
which this project could only trace by address) also toggles `omap_l137_addr_gap_misc.c`'s
own McASP2 bit-flag helpers around the CDIX reset call — a real, previously-undocumented
hardware coupling. Also surfaces a shared `0xC00E004C` context-handle constant used by
this cluster, `mcasp_init` (`mcasp.c`), AND `clcdc_display_object_init`
(`omap_l137_addr_gap_misc.c`) — sitting inside the same fixed SRAM page `soc_irq_gate.c`
already confirmed as real OMAP-L138/DA850 hardware.

**`omap_l137_addr_gap_misc.c` — 5 of 6 K1 clusters ported, 1 confirmed absent.** Clusters
1 (McASP2 reduced-reinit) and 6 (struct-zero-init) are EXACT size matches AND directly
confirmed by `mcasp.c`'s own K2 port, which already cited both functions
(`mcasp_reinit_reduced`/`mcasp_clock_step_a`) but deliberately left them bodyless pending
this file — the same collision-avoidance split K1 used between its own two files.
Cluster 2 (`usbdc_gap_config_slot`) ports structurally identical, subsystem still
unattributed. Cluster 3 (the UART-shaped register-pair) is **CONFIRMED ABSENT** — a
full-image raw byte search for all four of K1's distinctive literal immediates found
zero occurrences anywhere in K2's binary, code or data, not just a coverage gap. Cluster
4 (two tiny bit-extraction helpers) ports over plus one real K2-only addition
(`gap_store_low_byte`). Cluster 5 (default RGB565 palette loader) ports over and
CROSS-BUILD CONFIRMS a K1 open question: the suspicious `0x752ff`-iteration remap-loop
bound resolves to the identical value in K2's independently-compiled image, strong
evidence it's a genuine compiled constant rather than a per-build Ghidra misinference.

**2026-07-19 — last two boundary-gap candidates closed.** Full Ghidra auto-analysis plus
manual `CreateFunctionCmd`/`DisassembleCommand` resolved both remaining "uncontained call
site" candidates in `omap_l137_addr_gap_misc.c`:
- Cluster 2's second caller (call site `0xc00008fc`) is a real function, `0xc00008cc`
  (52 bytes), never bounded by either the `-noanalysis` sweep or full auto-analysis. It is a
  structural sibling of three OTHER functions (`FUN_c0000800`/`FUN_c0000864`/`FUN_c000094c`)
  that each have exactly one confirmed, reachable caller — but this one has NONE anywhere
  (zero xrefs, zero raw-byte literal hits across the full 917504-byte image): a confirmed,
  exhaustive dead end, same category as `omap_l108_syscfg.c`'s orphans. Full body given the
  name `panelbus_hw_bringup_unreached` in `panelbus_dispatch.c` (its other two calls are that
  file's own primitives).
- Cluster 4's sole caller (call site `0xc00005b8`) turned out to have never even been
  disassembled by Ghidra at all — a genuine gap in raw Instruction coverage, not just Function
  boundaries. Once created (`0xc0000594`, 0x84 bytes), it decompiles as a real state-machine
  handler with a CONFIRMED real caller: a small, previously-untyped function-pointer/state-id
  table at `0xc002a648` (`{ptr, state_id}` rows), which is where the raw-byte-search technique
  paid off in the opposite direction from Cluster 2's case.

This same investigation also caught and fixed a real cross-file bug: `panelbus_dispatch.c`
had previously (before the `0xc00008cc` boundary existed) mis-transcribed that function's
body as extra statements appended onto `panelbus_hw_bringup`'s own, and separately claimed
`i2c0_i2c1_base_select` had only one caller in the whole image. Both are corrected in that
file now — see its own updated header comments for the full account. The overall
architectural conclusion (no live K2 dispatcher consumes this I2C hardware beyond boot-time
bring-up) is unchanged; the previously-miscounted second caller turned out to be dead code,
not a hidden runtime consumer.

## Shared-driver-layer migration: substantially complete

As of this pass, every K1 file outside the four confirmed-obsolete `cpsoc.cpp`/`cad.cpp`/
`cpsoc_issp.cpp`/`ctouchpanel.cpp`-adjacent files has now been either ported into this
tree or explicitly confirmed absent with documented evidence (not merely un-investigated).
Remaining work going forward is depth — table contents and other NEEDS LIVE QUERY items
scattered across individual files' own headers — not breadth: there is no longer a known
K1 file with zero K2 investigation.

## Compile status

`make` (via `arm-none-eabi-gcc -fsyntax-only`, same gate as `K1_V06R06/`) passes clean
for all files in this tree as of this pass (33/33, including the concurrent shared-
driver-layer migration's files, the 2026-07-19 IRQ-gate/panelbus pass, the 7-file MIDI
subsystem cluster, `cdix_autoswitch.c`/`omap_l137_addr_gap_misc.c`, and the same-day
live-pass follow-on that resolved `usbdc_core_isr` and improved `usbdc_midi_status_glue.c`
to 5/16), plus this pass's own live-Ghidra follow-up on the 5-file panel-scan cluster below.

## 2026-07-19 — live Ghidra MCP follow-up on the panel-scan cluster (5 files)

The 2026-07-18 panel-scan pass above was done with NO live Ghidra access (concurrency
policy at the time). This pass had one-agent-only, read-only live Ghidra MCP access
(`get_function_info`, `decompile_function`, `get_disassembly`, `get_xrefs_to`,
`read_memory` — no `load_binary`, no mutating calls) against
`kronos2s_v01r10_panel.elf`, and used it to resolve nearly every NEEDS LIVE QUERY item
the prior pass left open across `panel_manager.c`, `panel_scan_updater.c`,
`switch_chattering_detector.c`, `system_info_holder.c`, and `omap_l108_syscfg.c`.

**Headline discovery**: the live Ghidra database has substantially more Function
objects than the pre-fetched 581-function static dump (`all_decompiled_k2.json`) the
2026-07-18 pass was limited to — several addresses that pass's own header called
"never function-ified, no decompile available" turned out to have real Ghidra
decompiles after all once queried live.

**`panel_manager.c`**: the full 73-entry LED bitmap table (`0xc0027460`) is now
transcribed byte-exact via `read_memory` — the prior pass's `0xff`-sentinel theory was
wrong, every entry is a real, reachable `(word_slot, bit_pos)` pair. The 77th
name-table entry is confirmed literal blank padding. Both previously-uninvestigated
anchor xrefs resolved: `0xc00061cc` is a literal-pool slot inside a real function at
`0xc00061d4` (`panel_manager_encode_scan_event`, a switch/knob debounced scan-read
encoder — independently reconfirming, without re-deriving, the characterization the
2026-07-19 `cpsoc.cpp`-adjacent-stragglers pass already recorded for this same address
but never folded back into this file); `0xc00066f8` is a literal-pool slot inside
`FUN_c0006700` (`panel_manager_dispatch_scan_byte`), a 312-instruction stateful
message dispatcher that turns out to be the CONFIRMED real caller of both
`switch_chattering_register()`/`_remove()`. Real callers found for
`panel_manager_set_led_bit()`/`_apply_led_group()` too: a diagnostic LED-list scroller
(`FUN_c00093a4`) and a large incoming wire-protocol opcode dispatcher (`FUN_c0009b54`)
— the latter directly confirms the file's own old speculation about "whatever
wire-protocol opcode replaces K1's cpsoc.cpp LED-bargraph handlers."

**`panel_scan_updater.c`**: all four sub-step callees (erase/write/verify/apply) now
have real Ghidra decompiles and are fully transcribed (dense SPI/TWI
command-response handshakes) rather than left as bare `extern` citations. Real caller
of `panel_scan_updater_run()` found (`FUN_c000685c`), confirming `expect_ver=0xb3`/
`expect_rev=0x39` as real constants. `panel_scan_updater_apply()`'s own signature is
corrected from a guessed `(int, int)` to the real `(const uint8_t *hexstream, int len)`
— revealing `panel_scan_updater_run()`'s own `b1`/`b2` params are very likely a
hex-encoded-firmware-stream pointer/length pair, not a "progress-report byte pair" as
previously guessed.

**`switch_chattering_detector.c`**: the register()/remove() list-node indexing
discrepancy is now Ghidra-decompile-CONFIRMED (not just capstone-suspected) — genuinely
real, still deliberately left unreconciled. Real, sole caller for both functions found:
`panel_manager_dispatch_scan_byte()` (see above), a genuine cross-file relationship
now recorded in both files.

**`system_info_holder.c`**: `system_info_holder_print_status()`'s own call structure
is corrected — it's two genuinely separate steps (format-into-buffer, then
draw-buffer), not one combined 4-arg call as the hand-disassembly originally modeled.
Both of the file's own open items resolved: `0xc000a7c8` (this file's own anchor xref)
is a bounds-checked 8-entry table accessor, now this file's strongest confirmed real
xref; `0xc0009b0c` is the "SYSTEM STARTUP FAILED" fault-screen cluster — its full text
recovered VERBATIM via `read_memory`, directly paralleling K1's own hard-fault screen.

**`omap_l108_syscfg.c`**: found real callers for `omap_syscfg_reset_and_enable()` and
`omap_syscfg_set_reg154()`; byte-verified (via `read_memory`, since Ghidra's own
auto-analysis still has no Function objects for them) that `reg130_a`/`reg130_b`/
`dual_pull_enable`'s existing hand-transcriptions are exact instruction-for-instruction
matches to real memory contents; found one brand-new 7th leaf
(`omap_syscfg_clear_pull_enable_0xc`) immediately following the previously-documented
cluster. **Cross-file correction**: `omap_syscfg_set_reg118()` was REMOVED — it was a
misattribution of `soc_periph.c`'s own `board_desc_set_pinmux_3word()` tail (same
address range, same exact `0x54704404` constant at `+0x118`), not an independent
function. This closes the exact follow-up `soc_periph.c`'s own header had already
flagged as needed but out of its own scope to perform.

**Not resolved even with live access**: several `panel_fault()` call sites across
`panel_scan_updater.c`'s four sub-steps had their line-number arguments elided by
Ghidra's own decompile (not recovered — marked explicitly rather than guessed); a
handful of uncharacterized callees (`FUN_c0005284`, `FUN_c001ac94`, `FUN_c000704c`,
`FUN_c00068d4`'s own fixed-global arguments) remain open; no caller exists anywhere in
Ghidra's own xref database for 4 of `omap_l108_syscfg.c`'s 6 functions despite being
real, byte-verified code. See each file's own header/Still-Open section for full
detail. `make` re-verified clean, 33/33, after all edits above.

## 2026-07-19 — second, independent live-Ghidra pass: re-verification + tree sweep

A SEPARATE pass from the one immediately above (different session, same day), tasked
specifically with independently re-verifying `omap_l108_syscfg.c`'s "4 orphan
functions" finding from scratch (rather than trusting the live pass above's own
self-report - this project's own established practice per its agent-memory notes on
not trusting unverified agent claims) and sweeping the rest of the tree for remaining
`NEEDS LIVE QUERY` markers. Same read-only tool set (`get_xrefs_to`/`get_xrefs_from`,
`get_function_info`, `decompile_function`, `get_disassembly`, `search_bytes`,
`search_strings`, `read_memory`), zero mutating calls, zero Agent-tool subagent calls.

**Independently reproduced** the prior pass's core `omap_l108_syscfg.c` finding (zero
xrefs for the 4 orphans) and went further on the specific function-pointer-table
hypothesis the task asked to test: a full-binary `search_bytes` sweep for each
orphan's raw little-endian address came back ZERO hits for all 4, actively ruling out
that mechanism (not just leaving it unconfirmed). Also pinned down the mechanical
reason no xref exists: Ghidra has never disassembled `0xc0001c04`-`0xc0001c4c` as
Instructions at all. Noted, honestly, that this pass's own `read_memory`/`search_bytes`
calls against that exact address range were intermittently flaky (transient "script
produced no output," cleared on retry) - every byte re-read matched the existing
transcription exactly once a retry succeeded, so the data itself is not in question,
only this pass's ability to single-shot a large read of it.

**Tree sweep resolved this pass**: all 5 clustered CDIX register tables in
`cdix4192.c`/`cdix_autoswitch.c` (byte-exact, confirmed contiguous end-to-end);
`mcasp.c`'s `0xc0002738` (confirmed real code, not just xref-inferred, matching the
predicted second-McASP-instance call pair plus a real continuation);
`omap_l137_usbdc_ext.c`'s last two unaddressed Section-3 functions
(`usbdc_desc_set_length`/`usbdc_desc_get_length`, both now with confirmed K2
addresses, bodies, and real callers); `omap_l137_usbdc_ep0.c`'s `FUN_c000a7dc`
question (live-decompiled and given a real caller - best-evidence conclusion: it IS
`cad_pedal_present`'s K2 replacement, expanded to a 3-valued classifier);
`panelbus_dispatch.c`'s previously-unbounded `panelbus_hw_bringup` continuation
(fully disassembled, correcting a prior "symmetric I2C0+I2C1" speculation - it
actually repeats the same I2C1 setup twice via a tail branch) and its own
`i2c0_i2c1_base_select` re-sweep (confirmed exactly one caller in the whole binary,
definitively closing that question in the negative).

**Left honestly open**: `omap_gpio.c`'s DIR/OUT_DATA-defaults question (still 30
confirmed callers of `gpio_bank_get_base`, none individually re-examined - out of
this pass's time budget); the 4 `omap_l108_syscfg.c` orphans' real caller mechanism
(table hypothesis now falsified, but no alternative confirmed - may be a computed
indirect branch this project's static tools can't trace, or genuinely dead code).
See each edited file's own header for full citation detail;
`docs/modules/KRONOS2S_V01R10.VSB.md`'s own "second independent live-Ghidra pass"
section has the narrative summary. `make` re-verified clean, 33/33, after all edits.

## 2026-07-19 — third live-Ghidra pass: `cobjectmgr.c`, `heap_alloc.c`, `task_sched.c` depth pass

A third, independent same-day live-Ghidra-MCP pass (read-only tools -
`get_disassembly`/`decompile_function`/`get_function_info`/`get_xrefs_to`/
`get_xrefs_from`/`search_strings`/`read_memory`/`search_bytes` against
`kronos2s_v01r10_panel.elf`, no `load_binary`, no mutating calls, same "2-agent cap,
no subagent fan-out" authorization as the passes above), targeting every NEEDS LIVE
QUERY / "still open" item flagged in `cobjectmgr.c`, `heap_alloc.c`, and
`task_sched.c`'s own file headers. Zero Agent-tool subagent calls made.

**`task_sched.c` - HEADLINE FINDING, resolves this file's own top-priority open
item.** `read_memory` on `sched_tcb_table_init_and_autostart`'s own resolved literals
(count @0xC002A6F8, task-id array @0xC002A68C, cfg-record array @0xC002A698, 0x20-byte
stride) dumped K2's ROM autostart table byte-exact: exactly 3 tasks (ids 1/2/3,
priorities 0/2/4). Hand-disassembling each task's own cfg+8 entry point (Ghidra has no
Function object anywhere in this range - `get_disassembly`/`decompile_function` both
returned "no output"/"no function found," the same recurring artifact this project has
hit repeatedly; fell back to raw `read_memory` + hand ARM decode) resolves ALL three,
and the answer is more interesting than a plain yes/no: **`eva_board_main` does not
exist as a single K2 function at all.** It is split across 2 of the 3 autostarted
tasks - task id=1 (priority 0, most urgent) runs `eva_board_init_table`'s own
walk-and-dispatch loop (confirmed via its own `mov lr,pc; mov pc,r3` computed-dispatch
tail landing exactly at `eva_board_main.c`'s own already-cited `0xC00072EC`
COMPUTED_CALL xref), then `eva_board_final_setup`, then loops FOREVER calling
`eva_board_boot_status_dispatch` (a genuine, hand-confirmed 2-instruction infinite
loop - branches back rather than falling through); task id=2 (priority 2) is a
SEPARATE stub that calls `eva_board_main_loop` (the real `master_dispatch_tick`
forever-loop). This directly corrects `eva_board_main.c`'s own current
single-straight-line reconstruction of these same 4 call sites (which was built purely
from `get_xrefs_to` call-site addresses, never from one linear disassembly) - NOT
edited into that file per its own instructions and this project's convention; reported
here for a future consolidation pass.

**Bonus resolution of `eva_board_main.c`'s own separate open item, found via the same
ROM-table dump, not a separate search**: task id=3 (priority 4, lowest - structurally
unreachable in normal operation, since task id=2's own main-loop body never blocks) is
an immediate, unconditional `crypto_at88_fault` call citing `"../EvaBoardMain.cpp"`
line `0x70` - this IS the K2 counterpart of `eva_board_watchdog_fault_wrapper`, the
function `eva_board_main.c`'s own header said could not be independently located as
that anchor string's only xref.

**Other `task_sched.c` resolutions**: a real transcription bug in this port's own
`sched_task_create_and_ready` (an erroneous extra `cfg +` pointer addition not present
in the live decompile, and not present in K1's own version either) was found via
`decompile_function` and fixed - which also definitively settles K1's own long-open
ambiguity about which of a task's two ROM-pushed stack words is the real jump target
(cfg+8, not cfg+4 - settled using the ROM table's own real data: every observed cfg+8
value is a code address, every cfg+4 value is a small integer). The delay-heap
sift-up/sift-down internals' real K2 addresses (`FUN_c001aae8`/`FUN_c001a7d0`) are now
confirmed via `sched_delay_heap_extract_min`'s own 2-arg/1-arg call sites - not
transcribed to C, addresses only. A new, real, previously-uncharacterized function was
found via `get_xrefs_to` on `sched_remove_from_ready` (~`0xC001AA98`, another unbounded
region) that removes the current task from ready then resets its TCB - resolves a
specific call-site address (`0xC001AAB4`) K1's own file cited but never traced, though
its shape does NOT match either `eva_board_sched_ready`'s or `eva_board_sched_requeue`'s
own K1 description, so neither is claimed found - both remain genuinely open.

**`cobjectmgr.c`**: `cobjectmgr_hardware_fault_watchdog`'s search was independently
re-checked from two angles, both negative but concretely so rather than merely
unswept: `get_xrefs_to` live on the ack primitive (`kobj_eventflag_clear`, K2
`FUN_c001a01c`) still returns exactly the same single caller the static dump already
found; and the same ROM-table dump above accounts for ALL of K2's autostarted tasks,
none of which is cobjectmgr-shaped - ruling out "it's an unswept 4th ROM-table entry"
concretely. The function's real K2 address remains genuinely unresolved.

**`heap_alloc.c`**: `decompile_function` on `heap_free` (`FUN_c0012e58`) succeeded in
full this pass (previously matched by structural shape only). Confirms the file's own
existing description exactly and adds real detail: `heap_state+8`'s "is this the top
chunk" identity-sentinel role is now explained (it IS the top-chunk pointer's own
address, no separate sentinel object - textbook dlmalloc idiom); heap_trim's real
trigger threshold (`*0xC00A0E78`, immediately before `heap_state`'s own base) and pad
argument (`*0xC01CE1EC`, in the same data region as `heap_stat_total`/`heap_errno`) are
now both resolved with real addresses. `heap_malloc`'s own `decompile_function` call
returned no output and was not retried further this pass (time budget) - the treebin/
designated-victim slot layout remains open. The K1-heap-size cross-check item was
checked and confirmed NOT resolvable even with live K2 access: K1's own file was
directly re-read this pass and genuinely never states its own heap-base/end constants
numerically either - closing this would need a live query against K1's own Ghidra
project, out of this pass's scope.

`make` re-verified clean, 33/33, after all edits above. See
`docs/modules/KRONOS2S_V01R10.VSB.md`'s own "third live-Ghidra pass" section for the
narrative summary, and each edited file's own header for full byte-level citation
detail.

## 2026-07-19 — fourth live-Ghidra pass: `soc_irq_gate.c`, `wire_dispatch.c`, `omap_gpio.c` NEEDS-LIVE-QUERY sweep

A fourth, independent same-day live-Ghidra-MCP pass (read-only tools -
`get_disassembly`/`decompile_function`/`get_function_info`/`get_xrefs_to`/`get_xrefs_from`/
`search_strings`/`read_memory`/`search_bytes` against `kronos2s_v01r10_panel.elf`, no
`load_binary`, no mutating calls, same "2-agent cap, no subagent fan-out" authorization as
the passes above), targeting every remaining NEEDS LIVE QUERY / "still open" item in
`soc_irq_gate.c`, `wire_dispatch.c`, and `omap_gpio.c`'s own file headers. Zero Agent-tool
subagent calls made.

**`soc_irq_gate.c` - `FUN_c0000040` RESOLVED, plus a real self-correction.** This file has
cited "FUN_c0000040 (ch-0x15 enable, Timer64P0 lazy-init singleton) - belongs to
soc_periph.c's own territory" as an unfilled gap since its very first 2026-07-19 pass.
`get_function_info` + `decompile_function` + `get_disassembly` on `0xc0000040` confirmed
its shape exactly (lazy-init guard, `timer64p0_base_get()` cache, `board_desc_init_type5()`
call, AINTC ch-0x15 EISR enable) and it is now DEFINED in `soc_periph.c` as
`timer64p0_enable_ch15`. Chasing this down a level (`read_memory` on the function's own
literal pool) surfaced a REAL, CONFIRMED ERROR this file had carried since its first pass:
`soc_irq_gate_timer0_quiesce`'s own cached-handle literal (`DAT_c0000124`) reads
`0xC00E0000` (table+0x00), not `0xC00E0008` (table+0x08) as previously claimed - direct
byte evidence, not a guess. This also RETRACTS a downstream claim that this same slot was
"the SAME slot as CLUSTER 2's own `mcasp_param_b_cache`" - that cache's own literal
(`DAT_c00001a8`) was independently read-verified as genuinely `0xC00E0008`, so the two
were never the same slot; only CLUSTER 1's own annotation was wrong. Both corrections are
made in `soc_irq_gate.c` itself, with the old vs. new evidence documented in place rather
than silently overwritten.

**A genuinely new, previously-undocumented 72-byte code region** was found in the gap
between the two now-fixed functions (`0xc0000098`-`0xc00000df`) while investigating the
above - no Ghidra Function object bounds it, so it was manually characterized from raw
`get_disassembly`. It opens with the exact same "read table+0x00, OR bit 0x2 into +0x44"
idiom as `soc_irq_gate_timer0_quiesce`'s own opening (confirmed via a second `read_memory`
call on its own literal pool), writes AINTC ch-0x15's SICR via a hardcoded literal address
instead of a live `aintc_base()` call, calls a tiny report-code gate function, then
tail-branches (a real ARM `b`, not `bl`+return) into a large, separately-prologued,
CPSR-IRQ-disable-guarded routine. `get_xrefs_to` on it shows exactly one reference - a
`PARAM`-type (not `CALL`-type) read from `task_sched.c`'s own already-cited `FUN_c00199dc`
(that file's own header independently names this as one of `eva_board_crt0`'s own
ROM-table-walk subsystem-bring-up calls), strongly suggesting this is itself a `task_sched.c`
ROM-autostart-table entry - flagged there, not claimed or defined in this file.

**`gap_slot_bringup` investigated, still not located - but surfaced a real cross-file bug
in a DIFFERENT file.** `get_xrefs_to` on `omap_l137_addr_gap_misc.c`'s own claimed K2
address for `usbdc_gap_config_slot` (`0xc0002d80`) returned exactly the 2 callers that
file's own header predicts, but `decompile_function` on `0xc0002d80` itself shows an I2C
ICMDR-shaped clock-divider config write (`+0x24`/`+0x30`/`+0xc`/`+0x10` register fields) -
nothing USB/gap-shaped at all. This is `panelbus_dispatch.c`'s own already-independently-
decompiled `panelbus_i2c_mode_config`, confirmed by its own header's matching description.
`omap_l137_addr_gap_misc.c`'s citation of this address as `usbdc_gap_config_slot` is a real
address-collision/misattribution bug (most likely confused with its neighbor during that
file's own pass) - flagged in `soc_irq_gate.c`'s own header for whoever next touches
`omap_l137_addr_gap_misc.c`, deliberately NOT fixed there (out of this pass's own assigned
files: `soc_irq_gate.c`, `wire_dispatch.c`, `omap_gpio.c`, plus `soc_periph.c` for the one
function it genuinely owns). `usbdc_gap_config_slot`'s real K2 address remains unlocated.
`soc_irq_gate_slot0x00_get`/`_ring3_state_reset` (K1's own cluster-11 tail items) also
remain unlocated, though a full-image `search_bytes` sweep for the `table+0x00` literal now
puts a firmer negative on record: only the 4 already-known real consumers of that address
exist project-wide, none shaped like a bare getter wrapper.

**`wire_dispatch.c` - two items resolved.** `decompile_function` on both
`eva_wire_pedal_send` (`FUN_c00058b8`) and `wire_dispatch_command` itself confirms the
long-flagged "distinguishing 2nd argument" between opcode reg `0x50` and `0x52` is a real,
previously-missing 3RD PARAMETER (`1`="set"/`0`="clear" into a `(word_slot, bit_pos)`-table
bit, the same table-lookup idiom this project's own `panel_manager_set_led_bit` already
established) - both call sites in `wire_dispatch_command` corrected to pass it explicitly.
`FUN_c0008d24` (the opcode `0xc6` continuation resolver, previously only role-confirmed,
"too dense to inspect") is now structurally characterized in real detail via
`decompile_function` + three `read_memory` calls on its own literal-pool constants: a
byte-per-entry, 256-entry LUT (confirmed at `0xC001B814`) expanding an 8-bit-indexed byte
stream into 16-bit values written into a `0x321`-halfword circular ring buffer - confirmed
NOT the same ring/cursor object as the opcode `0xc4` continuation (`FUN_c0009158`, which
was also re-decompiled this pass and found to target an 800-wide framebuffer with a
palette-indexed pixel write, a genuine improvement on its own prior "raw pixel write"
description). Not transcribed statement-for-statement (dense, ~140 real statements with
heavy duplicated wraparound-check inlining across two near-identical loop shapes) - matches
this project's own established practice for code this shape.

**`omap_gpio.c` - headline DIR/OUT_DATA question CLOSED, reasonable-confidence negative.**
`get_xrefs_to` on `gpio_bank_get_base` confirmed the same 30 callers `i2c_by_gpio.c`'s own
header already counted; every one was individually examined this pass (matched against an
already-documented file, or freshly `decompile_function`-ed: `FUN_c0000904`, `FUN_c0000ec8`,
`FUN_c0000ef0`, `FUN_c000303c`, `FUN_c0005088`, `FUN_c00050f0`, `FUN_c001041c`,
`FUN_c00080e4`, `FUN_c000a1d0` were all freshly decompiled here). NONE writes to the
pair-1/3/4 absolute DIR/OUT_DATA offsets K1 used (`+0x38`, `+0x88`/`+0x8c`, `+0xb0`/`+0xb4`).
A supplementary full-image `search_bytes` sweep for K1's own three DIR default constants,
as raw little-endian literal-pool bytes, found zero hits for all three - with the honest
caveat, stated in the file itself, that an ARM MVN-immediate encoding could hide an
equivalent constant from a byte search, so this is supporting evidence, not independent
proof. Two of the freshly-decompiled functions (`FUN_c0000ec8`/`FUN_c0000ef0`) were
double-checked against the rest of the tree before being credited as "new" - they turned
out to already be fully documented in `i2c_by_gpio.c` as `i2c_gpio_set_scl`/`i2c_gpio_set_sda`,
so nothing was duplicated.

`make` re-verified clean, 33/33, after all edits above. See
`docs/modules/KRONOS2S_V01R10.VSB.md`'s own "fourth live-Ghidra pass" section for the
narrative summary, and each edited file's own header for full byte-level citation detail.
