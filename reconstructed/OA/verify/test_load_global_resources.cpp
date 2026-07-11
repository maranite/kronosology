// SPDX-License-Identifier: GPL-2.0
/*
 * test_load_global_resources.cpp  -  host-side known-answer tests for
 * load_global_resources() (batch 52, init_module() step 11).
 *
 * Links src/init/load_global_resources.cpp for real. Its own genuine
 * deep-subsystem callees (CSTGMultisampleBankManager::
 * StartupInitializeROMBank/StartupInitializeRAMBank/ScanFileSystem,
 * CSTGInstalledEXProducts::Initialize, CSTGKLMManager::AuthorizeBuiltins,
 * CSTGHeapManager::Alloc(unsigned int)) are deliberately MOCKED here with
 * call-tracking/scripted-return stubs rather than linked -- their real
 * homes (bar2_stubs_auth.cpp for the first four, klm_manager.cpp/
 * heap_manager_alloc_static.cpp for the last two) give them either
 * deliberately-constant no-op/safe-default bodies or heavy unrelated
 * dependency chains; mocking lets this test verify load_global_resources()'s
 * OWN real control flow (call order, the countdown transform, the
 * confirmed hard-fail vs. soft-fail distinction, the conditional final
 * Alloc()) directly, same rationale as every other "mock the deferred/
 * heavy callees, link the real caller" KAT in this project.
 *
 * `CSTGHeapManager_SetLastFixedBlock()`/`GetProgressBarValue()` are NOT
 * mocked -- they're real, self-contained definitions inside
 * load_global_resources.cpp itself (this test links that file, so
 * redefining them here would be a duplicate-symbol error).
 * `gSystemIsInitialized` is DIFFERENT: it's a real, ALREADY-defined
 * global from src/engine/push_unsolicited_message.cpp (confirmed the
 * same `.bss+0x10725c` symbol via independent relocation traces from
 * both call sites) -- load_global_resources.cpp only declares it
 * `extern`, so this test provides its own local definition instead of
 * linking that unrelated file.
 *
 * `CSTGHeapManager::sInstance` needs a real MAP_32BIT-backed buffer big
 * enough to cover the confirmed `+0x1e8498` heap-translation-base field
 * (oa_heap.h) -- same established "heapMgrBuf must be big enough" pattern
 * as verify/test_midi_port_manager.cpp's own [3], with `+0x1e8498` set to
 * 0 so `oa_heap_base()` reduces to the raw pointer stored at `+0x38`,
 * avoiding a second ~2MB allocation.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "auth.h"
#include "oa_internal.h"
#include "oa_heap.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

extern "C" int load_global_resources(void);
/* Real definition lives in src/engine/push_unsolicited_message.cpp (NOT
 * linked here, to keep this test focused) -- provide our own, matching
 * that file's own established `unsigned int` type exactly. */
extern "C" unsigned int gSystemIsInitialized;
unsigned int gSystemIsInitialized;

/* ---- COmapNKS4_GetProgressBarPercent mock ---- */
static unsigned char g_progressPercent;
extern "C" unsigned char COmapNKS4_GetProgressBarPercent(void) { return g_progressPercent; }

char *CSTGHeapManager::sInstance;

/* ---- CSTGKLMManager::sInstance / AuthorizeBuiltins mock ---- */
struct CSTGKLMManager *CSTGKLMManager::sInstance;
static int g_authorizeBuiltinsCalls;
void CSTGKLMManager::AuthorizeBuiltins(void) { g_authorizeBuiltinsCalls++; }

/* ---- CSTGMultisampleBankManager mocks ---- */
static int g_romCalls, g_ramCalls, g_scanCalls;
static bool g_ramBankResult = true;
static const char *g_romName;
static bool g_romFlag;
static unsigned char g_romCountdown;
static unsigned int g_romThis, g_ramThis, g_scanThis;

void CSTGMultisampleBankManager::StartupInitializeROMBank(const char *name, bool flag, unsigned char n)
{
	g_romCalls++;
	g_romName = name;
	g_romFlag = flag;
	g_romCountdown = n;
	g_romThis = ToU32(this);
}
bool CSTGMultisampleBankManager::StartupInitializeRAMBank()
{
	g_ramCalls++;
	g_ramThis = ToU32(this);
	return g_ramBankResult;
}
void CSTGMultisampleBankManager::ScanFileSystem()
{
	g_scanCalls++;
	g_scanThis = ToU32(this);
}

/* ---- CSTGInstalledEXProducts::Initialize mock ---- */
static int g_productsInitCalls;
static bool g_productsInitResult = true;
static unsigned int g_productsThis;
bool CSTGInstalledEXProducts::Initialize()
{
	g_productsInitCalls++;
	g_productsThis = ToU32(this);
	return g_productsInitResult;
}

