// SPDX-License-Identifier: GPL-2.0
/*
 * test_engine_startup_bits2.cpp  -  KAT for
 * ../src/engine/engine_startup_bits2.cpp's five reconstructed methods:
 * CLoadBalancer::Initialize()/~CLoadBalancer(), CPowerOffTimer::
 * Initialize(), CSTGDiskCostManager::Initialize(), CSTGCommonLFO::
 * Initialize(), CSTGCommonStepSeq::Initialize().
 *
 * Links src/engine/managers.cpp (for CLoadBalancer/CPowerOffTimer/
 * CEmergencyStealer/CSTGDiskCostManager's already-reconstructed
 * constructors) and src/engine/engine_startup_bits.cpp (for
 * CSTGCPUInfo's already-reconstructed constructor) -- the same RTAI-
 * mutex mocks test_managers.cpp already established are reused here
 * verbatim, since managers.cpp's constructors need them regardless of
 * which specific class this test exercises.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"
#include "oa_bank_memory.h"

/* ---- mocks needed to link managers.cpp / engine_startup_bits.cpp ---- */
static int g_mutexInitCalls;
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" void *rtwrap_malloc(unsigned int size) { return malloc(size); }
extern "C" void rtwrap_pthread_mutex_init(void *, void *) { g_mutexInitCalls++; }
void CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors() { }
/* Link-satisfying mocks for sec 10.144's new small `Initialize()`/
 * `ProcessCommands()` bodies -- this file doesn't link global.cpp, so
 * ResolveActivePerformanceVarsManagerRaw() needs its own stub here. */
unsigned char *ResolveActivePerformanceVarsManagerRaw() { return 0; }
/* Sec 10.148: CSTGCDWorker_InitializeBuffer() is now real (managers.cpp)
 * and calls __kmalloc directly -- link-satisfying mock only, this file
 * never calls CSTGCDWorker::Initialize() itself. */
extern "C" void *__kmalloc(unsigned long size, unsigned int) { return malloc(size); }
void CSTGHDRManager::ProcessPlaybackCommands() { }
void CSTGHDRManager::ProcessRecordCommands() { }
void CSTGHDRManager::ProcessSamplerCommands() { }
static int g_mutexattrCalls;
extern "C" void rtwrap_pthread_mutexattr_init(void *) { g_mutexattrCalls++; }
extern "C" int  get_pthread_recursive_attr_constant(void) { return 1; }
static int g_condInitCalls;
extern "C" unsigned int get_sizeof_rtwrap_pthread_cond(void) { return 24; }
extern "C" void rtwrap_pthread_cond_init(void *, void *) { g_condInitCalls++; }
CSTGAudioManager::~CSTGAudioManager() { }
/* Sec 10.147: ~CSTGVoiceAllocator()/~CSTGMessageProcessor() are now real
 * (see managers.cpp) -- link-satisfying only, same reasoning as
 * test_global.cpp's own identical addition. */
extern "C" void rtwrap_pthread_mutex_destroy(void *) { }
extern "C" void rtwrap_free(void *) { }
/* CEffectorDatabase::~CEffectorDatabase() is now real too (sec 10.148,
 * see managers.cpp) -- link-satisfying only, nothing in this file
 * constructs one. */
extern "C" void rtwrap_pthread_mutexattr_settype(void *, int) { g_mutexattrCalls++; }
extern "C" void rtwrap_pthread_mutexattr_destroy(void *) { g_mutexattrCalls++; }
/* CSTGVoiceAllocator::EmergencyFreeVoiceList(void*) is now real too (sec
 * 10.149, see managers.cpp) -- link-satisfying only, nothing in this
 * file invokes it. */
extern "C" void rtwrap_pthread_mutex_lock(void *) { }
extern "C" void rtwrap_pthread_mutex_unlock(void *) { }
void CSTGVoiceAllocator::FreeVoice(CSTGVoice *) { }
void CSTGVoiceAllocator::DoPendingMoveVoices() { }

/* STGAPILR2IndivToPhysBusId's own real content is now homed directly in
 * managers.cpp (sec 10.132), linked into this binary directly. */
float gAllPlusHeadroom[4]  = { -99.0f, -99.0f, -99.0f, -99.0f };
float gAllMinusHeadroom[4] = {  99.0f,  99.0f,  99.0f,  99.0f };

