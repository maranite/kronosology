/*
 * test_out_link.cpp  -  host-side known-answer test for COutLink/COutLinkMono/
 * CSysExMsgOutLink/CSysExMsgClientOutLink (src/ipc/out_link.cpp), Eva
 * CSysExMsgClientOutLink follow-up pass, 2026-07-25.
 *
 * Checks:
 *   [1] COutLink/COutLinkMono/CSysExMsgOutLink/CSysExMsgClientOutLink ctors all run
 *       without crashing, mScopeId is populated via the real Api+0x3c call (same
 *       CallVSlot idiom as CTask::CTask()/CModule::CModule()), mLinks starts empty.
 *   [2] COutLinkMono::OutMono() with an empty mLinks array returns 5 immediately
 *       (the real "no destination CLink registered" early-out) without touching
 *       mLink at all (mLink stays NULL, dereferencing it would crash).
 *   [3] COutLinkMono::OutMono() with mLinks non-empty (via the real, already-
 *       reconstructed COmegaPtrArray::Add()) and a friend-poked fake CLink+receiver:
 *       confirms the 3 real field writes into the CLink descriptor (+0x18 flags,
 *       +0x1a len, +0x20 buf), the real indirect call through the receiver's own
 *       vtable slot 8 with the right args, the result stored back at CLink+8, and
 *       COutLink::TestResult()'s own identity-passthrough return.
 *   [4] CSysExMsgClientOutLink::SendMessage() forwards to OutMono() correctly
 *       (same empty-mLinks early-out, exercised through the full derived class).
 */

#include <cstdio>
#include <cstring>

#include "out_link.h"
#include "task.h"
#include "module.h"
#include "omega_ptr_array.h"
#include "system_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Friend accessor -- pokes mLink directly (real ctors never populate it, see
 * out_link.h) and lets the test build a fake CLink-shaped buffer, same convention
 * as client_comm_server.h's ClientCommServerTestHooks.
 */
struct OutLinkTestHooks {
	static void SetLink(COutLinkMono &o, void *fakeLink) { o.mLink = (CLink *)fakeLink; }
	static CLink *GetLink(const COutLinkMono &o) { return o.mLink; }
	static void *Links(COutLinkMono &o) { return o.mLinks; }
};

extern CSystemApi *Api;

static int g_scopeIdCalls;
extern "C" int FakeScopeIdFn(void *)
{
	g_scopeIdCalls++;
	return 0xbeef;
}
extern "C" void FakeApiNoOp() {}

/* Sized to cover slot 0x140/4 == 80 -- every real CTask this file constructs (as
 * COutLink's own `owner` argument) is genuinely destructed at scope exit by the
 * real ~CTask() (task.cpp), which dispatches through this slot unconditionally --
 * same bound test_task.cpp's own setup_fake_api() already established.
 */
static void *g_fakeApiVtbl[96];
struct FakeApiObj { void *vtbl; } g_fakeApiObj;

static void setup_fake_api()
{
	for (int i = 0; i < 96; i++)
		g_fakeApiVtbl[i] = (void *)FakeApiNoOp;
	g_fakeApiVtbl[0x3c / 4] = (void *)FakeScopeIdFn;
	g_fakeApiObj.vtbl = g_fakeApiVtbl;
	Api = (CSystemApi *)&g_fakeApiObj;
}

/* Fake CLink descriptor + fake receiver object, big enough to cover every offset
 * OutMono() touches (up to +0x24, plus the receiver's own vtable-slot-8 dispatch).
 */
struct FakeReceiver
{
	void *vtbl;
};

static int g_recvCalls;
static void *g_lastRecvThis;
static void *g_lastRecvArg;
static int g_recvReturnValue = 0x77;
extern "C" int FakeReceiverSlot8(void *self, void *arg)
{
	g_recvCalls++;
	g_lastRecvThis = self;
	g_lastRecvArg = arg;
	return g_recvReturnValue;
}

