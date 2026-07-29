// SPDX-License-Identifier: GPL-2.0
/*
 * test_drum_button_led.cpp  -  KAT for CDrumButtonLED's full 3-layer
 * real call chain down to SKSTGGate_SendToUI() (see
 * ../include/oa_drum_button_led.h / ../src/engine/drum_button_led.cpp).
 */

#include <cstdio>
#include <cstring>
#include "oa_drum_button_led.h"

static int g_sendCalls;
static unsigned char g_lastMsg[0x30];
extern "C" void SKSTGGate_SendToUI(const CSKMessage *msg)
{
	g_sendCalls++;
	memcpy(g_lastMsg, msg, sizeof(g_lastMsg));
}

static int g_fail;
static void check(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld (0x%lx)\n", label, got, (unsigned long)got); return; }
	printf("  FAIL  %-45s got=%ld(0x%lx) want=%ld(0x%lx)\n", label, got, (unsigned long)got, want, (unsigned long)want);
	g_fail++;
}
static unsigned short rd16(int off) { return *(unsigned short *)(g_lastMsg + off); }
static unsigned int rd32(int off) { return *(unsigned int *)(g_lastMsg + off); }

static void checkMessageShape(const char *label, unsigned int expectedCmd)
{
	check(label, g_sendCalls, 1);
	check("  +0x00 == 0x1c", rd16(0x00), 0x1c);
	check("  +0x02 == 2", rd16(0x02), 2);
	check("  +0x04 == 4", rd32(0x04), 4);
	check("  +0x08 == cmdId", rd32(0x08), (long)expectedCmd);
	check("  +0x0c == 0 (gap, zeroed)", rd32(0x0c), 0);
	check("  +0x10 == 0", rd32(0x10), 0);
	check("  +0x14 == 0", rd32(0x14), 0);
	check("  +0x18 == 0", rd32(0x18), 0);
}

int main(void)
{
	printf("CDrumButtonLED known-answer test\n");
	printf("========================================================================\n");

	CDrumButtonLED led;

	printf("[1] initialize() sets mState = 0 (start() then turns LED on)\n");
	{
		g_sendCalls = 0;
		led.initialize();
		led.start();
		checkMessageShape("start() after initialize() sends TurnOn (0x2e)", 0x2e);
	}

	printf("[2] start() is a no-op when mState != 0\n");
	{
		led.sleep();	/* sets mState = 1, also sends a Blink message */
		g_sendCalls = 0;
		led.start();
		check("start() while mState!=0 sends nothing", g_sendCalls, 0);
	}

	printf("[3] sleep() sets mState = 1 and sends Blink (0x30)\n");
	{
		led.initialize();
		g_sendCalls = 0;
		led.sleep();
		checkMessageShape("sleep() sends Blink", 0x30);
	}

	printf("[4] wakeup() resets mState = 0 and sends TurnOn (0x2e), unblocking start()\n");
	{
		g_sendCalls = 0;
		led.wakeup();
		checkMessageShape("wakeup() sends TurnOn", 0x2e);

		g_sendCalls = 0;
		led.start();
		checkMessageShape("start() after wakeup() sends TurnOn again (mState was reset)", 0x2e);
	}

	printf("[5] stop() unconditionally sends TurnOff (0x2f), no `this` needed\n");
	{
		g_sendCalls = 0;
		CDrumButtonLED::stop();
		checkMessageShape("stop() sends TurnOff", 0x2f);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
