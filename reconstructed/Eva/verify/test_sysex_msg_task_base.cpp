/*
 * test_sysex_msg_task_base.cpp  -  host-side known-answer test for
 * CSysExMsgTaskBase's Tier-A methods (src/ipc/sysex_msg_task_base.cpp, Stage 6
 * breadth sweep, 2026-07-25).
 *
 * Exec(CMessage&) is the one non-trivial Tier-A method: pure argument-unpack +
 * redispatch through this object's own vtable slot +0x14. Builds a raw
 * CMessage-shaped buffer and a fake single-slot-14 vtable to confirm the real
 * argument mapping (tag/length/payload-pointer-plus-one) and the inverted
 * return-value convention (handler 0 -> Exec returns -1, handler nonzero ->
 * Exec returns 0).
 */

#include <cstdio>
#include <cstring>

#include "sysex_msg_task_base.h"
#include "module.h"

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

	printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
