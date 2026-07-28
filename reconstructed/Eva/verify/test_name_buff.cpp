/*
 * test_name_buff.cpp  -  host-side known-answer test for CNameBuff's 17
 * reconstructed methods (include/name_buff.h). See that header's own comment
 * for full ground-truth addresses/provenance, including the real setup()
 * clamp-vs-unclamped-loop-count bug and the getter-clamps/setter-no-ops
 * out-of-range asymmetry.
 */

#include <cstdio>
#include <cstring>

#include "name_buff.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CNameBuff known-answer test\n");
	printf("============================\n");

	printf("[1] setup()\n");
	{
		CNameBuff nb;
		nb.setup(5);
		check("mBuf points at theDiskNameBuf", (void *)nb.getname(0) == (void *)theDiskNameBuf);
		for (int i = 0; i < 5; i++)
			check("setup(5) marks slot available", nb.isavailable(i));
	}

	printf("[2] setname/getname, short and long (>240 char) names\n");
	{
		CNameBuff nb;
		nb.setup(3);
		nb.setname("short.wav", 0);
		check("getname(0) == \"short.wav\"", strcmp(nb.getname(0), "short.wav") == 0);

		char longName[400];
		memset(longName, 'A', sizeof longName - 1);
		longName[sizeof longName - 1] = '\0';
		nb.setname(longName, 1);
		const char *got = nb.getname(1);
		check("getname(1) truncated to 240 chars", strlen(got) == 240);
		check("getname(1) truncated content all 'A'", got[0] == 'A' && got[239] == 'A');
	}

	printf("[3] setsize/getsize, setsecondarysize/getsecondarysize (64-bit)\n");
	{
		CNameBuff nb;
		nb.setup(2);
		unsigned long long big = 0x1122334455667788ULL;
		nb.setsize(big, 0);
		check("getsize(0) round-trips a 64-bit value", nb.getsize(0) == big);
		nb.setsecondarysize(0xCAFEBABEDEADBEEFULL, 1);
		check("getsecondarysize(1) round-trips a 64-bit value",
		      nb.getsecondarysize(1) == 0xCAFEBABEDEADBEEFULL);
	}

	printf("[4] setfkind/getfkind opaque round-trip\n");
	{
		CNameBuff nb;
		nb.setup(1);
		nb.setfkind(static_cast<EFileKind>(7), 0);
		check("getfkind(0) round-trips opaque value", nb.getfkind(0) == static_cast<EFileKind>(7));
	}

	printf("[5] setavailable/isavailable\n");
	{
		CNameBuff nb;
		nb.setup(2);
		nb.setavailable(false, 0);
		check("setavailable(false,0) -> isavailable(0)==false", !nb.isavailable(0));
		check("slot 1 untouched, still available", nb.isavailable(1));
	}

	printf("[6] setters silently no-op when idx >= mCount (real ground truth)\n");
	{
		CNameBuff nb;
		nb.setup(2);
		nb.setname("first", 0);
		nb.setname("SHOULD_NOT_APPEAR", 5); /* idx 5 >= mCount 2 -> no-op */
		check("out-of-range setname() did not corrupt slot 0",
		      strcmp(nb.getname(0), "first") == 0);
	}

	printf("[7] getters clamp an out-of-range idx to mCount-1 (real ground truth)\n");
	{
		CNameBuff nb;
		nb.setup(3);
		nb.setname("a", 0);
		nb.setname("b", 1);
		nb.setname("last", 2);
		check("getname(99) clamps to slot mCount-1 (\"last\")",
		      strcmp(nb.getname(99), "last") == 0);
		nb.setfkind(static_cast<EFileKind>(9), 2);
		check("getfkind(99) clamps to slot mCount-1's stored kind",
		      nb.getfkind(99) == static_cast<EFileKind>(9));
	}

	printf("[8] deletearray()/init() are confirmed true no-ops\n");
	{
		CNameBuff nb;
		nb.setup(1);
		nb.setname("keep-me", 0);
		nb.deletearray();
		nb.init();
		check("deletearray()/init() do not touch mCount", nb.isavailable(0));
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
