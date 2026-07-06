// SPDX-License-Identifier: GPL-2.0
/*
 * test_midi_port_manager.cpp  -  host-side known-answer test for
 * CSTGMidiPortManager::WriteSTGMidiOutQueue()/NotifyNKS4TestMode()
 * (batch 12, src/engine/midi_port_manager.cpp).
 *
 * Links midi_queue.cpp (for the real CSTGMidiQueue::Reset(),
 * NotifyNKS4TestMode()'s own dependency) and midi_queue_writer.cpp
 * (for the real CSTGMidiQueueWriter::Write(), WriteSTGMidiOutQueue's
 * own dependency) -- NOT global.cpp, which pulls in far more than this
 * test needs; `CSTGGlobal::sInstance`/`CSTGHeapManager::sInstance`
 * storage is provided locally instead, matching
 * test_lfo_stepseq_quad.cpp's/test_midi_dispatcher.cpp's own
 * established precedent for the exact same situation.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"

/* Local storage for CSTGGlobal::sInstance -- this test only needs the
 * pointer value (for the +0x6ac gate byte), never calls any of
 * CSTGGlobal's own methods, so it doesn't link global.cpp. */
CSTGGlobal *CSTGGlobal::sInstance;

/* Local minimal CSTGHeapManager stand-in + storage, matching
 * midi_port_manager.cpp's own internal declaration (same mangled
 * `sInstance` symbol) -- needed here so this test can SET the value;
 * see midi_port_manager.cpp's own top-of-file note on why it can't
 * just `#include "oa_heap.h"` (ODR conflict with oa_global.h's
 * CSTGGlobal). */
struct CSTGHeapManager { static char *sInstance; };
char *CSTGHeapManager::sInstance;

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%lx\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%lx)\n", want);
}

static unsigned char *map32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

