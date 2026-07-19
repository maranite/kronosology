/* SPDX-License-Identifier: GPL-2.0 */
/*
 * i2c_by_gpio.c - the NKS4 panel firmware's shared GPIO bit-bang I2C bus
 * driver ("../I2cByGpio.cpp"). Both crypto_at88.c (the AT88SC/NV2AC security
 * chip) and cdix4192.c (the DIX4192-family AES3/S/PDIF transceiver) build on
 * this single physical bus.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS2S_V01R10.VSB (Kronos 2,
 * "KRONOS II" product tag; same TI OMAP-L1x/ARM926EJ-S target as the K1
 * (KRONOS_V06R06.VSB) firmware, loaded at the same 0xC0000000 base), read via
 * the pre-fetched static dump (all_decompiled_k2.json/all_data_k2.json), not
 * the live Ghidra bridge (concurrency-unsafe with other agents' sessions on
 * the same two projects). This is a migration pass from the already-done K1
 * reconstruction at kronosology/reconstructed/K1_V06R06/i2c_by_gpio.c -
 * EVERY function below was independently re-matched against K2's own
 * decompile by code shape (not assumed to sit at the same address - it
 * doesn't; K2's whole low-address function layout is shifted/reordered
 * relative to K1's build) and every K1 finding this file depends on (retry
 * bounds, byte masks, the dead `chip` parameter, the shared busy-guard
 * global) was independently re-derived from K2's own DAT_/literal values,
 * not copied from K1's citations.
 *
 * ANCHOR: the literal string "../I2cByGpio.cpp" lives at 0xc002a720 (given by
 * the orchestrating task's own confirmed anchor list). Independently
 * reconfirmed here: i2c_gpio_busy_guard_enter's fault call
 * (`FUN_c000a730(0, DAT_c0001264, 0xbb)`) reads DAT_c0001264, which resolves
 * to literal value 0xc002a720 - exact match. Same single-assert-site anchor
 * pattern as K1.
 *
 * SCOPE NOTE (unchanged from K1): the dense I2C-only function cluster is
 * reconstructed here. Two address-adjacent functions are left `extern`:
 *   - gpio_bank_get_base (FUN_c0001710 in K2, was FUN_c0001990 in K1) - 30
 *     callers spanning the whole image (vs K1's 66; K2 clearly has fewer
 *     total functions/callers in this address range, but the "firmware-wide
 *     generic getter" classification is unchanged - callers include
 *     EvaBoardMain bring-up, LED clusters, and unrelated init code far
 *     outside this file's own cluster).
 *   - hw_timer_busy_wait (FUN_c000185c in K2, was FUN_c0001aa0 in K1) - 17
 *     callers firmware-wide (vs K1's 16, same story). Its wraparound period
 *     constant DAT_c00018e4 = 0x249f1 = 150001 decimal - IDENTICAL to K1's
 *     own DAT_c0001b28 value, confirming the underlying hardware timer
 *     characteristics are unchanged between the two boards.
 * Two more, address-distant-but-caller-local functions are also left
 * `extern`, same as K1:
 *   - gpio_bank_set_dir_bit (FUN_c000208c in K2, was FUN_c00025ac in K1) -
 *     exactly 1 caller (i2c_gpio_set_sda_dir below), confirming I2C-only
 *     usage in practice despite living at a distant address.
 *   - gpio_bank_read_sda_bit (FUN_c00020c0 in K2, was FUN_c00025e4 in K1) -
 *     exactly 1 caller (i2c_gpio_sda_read below), same story. NOTE: K2's
 *     version reads the SDA bit directly (`*(uint*)(base+0x20) >> 0x12 & 1`)
 *     rather than through a further FUN_c0002238 indirection K1 cited -
 *     functionally identical (still bit 18 of the bank's IN_DATA-shaped
 *     register at +0x20), just a shallower inline in this build.
 *
 * STRUCTURAL CORRECTION vs K1's OWN i2c_by_gpio.c (found by re-deriving from
 * K2's real decompile, not assumed): K1's i2c_gpio_write_block/read_block
 * were declared with a 6-argument shape `(chip, addr, cmd, arg1, arg2, len,
 * data)` where `addr` and `cmd` are two separate header-byte parameters, and
 * `len` is NEVER threaded into the frame_command call in that file's own
 * function bodies. Re-checked against K2's real FUN_c0001468 (write_block)/
 * FUN_c00013b8 (read_block): both call frame_command with exactly 4 explicit
 * arguments after `chip` (`FUN_c0001308(param_1,param_2,param_3,param_4,
 * param_5)`), where the caller's own 5th parameter is the actual transfer
 * length (declared `int`, truncated to the callee's `uint8_t` slot) - i.e.
 * there is NO separate "addr" parameter distinct from the AT88 opcode/"cmd"
 * byte, and `len` DOES occupy frame_command's 4th header-byte slot exactly as
 * K1's OWN crypto_at88.c (not its i2c_by_gpio.c) already documented under
 * "HEADER BYTE MAPPING". This matches K1's crypto_at88.c's 5-argument
 * `(chip, cmd, arg1, arg2, len, data)` shape, not i2c_by_gpio.c's 6-argument
 * one. K1's i2c_by_gpio.c signature for these two functions appears to be an
 * un-cross-checked simplification, not independently re-derived from a fresh
 * decompile of that specific pair - the version below follows the
 * decompile-confirmed 5-argument shape for both K1 and K2 addresses alike.
 *
 * FILE-SPLIT NOTE (per this migration pass's own explicit instructions - do
 * not double-port the same function into both output files): K1's
 * crypto_at88.c kept static, address-identical DUPLICATE copies of most of
 * this file's own functions (at88_i2c_start/stop/write_byte/read_byte/ack,
 * at88_frame_command, at88_i2c_write/read, at88_lock/at88_unlock) "for
 * continuity with earlier passes' citations". This K2 port does NOT
 * reproduce that duplication: every I2C-bit-bang-layer function (including
 * the busy-guard lock/unlock pair, which the anchor string itself confirms
 * belongs to this translation unit) is defined exactly once, here.
 * crypto_at88.c calls into this file's exported (non-static) entry points
 * via `extern` declarations instead of keeping its own copy.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Out-of-scope dependencies - generic, firmware-wide primitives this file
 *  calls into but does not itself define. See SCOPE NOTE above.
 * ------------------------------------------------------------------------- */
