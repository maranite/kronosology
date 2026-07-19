/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l137_usbdc_ext.c - K2 (KRONOS2S_V01R10.VSB) port of the USB
 * device-controller code outside omap_l137_usbdc.c's own anchor range:
 * low-level endpoint register/FIFO access, and (where the static dump
 * allows) the endpoint-event dispatcher and its wire_dispatch_command call
 * site.
 *
 * Source: K1_V06R06/omap_l137_usbdc_ext.c (READ-ONLY baseline, not edited).
 * Ground truth: static Ghidra dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json, 2026-07-18) for Sections 1/2/5/7;
 * Section 6 (usbdc_core_isr) RESOLVED 2026-07-19 via a dedicated single-
 * session live Ghidra MCP bridge pass (safe this one time under this
 * project's own "2-agent cap, no further fan-out" constraint) - manual
 * instruction-level transcription from get_disassembly, since Ghidra's
 * auto-analysis never bounded this address range as a function (decompile_
 * function/get_function_info both report "no function found" here, the same
 * artifact this project has hit before for eva_board_main.c's own crt0 chain
 * in K1 - not something to "fix" in Ghidra itself, per this task's own
 * constraints, just transcribed by hand from the raw listing).
 *
 * COVERAGE SUMMARY - Section 6 is now resolved; the remaining real gap is
 * Sections 3/4:
 *
 *  CONFIRMED AND PORTED (Sections 1, 2, 5, 7 - all structurally verified
 *  against K2's actual decompile, register offsets/bit patterns
 *  cross-checked identical to K1: 0x40e INDEX, 0x412/0x416 indexed TXCSR/
 *  RXCSR, 0x502/0x506 direct-window CSR, 0x418 RXCOUNT, 0x420 FIFO port -
 *  every one of these independently DAT_-resolved this pass and found
 *  identical to K1's own values):
 *   - Section 1 (low-level register access): all 14 helpers located and
 *     confirmed byte-for-byte structurally identical to K1.
 *   - Section 2 (FIFO read/write): both functions located and confirmed
 *     structurally identical to K1, including the real "FIFO port register
 *     re-read each word, not an incrementing pointer" idiom K1 documents.
 *   - Section 5 (usbdc_flush_ep1_4): located and confirmed structurally
 *     IDENTICAL to K1 down to the specific compiler quirk K1's own file
 *     calls out by name - endpoint 2's index is computed via
 *     `(char)(txcsr_offset - 0x10)` byte-truncated arithmetic rather than
 *     the literal 2 the other 3 endpoints use directly. Reproduced here the
 *     same way K1's own file does: as a small static helper called 4 times,
 *     since the real compiled code in BOTH images is a fully unrolled 4x
 *     copy, not a real loop (a reconstruction-readability choice, not a
 *     functional difference, per K1's own stated rationale).
 *   - Section 7 (usbdc_endpoint_event_dispatch, usbdc_ep_recv_bulk):
 *     both located and confirmed structurally identical in shape to K1
 *     (same event-code switch cases, same bulk-OUT clamp-to-0x200-then-
 *     fifo_read-then-wire_dispatch_command chain).
 *
 *  NOT FOUND - genuine, confirmed data gap, not merely "not reached this
 *  pass":
 *   - Section 3 (DMA/endpoint descriptor-table writers: usbdc_dma_engine_
 *     reset, usbdc_desc_set_length, usbdc_desc_get_length,
 *     usbdc_desc_table_global_init, usbdc_desc_arm_slot, usbdc_ep_arm_rx,
 *     usbdc_ep_arm_tx in K1) - still not present as bounded Ghidra Function
 *     objects in the static dump (same "no function found" artifact as
 *     Section 6 had). HOWEVER, this pass's own manual transcription of
 *     usbdc_core_isr's bus-reset branch (Section 6, below) INCIDENTALLY
 *     confirms 5 of these 7 functions genuinely exist and pins down their
 *     real K2 addresses via direct `bl` targets in the raw disassembly:
 *     usbdc_dma_engine_reset @0xc00035b4, usbdc_desc_table_global_init
 *     @0xc00036c8, usbdc_desc_arm_slot @0xc000379c, usbdc_ep_arm_rx
 *     @0xc00037c4, usbdc_ep_arm_tx @0xc00037f4 (usbdc_desc_set_length/
 *     usbdc_desc_get_length were NOT independently re-confirmed this pass -
 *     core_isr's own bus-reset branch never calls either directly, only the
 *     still-unresolved Section-4 completion helpers do in K1). Bodies NOT
 *     transcribed here - out of this pass's own assigned scope (Part 1
 *     asked only for usbdc_core_isr itself) - but the externs below are
 *     upgraded from "no confirmed K2 address" to citing these real,
 *     disassembly-confirmed addresses.
 *   - Section 4 (EP0 completion helpers: usbdc_ep0_notify_tx_complete,
 *     usbdc_ep0_notify_rx_complete) - still a genuine, unresolved gap.
 *     usbdc_core_isr's own transcribed body does NOT call either directly -
 *     only usbdc_endpoint_event_dispatch's own case 0xb does (Section 7,
 *     already in this file, via its own separately-named FUN_c00080e4/
 *     FUN_c0008394/FUN_c00084e8/FUN_c00084c4 - never reconciled with these
 *     two K1 names, unchanged this pass, see Section 7's own note).
 *   - Section 6 (usbdc_core_isr) - RESOLVED this pass, see below.
 *
 *  RESOLVED 2026-07-19 (dedicated single-session live Ghidra MCP bridge
 *  access, authorized once for this task only): usbdc_core_isr's real
 *  function body starts at 0xc0003840 and ends with its own `mov pc,lr` at
 *  0xc0003fd0 (occupying bytes 0xc0003fd0-0xc0003fd3), immediately followed
 *  with NO GAP by usbdc_flush_ep1_4's own already-confirmed start at
 *  0xc0003fd4 - closing the 1940-byte hole (0xc0003840-0xc0003fd4, 0x794
 *  bytes) completely and exactly, matching this file's own earlier
 *  "immediately before usbdc_flush_ep1_4" prediction to the byte. Manually
 *  transcribed instruction-by-instruction
 *  from get_disassembly (Ghidra's own decompiler still reports "no function
 *  found" here - the boundary was never fixed, per this task's own
 *  constraint against mutating Ghidra's analysis state). See the function's
 *  own definition below (Section 6) for the full transcription and every
 *  confirmed K1/K2 difference found.
 */

#include <stdint.h>
#include <stdbool.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);
extern void *wire_dispatch_command(void *handle, uint8_t *cmd, unsigned len);	/* K2 FUN_c0009b54, wire_dispatch.c's own K2 port */
void usbdc_ep_recv_bulk(void);	/* FUN_c000bc1c (K2), defined in Section 7 below - forward-declared for
				 * usbdc_endpoint_event_dispatch's own case 5, matching K1's own convention */

/* ===========================================================================
 * Section 1 - low-level endpoint register access. Every helper below
 * CONFIRMED structurally identical to K1: same INDEXED-window (dev+0x40e
 * INDEX, dev+0x412/+0x416 TXCSR/RXCSR-at-INDEX) vs DIRECT/flat-window
 * (dev+ep*0x10+0x502/+0x506) split, same bit positions throughout.
 * =========================================================================== */

/* usbdc_select_endpoint - writes INDEX (dev+0x40e). @0xc0003830 (K2).
 * K1: @0xc0003e10. */
void usbdc_select_endpoint(void *dev, uint8_t ep)	/* FUN_c0003830 (K2) */
{
	*((uint8_t *)dev + 0x40e) = ep;
}

/* usbdc_ep0_csr0_set_bits / usbdc_ep0_csr0_test_setupend - fixed EP0 CSR0
 * (dev+0x502). @0xc0004468 / @0xc0004480 (K2). K1: @0xc0004a34 / @0xc0004a4c. */
void usbdc_ep0_csr0_set_bits(void *dev)	/* FUN_c0004468 (K2) */
{
	*(uint16_t *)((uint8_t *)dev + 0x502) |= 0x20;
}

uint16_t usbdc_ep0_csr0_test_setupend(void *dev)	/* FUN_c0004480 (K2) */
{
	return (*(uint16_t *)((uint8_t *)dev + 0x502) >> 2) & 1;
}

/* Direct/flat per-endpoint CSR bit helpers - CONFIRMED same 0x502/0x506
 * direct windows, same bit positions as K1.
 * @0xc0004424 / @0xc0004444 / @0xc0004498 / @0xc00044c0 / @0xc00044ec /
 * @0xc000450c / @0xc0004550 (K2).
 * K1: @0xc00049f0 / @0xc0004a10 / @0xc0004a64 / @0xc0004a8c / @0xc0004ab8 /
 * @0xc0004ad8 / @0xc0004b1c. */
void usbdc_txcsr_direct_set_txpktrdy(void *dev, uint8_t ep)	/* FUN_c0004424 (K2) */
{
	uint8_t *p = (uint8_t *)dev + (ep & 0xff) * 0x10;
	*(uint16_t *)(p + 0x502) |= 0x10;
}

void usbdc_rxcsr_direct_set_bits(void *dev, uint8_t ep)	/* FUN_c0004444 (K2) */
{
	uint8_t *p = (uint8_t *)dev + (ep & 0xff) * 0x10;
	*(uint16_t *)(p + 0x506) |= 0x20;
}

void usbdc_txcsr_direct_clear_bits(void *dev, uint8_t ep)	/* FUN_c0004498 (K2) */
{
	extern uint16_t usbdc_txcsr_clear_mask;	/* K2 DAT_c00044bc */
	uint8_t *p = (uint8_t *)dev + (ep & 0xff) * 0x10;
	*(uint16_t *)(p + 0x502) &= usbdc_txcsr_clear_mask;
}

void usbdc_rxcsr_direct_clear_bits(void *dev, uint8_t ep)	/* FUN_c00044c0 (K2) */
{
	extern uint16_t usbdc_rxcsr_clear_mask;	/* K2 DAT_c00044e8 */
	uint8_t *p = (uint8_t *)dev + (ep & 0xff) * 0x10;
	*(uint16_t *)(p + 0x506) &= usbdc_rxcsr_clear_mask;
}

uint16_t usbdc_txcsr_direct_test_bit5(void *dev, uint8_t ep)	/* FUN_c00044ec (K2) */
{
	uint8_t *p = (uint8_t *)dev + (ep & 0xff) * 0x10;
	return (*(uint16_t *)(p + 0x502) >> 5) & 1;
}

uint16_t usbdc_rxcsr_direct_test_bit6(void *dev, uint8_t ep)	/* FUN_c000450c (K2) */
{
	uint8_t *p = (uint8_t *)dev + (ep & 0xff) * 0x10;
	return (*(uint16_t *)(p + 0x506) >> 6) & 1;
}

bool usbdc_txcsr_direct_test_ready(void *dev, uint8_t ep)	/* FUN_c0004550 (K2) */
{
	uint8_t *p = (uint8_t *)dev + (ep & 0xff) * 0x10;
	return (*(uint16_t *)(p + 0x502) & 3) == 3;
}

/* usbdc_txcsr_direct_test_bit1 - K1's FUN_c0004afc (sole caller: the
 * still-missing usbdc_core_isr's own generic USB-submit-primitive helper).
 * NOT independently re-located this pass - no K2 caller could be traced
 * since its own real caller chain runs through the missing core_isr region
 * (see file header). Declared as K1's own shape, unverified against K2. */
extern uint16_t usbdc_txcsr_direct_test_bit1(void *dev, uint8_t ep);	/* NEEDS LIVE QUERY - K2 address unknown */

/* Generic byte-offset peek/poke primitives. @0xc0004580 / @0xc0004588 /
 * @0xc0004590 / @0xc0004598 (K2). K1: @0xc0004b4c / @0xc0004b54 /
 * @0xc0004b5c / @0xc0004b64. */
uint32_t usbdc_raw_read32(void *dev, int offset)	/* FUN_c0004580 (K2) */
{
	return *(uint32_t *)((uint8_t *)dev + offset);
}

void usbdc_raw_write32(void *dev, int offset, uint32_t value)	/* FUN_c0004588 (K2) */
{
	*(uint32_t *)((uint8_t *)dev + offset) = value;
}

uint16_t usbdc_raw_read16(void *dev, int offset)	/* FUN_c0004590 (K2) */
{
	return *(uint16_t *)((uint8_t *)dev + offset);
}

void usbdc_raw_write16(void *dev, int offset, uint16_t value)	/* FUN_c0004598 (K2) */
{
	*(uint16_t *)((uint8_t *)dev + offset) = value;
}

/* ===========================================================================
 * Section 2 - FIFO read/write. CONFIRMED structurally identical to K1
 * (dev+ep*4+0x420 FIFO port array, same TXPKTRDY-set-on-completion /
 * RXPKTRDY-clear-on-completion tail, same "re-read the same FIFO register
 * each word, not an incrementing pointer" idiom, same 5-endpoint (0-4)
 * hardware limit).
 * =========================================================================== */
extern uint32_t usbdc_fifo_read_u32_unaligned(const void *src);	/* out of range in both K1 and K2 */

/* usbdc_fifo_write @0xc00041f4 (K2). K1: @0xc00047d4. */
void usbdc_fifo_write(void *dev, int ep, const uint8_t *src, int len)	/* FUN_c00041f4 (K2) */
{
	uint8_t *d = (uint8_t *)dev;

	if (ep > 4)
		return;

	while (len > 0) {
		if (len < 4) {
			d[ep * 4 + 0x423] = *src;
			src++;
			len--;
		} else {
			uint32_t word = usbdc_fifo_read_u32_unaligned(src);
			src += 4;
			len -= 4;
			*(uint32_t *)(d + ep * 4 + 0x420) = word;
		}
	}
	if (ep > 0) {
		uint8_t *p = d + ep * 0x10;
		*(uint16_t *)(p + 0x502) |= 1;
	}
}

/* usbdc_fifo_read @0xc000428c (K2). K1: @0xc0004858. */
void usbdc_fifo_read(void *dev, int ep, uint32_t *dst, unsigned len)	/* FUN_c000428c (K2) */
{
	extern uint16_t usbdc_rxcsr_clear_mask2;	/* K2 DAT_ - resolves 0xfffe, same as K1 */
	uint8_t *d = (uint8_t *)dev;
	uint32_t *fifo;
	unsigned rem;
	uint32_t local;

	if (ep >= 5)
		return;

	fifo = (uint32_t *)(d + ep * 4 + 0x420);
	rem = len & 3;
	for (int words = (int)len >> 2; words > 0; words--)
		*dst++ = *fifo;

	if (rem != 0) {
		local = *fifo;
		uint8_t *bp = (uint8_t *)dst, *lp = (uint8_t *)&local;
		while (rem > 0) {
			rem--;
			*bp++ = *lp++;
		}
	}
	if (ep > 0) {
		uint8_t *p = d + ep * 0x10;
		*(uint16_t *)(p + 0x506) &= usbdc_rxcsr_clear_mask2;
	}
}

/* ===========================================================================
 * Section 3 - DMA/endpoint descriptor-table writers (usbdc_dma_engine_reset,
 * usbdc_desc_set_length, usbdc_desc_get_length, usbdc_desc_table_global_init,
 * usbdc_desc_arm_slot, usbdc_ep_arm_rx, usbdc_ep_arm_tx in K1).
 *
 * BODIES NOT TRANSCRIBED (kept as externs matching K1's own signatures so
 * callers elsewhere in this project - cobjectmgr.c's own K2 port, if/when
 * it exists, hardcodes slot=1,sub=1 into usbdc_desc_arm_slot per K1's own
 * cross-file note - continue to link), but AS OF 2026-07-19 every one of
 * these 7 functions has a real, live-confirmed K2 address (usbdc_desc_
 * set_length/usbdc_desc_get_length resolved this pass, see their own notes
 * below; the other 5 already had confirmed addresses from the prior live
 * pass that resolved Section 6's usbdc_core_isr) - this is no longer an
 * open data gap for addresses/callers, only for full body transcription.
 * =========================================================================== */
extern void usbdc_dma_engine_reset(void *dev);				/* K2 CONFIRMED @0xc00035b4 (bl target, usbdc_core_isr's bus-reset branch) - body not transcribed. K1 @0xc0003b94 */

/* 2026-07-19 LIVE QUERY RESOLVED (both usbdc_desc_set_length and
 * usbdc_desc_get_length): found by live-disassembling the exact gap
 * between usbdc_dma_engine_reset (0xc00035b4, size 176 -> ends 0xc0003664)
 * and usbdc_desc_table_global_init (0xc00036c8) - matching K1's own
 * relative ordering (reset, set_length, get_length, table_init) exactly.
 * Neither has a Ghidra Function object bounding it (same "no boundary"
 * artifact as elsewhere in this project) but both are real, confirmed
 * Instructions with a real confirmed caller each:
 *
 *   usbdc_desc_set_length - K2 @0xc0003680 (36 bytes to 0xc00036a0
 *   inclusive). Body: loads a dev-table base via a literal pool pointer,
 *   folds in r1 (len) with the top bit forced set (`add r3,r1,#0x80000000`)
 *   and writes it to table[slot*0x20+0x200], then writes the raw len again
 *   to table[slot*0x20+0x200+0x18] and +0xc - matches K1's own
 *   (dev,len,slot) signature exactly (r0=dev unused directly - accessed via
 *   the table pointer instead, same phantom-forward idiom as elsewhere).
 *   CONFIRMED real caller (get_xrefs_to): FUN_c000432c, call site
 *   0xc0004390 - itself also calling usbdc_desc_arm_slot (0xc000379c) and
 *   FUN_c0008528, i.e. a generic per-transfer descriptor-arming helper, not
 *   core_isr itself (matching the file's own note that core_isr doesn't
 *   call this one directly).
 *
 *   usbdc_desc_get_length - K2 @0xc00036a8 (24 bytes to 0xc00036c0
 *   inclusive), immediately following set_length with zero gap. Body:
 *   loads the SAME dev-table base, indexes by `r1<<5` (ep), reads
 *   table[ep*0x20+0xc], then clears bits 0xff000000 AND 0xc00000 before
 *   returning - i.e. masks a packed length-plus-flags field down to the
 *   pure length, exactly explaining set_length's own `#0x80000000` flag-set
 *   above as a paired encoding. CONFIRMED 2 real callers (get_xrefs_to):
 *   0xc0003d6c (from_function: null - i.e. INSIDE the unbounded
 *   usbdc_core_isr region this file's own Section 6 already resolved,
 *   directly confirming this file's own prior note "core_isr's
 *   SETUP-pending branch calls it"); and FUN_c00043b8 (call site
 *   0xc00043e0), which also calls usbdc_desc_arm_slot and FUN_c0008018 -
 *   a near-twin of FUN_c000432c above, both sharing the same single caller
 *   FUN_c000bde4 (not further identified this pass), consistent with a
 *   generic TX/RX-side pair of per-transfer arming helpers.
 */
extern void usbdc_desc_set_length(void *dev, uint32_t len, int slot);		/* K2 CONFIRMED @0xc0003680, caller FUN_c000432c - see note above. K1 @0xc0003c60 */
extern uint32_t usbdc_desc_get_length(void *dev, int ep);			/* K2 CONFIRMED @0xc00036a8, callers: usbdc_core_isr (0xc0003d6c) + FUN_c00043b8 - see note above. K1 @0xc0003c88 */
extern void usbdc_desc_table_global_init(void);				/* K2 CONFIRMED @0xc00036c8 (bl target, usbdc_core_isr's bus-reset branch) - body not transcribed. K1 @0xc0003ca8 */
extern void usbdc_desc_arm_slot(uint32_t *dev, int slot, int sub);		/* K2 CONFIRMED @0xc000379c (bl target, 3 call sites in usbdc_core_isr's bus-reset branch + 1 in the SETUP-pending branch) - body not transcribed. K1 @0xc0003d7c */
extern void usbdc_ep_arm_rx(void *dev, uint8_t ep);				/* K2 CONFIRMED @0xc00037c4 (bl target, usbdc_core_isr's bus-reset branch) - body not transcribed. K1 @0xc0003da4 */
extern void usbdc_ep_arm_tx(void *dev, uint8_t ep);				/* K2 CONFIRMED @0xc00037f4 (bl target, usbdc_core_isr's bus-reset branch) - body not transcribed. K1 @0xc0003dd4 */

/* ===========================================================================
 * Section 4 - EP0 completion helpers (usbdc_ep0_notify_tx_complete,
 * usbdc_ep0_notify_rx_complete in K1).
 *
 * NOT PORTED - same confirmed data gap as Section 3, see file header.
 * =========================================================================== */
extern void usbdc_ep0_notify_tx_complete(void *dev, uint32_t len);		/* NEEDS LIVE QUERY - K1 @0xc00048f8 */
extern void usbdc_ep0_notify_rx_complete(void *dev, uint32_t ep_hint, uint32_t param3);	/* NEEDS LIVE QUERY - K1 @0xc0004984 */

/* ===========================================================================
 * Section 5 - FIFO flush-on-reset. CONFIRMED structurally IDENTICAL to K1,
 * including the specific byte-truncated-arithmetic quirk K1's own file
 * calls out for endpoint 2's index. Reproduced with the same static-helper-
 * plus-4-calls structure K1's own reconstruction uses (a readability
 * choice on both passes' part - the real compiled code in BOTH images is a
 * fully unrolled 4x sequence, confirmed directly from K2's own raw
 * decompile, not a real loop).
 * @0xc0003fd4 (K2, both the helper's 4 call sites AND the endpoint-2 index
 * quirk verified directly in this one function's raw decompile).
 * K1: @0xc00045b4.
 * =========================================================================== */
static void usbdc_flush_endpoint_fifos(void *dev, uint8_t ep)
{
	uint8_t *d = (uint8_t *)dev;
	d[0x40e] = ep;

	if ((*(uint16_t *)(d + 0x416) & 1) != 0) {
		*(uint16_t *)(d + 0x416) |= 0x10;
		*(uint16_t *)(d + 0x416) |= 0x10;
	}
	*(uint16_t *)(d + 0x416) |= 0x80;

	if ((*(uint16_t *)(d + 0x412) & 3) != 0) {
		*(uint16_t *)(d + 0x412) |= 8;
		*(uint16_t *)(d + 0x412) |= 8;
	}
	*(uint16_t *)(d + 0x412) |= 0x40;
}

void usbdc_flush_ep1_4(void *dev)	/* FUN_c0003fd4 (K2) */
{
	uint8_t *d = (uint8_t *)dev;

	usbdc_flush_endpoint_fifos(dev, 1);
	/* CONFIRMED identical quirk to K1: real K2 code computes endpoint 2's
	 * index as `(char)(TXCSR_offset - 0x10)` i.e. (uint8_t)(0x412-0x10)==2,
	 * not the plain literal 2 the other 3 endpoints use - re-verified
	 * directly against K2's own raw decompile this pass. */
	usbdc_flush_endpoint_fifos(dev, 2);
	usbdc_flush_endpoint_fifos(dev, 3);
	usbdc_flush_endpoint_fifos(dev, 4);
	(void)d;
}

/* ===========================================================================
 * Section 6 - the master USB-core ISR/poll handler (usbdc_core_isr in K1,
 * K1 @0xc0003e24, 1812 bytes). K2 @0xc0003840, 0x794 (1940) bytes, ending
 * with its own `mov pc,lr` at 0xc0003fd0 immediately before usbdc_flush_
 * ep1_4's already-confirmed 0xc0003fd4 - a clean, gapless boundary.
 *
 * RESOLVED 2026-07-19 via a dedicated, single-session live Ghidra MCP
 * bridge pass (get_disassembly, manually walked and transcribed
 * instruction-by-instruction in ~370-instruction chunks; Ghidra's own
 * decompiler/get_function_info both still report "no function found" for
 * this address - the boundary itself was never fixed, per this task's own
 * constraint against mutating Ghidra's analysis state, only hand-
 * transcribed from the raw listing, same technique this project has used
 * before for similarly unbounded regions, e.g. K1's own eva_board_main.c
 * crt0 chain).
 *
 * HEADLINE FINDING: this function is a near-EXACT structural and semantic
 * match to K1's usbdc_core_isr - same interrupt-status-word decode (masked
 * = *(d+0x20) & *(d+0x2c)), same bit-by-bit if/else-if dispatch order (bus
 * reset 0x40000 -> EP0 state machine masked&1 -> masked&1==0 chain: 0x80000
 * (event 0xb) -> 0x200/EP1 (event 3) -> SETUP-pending second wire_dispatch_
 * command call site -> 0x800/EP3 (event 7) -> 0x1000/EP4 (event 9) -> 0x2
 * (event 2, no dispatch) -> 0x4/EP2 (event 6) -> 0x8 (event 8, no dispatch)
 * -> 0x10/EP4 second check (event 10) -> tail), same bus-reset EP1-4 TXMAXP/
 * RXMAXP/CSR default re-init sequence at BYTE-IDENTICAL offsets/values to
 * K1 (0x40e/0x410/0x412/0x414/0x416/0x462/0x463/0x464/0x466/0x401/0x12a/
 * 0x1aa, all independently re-resolved from K2's own literal pool this pass
 * and found numerically IDENTICAL to K1's), same tail (0x200000/0x10000
 * event overwrite, then *(d+0x3c)=0). Confirmed real callees at the exact
 * addresses this file's own header already predicted from xref evidence:
 * class5_handler @0xc000b6b0 (call site 0xc0003c0c), class9_handler
 * @0xc000b730 (0xc0003c18), state3_handler @0xc000aa90 (0xc0003c64),
 * state4_handler @0xc000aca8 (0xc0003c90), usbdc_endpoint_event_dispatch
 * @0xc000bde4 (0xc0003c48/0xc0003c78/0xc0003f28) - every one of the 7
 * "None"-attributed call sites this file's header already found is
 * confirmed to belong to core_isr, exactly as predicted.
 *
 * REAL, CONFIRMED DIFFERENCES FROM K1 (not transcription artifacts - each
 * independently re-derived from K2's own raw disassembly/literal pool this
 * pass):
 *  1. usbdc_desc_arm_slot's SETUP-pending call site (K1's own file: "real
 *     call site: `FUN_c0003d7c(param_1);` - only ONE visible arg... slot/sub
 *     guessed as 0/0 rather than independently confirmed") - K2's raw
 *     disassembly shows the REAL register values explicitly: r1=1, r2=1
 *     before the `bl`. The real call is usbdc_desc_arm_slot(dev, 1, 1), NOT
 *     (dev, 0, 0). Whether K1's own binary genuinely differs or its own
 *     phantom-forward decompile artifact simply hid the same real values is
 *     not resolvable without K1 raw disassembly access (out of this pass's
 *     scope, K1_V06R06/ is a read-only baseline) - flagged as a K2-side
 *     confirmed fact, not asserted as a K1/K2 behavioral difference.
 *  2. The masked-bit-0x200 (EP1-ready) branch's CSR-like field sits at a
 *     DIFFERENT confirmed byte offset in K2: 0x516 (independently re-
 *     derived from K2's own literal pool, and independently recognizable as
 *     Section 1's own direct/flat RXCSR-window formula, ep*0x10+0x506, for
 *     ep=1) vs the offset K1's own file cites for the structurally-
 *     identical branch, 0x462 (`DAT_c000458c`). Both branches test/clear
 *     the identical bit pattern (bit2/bit3, then a 2x OR-0x10 loop, then a
 *     final bit-0 test producing event_code 3) - same logic, a genuinely
 *     different confirmed field offset. Flagged rather than silently
 *     reconciled, per this project's own convention for cross-version
 *     discrepancies; not resolved which side (if either) is "wrong."
 *  3. The three boot-flag writes (usbdc_resume_flag/usbdc_reset_pending_
 *     flag/usbdc_setup_pending_flag, extra&0x1000000/0x4000000/0x8000000)
 *     each compile through a real but functionally DEAD intermediate
 *     dereference of `dev` (e.g. `r3 = *(dev + 0x618c)`, then r3 is
 *     immediately overwritten by a second literal load before the actual
 *     global write) before landing on the same fixed global address K1's
 *     own version writes directly. Confirmed present in K2's raw
 *     disassembly for all three flags; functionally inert (the dereferenced
 *     value is never used) unless dev+0x618c is itself a volatile hardware
 *     register with a real read side effect, which was not independently
 *     checked this pass.
 *  4. The two `*usbdc_ep0_ctx_ptr &= 0xf0`-shaped nibble-clear operations
 *     (SENTSTALL/SETUPEND handling in the EP0 state-machine branch) compile
 *     to full 32-bit `ldr`/`str` in K2's raw disassembly, not `ldrb`/`strb`
 *     as K1's own byte-pointer-typed reconstruction implies. Functionally
 *     equivalent as long as the target dword's upper 3 bytes are always
 *     zero (not independently verified this pass) - flagged as a real,
 *     confirmed access-width difference, not silently normalized away.
 *  5. usbdc_setup_dispatch_buf's own indirection appears to collapse to a
 *     SINGLE literal-pool load in K2 (one `ldr r0,[pool]` feeding
 *     wire_dispatch_command's `cmd` argument directly) rather than the two-
 *     hop "load address, then dereference" shape K1's own `*usbdc_setup_
 *     dispatch_buf` notation implies - consistent with this project's
 *     broader, repeatedly-confirmed pattern of these "singleton" globals
 *     resolving to one fixed, always-populated literal (e.g. midi_engine.c's
 *     ring-2 singleton) rather than a genuine two-level runtime pointer
 *     chase. Not asserted as a functional difference, just a compile-time
 *     simplification observed in K2's own code.
 *  6. The three "masked-bit set, write event code, goto tail with NO
 *     dispatch call" cases (0x20000, 0x2, 0x8) are compiler-tail-merged in
 *     K2's raw code into ONE shared physical `str r3,[dev+0x28]; b tail`
 *     instruction pair, reached via 3 separate conditional branches each
 *     pre-loading r3 with its own event constant - a real ARM code-
 *     generation difference from how three independent physical sequences
 *     would look, but behaviorally identical to K1's three separate cases;
 *     represented below as three separate C cases, matching K1's own
 *     structure, since the shared-tail-fragment shape is purely a codegen
 *     artifact, not a semantic one.
 *
 * Confirmed real global addresses (K2, independently read from this
 * function's own literal pool): usbdc_resume_flag=0xc01cabb9, usbdc_reset_
 * pending_flag=0xc01cabb8, usbdc_setup_pending_flag=0xc01cabba (three
 * CONSECUTIVE byte addresses - a packed 3-flag cluster, not independently
 * noted in K1's own file); usbdc_ep0_ctx_ptr's real fixed target=0xc01cc848;
 * usbdc_setup_dispatch_handle's dereferenced base=0xc01caba4; usbdc_setup_
 * dispatch_buf=0xc01cb2ec (see difference #5 above).
 *
 * INCIDENTAL FINDING (see file header): this function's own bus-reset
 * branch directly confirms Section 3's real K2 addresses via `bl` targets -
 * usbdc_dma_engine_reset@0xc00035b4, usbdc_desc_table_global_init@
 * 0xc00036c8, usbdc_desc_arm_slot@0xc000379c, usbdc_ep_arm_rx@0xc00037c4,
 * usbdc_ep_arm_tx@0xc00037f4 - even though those functions' own bodies
 * remain out of this pass's scope (still declared bare `extern` in Section
 * 3 above, now with real address citations instead of "NEEDS LIVE QUERY").
 * =========================================================================== */
extern void usbdc_ep0_class5_handler(void *ctx);	/* FUN_c000b6b0 (K2), out of range - omap_l137_usbdc_ep0.c territory, confirmed call site 0xc0003c0c */
extern void usbdc_ep0_class9_handler(void *ctx);	/* FUN_c000b730 (K2), out of range - calls usbdc_flush_ep1_4, confirmed call site 0xc0003c18 */
extern void usbdc_ep0_state3_handler(void *ctx);	/* FUN_c000aa90 (K2), out of range, confirmed call site 0xc0003c64 */
extern void usbdc_ep0_state4_handler(void *ctx);	/* FUN_c000aca8 (K2), out of range, confirmed call site 0xc0003c90 */
extern uint8_t usbdc_endpoint_event_dispatch(void *ctx, uint32_t event);	/* FUN_c000bde4 (K2), reconstructed below (Section 7) */

void usbdc_core_isr(void *dev, void *ep0_ctx)	/* FUN_c0003840 (K2) */
{
	uint8_t *d = (uint8_t *)dev;
	extern uint8_t usbdc_resume_flag;		/* K2 0xc01cabb9 (dead dev+0x618c deref precedes the real write, see difference #3) */
	extern uint8_t usbdc_reset_pending_flag;	/* K2 0xc01cabb8, same dead-deref pattern (dev+0x61ac) */
	extern uint8_t usbdc_setup_pending_flag;	/* K2 0xc01cabba, same dead-deref pattern (dev+0x61bc) */
	extern uint8_t *usbdc_ep0_ctx_ptr;		/* K2 fixed target 0xc01cc848 - see difference #4 (32-bit access, not byte) */
	uint32_t status = *(uint32_t *)(d + 0x20);
	uint32_t enable_mask = *(uint32_t *)(d + 0x2c);
	uint32_t extra = *(uint32_t *)(d + 0x4090);
	int reset_seen = (extra & 0x1000000) != 0;
	uint32_t masked;
	uint32_t event_code;

	if (reset_seen)
		usbdc_resume_flag = 1;
	if ((extra & 0x4000000) != 0)
		usbdc_reset_pending_flag = 1;
	if ((extra & 0x8000000) != 0)
		usbdc_setup_pending_flag = 1;

	masked = status & enable_mask;

	if ((masked & 0x20000) != 0) {
		*(uint32_t *)(d + 0x28) = 0x20000;
		goto tail;
	}

	if ((masked & 0x40000) != 0) {
		/* USB bus reset - CONFIRMED byte-identical offsets/values to K1
		 * throughout (independently re-resolved from K2's own literal pool
		 * this pass): 0x40e/0x410/0x412/0x414/0x416/0x462/0x463/0x464/
		 *0x466/0x401/0x12a/0x1aa. */
		d[0x40e] = 1;
		*(uint16_t *)(d + 0x466) = 8;
		{
			int bit4 = (d[0x401] & 0x10) != 0;
			if (bit4) {
				d[0x463] = 0x15;
				*(uint16_t *)(d + 0x414) = 0x90;
			} else {
				d[0x463] = 0x16;
				*(uint16_t *)(d + 0x414) = 0x138;
			}
		}
		*(uint16_t *)(d + 0x416) = 0x4000;
		*(uint16_t *)(d + 0x464) = 0x88;
		d[0x462] = 0x10;
		*(uint16_t *)(d + 0x410) = 3;
		*(uint16_t *)(d + 0x412) = 0x6000;

		d[0x40e] = 2;
		*(uint16_t *)(d + 0x466) = 0x8a;
		d[0x463] = 0x16;
		*(uint16_t *)(d + 0x414) = 0x200;
		*(uint16_t *)(d + 0x416) = 0;
		*(uint16_t *)(d + 0x464) = 0x10a;
		d[0x462] = 4;
		*(uint16_t *)(d + 0x410) = 0x80;
		*(uint16_t *)(d + 0x412) = 0x2800;

		d[0x40e] = 3;
		*(uint16_t *)(d + 0x466) = 0x11a;
		d[0x463] = 0x13;
		*(uint16_t *)(d + 0x414) = 0x40;
		*(uint16_t *)(d + 0x416) = 0;
		*(int16_t *)(d + 0x464) = 0x12a;
		{
			int bit4 = (d[0x401] & 0x10) != 0;
			d[0x462] = bit4 ? 0x15 : 0x16;
			*(uint16_t *)(d + 0x410) = bit4 ? 0x90 : 0x138;
		}

		*(uint16_t *)(d + 0x412) = 0x6000;
		d[0x40e] = 4;
		*(int16_t *)(d + 0x466) = 0x1aa;
		d[0x463] = 0x13;
		*(uint16_t *)(d + 0x414) = 0x40;
		*(uint16_t *)(d + 0x416) = 0;
		*(int16_t *)(d + 0x464) = 0x1aa + 0x10;
		d[0x462] = 0x13;
		*(uint16_t *)(d + 0x410) = 0x40;
		*(uint16_t *)(d + 0x412) = 0x2000;

		usbdc_dma_engine_reset(dev);		/* K2 confirmed @0xc00035b4 */
		usbdc_desc_table_global_init();	/* K2 confirmed @0xc00036c8 */
		usbdc_desc_arm_slot((uint32_t *)dev, 0, 0);	/* K2 confirmed @0xc000379c */
		usbdc_ep_arm_rx(dev, 1);		/* K2 confirmed @0xc00037c4 */
		usbdc_desc_arm_slot((uint32_t *)dev, 0x14, 0x10);
		usbdc_ep_arm_tx(dev, 3);		/* K2 confirmed @0xc00037f4 */
		usbdc_desc_arm_slot((uint32_t *)dev, 1, 1);
		usbdc_ep_arm_rx(dev, 2);
		*(uint32_t *)(d + 4) &= ~8u;
		goto tail;
	}

	if ((masked & 1) == 0) {
		if ((masked & 0x80000) != 0) {
			*(uint32_t *)(d + 0x28) = 0x80000;
			event_code = 0xb;
		} else if ((masked >> 9 & 1) != 0) {
			*(uint32_t *)(d + 0x28) = 0x200;
			usbdc_select_endpoint(dev, 1);
			{
				/* CSR-like field @0xc0003fac literal = 0x516 (K2) -
				 * see difference #2 above re: K1's own 0x462 citation
				 * for this structurally-identical branch. */
				uint16_t csr = *(uint16_t *)(d + 0x516);
				if ((csr & 4) != 0) {
					*(uint16_t *)(d + 0x516) = csr & 0xfffb;
					for (int i = 1; i >= 0; i--)
						*(uint16_t *)(d + 0x516) |= 0x10;
					csr = *(uint16_t *)(d + 0x516);
				} else if ((csr & 8) != 0) {
					csr &= 0xfff7;
					*(uint16_t *)(d + 0x516) = csr;
				}
				if ((csr & 1) == 0)
					goto tail;
				event_code = 3;
			}
		} else if (usbdc_setup_pending_flag != 0) {
			/* SECOND wire_dispatch_command call site - CONFIRMED
			 * identical shape to K1, including the confirmed real
			 * usbdc_desc_arm_slot(dev,1,1) args, see difference #1. */
			extern void *usbdc_setup_dispatch_handle;	/* K2 0xc01caba4, dereferenced */
			extern uint8_t *usbdc_setup_dispatch_buf;	/* K2 0xc01cb2ec, see difference #5 */
			uint32_t len = usbdc_desc_get_length(dev, 1);
			void *r = wire_dispatch_command(usbdc_setup_dispatch_handle,
							 usbdc_setup_dispatch_buf, len);
			if (r != 0)
				usbdc_desc_arm_slot((uint32_t *)dev, 1, 1);	/* CONFIRMED real args (r1=1,r2=1
					in K2's raw disassembly) - NOT (0,0) as K1's own file guessed, see
					difference #1 above */
			usbdc_setup_pending_flag = 0;
			goto tail;
		} else if ((masked & 0x800) != 0) {
			*(uint32_t *)(d + 0x28) = 0x800;
			usbdc_select_endpoint(dev, 3);
			uint16_t csr = *(uint16_t *)(d + 0x536);
			if ((csr & 0x40) != 0) {
				*(uint16_t *)(d + 0x536) = csr & 0xffbf;
				*(uint16_t *)(d + 0x536) = (csr & 0xffbf) | 0x80;
				goto tail;
			}
			if ((csr & 1) == 0)
				goto tail;
			event_code = 7;
		} else if ((masked & 0x1000) != 0) {
			*(uint32_t *)(d + 0x28) = 0x1000;
			usbdc_select_endpoint(dev, 4);
			uint16_t csr = *(uint16_t *)(d + 0x546);
			if ((csr & 0x40) != 0) {
				*(uint16_t *)(d + 0x546) = (csr & 0xffbf) | 0x80;
				goto tail;
			}
			if ((csr & 1) == 0)
				goto tail;
			event_code = 9;
		} else if ((masked & 2) != 0) {
			*(uint32_t *)(d + 0x28) = 2;
			goto tail;
		} else if ((masked & 4) != 0) {
			*(uint32_t *)(d + 0x28) = 4;
			usbdc_select_endpoint(dev, 2);
			uint16_t csr = *(uint16_t *)(d + 0x522);
			if ((csr & 0x20) != 0) {
				*(uint16_t *)(d + 0x522) = (csr & 0xffdf) | 0x40;
				goto tail;
			}
			if ((csr & 1) != 0)
				goto tail;
			event_code = 6;
		} else if ((masked & 8) != 0) {
			*(uint32_t *)(d + 0x28) = 8;
			goto tail;
		} else if ((masked & 0x10) != 0) {
			*(uint32_t *)(d + 0x28) = 0x10;
			usbdc_select_endpoint(dev, 4);
			uint16_t csr = *(uint16_t *)(d + 0x542);
			if ((csr & 0x20) != 0) {
				*(uint16_t *)(d + 0x542) = (csr & 0xffdf) | 0x40;
				goto tail;
			}
			if ((*(uint16_t *)(d + 0x542) & 1) != 0)
				goto tail;
			event_code = 10;
		} else {
			goto tail;
		}
	} else {
		/* EP0 control-transfer state machine - CONFIRMED identical shape
		 * to K1. Difference #4 above: the two nibble-clear operations
		 * below compile to 32-bit ldr/str in K2, not byte ldrb/strb. */
		usbdc_select_endpoint(dev, 0);
		*(uint32_t *)(d + 0x28) = 1;
		{
			uint16_t csr0 = *(uint16_t *)(d + 0x502);
			uint32_t shifted = (uint32_t)csr0 << 0x10;
			if ((shifted & 0x40000) != 0) {
				csr0 &= 0xfffb;
				*(uint16_t *)(d + 0x502) = csr0;
				*(uint32_t *)usbdc_ep0_ctx_ptr &= 0xf0;	/* difference #4: 32-bit access */
			}
			if ((shifted & 0x100000) != 0) {
				*(uint16_t *)(d + 0x502) |= 0x100;
				csr0 = (*(uint16_t *)(d + 0x502)) | 0x80;
				*(uint16_t *)(d + 0x502) = (uint16_t)csr0;
				*(uint32_t *)usbdc_ep0_ctx_ptr &= 0xf0;	/* difference #4: 32-bit access */
			}
			if ((*usbdc_ep0_ctx_ptr & 0xf) == 0) {
				if ((*usbdc_ep0_ctx_ptr & 0x60) == 0x60) {
					uint8_t cls = usbdc_ep0_ctx_ptr[1] & 0xf;
					if (cls == 5)
						usbdc_ep0_class5_handler(ep0_ctx);
					else if (cls == 9)
						usbdc_ep0_class9_handler(ep0_ctx);
					*usbdc_ep0_ctx_ptr = (*usbdc_ep0_ctx_ptr & 0xbf) | 0x20;
				} else {
					*usbdc_ep0_ctx_ptr = (*usbdc_ep0_ctx_ptr & 0xbf) | 0x20;
					if ((csr0 & 1) != 0)
						usbdc_endpoint_event_dispatch(ep0_ctx, 0);
				}
			}
			{
				uint8_t nib = *usbdc_ep0_ctx_ptr & 0xf;
				if (nib == 3) {
					usbdc_ep0_state3_handler(ep0_ctx);
				} else if (nib == 1) {
					usbdc_endpoint_event_dispatch(ep0_ctx, 2);
				}
				nib = *usbdc_ep0_ctx_ptr & 0xf;
				if (nib == 4) {
					usbdc_ep0_state4_handler(ep0_ctx);
					goto tail;
				}
				if (nib != 2)
					goto tail;
				event_code = 1;
			}
		}
	}

	usbdc_endpoint_event_dispatch(ep0_ctx, event_code);

tail:
	if ((masked & 0x200000) != 0)
		*(uint32_t *)(d + 0x28) = 0x200000;
	if ((masked & 0x10000) != 0)
		*(uint32_t *)(d + 0x28) = 0x10000;
	*(uint32_t *)(d + 0x3c) = 0;
}

/* ===========================================================================
 * Section 7 - endpoint-event dispatcher and its wire_dispatch_command call
 * site. CONFIRMED structurally identical in shape to K1 (same event-code
 * switch cases 0/1/2/3/5/7/9/10/0xb; case 0's own SETUP-packet count==8
 * gate before dispatching to the standard-request sub-handler; case 5's own
 * clamp-to-0x200-then-fifo_read-then-wire_dispatch_command chain for bulk
 * OUT). Cases 7/9/10 forward to sibling endpoint handlers K1 ALSO leaves as
 * bare externs outside its own reconstructed scope (usbdc_ep_state7_handler/
 * _state9_handler/_state10_handler) - same convention followed here, their
 * K2 addresses (FUN_c000bc84/FUN_c000bcf8/FUN_c000bd6c) are known from this
 * pass's own xref data but their bodies were never in scope for either
 * firmware's reconstruction.
 * =========================================================================== */
extern uint8_t usbdc_ep_state1_handler(void *ctx, void *scratch);	/* omap_l137_usbdc_ep0.c K2 port, FUN_c000b0d4 */
extern uint8_t usbdc_ep_state2_handler(void *ctx, void *scratch);	/* omap_l137_usbdc_ep0.c K2 port, FUN_c000b568 */
extern int usbdc_ep_state3_query(void);				/* omap_l137_usbdc_ep0.c K2 port, FUN_c000ae90 */
extern void usbdc_ep_state7_handler(void);				/* FUN_c000bc84 (K2), out of scope both images */
extern void usbdc_ep_state9_handler(void);				/* FUN_c000bcf8 (K2), out of scope both images */
extern void usbdc_ep_state10_handler(void);				/* FUN_c000bd6c (K2), out of scope both images */
extern uint8_t usbdc_ep0_setup_dispatch(void *ctx, void *scratch);	/* FUN_c000b988 (K2), out of scope both images -
									 * case-0 standard-request sub-handler, confirmed
									 * located this pass but never in either file's own
									 * reconstruction scope, matching K1's own convention */
extern void usbdc_ep_state_notify(void *handle, uint32_t code);	/* out of range in both K1 and K2 -
									 * K2 call sites use FUN_c00080e4/FUN_c0008394/
									 * FUN_c00084e8/FUN_c00084c4, plausibly 4 distinct
									 * helpers rather than K1's own apparent 1-symbol-many-
									 * aliases pattern; NOT reconciled to a single K1-style
									 * name this pass - flagged rather than guessed */

/* usbdc_endpoint_event_dispatch @0xc000bde4 (K2). K1: @0xc000aae0. */
uint8_t usbdc_endpoint_event_dispatch(void *ctx, uint32_t event)	/* FUN_c000bde4 (K2) */
{
	extern void *usbdc_default_dev_handle;	/* K2 DAT_c000bfbc, == 0xc01cc84c, same shared dev handle */
	uint8_t *c = (uint8_t *)ctx;
	uint8_t result = 1;

	switch (event) {
	case 0: {
		int16_t n = (int16_t)usbdc_raw_read16(usbdc_default_dev_handle, 0x418);
		if (n == 8) {
			usbdc_fifo_read(usbdc_default_dev_handle, 0, (uint32_t *)(c + 0x50), 8);
			result = usbdc_ep0_setup_dispatch(ctx, c + 0x50);
		}
		break;
	}
	case 1:
		result = usbdc_ep_state1_handler(ctx, c + 0x50);
		break;
	case 2:
		result = usbdc_ep_state2_handler(ctx, c + 0x50);
		break;
	case 3:
		usbdc_ep_state3_query();
		break;
	case 5:
		usbdc_ep_recv_bulk();
		break;
	case 7:
		usbdc_ep_state7_handler();
		break;
	case 9:
		usbdc_ep_state9_handler();
		break;
	case 10:
		usbdc_ep_state10_handler();
		break;
	case 0xb: {
		/* CONFIRMED same overall shape as K1 (state3_query-gated event
		 * code 0x40/8, ctx+0x72/ctx+0x73-gated notify calls, ctx+0x6e
		 * burst-counter logic on the RX side) - the 4 distinct notify-
		 * helper symbols (FUN_c00080e4/FUN_c0008394/FUN_c00084e8/
		 * FUN_c00084c4) were NOT reconciled to K1's own apparent single
		 * usbdc_ep_state_notify symbol-with-aliases; K1's own notify
		 * helpers (usbdc_ep0_notify_tx_complete/_rx_complete, Section 4
		 * above) are themselves part of this project's confirmed data
		 * gap, so this whole branch's downstream behavior could not be
		 * fully cross-checked against K1 this pass. */
		int is_state1 = usbdc_ep_state3_query() == 1;
		uint32_t code = is_state1 ? 0x40 : 8;
		extern void usbdc_ep_notify_a(void *handle, uint32_t code);	/* FUN_c00080e4 (K2) */
		extern void usbdc_ep_notify_b(void *dev, uint32_t code);	/* FUN_c00043b8 (K2) */
		extern void usbdc_ep_notify_c(void *handle, uint32_t code);	/* FUN_c0008394 (K2) */
		extern void usbdc_ep_notify_d(void *handle, uint32_t code);	/* FUN_c00084e8 (K2) */
		extern void usbdc_ep_notify_burst(void *dev, uint32_t code);	/* FUN_c000432c (K2) */
		extern void usbdc_ep_notify_e(void *handle, uint32_t code);	/* FUN_c00084c4 (K2) */
		extern void *usbdc_ep_notify_handle;	/* K2 DAT_c000bfc4 */
		extern int32_t *usbdc_burst_counter;	/* K2 DAT_c000bfc8 */

		usbdc_ep_notify_a(usbdc_ep_notify_handle, code);
		if (c[0x72] == 1) {
			usbdc_ep_notify_b(usbdc_default_dev_handle, code);
			usbdc_ep_notify_c(usbdc_ep_notify_handle, code);
		}
		if (c[0x73] == 1) {
			usbdc_ep_notify_d(usbdc_ep_notify_handle, code);
			if (is_state1) {
				if (c[0x6e] == 0) {
					if (*usbdc_burst_counter > 0) {
						usbdc_ep_notify_burst(usbdc_default_dev_handle, code);
						*usbdc_burst_counter = 0;
					}
					(*usbdc_burst_counter)++;
				} else {
					usbdc_ep_notify_burst(usbdc_default_dev_handle, code);
					c[0x6e] = 0;
					*usbdc_burst_counter = 1;
				}
			} else {
				usbdc_ep_notify_burst(usbdc_default_dev_handle, code);
			}
		}
		usbdc_ep_notify_e(usbdc_ep_notify_handle, code);
		break;
	}
	default:
		break;
	}
	return result;
}

/* usbdc_ep_recv_bulk - CONFIRMED structurally identical to K1: same
 * pending-byte-count read, same clamp to 0x200 (512), same fifo_read into a
 * fixed global receive buffer, same direct wire_dispatch_command call - no
 * intermediate queue. @0xc000bc1c (K2). K1: @0xc000a918. */
void usbdc_ep_recv_bulk(void)	/* FUN_c000bc1c (K2) */
{
	extern void *usbdc_bulk_dev_handle;		/* K2 DAT_c000bc74 (dereferenced), == 0xc01cc84c */
	extern int usbdc_bulk_ep_index;		/* K2 DAT_c000bc78, == 0x418 (RXCOUNT) */
	extern uint8_t **usbdc_bulk_rx_buffer;		/* K2 DAT_c000bc7c (dereferenced) */
	extern void *usbdc_wire_handle;		/* K2 DAT_c000bc80 */
	uint16_t len;

	len = usbdc_raw_read16(usbdc_bulk_dev_handle, usbdc_bulk_ep_index);
	if (len > 0x1ff)
		len = 0x200;
	usbdc_fifo_read(usbdc_bulk_dev_handle, 2, (uint32_t *)*usbdc_bulk_rx_buffer, len);
	wire_dispatch_command(usbdc_wire_handle, *usbdc_bulk_rx_buffer, len);
}

/* -------------------------------------------------------------------------
 * Still genuinely open (K2), not fabricated:
 *  - Section 6 (usbdc_core_isr) is now RESOLVED - see its own definition
 *    above for the full transcription and every confirmed K1/K2 difference.
 *  - Sections 3 and 4 remain a genuine gap for their FULL BODIES (5 of
 *    Section 3's 7 functions now have a confirmed K2 address via core_isr's
 *    own `bl` targets, see Section 3's own updated comments and the
 *    "INCIDENTAL FINDING" note in Section 6 - bodies still not
 *    transcribed, out of this pass's own assigned scope). Section 4
 *    (usbdc_ep0_notify_tx_complete/_rx_complete) has no confirmed K2
 *    address at all - core_isr itself doesn't call either directly.
 *  - usbdc_txcsr_direct_test_bit1's own K2 address - its sole real caller
 *    (per K1) is usbdc_core_isr's own generic USB-submit-primitive helper,
 *    which core_isr's own now-transcribed body does not appear to call
 *    directly either (K1's own note about this caller was itself somewhat
 *    speculative) - still unresolved.
 *  - The 4-way notify-helper split in usbdc_endpoint_event_dispatch's own
 *    case 0xb (FUN_c00080e4/_c0008394/_c00084e8/_c00084c4) vs K1's own
 *    apparent single usbdc_ep_state_notify symbol - not reconciled; could
 *    be 4 genuinely distinct K2 helpers, or 4 aliases of one symbol the way
 *    K1's own dev-handle DAT_ chain worked, not independently confirmed
 *    either way this pass.
 *  - FUN_c0009bfc's own K1 role as core_isr's caller - no K2 counterpart
 *    was searched for this pass (out of scope: Part 1 asked only for
 *    core_isr itself, not its own caller).
 *  - Every item K1's own file already left open (register field meanings
 *    beyond TXCSR/RXCSR/TXMAXP/RXMAXP, the exact wire-format of the EP0-
 *    SETUP-class wire_dispatch_command call site, usbdc_desc_table_base's
 *    real DMA-engine relationship) remains equally open in K2 - none of
 *    these were resolved by anything found this pass.
 * ------------------------------------------------------------------------- */