/* ---- mocks specific to this test's own targets ---- */
static int g_lfoQuadCalls, g_stepSeqQuadCalls;
void CSTGLFOBase::InitializeQuad(STGLFOSubRateParams *) { g_lfoQuadCalls++; }
void CSTGStepSeqBase::InitializeQuad(STGStepSeqSubRateParams *) { g_stepSeqQuadCalls++; }
static int g_pushMsgCalls;
extern "C" void PushUnsolicitedMessage(void *) { g_pushMsgCalls++; }
/* CEmergencyStealer::~CEmergencyStealer() is now real (sec 10.148, see
 * managers.cpp) -- test [2] below now checks its real side effect
 * (CEmergencyStealer::sInstance cleared) directly, via CLoadBalancer's
 * own embedded `emergencyStealer` member. */
unsigned char *STGAPIFrontPanelStatus::sInstance;
static unsigned int g_onlineCpus = 2, g_khz = 1500000;
extern "C" unsigned int stg_num_online_cpus(void) { return g_onlineCpus; }
extern "C" unsigned int stg_get_cpu_khz(void) { return g_khz; }

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

int main(void)
{
	printf("engine_startup_bits2 known-answer test\n");
	printf("=========================================================\n");

	/* Give CSTGBankMemory a real backing pool for CSTGCommonLFO/
	 * CSTGCommonStepSeq::Initialize()'s own AllocAligned() calls. */
	static unsigned char bankBuf[0x10000];
	CSTGBankMemory::Initialize(bankBuf, sizeof(bankBuf));

	/* Real CSTGCPUInfo(0): clamps cpuCount to 4, computes field8/fieldC
	 * from stg_get_cpu_khz()/stg_num_online_cpus() -- not directly
	 * exercised by this test beyond providing CLoadBalancer::
	 * Initialize() a real, non-garbage `fieldC`/`field8`. */
	unsigned char cpuBuf[64];
	memset(cpuBuf, 0, sizeof(cpuBuf));
	CSTGCPUInfo *cpu = new (cpuBuf) CSTGCPUInfo(2);
	CSTGCPUInfo::sInstance = cpu;

	unsigned char statusBuf[0x30000];
	memset(statusBuf, 0, sizeof(statusBuf));
	STGAPIFrontPanelStatus::sInstance = statusBuf;

	/* CSTGAudioBusManager: constructed manually (raw buffer + direct
	 * field write) rather than via its own real constructor, since that
	 * constructor's own confirmed side effects (resetting
	 * gAllPlusHeadroom/gAllMinusHeadroom) aren't relevant to this
	 * test -- only busGainScale (+0x4, the field CLoadBalancer/
	 * CPowerOffTimer::Initialize() both read) matters here. */
	unsigned char busBuf[16];
	memset(busBuf, 0, sizeof(busBuf));
	CSTGAudioBusManager::sInstance = (CSTGAudioBusManager *)busBuf;
	CSTGAudioBusManager::sInstance->busGainScale = 48000.0f;

	printf("[1] CLoadBalancer::Initialize()\n");
	unsigned char lbBuf[256];
	memset(lbBuf, 0xcc, sizeof(lbBuf));
	CLoadBalancer *lb = new (lbBuf) CLoadBalancer();
	check_eq("sInstance == this", (long)(CLoadBalancer::sInstance == lb), 1);
	check_eq("embedded CEmergencyStealer::sInstance == &lb->emergencyStealer (real ctor, sec 10.148)",
		 (long)(CEmergencyStealer::sInstance == &lb->emergencyStealer), 1);
	lb->Initialize();
	check_eq("status[0x1091] == cpuInfo->cpuCount",
		 statusBuf[0x1091], (long)cpu->cpuCount);
	check_eq("status+0x10b0 raw literal", *(unsigned int *)(statusBuf + 0x10b0), 0x59682f00u);
	check_eq("status+0x10b4 == cpuInfo->fieldC", *(unsigned int *)(statusBuf + 0x10b4), (long)cpu->fieldC);
	check_eq("+0x88 == 1552000", *(unsigned int *)(lbBuf + 0x88), 1552000);
	check_eq("+0x8c == 1000000", *(unsigned int *)(lbBuf + 0x8c), 1000000);
	check_eq("+0x90 zeroed", *(unsigned int *)(lbBuf + 0x90), 0);
	check_eq("+0x94 zeroed", *(unsigned int *)(lbBuf + 0x94), 0);
	check_eq("status+0x10ac zeroed", *(unsigned int *)(statusBuf + 0x10ac), 0);

	printf("\n[2] CLoadBalancer::~CLoadBalancer()\n");
	lb->~CLoadBalancer();
	check_eq("sInstance cleared", (long)(CLoadBalancer::sInstance == 0), 1);
	check_eq("embedded CEmergencyStealer::sInstance also cleared (real dtor, sec 10.148)",
		 (long)(CEmergencyStealer::sInstance == 0), 1);

	printf("\n[3] CPowerOffTimer::Initialize() -- panel type 1 (in-table, small value: <=1800s)\n");
	statusBuf[0x5] = 1;	/* table[1] = 3600 -- wait, use type 0 for a <=1800 case */
	statusBuf[0x5] = 0;	/* table[0] = 1200, <= 1800 -> shared 120s-lead path */
	unsigned char potBuf[64];
	memset(potBuf, 0xcc, sizeof(potBuf));
	CPowerOffTimer *pot = new (potBuf) CPowerOffTimer();
	check_eq("sInstance == this", (long)(CPowerOffTimer::sInstance == pot), 1);
	pot->Initialize();
	check_eq("+0x14 (state) == 1", *(unsigned int *)(potBuf + 0x14), 1);
	check_eq("+0x8 (threshold) == 1200*48000", *(unsigned int *)(potBuf + 0x8),
		 (long)(unsigned int)(1200.0f * 48000.0f));
	check_eq("+0xc (lead) == 120*48000", *(unsigned int *)(potBuf + 0xc),
		 (long)(unsigned int)(120.0f * 48000.0f));

	printf("\n[4] CPowerOffTimer::Initialize() -- panel type 22 (unconditional disable)\n");
	statusBuf[0x5] = 22;
	CPowerOffTimer *pot2 = new (potBuf) CPowerOffTimer();
	pot2->Initialize();
	check_eq("+0x8 == -1 (disabled)", *(unsigned int *)(potBuf + 0x8), 0xffffffffu);
	check_eq("+0x14 == 0 (disabled state)", *(unsigned int *)(potBuf + 0x14), 0);
	check_eq("+0x4 == -1", *(unsigned int *)(potBuf + 0x4), 0xffffffffu);

	printf("\n[5] CPowerOffTimer::Initialize() -- panel type 1 (in-table, > 3600s)\n");
	statusBuf[0x5] = 2;	/* table[2] = 14400, > 3600 -> 300s lead */
	CPowerOffTimer *pot3 = new (potBuf) CPowerOffTimer();
	pot3->Initialize();
	check_eq("+0x14 == 1", *(unsigned int *)(potBuf + 0x14), 1);
	check_eq("+0xc (lead) == 300*48000", *(unsigned int *)(potBuf + 0xc),
		 (long)(unsigned int)(300.0f * 48000.0f));

	printf("\n[6] CSTGDiskCostManager::Initialize()\n");
	unsigned char dcmBuf[128];
	memset(dcmBuf, 0xcc, sizeof(dcmBuf));
	CSTGDiskCostManager *dcm = new (dcmBuf) CSTGDiskCostManager();
	check_eq("sInstance == this", (long)(CSTGDiskCostManager::sInstance == dcm), 1);
	dcm->Initialize();
	check_eq("+0x0 == 0x190", *(unsigned int *)(dcmBuf + 0x0), 0x190);
	check_eq("+0x4 == 0x190", *(unsigned int *)(dcmBuf + 0x4), 0x190);
	check_eq("+0x14 == 0x190000", *(unsigned int *)(dcmBuf + 0x14), 0x190000);
	check_eq("+0x18 == 0x20000", *(unsigned int *)(dcmBuf + 0x18), 0x20000);
	check_eq("+0x44 raw literal (0.03125f)", *(unsigned int *)(dcmBuf + 0x44), 0x3d000000u);
	check_eq("status+0x10f0 == 0x20000", *(unsigned int *)(statusBuf + 0x10f0), 0x20000);
	check_eq("status+0x1100 == +0x8 (0)", *(unsigned int *)(statusBuf + 0x1100), 0);

	printf("\n[7] CSTGCommonLFO::Initialize()\n");
	g_lfoQuadCalls = 0;
	CSTGCommonLFO::Initialize();
	check_eq("sSubRateParams allocated", (long)(CSTGCommonLFO::sSubRateParams != 0), 1);
	check_eq("InitializeQuad called 32 times (0x4a00/0x250)", g_lfoQuadCalls, 32);

	printf("\n[8] CSTGCommonStepSeq::Initialize()\n");
	g_stepSeqQuadCalls = 0;
	CSTGCommonStepSeq::Initialize();
	check_eq("sSubRateParams allocated", (long)(CSTGCommonStepSeq::sSubRateParams != 0), 1);
	check_eq("InitializeQuad called 32 times (0x2000/0x100)", g_stepSeqQuadCalls, 32);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
