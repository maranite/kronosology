/*
 * test_module_manager_add_module.cpp  -  host-side known-answer test for
 * CModuleManager::AddModule()/EnableUpdate() (src/base/module_manager.cpp, Stage 6
 * breadth sweep, 2026-07-25 -- upgraded from Tier-B link-stubs to Tier A).
 *
 * Builds a raw 0x44-byte CModuleManager blob the same way ckernel.cpp's real
 * CKernel::CKernel() does (2 embedded COmegaPtrArray sub-objects, vtable-swapped),
 * then drives AddModule()/Setup() against a handful of fake CModule-shaped objects
 * to check:
 *   - first AddModule() on an empty mModules just appends (no search performed)
 *   - a second AddModule() with a distinct name also just appends
 *   - AddModule() with a name matching an existing entry removes the old entry
 *     first, then appends the new one (real by-name re-registration behavior)
 *   - mBusy (+0x00) is cleared after every AddModule()
 *   - EnableUpdate() unconditionally sets mTopologyChanged (+0x40); only notifies
 *     the host (via CSysApiInstance::WriteMessageToHost(), promoted to Tier A
 *     2026-07-26 -- now checked directly via a fake g_poHostInterface vtable
 *     object, not just indirectly via mBusy/no-crash) when enable != 0
 *   - SAFETY-CRITICAL regression check: once AddModule() genuinely populates
 *     mModules, CModuleManager::Setup() dispatches each module's real vtable slot
 *     +8 exactly once per real module -- this is the exact scenario that would
 *     crash if any of mains.cpp's 8 real MMainXxx callers still installed a NULL
 *     placeholder vtable (see omega_vtables.h's "AddModule()/EnableUpdate() batch"
 *     note) or left a module's +4 name pointer uninitialized (see mains.cpp's
 *     CFileMan/CResMan fix) -- this test proves the general mechanism is safe end
 *     to end using a stand-in vtable, the live kronos_vm boot proves the real
 *     mains.cpp objects are too.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>

#include "module_manager.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "ckernel.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static int g_setupCalls;
static int g_configCalls;
static int g_startCalls;

extern "C" void FakeDtor(void *) {}
extern "C" void FakeSetup(void *) { g_setupCalls++; }
extern "C" void FakeConfig(void *) { g_configCalls++; }
extern "C" void FakeStart(void *) { g_startCalls++; }

/* Byte-offset-matched stand-in vtable: 0/4 = dtor pair, +8 = Setup, +0xc = Config,
 * +0x10 = Start -- same 5 slots CModuleManager::Setup/Config/Start dispatch through,
 * matching CModule's own real 7-slot layout for the first 5.
 */
static void *g_fakeModuleVtbl[5] = {
	(void *)FakeDtor, (void *)FakeDtor, (void *)FakeSetup, (void *)FakeConfig, (void *)FakeStart,
};

/* Fake g_poHostInterface stand-in (ckernel.h) -- CSysApiInstance::WriteMessageToHost()
 * (promoted Tier B -> Tier A 2026-07-26) faithfully has NO NULL check on
 * g_poHostInterface before dispatching its vtable slot +0xc, matching ground truth
 * exactly. This test's own EnableUpdate(1)/mStarted==1 path reaches that call
 * (module_manager.cpp), so g_poHostInterface must point at a real vtable-shaped
 * object here or the test itself would crash -- same "size is not depth" test-setup
 * gap this whole recheck sweep is about, just on the test side rather than the
 * production code side.
 */
static int g_hostMessageCalls;
static char g_lastHostMessage[0x20];
extern "C" void FakeHostWriteMessage(void *, const char *msg)
{
	/* Copy, not just save the pointer -- msg points into WriteMessageToHost()'s own
	 * stack buffer, which is invalid the instant this call returns. */
	g_hostMessageCalls++;
	strncpy(g_lastHostMessage, msg, sizeof(g_lastHostMessage) - 1);
	g_lastHostMessage[sizeof(g_lastHostMessage) - 1] = '\0';
}
static void *g_fakeHostVtbl[4] = {
	(void *)0, (void *)0, (void *)0, (void *)FakeHostWriteMessage, /* slot +0xc */
};
/* The real g_poHostInterface points at an OBJECT whose first word is its vtable
 * pointer -- not at the vtable array itself. */
static void *g_fakeHostInstance = g_fakeHostVtbl;
static void *g_fakeHostObj = &g_fakeHostInstance;

/* Real CModule layout: vtbl(+0)/mName(+4)/mTasks(+8, 0x18 bytes)/mUnknown20(+0x20)/
 * mState(+0x24)/mScopeId(+0x28) -- only vtbl/mName/mState are touched by anything
 * under test here; the rest is padding to keep the real +0x24 mState offset correct
 * (CModuleManager::Setup/Config/Start's own lifecycle gate).
 */
struct FakeModule {
	void *vtbl;    /* +0x00 */
	char *name;    /* +0x04 */
	char  pad[0x1c]; /* +0x08 .. +0x24 */
	int   state;   /* +0x24 */
};

static FakeModule *make_module(const char *name)
{
	FakeModule *m = (FakeModule *)malloc(sizeof(FakeModule));
	memset(m, 0, sizeof(*m));
	m->vtbl = g_fakeModuleVtbl;
	m->name = strdup(name);
	m->state = 0;
	return m;
}

