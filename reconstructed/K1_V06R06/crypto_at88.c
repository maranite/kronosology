/* SPDX-License-Identifier: GPL-2.0 */
/*
 * crypto_at88.c - the NKS4 panel firmware's own direct path to the AT88SC/NV2AC
 * security chip, independent of the host's OA.ko/OmapNKS4Module.ko/GetPubIdMod.ko.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB (TI OMAP-L1x, ARM926EJ-S,
 * loaded at 0xC0000000), 2026-07-17/2026-07-18. See this project's own README.md for
 * the full per-function citation list and open items. Original addresses given per
 * function so this stays checkable against a fresh decompile.
 *
 * Two independent callers of the same write(FUN_c0000ef4)/read(FUN_c0000f30) pair were
 * found and are both reconstructed here: a one-shot factory self-test
 * (crypto_at88_self_test, @0xc0001028) and a runtime queued-command relay
 * (crypto_at88_process_queue, @0xc0005e9c) whose read results get forwarded to the host
 * as a genuine AtmelRead (0xe1) event - the same opcode this repo's host-side docs
 * (kronosology/reconstructed/OmapNKS4Module/driver.cpp's ReceiveEventBuffer,
 * KronosNKS4/docs/protocol.md) already decode on the receiving end. Confirming this
 * event's construction from the *sending* side, independently, is new - see README.md.
 *
 * CLOSURE PASS (2026-07-18): every item this file's own README.md status section had
 * left "still genuinely open" is now resolved (queue producer, bit-13 status setter,
 * at88_frame_command's real retry bound, the I2C data-phase bodies, and the self-test's
 * true zero-caller status). See each function's own comment below for the citation.
 * The single biggest structural finding this pass: the ENTIRE I2C bit-bang layer's
 * `chip`/`void *` handle argument, threaded through every function in this file, is
 * dead - there is exactly one hardwired SDA/SCL GPIO pin pair (bit 0x40000/0x80000 off
 * two fixed GPIO-object globals, DAT_c0001194/DAT_c000116c), and at88_gpio_set_sda/
 * set_scl/set_sda_dir/at88_delay all ignore whatever pointer they're handed and drive
 * that same fixed pair unconditionally. This is the same root cause behind the
 * "phantom forwarded parameter" pattern the re-verification pass already found
 * independently in cdix4192.c and eva_board_main.c - all three are downstream
 * consumers of this one dead-argument layer, not three unrelated bugs.
 */

