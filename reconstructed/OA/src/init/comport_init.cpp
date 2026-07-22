// SPDX-License-Identifier: GPL-2.0
/*
 * comport_init.cpp  -  CSTGComPort::Initialize(). See oa_comport.h for
 * the ground-truthing details and this reconstruction's own stated
 * scope decision (a genuinely large, heavily-branching function with
 * severe register/variable reuse across paths -- the CONFIRMED,
 * high-confidence core is implemented faithfully; two comPortId
 * values' additional LDN-specific special-case blocks are a
 * documented, deliberate gap).
 *
 * Traced via a full objdump -d -r disassembly (.text+0xaa90, 2561
 * bytes) plus a direct extraction of every .rodata lookup/jump table
 * it references. Confirmed real algorithm:
 *
 *   1. Bounds-check comPortId <= 5 (hard fail otherwise, matching
 *      CSTGKeybedInterface_Startup's own confirmed 6-port scan).
 *   2. Look up a per-port "select byte" (.rodata+0x16e[comPortId] --
 *      confirmed real values: 1 for port 0, 0 for every other port)
 *      and cache it into CW83627::sInstance.
 *   3. If a Super-I/O chip has already been detected (CW83627::
 *      sInstance.found), reuse the cached config port; otherwise probe
 *      0x2E first (unlock sequence [0x87,0x01,0x55,0x55], then select
 *      register 0x20 and read a chip-ID byte from the data port,
 *      confirmed real validity range [1, 0xFE]), and if that fails,
 *      probe 0x4E (same sequence but the unlock's 4th byte is 0xAA,
 *      confirmed different from 0x2E's -- a real, not assumed,
 *      asymmetry). If both fail, Initialize returns 0.
 *   4. Re-enter config mode and select the target LDN (register 0x07 =
 *      a confirmed per-port mapping: port 0->LDN1, ports 1/2->LDN2,
 *      port 3->LDN3, port 4->LDN6, port 5->LDN0xD).
 *   5. Read the LDN's base address registers (0x60 = high byte, 0x61 =
 *      low byte, confirmed via the exact shift/or sequence used to
 *      combine them) to get the UART's I/O port base; validate it's in
 *      [0x100, 0xFF8] and 8-byte aligned (confirmed real checks), else
 *      fail.
 *   6. Re-enter config mode, re-select the LDN, read the assigned IRQ
 *      (register 0x70, masked to 4 bits), exit config mode (register
 *      0x02 = 0x02).
 *   7. Hardware-reset the UART itself (clear MCR/IER, read-and-discard
 *      LSR/RBR/MSR/IIR to clear any latched state).
 *   8. `rtwrap_request_irq`; on success, `rtwrap_assign_irq_to_cpu`
 *      (confirmed hardcoded to CPU 1).
 *   9. Bring the UART up: set the baud rate divisor (the exact same
 *      DLAB dance as `SetBaudRate`, reused here since it's the
 *      identical confirmed register sequence), LCR = 3 (8N1), FCR =
 *      fifoThresholdCode | 7 (enable + clear both FIFOs), `rtwrap_
 *      startup_irq`, MCR = 8 (OUT2, needed for the PC-style interrupt
 *      line), IER = 3 (RX-available + THRE interrupts enabled).
 *  10. `irqEnabled = 1`, return 1 (success).
 *
 * comPortId==3's special block (2026-07-22, now reconstructed -- see
 * below): decoded byte-for-byte from `.text+0xb2b8`..`+0xb349` and
 * cross-checked against the public Nuvoton NCT6776F/D datasheet (this
 * chip's documented sibling family -- see kronos_keybed_serial_protocol
 * memory / OA.ko_keybed_serial_protocol.md for the full writeup). It is
 * NOT a GPIO/pin-mux block as originally guessed -- it is a write to
 * three real *Global* (NOLDN) Chip Control Registers, which the
 * datasheet confirms live in CR 0x02-0x2F and are NOT gated by LDN
 * selection (i.e. this fires regardless of which LDN reg 0x07 currently
 * points at):
 *
 *   - CR25h (Interface Tri-state Enable) <- 0x00: clears UARTA/UARTB
 *     tri-state (bits 2/3) -- un-floats the physical serial pins so
 *     they actively drive/receive instead of high-impedance.
 *   - CR22h (Device Power Down)          <- 0xFF: ensures UARTA/UARTB
 *     (bits 4/5) are NOT powered down, and IPD (bit1) stays "not
 *     immediate power down".
 *   - CR10h/CR11h/CR13h/CR14h (Device IRQ Type/Polarity Selection)
 *     <- 0x00/0x00/0xFF/0xFF: sets edge-triggered (not level) and
 *     active-low (not active-high) for every IRQ channel -- the
 *     INVERSE of their FFh/FFh/00h/00h power-on-reset defaults.
 *     Gated by the datasheet's own documented CR26h[bit4] unlock/lock
 *     dance (read CR26, OR in bit4, write back; ... write CR10/11/13/14
 *     ...; read CR26 again, AND out bit4, write back) -- this project's
 *     disassembly matches that exact vendor-documented procedure
 *     instruction-for-instruction.
 *
 * comPortId 0's distinct extra step remains a documented, deliberate
 * gap (not yet decoded).
 *
 * NOTE (2026-07-22, live-hardware-checked): this block was originally
 * flagged as a candidate explanation for the keybed UART's
 * noise-shaped RX data (comPortId 3's Initialize() always runs before
 * comPortId 5's on real hardware, so its side effect would fire first
 * -- IF comPortId 3 ever got that far). A live read-only probe against
 * the real production chip (Winbond W83627UHG, confirmed via the
 * kernel's own `w83627ehf` binding banner) showed LDN 3 is **inactive
 * with base=0x0000** on real hardware -- `Initialize(3)`'s own
 * base-address validity check (`(newIoBase-0x100) > 0xef8`) fails
 * immediately, so this block never actually executes in practice. It
 * is kept here purely because it's a real, disassembly-confirmed part
 * of `CSTGComPort::Initialize()` (faithful reconstruction is the
 * point), but it is NOT the explanation for the keybed UART's earlier
 * "noise-shaped" result -- see kronos_keybed_serial_protocol memory /
 * OA.ko_keybed_serial_protocol.md for what that turned out to be
 * (nothing: a live production-hardware capture showed LDN 0xD
 * delivering clean, zero-framing-error, correctly-typed protocol
 * messages the whole time).
 */

