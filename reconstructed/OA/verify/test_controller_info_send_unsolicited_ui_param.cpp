// SPDX-License-Identifier: GPL-2.0
/*
 * test_controller_info_send_unsolicited_ui_param.cpp  -  host-side
 * known-answer test for CSTGControllerInfo::SendUnsolicitedUIParam
 * (batch 60). Mocks PushUnsolicitedMessage() (own dedicated symbol,
 * not linked from push_unsolicited_message.cpp) to capture the exact
 * outgoing message bytes for each of the four real dispatch paths.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-70s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

/* Own local storage for CSTGGlobal::sInstance -- large enough to cover
 * every field offset this function touches (+0x29cc4e0 is the highest). */
static unsigned char g_globalStorage[0x29cc600];
CSTGGlobal *CSTGGlobal::sInstance = (CSTGGlobal *)g_globalStorage;

static unsigned char g_lastMsg[64];
static int g_pushCalls;
extern "C" void PushUnsolicitedMessage(void *msg)
{
	g_pushCalls++;
	unsigned short size = *(unsigned short *)msg;
	memcpy(g_lastMsg, msg, size);
}

int main(void)
{
	printf("CSTGControllerInfo::SendUnsolicitedUIParam known-answer test\n");
	printf("=========================================================\n");

	unsigned char *g = g_globalStorage;

	printf("[1] Zero-path (fieldAt(0x29cc4dc)==0), mode==0 -- 0x20-tagged "
	       "message straight from fieldAt(0x688)/fieldAt(0x694)\n");
	{
		memset(g_globalStorage, 0, sizeof(g_globalStorage));
		*(unsigned int *)(g + 0x684) = 0;
		*(unsigned int *)(g + 0x688) = 0x11111111;
		*(unsigned int *)(g + 0x694) = 0x22222222;
		g_pushCalls = 0;

		CSTGControllerInfo::SendUnsolicitedUIParam(9, 5, 0, 1);

		check_eq("PushUnsolicitedMessage called once", (unsigned int)g_pushCalls, 1);
		check_eq("size tag == 0x20", *(unsigned short *)(g_lastMsg + 0x0), 0x20);
		check_eq("source == midiSource (1)", *(unsigned short *)(g_lastMsg + 0x2), 1);
		check_eq("+0x4 == 4", *(unsigned int *)(g_lastMsg + 0x4), 4);
		check_eq("+0x8 == 5", *(unsigned int *)(g_lastMsg + 0x8), 5);
		check_eq("+0xc == fieldAt(0x688)", *(unsigned int *)(g_lastMsg + 0xc), 0x11111111);
		check_eq("+0x10 == fieldAt(0x694)", *(unsigned int *)(g_lastMsg + 0x10), 0x22222222);
		check_eq("+0x14 == paramId (9)", *(unsigned int *)(g_lastMsg + 0x14), 9);
		check_eq("+0x18 == value (5)", *(unsigned int *)(g_lastMsg + 0x18), 5);
		check_eq("+0x1c == arg3 (0)", *(unsigned int *)(g_lastMsg + 0x1c), 0);
	}

	printf("[2] Zero-path, mode==1 -- 0x24-tagged, fieldAt(0x690)/fieldAt(0x69c), extra=0\n");
	{
		memset(g_globalStorage, 0, sizeof(g_globalStorage));
		*(unsigned int *)(g + 0x684) = 1;
		*(unsigned int *)(g + 0x690) = 0x33333333;
		*(unsigned int *)(g + 0x69c) = 0x44444444;
		g_pushCalls = 0;

		CSTGControllerInfo::SendUnsolicitedUIParam(10, 7, 2, 1);

		check_eq("PushUnsolicitedMessage called once", (unsigned int)g_pushCalls, 1);
		check_eq("size tag == 0x24", *(unsigned short *)(g_lastMsg + 0x0), 0x24);
		check_eq("+0x4 == 2", *(unsigned int *)(g_lastMsg + 0x4), 2);
		check_eq("+0x8 == 2", *(unsigned int *)(g_lastMsg + 0x8), 2);
		check_eq("+0xc == fieldAt(0x690)", *(unsigned int *)(g_lastMsg + 0xc), 0x33333333);
		check_eq("+0x10 == fieldAt(0x69c)", *(unsigned int *)(g_lastMsg + 0x10), 0x44444444);
		check_eq("+0x20 == 0 (mode1 extra)", *(unsigned int *)(g_lastMsg + 0x20), 0);
	}

	printf("[3] Zero-path, mode==2 -- 0x24-tagged, field1=0/fieldAt(0x6a0), extra=2\n");
	{
		memset(g_globalStorage, 0, sizeof(g_globalStorage));
		*(unsigned int *)(g + 0x684) = 2;
		*(unsigned int *)(g + 0x6a0) = 0x55555555;
		g_pushCalls = 0;

		CSTGControllerInfo::SendUnsolicitedUIParam(11, 8, 3, 1);

		check_eq("+0xc == 0", *(unsigned int *)(g_lastMsg + 0xc), 0);
		check_eq("+0x10 == fieldAt(0x6a0)", *(unsigned int *)(g_lastMsg + 0x10), 0x55555555);
		check_eq("+0x20 == 2 (mode2 extra)", *(unsigned int *)(g_lastMsg + 0x20), 2);
	}

	printf("[4] Zero-path, mode out of [0,2] -- silently dropped, no send\n");
	{
		memset(g_globalStorage, 0, sizeof(g_globalStorage));
		*(unsigned int *)(g + 0x684) = 5;
		g_pushCalls = 0;

		CSTGControllerInfo::SendUnsolicitedUIParam(12, 9, 0, 1);

		check_eq("PushUnsolicitedMessage NOT called", (unsigned int)g_pushCalls, 0);
	}

	printf("[5] Structured path (fieldAt(0x29cc4dc)!=0), mode==1 -- raw vtable-slot "
	       "22/23 dispatch on the resolved sub-object\n");
	{
		memset(g_globalStorage, 0, sizeof(g_globalStorage));
		*(unsigned int *)(g + 0x29cc4dc) = 1;
		*(unsigned int *)(g + 0x684) = 1; /* collapses onto the mode-1 formula */
		*(unsigned int *)(g + 0x69c) = 3;
		*(unsigned int *)(g + 0x690) = 0;
		*(unsigned int *)(g + 0x29cc4e0) = 0; /* slot index 0 */

		unsigned char *perf = g + 3 * 0x19e7 + 0 * 0xcf381 + 0x1c77f10 + 6;
		unsigned char *slotBase = perf + 0 * 0xe8;
		unsigned char *objB = slotBase + 0xb60;

		/*
		 * Production code dereferences vtable slots 0x58/0x5c as raw
		 * BYTE offsets, only 4 bytes apart (ground truth's own real
		 * 32-bit-native vtable stride) -- but a HOST function pointer
		 * needs the full 8 bytes, so the two slots' own 8-byte host
		 * reads UNAVOIDABLY overlap by 4 bytes. A naive `void*[]`-style
		 * setup corrupts whichever slot is written first (confirmed
		 * the hard way: an earlier version of this test called through
		 * a NULL/garbage address here). Fixed via `MAP_FIXED` at two
		 * hand-chosen addresses satisfying `high32(handler1) ==
		 * low32(handler2)` -- the ONLY way both 8-byte overlapping
		 * reads can simultaneously resolve to their own correct,
		 * independent target address. `handler1`/`handler2` are tiny
		 * hand-assembled `mov eax,imm32; ret` stubs (not compiled C++
		 * functions -- their addresses must be exact, chosen values).
		 */
		void *page1 = mmap((void *)0x600000000UL, 0x1000,
				    PROT_READ | PROT_WRITE | PROT_EXEC,
				    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		void *page2 = mmap((void *)0x700000000UL, 0x1000,
				    PROT_READ | PROT_WRITE | PROT_EXEC,
				    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		(void)page1;
		(void)page2;
		unsigned char *handler1 = (unsigned char *)0x600000000UL; /* high32=6, low32=0 */
		unsigned char *handler2 = (unsigned char *)0x700000006UL; /* high32=7, low32=6 */

		/* mov eax, 0x7777; ret */
		handler1[0] = 0xB8;
		*(unsigned int *)(handler1 + 1) = 0x7777;
		handler1[5] = 0xC3;
		/* mov eax, 0x42; ret (caller only reads AL via movzbl) */
		handler2[0] = 0xB8;
		*(unsigned int *)(handler2 + 1) = 0x42;
		handler2[5] = 0xC3;

		unsigned char vtableBuf[0x70];
		memset(vtableBuf, 0, sizeof(vtableBuf));
		*(void **)(vtableBuf + 0x58) = handler1;
		*(void **)(vtableBuf + 0x5c) = handler2;
		*(void **)(objB + 3) = vtableBuf;

		g_pushCalls = 0;
		CSTGControllerInfo::SendUnsolicitedUIParam(13, 10, 0, 1);

		check_eq("PushUnsolicitedMessage called once", (unsigned int)g_pushCalls, 1);
		check_eq("size tag == 0x20", *(unsigned short *)(g_lastMsg + 0x0), 0x20);
		check_eq("+0x4 == 4", *(unsigned int *)(g_lastMsg + 0x4), 4);
		check_eq("+0x8 == 5", *(unsigned int *)(g_lastMsg + 0x8), 5);
		check_eq("+0xc == vtable slot 22 result", *(unsigned int *)(g_lastMsg + 0xc), 0x7777);
		check_eq("+0x10 == vtable slot 23 result (byte)",
			 *(unsigned int *)(g_lastMsg + 0x10), 0x42);
		check_eq("+0x14 == paramId (13)", *(unsigned int *)(g_lastMsg + 0x14), 13);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