#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------------- *
 *  Lower-level I2C bit-bang primitives - fully traced, 2026-07-17/2026-07-18.
 *  @0xc0001588 (frame a 4-byte command), 0xc000143c (START condition),
 *  0xc000134c (write one byte + read its ACK bit), 0xc00011c0 (delay).
 *
 *  CORRECTION (2026-07-18): `chip` is a dead parameter throughout this entire
 *  layer. at88_gpio_set_sda/set_scl (FUN_c0001170/FUN_c0001148) both discard
 *  their own `chip` argument and unconditionally drive one hardwired GPIO bit
 *  each (0x40000 / 0x80000) via two fixed GPIO-object globals
 *  (DAT_c0001194/DAT_c000116c respectively) - confirmed by reading their real
 *  bodies, which never reference param_1 at all. at88_delay (FUN_c00011c0)
 *  likewise ignores BOTH its arguments and always busy-waits a single fixed
 *  tick count read from a global (DAT_c00011c8) via FUN_c0001aa0 - every call
 *  site's own delay-unit literal (0x32, or the retry path's leftover value of
 *  1) is transcribed here for fidelity but has no effect on real hardware.
 *  `chip` is kept in every signature below purely because the real functions
 *  still take the argument in their ABI (it's just unused) - a real caller
 *  substituting a different pointer would still hit the same physical pins.
 * ------------------------------------------------------------------------- */
extern void  at88_gpio_set_sda(void *chip, int level);		/* FUN_c0001170 - chip arg unused, see note above */
extern void  at88_gpio_set_scl(void *chip, int level);		/* FUN_c0001148 - chip arg unused, see note above */
extern void  at88_gpio_set_sda_dir(void *chip, int output);	/* FUN_c00011cc, 0=output/1=input inferred (matches every read-path use below), chip arg unused */
extern void  at88_delay(void *chip, int units);		/* FUN_c00011c0 - CORRECTION (2026-07-18): resolved. Real body is `FUN_c0001aa0(DAT_c00011c8); return;` - both `chip` and `units` are ignored, every call is really a fixed-duration busy-wait against one global tick count. */
extern int   at88_gpio_get_sda(void *chip);			/* FUN_c0001198 - raw GPIO bit sample via FUN_c0001990(DAT_c00011bc)+FUN_c00025e4(); chip arg unused */
static int   at88_i2c_read_ack(void *chip);			/* FUN_c000125c - forward-declared, defined below at88_i2c_send_read_ack; used both by at88_i2c_write_byte's per-byte ACK sample and at88_i2c_read's own end-of-transfer status bit */

/* I2C START condition: SDA high, SCL high, then SDA falls while SCL still
 * high (the START edge), then SCL falls to begin the clock - the textbook
 * bit-bang START sequence. */
static void at88_i2c_start(void *chip)				/* FUN_c000143c */
{
	at88_gpio_set_sda(chip, 1);
	at88_gpio_set_scl(chip, 1);
	at88_gpio_set_sda_dir(chip, 0);
	at88_delay(chip, 0x32);
	at88_gpio_set_sda(chip, 0);
	at88_delay(chip, 0x32);
	at88_gpio_set_scl(chip, 0);
	at88_delay(chip, 0x32);
}

/* I2C STOP condition: SDA low, SCL low, SDA=output, delay, SCL high, delay,
 * then SDA rises while SCL is still high (the STOP edge), delay - the
 * textbook bit-bang STOP sequence, mirroring at88_i2c_start above.
 *
 * RESOLVED (2026-07-18, closes a previously-open item): this is @0xc00013cc,
 * confirmed by tracing at88_i2c_write/at88_i2c_read's own bodies (both call
 * it unconditionally at the very end of every transaction, success or
 * failure - see those functions below). It is NOT called from
 * at88_frame_command's own address-byte retry loop - that finding from the
 * 2026-07-17 pass still stands, see at88_frame_command's own comment. */
static void at88_i2c_stop(void *chip)				/* FUN_c00013cc */
{
	at88_gpio_set_sda(chip, 0);
	at88_gpio_set_scl(chip, 0);
	at88_gpio_set_sda_dir(chip, 0);
	at88_delay(chip, 0x32);
	at88_gpio_set_scl(chip, 1);
	at88_delay(chip, 0x32);
	at88_gpio_set_sda(chip, 1);
	at88_delay(chip, 0x32);
}

/* Write one byte MSB-first (SDA settle -> SCL pulse per bit), then release
 * SDA and clock one more pulse to sample the slave's ACK bit. */
static int at88_i2c_write_byte(void *chip, uint8_t byte)	/* FUN_c000134c */
{
	int i;

	for (i = 7; i >= 0; i--) {
		at88_gpio_set_sda(chip, (byte >> 7) & 1);
		at88_delay(chip, 0x32);
		at88_gpio_set_scl(chip, 1);
		at88_delay(chip, 0x32);
		at88_gpio_set_scl(chip, 0);
		at88_delay(chip, 0x32);
		byte = (uint8_t)(byte << 1);
	}
	return at88_i2c_read_ack(chip);
}

/* Read one byte MSB-first: SDA released to input for the whole byte, then
 * per bit: SCL high, delay, sample SDA, SCL low, delay, shift into the
 * accumulator. SDA direction restored to output afterward.
 *
 * RESOLVED (2026-07-18, item 4): @0xc00012c8, previously untraced - this is
 * at88_i2c_read/FUN_c0001638's own per-byte data-phase primitive. */
static uint8_t at88_i2c_read_byte(void *chip)			/* FUN_c00012c8 */
{
	uint8_t byte = 0;
	int i;

	at88_gpio_set_sda_dir(chip, 1);
	for (i = 7; i >= 0; i--) {
		at88_gpio_set_scl(chip, 1);
		at88_delay(chip, 0x32);
		byte = (uint8_t)((byte << 1) | (at88_gpio_get_sda(chip) & 1));
		at88_gpio_set_scl(chip, 0);
		at88_delay(chip, 0x32);
	}
	at88_gpio_set_sda_dir(chip, 0);
	return byte;
}

/* Master sends an ACK pulse between data bytes during a multi-byte read
 * (SDA driven low, one SCL pulse) - the standard I2C "ACK every byte except
 * the last" convention for a multi-byte read, confirmed by at88_i2c_read's
 * own call site below (only called when more bytes remain).
 *
 * RESOLVED (2026-07-18, item 4): @0xc0001204. */
static void at88_i2c_send_read_ack(void *chip)		/* FUN_c0001204 */
{
	at88_gpio_set_sda(chip, 0);
	at88_delay(chip, 0x32);
	at88_gpio_set_scl(chip, 1);
	at88_delay(chip, 0x32);
	at88_gpio_set_scl(chip, 0);
	at88_delay(chip, 0x32);
}

/* Sample a single ACK/NACK-shaped bit off the bus: SDA released to input,
 * one SCL pulse, sample SDA (normalized to 0/1), SDA direction restored to
 * output. Used both as the write path's per-byte ACK sample (see
 * at88_i2c_write_byte above, which calls this directly) and, once more, as
 * the read path's own end-of-transfer status bit (see at88_i2c_read below).
 * @0xc000125c. */
static int at88_i2c_read_ack(void *chip)			/* FUN_c000125c */
{
	int bit;

	at88_gpio_set_sda_dir(chip, 1);
	at88_gpio_set_scl(chip, 1);
	at88_delay(chip, 0x32);
	bit = at88_gpio_get_sda(chip) != 0;
	at88_gpio_set_scl(chip, 0);
	at88_delay(chip, 0x32);
	at88_gpio_set_sda_dir(chip, 0);
	return bit;
}

/*
 * at88_frame_command - sends the 4-byte {addr, cmd, arg1, arg2} command
 * header: START, then write the address byte, retrying (re-START + write)
 * on NACK up to a real, now-confirmed bound, then three more
 * write-byte-with-ACK-check calls for the remaining header bytes, aborting
 * on the first NACK.
 *
 * RESOLVED (2026-07-18, item 3): `DAT_c0001634` = 0x4e1f = 19999 decimal -
 * the real retry bound (address byte may be attempted up to 20000 times
 * total before giving up). The retry step itself is unchanged from the
 * 2026-07-17 finding: a single at88_delay() call (passed a leftover register
 * value of 1, not the usual 0x32 literal - though per at88_delay's own note
 * above this makes no actual timing difference, the delay length is fixed
 * regardless) followed directly by a re-START, with NO at88_i2c_stop() call
 * anywhere in this loop - bus release only happens once, in the caller
 * (at88_i2c_write/at88_i2c_read below), after the whole transaction
 * (header + data) completes.
 *
 * Returns 1 on a fully-ACKed header, 0 on any failure (address never ACKed
 * within the retry bound, or a later header byte NACKed). @0xc0001588.
 */
#define AT88_FRAME_ADDR_RETRY_LIMIT	19999	/* DAT_c0001634 = 0x4e1f, confirmed 2026-07-18 */

int at88_frame_command(void *chip, uint8_t addr, uint8_t cmd, uint8_t arg1, uint8_t arg2)
{
	int attempts = 0;

	for (;;) {
		at88_i2c_start(chip);
		attempts++;
		if (at88_i2c_write_byte(chip, addr) == 0)
			break;	/* ACKed - proceed to the rest of the header */
		at88_delay(chip, 1);	/* real leftover-register delay value, see comment above */
		if (attempts > AT88_FRAME_ADDR_RETRY_LIMIT)
			return 0;
		/* loop: re-START and retry the address byte - no STOP in between */
	}

	if (at88_i2c_write_byte(chip, cmd) != 0)
		return 0;
	if (at88_i2c_write_byte(chip, arg1) != 0)
		return 0;
	if (at88_i2c_write_byte(chip, arg2) != 0)
		return 0;
	return 1;
}

/*
 * at88_i2c_write/at88_i2c_read (@0xc00016e8/@0xc0001638) - full data-phase
 * bodies, resolved 2026-07-18 (item 4; previously left as opaque externs,
 * "not separately traced this pass").
 *
 * Both bracket the whole transaction with at88_lock()/at88_unlock() (see
 * below, also newly resolved this pass) and unconditionally call
 * at88_i2c_stop() at the end regardless of success or failure.
 *
 * HEADER BYTE MAPPING - resolved by tracing the real call into
 * at88_frame_command: the AT88 opcode (0xb0/0xb2/0xb4/0xb8) is passed into
 * frame_command's *address*-byte slot (the one that gets retried on NACK) -
 * matches Atmel's own AT88SC datasheet, which calls these bytes the
 * "slave address" for the command, not a separate bus address; this
 * project's own `addr` parameter name for at88_frame_command was chosen
 * with that in mind. `cmd`'s slot receives `arg1`, `arg1`'s slot receives
 * `arg2`, and `arg2`'s slot receives `(uint8_t)len` - i.e. the 4th
 * transmitted header byte is the transfer length truncated to 8 bits, not a
 * separate fixed register value. (Confirmed against the self-test's own
 * $B0 write call: `crypto_at88_write(chip, 0xb0, 0, 0, 16, buf)` frames as
 * header bytes {0xb0, 0, 0, 16} - the trailing 16 is the length, landing in
 * frame_command's `arg2` slot, not the write call's own `arg2=0`.)
 */
static void at88_lock(void);
static void at88_unlock(void);

int at88_i2c_write(void *chip, uint8_t cmd, uint8_t arg1, uint8_t arg2,	/* FUN_c00016e8 */
		   uint32_t len, const uint8_t *data)
{
	int ok;

	at88_lock();
	ok = at88_frame_command(chip, cmd, arg1, arg2, (uint8_t)len);
	if (ok) {
		uint32_t i;

		for (i = 0; i < len; i++) {
			if (at88_i2c_write_byte(chip, data[i]) != 0) {
				ok = 0;
				break;
			}
		}
	}
	at88_i2c_stop(chip);
	at88_unlock();
	return ok;
}

int at88_i2c_read(void *chip, uint8_t cmd, uint8_t arg1, uint8_t arg2,	/* FUN_c0001638 */
		  uint32_t len, uint8_t *dest)
{
	int ok;

	at88_lock();
	ok = at88_frame_command(chip, cmd, arg1, arg2, (uint8_t)len);
	if (ok) {
		uint32_t i;

		for (i = 0; i < len; i++) {
			dest[i] = at88_i2c_read_byte(chip);
			if (i + 1 < len)
				at88_i2c_send_read_ack(chip);
		}
		/* trailing status bit, sampled once after all data bytes -
		 * folds into the overall result exactly like a normal ACK
		 * check (real code: `if (at88_i2c_read_ack(chip) != 1) ok = 0;`) */
		if (at88_i2c_read_ack(chip) != 1)
			ok = 0;
	}
	at88_i2c_stop(chip);
	at88_unlock();
	return ok;
}

/* thin dispatchers over the two primitives above - kept as separate real
 * functions to match the real binary's own two entry points, both of which are
 * called directly from other subsystems, not just from this file.
 *
 * CORRECTION (2026-07-18): the REAL FUN_c0000ef4/FUN_c0000f30 do NOT forward
 * their own `chip` parameter to at88_i2c_write/at88_i2c_read at all - they
 * pass the address of an uninitialized 4-byte on-stack buffer instead
 * (`undefined1 auStack_14[4]; FUN_c00016e8(auStack_14, param_2, ...)` in the
 * real decompile). This is the exact same "phantom forwarded parameter"
 * pattern the re-verification pass already found independently in
 * cdix4192.c and eva_board_main.c - see this file's top-of-file note. It is
 * provably harmless here (and only here, because this file already
 * independently confirmed the entire GPIO layer ignores `chip` regardless
 * of what's passed), so `chip` is still forwarded normally below for
 * clarity/recompilability rather than mechanically reproducing a read from
 * uninitialized stack memory. */
static inline void crypto_at88_write(void *chip, uint8_t cmd, uint8_t arg1,
				     uint8_t arg2, uint32_t len, const void *data)
{								/* FUN_c0000ef4 */
	at88_i2c_write(chip, cmd, arg1, arg2, len, data);
}
static inline void crypto_at88_read(void *chip, uint8_t cmd, uint8_t arg1,
				    uint8_t arg2, uint32_t len, void *dest)
{								/* FUN_c0000f30 */
	at88_i2c_read(chip, cmd, arg1, arg2, len, dest);
}

/* screen text-draw primitive: (x, y, string, ?) - 4th param's real meaning
 * (colour index? font id?) not resolved, matches every other unresolved-arg
 * flag in this file: modeled as an opaque int rather than guessed. */
extern void draw_text(int x, int y, const char *str, int unknown_arg4);	/* FUN_c0015650 */
/* CORRECTION (SPI-device closure pass, 2026-07-17): renamed from
 * crypto_at88_prepare_fault_screen. A fresh decompile taken while tracing an
 * unrelated cpsoc.c call site (FUN_c0010cd0, which brackets a queue push
 * with this function and its counterpart FUN_c0005510) shows the real body
 * is just raw CPSR condition-flag reads (Ghidra renders these as
 * `in_NG`/`in_ZR`/`in_CY`/`in_OV`/`in_Q` pseudo-registers - the standard
 * decompiler artifact for an inlined flags-save instruction, e.g. an ARM
 * `MRS`-then-disable-IRQ sequence) - a generic interrupt-save/disable
 * primitive, not anything fault-screen-specific. crypto_at88_fault below is
 * simply its one, ONE-WAY caller here: it disables interrupts before
 * painting the fatal error screen (so nothing else can preempt/corrupt the
 * shared display while it draws) and never calls the matching restore
 * (FUN_c0005510) since this function never returns anyway. See cpsoc.c's
 * own new section for the counterpart's real, symmetric use. */
extern int  irq_save_and_disable(void);					/* FUN_c0005500 */
extern void crypto_at88_format_fault_text(char *dst, const char *fmt,
					  const void *arg1, const void *arg3);	/* FUN_c00168fc */

/* ------------------------------------------------------------------------- *
 *  crypto_at88_fault - the hard-halt assert handler (real name unconfirmed;
 *  behavior fully confirmed: draws an error screen then never returns).
 *  @0xc000919c
 * ------------------------------------------------------------------------- */
void crypto_at88_fault(const void *unused_arg1, const char *file, int line)
{
	irq_save_and_disable();	/* one-way: never restored, this function never returns */
	/* real call site passes the same two format-target globals every time -
	 * this file (../CryptoAt88.cpp) always asserts against the same message
	 * buffer/template, per the real binary's DAT_c00091fc/DAT_c00091f8. */
	crypto_at88_format_fault_text((char *)0 /* DAT_c00091fc, real address */,
				      (const char *)0 /* DAT_c00091f8, real address */,
				      unused_arg1, (const void *)(intptr_t)line);
	draw_text(10, 10, (const char *)0 /* DAT_c00091fc */, 0);
	draw_text(10, 20, file, 0);
	for (;;) { }		/* confirmed: do {} while (true), never returns */
}

/*
 * at88_lock/at88_unlock (@0xc00014ac/@0xc00014e8) - a simple non-reentrant
 * critical-section flag bracketing every at88_i2c_write/at88_i2c_read
 * transaction. Both DAT_c00014e0 (checked/set by lock) and DAT_c00014f8
 * (cleared by unlock) resolve to the SAME runtime address, confirming this
 * is one shared flag, not two.
 *
 * RESOLVED (2026-07-18, closes a previously-open item): re-entering while
 * already locked is a hard firmware fault, via the same crypto_at88_fault
 * primitive documented above - `FUN_c000919c(0, DAT_c00014e4, 0xbb)` in the
 * real decompile. DAT_c00014e4 resolves to address 0xc0022cf8, which is
 * exactly the address of this project's own `"../I2cByGpio.cpp"` string
 * anchor (see README.md's subsystem table) - confirming this lock genuinely
 * belongs to the I2C bit-bang layer's own translation unit, not
 * CryptoAt88.cpp, even though it's reconstructed here alongside the rest of
 * the bit-bang code per this file's existing convention.
 */
static uint8_t at88_i2c_busy;	/* DAT_c00014e0 == DAT_c00014f8 */

static void at88_lock(void)					/* FUN_c00014ac */
{
	if (at88_i2c_busy)
		crypto_at88_fault(0, "../I2cByGpio.cpp", 0xbb);
	at88_i2c_busy = 1;
}
static void at88_unlock(void)					/* FUN_c00014e8 */
{
	at88_i2c_busy = 0;
}

/* ------------------------------------------------------------------------- *
 *  crypto_at88_self_test - factory zone-0 write/read-back round-trip test.
 *  @0xc0001028.
 *
 *  RESOLVED (2026-07-18, item 5): re-checked with full xref data across all
 *  691 functions in the image - genuinely ZERO static callers anywhere,
 *  including inside EvaBoardMain's own board bring-up (FUN_c00074bc,
 *  @0xc00074bc, which DOES call this file's crypto_at88_queue_init below but
 *  never this function). This function is either dead code left over from a
 *  factory-test build, or reached only through a mechanism this static
 *  analysis can't see (an indirect/table call). Either way there is no
 *  evidence of a $B8 handshake anywhere in its call path, because it has no
 *  call path at all - the open question "does $B8 happen before this runs"
 *  is moot until/unless a caller is found.
 * ------------------------------------------------------------------------- */
void crypto_at88_self_test(void *chip)
{
	uint8_t buf[16];
	int i;

	/* $B4 zone-select -> zone 0: {0xb4, 0x03, 0x00, 0x00} */
	crypto_at88_write(chip, 0xb4, 3, 0, 0, buf);

	/* known write pattern: 0,1,2,...,15 */
	for (i = 0; i < 16; i++)
		buf[i] = (uint8_t)i;
	/* $B0 write of that pattern to zone-0 address 0 */
	crypto_at88_write(chip, 0xb0, 0, 0, 16, buf);

	/* poison the buffer before reading back - proves the next read actually
	 * wrote real data rather than leaving stale contents that would
	 * accidentally still match. Confirmed real constant: 0xa5 repeated. */
	for (i = 15; i >= 0; i--)
		buf[i] = 0xa5;
	/* $B2 read of 16 bytes back from zone-0 address 0 */
	crypto_at88_read(chip, 0xb2, 0, 0, 16, buf);

	/* compare against the ORIGINAL write pattern (0,1,...,15), not the
	 * poison value - any mismatch is a hard, unrecoverable fault. */
	for (i = 0; i < 16; i++) {
		if (buf[i] != (uint8_t)i)
			crypto_at88_fault(0, "../CryptoAt88.cpp", 0xbd);
	}
}

/* ------------------------------------------------------------------------- *
 *  crypto_at88_queue_init (@0xc0000ec8) - resets the 2-deep ring buffer's
 *  write index / read index / count fields (offsets 0x40/0x41/0x42, see the
 *  struct below) to zero, then runs a bus-recovery sequence
 *  (at88_i2c_bus_reset, @0xc00014fc: SDA/SCL both driven high, a ~1000-unit
 *  delay, 7 SCL clock pulses while SDA is held high - the classic "toggle
 *  SCL to free a slave stuck holding SDA low" I2C bus-recovery idiom - then
 *  another ~1000-unit delay). Both are gated by at88_lock/at88_unlock.
 *
 *  RESOLVED (2026-07-18, ties into item 2/5): this is the AT88 queue's own
 *  init entry point, and its one confirmed caller is EvaBoardMain's board
 *  bring-up function (FUN_c00074bc, @0xc00074bc - also seen calling several
 *  other subsystems' own init routines, e.g. cpsoc's `FUN_c0007120` for LED
 *  register opcodes 0x78/0x79/0x7a/0x7b already documented in cpsoc.c). Not
 *  reconstructed in full here (out of this file's scope - see cobjectmgr.c/
 *  eva_board_main.c), only cited by address per this project's existing
 *  convention for shared dispatchers.
 *
 *  Like crypto_at88_queue_init's queue-index resets, at88_i2c_bus_reset's
 *  own real call passes the address of an uninitialized on-stack buffer as
 *  `chip` (the same "phantom forwarded parameter" pattern noted above) -
 *  again harmless given the GPIO layer ignores `chip` unconditionally.
 * ------------------------------------------------------------------------- */
static void at88_i2c_bus_reset(void *chip)			/* FUN_c00014fc */
{
	int i;

	at88_lock();
	at88_gpio_set_sda(chip, 1);
	at88_gpio_set_scl(chip, 1);
	at88_delay(chip, 1000);
	for (i = 6; i >= 0; i--) {
		at88_gpio_set_scl(chip, 0);
		at88_delay(chip, 0x32);
		at88_gpio_set_scl(chip, 1);
		at88_delay(chip, 0x32);
	}
	at88_delay(chip, 1000);
	at88_unlock();
}

/* ------------------------------------------------------------------------- *
 *  Queued command relay - a 2-deep ring buffer (index masked & 1) of pending
 *  AT88 write/read requests, drained by crypto_at88_process_queue. Ground
 *  truth for the dequeue primitive: @0xc0000fc8, 32-byte entries, count field
 *  at handle+0x42, read-index byte at handle+0x41; the matching (newly
 *  resolved) push primitive is @0xc0000f6c, same 32-byte stride, write-index
 *  byte at handle+0x40, gated on count<2 - confirming the "2-deep" sizing
 *  from both ends now, not just the pop side.
 *
 *  QUEUE OBJECT, resolved 2026-07-18: the ring buffer is a single fixed
 *  global, NOT anything derived from a caller-supplied handle.
 *  crypto_at88_process_queue's own DAT_c0005f58 (used for every
 *  at88_queue_pop/crypto_at88_write/crypto_at88_read call inside it) and the
 *  producer's DAT_c00084c8 (used by the push call below) are the exact same
 *  runtime address - one shared queue object linking the two ends.
 *
 *  PRODUCER, resolved 2026-07-18 (item 6): the queue is filled by
 *  FUN_c0007d1c, the firmware's central USB host-command dispatcher (see
 *  cpsoc.c's own citation of this same function for opcodes 0x50/0x51/0x52).
 *  Two of its opcode branches push into this queue:
 *    - host opcode 0xE0 ("AtmelWrite"): builds a WRITE entry - cmd byte taken
 *      directly from the incoming USB packet (no LSB forcing, so cmd's LSB
 *      is naturally 0 = write, matching the struct's cmd&1 selector below),
 *      arg1/arg2/len copied, followed by a variable-length data payload
 *      copied in reversed 4-byte groups - the SAME per-dword-reversal wire
 *      convention already reverse-engineered for the device->host direction
 *      in at88_relay_read_result below, now confirmed on the host->device
 *      side too. Gated on length < 0x21 (33) before the push is attempted.
 *    - host opcode 0xE1 ("AtmelRead"): builds a READ entry - cmd byte is
 *      `param_2[2] | 1`, i.e. the enqueue side explicitly FORCES the LSB to 1
 *      - this is the concrete, previously-missing confirmation for the
 *      struct's own `cmd` field comment below (it called the LSB semantics
 *      "not fully disambiguated"; it now is). arg1/arg2/len copied, same
 *      length gate.
 *  On a successful push, either branch calls `FUN_c001d22c(1, 0x2000)`.
 *
 *  BIT-13 STATUS SETTER, resolved 2026-07-18 (item 2): FUN_c001d22c is a
 *  generic "OR event flag bits into object #1's status word" primitive
 *  (confirmed by symmetry with FUN_c0008b64's own opening two calls,
 *  `FUN_c001d3a8(1, DAT_c000903c, 1, &local_2c)` /
 *  `FUN_c001d318(1, DAT_c000903c)`, which read-and-clear that exact same
 *  object-#1 status word masked by DAT_c000903c=0xb17f). `0x2000` is bit 13.
 *  So the full chain is: host sends an AtmelWrite/AtmelRead USB packet ->
 *  FUN_c0007d1c pushes an entry onto this queue and sets status bit 13 ->
 *  FUN_c0008b64's own dispatch loop (whatever cadence it runs on) sees bit
 *  13 set and calls crypto_at88_process_queue, which drains the queue for
 *  real over I2C and, for reads, relays the result back to the host as the
 *  same 0xE1-tagged wire event. This was this file's last major structural
 *  gap and is now closed.
 * ------------------------------------------------------------------------- */
struct at88_queue_entry {
	uint8_t  cmd;		/* LSB is a read(1)/write(0) selector - CONFIRMED
				 * 2026-07-18: the producer (FUN_c0007d1c) leaves
				 * it naturally 0 for a write-enqueue (host opcode
				 * 0xE0) and explicitly ORs in 1 for a
				 * read-enqueue (host opcode 0xE1, `cmd | 1`). The
				 * AT88 opcode proper is `cmd & 0xfe`, consumed
				 * that way by crypto_at88_process_queue below. */
	uint8_t  arg1, arg2, len;
	uint8_t  data[28];	/* rest of the 32-byte entry */
};

extern int  at88_queue_pop(void *queue_handle, struct at88_queue_entry *out);	/* FUN_c0000fc8 */
extern int  at88_queue_push(void *queue_handle, const struct at88_queue_entry *in); /* FUN_c0000f6c - producer, see FUN_c0007d1c citation above; not reconstructed here (lives in the shared USB command dispatcher, out of this file's scope) */
extern void at88_relay_read_result(void *unused_param,
				   struct at88_queue_entry *entry);		/* FUN_c0005da0 */

/* the shared queue object both crypto_at88_process_queue and the
 * FUN_c0007d1c producer operate on - DAT_c0005f58 == DAT_c00084c8,
 * confirmed the same runtime address, see the block comment above. Real
 * type/owner not resolved (opaque handle as far as this file is concerned). */
extern void *g_at88_queue;

/*
 * CORRECTION (2026-07-18): previously declared as taking two parameters,
 * `(queue_handle, dest_channel)`. Re-verified against the real function's
 * own decompile: FUN_c0005e9c takes exactly ONE parameter. Neither the
 * queue object (a fixed global, g_at88_queue/DAT_c0005f58 - see above) nor
 * the eventual USB destination channel (also a fixed global, see
 * at88_relay_read_result below) is derived from it. The one parameter this
 * function does take is simply forwarded one level down into
 * at88_relay_read_result, which likewise never uses it - a third, wholly
 * independent instance of the same "phantom forwarded parameter" pattern
 * already found in cdix4192.c and eva_board_main.c, this time spanning this
 * entire two-function call chain rather than a single function.
 *
 * The write path's own event: CONFIRMED fire-and-forget (item 1) - traced
 * both at this level (no call to at88_relay_read_result on the write
 * branch) AND one level down inside at88_i2c_write itself (its real body,
 * reconstructed above, never calls any event-relay function either). A
 * successful $B0/$B4/$B8 write produces no host-visible completion event at
 * any layer; the only acknowledgment the host gets is FUN_c0007d1c's own
 * generic, immediate command-dispatch return status at enqueue time, common
 * to every opcode that dispatcher handles, not anything AT88-specific.
 */
void crypto_at88_process_queue(void *unused_param)		/* FUN_c0005e9c */
{
	struct at88_queue_entry e;

	while (at88_queue_pop(g_at88_queue, &e)) {
		if ((e.cmd & 1) == 0) {
			/* write path - fire and forget, no result relayed
			 * (confirmed at two levels, see comment above) */
			crypto_at88_write(g_at88_queue, e.cmd, e.arg1, e.arg2,
					  e.len, e.data);
		} else {
			e.cmd &= 0xfe;
			memset(e.data, 0, sizeof(e.data));
			crypto_at88_read(g_at88_queue, e.cmd, e.arg1, e.arg2,
					 e.len, e.data);
			/* relay the read result to the host as an AtmelRead
			 * (0xe1) event - see at88_relay_read_result below. */
			at88_relay_read_result(unused_param, &e);
		}
	}
}

/*
 * at88_relay_read_result (@0xc0005da0) - builds the wire-format AtmelRead
 * (0xe1) event record from a completed read's queue entry and submits it to
 * the host-bound USB transmit queue (at88_usb_tx_submit, @0xc000acec - a
 * "buffer available + <128 bytes outstanding" gated submit, this firmware's
 * own mirror of the host's CSTGOmapNKS4Fifos input-FIFO consumer).
 *
 * CORRECTION (re-verification pass, 2026-07-17): a previous draft of this
 * function included a `FUN_c000acc8`-based "ready" pre-check
 * (at88_usb_tx_ready) before building/submitting the wire record.
 * Re-checked against fresh disassembly: there is NO call to FUN_c000acc8
 * anywhere in THIS function - the real code builds the wire buffer and
 * calls at88_usb_tx_submit (FUN_c000acec) directly. FUN_c000acc8 is
 * separately identified (see omap_l137_usbdc.c) as a thin wrapper around a
 * USB transfer-completion state-machine poller (FUN_c0004b88).
 * CONFIRMATION (2026-07-18): tracing FUN_c000acec's own body this pass shows
 * it calls FUN_c000acc8(dest, 0) as the VERY FIRST thing it does and bails
 * out immediately if that returns false - exactly the "gating happens
 * INSIDE at88_usb_tx_submit itself, not at this call site" prediction the
 * 2026-07-17 correction made, now independently confirmed.
 *
 * CORRECTION (2026-07-18): previously declared taking a `dest_channel`
 * parameter. The real function's own `param_1` is never referenced anywhere
 * in its body - the actual USB channel passed to at88_usb_tx_submit is a
 * separate fixed global (g_at88_usb_channel/DAT_c0005e98, confirmed distinct
 * from the queue object above). Parameter kept in the signature below only
 * because the real ABI still has the (unused) slot - see
 * crypto_at88_process_queue's own correction note for why it's always
 * called with a value that ultimately goes nowhere.
 *
 * Ground truth for the record layout: reading the real function's stack
 * variables in true memory order (not Ghidra's local_XX assignment order)
 * gives the first wire dword as [entry.arg2, entry.arg1, entry.cmd, 0xe1] -
 * i.e. the opcode byte 0xe1 is stored LAST in program order but ends up as
 * the HIGH byte of the first transmitted word, with the three command-header
 * bytes preceding it in reverse - the same per-dword byte-reversal
 * convention already reverse-engineered from the host side (see the queue
 * producer's own 0xE0 branch above for the same convention working in the
 * opposite direction). This is a faithful re-expression of the real
 * function's logic, not a byte-for-byte copy of Korg's own object code -
 * FUN_c0016804, the helper both this function and the 0xE0 producer branch
 * use to move each 4-byte group, is itself a plain (alignment-aware) block
 * copy; the "reversal" is entirely an artifact of which stack local gets
 * assigned which source byte before the copy, not anything FUN_c0016804
 * does on its own.
 *
 * CORRECTION (re-verification pass, 2026-07-17): word 1 and the trailing
 * payload loop were BOTH wrong in the same direction - the previous draft
 * transcribed them as straight, unreversed copies. Re-verified against
 * fresh disassembly:
 *  - word 1 is really [entry.data[2], entry.data[1], entry.data[0],
 *    entry.len] - the SAME "reverse the first 3 content bytes, keep the
 *    4th field (here `len`, not a tag byte) in place" pattern as word 0.
 *  - every trailing 4-byte payload group has no anchor field to keep in
 *    place (all 4 bytes are plain data), so the real order is a FULL
 *    4-byte reversal per group: source bytes [i, i+1, i+2, i+3] land in
 *    wire order [i+3, i+2, i+1, i], not a straight memcpy.
 *
 * CORRECTION (2026-07-18): the trailing loop's CONTINUATION condition was
 * wrong. Previously `i + 4 <= (int)entry->len` (i.e. only process a group if
 * all 4 of its source bytes are within `len`). Re-derived from the real
 * decompile's `SBORROW4(iVar2, len)`-based condition, which - for the small,
 * non-overflowing values `len` actually takes (0-255) - reduces cleanly to
 * plain signed comparison `iVar2 < len`. That means the real firmware
 * processes one MORE trailing group than the old condition allowed whenever
 * `len` isn't an exact `4*k + 3`: e.g. for len=16 (the self-test's own read
 * size) the real loop runs groups at i=3,7,11,15 (four groups, the last one
 * reading up to entry->data[18]) where the old condition stopped after
 * i=3,7,11 (three groups) - the real transmitted `wire_len` (and therefore
 * the exact byte count sent to the host) genuinely differs from the
 * previous draft - fixed below.
 *
 * NOTE ON BOUNDS (not a transcription bug, a property of the real firmware,
 * flagged here rather than silently "fixed away"): `entry` in the real
 * FUN_c0005da0 is `&local_38`, a 32-byte local stack buffer in the caller
 * (FUN_c0005e9c) laid out exactly like `struct at88_queue_entry` here (4
 * header bytes + `data[28]`). The queue producer (FUN_c0007d1c, see above)
 * only gates incoming length against `< 0x21` (33), which is LARGER than
 * the struct's real 28-byte data capacity - so for `len` roughly above
 * mid-20s, both the real producer's copy-in loop and this function's own
 * copy-out loop walk a few bytes past the nominal `data[28]` array, into
 * whatever memory follows it on the stack (adjacent locals in the real
 * binary; in this reconstructed C, whatever the compiler places after `e`
 * in crypto_at88_process_queue's own frame). This is a genuine, confirmed
 * mismatched-bounds quirk in the original firmware, not an artifact of this
 * reconstruction - transcribed faithfully rather than clamped, per this
 * project's own fidelity-over-safety convention, but worth knowing before
 * exercising this path with `len` near the producer's 32-byte gate.
 */
extern void at88_usb_tx_submit(void *dest_channel, const void *buf, int len);	/* FUN_c000acec (full) */
extern void *g_at88_usb_channel;	/* DAT_c0005e98 - fixed USB destination channel, see correction above */

void at88_relay_read_result(void *unused_param, struct at88_queue_entry *entry)
{
	uint8_t wire[64];
	int wire_len = 8;
	int i;

	(void)unused_param;	/* real function never references its own param_1, see correction note above */

	/* word 0: [arg2, arg1, cmd, 0xe1] - opcode 0xe1 (AtmelRead) high byte */
	wire[0] = entry->arg2;
	wire[1] = entry->arg1;
	wire[2] = entry->cmd;
	wire[3] = 0xe1;
	/* word 1: [data[2], data[1], data[0], len] - see correction note above */
	wire[4] = entry->data[2];
	wire[5] = entry->data[1];
	wire[6] = entry->data[0];
	wire[7] = entry->len;
	/* remaining payload bytes, 4 at a time, each group fully reversed -
	 * loop bound CORRECTED 2026-07-18, see comment above (`i < len`, not
	 * `i + 4 <= len`) */
	for (i = 3; i < (int)entry->len; i += 4, wire_len += 4) {
		wire[wire_len + 0] = entry->data[i + 3];
		wire[wire_len + 1] = entry->data[i + 2];
		wire[wire_len + 2] = entry->data[i + 1];
		wire[wire_len + 3] = entry->data[i + 0];
	}

	at88_usb_tx_submit(g_at88_usb_channel, wire, wire_len);
}
