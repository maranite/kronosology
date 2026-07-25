/*
 * test_sysex_msg_task_base.cpp  -  host-side known-answer test for
 * CSysExMsgTaskBase's Tier-A methods (src/ipc/sysex_msg_task_base.cpp, Stage 6
 * breadth sweep, 2026-07-25; extended in the SetMask/~CTask follow-up batch,
 * 2026-07-25, to cover the ctor/SetTimeout()/Exec()/dtor now promoted to Tier A).
 *
 * Exec(CMessage&) is the one non-trivial Tier-A method from the first pass: pure
 * argument-unpack + redispatch through this object's own vtable slot +0x14. Builds a
 * raw CMessage-shaped buffer and a fake single-slot-14 vtable to confirm the real
 * argument mapping (tag/length/payload-pointer-plus-one) and the inverted
 * return-value convention (handler 0 -> Exec returns -1, handler nonzero ->
 * Exec returns 0).
 */

#include <cstdio>
#include <cstring>

#include "sysex_msg_task_base.h"
#include "module.h"
#include "system_api.h"
#include "out_link.h"

/* CSysExApiInstance::{EventToMessage,MessageToEvent} counting-stub instrumentation --
 * see sysex_msg_task_base.cpp's own comment for why they're minimal stubs.
 */
extern int g_smtbTestEventToMessageCalls;
extern int g_smtbTestMessageToEventCalls;
extern unsigned char g_smtbTestLastCommId;

/* Friend accessor -- reads mOutLink so the ctor's real ECanTransmit branch can be
 * checked. Same convention as client_comm_server.h's ClientCommServerTestHooks.
 */
