// SPDX-License-Identifier: GPL-2.0
/*
 * test_rtfifo_init.cpp  -  host-side known-answer test for
 * stg_rtfifo_init() (see ../include/oa_rtfifo_init.h /
 * ../src/init/rtfifo_init.cpp).
 *
 * Mocks rtf_create/rtf_destroy/__register_chrdev/__unregister_chrdev/
 * rt_printk, asserting the exact confirmed minor/size sequence (0,1,3,
 * 4,5,7 with sizes 0x400/0x400/0x400/0x8000/0x10000/0x400), the char
 * device registration args ("stg_direct", major 0x98), the real
 * bail-on-first-failure behavior, and (now that stg_rtfifo_cleanup()
 * itself is reconstructed, see src/init/rtfifo_init.cpp) that cleanup
 * only destroys the minors actually marked live in the confirmed
 * bitmask, not a blanket destroy-all.
 */

#include <cstdio>
#include "oa_rtfifo_init.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-50s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

extern "C" {

static unsigned int g_createdMinors[8];
static int g_createdSizes[8];
static int g_createCalls;
static unsigned int g_destroyedMinors[16];
static int g_destroyCalls;
static int g_failAtCall = -1; /* which rtf_create call (1-based) fails, -1 = never */

int rtf_create(unsigned int fifo, int size)
{
	g_createCalls++;
	g_createdMinors[g_createCalls - 1] = fifo;
	g_createdSizes[g_createCalls - 1] = size;
	return (g_createCalls == g_failAtCall) ? -1 : 0;
}

int rtf_destroy(unsigned int fifo)
{
	g_destroyedMinors[g_destroyCalls] = fifo;
	g_destroyCalls++;
	return 0;
}

static unsigned int g_chrdevMajor, g_chrdevBaseminor, g_chrdevCount;
static char g_chrdevName[64];
static const void *g_chrdevFops;
static int g_chrdevCalls;
static int g_chrdevShouldFail;
int __register_chrdev(unsigned int major, unsigned int baseminor,
		       unsigned int count, const char *name, const void *fops)
{
	g_chrdevCalls++;
	g_chrdevMajor = major;
	g_chrdevBaseminor = baseminor;
	g_chrdevCount = count;
	unsigned int i = 0;
	while (name[i] && i < sizeof(g_chrdevName) - 1) { g_chrdevName[i] = name[i]; i++; }
	g_chrdevName[i] = 0;
	g_chrdevFops = fops;
	return g_chrdevShouldFail ? -1 : 0;
}

static unsigned int g_unregMajor, g_unregBaseminor, g_unregCount;
static char g_unregName[64];
static int g_unregCalls;
int __unregister_chrdev(unsigned int major, unsigned int baseminor,
			 unsigned int count, const char *name)
{
	g_unregCalls++;
	g_unregMajor = major;
	g_unregBaseminor = baseminor;
	g_unregCount = count;
	unsigned int i = 0;
	while (name[i] && i < sizeof(g_unregName) - 1) { g_unregName[i] = name[i]; i++; }
	g_unregName[i] = 0;
	return 0;
}

static int g_printkCalls;
void rt_printk(const char *, ...) { g_printkCalls++; }

} /* extern "C" */

/* Real stg_rtfifo_cleanup() only destroys minors 0/1/3/4/5/7 whose
 * bitmask bit was actually set -- we count how many of THOSE our_fifo_
 * setup() calls actually got far enough to mark live (i.e. how many
 * rtf_create() calls succeeded before the failure, if any). */
static int g_cleanupCalls; /* alias: how many times this test invoked cleanup itself */

static void reset(void)
{
	g_createCalls = g_destroyCalls = g_chrdevCalls = g_cleanupCalls = 0;
	g_unregCalls = g_printkCalls = 0;
	g_failAtCall = -1;
	g_chrdevShouldFail = 0;
	for (int i = 0; i < 8; i++) {
		g_createdMinors[i] = 0;
		g_createdSizes[i] = 0;
	}
	for (int i = 0; i < 16; i++)
		g_destroyedMinors[i] = 0;
	stg_rtfifo_test_reset_mask();
}

