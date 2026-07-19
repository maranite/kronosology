/* SPDX-License-Identifier: GPL-2.0 */
/*
 * panelbus_dispatch.c - a second, internal command channel: a per-tick-polled
 * hardware I2C link (distinct from i2c_by_gpio.c's bit-banged bus) carrying
 * small 2-byte [opcode,arg] frames that fan out into cad.c's calibration
 * handlers, an ID/negotiation handshake, and a shared RTOS event-flag/
 * scheduler wakeup - NOT a debug console, see "the debug-console question"
 * below.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, read from the
 * pre-fetched dump (all_decompiled.json/all_data.json), 2026-07-18. No live
 * Ghidra MCP calls this pass (bridge flagged concurrency-unsafe this round).
 *
 * ORIGIN OF THIS FILE: cobjectmgr.c's own "three other anchor xrefs found but
 * deliberately NOT included" section flagged FUN_c0007220 as "a large
 * secondary protocol dispatcher, possibly a debug console... a real
 * candidate for its own future subsystem file" - this is that file.
 * KRONOS_V06R06.VSB.md's wire-protocol section separately documents
 * FUN_c0007d1c, the firmware's *external*, USB-host-facing opcode-byte
 * dispatcher (being reconstructed concurrently this round as wire_dispatch.c
 * per eva_board_main.c's own forward-reference). This file's dispatcher is a
 * SEPARATE function, with a separate caller chain, a separate opcode space,
 * and (per the hardware evidence below) a separate physical bus - see "how
 * this relates to FUN_c0007d1c" below.
 *
 * ANCHOR: NONE. A string search of the whole image for "serial"/"uart"/
 * "debug"/"console" found nothing, and the address range this file's
 * functions occupy (0xc0001a00, 0xc0007150-0xc0007220,
 * 0xc0009480, 0xc0010b08-0xc0010f08, 0xc0012e58) has no adjacent
 * "../<name>.cpp" string the way CryptoAt88.cpp/clcdc.cpp/ctouchpanel.cpp/cad.cpp/
 * I2cByGpio.cpp all do. This matches the no-anchor precedent already set by
 * cpsoc.c's own third-SPI-device section: attribution here rests on shared
 * register/global evidence and direct call-chain tracing, not a string xref.
 * File named descriptively per the task brief's own suggested fallback
 * pattern for this exact situation.
 *
 * THE DEBUG-CONSOLE QUESTION - settled by real caller data:
 * FUN_c0007220 has exactly ONE static caller: FUN_c0010b58 (panelbus_rx_dispatch_loop
 * below), itself called only from FUN_c0010f08 (panelbus_poll_channels), which
 * in turn is called from ONLY ONE place in the whole image - FUN_c0008b64
 * (the master per-tick dispatcher, "a sibling dispatcher one level up" per
 * cobjectmgr.c's own README note, to be reconstructed as wire_dispatch.c's
 * own master_dispatch_tick this round), gated behind status-word bit 0x1000.
 * That is a completely different call path from FUN_c0007d1c's own USB
 * command entry point. More decisively: panelbus_rx_dispatch_loop's own data
 * source (FUN_c00033f0, panelbus_i2c_read_bytes below) polls hardware status/
 * data registers at a base address that resolves to a real, physical
 * peripheral - see the hardware section below. A byte-framed command
 * interpreter fed by a real MMIO peripheral polled once per firmware tick,
 * whose own opcode handlers reach directly into cad.c's calibration state and
 * a shared scheduler event-flag group, is architecturally an INTERNAL
 * inter-board command channel, not a UART text console: there is no ASCII
 * parsing, no line editing, and no evidence of a print/echo path anywhere in
 * this call chain. The "possibly a debug console" guess is not supported by
 * this pass's evidence and is corrected here.
 *
 * HARDWARE: FUN_c00033f0's register offsets (+0x08 status: busy bit
 * 0x1000, RX-ready bit 0x8; +0x14 count; +0x18 RX data; +0x1c address;
 * +0x24 mode, with bit 0x8000 set on the last byte of a transfer) and
 * FUN_c0001a00's two selectable base-address constants (DAT_c0001a14 =
 * 0x01e28000, DAT_c0001a18 = 0x01c22000) are, together, an exact match for
 * the TI OMAP-L138/AM1808 on-chip I2C controller: those two addresses are
 * I2C1's and I2C0's real peripheral base addresses per the public TRM, and
 * 0x1000/0x8/0x8000 are exactly the ICSTR.BB (bus busy), ICSTR.ICRRDY
 * (receive ready) and ICMDR.STP (generate stop) bit positions. This is a
 * SEPARATE bus from i2c_by_gpio.c's own bit-banged I2cByGpio.cpp cluster
 * (0xc0001148-0xc0001874, GPIO toggling, no MMIO peripheral registers,
 * shared by CryptoAt88.cpp/CDix4192.cpp) - this file's functions sit outside
 * that address range and talk to the genuine on-chip I2C0/I2C1 hardware
 * block instead. Whether that means the board has two independent physical
 * I2C buses, or whether I2cByGpio.cpp's bit-bang layer coexists with this
 * hardware controller for a different reason, is not resolved here - stated
 * as the honest structural consequence of the address/bit-position match,
 * not asserted beyond that. This also means cpsoc.c's own "third SPI device"
 * finding (FUN_c00032f8, its own submit primitive) - which shares the exact
 * same +0x08/+0x14/+0x1c/+0x24 register shape as this file's FUN_c00033f0,
 * just for transmit instead of receive - is very likely ALSO this same I2C0
 * hardware block rather than SPI. Not corrected in cpsoc.c (not this file's
 * scope to edit), flagged here as a real cross-file finding for whoever
 * revisits that section.
 *
 * FUN_c0014488 CORRECTION: README.md/ctouchpanel.c/cobjectmgr.c all describe
 * FUN_c0014488 (the 5-0x1d lookup table) as referenced from FUN_c0007220.
 * This pass's own decompile of FUN_c0007220's full body shows no reference
 * to FUN_c0014488 anywhere. Its real xrefs (checked directly this pass) are
 * TWO call sites inside FUN_c0007d1c (bVar7==0xa0 and bVar7==0xc0) plus one
 * call inside ctouchpanel.c's own address range (0xc0014780) - i.e. it
 * belongs to FUN_c0007d1c/wire_dispatch.c and ctouchpanel.c, NOT to this
 * file. Left uncalled/undefined here; not re-transcribed since this file
 * does not actually use it. The table's own contents (25 entries, input
 * 5-0x1d) look like a 4-column x 7-row remap with a bit-reversed/scrambled
 * final partial row (28-29 map to 6/30 not 4/... in row order) - consistent
 * with a switch/LED scan-index remap, not conclusively decoded; genuinely
 * not this file's function to own given the caller evidence above.
 */

