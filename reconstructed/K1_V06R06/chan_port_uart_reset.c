/* SPDX-License-Identifier: GPL-2.0 */
/*
 * chan_port_uart_reset.c - reconstructs FUN_c000bcc4 (0xc000bcc4, 500
 * bytes), the single function this task assigned as "0xc000bcc4 (1 fn,
 * near chan_slot_dispatch.c)". Confirmed genuinely uncovered: grepped for
 * "c000bcc4" across every *.c file in this project first - zero hits.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json, 2026-07-18 pass), read via query_dump.py. No live Ghidra
 * MCP calls this pass (bridge flagged concurrency-unsafe under this
 * project's own parallel-agent constraint).
 *
 * WHY THIS IS A REAL, ISOLATED GAP (not folded into either neighbor):
 * chan_link_hw.c's own file header states its assigned range is
 * 0xc000b414-0xc000b898; chan_slot_dispatch.c's own file header states its
 * assigned range is 0xc000bedc-0xc000c39c. FUN_c000bcc4 (500 bytes = 0x1f4)
 * runs 0xc000bcc4-0xc000beb8, sitting entirely in the address gap BETWEEN
 * those two files' own ranges - confirmed by direct size arithmetic
 * (0xc000bcc4 + 0x1f4 = 0xc000beb8, which is exactly DAT_c000beb8, the
 * literal-pool mask constant this same function reads - the function's own
 * end lines up exactly with its own trailing data).
 *
 * ANCHOR: NONE - same no-anchor situation as chan_slot_dispatch.c/
 * chan_param_ctrl.c (all 14 real "../<name>.cpp" strings in this image
 * already claimed elsewhere).
 *
 * RAW DECOMPILE (transcribed in full - the 3-way if/else-if/else selects
 * a fixed per-channel register base, then ALL THREE BRANCHES FALL THROUGH
 * TO ONE SHARED TAIL CALL after the if-chain, not a per-branch write -
 * important structural detail, easy to miss on a quick read):
 *
 *   void FUN_c000bcc4(undefined4 *param_1, int param_2)
 *   {
 *     if (param_2 == 1) {
 *       uVar1 = FUN_c0000c6c(*param_1, 0x80);
 *       FUN_c0000c38(*param_1, 0x80, uVar1 & DAT_c000beb8);
 *       FUN_c0000c38(*param_1, 0x82, 0x138);
 *       FUN_c0000c38(*param_1, 0x80);                 // <- only 2 visible args, see note
 *       FUN_c0000c38(*param_1, 0x88, 2);
 *       FUN_c0000c38(*param_1, 0x88, 2);
 *       iVar2 = FUN_c0000c6c(*param_1, 0x80);
 *       uVar3 = *param_1; uVar1 = (iVar2<<0x10|0x10000U)>>0x10; uVar4 = 0x80;
 *     } else if (param_2 == 2) {  ... same shape on reg base 0xa0, baud 3 ... }
 *     else { if (param_2 != 5) return;  ... same shape on reg base 0x100,
 *            baud reg DAT_c000bed0 (literal 0x102), baud val 0x138 ... }
 *     FUN_c0000c38(uVar3, uVar4, uVar1);
 *     return;
 *   }
 *
 * FUN_c0000c6c/FUN_c0000c38 are midi_hw_read16/midi_hw_write16
 * (soc_irq_gate.c). Register bases (0x80/0xa0/0x100) match the SAME
 * base(0x60)+channel*0x20 per-channel stride chan_slot_dispatch.c's own
 * midi_hw_fifo_write documents for channels 1/2/5.
 *
 * ARITHMETIC NOTE (worked out explicitly, not obvious from the raw shift
 * sequence): `(iVar2 << 0x10 | 0x10000) >> 0x10` in 32-bit unsigned
 * arithmetic reduces to `(iVar2 & 0xffff) | 1` - i.e. take the just-reread
 * status word's low 16 bits and FORCE BIT 0 SET. Combined with step 1's
 * `uVar1 & DAT_c000beb8` (DAT_c000beb8 = 0xfffe, i.e. CLEAR bit 0), the
 * real sequence per channel is: clear status bit 0, program the baud/mode
 * register, dummy-poll the status register, pulse register+8 with value 2
 * TWICE (real doubled write in the raw decompile, not a transcription
 * duplicate - a plausible "reset-then-arm" idiom, not independently
 * confirmed), then RE-SET status bit 0 - a believable disable/configure/
 * re-enable sequence, not a no-op round-trip as a naive shift-cancellation
 * reading would suggest.
 *
 * PHANTOM/UNCERTAIN 3RD ARGUMENT: the 4th call in each branch
 * (`FUN_c0000c38(*param_1, 0x80)`) shows only 2 arguments in the Ghidra
 * decompile - the callee's own 3rd parameter (`val`) is not set by any
 * instruction Ghidra attributes to this call site, matching this
 * project's already-established "phantom forwarded parameter" idiom
 * (cdix4192.c, eva_board_watchdog_fault_wrapper, chan_hw_fifo_write).
 * The most likely real register content is whatever the immediately
 * preceding baud-register write last set r2 to (0x138 / 3 / 0x138) -
 * modeled here as that same value, explicitly flagged as inferred, not
 * as a confirmed 3rd argument.
 *
 * Confirmed callers (2, both FUN_c000e498 - a large USB-EP0/channel-desc
 * dispatcher just past chan_param_ctrl.c's own assigned range and not
 * reconstructed by any file in this project so far): call sites
 * 0xc000e5e0 (channel=1) and 0xc000e6bc (channel=2) per FUN_c000e498's own
 * decompile - that function also reads DAT_c000e740/DAT_c000e744, the
 * SAME literal family as this function's own DAT_c000beb8/bed0, further
 * confirming the shared register file.
 *
 * @0xc000bcc4 (500 bytes).
 */

