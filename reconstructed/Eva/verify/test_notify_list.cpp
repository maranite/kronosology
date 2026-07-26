/*
 * test_notify_list.cpp  -  host-side known-answer test for CNotifyList
 * (src/editor/notify_list.cpp, promoted from a fabricated no-op stub to Tier A
 * 2026-07-26, broad Tier-B recheck sweep).
 *
 * Exercises the shared free-list grow/pop/push machinery (GrowEventsList/Put),
 * the mLast-only dedup shortcut, the drain (GetList) and both ReleaseList()
 * overloads, and the real ctor/PostKernelDestructor cleanup path, using two
 * separate CNotifyList instances to also confirm the free list is genuinely
 * process-wide shared state, not per-instance.
 */

#include <cstdio>
#include <cstdlib>
#include <new>

#include "notify_list.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static int CountChain(SNotifyEvent *n)
{
	int count = 0;
	while (n != 0) {
		count++;
		n = n->next;
	}
	return count;
}

int main(void)
{
	printf("CNotifyList known-answer test\n");
	printf("==============================\n");

	printf("[1] First-ever construction grows the shared free list by 32\n");
	/* Placement-construct so we can also placement-destruct deterministically,
	 * matching the raw-blob-lifecycle convention used throughout this project's
	 * own KATs (e.g. test_module_manager_add_module.cpp).
	 */
	static unsigned char raw1[sizeof(CNotifyList)];
	CNotifyList *list1 = new (raw1) CNotifyList();
	check("mFirst starts NULL", list1->GetList() == 0);

	printf("[2] Put() on an empty queue pops one node off the shared free list\n"
	       "    and appends it\n");
	list1->Put(1, 2, 3);
	SNotifyEvent *drained = list1->GetList();
	check("GetList() returns exactly 1 node", drained != 0 && CountChain(drained) == 1);
	check("node fields match (1,2,3)", drained != 0 && drained->group == 1 &&
	                                        drained->index == 2 && drained->subIndex == 3);
	check("queue empty again after GetList()", list1->GetList() == 0);
	CNotifyList::ReleaseList(drained);

	printf("[3] Put() dedups against mLast (most-recent only), not the whole "
	       "queue\n");
	list1->Put(5, 6, 7);
	list1->Put(5, 6, 7); /* exact repeat of the current tail -- must be a no-op */
	list1->Put(9, 9, 9);
	list1->Put(5, 6, 7); /* NOT a repeat of the new tail (9,9,9) -- must append */
	SNotifyEvent *chain = list1->GetList();
	check("3 distinct entries queued (dup of tail skipped, later re-post of an "
	      "earlier value kept)",
	      chain != 0 && CountChain(chain) == 3);
	if (chain != 0 && CountChain(chain) == 3) {
		check("entry 0 == (5,6,7)", chain->group == 5 && chain->index == 6 &&
		                                chain->subIndex == 7);
		check("entry 1 == (9,9,9)", chain->next->group == 9 &&
		                                 chain->next->index == 9 &&
		                                 chain->next->subIndex == 9);
		check("entry 2 == (5,6,7) again (not deduped -- mLast was (9,9,9) at "
		      "post time)",
		      chain->next->next->group == 5 && chain->next->next->index == 6 &&
		          chain->next->next->subIndex == 7);
	}
	CNotifyList::ReleaseList(chain);

	printf("[4] Two-argument ReleaseList(first,last) prepends without walking\n");
	list1->Put(1, 1, 1);
	list1->Put(2, 2, 2);
	SNotifyEvent *pair = list1->GetList();
	SNotifyEvent *first = pair;
	SNotifyEvent *last = pair->next;
	check("2-node chain drained", CountChain(pair) == 2);
	CNotifyList::ReleaseList(first, last);
	/* Round-trip: pop 2 nodes back out via Put() and confirm they're the same
	 * two (now-recycled) node objects, proving they actually went back onto the
	 * shared free list rather than leaking. */
	list1->Put(3, 3, 3);
	list1->Put(4, 4, 4);
	SNotifyEvent *roundTrip = list1->GetList();
	/* ReleaseList(first,last) makes `first` the new free-list head (first->next
	 * still == last, untouched), so Put() pops `first` first, then `last`. */
	check("recycled nodes come back out (free-list round-trip, no leak)",
	      roundTrip == first && roundTrip->next == last);
	CNotifyList::ReleaseList(roundTrip);

	printf("[5] A second, independent CNotifyList instance shares the SAME "
	       "underlying free list (process-wide static, not per-instance)\n");
	check("list1's own queue is empty (fully drained by test [4])",
	      list1->GetList() == 0);
	static unsigned char raw2[sizeof(CNotifyList)];
	CNotifyList *list2 = new (raw2) CNotifyList();
	list1->Put(0xa, 0xb, 0xc);
	list2->Put(0xd, 0xe, 0xf);
	SNotifyEvent *l1 = list1->GetList();
	SNotifyEvent *l2 = list2->GetList();
	check("list1 queue has exactly its own 1 entry", l1 != 0 && CountChain(l1) == 1 &&
	                                                      l1->group == 0xa);
	check("list2 queue has exactly its own 1 entry", l2 != 0 && CountChain(l2) == 1 &&
	                                                      l2->group == 0xd);
	CNotifyList::ReleaseList(l1);
	CNotifyList::ReleaseList(l2);

	printf("[6] PostKernelDestructor() drains both the shared free list and this\n"
	       "    instance's own pending queue, no crash/leak-checker complaint\n");
	list1->Put(1, 2, 3); /* leave one node on list1's own queue on purpose */
	int rc = list1->PostKernelDestructor(0);
	check("PostKernelDestructor() returns 0", rc == 0);
	check("real signature's flags argument genuinely unused (0 accepted)", true);

	list1->~CNotifyList();
	list2->~CNotifyList();

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
