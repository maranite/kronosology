/*
 * test_korg_ksc.cpp  -  host-side known-answer test for CKorgKsc
 * (src/init/korg_ksc.cpp). See include/korg_ksc.h for full ground-truth
 * provenance and the list of deferred (not reconstructed) methods.
 */

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#include "korg_ksc.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CKorgKsc known-answer test\n");
	printf("===========================\n");

	printf("[1] ctor with a NULL uuid -> mUUID starts empty\n");
	{
		CKorgKsc ksc("/tmp/test.KSC", 0, true, false);
		char buf[0x40];
		ksc.GetUUID(buf, sizeof(buf));
		check("GetUUID == \"\"", strcmp(buf, "") == 0);
	}

	printf("[2] ctor with a real uuid -> round-trips via GetUUID\n");
	{
		CKorgKsc ksc("/tmp/test.KSC", "11111111-2222-3333-4444-555555555555", true, true);
		char buf[0x40];
		ksc.GetUUID(buf, sizeof(buf));
		check("== seeded uuid", strcmp(buf, "11111111-2222-3333-4444-555555555555") == 0);
	}

	printf("[3] SetUUID/GetUUID round-trip\n");
	{
		CKorgKsc ksc("/tmp/test.KSC", 0, false, false);
		ksc.SetUUID("abcd-1234");
		char buf[0x40];
		ksc.GetUUID(buf, sizeof(buf));
		check("== abcd-1234", strcmp(buf, "abcd-1234") == 0);

		ksc.SetUUID(0); /* real ground truth: NULL clears to empty */
		ksc.GetUUID(buf, sizeof(buf));
		check("SetUUID(NULL) clears to empty", strcmp(buf, "") == 0);
	}

	printf("[4] GetUUID(NULL dest) is a real, confirmed no-op (must not crash)\n");
	{
		CKorgKsc ksc("/tmp/test.KSC", "some-uuid", false, false);
		ksc.GetUUID(0, 0x40);
		check("GetUUID(NULL, ...) returned without crashing", true);
	}

	printf("[5] MakeFolder: real host round-trip (mkdir)\n");
	{
		remove("/tmp/korg_ksc_test_dir");
		CKorgKsc ksc("/tmp/korg_ksc_test_dir.KSC", 0, false, false);
		ksc.MakeFolder();
		struct stat st;
		int rc = stat("/tmp/korg_ksc_test_dir", &st);
		check("directory created", rc == 0 && (st.st_mode & S_IFDIR) != 0);
		rmdir("/tmp/korg_ksc_test_dir");
	}

	printf("\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
