/*
 * test_korg_kmp.cpp  -  host-side known-answer test for CKorgKmp
 * (src/init/korg_kmp.cpp). See include/korg_kmp.h for full ground-truth
 * provenance and the list of deferred (not reconstructed) methods.
 */

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#include "korg_kmp.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Test-only access to CKorgKmp's private mRelativeChunks/mRelative3Chunks,
 * via the header's own KorgKmpTestHooks friend seam (same pattern as
 * test_korg_riff.cpp's own KorgRiffTestHooks).
 */
struct KorgKmpTestHooks {
	static void PushPair(CKorgKmp *kmp, unsigned char low, unsigned char high)
	{
		CKorgKmp::CMultisampleRelativeChunk *rec2 = new CKorgKmp::CMultisampleRelativeChunk();
		rec2->mUnknownLow = low;
		kmp->mRelativeChunks.push_back(rec2);

		CKorgKmp::CMultisampleRelative3Chunk *rec3 = new CKorgKmp::CMultisampleRelative3Chunk();
		rec3->mUnknownHigh = high;
		kmp->mRelative3Chunks.push_back(rec3);
	}
};

int main()
{
	printf("CKorgKmp known-answer test\n");
	printf("===========================\n");

	printf("[1] CMultisampleChunk GetName/SetName round-trip (16 bytes + force-null)\n");
	{
		CKorgKmp::CMultisampleChunk c;
		c.SetName("ZoneName");
		char buf[0x11];
		c.GetName(buf, sizeof(buf));
		check("== ZoneName", strcmp(buf, "ZoneName") == 0);

		c.SetName("0123456789ABCDEFGHIJ"); /* > 16 chars */
		c.GetName(buf, sizeof(buf));
		check("truncated to 16 + forced NUL at [0x10]", strlen(buf) == 16 && buf[0x10] == 0);
	}

	printf("[2] CMultisampleRelativeChunk GetName/SetName round-trip (12 bytes)\n");
	{
		CKorgKmp::CMultisampleRelativeChunk c;
		c.SetName("Zone1");
		char buf[0xd];
		c.GetName(buf, sizeof(buf));
		check("== Zone1", strcmp(buf, "Zone1") == 0);
	}

	printf("[3] ctor: fields round-trip via TypeString/IsBigEndian/MakeFolder/GetName\n");
	{
		CKorgKmp kmp("/tmp/test.KMP", "MyPatch", 1, CKorgKmp::Mono, 2, 3, 4, 5, 6, 7);
		check("IsBigEndian() == true", kmp.IsBigEndian());

		char name[0x19];
		kmp.GetName(name, sizeof(name));
		check("GetName == MyPatch (Mono, unsuffixed)", strcmp(name, "MyPatch") == 0);
	}

	printf("[4] ctor: Left/Right suffixing via MakeNameLeft/MakeNameRight\n");
	{
		CKorgKmp left("/tmp/l.KMP", "Piano", 0, CKorgKmp::Left, 0, 0, 0, 0, 0, 0);
		char name[0x19];
		left.GetName(name, sizeof(name));
		printf("      Left  displayName = \"%s\"\n", name);
		check("ends with -L", strlen(name) >= 2 && name[strlen(name) - 2] == '-' &&
		                        name[strlen(name) - 1] == 'L');

		CKorgKmp right("/tmp/r.KMP", "Piano", 0, CKorgKmp::Right, 0, 0, 0, 0, 0, 0);
		right.GetName(name, sizeof(name));
		printf("      Right displayName = \"%s\"\n", name);
		check("ends with -R", strlen(name) >= 2 && name[strlen(name) - 2] == '-' &&
		                        name[strlen(name) - 1] == 'R');
	}

	printf("[5] TypeString: Mono/Left/Right + out-of-range default\n");
	{
		check("Mono", strcmp(CKorgKmp::TypeString(CKorgKmp::Mono), "Mono") == 0);
		check("Left", strcmp(CKorgKmp::TypeString(CKorgKmp::Left), "Left") == 0);
		check("Right", strcmp(CKorgKmp::TypeString(CKorgKmp::Right), "Right") == 0);
		check("out-of-range == Unknown",
		      strcmp(CKorgKmp::TypeString((CKorgKmp::KorgType)99), "Unknown") == 0);
	}

	printf("[6] MakeFolder: real host round-trip (mkdir)\n");
	{
		remove("/tmp/korg_kmp_test_dir");
		CKorgKmp kmp("/tmp/korg_kmp_test_dir.KMP", "X", 0, CKorgKmp::Mono, 0, 0, 0, 0, 0, 0);
		kmp.MakeFolder();
		struct stat st;
		int rc = stat("/tmp/korg_kmp_test_dir", &st);
		check("directory created", rc == 0 && (st.st_mode & S_IFDIR) != 0);
		rmdir("/tmp/korg_kmp_test_dir");
	}

	printf("[7] IsStereoCounterpart: Mono is never a counterpart\n");
	{
		CKorgKmp a("/tmp/a.KMP", "Strings-L", 0, CKorgKmp::Mono, 0, 0, 0, 0, 0, 0);
		CKorgKmp b("/tmp/b.KMP", "Strings-R", 0, CKorgKmp::Mono, 0, 0, 0, 0, 0, 0);
		check("Mono vs Mono == false", !a.IsStereoCounterpart(&b));
	}

	printf("[8] IsStereoCounterpart: matching L/R base name -> true\n");
	{
		CKorgKmp left("/tmp/a.KMP", "Strings", 0, CKorgKmp::Left, 0, 0, 0, 0, 0, 0);
		CKorgKmp right("/tmp/b.KMP", "Strings", 0, CKorgKmp::Right, 0, 0, 0, 0, 0, 0);
		check("Left vs Right (same base) == true", left.IsStereoCounterpart(&right));
	}

	printf("[9] IsStereoCounterpart: mismatched base name -> false\n");
	{
		CKorgKmp left("/tmp/a.KMP", "Strings", 0, CKorgKmp::Left, 0, 0, 0, 0, 0, 0);
		CKorgKmp right("/tmp/b.KMP", "Piano", 0, CKorgKmp::Right, 0, 0, 0, 0, 0, 0);
		check("mismatched base names == false", !left.IsStereoCounterpart(&right));
	}

	printf("[10] CanAddSample: empty list -> unconditional true\n");
	{
		CKorgKmp kmp("/tmp/a.KMP", "X", 0, CKorgKmp::Mono, 0, 0, 0, 0, 0, 0);
		check("empty -> true", kmp.CanAddSample(0, 127));
	}

	printf("[11] CanAddSample: non-overlapping existing zone -> true\n");
	{
		CKorgKmp kmp("/tmp/a.KMP", "X", 0, CKorgKmp::Mono, 0, 0, 0, 0, 0, 0);
		/* existing zone covers low=0..high=63 (rec2.mUnknownLow=0, rec3.mUnknownHigh=63) */
		KorgKmpTestHooks::PushPair(&kmp, 0, 63);
		/* new sample low=64,high=127: rec3.mUnknownHigh(63) <= high(127) is true,
		 * but rec2.mUnknownLow(0) >= low(64) is false -> no overlap -> true */
		check("disjoint range -> true", kmp.CanAddSample(64, 127));
	}

	printf("[12] CanAddSample: overlapping existing zone -> false\n");
	{
		CKorgKmp kmp("/tmp/a.KMP", "X", 0, CKorgKmp::Mono, 0, 0, 0, 0, 0, 0);
		/* existing zone: rec2.mUnknownLow=64, rec3.mUnknownHigh=127 */
		KorgKmpTestHooks::PushPair(&kmp, 64, 127);
		/* new sample low=0,high=127: rec3.mUnknownHigh(127)<=high(127) true,
		 * rec2.mUnknownLow(64)>=low(0) true -> overlap -> false */
		check("overlapping range -> false", !kmp.CanAddSample(0, 127));
	}

	printf("\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
