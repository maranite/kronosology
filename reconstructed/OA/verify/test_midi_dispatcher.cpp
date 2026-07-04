// SPDX-License-Identifier: GPL-2.0
/*
 * test_midi_dispatcher.cpp  -  KAT for CSTGMidiDispatcher::
 * CSTGMidiDispatcher()/Initialize() (see src/engine/midi_dispatcher.cpp).
 *
 * Uses MAP_32BIT for CSTGMidiPortManager/CSTGHeapManager's own backing
 * buffers -- Initialize() truncates their addresses to `unsigned int`,
 * matching the real 32-bit target, the same host/target pointer-width
 * hazard hit repeatedly elsewhere in this project.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-55s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-55s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* Real definitions live elsewhere in this project (managers.cpp for
 * CSTGMidiPortManager's dtor, heap-manager work for CSTGHeapManager) --
 * defined here directly since this test only needs the VALUES, matching
 * this project's established convention for standalone unit KATs. */
CSTGMidiPortManager *CSTGMidiPortManager::sInstance;
CSTGMidiPortManager::~CSTGMidiPortManager() {}

/* Real definition lives in engine_init.cpp (sec 10.58), not linked by
 * this test -- defined here instead so this test can link standalone. */
CSTGMidiDispatcher *CSTGMidiDispatcher::sInstance;

/* Local minimal stand-in matching midi_dispatcher.cpp's own (same
 * mangled `sInstance` symbol) -- this test only needs the VALUE. */
struct CSTGHeapManager { static char *sInstance; };
char *CSTGHeapManager::sInstance;

static unsigned char g_readerArg[64];
unsigned char CSTGMidiQueue::AllocReader()
{
	void *slot = (void *)this;
	memcpy(g_readerArg, &slot, sizeof(slot));
	return 0x42;
}

/* HandleController's own 5-arg overload (sec 10.77) confirmed real,
 * deliberately deferred extern -- this test only exercises the new
 * 3-arg-pointer wrapper (sec 10.139), which forwards to this one. */
static int g_handleController5ArgCalls;
static unsigned char g_lastHC5Channel, g_lastHC5Arg2, g_lastHC5Arg3;
static int g_lastHC5Source, g_lastHC5Target;
void CSTGMidiDispatcher::HandleController(unsigned char arg1, unsigned char arg2, unsigned char arg3, int source, int target)
{
	g_handleController5ArgCalls++;
	g_lastHC5Channel = arg1;
	g_lastHC5Arg2 = arg2;
	g_lastHC5Arg3 = arg3;
	g_lastHC5Source = source;
	g_lastHC5Target = target;
}

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *map32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

