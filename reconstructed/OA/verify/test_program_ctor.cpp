// SPDX-License-Identifier: GPL-2.0
/*
 * test_program_ctor.cpp  -  host-side known-answer test for
 * CSTGProgram::CSTGProgram() and its four new sub-object ctors
 * (CIFXEffectSlot, CSTGVectorMotion, CSTGControllerInfo, CSTGCommonLFO)
 * -- batch 44, see src/engine/program_ctor.cpp.
 *
 * Poisons the object's memory with a non-zero pattern before
 * construction (same discipline as test_program_slot_ctor.cpp /
 * test_managers.cpp), so any field the ctor is SUPPOSED to leave
 * untouched shows up as still-poisoned rather than coincidentally
 * reading as zero.
 *
 * CSTGAudioInput::CSTGAudioInput() (real body lives in global.cpp) and
 * CSTGToneAdjust::CSTGToneAdjust() (real body lives in
 * program_slot_ctor.cpp) are both ALREADY independently verified for
 * real elsewhere (test_program_slot_ctor.cpp covers ToneAdjust; the
 * AudioInput ctor is covered wherever global.cpp's own tests exercise
 * it) -- deliberately NOT linked here (global.cpp pulls in a huge
 * unrelated dependency graph, matching this project's own "give a new
 * ctor its own isolated TU" precedent). This file provides its own
 * trivial pointer-capturing mocks for both, just to confirm
 * CSTGProgram::CSTGProgram() placement-constructs each at the right
 * offset -- not to re-verify their own internal field layouts.
 */

#include <cstdio>
#include <cstring>
#include "oa_global.h"
#include "oa_engine_init.h"
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
CSTGAudioInput::CSTGAudioInput() { g_audioInputThis = this; }
CSTGToneAdjust::CSTGToneAdjust() { g_toneAdjustThis = this; }

