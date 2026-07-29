/*
 * test_file_man_res_man.cpp  -  host-side known-answer test for the CFileMan/CResMan
 * ctor batch (Stage 6 breadth sweep, 2026-07-25 -- the "What's still open"
 * CFileMan/CResMan ctor batch, file_man.h/res_man.h/chunk_on_demand.h/cz_util.h),
 * plus the new COmegaPtrArray::RemoveAll() method it needed.
 *
 * Checks:
 *   [1] CZ::StrCmpIgnoreCase(): case-insensitive equal/less/greater/NULL/prefix cases
 *   [2] COmegaPtrArray::RemoveAll(): with callback (fires per-element, in pop-from-end
 *       order) and without (silent clear); both leave the array empty + mArray NULL
 *   [3] CChunkOnDemand: ctor doesn't crash, sizeof == 0x20 (32) bytes
 *   [4] CFileMan::CFileMan(): sizeof == 0xa5c; with a fake Api providing "Enabled" /
 *       numeric config strings, mBackgroundJobsEnabled/mMinIdleTimeToStartBGJobs/
 *       mDeltaTimeBetweenBGJobs all take the config-string branch (raw-offset read,
 *       cross-checked against the 2 public accessors for the one field that has them)
 *   [5] CFileMan::CFileMan(): with a fake Api returning NULL for every key, all 3
 *       fields fall back to their real defaults (0 / 1000 / 800)
 *   [6] CFileMan::Setup()/Config()/Start(): all confirmed no-ops (return immediately,
 *       don't touch any field)
 *   [7] CResMan::CResMan(): sizeof == 0x21a0; doesn't crash (no Api dependency at all);
 *       raw-offset spot checks match the real ctor's own writes (+0x34 == -1,
 *       +0x68..+0x6a == 0xff, +0x20b4 TVector vtable installed); mJob is now a
 *       REAL placement-constructed CRMJob (Eva "size is not depth" re-check batch,
 *       2026-07-26), confirmed via CRMJob's own ctor field writes read back through
 *       ResManTestHooks
 *   [8] CResMan::Start(): confirmed no-op
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>

#include "module.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "chunk_on_demand.h"
#include "cz_util.h"
#include "file_man.h"
#include "res_man.h"
#include "res_table.h"
#include "res_family.h"
#include "system_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

extern CSystemApi *Api; /* real global, mains.cpp */

/* --- [2] RemoveAll() callback-order tracking --- */
static int g_freeOrder[8];
static int g_freeCount;

struct FakeElem {
	void *vtbl;
	int   id;
};
static void FakeFreeElement(void *self, void *elem)
{
	(void)self;
	g_freeOrder[g_freeCount++] = static_cast<FakeElem *>(elem)->id;
}
typedef void (*FreeFn)(void *, void *);
static void *g_fakeArrayVtbl[3] = { 0, 0, (void *)(FreeFn)FakeFreeElement };

/* Raw-offset test hook, same idiom as test_small_modules.cpp's ModuleTestHooks /
 * test_dump_manager.cpp -- COmegaPtrArray's own real layout (omega_ptr_array.h) is
 * {vtbl, mUnknown04, mCapacity, mCount, mGrowBy, mArray}, all 4-byte fields.
 */
struct PtrArrayRaw {
	static void *Vtbl(const COmegaPtrArray &a) { return *(void *const *)&a; }
	static int Count(const COmegaPtrArray &a) { return *(const int *)((const char *)&a + 0xc); }
	static int Capacity(const COmegaPtrArray &a) { return *(const int *)((const char *)&a + 8); }
	static void *Array(const COmegaPtrArray &a) { return *(void *const *)((const char *)&a + 0x14); }
};

/* --- [4]/[5] fake Api config-string getter --- */
static const char *g_cfgKeys[3];
static const char *g_cfgVals[3];
static int g_cfgN;

static char *FakeGetConfigString(void *api, const char *key)
{
	(void)api;
	for (int i = 0; i < g_cfgN; i++) {
		if (strcmp(g_cfgKeys[i], key) == 0)
			return const_cast<char *>(g_cfgVals[i]);
	}
	return 0;
}
typedef char *(*GetCfgFn)(void *, const char *);