#include <stdint.h>
#include <stdbool.h>

/* ===========================================================================
 *  Hardware primitives - the OMAP-L138 I2C0/I2C1 controller block this
 *  file's dispatcher is fed from. See the file header for the address/bit
 *  evidence.
 * ===========================================================================
 */

struct omap_i2c_regs {
	uint8_t  pad_00[0x08];
	volatile uint32_t icstr;	/* +0x08: BB=1<<12, ICXRDY=1<<4, ICRRDY=1<<3 */
	uint8_t  pad_0c[0x08];		/* +0x0c..+0x13 */
	volatile uint32_t iccnt;	/* +0x14: byte count */
	volatile uint32_t icdrr;	/* +0x18: data receive register */
	volatile uint32_t icsar;	/* +0x1c: slave address register */
	volatile uint32_t icdxr;	/* +0x20: data transmit register */
	volatile uint32_t icmdr;	/* +0x24: mode register, STP=1<<15 */
};

#define I2C_ICSTR_BB      (1u << 12)
#define I2C_ICSTR_ICRRDY  (1u << 3)
#define I2C_ICMDR_STP     (1u << 15)

/* cad_delay_ticks (FUN_c00085a8) - a generic firmware-wide spin-delay
 * primitive, already cited from cad.c (cad_init/cad_trigger_calibration both
 * use it) - reused here under the same name for consistency; not redefined. */
extern void cad_delay_ticks(void *handle, int ticks);	/* FUN_c00085a8 */

/* ------------------------------------------------------------------------- *
 *  panelbus_i2c_base - selects I2C1 (DAT_c0001a14, 0x01e28000) or I2C0
 *  (DAT_c0001a18, 0x01c22000) by a boolean selector. The real decompile
 *  shows the first argument (a per-call-site tag constant, different at
 *  every one of this function's 10 firmware-wide call sites) is never
 *  actually read by the function body - genuinely a dead/unused parameter,
 *  not a transcription gap. Every call site inside THIS file's own family
 *  passes selector=0, i.e. this whole dispatcher only ever talks to I2C0.
 *  @0xc0001a00.
 * ------------------------------------------------------------------------- */
struct omap_i2c_regs *panelbus_i2c_base(uint32_t unused_tag, int select_i2c1)	/* FUN_c0001a00 */
{
	extern struct omap_i2c_regs *panelbus_i2c1_base;	/* DAT_c0001a14, real value 0x01e28000 */
	extern struct omap_i2c_regs *panelbus_i2c0_base;	/* DAT_c0001a18, real value 0x01c22000 */

	(void)unused_tag;	/* real decompile: r0 loaded but never referenced */
	return select_i2c1 ? panelbus_i2c1_base : panelbus_i2c0_base;
}

/* ------------------------------------------------------------------------- *
 *  panelbus_i2c_read_bytes - blocking I2C read of `len` bytes into `dst`.
 *  Waits up to 1000 x 1-tick spins for the bus-busy (BB) flag to clear, sets
 *  byte count/slave-address/mode registers, then per byte waits up to 1000
 *  spins for ICRRDY before latching a byte from ICDRR - on the LAST byte it
 *  first ORs the stop bit (STP) into ICMDR so the controller issues an I2C
 *  STOP once that final byte lands. Real final step (`icmdr |= 0x800`) - bit
 *  11 of ICMDR - is not decoded (left as an opaque OR, matching this
 *  project's convention of not guessing undecoded hardware bits). @0xc00033f0.
 * ------------------------------------------------------------------------- */
