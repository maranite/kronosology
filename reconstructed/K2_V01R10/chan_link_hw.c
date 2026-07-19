/* SPDX-License-Identifier: GPL-2.0 */
/*
 * chan_link_hw.c - K2 port of K1_V06R06/chan_link_hw.c (0xc000b414-0xc000b898,
 * 17 functions there). Migrated as part of the MIDI-subsystem cluster pass,
 * 2026-07-19.
 *
 * Ground truth: static Ghidra dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json), queried via query_dump_k2.py.
 * No live Ghidra MCP calls this pass.
 *
 * LOCATION METHOD: K1's chan_link_hw.c has no `__FILE__` anchor of its own
 * (see that file's own header) - address-adjacency to K1's midi_engine.c/
 * chan_param_ctrl.c isn't available in K2 either, so this pass located the
 * whole cluster from first principles via K2's own already-committed
 * soc_irq_gate.c "CLUSTER 10" work: that file's midi_hw_write16/_read16
 * (FUN_c000099c/FUN_c00009d0) are the SAME low-level MMIO primitives every
 * function in this file calls, and their own xrefs_to lists led straight to
 * every match below. Every match was independently re-derived from K2's own
 * decompile and DAT_ literal pool - not copied from K1's own citations - and
 * cross-checked structurally (same register offsets, same masks, same
 * literal constants, same Ghidra-reported byte SIZE in every single case
 * below, a level of exactness this project has not previously seen for an
 * unanchored cluster this large).
 *
 * HEADLINE FINDING: this cluster is essentially BYTE-FOR-BYTE UNCHANGED
 * between K1 and K2 - all 17 K1 functions have a confirmed K2 counterpart,
 * every one at an IDENTICAL Ghidra-reported size, and every numeric constant
 * independently re-resolved from K2's own DAT_ pool (0x1fe0, 0xfffe, 0xffdf,
 * 0x102, 0x186a0, 0x20ff, 0x11fd, 0x116, 0xffcb) is bit-for-bit identical to
 * K1's own values. This hardware layer (an external multi-port MIDI-shaped
 * UART/transceiver block on the async EMIF bus) did not change at all
 * between the two boards - only its address range moved. Given this,
 * transcription below follows K1's own file directly, with K2 addresses/
 * struct-offset citations substituted and independently re-verified.
 *
 * K1 vs K2 function map (all confirmed via decompile + exact size match):
 *   chan_slot_scratch_alloc    K1 FUN_c000b448 (36B)  -> K2 FUN_c000c74c (36B)
 *   chan_slot_entry_init       K1 FUN_c000b414 (52B)  -> K2 FUN_c000c718 (52B)
 *   chan_slot_alloc_and_init   K1 FUN_c000b46c (108B) -> K2 FUN_c000c770 (108B)
 *   chan_bitmask_decode        K1 FUN_c000b4d8 (160B) -> K2 FUN_c000c7dc (160B)
 *   chan_bitmask_decode_u8     K1 FUN_c000b580 (8B)   -> K2 FUN_c000c884 (8B)
 *   chan_table2_decode         K1 FUN_c000b588 (160B) -> K2 FUN_c000c88c (160B)
 *   chan_decode_result_get     K1 FUN_c000b630 (20B)  -> K2 FUN_c000c934 (20B)
 *   chan_link_probe_ready      K1 FUN_c000b644 (8B)   -> K2 FUN_c000c948 (8B)
 *   chan_slot_apply_code       K1 FUN_c000b64c (28B)  -> K2 FUN_c000c950 (28B)
 *   chan_slot_echo_code        K1 FUN_c000b66c (28B)  -> K2 FUN_c000c970 (28B)
 *   chan_class2_read_value     K1 FUN_c000b68c (52B)  -> K2 FUN_c000c990 (52B)
 *   chan_link_hw_service       K1 FUN_c000b6c4 (348B) -> K2 FUN_c000c9c8 (348B)
 *   chan_link_hw_base_get      K1 FUN_c000b898 (12B)  -> K2 FUN_c000cb9c (12B)
 *   midi_hw_set_reg_60         K1 FUN_c000b840 (16B)  -> K2 FUN_c000cb44 (16B)
 *   midi_hw_set_reg_f6         K1 FUN_c000b850 (16B)  -> K2 FUN_c000cb54 (16B)
 *   midi_hw_set_reg_e8         K1 FUN_c000b860 (16B)  -> K2 FUN_c000cb64 (16B)
 *   midi_hw_set_reg_d8         K1 FUN_c000b870 (32B)  -> K2 FUN_c000cb74 (32B)
 * All 17 of K1's functions - 17/17, no gaps, no leftovers.
 *
 * REAL, CONFIRMED DIFFERENCES from K1 (not transcription artifacts):
 *  - chan_link_hw_base_get (FUN_c000cb9c) is no longer a zero-caller
 *    orphan: its sole K2 caller is FUN_c000cf20 (the K2 counterpart of K1's
 *    midi_context_hw_init/midi_subsystem_init_entry cluster, out of this
 *    file's own scope - see midi_engine.c's own K2 port for that side).
 *  - chan_link_probe_ready's (FUN_c000c948) sole K2 caller,
 *    FUN_c000edec (chan_link_watchdog_tick's real K2 counterpart, see
 *    chan_param_ctrl.c's own K2 port), is independently confirmed - the
 *    exact chan_param_ctrl.c PART A cross-file relationship K1's own file
 *    documents (CROSS-FILE FINDING #1) reproduces exactly in K2.
 *  - K2's midi_hw_port_enable equivalent (FUN_c000dd64, K2's own
 *    midi_engine.c-territory counterpart of K1 FUN_c000ca60) is 420 bytes
 *    vs K1's smaller body - NOT part of this file (chan_link_hw.c never
 *    owned that function in K1 either), noted here only because it shares
 *    this file's own register family; see midi_engine.c's own K2 port.
 *  - chan_link_hw_service's own "pending"/"timer" globals resolve to
 *    0xC01CCD18 (pending, DAT_c000cb24) and (indirectly, via
 *    aemif_cs3_base_get's own dead-argument chain) 0xC00E004C (timer
 *    context, DAT_c000cb28) - DIFFERENT literal addresses than K1's
 *    0xC01CD31C/0xC00E0068, consistent with this being a genuinely
 *    independent data segment layout, not the same fixed addresses.
 *
 * POOL LAYOUT: identical to K1 (see K1's own file header for the full
 * struct - not reproduced field-by-field here since nothing in this K2 pass
 * found evidence it changed). Pool base DAT_c000c87c == DAT_c000c92c ==
 * 0xC01CC864 (both chan_bitmask_decode/chan_table2_decode share the
 * identical single-instance pool, exactly as in K1). Bitmask table base
 * (DAT_c000c880) resolves to 0xC0027A44; chan_table2_decode's own table base
 * (DAT_c000c930) resolves to 0xC00279F0 - 0x54 bytes apart, NOT the same
 * clean-stride relationship K1's own two tables had (0x68 apart there) -
 * genuinely unresolved which table this is, same open item K1 left.
 *
 * STILL OPEN (same items K1 left open, not independently re-resolved this
 * pass): DAT_c000c880/DAT_c000c930's own table CONTENTS (both zeroed in this
 * static dump); real physical identity of the register block (EMIFA CS3
 * attribution carries over from K1/soc_irq_gate.c's own citation, not
 * independently re-verified against a datasheet this pass); the pool's own
 * base address (0xC01CC864) not cross-checked against any other consumer
 * this pass (K1 never extracted its own pool address either).
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Out-of-scope dependencies - already defined in soc_irq_gate.c (K2's own
 *  "CLUSTER 10", ported ahead of this file in an earlier pass).
 * ------------------------------------------------------------------------- */
