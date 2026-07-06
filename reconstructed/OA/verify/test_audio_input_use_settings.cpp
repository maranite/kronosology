// SPDX-License-Identifier: GPL-2.0
/*
 * test_audio_input_use_settings.cpp  -  KAT for batch 18's
 * src/engine/audio_input_use_settings.cpp: CSTGAudioInput::UseSettings().
 *
 * Links the REAL src/engine/audio_input_mixer.cpp (SetPan/SetOutputBus/
 * SetFXCtrlBus/SetHDRBus, CSTGPan::CalculateMonoPanCoeffs,
 * CBusChangeStateMachine::StartBusChange, CSTGBusInfo::
 * GetSignalSelectionForBusType -- all sec 10.150/10.151) so UseSettings()
 * is exercised end-to-end against real mixer-object writes, not stubs.
 * Provides its own local mock of `ResolveActivePerformanceVarsManagerRaw()`
 * (normally defined in global.cpp, not linked here) returning a real
 * mmap32'd mixer object -- matching test_managers.cpp's own precedent of
 * a dedicated-TU-local definition of this exact shared extern, except
 * here it must return a REAL (non-null) pointer since UseSettings() has
 * NO null check at all before dereferencing it.
 *
 * The mixer-object layout (vtable ptr + mixerStateArray32 +
 * busChangeArray32, raw vtable slot-3 dispatch mock) is copied directly
 * from test_audio_input_mixer.cpp's own already-established setup.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include "oa_global.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

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

static void check_float_close(const char *label, float got, float want)
{
	float diff = got - want;
	if (diff < 0)
		diff = -diff;
	bool ok = diff < 0.0001f;
	if (!ok)
		g_fail++;
	printf("  %s  %-70s %f\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %f)\n", want);
}

unsigned char CSTGPerformanceVarsManager::sInstance[12];

/* Storage for the CSTGGlobal singleton pointer OnPerformanceDeactivate()
 * reads its `+0x680`/`+0x67f` fields through (batch 20). A plain buffer is
 * fine -- OnPerformanceDeactivate never stores this object's address into
 * a 32-bit field, so no MAP_32BIT is needed here. */
CSTGGlobal *CSTGGlobal::sInstance;

/* Raw vtable-slot-3 target SetFXCtrlBus/SetHDRBus both dispatch through
 * (matching the project's established raw-vtable-dispatch convention,
 * sec 10.149/test_audio_input_mixer.cpp precedent). */
static int g_vtableSlot3Calls;
static int VtableSlot3Target(void *self, int arg)
{
	(void)self;
	g_vtableSlot3Calls++;
	return arg + 0x1000;
}

static unsigned char *g_mixerObj;
unsigned char *ResolveActivePerformanceVarsManagerRaw()
{
	return g_mixerObj;
}

