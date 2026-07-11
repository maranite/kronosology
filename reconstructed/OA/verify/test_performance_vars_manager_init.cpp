// SPDX-License-Identifier: GPL-2.0
/*
 * test_performance_vars_manager_init.cpp  -  host-side known-answer test
 * for CSTGPerformanceVarsManager::Initialize() (batch 53).
 *
 * Links only performance_vars_manager_init.cpp + the real bank_memory.cpp
 * (needed for genuine, non-overlapping bump allocation across the two
 * 0xb6d0-byte CSTGPerformanceVars objects) -- the five sub-object
 * Initialize()/ctor dependencies (CSTGAudioInputMixerBase's ctor,
 * CSTGAudioInputMixer::Initialize, CSTGMasterLRMixer::Initialize,
 * CSTGEffectRackVars::Initialize, CSetListEQ::Initialize) and
 * CSTGChannelValues::Initialize() (already real elsewhere, in
 * global.cpp, but far too heavy to link here) are all given local
 * call-tracking mocks, matching this project's established "mock the
 * deferred/heavy callees, link the real caller" KAT precedent.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine_init.h"
#include "oa_bank_memory.h"

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

static unsigned char *mmap32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

/* ---- call-tracking mocks for the five deferred sub-object dependencies ---- */
static int g_ctorCalls;
CSTGAudioInputMixerBase::CSTGAudioInputMixerBase() { g_ctorCalls++; }

static int g_aimInitCalls;
static void *g_aimInitThis[4];
static unsigned int g_aimInitArg[4];
void CSTGAudioInputMixer::Initialize(unsigned int count)
{
	if (g_aimInitCalls < 4) {
		g_aimInitThis[g_aimInitCalls] = this;
		g_aimInitArg[g_aimInitCalls] = count;
	}
	g_aimInitCalls++;
}

static int g_lrInitCalls;
static void *g_lrInitThis[4];
static unsigned int g_lrInitArg[4];
void CSTGMasterLRMixer::Initialize(unsigned int count)
{
	if (g_lrInitCalls < 4) {
		g_lrInitThis[g_lrInitCalls] = this;
		g_lrInitArg[g_lrInitCalls] = count;
	}
	g_lrInitCalls++;
}

static int g_efxInitCalls;
static void *g_efxInitThis[4];
static void *g_efxInitOwner[4];
void CSTGEffectRackVars::Initialize(CSTGPerformanceVars *owner)
{
	if (g_efxInitCalls < 4) {
		g_efxInitThis[g_efxInitCalls] = this;
		g_efxInitOwner[g_efxInitCalls] = owner;
	}
	g_efxInitCalls++;
}

static int g_eqInitCalls;
static void *g_eqInitThis[4];
static unsigned int g_eqInitArg[4];
void CSetListEQ::Initialize(unsigned int count)
{
	if (g_eqInitCalls < 4) {
		g_eqInitThis[g_eqInitCalls] = this;
		g_eqInitArg[g_eqInitCalls] = count;
	}
	g_eqInitCalls++;
}

static int g_cvInitCalls;
static void *g_cvInitThis[40];
void CSTGChannelValues::Initialize()
{
	if (g_cvInitCalls < 40)
		g_cvInitThis[g_cvInitCalls] = this;
	g_cvInitCalls++;
}
void CSTGChannelValues::InitializeLongHand() {}
unsigned char CSTGChannelValues::sTemplateReady;
unsigned char CSTGChannelValues::sTemplate[0x92c];

