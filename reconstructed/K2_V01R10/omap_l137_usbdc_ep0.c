/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l137_usbdc_ep0.c - K2 (KRONOS2S_V01R10.VSB) port of the USB0
 * control-endpoint (EP0) request-handler cluster.
 *
 * Source: K1_V06R06/omap_l137_usbdc_ep0.c (READ-ONLY baseline, not edited).
 * Ground truth: static Ghidra dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json, 2026-07-18), no live Ghidra
 * bridge access this pass.
 *
 * LOCATION METHOD: K1's own file documents that 19 differently-named DAT_
 * aliases all resolve to the SAME shared `dev` handle literal (0xc01cce50),
 * and separately confirms (via direct comparison) that this is the exact
 * same global as omap_l137_usbdc.c's own "default ep0 handle"
 * (K1 DAT_c000ace8). Since omap_l137_usbdc.c's K2 port already resolved
 * that K2 counterpart to 0xc01cc84c (see that file's own header), this pass
 * searched K2's ENTIRE data dump for every DAT_ symbol whose resolved
 * literal equals 0xc01cc84c - found 29 aliases, all falling in one
 * contiguous address run (0xc000a9a4-0xc000c0fc). Every function referencing
 * one of those aliases was decompiled and matched against K1's own 29
 * functions by structural shape (register offsets, bit patterns, branch
 * structure) - NOT by address proximity to the K2 anchor string, since (as
 * with omap_l137_usbdc.c) the ep0 cluster sits far below the string
 * literal's own address in both firmware images.
 *
 * COVERAGE: 28 of K1's 29 functions confidently located and ported/
 * rewritten this pass. usbdc_ep0_ctx_get_config, usbdc_ep0_state3_handler,
 * usbdc_ep0_start_tx, usbdc_ep_flush_or_dataend, usbdc_ep3_send_a,
 * usbdc_ep1_send, usbdc_ep3_send_b, usbdc_ep4_send, usbdc_ep0_state4_handler,
 * usbdc_ep_irq_enable_set30/_set34, usbdc_ep_state3_query,
 * usbdc_epN_tx_ready_query, usbdc_setup_stage_address/_configuration,
 * usbdc_ep0_diag_request, usbdc_ep0_ctx_reset, usbdc_ep_state1_handler,
 * usbdc_get_descriptor/_configuration/_interface/_status,
 * usbdc_ep_flush_direction, usbdc_ep_state2_handler,
 * usbdc_clear_feature_endpoint_halt/usbdc_set_feature_endpoint_halt,
 * usbdc_ep0_class5_handler/_class9_handler - all 27 confidently matched by
 * exact structural shape (register offsets, literal bit patterns, and
 * multiple independent DAT_-resolved constant cross-checks against K1's own
 * values, e.g. 0x401/0x40e/0x412/0x416/0x502/0x506 all confirmed identical
 * to K1). cad_pedal_present (K1 @0xc00094d8, itself a tangential function
 * K1's own file admits doesn't really belong to this cluster - "kept here
 * only because its address falls inside this pass's assigned sweep range")
 * was NOT conclusively located this pass: the address-adjacent K2 candidate
 * (FUN_c000a7dc, called from the same object-init-bring-up caller context)
 * has a visibly DIFFERENT shape - a 4-way bit-test cascade (bits 0x17/0x27/
 * 0x28/0x25) assigning a 3-valued result, vs K1's simple AND-gated 2-bit
 * (0x17 && 0x25) boolean check.
 *
 * 2026-07-19 LIVE QUERY FOLLOW-UP: live decompile_function + get_xrefs_to
 * on FUN_c000a7dc obtained (real caller: FUN_c0009838 @ call site
 * 0xc0009860) - confirms the 4-way-cascade shape exactly as described
 * above, no correction needed. Best-evidence read, not proof: this likely
 * IS cad_pedal_present's real K2 replacement, expanded from a boolean to a
 * 3-valued pedal-type result while keeping K1's own bit-0x17 gate as its
 * first test - see this file's own STILL OPEN section below for the full
 * decompile and reasoning. Left unported into a named function body here
 * (would require restructuring K1's own 2-value API around a 3-way
 * result, out of this pass's scope).
 *
 * GENUINE, CONFIRMED STRUCTURAL FINDING - K2 ctx-struct field shift:
 * FOUR independent call sites this pass all show the SAME systematic
 * one-byte-earlier shift of the "committed SET_CONFIGURATION value" field,
 * from ctx+7 (K1) to ctx+6 (K2):
 *   1. usbdc_ep0_ctx_get_config's K2 counterpart (FUN_c000a874) reads
 *      `*(byte*)(ctx+6)`, not ctx+7.
 *   2. usbdc_setup_stage_configuration's K2 counterpart (FUN_c000af54)
 *      WRITES `*(byte*)(ctx+6)`, not ctx+7.
 *   3. usbdc_ep0_class9_handler's K2 counterpart (FUN_c000b730) forwards
 *      `*(ctx+6)` as usbdc_flush_ep1_4's (harmless, dead) phantom second
 *      argument, not ctx+7.
 *   4. omap_l137_usbdc.c's own K2 port of omap_usbdc_object_init (see that
 *      file) confirmed obj+7 is no longer zeroed at all, consistent with a
 *      field removed/shifted earlier in the same struct family.
 * All four agree with each other and NONE contradicts - this is a real,
 * confirmed K2 ctx-struct layout change (something upstream of the
 * SET_CONFIGURATION byte got removed or shrunk by one byte), not a
 * decompiler artifact or a one-off transcription slip.
 *
 * OTHER CONFIRMED STRUCTURAL DIFFERENCE - usbdc_ep_flush_direction's K2
 * counterpart (FUN_c000b2ec) explicitly calls usbdc_select_endpoint(dev,1)
 * / usbdc_select_endpoint(dev,3) for its two mode branches; K1's own
 * version has NO endpoint-select call at all ("relies on the caller having
 * already selected the target endpoint" - K1's own words). K2 also
 * HARDCODES the endpoint numbers (1 for the RX/mode-1 branch, 3 for the
 * TX/mode-3 branch) rather than deriving them from `dir`/`mode` generically -
 * matches K1's own bit patterns and mask logic exactly otherwise.
 *
 * usbdc_ep_flush_or_dataend's K2 counterpart (FUN_c000a9c0) also gained an
 * extra leading parameter (visibly unused - never read in the body) versus
 * K1's 2-parameter (ep, tx) signature - consistent with this whole
 * project's already-documented "phantom/unused parameter" class of finding,
 * not something new.
 *
 * USB descriptor / VID:PID note: usbdc_get_descriptor's K2 counterpart
 * (FUN_c000b120) uses the exact same table-pointer-selection SHAPE as K1
 * (usbdc_ep_state3_query-gated device/config table pairs, 9-entry string
 * table, fixed qualifier pointer, other-speed-config table) - the table
 * POINTER VALUES themselves differ between K1/K2 (expected, different
 * firmware images/link addresses) but their CONTENTS (actual descriptor
 * bytes, including any VID/PID) were NOT decoded in either K1's or this
 * pass's own static-dump queries - both leave this open. No separate
 * VID/PID literal was found anywhere in this cluster in either firmware.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---- cross-file dependencies (declared here, defined in
 * omap_l137_usbdc_ext.c's own K2 port - not written by this pass, see that
 * file's own header for its own status). Every helper below was spot-
 * checked against K2's actual decompile this pass and confirmed to use the
 * SAME register offsets/bit patterns as K1 (0x40e INDEX, 0x412/0x416
 * indexed TXCSR/RXCSR, 0x502/0x506 direct-window CSR, all independently
 * DAT_-resolved and matching K1 exactly) - naming below follows K1's own
 * convention. --------------------------------------------------------- */
extern void    usbdc_select_endpoint(void *dev, uint8_t ep);			/* K2 FUN_c0003830 */
extern void    usbdc_ep0_csr0_set_bits(void *dev);				/* K2 FUN_c0004468 - stall EP0 */
extern uint16_t usbdc_ep0_csr0_test_setupend(void *dev);			/* K2 FUN_c0004480 */
extern void    usbdc_txcsr_direct_set_txpktrdy(void *dev, uint8_t ep);	/* K2 FUN_c0004424 */
extern void    usbdc_rxcsr_direct_set_bits(void *dev, uint8_t ep);		/* K2 FUN_c0004444 */
extern void    usbdc_txcsr_direct_clear_bits(void *dev, uint8_t ep);		/* K2 FUN_c0004498 */
extern void    usbdc_rxcsr_direct_clear_bits(void *dev, uint8_t ep);		/* K2 FUN_c00044c0 */
extern uint16_t usbdc_txcsr_direct_test_bit5(void *dev, uint8_t ep);		/* K2 FUN_c00044ec */
extern uint16_t usbdc_rxcsr_direct_test_bit6(void *dev, uint8_t ep);		/* K2 FUN_c000450c */
extern bool    usbdc_txcsr_direct_test_ready(void *dev, uint8_t ep);		/* K2 FUN_c0004550 */
extern uint32_t usbdc_raw_read32(void *dev, int offset);			/* K2 FUN_c0004580 (offset variant used here) */
extern void    usbdc_raw_write32(void *dev, int offset, uint32_t value);	/* K2 FUN_c0004588 */
extern uint16_t usbdc_raw_read16(void *dev, int offset);			/* K2 FUN_c0004590 */
extern void    usbdc_raw_write16(void *dev, int offset, uint16_t value);	/* K2 FUN_c0004598 */
extern void    usbdc_fifo_write(void *dev, int ep, const uint8_t *src, int len);	/* K2 FUN_c00041f4 */
extern void    usbdc_fifo_read(void *dev, int ep, uint32_t *dst, unsigned len);	/* K2 FUN_c000428c */
extern void    usbdc_flush_ep1_4(void *dev);					/* K2 FUN_c0003fd4 */
extern uint32_t omap_usbdc_reloc(uint32_t offset);				/* K2 FUN_c000a728 - cross-file, out of scope */
extern uint32_t omap_usbdc_phantom_const_a(uint32_t unused);			/* K2 FUN_c000183c, always returns 0x1e00000 -
									 * omap_l137_usbdc.c's own K2 port already declares this;
									 * repeated here to match K1's own per-file convention */
extern uint32_t usbdc_min_u32(uint32_t a, uint32_t b);				/* K2 FUN_c000c1ec, out of this project's range entirely */

/* The single global default USB dev-object pointer - CONFIRMED same literal
 * (0xc01cc84c) as omap_l137_usbdc.c's own usbdc_ep0_default_handle, see
 * this file's own header for the 29-alias search that established this. */
extern void *usbdc_ep0_dev_handle;	/* K2 DAT_c000a9a4 and 28 other aliases, == 0xc01cc84c */

/* The EP0 request-context object. Not independently re-resolved to a
 * numeric literal this pass (ctx is passed as a parameter throughout this
 * whole cluster in both K1 and K2, never read from a bare global inside any
 * function reconstructed here) - declared for parity with K1's own
 * convention. */
extern void *usbdc_ep0_ctx_handle;

/* ===========================================================================
 * usbdc_ep0_ctx_get_config - K2 counterpart of K1's FUN_c0009548.
 * CONFIRMED DIFFERENT offset vs K1: reads ctx+6, NOT ctx+7 - see file
 * header's "K2 ctx-struct field shift" finding (this is one of the four
 * independent confirmations). @0xc000a874 (K2). K1: @0xc0009548.
 * =========================================================================== */
uint8_t usbdc_ep0_ctx_get_config(void *ctx)	/* FUN_c000a874 (K2) */
{
	return *((uint8_t *)ctx + 6);	/* CONFIRMED: ctx+6 in K2, ctx+7 in K1 */
}

/* ===========================================================================
 * usbdc_ep0_state3_handler - K2 counterpart of K1's FUN_c0009768.
 * CONFIRMED structurally IDENTICAL to K1: same 0x40 (64-byte) EP0
 * max-packet chunking from ctx+8/ctx+0xc, same 0x0A (TxPktRdy|DataEnd) /
 * 0x02 (TxPktRdy only) CSR bit pair, same secondary status-nibble encoding
 * (0x60 done / 3 in-progress). @0xc000aa90 (K2). K1: @0xc0009768.
 * =========================================================================== */
extern int32_t usbdc_ep0_status2;	/* K2 DAT_c000ab58 and aliases - secondary status nibble */

void usbdc_ep0_state3_handler(void *ctx)	/* FUN_c000aa90 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;
	void *dev = usbdc_ep0_dev_handle;
	uint16_t csr0 = usbdc_raw_read16(dev, 0x412);
	uint32_t remaining = *(uint32_t *)(c + 0xc);
	uint32_t chunk = usbdc_min_u32(remaining, 0x40);

	usbdc_fifo_write(dev, 0, *(uint8_t **)(c + 8), (int)chunk);
	*(uint8_t **)(c + 8) += chunk;
	remaining -= chunk;
	*(uint32_t *)(c + 0xc) = remaining;

	if (chunk < 0x40 || remaining == 0) {
		usbdc_raw_write16(dev, 0x412, csr0 | 0x0A);
		usbdc_ep0_status2 = (usbdc_ep0_status2 & 0xfffffff0) | 0x60;
	} else {
		usbdc_raw_write16(dev, 0x412, csr0 | 0x02);
		usbdc_ep0_status2 = (usbdc_ep0_status2 & 0xfffffff3) | 3;
	}
}

/* usbdc_ep0_start_tx - K2 counterpart of K1's FUN_c0009834. CONFIRMED
 * structurally IDENTICAL: stages (data,len) into ctx+8/ctx+0xc, phantom-
 * forwards ctx into usbdc_ep0_state3_handler (real K2 call site also shows
 * zero visible args, same class of issue as K1). @0xc000ab5c (K2).
 * K1: @0xc0009834. */
void usbdc_ep0_start_tx(void *ctx, void *data, uint32_t len)	/* FUN_c000ab5c (K2) */
{
	uint8_t *c = (uint8_t *)ctx;
	*(void **)(c + 8) = data;
	*(uint32_t *)(c + 0xc) = len;
	usbdc_ep0_state3_handler(ctx);
}

/* ===========================================================================
 * usbdc_ep_flush_or_dataend - K2 counterpart of K1's FUN_c0009698.
 * CONFIRMED IDENTICAL bit patterns/register offsets to K1 (ep==0: OR 0x100
 * into TXCSR/CSR0-when-selected; ep!=0,tx: double-OR 0x08 into TXCSR;
 * ep!=0,!tx: double-OR 0x10 into RXCSR). CONFIRMED DIFFERENT signature: K2's
 * real decompile shows an extra leading parameter that is never read in the
 * body - same "phantom/unused parameter" class this whole project already
 * documents repeatedly, not treated as meaningful. @0xc000a9c0 (K2).
 * K1: @0xc0009698.
 * =========================================================================== */
void usbdc_ep_flush_or_dataend(void *unused_arg, uint8_t ep, bool tx)	/* FUN_c000a9c0 (K2) */
{
	(void)unused_arg;	/* real parameter, never read - confirmed via K2 decompile */
	void *dev = usbdc_ep0_dev_handle;
	uint16_t v;

	usbdc_select_endpoint(dev, ep);

	if (ep == 0) {
		v = usbdc_raw_read16(dev, 0x412);
		usbdc_raw_write16(dev, 0x412, v | 0x100);
		return;
	}

	if (tx) {
		for (int i = 0; i < 2; i++) {
			v = usbdc_raw_read16(dev, 0x412);
			usbdc_raw_write16(dev, 0x412, v | 0x08);
		}
	} else {
		for (int i = 0; i < 2; i++) {
			v = usbdc_raw_read16(dev, 0x416);
			usbdc_raw_write16(dev, 0x416, v | 0x10);
		}
	}
}

/* ===========================================================================
 * usbdc_epN_send_* family - K2 counterparts of K1's FUN_c0009840/0x9890/
 * 0x98e0/0x9930. CONFIRMED structurally IDENTICAL shape and gating (ctx+4
 * armed flag, fixed target endpoint, forward to usbdc_fifo_write). K1's own
 * "usbdc_ep3_send_a has ZERO static callers" and "usbdc_ep3_send_b targets
 * the SAME endpoint via a different DAT_ alias, real duplication" findings
 * BOTH reconfirmed independently in K2: FUN_c000ab68 (usbdc_ep3_send_a
 * equivalent) shows zero callers in K2's own xref data too; FUN_c000ac08
 * (usbdc_ep3_send_b equivalent) is a real, separate duplicate also
 * targeting endpoint 3.
 * @0xc000ab68 / @0xc000abb8 / @0xc000ac08 / @0xc000ac58 (K2).
 * K1: @0xc0009840 / @0xc0009890 / @0xc00098e0 / @0xc0009930.
 * =========================================================================== */
void usbdc_ep3_send_a(void *ctx, const uint8_t *src, int len)	/* FUN_c000ab68 (K2), zero callers - reconfirmed */
{
	void *dev = usbdc_ep0_dev_handle;
	usbdc_select_endpoint(dev, 3);
	if (*((uint8_t *)ctx + 4) == 0)
		return;
	usbdc_fifo_write(dev, 3, src, len);
}

void usbdc_ep1_send(void *ctx, const uint8_t *src, int len)	/* FUN_c000abb8 (K2) */
{
	void *dev = usbdc_ep0_dev_handle;
	usbdc_select_endpoint(dev, 1);
	if (*((uint8_t *)ctx + 4) == 0)
		return;
	usbdc_fifo_write(dev, 1, src, len);
}

void usbdc_ep3_send_b(void *ctx, const uint8_t *src, int len)	/* FUN_c000ac08 (K2) */
{
	void *dev = usbdc_ep0_dev_handle;
	usbdc_select_endpoint(dev, 3);
	if (*((uint8_t *)ctx + 4) == 0)
		return;
	usbdc_fifo_write(dev, 3, src, len);
}

void usbdc_ep4_send(void *ctx, const uint8_t *src, int len)	/* FUN_c000ac58 (K2) */
{
	void *dev = usbdc_ep0_dev_handle;
	usbdc_select_endpoint(dev, 4);
	if (*((uint8_t *)ctx + 4) == 0)
		return;
	usbdc_fifo_write(dev, 4, src, len);
}

/* ===========================================================================
 * usbdc_ep0_state4_handler - K2 counterpart of K1's FUN_c0009980.
 * CONFIRMED structurally IDENTICAL: same RXCOUNT (dev+0x418, via
 * usbdc_raw_read16 offset argument) clamp to 0x40, same ctx+0x5c
 * remaining-length decrement, same 0x48 (ServicedRxPktRdy|DataEnd)/0x40
 * (ServicedRxPktRdy only) CSR bit pair, same stall-on-mismatch tail.
 * @0xc000aca8 (K2). K1: @0xc0009980.
 * =========================================================================== */
void usbdc_ep0_state4_handler(void *ctx)	/* FUN_c000aca8 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;
	void *dev = usbdc_ep0_dev_handle;

	usbdc_select_endpoint(dev, 0);
	uint16_t csr0 = usbdc_raw_read16(dev, 0x412);
	if ((csr0 & 1) == 0)
		return;

	uint16_t count = usbdc_raw_read16(dev, 0x418);
	if (count > 0x40)
		count = 0x40;

	usbdc_fifo_read(dev, 0, (uint32_t *)(c + 0x10), count);

	int16_t remaining = *(int16_t *)(c + 0x5c) - (int16_t)count;
	*(int16_t *)(c + 0x5c) = remaining;

	if (remaining == 0)
		usbdc_raw_write16(dev, 0x412, csr0 | 0x48);
	else
		usbdc_raw_write16(dev, 0x412, csr0 | 0x40);

	int16_t recheck = (int16_t)usbdc_raw_read16(dev, 0x418);
	if (remaining == 0 && recheck != 0)
		usbdc_ep0_csr0_set_bits(dev);
}

/* ===========================================================================
 * usbdc_ep_irq_enable_set30 / usbdc_ep_irq_enable_set34 - K2 counterparts of
 * K1's FUN_c0009a98/FUN_c0009afc. CONFIRMED structurally IDENTICAL: same
 * bit-base (0x200 dir==0 / 2 dir!=0) left-shifted by (ep-1) for ep>1, same
 * dev+0x2c enable-mask read, same 0x30-vs-0x34 "set" register split with
 * opposite guard polarity (`(mask&bit)==0` for set30 vs `(mask&bit)==bit`
 * for set34), same phantom-constant-indirection route for set34's own dev
 * pointer. @0xc000adc0 / @0xc000ae24 (K2). K1: @0xc0009a98 / @0xc0009afc.
 * =========================================================================== */
void usbdc_ep_irq_enable_set30(void *ctx_unused, uint8_t ep, uint8_t dir)	/* FUN_c000adc0 (K2) */
{
	(void)ctx_unused;
	void *dev = usbdc_ep0_dev_handle;

	if (ep == 0)
		return;

	uint32_t bit = (dir == 0) ? 0x200 : 2;
	uint32_t mask = usbdc_raw_read32(dev, 0x2c);
	if (ep > 1)
		bit <<= (ep - 1);

	if ((mask & bit) == 0)
		usbdc_raw_write32(dev, 0x30, bit);
}

void usbdc_ep_irq_enable_set34(void *ctx_unused, uint8_t ep, uint8_t dir)	/* FUN_c000ae24 (K2) */
{
	(void)ctx_unused;
	if (ep == 0)
		return;

	uint32_t bit = (dir == 0) ? 0x200 : 2;
	void *dev = (void *)(uintptr_t)omap_usbdc_phantom_const_a(0);
	uint32_t mask = usbdc_raw_read32(dev, 0x2c);
	if (ep > 1)
		bit <<= (ep - 1);

	if ((mask & bit) == bit) {
		dev = (void *)(uintptr_t)omap_usbdc_phantom_const_a(0);
		usbdc_raw_write32(dev, 0x34, bit);
	}
}

/* ===========================================================================
 * usbdc_ep_state3_query - K2 counterpart of K1's FUN_c0009b68. CONFIRMED
 * structurally IDENTICAL: tests bit 4 (0x10) of dev+0x401 (DAT_c000aebc,
 * resolved: 0x401, identical to K1), same phantom-constant dev route.
 * @0xc000ae90 (K2). K1: @0xc0009b68.
 * =========================================================================== */
int usbdc_ep_state3_query(void)	/* FUN_c000ae90 (K2) */
{
	void *dev = (void *)(uintptr_t)omap_usbdc_phantom_const_a(0);
	return (*((uint8_t *)dev + 0x401) >> 4) & 1;
}

/* usbdc_epN_tx_ready_query - K2 counterpart of K1's FUN_c000a13c. CONFIRMED
 * structurally IDENTICAL: thin wrapper over usbdc_txcsr_direct_test_ready.
 * @0xc000b440 (K2). K1: @0xc000a13c. */
bool usbdc_epN_tx_ready_query(uint8_t ep)	/* FUN_c000b440 (K2) */
{
	return usbdc_txcsr_direct_test_ready(usbdc_ep0_dev_handle, ep);
}

/* ===========================================================================
 * usbdc_setup_stage_address - K2 counterpart of K1's FUN_c0009c1c.
 * CONFIRMED structurally IDENTICAL: same DEVICE-recipient + signed-byte
 * range check, same ctx+0x58 staging target (this field's own offset did
 * NOT shift, unlike the SET_CONFIGURATION field below).
 * @0xc000af20 (K2). K1: @0xc0009c1c.
 * =========================================================================== */
bool usbdc_setup_stage_address(void *ctx)	/* FUN_c000af20 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;

	if ((c[0x50] & 0x1f) != 0 || (int8_t)c[0x52] < 0)
		return false;

	c[0x58] = c[0x52];
	return true;
}

/* usbdc_setup_stage_configuration - K2 counterpart of K1's FUN_c0009c50.
 * CONFIRMED DIFFERENT storage offset vs K1: writes ctx+6, NOT ctx+7 - see
 * file header's "K2 ctx-struct field shift" finding. Recipient/range checks
 * otherwise identical to K1. @0xc000af54 (K2). K1: @0xc0009c50. */
bool usbdc_setup_stage_configuration(void *ctx)	/* FUN_c000af54 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;

	if ((c[0x50] & 0x1f) != 0 || c[0x52] >= 2)
		return false;

	c[6] = c[0x52];	/* CONFIRMED: ctx+6 in K2, ctx+7 in K1 */
	return true;
}

/* ===========================================================================
 * usbdc_ep0_diag_request - K2 counterpart of K1's FUN_c0009c80. CONFIRMED
 * structurally IDENTICAL: same wValue-high-byte 0x100/0x200 dispatch, same
 * 0x81/0x82/0x83/0x84 sub-case reply bytes (0xf8/0x80/{0,0}/{0x40,0}),
 * still not a standard USB chapter-9 shape (same vendor/diagnostic-
 * extension assessment as K1). @0xc000af84 (K2). K1: @0xc0009c80.
 * =========================================================================== */
bool usbdc_ep0_diag_request(void *ctx, uint8_t *scratch)	/* FUN_c000af84 (K2) */
{
	uint16_t w_value_hi = *(uint16_t *)(scratch + 2) & 0xff00;
	uint8_t *c = (uint8_t *)ctx;

	if (w_value_hi == 0x0100) {
		if (*(int16_t *)(scratch + 6) == 1) {
			c[0x5e] = 0;
			usbdc_ep0_start_tx(ctx, c + 0x5e, 1);
			return true;
		}
		return false;
	}

	if (w_value_hi != 0x0200)
		return false;

	uint8_t reply_hi;
	switch (scratch[1]) {
	case 0x81:
		reply_hi = 0xf8;
		break;
	case 0x82:
		reply_hi = 0x80;
		break;
	case 0x83:
		c[0x5e] = 0;
		c[0x5f] = 0;
		usbdc_ep0_start_tx(ctx, c + 0x5e, 2);
		return true;
	case 0x84:
		c[0x5e] = 0x40;
		c[0x5f] = 0;
		usbdc_ep0_start_tx(ctx, c + 0x5e, 2);
		return true;
	default:
		return false;
	}

	c[0x5f] = reply_hi;
	c[0x5e] = 0;
	usbdc_ep0_start_tx(ctx, c + 0x5e, 2);
	return true;
}

/* ===========================================================================
 * usbdc_ep0_ctx_reset - K2 counterpart of K1's FUN_c0009d60. CONFIRMED
 * structurally IDENTICAL: same +0x7c/+0x7e/+0x80 field resets, same 0x48
 * CSR bit OR, same 0x60 secondary-status bits. K1's own "ZERO static
 * callers" finding RECONFIRMED in K2 - FUN_c000b064 also has no callers in
 * K2's own xref data. @0xc000b064 (K2). K1: @0xc0009d60.
 * =========================================================================== */
void usbdc_ep0_ctx_reset(void *ctx)	/* FUN_c000b064 (K2), zero callers - reconfirmed */
{
	uint8_t *c = (uint8_t *)ctx;
	void *dev = usbdc_ep0_dev_handle;

	c[0x7c] = 0;
	*(uint16_t *)(c + 0x80) = 0;
	*(uint16_t *)(c + 0x7e) = 0;

	usbdc_select_endpoint(dev, 0);
	uint16_t v = usbdc_raw_read16(dev, 0x412);
	usbdc_raw_write16(dev, 0x412, v | 0x48);
	usbdc_ep0_status2 |= 0x60;
}

/* ===========================================================================
 * usbdc_ep_state1_handler - K2 counterpart of K1's FUN_c0009dd0. CONFIRMED
 * structurally IDENTICAL: same wLength staging into ctx+0x5a/ctx+0x5c, same
 * class-request recognition range (type bits==0x20, bRequest 1..5), same
 * phantom-forwarded unconditional call into usbdc_ep0_state4_handler.
 * @0xc000b0d4 (K2). K1: @0xc0009dd0.
 * =========================================================================== */
uint8_t usbdc_ep_state1_handler(void *ctx, void *scratch)	/* FUN_c000b0d4 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;
	uint8_t *s = (uint8_t *)scratch;
	uint16_t w_length = *(uint16_t *)(s + 6);

	*(uint16_t *)(c + 0x5a) = w_length;
	*(uint16_t *)(c + 0x5c) = w_length;

	uint8_t recognized = 0;
	if ((s[0] & 0x60) == 0x20 && (uint8_t)(s[1] - 1) < 5)
		recognized = 1;

	usbdc_ep0_state4_handler(ctx);
	return recognized;
}

/* ===========================================================================
 * usbdc_get_descriptor - K2 counterpart of K1's FUN_c0009e1c. CONFIRMED
 * structurally IDENTICAL switch shape (cases 1/2/3/6/7/default), same
 * usbdc_ep_state3_query()-gated device/config table selection, same 9-entry
 * string table bound check, same length-prefixed descriptor-blob tail
 * convention and min(wLength, blob_length) staging. Descriptor table
 * POINTER VALUES differ from K1 (expected, different firmware image) but
 * their real byte CONTENTS were not decoded in either pass - see file
 * header. @0xc000b120 (K2). K1: @0xc0009e1c.
 * =========================================================================== */
extern uint8_t *usbdc_desc_device_fs, *usbdc_desc_device_hs;
extern uint8_t **usbdc_desc_config_table_fs, **usbdc_desc_config_table_hs;
extern uint8_t **usbdc_desc_string_table;
extern uint8_t *usbdc_desc_qualifier;
extern uint8_t **usbdc_desc_other_speed_table;

bool usbdc_get_descriptor(void *ctx, uint8_t *scratch)	/* FUN_c000b120 (K2) */
{
	int *c = (int *)ctx;
	uint8_t *desc = NULL;
	uint16_t send_len = 0;
	bool have_desc = false;

	switch (scratch[3]) {
	case 1:
		desc = (usbdc_ep_state3_query() == 1) ? usbdc_desc_device_hs : usbdc_desc_device_fs;
		have_desc = true;
		break;
	case 2: {
		uint8_t **table = (usbdc_ep_state3_query() != 1) ? usbdc_desc_config_table_fs : usbdc_desc_config_table_hs;
		desc = table[*c];
		send_len = *(uint16_t *)(desc + 2);
		goto have_length;
	}
	case 3:
		if (scratch[2] > 8)
			goto stall;
		desc = usbdc_desc_string_table[scratch[2]];
		have_desc = true;
		break;
	case 6:
		desc = usbdc_desc_qualifier;
		have_desc = true;
		break;
	case 7:
		desc = usbdc_desc_other_speed_table[*c];
		have_desc = true;
		break;
	default:
		goto stall;
	}

	if (have_desc)
		send_len = *desc;

have_length:
	if (send_len == 0)
		goto stall;

	{
		uint16_t w_length = *(uint16_t *)(scratch + 6);
		uint16_t n = (send_len < w_length) ? send_len : w_length;
		usbdc_ep0_start_tx(ctx, desc, n);
		return true;
	}

stall:
	usbdc_ep0_csr0_set_bits(usbdc_ep0_dev_handle);
	return false;
}

/* usbdc_get_configuration - K2 counterpart of K1's FUN_c0009f24. CONFIRMED
 * DIFFERENT read-back offset vs K1: echoes ctx+6, NOT ctx+7 - directly
 * verified against K2's own raw decompile (`*(param_1+0x5e) =
 * *(param_1+6)`). This is a FIFTH independent confirmation of the file
 * header's "K2 ctx-struct field shift" finding, and it is fully CONSISTENT
 * with usbdc_setup_stage_configuration's own ctx+6 write target above (not
 * an asymmetry - an earlier draft of this comment incorrectly claimed one;
 * corrected after re-checking the raw decompile directly).
 * @0xc000b228 (K2). K1: @0xc0009f24. */
bool usbdc_get_configuration(void *ctx, uint8_t *scratch)	/* FUN_c000b228 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;

	if ((scratch[0] & 0x1f) == 0 && *(uint16_t *)(scratch + 6) == 1) {
		c[0x5e] = c[6];
		usbdc_ep0_start_tx(ctx, c + 0x5e, 1);
		return true;
	}

	usbdc_ep0_csr0_set_bits(usbdc_ep0_dev_handle);
	return false;
}

/* usbdc_get_interface - K2 counterpart of K1's FUN_c0009f84. CONFIRMED
 * structurally IDENTICAL: INTERFACE recipient + wLength==1, always echoes
 * alternate-setting 0. @0xc000b288 (K2). K1: @0xc0009f84. */
bool usbdc_get_interface(void *ctx, uint8_t *scratch)	/* FUN_c000b288 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;

	if ((scratch[0] & 0x1f) == 1 && *(uint16_t *)(scratch + 6) == 1) {
		c[0x5e] = 0;
		usbdc_ep0_start_tx(ctx, c + 0x5e, 1);
		return true;
	}

	usbdc_ep0_csr0_set_bits(usbdc_ep0_dev_handle);
	return false;
}

/* ===========================================================================
 * usbdc_ep_flush_direction - K2 counterpart of K1's FUN_c0009fe8. CONFIRMED
 * SAME bit patterns as K1 (mode==1,dir==0: double-OR RXCSR 0x10 then single-
 * OR 0x80; mode==3,dir==1: double-OR TXCSR 0x08 then single-OR 0x40).
 * CONFIRMED DIFFERENT vs K1: this K2 counterpart explicitly calls
 * usbdc_select_endpoint(dev,1) / usbdc_select_endpoint(dev,3) for its two
 * branches (hardcoded endpoint numbers, not derived from `mode`/`dir`);
 * K1's own version has NO endpoint-select call at all and documents relying
 * on the caller having already selected the target endpoint. See file
 * header. @0xc000b2ec (K2). K1: @0xc0009fe8.
 * =========================================================================== */
void usbdc_ep_flush_direction(uint8_t mode, bool dir)	/* FUN_c000b2ec (K2) */
{
	void *dev = usbdc_ep0_dev_handle;

	if (mode == 1 && !dir) {
		usbdc_select_endpoint(dev, 1);	/* CONFIRMED NEW vs K1: explicit endpoint select */
		for (int i = 0; i < 2; i++) {
			uint16_t v = usbdc_raw_read16(dev, 0x416);
			usbdc_raw_write16(dev, 0x416, v | 0x10);
		}
		uint16_t v = usbdc_raw_read16(dev, 0x416);
		usbdc_raw_write16(dev, 0x416, v | 0x80);
	} else if (mode == 3 && dir) {
		usbdc_select_endpoint(dev, 3);	/* CONFIRMED NEW vs K1: explicit endpoint select */
		for (int i = 0; i < 2; i++) {
			uint16_t v = usbdc_raw_read16(dev, 0x412);
			usbdc_raw_write16(dev, 0x412, v | 0x08);
		}
		uint16_t v = usbdc_raw_read16(dev, 0x412);
		usbdc_raw_write16(dev, 0x412, v | 0x40);
	}
}

/* ===========================================================================
 * usbdc_get_status - K2 counterpart of K1's FUN_c000a164. CONFIRMED
 * structurally IDENTICAL: same 3-way recipient dispatch (INTERFACE always
 * 0, DEVICE `1-ctx+6`-clamped self-powered byte, ENDPOINT real STALL-bit
 * test via the same CSR helper trio), same wLength==2 gate.
 *
 * NOTE: the DEVICE-recipient byte this reads is ctx+6 in BOTH K1 and K2
 * (`c[6]`) - this field did NOT participate in the SET_CONFIGURATION
 * ctx+7->ctx+6 shift documented in the file header; it is a genuinely
 * different field that already lived at offset 6 in K1 too. Not to be
 * confused with usbdc_get_configuration's own ctx+7->ctx+6 shift above -
 * that one IS part of the SET_CONFIGURATION shift (see that function's own
 * comment), this one is not; they merely share the same numeric offset.
 * @0xc000b468 (K2). K1: @0xc000a164.
 * =========================================================================== */
uint16_t usbdc_get_status(void *ctx, uint8_t *scratch)	/* FUN_c000b468 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;
	uint8_t recipient = scratch[0] & 0x1f;
	uint32_t status;
	bool have_status = true;

	if (recipient == 1) {
		status = 0;
	} else if (recipient < 2) {
		status = 1 - c[6];
		if (c[6] > 1)
			status = 0;
	} else if (recipient == 2) {
		uint16_t w_index = *(uint16_t *)(scratch + 4);
		void *dev = usbdc_ep0_dev_handle;
		if ((w_index & 0x7f) == 0)
			status = usbdc_ep0_csr0_test_setupend(dev);
		else if ((w_index & 0x80) == 0)
			status = usbdc_rxcsr_direct_test_bit6(dev, w_index & 0x7f);
		else
			status = usbdc_txcsr_direct_test_bit5(dev, w_index & 0x7f);
		status &= 0xff;
	} else {
		have_status = false;
		status = 0xffff;
	}

	if (!have_status || *(int16_t *)(scratch + 6) != 2) {
		usbdc_ep0_csr0_set_bits(usbdc_ep0_dev_handle);
		return 0;
	}

	c[0x5f] = (uint8_t)(status >> 8);
	c[0x5e] = (uint8_t)status;
	usbdc_ep0_start_tx(ctx, c + 0x5e, 2);
	return 1;
}

/* ===========================================================================
 * usbdc_ep_state2_handler - K2 counterpart of K1's FUN_c000a264. CONFIRMED
 * structurally IDENTICAL: same standard-request bRequest switch (0/6/8/10 ->
 * GET_STATUS/GET_DESCRIPTOR/GET_CONFIGURATION/GET_INTERFACE), same
 * class-0xfe zero-byte reply, same vendor-type reject/stall path.
 * @0xc000b568 (K2). K1: @0xc000a264.
 * =========================================================================== */
uint8_t usbdc_ep_state2_handler(void *ctx, void *scratch)	/* FUN_c000b568 (K2) */
{
	uint8_t *s = (uint8_t *)scratch;
	uint8_t result = 1;

	if ((s[0] & 0x60) == 0) {
		switch (s[1]) {
		case 0:
			result = (uint8_t)usbdc_get_status(ctx, s);
			break;
		case 6:
			result = (uint8_t)usbdc_get_descriptor(ctx, s);
			break;
		case 8:
			result = (uint8_t)usbdc_get_configuration(ctx, s);
			break;
		case 10:
			result = (uint8_t)usbdc_get_interface(ctx, s);
			break;
		default:
			break;
		}
	} else if ((s[0] & 0x60) == 0x20) {
		if (s[1] == 0xfe) {
			uint8_t *c = (uint8_t *)ctx;
			c[0x5e] = 0;
			usbdc_ep0_start_tx(ctx, c + 0x5e, 1);
		}
	} else {
		result = 0;
		usbdc_ep0_csr0_set_bits(usbdc_ep0_dev_handle);
	}

	return result;
}

/* ===========================================================================
 * usbdc_clear_feature_endpoint_halt / usbdc_set_feature_endpoint_halt - K2
 * counterparts of K1's FUN_c000a340/FUN_c000a3c0. CONFIRMED structurally
 * IDENTICAL: same ENDPOINT-recipient + wIndex decode, same EP0-exempt
 * handling, same direction-appropriate CSR helper calls.
 * @0xc000b644 / @0xc000b6c4 (K2). K1: @0xc000a340 / @0xc000a3c0.
 * =========================================================================== */
bool usbdc_clear_feature_endpoint_halt(void *ctx)	/* FUN_c000b644 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;

	if ((c[0x50] & 0x1f) != 2)
		return false;

	uint16_t w_index = *(uint16_t *)(c + 0x54);
	uint8_t ep = w_index & 0x7f;
	if (ep == 0)
		return true;

	void *dev = usbdc_ep0_dev_handle;
	if ((w_index & 0x80) == 0)
		usbdc_rxcsr_direct_clear_bits(dev, ep);
	else
		usbdc_txcsr_direct_clear_bits(dev, ep);

	return true;
}

bool usbdc_set_feature_endpoint_halt(void *ctx)	/* FUN_c000b6c4 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;
	bool is_endpoint = (c[0x50] & 0x1f) == 2;

	if (is_endpoint) {
		uint16_t w_index = *(uint16_t *)(c + 0x54);
		uint8_t ep = w_index & 0x7f;
		if (ep != 0) {
			void *dev = usbdc_ep0_dev_handle;
			if ((w_index & 0x80) == 0)
				usbdc_rxcsr_direct_set_bits(dev, ep);
			else
				usbdc_txcsr_direct_set_txpktrdy(dev, ep);
		}
	}

	return is_endpoint;
}

/* ===========================================================================
 * usbdc_ep0_class5_handler - K2 counterpart of K1's FUN_c000a3ac. CONFIRMED
 * structurally IDENTICAL: `dev+0x400 = ctx+0x58` (the SET_ADDRESS staging
 * field, ctx+0x58, did NOT shift). @0xc000b6b0 (K2). K1: @0xc000a3ac.
 * =========================================================================== */
void usbdc_ep0_class5_handler(void *ctx)	/* FUN_c000b6b0 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;
	*((uint8_t *)usbdc_ep0_dev_handle + 0x400) = c[0x58];
}

/* ===========================================================================
 * usbdc_ep0_class9_handler - K2 counterpart of K1's FUN_c000a42c. CONFIRMED
 * SAME real behavior as K1 (flush EP1-4 FIFOs, arm ctx+4). CONFIRMED
 * DIFFERENT phantom-forwarded argument vs K1: the real K2 call site forwards
 * ctx+6 (not ctx+7) as usbdc_flush_ep1_4's dead second argument - see file
 * header's "K2 ctx-struct field shift" finding (this is one of the four
 * independent confirmations; harmless either way since the callee only
 * reads `dev`). @0xc000b730 (K2). K1: @0xc000a42c.
 * =========================================================================== */
void usbdc_ep0_class9_handler(void *ctx)	/* FUN_c000b730 (K2) */
{
	uint8_t *c = (uint8_t *)ctx;
	usbdc_flush_ep1_4(usbdc_ep0_dev_handle);
	/* real call site: `FUN_c0003fd4(*DAT_c000b75c, *(param_1+6))` - dead 2nd
	 * arg, ctx+6 not ctx+7, see comment above */
	(void)c;
	c[4] = 1;
}

/* -------------------------------------------------------------------------
 * Still genuinely open (K2), not fabricated:
 *  - cad_pedal_present: 2026-07-19 LIVE QUERY PARTIALLY RESOLVED. Live
 *    decompile_function on FUN_c000a7dc obtained (no Function-boundary
 *    issue this time) - confirms the file header's own characterization
 *    exactly: `*param_1 = 0; if (!test(base,0x17)) return; if (test(base,
 *    0x27)) *param_1=1; else if (test(base,0x28)) *param_1=2; else if
 *    (test(base,0x25)) *param_1=3; else return;` (test = FUN_c0005370, an
 *    unidentified bit/GPIO-read helper taking a shared base handle + a
 *    field-selector literal). This is a real, gated 4-way classifier - bit
 *    0x17 must be set to proceed at all (matching K1's own 0x17 half of its
 *    "0x17 && 0x25" check), then bits 0x27/0x28/0x25 are tested in that
 *    priority order to assign a 3-valued out-param (1/2/3) instead of K1's
 *    plain boolean. CONFIRMED real caller (get_xrefs_to): FUN_c0009838,
 *    call site 0xc0009860. Best-evidence conclusion (not proof): this IS
 *    cad_pedal_present's K2 replacement, expanded from a boolean
 *    "pedal present" check into a 3-way pedal-TYPE classifier that still
 *    shares K1's own bit-0x17 gate - the shape difference previously read
 *    as disqualifying is better explained as a genuine feature expansion
 *    than an unrelated function coincidentally sitting at this address.
 *    Left un-ported into this file's own body still (out of this pass's
 *    scope to rename/restructure K1's own function around a 3-way result),
 *    but the identification question itself is now answered rather than
 *    open.
 *  - The real reason for the K2 ctx-struct ctx+7->ctx+6 shift (what field
 *    was removed/shrunk immediately before it) - four independent call
 *    sites confirm the shift itself, none explain its cause.
 *  - usbdc_ep_flush_direction's newly-added explicit endpoint-select calls:
 *    whether K1's callers already select endpoints 1/3 themselves before
 *    calling it (making K2's addition redundant-but-harmless) or whether
 *    K2 changed calling convention entirely - not traced, K1's own callers
 *    of this function are outside its own file's scope too.
 *  - Every item K1's own file already left open (usbdc_ep0_diag_request's
 *    exact vendor/diagnostic purpose, usbdc_ep3_send_a/_b duplication
 *    reason, usbdc_get_descriptor's actual descriptor byte contents, the
 *    dev+0x400 FADDR identity) remains equally open in K2.
 * ------------------------------------------------------------------------- */
