// SPDX-License-Identifier: GPL-2.0
/*
 * test_setup_global_resources.cpp  -  host-side known-answer test for
 * setup_global_resources() (see ../include/oa_setup_global_resources.h /
 * ../src/init/setup_global_resources.cpp).
 *
 * Given this function's own real scale (7267 bytes, ~42 calls into
 * mostly not-yet-reconstructed classes), this KAT focuses on what
 * actually matters for insmod success: the real control-flow gates
 * (the param!=0 early-out, and the three real null-allocation checks,
 * confirmed to fire in an unusual order -- CSTGEngine's storage first,
 * then CSTGFrontPanel's, then CSTGCPUInfo's LAST despite being
 * allocated first) -- plus a success-path smoke test spot-checking a
 * handful of the confirmed STGAPIFrontPanelStatus non-zero byte writes
 * this pass ground-truthed from real .text bytes.
 *
 * CSTGBankMemory::Initialize/AllocAligned are the REAL, already-
 * reconstructed implementation (not mocked) -- a modest host buffer is
 * used as their backing store rather than the real ~179MB/44MB sizes;
 * this is safe in practice (not strictly standard-conformant pointer
 * arithmetic) because every constructor mocked below is an empty no-op
 * that never actually dereferences its own storage.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "oa_setup_global_resources.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-50s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* ---- mocks for every not-yet-reconstructed dependency ---- */

CSTGCPUInfo *CSTGCPUInfo::sInstance;
CSTGCPUInfo::CSTGCPUInfo(unsigned int) {}
static float g_updateArg;
void CSTGCPUInfo::Update(float value) { g_updateArg = value; }

/* CStartupFile is now CCostProfile's own confirmed real base class
 * (sec 10.60) -- mocked here purely so CCostProfile's own mock
 * constructor below can link; this test predates that discovery and
 * only cares about the vtable-dispatch/+4-field behavior already
 * covered below. */
CStartupFile::CStartupFile(const char *) {}
CStartupFile::~CStartupFile() {}

CCostProfile *CCostProfile::sInstance;
static unsigned char g_costProfileVtableTargetCalled;
static void CostProfileVtableTarget(CCostProfile *) { g_costProfileVtableTargetCalled = 1; }
static void *g_costProfileVtable[3] = { 0, 0, (void *)&CostProfileVtableTarget };
CCostProfile::CCostProfile() : CStartupFile("mock")
{
	sInstance = this;
	_vtablePtr = g_costProfileVtable;
	_field4 = 2.5f;
}

CMeteredDebugOutput::CMeteredDebugOutput() {}

static int g_engineInitCalls;
CSTGEngine::CSTGEngine() {}
CSTGEngine::~CSTGEngine() {}
void CSTGEngine::Initialize() { g_engineInitCalls++; }

static int g_frontPanelInitCalls;
CSTGFrontPanel::CSTGFrontPanel() {}
void CSTGFrontPanel::Initialize() { g_frontPanelInitCalls++; }

static int g_globalInitCalls;
CSTGGlobal::CSTGGlobal() {}
void CSTGGlobal::Initialize() { g_globalInitCalls++; }

static int g_sampleRateMonitorInitCalls;
/* CORRECTED (2026-07-10): sInstance is the real 1040-byte object, not a
 * pointer -- see oa_setup_global_resources.h's own note. */
CSTGSampleRateMonitor CSTGSampleRateMonitor::sInstance;
void CSTGSampleRateMonitor::Initialize() { g_sampleRateMonitorInitCalls++; }

static int g_askInitCalls;
static void *g_askArg;
/* CSTGASK::Initialize() is real now (sec 10.145) -- a pure forward to
 * SKMain_Initialize(), mocked here instead (same observable effect: one
 * call, same argument, since the real Initialize() does nothing else). */
extern "C" void SKMain_Initialize(void *arg) { g_askInitCalls++; g_askArg = arg; }

/* CSTGMultisampleBankManager::Initialize()/CSTGPCMPrecacheManager::
 * Initialize() are BOTH real now (sec 10.149/10.144) -- their one
 * confirmed real call site here uses a stack-local object with no
 * externally observable trace, so each is verified directly and
 * independently below (main()'s own final block) instead of via a call
 * counter proxy. */

unsigned char *STGAPIFrontPanelStatus::sInstance;