extern void *gpio_bank_get_base(void);				/* FUN_c0001710 (K1: FUN_c0001990) - fixed return DAT_c0001718 = 0x01E26000, the real TI OMAP-L138/DA850 GPIO-bank register base - IDENTICAL to K1's value, confirming same hardware bank; 30 callers firmware-wide */
extern void  hw_timer_busy_wait(void *timer_base, int units);	/* FUN_c000185c (K1: FUN_c0001aa0) - free-running-timer spin loop, wraparound period DAT_c00018e4=150001 (identical to K1's DAT_c0001b28); 17 callers firmware-wide, not I2C-specific */
extern void  crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000a730 (K1: FUN_c000919c), defined in crypto_at88.c - the shared assert/fault handler, 63 callers firmware-wide despite the name */

/* SDA-line-only helpers that physically live in the OTHER, generic
 * GPIO-bank-register file (see SCOPE NOTE) rather than in this cluster. */
extern void gpio_bank_set_dir_bit(void *base, int input);	/* FUN_c000208c (K1: FUN_c00025ac) - toggles bit 0x40000 (SDA, bank-0 bit 18) in the DIR register at base+0x10; 0=clear bit=output, nonzero=set bit=input - same TI convention as K1 */
extern int  gpio_bank_read_sda_bit(void *base);		/* FUN_c00020c0 (K1: FUN_c00025e4) - reads bit 18 (0x40000) straight out of base+0x20 in this K2 build (K1 went through one more helper call, FUN_c0002238, for the same read - functionally identical) */

