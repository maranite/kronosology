/*
 * test_client_comm_server.cpp  -  host-side known-answer test for
 * CClientCommServer (src/ipc/client_comm_server.cpp, Stage 6 breadth sweep,
 * 2026-07-25; follow-up pass same day adds ctor/dtor/5 more leaf methods once
 * CEvBuffersPool/CEvent became real).
 *
 * ComputeCRCByte()/CheckIncomingSexCRCByte() are a running XOR checksum over a byte
 * range -- easy to hand-compute expected values for small buffers and cross-check
 * the collapsed-loop implementation against a trivial reference loop.
 */

#include <cstdio>
#include <cstring>

#include "client_comm_server.h"
#include "event.h"
#include "module.h"
#include "sysex_msg_task_base.h"

/* CSexServiceTask::TransmitSysEx()/COutLinkMono::OutMono() are given minimal
 * counting-stub bodies directly in client_comm_server.cpp (not here) -- see that
 * file's own comment for why (this project's verify Makefile links every object
 * into every verify binary, so a test-local fake definition here would collide
 * with -- or be needed by -- every OTHER verify binary too). The counters below
 * are that stub's own test-observable instrumentation, extern-declared here.
 */
extern int g_ccsTestTransmitSysExCalls;
extern unsigned char g_ccsTestLastTransmitEcb;
extern int g_ccsTestOutMonoCalls;

/* Friend hook -- pokes mEvTag/mEvBuf directly so ComputeCRCByte() can be
 * exercised without running the real constructor (used only by the
 * ComputeCRCByte()/CheckIncomingSexCRCByte() block below, which predates
 * CEvBuffersPool/CEvent being real). Same convention as module.h's ModuleTestHooks.
 */