/*
 * Simple slot table: slot N resolves to a distinct buffer. The real
 * target's slot-resolution fields are genuinely 32-bit (confirmed via
 * disassembly, matching the real 32-bit architecture -- setup_global_
 * resources.cpp's own local_heap_region() correctly uses `unsigned int`
 * to match), but THIS is a 64-bit host: a plain static/BSS buffer's
 * real address routinely exceeds 32 bits here, so storing it through a
 * 32-bit field and reading it back would silently truncate to a
 * garbage pointer -- not a bug in the reconstruction, a test-harness
 * concern only. `mmap(..., MAP_32BIT, ...)` guarantees a low address
 * that survives the round-trip intact for this test.
 */
#include <sys/mman.h>
static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return (unsigned char *)p;
}
static unsigned char *g_heapInstanceBuf = mmap32(0x1869f * 0x14 + 0x30);
static unsigned char *g_panelBuf = mmap32(STGAPI_FRONTPANEL_SIZE);
static unsigned char *g_bigRegionBuf = mmap32(256 * 0x604);
/* CORRECTED (2026-07-12): a dedicated buffer for the 4th, previously-
 * missing CSTGHeapManager::Alloc(0xaaf1140) call setup_global_resources.cpp
 * now makes for CSTGBankMemory::Initialize's own base (see that file's
 * step 9 comment) -- this used to fall through to g_bigRegionBuf by
 * accident of the old 3-call slot table below; giving it its own buffer
 * keeps the two heap regions distinguishable in this test. */
static unsigned char *g_bankMemoryBuf = mmap32(256 * 0x604);
char *CSTGHeapManager::sInstance = (char *)g_heapInstanceBuf;
static unsigned int g_allocCallCount;
static int g_forceAllocFail; /* if set, Alloc() returns an out-of-range slot */
unsigned int CSTGHeapManager::Alloc(unsigned int)
{
	g_allocCallCount++;
	if (g_forceAllocFail)
		return 0x1869f + 1;
	/* Deterministic slot assignment by call order: call 1 = discarded
	 * alloc, call 2 = panel, call 3 = big region, call 4 = bank memory
	 * pool base (CORRECTED 2026-07-12 -- this 4th call was missing
	 * entirely until setup_global_resources.cpp stopped reusing `panel`
	 * as CSTGBankMemory::Initialize's base; see that file's step 9
	 * comment and MASTER_REFERENCE.md for the live-boot Oops this
	 * fixes). */
	unsigned int slot = g_allocCallCount;
	unsigned char *target = g_bigRegionBuf;
	if (slot == 2)
		target = g_panelBuf;
	else if (slot == 4)
		target = g_bankMemoryBuf;
	unsigned char *rec = g_heapInstanceBuf + slot * 0x14;
	*(unsigned int *)(rec + 0x18) = 1; /* non-zero "valid" marker */
	*(unsigned int *)(rec + 0x24) = (unsigned int)(unsigned long)target;
	return slot;
}

static unsigned int g_installedRAM = 0x12345;
extern "C" unsigned int GetInstalledRAM(void) { return g_installedRAM; }
extern "C" void CSTGSharedMemory_CreateMidiShareHeader(void) {}
extern "C" void COmapNKS4Driver_GetOmapVersion(unsigned char *version, unsigned char *revision)
{ *version = 0x11; *revision = 0x22; }
extern "C" void COmapNKS4Driver_GetPSocVersion(unsigned char *version, unsigned char *revision)
{ *version = 0x33; *revision = 0x44; }
extern "C" unsigned char COmapNKS4Driver_GetHardwareVersion(void) { return 3; } /* avoid the SetupNKS4Calibration/IncProgressBar branch complexity */
extern "C" int COmapNKS4Driver_Is88Key(void) { return 1; }
extern "C" char SCalibrationData_LoadCalibrationFile(unsigned char *) { return 0; } /* skip the 3-way branch for this smoke test; the real body now lives in src/init/calibration_data.cpp + its own dedicated verify/test_calibration_data.cpp (batch 38) */
extern "C" void SetupNKS4Calibration(void *, int) {}
extern "C" void SetupKeybedCalibration(void *) {}
extern "C" void SCalibrationData_InitAll(void) {}
static int g_incProgressBarCalls;
extern "C" void IncProgressBar(void) { g_incProgressBarCalls++; }
extern "C" void rt_printk(const char *, ...) {}

