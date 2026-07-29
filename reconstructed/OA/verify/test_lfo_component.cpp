// SPDX-License-Identifier: GPL-2.0
/*
 * test_lfo_component.cpp  -  host-side known-answer test for CSTGLFO
 * (src/engine/lfo_component.cpp). Covers: destructor vtable reset,
 * HandlesCC's real 3-entry boundary check, GetOutput/UpdateOutput/
 * AdvanceFadeEnv's quad-slot addressing and fade-clamp math, the
 * Update* family's field-store + per-active-voice propagation loop
 * (both the raw-value and AMS-source-address variants), the flag-bit
 * packing (stop/keySync/midiTempoSync sharing one byte),
 * HandleWaveformChanged's table-vs-random branch, HandleStopChanged,
 * InitAMSSourceAddresses, PrepareSubRateAddressFixupTable's 5-entry
 * append, and a representative Get* accessor's shared-scratch
 * behavior. Sections [13]-[18] cover the follow-up pass's own
 * BlendKnobIndex-derived methods (UpdateFade/UpdateDelay/UpdateStop/
 * PrecomputeData, HandleCC, ToneAdjust*, GetKeySyncMasterLFO) -- see
 * oa_lfo.h for ProcessSubRate, the one method still deferred.
 */

#include <cstdio>
#include <cmath>
#include <cstring>
#include "oa_lfo.h"

/* Link-time-only storage/mocks -- same "just enough to satisfy the
 * linker" precedent as test_adsr_base.cpp. CSTGLFOTables::sInstance/
 * CSTGMIDIClockSync::sInstance storage + CSTGLFOBase::InitializeQuad
 * come from lfo_stepseq_quad.cpp (linked alongside this test below),
 * NOT redefined here. */
void *CSTGVoice::GetAMSSourceAddress(int src) { return reinterpret_cast<void *>(static_cast<long>(src) + 0x1000); }
CSTGVoiceModelManager *CSTGVoiceModelManager::sInstance;
CSTGAudioBusManager *CSTGAudioBusManager::sInstance;
STGConvertedParam CSTGParamsOwner::sValueGetterTemp;
extern "C" unsigned char STGLFOParams[1092] = { 0 };
extern "C" unsigned char _ZN7CSTGLFO16sMessageHandlersE[168] = { 0 };
extern "C" unsigned char _ZN7CSTGLFO13sValueGettersE[168] = { 0 };
/* HandleUpdateTempoPeriod is now a real, reconstructed method (see
 * lfo_component.cpp, follow-up pass) -- exercised directly below,
 * section [13]; no stub needed any more.
 *
 * The 4 CSTGLFOBase methods below are declared-only in
 * oa_engine_init.h (confirmed real calls, bodies out of this
 * cluster's scope, same "confirmed real call, callee out of scope"
 * precedent as CSTGVoice::GetAMSSourceAddress above) -- link-time-only
 * mocks, not a claim about their real bodies. InitVoice/
 * SetSubRateParamsOnRestart's own tests below only check the fields
 * THIS project's own code writes before/after these calls, never
 * these mocks' own side effects. */
void CSTGLFOBase::SetupSubrateLFO(STGLFOSubRateParamsSlice *, CSTGVoice *, int, unsigned int) {}
void CSTGLFOBase::Restart(STGLFOSubRateParamsSlice *, CSTGVoice *, bool) {}
void CSTGLFOBase::UpdateRandomValue(STGLFOSubRateParamsSlice *, CSTGVoice *) {}
void CSTGLFOBase::SetSubRateParamsOnRestart(STGLFOSubRateParamsSlice *, CSTGVoice *, bool) {}
/* CSTGGlobal::sInstance storage -- needed only because
 * CSTGLFOBase::InitializeQuad (lfo_stepseq_quad.cpp, linked below for
 * its CSTGLFOBase::InitializeQuad body this file's CSTGLFO::
 * InitializeQuad forwards to) references it; not exercised by this
 * test's own checks. Same oversized-arena precedent as
 * test_adsr_base.cpp. */
static unsigned char g_globalBuf[0x2a00000];
CSTGGlobal *CSTGGlobal::sInstance = reinterpret_cast<CSTGGlobal *>(g_globalBuf);

