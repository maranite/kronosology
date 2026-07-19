/* SPDX-License-Identifier: GPL-2.0 */
/*
 * i2c_by_gpio.c - the NKS4 panel firmware's shared GPIO bit-bang I2C bus
 * driver ("../I2cByGpio.cpp"). Both crypto_at88.c (the AT88SC/NV2AC security
 * chip) and cdix4192.c (the DIX4192-family AES3/S/PDIF transceiver) build on
 * this single physical bus - this file is the ground-truth reconstruction of
 * that shared layer, previously only cross-referenced (not itself
 * reconstructed) from both of those files.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB (TI OMAP-L1x,
 * ARM926EJ-S, loaded at 0xC0000000), 2026-07-18, read from the pre-fetched
 * dump (all_decompiled.json/all_data.json), not the live Ghidra bridge.
 * Original addresses given per function so this stays checkable against a
 * fresh decompile.
 *
 * ANCHOR: the literal string "../I2cByGpio.cpp" lives at 0xc0022cf8. It has
 * no direct static xref from any decompiled function body - it's reached
 * indirectly through DAT_c00014e4, a literal-pool constant equal to
 * 0xc0022cf8 that i2c_gpio_busy_guard_enter() (FUN_c00014ac, below) passes
 * as the `file` argument to the shared assert/fault handler on a
 * bus-already-busy condition, at source line 0xbb (187) - i.e. this is the
 * ONE assert site in the whole file, and it is what tags this entire
 * function cluster as "../I2cByGpio.cpp" with certainty (same single-assert-
 * site anchor pattern already used for CryptoAt88.cpp/CDix4192.cpp/
 * clcdc.cpp/ctouchpanel.cpp/OmapL137Mcasp.cpp elsewhere in this project).
 *
 * SCOPE NOTE: this file covers the dense, contiguous function cluster
 * 0xc0001148-0xc0001874 (all-I2C-specific: START/STOP, byte I/O, ACK/NACK,
 * command framing, block transfer, register transfer). Two address-adjacent
 * functions were deliberately left OUT and are declared `extern` instead:
 *
 *   - FUN_c0001990 (GPIO-bank base-pointer getter) has 66 callers spanning
 *     the entire firmware image, far beyond this cluster - it is a
 *     firmware-wide generic GPIO accessor, not private to I2cByGpio.cpp,
 *     despite living at a nearby address.
 *   - FUN_c0001aa0 (hardware free-running-timer busy-wait engine) has 16
 *     callers spanning clcdc/cpsoc/touch-panel/etc dispatch code, also
 *     clearly firmware-wide generic, not I2C-specific.
 *
 * Conversely, two functions that ARE I2C-cluster-local by caller count
 * (FUN_c00025ac/FUN_c00025e4, each with exactly one caller, both inside
 * this cluster) physically live in a DIFFERENT, distant address range
 * (0xc00025xx) alongside a large family of unrelated LED/analog GPIO-bank
 * pokes (FUN_c00022d0/FUN_c00022e0/FUN_c0002238, part of cpsoc.c's already-
 * documented register cluster) - so despite being I2C-only IN PRACTICE,
 * they are declared extern here too, on the address-locality evidence that
 * they belong to that other, generic GPIO-bank-register file instead.
 * Noted as an honest observation, not a firm claim about the real Korg
 * source-file boundaries.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Out-of-scope dependencies - generic, firmware-wide primitives this file
 *  calls into but does not itself define. See SCOPE NOTE above.
 * ------------------------------------------------------------------------- */
extern void *gpio_bank_get_base(void);				/* FUN_c0001990 - fixed return DAT_c0001998 = 0x01E26000, a real TI OMAP-L138/DA850 GPIO-bank register base address; 66 callers firmware-wide */
extern void  hw_timer_busy_wait(void *timer_base, int units);	/* FUN_c0001aa0 - real free-running-timer spin loop (reads a 32-bit counter via *(base+0x10), handles wraparound against a fixed period constant DAT_c0001b28=150001, accumulates elapsed "units" until >= target); 16 callers firmware-wide, not I2C-specific */
extern void  crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000919c, defined in crypto_at88.c - the shared assert/fault handler, 51 callers firmware-wide despite the name */

