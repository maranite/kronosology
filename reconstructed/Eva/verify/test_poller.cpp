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
 *   [15] FindRegisteredClient()/MsgGetClientHandleByRef()/MsgGetClientHandleByVal()
 *        (2026-07-26 FindRegisteredClient batch): name-only vs name-pair search
 *        against a hand-built mClients array of connected/unconnected fake clients
 *        (own +0x1c/+0x10/+0x3c/+4/+4 pointer chain poked directly, same technique
 *        RegisterClient()'s own [13] KAT would need if it exercised Phase-1), both
 *        real "not found"/"array empty"/"nameA missing" distinct return codes, and
 *        both Msg*() wrappers' own gates (code bit, length threshold, embedded vs
 *        pointer payload shape, optional nameB) forwarding correctly to the real
 *        FindRegisteredClient() and writing the result back to the right slot.
 *   [14] MsgSetLed()/MsgSetLed16bits()/MsgBackupLEDs() (2026-07-26 CLEDBlinker/
 *        final-prerequisites follow-up batch): the +0x9 bit-0x2 gate + length==8
 *        check, the real "mResource == 0 -> return 0 with NO CLEDBlinker call at
 *        all" early-out both handlers share (confirmed: ground truth nests the
 *        entire CLEDBlinker::Register/Unregister dispatch inside the mResource
 *        check, not just the notify), all 3 real MsgSetLed() state branches
 *        (1="on"/2="blink"/other="off") including the real "already in that
 *        state -> no notify" early-outs and the real "blink never touches
 *        mZeroBlock or notifies" quirk, MsgSetLed16bits()'s real mask/newBits
 *        split, and MsgBackupLEDs()'s real save-vs-restore direction switch plus
 *        the 32-notify sweep on success. Cross-checks CLEDBlinker's own global
 *        mCount/mBitmap state directly (led_blinker.h), confirming CPoller's
 *        handlers and CLEDBlinker's own methods agree on the same LED bit.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "poller.h"
#include "led_blinker.h"
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
	static const unsigned char *LedBackup(const CPoller &p) { return p.mLedBackup; }
};

/* Same friend name led_blinker.h already declares -- separate verify binary from
 * test_led_blinker.cpp, no ODR conflict (each verify test file links its own
 * standalone program).
 */
struct LEDBlinkerTestHooks {
	static int Count(const CLEDBlinker &b) { return b.mCount; }
	static unsigned short Bitmap(const CLEDBlinker &b, int i) { return b.mBitmap[i]; }
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

