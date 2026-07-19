# KRONOS2S_V01R10.VSB — Kronos 2 NKS4 Panel Sub-System Firmware

Source: `Decomp/subsystem/KRONOS2S_V01R10.VSB` (917 760 bytes — byte-identical *size* to
`KRONOS_V06R06.VSB`, see below). This is the **Kronos 2** front-panel controller board
firmware, the sibling image to [`KRONOS_V06R06.VSB.md`](KRONOS_V06R06.VSB.md) (Kronos 1).
Same physical role as its K1 counterpart — a standalone embedded system (TI OMAP-L1x,
ARM926EJ-S) behind USB `0944:1005`, separate from the Kronos's own x86 host — but,
per the 2026-07-18 anchor-string comparison pass documented below, a **partly
redesigned codebase**, not a simple recompile/patch of K1.

This doc is the K2 counterpart to `KRONOS_V06R06.VSB.md`; read that doc first for the
container-format background and Ghidra-setup method, both of which transfer to K2
unchanged except where explicitly noted below. The reconstructed C source lives at
[`kronosology/reconstructed/K2_V01R10/`](../../reconstructed/K2_V01R10/) (see that
directory's own `README.md` for migration-pass-by-pass status); the K1 baseline it's
diffed against lives at
[`kronosology/reconstructed/K1_V06R06/`](../../reconstructed/K1_V06R06/) (READ-ONLY,
untouched by this work).

---

## Container format ("KORG SYSTEM FILE") — diffed against K1

Same 256-byte header layout as K1 (see `KRONOS_V06R06.VSB.md`'s own table for the
full field list); only the fields that actually differ are called out here, confirmed
by a direct byte-for-byte read of both raw `.VSB` files (`xxd`, not inferred):

| Offset | Size | K1 (`V06R06`) | K2 (`V01R10`) | Notes |
|---|---|---|---|---|
| `0x10` | 16 | `"KRONOS"` | `"KRONOS II"` | product tag, NUL-padded in both |
| `0x28` | 4 | `00 01 06 06` | `00 01 01 0a` | subtype `0001` unchanged; version `06.06` → `01.10`, matches filename `V01R10` |
| `0x2c` | 1 (of the 4-byte field previously logged as `80 02 03 ff`) | `80` | `00` | **new finding, this pass** — first byte of this still-unidentified 4-byte field changed `0x80`→`0x00` between K1 and K2; remaining 3 bytes (`02 03 ff`) unchanged. Still not decoded (flags/checksum-seed guess unconfirmed either way) |
| `0x41` | 1 (of the 2-byte field previously logged as `02 00`) | `00` | `01` | **new finding, this pass** — second byte of this still-unidentified field changed `0x00`→`0x01`. Previously guessed "possibly chunk/section count" — a K1→K2 delta of exactly 1 is at least *consistent* with that guess (one more chunk/section in K2), but not confirmed |
| `0x34`/`0x3c` | 4+4 | `00 00 0e 00` (917 504) | `00 00 0e 00` (917 504) | payload length identical — **the payload is the same size in both images**, only its contents differ |

**Whole-file byte diff** (direct comparison, both files 917 760 bytes): **257 559 of
917 760 bytes differ (28.06%)**; restricted to the payload only (skipping the 256-byte
header, which itself differs only in the fields tabulated above): **257 552 of 917 504
bytes differ (28.07%)** — i.e. essentially all of the header-vs-payload difference is
in the payload, as expected. This is consistent with "meaningfully redesigned in
places, not just a few patched addresses" — a simple hot-fix/relink would show a much
smaller diff fraction concentrated in a handful of runs; ~28% spread across the image
is compiler-output-scale change (new functions, removed functions, shifted layout
cascading through every subsequent address-embedding literal pool).

---

## Target hardware — same base load address, same reset-vector shape

Confirmed independently (raw payload bytes, not decompile output): K2 still links flat
at **`0xC0000000`** — the same 5×`LDR PC,[PC,#0x18]`-style ARM exception vector table
at payload offset `0x000` as K1, though the *non-reset* vectors 6/7/8 use slightly
different literal-pool offsets in K2 (`E5 9F F0 04` / `E5 9F F0 14` / `E5 9F F0 14`
vs. K1's, worth an independent re-check if anyone traces those paths — not chased in
this pass, only the reset vector was followed). The reset vector's own literal-pool
target **differs, as expected from any address-shifted rebuild**: `0xC0009534` (K1) →
**`0xC000A860`** (K2), confirmed both by reading the raw literal-pool bytes at payload
offset `0x20` and by decompiling `FUN_c000a860` in the K2 Ghidra dump — which shows
the exact same shape K1 already diagnosed for its own reset handler (a 2-call stub
tail-calling into a noreturn crt0 function, with the classic "Control flow encountered
bad instruction data" Ghidra artifact from linear disassembly running into the
following literal pool — not a new anomaly, the same one K1's README already resolved
for `FUN_c0009534`/`FUN_c0009540`).

---

## Firmware source-file inventory — anchor-string comparison, 2026-07-18

Every `../*.cpp`-style `__FILE__` literal was enumerated in both images' string tables
and diffed. This is the basis for this pass's whole migration scope:

**Present in BOTH K1 and K2** (the "shared low-level driver layer" — subject of this
migration pass, see `K2_V01R10/README.md` for per-file status):

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

**Present in K1, CONFIRMED ABSENT from K2's string table** (i.e. genuinely gone, not
just unreached by this string scan):

| K1-only source file | K1 anchor | Reconstructed as (K1) |
|---|---|---|
| `../cpsoc.cpp` | — | `K1_V06R06/cpsoc.c` |
| `../ctouchpanel.cpp` | — | `K1_V06R06/ctouchpanel.c` |
| `../McAspHandler.cpp` | — (0 xrefs even in K1 — dead string already in K1) | not reconstructed separately in K1 either |
| `../MCU/Component/OmapL108Spi.cpp` | `0xc0022d44` | `K1_V06R06/omap_l108_spi.c` |

The `OmapL108Spi.cpp` absence is a **new finding this pass**, not previously called
out in the parent task brief's own anchor list: K2's string table has **no**
`OmapL108Spi.cpp` literal anywhere, replaced positionally (same slot in the literal
pool's address ordering, between `OmapL108.cpp` and `OmapL137Mcasp.cpp`) by a new
`../MCU/Component/OmapL108Syscfg.cpp` string. Whether K2's SPI peripheral driver was
folded into a broader "Syscfg" unit, replaced by a different bus entirely, or simply
renamed is **not established** — out of this pass's scope (the task brief's own
shared-driver list names `OmapL108`, not `OmapL108Spi`), flagged here for whoever
picks up `OmapL108Syscfg.cpp` next.

**New in K2, no K1 counterpart at all** (being reconstructed fresh by a separate,
concurrent pass — **not** part of this migration work, listed here only for
completeness of the anchor-string picture):

| K2-only source file | K2 anchor |
|---|---|
| `../MCU/Component/OmapL108Syscfg.cpp` | `0xc002a748` |
| `../PanelManager.cpp` | `0xc002af98` |
| `../PanelScanUpdater.cpp` | `0xc002b18c` |
| `../SwitchOnChatteringDetector.cpp` | `0xc002b1f4` |
| `../SystemInfoHolder.cpp` | `0xc002b5d0` |

