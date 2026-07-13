// SPDX-License-Identifier: GPL-2.0
/*
 * keybed_receive.cpp  -  CSTGKeybedComPort::OnByteReceived() (the real
 * CSTGKeybedComPort::ReceiveByte(unsigned char), vtable slot 0) and
 * CSTGKeybedInterface_ReceiveMessage(). See oa_comport.h for the full
 * ground-truthing details (offsets, scope decision on the deferred
 * KEYBED_OFF_STATE==2 branch, sec 10.237).
 *
 * Faithful, instruction-level reconstruction from a full objdump
 * disassembly + relocation trace of the real `CSTGKeybedComPort::
 * ReceiveByte` (`.text+0x33e8e0`, 149 bytes) and the boot-reachable
 * portion of `CSTGKeybedInterface::ReceiveMessage` (`.text+0x33d9b0`,
 * 456 bytes total) in OA_real.ko.
 */

#include "oa_comport.h"
#include "oa_keybed_init.h"		 /* KEYBED_OFF_STATE/KEYBED_OFF_ACK_FLAG */
#include "oa_setup_global_resources.h"	 /* STGAPIFrontPanelStatus::sInstance */

/*
 * Confirmed real per-"type" expected message length table
 * (`.rodata+0xa8960` in OA_real.ko, byte-read directly): the header/
 * status byte's own `(byte & 0x70) >> 4` selects one of these 8 entries.
 * Type 7 (header bytes 0xF0-0xFF) is confirmed real as 0 -- no message
 * is ever assembled for that class, matching the disassembly's own
 * `numBytes != 0` guard before entering the accumulation state.
 */
static const unsigned char kNumBytesForMessageType[8] = { 3, 3, 3, 3, 1, 2, 4, 0 };

void CSTGKeybedComPort::OnByteReceived(unsigned char receivedByte)
{
	if (msgState == 1) {
		/* Mid-message: a new header byte (high bit set) aborts the
		 * in-progress message and restarts framing on this byte
		 * instead -- confirmed real via the `33e908`/`33e940`
		 * branch pair. */
		if ((signed char)receivedByte < 0) {
			msgState = 0;
			goto header;
		}
		goto accumulate;
	}

	/* msgState == 0: only a byte with the high bit set starts a new
	 * message; a stray data byte with no header seen yet is silently
	 * ignored (confirmed real -- no side effect at all on that path). */
	if ((signed char)receivedByte >= 0)
		return;

header : {
	unsigned int type = (receivedByte & 0x70) >> 4;
	unsigned char numBytes = kNumBytesForMessageType[type];
	msgExpectedLen = numBytes;
	if (numBytes == 0)
		return; /* type 7: no message framed, confirmed real */
	msgState = 1;
	msgCursor = 0;
	/* Falls through to accumulate, storing the header byte itself as
	 * buffer[0] -- confirmed real (the real disassembly reuses the
	 * exact same accumulation code for the header byte). */
}

accumulate:
	msgBuffer[msgCursor] = receivedByte;
	msgCursor++;
	if (msgCursor != msgExpectedLen)
		return;

	/* Message complete: hand it to CSTGKeybedInterface::ReceiveMessage
	 * and reset to idle -- confirmed real (`this` passed to
	 * ReceiveMessage is the fixed CSTGKeybedInterface::sInstance
	 * address, not this CSTGKeybedComPort sub-object's own address --
	 * see oa_comport.h's own comment). */
	CSTGKeybedInterface_ReceiveMessage(CSTGKeybedInterface_sInstance(), msgBuffer,
					    msgCursor);
	msgState = 0;
}

void CSTGKeybedInterface_ReceiveMessage(unsigned char *sInstance, const unsigned char *buf,
					 unsigned int len)
{
	(void)len; /* confirmed real: the boot-reachable state==1 branch
		    * below never re-checks len, since msgCursor==
		    * msgExpectedLen(==3 for the 0xa0-class message this
		    * branch cares about) is already guaranteed by
		    * OnByteReceived's own caller-side check above. */

	int state = sInstance[KEYBED_OFF_STATE];
	if (state != 1) {
		/* state==0: confirmed real early-ignore (nothing to do
		 * before the handshake has even opened a port).
		 * state==2 ("fully running"): confirmed-real, deliberately
		 * DEFERRED dispatch -- see oa_comport.h's own comment for
		 * why this is provably unreachable during init_module()'s
		 * own boot sequence. */
		return;
	}

	/* Confirmed real: (buf[0] & 0xf0) == 0xa0 is the keybed board's
	 * own handshake-ACK message shape -- the exact same 0xA0-0xAF
	 * "type 2" header class CSTGKeybedInterface_Startup's own probe
	 * byte (0xa5) also belongs to, confirmed via objdump -r
	 * (`and $0xf0,%ecx; cmp $0xa0,%cl`). */
	if ((buf[0] & 0xf0) == 0xa0) {
		unsigned short status16 = ((unsigned short)buf[1] << 8) | buf[2];
		*(unsigned short *)(STGAPIFrontPanelStatus::sInstance +
				     STGAPI_OFF_KEYBED_STATUS16) = status16;
		sInstance[KEYBED_OFF_ACK_FLAG] = 1;
	}
}
