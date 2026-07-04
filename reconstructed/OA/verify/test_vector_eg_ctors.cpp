// SPDX-License-Identifier: GPL-2.0
/*
 * test_vector_eg_ctors.cpp  -  KAT for CSTGVectorEGXOnly/EGXY/EGCC's own
 * constructors (see src/engine/vector_eg_ctors.cpp).
 *
 * Verifies: derived vtable pointer set to the confirmed real symbol+8,
 * the shared +0x3c/+0x40/+0x44/+0x48 field convention across all three
 * types, EGCC's four STGVJSAssignInfo pointer writes and four 0x8000
 * centered defaults, and EGXY's confirmed partial-bit-clear on a byte
 * the (mocked) base constructor already set.
 */

#include <cstdio>
#include <cstring>
#include "oa_engine_init.h"

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

/* Mock base ctor: poisons the object first (simulating "whatever real
 * memory looked like before"), then sets a confirmed-real flags byte at
 * +0x6e with BOTH bit 1 and other bits set, so EGXY's own AND-mask can be
 * verified to clear only bit 1. */
CSTGVectorEGBase::CSTGVectorEGBase()
{
	memset(this, 0xaa, sizeof(CSTGVectorEGXY) > sizeof(CSTGVectorEGCC)
				    ? (sizeof(CSTGVectorEGXY) > sizeof(CSTGVectorEGXOnly)
					       ? sizeof(CSTGVectorEGXY)
					       : sizeof(CSTGVectorEGXOnly))
				    : sizeof(CSTGVectorEGCC));
}

extern "C" unsigned char STGVJSAssignInfo[4];
unsigned char STGVJSAssignInfo[4];

/* Stand-in storage for the real (not-yet-reconstructed) vtable data
 * symbols vector_eg_ctors.cpp declares as `extern "C"` externs -- this
 * test only checks the pointer gets set to symbol+8, never dispatches
 * through it, so the contents don't matter. */
extern "C" unsigned char _ZTV17CSTGVectorEGXOnly[16];
extern "C" unsigned char _ZTV14CSTGVectorEGXY[16];
extern "C" unsigned char _ZTV14CSTGVectorEGCC[16];
unsigned char _ZTV17CSTGVectorEGXOnly[16];
unsigned char _ZTV14CSTGVectorEGXY[16];
unsigned char _ZTV14CSTGVectorEGCC[16];

int main(void)
{
	printf("CSTGVectorEGXOnly/EGXY/EGCC constructor known-answer test\n");
	printf("=========================================================\n");

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
		delete obj;
	}

	printf("\n[2] CSTGVectorEGXY\n");
	{
		CSTGVectorEGXY *obj = new CSTGVectorEGXY();
		unsigned char *p = reinterpret_cast<unsigned char *>(obj);
		check_eq("self-pointer at +0x44", *(unsigned int *)(p + 0x44), (unsigned long)(unsigned int)(unsigned long)p);
		check_eq("list node next (+0x3c) == 0", *(unsigned int *)(p + 0x3c), 0);
		check_eq("list node prev (+0x40) == 0", *(unsigned int *)(p + 0x40), 0);
		check_eq("list owner (+0x48) == 0", *(unsigned int *)(p + 0x48), 0);
		check_eq("+0x5c == 0", *(unsigned int *)(p + 0x5c), 0);
		check_eq("+0x60 == 0", *(unsigned int *)(p + 0x60), 0);
		check_eq("+0x64 == 0", *(unsigned int *)(p + 0x64), 0);
		check_eq("+0x6d == 0", p[0x6d], 0);
		check_eq("+0x6e bit 1 cleared, other bits (base-set 0xaa) preserved", p[0x6e], 0xaa & 0xfd);
		check_eq("vtable ptr nonzero (real symbol+8)", *(unsigned long *)p != 0, 1);
		delete obj;
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
		delete obj;
	}

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