	printf("[14] MsgSetLed()/MsgSetLed16bits()/MsgBackupLEDs() (CLEDBlinker unlock)\n");
	{
		new (&s_oLEDBlinker) CLEDBlinker(); /* reset the real global to a clean state */

		g_fakeLookupResult = &g_fakeResObj;
		g_fakeResourceType = 10;
		g_fakeConnectResult = 0;
		CPoller p(owner, "LedRes");

		/* --- MsgSetLed(), mResource == 0 case: no CLEDBlinker call at all --- */
		{
			CPoller pNoRes(owner, 0); /* ctor fallback path, mResource stays 0 */
			FakeMessage m;
			m.SetFlags(0x2);
			m.SetTaggedLen(8);
			int payload[2] = { 3, 1 }; /* ledCode=3, state=1 ("on") */
			m.SetWord10((unsigned int)(unsigned long)payload);
			check("MsgSetLed() returns 0 immediately when mResource == 0",
			      pNoRes.MsgSetLed(m.AsMessage()) == 0);
			check("MsgSetLed() with mResource==0 never touches CLEDBlinker",
			      LEDBlinkerTestHooks::Count(s_oLEDBlinker) == 0);
		}

		/* --- MsgSetLed() gates: code bit + length --- */
		{
			FakeMessage m;
			m.SetFlags(0);
			check("MsgSetLed() returns 4 when code bit 0x2 clear",
			      p.MsgSetLed(m.AsMessage()) == 4);
			m.SetFlags(0x2);
			m.SetTaggedLen(7);
			check("MsgSetLed() returns 5 when length != 8",
			      p.MsgSetLed(m.AsMessage()) == 5);
		}

		/* --- MsgSetLed() state==1 ("on"): sets mZeroBlock bit, unregisters from
		 * the blinker, notifies with {opcode=2, ledCode}.
		 */
		{
			FakeMessage m;
			m.SetFlags(0x2);
			m.SetTaggedLen(8);
			int payload[2] = { 5, 1 }; /* ledCode=5, state=1 */
			m.SetWord10((unsigned int)(unsigned long)payload);
			g_notifyCalls = 0;

			check("MsgSetLed(on) returns 0", p.MsgSetLed(m.AsMessage()) == 0);
			const unsigned short *zb =
			    reinterpret_cast<const unsigned short *>(PollerTestHooks::ZeroBlock(p));
			check("MsgSetLed(on) sets mZeroBlock word0 bit5", (zb[0] & (1 << 5)) != 0);
			check("MsgSetLed(on) notifies {opcode=2, value=ledCode}",
			      g_notifyCalls == 1 && g_lastNotifyOpcode == 2 && g_lastNotifyValue == 5);

			/* Real early-out: calling "on" again on an already-on LED returns 0
			 * with NO further notify.
			 */
			g_notifyCalls = 0;
			check("MsgSetLed(on) on an already-on LED is a no-op notify-wise",
			      p.MsgSetLed(m.AsMessage()) == 0 && g_notifyCalls == 0);
		}

		/* --- MsgSetLed() state==2 ("blink"): registers with the global blinker,
		 * returns immediately -- NO mZeroBlock update, NO notify.
		 */
		{
			FakeMessage m;
			m.SetFlags(0x2);
			m.SetTaggedLen(8);
			int payload[2] = { 9, 2 }; /* ledCode=9, state=2 */
			m.SetWord10((unsigned int)(unsigned long)payload);
			g_notifyCalls = 0;
			int countBefore = LEDBlinkerTestHooks::Count(s_oLEDBlinker);

			check("MsgSetLed(blink) returns 0", p.MsgSetLed(m.AsMessage()) == 0);
			check("MsgSetLed(blink) registers the LED with the global blinker",
			      LEDBlinkerTestHooks::Count(s_oLEDBlinker) == countBefore + 1 &&
			      (LEDBlinkerTestHooks::Bitmap(s_oLEDBlinker, 0) & (1 << 9)) != 0);
			const unsigned short *zb =
			    reinterpret_cast<const unsigned short *>(PollerTestHooks::ZeroBlock(p));
			check("MsgSetLed(blink) does NOT touch mZeroBlock bit9",
			      (zb[0] & (1 << 9)) == 0);
			check("MsgSetLed(blink) fires NO notify", g_notifyCalls == 0);
		}

		/* --- MsgSetLed() state==other ("off"): mirrors "on", opcode=1 --- */
		{
			FakeMessage m;
			m.SetFlags(0x2);
			m.SetTaggedLen(8);
			int payload[2] = { 5, 0 }; /* ledCode=5 (currently ON from above), state=0 */
			m.SetWord10((unsigned int)(unsigned long)payload);
			g_notifyCalls = 0;

			check("MsgSetLed(off) returns 0", p.MsgSetLed(m.AsMessage()) == 0);
			const unsigned short *zb =
			    reinterpret_cast<const unsigned short *>(PollerTestHooks::ZeroBlock(p));
			check("MsgSetLed(off) clears mZeroBlock word0 bit5", (zb[0] & (1 << 5)) == 0);
			check("MsgSetLed(off) notifies {opcode=1, value=ledCode}",
			      g_notifyCalls == 1 && g_lastNotifyOpcode == 1 && g_lastNotifyValue == 5);

			g_notifyCalls = 0;
			check("MsgSetLed(off) on an already-off LED is a no-op notify-wise",
			      p.MsgSetLed(m.AsMessage()) == 0 && g_notifyCalls == 0);
		}

		/* --- MsgSetLed16bits(): mask/newBits split, unconditional CLEDBlinker
		 * unregister of the masked bits (regardless of newBits), notify only if
		 * the word actually changed.
		 */
		{
			FakeMessage m;
			m.SetFlags(0x2);
			m.SetTaggedLen(8);
			/* group index 1 (a short at the payload's own +0), packed dword:
			 * low16 = newBits (bits 2,3 set), high16 = mask (bits 0,1,2,3).
			 */
			unsigned int payload[2];
			*reinterpret_cast<short *>(&payload[0]) = 1;
			payload[1] = (0xfu << 16) | 0xcu;
			m.SetWord10((unsigned int)(unsigned long)payload);
			g_notifyCalls = 0;

			check("MsgSetLed16bits() returns 0", p.MsgSetLed16bits(m.AsMessage()) == 0);
			const unsigned short *zb =
			    reinterpret_cast<const unsigned short *>(PollerTestHooks::ZeroBlock(p));
			check("MsgSetLed16bits() sets bits 2,3 and clears bits 0,1 in word1",
			      zb[1] == 0xc);
			check("MsgSetLed16bits() notifies {opcode=6, value=(word<<16)|group}",
			      g_notifyCalls == 1 && g_lastNotifyOpcode == 6 &&
			      g_lastNotifyValue == ((0xcu << 16) | 1u));

			/* Real: unregisters ALL 4 masked bits from the global blinker,
			 * regardless of which ones newBits would keep "on."
			 */
			check("MsgSetLed16bits() unregistered all 4 masked bits from the blinker",
			      (LEDBlinkerTestHooks::Bitmap(s_oLEDBlinker, 1) & 0xf) == 0);

			/* Calling again with the identical word is a real no-notify no-op
			 * (word unchanged).
			 */
			g_notifyCalls = 0;
			check("MsgSetLed16bits() with an unchanged word fires no notify",
			      p.MsgSetLed16bits(m.AsMessage()) == 0 && g_notifyCalls == 0);
		}

		/* --- MsgBackupLEDs(): save-and-clear, then restore --- */
		{
			FakeMessage m;
			m.SetFlags(0);
			check("MsgBackupLEDs() returns 4 when code bit 0x1 clear",
			      p.MsgBackupLEDs(m.AsMessage()) == 4);
			m.SetFlags(0x1);

			const unsigned short *zb =
			    reinterpret_cast<const unsigned short *>(PollerTestHooks::ZeroBlock(p));
			unsigned short word1Before = zb[1]; /* 0xc, from MsgSetLed16bits() above */

			m.SetWord10(1); /* nonzero -> SAVE-AND-CLEAR */
			g_notifyCalls = 0;
			check("MsgBackupLEDs(save) returns 0", p.MsgBackupLEDs(m.AsMessage()) == 0);
			check("MsgBackupLEDs(save) zeroes mZeroBlock", zb[1] == 0);
			const unsigned short *backup =
			    reinterpret_cast<const unsigned short *>(PollerTestHooks::LedBackup(p));
			check("MsgBackupLEDs(save) snapshotted the old value into mLedBackup",
			      backup[1] == word1Before);
			check("MsgBackupLEDs(save) fired the real 32-word notify sweep",
			      g_notifyCalls == 0x20);

			m.SetWord10(0); /* zero -> RESTORE */
			g_notifyCalls = 0;
			check("MsgBackupLEDs(restore) returns 0", p.MsgBackupLEDs(m.AsMessage()) == 0);
			check("MsgBackupLEDs(restore) brings back the saved value",
			      zb[1] == word1Before);
			check("MsgBackupLEDs(restore) also fires the 32-word notify sweep",
			      g_notifyCalls == 0x20);
		}
	}

