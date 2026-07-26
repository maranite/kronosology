/*
 * test_poller.cpp  -  host-side known-answer test for CPoller / CPoller::CIfcClient
 * (src/ui/poller.cpp), Eva Stage 6 batch, 2026-07-26 -- reassessing CPoller now that
 * its own previously-flagged blocker (CTask::SetMask()) is real. See poller.h's own
 * header comment for the full ground-truth writeup (byte-exact ctor/dtor trace,
 * CPanel::Setup() reachability finding, vtable derivation).
 *
 * Checks:
 *   [1] ctor with name == NULL: real fallback path (SetMask(1), mResource stays 0,
 *       both handle tables 0xFFFFFFFF-filled, mFlag390 == 0, the 3 trailing dwords
 *       0xFFFFFFFF, mZeroBlock all zero) -- the "no name given" real ground-truth case
 *   [2] ctor with name != NULL, Api+0xac lookup returns NULL: same fallback path
 *   [3] ctor with name != NULL, lookup succeeds but the resource's own vtbl+0x10 call
 *       doesn't return 10: same fallback path, mResource cleared back to 0 even though
 *       the lookup itself succeeded
 *   [4] ctor with name != NULL, lookup succeeds, vtbl+0x10 returns 10, but vtbl+0x8
 *       call(resource, 0) doesn't return 0: same fallback path
 *   [5] ctor with ALL 4 real gates passing: mResource == the real resource pointer,
 *       SetMask(1) NOT called (mask bit 0x01 stays clear)
 *   [6] dtor with mResource set: fires exactly one real vtbl+0xc disconnect(resource,
 *       0) call; dtor with mResource == 0: fires none
 *   [7] FindUnconnected()/IsValidHandle()/IsRegisteredHandle(): friend-poked mClients
 *       array of fake client blobs with a controllable +0x14 "connected" field
 *   [8] CIfcClient: PutAnalogEvt() fills the real 8-slot ring in order; the 9th call
 *       triggers a real flush-and-reset (OutMono() safely returns 5 immediately since
 *       mLinks is empty -- CIfcClient's own ctor never populates it, same established
 *       "empty mLinks, mLink never dereferenced" safety out_link.h's own KAT already
 *       exercises) -- cursor resets to the ring start and the 9th element lands at
 *       slot 0; FlushAnalogEvts() likewise resets a non-empty ring, no-ops on an
 *       already-empty one
 *   [9] TVector_CIfcClientPtr_MakeCapacity(): real min-32-then-doubling growth policy
 *       (NOT min-10, confirmed by direct disassembly of .text+0x089f7280 -- a
 *       DIFFERENT real constant from task.cpp's own TVector_SRegisteredIfc_
 *       MakeCapacity(), even though both are TVector<T,1> instantiations)
 *   [10] MsgShortBeep()/MsgRequestAnalogInputValue(): code-bit-0x100 gate, the
 *        opaque mResource->vtbl[7] notify (fake resource records the {opcode,value}
 *        struct actually passed)
 *   [11] MsgUnregisterClient(): code-bit-0x100 gate, invalid/out-of-range/
 *        not-connected/success return-code matrix (9/9/2/0), real Api+0x58
 *        per-outlink notify firing exactly once on success
 *   [12] MsgSetEncoderClient()/MsgSetTouchPanelClient()/MsgSetKeyboardClient(): the
 *        shared shape (clear-to-0xFFFFFFFF unset path, out-of-range/not-connected
 *        both returning 9, success storing the handle) against each one's own real
 *        field (mField394/398/39c)
 *   [13] MsgRegisterClientByVal()/MsgRegisterClientByRef(): length-gate thresholds
 *        (0x63/0xb), embedded-buffer vs pointer-to-string payload shapes, the real
 *        RegisterClient()'s Phase-3 append (fresh mClients, first call), the real
 *        Api+0x44 tail notify firing with the real owner/task/client/nameA/nameB
 *        args, and the real "reuse a still-unconnected slot" quirk on a 2nd call
 *        (does NOT append a 2nd client or rebind the slot's name)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "poller.h"
#include "module.h"
#include "system_api.h"
#include "stg_unsol_msg_handler.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

struct PollerTestHooks {
	static void *Resource(const CPoller &p) { return p.mResource; }
	static const unsigned int *HandleTable1(const CPoller &p) { return p.mHandleTable1; }
	static const unsigned int *HandleTable2(const CPoller &p) { return p.mHandleTable2; }
	static unsigned char Flag390(const CPoller &p) { return p.mFlag390; }
	static unsigned int Field394(const CPoller &p) { return p.mField394; }
	static unsigned int Field398(const CPoller &p) { return p.mField398; }
	static unsigned int Field39c(const CPoller &p) { return p.mField39c; }
	static const unsigned int *ZeroBlock(const CPoller &p) { return p.mZeroBlock; }
	static unsigned char Mask(const CPoller &p)
	{
		/* CTask::mMask lives at CTask+0x4c (task.h); CPoller has no own field
		 * there, safe to read via the same raw-offset convention every other
		 * cross-boundary read in this project uses.
		 */
		return *((const unsigned char *)&p + 0x4c);
	}
	static void SetClients(CPoller &p, void **begin, void **end)
	{
		*(void ***)(p.mClients + 4) = begin;
		*(void ***)(p.mClients + 8) = end;
	}
	static void *Cursor(const CPoller::CIfcClient &c) { return c.mCursor; }
	static const unsigned char *RingBuf(const CPoller::CIfcClient &c) { return c.mRingBuf; }
};