/* CModule::CModule() (module.cpp) unconditionally dispatches Api's own vtable slot
 * +0x3c ("per-object scope id assignment", system_api.h) for every module ctor,
 * CFileMan included -- must be stubbed too, or CFileMan::CFileMan()'s own
 * `CModule(CFileMan_SysName)` base-ctor call crashes on a NULL slot.
 */
static int FakeGetScopeId(void *api)
{
	(void)api;
	return 0;
}
typedef int (*GetScopeIdFn)(void *);

/* Round 55 batch: SetLoadRes()/TestAndSetBusy()/UnPrepareDestMap()/IsOnDemand()'s
 * own Api+0x94 soft-assert-report call. Matches res_entry.cpp's own ApiAssert()
 * shape exactly; just needs to not crash for these KAT purposes.
 */
static int g_assertCount;
static void FakeAssert(void *api, const char *fmt, const char *file, int line)
{
	(void)api; (void)fmt; (void)file; (void)line;
	g_assertCount++;
}
typedef void (*AssertFn)(void *, const char *, const char *, int);

/* Slot +0x38 = index 14, +0x3c = index 15, +0x94 = index 37 -- sized generously
 * past that, matching this project's own "properly-sized fake vtable" KAT
 * convention (see out_link.h's header note on the "96-slot-fake-Api-vtable"
 * gotcha).
 */
static void *g_fakeApiVtbl[40] = { 0 };
struct FakeApiObj {
	void *vtbl;
};
static FakeApiObj g_fakeApi;

/* Raw-offset spot checks for CFileMan/CResMan -- both classes derive from CModule
 * (base size 0x2c, module.h), so `(const char*)obj + 0x2c + N` reaches the derived
 * class' own Nth byte, matching each header's own documented layout exactly.
 */
static int I32(const void *obj, int off) { return *(const int *)((const char *)obj + off); }
static unsigned char U8(const void *obj, int off) { return *((const unsigned char *)obj + off); }
static void *PTR(const void *obj, int off) { return *(void *const *)((const char *)obj + off); }

/* Friend accessor for CResMan::mJob -- Eva "size is not depth" re-check batch,
 * 2026-07-26. Lets this KAT confirm mJob now points at a REAL, placement-
 * constructed CRMJob (rm_job.h) rather than raw malloc() garbage.
 */
struct ResManTestHooks {
	static CRMJob *Job(CResMan *rm) { return rm->mJob; }
};