/* Same raw-blob construction ckernel.cpp's real CKernel::CKernel() uses for
 * g_poModuleManager (module_manager.h's own header comment) -- 2 embedded
 * COmegaPtrArray sub-objects at dword index 1 (mModules) and 7 (mConstructors),
 * default-constructed then vtable-swapped.
 */
static CModuleManager *make_module_manager()
{
	unsigned *mm = (unsigned *)malloc(0x44);
	memset(mm, 0, 0x44);
	mm[0] = 0;
	new (mm + 1) COmegaPtrArray();
	mm[1] = (unsigned)PTR__TNamedPtrArray_08e80c10;
	new (mm + 7) COmegaPtrArray();
	mm[7] = (unsigned)PTR__TNamedPtrArray_08e80bf8;
	mm[0xd] = 0;
	mm[0xe] = 0;
	mm[0xf] = 0;
	mm[0x10] = 0;
	return (CModuleManager *)mm;
}

static int mm_count(CModuleManager *mgr)
{
	return *(int *)((char *)mgr + 0x10);
}

static void **mm_array(CModuleManager *mgr)
{
	return *(void ***)((char *)mgr + 0x18);
}

int main(void)
{
	printf("CModuleManager::AddModule()/EnableUpdate() known-answer test\n");
	printf("==============================================================\n");

	CModuleManager *mgr = make_module_manager();

	printf("[1] AddModule() on an empty mModules just appends\n");
	FakeModule *m1 = make_module("EditMan");
	*(char *)mgr = 1; /* seed mBusy nonzero so we can observe it being cleared */
	mgr->AddModule((CModule *)m1);
	check("count == 1", mm_count(mgr) == 1);
	check("array[0] == m1", mm_array(mgr)[0] == (void *)m1);
	check("mBusy cleared", *(char *)mgr == 0);

	printf("[2] AddModule() with a distinct name also just appends\n");
	FakeModule *m2 = make_module("ViewBase");
	mgr->AddModule((CModule *)m2);
	check("count == 2", mm_count(mgr) == 2);
	check("array[1] == m2", mm_array(mgr)[1] == (void *)m2);

	printf("[3] AddModule() with a name matching an existing entry removes the old\n"
	       "    entry first, then appends the new one\n");
	FakeModule *m3 = make_module("EditMan"); /* same name as m1 */
	mgr->AddModule((CModule *)m3);
	check("count stays 2 (m1 removed, m3 appended)", mm_count(mgr) == 2);
	check("m1 no longer present", mm_array(mgr)[0] != (void *)m1 && mm_array(mgr)[1] != (void *)m1);
	check("m3 present", mm_array(mgr)[0] == (void *)m3 || mm_array(mgr)[1] == (void *)m3);
	check("m2 still present", mm_array(mgr)[0] == (void *)m2 || mm_array(mgr)[1] == (void *)m2);

	printf("[4] SAFETY-CRITICAL: Setup() over a genuinely-populated mModules dispatches\n"
	       "    each module's real vtable slot +8 exactly once, no crash\n");
	mgr->Setup();
	check("Setup() dispatched exactly twice (m2 + m3, 2 live modules)", g_setupCalls == 2);
	check("m2 state advanced to 1", *(int *)((char *)m2 + 0x24) == 1);
	check("m3 state advanced to 1", *(int *)((char *)m3 + 0x24) == 1);

	printf("[5] Config() likewise, gated on state < 2\n");
	mgr->Config();
	check("Config() dispatched exactly twice", g_configCalls == 2);

	printf("[6] EnableUpdate(0): mTopologyChanged set, but mBusy/notify path skipped\n");
	*(char *)mgr = 1; /* seed mBusy nonzero */
	mgr->EnableUpdate(0);
	check("mTopologyChanged (+0x40) == 1", *(int *)((char *)mgr + 0x40) == 1);
	check("mBusy untouched (enable == 0)", *(char *)mgr == 1);

	/* WriteMessageToHost() is now real Tier A (2026-07-26) -- point g_poHostInterface
	 * at the fake vtable object above before either EnableUpdate() call can reach it. */
	g_poHostInterface = g_fakeHostObj;

	printf("[7] EnableUpdate(1) with mStarted == 0: mBusy cleared, no host notify attempted\n"
	       "    (checked directly now that WriteMessageToHost is real -- g_hostMessageCalls\n"
	       "    must stay 0)\n");
	*(char *)mgr = 1;
	*(int *)((char *)mgr + 0x3c) = 0; /* mStarted == 0 */
	mgr->EnableUpdate(1);
	check("mBusy cleared (enable != 0)", *(char *)mgr == 0);
	check("no host notify when mStarted == 0", g_hostMessageCalls == 0);

	printf("[8] EnableUpdate(1) with mStarted == 1: takes the WriteMessageToHost branch,\n"
	       "    real notification observed via the fake host-interface vtable\n");
	*(int *)((char *)mgr + 0x3c) = 1; /* mStarted == 1 */
	mgr->EnableUpdate(1);
	check("survived the WriteMessageToHost call path", true);
	check("host notify fired exactly once", g_hostMessageCalls == 1);
	check("host message is \"3\\x0c8\\r\" (a=3, b=8)",
	      strcmp(g_lastHostMessage, "3\x0c""8\r") == 0);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
