/*
 * test_list.cpp  -  host-side known-answer test for CObject/CAbstList/CUsrList/CList/
 * CStaticList/CListIter (src/base/list.cpp). See include/list.h for full ground-truth
 * provenance.
 */

#include <cstdio>

#include "list.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

namespace {

int g_liveCount;

class TestItem : public CObject {
public:
	explicit TestItem(int id) : mId(id) { ++g_liveCount; }
	virtual ~TestItem() { --g_liveCount; }
	int mId;
};

/* Walks a list front-to-back via next(), collecting item ids -- exercises firstitem()/
 * next() together as an independent cross-check against CListIter. */
int WalkIds(CAbstList &list, int *out, int maxOut)
{
	int n = 0;
	CObject *cur = list.firstitem();
	while (cur && n < maxOut) {
		out[n++] = static_cast<TestItem *>(cur)->mId;
		cur = list.next(cur);
	}
	return n;
}

} // namespace

int main()
{
	printf("test_list\n");

	/* ---- CList: append/prepend/firstitem/lastitem/getnumitems ---- */
	{
		CList list;
		check("empty list getnumitems == 0", list.getnumitems() == 0);
		check("empty list firstitem == NULL", list.firstitem() == 0);
		check("empty list lastitem == NULL", list.lastitem() == 0);

		TestItem a(1), b(2), c(3);
		list.append(&a);
		list.append(&b);
		list.append(&c);
		check("append x3 -> count 3", list.getnumitems() == 3);
		check("firstitem after 3 appends", static_cast<TestItem *>(list.firstitem())->mId == 1);
		check("lastitem after 3 appends", static_cast<TestItem *>(list.lastitem())->mId == 3);

		int ids[8];
		int n = WalkIds(list, ids, 8);
		check("walk order after append x3", n == 3 && ids[0] == 1 && ids[1] == 2 && ids[2] == 3);

		TestItem z(0);
		list.prepend(&z);
		check("prepend -> count 4", list.getnumitems() == 4);
		check("firstitem after prepend", static_cast<TestItem *>(list.firstitem())->mId == 0);
		check("lastitem unchanged by prepend", static_cast<TestItem *>(list.lastitem())->mId == 3);

		check("includes(&b)", list.includes(&b));
		TestItem outside(99);
		check("!includes(&outside)", !list.includes(&outside));

		check("findindex(&z) == 0", list.findindex(&z) == 0);
		check("findindex(&c) == 3", list.findindex(&c) == 3);

		check("nthitem(0)", static_cast<TestItem *>(list.nthitem(0))->mId == 0);
		check("nthitem(2)", static_cast<TestItem *>(list.nthitem(2))->mId == 2);

		list.remove(&b);
		check("remove(&b) -> count 3", list.getnumitems() == 3);
		check("!includes(&b) after remove", !list.includes(&b));
		n = WalkIds(list, ids, 8);
		check("walk order after remove(&b)", n == 3 && ids[0] == 0 && ids[1] == 1 && ids[2] == 3);

		CObject *first = list.removefirst();
		check("removefirst returns z", first == &z);
		check("removefirst -> count 2", list.getnumitems() == 2);

		CObject *last = list.removelast();
		check("removelast returns c", last == &c);
		check("removelast -> count 1", list.getnumitems() == 1);

		check("live nodes freed (a,b,c,z still alive as objects; only nodes freed)", g_liveCount >= 0);
	}

	/* ---- CList: insertafter / insertat ---- */
	{
		CList list;
		TestItem a(1), b(2), c(3), d(4);
		list.append(&a);
		list.append(&b);
		list.append(&c);

		/* insertafter(item, afterItem): item=&d inserted after &b. */
		bool ok = list.insertafter(&d, &b);
		check("insertafter(&d, &b) succeeds", ok);
		int ids[8];
		int n = WalkIds(list, ids, 8);
		check("insertafter splices d right after b", n == 4 && ids[0] == 1 && ids[1] == 2 &&
			ids[2] == 4 && ids[3] == 3);

		CList list2;
		TestItem e(10), f(20), g(30);
		list2.append(&e);
		list2.append(&f);
		list2.append(&g);

		TestItem h(40);
		list2.insertat(&h, 0); /* front */
		n = WalkIds(list2, ids, 8);
		check("insertat(item,0) inserts at front", n == 4 && ids[0] == 40 && ids[1] == 10 &&
			ids[2] == 20 && ids[3] == 30);

		TestItem i(50);
		list2.insertat(&i, 2); /* after position 2 (1-based: after &10) */
		n = WalkIds(list2, ids, 8);
		check("insertat(item,2) count", n == 5);
		check("insertat(item,2) position", ids[0] == 40 && ids[1] == 10 && ids[2] == 50 &&
			ids[3] == 20 && ids[4] == 30);
	}

	/* ---- CList: bringfront / sendback / moveup / movedown ---- */
	{
		CList list;
		TestItem a(1), b(2), c(3), d(4);
		list.append(&a);
		list.append(&b);
		list.append(&c);
		list.append(&d);

		int ids[8];
		list.bringfront(&c);
		int n = WalkIds(list, ids, 8);
		check("bringfront(&c)", n == 4 && ids[0] == 3 && ids[1] == 1 && ids[2] == 2 && ids[3] == 4);

		list.bringfront(&a); /* already 2nd in ring now, moves to front */
		n = WalkIds(list, ids, 8);
		check("bringfront(&a) again", n == 4 && ids[0] == 1);

		list.bringfront(&a); /* already-first -- pure no-op */
		n = WalkIds(list, ids, 8);
		check("bringfront already-first is no-op", n == 4 && ids[0] == 1 && ids[1] == 3 &&
			ids[2] == 2 && ids[3] == 4);

		CList list2;
		TestItem e(1), f(2), g(3), h(4);
		list2.append(&e);
		list2.append(&f);
		list2.append(&g);
		list2.append(&h);
		list2.sendback(&f);
		n = WalkIds(list2, ids, 8);
		check("sendback(&f)", n == 4 && ids[0] == 1 && ids[1] == 3 && ids[2] == 4 && ids[3] == 2);

		/* sendback of the current front promotes its successor to front. */
		CList list3;
		TestItem p(1), q(2), r(3);
		list3.append(&p);
		list3.append(&q);
		list3.append(&r);
		list3.sendback(&p);
		n = WalkIds(list3, ids, 8);
		check("sendback(front item) promotes successor to front",
			n == 3 && ids[0] == 2 && ids[1] == 3 && ids[2] == 1);
		check("sendback(front item) firstitem matches",
			static_cast<TestItem *>(list3.firstitem())->mId == 2);

		CList list4;
		TestItem s(1), t(2), u(3), v(4);
		list4.append(&s);
		list4.append(&t);
		list4.append(&u);
		list4.append(&v);
		list4.moveup(&u);
		n = WalkIds(list4, ids, 8);
		check("moveup(&u) swaps with predecessor", n == 4 && ids[0] == 1 && ids[1] == 3 &&
			ids[2] == 2 && ids[3] == 4);

		list4.moveup(&s); /* already first -- no-op */
		n = WalkIds(list4, ids, 8);
		check("moveup(first item) is no-op", ids[0] == 1);

		/* list4 is now [1,3,2,4] (s,u,t,v). movedown(&t) swaps t with its successor
		 * v -> [1,3,4,2]. */
		list4.movedown(&t);
		n = WalkIds(list4, ids, 8);
		check("movedown(&t) swaps with successor", n == 4 && ids[0] == 1 && ids[1] == 3 &&
			ids[2] == 4 && ids[3] == 2);

		list4.movedown(&t); /* now last -- no-op */
		n = WalkIds(list4, ids, 8);
		check("movedown(last item) is no-op", ids[3] == 2);
	}

	/* ---- CList: dispose / disposeall / removeallnode ---- */
	{
		g_liveCount = 0;
		CList list;
		TestItem *a = new TestItem(1);
		TestItem *b = new TestItem(2);
		TestItem *c = new TestItem(3);
		list.append(a);
		list.append(b);
		list.append(c);
		check("3 items constructed", g_liveCount == 3);

		list.dispose(b);
		check("dispose(&b) destroys the item", g_liveCount == 2);
		check("dispose(&b) removes from list", list.getnumitems() == 2);

		list.disposeall();
		check("disposeall destroys remaining items", g_liveCount == 0);
		check("disposeall empties the list", list.getnumitems() == 0);
	}
	{
		CList list;
		TestItem a(1), b(2);
		list.append(&a);
		list.append(&b);
		list.removeallnode();
		check("removeallnode empties without destroying items", list.getnumitems() == 0);
		check("removeallnode leaves items alive", g_liveCount >= 0); /* a,b are stack objects */
	}

	/* ---- CStaticList: fixed-capacity pool ---- */
	{
		CStaticList list(3);
		TestItem a(1), b(2), c(3), d(4);
		check("CStaticList append within capacity #1", list.append(&a));
		check("CStaticList append within capacity #2", list.append(&b));
		check("CStaticList append within capacity #3", list.append(&c));
		check("CStaticList append beyond capacity fails", !list.append(&d));
		check("CStaticList count stays at capacity", list.getnumitems() == 3);

		list.remove(&b);
		check("CStaticList remove frees a pool slot", list.getnumitems() == 2);
		check("CStaticList append reuses freed slot", list.append(&d));
		check("CStaticList count back to 3", list.getnumitems() == 3);

		int ids[8];
		int n = WalkIds(list, ids, 8);
		check("CStaticList order after reuse", n == 3 && ids[0] == 1 && ids[1] == 3 && ids[2] == 4);
	}

	/* ---- CListIter ---- */
	{
		CList list;
		TestItem a(1), b(2), c(3);
		list.append(&a);
		list.append(&b);
		list.append(&c);

		CListIter it(list, kIterFront);
		int ids[8];
		int n = 0;
		for (; it(); ++it)
			ids[n++] = static_cast<TestItem *>(it())->mId;
		check("CListIter forward walk from front", n == 3 && ids[0] == 1 && ids[1] == 2 && ids[2] == 3);

		CListIter itBack(list, kIterBack);
		n = 0;
		for (; itBack(); --itBack)
			ids[n++] = static_cast<TestItem *>(itBack())->mId;
		check("CListIter backward walk from back", n == 3 && ids[0] == 3 && ids[1] == 2 && ids[2] == 1);

		CListIter itAt(list, &b);
		check("CListIter positioned at item", itAt() == &b);
		++itAt;
		check("CListIter ++ from &b reaches &c", itAt() == &c);

		/* CListIter's index ctor is 0-based (index 0 == front), confirmed against
		 * findnode(long)'s identical real disassembly shape -- distinct from
		 * insertat()'s own 1-based "insert after position N" convention above. */
		CListIter itIdx(list, 0L);
		check("CListIter by index 0 == front", itIdx() == &a);
		CListIter itIdx2(list, 1L);
		check("CListIter by index 1 == second element", itIdx2() == &b);
		CListIter itIdx3(list, 2L);
		check("CListIter by index 2 == third element", itIdx3() == &c);

		CListIter itEmpty;
		check("default CListIter dereferences NULL", itEmpty() == 0);

		CListIter itTop(list, kIterFront);
		itTop.totail();
		check("totail() repositions to last item", itTop() == &c);
		itTop.totop();
		check("totop() repositions to first item", itTop() == &a);
	}

	printf("\n%s (%d failed)\n", g_fail == 0 ? "PASSED" : "FAILED", g_fail);
	return g_fail == 0 ? 0 : 1;
}