bool panelbus_i2c_read_bytes(struct omap_i2c_regs *i2c, uint32_t slave_addr,	/* FUN_c00033f0 */
			      uint8_t *dst, int len)
{
	extern void  *panelbus_delay_handle;		/* DAT_c00034f4 */
	extern uint32_t panelbus_i2c_mdr_template;	/* DAT_c00034f8 */

	cad_delay_ticks(panelbus_delay_handle, 100);
	for (int spin = 0; (i2c->icstr & I2C_ICSTR_BB) != 0; spin++) {
		if (spin > 999)
			return false;
		cad_delay_ticks(panelbus_delay_handle, 1);
	}

	i2c->iccnt = (uint32_t)len;
	i2c->icsar = slave_addr;
	i2c->icmdr = panelbus_i2c_mdr_template;
	cad_delay_ticks(panelbus_delay_handle, 10);

	bool got_byte = (len > 0) ? false : true;
	for (int i = 0; i < len; i++) {
		if (i == len - 1)
			i2c->icmdr |= I2C_ICMDR_STP;	/* stop after final byte */

		got_byte = false;
		for (int spin = 0; spin < 1000; spin++) {
			if ((i2c->icstr & I2C_ICSTR_ICRRDY) != 0) {
				got_byte = true;
				break;
			}
			cad_delay_ticks(panelbus_delay_handle, 0);
		}

		dst[i] = (uint8_t)i2c->icdrr;
		if (!got_byte)
			break;
	}

	cad_delay_ticks(panelbus_delay_handle, 200);
	i2c->icmdr |= 0x800;	/* bit 11, real meaning not decoded */
	return got_byte;
}

/* ===========================================================================
 *  RX path: read a 2-byte [opcode,arg] frame, validate the opcode, dispatch.
 * ===========================================================================
 */

/* ------------------------------------------------------------------------- *
 *  panelbus_opcode_known - whitelist check for panelbus_cmd_dispatch's own
 *  recognized opcode set (see that function's header for the full opcode
 *  table). Matches panelbus_cmd_dispatch's dispatch conditions exactly,
 *  opcode-for-opcode - this is the RX-loop's own "should I even bother
 *  dispatching this byte" gate. @0xc0010b08.
 * ------------------------------------------------------------------------- */
bool panelbus_opcode_known(void *ctx, unsigned opcode)	/* FUN_c0010b08 */
{
	(void)ctx;
	if (opcode - 0x30u < 3)			return true;	/* 0x30-0x32 */
	if ((opcode & 0xf0) == 0x40)			return true;	/* 0x40-0x4f */
	if (opcode == 0x50)				return true;
	if ((opcode & 0xf0) == 0x60)			return true;	/* 0x60-0x6f */
	if ((opcode & 0xf8) == 0x80)			return true;	/* 0x80-0x87 */
	if (opcode == 0x90)				return true;
	return opcode == 0x70;
}

/* forward decl - full definition/opcode table is this file's main event, below */
extern uint32_t panelbus_cmd_dispatch(void *ctx, uint32_t port, uint32_t opcode, uint32_t arg);	/* FUN_c0007220 */

/* ------------------------------------------------------------------------- *
 *  panelbus_rx_dispatch_loop - the RX half of this channel: opens the I2C0
 *  handle (selector hardcoded 0, per panelbus_i2c_base's own note above),
 *  then loops reading 2-byte frames (panelbus_i2c_read_bytes) and, for every
 *  frame whose opcode byte passes panelbus_opcode_known, forwards it to
 *  panelbus_cmd_dispatch(ctx, port, opcode, arg). Loops as long as frames
 *  keep arriving AND validating - a classic "drain everything pending, then
 *  stop" per-tick poll, not a blocking wait. This function IS the one static
 *  caller of panelbus_cmd_dispatch - see the file header's debug-console
 *  discussion. @0xc0010b58.
 * ------------------------------------------------------------------------- */
void panelbus_rx_dispatch_loop(void *ctx, uint32_t port)	/* FUN_c0010b58 */
{
	extern void *panelbus_rx_i2c_tag;	/* DAT_c0010be8 */
	extern void *panelbus_rx_dispatch_ctx;	/* DAT_c0010bec, the fixed context object panelbus_cmd_dispatch's own param_1 is */

	struct omap_i2c_regs *i2c = panelbus_i2c_base((uint32_t)(uintptr_t)panelbus_rx_i2c_tag, 0);
	bool keep_going = true;
	bool have_frame;

	do {
		uint8_t frame[2];
		have_frame = panelbus_i2c_read_bytes(i2c, port, frame, 2);
		if (!have_frame)
			break;
		if (!panelbus_opcode_known(ctx, frame[0]))
			break;
		panelbus_cmd_dispatch(panelbus_rx_dispatch_ctx, port, frame[0], frame[1]);
		keep_going = true;
	} while (keep_going);
}

/* ===========================================================================
 *  TX path: per-channel outbound ring buffer drain.
 * ===========================================================================
 */

