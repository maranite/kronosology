// SPDX-License-Identifier: GPL-2.0
/*
 * test_waveseq_setlist_init.cpp  -  host-side known-answer test for
 * CSTGWaveSeqData::Initialize()/CSetListBank::Initialize() (sec 10.84),
 * plus (batch 12) CSTGWaveSequence::CSTGWaveSequence()/CSetList::CSetList().
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

/* sec 10.229: CSTGWaveSeqData::Initialize()/CSetListBank::Initialize()
 * no longer do a raw vtable[7] fetch off each sub-object's own +0x0 --
 * both real vtables (`_ZTV16CSTGWaveSequence`/`_ZTV8CSetList`) resolve
 * that exact slot to the SAME real method, `CSTGParamsOwner::
 * UseDefaults()` (confirmed via objdump -r on OA_real.ko, identical
 * shape to CSTGGlobal's own sec 10.228 fix), so the real code now calls
 * it directly via `reinterpret_cast<CSTGParamsOwner*>`. The old
 * installFakeVtables()/Slot7Trap scheme poked a fake vtable pointer into
 * each sub-object that the real code path no longer even reads -- it
 * would silently stop exercising anything real, exactly the masking
 * hazard sec 10.228 found in test_global.cpp's own scenario [15].
 * Replaced with a real, call-counting mock of UseDefaults() itself
 * (this test binary links waveseq_setlist_init.cpp alone, so it must
 * supply this symbol -- production's real no-op lives in
 * bar2_stubs.cpp, not linked here). */
static int g_useDefaultsCalls;
void CSTGParamsOwner::UseDefaults() { g_useDefaultsCalls++; }

int main(void)
{
	printf("CSTGWaveSeqData/CSetListBank::Initialize() known-answer test\n");
	printf("=========================================================\n");

	printf("[1] CSetListBank::Initialize -- 128 sub-objects, stride 0x834, each dispatched once\n");
	{
		unsigned long size = 0x80ul * 0x834ul + 0x10;
		unsigned char *buf = (unsigned char *)mmap32(size);
		memset(buf, 0, size);
		g_useDefaultsCalls = 0;

		CSetListBank *b = (CSetListBank *)buf;
		b->Initialize();

		check_eq("UseDefaults() dispatched exactly 128 times", (unsigned int)g_useDefaultsCalls, 0x80);
		munmap(buf, size);
	}

	printf("[2] CSTGWaveSeqData::Initialize -- 598 sub-objects, stride 0xd14, each dispatched once\n");
	{
		unsigned long size = 0x256ul * 0xd14ul + 0x10;
		unsigned char *buf = (unsigned char *)mmap32(size);
		memset(buf, 0, size);
		g_useDefaultsCalls = 0;

		CSTGWaveSeqData *d = (CSTGWaveSeqData *)buf;
		d->Initialize();

		check_eq("UseDefaults() dispatched exactly 598 times", (unsigned int)g_useDefaultsCalls, 0x256);
		munmap(buf, size);
	}

	printf("[3] CSTGWaveSequence::CSTGWaveSequence()/CSetList::CSetList() --\n"
	       "    vtable-install-only (batch 12): confirmed real via the\n"
	       "    INLINED instructions at CSTGGlobal::CSTGGlobal()'s own\n"
	       "    two array-construction loops (neither ctor has its own\n"
	       "    standalone symbol in OA_real.ko)\n");
	{
		unsigned char waveSeqBuf[64];
		memset(waveSeqBuf, 0xCC, sizeof(waveSeqBuf));
		new (waveSeqBuf) CSTGWaveSequence();
		check_eq("CSTGWaveSequence vtable ptr == &_ZTV16CSTGWaveSequence+8",
			 *(unsigned int *)waveSeqBuf,
			 (unsigned int)(unsigned long)(_ZTV16CSTGWaveSequence + 8));
		check_eq("CSTGWaveSequence byte +4 untouched (poison)", waveSeqBuf[4], 0xCC);

		unsigned char setListBuf[64];
		memset(setListBuf, 0xCC, sizeof(setListBuf));
		new (setListBuf) CSetList();
		check_eq("CSetList vtable ptr == &_ZTV8CSetList+8",
			 *(unsigned int *)setListBuf,
			 (unsigned int)(unsigned long)(_ZTV8CSetList + 8));
		check_eq("CSetList byte +4 untouched (poison)", setListBuf[4], 0xCC);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
