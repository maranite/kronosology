// SPDX-License-Identifier: GPL-2.0
/*
 * test_sequence_ctor.cpp  -  host-side known-answer test for
 * CSTGSequence::CSTGSequence() (sec 10.153, see
 * src/engine/sequence_ctor.cpp).
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

/* CSTGCombi::CSTGCombi() lives elsewhere (bar2_stubs.cpp, real build --
 * still a deliberately-deferred no-op stub, sec 10.13) -- mocked here
 * purely so CSTGSequence's own (real) base-ctor call links on the host,
 * same treatment test_managers.cpp gives other not-yet-reconstructed
 * base-class dependencies. */
static int g_combiCtorCalls;
CSTGCombi::CSTGCombi() { g_combiCtorCalls++; }

int main(void)
{
	printf("CSTGSequence::CSTGSequence() known-answer test\n");
	printf("================================================\n");

	static unsigned char buf[0x2000];
	memset(buf, 0xcc, sizeof(buf));
	CSTGSequence *seq = new (buf) CSTGSequence();
	unsigned char *base = (unsigned char *)seq;

	printf("\n[1] Base ctor + own vtable\n");
	check_eq("CSTGCombi base ctor called once", g_combiCtorCalls, 1);
	check_eq("own vtable ptr installed", *(unsigned int *)base,
		 (unsigned int)(unsigned long)(_ZTV12CSTGSequence + 8));

	printf("\n[2] 16 embedded CSTGHDRTrack sub-objects (0x2c stride)\n");
	for (unsigned int i = 0; i < 16; i++) {
		unsigned char *track = base + 0x19e7 + i * 0x2c;
		char label[64];
		snprintf(label, sizeof(label), "track[%u] vtable ptr", i);
		check_eq(label, *(unsigned int *)track,
			 (unsigned int)(unsigned long)(_ZTV12CSTGHDRTrack + 8));
		snprintf(label, sizeof(label), "track[%u] +0x4 == 0", i);
		check_eq(label, track[0x4], 0);
		snprintf(label, sizeof(label), "track[%u] +0x5 == 0", i);
		check_eq(label, track[0x5], 0);
		snprintf(label, sizeof(label), "track[%u] +0x6 == 0", i);
		check_eq(label, track[0x6], 0);
		/* Real confirmed gap -- rest of the 44-byte slot untouched. */
		check_eq("track[.] +0x7 still poisoned (untouched gap)", track[0x7], 0xcc);
	}

	printf("\n[3] Final CSTGMetronomeSettings slot (same stride, different vtable)\n");
	unsigned char *metronome = base + 0x1ca7;
	check_eq("metronome vtable ptr", *(unsigned int *)metronome,
		 (unsigned int)(unsigned long)(_ZTV21CSTGMetronomeSettings + 8));
	check_eq("metronome +0x4 == 0", metronome[0x4], 0);
	check_eq("metronome +0x5 still poisoned (only +0x4 zeroed, unlike CSTGHDRTrack)",
		 metronome[0x5], 0xcc);

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nAll checks passed.\n");
	return 0;
}