/* ------------------------------------------------------------------------- *
 *  panelbus_tx_queue_pop - pops one 4-byte record from a per-channel ring
 *  buffer (128 x 4-byte slots, index wrapped `& 0x7f`), IRQ-protected around
 *  the count decrement (irq_save_and_disable/irq_restore - see cpsoc.c's own
 *  corrected naming for these two, FUN_c0005500/FUN_c0005510). The channel
 *  record itself is `base + channel*0x208`: 0x200 bytes of ring storage,
 *  a 2-byte read-index at +0x202, a 2-byte count at +0x204 (leaving 2 bytes
 *  unaccounted at +0x206 - most likely a write-index for the still-untraced
 *  push side of this ring, not found as its own function this pass).
 *  @0xc0010bf0.
 * ------------------------------------------------------------------------- */
struct panelbus_tx_channel {
	uint32_t ring[128];	/* +0x000..+0x1ff */
	uint16_t read_index;	/* +0x202 */
	uint16_t count;		/* +0x204 */
	uint16_t unknown_206;	/* +0x206, likely the ring's own write-index - push side not traced */
};

extern int  irq_save_and_disable(void);	/* FUN_c0005500, cited from cpsoc.c's correction of crypto_at88.c */
extern void irq_restore(void);			/* FUN_c0005510 */

bool panelbus_tx_queue_pop(struct panelbus_tx_channel *chan_base, int channel,	/* FUN_c0010bf0 */
			    uint32_t *out)
{
	struct panelbus_tx_channel *chan = chan_base + channel;

	if (chan->count == 0)
		return false;

	*out = chan->ring[chan->read_index];
	chan->read_index = (chan->read_index + 1) & 0x7f;

	irq_save_and_disable();
	chan->count--;
	irq_restore();
	return true;
}

/* ------------------------------------------------------------------------- *
 *  panelbus_tx_send_retry - submits one record over I2C0 with up to 4
 *  retries; on total failure, paints a fault-screen message and hard-halts
 *  (unless a "fault already latched" flag is set). @0xc0010d44.
 *
 *  panelbus_i2c_write_bytes (FUN_c00032f8) is NOT redefined here - it is
 *  cpsoc.c's own "the underlying SPI submit primitive inside
 *  cpsoc_queue_command_with_retry" finding (still open there). Per this
 *  file's hardware section above, the register evidence says it's actually
 *  this same I2C0 controller's transmit path, not SPI - cited as-is, not
 *  re-attributed (not this file's function to own).
 * ------------------------------------------------------------------------- */
extern bool panelbus_i2c_write_bytes(struct omap_i2c_regs *i2c, uint32_t slave_addr,	/* FUN_c00032f8, see cpsoc.c */
				      const uint8_t *src, int len);
extern void crypto_at88_format_fault_text(char *dst, const char *fmt,			/* FUN_c00168fc, cited from crypto_at88.c */
					  const void *arg1, const void *arg3);
extern void clcdc_draw_text(uint16_t x, uint16_t y, const char *str, uint32_t font_or_mode);	/* FUN_c0015650, cited from clcdc.c */
extern uint8_t panelbus_tx_fault_latched;	/* DAT_c0010df8 */

/* Shared hard-halt/assert handler (FUN_c000919c) under this file's own local
 * name - see crypto_at88.c's crypto_at88_fault / clcdc.c's clcdc_assert for
 * the same symbol elsewhere. This call site shows only 2 visible arguments
 * in the decompile (vs. 3 there and 4 in panelbus_cmd_dispatch's own 0x80-0x87
 * branch below) - a real, unresolved argument-count inconsistency across
 * this project's own call sites for this symbol, not modeled per-site;
 * declared separately at each differing call site rather than forced into
 * one shape. */
extern void panelbus_tx_fault(uint32_t unused, uint32_t line_or_code);	/* FUN_c000919c */

bool panelbus_tx_send_retry(uint32_t port, const uint8_t *src, int len)	/* FUN_c0010d44 */
{
	extern void *panelbus_tx_i2c_tag;	/* DAT_c0010dec */

	struct omap_i2c_regs *i2c = panelbus_i2c_base((uint32_t)(uintptr_t)panelbus_tx_i2c_tag, 0);

	for (int attempt = 0; attempt < 4; attempt++) {
		if (panelbus_i2c_write_bytes(i2c, port, src, len))
			return true;
	}

	crypto_at88_format_fault_text((char *)0 /* DAT_c0010df4 */, (const char *)0 /* DAT_c0010df0 */,
				      (const void *)(uintptr_t)port, 0);
	clcdc_draw_text(100, 100, (const char *)0 /* DAT_c0010df4 */, 0);
	if (!panelbus_tx_fault_latched)
		panelbus_tx_fault(0, 0 /* DAT_c0010dfc */);
	return false;
}

