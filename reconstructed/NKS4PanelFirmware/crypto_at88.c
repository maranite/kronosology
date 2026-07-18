/* SPDX-License-Identifier: GPL-2.0 */
/*
 * crypto_at88.c - the NKS4 panel firmware's own direct path to the AT88SC/NV2AC
 * security chip, independent of the host's OA.ko/OmapNKS4Module.ko/GetPubIdMod.ko.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB (TI OMAP-L1x, ARM926EJ-S,
 * loaded at 0xC0000000), 2026-07-17. See this project's own README.md for the full
 * per-function citation list and open items. Original addresses given per function so
 * this stays checkable against a fresh decompile.
 *
 * Two independent callers of the same write(FUN_c0000ef4)/read(FUN_c0000f30) pair were
 * found and are both reconstructed here: a one-shot factory self-test
 * (crypto_at88_self_test, @0xc0001028) and a runtime queued-command relay
 * (crypto_at88_process_queue, @0xc0005e9c) whose read results get forwarded to the host
 * as a genuine AtmelRead (0xe1) event - the same opcode this repo's host-side docs
 * (kronosology/reconstructed/OmapNKS4Module/driver.cpp's ReceiveEventBuffer,
 * KronosNKS4/docs/protocol.md) already decode on the receiving end. Confirming this
 * event's construction from the *sending* side, independently, is new - see README.md.
 */

