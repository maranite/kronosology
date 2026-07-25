/*
 * test_config_manager_create_modules.cpp  -  host-side known-answer test for
 * CModuleManager::AddConstructor()/RemoveConstructor() (src/base/module_manager.cpp)
 * and CConfigManager::CreateUserModules()/CreateFMDrivers() (src/init/config_manager.cpp),
 * Stage 6 breadth sweep follow-up batch, 2026-07-25.
 *
 * This is the batch that reconstructs the "distinct module factory array sub-
 * structure inside CModuleManager" (g_poModuleManager+0x28/+0x30, mConstructors)
 * this project's README/PLAN previously flagged as unreconstructed. Covers:
 *   [1] AddConstructor()/RemoveConstructor(): same by-name dedup/removal shape as
 *       AddModule(), operating on mConstructors instead of mModules.
 *   [2] The real vtable-slot fix: PTR__CSysApiInstance_08e81008[16] (byte offset
 *       0x40) is now AddConstructorVSlot, not the dead EvaVTableStub no-op --
 *       confirmed both by direct address comparison and by driving
 *       CSysApiInstance::AddConstructor() itself.
 *   [3] CreateUserModules(): real by-name lookup into mConstructors + factory
 *       Create() dispatch + direct COmegaPtrArray::Add() into mModules (NOT via
 *       CModuleManager::AddModule()) -- success, "factory not found", and "Create()
 *       returned NULL" paths.
 *   [4] CreateFMDrivers(): real FMApi-vtable-dispatched factory lookup (+0x2c) +
 *       Create() + register (+0x30), destroying the driver (vtable slot+4) if
 *       registration reports failure -- all 4 branches.
 *   [5] Both CreateUserModules()/CreateFMDrivers() are safe, real no-ops when the
 *       table's own first entry name is null (today's real placeholder convention,
 *       config_info.cpp) -- regression check for the "safe to zero" property this
 *       batch's own header comment relies on.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>

#include "module_manager.h"
#include "config_manager.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "system_api.h"
#include "sysapi_instance.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Real globals, linked in from mains.cpp/sysapi_instance.cpp (make verify links all
 * of $(OBJ) into each test binary, same pattern as test_config_manager_boot_slice.cpp).
 */
extern CSystemApi *Api;
extern CSystemApi *FMApi;
extern "C" void AddConstructorVSlot(void *, void *);

/* Same raw-blob CModuleManager construction as test_module_manager_add_module.cpp. */
static CModuleManager *make_module_manager()
{
	unsigned *mm = (unsigned *)malloc(0x44);
	memset(mm, 0, 0x44);
	new (mm + 1) COmegaPtrArray();
	mm[1] = (unsigned)PTR__TNamedPtrArray_08e80c10;
	new (mm + 7) COmegaPtrArray();
	mm[7] = (unsigned)PTR__TNamedPtrArray_08e80bf8;
	return (CModuleManager *)mm;
}

static int mods_count(CModuleManager *mgr) { return *(int *)((char *)mgr + 0x10); }
static void **mods_array(CModuleManager *mgr) { return *(void ***)((char *)mgr + 0x18); }
static int ctors_count(CModuleManager *mgr) { return *(int *)((char *)mgr + 0x28); }
static void **ctors_array(CModuleManager *mgr) { return *(void ***)((char *)mgr + 0x30); }

/* Fake CModuleConstructor-shaped object: {vtbl, name, counter}, exact same 3-word
 * shape mains.cpp's RegisterModuleDescriptor() builds. vtbl slot+8 = "Create".
 */
static int g_createCalls;
static void *g_lastCreateArg1, *g_lastCreateArg2;
static int g_lastCreateCounter;
static void *g_createReturnValue = (void *)0x1234; /* non-null by default */

extern "C" void *FakeCreate(void *self, void *p1, void *p2, int counter)
{
	(void)self;
	g_createCalls++;
	g_lastCreateArg1 = p1;
	g_lastCreateArg2 = p2;
	g_lastCreateCounter = counter;
	return g_createReturnValue;
}

extern "C" void FakeCtorDtor(void *) {}

static void *g_fakeCtorVtbl[3] = { (void *)FakeCtorDtor, (void *)FakeCtorDtor, (void *)FakeCreate };

struct FakeConstructor {
	void *vtbl;
	char *name;
	int counter;
};

static FakeConstructor *make_constructor(const char *name)
{
	FakeConstructor *c = (FakeConstructor *)malloc(sizeof(FakeConstructor));
	c->vtbl = g_fakeCtorVtbl;
	c->name = strdup(name);
	c->counter = 0;
	return c;
}

