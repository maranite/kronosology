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

/* Storage for CSTGMidiPortManager's own static port arrays (normally
 * defined in engine.cpp, not linked into this test) -- left all-NULL,
 * matching this project's own confirmed real boot state (no
 * reconstructed caller of RegisterMidiInPort/RegisterMidiOutPort exists
 * yet), so Initialize()'s own port-registration loop stays a confirmed
 * no-op in test [4] below. */
void *CSTGMidiPortManager::sMidiInPorts[4];
void *CSTGMidiPortManager::sMidiOutPorts[4];

/* Local minimal CSTGHeapManager stand-in, matching midi_port_manager.cpp's
 * own internal declaration (same mangled `sInstance` symbol) -- needed
 * here so tests [1]-[3] can SET the value via convenient `char*`
 * arithmetic; see midi_port_manager.cpp's own top-of-file note on why it
 * can't just `#include "oa_heap.h"` (ODR conflict with oa_global.h's
 * CSTGGlobal). Storage is now provided by src/mem/heap_manager.cpp
 * (linked into this test as of sec 10.230, for CSTGMidiQueue::
 * Initialize()'s own real `CSTGHeapManager::Alloc(unsigned long)` call)
 * -- NOT defined again here, that would be a duplicate definition of the
 * same real symbol. */
struct CSTGHeapManager { static char *sInstance; };

/* Real C-linkage wrapper (oa_heapmanager.h) -- declared directly rather
 * than `#include`d, since that header's own `class CSTGHeapManager`
 * declaration would conflict with this file's local minimal stand-in
 * above WITHIN THE SAME TRANSLATION UNIT (a hard redefinition error,
 * unlike the cross-TU same-mangled-name trick used everywhere else in
 * this project) -- this one function's signature doesn't reference the
 * class type at all, so it's safe to forward-declare standalone. */
extern "C" unsigned long CSTGHeapManager_Initialize(unsigned long base, unsigned long size);

/* Local minimal CSTGCPUInfo stand-in, matching midi_port_manager.cpp's
 * own internal declaration -- needed here to provide `sInstance`'s
 * storage (CSTGMidiPortManager::Initialize()'s own confirmed real
 * CPU-speed-scaled timing-constant tail reads `field8`). */
struct CSTGCPUInfo {
	static CSTGCPUInfo *sInstance;
	unsigned int _cpuCount, _khz;
	float field8;
};
CSTGCPUInfo *CSTGCPUInfo::sInstance;

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