#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------------- *
 *  Lower-level I2C bit-bang primitives - fully traced, 2026-07-17.
 *  @0xc0001588 (frame a 4-byte command), 0xc000143c (START condition),
 *  0xc000134c (write one byte + read its ACK bit), 0xc00011c0 (delay,
 *  ~50-cycle units - every call site in this file passes the same literal
 *  0x32; the exact time-per-unit isn't independently confirmed).
 * ------------------------------------------------------------------------- */
extern void  at88_gpio_set_sda(void *chip, int level);		/* FUN_c0001170 */
extern void  at88_gpio_set_scl(void *chip, int level);		/* FUN_c0001148 */
extern void  at88_gpio_set_sda_dir(void *chip, int output);	/* FUN_c00011cc, 0=output/1=input inferred, not confirmed */
extern void  at88_delay(void *chip, int units);		/* FUN_c00011c0 - real param wiring to FUN_c0001aa0(DAT_c00011c8) not fully resolved; corrected to 2 args, re-verification pass 2026-07-17 - every real call site passes chip */
extern int   at88_i2c_read_ack(void *chip);			/* FUN_c000125c */

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

/* Write one byte MSB-first (SDA settle -> SCL pulse per bit), then release
 * SDA and clock one more pulse to sample the slave's ACK bit. */
static int at88_i2c_write_byte(void *chip, uint8_t byte)	/* FUN_c000134c */
{
	for (int i = 7; i >= 0; i--) {
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

/*
 * at88_i2c_write/at88_i2c_read (@0xc00016e8/@0xc0001638, both built on
 * at88_frame_command below) - the {cmd, arg1, arg2, length} header framing
 * (same 4-byte shape as the host-side stgNV2AC_sync_cmd protocol - see
 * kronosology/docs/crypto/atmel_nv2ac.md) plus the actual data phase, not
 * independently re-derived from at88_frame_command's own logic this pass -
 * modeled at the same confidence level as before (structurally confirmed via
 * the self-test's real call arguments, data-phase byte transfer not
 * separately traced).
 */
extern void at88_i2c_write(void *chip, uint8_t cmd, uint8_t arg1, uint8_t arg2,
			   uint32_t len, void *data);	/* FUN_c00016e8 (via FUN_c0000ef4) */
extern void at88_i2c_read(void *chip, uint8_t cmd, uint8_t arg1, uint8_t arg2,
			  uint32_t len, void *dest);		/* FUN_c0001638 (via FUN_c0000f30) */

extern void at88_i2c_stop(void *chip);	/* real name/address not yet resolved - see comment below */

/*
 * at88_frame_command - sends the 4-byte {addr, cmd, arg1, arg2} command
 * header: START, then write the address byte, retrying on NACK up to a real
 * bound (`DAT_c0001634`, exact value not extracted this pass) - the classic
 * I2C "wait for slave ready after a previous write" polling idiom, common
 * for EEPROM-class devices immediately after a write cycle.
 *
 * CORRECTION (re-verification pass, 2026-07-17): the retry step was
 * previously described as "STOP + START again." Re-verified against fresh
 * disassembly and found WRONG: there is no STOP call anywhere in this
 * function. The real retry is a delay call - using a leftover register
 * value of 1, NOT the usual 0x32 delay-unit literal every other call site
 * in this file uses - followed directly by a re-START, with no bus release
 * in between. `at88_i2c_stop` remains declared below since some retry path
 * elsewhere in the image plausibly still needs it, but it is NOT called
 * from this function.
 *
 * Once the address ACKs, three more write-byte-with-ACK-check calls send
 * the remaining header bytes, aborting on the first NACK. Returns 1 on a
 * fully-ACKed header, 0 on any failure (address never ACKed within the
 * retry bound, or a later header byte NACKed). @0xc0001588. This is the
 * primitive both at88_i2c_write and at88_i2c_read frame their command
 * header through before their own data phases.
 *
 * NOT faithfully transcribed: the exact retry bound and loop structure
 * (modeled here as a single address-ACK attempt, retry structure documented
 * rather than guessed at, to avoid asserting a loop bound that was never
 * actually read off the real binary).
 *
 * CORRECTION (re-verification pass, 2026-07-17): the address-byte ACK check
 * below previously used INVERTED polarity relative to the cmd/arg1/arg2
 * checks that follow it (`== 0` instead of `!= 0`) - a real bug, now fixed.
 * All four header-byte writes use the same 0=ACK/nonzero=NACK convention.
 */
int at88_frame_command(void *chip, uint8_t addr, uint8_t cmd, uint8_t arg1, uint8_t arg2)
{
	at88_i2c_start(chip);
	if (at88_i2c_write_byte(chip, addr) != 0)
		return 0;	/* real firmware retries here; not transcribed, see comment above */

	if (at88_i2c_write_byte(chip, cmd) != 0)
		return 0;
	if (at88_i2c_write_byte(chip, arg1) != 0)
		return 0;
	if (at88_i2c_write_byte(chip, arg2) != 0)
		return 0;
	return 1;
}

/* thin dispatchers over the two primitives above - kept as separate real
 * functions to match the real binary's own two entry points, both of which are
 * called directly from other subsystems, not just from this file. */
static inline void crypto_at88_write(void *chip, uint8_t cmd, uint8_t arg1,
				     uint8_t arg2, uint32_t len, void *data)
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

/* ------------------------------------------------------------------------- *
 *  crypto_at88_self_test - factory zone-0 write/read-back round-trip test.
 *  @0xc0001028. No static callers found anywhere in the image (README.md's
 *  open item) - not established whether this runs every boot or only in a
 *  factory-test build, nor whether a real $B8 handshake happens earlier in
 *  EvaBoardMain's bring-up (zone 0 is documented AR0=0xd5 crypto-auth mode).
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
 *  Queued command relay - a 2-deep ring buffer (index masked & 1) of pending
 *  AT88 write/read requests, drained by crypto_at88_process_queue. Ground
 *  truth for the dequeue primitive: @0xc0000fc8, 32-byte entries, count field
 *  at handle+0x42, read-index byte at handle+0x41.
 *
 *  TRIGGER CONTEXT, resolved 2026-07-17: crypto_at88_process_queue
 *  (FUN_c0005e9c) has exactly one caller, FUN_c0008b64 - the firmware's
 *  central interrupt-status dispatch loop. That function reads one hardware
 *  status/event register once per invocation, then fans out to ~10
 *  subsystem handlers by bit: bit 13 (0x2000) calls this queue processor,
 *  bit 8 (0x100) calls clcdc's test-pattern generator (via its own
 *  dispatcher, FUN_c0008a5c), bit 15 (0x8000) calls clcdc_progress_bar
 *  directly. This answers the practical question - queued AT88 work gets
 *  processed whenever this central dispatcher's bit 13 is set, on whatever
 *  cadence that function itself runs (main-loop poll or interrupt, not yet
 *  determined) - even though the specific producer that SETS bit 13 in the
 *  status register (almost certainly an ISR reacting to a real AT88 command
 *  request, possibly relayed from the host) is still not traced.
 * ------------------------------------------------------------------------- */
struct at88_queue_entry {
	uint8_t  cmd;		/* LSB is a read(1)/write(0) selector, not part of
				 * the AT88 opcode itself - the opcode proper is
				 * presumably cmd & 0xfe or carried separately;
				 * not fully disambiguated, see README.md. */
	uint8_t  arg1, arg2, len;
	uint8_t  data[28];	/* rest of the 32-byte entry */
};

extern int  at88_queue_pop(void *queue_handle, struct at88_queue_entry *out);	/* FUN_c0000fc8 */
extern void at88_relay_read_result(void *dest_channel,
				   struct at88_queue_entry *entry);		/* FUN_c0005da0 */

void crypto_at88_process_queue(void *queue_handle, void *dest_channel)
{
	struct at88_queue_entry e;

	while (at88_queue_pop(queue_handle, &e)) {
		if ((e.cmd & 1) == 0) {
			/* write path - fire and forget, no result relayed
			 * (confirmed: no call to at88_relay_read_result here) */
			crypto_at88_write(queue_handle, e.cmd, e.arg1, e.arg2,
					  e.len, e.data);
		} else {
			e.cmd &= 0xfe;
			memset(e.data, 0, sizeof(e.data));
			crypto_at88_read(queue_handle, e.cmd, e.arg1, e.arg2,
					 e.len, e.data);
			/* relay the read result to the host as an AtmelRead
			 * (0xe1) event - see at88_relay_read_result below. */
			at88_relay_read_result(dest_channel, &e);
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
 * anywhere in this function - the real code builds the wire buffer and
 * calls at88_usb_tx_submit (FUN_c000acec) directly. FUN_c000acc8 is now
 * separately identified (see omap_l137_usbdc.c) as a thin wrapper around a
 * USB transfer-completion state-machine poller (FUN_c0004b88) unrelated to
 * this call path - the earlier "ready check" framing was simply wrong, not
 * a stale-but-real gate. Any buffer-availability gating for this path, if
 * it exists, happens INSIDE at88_usb_tx_submit itself, not at this call
 * site.
 *
 * Ground truth for the record layout: reading the real function's stack
 * variables in true memory order (not Ghidra's local_XX assignment order)
 * gives the first wire dword as [entry.arg2, entry.arg1, entry.cmd, 0xe1] -
 * i.e. the opcode byte 0xe1 is stored LAST in program order but ends up as
 * the HIGH byte of the first transmitted word, with the three command-header
 * bytes preceding it in reverse - the same per-dword byte-reversal convention
 * already reverse-engineered from the host side (ContinueProcessingEvent's
 * pixel-chunk swap; KronosNKS4's documented AtmelRead decode). This is a
 * faithful re-expression of the real function's logic, not a byte-for-byte
 * copy of Korg's own object code.
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
 */
extern void at88_usb_tx_submit(void *dest_channel, const void *buf, int len);	/* FUN_c000acec (full) */

void at88_relay_read_result(void *dest_channel, struct at88_queue_entry *entry)
{
	uint8_t wire[60];
	int wire_len = 8;

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
	/* remaining payload bytes, 4 at a time, each group fully reversed
	 * (see correction note above - not a straight memcpy) */
	for (int i = 3; i + 4 <= (int)entry->len; i += 4, wire_len += 4) {
		wire[wire_len + 0] = entry->data[i + 3];
		wire[wire_len + 1] = entry->data[i + 2];
		wire[wire_len + 2] = entry->data[i + 1];
		wire[wire_len + 3] = entry->data[i + 0];
	}

	at88_usb_tx_submit(dest_channel, wire, wire_len);
}