	printf("[15] FindRegisteredClient()/MsgGetClientHandleByRef()/MsgGetClientHandleByVal()\n");
	{
		/* Builds the real opaque +0x1c -> deref -> +0x10 -> +0x3c/+4 and +4
		 * pointer chain FindRegisteredClient()'s Phase-1-style scan walks, so
		 * this KAT can exercise the real name-match logic end to end rather
		 * than just the gate checks.
		 */
		struct FakeNameChain {
			unsigned char nameRecA[0x10];
			unsigned char nameRec[0x44];
			unsigned char linkQ[0x18];
			int linkPCell;
			unsigned char client[0x20];

			FakeNameChain(const char *regNameA, const char *regNameB, bool connected)
			{
				memset(nameRecA, 0, sizeof(nameRecA));
				memset(nameRec, 0, sizeof(nameRec));
				memset(linkQ, 0, sizeof(linkQ));
				memset(client, 0, sizeof(client));

				*reinterpret_cast<const char **>(nameRecA + 4) = regNameA;
				*reinterpret_cast<int *>(nameRec + 0x3c) =
				    (int)(unsigned long)nameRecA;
				*reinterpret_cast<const char **>(nameRec + 4) = regNameB;
				*reinterpret_cast<int *>(linkQ + 0x10) = (int)(unsigned long)nameRec;
				linkPCell = (int)(unsigned long)linkQ;
				*reinterpret_cast<int *>(client + 0x14) = connected ? 1 : 0;
				*reinterpret_cast<int *>(client + 0x1c) =
				    (int)(unsigned long)&linkPCell;
			}
			unsigned char *ClientPtr() { return client; }
		};

		CPoller p(owner, 0); /* mResource == 0, doesn't matter for this section */

		check("FindRegisteredClient() returns -1 on a genuinely empty mClients",
		      p.FindRegisteredClient("Alice", 0) == -1);

		FakeNameChain c0("Alice", "Bob", true);      /* connected, matches */
		FakeNameChain c1("Carol", "Dave", true);     /* connected, no match */
		FakeNameChain c2("Alice", "Bob", false);     /* UNCONNECTED, would match by name */

		void *elems[3] = { c0.ClientPtr(), c1.ClientPtr(), c2.ClientPtr() };
		PollerTestHooks::SetClients(p, elems, elems + 3);

		check("FindRegisteredClient() finds an exact (nameA,nameB) match on a "
		      "connected client",
		      p.FindRegisteredClient("Alice", "Bob") == 0);
		check("FindRegisteredClient() name-only search (nameB==NULL) also matches",
		      p.FindRegisteredClient("Alice", 0) == 0);
		check("FindRegisteredClient() an empty nameB string is treated as NULL",
		      p.FindRegisteredClient("Alice", "") == 0);
		check("FindRegisteredClient() nameA match but wrong nameB -> not found",
		      p.FindRegisteredClient("Alice", "WrongName") == -1);
		check("FindRegisteredClient() skips the UNCONNECTED client even though "
		      "its name would match (index 2, 'Alice'/'Bob')",
		      p.FindRegisteredClient("Alice", "Bob") != 2);
		check("FindRegisteredClient() returns 0 (not -1) when nameA itself is NULL",
		      p.FindRegisteredClient(0, "Bob") == 0);
		check("FindRegisteredClient() returns 0 when nameA is an empty string",
		      p.FindRegisteredClient("", "Bob") == 0);

		/* --- MsgGetClientHandleByRef(): gates + real forwarding --- */
		{
			FakeMessage m;
			m.SetFlags(0);
			check("MsgGetClientHandleByRef() returns 4 when code bit 0x2 clear",
			      p.MsgGetClientHandleByRef(m.AsMessage()) == 4);

			m.SetFlags(0x2);
			m.SetTaggedLen(0xb); /* below the real 0xc threshold */
			check("MsgGetClientHandleByRef() returns 5 below the length threshold",
			      p.MsgGetClientHandleByRef(m.AsMessage()) == 5);

			m.SetTaggedLen(0xc);
			unsigned int refBuf[3] = { 0xdeadbeef, 0, 0 };
			m.SetWord10((unsigned int)(unsigned long)refBuf);
			check("MsgGetClientHandleByRef() returns 6 when nameA pointer is NULL",
			      p.MsgGetClientHandleByRef(m.AsMessage()) == 6);

			refBuf[1] = (unsigned int)(unsigned long)"Carol";
			refBuf[2] = 0; /* nameB omitted -- must still search by nameA alone */
			int r = p.MsgGetClientHandleByRef(m.AsMessage());
			check("MsgGetClientHandleByRef() succeeds with nameB==NULL", r == 0);
			check("MsgGetClientHandleByRef() writes the real found index back (1, "
			      "'Carol')",
			      refBuf[0] == 1u);

			refBuf[1] = (unsigned int)(unsigned long)"NoSuchName";
			p.MsgGetClientHandleByRef(m.AsMessage());
			check("MsgGetClientHandleByRef() writes -1 back for a genuine miss",
			      refBuf[0] == 0xffffffffu);
		}

		/* --- MsgGetClientHandleByVal(): embedded-buffer payload shape --- */
		{
			FakeMessage m;
			m.SetFlags(0x2);
			m.SetTaggedLen(0x63); /* below the real 0x64 threshold */
			unsigned char valBuf[0x40];
			memset(valBuf, 0, sizeof(valBuf));
			m.SetWord10((unsigned int)(unsigned long)valBuf);
			check("MsgGetClientHandleByVal() returns 5 below the length threshold",
			      p.MsgGetClientHandleByVal(m.AsMessage()) == 5);

			m.SetTaggedLen(0x64);
			check("MsgGetClientHandleByVal() returns 6 when embedded nameA is empty",
			      p.MsgGetClientHandleByVal(m.AsMessage()) == 6);

			strcpy((char *)valBuf + 4, "Alice");
			strcpy((char *)valBuf + 0x34, "Bob");
			int r = p.MsgGetClientHandleByVal(m.AsMessage());
			check("MsgGetClientHandleByVal() succeeds with both embedded names set",
			      r == 0);
			check("MsgGetClientHandleByVal() writes the real found index back (0)",
			      *(unsigned int *)valBuf == 0u);

			/* empty embedded nameB collapses to NULL -- name-only search */
			valBuf[0x34] = 0;
			p.MsgGetClientHandleByVal(m.AsMessage());
			check("MsgGetClientHandleByVal() with an empty embedded nameB searches "
			      "by nameA alone",
			      *(unsigned int *)valBuf == 0u);
		}

		PollerTestHooks::SetClients(p, 0, 0);
	}

	printf("\n%s\n", g_fail ? "FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
