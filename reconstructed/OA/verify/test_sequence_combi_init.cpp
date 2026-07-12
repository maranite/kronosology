// SPDX-License-Identifier: GPL-2.0
/*
 * test_sequence_combi_init.cpp  -  host-side known-answer test for
 * CSTGSequence::Initialize()/CSTGCombi::Initialize()/CSTGProgramSlot::
 * Initialize()+UseDefaults()/CSTGPerformance::Initialize()/
 * CSTGEffectRack::Initialize() (batch 55, sec 10.230, see
 * src/engine/sequence_combi_init.cpp for the full derivation).
 *
 * Constructs a REAL CSTGSequence via placement-new (linking
 * combi_ctor.cpp/sequence_ctor.cpp/program_ctor.cpp/program_slot_ctor.cpp
 * alongside the new sequence_combi_init.cpp), matching
 * test_combi_ctor.cpp's own precedent: CIFXEffectSlot/CSTGVectorMotion/
 * CSTGControllerInfo/CSTGProgramSlot/CSTGToneAdjust all get their REAL
 * ctor bodies this way, only CSTGAudioInput needs a trivial local mock
 * (its real body lives in global.cpp, not linked here).
 *
 * The generic CSTGParamsOwner::UseDefaults() is mocked as a call
 * counter (production's real no-op lives in bar2_stubs.cpp, not linked
 * here) -- same established convention as test_waveseq_setlist_init.cpp
 * (sec 10.229). Expected total dispatch count, hand-traced from the
 * real call graph:
 *   16x CSTGProgramSlot::UseDefaults()'s own base-forwarding call   = 16
 *   16x CSTGProgramSlot::Initialize()'s own direct ToneAdjust call = 16
 *   CSTGPerformance::Initialize() (2x CommonEffectLFO + VectorMotion
 *     + AudioInput)                                                =  4
 *   CSTGCombi::Initialize()'s own tail dispatch                    =  1
 *   CSTGSequence::Initialize() (16x CSTGHDRTrack + 1x Metronome)    = 17
 *   ---------------------------------------------------------------------
 *   TOTAL                                                          = 54
 */

#include <cstdio>
#include <cstring>
#include "oa_global.h"
#include "oa_engine_init.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%lx\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%lx)\n", want);
}

static int g_useDefaultsCalls;
void CSTGParamsOwner::UseDefaults() { g_useDefaultsCalls++; }

static void *g_audioInputThis;
CSTGAudioInput::CSTGAudioInput() { g_audioInputThis = this; }

int main(void)
{
	printf("CSTGSequence/CSTGCombi/CSTGProgramSlot/CSTGPerformance/CSTGEffectRack::Initialize() known-answer test\n");
	printf("=======================================================================================================\n");

	static unsigned char buf[0x2000];
	memset(buf, 0xcc, sizeof(buf));
	CSTGSequence *seq = new (buf) CSTGSequence();
	unsigned char *self = (unsigned char *)seq;

	g_useDefaultsCalls = 0;
	seq->Initialize();

	printf("\n[1] total UseDefaults() dispatch count across the whole real chain\n");
	check_eq("UseDefaults() total == 54", (unsigned int)g_useDefaultsCalls, 54);

	printf("\n[2] CSTGProgramSlot::Initialize()'s own +0x10 = +0x4 index-byte copy (16 slots, 0xe8 stride @ +0xb63)\n");
	for (unsigned int i = 0; i < 16; i++) {
		unsigned char *slot = self + 0xb63 + i * 0xe8;
		char label[64];
		snprintf(label, sizeof(label), "slot[%u] +0x10 == +0x4 (== %u)", i, i);
		check_eq(label, slot[0x10], i);
	}

	printf("\n[3] CSTGProgramSlot::UseDefaults()'s own real body: 12 literal index bytes 1..12 @ +0x63..+0x6e\n");
	{
		unsigned char *slot0 = self + 0xb63;
		for (unsigned int i = 0; i < 12; i++) {
			char label[64];
			snprintf(label, sizeof(label), "slot[0] +0x%x == %u", 0x63 + i, i + 1);
			check_eq(label, slot0[0x63 + i], i + 1);
		}
	}

	printf("\n[4] CSTGEffectRack::Initialize()'s own 16 effect-slot index/flag bytes (perfType hardcoded 2 -> flag always 0)\n");
	{
		static const unsigned int kSlotOffsets[16] = {
			0x4,   0xac,  0x154, 0x1fc, 0x2a4, 0x34c, 0x3f4, 0x49c,
			0x544, 0x5ec, 0x694, 0x73c, 0x7e4, 0x880, 0x91c, 0x9b4,
		};
		unsigned char *effectRack = self + 0x4; /* CSTGEffectRack base = CSTGCombi+4 */
		for (unsigned int i = 0; i < 16; i++) {
			unsigned char *slot = effectRack + kSlotOffsets[i];
			char label[64];
			snprintf(label, sizeof(label), "effectSlot[%u] +0x4 index == %u", i, i);
			check_eq(label, slot[0x4], i);
			snprintf(label, sizeof(label), "effectSlot[%u] +0x6 flag == 0", i);
			check_eq(label, slot[0x6], 0);
		}
		check_eq("+0xa50 bit0 cleared", effectRack[0xa50] & 1, 0);
	}

	printf("\n[5] CSTGPerformance::Initialize()'s own literal float write @ +0xb5f == 1.0f\n");
	check_eq("+0xb5f float bits == 0x3f800000", *(unsigned int *)(self + 0xb5f), 0x3f800000u);

	(void)g_audioInputThis;

	printf("=======================================================================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
