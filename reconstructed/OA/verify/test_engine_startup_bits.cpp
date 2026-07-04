// SPDX-License-Identifier: GPL-2.0
/*
 * test_engine_startup_bits.cpp  -  host-side known-answer test for
 * ../src/engine/engine_startup_bits.cpp's five reconstructed methods.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_setup_global_resources.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-60s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}
static void check_float_eq(const char *label, float got, float want)
{
	if (got == want) {
		printf("  ok    %-60s %g\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=%g want=%g\n", label, got, want);
	g_fail++;
}

static unsigned int g_onlineCpus = 2, g_khz = 1500000; /* 1.5GHz, an easy-to-check value */
static int g_mutexattrInitCalls, g_mutexInitCalls, g_mallocCalls;

extern "C" {
unsigned int stg_num_online_cpus(void) { return g_onlineCpus; }
unsigned int stg_get_cpu_khz(void) { return g_khz; }
void rtwrap_pthread_mutexattr_init(void *) { g_mutexattrInitCalls++; }
int  get_pthread_recursive_attr_constant(void) { return 1; }
void rtwrap_pthread_mutexattr_settype(void *, int) { }
void rtwrap_pthread_mutexattr_destroy(void *) { }
unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
void *rtwrap_malloc(unsigned int size) { g_mallocCalls++; return malloc(size); }
void rtwrap_pthread_mutex_init(void *, void *) { g_mutexInitCalls++; }
}

int main(void)
{
	printf("engine_startup_bits known-answer test\n");
	printf("=========================================================\n");

	printf("[1] CSTGFrontPanel::CSTGFrontPanel() / Initialize()\n");
	unsigned char fpBuf[0x200];
	memset(fpBuf, 0xcc, sizeof(fpBuf));
	CSTGFrontPanel *fp = new (fpBuf) CSTGFrontPanel();
	check_eq("sInstance == this", (long)(CSTGFrontPanel::sInstance == fp), 1);
	fp->Initialize();
	check_eq("+0x104/+0x105/+0x106 all zeroed",
		 (long)(fpBuf[0x104] == 0 && fpBuf[0x105] == 0 && fpBuf[0x106] == 0), 1);
	check_float_eq("+0x108 == 0.9921875 (127/128)", *(float *)(fpBuf + 0x108), 0.9921875f);
	check_float_eq("+0x10c == 0.03448275849223137", *(float *)(fpBuf + 0x10c), 0.03448275849223137f);
	check_eq("identity table: byte[0]==0", fpBuf[4 + 0], 0);
	check_eq("identity table: byte[1]==1", fpBuf[4 + 1], 1);
	check_eq("identity table: byte[127]==127", fpBuf[4 + 127], 127);
	check_eq("zeroed table: byte[0]==0", fpBuf[0x84 + 0], 0);
	check_eq("zeroed table: byte[127]==0", fpBuf[0x84 + 127], 0);

	printf("\n[2] CMeteredDebugOutput::CMeteredDebugOutput()\n");
	unsigned char mdoBuf[0xbb90];
	memset(mdoBuf, 0xcc, sizeof(mdoBuf));
	CMeteredDebugOutput *mdo = new (mdoBuf) CMeteredDebugOutput();
	check_eq("mutexattr_init called once", g_mutexattrInitCalls, 1);
	check_eq("mutex_init called once", g_mutexInitCalls, 1);
	check_eq("malloc called once (for the mutex)", g_mallocCalls, 1);
	check_eq("sInstance == this", (long)(CMeteredDebugOutput::sInstance == mdo), 1);
	check_eq("+0xbb88 zeroed", mdoBuf[0xbb88], 0);
	check_eq("+0xbb8c zeroed", *(unsigned int *)(mdoBuf + 0xbb8c), 0);
	check_eq("+0xbb84 (write cursor) points at +0x4 (the embedded buffer)",
		 *(unsigned int *)(mdoBuf + 0xbb84),
		 (unsigned int)(unsigned long)(mdoBuf + 4));

	printf("\n[3] CSTGCPUInfo::CSTGCPUInfo(0) -- no override, clamp to 4\n");
	unsigned char cpuBuf[64];
	g_onlineCpus = 8; /* exceeds the confirmed real clamp of 4 */
	g_khz = 1500000;  /* 1.5GHz -> cyclesPerTick == 1000.0 at the confirmed 1500.0 divisor */
	CSTGCPUInfo *cpu = new (cpuBuf) CSTGCPUInfo(0);
	check_eq("sInstance == this", (long)(CSTGCPUInfo::sInstance == cpu), 1);
	check_eq("cpuCount clamped to 4", cpu->cpuCount, 4);
	check_eq("khz stored", cpu->khz, 1500000);
	check_float_eq("cyclesPerTick == khz*1000/1500 == 1,000,000", cpu->field8, 1000000.0f);
	check_eq("(int)cyclesPerTick", cpu->fieldC, 1000000);
	check_float_eq("field14 == field8 (confirmed same value)", cpu->field14, cpu->field8);
	check_float_eq("field10 == field18 (confirmed same value)", cpu->field10, cpu->field18);
	check_float_eq("field10 ~= 1/cyclesPerTick", cpu->field10, 1.0f / 1000000.0f);
	check_float_eq("field1c == 1000000/khz", cpu->field1c, 1000000.0f / 1500000.0f);
	check_eq("field20 == (int)(0.05*cyclesPerTick)", cpu->field20, (int)(0.05 * 1000000.0));

	printf("\n[4] CSTGCPUInfo::CSTGCPUInfo(3) -- explicit override, no clamp\n");
	unsigned char cpuBuf2[64];
	g_onlineCpus = 2;
	g_khz = 800000;
	CSTGCPUInfo *cpu2 = new (cpuBuf2) CSTGCPUInfo(3);
	check_eq("cpuCount == override (3), not stg_num_online_cpus()", cpu2->cpuCount, 3);

	printf("\n[5] CSTGCPUInfo::Update() -- uses the passed tick rate, not 1500.0\n");
	cpu2->Update(1000.0f);
	check_float_eq("cyclesPerTick == khz*1000/1000 == khz", cpu2->field8, 800000.0f);

	printf("\n[6] CSTGSampleRateMonitor::Initialize() -- reads CSTGCPUInfo::sInstance->field14\n");
	unsigned char srmBuf[0x410];
	memset(srmBuf, 0xcc, sizeof(srmBuf));
	CSTGCPUInfo::sInstance = cpu; /* field14 == 1,000,000.0f from scenario [3] */
	CSTGSampleRateMonitor *srm = (CSTGSampleRateMonitor *)srmBuf;
	srm->Initialize();
	check_eq("+0x8 == (int)field14 << 8", *(unsigned int *)(srmBuf + 0x8),
		 (unsigned int)1000000 << 8);
	check_eq("table[0] == (int)field14", *(int *)(srmBuf + 0xc), 1000000);
	check_eq("table[255] == (int)field14", *(int *)(srmBuf + 0xc + 255 * 4), 1000000);

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
