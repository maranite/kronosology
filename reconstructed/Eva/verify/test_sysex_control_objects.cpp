/*
 * test_sysex_control_objects.cpp  -  host-side known-answer test for
 * CSysExKarmaGEInfo/CSysExSongControl (see
 * include/sysex_control_objects.h for full ground-truth provenance).
 */

#include <cstdio>

#include "sysex_control_objects.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-48s 0x%lx\n", label, got);
		return;
	}
	printf("  FAIL  %-48s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

int main()
{
	printf("CSysExKarmaGEInfo/CSysExSongControl known-answer test\n");
	printf("=========================================================\n");

	{
		CSysExKarmaGEInfo o;
		check_eq("KarmaGEInfo::GetStorageId", o.GetStorageId(), 0x26);
		check_eq("KarmaGEInfo::GetNumBanks", o.GetNumBanks(), 1);
		check_eq("KarmaGEInfo::GetVersion", o.GetVersion(), 0);
		check_eq("KarmaGEInfo::GetObjectSize", o.GetObjectSize(), 0x7a0);
		check_eq("KarmaGEInfo::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x7a0);
		check_eq("KarmaGEInfo::GetSysExBankId(0) passthrough", o.GetSysExBankId(0), 0);
		check_eq("KarmaGEInfo::GetSysExBankId(7) passthrough", o.GetSysExBankId(7), 7);
		check_eq("KarmaGEInfo::GetNumOfObject", o.GetNumOfObject(99), 0);
		check_eq("KarmaGEInfo::GetTotalSizeForExport", o.GetTotalSizeForExport(1, 2), 0);

		CKGUtil::sm_poKGUIInfo = 0x1000;
		check_eq("KarmaGEInfo::GetObjectPointer == sm_poKGUIInfo+0x3c90",
			 o.GetObjectPointer(0, 0), 0x1000 + 0x3c90);
	}
	{
		CSysExSongControl o;
		check_eq("SongControl::GetStorageId", o.GetStorageId(), 0xc);
		check_eq("SongControl::GetVersion", o.GetVersion(), 0);
		check_eq("SongControl::GetObjectSize", o.GetObjectSize(), 0x1490);
		check_eq("SongControl::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x1490);

		/* GetRegistoredSong() is a no-op stand-in for a real, entirely
		 * unmodeled class (see header comment) -- no observable state
		 * to check beyond "the real singleton-through-a-raw-pointer
		 * call shape doesn't crash with a valid instance". */
		CSeqDataManager mgr;
		CKGUtil::sm_poSeqDataManager = (unsigned long)&mgr;
		o.GetObjectPointer(0, 0x77);
		check_eq("SongControl::GetObjectPointer completes without crashing", 1, 1);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
