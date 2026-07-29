// SPDX-License-Identifier: GPL-2.0
/*
 * test_audio_input_mixer.cpp  -  host-side known-answer test for
 * CSTGAudioInputMixerBase's four setters (sec 10.150): SetPan,
 * SetFXCtrlBus, SetOutputBus, SetHDRBus.
 */

#include <cstdio>
#include <cstring>
#include <new>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine.h"
#include "oa_bank_memory.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

static unsigned char *FromU32ForTest(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
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
/* Needed now that CSTGAudioInputMixer::Initialize()/CSTGMasterLRMixer::
 * Initialize() are real too (batch 58) -- same rationale as
 * sGlobalBusSet above, own local storage, not linked from
 * audio_bus_manager.cpp in this test binary. */
unsigned char CSTGAudioBusManager::sEffectThreadBusSets[240 * 0x80];
/* Needed now that CSTGAudioInputMixer::GetOutputBus() (2026-07-27) is
 * real -- same rationale as sGlobalBusSet/sEffectThreadBusSets above. */
unsigned char CSTGAudioBusManager::sSynthesisThreadBusSets[960 * 0x80];

/* CSTGControllerRTData::sInstance -- needed now that
 * CSTGAudioInputMixer::ShouldMute() (2026-07-27) is real and dereferences
 * it directly. Own local storage + a zeroed backing object (neutral
 * default: +0x22==0 and +0x24's low 3 bytes==0, taking the "read the
 * per-bus mute byte" branch rather than the bitmask-inverse branch),
 * matching this file's own established per-test-file storage precedent. */
CSTGControllerRTData *CSTGControllerRTData::sInstance;
static unsigned char g_fakeControllerRTData[0x40];

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

	memset(g_fakeControllerRTData, 0, sizeof(g_fakeControllerRTData));
	CSTGControllerRTData::sInstance = (CSTGControllerRTData *)g_fakeControllerRTData;

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

	printf("[5] CSTGAudioInputMixerBase ctor -- installs a non-null vtable ptr "
	       "(+0x0), zeroes _gap4[0]/mixerStateArray32, leaves busChangeArray32 "
	       "(+0xc) untouched (batch 58)\n");
	{
		unsigned char raw[0x10];
		memset(raw, 0xCC, sizeof(raw));
		CSTGAudioInputMixerBase *obj = new (raw) CSTGAudioInputMixerBase();
		check_eq("vtable ptr installed (non-null/non-poison)",
			 (unsigned int)(*(unsigned int *)raw != 0 &&
					*(unsigned int *)raw != 0xCCCCCCCC),
			 1);
		/*
		 * `_gap4[0]` is NOT checked for == 0 here: on the real 32-bit
		 * target it genuinely is (a separate 4-byte field, ground
		 * truth's own `movb $0x0,0x4(%eax)`) -- but on this 64-bit
		 * host, `raw[4]` is simultaneously byte 4 of the 8-byte
		 * vtable pointer the line above already confirmed is
		 * installed (there is no daylight between "the pointer's own
		 * significant bits" and "_gap4" once a real pointer needs all
		 * 8 bytes; the ctor's own header comment in
		 * audio_input_mixer.cpp covers this write-ordering tradeoff
		 * in full). Asserting `raw[4]==0` here would be asserting a
		 * property of THIS PARTICULAR ASLR slide, not of the ctor.
		 */
		check_eq("mixerStateArray32 zeroed", (unsigned int)*(unsigned int *)(raw + 8), 0);
		check_eq("busChangeArray32 (+0xc) left UNTOUCHED (still poison)",
			 (unsigned int)*(unsigned int *)(raw + 0xc), 0xCCCCCCCC);

		(void)obj;
	}

	printf("[6] CSTGAudioInputMixer::Initialize -- fixed 6-entry base "
	       "Initialize() + confirmed sGlobalBusSet[2,3,4,5,10,11] index "
	       "overwrite + SetSendBuses() tail call (batch 58)\n");
	{
		/*
		 * Deliberately does NOT go through the real ctor here (unlike
		 * [5] above): CSTGAudioInputMixerBase::Initialize()'s own
		 * confirmed-real `_gap4[0] = (unsigned char)count` write would
		 * clobber part of the vtable pointer the ctor installs, on
		 * THIS 64-bit host ONLY (both fields alias the same 8 bytes a
		 * host `void*` needs -- see the ctor's own header comment in
		 * audio_input_mixer.cpp; the real 32-bit target has no such
		 * hazard, `_gap4` there is a genuinely separate 4-byte field).
		 * Manual raw-object setup instead, matching sections [1]-[4]'s
		 * own established "vtable poked in directly, ctor never
		 * called" convention, sidesteps this test-only aliasing issue
		 * entirely.
		 */
		unsigned char *pool = (unsigned char *)mmap32(0x200000);
		CSTGBankMemory::Initialize(pool, 0x200000);

		/*
		 * CSTGAudioInputMixer::Initialize() always calls the base
		 * Initialize() with the FIXED constant 6 (ground truth, not
		 * caller-controlled -- see oa_global.h's own comment), which
		 * writes `_gap4[0] = 6`. On this 64-bit host that byte sits
		 * INSIDE the vtable pointer's own significant bits (offset+4,
		 * bits 32-39 of the full 8-byte host pointer -- there is no
		 * genuinely separate 4-byte "gap" once a real pointer needs
		 * all 8 bytes; only the real 32-bit target has one). Rather
		 * than fight that, this test places the vtable at a FIXED
		 * address whose own byte 4 is ALREADY 6 -- so the real,
		 * ground-truth-faithful `_gap4[0] = 6` write becomes a true
		 * no-op against an already-correct pointer, instead of a
		 * corruption. (`0x600000000` = `6 << 32`.)
		 */
		void **vtable6 = (void **)mmap((void *)0x610000000UL, 4 * sizeof(void *),
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		vtable6[3] = (void *)VtableSlot3Target;

		alignas(CSTGAudioInputMixer) unsigned char raw[64];
		memset(raw, 0, sizeof(raw));
		*(void ***)raw = vtable6;

		CSTGAudioInputMixer *mixer6 = (CSTGAudioInputMixer *)raw;
		mixer6->Initialize(99); /* count is stored but never read by the real body */
		check_eq("channelCountByte stores the (unused) count argument",
			 mixer6->channelCountByte, 99);

		unsigned char *mixerArr6 = FromU32ForTest(mixer6->mixerStateArray32);
		static const unsigned int kExpectIdx[6] = { 2, 3, 4, 5, 10, 11 };
		int ok = 1;
		for (int i = 0; i < 6; i++) {
			unsigned int got = *(unsigned int *)(mixerArr6 + i * 0x90 + 0x60);
			unsigned int want = (unsigned int)(unsigned long)
				(CSTGAudioBusManager::sGlobalBusSet + kExpectIdx[i] * 0x80);
			if (got != want)
				ok = 0;
		}
		check_eq("all six entries' +0x60 overwritten with sGlobalBusSet[2,3,4,5,10,11]",
			 (unsigned int)ok, 1);

		/* SetSendBuses() ran as part of Initialize(): both vtable-slot-3
		 * results (args 0x32/0x34, VtableSlot3Target echoes arg+0x1000)
		 * should be broadcast into every one of the six entries'
		 * +0x70/+0x74 fields. */
		int broadcastOk = 1;
		for (int i = 0; i < 6; i++) {
			unsigned int *e = (unsigned int *)(mixerArr6 + i * 0x90);
			if (e[0x70 / 4] != 0x32 + 0x1000 || e[0x74 / 4] != 0x34 + 0x1000)
				broadcastOk = 0;
		}
		check_eq("SetSendBuses() broadcast +0x70=0x1032/+0x74=0x1034 to all six entries",
			 (unsigned int)broadcastOk, 1);
	}

	printf("[7] CSTGMasterLRMixer::Initialize -- confirmed +0x10/+0x14 "
	       "sEffectThreadBusSets pointers (index*120+118 / index*120+12) "
	       "(batch 58)\n");
	{
		unsigned char raw[0x18];
		memset(raw, 0, sizeof(raw));
		CSTGMasterLRMixer *lr = (CSTGMasterLRMixer *)raw;

		lr->Initialize(0);
		unsigned int want10_0 = (unsigned int)(unsigned long)
			(CSTGAudioBusManager::sEffectThreadBusSets + 118 * 0x80);
		unsigned int want14_0 = (unsigned int)(unsigned long)
			(CSTGAudioBusManager::sEffectThreadBusSets + 12 * 0x80);
		check_eq("index=0: +0x10 == &sEffectThreadBusSets[118]",
			 *(unsigned int *)(raw + 0x10), want10_0);
		check_eq("index=0: +0x14 == &sEffectThreadBusSets[12]",
			 *(unsigned int *)(raw + 0x14), want14_0);

		lr->Initialize(1);
		unsigned int want10_1 = (unsigned int)(unsigned long)
			(CSTGAudioBusManager::sEffectThreadBusSets + (120 + 118) * 0x80);
		unsigned int want14_1 = (unsigned int)(unsigned long)
			(CSTGAudioBusManager::sEffectThreadBusSets + (120 + 12) * 0x80);
		check_eq("index=1: +0x10 == &sEffectThreadBusSets[120+118]",
			 *(unsigned int *)(raw + 0x10), want10_1);
		check_eq("index=1: +0x14 == &sEffectThreadBusSets[120+12]",
			 *(unsigned int *)(raw + 0x14), want14_1);
	}

	{
		printf("[round 66] CSTGAudioInputMixerBase::ShouldMute\n");
		check_eq("ShouldMute(0) -> false", (unsigned int)CSTGAudioInputMixerBase::ShouldMute(0), 0u);
		check_eq("ShouldMute(99) -> false (ignores index)",
			 (unsigned int)CSTGAudioInputMixerBase::ShouldMute(99), 0u);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