/* SDA-line-only helpers that physically live in the OTHER, generic
 * GPIO-bank-register file (see SCOPE NOTE) rather than in this cluster. */
extern void gpio_bank_set_dir_bit(void *base, int input);	/* FUN_c00025ac - toggles bit 0x40000 (SDA, GPIO bank-0 bit 18) in the DIR register at base+0x10; 0=clear bit=output, nonzero=set bit=input - matches the TI convention (DIR bit 0 = output) */
extern int  gpio_bank_read_sda_bit(void *base);		/* FUN_c00025e4 - reads the bank's IN_DATA-style register via FUN_c0002238(base,0) and extracts bit 18 (0x40000, the SDA line) */

/* ------------------------------------------------------------------------- *
 *  Bus-busy reentrancy guard - a single global flag byte (the same address
 *  read by busy_guard_enter and cleared by busy_guard_exit: DAT_c00014e0 ==
 *  DAT_c00014f8, both literal-pool-relocated to the identical global). Every
 *  transaction-level entry point below (write_block, read_block, write_reg8,
 *  read_reg8, bus_reset) brackets its work with enter/exit. Confirmed real
 *  hard-fault on re-entry, not just a comment: FUN_c000919c's own caller
 *  list includes a CONDITIONAL_CALL from this exact function at 0xc00014d0.
 * ------------------------------------------------------------------------- */

extern uint8_t i2c_gpio_busy_flag;	/* DAT_c00014e0/DAT_c00014f8, real address not extracted this pass - the same global read by both functions below */

/* i2c_gpio_busy_guard_enter - FUN_c00014ac, @0xc00014ac. Faults (never
 * returns) if the bus is already marked busy, citing "../I2cByGpio.cpp"
 * line 0xbb (187) - this IS this file's one real assert/anchor site. */
static void i2c_gpio_busy_guard_enter(void)
{
	if (i2c_gpio_busy_flag != 0)
		crypto_at88_fault(0, "../I2cByGpio.cpp", 0xbb);
	i2c_gpio_busy_flag = 1;
}

/* i2c_gpio_busy_guard_exit - FUN_c00014e8, @0xc00014e8. */
static void i2c_gpio_busy_guard_exit(void)
{
	i2c_gpio_busy_flag = 0;
}

/* ------------------------------------------------------------------------- *
 *  i2c_gpio_delay - FUN_c00011c0, @0xc00011c0. Every call site in this file
 *  passes a chip/context first argument (dead - see below) and a units
 *  value that varies per site (0x32, 0x14, 1, 1000, ...). The decompiled
 *  body shows only a single-argument tail call `FUN_c0001aa0(DAT_c00011c8)`
 *  - the same gap already flagged in crypto_at88.c's own at88_delay comment
 *  ("real param wiring to FUN_c0001aa0(DAT_c00011c8) not fully resolved").
 *  Re-examined this pass: DAT_c00011c8 is itself a relocated literal-pool
 *  pointer equal to the SAME fixed address (0xc00e0068) that every other
 *  "chip"-shaped argument in this file's own literal pools resolves to
 *  (DAT_c000116c/c0001194/c00011bc/c0001200 are all the identical value) -
 *  i.e. it is almost certainly the same dead per-instance pointer discussed
 *  below, not a genuine hardware timer base, which argues AGAINST reading
 *  it as "the real param_1 for FUN_c0001aa0." The most consistent
 *  reconstruction given AAPCS r0/r1 argument passing and FUN_c0001aa0's own
 *  confirmed two-argument signature is that `units` (this function's own
 *  r1) survives untouched into the tail call as FUN_c0001aa0's param_2,
 *  while DAT_c00011c8 supplies param_1 - but this is NOT independently
 *  confirmed by register-level disassembly this pass. Left honestly as a
 *  faithful reconstruction of confirmed behavior (every call site's
 *  observably-different real-world delay length requires units to reach
 *  the timer engine somehow) rather than a fully proven wiring.
 * ------------------------------------------------------------------------- */
