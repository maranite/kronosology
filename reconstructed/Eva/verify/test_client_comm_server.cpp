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
#include "out_link.h"
#include "sysex_msg_task_base.h"

/* CSexServiceTask::TransmitSysEx() is given a minimal counting-stub body directly in
 * client_comm_server.cpp (not here) -- see that file's own comment for why (this
 * project's verify Makefile links every object into every verify binary, so a
 * test-local fake definition here would collide with -- or be needed by -- every
 * OTHER verify binary too). The counter below is that stub's own test-observable
 * instrumentation, extern-declared here. `COutLinkMono::OutMono()` itself is now a
 * REAL reconstructed method (out_link.h) -- exercised directly below via a real
 * `CSysExMsgClientOutLink` instead of a counting stub.
 */
extern int g_ccsTestTransmitSysExCalls;
extern unsigned char g_ccsTestLastTransmitEcb;

/* Fake-CMessage builder for the OnReceiveMessage() KAT below (plain function,
 * not a lambda -- this project's build is -std=gnu++98). See that KAT's own
 * comment for the field-offset derivation.
 */
static void SetupFakeMsg(unsigned char *msgBuf, void *linkBuf, unsigned char len,
                          void *payload)
{
	std::memset(msgBuf, 0, 0x18);
	*reinterpret_cast<void **>(msgBuf + 0x4) = linkBuf;
	msgBuf[0xa] = len;
	*reinterpret_cast<void **>(msgBuf + 0x10) = payload;
}

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

	/* Added for the second follow-up pass's OnRxPacket()/OnReceiveSysExBuffer()/
	 * OnRxSexWhenInWAIT()/TransmitSexAnswer() KATs below -- same "friend pokes the
	 * real offsets directly" convention as the hooks above.
	 */
	static int State(const CClientCommServer &obj) { return obj.mState; }
	static void SetState(CClientCommServer &obj, int state) { obj.mState = state; }
	static unsigned char Unknown0e(const CClientCommServer &obj) { return obj.mUnknown0e; }
	static void SetUnknown0e(CClientCommServer &obj, unsigned char v) { obj.mUnknown0e = v; }
	static void SetModeService(CClientCommServer &obj, unsigned char v) { obj.mModeService = v; }
	static unsigned char Unknown1c(const CClientCommServer &obj) { return obj.mUnknown1c; }

	/* Added for the third follow-up pass's PrepareMsgBuffer()/UnprepareBuffer()/
	 * EventToMessage()/MessageToEvent()/Error()/OnRx{Msg,Sex}WhenIn{IDLE,SENT}()
	 * KATs below -- same convention as the hooks above.
	 */
	static void *TxBuf(const CClientCommServer &obj) { return obj.mTxBuf; }
	static unsigned char State0c(const CClientCommServer &obj) { return obj.mState0c; }
	static void SetState0c(CClientCommServer &obj, unsigned char v) { obj.mState0c = v; }
	static unsigned char Unknown08(const CClientCommServer &obj) { return obj.mUnknown08; }
	static void SetUnknown08(CClientCommServer &obj, int v) { obj.mUnknown08 = v; }
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

		/* Real CSysExMsgClientOutLink (out_link.h) -- not a null placeholder
		 * anymore, since SendMessageToClient() below now calls straight through
		 * to the real COutLinkMono::OutMono(), which dereferences `this`
		 * unconditionally (mLinks' own count check comes first).
		 */
		CSysExMsgClientOutLink outLink(client);

		g_ccsTestTransmitSysExCalls = 0;

		CClientCommServer ccs(owner, client, /*ecb=*/0x42, CClientCommServer::eCommModeReserved,
		                        CClientCommServer::eServiceReserved, &outLink);

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

		/* Real COutLinkMono::OutMono(): `outLink`'s own mLinks array starts empty
		 * (no real Connect()-type call has run), so ground truth's own real early
		 * return (error 5, "no destination registered") fires -- exercised here
		 * directly against the same outLink to check the actual return code,
		 * since SendMessageToClient() itself discards it (matching ground truth).
		 */
		unsigned char outMonoProbeBuf[1] = {0};
		check("outLink.OutMono() returns 5 (no destination CLink registered yet)",
		      outLink.OutMono(0x42, outMonoProbeBuf, 1) == 5);
		ccs.SendMessageToClient();
		check("SendMessageToClient() reaches COutLinkMono::OutMono() without crashing"
		      " (mOutLink's real inheritance chain, non-virtual call)",
		      true);

		/* --- TransmitSexAnswer() -- second follow-up pass, real body. --------- */
		{
			int callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.TransmitSexAnswer(static_cast<CClientCommServer::ESexMsgType>(2), 0x77);
			check("TransmitSexAnswer() calls TransmitSysEx() exactly once",
			      g_ccsTestTransmitSysExCalls == callsBefore + 1);

			const unsigned char *evBuf =
			    static_cast<const unsigned char *>(ClientCommServerTestHooks::EvBuf(ccs));
			check("TransmitSexAnswer() writes type into mEvBuf[5]", evBuf[5] == 2);
			check("TransmitSexAnswer() writes x into mEvBuf[6]", evBuf[6] == 0x77);
			/* NOTE: the length field (mEvTag bits 16-23) is set to 7 right
			 * before the TransmitSysEx() call, but ground truth's own SECOND
			 * reacquire cycle right after the call (`and eax,0xff00; or
			 * eax,0x8000000a`) clears it straight back to 0 before returning --
			 * same idiom TXData() already uses -- so it is NOT observable from
			 * outside this method once it returns; not asserted here for that
			 * reason (reproduced faithfully, not a test gap).
			 */
			int tag = ClientCommServerTestHooks::EvTag(ccs);
			check("TransmitSexAnswer() leaves mEvTag owning a buffer (still negative)",
			      tag < 0);
		}

		/* --- OnRxPacket() -- second follow-up pass, real body. ---------------
		 * mModeService's own bit 0x20 gate is a REAL early-return check (unlike
		 * most of this file's soft asserts) -- exercise both sides of it.
		 */
		{
			unsigned char pkt[8] = {0x11, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00};

			ClientCommServerTestHooks::SetModeService(ccs, 0x00); /* bit 0x20 clear */
			ClientCommServerTestHooks::SetState(ccs, 5 /* sentinel */);
			int callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxPacket(pkt, 8, 0x33);
			/* Error() is now real (third follow-up pass) -- the real gate still
			 * blocks dispatch (no TransmitSysEx() call from OnRxPacket() itself),
			 * but Error(0)'s own real final reset DOES now fire, setting mState
			 * back to 0 (IDLE) -- updated from this test's original expectation
			 * of "state stays 5" (written back when Error() was still a stub).
			 */
			check("OnRxPacket() with mModeService bit 0x20 clear does NOT dispatch"
			      " (real early-return gate), but DOES reach the now-real Error(0)",
			      g_ccsTestTransmitSysExCalls == callsBefore &&
			          ClientCommServerTestHooks::State(ccs) == 0);

			/* Now set the gate and mUnknown0e = 0xff ("no active tag yet") so the
			 * tag-mismatch soft-assert path isn't taken either.
			 */
			ClientCommServerTestHooks::SetModeService(ccs, 0x20);
			ClientCommServerTestHooks::SetUnknown0e(ccs, 0xff);
			ClientCommServerTestHooks::SetState(ccs, 5);

			/* len<=2: real "too short" resync path -- does NOT touch
			 * PrepareMsgBuffer() at all (that only happens on the good-checksum
			 * success path below), so this exercises OnRxPacket()'s own real
			 * control flow end to end regardless of PrepareMsgBuffer()'s own
			 * status.
			 */
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxPacket(pkt, 2, 0x33);
			check("OnRxPacket() with len<=2 resyncs: mState -> 2 (WAIT)",
			      ClientCommServerTestHooks::State(ccs) == 2);
			check("OnRxPacket() with len<=2 resyncs: mUnknown0e -> data[0]",
			      ClientCommServerTestHooks::Unknown0e(ccs) == pkt[0]);
			check("OnRxPacket() with len<=2 resyncs via TransmitSexAnswer(3,...)"
			      " (one more TransmitSysEx() call)",
			      g_ccsTestTransmitSysExCalls == callsBefore + 1);

			/* A checksum failure takes the SAME resync path. Build an 8-byte
			 * packet whose XOR fold (seeded with len-1) over data[1..len-2]
			 * is deliberately wrong.
			 */
			ClientCommServerTestHooks::SetUnknown0e(ccs, 0xff);
			ClientCommServerTestHooks::SetState(ccs, 5);
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxPacket(pkt, 8, 0x33); /* pkt[1..6] arbitrary, crc will not fold to 0 */
			check("OnRxPacket() with a bad checksum ALSO resyncs: mState -> 2 (WAIT)",
			      ClientCommServerTestHooks::State(ccs) == 2);
			check("OnRxPacket() with a bad checksum resyncs via TransmitSexAnswer(3,...)",
			      g_ccsTestTransmitSysExCalls == callsBefore + 1);

			/* A GOOD checksum takes the success path: mState -> 0 (IDLE),
			 * TransmitSexAnswer(2,...) + SendMessageToClient() both fire (2 more
			 * TransmitSysEx()-shaped effects: one real TransmitSysEx() call from
			 * TransmitSexAnswer(), one COutLinkMono::OutMono() call from
			 * SendMessageToClient() -- the latter isn't counted by
			 * g_ccsTestTransmitSysExCalls, only the former is).
			 */
			/* Real verification folds data[1..len-2] (indices 1..6 for len=8)
			 * XORed together with a seed of (len-1); data[7] (the very last
			 * byte) is never touched by the checksum at all. So good[6] itself
			 * is the byte that makes the fold land on zero -- computed here
			 * over good[1..5] with the same seed, NOT read back out of `good`
			 * before it's set.
			 */
			unsigned char good[8];
			good[0] = 0x11;
			for (int i = 1; i <= 5; i++) good[i] = static_cast<unsigned char>(0x40 + i);
			unsigned char crc = static_cast<unsigned char>(8 - 1);
			for (int i = 1; i <= 5; i++) crc ^= good[i];
			good[6] = crc; /* makes seed ^ good[1..6] fold to 0 */
			good[7] = 0x00;

			ClientCommServerTestHooks::SetUnknown0e(ccs, 0xff);
			ClientCommServerTestHooks::SetState(ccs, 5);
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxPacket(good, 8, 0x33);
			check("OnRxPacket() with a GOOD checksum succeeds: mState -> 0 (IDLE)",
			      ClientCommServerTestHooks::State(ccs) == 0);
			check("OnRxPacket() success path calls TransmitSexAnswer(2,...) for real"
			      " (one more TransmitSysEx() call)",
			      g_ccsTestTransmitSysExCalls == callsBefore + 1);
			check("OnRxPacket() success path resets mUnknown1c to 0 after sending",
			      ClientCommServerTestHooks::Unknown1c(ccs) == 0);
		}

		/* --- OnRxSexWhenInWAIT() -- second follow-up pass, real body. ---------
		 * type==0 tail-calls OnRxPacket() unchanged -- confirm it reaches the
		 * SAME real gating check (mModeService bit 0x20) OnRxPacket() itself
		 * has, proving this is a real re-dispatch and not a no-op.
		 */
		{
			unsigned char pkt[8] = {0x22, 1, 2, 3, 4, 5, 6, 7};

			ClientCommServerTestHooks::SetModeService(ccs, 0x00); /* gate clear */
			ClientCommServerTestHooks::SetState(ccs, 5);
			int callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxSexWhenInWAIT(static_cast<CClientCommServer::ESexMsgType>(0), pkt, 2, 0x00);
			/* Error() is now real -- OnRxPacket()'s own real gate still blocks
			 * the send, but reaches the now-real Error(0), resetting mState to
			 * 0 (IDLE) instead of leaving it untouched (updated from this
			 * test's original expectation, written when Error() was a stub).
			 */
			check("OnRxSexWhenInWAIT(type=0) tail-calls OnRxPacket(): same real gate"
			      " blocks it when mModeService bit 0x20 is clear (reaches the now-real"
			      " Error(0))",
			      g_ccsTestTransmitSysExCalls == callsBefore &&
			          ClientCommServerTestHooks::State(ccs) == 0);

			ClientCommServerTestHooks::SetModeService(ccs, 0x20);
			ClientCommServerTestHooks::SetUnknown0e(ccs, 0xff);
			ClientCommServerTestHooks::SetState(ccs, 5);
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxSexWhenInWAIT(static_cast<CClientCommServer::ESexMsgType>(0), pkt, 2, 0x00);
			check("OnRxSexWhenInWAIT(type=0) tail-call reaches OnRxPacket()'s own"
			      " len<=2 resync path (mState -> 2)",
			      ClientCommServerTestHooks::State(ccs) == 2 &&
			          g_ccsTestTransmitSysExCalls == callsBefore + 1);

			/* type 1-4: Error()-only paths. Error() is now real (third
			 * follow-up pass) -- types 1-3 call Error(0), type 4 calls
			 * Error(1); neither sets mode bit 1 (`&2`), so TransmitSysEx()
			 * is never called from here, but Error()'s own real final reset
			 * DOES fire every time, setting mState back to 0 (IDLE) --
			 * updated from this test's original "touches no state"
			 * expectation (written back when Error() was still a stub).
			 */
			for (int t = 1; t <= 4; t++) {
				ClientCommServerTestHooks::SetState(ccs, 9);
				callsBefore = g_ccsTestTransmitSysExCalls;
				ccs.OnRxSexWhenInWAIT(static_cast<CClientCommServer::ESexMsgType>(t), pkt, 2,
				                       0x00);
				char label[112];
				snprintf(label, sizeof(label),
				         "OnRxSexWhenInWAIT(type=%d) dispatches into the now-real Error():"
				         " mState -> 0 (IDLE), no TransmitSysEx() call",
				         t);
				check(label, ClientCommServerTestHooks::State(ccs) == 0 &&
				                  g_ccsTestTransmitSysExCalls == callsBefore);
			}

			/* type>4: out-of-range, real ground truth returns without even
			 * calling Error() -- genuinely DIFFERENT observable effect from
			 * the type 1-4 cases above now that Error() is real (state stays
			 * untouched here, vs. resetting to IDLE above), confirming this
			 * really is a distinct code path, not just Error()'s own no-op
			 * shell showing through.
			 */
			ClientCommServerTestHooks::SetState(ccs, 9);
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxSexWhenInWAIT(static_cast<CClientCommServer::ESexMsgType>(5), pkt, 2, 0x00);
			check("OnRxSexWhenInWAIT(type=5, out of range) is a no-op",
			      ClientCommServerTestHooks::State(ccs) == 9 &&
			          g_ccsTestTransmitSysExCalls == callsBefore);
		}

		/* --- OnReceiveSysExBuffer() -- second follow-up pass, real body. ------
		 * Real enforcing check: len > 255 -> silent early return. Otherwise
		 * dispatches on mState into OnRxSexWhenIn{IDLE,SENT,WAIT}; only the WAIT
		 * member is real this pass, so drive mState=2 (WAIT) with a type=0
		 * sysex payload and confirm the FULL chain
		 * (OnReceiveSysExBuffer -> OnRxSexWhenInWAIT -> OnRxPacket) runs
		 * end to end.
		 */
		{
			/* [addr overhead: 5 bytes][type=0][payload: 2 bytes] == 8 bytes,
			 * with mModeService's gate set so the OnRxPacket() tail actually
			 * dispatches (len<=2 resync path, same as above).
			 */
			unsigned char buf[8] = {0, 0, 0, 0, 0, /*type*/ 0, /*payload*/ 0xaa, 0xbb};

			ClientCommServerTestHooks::SetModeService(ccs, 0x20);
			ClientCommServerTestHooks::SetUnknown0e(ccs, 0xff);
			ClientCommServerTestHooks::SetState(ccs, 2); /* WAIT */
			int callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnReceiveSysExBuffer(buf, 8, 0x00);
			check("OnReceiveSysExBuffer() len>255 real bounds check + mState dispatch:"
			      " WAIT->type=0 chain reaches OnRxPacket()'s len<=2 resync"
			      " (mState -> 2, one more TransmitSysEx() call)",
			      ClientCommServerTestHooks::State(ccs) == 2 &&
			          g_ccsTestTransmitSysExCalls == callsBefore + 1);

			/* NOTE: `len > 255` can never actually hold -- `len` is `unsigned
			 * char` (max 255) in both ground truth (zero-extended from a byte
			 * register before the compare) and this reconstruction, so this
			 * REAL check is permanently vacuous given the real parameter width,
			 * not exercisable as a KAT. Documented rather than guessed around;
			 * same "real but unreachable given the caller's own type" category
			 * as several other soft-assert-adjacent checks in this file.
			 */
		}

		/* --- PrepareMsgBuffer()/UnprepareBuffer() -- third follow-up pass. ----
		 * Round-trip: UnprepareBuffer() encodes raw bytes into a caller-owned
		 * CLinkedEvent-shaped buffer, PrepareMsgBuffer() decodes the SAME
		 * bytes back out of a plain buffer -- self-consistency is a strong
		 * check for a matched encode/decode pair.
		 */
		{
			ClientCommServerTestHooks::SetModeService(ccs, 0x10); /* framed mode */

			/* A fresh CLinkedEvent-shaped buffer (mTag/mBuf/mNext) backed by a
			 * REAL pool-allocated chunk -- UnprepareBuffer() calls
			 * CEvBuffersPool::Lock() on *evBuf internally, which reads/writes
			 * a real chunk header 4 bytes before the payload pointer; handing
			 * it a raw stack array (no such header) would make Lock() read
			 * garbage and potentially return a DIFFERENT pointer than
			 * expected. Same ctor-established idiom: clear the "just
			 * allocated" state bit before the first Lock().
			 */
			void *poolBuf = CEvent::sm_oEvBuffersPool.Alloc(64);
			*(static_cast<unsigned char *>(poolBuf) - 3) &= 0x7f;
			struct { int tag; void *buf; void *next; } fakeEvent;
			fakeEvent.tag = static_cast<int>(0x8000000a);
			fakeEvent.buf = poolBuf;
			fakeEvent.next = 0;
			CLinkedEvent *ev = reinterpret_cast<CLinkedEvent *>(&fakeEvent);

			/* 10 raw bytes, deliberately including values with bit7 set so the
			 * framing transform's own bit-restoration is actually exercised.
			 */
			unsigned char original[10] = {0x00, 0xff, 0x55, 0xaa, 0x81, 0x40, 0x00, 0xfe, 0x7f, 0x01};
			ccs.UnprepareBuffer(ev, original, 10, /*x=*/0);

			int *tagWord = reinterpret_cast<int *>(&fakeEvent);
			unsigned int packedLen = (static_cast<unsigned>(*tagWord) >> 16) & 0xff;
			check("UnprepareBuffer() 10 bytes -> 2 groups (flag+7, flag+3): packed length"
			      " == 10 + 2 flag bytes == 12",
			      packedLen == 12);

			unsigned char decoded[10];
			memset(decoded, 0xcc, sizeof(decoded));
			unsigned char decodedLen = sizeof(decoded);
			ccs.PrepareMsgBuffer(decoded, decodedLen, static_cast<unsigned char *>(fakeEvent.buf),
			                     static_cast<unsigned char>(packedLen));
			check("PrepareMsgBuffer(UnprepareBuffer(original)) round-trips: length",
			      decodedLen == 10);
			bool roundTripOk = true;
			for (int i = 0; i < 10; i++)
				if (decoded[i] != original[i]) roundTripOk = false;
			check("PrepareMsgBuffer(UnprepareBuffer(original)) round-trips: bytes", roundTripOk);

			/* Raw passthrough (mModeService bit 0x10 clear) on both sides. */
			ClientCommServerTestHooks::SetModeService(ccs, 0x00);
			memset(fakeEvent.buf, 0, 64);
			fakeEvent.tag = static_cast<int>(0x8000000a);
			*(static_cast<unsigned char *>(fakeEvent.buf) - 3) &= 0x7f;
			ccs.UnprepareBuffer(ev, original, 10, /*x=*/3);
			bool passthroughOk = true;
			for (int i = 0; i < 10; i++)
				if (static_cast<unsigned char *>(fakeEvent.buf)[3 + i] != original[i]) passthroughOk = false;
			check("UnprepareBuffer() raw passthrough (bit 0x10 clear) writes verbatim at offset x",
			      passthroughOk);

			CEvent::sm_oEvBuffersPool.Free(fakeEvent.buf);

			unsigned char decoded2[10];
			unsigned char decodedLen2 = 200; /* capacity, strictly > dataLen(10) required */
			ccs.PrepareMsgBuffer(decoded2, decodedLen2, original, 10);
			check("PrepareMsgBuffer() raw passthrough (bit 0x10 clear) copies verbatim",
			      decodedLen2 == 10 && memcmp(decoded2, original, 10) == 0);

			/* Capacity-exceeded fatal early exit: outLen left untouched, no
			 * write past the (tiny) capacity.
			 */
			ClientCommServerTestHooks::SetModeService(ccs, 0x10);
			unsigned char tiny[4] = {0xaa, 0xaa, 0xaa, 0xaa};
			unsigned char tinyLen = 0; /* zero capacity -- must fail on the very first write */
			ccs.PrepareMsgBuffer(tiny, tinyLen, original, 10);
			check("PrepareMsgBuffer() fatal capacity exit leaves outLen untouched",
			      tinyLen == 0);
			check("PrepareMsgBuffer() fatal capacity exit does not write anything",
			      tiny[0] == 0xaa && tiny[1] == 0xaa);
		}

		/* --- EventToMessage()/MessageToEvent() -- third follow-up pass. ------
		 * Round-trip through a freshly Alloc()'d CLinkedEvent (MessageToEvent's
		 * own allocation strategy), and the REAL enforcing 6-byte header check.
		 */
		{
			struct { int tag; void *buf; void *next; } ev;
			ev.tag = 0xf; /* MessageToEvent()'s own expected placeholder */
			ev.buf = 0;
			ev.next = 0;
			CLinkedEvent *evPtr = reinterpret_cast<CLinkedEvent *>(&ev);

			unsigned char msg[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
			ccs.MessageToEvent(msg, 6, evPtr);

			int *tagWord = reinterpret_cast<int *>(&ev);
			check("MessageToEvent() leaves the event owning a buffer (tag negative)",
			      *tagWord < 0);
			unsigned char *rawBuf = static_cast<unsigned char *>(ev.buf);
			check("MessageToEvent() writes the fixed 6-byte SysEx header",
			      rawBuf[0] == 0xf0 && rawBuf[1] == 0x42 && rawBuf[3] == 0x60 &&
			          rawBuf[4] == 0x42 /* mEcb */ && rawBuf[5] == 1);

			unsigned char decodedMsg[8];
			unsigned char decodedMsgLen = sizeof(decodedMsg);
			ccs.EventToMessage(evPtr, decodedMsg, decodedMsgLen);
			check("EventToMessage(MessageToEvent(msg)) round-trips: length",
			      decodedMsgLen == 6);
			check("EventToMessage(MessageToEvent(msg)) round-trips: bytes",
			      memcmp(decodedMsg, msg, 6) == 0);

			/* REAL, enforcing header check: corrupt the start-of-sysex byte,
			 * confirm EventToMessage() returns without touching outLen at all
			 * (genuine early return, not a soft/logged assert).
			 */
			unsigned char savedByte0 = rawBuf[0];
			rawBuf[0] = 0x00;
			unsigned char sentinelLen = 0xab;
			ccs.EventToMessage(evPtr, decodedMsg, sentinelLen);
			check("EventToMessage() REAL header check rejects a corrupted start-of-sysex byte"
			      " (outLen left untouched)",
			      sentinelLen == 0xab);
			rawBuf[0] = savedByte0;

			CEvent::sm_oEvBuffersPool.Free(ev.buf);
		}

		/* --- Error() -- third follow-up pass. Real 3-way dispatch on mode's
		 * bits 0/1; every mode combination converges on the same final reset.
		 */
		{
			ClientCommServerTestHooks::SetState0c(ccs, 0x12); /* sentinel */
			ClientCommServerTestHooks::SetUnknown0e(ccs, 0x00); /* sentinel, != 0xff */
			ClientCommServerTestHooks::SetState(ccs, 9); /* sentinel */

			/* mode=0 (neither bit set): no TransmitSysEx() call, but the same
			 * final reset still runs.
			 */
			int callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.Error(static_cast<CClientCommServer::EErrNotifyMode>(0));
			check("Error(0) does not call TransmitSysEx() (mode bit 1 clear)",
			      g_ccsTestTransmitSysExCalls == callsBefore);
			check("Error(0) resets mUnknown0e/mState0d/mState/mUnknown08",
			      ClientCommServerTestHooks::Unknown0e(ccs) == 0xff &&
			          ClientCommServerTestHooks::State0d(ccs) == 0 &&
			          ClientCommServerTestHooks::State(ccs) == 0 &&
			          ClientCommServerTestHooks::Unknown08(ccs) == 0);

			/* mode=2 (bit 1 set, bit 0 clear): DOES call TransmitSysEx() (the
			 * type=4 error-marker send), no vtable dispatch on mClient.
			 */
			ClientCommServerTestHooks::SetState(ccs, 9);
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.Error(static_cast<CClientCommServer::EErrNotifyMode>(2));
			check("Error(2) calls TransmitSysEx() exactly once (mode bit 1 set)",
			      g_ccsTestTransmitSysExCalls == callsBefore + 1);
			check("Error(2) resets state the same way as Error(0)",
			      ClientCommServerTestHooks::State(ccs) == 0 &&
			          ClientCommServerTestHooks::Unknown08(ccs) == 0);

			/* mode=1 (bit 0 set): real vtable-slot-6 dispatch on mClient (a
			 * REAL, live CSysExMsgTaskBase instance, real ctor-installed
			 * vtable) -- exercised for "does not crash" + the same final
			 * reset, since this class's own file does not know (or need to
			 * know) what real method lives at that slot.
			 */
			ClientCommServerTestHooks::SetState(ccs, 9);
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.Error(static_cast<CClientCommServer::EErrNotifyMode>(1));
			check("Error(1) reaches mClient's real vtable slot 6 without crashing"
			      " and does not call TransmitSysEx() (mode bit 1 clear)",
			      g_ccsTestTransmitSysExCalls == callsBefore &&
			          ClientCommServerTestHooks::State(ccs) == 0);
		}

		/* --- OnRxSexWhenInIDLE() -- third follow-up pass. --------------------
		 * type 0 tail-dispatches to OnRxPacket(); type 1 either sends a raw
		 * echo (bit 0x20 clear) or falls to Error() (bit 0x20 set); types
		 * 2/3/4/default all reach the now-real Error().
		 */
		{
			unsigned char pkt[8] = {0x22, 1, 2, 3, 4, 5, 6, 7};

			/* type 0: same real gate OnRxPacket() itself has. */
			ClientCommServerTestHooks::SetModeService(ccs, 0x00); /* gate clear */
			ClientCommServerTestHooks::SetState(ccs, 5);
			int callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxSexWhenInIDLE(static_cast<CClientCommServer::ESexMsgType>(0), pkt, 8, 0x00);
			/* OnRxPacket()'s own real gate blocks dispatch (no TransmitSysEx()
			 * call) but reaches the now-real Error(0), which resets mState to
			 * 0 (IDLE) -- same "Error() is real now" update as the pre-existing
			 * OnRxPacket()/OnRxSexWhenInWAIT() tests above.
			 */
			check("OnRxSexWhenInIDLE(type=0) tail-dispatches to OnRxPacket()'s own real gate"
			      " (blocks send, reaches the now-real Error(0))",
			      g_ccsTestTransmitSysExCalls == callsBefore && ClientCommServerTestHooks::State(ccs) == 0);

			/* type 1, mModeService bit 0x20 CLEAR: raw echo path, builds
			 * mTxBuf via PrepareMsgBuffer() and calls OutMono() (does not
			 * crash, mUnknown1c resets to 0 afterward).
			 */
			ClientCommServerTestHooks::SetModeService(ccs, 0x00); /* bit 0x20 clear */
			unsigned char rawMsg[3] = {0x10, 0x20, 0x30};
			ccs.OnRxSexWhenInIDLE(static_cast<CClientCommServer::ESexMsgType>(1), rawMsg, 3, 0x77);
			check("OnRxSexWhenInIDLE(type=1, bit 0x20 clear) real echo path resets mUnknown1c",
			      ClientCommServerTestHooks::Unknown1c(ccs) == 0);
			const unsigned char *txBuf =
			    static_cast<const unsigned char *>(ClientCommServerTestHooks::TxBuf(ccs));
			check("OnRxSexWhenInIDLE(type=1, bit 0x20 clear) writes x into mTxBuf[0]",
			      txBuf[0] == 0x77);

			/* type 1, mModeService bit 0x20 SET: falls to Error(0) instead. */
			ClientCommServerTestHooks::SetModeService(ccs, 0x20);
			ClientCommServerTestHooks::SetState(ccs, 9);
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxSexWhenInIDLE(static_cast<CClientCommServer::ESexMsgType>(1), rawMsg, 3, 0x00);
			check("OnRxSexWhenInIDLE(type=1, bit 0x20 set) falls to Error(0): state resets",
			      ClientCommServerTestHooks::State(ccs) == 0 && g_ccsTestTransmitSysExCalls == callsBefore);

			/* types 2/3/4/default: all real Error() calls. */
			for (int t = 2; t <= 5; t++) {
				ClientCommServerTestHooks::SetState(ccs, 9);
				ccs.OnRxSexWhenInIDLE(static_cast<CClientCommServer::ESexMsgType>(t), pkt, 8, 0x00);
				char label[80];
				snprintf(label, sizeof(label), "OnRxSexWhenInIDLE(type=%d) reaches real Error()", t);
				check(label, ClientCommServerTestHooks::State(ccs) == 0);
			}
		}

		/* --- OnRxSexWhenInSENT() -- third follow-up pass. --------------------
		 * types 0/1 reset-then-redispatch into OnRxSexWhenInIDLE(); type 2/3
		 * match/mismatch; type 4/default no-ops.
		 */
		{
			unsigned char pkt[8] = {0x33, 1, 2, 3, 4, 5, 6, 7};

			/* type 0: resets to IDLE then redispatches with the SAME type (0)
			 * into OnRxSexWhenInIDLE(), which itself tail-calls OnRxPacket()
			 * -- confirm the real gate fires (mModeService bit 0x20 clear ->
			 * Error(0), state stays IDLE either way here).
			 */
			ClientCommServerTestHooks::SetModeService(ccs, 0x00);
			ClientCommServerTestHooks::SetState(ccs, 5); /* sentinel, must become IDLE */
			ClientCommServerTestHooks::SetUnknown08(ccs, 1);
			ccs.OnRxSexWhenInSENT(static_cast<CClientCommServer::ESexMsgType>(0), pkt, 8, 0x00);
			check("OnRxSexWhenInSENT(type=0) resets mUnknown08 to 0 before redispatch",
			      ClientCommServerTestHooks::Unknown08(ccs) == 0);

			/* type 2 (ack), match: resets to IDLE. */
			ClientCommServerTestHooks::SetState0c(ccs, 0x33);
			ClientCommServerTestHooks::SetState(ccs, 1); /* SENT */
			ClientCommServerTestHooks::SetUnknown0e(ccs, 0x00);
			unsigned char matchData[1] = {0x33};
			ccs.OnRxSexWhenInSENT(static_cast<CClientCommServer::ESexMsgType>(2), matchData, 1, 0);
			check("OnRxSexWhenInSENT(type=2) matching ack resets to IDLE",
			      ClientCommServerTestHooks::State(ccs) == 0 &&
			          ClientCommServerTestHooks::Unknown0e(ccs) == 0xff);

			/* type 2, mismatch: falls to Error(0) (same final reset, via the
			 * shared "give up" tail rather than the match branch's own).
			 */
			ClientCommServerTestHooks::SetState0c(ccs, 0x33);
			ClientCommServerTestHooks::SetState(ccs, 1);
			unsigned char mismatchData[1] = {0x99};
			ccs.OnRxSexWhenInSENT(static_cast<CClientCommServer::ESexMsgType>(2), mismatchData, 1, 0);
			check("OnRxSexWhenInSENT(type=2) mismatched ack still resets via Error(0)",
			      ClientCommServerTestHooks::State(ccs) == 0);

			/* type 3 (retry), match, retry counter under the cap: resends,
			 * bumps mState0d, stays SENT.
			 */
			ClientCommServerTestHooks::SetState0c(ccs, 0x44);
			ClientCommServerTestHooks::SetState(ccs, 1);
			unsigned char before0d = ClientCommServerTestHooks::State0d(ccs);
			int callsBefore = g_ccsTestTransmitSysExCalls;
			unsigned char retryData[1] = {0x44};
			ccs.OnRxSexWhenInSENT(static_cast<CClientCommServer::ESexMsgType>(3), retryData, 1, 0);
			check("OnRxSexWhenInSENT(type=3) matching retry resends and bumps mState0d",
			      g_ccsTestTransmitSysExCalls == callsBefore + 1 &&
			          ClientCommServerTestHooks::State0d(ccs) ==
			              static_cast<unsigned char>(before0d + 1) &&
			          ClientCommServerTestHooks::State(ccs) == 1);

			/* type 4 / default: genuine no-ops -- no TransmitSysEx(), no
			 * state change at all (not even Error()'s own reset).
			 */
			ClientCommServerTestHooks::SetState(ccs, 7); /* sentinel, must stay 7 */
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxSexWhenInSENT(static_cast<CClientCommServer::ESexMsgType>(4), pkt, 8, 0);
			check("OnRxSexWhenInSENT(type=4) is a genuine no-op (state untouched)",
			      ClientCommServerTestHooks::State(ccs) == 7 && g_ccsTestTransmitSysExCalls == callsBefore);
			ccs.OnRxSexWhenInSENT(static_cast<CClientCommServer::ESexMsgType>(9), pkt, 8, 0);
			check("OnRxSexWhenInSENT(type=9, default) is ALSO a genuine no-op",
			      ClientCommServerTestHooks::State(ccs) == 7 && g_ccsTestTransmitSysExCalls == callsBefore);
		}

		/* --- OnRxMsgWhenInIDLE()/OnRxMsgWhenInSENT() -- third follow-up pass.
		 * Checksum-framed path (mModeService bit 0x20 set): verify the
		 * appended CRC byte against an independent reference fold. No-checksum
		 * path (bit 0x20 clear): verify the full reset back to IDLE.
		 */
		{
			unsigned char msg[4] = {0xaa, 0xbb, 0xcc, 0xdd};

			/* No-checksum path: fire-and-forget, full reset to IDLE. */
			ClientCommServerTestHooks::SetModeService(ccs, 0x00);
			ClientCommServerTestHooks::SetState(ccs, 5);
			int callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxMsgWhenInIDLE(msg, 4, 0x55);
			check("OnRxMsgWhenInIDLE() no-checksum path sends once and resets to IDLE",
			      g_ccsTestTransmitSysExCalls == callsBefore + 1 &&
			          ClientCommServerTestHooks::State(ccs) == 0 &&
			          ClientCommServerTestHooks::Unknown0e(ccs) == 0xff);

			/* Checksum-framed path: verify the appended CRC byte independently
			 * (XOR fold over the packed payload, same fold ComputeCRCByte()
			 * itself performs, cross-checked via the existing
			 * ReferenceComputeCRC() helper above).
			 */
			ClientCommServerTestHooks::SetModeService(ccs, 0x20);
			ClientCommServerTestHooks::SetState(ccs, 5);
			ClientCommServerTestHooks::SetState0c(ccs, 0x10);
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxMsgWhenInIDLE(msg, 4, 0x55);
			check("OnRxMsgWhenInIDLE() checksum path transitions to SENT and sends once",
			      ClientCommServerTestHooks::State(ccs) == 1 &&
			          g_ccsTestTransmitSysExCalls == callsBefore + 1);
			check("OnRxMsgWhenInIDLE() checksum path bumps mState0c",
			      ClientCommServerTestHooks::State0c(ccs) == 0x11);

			const unsigned char *evBuf =
			    static_cast<const unsigned char *>(ClientCommServerTestHooks::EvBuf(ccs));
			int tag = ClientCommServerTestHooks::EvTag(ccs);
			unsigned int totalLen = (static_cast<unsigned>(tag) >> 16) & 0xff;
			/* headerOff = kSexPacketOverhead(5)+2 = 7 for the checksum path. */
			unsigned char expectCrc = ReferenceComputeCRC(evBuf, static_cast<unsigned char>(totalLen - 1), 7);
			check("OnRxMsgWhenInIDLE() checksum path appends the correct running-XOR CRC byte",
			      evBuf[totalLen - 1] == expectCrc);
			check("OnRxMsgWhenInIDLE() checksum path's 2-byte mini-header: type=0, seq=mState0c",
			      evBuf[5] == 0 && evBuf[6] == 0x11);

			/* OnRxMsgWhenInSENT(): sets mUnknown08=1, then tail-redispatches
			 * into OnRxMsgWhenInIDLE() unchanged.
			 */
			ClientCommServerTestHooks::SetModeService(ccs, 0x00);
			ClientCommServerTestHooks::SetState(ccs, 5);
			ClientCommServerTestHooks::SetUnknown08(ccs, 0);
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxMsgWhenInSENT(msg, 4, 0x66);
			check("OnRxMsgWhenInSENT() redispatches into OnRxMsgWhenInIDLE(): same real send"
			      " and reset to IDLE",
			      g_ccsTestTransmitSysExCalls == callsBefore + 1 &&
			          ClientCommServerTestHooks::State(ccs) == 0);
		}

		/* --- OnReceiveMessage(const CMessage&) -- 2026-07-27 closeout pass.
		 * Real ground truth (.text+0x08172010, 784 bytes) reads 3 fixed
		 * CMessage offsets (msg+0x10 data ptr, msg+0xa len byte, msg+0x4 an
		 * opaque 3-level pointer chase to a byte@+0x8c "x" tag) and dispatches
		 * on mState (IDLE/SENT/WAIT/default) through the 3 already-real
		 * Msg*WhenIn* siblings, with a real `(result < 1) ? -1 : 0` transform
		 * on the IDLE/SENT cases (ground truth's own `cmp eax,1`/`sbb eax,eax`).
		 * See client_comm_server.h's own header comment on this method for the
		 * full derivation. Fake-CMessage buffer built the same "raw bytes, not
		 * a struct" way test_poller.cpp's own FakeMessage already established.
		 */
		{
			unsigned char arrBuf[0x8d];
			std::memset(arrBuf, 0, sizeof(arrBuf));
			arrBuf[0x8c] = 0x77; /* the real "x" tag byte the chase ends on */

			unsigned char resBuf[0x24];
			std::memset(resBuf, 0, sizeof(resBuf));
			*reinterpret_cast<void **>(resBuf + 0x20) = arrBuf;

			unsigned char linkBuf[8];
			std::memset(linkBuf, 0, sizeof(linkBuf));
			*reinterpret_cast<void **>(linkBuf + 0x4) = resBuf;

			unsigned char payload[4] = {0xde, 0xad, 0xbe, 0xef};
			unsigned char msgBuf[0x18];
			const CMessage &fakeMsg = *reinterpret_cast<const CMessage *>(msgBuf);

			/* IDLE dispatch: sends once via OnRxMsgWhenInIDLE(); TransmitSysEx()'s
			 * host-build stub always returns 0, so the real (<1)?-1:0 transform
			 * yields -1.
			 */
			SetupFakeMsg(msgBuf, linkBuf, 4, payload);
			ClientCommServerTestHooks::SetModeService(ccs, 0x00);
			ClientCommServerTestHooks::SetState(ccs, 0); /* IDLE */
			int callsBefore = g_ccsTestTransmitSysExCalls;
			int r = ccs.OnReceiveMessage(fakeMsg);
			check("OnReceiveMessage() IDLE dispatch sends once via OnRxMsgWhenInIDLE()",
			      g_ccsTestTransmitSysExCalls == callsBefore + 1);
			check("OnReceiveMessage() IDLE dispatch resets back to IDLE",
			      ClientCommServerTestHooks::State(ccs) == 0);
			check("OnReceiveMessage() IDLE dispatch: real (<1)?-1:0 transform on "
			      "TransmitSysEx()'s stubbed 0 return yields -1",
			      r == -1);
			check("OnReceiveMessage() correctly threads the msg+0x4 opaque "
			      "pointer-chase 'x' tag byte through into mEvTag's high byte",
			      ((static_cast<unsigned>(ClientCommServerTestHooks::EvTag(ccs)) >> 8) &
			       0xff) == 0x77);

			/* SENT dispatch: ground truth re-inlines OnRxMsgWhenInSENT()'s own
			 * body here rather than calling it -- same observable effect
			 * (mUnknown08=1, then tail-redispatch into OnRxMsgWhenInIDLE()).
			 */
			SetupFakeMsg(msgBuf, linkBuf, 4, payload);
			ClientCommServerTestHooks::SetModeService(ccs, 0x00);
			ClientCommServerTestHooks::SetState(ccs, 1); /* SENT */
			ClientCommServerTestHooks::SetUnknown08(ccs, 0);
			callsBefore = g_ccsTestTransmitSysExCalls;
			r = ccs.OnReceiveMessage(fakeMsg);
			check("OnReceiveMessage() SENT dispatch redispatches into "
			      "OnRxMsgWhenInIDLE(): one real send",
			      g_ccsTestTransmitSysExCalls == callsBefore + 1);
			check("OnReceiveMessage() SENT dispatch resets back to IDLE",
			      ClientCommServerTestHooks::State(ccs) == 0);
			check("OnReceiveMessage() SENT dispatch: same (<1)?-1:0 transform "
			      "yields -1",
			      r == -1);

			/* WAIT dispatch: ground truth's own inlined copy of
			 * OnRxMsgWhenInWAIT() always returns 0 (unconditional `xor eax,eax`)
			 * regardless of TransmitSysEx() -- no send at all, straight to
			 * Error()'s own reset.
			 */
			SetupFakeMsg(msgBuf, linkBuf, 4, payload);
			ClientCommServerTestHooks::SetState(ccs, 2); /* WAIT */
			callsBefore = g_ccsTestTransmitSysExCalls;
			r = ccs.OnReceiveMessage(fakeMsg);
			check("OnReceiveMessage() WAIT dispatch does not send (goes straight "
			      "to Error())",
			      g_ccsTestTransmitSysExCalls == callsBefore);
			check("OnReceiveMessage() WAIT dispatch resets back to IDLE (Error() "
			      "reset)",
			      ClientCommServerTestHooks::State(ccs) == 0);
			check("OnReceiveMessage() WAIT dispatch always returns 0", r == 0);

			/* Invalid/default mState: soft-log-only, returns -1, no send, no
			 * state change at all.
			 */
			SetupFakeMsg(msgBuf, linkBuf, 4, payload);
			ClientCommServerTestHooks::SetState(ccs, 9); /* sentinel, must stay 9 */
			callsBefore = g_ccsTestTransmitSysExCalls;
			r = ccs.OnReceiveMessage(fakeMsg);
			check("OnReceiveMessage() invalid mState is a genuine no-op (no send)",
			      g_ccsTestTransmitSysExCalls == callsBefore);
			check("OnReceiveMessage() invalid mState leaves mState untouched",
			      ClientCommServerTestHooks::State(ccs) == 9);
			check("OnReceiveMessage() invalid mState returns -1", r == -1);
		}

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
