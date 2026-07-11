// SPDX-License-Identifier: GPL-2.0
/*
 * test_combi_ctor.cpp  -  host-side known-answer test for
 * CSTGCombi::CSTGCombi() -- batch 45, see src/engine/combi_ctor.cpp.
 *
 * Poisons the object's memory with a non-zero pattern before
 * construction (matching test_program_ctor.cpp's own discipline), so any
 * field the ctor is SUPPOSED to leave untouched shows up as
 * still-poisoned rather than coincidentally reading as zero.
 *
 * Links src/engine/program_ctor.cpp ALONGSIDE combi_ctor.cpp (rather than
 * re-mocking CIFXEffectSlot/CSTGVectorMotion/CSTGControllerInfo and the
 * shared _ZTV* placeholders combi_ctor.cpp itself does not define) --
 * those three sub-object ctors and every base/shared vtable symbol
 * CSTGCombi::CSTGCombi() references are ALREADY defined for real in
 * program_ctor.cpp, and this binary needs exactly ONE definition of each,
 * shared correctly between both CSTGProgram::CSTGProgram() (unused here,
 * but still linked in) and CSTGCombi::CSTGCombi(). This also means the
 * REAL CIFXEffectSlot/CSTGVectorMotion/CSTGControllerInfo field writes
 * get exercised at CSTGCombi's own offsets, not just a pointer-capture
 * mock. Only CSTGAudioInput (real body lives in global.cpp),
 * CSTGToneAdjust (real body lives in program_slot_ctor.cpp -- still
 * referenced by the linked-in CSTGProgram::CSTGProgram(), even though
 * this test never constructs a CSTGProgram), and CSTGProgramSlot (real
 * body also lives in program_slot_ctor.cpp) need their own trivial
 * pointer-capturing mocks here, mirroring test_program_ctor.cpp's own
 * precedent for the first two.
 */

#include <cstdio>
#include <cstring>
#include "oa_global.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got != want) {
		printf("  FAILED: %s (got 0x%lx, want 0x%lx)\n", label, got, want);
		g_fail++;
	} else {
		printf("  ok: %s\n", label);
	}
}

static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

static void *g_audioInputThis;
static void *g_toneAdjustThis;
static void *g_programSlotThis[16];
static int g_programSlotCount;

CSTGAudioInput::CSTGAudioInput() { g_audioInputThis = this; }
CSTGToneAdjust::CSTGToneAdjust() { g_toneAdjustThis = this; }
CSTGProgramSlot::CSTGProgramSlot() { g_programSlotThis[g_programSlotCount++] = this; }

