// SPDX-License-Identifier: GPL-2.0
/*
 * test_sampling_interface_ctor.cpp  -  host-side known-answer test for
 * CSTGSamplingInterface::CSTGSamplingInterface() (sec 10.160, see
 * src/engine/sampling_interface_ctor.cpp).
 *
 * Poisons the object's memory with a non-zero pattern before
 * construction, so any field the ctor is SUPPOSED to leave untouched
 * would show up as still-poisoned rather than accidentally reading as
 * zero and passing by coincidence (same discipline as test_managers.cpp/
 * test_program_slot_ctor.cpp). +0x31f is poked to 0xff specifically
 * (rather than relying on the blanket 0xcc poison) since it's the ONE
 * field this ctor only ANDs (`&= 0xfe`, no OR) -- 0xcc's own bit0 is
 * already 0, which would hide a dropped AND instruction (sec 10.153's
 * own "poison-pattern discipline" gotcha).
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

extern "C" unsigned char _ZTV21CSTGSamplingInterface[0x60];

int main(void)
{
	printf("CSTGSamplingInterface ctor known-answer test\n");
	printf("=============================================\n");

	static unsigned char buf[sizeof(CSTGSamplingInterface)];
	memset(buf, 0xcc, sizeof(buf));
	buf[0x31f] = 0xff; /* so the confirmed AND-only "clear bit0" RMW is observable */

	CSTGSamplingInterface *iface = new (buf) CSTGSamplingInterface();
	unsigned char *base = (unsigned char *)iface;

	printf("\n[1] vtable pointer install\n");
	check_eq("+0x0 == &_ZTV21CSTGSamplingInterface+8",
		 *(unsigned int *)base,
		 (unsigned int)(unsigned long)(_ZTV21CSTGSamplingInterface + 8));

	printf("\n[2] confirmed read-modify-write byte fields: (orig|1)&0xe1\n");
	check_eq("+0x55 == 0xc1 (0xcc poisoned: (0xcc|1)&0xe1)", base[0x55], 0xc1);
	check_eq("+0x150 == 0xc1", base[0x150], 0xc1);
	check_eq("+0x24b == 0xc1", base[0x24b], 0xc1);

	printf("\n[3] confirmed AND-only byte field: +0x31f &= 0xfe\n");
	check_eq("+0x31f == 0xfe (0xff with bit0 cleared)", base[0x31f], 0xfe);

	printf("\n[4] confirmed zeroed dword/word/byte fields (representative sample)\n");
	static const unsigned int zeroDwords[] = {
		0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c,
		0x40, 0x44, 0x48, 0x4c, 0x50,
		0x113, 0x117, 0x11b, 0x11f, 0x123, 0x127, 0x12b, 0x12f,
		0x133, 0x137, 0x13b, 0x13f, 0x143, 0x147, 0x14b,
		0x20e, 0x212, 0x216, 0x21a, 0x21e, 0x222, 0x226, 0x22a,
		0x22e, 0x232, 0x236, 0x23a, 0x23e, 0x242, 0x246,
		0x30d, 0x338, 0x32c, 0x334, 0x330,
		0x45c, 0x454, 0x464, 0x460,
	};
	for (unsigned int off : zeroDwords) {
		char label[32];
		snprintf(label, sizeof(label), "+0x%x dword == 0", off);
		check_eq(label, *(unsigned int *)(base + off), 0);
	}
	static const unsigned int zeroBytes[] = {
		0x54, 0x14f, 0x24a, 0x313, 0x315, 0x316, 0x317, 0x318, 0x319,
		0x31a, 0x31b, 0x31c, 0x31d, 0x31e, 0x43c, 0x43d, 0x323, 0x324,
		0x33c, 0x441, 0x320, 0x4, 0x444, 0x446, 0x468, 0x442, 0x56c,
	};
	for (unsigned int off : zeroBytes) {
		char label[32];
		snprintf(label, sizeof(label), "+0x%x byte == 0", off);
		check_eq(label, base[off], 0);
	}
	static const unsigned int zeroWords[] = {
		0x56, 0x151, 0x24c, 0x30b, 0x326, 0x328, 0x568,
	};
	for (unsigned int off : zeroWords) {
		char label[32];
		snprintf(label, sizeof(label), "+0x%x word == 0", off);
		check_eq(label, *(unsigned short *)(base + off), 0);
	}

	printf("\n[5] confirmed non-zero default fields\n");
	check_eq("+0xa0 dword == 0xffffffff", *(unsigned int *)(base + 0xa0), 0xffffffff);
	check_eq("+0x19b dword == 0xffffffff", *(unsigned int *)(base + 0x19b), 0xffffffff);
	check_eq("+0x296 dword == 0xffffffff", *(unsigned int *)(base + 0x296), 0xffffffff);
	check_eq("+0x309 word == 0xffff", *(unsigned short *)(base + 0x309), 0xffff);
	check_eq("+0x311 byte == 0x3c", base[0x311], 0x3c);
	check_eq("+0x312 byte == 0x3c", base[0x312], 0x3c);
	check_eq("+0x314 byte == 0x40", base[0x314], 0x40);
	check_eq("+0x322 byte == 0xff", base[0x322], 0xff);
	check_eq("+0x445 byte == 0xff", base[0x445], 0xff);
	check_eq("+0x44c word == 0xffff", *(unsigned short *)(base + 0x44c), 0xffff);
	check_eq("+0x448 dword == 0xbb80 (48000 sample rate, matches CSTGAudioEvent's own)",
		 *(unsigned int *)(base + 0x448), 0xbb80);
	check_eq("+0x43e byte == 1", base[0x43e], 1);
	check_eq("+0x56a word == 0x24b", *(unsigned short *)(base + 0x56a), 0x24b);
	check_eq("+0x56d byte == 2", base[0x56d], 2);

	printf("\n[6] sInstance\n");
	check_eq("CSTGSamplingInterface::sInstance == iface",
		 (unsigned long)CSTGSamplingInterface::sInstance, (unsigned long)iface);

	printf("\n[7] real gap: an untouched byte between confirmed fields stays poisoned\n");
	check_eq("+0x5f still 0xcc (untouched gap)", base[0x5f], 0xcc);
	check_eq("+0x200 still 0xcc (untouched gap)", base[0x200], 0xcc);

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nAll checks passed.\n");
	return 0;
}
