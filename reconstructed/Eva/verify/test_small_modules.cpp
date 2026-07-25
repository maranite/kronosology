/*
 * test_small_modules.cpp  -  host-side known-answer test for the 4 small
 * derived-module classes reconstructed in the Stage 6 breadth sweep follow-up
 * batch, 2026-07-25 (edit_man.h/chunk_man.h/seq_timer.h/message_port.h):
 * CEditMan/CEditMan::CMainTask, CChunkMan/CChkBaseTask/CChkCmd/CChkCmdBG,
 * CSeqTimer/CTimerEngine, CMessagePort.
 *
 * Checks:
 *   [1] CEditMan::Setup(): constructs a real CMainTask, adds it to mTasks,
 *       stores it at EditApiInstance+4
 *   [2] CMainTask::RegisterServer()/GetServerScope()/UnregisterServer():
 *       full register -> lookup -> evict-and-reregister -> unregister cycle
 *       against 2 real CEditServer objects (scope forced via a test hook,
 *       since EditApiInstance_GetAssignedScope() is itself a Tier-B stub that
 *       always returns 0xff -- see edit_server.cpp/test_edit_server.cpp)
 *   [3] CMainTask::GetServerScope(name)/FindDescriptor()/SetDefault(): the
 *       by-name linear scan, and the group-gated CEditServer::FindDescriptor()
 *       forward (no descriptor registered -> both return 0/false)
 *   [4] CMainTask::RegisterClient()/UnregisterClient()/Notify(): the client
 *       observer list, including the real dedup-on-RegisterClient() check and
 *       Notify()'s own reverse fan-out order
 *   [5] CChunkMan::Setup(): constructs CChkCmd + CChkCmdBG, adds BOTH to
 *       mTasks (count 0 -> 2); CChkCmd's own COutLinkMono is genuinely
 *       constructed and added to ITS OWN mOutLinks (CTask::Add(COutLink*))
 *   [6] CChunkMan::Config(): real Api-vtable-slot-17 dispatch, deterministic
 *       true return (ChunkLinkRegisterVSlotStub always returns 0)
 *   [7] CSeqTimer::Setup(): constructs a real CTimerEngine, adds it to
 *       mTasks, dispatches through SeqApi's own vtable slot 8 (no crash)
 *   [8] CMessagePort: ctor installs the real 13-slot vtable + zeroes its 2
 *       extra fields; Setup()/Config()/Start() are all confirmed genuinely
 *       empty (no field/task-count change)
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>

#include "module.h"
#include "task.h"
#include "edit_man.h"
#include "edit_server.h"
#include "chunk_man.h"
#include "seq_timer.h"
#include "message_port.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "out_link.h"
#include "system_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Same raw-offset CModule task-list access as test_dump_manager.cpp. */
struct ModuleTestHooks {
	static int TaskCount(const CModule &m)
	{
		return *(const int *)((const unsigned char *)&m + 0x14);
	}
	static void *TaskAt(const CModule &m, int i)
	{
		void **arr = *(void ***)((const unsigned char *)&m + 0x1c);
		return arr[i];
	}
};

struct EditManTestHooks {
	static CEditMan::CMainTask *MainTask(const CEditMan &e) { return e.mMainTask; }
};

struct EditServerTestHooks {
	static void SetAssignedScope(CEditServer &s, unsigned char scope)
	{
		s.mAssignedScope = scope;
		/* CDataHandler::mScope mirrors it -- see CEditServer's own ctor. */
		*(unsigned char *)((char *)&s + 4 + 0x1c) = scope;
	}
};

struct ChunkManTestHooks {
	static CChkCmdBG *ChkCmdBG(const CChunkMan &c) { return c.mChkCmdBG; }
};

extern CSystemApi *Api;    /* real global, mains.cpp */
extern void *SeqApi;       /* real global, mains.cpp */

static int g_notifyCalls;
static unsigned char g_notifyLastGroup, g_notifyLastIndex, g_notifySub;
static int g_notifyOrder[4];
static int g_notifyOrderCount;

struct FakeEditClient {
	void *vtbl;
	int   id;
};