#include <stdint.h>

extern uint16_t midi_hw_read16(uint32_t base_sel, unsigned reg_off);		/* FUN_c0000c6c, soc_irq_gate.c */
extern void     midi_hw_write16(uint32_t base_sel, unsigned reg_off, uint16_t val);	/* FUN_c0000c38, soc_irq_gate.c */

/* chan_uart_ctrl_clear_mask - DAT_c000beb8, real literal 0xfffe. */
#define CHAN_UART_CTRL_CLEAR_MASK	0xfffe
/* chan_uart_ch5_baud_reg - DAT_c000bed0, real literal 0x102 (channel 5's
 * baud/mode register - numerically equal to reg_base(0x100)+0x02, but
 * transcribed as the literal DAT_ read to stay faithful to the raw
 * decompile rather than assume the addition form). */
#define CHAN_UART_CH5_BAUD_REG		0x102

/* chan_port_uart_reset - handle is a pointer-to-handle (`*handle` used
 * throughout as the base_sel argument, matching this file's own
 * xref-confirmed callers, FUN_c000e498, which passes such a pointer).
 * channel selects one of three fixed per-channel register bases: 1 ->
 * 0x80, 2 -> 0xa0, 5 -> 0x100 (the SAME base(0x60)+channel*0x20 stride
 * midi_hw_fifo_write documents). Any other channel value is a silent
 * no-op (matches the raw `if (param_2 != 5) return;` fallthrough). */
void chan_port_uart_reset(uint32_t *handle, int channel)	/* FUN_c000bcc4 */
{
	uint32_t dev = *handle;
	unsigned reg_base, baud_reg;
	uint16_t baud_val;
	uint16_t status;

	switch (channel) {
	case 1:  reg_base = 0x80;  baud_reg = 0x82; baud_val = 0x138; break;
	case 2:  reg_base = 0xa0;  baud_reg = 0xa2; baud_val = 3;     break;
	case 5:  reg_base = 0x100; baud_reg = CHAN_UART_CH5_BAUD_REG; baud_val = 0x138; break;
	default: return;
	}

	status = midi_hw_read16(dev, reg_base);
	midi_hw_write16(dev, reg_base, status & CHAN_UART_CTRL_CLEAR_MASK);
	midi_hw_write16(dev, baud_reg, baud_val);
	midi_hw_write16(dev, reg_base, baud_val);	/* 3rd arg inferred/uncertain - see note above */
	midi_hw_write16(dev, reg_base + 8, 2);
	midi_hw_write16(dev, reg_base + 8, 2);		/* real doubled write, not a duplicate line */

	status = midi_hw_read16(dev, reg_base);
	midi_hw_write16(dev, reg_base, (status & 0xffffu) | 1u);	/* re-set status bit 0 - see arithmetic note */
}