#include "oa_comport.h"

/* TEMPORARY debugging aid (2026-07-21) -- NOT part of the real
 * reconstruction, to be deleted once the real-hardware keybed-handshake
 * failure is root-caused. Matches debug_marker.cpp's own established
 * regparm(0) printk convention (this build is -mregparm=3 throughout,
 * but the real kernel printk is cdecl/asmlinkage). */
extern "C" __attribute__((regparm(0))) int printk(const char *fmt, ...);

CW83627 CW83627::sInstance;

static const int kLdnByPort[6] = { 1, 2, 2, 3, 6, 0xd };
static const unsigned char kSelectByteByPort[6] = { 1, 0, 0, 0, 0, 0 };

static void WriteConfigReg(unsigned int configPort, unsigned char reg, unsigned char value)
{
	stg_outb(configPort, reg);
	stg_outb(configPort + 1, value);
}

static unsigned char ReadConfigReg(unsigned int configPort, unsigned char reg)
{
	stg_outb(configPort, reg);
	return stg_inb(configPort + 1);
}

/* Confirmed real: the unlock sequence's 4th byte differs between the
 * two possible config ports (0x55 for 0x2E, 0xAA for 0x4E) -- not
 * assumed symmetric. */
static void EnterConfig(unsigned int configPort)
{
	stg_outb(configPort, 0x87);
	stg_outb(configPort, 0x01);
	stg_outb(configPort, 0x55);
	stg_outb(configPort, (configPort == 0x4e) ? 0xaa : 0x55);
}

static void ExitConfig(unsigned int configPort)
{
	WriteConfigReg(configPort, 0x02, 0x02);
}

static bool DetectChipAt(unsigned int configPort)
{
	EnterConfig(configPort);
	unsigned char id = ReadConfigReg(configPort, 0x20);
	/* Confirmed real validity range: (id - 1) <= 0xfd, i.e. id in
	 * [1, 0xfe] -- a generic "something plausible answered" check,
	 * not a specific chip-ID match. */
	return (unsigned char)(id - 1) <= 0xfd;
}