/* ------------------------------------------------------------------------- *
 *  Bus-busy reentrancy guard - a single global flag byte, same runtime
 *  address read by busy_guard_enter and cleared by busy_guard_exit
 *  (confirmed via K2's own literal pool: both resolve to 0xc00e0048).
 *  Every transaction-level entry point below (write_block, read_block,
 *  write_reg8, read_reg8, bus_reset) brackets its work with enter/exit.
 * ------------------------------------------------------------------------- */

extern uint8_t i2c_gpio_busy_flag;	/* 0xc00e0048 in this K2 build (K1's DAT_c00014e0/DAT_c00014f8 target was not itself extracted there either - same "real address not independently confirmed, only the shared-ness is" caveat carries over) */

/* i2c_gpio_busy_guard_enter - FUN_c000122c, @0xc000122c (K1: FUN_c00014ac).
 * Faults (never returns) if the bus is already marked busy, citing
 * "../I2cByGpio.cpp" line 0xbb (187) via DAT_c0001264 = 0xc002a720 -
 * independently re-derived, matches this file's own anchor exactly. */
static void i2c_gpio_busy_guard_enter(void)
{
	if (i2c_gpio_busy_flag != 0)
		crypto_at88_fault(0, "../I2cByGpio.cpp", 0xbb);
	i2c_gpio_busy_flag = 1;
}

/* i2c_gpio_busy_guard_exit - FUN_c0001268, @0xc0001268 (K1: FUN_c00014e8). */
static void i2c_gpio_busy_guard_exit(void)
{
	i2c_gpio_busy_flag = 0;
}

/* ------------------------------------------------------------------------- *
 *  i2c_gpio_delay - FUN_c0000f40, @0xc0000f40 (K1: FUN_c00011c0). Every call
 *  site in this file historically passes a chip/context first argument
 *  (dead, see below) and a units value that varies per site (0x32, 0x14, 1,
 *  1000, ...). K2's own decompile shows the same shape as K1: a bare
 *  single-argument tail call `FUN_c000185c(DAT_c0000f48)` - `units` is not
 *  independently visible in the decompiled body here either (same
 *  register-wiring ambiguity K1 flagged, not more resolved in this build).
 *  DAT_c0000f48 = 0xc00e004c, four bytes past the busy-guard flag
 *  (0xc00e0048) - consistent with K1's finding that this "timer base" is
 *  really just another fixed small-object field, not a genuine per-instance
 *  hardware timer handle.
 * ------------------------------------------------------------------------- */
extern void *i2c_gpio_delay_timer_base;	/* 0xc00e004c in this K2 build */

static void i2c_gpio_delay(void *chip, int units)
{
	(void)chip;	/* dead at every leaf of this driver - see i2c_gpio_set_scl/set_sda below */
	hw_timer_busy_wait(i2c_gpio_delay_timer_base, units);
}

/* ------------------------------------------------------------------------- *
 *  SCL/SDA level + direction control - FUN_c0000ec8/FUN_c0000ef0/FUN_c0000f4c
 *  (K1: FUN_c0001148/FUN_c0001170/FUN_c00011cc).
 *
 *  CONFIRMED (independently, this K2 pass): `chip` (param_1) is declared but
 *  READ BY NEITHER set_scl NOR set_sda - both call gpio_bank_get_base() and
 *  ignore their own param_1 entirely, exactly like K1. One fixed GPIO bank
 *  (base 0x01E26000, IDENTICAL value to K1), SCL = bank-0 bit 19 (mask
 *  0x80000), SDA = bank-0 bit 18 (mask 0x40000) - same masks as K1.
 *
 *  STRUCTURAL DIFFERENCE vs K1 (real, not a transcription artifact): K1's
 *  set_scl/set_sda called through two further helper functions
 *  (gpio_bank_write_set/gpio_bank_write_clr, offsets +0x18/+0x1c) to
 *  actually touch the SET/CLR registers. In this K2 build those helpers are
 *  fully inlined - set_scl/set_sda write `base+0x18`/`base+0x1c` directly.
 *  Functionally identical (same register offsets, same mask, same
 *  SET-when-level!=0/CLR-when-level==0 polarity), just a different
 *  compiler-inlining choice; reproduced here as K2's real decompile shows it
 *  rather than force-fitting K1's extra call layer.
 * ------------------------------------------------------------------------- */

