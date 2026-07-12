// SPDX-License-Identifier: GPL-2.0
/*
 * test_vector_eg_ctors.cpp  -  KAT for CSTGVectorEGXOnly/EGXY/EGCC's own
 * constructors (see src/engine/vector_eg_ctors.cpp).
 *
 * Verifies: derived vtable pointer set to the confirmed real symbol+8,
 * the shared +0x3c/+0x40/+0x44/+0x48 field convention across all three
 * types, EGCC's four STGVJSAssignInfo pointer writes and four 0x8000
 * centered defaults, and EGXY's confirmed partial-bit-clear behavior at
 * +0x6e -- a byte the REAL base constructor does NOT touch at all (sec
 * 10.148 corrected sec 10.66's own earlier speculation on this exact
 * point, see oa_engine_init.h's own header comment on CSTGVectorEGBase);
 * this test poisons that byte itself (simulating "whatever uninitialized
 * memory looked like before", since these objects are placed into
 * pre-allocated CSTGBankMemory storage on the real target) rather than
 * relying on a base-constructor mock to set it.
 *
 * UPDATE (sec 10.227): CSTGVectorEGBase and its three derived siblings
 * are now genuinely C++-polymorphic (real `virtual void Init()`, see
 * oa_engine_init.h/vector_eg_ctors.cpp) -- the compiler emits the real
 * `_ZTVxxx` vtable symbols itself now, so this file's own manual
 * `extern "C" unsigned char _ZTVxxx[]` stand-in definitions are GONE
 * (they would now be duplicate-symbol link errors against the
 * compiler's own real vtables). The "vtable ptr nonzero" checks below
 * still make sense unchanged -- they never compared against a specific
 * symbol+8 address, only checked non-NULL.
 */

#include <cstdio>
#include <cstring>
#include <new>
#include "oa_engine_init.h"
#include "oa_global.h"

/* CSTGGlobal::sInstance's real definition lives in global.cpp, not
 * linked into this test target -- provide the storage here (this test
 * target's own established pattern, e.g. test_vector_manager_init.cpp's
 * CSTGVectorManager::sInstance). Only CSTGVectorEGXOnly::Init() reads
 * it (its own confirmed "+0x684 mode mirror" -- see vector_eg_ctors.cpp). */
