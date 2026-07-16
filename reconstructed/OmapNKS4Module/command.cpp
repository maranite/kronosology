// SPDX-License-Identifier: GPL-2.0
/*
 * command.cpp  -  COmapNKS4Command: the NKS4 panel wire protocol.
 *
 *  Each request is a 32-bit "command word"  opcode<<24 | reg<<16 | dataHi<<8 | dataLo
 *  handed to the panel via WaitForNKS4CommandWrite()/SubmitNKS4CommandWrite() (the word
 *  travels in EAX under -mregparm=3).  Query commands then wait for a response word and
 *  validate its high 16 bits (the reg echo):
 *      0x0066  comm-test ACK     0x0070  version       0x0171  port read
 *
 *  Command-register bytes recovered from the binary:
 *      0xee  communication check          0xf0  get version (reg=index)
 *      0x90  set #analog inputs           0x70  set #LED banks
 *      0x00  configure scanning (reg=enable bitmask, no opcode byte - confirmed real)
 *      0x71  read port config (reg=0x71, folded into opcode 0x01 - see driver.cpp's
 *            ReceiveEventBuffer idx==0x71 special case, NOT a generic idx<<16 echo)
 *      0xB0  set all analog input filter (opcode 0x01, data=(a<<4)|b)
 *      0x80  set rotary encoder sample speed (opcode 0x00, data=n)
 *      0x81/0x83/0x82  configure rotary encoders (opcode 0x01, 3-word sequence)
 *      0xC7  set LCD brightness (reg=level itself, not a data byte)
 *      0x06  reset module (reg=mode itself, not a data byte)
 *  All setter regs re-verified via fresh Ghidra decompile + disassembly, 2026-07-15
 *  (OmapNKS4Module.ko 3.2.2) - see KronosNKS4/docs/gaps.md "Setter command word
 *  encodings - RESOLVED" for the full derivation and what was wrong before.
 */

#include "omapnks4_internal.h"

