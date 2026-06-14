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
 *      0x00  configure scanning (reg=enable bitmask)
 *  (a few setter regs are noted inline where not yet disassembled.)
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
	/* reg/encoding assembled in EAX from a,b (see binary 0x2ec0) */
	return WaitForNKS4CommandWrite(((unsigned int)b << 8) | a) == 0;
}

bool SetRotaryEncoderSampleSpeed(unsigned int n)	/* n in [0,0xff] */
{
	if (n >= 0x100)
		return false;
	return WaitForNKS4CommandWrite((n << 8) & 0xff00) == 0;
}

bool ConfigureRotaryEncoders(unsigned int n, bool a, bool b)	/* n in [1,4]; 3 cmds */
{
	if (n == 0 || n >= 5)
		return false;
	return WaitForNKS4CommandWrite((n << 8)) == 0 &&
	       WaitForNKS4CommandWrite((a ? 0x100 : 0)) == 0 &&
	       WaitForNKS4CommandWrite((b ? 0x100 : 0)) == 0;
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
	return WaitForNKS4CommandWrite(((unsigned int)level << 8)) == 0;
}

bool ResetModule(unsigned char mode)
{
	return WaitForNKS4CommandWrite(((unsigned int)mode << 8)) == 0;
}

/* ---- response classifiers (echo of the reg in the high 16 bits) -------- */
bool IsCommunicationTestAckResponse(unsigned int word) { return (short)(word >> 16) == 0x0066; }
bool IsVersionResponse(unsigned int word)              { return (short)(word >> 16) == 0x0070; }
bool IsPortReadResponse(unsigned int word)             { return (short)(word >> 16) == 0x0171; }

}  /* namespace COmapNKS4Command */