/* --- fake Api global -------------------------------------------------------- */

extern CSystemApi *Api;

static int g_scopeIdCalls;
extern "C" int FakeScopeIdFn(void *) { g_scopeIdCalls++; return 0x1234; }
extern "C" void FakeApiNoOp() {}

static void *g_fakeLookupResult;
static const char *g_lastLookupName;
extern "C" void *FakeLookupFn(void *, const char *name)
{
	g_lastLookupName = name;
	return g_fakeLookupResult;
}

/* --- fake "resource" object the Api+0xac lookup returns -------------------- */

static int g_fakeResourceType;
static int g_fakeConnectResult;
static int g_connectCalls, g_disconnectCalls;
static void *g_lastConnectResource, *g_lastDisconnectResource;

extern "C" int FakeResGetType(void *) { return g_fakeResourceType; }
extern "C" int FakeResConnect(void *r, int arg)
{
	g_connectCalls++;
	g_lastConnectResource = r;
	check("connect() called with arg 0", arg == 0);
	return g_fakeConnectResult;
}
extern "C" int FakeResDisconnect(void *r, int arg)
{
	g_disconnectCalls++;
	g_lastDisconnectResource = r;
	check("disconnect() called with arg 0", arg == 0);
	return 0;
}

static int g_notifyCalls;
static unsigned int g_lastNotifyOpcode, g_lastNotifyValue;
extern "C" void FakeResNotify(void *, unsigned int *msg)
{
	g_notifyCalls++;
	g_lastNotifyOpcode = msg[0];
	g_lastNotifyValue = msg[1];
}

static void *g_fakeResVtbl[8];
struct FakeResObj { void *vtbl; } g_fakeResObj;

static void setup_fake_resource()
{
	for (int i = 0; i < 8; i++)
		g_fakeResVtbl[i] = (void *)FakeApiNoOp;
	g_fakeResVtbl[0x8 / 4] = (void *)FakeResConnect;
	g_fakeResVtbl[0xc / 4] = (void *)FakeResDisconnect;
	g_fakeResVtbl[0x10 / 4] = (void *)FakeResGetType;
	g_fakeResVtbl[0x1c / 4] = (void *)FakeResNotify;
	g_fakeResObj.vtbl = g_fakeResVtbl;
}

static int g_outLinkNotifyCalls;
static void *g_lastOutLinkNotified;
extern "C" void FakeOutLinkNotify(void *, void *link)
{
	g_outLinkNotifyCalls++;
	g_lastOutLinkNotified = link;
}