/* Fake FMApi object -- vtbl slot+0x2c = "get driver factory by name", slot+0x30 =
 * "register driver". Reuses the same FakeConstructor/FakeCreate shape for the
 * "factory" FMApi's own +0x2c hands back (same real idiom -- both CreateUserModules()
 * and CreateFMDrivers() dispatch through a vtbl-slot+8 Create() on whatever they get
 * back).
 */
static int g_fmGetFactoryCalls;
static const char *g_lastFactoryName;
static void *g_fmFactoryReturnValue;

extern "C" void *FakeFMGetFactory(void *self, const char *name)
{
	(void)self;
	g_fmGetFactoryCalls++;
	g_lastFactoryName = name;
	return g_fmFactoryReturnValue;
}

static int g_fmRegisterCalls;
static void *g_lastRegisterDriver, *g_lastRegisterArg;
static int g_fmRegisterReturnValue = 1; /* success by default */

extern "C" int FakeFMRegister(void *self, void *driver, void *arg)
{
	(void)self;
	g_fmRegisterCalls++;
	g_lastRegisterDriver = driver;
	g_lastRegisterArg = arg;
	return g_fmRegisterReturnValue;
}

static int g_driverDtorCalls;
extern "C" void FakeDriverDtor(void *) { g_driverDtorCalls++; }

/* Fake FMApi vtbl: only slots +0x2c/+0x30 (0xb/0xc dwords) matter; sized generously
 * past that so raw offset reads stay in-bounds.
 */
static void *g_fakeFMApiVtbl[16] = {};

struct FakeFMApi { void *vtbl; };
static FakeFMApi g_fakeFMApi;

/* Fake driver object returned by Create(): {vtbl}, vtbl slot+4 = deleting dtor. */
static void *g_fakeDriverVtbl[2] = { (void *)FakeDriverDtor, (void *)FakeDriverDtor };
struct FakeDriver { void *vtbl; };

