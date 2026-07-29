/*
 * test_sysex_object_names.cpp  -  host-side known-answer test for the
 * 8-class CSysEx*Name/CSysExSetListSlotComment family (see
 * include/sysex_object_names.h for full ground-truth provenance).
 */

#include <cstdio>

#include "sysex_object_names.h"

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	if (got == want) {
		printf("  ok    %-48s 0x%x\n", label, got);
		return;
	}
	printf("  FAIL  %-48s got=0x%x want=0x%x\n", label, got, want);
	g_fail++;
}

int main()
{
	printf("CSysEx*Name family known-answer test\n");
	printf("======================================\n");

	{
		CSysExSetListSlotComment o;
		check_eq("SetListSlotComment::GetStorageId", o.GetStorageId(), 0x1c);
		check_eq("SetListSlotComment::GetVersion", o.GetVersion(), 0);
		check_eq("SetListSlotComment::GetObjectSize", o.GetObjectSize(), 0x200);
		check_eq("SetListSlotComment::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x200);
	}
	{
		CSysExSetListSlotName o;
		check_eq("SetListSlotName::GetStorageId", o.GetStorageId(), 0x1d);
		check_eq("SetListSlotName::GetVersion", o.GetVersion(), 0);
		check_eq("SetListSlotName::GetObjectSize", o.GetObjectSize(), 0x18);
		check_eq("SetListSlotName::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x18);
	}
	{
		CSysExCombiName o;
		check_eq("CombiName::GetStorageId", o.GetStorageId(), 0x1e);
		check_eq("CombiName::GetVersion", o.GetVersion(), 0);
		check_eq("CombiName::GetObjectSize", o.GetObjectSize(), 0x18);
		check_eq("CombiName::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x18);
	}
	{
		CSysExProgName o;
		check_eq("ProgName::GetStorageId", o.GetStorageId(), 0x1f);
		check_eq("ProgName::GetVersion", o.GetVersion(), 0);
		check_eq("ProgName::GetObjectSize", o.GetObjectSize(), 0x18);
		check_eq("ProgName::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x18);
	}
	{
		CSysExSongName o;
		check_eq("SongName::GetStorageId", o.GetStorageId(), 0x20);
		check_eq("SongName::GetVersion", o.GetVersion(), 0);
		check_eq("SongName::GetObjectSize", o.GetObjectSize(), 0x18);
		check_eq("SongName::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x18);
	}
	{
		CSysExWaveSeqName o;
		check_eq("WaveSeqName::GetStorageId", o.GetStorageId(), 0x21);
		check_eq("WaveSeqName::GetVersion", o.GetVersion(), 0);
		check_eq("WaveSeqName::GetObjectSize", o.GetObjectSize(), 0x18);
		check_eq("WaveSeqName::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x18);
	}
	{
		CSysExDrumKitName o;
		check_eq("DrumKitName::GetStorageId", o.GetStorageId(), 0x22);
		check_eq("DrumKitName::GetVersion", o.GetVersion(), 0);
		check_eq("DrumKitName::GetObjectSize", o.GetObjectSize(), 0x18);
		check_eq("DrumKitName::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x18);
	}
	{
		CSysExSetListName o;
		check_eq("SetListName::GetStorageId", o.GetStorageId(), 0x23);
		check_eq("SetListName::GetVersion", o.GetVersion(), 0);
		check_eq("SetListName::GetObjectSize", o.GetObjectSize(), 0x18);
		check_eq("SetListName::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x18);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