/* Fake for Api's own vtbl slot +0x44 -- CPoller::RegisterClient()'s own tail
 * notify (system_api.h, poller.cpp). Records all 6 real string/int args so the
 * KAT can confirm they're forwarded correctly; return value is controllable
 * (default 1 == success, matches RegisterClient()'s own `result >= 1` gate).
 */
static int g_registerNotifyCalls;
static const char *g_lastRegisterOwnerName;
static const char *g_lastRegisterTaskName;
static const char *g_lastRegisterClientName;
static const char *g_lastRegisterNameA;
static const char *g_lastRegisterNameB;
static int g_registerNotifyResult = 1;
extern "C" int FakeRegisterNotify(void *, const char *ownerName, const char *taskName,
                                   const char *clientName, const char *nameA,
                                   const char *nameB, int zero)
{
	g_registerNotifyCalls++;
	g_lastRegisterOwnerName = ownerName;
	g_lastRegisterTaskName = taskName;
	g_lastRegisterClientName = clientName;
	g_lastRegisterNameA = nameA;
	g_lastRegisterNameB = nameB;
	check("Api+0x44 notify's trailing arg is 0", zero == 0);
	return g_registerNotifyResult;
}

static void *g_fakeApiVtbl[96];
struct FakeApiObj { void *vtbl; } g_fakeApiObj;

static void setup_fake_api()
{
	for (int i = 0; i < 96; i++)
		g_fakeApiVtbl[i] = (void *)FakeApiNoOp;
	g_fakeApiVtbl[0x3c / 4] = (void *)FakeScopeIdFn;
	g_fakeApiVtbl[0xac / 4] = (void *)FakeLookupFn;
	g_fakeApiVtbl[0x58 / 4] = (void *)FakeOutLinkNotify;
	g_fakeApiVtbl[0x44 / 4] = (void *)FakeRegisterNotify;
	g_fakeApiObj.vtbl = g_fakeApiVtbl;
	Api = (CSystemApi *)&g_fakeApiObj;
}

/* --- fake CMessage: real layout is +0x9 the code word's own high byte (tested
 * as flag bits 0x1/0x2 by these handlers), +0xa a tagged-length u16, +0x10 a
 * payload dword (pointer or scalar depending on message type) -- see poller.h's
 * own per-method header comments. Raw byte buffer, not a struct, to avoid any
 * compiler-inserted padding disturbing the real fixed offsets these handlers
 * read via reinterpret_cast, same discipline chunk_server.cpp's own KAT uses.
 */
struct FakeMessage {
	unsigned char raw[0x18];

	FakeMessage() { memset(raw, 0, sizeof(raw)); }
	void SetFlags(unsigned char f) { raw[9] = f; }
	void SetTaggedLen(unsigned short len) { *(unsigned short *)(raw + 0xa) = len; }
	void SetWord10(unsigned int v) { *(unsigned int *)(raw + 0x10) = v; }
	CMessage &AsMessage() { return *reinterpret_cast<CMessage *>(raw); }
};

