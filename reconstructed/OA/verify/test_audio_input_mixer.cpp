// SPDX-License-Identifier: GPL-2.0
/*
 * test_audio_input_mixer.cpp  -  host-side known-answer test for
 * CSTGAudioInputMixerBase's four setters (sec 10.150): SetPan,
 * SetFXCtrlBus, SetOutputBus, SetHDRBus.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine.h"

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

/* CSTGPan::CalculateMonoPanCoeffs, CBusChangeStateMachine::
 * StartBusChange, and CSTGBusInfo::GetSignalSelectionForBusType are all
 * real now (sec 10.151) -- no mocks here any more (multiple definition
 * otherwise); their own logic is exercised directly through the four
 * setters below. CSTGPerformanceVarsManager::sInstance's storage is
 * normally defined in bar2_stubs.cpp, NOT linked into this test binary
 * -- provide it here, matching this project's established per-test-file
 * storage precedent (e.g. test_engine.cpp/test_global.cpp's own copies
 * of the same static). */
unsigned char CSTGPerformanceVarsManager::sInstance[12];

/* Needed now that CSTGAudioInputMixerBase::Initialize() (batch 22) is
 * real and references it directly -- not linked from
 * audio_bus_manager.cpp in this test binary, own local storage. */
unsigned char CSTGAudioBusManager::sGlobalBusSet[34 * 0x80];

/* Raw vtable-slot-3 target SetFXCtrlBus/SetHDRBus both dispatch through
 * (matching the project's established raw-vtable-dispatch convention,
 * sec 10.149) -- a simple mock that echoes its own int argument back,
 * offset by a marker, so call sites can be distinguished. */
static int g_vtableSlot3Calls;
static void *g_lastVtableSlot3This;
static int g_lastVtableSlot3Arg;
static int VtableSlot3Target(void *self, int arg)
{
	g_vtableSlot3Calls++;
	g_lastVtableSlot3This = self;
	g_lastVtableSlot3Arg = arg;
	return arg + 0x1000;
}

