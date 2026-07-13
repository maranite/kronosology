// SPDX-License-Identifier: GPL-2.0
/*
 * test_keybed_receive.cpp  -  host-side known-answer test for
 * CSTGKeybedComPort::OnByteReceived() (the real
 * CSTGKeybedComPort::ReceiveByte) and CSTGKeybedInterface_ReceiveMessage()
 * (see ../include/oa_comport.h / ../src/init/keybed_receive.cpp, sec
 * 10.237).
 *
 * Links keybed_receive.cpp ALONE (not keybed_init.cpp) -- provides its
 * own minimal mocks for CSTGKeybedInterface_sInstance() and
 * STGAPIFrontPanelStatus::sInstance, matching this project's established
 * "mock what this specific pass doesn't need for real" convention (see
 * test_comport.cpp for the analogous precedent one layer down).
 *
 * Exercises the confirmed real control flow:
 *   [1] a 3-byte 0xA0-class message during KEYBED_OFF_STATE==1 sets the
 *       ACK flag and packs buf[1..2] into STGAPI_OFF_KEYBED_STATUS16.
 *   [2] the identical byte sequence during KEYBED_OFF_STATE==0 does NOT
 *       set the ACK flag (confirmed real early-ignore branch).
 *   [3] the per-type expected-length table (kNumBytesForMessageType) is
 *       applied correctly for a 1-byte (0xC0-class) message.
 *   [4] a 0xF0-class header (type 7, table value 0) never starts
 *       accumulation at all.
 *   [5] a non-0xA0-class 3-byte message during state==1 does NOT set
 *       the ACK flag (only the exact 0xA0-0xAF class does).
 *   [6] a new header byte arriving mid-message aborts the in-progress
 *       message and reframes on the new byte instead.
 *   [7] a stray data byte (no header seen yet) is silently ignored.
 */

#include <cstdio>
#include <cstring>
#include "oa_comport.h"
#include "oa_keybed_init.h"
#include "oa_setup_global_resources.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-60s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

static unsigned char g_keybedInstance[KEYBED_SINSTANCE_SIZE];
static unsigned char g_frontPanel[STGAPI_FRONTPANEL_SIZE];

extern "C" unsigned char *CSTGKeybedInterface_sInstance(void) { return g_keybedInstance; }
unsigned char *STGAPIFrontPanelStatus::sInstance = g_frontPanel;

/*
 * This test links src/init/comport.cpp purely to satisfy
 * CSTGKeybedComPort's vtable (its base class CSTGComPort::
 * GetByteToTransmit(), slot 1, and CSTGComPort's own typeinfo) -- none
 * of comport.cpp's own hardware-facing methods are exercised by any
 * test below, so these are trivial link-satisfying stubs, matching
 * test_comport.cpp's own established mock convention for this exact
 * dependency set.
 */
extern "C" {
unsigned char stg_inb(unsigned int) { return 0; }
void stg_outb(unsigned int, unsigned char) {}
unsigned long stg_local_irq_save(void) { return 0; }
void stg_local_irq_restore(unsigned long) {}
void rtwrap_shutdown_irq(unsigned int) {}
void rtwrap_release_irq(unsigned int) {}
}

static void reset(void)
{
	memset(g_keybedInstance, 0, sizeof(g_keybedInstance));
	memset(g_frontPanel, 0, sizeof(g_frontPanel));
}

static unsigned short status16(void)
{
	unsigned short v;
	memcpy(&v, g_frontPanel + STGAPI_OFF_KEYBED_STATUS16, sizeof(v));
	return v;
}

