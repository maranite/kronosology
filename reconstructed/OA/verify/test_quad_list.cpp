// SPDX-License-Identifier: GPL-2.0
/*
 * test_quad_list.cpp  -  host-side known-answer tests for CSTGQuad's
 * sorted-list primitives (Stage 2, see include/oa_quad.h).
 *
 * As with CSTGBankMemory, there's no third-party reference for this
 * Korg-internal structure -- vectors here are worked out by hand from the
 * confirmed algorithm (ascending-priority sort, ties insert-before) and
 * checked by walking the resulting list and asserting its exact order,
 * rather than just trusting round-trip self-consistency.
 */

#include <cstdio>
#include <cstring>
#include "oa_types.h"
#include "oa_quad.h"

static int g_fail;

static void check_true(const char *label, bool cond)
{
	if (cond) {
		printf("  ok    %s\n", label);
		return;
	}
	printf("  FAIL  %s\n", label);
	g_fail++;
}

static void check_eq_u(const char *label, unsigned int got, unsigned int want)
{
	if (got == want) {
		printf("  ok    %-40s %u\n", label, got);
		return;
	}
	printf("  FAIL  %-40s got=%u want=%u\n", label, got, want);
	g_fail++;
}

/* Walk bucket->head via mNext and return the number of nodes visited,
 * writing their priorities into `order`. Also cross-checks the mPrev chain
 * and bucket->tail match a correct doubly-linked list. */
static unsigned int walk_and_check(struct CSTGQuadList *bucket, unsigned short *order, unsigned int max)
{
	unsigned int n = 0;
	struct CSTGQuad *cur = bucket->head;
	struct CSTGQuad *prev = 0;

	while (cur && n < max) {
		if (cur->mPrev != prev) {
			printf("  FAIL  mPrev chain broken at index %u\n", n);
			g_fail++;
		}
		order[n++] = cur->mPriority;
		prev = cur;
		cur = cur->mNext;
	}
	if (bucket->tail != prev) {
		printf("  FAIL  bucket->tail does not match the last node walked\n");
		g_fail++;
	}
	return n;
}

int main(void)
{
	printf("CSTGQuad list-primitive known-answer test\n");
	printf("==========================================\n");

	struct CSTGQuadList buckets[2] = {};
	struct CSTGVoiceModel vm = {};
	vm.quadBuckets = buckets;
	vm.cachedQuadPriority = 0xffff;

	struct CSTGQuad q[6] = {};
	q[0].mPriority = 50; q[0].mBucketIndex = 0;
	q[1].mPriority = 10; q[1].mBucketIndex = 0;
	q[2].mPriority = 30; q[2].mBucketIndex = 0;
	q[3].mPriority = 30; q[3].mBucketIndex = 0;	/* tie with q[2] */
	q[4].mPriority = 90; q[4].mBucketIndex = 0;
	q[5].mPriority = 5;  q[5].mBucketIndex = 1;	/* different bucket entirely */

	printf("[1] Insert 50, 10, 30, 30(tie), 90 into bucket 0 in that order\n");
	vm.AddQuad(&q[0]);
	vm.AddQuad(&q[1]);
	vm.AddQuad(&q[2]);
	vm.AddQuad(&q[3]);
	vm.AddQuad(&q[4]);

	unsigned short order[8];
	unsigned int n = walk_and_check(&buckets[0], order, 8);
	check_eq_u("bucket 0 node count", n, 5);
	check_eq_u("bucket0.count field", buckets[0].count, 5);
	/* Expected ascending order: 10, 30(q[3], inserted-before-tie), 30(q[2]), 50, 90 --
	 * ties insert the NEW quad before the existing equal-priority one, so
	 * inserting q[3](30) after q[2](30) already present places q[3] first
	 * of the pair. */
	static const unsigned short expected[5] = { 10, 30, 30, 50, 90 };
	bool orderOk = (n == 5);
	for (unsigned int i = 0; orderOk && i < 5; i++)
		if (order[i] != expected[i])
			orderOk = false;
	check_true("ascending order with tie-break confirmed (10,30,30,50,90)", orderOk);
	check_true("tie insertion: q[3] (second-inserted 30) precedes q[2]",
		   buckets[0].head->mNext->mNext == &q[2] && buckets[0].head->mNext == &q[3]);
	check_eq_u("cachedQuadPriority after last AddQuad", vm.cachedQuadPriority, 90);

	/*
	 * NOTE: cachedQuadPriority lives on CSTGVoiceModel (+0xd8), not per
	 * bucket -- every AddQuad call on `vm`, regardless of which bucket it
	 * targets, overwrites it. So the bucket-1 independence check is done
	 * LAST, after every assertion that depends on the cache's value from
	 * the bucket-0 insertions above (an earlier draft of this test got
	 * this wrong by checking bucket independence in between, silently
	 * invalidating the cache assertions that followed it).
	 */
	printf("[2] RemoveQuad the current head (10) -- bucket->head must advance\n");
	vm.RemoveQuad(&q[1]);
	check_true("q[1] fully unlinked", q[1].mNext == 0 && q[1].mPrev == 0 && q[1].mOwnerList == 0);
	check_true("bucket 0 new head is q[3] (30)", buckets[0].head == &q[3]);
	check_eq_u("bucket 0 count after remove", buckets[0].count, 4);
	check_eq_u("cachedQuadPriority unaffected (was 90, removed 10)", vm.cachedQuadPriority, 90);

	printf("[3] RemoveQuad the current tail (90) -- bucket->tail must retreat, cache invalidates\n");
	vm.RemoveQuad(&q[4]);
	check_true("bucket 0 new tail is q[0] (50)", buckets[0].tail == &q[0]);
	check_eq_u("bucket 0 count after second remove", buckets[0].count, 3);
	check_eq_u("cachedQuadPriority reset to 0xffff (matched removed quad's priority)",
		   vm.cachedQuadPriority, 0xffff);

	printf("[4] RemoveQuad on an already-unlinked quad is a silent no-op\n");
	unsigned int countBefore = buckets[0].count;
	vm.RemoveQuad(&q[1]);	/* already removed in step 2 */
	check_eq_u("bucket 0 count unchanged", buckets[0].count, countBefore);

	printf("[5] Insert into a separate bucket (bucket 1) is independent\n");
	vm.AddQuad(&q[5]);
	check_eq_u("bucket 1 node count", buckets[1].count, 1);
	check_true("bucket 1 head is q[5]", buckets[1].head == &q[5]);
	check_eq_u("bucket 0 untouched by bucket-1 insert", buckets[0].count, 3);

	printf("==========================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
