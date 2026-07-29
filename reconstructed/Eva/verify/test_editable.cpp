// SPDX-License-Identifier: GPL-2.0
/*
 * test_editable.cpp  -  host-side KAT for CEditable's round-62 batch:
 * GetForbidden/SetForbidden/NoGetCallBack/NoSetCallBack. All 4 are real
 * ground-truth __cdecl free functions (no `this` at all) that ignore
 * every argument -- see include/editable.h's own header comment.
 */

#include <cstdio>
#include "editable.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

int main()
{
	printf("CEditable round-62 batch: Forbidden/NoCallBack stand-ins\n");

	int buf = 0x1234;
	check_eq("GetForbidden always returns -1", CEditable::GetForbidden(3, &buf), -1);
	check_eq("SetForbidden always returns -1", CEditable::SetForbidden(3, &buf, 0), -1);
	check_eq("NoGetCallBack always returns 0", CEditable::NoGetCallBack(3, &buf), 0);
	check_eq("NoSetCallBack always returns 0", CEditable::NoSetCallBack(3, &buf, 0), 0);
	check_eq("GetForbidden ignores buf pointer (NULL is fine too)",
		 CEditable::GetForbidden(0, NULL), -1);
	check_eq("NoSetCallBack ignores buf pointer (NULL is fine too)",
		 CEditable::NoSetCallBack(0, NULL, 0), 0);

	if (g_fail == 0)
		printf("PASS\n");
	else
		printf("FAIL (%d)\n", g_fail);
	return g_fail == 0 ? 0 : 1;
}