extern void     midi_hw_write16(uint32_t base_sel, unsigned reg_off, uint16_t val);	/* FUN_c000099c, soc_irq_gate.c (K1: FUN_c0000c38) */
extern uint16_t midi_hw_read16(uint32_t base_sel, unsigned reg_off);			/* FUN_c00009d0, soc_irq_gate.c (K1: FUN_c0000c6c) */

/* ------------------------------------------------------------------------- *
 *  Shared decode-result pool primitives. Pool base DAT_c000c87c/DAT_c000c92c,
 *  both == 0xC01CC864 (single instance, same as K1's identical-pool finding).
 * ------------------------------------------------------------------------- */

/* chan_slot_scratch_alloc - FUN_c000c74c, @0xc000c74c (36 bytes, K1:
 * FUN_c000b448, 36 bytes). Byte-for-byte identical logic to K1: doles out
 * one of 2 fixed 512-byte scratch buffers at pool+0x008/+0x208, tracked by
 * a one-shot counter at pool+0x004. */
static void *chan_slot_scratch_alloc(uint32_t *pool)	/* FUN_c000c74c */
{
	uint8_t *base = (uint8_t *)pool;
	int32_t slot = *(int32_t *)(base + 4);

	if (slot >= 2)
		return (void *)0;

	*(int32_t *)(base + 4) = slot + 1;
	return base + slot * 0x200 + 8;
}

