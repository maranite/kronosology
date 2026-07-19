/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cdix4192.c - the digital audio interface chip driver (the project's own
 * earlier docs describe it as "AKM/DIX-style"; the I2C address and
 * table-driven config/verify pattern here are consistent with a Texas
 * Instruments/Burr-Brown DIT4192/DIX4192-family AES3/S/PDIF transceiver,
 * not independently confirmed against a datasheet part number).
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 * Anchor: "../CDix4192.cpp" has exactly one xref (the same clean
 * single-assert-site pattern as CryptoAt88.cpp/clcdc.cpp/ctouchpanel.cpp/
 * mcasp.c's OmapL137Mcasp.cpp).
 */

#include <stdint.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

/* ------------------------------------------------------------------------- *
 *  cdix_reg_write/cdix_reg_read - thin per-register wrappers over the SAME
 *  shared I2cByGpio.cpp primitives already reconstructed for the AT88 chip
 *  in crypto_at88.c (0xc0001874/0xc00017ec sit in the identical low address
 *  range as crypto_at88.c's own 0xc0001588/0xc0001638/0xc00016e8) - real,
 *  disassembly-confirmed evidence that I2cByGpio.cpp is genuinely a shared,
 *  generic bit-bang I2C bus driver used by more than one chip on this board,
 *  not something private to the AT88 relay. Fixed 8-bit device address 0x70
 *  for every call (not independently confirmed against a datasheet, but
 *  consistent with common digital-audio-codec I2C addressing conventions).
 *  @0xc000fa64 (write), @0xc000fb90 (read).
 * ------------------------------------------------------------------------- */
extern void cdix_i2c_write_reg(void *chip, int addr, uint8_t reg, uint8_t value);	/* FUN_c0001874 */
extern void cdix_i2c_read_reg(void *chip, int addr, uint8_t reg, uint8_t *out);	/* FUN_c00017ec */

#define CDIX_I2C_ADDR 0x70

/* CORRECTION (re-verification pass, 2026-07-17): the original version of
 * this file claimed `chip` was forwarded into cdix_i2c_write_reg/read_reg.
 * Independent re-verification against fresh disassembly found this is
 * WRONG: the incoming `chip` argument (r0) is never read by either wrapper
 * - it's overwritten before use, and an UNINITIALIZED local stack buffer
 * is passed as the first argument to the I2C primitive instead. Left as a
 * `void *chip` parameter here only because `cdix_configure_and_verify`
 * genuinely does pass its own `chip` argument at this call site - but that
 * argument is real firmware dead code from these two wrappers' own
 * perspective, not something this reconstruction should claim is used.
 * Not yet resolved: whether this is a genuine upstream firmware quirk (an
 * uninitialized handle that happens to not matter because the I2C bus is a
 * singleton, addressed purely by CDIX_I2C_ADDR), or a sign that these two
 * functions' real source-level signature never took a chip/context
 * parameter at all and Ghidra's frame analysis mislabeled unrelated stack
 * space as `param_1`. Left honestly unresolved rather than guessed at. */
static void cdix_reg_write(void *chip, uint8_t reg, uint8_t value)	/* FUN_c000fa64 */
{
	(void)chip;	/* NOT actually forwarded - see correction note above */
	cdix_i2c_write_reg(0 /* real call passes an uninitialized stack buffer here */, CDIX_I2C_ADDR, reg, value);
}

static void cdix_reg_read(void *chip, uint8_t reg, uint8_t *out)	/* FUN_c000fb90 */
{
	(void)chip;	/* NOT actually forwarded - see correction note above */
	cdix_i2c_read_reg(0 /* real call passes an uninitialized stack buffer here */, CDIX_I2C_ADDR, reg, out);
}

/* ------------------------------------------------------------------------- *
 *  cdix_configure_and_verify - the confirmed anchor function. Walks a
 *  {register, value} pair table (4-byte-stride entries, terminated by a
 *  register byte of -1/0xff) TWICE: first pass writes every pair via
 *  cdix_reg_write; second pass re-reads every register via cdix_reg_read and
 *  hard-faults on the first mismatch against the expected value. A genuine
 *  write-then-verify power-on configuration sequence, not just a blind
 *  init - the chip's registers are actually read back to confirm the writes
 *  took effect. @0xc000fbbc.
 *
 *  No static callers found anywhere in the image (confirmed via xref
 *  search) - same "zero static callers" pattern already seen for
 *  cobjectmgr.c's object destructor and CryptoAt88.cpp's own self-test,
 *  meaning this is very likely invoked through some indirect call Ghidra's
 *  static analysis couldn't resolve. CORRECTION (SPI/USB cleanup pass,
 *  2026-07-17): originally attributed to `EvaBoardMain.cpp`'s own
 *  init-table walker - that table's actual contents are now known (see
 *  eva_board_main.c) to be a single lazy-init singleton entry that does NOT
 *  call this function. The real invocation mechanism remains genuinely
 *  unresolved, not explained by the init table after all.
 * ------------------------------------------------------------------------- */
struct cdix_reg_entry {
	int8_t  reg;		/* -1 (0xff) terminates the table */
	uint8_t value;
	uint8_t pad[2];		/* real entry stride is 4 bytes; these 2 bytes'
				 * content not inspected this pass */
};

void cdix_configure_and_verify(void *chip)	/* FUN_c000fbbc */
{
	/* Real table contents (re-verification pass, 2026-07-17), real address
	 * 0xc001fb6c, 3 entries, 0xff-terminated:
	 *   { 0x7f, 0x00 }, { 0x03, 0x29 }, { 0x04, 0x03 }
	 * Which DIX4192 register each number addresses (sample rate, format,
	 * clock source, ...) is not decoded - see the README's own note. */
	extern const struct cdix_reg_entry cdix_config_table[];	/* DAT_c000fc40 */

	for (const struct cdix_reg_entry *e = cdix_config_table; e->reg != -1; e++)
		cdix_reg_write(chip, (uint8_t)e->reg, e->value);

	for (const struct cdix_reg_entry *e = cdix_config_table; e->reg != -1; e++) {
		uint8_t readback;

		cdix_reg_read(chip, (uint8_t)e->reg, &readback);
		if (readback != e->value)
			/* DAT_c000fc44 resolved (verification pass, 2026-07-18,
			 * static-dump re-query): 0xc0023180, the exact address of
			 * the "../CDix4192.cpp" string itself - confirms this
			 * really is the file's own single anchor xref, not a
			 * coincidence of address arithmetic. */
			crypto_at88_fault(0, (const char *)0xc0023180, 0x18c);
	}
}

/* NEEDS LIVE QUERY: 0xc001fb6c (the real cdix_config_table address, itself
 * confirmed this pass - DAT_c000fc40 resolves to exactly this address) -
 * the static all_data.json dump has no entry for raw memory content at this
 * address (it only captured DAT_/PTR_ tokens actually referenced by name in
 * decompiled code, not arbitrary computed addresses), so the 2 padding
 * bytes per table entry remain unread. A live `read_memory 0xc001fb6c 16`
 * would resolve this in one call. */