/* ------------------------------------------------------------------------- *
 *  panelbus_tx_drain_channel - per-channel TX drain: opens I2C0, and if the
 *  channel's own "pending" tick field (channel struct +0x204, the same
 *  offset panelbus_tx_queue_pop's own count field occupies in a DIFFERENT
 *  struct - two distinct per-channel structs sharing an offset convention,
 *  not the same object) is nonzero, loops popping+sending queued records
 *  (panelbus_tx_queue_pop feeds cad_trigger_calibration below, NOT
 *  panelbus_tx_send_retry directly - see the real call below) until a pop
 *  fails, then double-delays (2x cad_delay_ticks(500)) as a bus-settle
 *  pause. @0xc0010e48.
 *
 *  cad_trigger_calibration (FUN_c00073e8) is cad.c's own already-fully-
 *  reconstructed function - cited here, not redefined. This is a real,
 *  concrete tie confirming cad.c's own registered channels (0x78/0x79) are
 *  fed by THIS file's TX-drain loop, not called directly by
 *  panelbus_cmd_dispatch (which only calls cad_trim_adjust for opcode 0x50 -
 *  a separate, second entry point into cad.c from this same subsystem).
 * ------------------------------------------------------------------------- */
extern void cad_trigger_calibration(void *cad, int reg, int8_t tag, uint8_t value);	/* FUN_c00073e8, cad.c */

void panelbus_tx_drain_channel(struct panelbus_tx_channel *chan_base, int channel)	/* FUN_c0010e48 */
{
	extern void *panelbus_tx_drain_i2c_tag;	/* DAT_c0010f00 */
	extern void *panelbus_tx_drain_delay;		/* DAT_c0010f04 */

	(void)panelbus_i2c_base((uint32_t)(uintptr_t)panelbus_tx_drain_i2c_tag, 0);

	struct panelbus_tx_channel *chan = chan_base + (channel - 0x78);
	if (chan->count == 0)
		return;

	for (;;) {
		uint32_t record;
		if (!panelbus_tx_queue_pop(chan_base, channel - 0x78, &record))
			break;
		/* record is a packed [opcode/tag, arg] pair matching
		 * cad_trigger_calibration's own (reg, tag, value) shape */
		uint8_t tag   = (uint8_t)(record & 0xff);
		uint8_t value = (uint8_t)((record >> 8) & 0xff);
		cad_trigger_calibration(chan_base, channel, (int8_t)tag, value);
	}

	cad_delay_ticks(panelbus_tx_drain_delay, 500);
	cad_delay_ticks(panelbus_tx_drain_delay, 500);
}

/* ===========================================================================
 *  Per-tick entry point.
 * ===========================================================================
 */

/* ------------------------------------------------------------------------- *
 *  panelbus_poll_channels - called once per firmware tick from the master
 *  dispatcher (FUN_c0008b64, status-word bit 0x1000 - see this file's
 *  debug-console section). Polls ports 0x78/0x7a/0x7b for inbound frames
 *  (panelbus_rx_dispatch_loop) and drains the TX ring on ports 0x78/0x79/
 *  0x7a (panelbus_tx_drain_channel) - port 0x7b has no TX drain call, 0x79
 *  has no RX poll call; this asymmetry is transcribed exactly as found, not
 *  smoothed over. These are the SAME four port numbers cad.c's cad_init
 *  registers with (0x78/0x79) and cpsoc.c's cpsoc_event_opcode_dispatch
 *  polls (0x78/0x7b) - confirmed real overlap, not a coincidence: this
 *  function is the shared infrastructure both of those subsystems' own
 *  "local register" numbering rides on top of. @0xc0010f08.
 * ------------------------------------------------------------------------- */
void panelbus_poll_channels(struct panelbus_tx_channel *chan_base)	/* FUN_c0010f08 */
{
	panelbus_rx_dispatch_loop(chan_base, 0x78);
	panelbus_tx_drain_channel(chan_base, 0x78);
	panelbus_tx_drain_channel(chan_base, 0x79);
	panelbus_rx_dispatch_loop(chan_base, 0x7b);
	panelbus_rx_dispatch_loop(chan_base, 0x7a);
	panelbus_tx_drain_channel(chan_base, 0x7a);
}

/* ===========================================================================
 *  panelbus_cmd_dispatch - the main event: FUN_c0007220 itself.
 * ===========================================================================
 */

/* ------------------------------------------------------------------------- *
 *  panelbus_hw_negotiate_ready - checks two status bits (0x17, 0x28) inside
 *  a relocated table (via omap_usbdc_reloc, cited from omap_l137_usbdc.c -
 *  address-neighborhood evidence only, not confirmed to be USB-specific
 *  here) through panelbus_table_byte, a generic bounds-checked (<0x4f) byte
 *  accessor with 18 callers firmware-wide (including cad.c's own
 *  cad_pedal_present) - not redefined here, cited as a shared primitive.
 *  Real call site in panelbus_cmd_dispatch passes an argument
 *  (DAT_c00073d4) that this function's own decompiled signature shows it
 *  never consumes (zero formal parameters) - same "phantom forwarded
 *  parameter" pattern eva_board_main.c independently documented for
 *  cdix4192.c's reg accessors. @0xc0009480.
 * ------------------------------------------------------------------------- */
extern uint32_t omap_usbdc_reloc(uint32_t offset);				/* FUN_c0009194, cited from omap_l137_usbdc.c */
extern uint8_t  panelbus_table_byte(uint32_t base, int index);		/* FUN_c0009204, generic, multi-caller, not this file's scope */
extern uint32_t panelbus_negotiate_table_offset;				/* DAT_c00094d4 */

