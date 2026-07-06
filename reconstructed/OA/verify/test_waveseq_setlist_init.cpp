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

static int g_slot7Calls;
static void Slot7Trap(void *) { g_slot7Calls++; }

/* Installs a fake vtable (slot 7 = Slot7Trap) at every sub-object's own
 * +0x0 across `count` entries of `stride` bytes starting at `base`.
 * The vtable storage itself must survive 32-bit truncation too (a
 * plain `static` array's address isn't guaranteed to on a PIE host --
 * see oa_global.h's own g_programSlotVtable comment, sec 10.81), so
 * it's allocated via mmap32 as well, leaked deliberately (test-only). */
static void installFakeVtables(unsigned char *base, unsigned int count, unsigned int stride)
{
	void **vt = (void **)mmap32(8 * sizeof(void *));
	for (int i = 0; i < 8; i++)
		vt[i] = 0;
	vt[7] = (void *)Slot7Trap;
	unsigned int packed = (unsigned int)(unsigned long)vt;
	for (unsigned int i = 0; i < count; i++)
		*(unsigned int *)(base + i * stride) = packed;
}

int main(void)
{
	printf("CSTGWaveSeqData/CSetListBank::Initialize() known-answer test\n");
	printf("=========================================================\n");

	printf("[1] CSetListBank::Initialize -- 128 sub-objects, stride 0x834, each dispatched once\n");
	{
		unsigned long size = 0x80ul * 0x834ul + 0x10;
		unsigned char *buf = (unsigned char *)mmap32(size);
		memset(buf, 0, size);
		installFakeVtables(buf, 0x80, 0x834);
		g_slot7Calls = 0;

		CSetListBank *b = (CSetListBank *)buf;
		b->Initialize();

		check_eq("dispatched exactly 128 times", (unsigned int)g_slot7Calls, 0x80);
		munmap(buf, size);
	}

	printf("[2] CSTGWaveSeqData::Initialize -- 598 sub-objects, stride 0xd14, each dispatched once\n");
	{
		unsigned long size = 0x256ul * 0xd14ul + 0x10;
		unsigned char *buf = (unsigned char *)mmap32(size);
		memset(buf, 0, size);
		installFakeVtables(buf, 0x256, 0xd14);
		g_slot7Calls = 0;

		CSTGWaveSeqData *d = (CSTGWaveSeqData *)buf;
		d->Initialize();

		check_eq("dispatched exactly 598 times", (unsigned int)g_slot7Calls, 0x256);
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
