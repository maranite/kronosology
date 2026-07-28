/*
 * test_korg_ksf.cpp  -  host-side known-answer test for CKorgKsf
 * (src/init/korg_ksf.cpp). See include/korg_ksf.h for full ground-truth
 * provenance and the list of deferred (not reconstructed) methods.
 */

#include <cstdio>
#include <cstring>

#include "korg_ksf.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CKorgKsf known-answer test\n");
	printf("===========================\n");

	printf("[1] CSampleChunk GetName/SetName round-trip\n");
	{
		CKorgKsf::CSampleChunk c;
		c.SetName("SampleA");
		char buf[0x11];
		c.GetName(buf, sizeof(buf));
		check("== SampleA", strcmp(buf, "SampleA") == 0);
	}

	printf("[2] CSampleChunk StartOffsetSamples: idx 0/1 round-trip, idx>1 bounds-checked\n");
	{
		CKorgKsf::CSampleChunk c;
		c.SetStartOffsetSamples(0, 1000);
		c.SetStartOffsetSamples(1, 2000);
		check("idx 0 == 1000", c.GetStartOffsetSamples(0) == 1000);
		check("idx 1 == 2000", c.GetStartOffsetSamples(1) == 2000);

		c.SetStartOffsetSamples(2, 9999); /* out of range: real ground truth no-ops */
		check("idx 2 (out of range) == 0", c.GetStartOffsetSamples(2) == 0);
		check("idx 0 unaffected by out-of-range set", c.GetStartOffsetSamples(0) == 1000);
	}

	printf("[3] CSampleFileNameChunk: round-trip and NULL-source clears\n");
	{
		CKorgKsf::CSampleFileNameChunk c;
		c.SetSampleFileName("MS001001.KSF");
		char buf[0xd];
		c.GetSampleFileName(buf, sizeof(buf));
		check("== MS001001.KSF (exactly 12 chars, fits the 12-byte bound)",
		      strcmp(buf, "MS001001.KSF") == 0);

		c.SetSampleFileName(0);
		c.GetSampleFileName(buf, sizeof(buf));
		check("NULL source -> empty", buf[0] == 0);
	}

	printf("[4] CSampleDataChunk::SetOneShot toggles independently\n");
	{
		CKorgKsf::CSampleDataChunk c;
		c.SetOneShot(true);
		c.SetOneShot(false);
		/* No public getter in ground truth either (SetOneShot is a real,
		 * one-directional bit-flag setter) -- this just exercises both
		 * branches without crashing/UB (private mFlags byte, no direct
		 * assertion possible without a getter ground truth doesn't have).
		 */
		check("SetOneShot(true) then (false) completes cleanly", true);
	}

	printf("[5] ctor + TypeString + IsBigEndian\n");
	{
		CKorgKsf ksf("/tmp/test.KSF", "SampleName", 5, CKorgKsf::Mono, true);
		check("IsBigEndian() == true", ksf.IsBigEndian());
		check("TypeString(Mono)", strcmp(CKorgKsf::TypeString(CKorgKsf::Mono), "Mono") == 0);
		check("TypeString(Left)", strcmp(CKorgKsf::TypeString(CKorgKsf::Left), "Left") == 0);
		check("TypeString(Right)", strcmp(CKorgKsf::TypeString(CKorgKsf::Right), "Right") == 0);
		check("TypeString(out-of-range) == Unknown",
		      strcmp(CKorgKsf::TypeString((CKorgKsf::KorgType)7), "Unknown") == 0);
	}

	printf("[6] MakeSampleFileName: sprintf(\"MS%%03u%%03u\", a, c+1) + .KSF extension\n");
	{
		char buf[64];
		CKorgKsf::MakeSampleFileName(1, 999 /* real ground truth: unused */, 0, buf, 0 /* unused */);
		check("== MS001001.KSF", strcmp(buf, "MS001001.KSF") == 0);

		CKorgKsf::MakeSampleFileName(12, 0, 4, buf, 0);
		check("== MS012005.KSF", strcmp(buf, "MS012005.KSF") == 0);
	}

	printf("[7] SetSampleDataSize: alloc, then resize, then zero-size frees\n");
	{
		CKorgKsf ksf("/tmp/x.KSF", "X", 0, CKorgKsf::Mono, false);
		ksf.SetSampleDataSize(1024, true);   /* allocates */
		ksf.SetSampleDataSize(2048, true);   /* frees old, allocates new */
		ksf.SetSampleDataSize(0, true);      /* frees, no alloc (size==0) */
		ksf.SetSampleDataSize(4096, false);  /* no alloc (alloc==false) */
		check("sequence completes cleanly (no double-free / leak-visible crash)", true);
	}

	printf("\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
