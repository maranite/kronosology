// SPDX-License-Identifier: GPL-2.0
/*
 * test_atmel_primitives.cpp  -  host-side known-answer test for
 * cm_GetRandomBytes()/cm_SetChallengeParams() (src/auth/
 * atmel_primitives.cpp, batch 38).
 *
 * Links src/auth/atmel_primitives.cpp directly. Mocks the one real
 * external it forwards to (get_random_bytes) and reads back the three
 * plain-C globals cm_SetChallengeParams caches into.
 */

#include <cstdio>
#include <cstring>

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

static int g_getRandomCalls;
static void *g_lastBuf;
static unsigned int g_lastLen;

extern "C" void get_random_bytes(void *buf, unsigned int len)
{
	g_getRandomCalls++;
	g_lastBuf = buf;
	g_lastLen = len;
	memset(buf, 0x42, len); /* fill so a real caller could observe it worked */
}

/* Plain C++ linkage (no extern "C"), matching oa_atmel.h's own
 * un-wrapped declarations. */
void cm_GetRandomBytes(unsigned char *buf, int len);
void cm_SetChallengeParams(const char *p, const char *q, const char *g);

extern "C" const char *gGpaChallengeP;
extern "C" const char *gGpaChallengeG;
extern "C" const char *gGpaChallengeQ;

int main()
{
	printf("[1] cm_GetRandomBytes forwards (buf, len) unchanged to get_random_bytes\n");
	unsigned char challenge[8];
	memset(challenge, 0, sizeof(challenge));
	cm_GetRandomBytes(challenge, 8);
	check_eq("get_random_bytes call count", g_getRandomCalls, 1);
	check_eq("buf forwarded unchanged", g_lastBuf == challenge, 1);
	check_eq("len forwarded unchanged", g_lastLen, 8);
	check_eq("buffer actually filled by the mock", challenge[0], 0x42);

	printf("[2] cm_SetChallengeParams caches p/q/g into their own globals\n");
	const char *p = "2758700829844358015";
	const char *q = "41504784743492877417";
	const char *g = "08285927170653268036";
	cm_SetChallengeParams(p, q, g);
	check_eq("gGpaChallengeP == p", gGpaChallengeP == p, 1);
	check_eq("gGpaChallengeQ == q", gGpaChallengeQ == q, 1);
	check_eq("gGpaChallengeG == g", gGpaChallengeG == g, 1);

	printf("[3] a second call with different pointers overwrites cleanly\n");
	const char *p2 = "x", *q2 = "y", *g2 = "z";
	cm_SetChallengeParams(p2, q2, g2);
	check_eq("gGpaChallengeP updated", gGpaChallengeP == p2, 1);
	check_eq("gGpaChallengeQ updated", gGpaChallengeQ == q2, 1);
	check_eq("gGpaChallengeG updated", gGpaChallengeG == g2, 1);

	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