int main()
{
	printf("COutLink/COutLinkMono/CSysExMsgOutLink/CSysExMsgClientOutLink "
	       "known-answer test\n");
	printf("================================================================\n");

	setup_fake_api();

	printf("[1] ctors run cleanly, mScopeId real, mLinks starts empty\n");
	{
		CModule m("OutLinkOwnerModule");
		CTask owner(m, "OutLinkOwnerTask", 0, 0, 0x804b);

		g_scopeIdCalls = 0;
		COutLink link(owner, "PlainLink", COutLink::eDirectionOut, 0x1234, 1);
		check("COutLink ctor calls Api+0x3c exactly once", g_scopeIdCalls == 1);

		g_scopeIdCalls = 0;
		CSysExMsgOutLink monoLink(owner, "PlainMonoLink");
		check("COutLinkMono-family ctor also calls Api+0x3c exactly once",
		      g_scopeIdCalls == 1);
		check("mLinks starts empty",
		      *(int *)((unsigned char *)OutLinkTestHooks::Links(monoLink) + 0xc) == 0);

		CSysExMsgClientOutLink clientLink(owner);
		check("CSysExMsgClientOutLink ctor runs without crashing", true);
	}

	printf("[2] OutMono() with empty mLinks returns 5 immediately, mLink stays "
	       "untouched (NULL)\n");
	{
		CModule m("EmptyLinksModule");
		CTask owner(m, "EmptyLinksTask", 0, 0, 0x804b);
		CSysExMsgOutLink link(owner, "EmptyLinksTest");

		check("mLink starts NULL", OutLinkTestHooks::GetLink(link) == 0);

		unsigned char buf[1] = { 0x55 };
		int result = link.OutMono(0x10, buf, 1);
		check("OutMono() with empty mLinks returns 5", result == 5);
		check("mLink still NULL afterward (never dereferenced)",
		      OutLinkTestHooks::GetLink(link) == 0);
	}

	printf("[3] OutMono() with a real (non-empty) mLinks entry and a friend-poked "
	       "fake CLink+receiver -- full data-driven dispatch\n");
	{
		CModule m("FullLinksModule");
		CTask owner(m, "FullLinksTask", 0, 0, 0x804b);
		CSysExMsgOutLink link(owner, "FullLinksTest");

		/* Populate mLinks via the real, already-reconstructed COmegaPtrArray::Add()
		 * -- any non-null pointer works, OutMono() only checks the count, never
		 * reads the array's own contents.
		 */
		int dummyElem = 0;
		((COmegaPtrArray *)OutLinkTestHooks::Links(link))->Add(&dummyElem);
		check("mLinks count is now 1 (COmegaPtrArray::Add(), already real)",
		      *(int *)((unsigned char *)OutLinkTestHooks::Links(link) + 0xc) == 1);

		/* Fake CLink descriptor: zeroed 0x30-byte buffer, +0x24 points at a fake
		 * receiver whose own vtbl slot 8 is FakeReceiverSlot8.
		 */
		unsigned char fakeLink[0x30];
		memset(fakeLink, 0, sizeof(fakeLink));
		void *recvVtbl[3] = { (void *)FakeApiNoOp, (void *)FakeApiNoOp,
		                       (void *)FakeReceiverSlot8 };
		FakeReceiver recv;
		recv.vtbl = recvVtbl;
		*(void **)(fakeLink + 0x24) = &recv;
		/* Seed +0x18's low nibble with something recognizable so the
		 * "preserve high nibble, OR in 0x200|ecb" masking is actually exercised
		 * (not just writing to an already-zero field).
		 */
		*(unsigned short *)(fakeLink + 0x18) = 0x5000;

		OutLinkTestHooks::SetLink(link, fakeLink);

		unsigned char payload[4] = { 0xaa, 0xbb, 0xcc, 0xdd };
		g_recvCalls = 0;
		g_lastRecvThis = 0;
		g_lastRecvArg = 0;
		g_recvReturnValue = 0x77;

		int result = link.OutMono(/*ecb=*/0x42, payload, /*len=*/4);

		check("OutMono() writes len to CLink+0x1a",
		      *(unsigned short *)(fakeLink + 0x1a) == 4);
		check("OutMono() writes buf to CLink+0x20",
		      *(void **)(fakeLink + 0x20) == (void *)payload);
		check("OutMono() masks CLink+0x18: high nibble preserved, 0x200|ecb set in",
		      *(unsigned short *)(fakeLink + 0x18) == (unsigned short)(0x5000 | 0x200 | 0x42));
		check("OutMono() dispatches through the receiver's own vtable slot 8 "
		      "exactly once",
		      g_recvCalls == 1);
		check("...with the receiver object itself as the first arg",
		      g_lastRecvThis == (void *)&recv);
		check("...and CLink+0x10 as the second arg",
		      g_lastRecvArg == (void *)(fakeLink + 0x10));
		check("OutMono() stores the receiver's return value at CLink+8",
		      *(int *)(fakeLink + 8) == 0x77);
		check("OutMono()'s own return value is TestResult()'s identity passthrough "
		      "of that same result",
		      result == 0x77);
	}

	printf("[4] CSysExMsgClientOutLink::SendMessage() forwards to OutMono()\n");
	{
		CModule m("SendMessageModule");
		CTask owner(m, "SendMessageTask", 0, 0, 0x804b);
		CSysExMsgClientOutLink link(owner);

		unsigned char data[2] = { 0x01, 0x02 };
		int result = link.SendMessage(0x20, data, 2);
		check("SendMessage() on an empty-mLinks link returns 5 (same OutMono() "
		      "early-out, reached through SendMessage()'s own forward)",
		      result == 5);
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail != 0;
}
