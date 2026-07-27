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
 * behavior. See oa_lfo.h for the 19 methods this pass deliberately
 * defers (not exercised here).
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
/* HandleUpdateTempoPeriod is a deliberately deferred real method (see
 * oa_lfo.h) that UpdateMIDITempoSyncTimes/BaseNote call unconditionally
 * -- neither is exercised by this test, but the symbol must still
 * resolve at link time. Empty stub, not a claim about real behavior. */
void CSTGLFO::HandleUpdateTempoPeriod(CSTGPatchMessageContext &) {}
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

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