int main()
{
	setup_fake_api();
	setup_fake_resource();

	CModule owner("TestModule");

	printf("[1] ctor with name == NULL: real fallback path\n");
	{
		CPoller p(owner, 0);
		check("mResource == 0", PollerTestHooks::Resource(p) == 0);
		check("mMask bit 0x01 set (SetMask(1) fired)",
		      (PollerTestHooks::Mask(p) & 0x01) != 0);
		bool allFF1 = true;
		for (int i = 0; i < 0x40; i++)
			if (PollerTestHooks::HandleTable1(p)[i] != 0xffffffff) allFF1 = false;
		check("mHandleTable1 fully 0xFFFFFFFF-filled (64 entries)", allFF1);
		bool allFF2 = true;
		for (int i = 0; i < 0x80; i++)
			if (PollerTestHooks::HandleTable2(p)[i] != 0xffffffff) allFF2 = false;
		check("mHandleTable2 fully 0xFFFFFFFF-filled (128 entries)", allFF2);
		check("mFlag390 == 0", PollerTestHooks::Flag390(p) == 0);
		check("mField394 == 0xFFFFFFFF", PollerTestHooks::Field394(p) == 0xffffffffu);
		check("mField398 == 0xFFFFFFFF", PollerTestHooks::Field398(p) == 0xffffffffu);
		check("mField39c == 0xFFFFFFFF", PollerTestHooks::Field39c(p) == 0xffffffffu);
		bool allZero = true;
		for (int i = 0; i < 0x10; i++)
			if (PollerTestHooks::ZeroBlock(p)[i] != 0) allZero = false;
		check("mZeroBlock fully zeroed (16 dwords)", allZero);
	}

	printf("[2] ctor with name != NULL, lookup returns NULL: same fallback path\n");
	{
		g_fakeLookupResult = 0;
		CPoller p(owner, "PanelRes");
		check("real name reached the Api+0xac lookup",
		      g_lastLookupName != 0 && strcmp(g_lastLookupName, "PanelRes") == 0);
		check("mResource == 0", PollerTestHooks::Resource(p) == 0);
		check("mMask bit 0x01 set", (PollerTestHooks::Mask(p) & 0x01) != 0);
	}

	printf("[3] lookup succeeds, vtbl+0x10 doesn't return 10: fallback, mResource "
	       "cleared\n");
	{
		g_fakeLookupResult = &g_fakeResObj;
		g_fakeResourceType = 7;
		CPoller p(owner, "PanelRes");
		check("mResource cleared back to 0", PollerTestHooks::Resource(p) == 0);
		check("mMask bit 0x01 set", (PollerTestHooks::Mask(p) & 0x01) != 0);
	}

	printf("[4] vtbl+0x10 returns 10, vtbl+0x8 connect() doesn't return 0: fallback\n");
	{
		g_fakeLookupResult = &g_fakeResObj;
		g_fakeResourceType = 10;
		g_fakeConnectResult = -1;
		g_connectCalls = 0;
		CPoller p(owner, "PanelRes");
		check("connect() was called exactly once", g_connectCalls == 1);
		check("mResource cleared back to 0", PollerTestHooks::Resource(p) == 0);
		check("mMask bit 0x01 set", (PollerTestHooks::Mask(p) & 0x01) != 0);
	}

	printf("[5] all 4 real gates pass: mResource kept, SetMask(1) NOT fired\n");
	{
		g_fakeLookupResult = &g_fakeResObj;
		g_fakeResourceType = 10;
		g_fakeConnectResult = 0;
		g_connectCalls = 0;
		CPoller p(owner, "PanelRes");
		check("connect() was called exactly once", g_connectCalls == 1);
		check("mResource == the real resource pointer",
		      PollerTestHooks::Resource(p) == &g_fakeResObj);
		check("mMask bit 0x01 stays clear", (PollerTestHooks::Mask(p) & 0x01) == 0);
	}

	printf("[6] dtor: real disconnect(resource, 0) fires iff mResource != 0\n");
	{
		g_fakeLookupResult = &g_fakeResObj;
		g_fakeResourceType = 10;
		g_fakeConnectResult = 0;
		g_disconnectCalls = 0;
		{
			CPoller p(owner, "PanelRes");
			(void)p;
		}
		check("disconnect() fired exactly once for a live mResource",
		      g_disconnectCalls == 1);
		check("disconnect() got the real resource pointer",
		      g_lastDisconnectResource == &g_fakeResObj);

		g_fakeLookupResult = 0;
		g_disconnectCalls = 0;
		{
			CPoller p(owner, 0);
			(void)p;
		}
		check("disconnect() NOT fired when mResource stayed 0", g_disconnectCalls == 0);
	}

	printf("[7] FindUnconnected()/IsValidHandle()/IsRegisteredHandle()\n");
	{
		unsigned char blobA[0x18]; memset(blobA, 0, sizeof(blobA));
		unsigned char blobB[0x18]; memset(blobB, 0, sizeof(blobB));
		unsigned char blobC[0x18]; memset(blobC, 0, sizeof(blobC));
		*(int *)(blobA + 0x14) = 1; /* connected */
		*(int *)(blobB + 0x14) = 0; /* NOT connected */
		*(int *)(blobC + 0x14) = 1; /* connected */
		void *elems[3] = { blobA, blobB, blobC };

		CPoller p(owner, 0);
		PollerTestHooks::SetClients(p, elems, elems + 3);

		check("FindUnconnected() finds index 1 (blobB)", p.FindUnconnected() == 1);
		check("IsValidHandle(0/1/2) true", p.IsValidHandle(0) && p.IsValidHandle(1) &&
		                                        p.IsValidHandle(2));
		check("IsValidHandle(3) false (== count)", !p.IsValidHandle(3));
		check("IsValidHandle(0xFFFFFFFF) false (real sentinel)",
		      !p.IsValidHandle(0xffffffffu));
		check("IsRegisteredHandle(0) true (blobA connected)", p.IsRegisteredHandle(0));
		check("IsRegisteredHandle(1) false (blobB not connected)",
		      !p.IsRegisteredHandle(1));
		check("IsRegisteredHandle(2) true (blobC connected)", p.IsRegisteredHandle(2));
		check("IsRegisteredHandle(3) false (out of range)", !p.IsRegisteredHandle(3));

		/* Detach the friend-poked array before mClients is torn down by
		 * ~CPoller() -- these are stack blobs, not malloc'd. */
		PollerTestHooks::SetClients(p, 0, 0);
	}

	printf("[8] CIfcClient: real 8-slot ring fill + flush-and-reset on overflow\n");
	{
		CTask taskOwner(owner, "TaskOwner", 2, 1, 0x804b);
		CPoller::CIfcClient c(taskOwner, "Client", 0);

		check("mCursor starts at the ring start",
		      PollerTestHooks::Cursor(c) == PollerTestHooks::RingBuf(c));

		CPanelOut::SAnalogEvt evt;
		for (int i = 0; i < 8; i++) {
			evt.type = 0x19;
			evt.value = (short)(100 + i);
			c.PutAnalogEvt(evt);
		}
		const unsigned char *ring = PollerTestHooks::RingBuf(c);
		check("ring is full after 8 puts (cursor at ring+0x40)",
		      PollerTestHooks::Cursor(c) == ring + 0x40);
		check("slot 0 holds the first pushed value (100)",
		      *(const short *)(ring + 4) == 100);
		check("slot 7 holds the 8th pushed value (107)",
		      *(const short *)(ring + 7 * 8 + 4) == 107);

		evt.type = 0x19;
		evt.value = 999;
		c.PutAnalogEvt(evt); /* 9th put: real flush-and-reset, then appended at slot 0 */
		check("cursor advanced by exactly one slot after the 9th put "
		      "(flush-and-reset, then append)",
		      PollerTestHooks::Cursor(c) == ring + 8);
		check("slot 0 now holds the 9th value (999, ring was reset)",
		      *(const short *)(ring + 4) == 999);

		c.FlushAnalogEvts();
		check("FlushAnalogEvts() resets a non-empty ring to the start",
		      PollerTestHooks::Cursor(c) == ring);
		c.FlushAnalogEvts();
		check("FlushAnalogEvts() on an already-empty ring is a no-op",
		      PollerTestHooks::Cursor(c) == ring);
	}

	printf("[9] TVector_CIfcClientPtr_MakeCapacity(): real min-32-then-doubling\n");
	{
		unsigned char vec[0x10];
		memset(vec, 0, sizeof(vec));

		TVector_CIfcClientPtr_MakeCapacity(vec, 5);
		unsigned char *begin1 = *(unsigned char **)(vec + 4);
		unsigned char *cap1   = *(unsigned char **)(vec + 0xc);
		check("MakeCapacity(5) grows to 32 elements (min, NOT 10)",
		      (unsigned int)(cap1 - begin1) / 4 == 32);

		TVector_CIfcClientPtr_MakeCapacity(vec, 32);
		unsigned char *begin2 = *(unsigned char **)(vec + 4);
		check("MakeCapacity(32) is a no-op (already exactly at capacity)",
		      begin2 == begin1);

		TVector_CIfcClientPtr_MakeCapacity(vec, 33);
		unsigned char *begin3 = *(unsigned char **)(vec + 4);
		unsigned char *cap3   = *(unsigned char **)(vec + 0xc);
		check("MakeCapacity(33) doubles to 64 elements",
		      (unsigned int)(cap3 - begin3) / 4 == 64);
		check("MakeCapacity() reallocated (new block address)", begin3 != begin1);

		free(*(unsigned char **)(vec + 4));
	}

	printf("[10] MsgShortBeep()/MsgRequestAnalogInputValue(): code-bit-0x100 gate + "
	       "opaque mResource notify\n");
	{
		g_fakeLookupResult = &g_fakeResObj;
		g_fakeResourceType = 10;
		g_fakeConnectResult = 0;
		CPoller p(owner, "PanelRes");

		FakeMessage m;
		g_notifyCalls = 0;
		m.SetFlags(0); /* bit 0x1 clear */
		check("MsgShortBeep() returns 4 when code bit 0x100 clear",
		      p.MsgShortBeep(m.AsMessage()) == 4);
		check("no notify fired", g_notifyCalls == 0);

		m.SetFlags(0x1);
		check("MsgShortBeep() returns 0 when bit set + mResource live",
		      p.MsgShortBeep(m.AsMessage()) == 0);
		check("notify fired exactly once with opcode 7",
		      g_notifyCalls == 1 && g_lastNotifyOpcode == 7);

		g_notifyCalls = 0;
		m.SetFlags(0);
		check("MsgRequestAnalogInputValue() returns 4 when bit clear",
		      p.MsgRequestAnalogInputValue(m.AsMessage()) == 4);
		m.SetFlags(0x1);
		m.SetWord10(42);
		check("MsgRequestAnalogInputValue() returns 0 when bit set",
		      p.MsgRequestAnalogInputValue(m.AsMessage()) == 0);
		check("notify fired with opcode 5, value == msg's own +0x10 (42)",
		      g_notifyCalls == 1 && g_lastNotifyOpcode == 5 && g_lastNotifyValue == 42);
	}

	printf("[11] MsgUnregisterClient(): return-code matrix + real Api+0x58 notify\n");
	{
		CPoller p(owner, 0); /* mResource irrelevant here */

		unsigned char blobA[0x18]; memset(blobA, 0, sizeof(blobA));
		unsigned char blobB[0x18]; memset(blobB, 0, sizeof(blobB));
		*(int *)(blobA + 0x14) = 1; /* connected */
		*(int *)(blobB + 0x14) = 0; /* NOT connected */
		void *elems[2] = { blobA, blobB };
		PollerTestHooks::SetClients(p, elems, elems + 2);

		FakeMessage m;
		m.SetFlags(0);
		check("returns 4 when code bit 0x100 clear",
		      p.MsgUnregisterClient(m.AsMessage()) == 4);

		m.SetFlags(0x1);
		m.SetWord10(0xffffffff);
		check("returns 9 for handle == 0xFFFFFFFF",
		      p.MsgUnregisterClient(m.AsMessage()) == 9);

		m.SetWord10(2); /* out of range, count == 2 */
		check("returns 9 for out-of-range handle",
		      p.MsgUnregisterClient(m.AsMessage()) == 9);

		g_outLinkNotifyCalls = 0;
		m.SetWord10(1); /* blobB, not connected */
		check("returns 2 for a valid but NOT-connected handle",
		      p.MsgUnregisterClient(m.AsMessage()) == 2);
		check("no Api+0x58 notify fired for the not-connected case",
		      g_outLinkNotifyCalls == 0);

		m.SetWord10(0); /* blobA, connected */
		check("returns 0 for a valid, connected handle",
		      p.MsgUnregisterClient(m.AsMessage()) == 0);
		check("Api+0x58 notify fired exactly once with the real client pointer",
		      g_outLinkNotifyCalls == 1 && g_lastOutLinkNotified == blobA);

		PollerTestHooks::SetClients(p, 0, 0);
	}

	printf("[12] MsgSetEncoderClient()/MsgSetTouchPanelClient()/MsgSetKeyboardClient()\n");
	{
		CPoller p(owner, 0);

		unsigned char blobA[0x18]; memset(blobA, 0, sizeof(blobA));
		unsigned char blobB[0x18]; memset(blobB, 0, sizeof(blobB));
		*(int *)(blobA + 0x14) = 1; /* connected */
		*(int *)(blobB + 0x14) = 0; /* NOT connected */
		void *elems[2] = { blobA, blobB };
		PollerTestHooks::SetClients(p, elems, elems + 2);

		FakeMessage m;
		m.SetFlags(0);
		check("MsgSetEncoderClient() returns 4 when bit clear",
		      p.MsgSetEncoderClient(m.AsMessage()) == 4);

		m.SetFlags(0x1);
		m.SetWord10(0xffffffff);
		check("MsgSetEncoderClient(handle=-1) returns 0 (clears the slot)",
		      p.MsgSetEncoderClient(m.AsMessage()) == 0);
		check("mField394 == 0xFFFFFFFF after clear",
		      PollerTestHooks::Field394(p) == 0xffffffffu);

		m.SetWord10(1); /* blobB, not connected */
		check("MsgSetEncoderClient(not-connected handle) returns 9",
		      p.MsgSetEncoderClient(m.AsMessage()) == 9);
		check("mField394 left reset (0xFFFFFFFF), not the failed handle",
		      PollerTestHooks::Field394(p) == 0xffffffffu);

		m.SetWord10(5); /* out of range */
		check("MsgSetEncoderClient(out-of-range handle) returns 9",
		      p.MsgSetEncoderClient(m.AsMessage()) == 9);

		m.SetWord10(0); /* blobA, connected */
		check("MsgSetEncoderClient(valid connected handle) returns 0",
		      p.MsgSetEncoderClient(m.AsMessage()) == 0);
		check("mField394 == 0 (the validated handle)",
		      PollerTestHooks::Field394(p) == 0u);

		m.SetWord10(0);
		check("MsgSetTouchPanelClient(valid connected handle) returns 0",
		      p.MsgSetTouchPanelClient(m.AsMessage()) == 0);
		check("mField398 == 0", PollerTestHooks::Field398(p) == 0u);

		m.SetWord10(0);
		check("MsgSetKeyboardClient(valid connected handle) returns 0",
		      p.MsgSetKeyboardClient(m.AsMessage()) == 0);
		check("mField39c == 0", PollerTestHooks::Field39c(p) == 0u);

		PollerTestHooks::SetClients(p, 0, 0);
	}

	printf("[13] MsgRegisterClientByVal()/MsgRegisterClientByRef()\n");
	{
		CPoller p(owner, 0);

		FakeMessage m;
		m.SetFlags(0);
		check("MsgRegisterClientByVal() returns 4 when code bit 0x200 clear",
		      p.MsgRegisterClientByVal(m.AsMessage()) == 4);

		m.SetFlags(0x2);
		m.SetTaggedLen(0x63); /* == threshold, real check is strictly > */
		check("MsgRegisterClientByVal() returns 5 at the length threshold (0x63)",
		      p.MsgRegisterClientByVal(m.AsMessage()) == 5);

		m.SetTaggedLen(0x64);
		unsigned char valBuf[0x40]; memset(valBuf, 0, sizeof(valBuf));
		m.SetWord10((unsigned int)(unsigned long)valBuf);
		check("MsgRegisterClientByVal() returns 6 when embedded name A is empty",
		      p.MsgRegisterClientByVal(m.AsMessage()) == 6);

		valBuf[0x4] = 'X';
		check("MsgRegisterClientByVal() returns 6 when embedded name B is empty",
		      p.MsgRegisterClientByVal(m.AsMessage()) == 6);

		valBuf[0x34] = 'Y';
		int r = p.MsgRegisterClientByVal(m.AsMessage());
		check("MsgRegisterClientByVal() succeeds once both names are non-empty",
		      r == 0);
		/* Real RegisterClient(): mClients starts empty, so this is a genuine
		 * Phase-3 "append a brand-new CIfcClient" -- lands at index 0.
		 */
		check("RegisterClient() real append landed at index 0 (payload+0 write-back)",
		      *(unsigned int *)valBuf == 0u);
		check("Api+0x44 notify fired once, nameA/nameB forwarded verbatim",
		      g_registerNotifyCalls == 1 &&
		      strcmp(g_lastRegisterNameA, "X") == 0 &&
		      strcmp(g_lastRegisterNameB, "Y") == 0);
		check("Api+0x44 notify got the real owner-module/task names",
		      g_lastRegisterOwnerName != 0 && strcmp(g_lastRegisterOwnerName, "TestModule") == 0 &&
		      g_lastRegisterTaskName != 0 && strcmp(g_lastRegisterTaskName, "Poller") == 0);
		check("mClients now has exactly 1 registered element",
		      p.IsValidHandle(0) && !p.IsValidHandle(1));

		FakeMessage m2;
		m2.SetFlags(0x2);
		m2.SetTaggedLen(0xb);
		check("MsgRegisterClientByRef() returns 5 at the length threshold (0xb)",
		      p.MsgRegisterClientByRef(m2.AsMessage()) == 5);

		m2.SetTaggedLen(0xc);
		unsigned int refBuf[3] = { 0, 0, 0 };
		m2.SetWord10((unsigned int)(unsigned long)refBuf);
		check("MsgRegisterClientByRef() returns 6 when name-A pointer is NULL",
		      p.MsgRegisterClientByRef(m2.AsMessage()) == 6);

		const char *nameA = "ClientA";
		refBuf[1] = (unsigned int)(unsigned long)nameA;
		check("MsgRegisterClientByRef() returns 6 when name-B pointer is NULL",
		      p.MsgRegisterClientByRef(m2.AsMessage()) == 6);

		const char *nameB = "ClientB";
		refBuf[2] = (unsigned int)(unsigned long)nameB;
		int r2 = p.MsgRegisterClientByRef(m2.AsMessage());
		check("MsgRegisterClientByRef() succeeds once both name pointers are set",
		      r2 == 0);
		/* Real, preserved quirk (poller.h's own header comment): the client
		 * RegisterClient() just created is still "unconnected" (its embedded
		 * COutLink::mLinks.mCount == 0 -- nothing in RegisterClient() itself
		 * ever marks a client connected, same "not populated by any ctor in
		 * this family" quirk out_link.h already documents for mLink). So this
		 * SECOND call's Phase-2 scan finds THAT SAME slot free and REUSES it
		 * (index 0 again) instead of appending a second client -- it does NOT
		 * get rebound to (ClientA, ClientB); the Api+0x44 notify below fires
		 * with the ORIGINAL "Out_<mID>" name once more.
		 */
		check("RegisterClient() reused the SAME still-unconnected slot (index 0), "
		      "did not append a second client",
		      refBuf[0] == 0u && p.IsValidHandle(0) && !p.IsValidHandle(1));
		check("Api+0x44 notify fired a 2nd time, this call's own nameA/nameB forwarded",
		      g_registerNotifyCalls == 2 &&
		      strcmp(g_lastRegisterNameA, "ClientA") == 0 &&
		      strcmp(g_lastRegisterNameB, "ClientB") == 0);
	}

	printf("\n%s\n", g_fail ? "FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