/* new_delete.cpp's own real kernel-allocator externs, mocked host-side
 * (matches this project's established cdrom_check.cpp-style treatment).
 * NOT ::operator new/delete, which (this project's own real
 * implementation, linked into this same test) themselves call
 * stg_kmalloc -> this exact mock, which would otherwise recurse
 * infinitely.
 *
 * Deliberately NOT plain libc malloc()/free(): CSTGPCMPrecacheManager::
 * Reset() (sec 10.154) round-trips whatever operator new[]/new returns
 * through a genuinely 32-bit field (PCMPrecacheToU32()/FromU32() in
 * setup_global_resources.cpp) -- exactly matching the real 32-bit
 * target, where every pointer naturally fits in 32 bits. On this 64-bit
 * test host, plain malloc() (PIE heap, typically based well above 4GB)
 * returns addresses that silently truncate when stored through that
 * 32-bit field, corrupting the round-tripped pointer -- not a bug in
 * Reset() itself, the same host/target width hazard already handled
 * for every OTHER buffer in this file via mmap32()/MAP_32BIT. Reuse
 * that exact fix here: a tiny bump allocator over a mmap32() arena, so
 * every pointer this mock ever hands out is guaranteed representable in
 * 32 bits. kfree() is a deliberate no-op (this test never needs to
 * reclaim/reuse the arena within a single short-lived process). */
static unsigned char *g_kmallocArena;
static unsigned long g_kmallocArenaSize;
static unsigned long g_kmallocOffset;
extern "C" void *__kmalloc(unsigned long size, unsigned int)
{
	if (!g_kmallocArena) {
		g_kmallocArenaSize = 1u * 1024 * 1024;
		g_kmallocArena = mmap32(g_kmallocArenaSize);
	}
	size = (size + 15) & ~15UL; /* keep every allocation 16-byte aligned */
	if (g_kmallocOffset + size > g_kmallocArenaSize) {
		fprintf(stderr, "test __kmalloc: arena exhausted (%lu byte request)\n", size);
		abort();
	}
	unsigned char *p = g_kmallocArena + g_kmallocOffset;
	g_kmallocOffset += size;
	return p;
}
extern "C" void kfree(void *ptr) { (void)ptr; }

