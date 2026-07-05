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

/* Real ground-truth dependencies (CSTGPan::CalculateMonoPanCoeffs,
 * CBusChangeStateMachine::StartBusChange, CSTGBusInfo::
 * GetSignalSelectionForBusType) are confirmed-real, deliberately
 * deferred externs (their own bodies not reconstructed anywhere in this
 * project yet) -- mocked here with call-tracking so the four setters'
 * OWN logic (table lookups, offset arithmetic, raw vtable dispatch,
 * branch selection) can be verified directly. */
static int g_calcPanCalls;
static float g_lastPanScale, g_lastPanValue;
void CSTGPan::CalculateMonoPanCoeffs(STGMonoPanCoeffs &out, float scale, float pan)
{
	g_calcPanCalls++;
	g_lastPanScale = scale;
	g_lastPanValue = pan;
	out.coeff0 = scale * 100.0f;
	out.coeff4 = pan * 100.0f;
}

static int g_startBusChangeCalls;
static void *g_lastStartBusChangeThis;
static int g_lastBusId, g_lastBusType;
static unsigned int g_lastArg3;
void CBusChangeStateMachine::StartBusChange(int busId, int busType, unsigned int arg3)
{
	g_startBusChangeCalls++;
	g_lastStartBusChangeThis = this;
	g_lastBusId = busId;
	g_lastBusType = busType;
	g_lastArg3 = arg3;
}

static int g_signalSelectionReturn;
static int g_getSignalSelectionCalls;
static int g_lastSignalSelectionBusType;
int CSTGBusInfo::GetSignalSelectionForBusType(int busType)
{
	g_getSignalSelectionCalls++;
	g_lastSignalSelectionBusType = busType;
	return g_signalSelectionReturn;
}

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

	printf("[1] SetPan -- calls CalculateMonoPanCoeffs(out, 1.0f, value), "
	       "stores result at mixerStateArray[idx*0x90+0x0/+0x4]\n");
	{
		g_calcPanCalls = 0;
		mixer->SetPan(2, 0.5f);
		check_eq("CalculateMonoPanCoeffs called once", (unsigned int)g_calcPanCalls, 1);
		check_eq("scale arg == 1.0f", (unsigned int)g_lastPanScale, 1);
		check_eq("pan arg == value (0.5f)", (unsigned int)(g_lastPanValue == 0.5f), 1);
		float *entry = (float *)(mixerStateArray + 2 * 0x90);
		check_eq("coeff0 stored at +0x0", (unsigned int)(entry[0] == 100.0f), 1);
		check_eq("coeff4 stored at +0x4", (unsigned int)(entry[1] == 50.0f), 1);
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

	printf("[3] SetOutputBus -- calls StartBusChange() as a genuine member "
	       "method on busChangeArray[idx*0x10]\n");
	{
		g_startBusChangeCalls = 0;
		mixer->SetOutputBus(3, 0); /* STGAPIOutToPhysBusId[0]==48, STGAPIOutToBusType[0]==0 */
		check_eq("StartBusChange called once", (unsigned int)g_startBusChangeCalls, 1);
		check_eq("this == busChangeArray + 3*0x10",
			 g_lastStartBusChangeThis == (busChangeArray + 3 * 0x10), 1);
		check_eq("busId == STGAPIOutToPhysBusId[0] (48)", (unsigned int)g_lastBusId, 48);
		check_eq("busType == STGAPIOutToBusType[0] (0)", (unsigned int)g_lastBusType, 0);
		check_eq("arg3 == confirmed real constant 0x38", g_lastArg3, 0x38);
	}

	printf("[4] SetHDRBus -- raw vtable slot 3 dispatch (result -> +0x6c), "
	       "plus GetSignalSelectionForBusType 4-way branch (-> +0x50/+0x54/+0x58)\n");
	{
		g_vtableSlot3Calls = 0;
		g_signalSelectionReturn = 1;
		mixer->SetHDRBus(0, 0); /* STGAPIHDRPhysBusIds[0]==32, STGAPIHDRBusTypes[0]==0 */
		check_eq("vtable slot 3 called once", (unsigned int)g_vtableSlot3Calls, 1);
		check_eq("table lookup value passed (32)", (unsigned int)g_lastVtableSlot3Arg, 32);
		check_eq("GetSignalSelectionForBusType arg == STGAPIHDRBusTypes[0] (0)",
			 (unsigned int)g_lastSignalSelectionBusType, 0);
		unsigned int *entry = (unsigned int *)(mixerStateArray + 0 * 0x90);
		check_eq("+0x6c == 32+0x1000 (routed result)", entry[0x6c / 4], 32 + 0x1000);
		check_eq("signalSelection==1 -> +0x50==-1", entry[0x50 / 4], (unsigned int)-1);
		check_eq("signalSelection==1 -> +0x54==0", entry[0x54 / 4], 0);
		check_eq("signalSelection==1 -> +0x58==0", entry[0x58 / 4], 0);

		g_signalSelectionReturn = 2;
		mixer->SetHDRBus(1, 1);
		unsigned int *entry1 = (unsigned int *)(mixerStateArray + 1 * 0x90);
		check_eq("signalSelection==2 -> +0x50==0", entry1[0x50 / 4], 0);
		check_eq("signalSelection==2 -> +0x54==-1", entry1[0x54 / 4], (unsigned int)-1);
		check_eq("signalSelection==2 -> +0x58==0", entry1[0x58 / 4], 0);

		g_signalSelectionReturn = 0;
		mixer->SetHDRBus(2, 2);
		unsigned int *entry2 = (unsigned int *)(mixerStateArray + 2 * 0x90);
		check_eq("signalSelection==0 -> +0x50==0", entry2[0x50 / 4], 0);
		check_eq("signalSelection==0 -> +0x54==0", entry2[0x54 / 4], 0);
		check_eq("signalSelection==0 -> +0x58==-1", entry2[0x58 / 4], (unsigned int)-1);

		g_signalSelectionReturn = 99; /* any other value */
		mixer->SetHDRBus(3, 3);
		unsigned int *entry3 = (unsigned int *)(mixerStateArray + 3 * 0x90);
		check_eq("signalSelection==other -> +0x50==0", entry3[0x50 / 4], 0);
		check_eq("signalSelection==other -> +0x54==0", entry3[0x54 / 4], 0);
		check_eq("signalSelection==other -> +0x58==0", entry3[0x58 / 4], 0);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