#define I2C_GPIO_SCL_MASK	0x80000u	/* bank-0 bit 19 */
#define I2C_GPIO_SDA_MASK	0x40000u	/* bank-0 bit 18 */
#define I2C_GPIO_BANK_SET_OFF	0x18		/* SET-style register, confirmed inline in this K2 build */
#define I2C_GPIO_BANK_CLR_OFF	0x1c		/* CLR-style register, confirmed inline in this K2 build */

/* i2c_gpio_set_scl - FUN_c0000ec8, @0xc0000ec8. */
static void i2c_gpio_set_scl(void *chip, int level)
{
	(void)chip;	/* confirmed unused in the real function */
	uint8_t *base = (uint8_t *)gpio_bank_get_base();

	if (level != 0)
		*(volatile uint32_t *)(base + I2C_GPIO_BANK_SET_OFF) = I2C_GPIO_SCL_MASK;
	else
		*(volatile uint32_t *)(base + I2C_GPIO_BANK_CLR_OFF) = I2C_GPIO_SCL_MASK;
}

/* i2c_gpio_set_sda - FUN_c0000ef0, @0xc0000ef0. */
static void i2c_gpio_set_sda(void *chip, int level)
{
	(void)chip;	/* confirmed unused in the real function */
	uint8_t *base = (uint8_t *)gpio_bank_get_base();

	if (level != 0)
		*(volatile uint32_t *)(base + I2C_GPIO_BANK_SET_OFF) = I2C_GPIO_SDA_MASK;
	else
		*(volatile uint32_t *)(base + I2C_GPIO_BANK_CLR_OFF) = I2C_GPIO_SDA_MASK;
}

/* i2c_gpio_set_sda_dir - FUN_c0000f4c, @0xc0000f4c (K1: FUN_c00011cc).
 * `chip` still unused (forwarded to gpio_bank_set_dir_bit only as far as the
 * delay call at the end); base for the DIR-bit helper is gpio_bank_get_base()
 * again. Followed by a fixed 20-unit delay (0x14), matches K1 exactly. */
static void i2c_gpio_set_sda_dir(void *chip, int input)
{
	void *base = gpio_bank_get_base();

	gpio_bank_set_dir_bit(base, input);
	i2c_gpio_delay(chip, 0x14);
}

/* i2c_gpio_sda_read - FUN_c0000f18, @0xc0000f18 (K1: FUN_c0001198). Raw SDA
 * line level (0/1), normalized to exactly 0 or 1. */
static int i2c_gpio_sda_read(void *chip)
{
	(void)chip;
	void *base = gpio_bank_get_base();
	int bit = gpio_bank_read_sda_bit(base);

	return (bit != 0) ? 1 : 0;
}

/* ------------------------------------------------------------------------- *
 *  i2c_gpio_start / i2c_gpio_stop - FUN_c00011bc / FUN_c000114c
 *  (K1: FUN_c000143c / FUN_c00013cc). Both re-verified byte-for-byte
 *  identical in call shape to K1's own reconstruction.
 * ------------------------------------------------------------------------- */

/* i2c_gpio_start - FUN_c00011bc, @0xc00011bc. SDA high, SCL high, then SDA
 * falls while SCL still high (the START edge), then SCL falls. */
