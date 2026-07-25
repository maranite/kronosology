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
			ClientCommServerTestHooks::SetState(ccs, 5 /* sentinel, must stay unchanged */);
			int callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxPacket(pkt, 8, 0x33);
			check("OnRxPacket() with mModeService bit 0x20 clear does NOT dispatch"
			      " (real early-return gate, Error() is a no-op stub)",
			      g_ccsTestTransmitSysExCalls == callsBefore &&
			          ClientCommServerTestHooks::State(ccs) == 5);

			/* Now set the gate and mUnknown0e = 0xff ("no active tag yet") so the
			 * tag-mismatch soft-assert path isn't taken either.
			 */
			ClientCommServerTestHooks::SetModeService(ccs, 0x20);
			ClientCommServerTestHooks::SetUnknown0e(ccs, 0xff);
			ClientCommServerTestHooks::SetState(ccs, 5);

			/* len<=2: real "too short" resync path -- does NOT touch
			 * PrepareMsgBuffer() (still a Tier-B no-op stub), so this exercises
			 * OnRxPacket()'s own real control flow end to end.
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
			check("OnRxSexWhenInWAIT(type=0) tail-calls OnRxPacket(): same real gate"
			      " blocks it when mModeService bit 0x20 is clear",
			      g_ccsTestTransmitSysExCalls == callsBefore &&
			          ClientCommServerTestHooks::State(ccs) == 5);

			ClientCommServerTestHooks::SetModeService(ccs, 0x20);
			ClientCommServerTestHooks::SetUnknown0e(ccs, 0xff);
			ClientCommServerTestHooks::SetState(ccs, 5);
			callsBefore = g_ccsTestTransmitSysExCalls;
			ccs.OnRxSexWhenInWAIT(static_cast<CClientCommServer::ESexMsgType>(0), pkt, 2, 0x00);
			check("OnRxSexWhenInWAIT(type=0) tail-call reaches OnRxPacket()'s own"
			      " len<=2 resync path (mState -> 2)",
			      ClientCommServerTestHooks::State(ccs) == 2 &&
			          g_ccsTestTransmitSysExCalls == callsBefore + 1);

			/* type 1-4: Error()-only paths (Error() is a Tier-B no-op stub) --
			 * confirm the dispatch itself doesn't crash and touches no state.
			 */
			for (int t = 1; t <= 4; t++) {
				ClientCommServerTestHooks::SetState(ccs, 9);
				callsBefore = g_ccsTestTransmitSysExCalls;
				ccs.OnRxSexWhenInWAIT(static_cast<CClientCommServer::ESexMsgType>(t), pkt, 2,
				                       0x00);
				char label[112];
				snprintf(label, sizeof(label),
				         "OnRxSexWhenInWAIT(type=%d) dispatches without touching mState/"
				         "TransmitSysEx (Error() stub)",
				         t);
				check(label, ClientCommServerTestHooks::State(ccs) == 9 &&
				                  g_ccsTestTransmitSysExCalls == callsBefore);
			}

			/* type>4: out-of-range, real ground truth returns without even
			 * calling Error() -- same observable effect as the Error()-stub
			 * cases above given Error() is a no-op, but structurally a
			 * different code path (see header comment).
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