int main(void)
{
	printf("CSTGMidiPortManager::WriteSTGMidiOutQueue()/NotifyNKS4TestMode() known-answer test\n");
	printf("=========================================================\n");

	printf("[1] WriteSTGMidiOutQueue: gate byte (+0x6ac) clear -> forwards to Write()\n");
	{
		unsigned char *globalBuf = map32(0x1000);
		unsigned char *portMgrBuf = map32(0x140);
		unsigned char *ringCtlBuf = map32(64);
		unsigned char *msgBufBase = map32(64);
		memset(globalBuf, 0, 0x1000);
		memset(portMgrBuf, 0, 0x140);
		memset(ringCtlBuf, 0, 64);
		memset(msgBufBase, 0xCC, 64);

		CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;
		*(unsigned int *)(ringCtlBuf + 0x8) = 0xf; /* mask -> 16-byte ring */
		*(unsigned int *)(portMgrBuf + 0x138) = ToU32(ringCtlBuf);
		*(unsigned int *)(portMgrBuf + 0x13c) = ToU32(msgBufBase);

		globalBuf[0x6ac] = 0; /* gate OPEN */

		CSTGMidiPortManager *mgr = (CSTGMidiPortManager *)portMgrBuf;
		unsigned char msg[3] = { 0xB2, 0x07, 0x40 };
		mgr->WriteSTGMidiOutQueue(msg, 3);

		check_eq("writeCursor advanced by 3", *(unsigned int *)(ringCtlBuf + 0xc), 3);
		check_eq("byte 0 copied", msgBufBase[0], 0xB2);
		check_eq("byte 1 copied", msgBufBase[1], 0x07);
		check_eq("byte 2 copied", msgBufBase[2], 0x40);
	}

	printf("[2] WriteSTGMidiOutQueue: gate byte (+0x6ac) SET -> confirmed no-op\n");
	{
		unsigned char *globalBuf = map32(0x1000);
		unsigned char *portMgrBuf = map32(0x140);
		unsigned char *ringCtlBuf = map32(64);
		unsigned char *msgBufBase = map32(64);
		memset(globalBuf, 0, 0x1000);
		memset(portMgrBuf, 0, 0x140);
		memset(ringCtlBuf, 0, 64);
		memset(msgBufBase, 0xCC, 64);

		CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;
		*(unsigned int *)(ringCtlBuf + 0x8) = 0xf;
		*(unsigned int *)(portMgrBuf + 0x138) = ToU32(ringCtlBuf);
		*(unsigned int *)(portMgrBuf + 0x13c) = ToU32(msgBufBase);

		globalBuf[0x6ac] = 1; /* gate CLOSED */

		CSTGMidiPortManager *mgr = (CSTGMidiPortManager *)portMgrBuf;
		unsigned char msg[3] = { 0xB2, 0x07, 0x40 };
		mgr->WriteSTGMidiOutQueue(msg, 3);

		check_eq("writeCursor untouched (still 0)", *(unsigned int *)(ringCtlBuf + 0xc), 0);
		check_eq("buffer untouched (still poison)", msgBufBase[0], 0xCC);
	}

	printf("[3] NotifyNKS4TestMode: heap up, slot in range -> resets all 4\n"
	       "    embedded CSTGMidiQueue objects (stride 0x64), leaving\n"
	       "    mask/readerCount untouched (same confirmed Reset() gap\n"
	       "    as test_midi_queue.cpp's own [5])\n");
	{
		/* heapMgrBuf must be big enough to cover +0x1e8498 (the
		 * confirmed real heap-translation-base field, see
		 * oa_heap.h) -- set to 0 here so heapBase == the raw
		 * pointer at +0x38 and region == the raw pointer at
		 * +0x24+slot*0x14, exercising the real formula's control
		 * flow without needing a second real translation base. */
		unsigned long heapMgrSize = 0x1e8500;
		unsigned char *heapMgrBuf = map32(heapMgrSize);
		memset(heapMgrBuf, 0, heapMgrSize);

		unsigned char *heapBaseBuf = map32(0x10);
		memset(heapBaseBuf, 0, 0x10);
		unsigned char *regionBuf = map32(0x200);
		memset(regionBuf, 0xCC, 0x200);

		unsigned int slot = 0;
		*(unsigned int *)(heapMgrBuf + 0x38) = ToU32(heapBaseBuf); /* heapBase ptr */
		*(unsigned int *)(heapMgrBuf + 0x1e8498) = 0;              /* translation base = 0 */
		*(unsigned int *)(heapBaseBuf + 0x8) = slot;               /* slot stored inside heap base */
		*(unsigned int *)(heapMgrBuf + 0x24 + slot * 0x14) = ToU32(regionBuf);

		CSTGHeapManager::sInstance = (char *)heapMgrBuf;

		/* Poison mask (+0x8)/readerCount (+0x20) of each of the 4
		 * embedded CSTGMidiQueue slots to confirm they're left
		 * untouched (Reset()'s own confirmed real gap). */
		for (int i = 0; i < 4; i++) {
			unsigned char *q = regionBuf + i * 0x64;
			*(unsigned int *)(q + 0x8) = 0x12345678 + i; /* mask, poison */
			q[0x20] = 4;                                  /* readerCount, poison */
		}

		unsigned char portMgrBuf[16];
		CSTGMidiPortManager *mgr = (CSTGMidiPortManager *)portMgrBuf;
		mgr->NotifyNKS4TestMode();

		for (int i = 0; i < 4; i++) {
			unsigned char *q = regionBuf + i * 0x64;
			char label[64];
			snprintf(label, sizeof(label), "slot[%d] writeCursor == 0", i);
			check_eq(label, *(unsigned int *)(q + 0xc), 0);
			for (int r = 0; r < 4; r++) {
				snprintf(label, sizeof(label), "slot[%d] readerPos[%d] == 0", i, r);
				check_eq(label, *(unsigned int *)(q + 0x10 + r * 4), 0);
			}
			snprintf(label, sizeof(label), "slot[%d] mask untouched", i);
			check_eq(label, *(unsigned int *)(q + 0x8), 0x12345678u + i);
			snprintf(label, sizeof(label), "slot[%d] readerCount untouched", i);
			check_eq(label, q[0x20], 4);
		}
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