extern void *i2c_gpio_delay_timer_base;	/* DAT_c00011c8 and equivalents - real target address not extracted this pass, see comment above */

static void i2c_gpio_delay(void *chip, int units)
{
	(void)chip;	/* dead at every leaf of this driver - see i2c_gpio_set_scl/set_sda below */
	hw_timer_busy_wait(i2c_gpio_delay_timer_base, units);
}

/* ------------------------------------------------------------------------- *
 *  SCL/SDA level + direction control - FUN_c0001148/FUN_c0001170/FUN_c00011cc.
 *
 *  CONFIRMED (this pass): `chip` (param_1) is declared in all three real
 *  functions but is READ BY NEITHER FUN_c0001148 NOR FUN_c0001170 - both
 *  bodies call gpio_bank_get_base() (FUN_c0001990, its own zero-real-
 *  parameter fixed-global getter) and ignore their own param_1 entirely.
 *  This is the bottom of a "chip" parameter that is threaded faithfully
 *  through every layer ABOVE this point in this file, but dead-ends here -
 *  the whole bus is a hardware singleton: one fixed GPIO bank (base
 *  0x01E26000, DAT_c0001998, a real TI OMAP-L138/DA850 GPIO controller
 *  address), SCL = bank-0 bit 19 (mask 0x80000), SDA = bank-0 bit 18
 *  (mask 0x40000).
 *
 *  Separately (see crypto_at88.c/cdix4192.c and the note in
 *  i2c_gpio_write_block/read_block/write_reg8/read_reg8 below), at least
 *  four distinct TOP-level callers outside this file (crypto_at88's write/
 *  read wrappers, its queue-init/bus-reset call, and cdix4192.c's own
 *  cdix_reg_write/cdix_reg_read - the latter pair already documented that
 *  file's own "chip not actually forwarded" correction) pass an
 *  uninitialized local stack buffer as this same argument rather than their
 *  own real incoming handle. So the parameter is dead both at its source
 *  (nothing meaningful is ever put into it) and at its sink (nothing here
 *  ever reads it) - it survives only as plumbing through this file's own
 *  internal call chain in between.
 * ------------------------------------------------------------------------- */

#define I2C_GPIO_SCL_MASK	0x80000u	/* bank-0 bit 19 */
#define I2C_GPIO_SDA_MASK	0x40000u	/* bank-0 bit 18 */

extern void gpio_bank_write_set(void *base, int bank, uint32_t mask);	/* FUN_c00022d0 - offset+0x18 SET-style register write; out of scope, generic GPIO-bank file */
extern void gpio_bank_write_clr(void *base, int bank, uint32_t mask);	/* FUN_c00022e0 - offset+0x1c CLR-style register write; out of scope, generic GPIO-bank file */

/* i2c_gpio_set_scl - FUN_c0001148, @0xc0001148. */
static void i2c_gpio_set_scl(void *chip, int level)
{
	(void)chip;	/* confirmed unused in the real function */
	void *base = gpio_bank_get_base();

	if (level != 0)
		gpio_bank_write_set(base, 0, I2C_GPIO_SCL_MASK);
	else
		gpio_bank_write_clr(base, 0, I2C_GPIO_SCL_MASK);
}

/* i2c_gpio_set_sda - FUN_c0001170, @0xc0001170. */
static void i2c_gpio_set_sda(void *chip, int level)
{
	(void)chip;	/* confirmed unused in the real function */
	void *base = gpio_bank_get_base();

	if (level != 0)
		gpio_bank_write_set(base, 0, I2C_GPIO_SDA_MASK);
	else
		gpio_bank_write_clr(base, 0, I2C_GPIO_SDA_MASK);
}