Note the K2 literal-pool address *ordering* of the shared files exactly mirrors K1's
own ordering (CryptoAt88 → I2cByGpio → OmapL108 → [Spi/Syscfg] → OmapL137Mcasp →
OmapL137Usbdc → … → EvaBoardMain → cobjectmgr → … → CDix4192 → clcdc), with the new
K2-only files' literals simply inserted at the two points where K2 gained whole new
subsystems (between `OmapL137Usbdc.cpp`/`EvaBoardMain.cpp`, and between
`cobjectmgr.cpp`/`CDix4192.cpp`) — consistent with an incremental link-order-preserving
addition of new translation units rather than a full reshuffle, and a useful
navigational fact: it means each shared file's neighbors-in-the-string-table are the
same neighbors in both images, which is how this pass located each file's approximate
K2 code region before falling back to decompile-shape matching for the exact function
boundaries. `../cad.cpp` (K1's analog/knob driver) was not part of this pass's
anchor-string check either way — status unknown, flag for a future pass.

---

## Migration status — driver-layer C reconstruction, 2026-07-18 pass

See [`K2_V01R10/README.md`](../../reconstructed/K2_V01R10/README.md) for the full
per-file breakdown (function counts, what differed from K1 vs. what ported unchanged,
open items, NEEDS LIVE QUERY items). Summary:

| K1 file | K2 file | Status |
|---|---|---|
| `aintc.c` | `aintc.c` | Confirmed byte-for-byte identical logic, re-addressed; K2 has 20 vs K1's 24 `aintc_base` callers (fewer early-bringup stubs, consistent with cpsoc/cad removal) |
| `omap_l108.c` | `omap_l108.c` | Confirmed structurally identical tick-service API; `cad_delay_ticks`/`cad_calibration_progress_pump` (K1's own callers of it) have NO confirmed K2 mapping — NEEDS LIVE QUERY |
| `i2c_by_gpio.c` | `i2c_by_gpio.c` | Done; corrected a real K1 signature bug in the process (`write_block`/`read_block` are 5-arg, not K1's own documented 6-arg shape) |
| `mcasp.c` | `mcasp.c` | Confirmed byte-for-byte structurally identical to K1, including embedded fault-call line numbers — a straight recompile/relink, not a hardware-generation port |
| `crypto_at88.c` | `crypto_at88.c` | Migrated, anchor independently reconfirmed |
| `cdix4192.c` | `cdix4192.c` | Structurally identical port of K1 |
| `clcdc.c` | `clcdc.c` | Migrated |
| `cobjectmgr.c` | `cobjectmgr.c` | Migrated |
| `eva_board_main.c` | `eva_board_main.c` | Migrated; HEADLINE DIFFERENCE FROM K1: `eva_board_start_task(1,4)`'s own K2 call site could not be located — see `task_sched.c` below, which resolves this (K2 auto-starts boot tasks via a ROM table walk in crt0, not an explicit call from `eva_board_main` itself) |
| `omap_l137_usbdc.c` / `_ep0.c` / `_ext.c` | same | Migrated; `_ext.c` has a real, confirmed Ghidra data gap in part of its own range |
| (none — new file) | `omap_l108_syscfg.c` | New K2-only low-level driver, see "New panel-scan architecture" section below |
| `soc_periph.c` | `soc_periph.c` | **This pass.** Peripheral base-address table confirmed structurally near-identical to K1 (11 of 13 K1 entries carry over with identical or TRM-consistent values); 2 genuinely NEW table entries (I2C0/I2C1 selector, eCAP2 base); SPI0 base survives in K2 despite cpsoc/cad removal — now consumed by `panel_scan_updater.c`'s firmware-update sequencer instead; UART selector shrank from K1's 3-way to a single UART1-only accessor; pinmux writes merged from 3 one-word K1 leaves into 1 three-word K2 leaf |
| `omap_gpio.c` | `omap_gpio.c` | **This pass.** HEADLINE FINDING: K1's generic pair-indexed GPIO register primitives (`gpio_reg_read_in/set_bit/clear_bit`, `bank_base + pair*0x28 + offset`) are GONE in K2 — confirmed via full-image pattern search, zero real matches. Every K2 GPIO-touching leaf instead hardcodes one fixed bit at one fixed offset directly. `gpio_bank_hw_init`'s K2 counterpart is a real but genuinely smaller subset of K1's own register writes (no DIR/OUT_DATA defaults for pairs 1/3/4) |
| `heap_alloc.c` | `heap_alloc.c` | **This pass.** Confirmed structurally identical to K1 (dlmalloc-derivative shape unchanged) — `heap_sbrk`/`heap_trim` statement-for-statement identical, `heap_malloc`'s opening rounding/bin-selection logic identical including the same shift/offset constants |
| `task_sched.c` | `task_sched.c` | **This pass.** HEADLINE FINDING: K2 kept the exact same RTOS-shaped task scheduler (TCB table, priority ready-queue/bitmap, kernel-object event-flag API, delay/timeout min-heap) as K1, just re-addressed. Resolves `eva_board_main.c`'s own open question about `eva_board_start_task`'s K2 fate: boot tasks are auto-started via a ROM table walk in crt0 (`sched_task_create_and_ready`), not an explicit call. One real, confirmed behavioral difference from K1 found in `sched_wait_list_insert` (see file header) |
| `soc_irq_gate.c` | `soc_irq_gate.c` | **2026-07-19 pass.** Confirmed structurally near-identical to K1 for 10 of 11 AINTC channel-gate leaves plus both "group" enable/disable dispatchers (found by a full-image sweep for every K2 function calling `aintc_base()` directly). Real, confirmed differences: Timer64P1 (ch 0x17) and channel 0x36 gating dropped entirely; channel 0x2a now has a real enable side (inlined into the group-A dispatcher, resolving K1's own "no enable stub found" open item); channel 0x32's disable path now re-arms its own enable guard (K1's didn't); the shared 0xC00E0000 bookkeeping table (a fixed OMAP-L138/DA850 physical SRAM address, not firmware-relative) is CONFIRMED identical between K1 and K2, just smaller (0xC00E0000-0xC00E004C vs K1's -0x0068) |
| `panelbus_dispatch.c` | `panelbus_dispatch.c` | **2026-07-19 pass.** HEADLINE FINDING: K1's own per-tick I2C0/I2C1 opcode dispatcher (built on `cad.cpp`'s calibration handlers and `cpsoc.cpp`'s "third SPI device") has **no confirmed K2 counterpart** — a full xref sweep of the shared I2C0/I2C1 base selector (already defined in `soc_periph.c` as `i2c0_i2c1_base_select`) finds exactly two call sites, both inside one crt0 hardware bring-up stub, zero further consumption anywhere in the covered dump. Consistent with `cad.cpp`/`cpsoc.cpp` both being confirmed absent from K2's string table (see the anchor-string table above) — this closes `soc_periph.c`'s own previously-open "real consumption... not fully swept" item for that selector, in the negative |

---

## New panel-scan architecture — 2026-07-18 pass

Reconstructed the 5 genuinely-new K2-only files (`PanelManager.cpp`,
`PanelScanUpdater.cpp`, `SwitchOnChatteringDetector.cpp`, `SystemInfoHolder.cpp`, and
`../MCU/Component/OmapL108Syscfg.cpp` — grouped in with this pass despite being a
low-level driver, not panel-scan logic, because it's new and directly fills
`OmapL108Spi.cpp`'s old slot, see the anchor-ordering note above). Full per-file detail,
including every "still open"/NEEDS LIVE QUERY item, lives in
[`K2_V01R10/README.md`](../../reconstructed/K2_V01R10/README.md) — summarized here:

**Real methodology gap worth flagging up front**: the pre-fetched Ghidra static dump
for K2 does not include any of the actual functions these 5 files' code lives in as
decompiled "Function" objects, despite those addresses sitting well inside the dump's
own nominal coverage range. Recovered instead via manual `capstone` ARM32 disassembly
of the raw wrapped-ELF image, located by byte-pattern search rather than Ghidra's
xref graph — real, faithful register-level transcriptions, but without decompiler-grade
type inference or control-flow cleanup. Lower confidence than any other file in this
project's history; each function below should be re-verified against a live Ghidra
decompile once available rather than taken as final.

| File | What it actually does (verified from disassembly) | Confidence |
|---|---|---|
| `PanelManager.cpp` | Consolidates K1's `cpsoc.cpp` (switch/LED naming) + `cad.cpp` (knob/slider/pedal naming) into one file — a real, confirmed merge, not a guess. 77-entry switch/LED name table (vs. K1's 73), single-bit LED-state set/clear via a `(word_slot, bit_pos)` lookup table, 16-bit bulk-apply wrapper. | Medium — 2 real functions transcribed, deep coverage (diagnostic menu, name-lookup accessor) still open |
| `PanelScanUpdater.cpp` | Direct split-out of K1's PSoC field-firmware-update feature (previously mixed into `cpsoc.cpp`) into its own file. Top-level update sequencer fully transcribed (status text, erase/write/verify/apply steps, version/revision check, fault-on-mismatch). | Medium — top-level control flow real, 4 sub-steps cited but not transcribed |
| `SwitchOnChatteringDetector.cpp` | A genuinely new standalone debounce module (K1 never had one) — real 3-state (idle/debouncing/confirmed) per-switch FSM with a doubly-linked active-entry list, both register/remove functions transcribed. | Medium — real FSM, one confirmed-but-unreconciled cross-function indexing discrepancy flagged (see file) |
| `SystemInfoHolder.cpp` | Version/health-status display singleton — prints `SyncErrorCount` and a renamed `"Panel Scanner Version:%02d Revision:%02d"` (K1: `"Panel Scan System Version..."`). | Lower — file attribution is content-based, not purely positional; flagged explicitly in-file |
| `../MCU/Component/OmapL108Syscfg.cpp` | SoC pin-mux / module reset-and-enable peripheral driver, filling `OmapL108Spi.cpp`'s old link-order slot — consistent with the SPI bus's only two K1 consumers (`cpsoc.cpp`/`cad.cpp`) both being gone. Real reset/hold/deassert/configure/poll-with-timeout sequence plus several PINMUX-shaped register writes. | **Highest this pass** — the only one of the 5 files with a directly-confirmed real anchor-string xref (not inferred from content/neighborhood) |

Not established this pass: any caller for any of the transcribed functions (same static-
dump coverage gap, would need a live Ghidra xref search); the real switch/LED-index
mapping for `PanelManager.cpp`'s full 73-entry bit-position table; whether K2 kept,
renamed, or dropped K1's `"Psoc version error %02x != %02x : Id %03d"` string (absent
from K2's string table entirely — not found in the ranges swept this pass).

---

## Not covered by this pass

- `cad.cpp` (analog knobs/sliders/pedals) — not checked against K2's string table this
  pass; status unknown. (Its K1 naming role is now believed folded into
  `PanelManager.cpp` — see the new-architecture section above — but a standalone
  `cad.cpp`-equivalent driver file's fate in K2 is still unconfirmed.)
- Everything K1's own doc (`KRONOS_V06R06.VSB.md`) documents from string/data evidence
  alone rather than from a reconstructed-C-source pass (the boot splash resource, the
  73-entry switch/LED name table, the palette cross-check, the wire-protocol opcode
  table) — all of that is K1-specific work products cited against K1 addresses/data;
  none of it has been independently re-verified against K2 in this pass, and it should
  NOT be assumed to carry over unchanged (the switch/LED table in particular lived in
  `cpsoc.cpp`, which this pass confirmed is entirely absent from K2 — a K2-equivalent
  table, if one exists, would live in one of the new `Panel*.cpp` files above, out of
  this pass's scope).

## IRQ-gate and second-I2C-bus infrastructure — 2026-07-19 pass

Migrated K1's `soc_irq_gate.c` (the AINTC channel enable/disable leaf cluster sitting
right after the ARM exception vector table) and `panelbus_dispatch.c` (K1's on-chip
I2C0/I2C1 hardware controller dispatcher) into K2. Ground truth: `all_decompiled_k2.json`/
`all_data_k2.json` via `query_dump_k2.py`, no live Ghidra MCP calls. Full detail,
including every cited K2 address, cross-file naming reconciliation (with `mcasp.c`'s own
`mcasp_reinit_reduced` citation and `omap_gpio.c`'s own `gpio_bank_hw_init` citation), and
STILL OPEN items lives in each file's own header comment; summarized here.

**`soc_irq_gate.c`** — located via a full-image sweep for every K2 function calling
`aintc_base()` directly (14 hits: 11 real channel-gate leaves, `aintc_base` self-
referencing, `eva_board_crt0`, and the group-A dispatcher itself). All 11 leaves plus
both "group" enable/disable dispatchers (`soc_irq_gate_group_a_enable`/
`_group_b_disable`) are now defined, cluster-matched against K1's own file. The shared
~0x4C-byte bookkeeping table these leaves all reference resolves to **0xC00E0000** —
the IDENTICAL fixed physical address K1's own file already found for its own larger
(0xC00E0000-0xC00E0068) version of the same table — strong evidence it's a fixed
OMAP-L138/DA850 SRAM/scratch location on the silicon itself, not firmware-relative data,
since it survived a genuine K1→K2 firmware rewrite at the same address. Real, confirmed
architecture differences: Timer64P1 (ch 0x17) and channel 0x36 IRQ gating are gone
entirely (zero matches in a full-image literal sweep); channel 0x2a's enable side, which
K1's own file left as an explicit open question ("no matching enable stub found"), is now
confirmed real — inlined into the group-A dispatcher's own tail rather than a standalone
leaf; channel 0x32's disable path re-arms its own enable-side guard on every call (K1's
never did); channel 0x2a's disable-side GPIO ack write is inlined rather than routed
through a dedicated `omap_gpio.c` helper the way K1 did it. Also confirms, by direct
decompile, two items K1's own file left open: `FUN_c00019e0`/`FUN_c00019e8` really are
McASP-instance base getters (now named `mcasp0_base_get`/`mcasp0_fifo_base_get` in
`soc_periph.c`), and the mysterious "eva_link_status_change also calls group-B disable
stubs directly" pattern reproduces exactly in K2 (`FUN_c000a4bc`, cited not redefined,
already an open extern in `wire_dispatch.c`).

**`panelbus_dispatch.c`** — the opposite outcome: K1's own elaborate per-tick RX/TX
opcode dispatcher (built entirely on `cad.cpp`'s calibration handlers and `cpsoc.cpp`'s
"third SPI device", both confirmed absent from K2's string table, see the anchor-string
table above) has **no confirmed K2 counterpart**. The underlying hardware primitive (the
I2C0/I2C1 base selector) does survive, unchanged — but it's already fully defined in
`soc_periph.c` as `i2c0_i2c1_base_select`, and a full xref sweep of its own `callers`
list finds exactly two call sites, both inside a single crt0 hardware bring-up stub
(`panelbus_hw_bringup`, reconstructed here for the first time — resolves one of
`eva_board_main.c`'s own 11 "not individually traced" crt0 calls). No RX poll loop, TX
ring, or opcode table was found anywhere in the covered K2 dump. This closes
`soc_periph.c`'s own previously-open item ("`i2c0_i2c1_base_select`'s real consumption
beyond the one traced caller — not fully swept") in the negative: there is no further
consumption to find in this static dump's coverage. Genuinely open whether the real K2
panel-scan architecture uses this hardware block at all (most likely not — its
confirmed consumer, `PanelScanUpdater.cpp`, uses SPI per `soc_periph.c`'s own "SPI
SURVIVES" finding) or whether a further consumer exists outside this dump's own
function-boundary coverage (same known Ghidra-timeout gap the panel-scan files
themselves ran into) — NEEDS LIVE QUERY to settle definitively.

## MIDI subsystem cluster, 2026-07-19

Migrated K1's own USB-MIDI transport cluster (`midi_engine.c`, `chan_link_hw.c`,
`chan_param_ctrl.c`, `chan_slot_dispatch.c`, `usbdc_midi_status_glue.c`,
`uart1_midi_queue.c`, `midi_chan_status_queues.c` — 7 files, 4466 K1 source lines) into
K2. Ground truth: `all_decompiled_k2.json`/`all_data_k2.json` via `query_dump_k2.py`, no
live Ghidra MCP calls this pass. Located by walking outward from `soc_irq_gate.c`'s own
already-committed `midi_hw_write16`/`_read16`/`_fifo_write`/`_fifo_read` (its own
"CLUSTER 10", whose 130/53 caller counts this pass's own headline finding already
matched K1's 129/53) through every one of that cluster's own K2 xrefs.

**Headline finding, reproduced independently in all 7 files**: this whole MIDI transport
stack — hardware register layer, USB-MIDI CIN framer/event state machine, the link
watchdog/queue-drain cluster, the per-channel parameter engine, the UART1 16550-shaped
driver, and the TX/RX status-queue pair — is essentially **UNCHANGED between K1 and K2**,
just re-addressed. Across the 5 files ported with high confidence
(`chan_link_hw.c`/`chan_slot_dispatch.c`/`chan_param_ctrl.c`/`midi_engine.c`/
`midi_chan_status_queues.c`/`uart1_midi_queue.c`, 66 of K1's own ~76 functions in total),
the overwhelming majority match at an *exact* Ghidra-reported byte size, and every
numeric register constant independently re-resolved from K2's own `DAT_` pool (`0x1fe0`,
`0xfffe`, `0xffdf`, `0x102`, `0x186a0`, `0x20ff`, `0x11fd`, `0x116`, `0xffcb`, `0xffc0`,
`0xffff`, plus every per-cable/ring struct offset) is bit-for-bit identical to K1's own
values — the strongest cross-board stability result this project has found for any
subsystem so far.

**Real, confirmed differences from K1** (not transcription artifacts):
- **Three K1 globals merge into one in K2**: `midi_hw_mode_flags` (`midi_engine.c`),
  `chan_port_hwctx_global` (`chan_slot_dispatch.c`), and `chan_global_hi_mode_flags`
  (`chan_param_ctrl.c`) are three *separate* addresses in K1 but resolve to the
  **identical** literal `0xC01CCD10` in K2, independently re-derived from three unrelated
  call sites.
- **`uart1_midi_queue.c` and `midi_chan_status_queues.c` merge into one contiguous K2
  address run** (`0xc0011010`-`0xc0011a80`, no gap) — K1 kept the UART1 driver and the
  TX/RX status-queue cluster in two separate files; K2 built them as one compilation
  unit.
- **`chan_link_rt_queue_push`'s own K2 role is genuinely expanded**, not a 1:1 port — the
  K2 function occupying its cross-file slot (`chan_param_ctrl.c`'s own `FUN_c000ea68`,
  356 bytes vs K1's 144) still touches the same `+0x13a` counter field but also drains
  ring 1 and touches event-flag-shaped globals K1's own version never did — left as an
  `extern` with no body rather than force a possibly-wrong port, see that file's own
  header.
- **`midi_ring1_push_zeros`** (`midi_engine.c`'s own `FUN_c000e7cc`) is a real, confirmed
  new K2 function with no K1 counterpart — a "push N zero-filled words into ring 1" loop
  sharing K1's own ring-1 field layout exactly.
- **`uart1_tx_byte`** (`uart1_midi_queue.c`) gained a real side effect in K2: it now also
  sets a flag global whenever the transmitted byte isn't `0xFE` (Active Sensing) — the
  same condition K1's own drop-filter tests, but K1's version of this function never
  touched that flag itself.

**`usbdc_midi_status_glue.c` was the one incomplete file this pass** — only 3 of its own
16 K1 functions (`chan_ring2_relay_and_status`, `usbdc_ep_regblock_ptr_a`/`_b`, the
latter two with caller counts matching K1's exactly, 8 and 6) were located this pass; see
below for 2 more closed the following day.

See each of the 7 files' own header comments in `K2_V01R10/` for the complete
function-by-function K1↔K2 address map, every individually-verified `DAT_` constant, and
each file's own STILL OPEN section.

## `usbdc_core_isr` resolved + `usbdc_midi_status_glue.c` improved — 2026-07-19 (live pass)

Follow-on pass using a dedicated, single-session live Ghidra MCP bridge connection
(`get_disassembly`/`decompile_function`/`get_xrefs_to`/`read_memory`), authorized once for
this specific task under this project's own "2-agent cap, no further fan-out"
constraint — not the usual static-dump-only methodology. Two real gaps closed:

**`usbdc_core_isr` (`omap_l137_usbdc_ext.c`, Section 6) — RESOLVED.** This was the single
largest open item in the entire K2 shared-driver-layer migration: a genuine, confirmed
1940-byte hole in K2's function list (`0xc0003840`-`0xc0003fd4`) that Ghidra's own
auto-analysis never bounded as a function (still true after this pass — `decompile_
function`/`get_function_info` both report "no function found" there; the boundary itself
was never fixed, only manually transcribed from the raw instruction listing). The
function is a near-exact structural and semantic match to K1's own `usbdc_core_isr`
(same interrupt-status decode, same bus-reset EP1-4 re-init sequence at byte-identical
offsets/values, same per-endpoint event dispatch chain, same tail) with six real,
independently-confirmed differences from K1, not transcription artifacts:

1. The SETUP-pending branch's `usbdc_desc_arm_slot` call uses REAL confirmed register
   values `(dev, 1, 1)` — K1's own file could only guess `(dev, 0, 0)` here due to a
   phantom-forwarded-argument decompiler artifact obscuring the real values in K1's own
   pseudocode.
2. The EP1-ready branch's CSR-like field sits at a different confirmed byte offset in K2
   (`0x516`, matching Section 1's own direct/flat RXCSR-window formula for endpoint 1)
   than the offset K1's own file cites for the structurally identical branch (`0x462`).
3. The three boot-flag writes each compile through a real but functionally dead
   intermediate dereference of `dev` before the actual fixed-global write — harmless
   unless that dev-relative address is itself a volatile hardware register with a read
   side effect (not checked this pass).
4. The EP0 context nibble-clear operations (`&= 0xf0`) compile to full 32-bit `ldr`/`str`
   in K2, not the byte `ldrb`/`strb` K1's own byte-pointer-typed reconstruction implies.
5. `usbdc_setup_dispatch_buf`'s own indirection collapses to a single literal-pool load in
   K2 rather than K1's implied two-hop dereference — consistent with this project's
   broader pattern of these "singleton" globals resolving to one fixed, always-populated
   literal.
6. Three "write event code, no dispatch call, goto tail" cases (masked bits `0x20000`,
   `0x2`, `0x8`) are compiler-tail-merged into one shared physical instruction sequence in
   K2's compiled code — a codegen artifact, not a behavioral difference; represented as
   three separate cases in the reconstructed C, matching K1's own structure.

Incidentally, transcribing `usbdc_core_isr`'s own bus-reset branch also confirmed real K2
addresses for 5 of Section 3's 7 still-unbodied DMA/descriptor-table functions
(`usbdc_dma_engine_reset@0xc00035b4`, `usbdc_desc_table_global_init@0xc00036c8`,
`usbdc_desc_arm_slot@0xc000379c`, `usbdc_ep_arm_rx@0xc00037c4`, `usbdc_ep_arm_tx@
0xc00037f4`) via their own direct `bl` targets — bodies still not transcribed (out of this
pass's own scope), but the file's externs now cite real addresses instead of "NEEDS LIVE
QUERY." Section 4 (`usbdc_ep0_notify_tx_complete`/`_rx_complete`) remains fully open —
`usbdc_core_isr`'s own transcribed body doesn't call either directly.

**`usbdc_midi_status_glue.c` improved to 5/16.** Two more K1 functions located via live
xref-following: `chan_irq_toggle` (K2 `FUN_c00087d8`, found via `get_xrefs_to` on the
already-known `usbdc_ep_irqmask_set`/`_clear` pair — one of its 4 confirmed K2 callers is
`usbdc_ep_state7_handler`, `omap_l137_usbdc_ext.c`'s own sibling file, a real cross-file
confirmation) and `chan_ring_drain_pack` (K2 `FUN_c0008b04`, found by following
`chan_slot_dispatch.c`'s own already-identified K2 port-interrupt dispatcher, `FUN_
c000d6a0`, one hop to its callee). A real, confirmed K2-only difference: `chan_ring_
drain_pack` brackets its ring-counter read and decrement in IRQ-guard-shaped calls
(`FUN_c0004f40`/`FUN_c0004f50`) that K1's own reconstructed C shows no trace of. A
candidate false lead (`FUN_c00084f4`) was checked and explicitly rejected rather than
forced. Still genuinely open: `chan_status_promote_on_flag`, `chan_maybe_enable_irq4`,
`chan_status_notify`, `chan_status_byte_msb`, and the `chan_ring_entry_clear_0..3`
quartet — see the file's own header for search leads left for a future pass.

**Compile status**: 33/33 files still compile cleanly (`arm-none-eabi-gcc -fsyntax-only`)
after this pass's edits.

## MIDI cluster deep-dive: `usbdc_midi_status_glue.c` to 12/16, `chan_link_rt_queue_push` and `midi_hw_flush_alt` resolved — 2026-07-19 (second live pass, same day)

Second same-day dedicated single-session live Ghidra MCP pass, same "2-agent cap, no
further fan-out" authorization, closing the three concrete open items the sections above
left behind. Zero Agent-tool subagent calls made.

**`usbdc_midi_status_glue.c` improved from 5/16 to 12/16.** `search_bytes` on the exact
byte patterns of `chan_status_obj`'s (`0xC01CB344`) and `chan_ring_obj`'s (`0xC01CBA5C`)
own resolved literals located 7 more functions in one sweep: `chan_maybe_enable_irq4` (K2
`FUN_c0008b98`), `chan_status_notify` (K2 `FUN_c0008be8`), `chan_status_byte_msb` (K2
`FUN_c0008c14`), and the full `chan_ring_entry_clear_0..3` quartet (K2 `FUN_c0008c3c`/
`_c48`/`_c54`/`_c60`) — the last four sitting in a tight cluster immediately after
`chan_ring_drain_pack`'s own body and before `usbdc_ep_regblock_ptr_a`. Two real,
confirmed differences from K1: `chan_status_byte_msb`'s boundary test collapses (verified
by hand enumeration, same technique this project requires throughout) to `b >= 0x7f`, a
one-value shift from K1's `(b & 0x80) != 0` (`b >= 0x80`); `chan_ring_entry_clear_1/_2/_3`
(`+0x10`/`+0x1c`/`+0x28`) are now bracketed in the same IRQ-guard pair
(`irq_guard_enter_k2`/`_exit_k2`) `chan_ring_drain_pack` already uses, while `clear_0`
(`+4`) stays unguarded — an asymmetric addition K1 never had. Caller grouping by offset
parity (entries 0/2 from one caller, 1/3 from another) is confirmed IDENTICAL in shape to
K1's own quartet, just at new K2 addresses (`FUN_c000b760`/`FUN_c000f880`). Only
`chan_status_promote_on_flag` remains open — its K1 body never touches either object this
pass searched for (it uses a different, unrelated fixed handle instead), so a byte-search
literal sweep could not have found it by construction; `chan_dispatch_probe`'s own K2 body
(`FUN_c00117c8`, called by the newly-resolved `chan_maybe_enable_irq4`) is left as the
concrete lead for a future pass. Two live-bridge tool quirks worth flagging for future
sessions: `search_bytes` returns an opaque error instead of a genuine zero-match result
when a pattern has no hits anywhere in the image (caught via a digit-transposition typo
that looked identical to a real miss); `decompile_function` on an unbounded/non-function
address can silently return a DIFFERENT, unrelated real function's body rather than
erroring (how `chan_ring_entry_clear_2` was actually found, by accident, while probing a
branch target that turned out to have no function object of its own).

**`chan_param_ctrl.c`'s `chan_link_rt_queue_push` RESOLVED as a genuine K2-side
restructuring, transcribed in full — not left as an unfilled `extern` anymore.**
`decompile_function` on `FUN_c000ea68` (356 bytes) succeeded cleanly. K1's own version is
a small, self-contained "push one realtime byte into a 64-slot ring" primitive taking an
incoming byte parameter. K2's counterpart takes NO incoming byte — it now POPS its own
internal realtime ring (a new read-index field at `link+0x139`, immediately before the
pending-count field at `link+0x13a` K1 already had), packages the popped byte as a 4-byte
USB-MIDI Realtime CIN frame (`{0x0f, byte, 0, 0}`) and transmits it via `chan_link_tx`,
THEN separately drains ring 1 when it has pending data (optionally poking
`midi_hw_set_reg_f6` first) and arms/re-arms two IRQ-enable lines (3 and 4) based on two
independent readiness checks, finally conditionally acknowledging the link via
`chan_link_ack` — a combined "realtime service tick" that subsumes the old push
primitive's role entirely. Two of `chan_irq_toggle`'s own `which`/`enable` arguments at
this call site could not be recovered from the caller's own decompile (only 2 of 4
arguments visible, the rest presumably phantom-forwarded from live register state) —
transcribed honestly as explicitly-flagged unverified placeholders, per this project's own
established convention, rather than invented.

**`midi_engine.c`'s `midi_hw_flush_alt` RESOLVED; `midi_hw_flush_notify` remains open.**
`get_disassembly` on `midi_stream_decode_step`'s own "maybe_flush" tail found the real
`bl` target (`0xc000d5b4`) for a call this file already knew the shape of but had never
resolved an address for. Its real body is a bare, unconditional one-line forwarder to
`chan_link_ack` (`chan_param_ctrl.c`'s `FUN_c000d564`) — no independent flush logic
survives in K2, whatever K1's own (also never-decompiled) version may have done
independently. `midi_hw_flush_notify` was NOT found: `get_xrefs_to` on `chan_link_ack`
returns exactly 6 real callers, and the two not already otherwise accounted for
(`FUN_c000a308`/`FUN_c000ef8c`) both have real callers of their own — i.e. neither is a
bare thin-forwarder the way `midi_hw_flush_alt` is — so there is no positive evidence
pointing at either. Left open rather than guessed.

**Compile status**: 33/33 files still compile cleanly (`arm-none-eabi-gcc -fsyntax-only`)
after this pass's edits.

## `cpsoc.cpp`-adjacent stragglers: `clcdc_test_dispatch.c`, `cdix_autoswitch.c`,
## `omap_l137_addr_gap_misc.c` — 2026-07-19

Migrated K1's last three `cpsoc.cpp`-adjacent files. Ground truth:
`all_decompiled_k2.json`/`all_data_k2.json` via `query_dump_k2.py`, cross-checked with a
raw capstone/byte-pattern static search of the wrapped ELF image
(`kronos2s_v01r10_panel.elf`, using the same `k2disasm.py` helper the panel-scan-cluster
pass built) for two items the function-only dump doesn't cover — no live Ghidra MCP calls
this pass.

**`clcdc_test_dispatch.c` — CONFIRMED ABSENT from K2, no file created.** K1's factory
test-menu keypress dispatcher (`clcdc_test_pattern_dispatch`, keyed off a key-code byte
at `cpsoc`'s own scratch offset `0x821`, driving `clcdc_test_pattern` and two
`cad`-adjacent analog-arg setters via `cpsoc_led_set`/`_clear`) has no surviving K2
counterpart. Evidence, not assumption: `clcdc_test_pattern`'s own K2 port (already in
`clcdc.c`) has exactly ONE call site in the entire static dump (`FUN_c000a4bc`), and that
function is tick/counter-driven (a `param+0x30 > 2000` timeout gate, structurally similar
to a debounce/watchdog state, not a key-code switch) — not a keypress dispatcher. A raw
capstone disassembly of `PanelManager.cpp`'s own two uninvestigated anchor xrefs left
open by the panel-scan-cluster pass (`0xc00061cc`, `0xc00066f8`) independently rules both
out as candidates: the first is a switch/knob scan-event encoder (mode-selected calls
into analog-read/switch-scan helpers, no `0x821`-shaped offset access, no
`0x1f-0x25`/`0x18-0x1e`/`0x3a-0x3d`/`0x42-0x49` key-range switch), the second is a
MIDI-status-byte-shaped dispatcher (compares against `0x80`/`0x90`/`0xa0`/`0xa4`, classic
MIDI channel-voice status ranges). Consistent with `cpsoc.cpp` itself — the whole
scratch-struct/LED-set-clear mechanism this dispatcher depended on — being confirmed
entirely absent from K2's string table already (see the anchor-string table above): the
factory test-menu system this file exposed appears to be genuinely gone in K2, not
relocated. `clcdc_test_pattern` itself (the underlying "run test-pattern mode N"
primitive) does survive, already ported in `clcdc.c`, just with a different (tick-driven,
not keypress-driven) caller.

**`cdix_autoswitch.c` — ported cleanly**, all three functions (`cdix_set_format_reg`/
`cdix_apply_mode_table`/`cdix_reset_and_configure`) EXACT byte-size matches against K1
(20/20, 76/76, 120/120), structurally identical operation-for-operation. New finding this
pass: K2's own outer CDIX auto-switch state machine (`FUN_c0010380`/`FUN_c0010258`, K2
counterparts of K1's `FUN_c000f01c`/`FUN_c000f0c8` — untraceable in K1's own static dump,
directly covered in K2's) also directly toggles `omap_l137_addr_gap_misc.c`'s own McASP2
bit-flag helpers (`mcasp2_set_bit25`/`_bit15`) around the CDIX reset call — a real,
previously-undocumented hardware coupling between the digital-audio-interface reset path
and the second McASP instance, most plausibly a shared clock/reset domain. Also: the
shared context-handle constant this cluster and `mcasp_init` (`mcasp.c`) and
`clcdc_display_object_init` (`omap_l137_addr_gap_misc.c`) all key off, `0xC00E004C`, sits
inside the same fixed `0xC00E0000`-based SRAM page `soc_irq_gate.c`'s own port already
confirmed as real OMAP-L138/DA850 hardware — three otherwise-unrelated subsystems share
one physical bookkeeping page.

**`omap_l137_addr_gap_misc.c` — 5 of 6 K1 clusters ported, 1 confirmed genuinely absent.**
Cluster-by-cluster: (1) McASP2 reduced-reinit — EXACT size match ×3, and directly
confirmed by `mcasp.c`'s own K2 port, which already cites this cluster's top function
(`mcasp_reinit_reduced`) but deliberately left it bodyless pending this file, the same
collision-avoidance split K1 used between its own two files. (2) `usbdc_gap_config_slot`
— structurally identical, subsystem still unattributed, same as K1. (3) The UART-shaped
register-pair cluster — **CONFIRMED ABSENT**, not a coverage gap: a full-image raw byte
search (all `0xC0000000`-`0xC00E0000`, i.e. beyond the static dump's own function-only
coverage) for all four of K1's distinctive literal immediates (`0xe00`/`0xe01`/`0xf01`
mode selectors, `0x4a10`/`0x14a10` baud/divisor pair) found zero occurrences of any of
them anywhere in K2's binary, code or data. Whatever peripheral K1 drove through this
register shape is either genuinely dropped in K2 or reconfigured through a completely
different, unsearched register layout. (4) Two tiny bit-extraction helpers — both found,
plus one real K2-only addition (`gap_store_low_byte`, the obvious write-side counterpart
K1's own file never documented). (5) The default 256-entry RGB565 palette loader —
found, ported; **cross-build confirms a K1 open question**: K1's own suspicious
`0x752ff`-iteration remap-loop bound (documented as "cannot be correct firmware
behaviour" but left untouched rather than silently corrected) resolves to the EXACT SAME
value in K2's independently-compiled image — strong evidence this is a genuine (if still
unexplained) compiled constant, not a per-build Ghidra data-type misinference. (6) The
large struct-zero-init function — EXACT size match, and (same collision-avoidance pattern
as cluster 1) directly confirmed by `mcasp.c`'s own K2 port, which cites it as
`mcasp_clock_step_a` but left it bodyless pending this file.

See each file's own header comment in `K2_V01R10/` for full per-cluster detail, DAT_
resolution, and STILL OPEN items.

## 2026-07-19 — live Ghidra MCP follow-up on the panel-scan cluster

The 2026-07-18 panel-scan pass (`panel_manager.c`, `panel_scan_updater.c`,
`switch_chattering_detector.c`, `system_info_holder.c`, `omap_l108_syscfg.c`) was done
entirely without live Ghidra access. This pass had one-agent-only, read-only live
Ghidra MCP access to `kronos2s_v01r10_panel.elf` and used it to resolve nearly every
NEEDS LIVE QUERY item that pass left open. Headline finding: the live Ghidra database
has substantially more Function objects than the pre-fetched static dump the prior
pass was limited to — several "never function-ified" addresses turned out to have
real decompiles once queried live.

Resolved: `panel_manager.c`'s full 73-entry LED bitmap table (byte-exact, no `0xff`
sentinel actually present anywhere — that theory was wrong), both previously-open
anchor xrefs (one is a scan-event encoder, the other a 312-instruction message
dispatcher confirmed as the real caller of `switch_chattering_detector.c`'s own two
functions), and real callers for the LED-bit functions (a diagnostic scroller plus a
wire-protocol opcode dispatcher). `panel_scan_updater.c`'s four sub-steps are now
fully transcribed real SPI/TWI handshakes (not bare externs), with a real caller found
and `apply()`'s signature corrected to a (hex-stream pointer, length) pair.
`switch_chattering_detector.c`'s register/remove indexing discrepancy is now
Ghidra-decompile-confirmed as genuinely real, not a transcription artifact.
`system_info_holder.c`'s both open anchor-adjacent items resolved: a bounds-checked
table accessor, and the "SYSTEM STARTUP FAILED" fault screen with its full text
recovered verbatim. `omap_l108_syscfg.c` gained real callers for two functions,
byte-verified three more against live memory contents, found one brand-new 7th leaf,
and had one function (`omap_syscfg_set_reg118`) REMOVED as a cross-file misattribution
of `soc_periph.c`'s own `board_desc_set_pinmux_3word()` — closing a follow-up
`soc_periph.c`'s own header had explicitly flagged as needed.

See each of the 5 files' own headers in `K2_V01R10/` for full per-item detail and the
still-genuinely-open remainder (mostly: a handful of uncharacterized callees, and
several `panel_fault()` line numbers Ghidra's own decompile elided rather than
recovered).

## Shared-driver-layer migration: substantially complete

As of this pass, every K1 file NOT already flagged as one of the four confirmed-obsolete
`cpsoc.cpp`/`cad.cpp`/`cpsoc_issp.cpp`/`ctouchpanel.cpp`-adjacent files (`cad.c`,
`cpsoc.c`, `cpsoc_issp.c`, `ctouchpanel.c` — none of which have a K2 counterpart and none
of which are expected to, per the anchor-string table above) has now been either ported
into `K2_V01R10/` or explicitly confirmed absent with documented evidence. The shared
low-level driver layer, the new panel-scan architecture, the IRQ-gate/second-I2C-bus
infrastructure, the full MIDI transport cluster, and the remaining `cpsoc.cpp`-adjacent
stragglers (this section) are all now accounted for. Remaining work is depth, not
breadth: several files carry their own NEEDS LIVE QUERY items (table contents, a handful
of unresolved callers, `eva_crt0_tick_glue.c`'s own K1 role not yet checked against K2) —
see `K2_V01R10/README.md`'s own running status for the complete list.

## 2026-07-19 — second independent live-Ghidra pass: NEEDS LIVE QUERY sweep

A second, independent live-Ghidra-MCP pass (single agent, read-only tools only, run
under this project's 2-agent cap with zero subagent spawning), separate from the
same-day panel-scan follow-up above. Purpose: re-verify `omap_l108_syscfg.c`'s own
"4 of 6 functions have no caller" finding from scratch rather than trust the prior
pass's self-report, then sweep the whole `K2_V01R10/` tree for remaining `NEEDS LIVE
QUERY` markers and resolve as many as practical.

**`omap_l108_syscfg.c` re-verification**: independently reproduced the prior pass's
core finding (zero `get_xrefs_to` hits for `reg130_a`/`reg130_b`/`dual_pull_enable`/
`clear_pull_enable_0xc`) and went one step further on the specific hypothesis the task
asked to test — a function-pointer table in the style of `eva_board_init_table`/
`task_sched.c`'s ROM task table. A full-binary `search_bytes` sweep for each orphan's
exact raw little-endian address found **zero occurrences anywhere in the image**,
actively ruling out that specific mechanism rather than leaving it a hand-wave. Also
mechanically pinned down *why* no xref exists: Ghidra's own database has never
disassembled `0xc0001c04`-`0xc0001c4c` as Instructions at all (`get_disassembly`
requested directly at those addresses silently skips to the next address that IS real
code) — there is no Instruction/Function object for the xref engine to compute
against, a stronger diagnosis than "unattributed." One methodology note recorded
honestly: this pass's own `read_memory`/`search_bytes` calls against that same address
range were noticeably flakier (intermittent "script produced no output" errors, cleared
on retry) than the prior pass reported — treated as live-bridge flakiness under
concurrent load, not as a data problem; every byte re-read this pass matched the
existing hand-transcription exactly.

**Tree-wide sweep, resolved this pass**:
- `cdix4192.c` + `cdix_autoswitch.c` — all 5 clustered CDIX register tables
  (`cdix_config_table`, `cdix_mode0_table`, `cdix_mode1_table`, `cdix_reset_table_a`,
  `cdix_reset_table_b`) read byte-exact via live `read_memory` and confirmed
  byte-contiguous end-to-end in `.rodata` (each table's terminator lands exactly at
  the next table's independently-known start address — strong self-consistency
  evidence). Real findings: mode0/mode1 tables differ in exactly 2 of 13 entries;
  `reset_table_a`'s 12 entries are reconstructible as `config_table`'s opening pair
  plus `mode1_table`'s own tail, bit-for-bit.
- `mcasp.c` — `0xc0002738` confirmed via live disassembly to be real code (not just
  xref-inferred): the predicted `mcasp_configure_clock`/`mcasp_configure_pins` call
  pair, plus a real, non-trivial continuation writing further literal-pool constants.
- `omap_l137_usbdc_ext.c` — Section 3's last two unaddressed functions resolved with
  real K2 addresses, bodies, and confirmed callers: `usbdc_desc_set_length`
  (`0xc0003680`, caller `FUN_c000432c`) and `usbdc_desc_get_length` (`0xc00036a8`,
  callers include `usbdc_core_isr` itself, directly confirming that file's own prior
  note). All 7 Section-3 functions now have confirmed K2 addresses.
- `omap_l137_usbdc_ep0.c` — `FUN_c000a7dc` (the `cad_pedal_present` candidate) live
  decompiled and given a real confirmed caller (`FUN_c0009838`); best-evidence
  conclusion is that it IS `cad_pedal_present`'s K2 replacement, expanded from a
  boolean to a 3-valued pedal-type classifier while keeping K1's own bit-0x17 gate.
- `panelbus_dispatch.c` — `panelbus_hw_bringup`'s previously-unbounded continuation
  (`0xc00008d4`-`0xc0000904`) fully disassembled, correcting the prior "symmetric
  I2C0+I2C1 pair" speculation: it actually repeats the SAME idx=1/I2C1 setup a second
  time via a tail branch, not a mirrored I2C0 pass. Separately, `get_function_info` on
  `i2c0_i2c1_base_select` confirms exactly one caller in the whole binary
  (`panelbus_hw_bringup`), definitively closing whether any further K2 dispatcher
  consumes this selector (answer: no).

**Checked but left honestly open** (true negatives, not un-investigated): `omap_gpio.c`'s
question about K1's missing DIR/OUT_DATA defaults — `gpio_bank_get_base` still has
exactly the same 30 confirmed callers as before, none individually re-examined this
pass (would require checking each body); `omap_l108_syscfg.c`'s 4 orphan functions'
real caller mechanism remains unresolved even after the table hypothesis was
falsified. See each file's own header in `K2_V01R10/` for full detail. `make` re-verified
clean, 33/33, after all edits above. Zero Agent-tool subagent calls made this pass.

## 2026-07-19 — third live-Ghidra pass: `task_sched.c` ROM table dumped, `eva_board_main` resolved as split across 3 autostarted tasks

A third, independent same-day live-Ghidra-MCP pass (read-only tools against
`kronos2s_v01r10_panel.elf`, same "2-agent cap, no subagent fan-out" authorization,
zero Agent-tool calls made), targeting `task_sched.c`'s own single highest-priority
open item: whether `eva_board_main` is itself one of the ROM-table auto-started tasks.

**HEADLINE FINDING**: `read_memory` on `sched_tcb_table_init_and_autostart`'s own
resolved literals dumped K2's entire ROM autostart table byte-exact — exactly 3 tasks
(ids 1/2/3, priorities 0/2/4). Hand-disassembling each task's own entry point (Ghidra
has no Function object anywhere in this address range, same recurring artifact this
project has hit repeatedly) resolves all three, and the answer to the open question is
more interesting than a yes/no: **`eva_board_main` does not exist as a single K2
function at all.** It has been split into two of the three autostarted tasks —
task id=1 (priority 0, most urgent) runs `eva_board_init_table`'s own walk-and-dispatch
loop, then `eva_board_final_setup`, then loops **forever** calling
`eva_board_boot_status_dispatch` (a genuine 2-instruction infinite loop, confirmed by
hand-decoding the branch back); task id=2 (priority 2) is a separate stub that calls
`eva_board_main_loop` (the real `master_dispatch_tick` forever-loop). This directly
corrects `eva_board_main.c`'s own current reconstruction, which presents these as one
straight-line function calling all four steps in sequence — that reconstruction was
built purely from `get_xrefs_to` call-site addresses, never from one linear
disassembly, and this pass's byte-level trace shows the boot-status-dispatch loop
never falls through to the main-loop call. **Not edited into `eva_board_main.c`**, per
that file's own instructions and this project's established convention — reported here
for a future consolidation pass.

**Bonus resolution, `eva_board_main.c`'s own separate open item**: task id=3 (priority
4, lowest — structurally unreachable in normal operation since neither of the other
two tasks ever both block) is an immediate, unconditional `crypto_at88_fault` call
citing `"../EvaBoardMain.cpp"` line `0x70` — this IS the K2 counterpart of
`eva_board_watchdog_fault_wrapper`, the function `eva_board_main.c`'s own header said
could not be independently located as the anchor string's only xref. Found via the
same ROM-table dump, not a separate search.

**Other resolutions this pass**: a real transcription bug in `task_sched.c`'s own
`sched_task_create_and_ready` (an erroneous extra pointer addition) was found and
fixed via `decompile_function`, which also definitively resolves K1's own long-open
ambiguity about which of a task's two ROM-pushed stack words is the real jump target
(it's cfg+8, not cfg+4 — settled using the ROM table's own real data, all of whose
cfg+8 values are code addresses). The delay-heap sift-up/sift-down internals'
real K2 addresses are now confirmed via `sched_delay_heap_extract_min`'s own call
graph. A new, real function was found near `sched_remove_from_ready`'s own cluster
(~`0xC001AA98`) that resets the current task's TCB after removing it from ready —
resolves a specific address K1's own file cited but never traced, though it does not
match either `eva_board_sched_ready`'s or `eva_board_sched_requeue`'s own K1 shape, so
neither is claimed as found. Separately, `cobjectmgr.c`'s own
`cobjectmgr_hardware_fault_watchdog` search was independently re-checked from two
angles (live xref re-query on the ack primitive, and the same ROM-table dump) — both
came back negative, but concretely so: the watchdog is confirmed to be none of K2's 3
autostarted tasks, ruling out that mechanism rather than leaving it unswept.
`heap_alloc.c`'s `heap_free` was also fully live-decompiled (previously matched by
shape only), adding real detail on `heap_state`'s own bitmap/top-chunk/bin-index
layout and heap_trim's real trigger threshold and pad argument. Full per-file detail,
addresses, and byte-level citations in `task_sched.c`, `cobjectmgr.c`, and
`heap_alloc.c`'s own header comments in `K2_V01R10/`. `make` re-verified clean, 33/33,
after all edits above.

---

## 2026-07-19 — fourth live-Ghidra pass: `soc_irq_gate.c` table+0x00 correction, `wire_dispatch.c` pedal-send/opcode-0xc6 resolution, `omap_gpio.c` DIR/OUT_DATA question closed

A fourth, independent same-day live-Ghidra-MCP pass (read-only tools against
`kronos2s_v01r10_panel.elf`, same "2-agent cap, no subagent fan-out" authorization,
zero Agent-tool calls made), targeting the remaining `NEEDS LIVE QUERY`/"still open"
items in `soc_irq_gate.c`, `wire_dispatch.c`, and `omap_gpio.c`.

**`soc_irq_gate.c`**: `FUN_c0000040` (the ch-0x15/Timer64P0 lazy-init AINTC-enable
singleton this file has cited-but-never-defined since its first pass) is now DEFINED,
in `soc_periph.c` as `timer64p0_enable_ch15` — genuinely that file's own subsystem, not
an AINTC-gate leaf. Live decompilation of it surfaced and CORRECTED a real, confirmed
error this file had carried since its first pass: `soc_irq_gate_timer0_quiesce`'s own
cached-handle literal is `table+0x00` (`0xC00E0000`), not `table+0x08` as previously
claimed — confirmed via direct `read_memory`, not assumed. This also retracts a
downstream "same slot as the CLUSTER 2 mcasp param cache" claim, which was only true by
coincidence of a wrong address on one side (CLUSTER 2's own `0xC00E0008` was independently
re-verified correct and unchanged). A genuinely new, previously-undocumented 72-byte code
region was found sitting in the gap between the two now-corrected functions
(`0xc0000098`-`0xc00000df`, no Ghidra Function object) — manually characterized from raw
disassembly as sharing CLUSTER 1's own bit-set idiom plus a report-code call, reached only
via a `PARAM`-type reference from `task_sched.c`'s own already-cited `FUN_c00199dc`
(strongly suggesting it's a `task_sched.c` ROM-autostart-table entry) — flagged for that
file, not claimed here. The long-standing `gap_slot_bringup` `NEEDS LIVE QUERY` was
investigated via `get_xrefs_to` on `omap_l137_addr_gap_misc.c`'s own claimed
`usbdc_gap_config_slot` address (`0xc0002d80`) — and this SURFACED A REAL CROSS-FILE BUG:
that address is actually `panelbus_dispatch.c`'s own already-decompiled and independently
confirmed `panelbus_i2c_mode_config`, not `usbdc_gap_config_slot` at all (confirmed via a
direct `decompile_function` call showing an I2C ICMDR-shaped config write, nothing
USB/gap-shaped). Flagged in `soc_irq_gate.c`'s own header for whoever next touches
`omap_l137_addr_gap_misc.c`; NOT fixed there (out of this pass's own assigned files).
`usbdc_gap_config_slot`'s real K2 address remains unlocated. `slot0x00_get`/
`ring3_state_reset` remain unlocated, but with a fuller negative case now on record (a
full-image `search_bytes` sweep for the `table+0x00` literal found only the 4 already-
known real consumers, none a bare getter wrapper).

**`wire_dispatch.c`**: `eva_wire_pedal_send`'s real signature is 3-parameter, not 2 —
`decompile_function` on both the callee and `wire_dispatch_command` itself confirms the
long-flagged "distinguishing 2nd argument" between opcode reg `0x50` and `0x52` is a real
3rd parameter (`1`="set" / `0`="clear" into a `(word_slot, bit_pos)`-table-addressed bit,
the same idiom this project's own `panel_manager_set_led_bit` already established) — both
call sites corrected. `FUN_c0008d24` (the opcode `0xc6` continuation resolver, previously
only role-confirmed) is now structurally characterized in real detail via
`decompile_function` + `read_memory`: an 8-bit-indexed-pixel-to-16-bit LUT expansion (a
confirmed 256-entry table at `0xC001B814`) feeding a `0x321`-halfword circular ring buffer
— genuinely a different mechanism from the opcode `0xc4` continuation's own 800-wide
framebuffer cursor, not the same ring as previously left ambiguous. Not transcribed
statement-for-statement (dense, ~140 real statements) per this project's established
practice for code this shape.

**`omap_gpio.c`**: the file's own headline "still open" question — whether K1's missing
pair-1/3/4 GPIO DIR/OUT_DATA boot-time defaults exist somewhere else in K2 — is now closed
with reasonable confidence in the negative. All 30 confirmed callers of
`gpio_bank_get_base` (`get_xrefs_to`) were individually examined (matched against an
already-documented file, or freshly `decompile_function`-ed): none writes to the pair-1/3/4
absolute offsets (`+0x38`, `+0x88`/`+0x8c`, `+0xb0`/`+0xb4`). A supplementary full-image
`search_bytes` sweep for K1's own three DIR default constants, as raw little-endian
literal-pool bytes, found zero hits (with the honest caveat that an ARM MVN-immediate
encoding could hide an equivalent constant from a byte search). Two functions incidentally
decompiled during this sweep (`FUN_c0000ec8`/`FUN_c0000ef0`) turned out to already be
fully documented in `i2c_by_gpio.c` as `i2c_gpio_set_scl`/`i2c_gpio_set_sda` — cross-checked
before being (nearly) mis-added as new.

`make` re-verified clean, 33/33, after all edits above.

---

## Status

| Item | Status |
|---|---|
| Container header | Diffed against K1 byte-for-byte; product tag/version fields decoded, two single-byte deltas in still-unidentified fields newly noted (see table above), same two fields (`0x2c`, `0x40`) remain undecoded as in K1 |
| Load address / architecture | Confirmed: ARM, `0xC0000000`, same reset-vector table shape as K1 (reset target address differs as expected: `0xC000A860` vs K1's `0xC0009534`) |
| Firmware source-file inventory | Confirmed via anchor-string comparison against K1 — 9 shared files, 4 K1-only (gone), 5 K2-only (new), `cad.cpp` status unchecked |
| Shared driver-layer C reconstruction | This pass — see `K2_V01R10/README.md` for per-file detail |
| New panel-scan architecture (`PanelManager.cpp`, `PanelScanUpdater.cpp`, `SwitchOnChatteringDetector.cpp`, `SystemInfoHolder.cpp`, `OmapL108Syscfg.cpp`) | First pass done via manual capstone disassembly; 2026-07-19 live-Ghidra follow-up resolved nearly every NEEDS LIVE QUERY item (full LED table, real callers, corrected signatures, one cross-file misattribution removed) — see "live Ghidra MCP follow-up" section above and `K2_V01R10/README.md` |
| IRQ-gate leaf cluster / second I2C bus (`soc_irq_gate.c`, `panelbus_dispatch.c`) | 2026-07-19 pass — `soc_irq_gate.c` near-identical port with real, documented differences; `panelbus_dispatch.c` confirmed to have no surviving higher-level dispatcher in K2, only the boot-time hardware bring-up primitive — see section above |
| Switch/LED name table, boot splash, wire-protocol opcode table, PSoC protocol | K1's own versions NOT re-verified against K2 — do not assume unchanged. K2's own switch/LED name table (77 entries, grown from K1's 73) IS now located, in `PanelManager.cpp` — see above |
| MIDI subsystem cluster (`midi_engine.c`, `chan_link_hw.c`, `chan_param_ctrl.c`, `chan_slot_dispatch.c`, `uart1_midi_queue.c`, `midi_chan_status_queues.c`, `usbdc_midi_status_glue.c`) | 2026-07-19 pass — 6 of 7 files ported with high confidence (mostly exact-byte-size K1↔K2 matches); `usbdc_midi_status_glue.c` improved to 12/16 K1 functions located (only `chan_status_promote_on_flag` still open), `chan_link_rt_queue_push` resolved as a genuine K2-side restructuring (not a 1:1 port), `midi_hw_flush_alt` resolved (bare forwarder to `chan_link_ack`, `midi_hw_flush_notify` still open) — all via a second same-day live follow-on pass, see "MIDI cluster deep-dive" section above |
| `usbdc_core_isr` (`omap_l137_usbdc_ext.c` Section 6) | RESOLVED — 2026-07-19 live-pass follow-on. Manually transcribed from raw disassembly (Ghidra never bounded it as a Function); near-exact structural match to K1 with 6 confirmed real differences documented in the function's own header and the "`usbdc_core_isr` resolved" section above |
| `cpsoc.cpp`-adjacent stragglers (`clcdc_test_dispatch.c`, `cdix_autoswitch.c`, `omap_l137_addr_gap_misc.c`) | 2026-07-19 pass — `clcdc_test_dispatch.c` confirmed absent (no K2 file); `cdix_autoswitch.c` ported cleanly, 3/3 exact-size matches, one new McASP2/CDIX coupling found; `omap_l137_addr_gap_misc.c` 5/6 K1 clusters ported, 1 (UART-shaped register pair) confirmed genuinely absent via full-image byte search — see section above |
| Shared-driver-layer migration | Substantially complete as of this pass — every K1 file outside the four confirmed-obsolete `cpsoc.cpp`/`cad.cpp`/`cpsoc_issp.cpp`/`ctouchpanel.cpp`-adjacent files is now ported or explicitly confirmed absent; remaining work is depth (NEEDS LIVE QUERY items), not breadth — see section above |
| `eva_board_main`'s own ROM-table autostart fate (`task_sched.c`) | RESOLVED — third live-Ghidra pass, 2026-07-19. `eva_board_main` does not exist as one K2 function: split across 2 of K2's 3 ROM-table-autostarted tasks (init_table+final_setup+boot_status_dispatch-forever task, and a separate main_loop task); the 3rd, lowest-priority task resolves `eva_board_main.c`'s own previously-unlocated `"../EvaBoardMain.cpp"` xref as `eva_board_watchdog_fault_wrapper`'s K2 counterpart. See section above and `task_sched.c`'s own header |
| `soc_irq_gate.c`/`wire_dispatch.c`/`omap_gpio.c` deep-dive | Fourth live-Ghidra pass, 2026-07-19. `FUN_c0000040` defined (`soc_periph.c::timer64p0_enable_ch15`), correcting a real `table+0x08`→`table+0x00` address error this file carried since its first pass; `gap_slot_bringup` still unlocated but a real cross-file address-collision bug surfaced in `omap_l137_addr_gap_misc.c` (flagged there, not fixed); `wire_dispatch.c`'s `eva_wire_pedal_send` 3rd argument and opcode-`0xc6` continuation resolver both resolved in detail; `omap_gpio.c`'s DIR/OUT_DATA question closed (reasonable-confidence negative) via an exhaustive 30-caller sweep. See section above and each file's own header |