static void i2c_gpio_start(void *chip)
{
	i2c_gpio_set_sda(chip, 1);
	i2c_gpio_set_scl(chip, 1);
	i2c_gpio_set_sda_dir(chip, 0);
	i2c_gpio_delay(chip, 0x32);
	i2c_gpio_set_sda(chip, 0);
	i2c_gpio_delay(chip, 0x32);
	i2c_gpio_set_scl(chip, 0);
	i2c_gpio_delay(chip, 0x32);
}

/* i2c_gpio_stop - FUN_c000114c, @0xc000114c. */
static void i2c_gpio_stop(void *chip)
{
	i2c_gpio_set_sda(chip, 0);
	i2c_gpio_set_scl(chip, 0);
	i2c_gpio_set_sda_dir(chip, 0);
	i2c_gpio_delay(chip, 0x32);
	i2c_gpio_set_scl(chip, 1);
	i2c_gpio_delay(chip, 0x32);
	i2c_gpio_set_sda(chip, 1);
	i2c_gpio_delay(chip, 0x32);
}

/* ------------------------------------------------------------------------- *
 *  i2c_gpio_write_byte / i2c_gpio_read_byte / i2c_gpio_ack_or_nack -
 *  FUN_c00010cc / FUN_c0001048 / FUN_c0000fdc
 *  (K1: FUN_c000134c / FUN_c00012c8 / FUN_c000125c). All three re-verified
 *  identical in shape to K1.
 * ------------------------------------------------------------------------- */

static int i2c_gpio_ack_or_nack(void *chip);	/* forward decl - defined below, mirrors real function order */

/* i2c_gpio_write_byte - FUN_c00010cc, @0xc00010cc. Write one byte MSB-first
 * (SDA settle -> SCL pulse per bit), then sample the ACK bit. Returns
 * i2c_gpio_ack_or_nack's convention: 0=ACK, 1=NACK. */
static int i2c_gpio_write_byte(void *chip, uint8_t byte)
{
	int i;

	for (i = 7; i >= 0; i--) {
		i2c_gpio_set_sda(chip, (byte >> 7) & 1);
		i2c_gpio_delay(chip, 0x32);
		i2c_gpio_set_scl(chip, 1);
		i2c_gpio_delay(chip, 0x32);
		i2c_gpio_set_scl(chip, 0);
		i2c_gpio_delay(chip, 0x32);
		byte = (uint8_t)(byte << 1);
	}
	return i2c_gpio_ack_or_nack(chip);
}

/* i2c_gpio_read_byte - FUN_c0001048, @0xc0001048. Releases SDA (dir=input),
 * clocks in 8 bits MSB-first, then retakes SDA (dir=output). */
static uint8_t i2c_gpio_read_byte(void *chip)
{
	uint32_t byte = 0;
	int i;

	i2c_gpio_set_sda_dir(chip, 1);
	for (i = 7; i >= 0; i--) {
		i2c_gpio_set_scl(chip, 1);
		i2c_gpio_delay(chip, 0x32);
		byte = ((byte & 0x7f) << 1) | (i2c_gpio_sda_read(chip) & 0xff);
		i2c_gpio_set_scl(chip, 0);
		i2c_gpio_delay(chip, 0x32);
	}
	i2c_gpio_set_sda_dir(chip, 0);
	return (uint8_t)byte;
}

/* i2c_gpio_ack_or_nack - FUN_c0000fdc, @0xc0000fdc. Release SDA, pulse SCL
 * once, sample. Returns 1 if SDA sampled high (NACK), 0 if low (ACK). */
static int i2c_gpio_ack_or_nack(void *chip)
{
	int bit;

	i2c_gpio_set_sda_dir(chip, 1);
	i2c_gpio_set_scl(chip, 1);
	i2c_gpio_delay(chip, 0x32);
	bit = i2c_gpio_sda_read(chip);
	if (bit != 0)
		bit = 1;
	i2c_gpio_set_scl(chip, 0);
	i2c_gpio_delay(chip, 0x32);
	i2c_gpio_set_sda_dir(chip, 0);
	return bit;
}