static void check_str(const char *label, const char *got, const char *want)
{
	bool ok = strcmp(got, want) == 0;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s \"%s\"\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted \"%s\")\n", want);
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

	printf("[4] Initialize(): 5 embedded CSTGMidiQueue rings, ringCtl/bufBase\n"
	       "    wiring at +0x138/+0x208 -- the sec 10.230/MASTER_REFERENCE\n"
	       "    CSTGMidiQueueWriter::Write() ringCtl-NULL crash fix\n");
	{
		/* Real CSTGHeapManager, same setup style as test_heap_manager.cpp
		 * -- CSTGMidiQueue::Initialize() (midi_queue_init.cpp) now calls
		 * the REAL CSTGHeapManager::Alloc(unsigned long), so this test
		 * needs a genuinely working heap, not a raw-buffer stand-in. */
		unsigned long heapBufSize = 8 * 1024 * 1024;
		unsigned char *heapBuf = map32(heapBufSize);
		CSTGHeapManager_Initialize((unsigned long)heapBuf, heapBufSize);

		unsigned char *cpuInfoBuf = map32(sizeof(CSTGCPUInfo));
		memset(cpuInfoBuf, 0, sizeof(CSTGCPUInfo));
		CSTGCPUInfo *cpuInfo = (CSTGCPUInfo *)cpuInfoBuf;
		cpuInfo->field8 = 2500.0f; /* arbitrary cyclesPerTick: chosen so
					    * 0.04x/0.2x land on exact integers
					    * (100/500), avoiding a float-
					    * rounding false failure. */
		CSTGCPUInfo::sInstance = cpuInfo;

		unsigned char *portMgrBuf = map32(0x210);
		memset(portMgrBuf, 0xAA, 0x210); /* poison every byte first */
		/* Reproduce engine_init.cpp's own confirmed real struct-init
		 * block (the exact pre-condition the real caller sets up
		 * right before calling Initialize()). */
		portMgrBuf[0x0] = 0;
		portMgrBuf[0x1] = 0;
		portMgrBuf[0x2] = 0;
		portMgrBuf[0x3] = 0;
		*(unsigned int *)(portMgrBuf + 0xc)   = 0xffffffff;
		*(unsigned int *)(portMgrBuf + 0x70)  = 0xffffffff;
		*(unsigned int *)(portMgrBuf + 0xd4)  = 0xffffffff;
		*(unsigned int *)(portMgrBuf + 0x138) = 0;
		*(unsigned int *)(portMgrBuf + 0x13c) = 0;
		*(unsigned int *)(portMgrBuf + 0x140) = 0xffffffff;
		*(unsigned int *)(portMgrBuf + 0x1a4) = 0xffffffff;
		*(unsigned int *)(portMgrBuf + 0x208) = 0; /* the crash field */
		*(unsigned int *)(portMgrBuf + 0x20c) = 0;

		CSTGMidiPortManager *mgr = (CSTGMidiPortManager *)portMgrBuf;
		mgr->Initialize();

		check_eq("qStgOut (+0xc) mask == 0xfff",    *(unsigned int *)(portMgrBuf+0xc+0x8), 0xfff);
		check_eq("qStgOut (+0xc) format == 0",       *(unsigned int *)(portMgrBuf+0xc+0x4), 0);
		check_eq("qStgOut (+0xc) writeCursor == 0",  *(unsigned int *)(portMgrBuf+0xc+0xc), 0);
		check_str("qStgOut desc",  (char *)(portMgrBuf+0xc+0x21),  "STG MIDI Out");

		check_eq("qKgReg (+0x70) mask == 0x3ff",     *(unsigned int *)(portMgrBuf+0x70+0x8), 0x3ff);
		check_str("qKgReg desc",   (char *)(portMgrBuf+0x70+0x21), "KG Regular MIDI Out");

		check_eq("qKgRt (+0xd4) mask == 0x7f",       *(unsigned int *)(portMgrBuf+0xd4+0x8), 0x7f);
		check_str("qKgRt desc",    (char *)(portMgrBuf+0xd4+0x21), "KG Real Time MIDI Out");

		check_eq("qStgToKg (+0x140) mask == 0x1ff",  *(unsigned int *)(portMgrBuf+0x140+0x8), 0x1ff);
		check_eq("qStgToKg (+0x140) format == 1",    *(unsigned int *)(portMgrBuf+0x140+0x4), 1);
		check_str("qStgToKg desc", (char *)(portMgrBuf+0x140+0x21), "STG->KG");

		check_eq("qKgToStg (+0x1a4) mask == 0xff",   *(unsigned int *)(portMgrBuf+0x1a4+0x8), 0xff);
		check_eq("qKgToStg (+0x1a4) format == 1",    *(unsigned int *)(portMgrBuf+0x1a4+0x4), 1);
		check_str("qKgToStg desc", (char *)(portMgrBuf+0x1a4+0x21), "KG->STG");

		check_eq("fieldAt(0x138) == &qStgOut (ringCtl)",
			 *(unsigned int *)(portMgrBuf+0x138), ToU32(portMgrBuf+0xc));
		check_eq("fieldAt(0x13c) resolved (bufBase != 0)",
			 *(unsigned int *)(portMgrBuf+0x13c) != 0, 1);

		printf("      -- the actual sec 10.230 crash fix: --\n");
		check_eq("fieldAt(0x208) == &qStgToKg (ringCtl, was NULL before this fix)",
			 *(unsigned int *)(portMgrBuf+0x208), ToU32(portMgrBuf+0x140));
		check_eq("fieldAt(0x20c) resolved (bufBase != 0)",
			 *(unsigned int *)(portMgrBuf+0x20c) != 0, 1);

		check_eq("fieldAt(0x4) == (int)(0.04*cyclesPerTick)", (unsigned int)*(int *)(portMgrBuf+0x4), 100);
		check_eq("fieldAt(0x8) == (int)(0.2*cyclesPerTick)",  (unsigned int)*(int *)(portMgrBuf+0x8), 500);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
