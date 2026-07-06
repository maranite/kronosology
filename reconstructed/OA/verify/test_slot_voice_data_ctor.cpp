// SPDX-License-Identifier: GPL-2.0
/*
 * test_slot_voice_data_ctor.cpp  -  host-side known-answer test for
 * CSTGSlotVoiceData::CSTGSlotVoiceData(), CSTGMidiCCFilter::Initialize(),
 * and CSTGHeldKeyList::CSTGHeldKeyList() (sec 10.155, see
 * src/engine/slot_voice_data_ctor.cpp).
 *
 * Poisons the object's memory with a non-zero pattern before
 * construction (same discipline as test_program_slot_ctor.cpp/
 * test_controller_rt_data_ctor.cpp) and uses mmap32() (MAP_32BIT) for
 * every buffer whose address gets truncated into a packed 32-bit field,
 * matching the established host-KAT convention for round-tripping
 * truncated heap pointers (test_smoother_init.cpp etc).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_bank_memory.h"
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

static unsigned char *mmap32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

/* rtwrap_* mocks, matching test_managers.cpp's own established convention. */
static int g_mutexInitCalls;
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" void *rtwrap_malloc(unsigned int size) { return mmap32(size < 4096 ? 4096 : size); }
extern "C" void rtwrap_pthread_mutex_init(void *, void *) { g_mutexInitCalls++; }

static void test_midi_cc_filter(void)
{
	printf("\n=== CSTGMidiCCFilter::Initialize() ===\n");
	CSTGMidiCCFilter filter;
	memset(&filter, 0, sizeof(filter));
	filter.Initialize();

	/* Confirmed real: ORs in bits 0..119 (0x78), NOT the full 128. */
	for (unsigned int cc = 0; cc < 120; cc++) {
		char label[32];
		snprintf(label, sizeof(label), "bit %u set", cc);
		check_eq(label, (filter.bits[cc >> 5] >> (cc & 0x1f)) & 1, 1);
	}
	for (unsigned int cc = 120; cc < 128; cc++) {
		char label[32];
		snprintf(label, sizeof(label), "bit %u NOT set (beyond 0x78 bound)", cc);
		check_eq(label, (filter.bits[cc >> 5] >> (cc & 0x1f)) & 1, 0);
	}
}

static void test_held_key_list(void)
{
	printf("\n=== CSTGHeldKeyList::CSTGHeldKeyList() ===\n");
	unsigned char buf[0xa10];
	memset(buf, 0xcc, sizeof(buf));
	CSTGHeldKeyList *list = new (buf) CSTGHeldKeyList();
	unsigned char *base = (unsigned char *)list;

	for (unsigned int i = 0; i < 128; i++) {
		unsigned char *node = base + i * 0x14;
		char label[48];
		snprintf(label, sizeof(label), "node[%u] next(+0x0) == 0", i);
		check_eq(label, *(unsigned int *)(node + 0x0), 0);
		snprintf(label, sizeof(label), "node[%u] prev(+0x4) == 0", i);
		check_eq(label, *(unsigned int *)(node + 0x4), 0);
		snprintf(label, sizeof(label), "node[%u] +0x8 == 0", i);
		check_eq(label, *(unsigned int *)(node + 0x8), 0);
		snprintf(label, sizeof(label), "node[%u] +0xc == &node (self-pointer)", i);
		check_eq(label, *(unsigned int *)(node + 0xc), ToU32(node));
		snprintf(label, sizeof(label), "node[%u] +0x10 == 0", i);
		check_eq(label, *(unsigned int *)(node + 0x10), 0);
	}
	check_eq("head(+0xa00) == 0", *(unsigned int *)(base + 0xa00), 0);
	check_eq("tail(+0xa04) == 0", *(unsigned int *)(base + 0xa04), 0);
	check_eq("count(+0xa08) == 0", *(unsigned int *)(base + 0xa08), 0);
}