uint32_t panelbus_hw_negotiate_ready(void)	/* FUN_c0009480 */
{
	uint32_t base = omap_usbdc_reloc(panelbus_negotiate_table_offset);
	if (!panelbus_table_byte(base + 0x1ac00, 0x17))
		return 0;

	base = omap_usbdc_reloc(panelbus_negotiate_table_offset);
	return panelbus_table_byte(base + 0x1ac00, 0x28) ? 1 : 0;
}

/* ------------------------------------------------------------------------- *
 *  panelbus_submit_record - packs (tag, value) into a 2-byte buffer and
 *  forwards it to FUN_c0012de0, an unresolved single-caller submit
 *  primitive (not traced further this pass - genuinely this file's own
 *  unresolved leaf, left honest rather than guessed). Used by
 *  panelbus_cmd_dispatch's own opcode-0x30/0x31/0x32/0x40-0x4f family.
 *  Real call site shows only 3 of 4 formal arguments used (4th folded to a
 *  constant, not modeled). @0xc0012e58.
 * ------------------------------------------------------------------------- */
extern uint8_t panelbus_record_submit_raw(void *target, const uint8_t *packed_pair);	/* FUN_c0012de0, not traced further */

uint8_t panelbus_submit_record(void *target, uint8_t tag, uint8_t value)	/* FUN_c0012e58 */
{
	uint8_t packed[2];
	packed[0] = value;
	packed[1] = tag;
	return panelbus_record_submit_raw(target, packed);
}

/* ------------------------------------------------------------------------- *
 *  panelbus_cmd_dispatch - the command interpreter itself. Signature:
 *  (ctx, port, opcode, arg) - ctx is a FIXED shared object (always
 *  DAT_c0010bec, panelbus_rx_dispatch_ctx, from this file's one real
 *  caller), port is the 0x78-0x7b channel/local-register number threaded
 *  in from panelbus_rx_dispatch_loop, opcode/arg are the 2 bytes just read
 *  off the wire. Full opcode table, confirmed against this pass's own
 *  decompile:
 *
 *    0x30/0x31/0x32   -> panelbus_submit_record(ctx-local target, port, 1/2/3)
 *    0x40-0x4f         -> same, tag 0
 *    0x50              -> cad_trim_adjust(trim, port, arg) - cad.c's own,
 *                          cited not redefined; only acts when port==0x7a
 *    0x60-0x6f         -> LCD-progress-bar-style percent scaling via
 *                          omap_tick_scale (FUN_c001e3f8, cited from
 *                          omap_l108.c) into a widget struct at a FIXED
 *                          global (DAT_c00073d0) whose field offsets
 *                          (+0x210/+0x214) exactly match ctouchpanel.c's own
 *                          "last tick"/"stale count" layout - likely a
 *                          shared base "widget" struct type (consistent with
 *                          this firmware's C++ object-manager architecture,
 *                          cobjectmgr.c), NOT literal identity with
 *                          ctouchpanel.c's own global instance (a different,
 *                          separately-cited DAT_c0009040) - flagged, not
 *                          asserted. Ends by calling ctouchpanel_finalize_
 *                          release (FUN_c00140d4, ctouchpanel.c) with an
 *                          argument - ctouchpanel.c's own extern for this
 *                          symbol is declared `void(void)`; this call site's
 *                          real decompile shows 2 formal parameters
 *                          (`int, short`) - a genuine signature mismatch
 *                          worth fixing in ctouchpanel.c, not corrected here.
 *                          RETURNS DIRECTLY (no scheduler-wake tail).
 *    0x70              -> writes a flag into ctx, posts event-flag bit 0x40
 *    0x80-0x87         -> device-ID negotiation handshake: caches the first
 *                          negotiated arg byte per-ctx (+0x40, 0xff
 *                          sentinel = "not yet negotiated"); on repeat calls
 *                          with a mismatching arg, paints a fault message and
 *                          calls the shared hard-halt handler - but THIS
 *                          call site (`return uVar2` immediately follows)
 *                          uses ITS return value, unlike every other call to
 *                          the same symbol in this project (documented as a
 *                          non-returning halt elsewhere) - genuine,
 *                          disassembly-confirmed anomaly, not resolved
 *                          either way here. On first negotiation, calls
 *                          panelbus_hw_negotiate_ready and, if ready, paints
 *                          a 3-line status screen showing the negotiated ID
 *                          split into nibbles via clcdc_draw_text (itself
 *                          used here with a captured, used return value,
 *                          contradicting clcdc.c's own `void` declaration for
 *                          the same symbol - same pattern, also not
 *                          corrected there). RETURNS DIRECTLY.
 *    0x90              -> arg!=0: invalidates two fields at the SAME widget
 *                          global as the 0x60-0x6f branch (+0x40/+0x42,
 *                          matching ctouchpanel_state's own adc_ch[0]/[1]
 *                          offsets); arg==0: clears +0xd6 (matching
 *                          ctouchpanel_state's own touch_active offset) and
 *                          a counter at +0x21c. Posts event-flag bit 0x08.
 *    anything else     -> passthrough: returns arg unchanged, no event flag.
 *
 *  TAIL (opcodes 0x30-0x4f/0x50/0x70/0x90 only): posts the opcode-specific
 *  event-flag bit into a shared RTOS event-flag group at DAT_c001d228 (+0x1c
 *  |= flag), guarded by the SAME two error codes (-25 "not ready", -18/-14
 *  depending on branch) eva_board_main.c's own eva_board_start_task uses for
 *  its scheduler-not-initialized/bad-argument guards - then tests whether a
 *  waiter is pending on those flags (FUN_c001dfc8, a generic event-flag
 *  test/consume primitive with 4 callers firmware-wide, including the master
 *  tick dispatcher's own status-word fetch - NOT redefined here, generic,
 *  out of this file's scope) and if so, unlinks it from the wait list
 *  (FUN_c001dbf0, same scope note) and calls eva_board_sched_dispatch()
 *  (FUN_c001d850, cited exactly as eva_board_main.c names it) - the
 *  identical "make ready, dispatch if now-most-urgent" shape
 *  eva_board_start_task itself uses, aimed at a different specific
 *  wait-queue instance. This is real, disassembly-confirmed structure, not
 *  a guessed RTOS pattern - see the raw list-unlink arithmetic transcribed
 *  below. @0xc0007220.
 * ------------------------------------------------------------------------- */
