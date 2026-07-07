// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_daemons.cpp  -  host-side known-answer test for
 * signal_timed_out_daemons() (src/init/stg_daemons.cpp, sec 10.183).
 *
 * Links only src/init/stg_daemons.cpp. Mocks its two dependencies:
 *   - GetSTGTickCount(): returns a SCRIPTED tick sequence so the test
 *     controls "now" (first call) independently of the fresh tick each
 *     kick re-reads to re-stamp lastTick (subsequent calls) -- this is
 *     why GetSTGTickCount is a separate TU (real body + its own KAT in
 *     test_tick_count.cpp), so it can be mocked here.
 *   - rt_pend_linux_srq(): records every srq value pended, in order.
 * Drives gStgDaemons directly (exported via oa_daemons.h).
 */

#include <cstdio>
#include <cstring>
#include "oa_daemons.h"

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) { printf("  ok    %-54s 0x%lx\n", label, got); return; }
	printf("  FAIL  %-54s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

/* ---- scripted GetSTGTickCount mock ---- */
static unsigned int g_tickSeq[64];
static int g_tickIdx;
static int g_tickCalls;
extern "C" unsigned int GetSTGTickCount(void)
{
	unsigned int v = g_tickSeq[g_tickIdx];
	if (g_tickIdx < 63) g_tickIdx++;
	g_tickCalls++;
	return v;
}

/* ---- rt_pend_linux_srq recording mock ---- */
static unsigned int g_pended[32];
static int g_pendCount;
extern "C" void rt_pend_linux_srq(unsigned int srq)
{
	if (g_pendCount < 32) g_pended[g_pendCount] = srq;
	g_pendCount++;
}

/* Reset all mock state; script the tick sequence: call 0 -> `now`, all
 * later calls -> `resetTick` (a distinct value so a reset lastTick is
 * observable). */
static void reset(unsigned int now, unsigned int resetTick)
{
	memset(gStgDaemons, 0, sizeof(gStgDaemons));
	g_tickSeq[0] = now;
	for (int i = 1; i < 64; i++) g_tickSeq[i] = resetTick;
	g_tickIdx = 0; g_tickCalls = 0;
	g_pendCount = 0; memset(g_pended, 0, sizeof(g_pended));
}

int main(void)
{
	printf("signal_timed_out_daemons watchdog known-answer test\n");
	printf("===================================================\n");

	printf("[1] all 7 daemons within timeout -> no kicks, no lastTick change\n");
	reset(/*now=*/1000, /*resetTick=*/5000);
	for (int i = 0; i < STG_DAEMON_COUNT; i++) {
		gStgDaemons[i].lastTick = 990;   /* elapsed 10 */
		gStgDaemons[i].timeout  = 100;
		gStgDaemons[i].srq      = 0x100 + i;
	}
	signal_timed_out_daemons();
	check_eq("no srq pended", g_pendCount, 0);
	check_eq("GetSTGTickCount called exactly once (only `now`)", g_tickCalls, 1);
	check_eq("daemon[0].lastTick unchanged", gStgDaemons[0].lastTick, 990);
	check_eq("daemon[6].lastTick unchanged", gStgDaemons[6].lastTick, 990);

	printf("[2] exactly one daemon (index 3) timed out -> one kick\n");
	reset(1000, 5000);
	for (int i = 0; i < STG_DAEMON_COUNT; i++) {
		gStgDaemons[i].lastTick = 990;   /* elapsed 10 */
		gStgDaemons[i].timeout  = 100;
		gStgDaemons[i].srq      = 0x100 + i;
	}
	gStgDaemons[3].lastTick = 800;           /* elapsed 200 > 100 */
	signal_timed_out_daemons();
	check_eq("exactly one srq pended", g_pendCount, 1);
	check_eq("...it was daemon[3].srq", g_pended[0], 0x103);
	check_eq("daemon[3].lastTick re-stamped to the fresh tick", gStgDaemons[3].lastTick, 5000);
	check_eq("neighbour daemon[2].lastTick untouched", gStgDaemons[2].lastTick, 990);
	check_eq("neighbour daemon[4].lastTick untouched", gStgDaemons[4].lastTick, 990);

	printf("[3] boundary: elapsed==timeout does NOT fire; ==timeout+1 does (strict >)\n");
	reset(1000, 5000);
	gStgDaemons[0].lastTick = 900; gStgDaemons[0].timeout = 100; gStgDaemons[0].srq = 0xA0; /* elapsed 100 == timeout */
	gStgDaemons[1].lastTick = 899; gStgDaemons[1].timeout = 100; gStgDaemons[1].srq = 0xA1; /* elapsed 101 > timeout */
	for (int i = 2; i < STG_DAEMON_COUNT; i++) { gStgDaemons[i].lastTick = 1000; gStgDaemons[i].timeout = 100; }
	signal_timed_out_daemons();
	check_eq("only the strictly-over daemon fired", g_pendCount, 1);
	check_eq("...it was daemon[1] (elapsed 101)", g_pended[0], 0xA1);
	check_eq("daemon[0] (elapsed==timeout) did NOT reset", gStgDaemons[0].lastTick, 900);

	printf("[4] all 7 timed out (zero-init blocks, now>0) -> 7 kicks in order\n");
	reset(1000, 5000);
	for (int i = 0; i < STG_DAEMON_COUNT; i++)   /* lastTick=0, timeout=0 -> elapsed 1000 > 0 */
		gStgDaemons[i].srq = 0x200 + i;
	signal_timed_out_daemons();
	check_eq("all 7 daemons kicked", g_pendCount, 7);
	int order_ok = 1;
	for (int i = 0; i < 7; i++) if (g_pended[i] != (unsigned int)(0x200 + i)) order_ok = 0;
	check_eq("kicks issued in daemon-index order 0..6", order_ok, 1);
	check_eq("every lastTick re-stamped to fresh tick", gStgDaemons[6].lastTick, 5000);

	printf("[5] wrapping tick arithmetic: recent-across-wrap no fire, old-across-wrap fires\n");
	reset(/*now=*/50, 5000);
	gStgDaemons[0].lastTick = 40;          gStgDaemons[0].timeout = 100; gStgDaemons[0].srq = 0xB0; /* elapsed 10 */
	gStgDaemons[1].lastTick = 0xFFFFFFF0u; gStgDaemons[1].timeout = 100; gStgDaemons[1].srq = 0xB1; /* (u32)(50-(-16))=66 <=100 */
	gStgDaemons[2].lastTick = 0xFFFFFF00u; gStgDaemons[2].timeout = 100; gStgDaemons[2].srq = 0xB2; /* (u32)(50-...) =306 >100 */
	for (int i = 3; i < STG_DAEMON_COUNT; i++) { gStgDaemons[i].lastTick = 50; gStgDaemons[i].timeout = 100; }
	signal_timed_out_daemons();
	check_eq("only the genuinely-old-across-wrap daemon fired", g_pendCount, 1);
	check_eq("...it was daemon[2]", g_pended[0], 0xB2);
	check_eq("recent-across-wrap daemon[1] did NOT fire", gStgDaemons[1].lastTick, 0xFFFFFFF0u);

	printf("\n%s (%d failed checks)\n",
	       g_fail ? "SOME CHECKS FAILED" : "all checks passed", g_fail);
	return g_fail ? 1 : 0;
}