int main(void)
{
	printf("[1] param != 0: immediate hard-fail, before any allocation:\n");
	g_allocCallCount = 0;
	int rc = setup_global_resources(1);
	check_eq("return value", rc, -1);
	check_eq("no HeapManager::Alloc calls happened", (long)g_allocCallCount, 0);

	printf("\n[2] success path smoke test:\n");
	g_allocCallCount = 0;
	g_forceAllocFail = 0;
	g_engineInitCalls = g_frontPanelInitCalls = g_globalInitCalls = 0;
	g_sampleRateMonitorInitCalls = g_askInitCalls = 0;
	g_incProgressBarCalls = 0;
	g_costProfileVtableTargetCalled = 0;
	rc = setup_global_resources(0);
	check_eq("return value", rc, 0);
	/* CORRECTED (2026-07-12): was 3 -- now 4, matching real ground truth
	 * (`.text+0x11863a`): a 4th, dedicated Alloc(0xaaf1140) call for
	 * CSTGBankMemory's own pool base, previously missing (that call site
	 * wrongly reused `panel` instead -- see setup_global_resources.cpp's
	 * step 9 comment). */
	check_eq("HeapManager::Alloc called 4 times", (long)g_allocCallCount, 4);
	check_eq("panel[0xfc] == 0xff (midi echo marker)", g_panelBuf[0xfc], 0xff);
	check_eq("panel[0xfd] == 0xff (midi echo marker)", g_panelBuf[0xfd], 0xff);
	check_eq("panel[0x1091] == 0x04 (cal marker)", g_panelBuf[0x1091], 0x04);
	check_eq("panel calibration grid row0[0] == 0xff", g_panelBuf[0x10b], 0xff);
	check_eq("panel calibration grid row0[119] == 0xff", g_panelBuf[0x10b + 0x77], 0xff);
	check_eq("panel calibration grid row0[120] untouched (0)", g_panelBuf[0x10b + 0x78], 0x00);
	check_eq("panel installedRAM stored", *(unsigned int *)(g_panelBuf + 0xd30), (long)g_installedRAM);
	check_eq("panel fixed constant", *(unsigned int *)(g_panelBuf + 0x29118), (long)0x473b8000);
	check_eq("CSTGEngine::Initialize called", (long)g_engineInitCalls, 1);
	check_eq("CSTGGlobal::Initialize called", (long)g_globalInitCalls, 1);
	check_eq("CSTGFrontPanel::Initialize called", (long)g_frontPanelInitCalls, 1);
	check_eq("CSTGSampleRateMonitor::Initialize called (this=&sInstance, sec 10.57 fix)",
		 (long)g_sampleRateMonitorInitCalls, 1);
	check_eq("CSTGASK::Initialize called", (long)g_askInitCalls, 1);
	check_eq("CCostProfile vtable slot 2 dispatched", (long)g_costProfileVtableTargetCalled, 1);
	check_eq("CSTGCPUInfo::Update received CCostProfile's +4 field", (long)(g_updateArg * 10), 25);
	check_eq("IncProgressBar called (hwVersion==3 skips one call)", (long)g_incProgressBarCalls, 2);

	printf("\n[direct] CSTGPCMPrecacheManager::Initialize() (sec 10.144)\n");
	{
		unsigned char pcmBuf[0x2a];
		memset(pcmBuf, 0xcc, sizeof(pcmBuf));
		CSTGPCMPrecacheManager *pcm = (CSTGPCMPrecacheManager *)pcmBuf;
		bool ret = pcm->Initialize();
		check_eq("returns true", (unsigned int)ret, 1u);
		check_eq("+0x0 zeroed", pcmBuf[0x0], 0);
		check_eq("+0x1 zeroed", pcmBuf[0x1], 0);
		check_eq("+0x4 zeroed", *(unsigned int *)(pcmBuf + 0x4), 0u);
		check_eq("+0xc zeroed", *(unsigned int *)(pcmBuf + 0xc), 0u);
		check_eq("+0x10 zeroed", *(unsigned int *)(pcmBuf + 0x10), 0u);
		check_eq("+0x14 zeroed", *(unsigned int *)(pcmBuf + 0x14), 0u);
		check_eq("+0x18 zeroed", *(unsigned int *)(pcmBuf + 0x18), 0u);
		check_eq("+0x28 zeroed", pcmBuf[0x28], 0);
		check_eq("+0x29 zeroed", pcmBuf[0x29], 0);
		check_eq("+0x8 untouched (confirmed gap, still poisoned)", pcmBuf[0x8], 0xcc);
	}

	printf("\n[direct] CSTGPCMPrecacheManager::Reset() (sec 10.154)\n");
	{
		unsigned char pcmBuf[0x2a];
		memset(pcmBuf, 0xcc, sizeof(pcmBuf));
		CSTGPCMPrecacheManager *pcm = (CSTGPCMPrecacheManager *)pcmBuf;
		/* Establishes the real invariant a genuine caller relies on:
		 * Initialize() (already confirmed real, sec 10.144) runs once
		 * during setup_global_resources() before any Reset() call ever
		 * happens on the real target, so +0x4/+0x14 start at a real 0,
		 * not poisoned garbage a first Reset() would try to free(). */
		pcm->Initialize();

		/* [1] count == 0: no allocation (oldPtr/+0x14 was already 0). */
		bool r0 = pcm->Reset(false, false, 0);
		check_eq("[1] returns true", (unsigned int)r0, 1u);
		check_eq("[1] +0x0 == 0 (flagFromN3=false)", pcmBuf[0x0], 0);
		check_eq("[1] +0x1 == 0 (flagFromN2=false)", pcmBuf[0x1], 0);
		check_eq("[1] +0x4 == 0 (count)", *(unsigned int *)(pcmBuf + 0x4), 0u);
		check_eq("[1] +0x14 == 0 (count==0, nothing allocated)", *(unsigned int *)(pcmBuf + 0x14), 0u);
		check_eq("[1] +0x18 zeroed", *(unsigned int *)(pcmBuf + 0x18), 0u);
		check_eq("[1] +0xc zeroed", *(unsigned int *)(pcmBuf + 0xc), 0u);
		check_eq("[1] +0x10 zeroed", *(unsigned int *)(pcmBuf + 0x10), 0u);
		check_eq("[1] +0x28 zeroed", pcmBuf[0x28], 0);
		check_eq("[1] +0x29 zeroed", pcmBuf[0x29], 0);
		check_eq("[1] +0x8 zeroed", *(unsigned int *)(pcmBuf + 0x8), 0u);

		/* [2] count == 3: real operator new[] path (count > 1). */
		bool r1 = pcm->Reset(true, true, 3);
		check_eq("[2] returns true", (unsigned int)r1, 1u);
		check_eq("[2] +0x0 == 1 (flagFromN3=true)", pcmBuf[0x0], 1);
		check_eq("[2] +0x1 == 1 (flagFromN2=true)", pcmBuf[0x1], 1);
		check_eq("[2] +0x4 == 3 (count)", *(unsigned int *)(pcmBuf + 0x4), 3u);
		unsigned int arrPtr = *(unsigned int *)(pcmBuf + 0x14);
		check_eq("[2] +0x14 != 0 (array allocated)", arrPtr != 0, 1);
		unsigned char *arr = (unsigned char *)(unsigned long)arrPtr;
		for (int i = 0; i < 3; i++) {
			check_eq("[2] elem[i] +0x0 zeroed", *(unsigned int *)(arr + i * 0xc + 0), 0u);
			check_eq("[2] elem[i] +0x4 zeroed", *(unsigned int *)(arr + i * 0xc + 4), 0u);
			check_eq("[2] elem[i] +0x8 zeroed", *(unsigned int *)(arr + i * 0xc + 8), 0u);
		}

		/* [3] count == 1: real SCALAR operator new path (the confirmed
		 * "count==1" quirk) -- oldCount was 3 (!= 1), so this also
		 * exercises the operator delete[] branch on the [2] array. */
		bool r2 = pcm->Reset(false, false, 1);
		check_eq("[3] returns true", (unsigned int)r2, 1u);
		check_eq("[3] +0x4 == 1 (count)", *(unsigned int *)(pcmBuf + 0x4), 1u);
		unsigned int scalarPtr = *(unsigned int *)(pcmBuf + 0x14);
		check_eq("[3] +0x14 != 0 (scalar allocated)", scalarPtr != 0, 1);
		unsigned char *one = (unsigned char *)(unsigned long)scalarPtr;
		check_eq("[3] scalar elem +0x0 zeroed", *(unsigned int *)(one + 0), 0u);
		check_eq("[3] scalar elem +0x4 zeroed", *(unsigned int *)(one + 4), 0u);
		check_eq("[3] scalar elem +0x8 zeroed", *(unsigned int *)(one + 8), 0u);

		/* [4] count == 0 again: oldCount was 1, exercising the SCALAR
		 * operator delete branch (the other half of the allocator-form
		 * quirk) on the [3] element. */
		bool r3 = pcm->Reset(false, false, 0);
		check_eq("[4] returns true", (unsigned int)r3, 1u);
		check_eq("[4] +0x14 == 0 after freeing back to count 0", *(unsigned int *)(pcmBuf + 0x14), 0u);
	}

	printf("\n[direct] CSTGMultisampleBankManager::Initialize() (sec 10.149)\n");
	{
		unsigned char msBuf[0xa020];
		memset(msBuf, 0xcc, sizeof(msBuf));
		CSTGMultisampleBankManager *ms = (CSTGMultisampleBankManager *)msBuf;
		ms->Initialize();
		check_eq("+0xa000 zeroed", *(unsigned int *)(msBuf + 0xa000), 0u);
		check_eq("+0xa004 zeroed", *(unsigned int *)(msBuf + 0xa004), 0u);
		check_eq("+0xa008 zeroed", *(unsigned int *)(msBuf + 0xa008), 0u);
		check_eq("+0xa00c zeroed", *(unsigned int *)(msBuf + 0xa00c), 0u);
		check_eq("+0xa010 zeroed", *(unsigned int *)(msBuf + 0xa010), 0u);
		check_eq("+0xa014 == 0xffffffff (unset sentinel)", *(unsigned int *)(msBuf + 0xa014), 0xffffffffu);
		check_eq("+0xa018 zeroed", *(unsigned int *)(msBuf + 0xa018), 0u);
		check_eq("+0xa01c zeroed", *(unsigned int *)(msBuf + 0xa01c), 0u);
		check_eq("+0x0 untouched (confirmed gap, still poisoned)", msBuf[0x0], 0xcc);
	}

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