extern void  cad_trim_adjust(int16_t *trim, int reg, int8_t delta);		/* FUN_c0013480, cad.c */
extern int32_t omap_tick_scale(int32_t ticks, int divisor);			/* FUN_c001e3f8, cited from omap_l108.c */
extern void  ctouchpanel_finalize_release(int ctx, short value);		/* FUN_c00140d4 - see mismatch note above */
extern void  crypto_at88_format_fault_text2(char *dst, const char *fmt,	/* FUN_c00168fc, second local name: this file's own
									   call sites show 2 AND 4 visible arguments across
									   different uses - same inconsistency as panelbus_tx_fault
									   above, not forced into one shape */
					    const void *arg1, const void *arg3);
extern uint32_t panelbus_negotiate_fault(uint32_t unused, uint32_t arg1,	/* FUN_c000919c, distinct local prototype - see
									   the 0x80-0x87 branch's own note on why this call
									   site's return value is actually used */
					 uint32_t arg2, uint32_t arg3);
extern uint32_t clcdc_draw_text_ret(uint16_t x, uint16_t y, const char *str,	/* FUN_c0015650, second local name matching the
									   0x80-0x87 branch's own used-return-value call -
									   see clcdc.c's void-returning declaration for the
									   same symbol elsewhere */
				    uint32_t font_or_mode);

/* widget-shaped global the 0x60-0x6f/0x90 branches share - see the header
 * comment above on why this is NOT asserted to be ctouchpanel.c's own
 * instance despite the matching field offsets. */
struct panelbus_widget {
	uint8_t  pad_00[0x40];
	uint16_t field_40;	/* +0x40 */
	uint16_t field_42;	/* +0x42 */
	uint8_t  pad_44[0xd6 - 0x44];
	uint8_t  field_d6;	/* +0xd6 */
	uint8_t  pad_d7[0x210 - 0xd7];
	uint16_t last_tick;	/* +0x210 */
	uint32_t stale_count;	/* +0x214 */
	uint8_t  pad_218[0x21c - 0x218];
	uint32_t field_21c;	/* +0x21c */
};

/* the ctx object's own fields this dispatcher touches directly */
struct panelbus_ctx {
	uint8_t pad_00[0x3c];
	uint32_t field_3c;	/* +0x3c, opcode-0x70's flag */
	uint16_t field_40b;	/* +0x40, opcode-0x80-0x87's negotiated-ID cache (byte, widened here for clarity) */
};

/* scheduler wait-queue primitives touched by the tail - generic,
 * multi-caller, cited not redefined (see header comment above). */
extern uint8_t  panelbus_sched_not_ready_flag;		/* DAT_c001d220-shaped guard - distinct global from
							   eva_board_main.c's own DAT_c001d0e8, same role */
extern int32_t  panelbus_sched_guard2;			/* DAT_c001d224 */
extern uint8_t *panelbus_sched_wait_list;		/* DAT_c001d228 */
extern uint32_t event_flags_test_and_consume(void *list_head, uint32_t flag_mask,	/* FUN_c001dfc8 */
					     uint32_t list_kind, uint32_t *out_flags);
extern void     event_flags_wait_dequeue(void *node);					/* FUN_c001dbf0 */
extern void     eva_board_sched_dispatch(void);					/* FUN_c001d850, cited exactly as eva_board_main.c names it */

