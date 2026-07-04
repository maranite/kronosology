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
 * NOT reconstructed (a real, deliberate, documented gap -- see
 * oa_comport.h's own note): comPortId 0 and 3 are confirmed via
 * `.rodata`'s own per-port flag tables to have ADDITIONAL LDN-specific
 * config beyond the generic sequence above (a GPIO-register-shaped
 * block for port 3's LDN touching registers 0x10/0x11/0x13/0x14/0x22/
 * 0x25/0x26, and a distinct extra step for port 0). This
 * reconstruction takes the generic path for all 6 ports instead.
 */

#include "oa_comport.h"

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
	if ((newIoBase - 0x100) > 0xef8 || (newIoBase & 7) != 0)
		return 0;
	ioBase = newIoBase;

	/* IRQ discovery. */
	EnterConfig(configPort);
	WriteConfigReg(configPort, 0x07, (unsigned char)ldn);
	unsigned int irq = ReadConfigReg(configPort, 0x70) & 0xf;
	irqNumber = irq;
	ExitConfig(configPort);

	/* Hardware-reset the UART: clear MCR/IER, read-and-discard
	 * LSR/RBR/MSR/IIR to clear any latched status. */
	stg_outb(ioBase + 4, 0);
	stg_outb(ioBase + 1, 0);
	stg_inb(ioBase + 5);
	stg_inb(ioBase);
	stg_inb(ioBase + 6);
	stg_inb(ioBase + 2);

	if (rtwrap_request_irq(irq, &CSTGComPort::RTAIInterruptHandler, this, 0) != 0)
		return 0;
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