int main(void)
{
	printf("CSTGSlotVoiceData ctor known-answer test\n");
	printf("=========================================\n");

	test_midi_cc_filter();
	test_held_key_list();

	printf("\n=== CSTGSlotVoiceData::CSTGSlotVoiceData() ===\n");
	CSTGBankMemory::Initialize(mmap32(0x40000), 0x40000);

	const unsigned long objSize = 0x2900;
	unsigned char *buf = mmap32(objSize);
	memset(buf, 0xcc, objSize);
	/* 0xcc's own bits 0-1 are already {0,0} -- poking each entry's +0xb
	 * byte to 0xff first makes the confirmed "(orig|1)&~2" RMW's bit1-
	 * clearing half actually observable, not a false-negative-proof
	 * check (the exact poison-pattern gotcha already on record, sec
	 * 10.153). */
	for (unsigned int i = 0; i < 121; i++)
		buf[0x1488 + i * 0xc + 0xb] = 0xff;
	int mutexCallsBefore = g_mutexInitCalls;
	CSTGSlotVoiceData *svd = new (buf) CSTGSlotVoiceData();
	unsigned char *base = (unsigned char *)svd;

	printf("\n[1] owner back-pointers (+0xc/+0x1c/+0x2c == this)\n");
	check_eq("+0xc == this", *(unsigned int *)(base + 0xc), ToU32(base));
	check_eq("+0x1c == this", *(unsigned int *)(base + 0x1c), ToU32(base));
	check_eq("+0x2c == this", *(unsigned int *)(base + 0x2c), ToU32(base));

	printf("\n[2] confirmed zeroed dword fields\n");
	static const unsigned int zeroDwords[] = {
		0x4, 0x8, 0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x30,
		0x48, 0x44, 0x4c, 0x54, 0x50, 0x58, 0x60, 0x5c, 0x64,
	};
	for (unsigned int off : zeroDwords) {
		char label[32];
		snprintf(label, sizeof(label), "+0x%x dword == 0", off);
		check_eq(label, *(unsigned int *)(base + off), 0);
	}

	printf("\n[3] mutex setup (2 calls total)\n");
	check_eq("rtwrap_pthread_mutex_init called twice",
		 (unsigned long)(g_mutexInitCalls - mutexCallsBefore), 2);
	check_eq("+0x1468 mutex1 ptr != 0", *(unsigned int *)(base + 0x1468) != 0, 1);
	check_eq("+0x146c mutex2 ptr != 0", *(unsigned int *)(base + 0x146c) != 0, 1);
	check_eq("mutex1 != mutex2",
		 *(unsigned int *)(base + 0x1468) != *(unsigned int *)(base + 0x146c), 1);

	printf("\n[4] +0x1470/+0x1474\n");
	check_eq("+0x1470 dword == 0", *(unsigned int *)(base + 0x1470), 0);
	check_eq("+0x1474 dword == 0xffffffff", *(unsigned int *)(base + 0x1474), 0xffffffff);

	printf("\n[5] two 0x6c00-byte AllocAligned buffers\n");
	unsigned int bufAoff = *(unsigned int *)(base + 0x1478);
	unsigned int bufBoff = *(unsigned int *)(base + 0x147c);
	check_eq("+0x1478 buffer ptr != 0", bufAoff != 0, 1);
	check_eq("+0x147c buffer ptr != 0", bufBoff != 0, 1);
	check_eq("bufA != bufB", bufAoff != bufBoff, 1);

	printf("\n[6] 121-entry x 12-byte voice-slot array at +0x1488\n");
	for (unsigned int i = 0; i < 121; i++) {
		unsigned char *e = base + 0x1488 + i * 0xc;
		char label[48];
		snprintf(label, sizeof(label), "entry[%u] +0x0 == 0", i);
		check_eq(label, *(unsigned int *)(e + 0x0), 0);
		snprintf(label, sizeof(label), "entry[%u] +0x4 == 0", i);
		check_eq(label, *(unsigned int *)(e + 0x4), 0);
		snprintf(label, sizeof(label), "entry[%u] +0x8 word == 0", i);
		check_eq(label, *(unsigned short *)(e + 0x8), 0);
		snprintf(label, sizeof(label), "entry[%u] +0xa == 1", i);
		check_eq(label, e[0xa], 1);
		snprintf(label, sizeof(label), "entry[%u] +0xb == 0xfd (0xff with bit1 cleared)", i);
		check_eq(label, e[0xb], 0xfd);
	}
	/* +0x1a34 is the first byte past the 121-entry array -- confirmed
	 * real untouched (still poisoned). */
	check_eq("+0x1a34 still 0xcc (untouched, past the 121-entry array)",
		 base[0x1a34], 0xcc);

	printf("\n[7] embedded CSTGMidiCCFilter at +0x1db4 (bits 0..119 set)\n");
	CSTGMidiCCFilter *filt = (CSTGMidiCCFilter *)(base + 0x1db4);
	for (unsigned int cc = 0; cc < 120; cc++) {
		char label[32];
		snprintf(label, sizeof(label), "MidiCCFilter bit %u set", cc);
		check_eq(label, (filt->bits[cc >> 5] >> (cc & 0x1f)) & 1, 1);
	}

	printf("\n[8] confirmed gap fields before the embedded CSTGHeldKeyList\n");
	check_eq("+0x1e14 dword == 0", *(unsigned int *)(base + 0x1e14), 0);
	check_eq("+0x1e18 dword == 0", *(unsigned int *)(base + 0x1e18), 0);
	check_eq("+0x1e1c dword == 0", *(unsigned int *)(base + 0x1e1c), 0);
	check_eq("+0x1e7c byte == 0", base[0x1e7c], 0);
	check_eq("+0x1e20 still 0xcc (confirmed real untouched gap)", base[0x1e20], 0xcc);
	check_eq("+0x1e3b still 0xcc (confirmed real untouched gap)", base[0x1e3b], 0xcc);
	for (unsigned int off = 0x1e3c; off <= 0x1e78; off += 4) {
		char label[32];
		snprintf(label, sizeof(label), "+0x%x dword == 0", off);
		check_eq(label, *(unsigned int *)(base + off), 0);
	}

	printf("\n[9] embedded CSTGHeldKeyList at +0x1e80 (spot-check)\n");
	unsigned char *hkl = base + 0x1e80;
	check_eq("HeldKeyList node[0] +0xc == &node (self-pointer)",
		 *(unsigned int *)(hkl + 0xc), ToU32(hkl));
	check_eq("HeldKeyList head(+0xa00) == 0", *(unsigned int *)(hkl + 0xa00), 0);
	check_eq("HeldKeyList count(+0xa08) == 0", *(unsigned int *)(hkl + 0xa08), 0);

	printf("\n[10] fields after the embedded CSTGHeldKeyList ends (+0x288c)\n");
	check_eq("+0x28c8 byte == 0", base[0x28c8], 0);
	check_eq("+0x28dc byte == 0", base[0x28dc], 0);
	check_eq("+0x28dd byte == 0", base[0x28dd], 0);
	check_eq("+0x28de byte == 0", base[0x28de], 0);
	check_eq("+0x28df byte == 0", base[0x28df], 0);

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nAll checks passed.\n");
	return 0;
}