/* i2c_gpio_master_ack - FUN_c0000f84, @0xc0000f84 (K1: FUN_c0001204). Drive
 * SDA low then pulse SCL once - the master generating its own ACK between
 * successive bytes of a multi-byte read. */
static void i2c_gpio_master_ack(void *chip)
{
	i2c_gpio_set_sda(chip, 0);
	i2c_gpio_delay(chip, 0x32);
	i2c_gpio_set_scl(chip, 1);
	i2c_gpio_delay(chip, 0x32);
	i2c_gpio_set_scl(chip, 0);
	i2c_gpio_delay(chip, 0x32);
}

/* ------------------------------------------------------------------------- *
 *  i2c_gpio_frame_command - FUN_c0001308, @0xc0001308 (K1: FUN_c0001588).
 *  Sends the 4-byte {addr, cmd, arg1, arg2} header.
 *
 *  RETRY BOUND independently re-derived for K2: the constant at 0xc00013b4 =
 *  0x4e1f = 19999 decimal - IDENTICAL to K1's DAT_c0001634 value. Same
 *  "no STOP anywhere in the retry loop, only a 1-unit delay(chip,1) between
 *  attempts" shape as K1 (the delay is a no-op regardless per i2c_gpio_delay
 *  above, but note K2's own decompile shows this specific call site
 *  (`FUN_c0000f40(param_1);`) with the `1` literal itself not visible in the
 *  decompiled text - only one argument is shown at this particular call
 *  site, vs the explicit `,1000`/`,0x32` literals visible elsewhere in this
 *  file. Harmless either way since i2c_gpio_delay ignores both its
 *  arguments; the `1` below is kept for documentation continuity with K1's
 *  own citation of this same retry-loop delay).
 * ------------------------------------------------------------------------- */
#define I2C_GPIO_FRAME_ADDR_RETRY_LIMIT	19999	/* 0xc00013b4 = 0x4e1f, confirmed identical to K1 */

static int i2c_gpio_frame_command(void *chip, uint8_t addr, uint8_t cmd,
				  uint8_t arg1, uint8_t arg2)
{
	int retries = 0;

	for (;;) {
		i2c_gpio_start(chip);
		if (i2c_gpio_write_byte(chip, addr) == 0)
			break;	/* address ACKed */
		retries++;
		i2c_gpio_delay(chip, 1);
		if (retries > I2C_GPIO_FRAME_ADDR_RETRY_LIMIT)
			return 0;
	}
	if (i2c_gpio_write_byte(chip, cmd) != 0)
		return 0;
	if (i2c_gpio_write_byte(chip, arg1) != 0)
		return 0;
	if (i2c_gpio_write_byte(chip, arg2) != 0)
		return 0;
	return 1;
}

/* ------------------------------------------------------------------------- *
 *  i2c_gpio_addr_start - FUN_c00014f8, @0xc00014f8 (K1: FUN_c0001778). The
 *  CDIX-side "write address byte + register byte" combo: START, write
 *  addr(+W), retry up to 5 times on address NACK, then write the register
 *  byte once ACKed. Retry bound (5) re-confirmed identical to K1 - a real,
 *  confirmed asymmetry vs frame_command's 19999-attempt bound, not a
 *  transcription inconsistency, on both boards.
 * ------------------------------------------------------------------------- */