int main(void)
{
	CSTGKeybedComPort comPort;

	printf("[1] 0xA0-class message, state==1: ACK set + status16 packed:\n");
	reset();
	memset(&comPort, 0, sizeof(comPort));
	g_keybedInstance[KEYBED_OFF_STATE] = 1;
	comPort.OnByteReceived(0xA0);
	check_eq("no ACK yet after 1 of 3 bytes", g_keybedInstance[KEYBED_OFF_ACK_FLAG], 0);
	comPort.OnByteReceived(0x12);
	comPort.OnByteReceived(0x34);
	check_eq("ACK flag set after 3rd byte", g_keybedInstance[KEYBED_OFF_ACK_FLAG], 1);
	check_eq("status16 == (buf[1]<<8)|buf[2]", status16(), 0x1234);
	check_eq("msgState reset to 0 (idle) after completion", comPort.msgState, 0);

	printf("\n[2] identical bytes, state==0: ACK NOT set (early-ignore branch):\n");
	reset();
	memset(&comPort, 0, sizeof(comPort));
	g_keybedInstance[KEYBED_OFF_STATE] = 0;
	comPort.OnByteReceived(0xA0);
	comPort.OnByteReceived(0x12);
	comPort.OnByteReceived(0x34);
	check_eq("ACK flag stays 0", g_keybedInstance[KEYBED_OFF_ACK_FLAG], 0);
	check_eq("msgState still resets to idle", comPort.msgState, 0);

	printf("\n[3] type-4 (0xC0-class) is a 1-byte message per the length table:\n");
	reset();
	memset(&comPort, 0, sizeof(comPort));
	g_keybedInstance[KEYBED_OFF_STATE] = 1;
	comPort.OnByteReceived(0xC7);
	check_eq("completes after exactly 1 byte", comPort.msgState, 0);
	check_eq("no ACK (0xc0 class != 0xa0 class)", g_keybedInstance[KEYBED_OFF_ACK_FLAG], 0);

	printf("\n[4] type-7 (0xF0-class) never starts accumulation (table value 0):\n");
	reset();
	memset(&comPort, 0, sizeof(comPort));
	comPort.OnByteReceived(0xF5);
	check_eq("msgState stays 0 (no message framed)", comPort.msgState, 0);
	check_eq("msgCursor stays 0", comPort.msgCursor, 0);

	printf("\n[5] 0xB0-class 3-byte message, state==1: ACK NOT set (wrong class):\n");
	reset();
	memset(&comPort, 0, sizeof(comPort));
	g_keybedInstance[KEYBED_OFF_STATE] = 1;
	comPort.OnByteReceived(0xB0);
	comPort.OnByteReceived(0x00);
	comPort.OnByteReceived(0x00);
	check_eq("ACK flag stays 0 (only 0xa0-class sets it)", g_keybedInstance[KEYBED_OFF_ACK_FLAG], 0);

	printf("\n[6] a new header byte mid-message aborts and reframes:\n");
	reset();
	memset(&comPort, 0, sizeof(comPort));
	g_keybedInstance[KEYBED_OFF_STATE] = 1;
	comPort.OnByteReceived(0xB0); /* type 3, expects 3 bytes total */
	comPort.OnByteReceived(0x11); /* 2nd byte of the aborted message */
	comPort.OnByteReceived(0xA0); /* new header arrives early -- aborts, reframes as type 2 */
	check_eq("still mid-message after the reframe (only 1 of 3 new bytes seen)",
		 comPort.msgState, 1);
	check_eq("cursor restarted at 1 for the new message", comPort.msgCursor, 1);
	comPort.OnByteReceived(0x56);
	comPort.OnByteReceived(0x78);
	check_eq("ACK set once the REFRAMED message completes", g_keybedInstance[KEYBED_OFF_ACK_FLAG], 1);
	check_eq("status16 reflects only the reframed message's own bytes", status16(), 0x5678);

	printf("\n[7] a stray data byte with no header seen yet is silently ignored:\n");
	reset();
	memset(&comPort, 0, sizeof(comPort));
	comPort.OnByteReceived(0x42);
	check_eq("msgState stays 0", comPort.msgState, 0);
	check_eq("msgCursor stays 0", comPort.msgCursor, 0);
	check_eq("no ACK", g_keybedInstance[KEYBED_OFF_ACK_FLAG], 0);

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