struct SysExMsgTaskBaseTestHooks {
	static CSysExMsgClientOutLink *OutLink(const CSysExMsgTaskBase &t) { return t.mOutLink; }
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Real CMessage layout (per Exec(CMessage&)'s own decompile): +0x08 tag byte,
 * +0x0a signed short taggedLen, +0x10 pointer to [firstPayloadByte][rest...].
 */
struct FakeMessage {
	unsigned char pad0[8];
	unsigned char tag;      /* +0x08 */
	unsigned char pad9;
	short         taggedLen; /* +0x0a */
	unsigned char pad0c[4]; /* +0x0c..0x10 */
	const unsigned char *payloadPtr; /* +0x10 */
};

/* Records the args the fake handler was called with, and what it should
 * return, set up per-case before calling Exec().
 */
static void *g_capturedThis;
static unsigned char g_capturedFirstByte, g_capturedTag, g_capturedLen;
static const unsigned char *g_capturedPayload;
static int g_handlerReturn;

extern "C" int FakeHandlerSlot14(void *thisPtr, unsigned char firstByte, unsigned char tag,
                                   const unsigned char *payload, unsigned char len)
{
	g_capturedThis = thisPtr;
	g_capturedFirstByte = firstByte;
	g_capturedTag = tag;
	g_capturedPayload = payload;
	g_capturedLen = len;
	return g_handlerReturn;
}

/* --- ctor/SetTimeout()/Exec()/dtor test scaffolding (promoted from Tier B this
 * batch) -- must live at file scope, not inside main(), since C++ doesn't allow
 * nested function definitions.
 */
struct TaskTestHooks {
	static unsigned char Mask(const CTask &t)
	{
		return *(reinterpret_cast<const unsigned char *>(&t) + 0x4c);
	}
};

static int g_destroyCalls;
extern "C" void FakeNotifyDestroy(void *, CTask *) { g_destroyCalls++; }
extern "C" int FakeScopeId2(void *) { return 0; }
extern "C" void FakeApiNoOp2() {}
static void *g_fakeApiVtbl2[96];
struct FakeApiObj2 { void *vtbl; } g_fakeApiObj2;
extern CSystemApi *Api;

int main()
{
	printf("test_sysex_msg_task_base:\n");

	/* Minimal fake vtable: only slot 0x14/4 == 5 matters to Exec(CMessage&). */
	void *fakeVtbl[6];
	for (int i = 0; i < 6; i++) fakeVtbl[i] = 0;
	fakeVtbl[5] = (void *)FakeHandlerSlot14;

	/* CSysExMsgTaskBase derives from CTask, whose own real ctor mallocs a name
	 * string and needs a real CModule& -- out of scope for this pure-method
	 * test, so (matching test_module_adjust_task_mask.cpp's own "poke real
	 * offsets, skip the real ctor" convention) build a raw buffer instead of
	 * calling the real constructor, and manually install the fake vtable at
	 * offset 0 (Exec(CMessage&) reads `*(void***)this` directly, matching the
	 * real disassembly's own vtable-slot dispatch).
	 */
	union {
		double alignForcer;
		unsigned char bytes[sizeof(CSysExMsgTaskBase)];
	} raw;
	memset(raw.bytes, 0, sizeof(raw.bytes));
	*(void ***)raw.bytes = fakeVtbl;
	CSysExMsgTaskBase *obj = reinterpret_cast<CSysExMsgTaskBase *>(raw.bytes);

	unsigned char payload[4] = {0x11, 0x22, 0x33, 0x44};
	FakeMessage msg;
	memset(&msg, 0, sizeof(msg));
	msg.tag = 0x07;
	msg.taggedLen = 5; /* real code passes (taggedLen - 1) & 0xff as the handler's len */
	msg.payloadPtr = payload; /* handler receives payloadPtr+1, and *payloadPtr as firstByte */

	g_handlerReturn = 1; /* nonzero -> Exec() should return 0 */
	int rc = obj->Exec(*reinterpret_cast<CMessage *>(&msg));
	check("Exec() dispatches through this-object's own vtable slot 0x14",
	      g_capturedThis == (void *)obj);
	check("Exec() passes firstPayloadByte == *payloadPtr", g_capturedFirstByte == payload[0]);
	check("Exec() passes tag == CMessage's own +0x08 byte", g_capturedTag == 0x07);
	check("Exec() passes payload == payloadPtr + 1", g_capturedPayload == payload + 1);
	check("Exec() passes len == (taggedLen - 1) & 0xff", g_capturedLen == 4);
	check("Exec() returns 0 when handler returns nonzero", rc == 0);

	g_handlerReturn = 0; /* zero -> Exec() should return -1 */
	rc = obj->Exec(*reinterpret_cast<CMessage *>(&msg));
	check("Exec() returns -1 when handler returns 0", rc == -1);

	/* Trivial Tier-A overrides -- real, genuinely empty bodies; just confirm
	 * they're callable and don't crash / return the documented values.
	 */
	obj->OnSexLinkError();
	check("OnSexLinkError() is callable (empty body)", true);
	check("OnReceiveMessage() returns 0", obj->OnReceiveMessage(1, 2, payload, 3) == 0);
	obj->OnTimeout();
	check("OnTimeout() is callable (empty body)", true);

	/* --- ctor/SetTimeout()/Exec()/dtor, promoted from Tier B this batch -------- */

	for (int i = 0; i < 96; i++)
		g_fakeApiVtbl2[i] = (void *)FakeApiNoOp2;
	g_fakeApiVtbl2[0x3c / 4] = (void *)FakeScopeId2;
	g_fakeApiVtbl2[0x140 / 4] = (void *)FakeNotifyDestroy;
	g_fakeApiObj2.vtbl = g_fakeApiVtbl2;
	Api = (CSystemApi *)&g_fakeApiObj2;

	printf("[ctor] CSysExMsgTaskBase(owner, canTransmit=0, needsTimeout=0) -- real "
	       "base CTask::CTask() + real vtable-pair install + real 'mMask & 0x08' "
	       "check\n");
	{
		CModule owner("SysExOwnerModule");
		CSysExMsgTaskBase task(owner, 0, 0);

		/* Base CTask ctor: scheduleFlag = (needsTimeout==1) = false = 0 ->
		 * base mask 0x04, +2 (owner mState==0 < 4, fresh module) -> 0x06.
		 * 0x06 & 0x08 == 0, so the ctor's own SetMask(1) branch must NOT fire.
		 */
		check("mMask == 0x06 (base 0x04 +2, bit 0x08 clear -> ctor's SetMask(1) "
		      "branch not taken)",
		      TaskTestHooks::Mask(task) == 0x06);

		printf("[SetTimeout] real fixed-point period computation + tail "
		       "SetMask(0)\n");
		task.SetTimeout(100);
		/* HAL_GetScheduleInterval() stub == 1 -> ticks == 100;
		 * period == (100 * 0xcccd) >> 19 == 9 (nonzero).
		 */
		check("SetTimeout(100): mMask bit 0x01 cleared (tail SetMask(0))",
		      (TaskTestHooks::Mask(task) & 0x01) == 0);

		printf("[Exec 0-arg] not-yet-elapsed branch: no SetMask/dispatch\n");
		unsigned char before = TaskTestHooks::Mask(task);
		task.Exec();
		check("Exec(): mMask unchanged while timeout hasn't elapsed "
		      "(HAL_GetSystemTime() stub == 0, mTimeoutStart == 0, "
		      "mTimeoutTicks == 100)",
		      TaskTestHooks::Mask(task) == before);

		printf("[Exec 0-arg] elapsed branch: SetMask(1) + slot 0x1c redispatch "
		       "(no crash, EvaVTableStub)\n");
		/* Poke mTimeoutTicks (+0x80) to 0 directly -- same raw-offset license
		 * the ctor/SetTimeout() themselves use, bypassing the need for a
		 * friend declaration.
		 */
		*reinterpret_cast<unsigned int *>(reinterpret_cast<char *>(&task) + 0x80) = 0;
		task.Exec();
		check("Exec(): mMask bit 0x01 set once elapsed (real SetMask(1) call)",
		      (TaskTestHooks::Mask(task) & 0x01) != 0);
	} /* ~CSysExMsgTaskBase() -> ~CTask() runs here */
	check("~CSysExMsgTaskBase() ran without crashing and fired the base "
	      "~CTask()'s own +0x140 destroy notification exactly once",
	      g_destroyCalls == 1);

	printf("[ctor ECanTransmit==1] real CSysExMsgClientOutLink construction + "
	       "CTask::Add() (Eva CSysExMsgClientOutLink follow-up pass, 2026-07-25)\n");
	{
		CModule owner2("SysExOwnerModule2");
		CSysExMsgTaskBase noLinkTask(owner2, /*canTransmit=*/0, 0);
		check("canTransmit=0: mOutLink stays NULL",
		      SysExMsgTaskBaseTestHooks::OutLink(noLinkTask) == 0);

		CSysExMsgTaskBase linkedTask(owner2, /*canTransmit=*/1, 0);
		check("canTransmit=1: mOutLink is now a real, non-NULL "
		      "CSysExMsgClientOutLink*",
		      SysExMsgTaskBaseTestHooks::OutLink(linkedTask) != 0);

		printf("[SendMsg] real forward to mOutLink->SendMessage() -- an "
		       "empty-mLinks link returns error 5, so SendMsg() returns false\n");
		unsigned char data[2] = {0xaa, 0xbb};
		check("SendMsg() returns false (mOutLink's own mLinks starts empty, "
		      "OutMono()'s real 'no destination registered' early-out)",
		      linkedTask.SendMsg(data, 2) == false);

		printf("[EventToMessage/MessageToEvent] real forward to the "
		       "CSysExApiInstance counting stub\n");
		g_smtbTestEventToMessageCalls = 0;
		g_smtbTestMessageToEventCalls = 0;
		g_smtbTestLastCommId = 0;

		unsigned char outBuf[4];
		unsigned char outLen = 0;
		linkedTask.EventToMessage(0, outBuf, outLen);
		check("EventToMessage() forwards exactly once",
		      g_smtbTestEventToMessageCalls == 1);
		check("EventToMessage() passes this task's own mCommId (0xff, ctor's "
		      "'uninitialized' sentinel, never set by any reconstructed caller)",
		      g_smtbTestLastCommId == 0xff);

		linkedTask.MessageToEvent(data, 2, 0);
		check("MessageToEvent() forwards exactly once",
		      g_smtbTestMessageToEventCalls == 1);
	}

	printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