int main(void)
{
	printf("CSTGProgram ctor cluster known-answer test (batch 44)\n");
	printf("=======================================================\n");

	printf("\n[1] CIFXEffectSlot::CIFXEffectSlot() in isolation\n");
	{
		unsigned char buf[0x100];
		memset(buf, 0xcc, sizeof(buf));
		CIFXEffectSlot *slot = new (buf) CIFXEffectSlot();
		unsigned char *b = (unsigned char *)slot;
		check_eq("vtable ptr", *(unsigned int *)b, ToU32(_ZTV14CIFXEffectSlot + 8));
		check_eq("+0x4 == 0", b[0x4], 0);
		check_eq("+0x5 == 0", b[0x5], 0);
		check_eq("+0x6 == 0", b[0x6], 0);
		check_eq("+0x8 word == 1", *(unsigned short *)(b + 0x8), 1);
		check_eq("+0x98 == 0", b[0x98], 0);
		check_eq("+0x99 == 0", b[0x99], 0);
		check_eq("+0x9a == 0", b[0x9a], 0);
		check_eq("+0x9b == 0x19", b[0x9b], 0x19);
		check_eq("+0x9c float == 64.0f", *(unsigned int *)(b + 0x9c), 0x42800000);
		check_eq("+0xa0 == 0", *(unsigned int *)(b + 0xa0), 0);
		check_eq("+0xa4 == 0", *(unsigned int *)(b + 0xa4), 0);
		/* +0x9 is NOT a real gap: it's the upper byte of the +0x8 WORD
		 * write (value 1 -> bytes 01 00 little-endian), so it's
		 * genuinely zeroed too, not poisoned -- +0x7 (between the
		 * +0x6 byte and the +0x8 word) is the real untouched gap. */
		check_eq("+0x7 untouched (still poisoned)", b[0x7], 0xcc);
	}

	printf("\n[2] CSTGVectorMotion::CSTGVectorMotion() in isolation\n");
	{
		unsigned char buf[0x80];
		memset(buf, 0xcc, sizeof(buf));
		CSTGVectorMotion *vm = new (buf) CSTGVectorMotion();
		unsigned char *b = (unsigned char *)vm;
		check_eq("vtable ptr", *(unsigned int *)b, ToU32(_ZTV16CSTGVectorMotion + 8));
		static const unsigned int zeroBytes[] = { 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb,
							   0x2e, 0x38, 0x42, 0x4c, 0x56 };
		for (unsigned int off : zeroBytes) {
			char label[32];
			snprintf(label, sizeof(label), "+0x%x == 0", off);
			check_eq(label, b[off], 0);
		}
		static const unsigned int packedOffs[] = { 0x15, 0x19, 0x1d, 0x21 };
		for (unsigned int off : packedOffs) {
			char label[48];
			snprintf(label, sizeof(label), "+0x%x == 0x3b810204", off);
			check_eq(label, *(unsigned int *)(b + off), 0x3b810204u);
		}
		check_eq("+0xc untouched (still poisoned)", b[0xc], 0xcc);
	}

	printf("\n[3] CSTGControllerInfo::CSTGControllerInfo() in isolation\n");
	{
		unsigned char buf[0x40];
		memset(buf, 0xcc, sizeof(buf));
		CSTGControllerInfo *ci = new (buf) CSTGControllerInfo();
		unsigned char *b = (unsigned char *)ci;
		check_eq("vtable ptr", *(unsigned int *)b, ToU32(_ZTV18CSTGControllerInfo + 8));
		for (unsigned int off = 0x4; off <= 0x13; off++) {
			char label[32];
			snprintf(label, sizeof(label), "+0x%x == 0", off);
			check_eq(label, b[off], 0);
		}
		check_eq("+0x14 untouched (still poisoned)", b[0x14], 0xcc);
	}

	printf("\n[4] CSTGCommonLFO::CSTGCommonLFO() in isolation\n");
	{
		unsigned char buf[0x60];
		memset(buf, 0xcc, sizeof(buf));
		CSTGCommonLFO *lfo = new (buf) CSTGCommonLFO();
		unsigned char *b = (unsigned char *)lfo;
		check_eq("vtable ptr +0x0", *(unsigned int *)b, ToU32(_ZTV13CSTGCommonLFO + 8));
		check_eq("vtable ptr +0x4", *(unsigned int *)(b + 0x4), ToU32(_ZTV13CSTGCommonLFO + 0x6c));
		static const unsigned int zeroBytes[] = { 0x11, 0x1a, 0x1b, 0x20, 0x26, 0x2b, 0x30, 0x1e, 0x1d };
		for (unsigned int off : zeroBytes) {
			char label[32];
			snprintf(label, sizeof(label), "+0x%x == 0", off);
			check_eq(label, b[off], 0);
		}
		static const unsigned int zeroDwords[] = { 0xd, 0x22, 0x27, 0x2c };
		for (unsigned int off : zeroDwords) {
			char label[32];
			snprintf(label, sizeof(label), "+0x%x dword == 0", off);
			check_eq(label, *(unsigned int *)(b + off), 0);
		}
		check_eq("+0x14 untouched (still poisoned)", b[0x14], 0xcc);
	}

	printf("\n[5] CSTGProgram::CSTGProgram() full construction\n");
	{
		static unsigned char buf[0xd00];
		memset(buf, 0xcc, sizeof(buf));
		CSTGProgram *prog = new (buf) CSTGProgram();
		unsigned char *self = (unsigned char *)prog;

		check_eq("+0x0 (own derived vtable, overwritten) == _ZTV11CSTGProgram+8",
			 *(unsigned int *)(self + 0x0), ToU32(_ZTV11CSTGProgram + 8));
		check_eq("+0x4 (CSTGEffectRack base vtable) == _ZTV14CSTGEffectRack+8",
			 *(unsigned int *)(self + 0x4), ToU32(_ZTV14CSTGEffectRack + 8));

		printf("\n  [5a] twelve CIFXEffectSlot sub-objects, 0xa8 stride\n");
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

		printf("\n  [5b] CMFXEffectSlot x2, CTFXEffectSlot x2\n");
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

		printf("\n  [5c] CSTGEffectBalance, CSTGCommonEffectLFO x2\n");
		check_eq("EffectBalance@+0xa55 vtable", *(unsigned int *)(self + 0xa55), ToU32(_ZTV17CSTGEffectBalance + 8));
		check_eq("CommonEffectLFO#1@+0xa59 vtable", *(unsigned int *)(self + 0xa59), ToU32(_ZTV19CSTGCommonEffectLFO + 8));
		check_eq("CommonEffectLFO#2@+0xa6a vtable", *(unsigned int *)(self + 0xa6a), ToU32(_ZTV19CSTGCommonEffectLFO + 8));
		check_eq("+0xa66 == 0", self[0xa66], 0);
		check_eq("+0xa77 == 0", self[0xa77], 0);

		printf("\n  [5d] CSTGVectorMotion@+0xa7b, CSTGControllerInfo@+0xad3, CSTGAudioInput@+0xae7\n");
		check_eq("VectorMotion vtable", *(unsigned int *)(self + 0xa7b), ToU32(_ZTV16CSTGVectorMotion + 8));
		check_eq("VectorMotion packed +0x15 relative (0xa7b+0x15=0xa90)",
			 *(unsigned int *)(self + 0xa90), 0x3b810204u);
		check_eq("ControllerInfo vtable", *(unsigned int *)(self + 0xad3), ToU32(_ZTV18CSTGControllerInfo + 8));
		check_eq("AudioInput placement-constructed at +0xae7",
			 (unsigned long)g_audioInputThis, (unsigned long)(self + 0xae7));

		printf("\n  [5e] CSTGCommonLFO@+0xb74 (nested dual-vtable)\n");
		check_eq("CommonLFO +0x0 vtable", *(unsigned int *)(self + 0xb74), ToU32(_ZTV13CSTGCommonLFO + 8));
		check_eq("CommonLFO +0x4 vtable", *(unsigned int *)(self + 0xb78), ToU32(_ZTV13CSTGCommonLFO + 0x6c));

		printf("\n  [5f] CSTGCommonStepSeq overwrite @ +0xba5/+0xba9 (ParamsOwner/StepSeqBase overwritten)\n");
		check_eq("+0xba5 == CommonStepSeq+8 (NOT ParamsOwner -- overwritten)",
			 *(unsigned int *)(self + 0xba5), ToU32(_ZTV17CSTGCommonStepSeq + 8));
		check_eq("+0xba9 == CommonStepSeq+0x68 (NOT StepSeqBase -- overwritten)",
			 *(unsigned int *)(self + 0xba9), ToU32(_ZTV17CSTGCommonStepSeq + 0x68));
		check_eq("+0xbb4 == 0", self[0xbb4], 0);
		check_eq("+0xbb0 dword == 0", *(unsigned int *)(self + 0xbb0), 0);
		check_eq("+0xbc1 == 0", self[0xbc1], 0);
		check_eq("+0xbbd dword == 0", *(unsigned int *)(self + 0xbbd), 0);
		check_eq("+0xbc2 == 0", self[0xbc2], 0);
		for (unsigned int off = 0xbe3; off < 0xbe3 + 0x20; off++) {
			char label[32];
			snprintf(label, sizeof(label), "+0x%x (32-byte zero run) == 0", off);
			check_eq(label, self[off], 0);
		}

		printf("\n  [5g] CSTGToneAdjust@+0xc4d + trailing zeroed fields\n");
		check_eq("ToneAdjust placement-constructed at +0xc4d",
			 (unsigned long)g_toneAdjustThis, (unsigned long)(self + 0xc4d));
		static const unsigned int trailingZeroBytes[] = {
			0xc23, 0xc24, 0xc27, 0xc30, 0xc46, 0xc47, 0xc48, 0xc49,
			0xc4a, 0xc4b, 0xc4c, 0xcb6, 0xcb7, 0xcd5, 0xcd6, 0xcd7,
			0xcd8, 0xcd9, 0xcda, 0xcdb, 0xcdc, 0xcdd, 0xcde, 0xcdf,
			0xce0, 0xce1, 0xce2, 0xce3,
		};
		for (unsigned int off : trailingZeroBytes) {
			char label[32];
			snprintf(label, sizeof(label), "+0x%x == 0", off);
			check_eq(label, self[off], 0);
		}
		static const unsigned int trailingZeroDwords[] = { 0xb63, 0xb6b, 0xb67, 0xb6f };
		for (unsigned int off : trailingZeroDwords) {
			char label[32];
			snprintf(label, sizeof(label), "+0x%x dword == 0", off);
			check_eq(label, *(unsigned int *)(self + off), 0);
		}

		printf("\n  [5h] confirmed real gaps stay poisoned\n");
		/* +0x7 relative to the first CIFXEffectSlot (absolute +0xf) is
		 * never touched by CIFXEffectSlot::CIFXEffectSlot() (confirmed
		 * fields: +0x4/+0x5/+0x6/+0x8(word)/+0x98../+0xa7 -- +0x7 sits
		 * in the untouched gap between +0x6 and +0x8). */
		check_eq("first CIFXEffectSlot's own +0x7 gap still 0xcc", self[0x8 + 0x7], 0xcc);
		check_eq("well past the confirmed tail (+0xd00-1) still 0xcc", self[sizeof(buf) - 1], 0xcc);
	}

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nAll checks passed.\n");
	return 0;
}