char CSTGComPort::Initialize(eComPortId comPortIdArg, eBaudRateCode baudRateCodeArg,
			      eReceiveFifoThresholdCode fifoThresholdCodeArg)
{
	int cpid = (int)comPortIdArg;
	int baudRateCode = (int)baudRateCodeArg;
	int fifoThresholdCode = (int)fifoThresholdCodeArg;

	comPortId = cpid;
	if ((unsigned int)cpid > 5)
		return 0;

	CW83627::sInstance.selectByte = kSelectByteByPort[cpid];

	unsigned int configPort;
	if (CW83627::sInstance.found) {
		configPort = CW83627::sInstance.configPort;
	} else if (DetectChipAt(0x2e)) {
		configPort = 0x2e;
	} else if (DetectChipAt(0x4e)) {
		configPort = 0x4e;
	} else {
		printk("<3>OA_COMPORT_DBG port %d: no W83627 Super I/O chip found at 0x2e or 0x4e\n", cpid);
		return 0;
	}
	CW83627::sInstance.found = 1;
	CW83627::sInstance.configPort = configPort;

	int ldn = kLdnByPort[cpid];

	/* Base address discovery. */
	EnterConfig(configPort);
	WriteConfigReg(configPort, 0x07, (unsigned char)ldn);
	unsigned int baseHigh = ReadConfigReg(configPort, 0x60);
	unsigned int baseLow = ReadConfigReg(configPort, 0x61);
	unsigned int newIoBase = (baseHigh << 8) | baseLow;
	if ((newIoBase - 0x100) > 0xef8 || (newIoBase & 7) != 0) {
		printk("<3>OA_COMPORT_DBG port %d: chip found at config 0x%x, LDN %d, but bad ioBase 0x%x\n",
		       cpid, configPort, ldn, newIoBase);
		return 0;
	}
	ioBase = newIoBase;

	/* comPortId==3 (LDN 3, UART B) special block -- see this file's
	 * header comment. Global (NOLDN) registers, not gated by the LDN
	 * select above; config port is still open from the base-address
	 * read. */
	if (cpid == 3) {
		WriteConfigReg(configPort, 0x25, 0x00); /* Interface Tri-state Enable: un-float UARTA/UARTB */
		WriteConfigReg(configPort, 0x22, 0xff); /* Device Power Down: UARTA/UARTB not powered down */

		unsigned char cr26 = ReadConfigReg(configPort, 0x26);
		WriteConfigReg(configPort, 0x26, cr26 | 0x10); /* unlock CR10/11/13/14 per datasheet Note1 */

		WriteConfigReg(configPort, 0x10, 0x00); /* Device IRQ TYPE Selection: all edge-triggered */
		WriteConfigReg(configPort, 0x11, 0x00); /* Device IRQ TYPE Selection (HM/WDTO/SMI): edge */
		WriteConfigReg(configPort, 0x13, 0xff); /* Device IRQ Polarity Selection <15:8>: active-low */
		WriteConfigReg(configPort, 0x14, 0xff); /* Device IRQ Polarity Selection <7:0>: active-low */

		cr26 = ReadConfigReg(configPort, 0x26);
		WriteConfigReg(configPort, 0x26, cr26 & ~0x10); /* re-lock */
	}

	/* IRQ discovery. */
	EnterConfig(configPort);
	WriteConfigReg(configPort, 0x07, (unsigned char)ldn);
	unsigned int irq = ReadConfigReg(configPort, 0x70) & 0xf;
	irqNumber = irq;
	ExitConfig(configPort);

	printk("<3>OA_COMPORT_DBG port %d: chip@0x%x LDN %d ioBase=0x%x irq=%d\n",
	       cpid, configPort, ldn, ioBase, irq);

	/* Hardware-reset the UART: clear MCR/IER, read-and-discard
	 * LSR/RBR/MSR/IIR to clear any latched status. */
	stg_outb(ioBase + 4, 0);
	stg_outb(ioBase + 1, 0);
	stg_inb(ioBase + 5);
	stg_inb(ioBase);
	stg_inb(ioBase + 6);
	stg_inb(ioBase + 2);

	if (rtwrap_request_irq(irq, &CSTGComPort::RTAIInterruptHandler, this, 0) != 0) {
		printk("<3>OA_COMPORT_DBG port %d: rtwrap_request_irq(%d) failed\n", cpid, irq);
		return 0;
	}
	rtwrap_assign_irq_to_cpu(irq, 1);

	/* Final UART bring-up -- the exact same DLAB/baud-rate dance as
	 * SetBaudRate, reused since it's the identical confirmed register
	 * sequence. */
	unsigned char lcr = stg_inb(ioBase + 3);
	stg_outb(ioBase + 3, lcr | 0x80);
	stg_outb(ioBase + 1, (unsigned char)(baudRateCode >> 8));
	stg_outb(ioBase + 0, (unsigned char)baudRateCode);
	lcr = stg_inb(ioBase + 3);
	stg_outb(ioBase + 3, lcr & 0x7f);

	stg_outb(ioBase + 3, 3);				  /* LCR: 8N1 */
	stg_outb(ioBase + 2, (unsigned char)(fifoThresholdCode | 7)); /* FCR */
	rtwrap_startup_irq(irq);
	stg_outb(ioBase + 4, 8); /* MCR: OUT2 */
	stg_outb(ioBase + 1, 3); /* IER: RX + THRE */

	irqEnabled = 1;
	return 1;
}