int main()
{
	printf("CFileMan/CResMan ctor known-answer test\n");
	printf("========================================\n");

	/* --- [1] CZ::StrCmpIgnoreCase --- */
	printf("[1] CZ::StrCmpIgnoreCase()\n");
	{
		check("\"Enabled\" vs \"Enabled\" == 0", CZ::StrCmpIgnoreCase("Enabled", "Enabled") == 0);
		check("\"enabled\" vs \"ENABLED\" == 0 (case-insensitive)",
		      CZ::StrCmpIgnoreCase("enabled", "ENABLED") == 0);
		check("\"Disabled\" vs \"Enabled\" != 0", CZ::StrCmpIgnoreCase("Disabled", "Enabled") != 0);
		check("\"abc\" vs \"abd\" == 0xffffffff (less)",
		      CZ::StrCmpIgnoreCase("abc", "abd") == 0xffffffffu);
		check("\"abd\" vs \"abc\" == 1 (greater)", CZ::StrCmpIgnoreCase("abd", "abc") == 1);
		check("\"ab\" vs \"abc\" == 0xffffffff (shorter == less)",
		      CZ::StrCmpIgnoreCase("ab", "abc") == 0xffffffffu);
		check("\"abc\" vs \"ab\" == 1 (longer == greater)",
		      CZ::StrCmpIgnoreCase("abc", "ab") == 1);
		check("NULL vs NULL == 0", CZ::StrCmpIgnoreCase(0, 0) == 0);
		check("NULL vs \"x\" == 0xffffffff", CZ::StrCmpIgnoreCase(0, "x") == 0xffffffffu);
		check("\"x\" vs NULL == 1", CZ::StrCmpIgnoreCase("x", 0) == 1);
	}

	/* --- [2] COmegaPtrArray::RemoveAll() --- */
	printf("[2] COmegaPtrArray::RemoveAll()\n");
	{
		COmegaPtrArray arr(1, 0, 1);
		*(void **)&arr = g_fakeArrayVtbl;
		FakeElem e0 = { 0, 100 }, e1 = { 0, 101 }, e2 = { 0, 102 };
		arr.Add(&e0);
		arr.Add(&e1);
		arr.Add(&e2);

		g_freeCount = 0;
		arr.RemoveAll(1);
		check("RemoveAll(1): callback fired 3 times", g_freeCount == 3);
		check("RemoveAll(1): pop-from-end order (102, 101, 100)",
		      g_freeOrder[0] == 102 && g_freeOrder[1] == 101 && g_freeOrder[2] == 100);
		check("RemoveAll(1): count == 0 after", PtrArrayRaw::Count(arr) == 0);
		check("RemoveAll(1): capacity == 0 after", PtrArrayRaw::Capacity(arr) == 0);
		check("RemoveAll(1): mArray == NULL after", PtrArrayRaw::Array(arr) == 0);

		COmegaPtrArray arr2(1, 0, 1);
		*(void **)&arr2 = g_fakeArrayVtbl;
		arr2.Add(&e0);
		arr2.Add(&e1);
		g_freeCount = 0;
		arr2.RemoveAll(0);
		check("RemoveAll(0): callback NOT fired", g_freeCount == 0);
		check("RemoveAll(0): count == 0 after (silent clear)", PtrArrayRaw::Count(arr2) == 0);
		check("RemoveAll(0): mArray == NULL after", PtrArrayRaw::Array(arr2) == 0);

		/* Redundant clear on an already-empty array (CResMan::CResMan()'s own real
		 * call shape) must not crash and must stay a no-op.
		 */
		COmegaPtrArray arr3(2, 0, 1);
		arr3.RemoveAll(1);
		check("RemoveAll(1) on an already-empty array is a safe no-op",
		      PtrArrayRaw::Count(arr3) == 0 && PtrArrayRaw::Array(arr3) == 0);
	}

	/* --- [3] CChunkOnDemand --- */
	printf("[3] CChunkOnDemand\n");
	{
		check("sizeof(CChunkOnDemand) == 0x20", sizeof(CChunkOnDemand) == 0x20);
		unsigned char raw[0x20];
		memset(raw, 0xcd, sizeof(raw));
		CChunkOnDemand *c = new (raw) CChunkOnDemand();
		(void)c;
		check("ctor doesn't crash", true);
	}

	/* --- [4] CFileMan::CFileMan() with config strings present --- */
	printf("[4] CFileMan::CFileMan() (config strings present)\n");
	{
		check("sizeof(CFileMan) == 0xa5c", sizeof(CFileMan) == 0xa5c);

		g_fakeApiVtbl[0x38 / 4] = (void *)(GetCfgFn)FakeGetConfigString;
		g_fakeApiVtbl[0x3c / 4] = (void *)(GetScopeIdFn)FakeGetScopeId;
		g_fakeApiVtbl[0x94 / 4] = (void *)(AssertFn)FakeAssert;
		g_fakeApi.vtbl = g_fakeApiVtbl;
		Api = reinterpret_cast<CSystemApi *>(&g_fakeApi);

		g_cfgN = 3;
		g_cfgKeys[0] = "FMBackGroundJobs";       g_cfgVals[0] = "enabled"; /* lowercase */
		g_cfgKeys[1] = "FMMinIdleTimeToStartBGJobs"; g_cfgVals[1] = "2500";
		g_cfgKeys[2] = "FMDeltaTimeBetweenBGJobs";   g_cfgVals[2] = "1234";

		unsigned char *raw = (unsigned char *)malloc(sizeof(CFileMan));
		CFileMan *fm = new (raw) CFileMan();

		check("IsBackgroundJobsEnabled() == 1 (case-insensitive \"enabled\" match)",
		      fm->IsBackgroundJobsEnabled() == 1);
		check("mMinIdleTimeToStartBGJobs == 2500 (raw offset +0xa54)", I32(fm, 0xa54) == 2500);
		check("mDeltaTimeBetweenBGJobs == 1234 (raw offset +0xa58)", I32(fm, 0xa58) == 1234);

		fm->EnableBackgroundJobs(0);
		check("EnableBackgroundJobs(0) -> IsBackgroundJobsEnabled() == 0",
		      fm->IsBackgroundJobsEnabled() == 0);

		free(raw);
	}

	/* --- [5] CFileMan::CFileMan() with no config strings (defaults) --- */
	printf("[5] CFileMan::CFileMan() (no config strings -- defaults)\n");
	{
		g_cfgN = 0; /* FakeGetConfigString now returns NULL for every key */

		unsigned char *raw = (unsigned char *)malloc(sizeof(CFileMan));
		CFileMan *fm = new (raw) CFileMan();

		check("IsBackgroundJobsEnabled() == 0 (default)", fm->IsBackgroundJobsEnabled() == 0);
		check("mMinIdleTimeToStartBGJobs == 1000 (default, raw offset +0xa54)",
		      I32(fm, 0xa54) == 1000);
		check("mDeltaTimeBetweenBGJobs == 800 (default, raw offset +0xa58)",
		      I32(fm, 0xa58) == 800);

		/* --- [6] Setup()/Config()/Start() confirmed no-ops --- */
		printf("[6] CFileMan::Setup()/Config()/Start() (confirmed empty)\n");
		int before54 = I32(fm, 0xa54), before58 = I32(fm, 0xa58);
		int beforeEnabled = fm->IsBackgroundJobsEnabled();
		fm->Setup();
		fm->Config();
		fm->Start();
		check("Setup()/Config()/Start() don't touch any field",
		      I32(fm, 0xa54) == before54 && I32(fm, 0xa58) == before58 &&
		      fm->IsBackgroundJobsEnabled() == beforeEnabled);

		free(raw);
	}

	/* --- [7] CResMan::CResMan() --- */
	printf("[7] CResMan::CResMan()\n");
	{
		check("sizeof(CResMan) == 0x21a0", sizeof(CResMan) == 0x21a0);

		unsigned char *raw = (unsigned char *)malloc(sizeof(CResMan));
		memset(raw, 0xcd, sizeof(CResMan));
		CResMan *rm = new (raw) CResMan();

		check("ctor doesn't crash (no Api dependency)", true);
		check("vtbl installed (PTR__CResMan_08e88b08)", PTR(rm, 0) == (void *)PTR__CResMan_08e88b08);
		check("+0x34 == -1 (raw ctor write)", I32(rm, 0x34) == -1);
		check("+0x68..+0x6a == 0xff (raw ctor write)",
		      U8(rm, 0x68) == 0xff && U8(rm, 0x69) == 0xff && U8(rm, 0x6a) == 0xff);
		check("+0x38 == 0 (raw ctor write, zeroed region)", I32(rm, 0x38) == 0);
		check("mResults vtbl installed at +0x78 (TPtrArray<CRMResult::SSingleError>)",
		      PTR(rm, 0x78) == (void *)PTR__TPtrArray_08e88bb8);
		check("mResults empty after RemoveAll(1)", I32(rm, 0x78 + 0xc) == 0);
		check("first TVector<CResEntryEx,1> vtbl installed at +0x20b4",
		      PTR(rm, 0x20b4) == (void *)PTR__TVector_08e88ba8);
		check("10th TVector<CResEntryEx,1> vtbl installed at +0x218c",
		      PTR(rm, 0x218c) == (void *)PTR__TVector_08e88ba8);

		/* mJob -- now real-constructed (Eva "size is not depth" re-check batch,
		 * 2026-07-26): CRMJob::CRMJob()'s own real field writes (rm_job.h) must
		 * show up inside the malloc'd block, not just a non-NULL pointer.
		 */
		CRMJob *job = ResManTestHooks::Job(rm);
		check("mJob non-NULL", job != 0);
		check("mJob->+0x40..+0x42 == 0xff (CRMJob ctor sentinel triple)",
		      U8(job, 0x40) == 0xff && U8(job, 0x41) == 0xff && U8(job, 0x42) == 0xff);
		check("mJob->+0x44 == -1 (CRMJob ctor)", I32(job, 0x44) == -1);
		check("mJob->+0x48 == 0 (CRMJob ctor)", I32(job, 0x48) == 0);
		check("mJob->+0x50 == 1 (CRMJob ctor)", I32(job, 0x50) == 1);

		/* --- [8] Start() confirmed no-op --- */
		printf("[8] CResMan::Start() (confirmed empty)\n");
		int before34 = I32(rm, 0x34);
		rm->Start();
		check("Start() doesn't touch any field", I32(rm, 0x34) == before34);

		free(raw);
	}

	/* --- Round 55 batch (2026-07-29, solo) --- */
	printf("[9] CFileMan round 55 batch\n");
	{
		unsigned char *raw = (unsigned char *)malloc(sizeof(CFileMan));
		CFileMan *fm = new (raw) CFileMan();

		CFileMan::UnitNameCompare("Foo", "foo"); /* real: void, calls CZ::StrCmpIgnoreCase
		                                           * but discards the result -- genuinely
		                                           * useless in the real binary, faithfully
		                                           * preserved; just confirm it doesn't crash */
		check("UnitNameCompare: doesn't crash (real function discards its own comparison)", true);

		check("IsAccessDenied: requested bit not granted -> denied",
		      CFileMan::IsAccessDenied(1, 0) == true);
		check("IsAccessDenied: requested bit granted -> not denied (falls to bit2 check, unset)",
		      CFileMan::IsAccessDenied(1, 1) == false);
		check("IsAccessDenied: bit1 unset in granted -> denied",
		      CFileMan::IsAccessDenied(2, 0) == true);
		check("IsAccessDenied: neither bit0/1 requested, bit2 unset -> not denied",
		      CFileMan::IsAccessDenied(0, 0) == false);
		check("IsAccessDenied: bit2 requested and granted -> not denied",
		      CFileMan::IsAccessDenied(4, 4) == false);
		check("IsAccessDenied: bit2 requested, not granted -> denied",
		      CFileMan::IsAccessDenied(4, 0) == true);

		/* ctor already sets every mUnitTable[i]/mHandleTable[i]/mIOCTLDevTable[i]
		 * to {1,0,...} ("free") -- GetXxx() on a still-free slot returns 0.
		 */
		check("GetFile(): still-free slot -> 0", fm->GetFile(5) == 0);
		check("GetFile(): out-of-range index -> 0", fm->GetFile(200) == 0);
		check("GetUnitForModify(): still-free slot -> 0", fm->GetUnitForModify(5) == 0);
		check("GetIOCTLDev(): still-free slot -> 0 (unclaimed)", fm->GetIOCTLDev(3) == 0);
		check("GetIOCTLDev(): out-of-range index -> 0", fm->GetIOCTLDev(200) == 0);

		/* Directly claim mUnitTable[5]/mHandleTable[7]/mIOCTLDevTable[3] via the
		 * same {flag=0 (in use), value} shape the real ctor establishes, using
		 * the friend test-hook convention already used elsewhere in this file.
		 */
		*(int *)((unsigned char *)fm + 0x2c + 5 * 8) = 0;      /* mUnitTable[5].mField0 = 0 (in use) */
		*(int *)((unsigned char *)fm + 0x2c + 5 * 8 + 4) = 77; /* mUnitTable[5].mField4 = 77 */
		check("GetFile(): in-use slot -> stored value", fm->GetFile(5) == 77);

		*(int *)((unsigned char *)fm + 0x42c + 7 * 8) = 0;
		*(int *)((unsigned char *)fm + 0x42c + 7 * 8 + 4) = 88;
		check("GetUnitForModify(): in-use slot -> stored value", fm->GetUnitForModify(7) == 88);

		*(int *)((unsigned char *)fm + 0x82c + 3 * 0x10) = 0; /* mIOCTLDevTable[3].mField0 = 0 (in use) */
		void *dev = fm->GetIOCTLDev(3);
		check("GetIOCTLDev(): in-use slot -> address of that slot",
		      dev == (void *)((unsigned char *)fm + 0x82c + 3 * 0x10));

		check("RemoveIOCTLDev(): already-free slot -> 0 (no-op)", fm->RemoveIOCTLDev(9) == 0);
		check("RemoveIOCTLDev(): in-use slot -> 1, resets to {1,0,0,0}",
		      fm->RemoveIOCTLDev(3) == 1 && fm->GetIOCTLDev(3) == 0 &&
		      I32(fm, 0x82c + 3 * 0x10) == 1);

		free(raw);
	}

	printf("[10] CResMan round 55 batch\n");
	{
		unsigned char *raw = (unsigned char *)malloc(sizeof(CResMan));
		CResMan *rm = new (raw) CResMan();

		/* g_atResFamilies is a real global array; its own static-initializer
		 * ctor already ran before main() (CResFamily::CResFamily(), +0x24/+0x28/
		 * +0x2c all default to 1 -- res_family.cpp).
		 */
		check("IsAutoUnloadEnabled(): default (ctor sets +0x28 = 1)",
		      rm->IsAutoUnloadEnabled(0) == 1);
		check("IsOnDemand(): default (ctor sets +0x24 = 1, != 0 -> false)",
		      IsOnDemand(0) == false);

		g_assertCount = 0;
		rm->SetLoadRes(0, 4660 /* 0x1234 */);
		check("SetLoadRes(): writes g_atResFamilies[0]+0x2c",
		      *(int *)((unsigned char *)&g_atResFamilies[0] + 0x2c) == 4660);
		check("SetLoadRes(): in-range family -> no assert", g_assertCount == 0);

		g_assertCount = 0;
		rm->SetLoadRes(50, 1); /* out of range (>0x1f) -- soft assert, writes anyway (UB address in
		                          * real hardware; here g_atResFamilies is exactly 32 elements, so
		                          * this intentionally checks the assert fires, not the OOB write). */
		check("SetLoadRes(): out-of-range family -> soft assert fires", g_assertCount == 1);

		g_assertCount = 0;
		CRMApiCallBack *owner1 = reinterpret_cast<CRMApiCallBack *>(0x1234);
		bool claimed1 = rm->TestAndSetBusy(owner1);
		check("TestAndSetBusy(): first claim succeeds", claimed1 == true);
		check("TestAndSetBusy(): valid owner -> no assert", g_assertCount == 0);

		CRMApiCallBack *owner2 = reinterpret_cast<CRMApiCallBack *>(0x5678);
		bool claimed2 = rm->TestAndSetBusy(owner2);
		check("TestAndSetBusy(): already-busy -> second claim fails", claimed2 == false);

		g_assertCount = 0;
		rm->TestAndSetBusy(0);
		check("TestAndSetBusy(): NULL owner -> soft assert fires (still proceeds)", g_assertCount == 1);

		STriplet **map = (STriplet **)malloc(sizeof(STriplet *));
		*map = (STriplet *)malloc(sizeof(STriplet));
		g_assertCount = 0;
		rm->UnPrepareDestMap(map);
		check("UnPrepareDestMap(): frees *map and NULLs it", *map == 0);
		check("UnPrepareDestMap(): non-NULL input -> no assert", g_assertCount == 0);

		g_assertCount = 0;
		rm->UnPrepareDestMap(map); /* *map is already NULL now */
		check("UnPrepareDestMap(): NULL *map -> soft assert fires, still NULLs (no-op)",
		      g_assertCount == 1 && *map == 0);
		free(map);

		free(raw);
	}

	if (g_fail == 0)
		printf("\nall checks passed\n");
	else
		printf("\n%d check(s) FAILED\n", g_fail);

	return g_fail == 0 ? 0 : 1;
}