/* chan_slot_entry_init - FUN_c000c718, @0xc000c718 (52 bytes, K1:
 * FUN_c000b414, 52 bytes). Identical belt-and-suspenders bank==0/wire_code!=0
 * buf-keep gate as K1. */
static void chan_slot_entry_init(uint32_t *entry, uint32_t bank, uint32_t wire_code, void *buf)	/* FUN_c000c718 */
{
	int keep_buf = (bank <= 1) ? (int)(1 - bank) : 0;

	if (wire_code == 0)
		keep_buf = 0;
	if (keep_buf == 0)
		buf = (void *)0;

	entry[6] = (uint32_t)(uintptr_t)buf;
	entry[5] = 0;
	entry[0] = bank;
	entry[1] = wire_code;
	entry[2] = 0;
	entry[3] = 0;
	entry[4] = 0;
}

/* chan_slot_alloc_and_init - FUN_c000c770, @0xc000c770 (108 bytes, K1:
 * FUN_c000b46c, 108 bytes). Identical 5-entry cap and element addressing. */
static uint32_t *chan_slot_alloc_and_init(uint32_t *pool, uint32_t bank, uint32_t wire_code)	/* FUN_c000c770 */
{
	uint32_t *entry = (uint32_t *)0;

	if (pool[0] < 5) {
		void *buf;

		entry = pool + pool[0] * 7 + 0x102;
		buf = (bank == 0) ? chan_slot_scratch_alloc(pool) : (void *)0;
		chan_slot_entry_init(entry, bank, wire_code, buf);
		pool[0] = pool[0] + 1;
	}
	return entry;
}

/* chan_bitmask_decode - FUN_c000c7dc, @0xc000c7dc (160 bytes, K1:
 * FUN_c000b4d8, 160 bytes). Identical two-bank word-chain walk; table base
 * DAT_c000c880 -> 0xC0027A44 (K2's own address, NOT the same literal as K1's
 * 0xc001f690 - different data segment layout, same role). */
void chan_bitmask_decode(uint32_t *out, uint32_t idx)	/* FUN_c000c7dc */
{
	extern uint32_t *chan_decode_pool;		/* DAT_c000c87c -> 0xC01CC864 */
	extern uint8_t  *chan_bitmask_table_base;	/* DAT_c000c880 -> 0xC0027A44 */
	uint32_t *pool = chan_decode_pool;
	uint32_t bank;

	pool[0] = 0;
	pool[1] = 0;
	for (bank = 0; bank < 6; bank++)
		out[bank] = 0;

	for (bank = 0; bank < 2; bank++) {
		uint8_t *chain = *(uint8_t **)(chan_bitmask_table_base + (bank + (idx & 0xff) * 2) * 4);
		int off = 4;

		for (;;) {
			if ((chain[off] & 0xe) != 0xe) {
				uint16_t word = *(uint16_t *)(chain + off);
				uint32_t *entry = chan_slot_alloc_and_init(pool, bank, word >> 6);

				out[off / 4 - 1] = (uint32_t)(uintptr_t)entry;
			}
			if (chain[off] & 1)
				break;
			off += 4;
		}
	}
}