int main(void)
{
	printf("CSTGAudioInput::UseSettings() known-answer test\n");
	printf("================================================\n");

	unsigned char *mixerObj = (unsigned char *)mmap32(0x10);
	void **vtable = (void **)mmap32(4 * sizeof(void *));
	vtable[3] = (void *)VtableSlot3Target;
	*(void ***)mixerObj = vtable;

	unsigned char *mixerStateArray = (unsigned char *)mmap32(6 * 0x90);
	unsigned char *busChangeArray = (unsigned char *)mmap32(6 * 0x10);
	memset(mixerStateArray, 0xCC, 6 * 0x90);
	memset(busChangeArray, 0, 6 * 0x10);
	*(unsigned int *)(mixerObj + 0x8) = (unsigned int)(unsigned long)mixerStateArray;
	*(unsigned int *)(mixerObj + 0xc) = (unsigned int)(unsigned long)busChangeArray;
	g_mixerObj = mixerObj;

	unsigned char *aiStorage = (unsigned char *)mmap32(0x80);
	CSTGAudioInput *ai = (CSTGAudioInput *)aiStorage;

	printf("[1] Level/Send1/Send2: raw dword copies, all 6 buses\n");
	{
		memset(aiStorage, 0, 0x80);
		memset(mixerStateArray, 0xCC, 6 * 0x90);
		for (unsigned int i = 0; i < 6; i++) {
			*(int *)(aiStorage + 0x4 + i * 4) = (int)(100 + i);
			*(int *)(aiStorage + 0x34 + i * 4) = (int)(200 + i);
			*(int *)(aiStorage + 0x4c + i * 4) = (int)(300 + i);
		}
		aiStorage[0x76] = 0x2b; /* bits 0,1,3,5 set -- mute-enable mask */
		for (unsigned int i = 0; i < 6; i++)
			*(float *)(aiStorage + 0x1c + i * 4) = 1.0f; /* pan raw==1.0 -> diff<=0 -> pan=0 */

		ai->UseSettings();

		for (unsigned int i = 0; i < 6; i++) {
			unsigned char *region = mixerStateArray + i * 0x90;
			char label[96];
			snprintf(label, sizeof(label), "bus %u: Level region+0x78", i);
			check_eq(label, *(unsigned int *)(region + 0x78), 100 + i);
			snprintf(label, sizeof(label), "bus %u: Send1 region+0x7c", i);
			check_eq(label, *(unsigned int *)(region + 0x7c), 200 + i);
			snprintf(label, sizeof(label), "bus %u: Send2 region+0x80", i);
			check_eq(label, *(unsigned int *)(region + 0x80), 300 + i);
			snprintf(label, sizeof(label), "bus %u: mute-enable bit region+0x84", i);
			check_eq(label, region[0x84], (0x2b >> i) & 1);
		}
		check_eq("+0x77 bit1 set after UseSettings()", aiStorage[0x77] & 0x2, 0x2);
	}

	printf("\n[2] Pan formula: raw<=1.0f -> 0.0f; raw>1.0f -> (raw-1.0f)/126.0f\n");
	{
		memset(aiStorage, 0, 0x80);
		memset(mixerStateArray, 0xCC, 6 * 0x90);
		*(float *)(aiStorage + 0x1c + 0 * 4) = 1.0f;   /* diff=0 -> pan=0 */
		*(float *)(aiStorage + 0x1c + 1 * 4) = 0.5f;   /* diff<0 -> pan=0 */
		*(float *)(aiStorage + 0x1c + 2 * 4) = 127.0f;  /* diff=126 -> pan=1.0 */
		*(float *)(aiStorage + 0x1c + 3 * 4) = 64.0f;   /* diff=63 -> pan=0.5 */
		*(float *)(aiStorage + 0x1c + 4 * 4) = 1.0f;
		*(float *)(aiStorage + 0x1c + 5 * 4) = 1.0f;

		ai->UseSettings();

		/* SetPan(idx, pan) stores CalculateMonoPanCoeffs(1.0f, pan) at region+0x0/+0x4;
		 * cross-check bus 2 (pan==1.0, hard-right) and bus 3 (pan==0.5, equal-power center)
		 * against a direct call to the real function, same style as test_audio_input_mixer.cpp. */
		STGMonoPanCoeffs ref2, ref3, ref0;
		CSTGPan::CalculateMonoPanCoeffs(ref2, 1.0f, 1.0f);
		CSTGPan::CalculateMonoPanCoeffs(ref3, 1.0f, 0.5f);
		CSTGPan::CalculateMonoPanCoeffs(ref0, 1.0f, 0.0f);
		check_float_close("bus0 (raw==1.0 -> pan=0.0) coeff0 matches direct call",
				   *(float *)(mixerStateArray + 0 * 0x90 + 0x0), ref0.coeff0);
		check_float_close("bus1 (raw==0.5 -> pan clamped to 0.0) coeff0 matches direct call",
				   *(float *)(mixerStateArray + 1 * 0x90 + 0x0), ref0.coeff0);
		check_float_close("bus2 (raw==127 -> pan=1.0, hard right) coeff0 matches direct call",
				   *(float *)(mixerStateArray + 2 * 0x90 + 0x0), ref2.coeff0);
		check_float_close("bus3 (raw==64 -> pan=0.5, equal power center) coeff0 matches direct call",
				   *(float *)(mixerStateArray + 3 * 0x90 + 0x0), ref3.coeff0);
		check_float_close("bus3 coeff4 matches direct call too",
				   *(float *)(mixerStateArray + 3 * 0x90 + 0x4), ref3.coeff4);
	}

	printf("\n[3] BusSelect/FXCtrlBus/HDRBus dispatched per-bus with correct signed values\n");
	{
		memset(aiStorage, 0, 0x80);
		memset(mixerStateArray, 0xCC, 6 * 0x90);
		memset(busChangeArray, 0, 6 * 0x10);
		g_vtableSlot3Calls = 0;
		CSTGPerformanceVarsManager::sInstance[8] = 0;

		for (unsigned int i = 0; i < 6; i++) {
			aiStorage[0x64 + i] = (unsigned char)i;      /* output bus id (0..5, valid eSTGAPIBusIDOut range) */
			aiStorage[0x6a + i] = (unsigned char)(i % 3); /* FX ctrl bus (0..2) */
			aiStorage[0x70 + i] = (unsigned char)(i % 7); /* HDR bus (0..6) */
			*(float *)(aiStorage + 0x1c + i * 4) = 1.0f;
		}

		ai->UseSettings();

		/* STGAPIOutToPhysBusId[0..5], copied from audio_input_mixer.cpp's
		 * own confirmed-real .rodata table (sec 10.150) -- that array has
		 * internal (static) linkage there, so reproduced here as literals
		 * rather than referenced directly. */
		static const unsigned char kExpectedPhysBusId[6] = { 48, 54, 56, 58, 60, 62 };
		for (unsigned int i = 0; i < 6; i++) {
			unsigned char *bcsm = busChangeArray + i * 0x10;
			char label[96];
			snprintf(label, sizeof(label), "bus %u: StartBusChange latched busId", i);
			check_eq(label, bcsm[0xa], kExpectedPhysBusId[i]);
		}
		/* SetFXCtrlBus + SetHDRBus each dispatch the raw vtable slot 3 once per bus -> 12 total. */
		check_eq("vtable slot 3 dispatched 12 times (6 FXCtrl + 6 HDR)",
			 (unsigned int)g_vtableSlot3Calls, 12);
	}

	printf("\n[4] OnPerformanceDeactivate(): clears bit1 of global +0x67f or this +0x77\n");
	{
		unsigned char *gStorage = (unsigned char *)malloc(0x700);
		CSTGGlobal::sInstance = (CSTGGlobal *)gStorage;

		/* Case A: global-settings-enabled (+0x680 != 0) -> clear the
		 * GLOBAL bit at +0x67f, leave this->+0x77 untouched. Poison
		 * bit1 to 1 on both first so a clear is actually observable and
		 * an accidental no-op/wrong-target would be caught. */
		memset(gStorage, 0, 0x700);
		memset(aiStorage, 0, 0x80);
		gStorage[0x680] = 1;
		gStorage[0x67f] = 0xff;   /* bit1 set -> must be cleared */
		aiStorage[0x77] = 0xff;   /* must be left untouched in case A */
		ai->OnPerformanceDeactivate();
		check_eq("A: +0x680!=0 -> global +0x67f bit1 cleared", gStorage[0x67f], 0xfd);
		check_eq("A: this +0x77 left untouched", aiStorage[0x77], 0xff);

		/* Case B: global settings OFF (+0x680==0) but this input already
		 * flagged active (this->+0x77 bit0 set) -> still clear the
		 * GLOBAL bit, leave this->+0x77 untouched. */
		memset(gStorage, 0, 0x700);
		memset(aiStorage, 0, 0x80);
		gStorage[0x680] = 0;
		gStorage[0x67f] = 0xff;
		aiStorage[0x77] = 0x01;   /* bit0 set -> first-operand-false, second-true */
		ai->OnPerformanceDeactivate();
		check_eq("B: (+0x77&1) -> global +0x67f bit1 cleared", gStorage[0x67f], 0xfd);
		check_eq("B: this +0x77 left untouched (still 0x01)", aiStorage[0x77], 0x01);

		/* Case C: global settings OFF and this input NOT flagged active
		 * -> clear THIS input's own +0x77 bit1, leave the global bit
		 * untouched. */
		memset(gStorage, 0, 0x700);
		memset(aiStorage, 0, 0x80);
		gStorage[0x680] = 0;
		gStorage[0x67f] = 0xff;   /* must be left untouched in case C */
		aiStorage[0x77] = 0xfe;   /* bit0 clear, bit1 set -> must be cleared */
		ai->OnPerformanceDeactivate();
		check_eq("C: else -> this +0x77 bit1 cleared", aiStorage[0x77], 0xfc);
		check_eq("C: global +0x67f left untouched", gStorage[0x67f], 0xff);

		free(gStorage);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
