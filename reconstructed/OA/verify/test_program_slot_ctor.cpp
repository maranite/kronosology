// SPDX-License-Identifier: GPL-2.0
/*
 * test_program_slot_ctor.cpp  -  host-side known-answer test for
 * CSTGToneAdjust::CSTGToneAdjust() and CSTGProgramSlot::CSTGProgramSlot()
 * (sec 10.153, see src/engine/program_slot_ctor.cpp).
 *
 * Poisons the object's memory with a non-zero pattern before
 * construction, so any field the ctor is SUPPOSED to leave untouched
 * would show up as still-poisoned rather than accidentally reading as
 * zero and passing by coincidence (same discipline as test_managers.cpp).
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

int main(void)
{
	printf("CSTGProgramSlot/CSTGToneAdjust ctor known-answer test\n");
	printf("======================================================\n");

	unsigned char buf[0x100];
	memset(buf, 0xcc, sizeof(buf));
	buf[0x43] = 0xff; /* so the confirmed "clear bit1" read-modify-write is observable */
	buf[0x45] = 0x0c; /* so the confirmed "set bit7" read-modify-write is observable */
	CSTGProgramSlot *slot = new (buf) CSTGProgramSlot();
	unsigned char *base = (unsigned char *)slot;

	printf("\n[1] CSTGProgramSlot's own confirmed zeroed byte fields\n");
	static const unsigned int zeroBytes[] = {
		0x9, 0xa, 0xd, 0x11, 0x12, 0x13, 0x26, 0x27, 0x30, 0x47,
		0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65,
		0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
		0x10, 0xb, 0x4,
	};
	for (unsigned int off : zeroBytes) {
		char label[32];
		snprintf(label, sizeof(label), "+0x%x == 0", off);
		check_eq(label, base[off], 0);
	}

	printf("\n[2] CSTGProgramSlot's own confirmed dword/float fields\n");
	check_eq("+0x5..+0x8 dword == 0", *(unsigned int *)(base + 0x5), 0);
	check_eq("+0x6f dword == 0", *(unsigned int *)(base + 0x6f), 0);
	check_eq("+0x73 float == 1.0f", *(unsigned int *)(base + 0x73), 0x3f800000);
	check_eq("+0x77 float == 1.0f", *(unsigned int *)(base + 0x77), 0x3f800000);
	check_eq("+0x7b float == 1.0f", *(unsigned int *)(base + 0x7b), 0x3f800000);
	check_eq("+0x31 float == 1.0f", *(unsigned int *)(base + 0x31), 0x3f800000);
	check_eq("+0x39 dword == 0", *(unsigned int *)(base + 0x39), 0);
	check_eq("+0x35 dword == 0x3c010204", *(unsigned int *)(base + 0x35), 0x3c010204);
	check_eq("+0x43 == 0xfd (0xff with bit1 cleared, other bits preserved)", base[0x43], 0xfd);
	check_eq("+0x45 == 0x8c (0x0c with bit7 set, other bits preserved)", base[0x45], 0x8c);

	printf("\n[3] Real gap: +0x25..+0x2f, +0x48..+0x5b untouched (still poisoned)\n");
	check_eq("+0x28 still 0xcc (untouched gap)", base[0x28], 0xcc);
	check_eq("+0x50 still 0xcc (untouched gap)", base[0x50], 0xcc);

	printf("\n[4] Embedded CSTGToneAdjust at +0x7f\n");
	unsigned char *ta = base + 0x7f;
	for (unsigned int off = 0x4; off <= 0x24; off++) {
		char label[32];
		snprintf(label, sizeof(label), "ToneAdjust +0x%x == 0", off);
		check_eq(label, ta[off], 0);
	}
	for (unsigned int off = 0x45; off <= 0x67; off += 2) {
		char label[48];
		snprintf(label, sizeof(label), "ToneAdjust +0x%x word == 0", off);
		check_eq(label, *(unsigned short *)(ta + off), 0);
	}
	check_eq("ToneAdjust +0x30 still 0xcc (untouched gap)", ta[0x30], 0xcc);

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nAll checks passed.\n");
	return 0;
}
