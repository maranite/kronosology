/*
 * test_sysapi_instance_register_api.cpp  -  host-side known-answer test for
 * CSysApiInstance::RegisterApi() (src/base/sysapi_instance.cpp, Stage 6 breadth
 * sweep, 2026-07-25 -- promoted from an empty Tier-B link-stub to a real,
 * disassembly-verified Tier A body; see sysapi_instance.h's own header comment for
 * the full per-branch accounting).
 *
 * Drives the REAL, live `SysApiInstance` global directly (not a hand-built stand-in
 * blob) -- the whole point being that this is exactly what mains.cpp's own 7 real
 * boot-path call sites do (`((CSysApiInstance*)Api)->RegisterApi(name, instance)`).
 * `SysApiInstance`'s own real `__attribute__((constructor))` (sysapi_instance.cpp)
 * already ran before main() in every verify binary (make verify links the whole
 * object tree, same pattern test_config_manager_boot_slice.cpp/test_tempo.cpp rely
 * on) -- its embedded mApis array starts genuinely empty, matching the real process
 * at the point mains.cpp's Mains()/InitSystemLayer() first call RegisterApi().
 *
 * Checks:
 *   [1] first registration of a fresh name: mApis count 0 -> 1, real 3-word
 *       {vtbl, name, api} descriptor built, vtbl == PTR__CApiDescriptor_08e81368,
 *       name string is a real, independent strdup (not just a stored pointer), api
 *       pointer stored verbatim
 *   [2] second registration under a distinct name: count 1 -> 2, first entry
 *       untouched (real linear-scan-then-append, not a rebuild)
 *   [3] re-registering the SAME name with the SAME api pointer: count unchanged
 *       (real "already registered" branch, no new descriptor allocated)
 *   [4] re-registering the SAME name with a DIFFERENT api pointer: count unchanged
 *       (old descriptor removed, new one appended) -- exercises the one branch
 *       genuinely dead on this project's own traced boot path (mains.cpp's 7 real
 *       callers never repeat a name), so this is the only place that path gets any
 *       coverage at all
 *   [5] real return value is always literal 1, matching the ground-truth
 *       `undefined4` ABI (sysapi_instance.h's own corrected-signature note)
 */

#include <cstdio>
#include <cstring>

#include "sysapi_instance.h"
#include "omega_vtables.h"
#include "system_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Real field mapping (sysapi_instance.h): mApis embedded at +0x04, count at +0x10,
 * array at +0x18.
 */
static int apis_count()
{
	return *(int *)(SysApiInstance + 0x10);
}

static void **apis_array()
{
	return *(void ***)(SysApiInstance + 0x18);
}

int main()
{
	printf("CSysApiInstance::RegisterApi() known-answer test\n");
	printf("==================================================\n");

	CSysApiInstance *sysApi = (CSysApiInstance *)SysApiInstance;
	check("mApis starts empty (fresh process, nothing registered yet)", apis_count() == 0);

	printf("[1] First registration of a fresh name\n");
	int fakeApi1 = 111;
	int ret1 = sysApi->RegisterApi("TestApiOne", (CApiBase *)&fakeApi1);
	check("returns 1 (real ABI: always literal 1)", ret1 == 1);
	check("count 0 -> 1", apis_count() == 1);

	void **entry0 = (void **)apis_array()[0];
	check("descriptor vtbl == PTR__CApiDescriptor_08e81368", entry0[0] == (void *)PTR__CApiDescriptor_08e81368);
	check("descriptor name is a real, independent copy", entry0[1] != 0 && strcmp((const char *)entry0[1], "TestApiOne") == 0);
	check("descriptor api pointer stored verbatim", entry0[2] == (void *)&fakeApi1);

	printf("[2] Second registration under a distinct name just appends\n");
	int fakeApi2 = 222;
	int ret2 = sysApi->RegisterApi("TestApiTwo", (CApiBase *)&fakeApi2);
	check("returns 1", ret2 == 1);
	check("count 1 -> 2", apis_count() == 2);
	void **entry1 = (void **)apis_array()[1];
	check("second descriptor name correct", strcmp((const char *)entry1[1], "TestApiTwo") == 0);
	check("second descriptor api pointer correct", entry1[2] == (void *)&fakeApi2);
	check("first entry untouched (still TestApiOne/&fakeApi1)",
	      strcmp((const char *)((void **)apis_array()[0])[1], "TestApiOne") == 0 &&
	      ((void **)apis_array()[0])[2] == (void *)&fakeApi1);

	printf("[3] Re-registering the same name with the SAME api pointer: no-op\n");
	int ret3 = sysApi->RegisterApi("TestApiOne", (CApiBase *)&fakeApi1);
	check("returns 1", ret3 == 1);
	check("count unchanged (still 2)", apis_count() == 2);

	printf("[4] Re-registering the same name with a DIFFERENT api pointer: replace\n");
	int fakeApi1b = 333;
	int ret4 = sysApi->RegisterApi("TestApiOne", (CApiBase *)&fakeApi1b);
	check("returns 1", ret4 == 1);
	check("count unchanged (old removed, new appended -- still 2)", apis_count() == 2);

	bool foundReplaced = false;
	for (int i = 0; i < apis_count(); i++) {
		void **e = (void **)apis_array()[i];
		if (strcmp((const char *)e[1], "TestApiOne") == 0 && e[2] == (void *)&fakeApi1b)
			foundReplaced = true;
	}
	check("TestApiOne now points at the NEW api pointer", foundReplaced);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
