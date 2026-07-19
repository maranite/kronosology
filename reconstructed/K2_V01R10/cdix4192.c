/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cdix4192.c - K2 (KRONOS2S_V01R10.VSB / "KRONOS II") port of the digital
 * audio interface chip driver already reconstructed for K1 in
 * K1_V06R06/cdix4192.c. See that file's own header for the chip-identity
 * caveat (TI/Burr-Brown DIT4192/DIX4192-family AES3/S/PDIF transceiver,
 * not independently confirmed against a datasheet part number).
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB
 * (query_dump_k2.py), 2026-07-18. Anchor: the literal "../CDix4192.cpp"
 * string lives at 0xc002b5e8 (K1: 0xc0023180) and is referenced by exactly
 * ONE DAT_ global (DAT_c001100c, resolved via its Ghidra data_value field)
 * inside cdix_configure_and_verify below - the same single-assert-site
 * anchor pattern K1 used.
 *
 * Every function in this file is a STRUCTURALLY IDENTICAL port of its K1
 * counterpart - same operations, same "phantom chip parameter" quirk, same
 * fixed I2C device address - only addresses and the assert's line-number
 * literal differ. No logic differences found between K1 and K2 for this
 * subsystem.
 */

#include <stdint.h>

extern void cdix_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000a730 - K2's shared fault/assert/hang helper; same role as K1's FUN_c000919c/crypto_at88_fault, confirmed by decompile shape (draws two text lines via the draw_text primitive, then loops forever) and by its 63 static callers spanning the whole image */

/* ------------------------------------------------------------------------- *
 *  cdix_reg_write/cdix_reg_read - PORTED from K1, verified structurally
 *  identical against K2's real decompile. Same shared-I2C-bit-bang-driver
 *  wrapper shape as K1 (see that file's own note on I2cByGpio.cpp being a
 *  genuinely shared bus driver, not private to this chip), and the SAME
 *  "chip parameter not actually forwarded" quirk: the real K2 disassembly
 *  also passes an uninitialized local stack buffer (`auStack_14` in the raw
 *  decompile) as the first argument to the I2C primitive, exactly like K1.
 *  Fixed 8-bit I2C device address 0x70, unchanged from K1.
 *  K2 addresses: cdix_reg_write @0xc0010e2c (K1: 0xc000fa64), cdix_reg_read
 *  @0xc0010f58 (K1: 0xc000fb90). Both confirmed structurally identical
 *  (same instruction shape, same uninitialized-buffer quirk) against K1's
 *  0xc000fa64/0xc000fb90.
 * ------------------------------------------------------------------------- */
extern void cdix_i2c_write_reg(void *chip, int addr, uint8_t reg, uint8_t value);	/* FUN_c00015f4 - K2 counterpart of K1's FUN_c0001874 (shared I2cByGpio.cpp primitive, not authored in this file) */
extern void cdix_i2c_read_reg(void *chip, int addr, uint8_t reg, uint8_t *out);	/* FUN_c000156c - K2 counterpart of K1's FUN_c00017ec */

#define CDIX_I2C_ADDR 0x70

static void cdix_reg_write(void *chip, uint8_t reg, uint8_t value)	/* FUN_c0010e2c */
{
	(void)chip;	/* NOT actually forwarded - same quirk as K1, re-confirmed against K2's own decompile */
	cdix_i2c_write_reg(0 /* real K2 call also passes an uninitialized stack buffer here */, CDIX_I2C_ADDR, reg, value);
}

static void cdix_reg_read(void *chip, uint8_t reg, uint8_t *out)	/* FUN_c0010f58 */
{
	(void)chip;	/* NOT actually forwarded - same quirk as K1 */
	cdix_i2c_read_reg(0 /* real K2 call also passes an uninitialized stack buffer here */, CDIX_I2C_ADDR, reg, out);
}

/* ------------------------------------------------------------------------- *
 *  cdix_configure_and_verify - PORTED from K1, verified structurally
 *  identical: walks a {register, value} pair table (4-byte-stride entries,
 *  0xff-terminated) TWICE, write-then-verify, hard-faulting on the first
 *  readback mismatch. @0xc0010f84 (K1: 0xc000fbbc).
 *
 *  Zero static callers found in K2 either (confirmed via an explicit callers
 *  query on 0xc0010f84) - the real invocation mechanism remains just as
 *  unresolved in K2 as it was in K1.
 *
 *  Config table address CONFIRMED (via DAT_c0011008's resolved data_value):
 *  0xc0027f28 (K1: 0xc001fb6c).
 *
 *  2026-07-19 LIVE QUERY RESOLVED: table contents read via live
 *  read_memory(0xc0027f28, 16) - matches the "3 real entries" prediction
 *  exactly: {reg=0x7f,val=0x00}, {reg=0x03,val=0x29}, {reg=0x04,val=0x03},
 *  then the {reg=0xff,...} terminator at the 4th slot. Pad bytes are 0x00
 *  in every entry (never independently meaningful this pass). This table
 *  sits immediately after cdix_autoswitch.c's own cdix_reset_table_a
 *  (0xc0027ef4-0xc0027f24, terminator included) with zero gap - the two
 *  tables are byte-contiguous in .rodata, independently confirming both
 *  reads are correctly bounded (neither runs into the other's data).
 *
 *  Fault call site CONFIRMED: `cdix_fault(0, (const char *)0xc002b5e8, 400)`.
 *  The file-string argument resolves (via DAT_c001100c's data_value) to
 *  exactly 0xc002b5e8, the CDix4192.cpp string's own address - the same
 *  "resolves to its own anchor string" double-confirmation K1 had for
 *  0xc0023180. The line number itself, 400 decimal, differs from K1's 0x18c
 *  (396 decimal) - a small, expected divergence consistent with K2 being a
 *  genuinely separate compile of a slightly different CDix4192.cpp source
 *  revision, not a recompiled-unchanged K1 (the same kind of small line-
 *  number drift was independently found in this pass's clcdc.c work too).
 * ------------------------------------------------------------------------- */
struct cdix_reg_entry {
	int8_t  reg;		/* -1 (0xff) terminates the table */
	uint8_t value;
	uint8_t pad[2];		/* real entry stride is 4 bytes; content not inspected, same as K1 */
};

void cdix_configure_and_verify(void *chip)	/* FUN_c0010f84 */
{
	/* 2026-07-19: table contents CONFIRMED via live read_memory(0xc0027f28, 16):
	 * { {0x7f,0x00}, {0x03,0x29}, {0x04,0x03}, {-1,...} } - see header note. */
	extern const struct cdix_reg_entry cdix_config_table[];	/* DAT_c0011008 = 0xc0027f28 */

	for (const struct cdix_reg_entry *e = cdix_config_table; e->reg != -1; e++)
		cdix_reg_write(chip, (uint8_t)e->reg, e->value);

	for (const struct cdix_reg_entry *e = cdix_config_table; e->reg != -1; e++) {
		uint8_t readback;

		cdix_reg_read(chip, (uint8_t)e->reg, &readback);
		if (readback != e->value)
			/* DAT_c001100c confirmed: 0xc002b5e8, this file's own
			 * "../CDix4192.cpp" anchor string address. */
			cdix_fault(0, (const char *)0xc002b5e8, 400);
	}
}