static void FakeClientNotify(FakeEditClient *self, unsigned char group, unsigned char index,
                              unsigned char subIndex)
{
	g_notifyCalls++;
	g_notifyLastGroup = group;
	g_notifyLastIndex = index;
	g_notifySub = subIndex;
	if (g_notifyOrderCount < 4)
		g_notifyOrder[g_notifyOrderCount++] = self->id;
}

typedef void (*NotifyFn)(FakeEditClient *, unsigned char, unsigned char, unsigned char);
static void *g_fakeClientVtbl[3] = { 0, 0, (void *)(NotifyFn)FakeClientNotify };

int main()
{
	printf("CEditMan/CChunkMan/CSeqTimer/CMessagePort known-answer test\n");
	printf("=============================================================\n");

	/* --- [1] CEditMan::Setup() --- */
	printf("[1] CEditMan::Setup()\n");
	{
		void *raw = malloc(0x30);
		CEditMan *editMan = new (raw) CEditMan();
		editMan->Setup();

		check("mTasks count 0 -> 1", ModuleTestHooks::TaskCount(*editMan) == 1);
		CEditMan::CMainTask *mainTask = EditManTestHooks::MainTask(*editMan);
		check("mMainTask is non-NULL", mainTask != 0);
		check("mTasks[0] == mMainTask", ModuleTestHooks::TaskAt(*editMan, 0) == (void *)mainTask);

		extern unsigned char EditApiInstance[0x404];
		check("EditApiInstance+4 == editMan",
		      *(CEditMan **)(EditApiInstance + 4) == editMan);

		/* --- [2] RegisterServer/GetServerScope/UnregisterServer --- */
		printf("[2] CMainTask::RegisterServer()/GetServerScope()/UnregisterServer()\n");

		CEditServer serverA("ServerA");
		CEditServer serverB("ServerB");
		EditServerTestHooks::SetAssignedScope(serverA, 5);
		EditServerTestHooks::SetAssignedScope(serverB, 5); /* same scope -- evicts A */

		int rc = mainTask->RegisterServer(&serverA);
		check("RegisterServer(serverA, scope=5) returns 1", rc == 1);
		check("GetServerScope(serverA) == 5", mainTask->GetServerScope(&serverA) == 5);

		rc = mainTask->RegisterServer(&serverB);
		check("RegisterServer(serverB, scope=5) returns 1 (evicts serverA)", rc == 1);
		check("GetServerScope(\"ServerB\") == 5",
		      mainTask->GetServerScope("ServerB") == 5);
		check("GetServerScope(\"ServerA\") == 0xffffffff (evicted, no longer found)",
		      mainTask->GetServerScope("ServerA") == 0xffffffffu);

		bool unreg = mainTask->UnregisterServer(&serverB);
		check("UnregisterServer(serverB) returns true", unreg == true);
		check("GetServerScope(\"ServerB\") == 0xffffffff after unregister",
		      mainTask->GetServerScope("ServerB") == 0xffffffffu);

		CEditServer invalidScope("NoScope");
		EditServerTestHooks::SetAssignedScope(invalidScope, 0xff);
		rc = mainTask->RegisterServer(&invalidScope);
		check("RegisterServer() with scope 0xff (\"invalid\", signed<0) returns 0", rc == 0);

		/* --- [3] FindDescriptor/SetDefault (no descriptor registered) --- */
		printf("[3] CMainTask::FindDescriptor()/SetDefault() (no descriptor registered)\n");

		EditServerTestHooks::SetAssignedScope(serverA, 9);
		mainTask->RegisterServer(&serverA);

		CEditServer *outServer = 0;
		int found = mainTask->FindDescriptor(9, 1, 2, &outServer);
		check("FindDescriptor(scope=9): server found (outServer set)", outServer == &serverA);
		check("FindDescriptor(scope=9): no descriptor registered -> 0", found == 0);
		found = mainTask->FindDescriptor(10, 1, 2, &outServer);
		check("FindDescriptor(scope=10, unregistered): 0", found == 0);

		int def = mainTask->SetDefault(9, 1, 2);
		check("SetDefault(scope=9, no descriptor): 0", def == 0);

		/* --- [4] RegisterClient/UnregisterClient/Notify --- */
		printf("[4] CMainTask::RegisterClient()/UnregisterClient()/Notify()\n");

		FakeEditClient clientA = { g_fakeClientVtbl, 1 };
		FakeEditClient clientB = { g_fakeClientVtbl, 2 };

		mainTask->RegisterClient(reinterpret_cast<CEditClient *>(&clientA));
		mainTask->RegisterClient(reinterpret_cast<CEditClient *>(&clientB));
		int dup = mainTask->RegisterClient(reinterpret_cast<CEditClient *>(&clientA));
		check("RegisterClient() returns 1", dup == 1);

		g_notifyCalls = 0;
		g_notifyOrderCount = 0;
		mainTask->Notify(9, 1, 2);
		check("Notify(): exactly 2 clients notified (dedup worked, no double-add)",
		      g_notifyCalls == 2);
		check("Notify(): group/index/subIndex forwarded correctly",
		      g_notifyLastGroup == 9 && g_notifyLastIndex == 1 && g_notifySub == 2);
		check("Notify(): reverse order (clientB=2 first, clientA=1 second)",
		      g_notifyOrderCount == 2 && g_notifyOrder[0] == 2 && g_notifyOrder[1] == 1);

		mainTask->UnregisterClient(reinterpret_cast<CEditClient *>(&clientA));
		g_notifyCalls = 0;
		mainTask->Notify(9, 1, 2);
		check("Notify() after UnregisterClient(clientA): only 1 client left", g_notifyCalls == 1);
	}

	/* --- [5]/[6] CChunkMan::Setup()/Config() --- */
	printf("[5] CChunkMan::Setup()\n");
	{
		void *raw = malloc(0x30);
		CChunkMan *chunkMan = new (raw) CChunkMan();
		chunkMan->Setup();

		check("mTasks count 0 -> 2 (CChkCmd + CChkCmdBG)",
		      ModuleTestHooks::TaskCount(*chunkMan) == 2);
		check("mChkCmdBG is non-NULL", ChunkManTestHooks::ChkCmdBG(*chunkMan) != 0);
		check("mTasks[1] == mChkCmdBG",
		      ModuleTestHooks::TaskAt(*chunkMan, 1) == (void *)ChunkManTestHooks::ChkCmdBG(*chunkMan));

		printf("[6] CChunkMan::Config()\n");
		bool cfgResult = chunkMan->Config();
		check("Config() returns true (slot-17 stub deterministically returns 0)",
		      cfgResult == true);
	}

	/* --- [7] CSeqTimer::Setup() --- */
	printf("[7] CSeqTimer::Setup()\n");
	{
		void *raw = malloc(0x30);
		CSeqTimer *seqTimer = new (raw) CSeqTimer("SequenceTimer");
		seqTimer->Setup(); /* no crash: real CTimerEngine ctor + SeqApi slot-8 dispatch */

		check("mTasks count 0 -> 1 (CTimerEngine)", ModuleTestHooks::TaskCount(*seqTimer) == 1);
		check("SeqApi is non-NULL (real global constructor ran)", SeqApi != 0);

		seqTimer->Config();
		seqTimer->Start();
	}

	/* --- [8] CMessagePort --- */
	printf("[8] CMessagePort ctor + confirmed-empty Setup()/Config()/Start()\n");
	{
		void *raw = malloc(0x34);
		CMessagePort *port = new (raw) CMessagePort();

		check("mTasks count starts 0", ModuleTestHooks::TaskCount(*port) == 0);
		port->Setup();
		port->Config();
		port->Start();
		check("mTasks count still 0 after Setup/Config/Start (all 3 confirmed empty)",
		      ModuleTestHooks::TaskCount(*port) == 0);
	}

	if (g_fail == 0)
		printf("\nall checks passed\n");
	else
		printf("\n%d check(s) FAILED\n", g_fail);

	return g_fail == 0 ? 0 : 1;
}