int main(void)
{
	printf("CSTGCombi ctor known-answer test (batch 45)\n");
	printf("============================================\n");

	static unsigned char buf[0x1a00];
	memset(buf, 0xcc, sizeof(buf));
	CSTGCombi *combi = new (buf) CSTGCombi();
	unsigned char *self = (unsigned char *)combi;

	printf("\n[1] own vtable overwrite + secondary base vtable\n");
	check_eq("+0x0 (own derived vtable, overwritten) == _ZTV9CSTGCombi+8",
		 *(unsigned int *)(self + 0x0), ToU32(_ZTV9CSTGCombi + 8));
	check_eq("+0x4 (CSTGEffectRack base vtable) == _ZTV14CSTGEffectRack+8",
		 *(unsigned int *)(self + 0x4), ToU32(_ZTV14CSTGEffectRack + 8));

	printf("\n[2] twelve CIFXEffectSlot sub-objects, 0xa8 stride (real ctor)\n");
	static const unsigned int kIFXOffsets[] = {
		0x8, 0xb0, 0x158, 0x200, 0x2a8, 0x350,
		0x3f8, 0x4a0, 0x548, 0x5f0, 0x698, 0x740,
	};
	for (unsigned int off : kIFXOffsets) {
		char label[48];
		snprintf(label, sizeof(label), "CIFXEffectSlot@+0x%x vtable", off);
		check_eq(label, *(unsigned int *)(self + off), ToU32(_ZTV14CIFXEffectSlot + 8));
		snprintf(label, sizeof(label), "CIFXEffectSlot@+0x%x own +0x9b == 0x19", off);
		check_eq(label, self[off + 0x9b], 0x19);
	}

	printf("\n[3] CMFXEffectSlot x2, CTFXEffectSlot x2 (direct field writes, no ctor)\n");
	check_eq("CMFX#1@+0x7e8 vtable", *(unsigned int *)(self + 0x7e8), ToU32(_ZTV14CMFXEffectSlot + 8));
	check_eq("CMFX#1 +0x7f0 word == 1", *(unsigned short *)(self + 0x7f0), 1);
	check_eq("CMFX#2@+0x884 vtable", *(unsigned int *)(self + 0x884), ToU32(_ZTV14CMFXEffectSlot + 8));
	check_eq("CMFX#2 +0x88c word == 1", *(unsigned short *)(self + 0x88c), 1);
	check_eq("CMFX#2 +0x880 dword == 0", *(unsigned int *)(self + 0x880), 0);
	check_eq("CTFX#1@+0x920 vtable", *(unsigned int *)(self + 0x920), ToU32(_ZTV14CTFXEffectSlot + 8));
	check_eq("CTFX#1 +0x928 word == 1", *(unsigned short *)(self + 0x928), 1);
	check_eq("CTFX#1 +0x91c dword == 0", *(unsigned int *)(self + 0x91c), 0);
	check_eq("CTFX#2@+0x9b8 vtable", *(unsigned int *)(self + 0x9b8), ToU32(_ZTV14CTFXEffectSlot + 8));
	check_eq("CTFX#2 +0x9c0 word == 1", *(unsigned short *)(self + 0x9c0), 1);

	printf("\n[4] CSTGEffectBalance, CSTGCommonEffectLFO x2\n");
	check_eq("EffectBalance@+0xa55 vtable", *(unsigned int *)(self + 0xa55), ToU32(_ZTV17CSTGEffectBalance + 8));
	check_eq("CommonEffectLFO#1@+0xa59 vtable", *(unsigned int *)(self + 0xa59), ToU32(_ZTV19CSTGCommonEffectLFO + 8));
	check_eq("CommonEffectLFO#2@+0xa6a vtable", *(unsigned int *)(self + 0xa6a), ToU32(_ZTV19CSTGCommonEffectLFO + 8));
	check_eq("+0xa66 == 0", self[0xa66], 0);
	check_eq("+0xa77 == 0", self[0xa77], 0);

	printf("\n[5] CSTGVectorMotion@+0xa7b, CSTGControllerInfo@+0xad3 (real ctors), CSTGAudioInput@+0xae7 (mocked)\n");
	check_eq("VectorMotion vtable", *(unsigned int *)(self + 0xa7b), ToU32(_ZTV16CSTGVectorMotion + 8));
	check_eq("VectorMotion packed +0x15 relative (0xa7b+0x15=0xa90)",
		 *(unsigned int *)(self + 0xa90), 0x3b810204u);
	check_eq("ControllerInfo vtable", *(unsigned int *)(self + 0xad3), ToU32(_ZTV18CSTGControllerInfo + 8));
	check_eq("AudioInput placement-constructed at +0xae7",
		 (unsigned long)g_audioInputThis, (unsigned long)(self + 0xae7));

	printf("\n[6] sixteen CSTGProgramSlot sub-objects, 0xe8 stride (mocked), each with its own index byte\n");
	check_eq("CSTGProgramSlot ctor called 16 times", g_programSlotCount, 16);
	for (unsigned int i = 0; i < 16; i++) {
		char label[48];
		unsigned char *slot = self + 0xb63 + i * 0xe8;
		snprintf(label, sizeof(label), "CSTGProgramSlot[%u] placement offset", i);
		check_eq(label, (unsigned long)g_programSlotThis[i], (unsigned long)slot);
		snprintf(label, sizeof(label), "CSTGProgramSlot[%u] own +0x4 index byte", i);
		check_eq(label, slot[0x4], i);
	}

	printf("\n[7] trailing byte @ +0x19e3 (NOT a 17th sub-object)\n");
	check_eq("+0x19e3 == 0 (past the 16th slot's own 0xe8-byte extent)", self[0x19e3], 0);
	check_eq("+0x19e4 untouched (still poisoned)", self[0x19e4], 0xcc);
	check_eq("+0x19e6 untouched (still poisoned, right before CSTGSequence's own +0x19e7 tail)",
		 self[0x19e6], 0xcc);

	printf("\n[8] confirmed real gap stays poisoned\n");
	check_eq("first CIFXEffectSlot's own +0x7 gap still 0xcc", self[0x8 + 0x7], 0xcc);
	check_eq("well past the confirmed tail (+0x1a00-1) still 0xcc", self[sizeof(buf) - 1], 0xcc);

	(void)g_toneAdjustThis; /* only referenced to avoid an unused-variable warning */

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nAll checks passed.\n");
	return 0;
}