/* ---- CSTGHeapManager::Alloc(unsigned int) mock ---- */
static int g_allocCalls;
static unsigned int g_allocArg;
static unsigned int g_allocResult = 0x1234;
unsigned int CSTGHeapManager::Alloc(unsigned int size)
{
	g_allocCalls++;
	g_allocArg = size;
	return g_allocResult;
}

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok) g_fail++;
	printf("  %s  %-56s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok) printf("        (wanted 0x%x)\n", want);
}
static void check_true(const char *label, bool ok)
{
	if (!ok) g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static unsigned char *g_heapMgrBuf, *g_heapRegionBuf;

static void reset_all()
{
	memset(g_heapMgrBuf, 0, 0x1e8500);
	memset(g_heapRegionBuf, 0, 0x70000);
	*(unsigned int *)(g_heapMgrBuf + 0x38) = ToU32(g_heapRegionBuf);
	*(unsigned int *)(g_heapMgrBuf + 0x1e8498) = 0;
	CSTGHeapManager::sInstance = (char *)g_heapMgrBuf;

	g_progressPercent = 0;
	g_authorizeBuiltinsCalls = 0;
	g_romCalls = g_ramCalls = g_scanCalls = 0;
	g_ramBankResult = true;
	g_romName = 0; g_romFlag = false; g_romCountdown = 0;
	g_romThis = g_ramThis = g_scanThis = 0;
	g_productsInitCalls = 0;
	g_productsInitResult = true;
	g_productsThis = 0;
	g_allocCalls = 0;
	g_allocArg = 0;
	g_allocResult = 0x1234;
	gSystemIsInitialized = 0;
}

int main(void)
{
	printf("load_global_resources test\n");
	printf("==============================================================\n");

	g_heapMgrBuf = (unsigned char *)mmap32(0x1e8500);
	g_heapRegionBuf = (unsigned char *)mmap32(0x70000);

	printf("[1] StartupInitializeRAMBank() fails: real hard-fail path, return -1\n");
	{
		reset_all();
		g_ramBankResult = false;

		int rc = load_global_resources();

		check_eq("return -1 (hard fail)", (unsigned int)rc, (unsigned int)-1);
		check_eq("ROM bank init called once", g_romCalls, 1);
		check_eq("RAM bank init called once", g_ramCalls, 1);
		check_eq("ScanFileSystem NOT called (hard-fail short-circuits)", g_scanCalls, 0);
		check_eq("AuthorizeBuiltins NOT called", g_authorizeBuiltinsCalls, 0);
		check_eq("gSystemIsInitialized NOT set", (unsigned int)gSystemIsInitialized, 0);
		check_eq("products->Initialize() NOT called", g_productsInitCalls, 0);
		check_eq("CSTGHeapManager::Alloc NOT called", g_allocCalls, 0);
	}

	printf("[2] Full success path, low progress (0): countdown==0x31, RAM budget under 10MB -> Alloc called\n");
	{
		reset_all();
		g_progressPercent = 0;
		*(unsigned int *)(g_heapRegionBuf + 0x6a534) = 0x9ffff;	/* well under 0x9fffff */
		*(unsigned int *)(g_heapRegionBuf + 0x10) = 0xCCCCCCCC;	/* poison, must be overwritten */

		int rc = load_global_resources();

		check_eq("return 0 (success)", (unsigned int)rc, 0);
		check_eq("countdown == 0x31 (progress==0 branch)", g_romCountdown, 0x31);
		check_true("ROM bank init 'this' == multisample bank mgr (heap_base+0x60524)",
			   g_romThis == ToU32(g_heapRegionBuf + 0x60524));
		check_true("ROM bank flag arg == true", g_romFlag == true);
		check_true("ROM bank name arg non-null", g_romName != 0);
		check_eq("ScanFileSystem called once", g_scanCalls, 1);
		check_eq("AuthorizeBuiltins called once", g_authorizeBuiltinsCalls, 1);
		check_eq("gSystemIsInitialized set to 1", (unsigned int)gSystemIsInitialized, 1);
		check_eq("products->Initialize() called once", g_productsInitCalls, 1);
		check_true("products 'this' == heap_base+0x14",
			   g_productsThis == ToU32(g_heapRegionBuf + 0x14));
		check_eq("CSTGHeapManager::Alloc called once", g_allocCalls, 1);
		check_eq("Alloc arg == 0xa00000 - usedBytes", g_allocArg, 0xa00000 - 0x9ffff);
		check_eq("returned slot stored at heap_base+0x10",
			 *(unsigned int *)(g_heapRegionBuf + 0x10), g_allocResult);
	}

	printf("[3] Full success path, high progress (100): countdown clamps to 1\n");
	{
		reset_all();
		g_progressPercent = 100;
		*(unsigned int *)(g_heapRegionBuf + 0x6a534) = 0;

		int rc = load_global_resources();

		check_eq("return 0", (unsigned int)rc, 0);
		check_eq("countdown == 1 (progress>0x30 branch)", g_romCountdown, 1);
	}

	printf("[4] usedBytes > 0x9fffff: final Alloc() genuinely SKIPPED\n");
	{
		reset_all();
		*(unsigned int *)(g_heapRegionBuf + 0x6a534) = 0xa00000;	/* well over 0x9fffff */
		*(unsigned int *)(g_heapRegionBuf + 0x10) = 0xDEADBEEF;	/* must stay untouched */

		int rc = load_global_resources();

		check_eq("return 0", (unsigned int)rc, 0);
		check_eq("CSTGHeapManager::Alloc NOT called", g_allocCalls, 0);
		check_eq("heap_base+0x10 left untouched",
			 *(unsigned int *)(g_heapRegionBuf + 0x10), 0xDEADBEEF);
	}

	printf("[5] products->Initialize() soft-fails: gSystemIsInitialized STILL set, overall return still 0\n");
	{
		reset_all();
		g_productsInitResult = false;
		*(unsigned int *)(g_heapRegionBuf + 0x6a534) = 0xa00000;	/* skip the Alloc branch, keep this scenario focused */

		int rc = load_global_resources();

		check_eq("return 0 (soft failure does not propagate)", (unsigned int)rc, 0);
		check_eq("gSystemIsInitialized STILL set to 1", (unsigned int)gSystemIsInitialized, 1);
		check_eq("products->Initialize() was called", g_productsInitCalls, 1);
	}

	printf("==============================================================\n");
	if (g_fail) {
		printf("%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
