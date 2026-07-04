// SPDX-License-Identifier: GPL-2.0
/*
 * test_push_unsolicited_message.cpp  -  known-answer test for
 * PushUnsolicitedMessage(void*) (sec 10.90). See src/engine/
 * push_unsolicited_message.cpp for the confirmed real shape.
 */

#include <cstdio>
#include <cstring>
#include "oa_rtfifo_init.h"

extern unsigned char g_unsolicitedMessagesEnabled;
extern unsigned char g_unsolicitedMessageCounterEnabled;
extern unsigned int g_unsolicitedMessageCount;
extern "C" unsigned int gSystemIsInitialized;
extern "C" void PushUnsolicitedMessage(void *msg);

static int g_fail;
static int g_rtfPutIfCalls;
static unsigned int g_lastFifo;
static int g_lastSize;
static unsigned char g_lastBuf[64];

extern "C" int rtf_put_if(unsigned int fifo, const void *buf, int size)
{
	g_rtfPutIfCalls++;
	g_lastFifo = fifo;
	g_lastSize = size;
	memcpy(g_lastBuf, buf, (size_t)size < sizeof(g_lastBuf) ? (size_t)size : sizeof(g_lastBuf));
	return 0;
}
/* Unused by this function, but link-required by oa_rtfifo_init.h's own
 * declarations. */
extern "C" int rtf_create(unsigned int, int) { return 0; }
extern "C" int rtf_destroy(unsigned int) { return 0; }

static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	printf("  %-4s %-55s 0x%x\n", got == want ? "ok" : "FAIL", label, got);
	if (got != want)
		g_fail++;
}

int main(void)
{
	printf("PushUnsolicitedMessage known-answer test\n");
	printf("=========================================================\n");

	unsigned char msg[0x18];
	memset(msg, 0, sizeof(msg));
	*(unsigned short *)(msg + 0) = 0x18;
	*(unsigned short *)(msg + 2) = 5; /* source, not 9 -- no rewrite expected */

	printf("[1] messages disabled -> no-op\n");
	g_unsolicitedMessagesEnabled = 0;
	gSystemIsInitialized = 1;
	g_rtfPutIfCalls = 0;
	PushUnsolicitedMessage(msg);
	check_eq("rtf_put_if NOT called", (unsigned int)g_rtfPutIfCalls, 0u);

	printf("[2] messages enabled, system NOT initialized -> no-op\n");
	g_unsolicitedMessagesEnabled = 1;
	gSystemIsInitialized = 0;
	g_rtfPutIfCalls = 0;
	PushUnsolicitedMessage(msg);
	check_eq("rtf_put_if NOT called", (unsigned int)g_rtfPutIfCalls, 0u);

	printf("[3] both gates clear -> forwards to rtf_put_if(minor 5, msg, msg[0])\n");
	gSystemIsInitialized = 1;
	g_unsolicitedMessageCounterEnabled = 0;
	g_unsolicitedMessageCount = 0;
	g_rtfPutIfCalls = 0;
	PushUnsolicitedMessage(msg);
	check_eq("rtf_put_if called once", (unsigned int)g_rtfPutIfCalls, 1u);
	check_eq("fifo minor == 5", g_lastFifo, 5u);
	check_eq("size == msg[0] (0x18)", (unsigned int)g_lastSize, 0x18u);
	check_eq("counter NOT incremented (counter-enable clear)", g_unsolicitedMessageCount, 0u);
	check_eq("source untouched (was 5, not 9)", *(unsigned short *)(g_lastBuf + 2), 5u);

	printf("[4] counter-enable set -> counter increments\n");
	g_unsolicitedMessageCounterEnabled = 1;
	PushUnsolicitedMessage(msg);
	check_eq("counter incremented once", g_unsolicitedMessageCount, 1u);

	printf("[5] source == 9 -> rewritten to 8 before sending (confirmed real quirk)\n");
	*(unsigned short *)(msg + 2) = 9;
	PushUnsolicitedMessage(msg);
	check_eq("sent message's source == 8, not 9", *(unsigned short *)(g_lastBuf + 2), 8u);
	check_eq("caller's own msg buffer ALSO rewritten in place",
		 *(unsigned short *)(msg + 2), 8u);

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