struct ClientCommServerTestHooks {
	static void SetEvent(CClientCommServer &obj, const unsigned char *buf, unsigned char len)
	{
		obj.mEvTag = (int)((unsigned)len << 16);
		obj.mEvBuf = (void *)buf;
	}
	static int EvTag(const CClientCommServer &obj) { return obj.mEvTag; }
	static void *EvBuf(const CClientCommServer &obj) { return obj.mEvBuf; }
	static unsigned char State0d(const CClientCommServer &obj) { return obj.mState0d; }
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Reference implementation -- a naive, unrolled-loop-free XOR fold, matching
 * ComputeCRCByte()'s own documented semantics: crc = buf[start], then XOR in
 * buf[start+1 .. len-1] (byte-wrapping index).
 */
static unsigned char ReferenceComputeCRC(const unsigned char *buf, unsigned char len,
                                          unsigned char start)
{
	unsigned char crc = buf[start];
	for (unsigned char i = (unsigned char)(start + 1); i < len; i = (unsigned char)(i + 1))
		crc ^= buf[i];
	return crc;
}

int main()
{
	/* CClientCommServer has a Tier-B ctor requiring real CSexServiceTask&/
	 * CSysExMsgTaskBase& objects this pass doesn't construct -- allocate raw
	 * storage and only exercise the two Tier-A pure methods, matching
	 * test_module_adjust_task_mask.cpp's own "poke the real offsets, skip the
	 * real ctor" pattern.
	 */
	union {
		double alignForcer;
		unsigned char bytes[sizeof(CClientCommServer)];
	} raw;
	memset(raw.bytes, 0, sizeof(raw.bytes));
	CClientCommServer *obj = reinterpret_cast<CClientCommServer *>(raw.bytes);

	/* --- ComputeCRCByte --- */
	{
		unsigned char buf[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x00};
		ClientCommServerTestHooks::SetEvent(*obj, buf, 8);

		for (unsigned char start = 0; start < 7; start++) {
			unsigned char expect = ReferenceComputeCRC(buf, 8, start);
			unsigned char got = obj->ComputeCRCByte(start);
			char label[64];
			snprintf(label, sizeof(label), "ComputeCRCByte(start=%u) == 0x%02x", start, expect);
			check(label, got == expect);
		}

		/* Single-byte range (start == len-1): crc is just buf[start], no folding. */
		ClientCommServerTestHooks::SetEvent(*obj, buf, 1);
		check("ComputeCRCByte single-byte range", obj->ComputeCRCByte(0) == buf[0]);

		/* Longer buffer, exercises the >8-byte unrolled-loop-equivalent path. */
		unsigned char big[20];
		for (int i = 0; i < 20; i++) big[i] = (unsigned char)(i * 7 + 3);
		ClientCommServerTestHooks::SetEvent(*obj, big, 20);
		check("ComputeCRCByte 20-byte buffer, start=0",
		      obj->ComputeCRCByte(0) == ReferenceComputeCRC(big, 20, 0));
		check("ComputeCRCByte 20-byte buffer, start=5",
		      obj->ComputeCRCByte(5) == ReferenceComputeCRC(big, 20, 5));
	}

	/* --- CheckIncomingSexCRCByte --- */
	{
		/* len < 2 always rejects, regardless of content. */
		unsigned char one[1] = {0x00};
		check("CheckIncomingSexCRCByte len=0 rejects", obj->CheckIncomingSexCRCByte(one, 0) == false);
		check("CheckIncomingSexCRCByte len=1 rejects", obj->CheckIncomingSexCRCByte(one, 1) == false);

		/* A buffer whose own trailing byte is the running XOR of the rest
		 * self-verifies (crc folds to 0).
		 */
		unsigned char valid[5] = {0x11, 0x22, 0x33, 0x44, 0x00};
		unsigned char x = 0;
		for (int i = 0; i < 4; i++) x ^= valid[i];
		valid[4] = x;
		check("CheckIncomingSexCRCByte accepts self-consistent buffer",
		      obj->CheckIncomingSexCRCByte(valid, 5) == true);

		/* Corrupting any single byte must reject. */
		unsigned char corrupt[5] = {0x11, 0x22, 0x33, 0x44, (unsigned char)(x ^ 0x01)};
		check("CheckIncomingSexCRCByte rejects corrupted buffer",
		      obj->CheckIncomingSexCRCByte(corrupt, 5) == false);

		/* Longer (>16-byte) self-consistent buffer, exercises the SIMD-fold-
		 * equivalent path the real binary uses for len >= 0x11.
		 */
		unsigned char longbuf[24];
		for (int i = 0; i < 23; i++) longbuf[i] = (unsigned char)(i * 13 + 1);
		unsigned char lx = 0;
		for (int i = 0; i < 23; i++) lx ^= longbuf[i];
		longbuf[23] = lx;
		check("CheckIncomingSexCRCByte accepts 24-byte self-consistent buffer",
		      obj->CheckIncomingSexCRCByte(longbuf, 24) == true);
		longbuf[23] ^= 0x80;
		check("CheckIncomingSexCRCByte rejects corrupted 24-byte buffer",
		      obj->CheckIncomingSexCRCByte(longbuf, 24) == false);
	}

	/* --- Real ctor/dtor + the 5 newly-real leaf methods, driven through an
	 * actual CClientCommServer instance this time (not the raw-bytes trick
	 * above, which predates CEvBuffersPool/CEvent being real) -- same
	 * "construct real dependencies on the stack" pattern
	 * test_sysex_msg_task_base.cpp's own ctor test already established.
	 */
	{
		CModule ownerModule("SysExOwnerModuleForCCS");
		CSysExMsgTaskBase client(ownerModule, 0, 0);
		CSexServiceTask owner;

		g_ccsTestTransmitSysExCalls = 0;
		g_ccsTestOutMonoCalls = 0;

		CClientCommServer ccs(owner, client, /*ecb=*/0x42, CClientCommServer::eCommModeReserved,
		                        CClientCommServer::eServiceReserved, /*outLink=*/0);

		check("ctor: mEvTag is negative (owns a real pool-allocated buffer)",
		      ClientCommServerTestHooks::EvTag(ccs) < 0);
		check("ctor: mEvTag's low byte is the event class-code 0x0a",
		      (ClientCommServerTestHooks::EvTag(ccs) & 0xff) == 0x0a);
		check("ctor: mEvBuf is non-null (CEvBuffersPool::Alloc()+Lock() both real now)",
		      ClientCommServerTestHooks::EvBuf(ccs) != 0);

		ccs.SendToSysExLink();
		check("SendToSysExLink() calls CSexServiceTask::TransmitSysEx() exactly once",
		      g_ccsTestTransmitSysExCalls == 1);
		check("SendToSysExLink() passes this client's own ecb through",
		      g_ccsTestLastTransmitEcb == 0x42);

		ccs.RetryTXPacket();
		check("RetryTXPacket() also calls TransmitSysEx()", g_ccsTestTransmitSysExCalls == 2);

		unsigned char before = ClientCommServerTestHooks::State0d(ccs);
		ccs.OnProcessRetry(0 /* == mState0c's own ctor-zeroed value */);
		check("OnProcessRetry() with a matching expected-state bumps the retry counter"
		      " and resends",
		      ClientCommServerTestHooks::State0d(ccs) == static_cast<unsigned char>(before + 1) &&
		          g_ccsTestTransmitSysExCalls == 3);

		int callsBeforeMismatch = g_ccsTestTransmitSysExCalls;
		ccs.OnProcessRetry(0xff /* deliberately wrong expected state */);
		check("OnProcessRetry() with a MISMATCHED expected state does NOT resend",
		      g_ccsTestTransmitSysExCalls == callsBeforeMismatch);

		void *evBufBeforeTx = ClientCommServerTestHooks::EvBuf(ccs);
		ccs.TXData();
		check("TXData() also calls TransmitSysEx()", g_ccsTestTransmitSysExCalls == 4);
		check("TXData() leaves mEvTag negative (still owns a buffer) after re-Lock()",
		      ClientCommServerTestHooks::EvTag(ccs) < 0);
		check("TXData() leaves mEvBuf non-null after its own re-acquire sequence",
		      ClientCommServerTestHooks::EvBuf(ccs) != 0);
		(void)evBufBeforeTx;

		ccs.SendMessageToClient();
		check("SendMessageToClient() calls COutLinkMono::OutMono() exactly once",
		      g_ccsTestOutMonoCalls == 1);

		/* ~CClientCommServer() runs at scope exit -- if mEvBuf's chunk header
		 * or CEvBuffersPool's own free lists were corrupted by any of the
		 * above, a further pool operation after this block (see the trailing
		 * post-destruction alloc below) would misbehave/crash.
		 */
	}
	{
		/* Pool sanity after a real CClientCommServer's full lifetime: alloc a
		 * fresh small chunk and confirm the pool still hands out a valid,
		 * independently-writable buffer (i.e. the ctor/dtor's own Alloc/Lock/
		 * Free traffic against CEvent::sm_oEvBuffersPool left it in a
		 * consistent state).
		 */
		void *p = CEvent::sm_oEvBuffersPool.Alloc(8);
		check("CEvBuffersPool still healthy after a full CClientCommServer lifetime",
		      p != 0);
		std::memset(p, 0x99, 8);
		check("...and the returned chunk is independently writable",
		      static_cast<unsigned char *>(p)[0] == 0x99);
		CEvent::sm_oEvBuffersPool.Free(p);
	}

	printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