int main(void)
{
	printf("[1] full success path:\n");
	reset();
	int rc = stg_rtfifo_init();
	check_eq("return value (success)", rc, 0);
	check_eq("rtf_create called 6 times", g_createCalls, 6);
	check_eq("rtf_destroy called 6 times (once per minor, before create)", g_destroyCalls, 6);
	static const unsigned int wantMinors[6] = { 0, 1, 3, 4, 5, 7 };
	static const int wantSizes[6] = { 0x400, 0x400, 0x400, 0x8000, 0x10000, 0x400 };
	for (int i = 0; i < 6; i++) {
		char label[64];
		snprintf(label, sizeof(label), "create[%d] minor", i);
		check_eq(label, g_createdMinors[i], wantMinors[i]);
		snprintf(label, sizeof(label), "create[%d] size", i);
		check_eq(label, g_createdSizes[i], wantSizes[i]);
		snprintf(label, sizeof(label), "destroy[%d] minor (matches create order)", i);
		check_eq(label, g_destroyedMinors[i], wantMinors[i]);
	}
	check_eq("__register_chrdev called once", g_chrdevCalls, 1);
	check_eq("chrdev major == 0x98", g_chrdevMajor, 0x98);
	check_eq("chrdev baseminor == 0", g_chrdevBaseminor, 0);
	check_eq("chrdev count == 0x100", g_chrdevCount, 0x100);
	check_eq("chrdev name == \"stg_direct\"", (long)(g_chrdevName[0] == 's' &&
		 g_chrdevName[9] == 't' && g_chrdevName[10] == '\0'), 1);
	check_eq("chrdev fops non-null", g_chrdevFops != 0, 1);
	check_eq("__unregister_chrdev NOT called on success (cleanup not invoked)", g_unregCalls, 0);

	printf("\n[2] third FIFO (minor 3) fails -> bails immediately, real cleanup runs:\n");
	reset();
	g_failAtCall = 3;
	rc = stg_rtfifo_init();
	check_eq("return value (failure)", rc, -1);
	check_eq("rtf_create called exactly 3 times (stopped at the failure)", g_createCalls, 3);
	check_eq("__register_chrdev NOT called", g_chrdevCalls, 0);
	/* Real stg_rtfifo_cleanup() only tears down minors ACTUALLY marked
	 * live (0 and 1 succeeded before minor 3's create failed) -- 3
	 * initial destroy-before-create calls (minors 0,1,3) plus 2 more
	 * from cleanup itself (minors 0,1; minor 3 never got marked live
	 * since its own rtf_create failed). */
	check_eq("rtf_destroy called 5 times total (3 initial + 2 from real cleanup)",
		 g_destroyCalls, 5);
	check_eq("cleanup's own destroy targets are minors 0 and 1",
		 (long)(g_destroyedMinors[3] == 0 && g_destroyedMinors[4] == 1), 1);
	check_eq("__unregister_chrdev called once (real cleanup always runs it)", g_unregCalls, 1);
	check_eq("unregister chrdev major == 0x98", g_unregMajor, 0x98);
	check_eq("unregister chrdev name == \"stg_direct\"", (long)(g_unregName[0] == 's' &&
		 g_unregName[9] == 't' && g_unregName[10] == '\0'), 1);
	check_eq("rt_printk called 3 times (real cleanup's diagnostic dumps)", g_printkCalls, 3);

	printf("\n[3] all 6 FIFOs succeed but char device registration fails:\n");
	reset();
	g_chrdevShouldFail = 1;
	rc = stg_rtfifo_init();
	check_eq("return value (failure)", rc, -1);
	check_eq("rtf_create called all 6 times", g_createCalls, 6);
	check_eq("__register_chrdev called once", g_chrdevCalls, 1);
	/* All 6 minors were marked live -> real cleanup destroys all 6
	 * again, on top of the 6 initial destroy-before-create calls. */
	check_eq("rtf_destroy called 12 times total (6 initial + 6 from real cleanup)",
		 g_destroyCalls, 12);
	check_eq("__unregister_chrdev called once", g_unregCalls, 1);

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