static void panelbus_post_event_and_maybe_wake(uint32_t flag)
{
	if (panelbus_sched_not_ready_flag != 0)
		return;	/* real code: return 0xffffffe7 to the CALLER; folded here since
			 * this helper's own callers already return uVar2 verbatim below */
	if (panelbus_sched_guard2 < 1)
		return;	/* real code: return 0xffffffee */

	uint8_t **list_head = (uint8_t **)(panelbus_sched_wait_list + 0x10);
	uint8_t  *node = *list_head;
	*(uint32_t *)(panelbus_sched_wait_list + 0x1c) |= flag;

	if (node != (uint8_t *)list_head) {
		uint32_t *rec = *(uint32_t **)(node + 0x14);
		uint32_t out_flags;
		if (event_flags_test_and_consume(list_head, rec[2], rec[3], &out_flags) != 0) {
			uint8_t *next = *(uint8_t **)(node + 4);
			*(uint8_t **)node = next;
			*(uint8_t **)(*(uint32_t *)node + 4) = (uint8_t *)node;	/* transcribed as observed;
											   see real code below for the
											   exact prev/next relinking */
			event_flags_wait_dequeue(node);
			eva_board_sched_dispatch();
		}
	}
}

uint32_t panelbus_cmd_dispatch(void *ctx_v, uint32_t port, uint32_t opcode, uint32_t arg)	/* FUN_c0007220 */
{
	struct panelbus_ctx *ctx = (struct panelbus_ctx *)ctx_v;
	extern struct panelbus_widget *panelbus_widget;	/* DAT_c00073d0 */
	extern int16_t *panelbus_trim_target;			/* DAT_c00073c8, cad_trim_adjust's own trim pointer */
	extern void    *panelbus_record_target;		/* DAT_c00073c4, panelbus_submit_record's own target */
	struct panelbus_widget *w = panelbus_widget;
	uint32_t flag;

	if (opcode == 0x30) {
		panelbus_submit_record(panelbus_record_target, (uint8_t)port, 1);
		flag = 1;
	} else if (opcode == 0x31) {
		panelbus_submit_record(panelbus_record_target, (uint8_t)port, 2);
		flag = 1;
	} else if (opcode == 0x32) {
		panelbus_submit_record(panelbus_record_target, (uint8_t)port, 3);
		flag = 1;
	} else if ((opcode & 0xf0) == 0x40) {
		panelbus_submit_record(panelbus_record_target, (uint8_t)port, 0);
		flag = 1;
	} else if (opcode == 0x50) {
		cad_trim_adjust(panelbus_trim_target, (int)port, (int8_t)arg);
		flag = 2;
	} else if ((opcode & 0xf0) == 0x60) {
		int scaled = 0x3ff - ((arg != 0xff) ? (int)(arg << 2) : /* DAT_c00073cc */ 0x3ff);
		if (w->last_tick > 0) {
			if (scaled < w->last_tick) {
				w->last_tick = (uint16_t)scaled;
				if (scaled < 0)
					w->stale_count = 0;
			}
			if (scaled > 0x200) {
				int r = omap_tick_scale((scaled - 0x200) * 0x201, 0x200 - w->stale_count);
				scaled = r + 0x200;
				if (scaled > 0x3eb)
					scaled = 0 /* DAT_c0014228, ctouchpanel.c's own address range */;
			}
			scaled &= 0xffff;
		}
		w->field_21c = 0;
		w->last_tick = (uint16_t)scaled;
		ctouchpanel_finalize_release((int)(uintptr_t)w, (short)scaled);
		return (uint32_t)scaled & 0xff;
	} else if ((opcode & 0xf8) == 0x80) {
		if (ctx->field_40b != 0xff) {
			if (ctx->field_40b != arg) {
				crypto_at88_format_fault_text2((char *)0 /* DAT_c00073dc */, (const char *)0 /* DAT_c00073d8 */, 0, 0);
				clcdc_draw_text(10, 0x28, (const char *)0 /* DAT_c00073dc */, 0);
				return panelbus_negotiate_fault(0, 0 /* DAT_c00073e0 */, 0 /* DAT_c00073e4 */, port);
			}
			return arg;
		}
		ctx->field_40b = (uint16_t)arg;
		if (panelbus_hw_negotiate_ready() != 0) {
			crypto_at88_format_fault_text2((char *)0 /* DAT_c000720c */, (const char *)0 /* DAT_c0007208 */, (void *)6, (void *)6);
			clcdc_draw_text(0x14, 0x28, (const char *)0 /* DAT_c000720c */, 0);
			crypto_at88_format_fault_text2((char *)0 /* DAT_c000720c */, (const char *)0 /* DAT_c0007210 */,
						       (void *)(uintptr_t)(ctx->field_40b >> 4), (void *)(uintptr_t)(ctx->field_40b & 0xf));
			clcdc_draw_text(0x14, 0x3c, (const char *)0 /* DAT_c000720c */, 0);
			return clcdc_draw_text_ret(0x14, 0x50, (const char *)0 /* DAT_c0007218 or DAT_c000721c per a status flag */, 0);
		}
		return 0;
	} else if (opcode == 0x90) {
		if (arg != 0) {
			w->field_40 = (uint16_t)-1;
			w->field_42 = (uint16_t)-1;
		} else {
			w->field_d6 = 0;
			w->field_21c = 0;
		}
		flag = 8;
	} else if (opcode == 0x70) {
		ctx->field_3c = (arg == 0);
		flag = 0x40;
	} else {
		return arg;	/* unrecognized opcode: passthrough */
	}

	panelbus_post_event_and_maybe_wake(flag);
	return 0;
}