namespace COmapNKS4Command {

/* ---- queries ----------------------------------------------------------- */

bool CommunicationCheck(void)
{
	unsigned int resp = 0;

	if (SubmitNKS4CommandWrite(0x00ee0000) == 0 &&
	    WaitForNKS4ReadEvent(&resp) == 0 &&
	    (unsigned short)(resp >> 16) == 0x0066)		/* IsCommunicationTestAckResponse */
		return true;

	printk("<6>OmapNKS4:%s: line %d: Comm check - bad response, sent 0x%08lx, rcvd 0x%08lx\n\n",
	       "CommunicationCheck", 0xd0, 0x00ee0000, resp);
	return false;
}

/* GetVersion(index): reg 0xf0; response 0x70, value=byte1, revision=byte0. */
bool GetVersion(int index, unsigned char *version, unsigned char *revision)
{
	unsigned int cmd = 0x00f00000 | ((index << 8) & 0xff00);
	unsigned int resp = 0;

	if (SubmitNKS4CommandWrite(cmd) == 0 &&
	    WaitForNKS4ReadEvent(&resp) == 0 &&
	    (unsigned short)(resp >> 16) == 0x0070) {
		*version  = (unsigned char)(resp >> 8);
		*revision = (unsigned char)(resp);
		return true;
	}
	return false;
}

/* 4-out variant: reads two version/revision pairs at once (OMAP + PSoC). */
bool GetVersion(unsigned char *aV, unsigned char *aR, unsigned char *bV, unsigned char *bR)
{
	unsigned int resp = 0;

	if (SubmitNKS4CommandWrite(0x00f00000) == 0 &&
	    WaitForNKS4ReadEvent(&resp) == 0 &&
	    (unsigned short)(resp >> 16) == 0x0070) {
		*aV = (unsigned char)(resp >> 8);  *aR = (unsigned char)(resp);
		/* second pair follows in the same response stream */
		if (WaitForNKS4ReadEvent(&resp) == 0) {
			*bV = (unsigned char)(resp >> 8);  *bR = (unsigned char)(resp);
		}
		return true;
	}
	return false;
}

/* ReadPortConfiguration: response 0x171; decodes is-88-key + hardware version. */
bool ReadPortConfiguration(bool *is88, unsigned char *hwVer)
{
	unsigned int resp = 0;
	bool ok = (SubmitNKS4CommandWrite(0x01710000) == 0 &&
		   WaitForNKS4ReadEvent(&resp) == 0 &&
		   (unsigned short)(resp >> 16) == 0x0171);
	*is88 = false;
	if (ok) {
		*hwVer = (unsigned char)(resp);
		*is88  = ((resp >> 8) & 1) != 0;
	}
	return ok;
}

/* ---- setters (validate range, then send a single command word) --------- */

bool SetNumberOfAnalogInputs(unsigned int n)		/* reg 0x90, n in [0,0x3f] */
{
	if (n >= 0x40)
		return false;
	return WaitForNKS4CommandWrite(0x01900000 | ((n << 8) & 0xff00)) == 0;
}

bool SetNumberOfLEDs(unsigned int n)			/* reg 0x70; data = banks-1 */
{
	if (n > 0x200)
		return false;
	unsigned int banks = ((n + 0x1f) >> 5) - 1;	/* ceil(n/32) - 1 */
	return WaitForNKS4CommandWrite(0x00700000 | ((banks << 8) & 0xff00)) == 0;
}

bool SetAllAnalogInputFilter(unsigned char a, unsigned char b)	/* both in [0,0x0f] */
{
	if (a >= 0x10 || b >= 0x10)
		return false;
	/* Ground truth (fresh Ghidra decompile + disassembly, 2026-07-15,
	 * SetAllAnalogInputFilter@0x12ec0): word = 0x01B00000 | ((a<<4)|b) - opcode 0x01,
	 * reg 0xB0, data byte = high-nibble a / low-nibble b. The previous version
	 * (`(b<<8)|a`) omitted the opcode/reg bytes entirely - not a real command word. */
	return WaitForNKS4CommandWrite(0x01B00000 | (((unsigned int)a << 4) | b)) == 0;
}

bool SetRotaryEncoderSampleSpeed(unsigned int n)	/* n in [0,0xff] */
{
	if (n >= 0x100)
		return false;
	/* Ground truth (fresh Ghidra decompile + disassembly, 2026-07-15,
	 * SetRotaryEncoderSampleSpeed@0x12fc0): word = 0x00800000 | (n<<8) - opcode 0x00,
	 * reg 0x80. Previously missing the reg byte entirely. */
	return WaitForNKS4CommandWrite(0x00800000 | ((n << 8) & 0xff00)) == 0;
}

bool ConfigureRotaryEncoders(unsigned int n, bool a, bool b)
{
	/* Ground truth (fresh Ghidra decompile + disassembly, 2026-07-15,
	 * ConfigureRotaryEncoders@0x12f40): n==0 is a VALID no-op that returns success
	 * without sending anything - previously treated as an error. n in [1,4] sends a
	 * real 3-word sequence (opcode 0x01 throughout):
	 *   word1 = 0x01810000 | (((n-1) | (a?0x80:0) | (b?0x40:0)) << 8)
	 *   word2 = 0x01830000  (fixed, no data)
	 *   word3 = 0x01820100  (fixed, no data)
	 * The previous version sent three completely different, unopcoded words
	 * (`n<<8`, `a?0x100:0`, `b?0x100:0`) and rejected n==0. */
	if (n == 0)
		return true;
	if (n >= 5)
		return false;
	unsigned int flags = (n - 1) | (a ? 0x80 : 0) | (b ? 0x40 : 0);
	return WaitForNKS4CommandWrite(0x01810000 | (flags << 8)) == 0 &&
	       WaitForNKS4CommandWrite(0x01830000) == 0 &&
	       WaitForNKS4CommandWrite(0x01820100) == 0;
}

/*
 * ConfigureScanning(keys, ctrls, wheels, spdif, leds, lcd, jack):
 *   builds an enable bitmask and sends it in the command's reg byte.
 *   bit7=keys bit6=ctrls bit4=wheels bit3=spdif bit2=leds bit1=lcd bit0=jack.
 */
bool ConfigureScanning(bool keys, bool ctrls, bool wheels, bool spdif,
		       bool leds, bool lcd, bool jack)
{
	unsigned int mask = (keys ? 0x80 : 0) | (ctrls ? 0x40 : 0) | (wheels ? 0x10 : 0) |
			    (spdif ? 0x08 : 0) | (leds ? 0x04 : 0) | (lcd ? 0x02 : 0) |
			    (jack ? 0x01 : 0);
	return WaitForNKS4CommandWrite(mask << 8) == 0;
}

bool SetLCDBrightness(unsigned char level)
{
	/* Ground truth (fresh Ghidra decompile + disassembly, 2026-07-15,
	 * SetLCDBrightness@0x12ff0): word = 0xC7000000 | (level<<16) - level goes in the
	 * REG byte (opcode fixed at 0xC7), not dataHi as previously assumed. */
	return WaitForNKS4CommandWrite(0xC7000000 | ((unsigned int)level << 16)) == 0;
}

bool ResetModule(unsigned char mode)
{
	/* Ground truth (fresh Ghidra decompile + disassembly, 2026-07-15,
	 * ResetModule@0x13010): word = 0x06000000 | (mode<<16) - same reg-byte placement
	 * as SetLCDBrightness, previously assumed to be dataHi (<<8). */
	return WaitForNKS4CommandWrite(0x06000000 | ((unsigned int)mode << 16)) == 0;
}

/* ---- response classifiers (echo of the reg in the high 16 bits) -------- */
bool IsCommunicationTestAckResponse(unsigned int word) { return (short)(word >> 16) == 0x0066; }
bool IsVersionResponse(unsigned int word)              { return (short)(word >> 16) == 0x0070; }
bool IsPortReadResponse(unsigned int word)             { return (short)(word >> 16) == 0x0171; }

}  /* namespace COmapNKS4Command */