/* i2c_gpio_set_sda_dir - FUN_c00011cc, @0xc00011cc. Unlike set_scl/set_sda,
 * this one DOES use its chip argument (forwarded into gpio_bank_set_dir_bit
 * as the base, per FUN_c00011cc's real body: `FUN_c00025ac(uVar1,param_2)`
 * where uVar1 = gpio_bank_get_base() result again - so param_1/chip is
 * STILL unused; uVar1 is the getter's own return value, not chip). Also
 * settles crypto_at88.c's "0=output/1=input inferred, not confirmed" note:
 * CONFIRMED this pass - dir=0 clears the DIR bit (output, TI convention),
 * dir!=0 sets it (input). Followed by a fixed 20-unit delay (0x14) not
 * previously documented anywhere in this project. */
static void i2c_gpio_set_sda_dir(void *chip, int input)
{
	void *base = gpio_bank_get_base();

	gpio_bank_set_dir_bit(base, input);
	i2c_gpio_delay(chip, 0x14);
}

/* i2c_gpio_sda_read - FUN_c0001198, @0xc0001198. Raw SDA line level (0/1),
 * normalized to exactly 0 or 1. Two callers, both in this file:
 * i2c_gpio_ack_or_nack (below) and i2c_gpio_read_byte (below). */
static int i2c_gpio_sda_read(void *chip)
{
	(void)chip;
	void *base = gpio_bank_get_base();
	int bit = gpio_bank_read_sda_bit(base);

	return (bit != 0) ? 1 : 0;
}

/* ------------------------------------------------------------------------- *
 *  i2c_gpio_start / i2c_gpio_stop - FUN_c000143c / FUN_c00013cc.
 *
 *  i2c_gpio_stop RESOLVES crypto_at88.c's open item: that file declared
 *  `extern void at88_i2c_stop(void *chip)` with "real name/address not yet
 *  resolved." It is FUN_c00013cc, @0xc00013cc - confirmed by its call
 *  pattern (SDA low -> SCL low -> SDA-dir output -> SCL high -> SDA rises
 *  while SCL still high, the textbook STOP edge) and by being the function
 *  every transaction in this file (write_block, read_block, write_reg8,
 *  read_reg8) calls exactly once, right before clearing the busy guard.
 * ------------------------------------------------------------------------- */

/* i2c_gpio_start - FUN_c000143c, @0xc000143c. SDA high, SCL high, then SDA
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

/* i2c_gpio_stop - FUN_c00013cc, @0xc00013cc. */
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
 *  FUN_c000134c / FUN_c00012c8 / FUN_c000125c.
 *
 *  i2c_gpio_read_byte is a NEW addition to this project's naming - it was
 *  previously only present as an unnamed internal step inside at88_i2c_read
 *  (FUN_c0001638) and cdix_i2c_read_reg (FUN_c00017ec)'s decompiles, never
 *  itself named/extracted.
 *
 *  i2c_gpio_ack_or_nack (FUN_c000125c) is used TWICE for two different
 *  purposes with identical mechanics - as the ACK sample after a master
 *  WRITE (release SDA, clock once, sample: 0=ACK/low, 1=NACK/high - the
 *  usual slave-drives-ACK case), and again after the LAST byte of a master
 *  READ (i2c_gpio_read_block, below) where the master itself never
 *  explicitly drives SDA low - it just releases the line (dir=input) and
 *  clocks once, relying on the bus pull-up to produce a NACK-shaped '1'
 *  sample, which the caller then verifies. Documented as observed, not
 *  asserted as "the" canonical NACK-generation idiom.
 * ------------------------------------------------------------------------- */

static int i2c_gpio_ack_or_nack(void *chip);	/* forward decl - defined below, mirrors real function order (FUN_c000125c comes after FUN_c000134c in the binary but write_byte calls it) */