int main(void)
{
	printf("CModuleManager::AddConstructor()/RemoveConstructor() +\n"
	       "CConfigManager::CreateUserModules()/CreateFMDrivers() known-answer test\n"
	       "=========================================================================\n");

	CModuleManager *mgr = make_module_manager();

	printf("[1] AddConstructor()/RemoveConstructor(): same by-name dedup shape as AddModule()\n");
	FakeConstructor *c1 = make_constructor("EditorClass");
	mgr->AddConstructor((CModuleConstructor *)c1);
	check("count == 1", ctors_count(mgr) == 1);
	check("array[0] == c1", ctors_array(mgr)[0] == (void *)c1);

	FakeConstructor *c2 = make_constructor("PanelClass");
	mgr->AddConstructor((CModuleConstructor *)c2);
	check("count == 2", ctors_count(mgr) == 2);

	FakeConstructor *c3 = make_constructor("EditorClass"); /* same name as c1 */
	mgr->AddConstructor((CModuleConstructor *)c3);
	check("count stays 2 (c1 removed, c3 appended)", ctors_count(mgr) == 2);
	check("c1 no longer present",
	      ctors_array(mgr)[0] != (void *)c1 && ctors_array(mgr)[1] != (void *)c1);
	check("c3 present", ctors_array(mgr)[0] == (void *)c3 || ctors_array(mgr)[1] == (void *)c3);
	check("c2 still present", ctors_array(mgr)[0] == (void *)c2 || ctors_array(mgr)[1] == (void *)c2);

	mgr->RemoveConstructor((CModuleConstructor *)c2);
	check("RemoveConstructor(c2): count back to 1", ctors_count(mgr) == 1);
	check("remaining entry is c3", ctors_array(mgr)[0] == (void *)c3);

	mgr->RemoveConstructor((CModuleConstructor *)c2); /* already removed -- no-op, no crash */
	check("RemoveConstructor() on an already-absent name is a safe no-op",
	      ctors_count(mgr) == 1);

	printf("[2] Real vtable-slot fix: Api's own slot +0x40 (index 16) is AddConstructorVSlot,\n"
	       "    not the dead EvaVTableStub no-op\n");
	check("PTR__CSysApiInstance_08e81008[16] == AddConstructorVSlot",
	      PTR__CSysApiInstance_08e81008[16] == (void *)AddConstructorVSlot);

	g_poModuleManager = mgr;
	FakeConstructor *c4 = make_constructor("BatchDiskManClass");
	((CSysApiInstance *)Api)->AddConstructor((CModuleConstructor *)c4);
	check("CSysApiInstance::AddConstructor() forwards to CModuleManager::AddConstructor()",
	      ctors_count(mgr) == 2 &&
	      (ctors_array(mgr)[0] == (void *)c4 || ctors_array(mgr)[1] == (void *)c4));

	/* mgr now holds {c3="EditorClass", c4="BatchDiskManClass"} -- reused below. */

	printf("[3] CConfigManager::CreateUserModules()\n");

	struct Entry { const char *name; void *p1; void *p2; };

	printf("  [3a] success path: factory found in mConstructors, Create() succeeds,\n"
	       "       result added straight into mModules (not via AddModule())\n");
	{
		Entry table[2] = {
			{ "EditorClass", (void *)0x1111, (void *)0x2222 },
			{ 0, 0, 0 },
		};
		CConfigManager::sm_ptCreateInfo = table;
		g_createCalls = 0;
		g_createReturnValue = (void *)0xabcd;
		int preCount = mods_count(mgr);

		CConfigManager::CreateUserModules();

		check("Create() dispatched exactly once", g_createCalls == 1);
		check("Create() got param1 == 0x1111", g_lastCreateArg1 == (void *)0x1111);
		check("Create() got param2 == 0x2222", g_lastCreateArg2 == (void *)0x2222);
		check("Create() got counter == 0 (c3's initial counter)", g_lastCreateCounter == 0);
		check("c3's own counter field incremented to 1", c3->counter == 1);
		check("mModules count grew by exactly 1", mods_count(mgr) == preCount + 1);
		check("new module == Create()'s return value",
		      mods_array(mgr)[mods_count(mgr) - 1] == (void *)0xabcd);
	}

	printf("  [3b] \"factory not found\" path: no matching name in mConstructors --\n"
	       "       ApiWarn1 fires (Api's own vtbl+0x90, EvaVTableStub-backed), no crash,\n"
	       "       mModules unchanged\n");
	{
		Entry table[2] = {
			{ "NoSuchModuleClass", (void *)0x3333, (void *)0x4444 },
			{ 0, 0, 0 },
		};
		CConfigManager::sm_ptCreateInfo = table;
		g_createCalls = 0;
		int preCount = mods_count(mgr);

		CConfigManager::CreateUserModules();

		check("Create() never dispatched", g_createCalls == 0);
		check("mModules unchanged", mods_count(mgr) == preCount);
	}

	printf("  [3c] \"Create() returned NULL\" path: factory found, but Create() fails --\n"
	       "       ApiWarn1 fires, module NOT added to mModules\n");
	{
		Entry table[2] = {
			{ "BatchDiskManClass", (void *)0x5555, (void *)0x6666 },
			{ 0, 0, 0 },
		};
		CConfigManager::sm_ptCreateInfo = table;
		g_createCalls = 0;
		g_createReturnValue = 0;
		int preCount = mods_count(mgr);

		CConfigManager::CreateUserModules();

		check("Create() dispatched once", g_createCalls == 1);
		check("mModules unchanged (Create() returned NULL)", mods_count(mgr) == preCount);
	}

	printf("  [3d] sm_ptCreateInfo == 0: real early-return, no crash\n");
	{
		CConfigManager::sm_ptCreateInfo = 0;
		CConfigManager::CreateUserModules();
		check("survived sm_ptCreateInfo == 0", true);
	}

	printf("  [3e] real placeholder convention: first entry's name is null ->\n"
	       "       real, safe no-op (regression check for config_info.cpp's own\n"
	       "       zero-initialized s_tConfigInfo_placeholder)\n");
	{
		unsigned char zeroTable[4] = {};
		CConfigManager::sm_ptCreateInfo = zeroTable;
		g_createCalls = 0;
		CConfigManager::CreateUserModules();
		check("loop body never entered (first name field is null)", g_createCalls == 0);
	}

	printf("[4] CConfigManager::CreateFMDrivers()\n");

	g_fakeFMApiVtbl[0xb] = (void *)FakeFMGetFactory; /* +0x2c */
	g_fakeFMApiVtbl[0xc] = (void *)FakeFMRegister;   /* +0x30 */
	g_fakeFMApi.vtbl = g_fakeFMApiVtbl;
	FMApi = (CSystemApi *)&g_fakeFMApi;

	FakeConstructor *driverFactory = make_constructor("LinuxDriverClass");

	printf("  [4a] success path: factory found, Create() succeeds, register() succeeds --\n"
	       "       driver NOT destroyed\n");
	{
		Entry table[2] = {
			{ "LinuxDriverClass", (void *)0x7777, (void *)0x8888 },
			{ 0, 0, 0 },
		};
		CConfigManager::sm_ptFMDriverInfo = table;
		g_fmGetFactoryCalls = 0;
		g_createCalls = 0;
		g_fmRegisterCalls = 0;
		g_driverDtorCalls = 0;
		g_fmFactoryReturnValue = driverFactory;
		g_createReturnValue = 0; /* overwritten below with a real FakeDriver */

		FakeDriver driver;
		driver.vtbl = g_fakeDriverVtbl;
		g_createReturnValue = &driver;
		g_fmRegisterReturnValue = 1;

		CConfigManager::CreateFMDrivers();

		check("FMApi factory lookup dispatched once", g_fmGetFactoryCalls == 1);
		check("factory lookup got name == \"LinuxDriverClass\"",
		      strcmp(g_lastFactoryName, "LinuxDriverClass") == 0);
		check("Create() dispatched once", g_createCalls == 1);
		check("Create() got param1 == 0x7777", g_lastCreateArg1 == (void *)0x7777);
		check("Create() got param2 == 0x8888", g_lastCreateArg2 == (void *)0x8888);
		check("driverFactory's own counter incremented to 1", driverFactory->counter == 1);
		check("FMApi register dispatched once", g_fmRegisterCalls == 1);
		check("register got the created driver", g_lastRegisterDriver == (void *)&driver);
		check("register got param2 == 0x8888", g_lastRegisterArg == (void *)0x8888);
		check("driver NOT destroyed (registration succeeded)", g_driverDtorCalls == 0);
	}

	printf("  [4b] register() fails: driver IS destroyed (vtbl slot+4)\n");
	{
		Entry table[2] = {
			{ "LinuxDriverClass", (void *)0x9999, (void *)0xaaaa },
			{ 0, 0, 0 },
		};
		CConfigManager::sm_ptFMDriverInfo = table;
		g_driverDtorCalls = 0;
		FakeDriver driver;
		driver.vtbl = g_fakeDriverVtbl;
		g_createReturnValue = &driver;
		g_fmRegisterReturnValue = 0; /* registration fails */

		CConfigManager::CreateFMDrivers();

		check("driver destroyed once (registration failed)", g_driverDtorCalls == 1);
	}

	printf("  [4c] factory not found: ApiWarn1 fires, loop continues (no crash)\n");
	{
		Entry table[2] = {
			{ "NoSuchDriverClass", (void *)0xbbbb, (void *)0xcccc },
			{ 0, 0, 0 },
		};
		CConfigManager::sm_ptFMDriverInfo = table;
		g_fmFactoryReturnValue = 0;
		g_createCalls = 0;

		CConfigManager::CreateFMDrivers();

		check("Create() never dispatched (no factory)", g_createCalls == 0);
	}

	printf("  [4d] Create() fails: ApiWarn1 fires, register() never attempted\n");
	{
		Entry table[2] = {
			{ "LinuxDriverClass", (void *)0xdddd, (void *)0xeeee },
			{ 0, 0, 0 },
		};
		CConfigManager::sm_ptFMDriverInfo = table;
		g_fmFactoryReturnValue = driverFactory;
		g_createReturnValue = 0;
		g_fmRegisterCalls = 0;

		CConfigManager::CreateFMDrivers();

		check("register() never dispatched (Create() returned NULL)", g_fmRegisterCalls == 0);
	}

	printf("  [4e] sm_ptFMDriverInfo == 0: real early-return, no crash\n");
	{
		CConfigManager::sm_ptFMDriverInfo = 0;
		CConfigManager::CreateFMDrivers();
		check("survived sm_ptFMDriverInfo == 0", true);
	}

	printf("  [4f] real placeholder convention: first entry's name is null -> real,\n"
	       "       safe no-op (regression check for config_info.cpp's own\n"
	       "       zero-initialized s_atFMDriverInfo_placeholder)\n");
	{
		unsigned char zeroTable[0x2c] = {};
		CConfigManager::sm_ptFMDriverInfo = zeroTable;
		g_fmGetFactoryCalls = 0;
		CConfigManager::CreateFMDrivers();
		check("loop body never entered (first name field is null)", g_fmGetFactoryCalls == 0);
	}

	/* Restore static globals to a harmless state -- not load-bearing for this
	 * process (it exits right after), but keeps the test's own intent clear.
	 */
	CConfigManager::sm_ptCreateInfo = 0;
	CConfigManager::sm_ptFMDriverInfo = 0;
	FMApi = 0;
	g_poModuleManager = 0;

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
