// SPDX-License-Identifier: GPL-2.0
/*
 * test_slot_voice_data_midi_filters.cpp  -  host-side known-answer test
 * for CSTGSlotVoiceData::UpdateAllActiveMIDIFilters() (batch 16, sec
 * 10.163).
 *
 * Standalone TU: only links slot_voice_data_midi_filters.cpp, so it
 * provides its own local storage for CSTGGlobal::sInstance and its own
 * mock for CSTGSlotVoiceData::UpdateMIDIFilterAndResendAllCCs() (the
 * function's OWN real body -- the deliberately-deferred no-op given in
 * the same source file -- is intentionally NOT what's exercised here;
 * this test verifies the dispatch/skip LOGIC around it, matching this
 * project's usual "confirm the real body's own not-yet-reconstructed
 * sibling gets called with the right receiver, not what that sibling
 * itself does" pattern).
 *
 * `CSTGGlobal::sInstance` uses this project's own established
 * `calloc(1, 0x29c9fc0)` convention (test_engine.cpp et al) rather than
 * `mmap32()` -- it's a plain native host pointer, dereferenced directly
 * (never packed to/reconstituted from 32 bits itself), so it doesn't
 * need the sub-4GB guarantee; the large size is cheap in practice
 * (demand-paged/zero-fill, not actually resident until touched).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
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

CSTGGlobal *CSTGGlobal::sInstance;

static int g_umfCalls;
static void *g_lastUmfThis;
void CSTGSlotVoiceData::UpdateMIDIFilterAndResendAllCCs()
{
	g_umfCalls++;
	g_lastUmfThis = this;
}

int main(void)
{
	printf("CSTGSlotVoiceData::UpdateAllActiveMIDIFilters known-answer test\n");
	printf("=================================================================\n");

	size_t globalSize = 0x29c9fc0;
	unsigned char *globalBuf = (unsigned char *)calloc(1, globalSize);
	CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;
	unsigned char *tableBuf = globalBuf + 0x29c990c;

	/* Two candidate payload objects, real mmap32-backed (dereferenced
	 * unconditionally via the confirmed real "node non-null implies
	 * payload non-null" invariant -- see file header comment/gotcha). */
	unsigned char *node0 = (unsigned char *)mmap32(0x100);
	unsigned char *payload0 = (unsigned char *)mmap32(0x100);
	memset(node0, 0, 0x100);
	memset(payload0, 0xcc, 0x100);
	*(unsigned int *)(node0 + 8) = (unsigned int)(unsigned long)payload0;

	unsigned char *node1 = (unsigned char *)mmap32(0x100);
	unsigned char *payload1 = (unsigned char *)mmap32(0x100);
	memset(node1, 0, 0x100);
	memset(payload1, 0xcc, 0x100);
	*(unsigned int *)(node1 + 8) = (unsigned int)(unsigned long)payload1;

	printf("[1] all 16 slots null -> zero calls\n");
	{
		g_umfCalls = 0;
		CSTGSlotVoiceData::UpdateAllActiveMIDIFilters();
		check_eq("zero calls", (unsigned int)g_umfCalls, 0);
	}

	printf("[2] slot 3 active, payload+0x40==0 -> called once, on payload0\n");
	{
		*(unsigned int *)(tableBuf + 3 * 12) = (unsigned int)(unsigned long)node0;
		payload0[0x40] = 0;
		g_umfCalls = 0;
		g_lastUmfThis = 0;
		CSTGSlotVoiceData::UpdateAllActiveMIDIFilters();
		check_eq("called once", (unsigned int)g_umfCalls, 1);
		check_eq("called on payload0", (unsigned int)(g_lastUmfThis == payload0), 1);
	}

	printf("[3] slot 3 active but payload+0x40!=0 -> skipped\n");
	{
		payload0[0x40] = 1;
		g_umfCalls = 0;
		CSTGSlotVoiceData::UpdateAllActiveMIDIFilters();
		check_eq("zero calls (muted)", (unsigned int)g_umfCalls, 0);
	}

	printf("[4] slots 3 and 11 both active, both payload+0x40==0 -> called twice\n");
	{
		payload0[0x40] = 0;
		*(unsigned int *)(tableBuf + 11 * 12) = (unsigned int)(unsigned long)node1;
		payload1[0x40] = 0;
		g_umfCalls = 0;
		CSTGSlotVoiceData::UpdateAllActiveMIDIFilters();
		check_eq("called twice", (unsigned int)g_umfCalls, 2);
	}

	printf("[5] slot 0 and slot 15 (table boundary entries) also honored\n");
	{
		memset(tableBuf, 0, 16 * 12);	/* clear the whole 16-entry table */
		*(unsigned int *)(tableBuf + 0 * 12) = (unsigned int)(unsigned long)node0;
		payload0[0x40] = 0;
		*(unsigned int *)(tableBuf + 15 * 12) = (unsigned int)(unsigned long)node1;
		payload1[0x40] = 0;
		g_umfCalls = 0;
		CSTGSlotVoiceData::UpdateAllActiveMIDIFilters();
		check_eq("called twice (slot 0 + slot 15)", (unsigned int)g_umfCalls, 2);
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return g_fail ? 1 : 0;
}