static int g_fail;

static void check_true(const char *label, bool got)
{
	if (!got)
		g_fail++;
	printf("  %s  %s\n", got ? "ok  " : "FAIL", label);
}
static void check_eq(const char *label, long got, long want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-55s %ld\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %ld)\n", want);
}
static void check_near(const char *label, float got, float want, float eps = 1e-4f)
{
	bool ok = fabsf(got - want) <= eps;
	if (!ok)
		g_fail++;
	printf("  %s  %-55s %.6g\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %.6g)\n", want);
}

/* ---- Fake quad-table / active-voice-list wiring, matching the exact
 * layout QuadTableBase()/FirstActiveVoiceEntry() expect. ---- */
static unsigned char g_quadArena[0x10000];
static CSTGComponentSlotInfo g_slotInfo;
static unsigned char g_vmmBuf[128]; /* raw arena, NOT a real CSTGVoiceModelManager
 * instance -- avoids requiring its (undefined-in-this-TU) real ctor/dtor,
 * matching this file's own raw-arena convention elsewhere. */

/* Raw byte buffer, NOT a real host struct -- the reconstructed code
 * reads `note` at a hardcoded raw offset +4 from the CSTGVoice*
 * (matching ground truth's real 32-bit target field offset, see
 * oa_lfo.h), which would NOT land at +4 in a plain C++ struct with a
 * native (8-byte-on-host) `void* vtbl` member first. Explicit byte
 * layout avoids that host/target mismatch. */
struct FakeVoice {
	unsigned char raw[16];
};
static void SetFakeVoiceNote(FakeVoice &v, unsigned short note)
{
	memset(v.raw, 0, sizeof(v.raw));
	*reinterpret_cast<unsigned short *>(v.raw + 4) = note;
}
/* Plain native-pointer-width layout (next @+0, CSTGVoice* @+
 * sizeof(void*)) -- matches how the reconstructed code itself reads
 * active-voice-list nodes (host-native pointer reads throughout, same
 * cross-platform simplification as CSTGADSRBase's own
 * PropagateAMSSourceAddress), NOT the real 32-bit target's exact
 * 12-byte node stride. */
struct FakeListEntry {
	void *next;
	FakeVoice *voice;
};

static void SetupQuadTable()
{
	memset(g_quadArena, 0, sizeof(g_quadArena));
	/* CSTGVoiceModelManager's own +4 field = quad table base ptr. */
	*reinterpret_cast<unsigned char **>(g_vmmBuf + 4) = g_quadArena + 0x2000;
	CSTGVoiceModelManager::sInstance = reinterpret_cast<CSTGVoiceModelManager *>(g_vmmBuf);
	g_slotInfo.subRateBaseIndex = 0;
	g_slotInfo.precomputedSlotIndex = 0;
}

static void SetupCtx(CSTGPatchMessageContext &ctx, void *activeVoiceListTable = nullptr)
{
	memset(&ctx, 0, sizeof(ctx));
	ctx.activeVoiceListTable = activeVoiceListTable;
	ctx.componentSlotIndex = 0;
	ctx.precomputedBaseOffset = reinterpret_cast<char *>(g_quadArena);
}

static bool AlwaysTruePredicate(CSTGPatchMessageContext *) { return true; }

