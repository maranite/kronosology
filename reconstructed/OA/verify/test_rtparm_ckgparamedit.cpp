// SPDX-License-Identifier: GPL-2.0
/*
 * test_rtparm_ckgparamedit.cpp  -  KAT for
 * CKGParamEdit::GetRTParmBufferSelectId(int) (src/engine/
 * rtparm_ckgparamedit.cpp). Split out of test_rtparm_family.cpp's own
 * binary -- see that source file's header comment for why (a real,
 * pre-existing linkage conflict between oa_ckg_module_param_msg_handler.h
 * and oa_rtparm_pe_table.h, both needed together only by this pass).
 */

#include <cstdio>
#include "oa_ckg_module_param_msg_handler.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-40s %ld\n", label, got); return; }
	printf("  FAIL  %-40s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

int main()
{
	CKGParamEdit pe;
	check_eq("GetRTParmBufferSelectId(0)", pe.GetRTParmBufferSelectId(0), 1);
	check_eq("GetRTParmBufferSelectId(3)", pe.GetRTParmBufferSelectId(3), 4);
	check_eq("GetRTParmBufferSelectId(4) oob", pe.GetRTParmBufferSelectId(4), 0);

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
