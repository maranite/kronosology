/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l137_usbdc_ep0.c - the USB0 control-endpoint (EP0) request-handler
 * cluster: every function omap_l137_usbdc.c's own confirmed anchor range
 * and omap_l137_usbdc_ext.c's Section 6/7 ISR dispatch call into but leave
 * as bare externs "out of range". Reconstructs the address range
 * 0xc0009480-0xc000a45c (this project's assigned sweep), MINUS the 5
 * addresses already reconstructed elsewhere:
 *   0xc0009480  panelbus_hw_negotiate_ready   (panelbus_dispatch.c)
 *   0xc0009534  eva_board_reset_handler       (eva_board_main.c)
 *   0xc0009540  spurious - unreachable literal pool, not real code
 *               (eva_board_main.c's own reset-vector section explains this)
 *   0xc0009550  omap_usbdc_clear_pending_state (omap_l137_usbdc.c)
 *   0xc0009574  omap_usbdc_object_init         (omap_l137_usbdc.c)
 * 29 functions reconstructed here.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json, 2026-07-18), no live Ghidra bridge access this pass
 * (concurrency-unsafe, per this project's own operating constraint).
 *
 * NOT its own anchored compilation unit: no "../<name>.cpp" filename string is
 * reachable from any fault/assert call site in this address range (this
 * cluster uses "stall EP0" as its error path - usbdc_ep0_csr0_set_bits,
 * see below - not the crypto_at88_fault-style hard-halt assert every other
 * subsystem's error path goes through, so there is no file-string literal
 * to find here at all). Attributed on overwhelming code-shape + address
 * evidence instead, same discipline already established by panelbus_dispatch.c/
 * wire_dispatch.c/heap_alloc.c in this project:
 *  - Every function below either directly manipulates dev+0x412 (TXCSR),
 *    dev+0x416 (RXCSR), dev+0x40e (INDEX), or dev+0x502 (fixed EP0 CSR0) -
 *    the EXACT SAME offsets omap_l137_usbdc.c's omap_usbdc_init_ep0/
 *    omap_usbdc_poll_transfer and omap_l137_usbdc_ext.c's Section 1 helpers
 *    already established, via the SAME global dev-object pointer (see
 *    usbdc_ep0_dev_handle below - confirmed by direct value comparison in
 *    all_data.json, not assumed from proximity: DAT_c000975c/c0009828/
 *    c000988c/c00098dc/c000992c/c0009a88/c0009af8/c0009dc4/c0009f20/
 *    c0009f80/c0009fe4/c000a0e4/c000a160/c000a260/c000a33c/c000a3a8/
 *    c000a3bc/c000a428/c000a458 ALL resolve to the identical raw pointer
 *    value 0xc01cce50).
 *  - omap_l137_usbdc_ext.c's own Section 6 (usbdc_core_isr) and Section 7
 *    (usbdc_endpoint_event_dispatch) explicitly forward-declare 7 of this
 *    file's functions as externs, calling them "out of range" / "endpoint-
 *    handler cluster" - usbdc_ep0_class5_handler, usbdc_ep0_class9_handler,
 *    usbdc_ep0_state3_handler, usbdc_ep0_state4_handler,
 *    usbdc_ep_state1_handler, usbdc_ep_state2_handler, usbdc_ep_state3_query.
 *    This file supplies their real bodies, matching those files' own
 *    chosen names and signatures exactly.
 *
 * GENUINE NEW FINDING this pass (resolves an open question in
 * omap_l137_usbdc_ext.c's own Section 1 header comment, NOT edited there):
 * that file's own note says CSR0 lives at the FIXED dev+0x502 offset,
 * "NOT the indexed TXCSR at +0x412". This cluster's own EP0 data-stage
 * pump functions (usbdc_ep0_state3_handler/usbdc_ep0_state4_handler below)
 * prove BOTH are real and in active use: they OR bit pattern 0x0A
 * (TxPktRdy|DataEnd) and 0x48 (ServicedRxPktRdy|DataEnd) into dev+0x412
 * while EP0 is selected via INDEX - exact matches for real MUSB-derived
 * USB0 core CSR0 bit positions (bit1=TxPktRdy, bit3=DataEnd, bit6=
 * ServicedRxPktRdy) per the public TI DA8xx/OMAP-L138 TRM. This is
 * independent, concrete confirmation that dev+0x412 (the INDEXED TXCSR
 * shadow, when INDEX=0) is functionally CSR0-equivalent, coexisting with
 * the FIXED dev+0x502 CSR0 window omap_l137_usbdc_ext.c's own helpers use -
 * both are genuine, real hardware aliases of the same physical register on
 * MUSB-derived cores, not a transcription conflict between the two files.
 *
 * A second real object, distinct from `dev`, is threaded through most of
 * this file: an EP0 "ctx" (request/SETUP context) object - the exact same
 * `ep0_ctx`/`ctx` parameter omap_l137_usbdc_ext.c's usbdc_core_isr and
 * usbdc_endpoint_event_dispatch already pass around opaquely. Its own
 * global handle (DAT_c0009098, confirmed via all_data.json to resolve to
 * 0xc01cacc0 - a DIFFERENT literal value from the dev handle above, so
 * genuinely a separate object, not another dev-handle alias) is declared
 * below as usbdc_ep0_ctx_handle. Fields on it resolved this pass:
 *   ctx+0x04  a "queued/armed" flag (distinct from dev+4's own hardware-
 *             ready flag) gating the usbdc_epN_send_* family below.
 *   ctx+0x07  the committed SET_CONFIGURATION value (0 or 1) - written by
 *             usbdc_setup_stage_configuration, read back by
 *             usbdc_ep0_ctx_get_config and by usbdc_get_configuration.
 *   ctx+0x08/+0x0c  data pointer / remaining-length pair for the EP0 IN
 *             (TX) data stage - written by usbdc_ep0_start_tx, consumed by
 *             usbdc_ep0_state3_handler.
 *   ctx+0x50..0x57  the raw 8-byte USB SETUP packet, exactly as
 *             omap_l137_usbdc_ext.c's own usbdc_endpoint_event_dispatch
 *             case 0 already documents copying via usbdc_fifo_read(dev, 0,
 *             ctx+0x50, 8) - bmRequestType/bRequest/wValue/wIndex/wLength
 *             in standard USB byte order, confirmed by every handler below
 *             that reads it.
 *   ctx+0x58  a staged SET_ADDRESS value, committed to the real FADDR
 *             hardware register (dev+0x400) by usbdc_ep0_class5_handler.
 *   ctx+0x5a/+0x5c  a second data pointer / remaining-length pair, this one
 *             for the EP0 OUT (RX) data stage.
 *   ctx+0x5e/+0x5f  a small scratch reply buffer (1-2 bytes) staged by
 *             several GET_* handlers before calling usbdc_ep0_start_tx.
 *
 * Standard USB chapter-9 bRequest values are CONFIRMED for the 4 handlers
 * usbdc_ep_state2_handler switches on directly (0=GET_STATUS, 6=
 * GET_DESCRIPTOR, 8=GET_CONFIGURATION, 0x0A=GET_INTERFACE - literal
 * bRequest byte matches, not guessed) and inferred with high confidence for
 * usbdc_setup_stage_address (SET_ADDRESS, wValue 7-bit range 0-127) and
 * usbdc_setup_stage_configuration (SET_CONFIGURATION, wValue 0-1) by their
 * own field ranges and the SET_ADDRESS/FADDR commit chain traced through
 * usbdc_ep0_class5_handler. usbdc_ep0_diag_request's own wValue-high-byte/
 * bRequest sub-dispatch (0x81-0x84) does NOT match any standard chapter-9
 * shape - almost certainly one of this firmware's own vendor/diagnostic
 * extensions (same pattern as wire_dispatch.c's/panelbus_dispatch.c's own
 * custom opcode ranges layered outside standard protocol), left
 * un-renamed to a specific USB request rather than guessed.
 *
 * Every "reject/stall" path in this cluster calls usbdc_ep0_csr0_set_bits
 * (FUN_c0004a34, already reconstructed in omap_l137_usbdc_ext.c - ORs bit
 * 0x20 into CSR0/dev+0x502, the real MUSB SendStall bit) - NOT the
 * crypto_at88_fault-style hard-halt assert. Several call sites pass extra
 * visible arguments beyond that function's real 1-parameter signature
 * (e.g. usbdc_get_configuration's `FUN_c0004a34(*DAT_c0009f80,param_1,
 * param_2)`) - the same "phantom-forwarded-argument" class of Ghidra
 * signature-analysis gap this project has repeatedly documented elsewhere
 * (cdix4192.c, eva_board_main.c, omap_l137_usbdc.c's own FUN_c000acc8);
 * the real callee still only reads dev, so this is harmless, not a
 * transcription error.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---- cross-file dependencies (declared here, defined elsewhere; none of
 * the following files are edited by this one) ---------------------------- */
extern void    usbdc_select_endpoint(void *dev, uint8_t ep);			/* omap_l137_usbdc_ext.c, FUN_c0003e10 */
extern void    usbdc_ep0_csr0_set_bits(void *dev);				/* omap_l137_usbdc_ext.c, FUN_c0004a34 - stall EP0 */
extern uint16_t usbdc_ep0_csr0_test_setupend(void *dev);			/* omap_l137_usbdc_ext.c, FUN_c0004a4c */
extern void    usbdc_txcsr_direct_set_txpktrdy(void *dev, uint8_t ep);	/* omap_l137_usbdc_ext.c, FUN_c00049f0 */
extern void    usbdc_rxcsr_direct_set_bits(void *dev, uint8_t ep);		/* omap_l137_usbdc_ext.c, FUN_c0004a10 */
extern void    usbdc_txcsr_direct_clear_bits(void *dev, uint8_t ep);		/* omap_l137_usbdc_ext.c, FUN_c0004a64 */
extern void    usbdc_rxcsr_direct_clear_bits(void *dev, uint8_t ep);		/* omap_l137_usbdc_ext.c, FUN_c0004a8c */
extern uint16_t usbdc_txcsr_direct_test_bit5(void *dev, uint8_t ep);		/* omap_l137_usbdc_ext.c, FUN_c0004ab8 */
extern uint16_t usbdc_rxcsr_direct_test_bit6(void *dev, uint8_t ep);		/* omap_l137_usbdc_ext.c, FUN_c0004ad8 */
extern bool    usbdc_txcsr_direct_test_ready(void *dev, uint8_t ep);		/* omap_l137_usbdc_ext.c, FUN_c0004b1c */
extern uint32_t usbdc_raw_read32(void *dev, int offset);			/* omap_l137_usbdc_ext.c, FUN_c0004b4c */
extern void    usbdc_raw_write32(void *dev, int offset, uint32_t value);	/* omap_l137_usbdc_ext.c, FUN_c0004b54 */
extern uint16_t usbdc_raw_read16(void *dev, int offset);			/* omap_l137_usbdc_ext.c, FUN_c0004b5c */
extern void    usbdc_raw_write16(void *dev, int offset, uint16_t value);	/* omap_l137_usbdc_ext.c, FUN_c0004b64 */
extern void    usbdc_fifo_write(void *dev, int ep, const uint8_t *src, int len);	/* omap_l137_usbdc_ext.c, FUN_c00047d4 */
extern void    usbdc_fifo_read(void *dev, int ep, uint32_t *dst, unsigned len);	/* omap_l137_usbdc_ext.c, FUN_c0004858 */
extern void    usbdc_flush_ep1_4(void *dev);					/* omap_l137_usbdc_ext.c, FUN_c00045b4 */
extern uint32_t omap_usbdc_reloc(uint32_t offset);				/* omap_l137_usbdc.c, FUN_c0009194 - generic reloc-base deref */
extern uint8_t  panelbus_table_byte(uint32_t base, int index);		/* panelbus_dispatch.c, FUN_c0009204 - generic table byte read */
extern uint32_t usbdc_min_u32(uint32_t a, uint32_t b);				/* FUN_c000aee8, out of this project's range entirely - clamp helper */

/* The single global default USB dev-object pointer - see file header for
 * the DAT_ address list this resolves from (all independently confirmed
 * identical, 0xc01cce50). Declared once here; every function below that
 * "loads its own DAT_" in the raw decompile is really just reading this
 * same global through a differently-named symbol. */
extern void *usbdc_ep0_dev_handle;	/* DAT_c000975c and 18 other aliases, == 0xc01cce50 */

/* The EP0 request-context object - see file header for its field layout.
 * DAT_c0009098, confirmed to resolve to a DIFFERENT literal (0xc01cacc0)
 * from usbdc_ep0_dev_handle above, i.e. genuinely a separate object. */
extern void *usbdc_ep0_ctx_handle;	/* DAT_c0009098 */

/* ===========================================================================
 * cad_pedal_present - NOT part of the USB EP0 cluster above; happens to sit
 * immediately after eva_board_main.c's own reset-vector/literal-pool section
 * (0xc00094d8, between eva_board_reset_handler at 0xc0009534's PRECEDING
 * code and this file's own USB cluster). Genuinely uncovered: cad.c and
 * omap_l108.c BOTH already forward-declare this exact symbol as
 * `extern int cad_pedal_present(void *probe_handle);` (comment: FUN_c00094d8)
 * for their own use, but neither file defines its body - this is that
 * definition. CROSS-FILE FOLLOW-UP: the natural home for this function is
 * cad.c (or eva_board_main.c, whose own board-capability-table convention
 * it reuses), not this USB-focused file; kept here only because its address
 * falls inside this pass's assigned sweep range and it was otherwise an
 * orphaned extern with no real implementation anywhere in the project.
 *
 * Body: reads board-capability bit 0x17 (23) from a table at
 * reloc_base+0x1ac00 (reloc_base = omap_usbdc_reloc(DAT_c000952c), the same
 * generic relocation-base-dereference helper cpsoc.c/eva_board_main.c/
 * mcasp.c already independently attribute to FUN_c0009194 - genuinely
 * firmware-wide, not USB-specific despite the historical name); if set,
 * ALSO checks bit 0x25 (37) of the same table and returns whether THAT bit
 * is set too. Two-bit AND-gated presence check, matching cad.c's own
 * documented "5 expansion-pedal presence probes" pattern (each probe
 * checking two related capability bits for one pedal jack). @0xc00094d8.
 * -------------------------------------------------------------------------
 * Its own `probe_handle` parameter is entirely unused - confirmed dead,
 * same "phantom forwarded parameter" pattern already catalogued elsewhere
 * in this project (cdix4192.c, eva_board_main.c, heap_alloc.c).
 * =========================================================================== */
extern uint32_t usbdc_board_cap_reloc_base;	/* DAT_c000952c, resolves to 0xc01ccb10 */

int cad_pedal_present(void *probe_handle)	/* FUN_c00094d8 */
{
	(void)probe_handle;	/* real parameter, never read - see note above */
	uint32_t base = omap_usbdc_reloc(usbdc_board_cap_reloc_base);

	if (panelbus_table_byte(base + 0x1ac00, 0x17) == 0)
		return 0;

	base = omap_usbdc_reloc(usbdc_board_cap_reloc_base);
	return panelbus_table_byte(base + 0x1ac00, 0x25) != 0;
}

/* ===========================================================================
 * usbdc_ep0_ctx_get_config - trivial 2-instruction accessor
 * ("ldrb r0,[r0,#7]; mov pc,lr"), already identified as raw disassembly in
 * eva_board_main.c's own reset-vector section comment but never given a
 * real C definition anywhere until this pass. Returns ctx+7, the committed
 * SET_CONFIGURATION value written by usbdc_setup_stage_configuration below.
 * Sole caller: master_dispatch_tick (wire_dispatch.c, FUN_c0008b64) -
 * confirmed by that function's own decompile: `cVar5 =
 * FUN_c0009548(DAT_c0009098)` (DAT_c0009098 == usbdc_ep0_ctx_handle, see
 * above) gates a one-time EP1/EP3/EP4 IRQ-enable-and-ZLP-send sequence the
 * very first tick after the host issues SET_CONFIGURATION(1). @0xc0009548.
 * =========================================================================== */
uint8_t usbdc_ep0_ctx_get_config(void *ctx)	/* FUN_c0009548 */
{
	return *((uint8_t *)ctx + 7);
}

/* ===========================================================================
 * usbdc_ep0_state3_handler - the EP0 IN (TX) data-stage pump. Sends up to
 * 0x40 (64, the real EP0 max-packet-size already established in
 * omap_l137_usbdc.c's own omap_usbdc_init_ep0) bytes per call from
 * ctx+8/ctx+0xc (data pointer / remaining length, set up by
 * usbdc_ep0_start_tx below), then ORs the matching MUSB CSR0 bit pattern
 * into dev+0x412 (INDEXED-window CSR0 alias when EP0 is selected - see
 * file header for why this is confirmed CSR0-equivalent, not TXCSR):
 *   0x0A (TxPktRdy|DataEnd)      - last packet of this transfer
 *   0x02 (TxPktRdy only)         - more data still queued
 * Also updates a secondary status-nibble global (usbdc_ep0_status2 below)
 * with a matching done(0x60)/in-progress(3) encoding.
 * No usbdc_select_endpoint call inside this function itself - relies on
 * INDEX already being 0 from the caller context (usbdc_core_isr's EP0
 * branch in omap_l137_usbdc_ext.c already selects EP0 before reaching any
 * of this cluster). @0xc0009768.
 * =========================================================================== */
extern int32_t usbdc_ep0_status2;	/* DAT_c0009830/c0009dcc/c000a3a8-family secondary status nibble, same global throughout this cluster */

void usbdc_ep0_state3_handler(void *ctx)	/* FUN_c0009768 */
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
		usbdc_raw_write16(dev, 0x412, csr0 | 0x0A);	/* TxPktRdy | DataEnd */
		usbdc_ep0_status2 = (usbdc_ep0_status2 & 0xfffffff0) | 0x60;
	} else {
		usbdc_raw_write16(dev, 0x412, csr0 | 0x02);	/* TxPktRdy only */
		usbdc_ep0_status2 = (usbdc_ep0_status2 & 0xfffffff3) | 3;
	}
}

/* usbdc_ep0_start_tx - stages a (data, len) pair into ctx+8/ctx+0xc, then
 * immediately kicks the pump above. Real call site
 * (`FUN_c0009768();` with zero visible args) is a phantom-forward of
 * `ctx`, same class of issue documented throughout this cluster - every
 * other call site in this file passes it explicitly. @0xc0009834. */
void usbdc_ep0_start_tx(void *ctx, void *data, uint32_t len)	/* FUN_c0009834 */
{
	uint8_t *c = (uint8_t *)ctx;
	*(void **)(c + 8) = data;
	*(uint32_t *)(c + 0xc) = len;
	usbdc_ep0_state3_handler(ctx);
}

/* ===========================================================================
 * usbdc_ep_flush_or_dataend - selects endpoint `ep` via INDEX, then:
 *   ep == 0     : ORs bit 0x100 into dev+0x412 (CSR0-when-INDEX-0 again -
 *                 a bit outside the 0x0A/0x48 pair usbdc_ep0_state3/4
 *                 already established; real meaning not independently
 *                 decoded, flagged rather than guessed).
 *   ep != 0, tx : double-OR bit 0x08 into TXCSR (dev+0x412) - the EXACT
 *                 same "FlushFIFO written twice" idiom
 *                 omap_l137_usbdc_ext.c's own usbdc_flush_endpoint_fifos
 *                 already documents as a real MUSB double-packet-buffer
 *                 hardware requirement.
 *   ep != 0, !tx: double-OR bit 0x10 into RXCSR (dev+0x416) - same idiom,
 *                 RX side.
 * A real, address-confirmed structural sibling of that other file's own
 * flush helper - NOT edited there since this function's own ep0 case
 * (bit 0x100) has no equivalent in that helper, so it isn't a pure
 * duplicate. Called 3 times from FUN_c000a45c (outside this file's own
 * range - the next function after this sweep, not reconstructed).
 * @0xc0009698.
 * =========================================================================== */
void usbdc_ep_flush_or_dataend(uint8_t ep, bool tx)	/* FUN_c0009698 */
{
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
 * usbdc_epN_send_* family - four near-identical wrappers: gate on ctx+4
 * (a "queued/armed" flag distinct from dev+4's own hardware-ready flag -
 * see file header), then select a FIXED endpoint and forward (src, len)
 * straight into usbdc_fifo_write. `ctx` is passed in but the real dev
 * pointer always comes from this cluster's shared global
 * (usbdc_ep0_dev_handle), not from any argument - only ctx+4's flag byte
 * is actually read from the caller-supplied handle.
 *
 * usbdc_ep3_send_a (0xc0009840) has ZERO static callers anywhere in the
 * full 691-function xref data - a genuine, confirmed "unreached in this
 * static image" finding, consistent with this project's own established
 * style of reporting such cases honestly rather than guessing a caller.
 * usbdc_ep3_send_b (0xc00098e0) targets the exact same endpoint (3) via a
 * DIFFERENT DAT_ dev-handle alias and is called from master_dispatch_tick -
 * real duplication in the compiled firmware, not a transcription artifact;
 * why two near-identical EP3 senders exist is not resolved this pass.
 * =========================================================================== */
void usbdc_ep3_send_a(void *ctx, const uint8_t *src, int len)	/* FUN_c0009840, zero callers */
{
	void *dev = usbdc_ep0_dev_handle;
	usbdc_select_endpoint(dev, 3);
	if (*((uint8_t *)ctx + 4) == 0)
		return;
	usbdc_fifo_write(dev, 3, src, len);
}

void usbdc_ep1_send(void *ctx, const uint8_t *src, int len)	/* FUN_c0009890 */
{
	void *dev = usbdc_ep0_dev_handle;
	usbdc_select_endpoint(dev, 1);
	if (*((uint8_t *)ctx + 4) == 0)
		return;
	usbdc_fifo_write(dev, 1, src, len);
}

void usbdc_ep3_send_b(void *ctx, const uint8_t *src, int len)	/* FUN_c00098e0 */
{
	void *dev = usbdc_ep0_dev_handle;
	usbdc_select_endpoint(dev, 3);
	if (*((uint8_t *)ctx + 4) == 0)
		return;
	usbdc_fifo_write(dev, 3, src, len);
}

void usbdc_ep4_send(void *ctx, const uint8_t *src, int len)	/* FUN_c0009930 */
{
	void *dev = usbdc_ep0_dev_handle;
	usbdc_select_endpoint(dev, 4);
	if (*((uint8_t *)ctx + 4) == 0)
		return;
	usbdc_fifo_write(dev, 4, src, len);
}

/* ===========================================================================
 * usbdc_ep0_state4_handler - the EP0 OUT (RX) data-stage pump, the mirror
 * of usbdc_ep0_state3_handler above. Selects EP0, and if TXCSR/CSR0
 * (dev+0x412) bit 0 is set, reads the pending RXCOUNT (dev+0x418 - the
 * SAME register offset usbdc_ep_recv_bulk in omap_l137_usbdc_ext.c already
 * uses as its own bulk-OUT byte count), clamps to 0x40 (EP0 max-packet),
 * drains that many bytes via usbdc_fifo_read into ctx+0x10, decrements the
 * OUT-stage remaining-length counter at ctx+0x5c, then:
 *   remaining == 0 : ORs 0x48 (ServicedRxPktRdy|DataEnd) into dev+0x412 -
 *                    exact MUSB CSR0 bits for "last OUT packet received,
 *                    end the data stage" (bit3=DataEnd, bit6=
 *                    ServicedRxPktRdy per the public TI TRM - independent
 *                    confirmation, same class of evidence as
 *                    usbdc_ep0_state3_handler's 0x0A above).
 *   otherwise      : ORs 0x40 (ServicedRxPktRdy only) - acknowledge without
 *                    ending the stage.
 * Finally re-reads RXCOUNT; if the stage is nominally complete but more
 * bytes are STILL pending (a host that sent more than wLength promised),
 * stalls EP0 via usbdc_ep0_csr0_set_bits. @0xc0009980.
 * =========================================================================== */
void usbdc_ep0_state4_handler(void *ctx)	/* FUN_c0009980 */
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
		usbdc_raw_write16(dev, 0x412, csr0 | 0x48);	/* ServicedRxPktRdy | DataEnd */
	else
		usbdc_raw_write16(dev, 0x412, csr0 | 0x40);	/* ServicedRxPktRdy only */

	int16_t recheck = (int16_t)usbdc_raw_read16(dev, 0x418);
	if (remaining == 0 && recheck != 0)
		usbdc_ep0_csr0_set_bits(dev);	/* more data than expected - stall */
}

/* ===========================================================================
 * usbdc_ep_irq_enable_set30 / usbdc_ep_irq_enable_set34 - arm the
 * per-endpoint interrupt-enable bit in dev+0x2c (the SAME "enable_mask"
 * word usbdc_core_isr in omap_l137_usbdc_ext.c reads every ISR call:
 * `enable_mask = *(uint32_t *)(d + 0x2c);`). Bit base 0x200 (dir==0,
 * "TX-ready" family) or 2 (dir!=0, "RX-ready" family), left-shifted by
 * (ep-1) for ep>1 - matches usbdc_core_isr's OWN per-endpoint event bit
 * layout exactly: RX-ready bits 0x2/0x4/0x8/0x10 for EP1-4 and TX-ready-
 * adjacent bits 0x200/../0x1000 for EP1/EP3/EP4 are the literal bits that
 * function's own masked-status switch tests - a real, address-confirmed
 * correlation (not assumed from naming alone).
 *
 * The two functions are otherwise near-duplicates: SET30 writes the
 * computed bit through dev+0x30 (skips the write if the bit is already
 * clear... no - guards on `(mask & bit) == 0`, i.e. writes only if NOT
 * already enabled) using the direct dev-handle global; SET34 reaches the
 * SAME dev object through the `omap_usbdc_phantom_const_a`-style fixed-
 * constant indirection (FUN_c0001a80, already established in
 * omap_l137_usbdc.c as "always returns 0x1e00000") and writes through
 * dev+0x34 instead, guarding on `(mask & bit) == bit` (opposite polarity -
 * "write only if not YET fully set", functionally similar intent). Two
 * genuinely different code paths reaching the same runtime dev pointer and
 * the same conceptual "enable this IRQ bit" operation via two different
 * hardware "set" registers (0x30 vs 0x34) - real, not reconciled to one
 * canonical form since the raw decompile keeps them as two very literally
 * different functions.
 *
 * Both called from master_dispatch_tick's one-time post-SET_CONFIGURATION
 * init block (see usbdc_ep0_ctx_get_config's own note above) and from
 * several endpoint-event handlers outside this file's own range.
 * @0xc0009a98 / @0xc0009afc.
 * =========================================================================== */
extern uint32_t omap_usbdc_phantom_const_a(uint32_t unused);	/* omap_l137_usbdc.c, FUN_c0001a80, always returns 0x1e00000 */

void usbdc_ep_irq_enable_set30(void *ctx_unused, uint8_t ep, uint8_t dir)	/* FUN_c0009a98 */
{
	(void)ctx_unused;	/* real argument, but the real dev pointer always comes from the global below */
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

void usbdc_ep_irq_enable_set34(void *ctx_unused, uint8_t ep, uint8_t dir)	/* FUN_c0009afc */
{
	(void)ctx_unused;
	if (ep == 0)
		return;

	uint32_t bit = (dir == 0) ? 0x200 : 2;
	void *dev = (void *)(uintptr_t)omap_usbdc_phantom_const_a(0);	/* always 0x1e00000, same runtime dev object */
	uint32_t mask = usbdc_raw_read32(dev, 0x2c);
	if (ep > 1)
		bit <<= (ep - 1);

	if ((mask & bit) == bit) {
		dev = (void *)(uintptr_t)omap_usbdc_phantom_const_a(0);
		usbdc_raw_write32(dev, 0x34, bit);
	}
}

/* ===========================================================================
 * usbdc_ep_state3_query - tests bit 4 (0x10) of dev+0x401, the SAME shared
 * status/flags byte omap_l137_usbdc.c's omap_usbdc_init_ep0 (sets bits
 * 0x21) and omap_usbdc_poll_transfer (sets bit 0x40) already use, reached
 * here via the FUN_c0001a80 fixed-constant indirection (always 0x1e00000 -
 * same runtime object as usbdc_ep0_dev_handle, confirmed by that function's
 * own documented behavior in omap_l137_usbdc.c). Used by
 * usbdc_get_descriptor below to pick between two alternate descriptor sets
 * (plausibly a high-speed/full-speed selector - see that function's own
 * note) and, per omap_l137_usbdc_ext.c's Section 7, by
 * usbdc_endpoint_event_dispatch's case 0xb to choose an event code
 * (0x40 vs 8). @0xc0009b68.
 * =========================================================================== */
int usbdc_ep_state3_query(void)	/* FUN_c0009b68 */
{
	void *dev = (void *)(uintptr_t)omap_usbdc_phantom_const_a(0);
	return (*((uint8_t *)dev + 0x401) >> 4) & 1;
}

/* ===========================================================================
 * usbdc_epN_tx_ready_query - thin one-line wrapper over
 * usbdc_txcsr_direct_test_ready (omap_l137_usbdc_ext.c), forwarding the
 * fixed dev handle and the caller-supplied endpoint number. Called from
 * FUN_c0006858 and FUN_c000a45c (both outside this file's own range).
 * @0xc000a13c.
 * =========================================================================== */
bool usbdc_epN_tx_ready_query(uint8_t ep)	/* FUN_c000a13c */
{
	return usbdc_txcsr_direct_test_ready(usbdc_ep0_dev_handle, ep);
}

/* ===========================================================================
 * usbdc_setup_stage_address - SET_ADDRESS staging handler. Recipient check
 * (ctx+0x50 & 0x1f == 0, i.e. DEVICE) plus a signed-byte range check on
 * wValue's low byte (ctx+0x52, "-1 < value" == value in 0..127) matches the
 * real USB SET_ADDRESS request exactly (7-bit device address). Stores the
 * requested address into ctx+0x58 WITHOUT touching hardware yet - real USB
 * semantics require the new address take effect only after the STATUS
 * stage completes; usbdc_ep0_class5_handler below is the real commit point,
 * writing this staged byte into dev+0x400 (the FADDR register). Called
 * from usbdc_ep0_setup_dispatch (FUN_c000a684, outside this file's own
 * range - the case-0 standard-request sub-handler that
 * omap_l137_usbdc_ext.c's own Section 7 already forward-declares).
 * @0xc0009c1c.
 * =========================================================================== */
bool usbdc_setup_stage_address(void *ctx)	/* FUN_c0009c1c */
{
	uint8_t *c = (uint8_t *)ctx;

	if ((c[0x50] & 0x1f) != 0 || (int8_t)c[0x52] < 0)
		return false;

	c[0x58] = c[0x52];
	return true;
}

/* usbdc_setup_stage_configuration - sibling of the above: same DEVICE
 * recipient check, wValue low byte restricted to 0/1 (this device only
 * implements one real configuration). Stores directly into ctx+7 - the
 * SAME field usbdc_ep0_ctx_get_config reads back, and
 * usbdc_get_configuration echoes verbatim on a later GET_CONFIGURATION.
 * @0xc0009c50. */
bool usbdc_setup_stage_configuration(void *ctx)	/* FUN_c0009c50 */
{
	uint8_t *c = (uint8_t *)ctx;

	if ((c[0x50] & 0x1f) != 0 || c[0x52] >= 2)
		return false;

	c[7] = c[0x52];
	return true;
}

/* ===========================================================================
 * usbdc_ep0_diag_request - a two-level dispatch on the raw SETUP packet
 * (`scratch`, == ctx+0x50) that does NOT match any standard USB chapter-9
 * request shape (see file header) - almost certainly a firmware-private
 * vendor/diagnostic extension:
 *   wValue high byte == 1 : if wLength == 1, stage+send a single zero byte.
 *   wValue high byte == 2 : sub-dispatch on the RAW bRequest byte itself
 *     (0x81/0x82 - stage a fixed 2-byte reply, 0xf8 or 0x80; 0x83/0x84 -
 *     stage a fixed 2-byte reply, {0,0} or {0x40,0}); any other bRequest
 *     falls through unhandled.
 *   anything else          : unhandled (falls through to caller's own
 *     default/stall path, not reproduced here).
 * Returns 0 (handled) or 1 (not handled) to its caller
 * (usbdc_ep0_setup_dispatch, FUN_c000a684, outside this file's range).
 * @0xc0009c80.
 * =========================================================================== */
bool usbdc_ep0_diag_request(void *ctx, uint8_t *scratch)	/* FUN_c0009c80 */
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
 * usbdc_ep0_ctx_reset - resets three ctx fields (+0x7c/+0x7e/+0x80 - the
 * SAME byte offsets omap_l137_usbdc.c's omap_usbdc_object_init zeroes on
 * ITS OWN `obj` parameter; whether these are genuinely the same struct
 * family or coincidentally-matching offsets on two different objects is
 * NOT resolved this pass, flagged as an open cross-file correlation), then
 * selects EP0 and ORs bit 0x48 (ServicedRxPktRdy|DataEnd - the SAME "force
 * end of OUT data stage" pattern usbdc_ep0_state4_handler's own completion
 * branch above uses) into dev+0x412, and sets the shared secondary-status
 * global's bits 0x60 (matching usbdc_ep0_state3_handler's own "done"
 * encoding). Reads as an EP0 transfer abort/force-reset primitive.
 *
 * ZERO static callers found anywhere in the full 691-function xref data -
 * a genuine, confirmed "unreached in this static image" finding, same
 * honest-reporting convention as usbdc_ep3_send_a above and this project's
 * other documented zero-caller functions (crypto_at88_self_test,
 * cdix_configure_and_verify, cobjectmgr_object_destroy). @0xc0009d60.
 * =========================================================================== */
void usbdc_ep0_ctx_reset(void *ctx)	/* FUN_c0009d60, zero callers */
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
 * usbdc_ep_state1_handler - validates a pending CLASS request (bmRequestType
 * type bits == 0x20) whose bRequest is in the narrow range 1..5 (5 known
 * sub-commands), stages the raw wLength (scratch+6) into ctx+0x5a AND
 * ctx+0x5c (the OUT-stage remaining-length counter usbdc_ep0_state4_handler
 * consumes), then unconditionally advances the OUT-stage pump
 * (usbdc_ep0_state4_handler - real call site has zero visible arguments in
 * the raw decompile, a phantom-forward of `ctx`, same class of issue
 * documented throughout this cluster). Returns whether the request was
 * recognized. @0xc0009dd0.
 * =========================================================================== */
uint8_t usbdc_ep_state1_handler(void *ctx, void *scratch)	/* FUN_c0009dd0 */
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
 * usbdc_get_descriptor - GET_DESCRIPTOR (bRequest==6, confirmed literal
 * match via usbdc_ep_state2_handler's own case-6 dispatch below). Switches
 * on the descriptor-type byte (scratch+3, the high byte of wValue):
 *   1 (DEVICE)              : usbdc_ep_state3_query()-selected device
 *                              descriptor pointer (of 2 alternates - see
 *                              below).
 *   2 (CONFIGURATION)        : indexed by ctx's own leading dword
 *                              (*ctx, presumably a config/interface index)
 *                              into one of 2 pointer tables, ALSO selected
 *                              by usbdc_ep_state3_query() - device/
 *                              high-speed vs configuration/full-speed
 *                              descriptor pointer, a real, structurally-
 *                              confirmed dual-speed USB2.0 pattern (see
 *                              below).
 *   3 (STRING)                : indexed (bounded < 9) by wValue's low byte
 *                              (scratch+2) into a 9-entry string table.
 *   6 (DEVICE_QUALIFIER)      : fixed pointer - the OTHER hallmark of a
 *                              genuine dual-speed USB2.0 descriptor set
 *                              (device_qualifier + other_speed_config are
 *                              only meaningful together).
 *   7 (OTHER_SPEED_CONFIG)    : same *ctx-indexed table access as case 2.
 *   default                   : falls through to the reject tail below.
 * Common tail: reads the descriptor blob's own first byte as its length
 * (this firmware's own length-prefixed descriptor-blob convention - NOT a
 * standard multi-field bLength/wTotalLength read, transcribed as found);
 * if nonzero, stages min(wLength, blob_length) bytes via
 * usbdc_ep0_start_tx and returns true. If the resolved descriptor pointer
 * is NULL (no case matched) OR the blob's length byte is 0, stalls EP0 via
 * usbdc_ep0_csr0_set_bits and returns false. @0xc0009e1c.
 *
 * The two DEVICE_DESCRIPTOR-shaped table pairs used by cases 1/2/7 (see
 * DAT_c0009f04/f08 for case 1, DAT_c0009f0c/f10 for case 2's table-of-4,
 * DAT_c0009f14 for case 3's string table, DAT_c0009f18 for case 6, and
 * DAT_c0009f1c for case 7's own table-of-4) all resolve to real rodata
 * pointers in this image (confirmed present in all_data.json, values not
 * independently decoded to actual descriptor bytes this pass).
 * =========================================================================== */
extern uint8_t *usbdc_desc_device_fs, *usbdc_desc_device_hs;			/* DAT_c0009f04/f08 */
extern uint8_t **usbdc_desc_config_table_fs, **usbdc_desc_config_table_hs;	/* DAT_c0009f0c/f10 */
extern uint8_t **usbdc_desc_string_table;					/* DAT_c0009f14, 9 entries */
extern uint8_t *usbdc_desc_qualifier;						/* DAT_c0009f18 */
extern uint8_t **usbdc_desc_other_speed_table;					/* DAT_c0009f1c */

bool usbdc_get_descriptor(void *ctx, uint8_t *scratch)	/* FUN_c0009e1c */
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

/* ===========================================================================
 * usbdc_get_configuration - GET_CONFIGURATION (bRequest==8, confirmed
 * literal match). DEVICE recipient + wLength==1 required; echoes the
 * committed configuration byte (ctx+7 - the SAME field
 * usbdc_setup_stage_configuration writes) back to the host via
 * usbdc_ep0_start_tx. Otherwise stalls EP0. @0xc0009f24.
 * =========================================================================== */
bool usbdc_get_configuration(void *ctx, uint8_t *scratch)	/* FUN_c0009f24 */
{
	uint8_t *c = (uint8_t *)ctx;

	if ((scratch[0] & 0x1f) == 0 && *(uint16_t *)(scratch + 6) == 1) {
		c[0x5e] = c[7];
		usbdc_ep0_start_tx(ctx, c + 0x5e, 1);
		return true;
	}

	usbdc_ep0_csr0_set_bits(usbdc_ep0_dev_handle);
	return false;
}

/* ===========================================================================
 * usbdc_get_interface - GET_INTERFACE (bRequest==0x0A, confirmed literal
 * match). INTERFACE recipient + wLength==1 required; always echoes
 * alternate-setting 0 (this firmware implements no alternate interface
 * settings) rather than reading back any per-interface state. Otherwise
 * stalls EP0. @0xc0009f84.
 * =========================================================================== */
bool usbdc_get_interface(void *ctx, uint8_t *scratch)	/* FUN_c0009f84 */
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
 * usbdc_ep_flush_direction - unconditional (no RXPKTRDY/TXCSR-bit gating,
 * unlike omap_l137_usbdc_ext.c's own usbdc_flush_endpoint_fifos) double-OR
 * FIFO-flush primitive, structurally matching that function's own bit
 * patterns exactly:
 *   mode==1, dir==0 : double-OR RXCSR (dev+0x416) bit 0x10, then single-OR
 *                     bit 0x80 - byte-for-byte the same RX flush sequence.
 *   mode==3, dir==1 : double-OR TXCSR (dev+0x412) bit 0x08, then single-OR
 *                     bit 0x40 - byte-for-byte the same TX flush sequence.
 * `mode` and `dir` are required to agree (function is a no-op otherwise -
 * an implicit consistency check, not independently explained). Called
 * twice from FUN_c000a45c (outside this file's own range). Real relation
 * to usbdc_flush_endpoint_fifos (same bit patterns, no RXPKTRDY/TXCSR-bit
 * gate, no endpoint-select call of its own - relies on the caller having
 * already selected the target endpoint) not resolved this pass - flagged
 * as a probable sibling/earlier-generation variant, not corrected into
 * that other file. @0xc0009fe8.
 * =========================================================================== */
void usbdc_ep_flush_direction(uint8_t mode, bool dir)	/* FUN_c0009fe8 */
{
	void *dev = usbdc_ep0_dev_handle;

	if (mode == 1 && !dir) {
		for (int i = 0; i < 2; i++) {
			uint16_t v = usbdc_raw_read16(dev, 0x416);
			usbdc_raw_write16(dev, 0x416, v | 0x10);
		}
		uint16_t v = usbdc_raw_read16(dev, 0x416);
		usbdc_raw_write16(dev, 0x416, v | 0x80);
	} else if (mode == 3 && dir) {
		for (int i = 0; i < 2; i++) {
			uint16_t v = usbdc_raw_read16(dev, 0x412);
			usbdc_raw_write16(dev, 0x412, v | 0x08);
		}
		uint16_t v = usbdc_raw_read16(dev, 0x412);
		usbdc_raw_write16(dev, 0x412, v | 0x40);
	}
}

/* ===========================================================================
 * usbdc_get_status - GET_STATUS (bRequest==0, confirmed literal match via
 * usbdc_ep_state2_handler's own case-0 dispatch below). Recipient-dependent:
 *   INTERFACE (scratch[0]&0x1f == 1) : always reports status 0.
 *   DEVICE    (recipient == 0)        : reports `1 - ctx+6` (a self-powered/
 *                                        remote-wakeup byte), clamped to 0
 *                                        if that byte is out of the 0..1
 *                                        range - i.e. bit 0 (self-powered)
 *                                        set unless ctx+6 says otherwise.
 *   ENDPOINT  (recipient == 2)        : reads the real per-endpoint STALL
 *                                        bit via the SAME CSR helpers
 *                                        omap_l137_usbdc_ext.c's own
 *                                        Section 1 already established -
 *                                        wIndex bit 0 clear selects EP0's
 *                                        own CSR0 SETUPEND-shaped test
 *                                        (usbdc_ep0_csr0_test_setupend,
 *                                        reused here rather than a
 *                                        dedicated stall-bit test - real,
 *                                        not a transcription choice),
 *                                        wIndex bit 7 selects RX- vs
 *                                        TX-direction CSR bit test
 *                                        (usbdc_rxcsr_direct_test_bit6 /
 *                                        usbdc_txcsr_direct_test_bit5).
 * wLength must be exactly 2 for any of the above to actually reply;
 * otherwise (or if the recipient matched none of the 3 cases) stalls EP0.
 * @0xc000a164.
 * =========================================================================== */
uint16_t usbdc_get_status(void *ctx, uint8_t *scratch)	/* FUN_c000a164 */
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
 * usbdc_ep_state2_handler - the confirmed standard-request dispatcher.
 * Recipient/type bits (scratch[0] & 0x60):
 *   0x00 (standard) : switches on bRequest (scratch[1]) - 0=GET_STATUS,
 *                      6=GET_DESCRIPTOR, 8=GET_CONFIGURATION,
 *                      0x0A=GET_INTERFACE (all 4 literal bRequest byte
 *                      matches, confirmed not guessed).
 *   0x20 (class)     : bRequest==0xfe (254) only - stage+send a single
 *                      zero byte.
 *   else (vendor)    : always rejected (result=0, stall - via
 *                      usbdc_ep0_csr0_set_bits, called here with its real
 *                      1-argument signature).
 * Already named/signature-matched to omap_l137_usbdc_ext.c's own extern
 * declaration (`extern uint8_t usbdc_ep_state2_handler(void *ctx, void
 * *scratch);`), its sole caller per that file's own Section 7
 * (usbdc_endpoint_event_dispatch, case 2). @0xc000a264.
 * =========================================================================== */
uint8_t usbdc_ep_state2_handler(void *ctx, void *scratch)	/* FUN_c000a264 */
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
 * usbdc_clear_feature_endpoint_halt - CLEAR_FEATURE(ENDPOINT_HALT).
 * Recipient must be ENDPOINT (scratch/ctx+0x50 & 0x1f == 2); wIndex
 * (ctx+0x54) low 7 bits select the endpoint, bit 7 the direction. EP0
 * itself (endpoint number 0) is a no-op (returns true without touching
 * hardware - EP0 cannot be halted). Otherwise clears the real STALL bit
 * via the direction-appropriate CSR helper already established in
 * omap_l137_usbdc_ext.c (usbdc_rxcsr_direct_clear_bits for OUT,
 * usbdc_txcsr_direct_clear_bits for IN). @0xc000a340.
 * =========================================================================== */
bool usbdc_clear_feature_endpoint_halt(void *ctx)	/* FUN_c000a340 */
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

/* ===========================================================================
 * usbdc_ep0_class5_handler - the real SET_ADDRESS commit point. Copies the
 * value usbdc_setup_stage_address staged at ctx+0x58 into dev+0x400 - one
 * byte before the well-established dev+0x401 status/flags byte, and
 * genuinely plausible as the real MUSB FADDR (function address) hardware
 * register given the exact SET_ADDRESS staging chain traced above. Reached
 * via omap_l137_usbdc_ext.c's own usbdc_core_isr, whose EP0 branch
 * dispatches here when the request's own "class nibble"
 * (usbdc_ep0_ctx_ptr[1] & 0xf) == 5 - a firmware-private dispatch key, NOT
 * literal USB bmRequestType class bits (recipient/type bits were already
 * consumed by the standard-request path above; this is a second, later-
 * stage dispatch on a value this firmware itself computes and stores).
 * Already named/signature-matched to that file's own extern declaration.
 * @0xc000a3ac.
 * =========================================================================== */
void usbdc_ep0_class5_handler(void *ctx)	/* FUN_c000a3ac */
{
	uint8_t *c = (uint8_t *)ctx;
	*((uint8_t *)usbdc_ep0_dev_handle + 0x400) = c[0x58];
}

/* ===========================================================================
 * usbdc_set_feature_endpoint_halt - SET_FEATURE(ENDPOINT_HALT), the mirror
 * of usbdc_clear_feature_endpoint_halt above: same recipient/wIndex
 * decode, EP0 is still exempted (returns the recipient-match boolean
 * without touching hardware for ep==0 - unlike the clear-feature sibling,
 * this path doesn't special-case ep==0 with an early return, it simply
 * never enters the `ep != 0` body), sets (rather than clears) the STALL
 * bit via usbdc_rxcsr_direct_set_bits (OUT) / usbdc_txcsr_direct_set_txpktrdy
 * (IN - reusing that TXPKTRDY-named helper for its bit-0x10 side effect,
 * same as its own real body; not a naming contradiction, see that
 * function's own definition in omap_l137_usbdc_ext.c). @0xc000a3c0.
 * =========================================================================== */
bool usbdc_set_feature_endpoint_halt(void *ctx)	/* FUN_c000a3c0 */
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
 * usbdc_ep0_class9_handler - the SET_CONFIGURATION commit handler (class
 * nibble == 9, same firmware-private dispatch key scheme as class 5
 * above). Flushes all 4 non-control endpoints' FIFOs via
 * usbdc_flush_ep1_4 (omap_l137_usbdc_ext.c - a real bus/config
 * reconfiguration action) then marks ctx+4 = 1, the SAME "queued/armed"
 * flag the usbdc_epN_send_* family above gates on - i.e. this is the exact
 * point where those senders become live. The real call site
 * (`FUN_c00045b4(*DAT_c000a458, *(param_1+7))`) passes usbdc_flush_ep1_4 a
 * second visible argument (ctx+7, the committed configuration byte) that
 * function's own real 1-parameter body never reads - another phantom-
 * forwarded argument, harmless. Already named/signature-matched to
 * omap_l137_usbdc_ext.c's own extern declaration. @0xc000a42c.
 * =========================================================================== */
void usbdc_ep0_class9_handler(void *ctx)	/* FUN_c000a42c */
{
	uint8_t *c = (uint8_t *)ctx;
	usbdc_flush_ep1_4(usbdc_ep0_dev_handle);
	c[4] = 1;
}

/* -------------------------------------------------------------------------
 * Still genuinely open, not fabricated:
 *  - usbdc_ep_flush_or_dataend's own ep==0/bit-0x100 case: real meaning of
 *    that bit not independently decoded (outside the 0x0A/0x48 pair
 *    confirmed elsewhere in this file).
 *  - usbdc_ep0_diag_request's exact wValue/bRequest binding: confirmed to
 *    NOT be a standard USB chapter-9 request, but its real firmware-level
 *    purpose (diagnostics? factory test hooks?) isn't traced past its own
 *    body.
 *  - usbdc_ep3_send_a vs usbdc_ep3_send_b: real reason two near-identical
 *    EP3 senders exist, reached via different DAT_ dev-handle aliases, one
 *    with zero static callers.
 *  - usbdc_ep0_ctx_reset's own +0x7c/+0x7e/+0x80 field correlation with
 *    omap_l137_usbdc.c's `obj` struct of the same offsets - same struct
 *    family or coincidence, and its own zero-caller status.
 *  - usbdc_get_descriptor's device/config descriptor table CONTENTS (the
 *    pointers themselves are confirmed real rodata addresses; the bytes
 *    they point to are not decoded this pass).
 *  - The exact hardware identity of dev+0x400 as FADDR (strongly
 *    consistent with the traced SET_ADDRESS staging/commit chain, not
 *    independently confirmed against a TRM register map).
 *  - usbdc_ep_irq_enable_set30/_set34's own dev+0x30 vs dev+0x34 registers'
 *    real hardware names (structurally "interrupt enable set" registers,
 *    not confirmed against a TRM).
 * ------------------------------------------------------------------------- */