/* chan_bitmask_decode_u8 - FUN_c000c884, @0xc000c884 (8 bytes, K1:
 * FUN_c000b580, 8 bytes). Plain byte-width-truncating wrapper. Sole K2
 * caller: FUN_c000cf20 (midi_engine.c territory's context-init cluster). */
void chan_bitmask_decode_u8(uint32_t *out, uint8_t idx)	/* FUN_c000c884 */
{
	chan_bitmask_decode(out, idx);
}

/* chan_table2_decode - FUN_c000c88c, @0xc000c88c (160 bytes, K1:
 * FUN_c000b588, 160 bytes). Structural twin of chan_bitmask_decode, sourcing
 * from a DIFFERENT table base (DAT_c000c930 -> 0xC00279F0, 0x54 bytes below
 * chan_bitmask_table_base - NOT the same 0x68-byte gap K1 had between its
 * own two tables). Genuinely unresolved which real table this is, same open
 * item as K1. */
void chan_table2_decode(uint32_t *out, uint32_t idx)	/* FUN_c000c88c */
{
	extern uint32_t *chan_decode_pool;	/* DAT_c000c92c, == chan_decode_pool above, same global (0xC01CC864) */
	extern uint8_t  *chan_table2_base;	/* DAT_c000c930 -> 0xC00279F0, NOT chan_bitmask_table_base - open item */
	uint32_t *pool = chan_decode_pool;
	uint32_t bank;

	pool[0] = 0;
	pool[1] = 0;
	for (bank = 0; bank < 6; bank++)
		out[bank] = 0;

	for (bank = 0; bank < 2; bank++) {
		uint8_t *chain = *(uint8_t **)(chan_table2_base + (bank + (idx & 0xff) * 2) * 4);
		int off = 4;

		for (;;) {
			if ((chain[off] & 0xe) != 0xe) {
				uint16_t word = *(uint16_t *)(chain + off);
				uint32_t *entry = chan_slot_alloc_and_init(pool, bank, word >> 6);

				out[off / 4 - 1] = (uint32_t)(uintptr_t)entry;
			}
			if (chain[off] & 1)
				break;
			off += 4;
		}
	}
}

/* chan_decode_result_get - FUN_c000c934, @0xc000c934 (20 bytes, K1:
 * FUN_c000b630, 20 bytes). Identical `(idx & 0xff) - 1` indexing. 3
 * confirmed K2 callers (FUN_c000d46c/_d530/_d5bc - the K2 counterparts of
 * K1's own unclaimed "slot dispatch cluster", chan_slot_dispatch.c's own
 * territory, out of this file's scope - see that file's own K2 port). */
uint32_t chan_decode_result_get(uint32_t *arr, uint32_t idx)	/* FUN_c000c934 */
{
	return arr[(idx & 0xff) - 1];
}

/* ------------------------------------------------------------------------- *
 *  Per-link hardware register access.
 * ------------------------------------------------------------------------- */

/* chan_link_probe_ready - FUN_c000c948, @0xc000c948 (8 bytes, K1:
 * FUN_c000b644, 8 bytes). Identical target+0x69 field read. Sole K2 caller:
 * FUN_c000edec, the confirmed K2 counterpart of chan_param_ctrl.c's own
 * chan_link_watchdog_tick - reproduces K1's own CROSS-FILE FINDING #1
 * exactly. */
int chan_link_probe_ready(uint32_t target)	/* FUN_c000c948 */
{
	return *(uint8_t *)(target + 0x69);
}

/* chan_slot_apply_code - FUN_c000c950, @0xc000c950 (28 bytes, K1:
 * FUN_c000b64c, 28 bytes). Identical register addressing, mask DAT_c000c96c
 * = 0x1fe0 (K1: 0x1fe0, exact match). */
void chan_slot_apply_code(uint32_t target, uint8_t code)	/* FUN_c000c950 */
{
	uint32_t base = *(uint32_t *)(uintptr_t)target;
	unsigned reg = ((uint32_t)code << 5 & 0x1fe0) + 0x68;

	midi_hw_write16(base, reg, 4);
}

