// SPDX-License-Identifier: GPL-2.0
/*
 * test_cost_profile.cpp  -  KAT for CCostProfile::CCostProfile()
 * (see oa_setup_global_resources.h / src/engine/cost_profile.cpp).
 *
 * Mocks CStartupFile (a confirmed real, deliberately deferred base
 * class -- see the header's own note) purely so the real
 * CCostProfile constructor links and can be exercised.
 */

#include <cstdio>
#include <cstring>
#include "oa_setup_global_resources.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-55s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-55s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

static char g_startupFileName[64];
static int g_startupFileCtorCalls, g_startupFileDtorCalls;
CStartupFile::CStartupFile(const char *name)
{
	g_startupFileCtorCalls++;
	strncpy(g_startupFileName, name, sizeof(g_startupFileName) - 1);
	_vtablePtr = 0;
	_field4 = 0.0f;
}
CStartupFile::~CStartupFile() { g_startupFileDtorCalls++; }

extern "C" unsigned char _ZTV12CCostProfile[16];

int main(void)
{
	printf("CCostProfile known-answer test\n");
	printf("=========================================================\n");

	unsigned char buf[sizeof(CCostProfile)];
	memset(buf, 0xcc, sizeof(buf));

	CCostProfile *cp = new (buf) CCostProfile();

	printf("[1] base class construction\n");
	check_eq("CStartupFile ctor called once", g_startupFileCtorCalls, 1);
	check_eq("CStartupFile ctor arg == \"CostProfile\"",
		 (long)(strcmp(g_startupFileName, "CostProfile") == 0), 1);

	printf("\n[2] vtable pointer\n");
	check_eq("_vtablePtr == &_ZTV12CCostProfile + 8",
		 (long)cp->_vtablePtr, (long)(void *)(_ZTV12CCostProfile + 8));

	printf("\n[3] sInstance\n");
	check_eq("sInstance == this", (long)(CCostProfile::sInstance == cp), 1);

	printf("\n[4] the big unrolled zero region (+0x8..+0x327 target-relative)\n");
	{
		int allZero = 1;
		for (unsigned int i = 0; i < sizeof(cp->_unrecovered_zeroed); i++)
			if (cp->_unrecovered_zeroed[i] != 0)
				allZero = 0;
		check_eq("_unrecovered_zeroed is fully zeroed", allZero, 1);
	}

	printf("\n[5] the 198-entry array: fields 4/8/c/10 zeroed, field 0 left untouched\n");
	{
		int fieldsZeroed = 1;
		for (unsigned int i = 0; i < CCOSTPROFILE_ENTRY_COUNT; i++) {
			if (cp->entries[i].field4 != 0 || cp->entries[i].field8 != 0 ||
			    cp->entries[i].fieldC != 0 || cp->entries[i].field10 != 0)
				fieldsZeroed = 0;
		}
		check_eq("entries[*].field4/8/c/10 all zeroed", fieldsZeroed, 1);

		/* _unaccounted0 was poisoned to 0xcccccccc before construction;
		 * confirm the real ctor genuinely never writes it (matches the
		 * real disassembly's own confirmed quirk). */
		int untouched = 1;
		for (unsigned int i = 0; i < CCOSTPROFILE_ENTRY_COUNT; i++)
			if (cp->entries[i]._unaccounted0 != 0xcccccccc)
				untouched = 0;
		check_eq("entries[*]._unaccounted0 left untouched (still poisoned)", untouched, 1);
	}

	printf("\n[6] size cross-check against setup_global_resources.cpp's own\n"
	       "    confirmed real allocation (::operator new(0x12a0))\n");
	check_eq("sizeof(CCostProfileEntry) == 20 (0x14, confirmed real stride)",
		 (long)sizeof(CCostProfileEntry), 20);
	check_eq("CCOSTPROFILE_ENTRY_COUNT == 198 (0xf78/0x14, confirmed real loop bound)",
		 CCOSTPROFILE_ENTRY_COUNT, 198);

	printf("\n[7] destruction\n");
	cp->~CCostProfile();
	check_eq("CStartupFile dtor called once (base chaining)", g_startupFileDtorCalls, 1);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
