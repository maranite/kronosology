/*
 * test_omega_ptr_array.cpp  -  host-side known-answer test for COmegaPtrArray's
 * round-70 additions: SetAtIndex()/~COmegaPtrArray() (src/base/omega_ptr_array.cpp).
 * The rest of the class (ctors, Add, Destroy, FindIndex, RemoveAtIndex, RemoveAll,
 * Shrink) is already covered by other classes' own tests that exercise it
 * indirectly; this file is scoped to the 2 new methods only.
 */

#include <cstdio>
#include <new>
#include "omega_ptr_array.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("COmegaPtrArray round-70 known-answer test\n");
	printf("===========================================\n");

	COmegaPtrArray a(1, 4, 0);
	int e0 = 100, e1 = 200, e2 = 300, e3 = 400, replacement = 999;
	a.Add(&e0);
	a.Add(&e1);
	a.Add(&e2);
	a.Add(&e3);

	void *old = a.SetAtIndex(2, &replacement);
	check("SetAtIndex(2) returns the old value", old == &e2);
	check("SetAtIndex(2) installs the new value", a.Get(2) == &replacement);
	check("SetAtIndex(2) leaves neighbors untouched", a.Get(1) == &e1 && a.Get(3) == &e3);

	void *old2 = a.SetAtIndex(4, &replacement); /* == mCount, out of bounds */
	check("SetAtIndex(mCount) no-op, returns null", old2 == 0);
	check("SetAtIndex(mCount) doesn't grow the array", a.Count() == 4);

	a.Destroy();

	/* ~COmegaPtrArray(): confirm the vtable-pointer-reset-only body doesn't
	 * crash and resets mVtbl away from whatever the ctor installed. */
	unsigned char buf[0x18];
	COmegaPtrArray *b = new (buf) COmegaPtrArray();
	void *ctorVtbl = *(void **)buf;
	b->~COmegaPtrArray();
	check("dtor resets vtbl to the same install-only placeholder the ctor used",
	      *(void **)buf == ctorVtbl);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