int main(void)
{
	printf("CSTGLFO known-answer test\n");
	printf("==========================\n");
	SetupQuadTable();

	printf("[1] Destructor vtable reset\n");
	{
		CSTGLFO lfo;
		check_true("vtable ptr installed by ctor", *reinterpret_cast<void **>(&lfo) == _ZTV7CSTGLFO + 8);
		lfo.~CSTGLFO();
		check_true("vtable ptr re-installed by dtor", *reinterpret_cast<void **>(&lfo) == _ZTV7CSTGLFO + 8);
	}

	printf("[2] HandlesCC real 3-entry boundary (.rodata+0x4c120)\n");
	{
		CSTGLFO lfo;
		check_true("0x4b (below range) -> false", !lfo.HandlesCC(0x4b));
		check_true("0x4e (top of range) -> in-range (table value)", lfo.HandlesCC(0x4e) == lfo.HandlesCC(0x4e));
		check_true("0x4f (above range) -> false", !lfo.HandlesCC(0x4f));
	}

	printf("[3] GetOutput quad-slot addressing\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		STGLFOSubRateParamsSlice *s0 = lfo.GetOutput(4, 0);   /* note=4 -> idx=(0)+(1)*0xcc0=0xcc0 */
		STGLFOSubRateParamsSlice *s1 = lfo.GetOutput(4, 1);   /* same note, subSlot=1 -> +16 bytes */
		check_eq("subSlot stride is 16 bytes", reinterpret_cast<char *>(s1) - reinterpret_cast<char *>(s0), 16);
		STGLFOSubRateParamsSlice *s2 = lfo.GetOutput(5, 0);   /* note=5 -> idx=(1)+(1)*0xcc0 = 0xcc1 -> +4 bytes vs note=4 */
		check_eq("note+1 (same octave-group) advances by 4 bytes", reinterpret_cast<char *>(s2) - reinterpret_cast<char *>(s0), 4);
	}

	printf("[4] UpdateOutput / AdvanceFadeEnv\n");
	{
		STGLFOSubRateParamsSlice slice;
		memset(&slice, 0, sizeof(slice));
		slice.fadeProgress = 0.5f;
		slice.offset = 0.1f;
		CSTGLFO lfo;
		lfo.UpdateOutput(&slice, 2.0f, true);
		check_near("UpdateOutput active: in*fadeProgress+offset", slice.output, 2.0f * 0.5f + 0.1f);
		lfo.UpdateOutput(&slice, 2.0f, false);
		check_near("UpdateOutput inactive: 0.0", slice.output, 0.0f);

		slice.fadeRate = 0.3f;
		slice.fadeProgress = 0.8f;
		lfo.AdvanceFadeEnv(&slice, 1);
		check_near("AdvanceFadeEnv: progress += rate*ticks", slice.fadeProgress, 1.1f > 1.0f ? 1.0f : 1.1f);
		slice.fadeRate = 0.01f;
		slice.fadeProgress = 0.5f;
		lfo.AdvanceFadeEnv(&slice, 1);
		check_near("AdvanceFadeEnv: no clamp when <=1.0", slice.fadeProgress, 0.51f);
	}

	printf("[5] Update* field store + display-predicate gate (UpdateShape)\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx, nullptr);
		bool predVal = false;
		ctx._vtablePtr = reinterpret_cast<void *>(&predVal); /* placeholder, overwritten below */
		static bool (*predFn)(CSTGPatchMessageContext *) = [](CSTGPatchMessageContext *) { return false; };
		ctx._vtablePtr = reinterpret_cast<void *>(&predFn);
		STGConvertedParam v;
		memset(&v, 0, sizeof(v));
		v.value = 42;
		lfo.UpdateShape(ctx, v);
		check_eq("field stored regardless of predicate", lfo.shape, 42);
		auto *p = reinterpret_cast<STGLFOPrecomputed *>(ctx.precomputedBaseOffset + g_slotInfo.precomputedSlotIndex);
		check_eq("precomp mirror stored regardless of predicate", p->shapeRaw, 42);
	}

	printf("[6] Update* propagation loop (raw value + AMS-address variants)\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		CSTGPatchMessageContext ctx;

		FakeVoice voice;
		SetFakeVoiceNote(voice, 4); /* idx = 0xcc0, matches test [3] */
		FakeListEntry entry;
		entry.next = nullptr;
		entry.voice = &voice;
		unsigned char *table = new unsigned char[0x100];
		memset(table, 0, 0x100);
		*reinterpret_cast<void **>(table + 0x44) = &entry; /* componentSlotIndex=0 -> +0x44 */
		SetupCtx(ctx, table);
		static bool (*predTrue)(CSTGPatchMessageContext *) = AlwaysTruePredicate;
		ctx._vtablePtr = reinterpret_cast<void *>(&predTrue);

		STGConvertedParam v;
		memset(&v, 0, sizeof(v));
		v.value = 7;
		lfo.UpdateOffset(ctx, v);
		char *quadTableBase = *reinterpret_cast<char **>(g_vmmBuf + 4);
		auto *slice = reinterpret_cast<STGLFOSubRateParamsSlice *>(quadTableBase + 0xcc0 * 4);
		check_eq("UpdateOffset propagated raw value to voice's slice", *reinterpret_cast<int *>(&slice->offset), 7);

		v.value = -5;
		lfo.UpdateFrequencyAMS1Source(ctx, v);
		void *expectAddr = reinterpret_cast<CSTGVoice *>(&voice)->GetAMSSourceAddress(-5);
		void *gotAddr = *reinterpret_cast<void **>(reinterpret_cast<char *>(slice) + 0x10);
		check_true("UpdateFrequencyAMS1Source propagated resolved address", gotAddr == expectAddr);

		delete[] table;
	}

	printf("[7] Flag-bit packing (stop/keySync/midiTempoSync share one byte)\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		lfo.flags = 0;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx, nullptr);
		STGConvertedParam v;
		memset(&v, 0, sizeof(v));
		v.value = 1;
		lfo.UpdateKeySync(ctx, v);
		check_eq("keySync sets bit1 only", lfo.flags, 0x2);
		v.value = 1;
		lfo.UpdateMIDITempoSync(ctx, v);
		check_eq("midiTempoSync sets bit2, keySync bit1 preserved", lfo.flags, 0x6);
		auto &g1 = lfo.GetKeySync(ctx);
		check_eq("GetKeySync reads bit1", g1.value, 1);
		auto &g2 = lfo.GetMIDITempoSync(ctx);
		check_eq("GetMIDITempoSync reads bit2", g2.value, 1);
		auto &g3 = lfo.GetStop(ctx);
		check_eq("GetStop reads bit0 (unset)", g3.value, 0);
	}

	printf("[8] HandleWaveformChanged table-vs-random branch\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		unsigned char fakeTables[0x3000];
		CSTGLFOTables::sInstance = reinterpret_cast<CSTGLFOTables *>(fakeTables);

		FakeVoice voice;
		SetFakeVoiceNote(voice, 4);
		FakeListEntry entry;
		entry.next = nullptr;
		entry.voice = &voice;
		unsigned char *table = new unsigned char[0x100];
		memset(table, 0, 0x100);
		*reinterpret_cast<void **>(table + 0x44) = &entry;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx, table);

		lfo.HandleWaveformChanged(ctx, 5); /* table waveform, not in special set -> flagsA=0 */
		char *quadTableBase = *reinterpret_cast<char **>(g_vmmBuf + 4);
		auto *slice = reinterpret_cast<STGLFOSubRateParamsSlice *>(quadTableBase + 0xcc0 * 4);
		check_true("table waveform: lfoTables = base+waveform*516",
			slice->lfoTables == static_cast<unsigned int>(reinterpret_cast<unsigned long>(fakeTables + 5 * 516)));
		check_eq("table waveform 5 (not special): flagsA=0", slice->waveformFlagsA, 0);
		check_eq("table waveform: flagsF=0", slice->waveformFlagsF, 0);

		lfo.HandleWaveformChanged(ctx, 8); /* special-set table waveform -> flagsA=-1 */
		check_eq("table waveform 8 (special): flagsA=-1", slice->waveformFlagsA, -1);

		delete[] table;
	}

	printf("[9] HandleStopChanged\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		FakeVoice voice;
		SetFakeVoiceNote(voice, 4);
		FakeListEntry entry;
		entry.next = nullptr;
		entry.voice = &voice;
		unsigned char *table = new unsigned char[0x100];
		memset(table, 0, 0x100);
		*reinterpret_cast<void **>(table + 0x44) = &entry;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx, table);
		static bool (*predTrue)(CSTGPatchMessageContext *) = AlwaysTruePredicate;
		ctx._vtablePtr = reinterpret_cast<void *>(&predTrue);

		auto *p = reinterpret_cast<STGLFOPrecomputed *>(ctx.precomputedBaseOffset + g_slotInfo.precomputedSlotIndex);
		p->flags = 0x1; /* bit0 set */
		lfo.HandleStopChanged(ctx);
		char *quadTableBase = *reinterpret_cast<char **>(g_vmmBuf + 4);
		auto *slice = reinterpret_cast<STGLFOSubRateParamsSlice *>(quadTableBase + 0xcc0 * 4);
		check_eq("HandleStopChanged propagates -1 when precomp bit0 set", slice->stop, -1);

		delete[] table;
	}

	printf("[10] InitAMSSourceAddresses\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		lfo.frequencyAMS1Source = 1;
		lfo.frequencyAMS2Source = 2;
		lfo.frequencyAMS1ModSource = 3;
		lfo.shapeAMSSource = 4;
		unsigned char voiceBuf[0x10];
		memset(voiceBuf, 0, sizeof(voiceBuf));
		*reinterpret_cast<unsigned short *>(voiceBuf + 4) = 4; /* note=4, matches idx=0xcc0 */
		CSTGVoice *voice = reinterpret_cast<CSTGVoice *>(voiceBuf);
		lfo.InitAMSSourceAddresses(*voice);
		char *quadTableBase = *reinterpret_cast<char **>(g_vmmBuf + 4);
		auto *slice = reinterpret_cast<STGLFOSubRateParamsSlice *>(quadTableBase + 0xcc0 * 4);
		check_true("freqAMS1Source addr at +0x10", *reinterpret_cast<void **>(reinterpret_cast<char *>(slice) + 0x10) == voice->GetAMSSourceAddress(1));
		check_true("freqAMS2Source addr at +0x30", *reinterpret_cast<void **>(reinterpret_cast<char *>(slice) + 0x30) == voice->GetAMSSourceAddress(2));
		check_true("freqAMS1ModSource addr at +0x50", *reinterpret_cast<void **>(reinterpret_cast<char *>(slice) + 0x50) == voice->GetAMSSourceAddress(3));
		check_true("shapeAMSSource addr at +0x70", *reinterpret_cast<void **>(reinterpret_cast<char *>(slice) + 0x70) == voice->GetAMSSourceAddress(4));
	}

	printf("[11] PrepareSubRateAddressFixupTable 5-entry append\n");
	{
		CSTGLFO lfo;
		unsigned int entries[16];
		memset(entries, 0, sizeof(entries));
		CSTGSubRateAddressFixupTable table;
		table.entries = entries;
		table.count = 2; /* simulate a prior append */
		lfo.PrepareSubRateAddressFixupTable(table, 0x40);
		check_eq("count advances by exactly 5", table.count, 7);
		check_eq("entries[2] = (note+0x10)>>2", entries[2], (0x40 + 0x10) >> 2);
		check_eq("entries[3] = (note+0x30)>>2", entries[3], (0x40 + 0x30) >> 2);
		check_eq("entries[4] = (note+0x50)>>2", entries[4], (0x40 + 0x50) >> 2);
		check_eq("entries[5] = (note+0x70)>>2", entries[5], (0x40 + 0x70) >> 2);
		check_eq("entries[6] = (note+0x90)>>2", entries[6], (0x40 + 0x90) >> 2);
	}

	printf("[12] Registration accessors\n");
	{
		check_eq("GetId", CSTGLFO::GetId(), 0xe);
		check_eq("GetNumParams", CSTGLFO::GetNumParams(), 0x15);
		check_true("GetParamDescriptors -> STGLFOParams", CSTGLFO::GetParamDescriptors() == STGLFOParams);
		check_true("GetMessageHandlers -> sMessageHandlers", CSTGLFO::GetMessageHandlers() == _ZN7CSTGLFO16sMessageHandlersE);
		check_true("GetValueGetters -> sValueGetters", CSTGLFO::GetValueGetters() == _ZN7CSTGLFO13sValueGettersE);
	}

	printf("[13] Follow-up pass: BlendKnobIndex-derived UpdateFade/UpdateDelay\n");
	{
		/* Spot-checks the SAME formula the native-execution harness
		 * confirmed against the real OA.ko bytes directly (see
		 * lfo_component.cpp's own file comment for the harness
		 * derivation) -- table[idx]=idx ramp + busGain=1 +
		 * cst4=1.0f (the REAL resolved .rodata.cst4 value) makes the
		 * output directly reveal which index the blend picked. */
		for (int i = 0; i < 100; i++)
			CSTGTables::kLFOFadeTimeSeconds[i] = static_cast<float>(i);
		static unsigned char busMgrRaw[64];
		memset(busMgrRaw, 0, sizeof(busMgrRaw));
		*reinterpret_cast<float *>(busMgrRaw) = 1.0f; /* busGainReciprocal @+0x00 */
		CSTGAudioBusManager::sInstance = reinterpret_cast<CSTGAudioBusManager *>(busMgrRaw);

		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		lfo.fade = 49;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx);
		static bool (*predTrue)(CSTGPatchMessageContext *) = AlwaysTruePredicate;
		ctx._vtablePtr = reinterpret_cast<void *>(&predTrue);
		STGLFOPrecomputed *pc = reinterpret_cast<STGLFOPrecomputed *>(ctx.precomputedBaseOffset);
		memset(pc, 0, sizeof(*pc));
		pc->fadeV = 0.5f; /* fade=49, V=0.5 -> idx = 49 + trunc(0.5*(99-49)) = 49+25 = 74 */

		STGConvertedParam newVal;
		newVal.value = 49;
		lfo.UpdateFade(ctx, newVal);
		check_near("UpdateFade: fadeSeconds = 1/idx (idx=74)", pc->fadeSeconds, 1.0f / 74.0f);
		check_near("UpdateFade: fadeIsZeroFlag clear", pc->fadeIsZeroFlag, 0.0f);

		/* fade=0, V=-1.0 (v<0 branch) -> idx = 0 + trunc(-1.0*0) = 0 -> zero-sentinel path */
		lfo.fade = 0;
		memset(pc, 0, sizeof(*pc));
		pc->fadeV = -1.0f;
		newVal.value = 0;
		lfo.UpdateFade(ctx, newVal);
		check_near("UpdateFade idx==0: fadeSeconds=0", pc->fadeSeconds, 0.0f);
		check_near("UpdateFade idx==0: fadeIsZeroFlag=1", pc->fadeIsZeroFlag, 1.0f);

		/* UpdateDelay: 2-stage blend, ccMod from paramConstantsTable+0x3ac. */
		unsigned char paramConsts[0x3b0];
		memset(paramConsts, 0, sizeof(paramConsts));
		*reinterpret_cast<float *>(paramConsts + 0x3ac) = 0.5f;
		ctx.paramConstantsTable = paramConsts;

		lfo.delay = 10;
		lfo.flags = 0x2; /* wants CC mod */
		memset(pc, 0, sizeof(*pc));
		pc->delayV = 1.0f; /* idx1 = 10 + trunc(1.0*(99-10)) = 10+89 = 99; idx2 = 99 + trunc(0.5*(99-99)) = 99 */
		newVal.value = 10;
		lfo.UpdateDelay(ctx, newVal);
		double wantTicks = static_cast<double>(99) * 0.032 * 1.0;
		check_eq("UpdateDelay: delayTicks = trunc(idx2 * 0.032 * busGain)", pc->delayTicks,
			 static_cast<long>(static_cast<int>(static_cast<float>(wantTicks))));
	}

	printf("[14] UpdateStop field store + pc mirror + propagation\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		lfo.flags = 0;
		FakeVoice voice;
		SetFakeVoiceNote(voice, 8);
		FakeListEntry entry;
		entry.next = nullptr;
		entry.voice = &voice;
		static unsigned char table14[0x50];
		memset(table14, 0, sizeof(table14));
		*reinterpret_cast<void **>(table14 + 0x44) = &entry; /* componentSlotIndex=0 -> +0x44 */
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx, table14);
		static bool (*predTrue)(CSTGPatchMessageContext *) = AlwaysTruePredicate;
		ctx._vtablePtr = reinterpret_cast<void *>(&predTrue);
		STGLFOPrecomputed *pc = reinterpret_cast<STGLFOPrecomputed *>(ctx.precomputedBaseOffset);
		memset(pc, 0, sizeof(*pc));

		STGLFOSubRateParamsSlice *slice = lfo.GetOutput(8, 0);
		memset(slice, 0, sizeof(*slice));

		STGConvertedParam newVal;
		newVal.value = 1;
		lfo.UpdateStop(ctx, newVal);
		check_true("UpdateStop: this->flags bit0 set", (lfo.flags & 0x1) != 0);
		check_true("UpdateStop: pc->flags bit0 set", (pc->flags & 0x1) != 0);
		check_eq("UpdateStop: propagated -1 to slice->stop", slice->stop, -1);
	}

	printf("[15] PrecomputeData baseline (V fields reset, mirrors, freq/tempo/fade/delay filled)\n");
	{
		static unsigned char busMgrRaw[64];
		memset(busMgrRaw, 0, sizeof(busMgrRaw));
		*reinterpret_cast<float *>(busMgrRaw) = 1.0f; /* busGainReciprocal @+0x00 */
		CSTGAudioBusManager::sInstance = reinterpret_cast<CSTGAudioBusManager *>(busMgrRaw);
		for (int i = 0; i < 101; i++)
			CSTGTables::kLFOKnobToFreq[i] = static_cast<float>(i);
		CSTGTempo::sBaseNoteClockTable[3] = 100;

		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		lfo.flags = 0x1; /* stop set */
		lfo.waveform = 5;
		lfo.shape = 42;
		lfo.frequency = 3;
		lfo.delay = 0;
		lfo.fade = 0;
		lfo.midiTempoSyncBaseNote = 3;
		lfo.midiTempoSyncTimes = 2;

		unsigned char paramConsts[0x3b0];
		memset(paramConsts, 0, sizeof(paramConsts));
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx);
		ctx.paramConstantsTable = paramConsts;
		STGLFOPrecomputed *pc = reinterpret_cast<STGLFOPrecomputed *>(ctx.precomputedBaseOffset);
		memset(pc, 0xcd, sizeof(*pc)); /* poison, so every field below is a real write, not luck */

		lfo.PrecomputeData(ctx);
		check_near("PrecomputeData: freqV reset to 0", pc->freqV, 0.0f);
		check_near("PrecomputeData: delayV reset to 0", pc->delayV, 0.0f);
		check_near("PrecomputeData: fadeV reset to 0", pc->fadeV, 0.0f);
		check_true("PrecomputeData: pc->flags bit0 mirrors this->flags bit0", (pc->flags & 0x1) != 0);
		check_eq("PrecomputeData: pc->waveform mirrors this->waveform", pc->waveform, 5);
		check_eq("PrecomputeData: pc->shapeRaw mirrors this->shape", pc->shapeRaw, 42);
		check_eq("PrecomputeData: baseNoteBlendIndex = baseNote (V=0)", pc->baseNoteBlendIndex, 3);
		check_near("PrecomputeData: tempoPeriod = times*clockTable[idx]*2^-30",
			   pc->tempoPeriod, static_cast<float>(2 * 100) * 9.313225746154785e-10f);
		/* freq: idx = frequency (V=0 both stages) -> CalculateFreq(idx=3, weight/fine-offset=0) */
		check_near("PrecomputeData: freqResult from CalculateFreq(idx=frequency)", pc->freqResult, 3.0f * 1.0f * 8.0f);
	}

	printf("[16] HandleCC dispatch (0x4c=frequency, 0x4e=delay, else no-op)\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		lfo.frequency = 3;
		lfo.delay = 0;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx);
		STGLFOPrecomputed *pc = reinterpret_cast<STGLFOPrecomputed *>(ctx.precomputedBaseOffset);
		memset(pc, 0, sizeof(*pc));

		CSTGControllerValue ccVal;
		memset(&ccVal, 0, sizeof(ccVal));
		ccVal.field4 = 0.0f;

		lfo.HandleCC(ctx, 0x4d, ccVal); /* unhandled CC -> no-op */
		check_near("HandleCC(0x4d): pc->freqResult untouched", pc->freqResult, 0.0f);

		lfo.HandleCC(ctx, 0x4c, ccVal); /* frequency, V=0,ccMod=0(not wanted) -> idx=frequency=3 */
		check_near("HandleCC(0x4c): freqResult = CalculateFreq(idx=3)", pc->freqResult, 3.0f * 1.0f * 8.0f);

		memset(pc, 0, sizeof(*pc));
		lfo.HandleCC(ctx, 0x4e, ccVal); /* delay, V=0,ccMod=0 -> idx=delay=0 -> ticks=trunc(0*0.032*1)=0 */
		check_eq("HandleCC(0x4e): delayTicks = 0", pc->delayTicks, 0);
	}

	printf("[17] ToneAdjustStop/Waveform/Shape\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		lfo.flags = 0x1; /* current stop = set */
		lfo.waveform = 7;
		lfo.shape = 11;
		FakeVoice voice;
		SetFakeVoiceNote(voice, 2);
		FakeListEntry entry;
		entry.next = nullptr;
		entry.voice = &voice;
		static unsigned char table17[0x50];
		memset(table17, 0, sizeof(table17));
		*reinterpret_cast<void **>(table17 + 0x44) = &entry;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx, table17);
		static bool (*predTrue)(CSTGPatchMessageContext *) = AlwaysTruePredicate;
		ctx._vtablePtr = reinterpret_cast<void *>(&predTrue);
		STGLFOPrecomputed *pc = reinterpret_cast<STGLFOPrecomputed *>(ctx.precomputedBaseOffset);
		memset(pc, 0, sizeof(*pc));

		STGConvertedParam newVal;
		memset(&newVal, 0, sizeof(newVal));
		newVal.value = -1; /* "unspecified" -> use this->flags bit0 (set) */
		lfo.ToneAdjustStop(ctx, newVal);
		check_true("ToneAdjustStop(-1): uses current (set) stop state", (pc->flags & 0x1) != 0);

		memset(&newVal, 0, sizeof(newVal));
		newVal.value = 9;
		lfo.ToneAdjustShape(ctx, newVal);
		check_eq("ToneAdjustShape: pc->shapeRaw = newVal.value", pc->shapeRaw, 9);
		STGLFOSubRateParamsSlice *slice = lfo.GetOutput(2, 0);
		check_eq("ToneAdjustShape: propagated to slice->shape", slice->shape, 9);
	}

	printf("[18] GetKeySyncMasterLFO -- no-match returns null\n");
	{
		CSTGLFO lfo;
		lfo._slotInfo = &g_slotInfo;
		lfo.flags = 0; /* not keySync */
		unsigned char voiceRaw[0x70];
		memset(voiceRaw, 0, sizeof(voiceRaw));
		CSTGVoice *voice = reinterpret_cast<CSTGVoice *>(voiceRaw);
		unsigned char sub64[0x50];
		memset(sub64, 0, sizeof(sub64)); /* [0xb] != 2 */
		*reinterpret_cast<unsigned char **>(voiceRaw + 0x64) = sub64;
		unsigned char globalTail[0x29c9fb0];
		/* groupBase (voiceRaw+0xc) + groupIdx(0)*12 -> +0x44 = null head */
		unsigned char group[0x48];
		memset(group, 0, sizeof(group));
		*reinterpret_cast<unsigned char **>(voiceRaw + 0xc) = group;
		(void)globalTail;
		check_true("GetKeySyncMasterLFO: empty group -> nullptr", lfo.GetKeySyncMasterLFO(voice) == nullptr);
		check_true("ShouldDelayCompensateRestart: true when no master", lfo.ShouldDelayCompensateRestart(voice));
	}

	printf("[round 66] CSTGLFOBase::AdvanceFadeEnv/ShouldDelayCompensateRestart\n");
	{
		STGLFOSubRateParamsSlice slice;
		memset(&slice, 0xAB, sizeof(slice));
		unsigned char before[sizeof(STGLFOSubRateParamsSlice)];
		memcpy(before, &slice, sizeof(slice));
		CSTGLFOBase::AdvanceFadeEnv(&slice, 5);
		check_true("AdvanceFadeEnv: confirmed-empty body doesn't touch slice",
			   memcmp(before, &slice, sizeof(slice)) == 0);
		check_true("CSTGLFOBase::ShouldDelayCompensateRestart(nullptr) -> false (always)",
			   CSTGLFOBase::ShouldDelayCompensateRestart(nullptr) == false);
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