/* chan_slot_echo_code - FUN_c000c970, @0xc000c970 (28 bytes, K1:
 * FUN_c000b66c, 28 bytes). Identical to chan_slot_apply_code but writes 8,
 * mask DAT_c000c98c = 0x1fe0 (exact match). */
void chan_slot_echo_code(uint32_t target, uint8_t code)	/* FUN_c000c970 */
{
	uint32_t base = *(uint32_t *)(uintptr_t)target;
	unsigned reg = ((uint32_t)code << 5 & 0x1fe0) + 0x68;

	midi_hw_write16(base, reg, 8);
}

/* chan_class2_read_value - FUN_c000c990, @0xc000c990 (52 bytes, K1:
 * FUN_c000b68c, 52 bytes). Identical bit-3-presence test on register
 * ((idx<<5)&0x1fe0)+0x74, returning only 0/1 - reproduces K1's own
 * CROSS-FILE MISMATCH note verbatim (chan_param_ctrl.c's own K2 forward
 * declaration for this address still expects a general 0-0xffff value). */
uint32_t chan_class2_read_value(uint32_t target, uint8_t idx)	/* FUN_c000c990 */
{
	uint32_t base = *(uint32_t *)(uintptr_t)target;
	unsigned reg = ((uint32_t)idx << 5 & 0x1fe0) + 0x74;
	uint16_t val = midi_hw_read16(base, reg);

	return (val & 8) != 0;
}

/* chan_link_hw_base_get - FUN_c000cb9c, @0xc000cb9c (12 bytes, K1:
 * FUN_c000b898, 12 bytes). Trivial dereference. Sole K2 caller: FUN_c000cf20
 * (midi_engine.c territory's context-init cluster, out of this file's
 * scope) - NOT a zero-caller orphan in K2, unlike K1's own "sole caller
 * FUN_c000bc1c (out of range)" note. */
uint32_t chan_link_hw_base_get(uint32_t *handle)	/* FUN_c000cb9c */
{
	return handle[0];
}

/* ------------------------------------------------------------------------- *
 *  midi_hw_set_reg_XX family - plain fixed-value register pokes.
 * ------------------------------------------------------------------------- */

/* midi_hw_set_reg_60 - FUN_c000cb44, @0xc000cb44 (16 bytes, K1: FUN_c000b840,
 * 16 bytes). 2 K2 callers: FUN_c000fd0c, FUN_c000d6a0 (conditional). */
void midi_hw_set_reg_60(uint32_t *handle)	/* FUN_c000cb44 */
{
	midi_hw_write16(handle[0], 0x60, 8);
}

/* midi_hw_set_reg_f6 - FUN_c000cb54, @0xc000cb54 (16 bytes, K1: FUN_c000b850,
 * 16 bytes). 3 K2 callers: FUN_c000a308, FUN_c000ea68 x2 (conditional). */
void midi_hw_set_reg_f6(uint32_t *handle)	/* FUN_c000cb54 */
{
	midi_hw_write16(handle[0], 0xf6, 1);
}

/* midi_hw_set_reg_e8 - FUN_c000cb64, @0xc000cb64 (16 bytes, K1: FUN_c000b860,
 * 16 bytes). Sole K2 caller: FUN_c000a308 (same caller as midi_hw_set_reg_f6
 * above - both fired from the same site, consistent with K1's own
 * master_dispatch_tick "USB-adjacent cluster" pairing). */
void midi_hw_set_reg_e8(uint32_t *handle)	/* FUN_c000cb64 */
{
	midi_hw_write16(handle[0], 0xe8, 0x80);
}

/* midi_hw_set_reg_d8 - FUN_c000cb74, @0xc000cb74 (32 bytes, K1: FUN_c000b870,
 * 32 bytes). Identical enable ? 0x10 : 0 logic. 2 K2 callers, both
 * conditional (FUN_c000e400, and one call site with no containing function
 * object in the static dump - same "unbounded/unboxed call site" artifact
 * this project has repeatedly documented elsewhere). */
