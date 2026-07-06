// SPDX-License-Identifier: GPL-2.0
/*
 * test_controller_rt_data_ctor.cpp  -  host-side known-answer test for
 * CSTGControllerRTData::CSTGControllerRTData() (sec 10.155, see
 * src/engine/controller_rt_data_ctor.cpp).
 *
 * Poisons the object's memory with a non-zero pattern before
 * construction (same discipline as test_program_slot_ctor.cpp), and
 * specifically pre-sets the two AND-masked bitfields (+0x21/+0x2f) to
 * values where the masked bits start SET -- 0xcc's own bit pattern
 * already has bits 0-1 clear, which would make the +0x21 &= 0xfc check
 * a false-negative-proof test if left at the blanket poison value (the
 * exact gotcha already on record from sec 10.153's own KAT discipline
 * note).
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
	printf("CSTGControllerRTData ctor known-answer test\n");
	printf("============================================\n");

	unsigned char buf[0x100];
	memset(buf, 0xcc, sizeof(buf));
	buf[0x21] = 0xff; /* so the confirmed "&= 0xfc" RMW is observable */
	buf[0x2f] = 0xff; /* so the confirmed "&= 0xf0" RMW is observable */
	buf[0x49] = 0x00; /* so the confirmed "|= 0x01" RMW is observable */

	CSTGControllerRTData::sInstance = 0;
	CSTGControllerRTData *rt = new (buf) CSTGControllerRTData();
	unsigned char *base = (unsigned char *)rt;

	printf("\n[1] sInstance self-registration\n");
	check_eq("sInstance == this", (unsigned long)CSTGControllerRTData::sInstance,
		 (unsigned long)rt);

	printf("\n[2] confirmed zeroed byte fields\n");
	static const unsigned int zeroBytes[] = {
		0x0, 0x1, 0x2, 0x3, 0xc, 0xd, 0xe, 0xf, 0x10, 0x14, 0x15, 0x16,
		0x1c, 0x1d, 0x1e, 0x1f, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
		0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	};
	for (unsigned int off : zeroBytes) {
		char label[32];
		snprintf(label, sizeof(label), "+0x%x == 0", off);
		check_eq(label, base[off], 0);
	}

	printf("\n[3] confirmed non-zero scalar fields\n");
	check_eq("+0x4 == 0x7f", base[0x4], 0x7f);
	check_eq("+0x5 == 0x01", base[0x5], 0x01);
	check_eq("+0x8 dword == 0", *(unsigned int *)(base + 0x8), 0);
	check_eq("+0x12 word == 0x2000", *(unsigned short *)(base + 0x12), 0x2000);
	check_eq("+0x18 word == 0x200", *(unsigned short *)(base + 0x18), 0x200);
	check_eq("+0x1a word == 0x200", *(unsigned short *)(base + 0x1a), 0x200);
	check_eq("+0x20 == 0x40", base[0x20], 0x40);
	check_eq("+0x2d == 0x02", base[0x2d], 0x02);
	check_eq("+0x2e == 0x06", base[0x2e], 0x06);
	check_eq("+0x22 word == 0", *(unsigned short *)(base + 0x22), 0);
	check_eq("+0x24 word == 0", *(unsigned short *)(base + 0x24), 0);
	check_eq("+0x30 dword == 0", *(unsigned int *)(base + 0x30), 0);
	check_eq("+0x34 dword == 0", *(unsigned int *)(base + 0x34), 0);
	check_eq("+0x38 dword == 0", *(unsigned int *)(base + 0x38), 0);
	check_eq("+0x3c dword == 0", *(unsigned int *)(base + 0x3c), 0);
	check_eq("+0x4c float == 1.0f", *(unsigned int *)(base + 0x4c), 0x3f800000);
	check_eq("+0x50 float == 0.5f", *(unsigned int *)(base + 0x50), 0x3f000000);

	printf("\n[4] confirmed real read-modify-write quirks\n");
	check_eq("+0x21 == 0xfc (0xff with bits0-1 cleared, rest preserved)",
		 base[0x21], 0xfc);
	check_eq("+0x2f == 0xf0 (0xff with low nibble cleared, rest preserved)",
		 base[0x2f], 0xf0);
	check_eq("+0x49 == 0x01 (0x00 with bit0 set)", base[0x49], 0x01);

	printf("\n[5] 17 confirmed {0xff,0xff,0x00} triples, +0x54..+0x86\n");
	for (int i = 0; i < 17; i++) {
		unsigned char *g = base + 0x54 + i * 3;
		char label[48];
		snprintf(label, sizeof(label), "triple[%d] == {ff,ff,00}", i);
		check_eq(label, (g[0] == 0xff && g[1] == 0xff && g[2] == 0x00) ? 1 : 0, 1);
	}

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nAll checks passed.\n");
	return 0;
}