CSTGGlobal *CSTGGlobal::sInstance;

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) {
		printf("  ok    %-60s 0x%lx\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

extern "C" unsigned char STGVJSAssignInfo[4];
unsigned char STGVJSAssignInfo[4];

/* Fake CSTGGlobal backing storage, just big enough to reach the
 * confirmed real +0x684 "mode" field CSTGVectorEGXOnly::Init() reads. */
static unsigned char g_fakeGlobal[0x688];

int main(void)
{
	printf("CSTGVectorEGXOnly/EGXY/EGCC constructor known-answer test\n");
	printf("=========================================================\n");

	CSTGGlobal::sInstance = reinterpret_cast<CSTGGlobal *>(g_fakeGlobal);
	*(unsigned int *)(g_fakeGlobal + 0x684) = 0x1234;

	printf("[1] CSTGVectorEGXOnly\n");
	{
		CSTGVectorEGXOnly *obj = new CSTGVectorEGXOnly();
		unsigned char *p = reinterpret_cast<unsigned char *>(obj);
		check_eq("self-pointer at +0x44", *(unsigned int *)(p + 0x44), (unsigned long)(unsigned int)(unsigned long)p);
		check_eq("list node next (+0x3c) == 0", *(unsigned int *)(p + 0x3c), 0);
		check_eq("list node prev (+0x40) == 0", *(unsigned int *)(p + 0x40), 0);
		check_eq("list owner (+0x48) == 0", *(unsigned int *)(p + 0x48), 0);
		check_eq("+0x60 == 0", *(unsigned int *)(p + 0x60), 0);
		check_eq("+0x64 == 0", *(unsigned int *)(p + 0x64), 0);
		check_eq("+0x68 == 0", *(unsigned int *)(p + 0x68), 0);
		check_eq("+0x6c == 0", *(unsigned int *)(p + 0x6c), 0);
		check_eq("+0x70 == 0", *(unsigned int *)(p + 0x70), 0);
		check_eq("+0x74 == 0", *(unsigned int *)(p + 0x74), 0);
		check_eq("vtable ptr nonzero (real symbol+8)", *(unsigned long *)p != 0, 1);

		/* sec 10.227: exercise the real vtable-slot-0 dispatch itself
		 * (the exact call CSTGVectorManager::Initialize() makes on
		 * every real boot), through a base-class pointer so this is a
		 * genuine virtual call, not a direct one. */
		p[0x80] = 0xff;
		*(unsigned int *)(p + 0x50) = 0xffffffff;
		*(unsigned int *)(p + 0x54) = 0xffffffff;
		*(unsigned int *)(p + 0x58) = 0xffffffff;
		*(unsigned int *)(p + 0x5c) = 0xffffffff;
		*(unsigned int *)(p + 0x4c) = 0xffffffff;
		static_cast<CSTGVectorEGBase *>(obj)->Init();
		check_eq("Init(): +0x80 cleared", p[0x80], 0);
		check_eq("Init(): +0x50 cleared", *(unsigned int *)(p + 0x50), 0);
		check_eq("Init(): +0x54 cleared", *(unsigned int *)(p + 0x54), 0);
		check_eq("Init(): +0x58 cleared", *(unsigned int *)(p + 0x58), 0);
		check_eq("Init(): +0x5c cleared", *(unsigned int *)(p + 0x5c), 0);
		check_eq("Init(): +0x4c mirrors CSTGGlobal's own +0x684 mode field",
			 *(unsigned int *)(p + 0x4c), 0x1234);

		delete obj;
	}

	printf("\n[2] CSTGVectorEGXY\n");
	{
		/* Poison the raw storage BEFORE construction (simulating
		 * "whatever uninitialized memory looked like before" -- the
		 * real base ctor does NOT touch +0x6e at all, confirmed sec
		 * 10.148) so the AND-mask's own real effect (clear bit 1 only,
		 * leave the rest alone) is verifiable regardless of what value
		 * happened to precede it. */
		unsigned char raw[sizeof(CSTGVectorEGXY)];
		memset(raw, 0xaa, sizeof(raw));
		CSTGVectorEGXY *obj = new (raw) CSTGVectorEGXY();
		unsigned char *p = reinterpret_cast<unsigned char *>(obj);
		check_eq("self-pointer at +0x44", *(unsigned int *)(p + 0x44), (unsigned long)(unsigned int)(unsigned long)p);
		check_eq("list node next (+0x3c) == 0", *(unsigned int *)(p + 0x3c), 0);
		check_eq("list node prev (+0x40) == 0", *(unsigned int *)(p + 0x40), 0);
		check_eq("list owner (+0x48) == 0", *(unsigned int *)(p + 0x48), 0);
		check_eq("+0x5c == 0", *(unsigned int *)(p + 0x5c), 0);
		check_eq("+0x60 == 0", *(unsigned int *)(p + 0x60), 0);
		check_eq("+0x64 == 0", *(unsigned int *)(p + 0x64), 0);
		check_eq("+0x6d == 0", p[0x6d], 0);
		check_eq("+0x6e bit 1 cleared, other bits (poisoned 0xaa) preserved", p[0x6e], 0xaa & 0xfd);
		check_eq("vtable ptr nonzero (real symbol+8)", *(unsigned long *)p != 0, 1);

		/* sec 10.227: exercise the real vtable-slot-0 dispatch. */
		p[0x6e] |= 0x02; /* re-set the bit Init() should clear again */
		*(unsigned int *)(p + 0x50) = 0xffffffff;
		*(unsigned int *)(p + 0x54) = 0xffffffff;
		*(unsigned int *)(p + 0x58) = 0xffffffff;
		*(unsigned int *)(p + 0x4c) = 0xffffffff;
		static_cast<CSTGVectorEGBase *>(obj)->Init();
		check_eq("Init(): +0x6e bit 1 cleared again", p[0x6e] & 0x02, 0);
		check_eq("Init(): +0x50 cleared", *(unsigned int *)(p + 0x50), 0);
		check_eq("Init(): +0x54 cleared", *(unsigned int *)(p + 0x54), 0);
		check_eq("Init(): +0x58 cleared", *(unsigned int *)(p + 0x58), 0);
		check_eq("Init(): +0x4c cleared", *(unsigned int *)(p + 0x4c), 0);

		obj->~CSTGVectorEGXY();
	}

	printf("\n[3] CSTGVectorEGCC\n");
	{
		CSTGVectorEGCC *obj = new CSTGVectorEGCC();
		unsigned char *p = reinterpret_cast<unsigned char *>(obj);
		check_eq("self-pointer at +0x44", *(unsigned int *)(p + 0x44), (unsigned long)(unsigned int)(unsigned long)p);
		check_eq("list node next (+0x3c) == 0", *(unsigned int *)(p + 0x3c), 0);
		check_eq("list node prev (+0x40) == 0", *(unsigned int *)(p + 0x40), 0);
		check_eq("list owner (+0x48) == 0", *(unsigned int *)(p + 0x48), 0);
		unsigned int wantAssignInfo = (unsigned int)(unsigned long)STGVJSAssignInfo;
		check_eq("+0x54 == &STGVJSAssignInfo (truncated)", *(unsigned int *)(p + 0x54), wantAssignInfo);
		check_eq("+0x58 == &STGVJSAssignInfo (truncated)", *(unsigned int *)(p + 0x58), wantAssignInfo);
		check_eq("+0x5c == &STGVJSAssignInfo (truncated)", *(unsigned int *)(p + 0x5c), wantAssignInfo);
		check_eq("+0x60 == &STGVJSAssignInfo (truncated)", *(unsigned int *)(p + 0x60), wantAssignInfo);
		check_eq("+0x66 == 0x8000 (centered default)", *(unsigned short *)(p + 0x66), 0x8000);
		check_eq("+0x68 == 0x8000", *(unsigned short *)(p + 0x68), 0x8000);
		check_eq("+0x6a == 0x8000", *(unsigned short *)(p + 0x6a), 0x8000);
		check_eq("+0x6c == 0x8000", *(unsigned short *)(p + 0x6c), 0x8000);
		check_eq("+0x4c == 0", *(unsigned int *)(p + 0x4c), 0);
		check_eq("vtable ptr nonzero (real symbol+8)", *(unsigned long *)p != 0, 1);

		/* sec 10.227: exercise both confirmed CSTGVectorEGCC::Init()
		 * branches (index != 0x10 resets the four STGVJSAssignInfo
		 * pointers; index == 0x10 leaves them alone). The confirmed
		 * real "+0x4 index" field is only safely distinct from the
		 * vtable pointer on the 32-bit TARGET (4-byte vtable ptr); on a
		 * 64-bit host build the vtable pointer is 8 bytes and +0x4
		 * fully aliases it, so writing +0x4 directly would corrupt a
		 * SUBSEQUENT virtual dispatch's vtable read. Call Init()
		 * qualified (non-virtual) here instead -- the real vtable-
		 * slot-0 dispatch mechanism itself is already exercised
		 * end-to-end for all three classes (including this one, 17
		 * real dispatches) by test_vector_manager_init.cpp; this block
		 * is specifically about CSTGVectorEGCC::Init()'s OWN body
		 * logic. */
		*(unsigned short *)(p + 0x4) = 5; /* index != 0x10 */
		*(unsigned int *)(p + 0x4c) = 0xffffffff;
		obj->CSTGVectorEGCC::Init();
		check_eq("Init() index!=0x10: +0x4c cleared", *(unsigned int *)(p + 0x4c), 0);
		check_eq("Init() index!=0x10: +0x54 reset to 0", *(unsigned int *)(p + 0x54), 0);
		check_eq("Init() index!=0x10: +0x58 reset to 0", *(unsigned int *)(p + 0x58), 0);
		check_eq("Init() index!=0x10: +0x5c reset to 0", *(unsigned int *)(p + 0x5c), 0);
		check_eq("Init() index!=0x10: +0x60 reset to 0", *(unsigned int *)(p + 0x60), 0);

		delete obj;
	}
	{
		CSTGVectorEGCC *obj = new CSTGVectorEGCC();
		unsigned char *p = reinterpret_cast<unsigned char *>(obj);
		unsigned int wantAssignInfo = (unsigned int)(unsigned long)STGVJSAssignInfo;

		*(unsigned short *)(p + 0x4) = 0x10; /* index == 0x10, the one EGCC per batch this skips */
		*(unsigned int *)(p + 0x4c) = 0xffffffff;
		obj->CSTGVectorEGCC::Init(); /* qualified, non-virtual -- see note above */
		check_eq("Init() index==0x10: +0x4c cleared regardless", *(unsigned int *)(p + 0x4c), 0);
		check_eq("Init() index==0x10: +0x54 left untouched (still &STGVJSAssignInfo)",
			 *(unsigned int *)(p + 0x54), wantAssignInfo);
		check_eq("Init() index==0x10: +0x60 left untouched (still &STGVJSAssignInfo)",
			 *(unsigned int *)(p + 0x60), wantAssignInfo);

		delete obj;
	}

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