void midi_hw_set_reg_d8(uint32_t *handle, uint8_t enable)	/* FUN_c000cb74 */
{
	midi_hw_write16(handle[0], 0xd8, enable ? 0x10 : 0);
}

/* ------------------------------------------------------------------------- *
 *  chan_link_hw_service - FUN_c000c9c8, @0xc000c9c8 (348 bytes, K1:
 *  FUN_c000b6c4, 348 bytes - EXACT size match). Byte-for-byte identical
 *  control flow and every numeric constant to K1: pending-flag gate,
 *  ~100000-unit busy-wait (0x186a0, via hw_timer_busy_wait), reg 0x36=0x80
 *  strobe, reg 0x34 bit 0 -> link_up stored at handle+0x69, on link-down the
 *  full bring-up (reg 0x72=0x20ff, 5-port 0x80/0xa0/0xc0/0xe0/0x100 RMW loop
 *  masked 0xfffe, 5 fixed 0x96/0xb6/0xd6/0xf6/0x116 writes = 0x11fd, reg
 *  0x32 bit-2 conditional clear masked 0xffcb), on link-up the reg 0x32
 *  bit-2 conditional set, sole caller confirmed FUN_c000a58c (K2's own
 *  equivalent of wire_dispatch.c's master_dispatch_tick - see
 *  wire_dispatch.c's own file, not edited here).
 *
 *  Pending flag (DAT_c000cb24) resolves to 0xC01CCD18 - a DIFFERENT literal
 *  than K1's 0xC01CD31C (see file header, "real differences"). Timer
 *  context (DAT_c000cb28, fed through aemif_cs3_base_get's own dead-argument
 *  chain, mirroring K1's own "phantom forwarded parameter" pattern) resolves
 *  to 0xC00E004C - also different from K1's 0xC00E0068, but in the SAME
 *  0xC00E0xxx region soc_irq_gate.c's own shared bookkeeping table
 *  (0xC00E0000-0xC00E004C) occupies, consistent evidence this is still the
 *  same class of fixed physical SRAM location, just a different slot.
 * ------------------------------------------------------------------------- */
extern void hw_timer_busy_wait(void *timer_base, int units);	/* FUN_c000185c, defined in soc_periph.c (K1: FUN_c0001aa0) */

void chan_link_hw_service(uint32_t *handle)	/* FUN_c000c9c8 */
{
	extern uint32_t *chan_link_hw_pending;		/* DAT_c000cb24 -> 0xC01CCD18 */
	extern void     *chan_link_hw_timer_ctx;	/* DAT_c000cb28 -> 0xC00E004C, opaque, not decoded */
	uint32_t base = handle[0];
	int link_up;
	uint16_t status;

	if (*chan_link_hw_pending == 0)
		return;

	hw_timer_busy_wait(chan_link_hw_timer_ctx, 0x186a0);

	midi_hw_write16(base, 0x36, 0x80);
	link_up = midi_hw_read16(base, 0x34) & 1;
	*((uint8_t *)handle + 0x69) = (uint8_t)link_up;

	if (!link_up) {
		static const unsigned rmw_regs[5]   = { 0x80, 0xa0, 0xc0, 0xe0, 0x100 };
		static const unsigned fixed_regs[5] = { 0x96, 0xb6, 0xd6, 0xf6, 0x116 };
		int i;

		midi_hw_write16(base, 0x72, 0x20ff);

		for (i = 0; i < 5; i++) {
			uint16_t v = midi_hw_read16(base, rmw_regs[i]);

			midi_hw_write16(base, rmw_regs[i], v & 0xfffe);
		}

		for (i = 0; i < 5; i++)
			midi_hw_write16(base, fixed_regs[i], 0x11fd);

		status = midi_hw_read16(base, 0x32);
		if ((status & 4) == 0)
			goto done;
		status = status & 0xffff & 0xffcb;
	} else {
		status = midi_hw_read16(base, 0x32);
		if (status & 4)
			goto done;
		status = (status & 0xffff) | 4;
	}

	midi_hw_write16(base, 0x32, status);
done:
	*chan_link_hw_pending = 0;
}