int main(void)
{
	printf("CSTGPerformanceVarsManager::Initialize() KAT\n");
	printf("=============================================\n");

	unsigned char *pool = mmap32(0x40000);
	CSTGBankMemory::Initialize(pool, 0x40000);

	memset(CSTGPerformanceVarsManager::sInstance, 0xcc, 12);

	((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->Initialize();

	/* sInstance[0]/[4] are packed 32-bit fields (ToU32/FromU32 convention),
	 * NOT native 8-byte pointers -- a native `unsigned char**` dereference
	 * here would read 8 bytes spanning BOTH packed slots on a 64-bit host. */
	unsigned char *mgr0 = (unsigned char *)(unsigned long)(*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0));
	unsigned char *mgr1 = (unsigned char *)(unsigned long)(*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4));

	printf("[1] both slots allocated, distinct, 0xb6d0 bytes apart (or more)\n");
	{
		check_eq("mgr0 != NULL", mgr0 != 0, 1);
		check_eq("mgr1 != NULL", mgr1 != 0, 1);
		check_eq("mgr0 != mgr1", mgr0 != mgr1, 1);
		check_eq("mgr1 - mgr0 >= 0xb6d0", (unsigned int)(mgr1 - mgr0) >= 0xb6d0, 1);
	}

	printf("[2] sInstance[8] (active-slot selector) untouched by Initialize()\n");
	{
		check_eq("sInstance[8] still 0xcc (Initialize() never writes it)",
			 CSTGPerformanceVarsManager::sInstance[8], 0xcc);
	}

	printf("[3] per-mgr scalar fields (slot 0)\n");
	{
		check_eq("mgr0+0x0 == 8", *(unsigned int *)(mgr0 + 0x0), 8);
		check_eq("mgr0+0x23d0 == 0 (slot index i=0)", mgr0[0x23d0], 0);
		check_eq("mgr0+0x23d1 == 0", mgr0[0x23d1], 0);
		check_eq("mgr0+0x23dc == 0", mgr0[0x23dc], 0);
		check_eq("mgr0+0x23e4 == 0x3e8", *(unsigned int *)(mgr0 + 0x23e4), 0x3e8);
		check_eq("mgr0+0x23f0 == 0x3f800000 (1.0f)", *(unsigned int *)(mgr0 + 0x23f0), 0x3f800000);
		check_eq("mgr0+0x2404 == 0", *(unsigned int *)(mgr0 + 0x2404), 0);
		check_eq("mgr0+0x2408 == 0", *(unsigned int *)(mgr0 + 0x2408), 0);
	}
	printf("[4] per-mgr scalar fields (slot 1, i=1)\n");
	{
		check_eq("mgr1+0x23d0 == 1 (slot index i=1)", mgr1[0x23d0], 1);
	}

	printf("[5] LOOP1 (12x 0x210-stride channel-mixer heads at mgr0+0x20)\n");
	{
		unsigned char *p = mgr0 + 0x20;
		check_eq("entry0 +0x8 == 0", *(unsigned int *)(p + 0x8), 0);
		check_eq("entry0 +0x1c8 == 0x20", p[0x1c8], 0x20);
		check_eq("entry0 +0x200 == 64.0f bits", *(unsigned int *)(p + 0x200), 0x42800000);
		unsigned char *p11 = mgr0 + 0x20 + 11 * 0x210;
		check_eq("entry11(last) +0x1f4 == 64.0f bits", *(unsigned int *)(p11 + 0x1f4), 0x42800000);
	}

	printf("[6] 16-entry small-mixer-state pointer table (mgr0+0x1e60..+0x1e9c)\n");
	{
		check_eq("table[0] == mgr0+0x20", *(unsigned int *)(mgr0 + 0x1e60),
			 (unsigned int)(unsigned long)(mgr0 + 0x20));
		check_eq("table[11] == mgr0+0x16d0", *(unsigned int *)(mgr0 + 0x1e8c),
			 (unsigned int)(unsigned long)(mgr0 + 0x16d0));
		check_eq("table[12] == mgr0+0x18e0", *(unsigned int *)(mgr0 + 0x1e90),
			 (unsigned int)(unsigned long)(mgr0 + 0x18e0));
		check_eq("table[13] == mgr0+0x1a50", *(unsigned int *)(mgr0 + 0x1e94),
			 (unsigned int)(unsigned long)(mgr0 + 0x1a50));
		check_eq("table[14] == mgr0+0x1bc0", *(unsigned int *)(mgr0 + 0x1e98),
			 (unsigned int)(unsigned long)(mgr0 + 0x1bc0));
		check_eq("table[15] == mgr0+0x1d10", *(unsigned int *)(mgr0 + 0x1e9c),
			 (unsigned int)(unsigned long)(mgr0 + 0x1d10));
	}

	printf("[7] sub-object Initialize() dispatch (per slot: AIM, LR, EFX, EQ)\n");
	{
		check_eq("CSTGAudioInputMixer::Initialize called exactly twice (2 slots)",
			 g_aimInitCalls, 2);
		check_eq("  slot0 this == mgr0", (unsigned int)(unsigned long)g_aimInitThis[0],
			 (unsigned int)(unsigned long)mgr0);
		check_eq("  slot0 arg  == 0", g_aimInitArg[0], 0);
		check_eq("  slot1 this == mgr1", (unsigned int)(unsigned long)g_aimInitThis[1],
			 (unsigned int)(unsigned long)mgr1);
		check_eq("  slot1 arg  == 1", g_aimInitArg[1], 1);

		check_eq("CSTGMasterLRMixer::Initialize called exactly twice", g_lrInitCalls, 2);
		check_eq("  slot0 this == mgr0+0x2140", (unsigned int)(unsigned long)g_lrInitThis[0],
			 (unsigned int)(unsigned long)(mgr0 + 0x2140));
		check_eq("  slot1 arg == 1", g_lrInitArg[1], 1);

		check_eq("CSTGEffectRackVars::Initialize called exactly twice", g_efxInitCalls, 2);
		check_eq("  slot0 this == mgr0+0x20", (unsigned int)(unsigned long)g_efxInitThis[0],
			 (unsigned int)(unsigned long)(mgr0 + 0x20));
		check_eq("  slot0 owner == mgr0 (self-referential)",
			 (unsigned int)(unsigned long)g_efxInitOwner[0],
			 (unsigned int)(unsigned long)mgr0);

		check_eq("CSetListEQ::Initialize called exactly twice", g_eqInitCalls, 2);
		check_eq("  slot1 this == mgr1+0x2160", (unsigned int)(unsigned long)g_eqInitThis[1],
			 (unsigned int)(unsigned long)(mgr1 + 0x2160));
		check_eq("  slot1 arg == 1", g_eqInitArg[1], 1);
	}

	printf("[8] CSTGChannelValues::Initialize() called 16x per slot, 32 total,\n"
	       "    stride 0x92c starting at mgr+0x2410\n");
	{
		check_eq("32 total calls", g_cvInitCalls, 32);
		check_eq("call 0 this == mgr0+0x2410", (unsigned int)(unsigned long)g_cvInitThis[0],
			 (unsigned int)(unsigned long)(mgr0 + 0x2410));
		check_eq("call 15 this == mgr0+0xb6d0-0x92c (last slot, exactly buffer end)",
			 (unsigned int)(unsigned long)g_cvInitThis[15],
			 (unsigned int)(unsigned long)(mgr0 + 0x2410 + 15 * 0x92c));
		check_eq("call 16 (first of slot1) this == mgr1+0x2410",
			 (unsigned int)(unsigned long)g_cvInitThis[16],
			 (unsigned int)(unsigned long)(mgr1 + 0x2410));
	}

	printf("[9] CSTGAudioInputMixerBase base ctor invoked once per slot (via\n"
	       "    the implicit CSTGAudioInputMixer derived ctor)\n");
	{
		check_eq("ctor called exactly twice", g_ctorCalls, 2);
	}

	printf("[10] channel-0 inline pre-zero pass is functionally inert (fully\n"
	       "     overwritten by the CSTGChannelValues::Initialize() mock's own\n"
	       "     no-op body here -- just confirm it doesn't crash/corrupt\n"
	       "     neighboring fields)\n");
	{
		unsigned char *ch0 = mgr0 + 0x2410;
		check_eq("ch0+0x0 == 0 (pre-zero pass)", *(unsigned int *)(ch0 + 0x0), 0);
		check_eq("ch0+0x5aa == 1 (pre-zero pass tail)", ch0[0x5aa], 1);
	}

	if (g_fail) {
		printf("\n%d CHECK(S) FAILED\n", g_fail);
		return 1;
	}
	printf("\nALL CHECKS PASSED\n");
	return 0;
}
