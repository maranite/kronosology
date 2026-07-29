/*
 * test_sysex_objects.cpp  -  host-side known-answer test for the
 * CSysExSong/CSysExDrumKit/CSysExCombi/CSysExWaveSeq/CSysExSetList/
 * CSysExSongTimbreSet family (see include/sysex_objects.h for full
 * ground-truth provenance).
 */

#include <cstdio>

#include "sysex_objects.h"

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
	printf("CSysExSong/DrumKit/Combi/WaveSeq/SetList/SongTimbreSet known-answer test\n");
	printf("==========================================================================\n");

	{
		CSysExSong o;
		check_eq("Song::GetStorageId", o.GetStorageId(), 0x25);
		check_eq("Song::HasDigests", o.HasDigests(), 1);
		check_eq("Song::GetVersion", o.GetVersion(), 3);
		check_eq("Song::GetObjectSize", o.GetObjectSize(), 0x3314);
		check_eq("Song::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x3314);
		check_eq("Song::GetNumObjectsForDigest(0)", o.GetNumObjectsForDigest(0), 200);
		check_eq("Song::GetNumObjectsForDigest(ignores param)", o.GetNumObjectsForDigest(0x1234), 200);
	}
	{
		CSysExDrumKit o;
		check_eq("DrumKit::GetStorageId", o.GetStorageId(), 3);
		check_eq("DrumKit::HasDigests", o.HasDigests(), 1);
		check_eq("DrumKit::GetVersion", o.GetVersion(), 3);
		check_eq("DrumKit::GetObjectSize", o.GetObjectSize(), 0x9618);
		check_eq("DrumKit::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x9618);
	}
	{
		CSysExCombi o;
		check_eq("Combi::GetStorageId", o.GetStorageId(), 2);
		check_eq("Combi::HasDigests", o.HasDigests(), 1);
		check_eq("Combi::GetVersion", o.GetVersion(), 3);
		check_eq("Combi::GetObjectSize", o.GetObjectSize(), 0x1e82);
		check_eq("Combi::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x1e82);
	}
	{
		CSysExWaveSeq o;
		check_eq("WaveSeq::GetStorageId", o.GetStorageId(), 4);
		check_eq("WaveSeq::HasDigests", o.HasDigests(), 1);
		check_eq("WaveSeq::GetVersion", o.GetVersion(), 1);
		check_eq("WaveSeq::GetObjectSize", o.GetObjectSize(), 0x8a8);
		check_eq("WaveSeq::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x8a8);
	}
	{
		CSysExSetList o;
		check_eq("SetList::GetStorageId", o.GetStorageId(), 0x1b);
		check_eq("SetList::HasDigests", o.HasDigests(), 1);
		check_eq("SetList::GetVersion", o.GetVersion(), 0);
		check_eq("SetList::GetObjectSize", o.GetObjectSize(), 0x10f28);
		check_eq("SetList::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x10f28);
	}
	{
		CSysExSongTimbreSet o;
		check_eq("SongTimbreSet::GetStorageId", o.GetStorageId(), 0xd);
		check_eq("SongTimbreSet::GetVersion", o.GetVersion(), 3);
		check_eq("SongTimbreSet::GetObjectSize", o.GetObjectSize(), 0x1e82);
		check_eq("SongTimbreSet::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x1e82);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