static int i2c_gpio_addr_start(void *chip, uint8_t addr_byte, uint8_t reg)
{
	int retries = 0;

	do {
		i2c_gpio_start(chip);
		if (i2c_gpio_write_byte(chip, addr_byte) == 0) {
			/* address ACKed - send the register byte */
			return (i2c_gpio_write_byte(chip, reg) == 0) ? 1 : 0;
		}
		retries++;
		i2c_gpio_delay(chip, 1);
	} while (retries < 5);
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  i2c_gpio_write_block / i2c_gpio_read_block - FUN_c0001468 / FUN_c00013b8
 *  (K1: FUN_c00016e8 / FUN_c0001638) - these are what K1's crypto_at88.c
 *  called at88_i2c_write / at88_i2c_read. Defined ONLY here per this
 *  migration pass's file-split rule (see top-of-file FILE-SPLIT NOTE);
 *  crypto_at88.c's crypto_at88_write/crypto_at88_read call these via
 *  `extern`.
 *
 *  SIGNATURE CORRECTED vs K1's own i2c_by_gpio.c (see top-of-file note): 5
 *  params after `chip` (cmd, arg1, arg2, len, data), NOT 6 - `cmd` doubles as
 *  frame_command's address-byte slot (the AT88 opcode 0xb0/0xb2/0xb4/0xb8 IS
 *  the "address" byte per the AT88SC datasheet's own terminology, matching
 *  K1's crypto_at88.c naming choice), and `len` (truncated to uint8_t) is
 *  frame_command's 4th header byte - both independently re-confirmed against
 *  K2's own decompile (`FUN_c0001308(param_1,param_2,param_3,param_4,
 *  param_5)` with param_5 = the caller's own length parameter).
 * ------------------------------------------------------------------------- */

/* i2c_gpio_write_block - FUN_c0001468, @0xc0001468 (= crypto_at88's write).
 * Busy-guard, frame the 4-byte header (cmd/arg1/arg2/len), then write `len`
 * data bytes, aborting on the first NACK. Always STOPs and clears the busy
 * guard. */
int i2c_gpio_write_block(void *chip, uint8_t cmd, uint8_t arg1, uint8_t arg2,
			 int len, const uint8_t *data)
{
	int ok;
	int i;

	i2c_gpio_busy_guard_enter();
	ok = i2c_gpio_frame_command(chip, cmd, arg1, arg2, (uint8_t)len);
	if (ok) {
		for (i = 0; i < len; i++) {
			if (i2c_gpio_write_byte(chip, data[i]) != 0) {
				ok = 0;
				break;
			}
		}
	}
	i2c_gpio_stop(chip);
	i2c_gpio_busy_guard_exit();
	return ok;
}

/* i2c_gpio_read_block - FUN_c00013b8, @0xc00013b8 (= crypto_at88's read).
 * Busy-guard, frame the 4-byte header, then read `len` bytes, master-ACKing
 * every byte except the last, then a final i2c_gpio_ack_or_nack sanity check
 * that must sample NACK (1) for the transfer to be reported successful.
 * Always STOPs and clears the busy guard. */
int i2c_gpio_read_block(void *chip, uint8_t cmd, uint8_t arg1, uint8_t arg2,
			int len, uint8_t *dest)
{
	int ok;
	int i;

	i2c_gpio_busy_guard_enter();
	ok = i2c_gpio_frame_command(chip, cmd, arg1, arg2, (uint8_t)len);
	if (ok) {
		for (i = 0; i < len; i++) {
			dest[i] = i2c_gpio_read_byte(chip);
			if (i + 1 < len)
				i2c_gpio_master_ack(chip);
		}
		if (i2c_gpio_ack_or_nack(chip) != 1)
			ok = 0;
	}
	i2c_gpio_stop(chip);
	i2c_gpio_busy_guard_exit();
	return ok;
}

/* ------------------------------------------------------------------------- *
 *  i2c_gpio_write_reg8 / i2c_gpio_read_reg8 - FUN_c00015f4 / FUN_c000156c
 *  (K1: FUN_c0001874 / FUN_c00017ec) - cdix4192.c's own cdix_i2c_write_reg /
 *  cdix_i2c_read_reg (same addresses), the generic "{7-bit addr, register,
 *  value}" tier alongside the AT88-specific 4-byte-header tier above.
 * ------------------------------------------------------------------------- */

/* i2c_gpio_write_reg8 - FUN_c00015f4, @0xc00015f4 (= cdix_i2c_write_reg).
 * addr7 is a 7-bit I2C device address. Returns 1 on success, 0 on any NACK. */
int i2c_gpio_write_reg8(void *chip, uint8_t addr7, uint8_t reg, uint8_t value)
{
	int ok = 0;

	i2c_gpio_busy_guard_enter();
	if (i2c_gpio_addr_start(chip, (uint8_t)((addr7 & 0x7f) << 1), reg)) {
		if (i2c_gpio_write_byte(chip, value) == 0)
			ok = 1;
	}
	i2c_gpio_stop(chip);
	i2c_gpio_busy_guard_exit();
	return ok;
}

/* i2c_gpio_read_reg8 - FUN_c000156c, @0xc000156c (= cdix_i2c_read_reg).
 *
 * CONFIRMED IDENTICAL K2 QUIRK: this function's return value is ALWAYS 0,
 * exactly like K1 - the real decompiled body never propagates any internal
 * ACK/NACK check into the returned value (`return 0;` unconditional, see
 * K2's own FUN_c000156c body). `*out` is only actually written if every
 * phase up to the data read ACKed. Preserved exactly as observed. */
int i2c_gpio_read_reg8(void *chip, uint8_t addr7, uint8_t reg, uint8_t *out)
{
	uint32_t addr_byte = (uint32_t)(addr7 & 0xff) << 1;

	i2c_gpio_busy_guard_enter();
	if (i2c_gpio_addr_start(chip, (uint8_t)(addr_byte & 0xfe), reg)) {
		i2c_gpio_start(chip);	/* repeated START */
		if (i2c_gpio_write_byte(chip, (uint8_t)(addr_byte | 1)) == 0) {
			*out = i2c_gpio_read_byte(chip);
			i2c_gpio_ack_or_nack(chip);	/* result unused, matches real code */
		}
	}
	i2c_gpio_stop(chip);
	i2c_gpio_busy_guard_exit();
	return 0;	/* confirmed: always 0, see comment above */
}

/* ------------------------------------------------------------------------- *
 *  i2c_gpio_bus_reset - FUN_c000127c, @0xc000127c (K1: FUN_c00014fc). A
 *  9-clock-pulse bus recovery/init sequence: busy-guard, SDA=1/SCL=1,
 *  delay(1000), 7 SCL toggle pulses (delay 0x32 each edge), delay(1000),
 *  busy-guard-exit. Identical shape to K1.
 *
 *  Its ONLY caller in this K2 image is FUN_c0000c48 - crypto_at88.c's own
 *  queue-handle init (K1: FUN_c0000ec8, K2: FUN_c0000c48), which zeroes the
 *  count/read-index/write-index fields at handle+0x40/0x41/0x42 exactly like
 *  K1's version, then calls this function. As in K1, that init function
 *  itself is NOT reconstructed as a standalone body in either output file
 *  here (out of scope - lives in eva_board_main.c/cobjectmgr.c territory,
 *  only cited by address, matching K1's own convention); its one confirmed
 *  caller is FUN_c0009838 (EvaBoardMain-equivalent board bring-up, K1's
 *  FUN_c00074bc).
 *
 *  CONFIRMED separately: FUN_c0000c48 passes a local, uninitialized 4-byte
 *  stack buffer as this function's `chip` argument, not a real handle - same
 *  "dead chip argument" pattern as K1, harmless given the whole GPIO layer
 *  ignores `chip` unconditionally.
 * ------------------------------------------------------------------------- */
void i2c_gpio_bus_reset(void *chip)
{
	int i;

	i2c_gpio_busy_guard_enter();
	i2c_gpio_set_sda(chip, 1);
	i2c_gpio_set_scl(chip, 1);
	i2c_gpio_delay(chip, 1000);
	for (i = 6; i >= 0; i--) {
		i2c_gpio_set_scl(chip, 0);
		i2c_gpio_delay(chip, 0x32);
		i2c_gpio_set_scl(chip, 1);
		i2c_gpio_delay(chip, 0x32);
	}
	i2c_gpio_delay(chip, 1000);
	i2c_gpio_busy_guard_exit();
}