/* i2c_gpio_write_byte - FUN_c000134c, @0xc000134c. Write one byte MSB-first
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

/* i2c_gpio_read_byte - FUN_c00012c8, @0xc00012c8. Releases SDA (dir=input),
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

/* i2c_gpio_ack_or_nack - FUN_c000125c, @0xc000125c. Release SDA, pulse SCL
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

/* i2c_gpio_master_ack - FUN_c0001204, @0xc0001204. Drive SDA low then pulse
 * SCL once - the master generating its own ACK between successive bytes of
 * a multi-byte read (i2c_gpio_read_block, below, calls this after every
 * byte except the last). */
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
 *  i2c_gpio_frame_command - FUN_c0001588, @0xc0001588. Sends the 4-byte
 *  {addr, cmd, arg1, arg2} header used by the AT88-side API
 *  (at88_frame_command in crypto_at88.c is THIS function - same address).
 *
 *  CONFIRMED this pass, resolving two of crypto_at88.c's own open items:
 *   - DAT_c0001634 (the address-ACK retry bound) = 0x4e1f = 19999. Real
 *     retry loop: START, write address byte; if NACKed, delay(chip, 1)
 *     (the "leftover register value of 1" crypto_at88.c's own comment
 *     already flagged - confirmed here as a literal, deliberate 1-unit
 *     delay, not a decompiler artifact) and retry, up to 19999 attempts,
 *     with NO STOP in between retries (crypto_at88.c's existing correction
 *     already got this right).
 *   - at88_i2c_stop's real address is FUN_c00013cc (see above) - but note
 *     frame_command itself never calls stop; that only happens at the
 *     transaction level (write_block/read_block below), matching
 *     crypto_at88.c's existing framing.
 * ------------------------------------------------------------------------- */
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
		if (retries > 0x4e1f)
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
 *  i2c_gpio_addr_start - FUN_c0001778, @0xc0001778. The CDIX-side "write
 *  address byte + register byte" combo: START, write addr(+W), retry up to
 *  5 times on address NACK (delay(chip,1) between retries, same idiom as
 *  frame_command above but a MUCH smaller bound - 5 vs 19999, a genuine,
 *  real asymmetry between the two devices' post-write poll behavior, not a
 *  transcription inconsistency), then write the register byte once ACKed.
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
 *  i2c_gpio_write_block / i2c_gpio_read_block - FUN_c00016e8 / FUN_c0001638.
 *  These ARE at88_i2c_write / at88_i2c_read from crypto_at88.c (same
 *  addresses) - crypto_at88.c declared them extern with the data-phase
 *  logic "not independently re-derived...this pass"; that data phase is
 *  now fully reconstructed here.
 * ------------------------------------------------------------------------- */

/* i2c_gpio_write_block - FUN_c00016e8, @0xc00016e8 (= at88_i2c_write).
 * Busy-guard, frame the 4-byte header, then write `len` data bytes,
 * aborting on the first NACK. Always STOPs and clears the busy guard. */
int i2c_gpio_write_block(void *chip, uint8_t addr, uint8_t cmd, uint8_t arg1,
			 uint8_t arg2, int len, const uint8_t *data)
{
	int ok = 1;
	int i;

	i2c_gpio_busy_guard_enter();
	if (i2c_gpio_frame_command(chip, addr, cmd, arg1, arg2)) {
		for (i = 0; i < len; i++) {
			if (i2c_gpio_write_byte(chip, data[i]) != 0) {
				ok = 0;
				break;
			}
		}
	} else {
		ok = 0;
	}
	i2c_gpio_stop(chip);
	i2c_gpio_busy_guard_exit();
	return ok;
}

/* i2c_gpio_read_block - FUN_c0001638, @0xc0001638 (= at88_i2c_read).
 * Busy-guard, frame the 4-byte header, then read `len` bytes, master-ACKing
 * every byte except the last, then a final i2c_gpio_ack_or_nack sanity
 * check (see that function's own comment) that must sample NACK (1) for
 * the transfer to be reported successful. Always STOPs and clears the busy
 * guard. */