int main(void)
{
	printf("CSTGMidiDispatcher known-answer test\n");
	printf("=========================================================\n");

	unsigned char dispBuf[sizeof(CSTGMidiDispatcher)];
	memset(dispBuf, 0xcc, sizeof(dispBuf));

	printf("[1] CSTGMidiDispatcher::CSTGMidiDispatcher()\n");
	CSTGMidiDispatcher *disp = new (dispBuf) CSTGMidiDispatcher();
	check_eq("sInstance == this", (long)(CSTGMidiDispatcher::sInstance == disp), 1);
	check_eq("+0x4 zeroed", *(unsigned int *)(dispBuf + 0x4), 0);
	check_eq("+0x8 zeroed", *(unsigned int *)(dispBuf + 0x8), 0);
	check_eq("+0x60/+0x61 zeroed, +0x62/+0x63 left untouched (still poisoned)",
		 (long)(dispBuf[0x60] == 0 && dispBuf[0x61] == 0 &&
			dispBuf[0x62] == 0xcc && dispBuf[0x63] == 0xcc), 1);
	check_eq("+0x9c/+0x9d zeroed (last of the 0x60-0x9f group)",
		 (long)(dispBuf[0x9c] == 0 && dispBuf[0x9d] == 0), 1);
	check_eq("+0xa2 == 1 (confirmed real literal)", dispBuf[0xa2], 1);
	check_eq("+0xa3 left untouched (still poisoned)", dispBuf[0xa3], 0xcc);
	check_eq("+0xa4 zeroed (dword)", *(unsigned int *)(dispBuf + 0xa4), 0);
	check_eq("+0x20..+0x2f fully zeroed", (long)(dispBuf[0x20] == 0 && dispBuf[0x2f] == 0), 1);
	check_eq("+0x30..+0x3f fully zeroed", (long)(dispBuf[0x30] == 0 && dispBuf[0x3f] == 0), 1);
	check_eq("+0xc..+0x1f left untouched (confirmed real gap, still poisoned)",
		 (long)(dispBuf[0xc] == 0xcc && dispBuf[0x1f] == 0xcc), 1);

	printf("\n[2] Initialize() -- invalid slot (0xffffffff sentinel, matches\n"
	       "    engine_init.cpp's own already-confirmed CSTGMidiPortManager init)\n");
	unsigned char *portMgrBuf = map32(0x400);
	memset(portMgrBuf, 0, 0x400);
	*(unsigned int *)(portMgrBuf + 0x1a4) = 0xffffffff;
	CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)portMgrBuf;

	/* Must be big enough to cover the real CSTGHeapManager's own
	 * confirmed +0x1e8498 heapBase field (sec 10.59/10.63) -- this
	 * test never actually builds a real handle table, just pokes the
	 * two fields Initialize() itself reads. */
	unsigned long heapBufSize = 0x1e8500;
	unsigned char *heapBuf = map32(heapBufSize);
	memset(heapBuf, 0, heapBufSize);
	*(unsigned int *)(heapBuf + 0x1e8498) = 0; /* not reached on this path */
	CSTGHeapManager::sInstance = (char *)heapBuf;

	disp->Initialize();
	check_eq("+0x4 == &portMgr[+0x1a4]", *(unsigned int *)(dispBuf + 0x4),
		 ToU32(portMgrBuf + 0x1a4));
	check_eq("+0x8 stays 0 (invalid slot -> no resolution)",
		 *(unsigned int *)(dispBuf + 0x8), 0);
	check_eq("AllocReader called with &portMgr[+0x1a4]",
		 (long)(*(void **)g_readerArg == (void *)(portMgrBuf + 0x1a4)), 1);
	check_eq("+0xc == AllocReader's return value (0x42)", dispBuf[0xc], 0x42);
	check_eq("+0xd == 0", dispBuf[0xd], 0);

	printf("\n[3] Initialize() -- valid slot, real heap-resolution formula\n"
	       "    (heap+0x18+slot*0x14, reading that entry's own +0xc field)\n");
	*(unsigned int *)(portMgrBuf + 0x1a4) = 5; /* a valid, in-range slot */
	unsigned char *entry5 = heapBuf + 0x18 + 5 * 0x14;
	*(unsigned int *)(entry5 + 0xc) = 0x1000; /* confirmed "offset" field */
	*(unsigned int *)(heapBuf + 0x1e8498) = 0x2000; /* heapBase */

	disp->Initialize();
	check_eq("+0x8 == heapBase + offset (0x3000)",
		 *(unsigned int *)(dispBuf + 0x8), 0x3000);

	printf("\n[4] HandleController(const unsigned char*, int, int) (sec 10.139)\n"
	       "    unpacks a 3-byte MIDI message and forwards to the 5-arg overload\n");
	unsigned char msg[3] = { 0x93, 0x40, 0x7f }; /* channel = 0x93 & 0xf = 3 */
	g_handleController5ArgCalls = 0;
	disp->HandleController(msg, 1, -1);
	check_eq("forwarded exactly once", g_handleController5ArgCalls, 1);
	check_eq("channel == byte[0] & 0xf", g_lastHC5Channel, 3);
	check_eq("arg2 == byte[1]", g_lastHC5Arg2, 0x40);
	check_eq("arg3 == byte[2]", g_lastHC5Arg3, 0x7f);
	check_eq("source passed through", g_lastHC5Source, 1);
	check_eq("target passed through", g_lastHC5Target, -1);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
