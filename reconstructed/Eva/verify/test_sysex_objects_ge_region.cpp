/*
 * test_sysex_objects_ge_region.cpp  -  host-side known-answer test for
 * CSysExGlobal/CSysExKarmaGE/CSysExGETemplate/CSysExRegion (see
 * include/sysex_objects_ge_region.h for full ground-truth provenance).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sysex_objects_ge_region.h"
#include "sysex_control_objects.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-56s 0x%lx\n", label, got);
		return;
	}
	printf("  FAIL  %-56s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

int main()
{
	printf("CSysExGlobal/CSysExKarmaGE/CSysExGETemplate/CSysExRegion KAT\n");
	printf("==============================================================\n");

	{
		CSysExGlobal o;
		check_eq("Global::GetStorageId", o.GetStorageId(), 5);
		check_eq("Global::GetNumBanks", o.GetNumBanks(), 1);
		check_eq("Global::HasDigests", o.HasDigests(), 1);
		check_eq("Global::GetVersion", o.GetVersion(), 2);
		check_eq("Global::GetObjectSize", o.GetObjectSize(), 0x6084);
		check_eq("Global::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x6084);
		check_eq("Global::GetSysExBankId always 0", o.GetSysExBankId(7), 0);
		check_eq("Global::GetNumOfObject", o.GetNumOfObject(0), 1);
		check_eq("Global::GetNumObjectsForDigest", o.GetNumObjectsForDigest(0), 1);
		o.GetObjectPointer(0, 0); /* discarded CStorage calls -- no crash is the check */
		check_eq("Global::GetObjectPointer completes without crashing", 1, 1);
	}
	{
		CSysExKarmaGE o;
		check_eq("KarmaGE::GetStorageId", o.GetStorageId(), 6);
		check_eq("KarmaGE::GetNumBanks", o.GetNumBanks(), 0xc);
		check_eq("KarmaGE::HasDigests", o.HasDigests(), 1);
		check_eq("KarmaGE::GetVersion", o.GetVersion(), 0);
		check_eq("KarmaGE::GetObjectSize", o.GetObjectSize(), 0x9ec);
		check_eq("KarmaGE::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x9f0);
		check_eq("KarmaGE::GetSysExBankId(3) passthrough", o.GetSysExBankId(3), 3);
		check_eq("KarmaGE::GetNumOfObject", o.GetNumOfObject(0), 0x80);
		check_eq("KarmaGE::GetObjectPointer == GetUserGE(0)+idx*0x9ec",
			 o.GetObjectPointer(0, 2), 0 + 2 * 0x9ec);
	}
	{
		CSysExGETemplate o;
		check_eq("GETemplate::GetStorageId", o.GetStorageId(), 7);
		check_eq("GETemplate::GetNumBanks", o.GetNumBanks(), 4);
		check_eq("GETemplate::HasDigests", o.HasDigests(), 1);
		check_eq("GETemplate::GetVersion", o.GetVersion(), 0);
		check_eq("GETemplate::GetObjectSize", o.GetObjectSize(), 0x10580);
		check_eq("GETemplate::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x10584);
		check_eq("GETemplate::GetSysExBankId(9) passthrough", o.GetSysExBankId(9), 9);
		check_eq("GETemplate::GetNumOfObject", o.GetNumOfObject(0), 1);
		check_eq("GETemplate::GetObjectPointer == GetUserKarmaTemplate(0)+idx*0x10580",
			 o.GetObjectPointer(0, 3), 0 + 3 * 0x10580);
	}
	{
		CSysExRegion o;
		check_eq("Region::GetStorageId", o.GetStorageId(), 0xb);
		check_eq("Region::GetNumBanks", o.GetNumBanks(), 1);
		check_eq("Region::HasDigests", o.HasDigests(), 1);
		check_eq("Region::GetVersion", o.GetVersion(), 1);
		check_eq("Region::GetObjectSize", o.GetObjectSize(), 0x130);
		check_eq("Region::GetObjectSizeForExport", o.GetObjectSizeForExport(), 0x130);
		check_eq("Region::GetSysExBankId always 0", o.GetSysExBankId(5), 0);
		check_eq("Region::GetNumOfObject", o.GetNumOfObject(0), 10000);

		CKGUtil::sm_poRegionHolder = 0x1000;
		check_eq("Region::GetObjectPointer idx=42 -> 42*0x130+base",
			 o.GetObjectPointer(0, 42), (long)(42 * 0x130 + 0x1000));
		check_eq("Region::GetObjectPointer idx out of range clamps to 0",
			 o.GetObjectPointer(0, 20000), (long)(0 * 0x130 + 0x1000));

		/* GetTotalSizeForExport: real body reads a byte at
		 * sm_poRegionHolder+0x18+i*0x130 for every i in [0,10000),
		 * IGNORING both its own explicit range arguments (a real,
		 * confirmed ground-truth quirk -- see header comment). Needs
		 * a real backing buffer since the body actually dereferences
		 * it. */
		const size_t bufSize = (size_t)10000 * 0x130 + 0x18 + 1;
		unsigned char *buf = (unsigned char *)calloc(1, bufSize);
		CKGUtil::sm_poRegionHolder = (unsigned long)buf;
		buf[0x18 + 0 * 0x130] = 1;
		buf[0x18 + 1 * 0x130] = 1;
		buf[0x18 + 9999 * 0x130] = 1;
		check_eq("Region::GetTotalSizeForExport counts 3 active * 0x130",
			 o.GetTotalSizeForExport(0, 0), 3 * 0x130);
		check_eq("...ignores its own [param1,param2] range (real quirk)",
			 o.GetTotalSizeForExport(5000, 5001), 3 * 0x130);
		free(buf);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