int i2c_gpio_read_block(void *chip, uint8_t addr, uint8_t cmd, uint8_t arg1,
			uint8_t arg2, int len, uint8_t *dest)
{
	int ok;
	int i;

	i2c_gpio_busy_guard_enter();
	ok = i2c_gpio_frame_command(chip, addr, cmd, arg1, arg2);
	if (ok) {
		for (i = 0; i < len; i++) {
			dest[i] = i2c_gpio_read_byte(chip);
			if (i < len - 1)
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
 *  i2c_gpio_write_reg8 / i2c_gpio_read_reg8 - FUN_c0001874 / FUN_c00017ec.
 *  These are cdix4192.c's own cdix_i2c_write_reg / cdix_i2c_read_reg (same
 *  addresses) - the generic "{7-bit addr, register, value}" tier this file
 *  offers alongside the AT88-specific 4-byte-header tier above. Fixed 8-bit
 *  device address handling: write masks the incoming 7-bit addr with
 *  `& 0x7f` before `<< 1`; read masks with `& 0xff` before `<< 1` then
 *  `& 0xfe` after - functionally identical for any valid 7-bit input, a
 *  faithfully-preserved asymmetry in the real source, not a bug.
 * ------------------------------------------------------------------------- */

/* i2c_gpio_write_reg8 - FUN_c0001874, @0xc0001874 (= cdix_i2c_write_reg).
 * addr7 is a 7-bit I2C device address (e.g. CDIX's fixed 0x70). Returns
 * 1 on success, 0 on any NACK. */
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

/* i2c_gpio_read_reg8 - FUN_c00017ec, @0xc00017ec (= cdix_i2c_read_reg).
 *
 * CONFIRMED real firmware quirk, worth flagging for cdix4192.c's own docs:
 * this function's return value is ALWAYS 0, regardless of whether the
 * address, register, or repeated-start read-address phase actually
 * succeeded - the real decompiled body never propagates any of its
 * internal ACK/NACK checks into the returned undefined4. `*out` is only
 * actually written if every phase up to the data read ACKed; on any
 * earlier failure it is left untouched by this function. Preserved exactly
 * as observed rather than "fixed" - cdix_reg_read (cdix4192.c) already
 * doesn't check this return value, consistent with it being meaningless in
 * the real firmware. */
int i2c_gpio_read_reg8(void *chip, uint8_t addr7, uint8_t reg, uint8_t *out)
{
	uint8_t addr_byte = (uint8_t)((addr7 & 0xff) << 1);

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
 *  i2c_gpio_bus_reset - FUN_c00014fc, @0xc00014fc. A 9-clock-pulse bus
 *  recovery/init sequence: busy-guard, SDA=1/SCL=1, delay(1000), 7 SCL
 *  toggle pulses (delay 0x32 each edge), delay(1000), busy-guard-exit. The
 *  classic "clock out a stuck slave" I2C bus-clear idiom (up to 9 clocks
 *  frees a slave holding SDA low), here fixed at 7 pulses plus the
 *  preceding START-shaped edge = 8 total, close to but not exactly the
 *  textbook 9.
 *
 *  Its ONLY caller in the whole image is FUN_c0000ec8 - crypto_at88.c's own
 *  queue-handle init (the function that zeroes the count/read-index fields
 *  crypto_at88.c already documents at handle+0x40/0x41/0x42). This was not
 *  previously modeled in crypto_at88.c at all; flagged as a follow-up
 *  recommendation in this project's own tracking, not added there directly
 *  per this pass's scope (crypto_at88.c was concurrently owned by another
 *  agent).
 *
 *  CONFIRMED separately (same call site): FUN_c0000ec8 passes a local,
 *  uninitialized 4-byte stack buffer as this function's `chip` argument,
 *  not its own real queue_handle parameter - the SAME "dead chip argument"
 *  pattern independently confirmed at three other top-level call sites in
 *  this project (crypto_at88_write/crypto_at88_read's FUN_c0000ef4/
 *  FUN_c0000f30, and cdix4192.c's cdix_reg_write/cdix_reg_read). Given four
 *  independent top-level occurrences of the identical pattern, this is a
 *  systemic real-firmware characteristic, not an isolated cdix4192.c
 *  quirk: the "chip" parameter this driver threads through every one of
 *  its own internal calls is functionally dead end-to-end - nothing
 *  upstream ever supplies a real value, and nothing downstream (see
 *  i2c_gpio_set_scl/set_sda above) ever reads it. The whole bus is
 *  addressed purely through the fixed GPIO base (0x01E26000) and the I2C
 *  address byte argument, not any software context object.
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