int main(void)
{
	printf("CSTGAudioInputMixerBase known-answer test\n");
	printf("=========================================================\n");

	/* Layout: +0x0 vtable ptr (8 bytes here -- see note below), +0x8
	 * mixerStateArray32 (packed 32-bit, matches the real target's own
	 * 4-byte field), +0xc busChangeArray32 (same).
	 *
	 * The vtable pointer at +0x0 is the ONE deliberate exception to
	 * this project's usual packed-32-bit-field convention: dispatching
	 * through it on this 64-bit host requires a genuine host-native
	 * (8-byte) function-pointer table, not a packed 32-bit address --
	 * the same test-only concession this project's raw-vtable-dispatch
	 * KATs already make elsewhere (e.g. test_audio_start.cpp's own fake
	 * vtables). This does NOT affect the real 32-bit target build,
	 * where `void*` really is 4 bytes and `*(void***)this` naturally
	 * reads exactly that -- it only affects how THIS TEST's own object
	 * layout has to be built. */
	unsigned char *mixerObj = (unsigned char *)mmap32(0x10);
	void **vtable = (void **)mmap32(4 * sizeof(void *));
	vtable[3] = (void *)VtableSlot3Target;
	*(void ***)mixerObj = vtable;

	/* mixerStateArray/busChangeArray are allocated via mmap32 (MAP_32BIT),
	 * so their own addresses always fit losslessly in the packed
	 * 32-bit fields the real struct actually declares. */
	unsigned char *mixerStateArray = (unsigned char *)mmap32(4 * 0x90);
	unsigned char *busChangeArray = (unsigned char *)mmap32(4 * 0x10);
	memset(mixerStateArray, 0xCC, 4 * 0x90);
	memset(busChangeArray, 0xCC, 4 * 0x10);
	*(unsigned int *)(mixerObj + 0x8) = (unsigned int)(unsigned long)mixerStateArray;
	*(unsigned int *)(mixerObj + 0xc) = (unsigned int)(unsigned long)busChangeArray;

	CSTGAudioInputMixerBase *mixer = (CSTGAudioInputMixerBase *)mixerObj;

	printf("[1] SetPan -- calls the real CalculateMonoPanCoeffs(out, 1.0f, value), "
	       "stores result at mixerStateArray[idx*0x90+0x0/+0x4]\n");
	{
		mixer->SetPan(2, 0.5f);
		STGMonoPanCoeffs ref;
		CSTGPan::CalculateMonoPanCoeffs(ref, 1.0f, 0.5f);
		float *entry = (float *)(mixerStateArray + 2 * 0x90);
		check_eq("coeff0 stored at +0x0 matches a direct call to the real function",
			 (unsigned int)(entry[0] == ref.coeff0), 1);
		check_eq("coeff4 stored at +0x4 matches a direct call to the real function",
			 (unsigned int)(entry[1] == ref.coeff4), 1);
		/* Center-pan (0.5) equal-power sanity check: both channels reduce
		 * to scale*sqrt(2)/2 (~0.70710678f) and are equal to each other --
		 * confirms this is a real continuous pan law, not an arbitrary
		 * curve (see CSTGPan::CalculateMonoPanCoeffs's own header comment
		 * in oa_global.h/audio_input_mixer.cpp). */
		check_eq("center pan (0.5) ~= sqrt(2)/2 (equal power)",
			 (unsigned int)(entry[0] > 0.7071f && entry[0] < 0.7072f), 1);
		check_eq("coeff0 == coeff4 at center pan (symmetry)",
			 (unsigned int)(entry[0] == entry[1]), 1);
	}

	printf("[2] SetFXCtrlBus -- raw vtable slot 3 dispatch with "
	       "STGAPIFXCtrlToWritePhysBusId[value], result stored at +0x68\n");
	{
		g_vtableSlot3Calls = 0;
		mixer->SetFXCtrlBus(1, 1); /* STGAPIFXCtrlToWritePhysBusId[1] == 78 */
		check_eq("vtable slot 3 called once", (unsigned int)g_vtableSlot3Calls, 1);
		check_eq("this passed through unchanged", g_lastVtableSlot3This == mixerObj, 1);
		check_eq("table lookup value passed (78)", (unsigned int)g_lastVtableSlot3Arg, 78);
		unsigned int result = *(unsigned int *)(mixerStateArray + 1 * 0x90 + 0x68);
		check_eq("result (78+0x1000) stored at +0x68", result, 78 + 0x1000);
	}

	printf("[3] SetOutputBus -- calls the real StartBusChange() as a genuine "
	       "member method on busChangeArray[idx*0x10], confirmed 3-way "
	       "epoch/early-out logic (sec 10.151)\n");
	{
		unsigned char *bcsm = busChangeArray + 3 * 0x10;
		memset(bcsm, 0, 0x10);
		CSTGPerformanceVarsManager::sInstance[8] = 0;

		mixer->SetOutputBus(3, 0); /* STGAPIOutToPhysBusId[0]==48, STGAPIOutToBusType[0]==0 */
		check_eq("first call: busId latched (+0xa==48)", bcsm[0xa], 48);
		check_eq("first call: busType latched (+0xb==0)", bcsm[0xb], 0);
		check_eq("first call: perf-vars epoch latched (+0xc==0)",
			 *(unsigned int *)(bcsm + 0xc), 0);
		check_eq("first call: +0x0 flag set (first-time init)",
			 *(unsigned int *)(bcsm + 0x0), 1);
		check_eq("first call: +0x4 == confirmed real constant 0x38, plus one",
			 *(unsigned int *)(bcsm + 0x4), 0x39);

		/* Same busId/busType/epoch -> confirmed real early-out: nothing
		 * touched at all, not even a re-latch. */
		*(unsigned int *)(bcsm + 0x4) = 0xDEADBEEF; /* poison, to prove untouched */
		mixer->SetOutputBus(3, 0);
		check_eq("unchanged busId/busType/epoch -> +0x4 left untouched (early-out)",
			 *(unsigned int *)(bcsm + 0x4), 0xDEADBEEF);

		/* Toggle the perf-vars slot-selector epoch -> re-latches +0xc,
		 * but +0x0 is already non-zero, so +0x4's own "set once" value
		 * stays untouched even though a re-latch happened. */
		CSTGPerformanceVarsManager::sInstance[8] = 1;
		mixer->SetOutputBus(3, 0);
		check_eq("epoch changed -> +0xc re-latched to the new epoch (1)",
			 *(unsigned int *)(bcsm + 0xc), 1);
		check_eq("epoch changed -> +0x4 still untouched (set-once semantics)",
			 *(unsigned int *)(bcsm + 0x4), 0xDEADBEEF);

		CSTGPerformanceVarsManager::sInstance[8] = 0; /* restore for later scenarios */
	}

	printf("[4] SetHDRBus -- raw vtable slot 3 dispatch (result -> +0x6c), "
	       "plus the real GetSignalSelectionForBusType's 3-way branch "
	       "(-> +0x50/+0x54/+0x58)\n");
	{
		g_vtableSlot3Calls = 0;
		mixer->SetHDRBus(0, 0); /* STGAPIHDRBusTypes[0]==0 -> selection 0 */
		check_eq("vtable slot 3 called once", (unsigned int)g_vtableSlot3Calls, 1);
		check_eq("table lookup value passed (32)", (unsigned int)g_lastVtableSlot3Arg, 32);
		unsigned int *entry = (unsigned int *)(mixerStateArray + 0 * 0x90);
		check_eq("+0x6c == 32+0x1000 (routed result)", entry[0x6c / 4], 32 + 0x1000);
		check_eq("selection 0 (busType 0, default) -> +0x50==0", entry[0x50 / 4], 0);
		check_eq("selection 0 (busType 0, default) -> +0x54==0", entry[0x54 / 4], 0);
		check_eq("selection 0 (busType 0, default) -> +0x58==-1", entry[0x58 / 4], (unsigned int)-1);

		mixer->SetHDRBus(1, 1); /* STGAPIHDRBusTypes[1]==3 -> selection 1 */
		unsigned int *entry1 = (unsigned int *)(mixerStateArray + 1 * 0x90);
		check_eq("selection 1 (busType 3) -> +0x50==-1", entry1[0x50 / 4], (unsigned int)-1);
		check_eq("selection 1 (busType 3) -> +0x54==0", entry1[0x54 / 4], 0);
		check_eq("selection 1 (busType 3) -> +0x58==0", entry1[0x58 / 4], 0);

		mixer->SetHDRBus(2, 2); /* STGAPIHDRBusTypes[2]==4 -> selection 2 */
		unsigned int *entry2 = (unsigned int *)(mixerStateArray + 2 * 0x90);
		check_eq("selection 2 (busType 4) -> +0x50==0", entry2[0x50 / 4], 0);
		check_eq("selection 2 (busType 4) -> +0x54==-1", entry2[0x54 / 4], (unsigned int)-1);
		check_eq("selection 2 (busType 4) -> +0x58==0", entry2[0x58 / 4], 0);

		/* SetHDRBus's own 4th ("else") branch is confirmed DEAD CODE with
		 * the real GetSignalSelectionForBusType wired in: that function
		 * can only ever return 0, 1, or 2 (its own confirmed 2-entry
		 * table plus a 0 default), so the "anything else" branch can
		 * never be reached in practice -- not tested here for exactly
		 * that reason (there is no real busType/value combination that
		 * reaches it). */
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
