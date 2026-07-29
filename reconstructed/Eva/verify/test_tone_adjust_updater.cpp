/*
 * test_tone_adjust_updater.cpp  -  host-side known-answer test for
 * CToneAdjustUpdater's 14 real methods landed in round 48 (solo,
 * 2026-07-29). See include/tone_adjust_updater.h for the full
 * derivation.
 */
#include <cstdio>
#include "tone_adjust_updater.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	/* [1] ctor/dtor: real no-ops, reached here */
	{
		CToneAdjustUpdater u;
	}
	check("ctor/dtor: no-op, reached here", true);

	/* [2] ProgSwitchValueBuffer: plain indexed get/set */
	CToneAdjustUpdater::SetProgSwitchValueBuffer(3, 0x1234);
	check("Set/GetProgSwitchValueBuffer(3) round-trip",
	      CToneAdjustUpdater::GetProgSwitchValueBuffer(3) == 0x1234);
	CToneAdjustUpdater::SetProgSwitchValueBuffer(0, -7);
	check("Set/GetProgSwitchValueBuffer(0) round-trip, negative value",
	      CToneAdjustUpdater::GetProgSwitchValueBuffer(0) == -7);

	/* [3] ConvertDKitNumToBank / ConvertWSeqNumToBank: pure arithmetic */
	check("ConvertDKitNumToBank(0) == 0", CToneAdjustUpdater::ConvertDKitNumToBank(0) == 0);
	check("ConvertDKitNumToBank(0x27) == 0 (boundary)",
	      CToneAdjustUpdater::ConvertDKitNumToBank(0x27) == 0);
	check("ConvertDKitNumToBank(0x28) == 1 (just over boundary)",
	      CToneAdjustUpdater::ConvertDKitNumToBank(0x28) == 1);
	check("ConvertDKitNumToBank(0x38) == 2", CToneAdjustUpdater::ConvertDKitNumToBank(0x38) == 2);

	check("ConvertWSeqNumToBank(0x95) == 0 (boundary)",
	      CToneAdjustUpdater::ConvertWSeqNumToBank(0x95) == 0);
	check("ConvertWSeqNumToBank(0x96) == 1 (just over boundary)",
	      CToneAdjustUpdater::ConvertWSeqNumToBank(0x96) == 1);
	check("ConvertWSeqNumToBank(0xb6) == 2",
	      CToneAdjustUpdater::ConvertWSeqNumToBank(0xb6) == 2);

	/* [4] GetFormatterForXxx family: in-range table read + out-of-range fallback */
	check("GetFormatterForPCM(0x4f) [in range, zero-init table] == 0",
	      CToneAdjustUpdater::GetFormatterForPCM(0x4f) == 0);
	check("GetFormatterForPCM(0x50) [out of range] == 0x68 fallback",
	      CToneAdjustUpdater::GetFormatterForPCM(0x50) == 0x68);
	check("GetFormatterForCommon(0x25) [out of range] == 0x68 fallback",
	      CToneAdjustUpdater::GetFormatterForCommon(0x25) == 0x68);
	check("GetFormatterForPCMStoredValueForProg(0x10) [below range] == 0xd7 fallback",
	      CToneAdjustUpdater::GetFormatterForPCMStoredValueForProg(0x10) == 0xd7);
	check("GetFormatterForPCMStoredValueForProg(0x3d) [above range] == 0xd7 fallback",
	      CToneAdjustUpdater::GetFormatterForPCMStoredValueForProg(0x3d) == 0xd7);
	check("GetFormatterForPCMStoredValueForTimbre(0x50) [above range] == 0xd7 fallback",
	      CToneAdjustUpdater::GetFormatterForPCMStoredValueForTimbre(0x50) == 0xd7);
	check("GetFormatterForCommonStoredValue(0x1f) [above range] == 0xd7 fallback",
	      CToneAdjustUpdater::GetFormatterForCommonStoredValue(0x1f) == 0xd7);

	/* [5] GetAssignTypeForXxx family: two-tier table + fallback */
	check("GetAssignTypeForPCM(0x50) [above both tables] == 1 fallback",
	      CToneAdjustUpdater::GetAssignTypeForPCM(0x50) == 1);
	check("GetAssignTypeForCommon(0x25) [above table] == 2 fallback",
	      CToneAdjustUpdater::GetAssignTypeForCommon(0x25) == 2);

	/* [6] IsAssignAvailable: pure boolean logic */
	check("IsAssignAvailable(2, 0) == true (not in {0,1,10})",
	      CToneAdjustUpdater::IsAssignAvailable(2, 0) == true);
	check("IsAssignAvailable(1, 0) == false (algorithm==1, arg2==0)",
	      CToneAdjustUpdater::IsAssignAvailable(1, 0) == false);
	check("IsAssignAvailable(1, 5) == true (algorithm==1, arg2!=0)",
	      CToneAdjustUpdater::IsAssignAvailable(1, 5) == true);
	check("IsAssignAvailable(10, 0) == false", CToneAdjustUpdater::IsAssignAvailable(10, 0) == false);
	check("IsAssignAvailable(0, 0) == false", CToneAdjustUpdater::IsAssignAvailable(0, 0) == false);

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
