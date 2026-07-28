/*
 * test_chunk_client.cpp  -  host-side known-answer test for CChkItem/
 * CDumpReqDescr/CDumpHeaderDescr/CChunkClient (src/dump/chunk_client.cpp). See
 * include/chunk_client.h for full ground-truth provenance.
 */

#include <cstdio>
#include <cstring>

#include "chunk_client.h"
#include "module.h"
#include "omega_ptr_array.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* ---- fake ChkApi, same convention as test_partition_table.cpp's fake Api:
 * ChkApi is a real global defined in mains.cpp (void *ChkApi = 0;), zero unless
 * mains.cpp's own MMainChunkMan() ran (which this bare test doesn't do) -- a
 * genuine null-vtable deref otherwise, so install a minimal fake object with a
 * real vtbl+0x40 slot before calling CChunkClient::LoadResSync(). ----
 */
extern void *ChkApi;

static void *g_capturedChkApiObj;
static unsigned char g_capturedCommId;
static void *g_capturedChunk;
static const void *g_capturedElems;

extern "C" void FakeChkApiLoadResSync(void *obj, unsigned char commId, void *chunk,
                                       const void *elems)
{
	g_capturedChkApiObj = obj;
	g_capturedCommId = commId;
	g_capturedChunk = chunk;
	g_capturedElems = elems;
}

static void *g_fakeChkApiVtable[0x40 / 4 + 1];
struct FakeChkApiObj { void *vtbl; } g_fakeChkApiObj;

static void InstallFakeChkApi()
{
	for (unsigned i = 0; i < sizeof(g_fakeChkApiVtable) / sizeof(g_fakeChkApiVtable[0]); ++i)
		g_fakeChkApiVtable[i] = 0;
	g_fakeChkApiVtable[0x40 / 4] = (void *)FakeChkApiLoadResSync;
	g_fakeChkApiObj.vtbl = g_fakeChkApiVtable;
	ChkApi = &g_fakeChkApiObj;
}

/* Friend accessor -- same convention as buffering_task.h's BufferingTaskTestHooks/
 * sysex_msg_task_base.h's SysExMsgTaskBaseTestHooks: reach private fields a
 * fresh-object test otherwise can't observe (no subclass exists to override the
 * IsXxxToBeExecuted() hooks, so most Save/Load "success" paths are unreachable
 * from a bare host test -- state-transition/cleanup assertions below are what's
 * actually observable).
 */
struct ChunkClientTestHooks {
	static unsigned char CommId(const CChunkClient &c) { return c.mCommId; }
	static int State(const CChunkClient &c) { return c.mState; }
	static int PendingCount(const CChunkClient &c) { return c.mPendingCount; }
	static unsigned long ByteCount(const CChunkClient &c) { return c.mByteCount; }
	static COutLinkMono *OutLink(const CChunkClient &c) { return c.mOutLinkMono; }
	static COmegaPtrArray *ChkItemArray(const CChunkClient &c) { return c.mChkItemArray; }
	static void *Scratch(const CChunkClient &c) { return c.mScratch; }
	static void SetState(CChunkClient &c, int s) { c.mState = s; }
	static void SetPendingCount(CChunkClient &c, int p) { c.mPendingCount = p; }
	static int PrepareList(CChunkClient &c, const CDumpReqDescr *req, COmegaPtrArray &out)
	{
		return c.PrepareList(req, out);
	}
};

/* Real CMessage layout (per Exec(CMessage&)'s own decompile, same convention as
 * test_sysex_msg_task_base.cpp's FakeMessage): +0x08 a 2-byte flags/command
 * field (bit 0x100 = "has command", low byte = command), +0x10 a 4-byte payload.
 */
struct FakeMessage {
	unsigned char pad0[8];
	unsigned short flagsAndCmd; /* +0x08 */
	unsigned char pad0a[6];     /* +0x0a..0x10 */
	unsigned long payload;      /* +0x10 */
};

int main()
{
	InstallFakeChkApi();

	printf("CChkItem/CDumpReqDescr/CDumpHeaderDescr/CChunkClient known-answer test\n");
	printf("========================================================================\n");

	printf("[1] CChkItem ctor + Serialize round-trip\n");
	{
		const unsigned char payload[3] = {0x11, 0x22, 0x33};
		CChkItem item(0x42, 3, payload);
		unsigned char buf[8];
		int n = item.Serialize(buf);
		check("Serialize returns len+2", n == 5);
		check("type byte", buf[0] == 0x42);
		check("len byte", buf[1] == 3);
		check("payload bytes", memcmp(buf + 2, payload, 3) == 0);
	}

	printf("[2] CChkItem default ctor + DeSerialize round-trip\n");
	{
		unsigned char buf[5] = {0x07, 2, 0xaa, 0xbb};
		CChkItem item;
		item.DeSerialize(buf);
		unsigned char out[8];
		int n = item.Serialize(out);
		check("round-trips through Serialize", n == 4 && memcmp(out, buf, 4) == 0);
	}

	printf("[3] CDumpReqDescr::SetMicro + Serialize/DeSerialize round-trip\n");
	{
		const unsigned char data[4] = {1, 2, 3, 4};
		CDumpReqDescr req;
		req.SetMicro(0x55, 4, data);
		check("GetType() == 1 (micro)", req.GetType() == 1);
		check("GetMicroId()", req.GetMicroId() == 0x55);
		check("GetLen()", req.GetLen() == 4);

		unsigned char buf[16];
		unsigned n = req.Serialize(buf, sizeof(buf));
		check("Serialize len", n == 7);
		check("Serialize bytes", buf[0] == 1 && buf[1] == 0x55 && buf[2] == 4 &&
		                             memcmp(buf + 3, data, 4) == 0);

		CDumpReqDescr req2;
		unsigned n2 = req2.DeSerialize(buf, (unsigned char)n);
		check("DeSerialize len", n2 == 7);
		check("DeSerialize round-trip", req2.GetType() == 1 && req2.GetMicroId() == 0x55 &&
		                                    req2.GetLen() == 4 &&
		                                    memcmp(req2.GetData(), data, 4) == 0);
	}

	printf("[4] CDumpReqDescr::SetSingle + Reset\n");
	{
		const unsigned char data[2] = {0xaa, 0xbb};
		CDumpReqDescr req;
		req.SetSingle(7, 2, data);
		check("GetType() == 2 (single)", req.GetType() == 2);
		check("GetResourceId()", req.GetResourceId() == 7);
		req.Reset();
		check("Reset clears type", req.GetType() == 0);
		check("Reset sets micro sentinel", req.GetMicroId() == 0xff);
		check("Reset clears len", req.GetLen() == 0);
	}

	printf("[5] CDumpReqDescr::operator= deep copy\n");
	{
		const unsigned char data[3] = {9, 8, 7};
		CDumpReqDescr a;
		a.SetMicro(3, 3, data);
		CDumpReqDescr b;
		b = a;
		check("copy has same type/id/len", b.GetType() == 1 && b.GetMicroId() == 3 && b.GetLen() == 3);
		check("copy has its OWN data buffer", b.GetData() != a.GetData());
		check("copy data matches", memcmp(b.GetData(), data, 3) == 0);
	}

	printf("[6] CDumpHeaderDescr::SetMicro + Serialize/DeSerialize round-trip (incl. byteCount)\n");
	{
		const unsigned char data[2] = {0x10, 0x20};
		CDumpHeaderDescr hdr;
		hdr.SetMicro(9, 2, data, 0x11223344UL);
		check("GetByteCount()", hdr.GetByteCount() == 0x11223344UL);

		unsigned char buf[16];
		unsigned n = hdr.Serialize(buf, sizeof(buf));
		check("Serialize len (base 5 + 4 byteCount)", n == 9);
		check("byteCount big-endian", buf[5] == 0x11 && buf[6] == 0x22 && buf[7] == 0x33 &&
		                                  buf[8] == 0x44);

		CDumpHeaderDescr hdr2;
		unsigned n2 = hdr2.DeSerialize(buf, (unsigned char)n);
		check("DeSerialize len", n2 == 9);
		check("DeSerialize round-trip", hdr2.GetType() == 1 && hdr2.GetMicroId() == 9 &&
		                                    hdr2.GetByteCount() == 0x11223344UL);
	}

	printf("[7] CDumpHeaderDescr::operator= (both overloads)\n");
	{
		const unsigned char data[1] = {0x5a};
		CDumpHeaderDescr src;
		src.SetSingle(2, 1, data, 0xdeadbeefUL);

		CDumpHeaderDescr dstA;
		dstA = src; /* CDumpHeaderDescr const& overload -- byteCount copied */
		check("operator=(CDumpHeaderDescr const&) copies byteCount", dstA.GetByteCount() == 0xdeadbeefUL);

		CDumpReqDescr plain;
		plain.SetMicro(1, 1, data);
		CDumpHeaderDescr dstB;
		dstB.SetSingle(0, 0, 0, 0x99); /* seed a nonzero byteCount first */
		dstB = plain; /* CDumpReqDescr const& overload -- byteCount zeroed, NOT copied */
		check("operator=(CDumpReqDescr const&) zeroes byteCount", dstB.GetByteCount() == 0);
		check("operator=(CDumpReqDescr const&) copies base fields", dstB.GetType() == 1 &&
		                                                                 dstB.GetMicroId() == 1);
	}

	CModule owner("TestChunkMan");

	printf("[8] CChunkClient ctor -- real field initialization\n");
	{
		CChunkClient c(owner);
		check("mCommId starts 0xff", ChunkClientTestHooks::CommId(c) == 0xff);
		check("mState starts 0 (idle)", ChunkClientTestHooks::State(c) == 0);
		check("mPendingCount starts 0", ChunkClientTestHooks::PendingCount(c) == 0);
		check("mOutLinkMono constructed", ChunkClientTestHooks::OutLink(c) != 0);
		check("mChkItemArray starts null", ChunkClientTestHooks::ChkItemArray(c) == 0);
		check("mScratch starts null", ChunkClientTestHooks::Scratch(c) == 0);
	}

	printf("[9] Abort()/StoppedByUser() -- base class always no-ops (IsXxx defaults false)\n");
	{
		CChunkClient c(owner);
		ChunkClientTestHooks::SetState(c, 5); /* even with a session "active"... */
		check("Abort() returns false", c.Abort() == false);
		check("StoppedByUser() returns false", c.StoppedByUser() == false);
		check("state unchanged (no override -> no-op)", ChunkClientTestHooks::State(c) == 5);
	}

	printf("[10] SaveDump/LoadDump/SaveFile/LoadFile -- base class IsXxx always false -> fail cleanly\n");
	{
		CChunkClient c(owner);
		CDumpReqDescr req;
		CDumpHeaderDescr hreq;
		check("SaveDump() fails (no override)", c.SaveDump(req) == false);
		check("LoadDump() fails (no override)", c.LoadDump(hreq) == false);
		check("SaveFile() fails (no override)", c.SaveFile("test.bin", req) == false);
		check("LoadFile() fails (no override)", c.LoadFile("test.bin", hreq) == false);
		check("state stays idle after all 4 failures", ChunkClientTestHooks::State(c) == 0);
	}

	printf("[11] PrepareList() -- all 3 real descriptor-type branches\n");
	{
		CChunkClient c(owner);
		COmegaPtrArray list(5, 5, 1);

		const unsigned char data[2] = {0x01, 0x02};
		CDumpReqDescr microReq;
		microReq.SetMicro(0x33, 2, data);
		int rc1 = ChunkClientTestHooks::PrepareList(c, &microReq, list);
		check("PrepareList(micro) returns 1", rc1 == 1);
		check("PrepareList(micro) appended one CChkItem", list.Count() == 1);

		CDumpReqDescr singleReq;
		singleReq.SetSingle(1, 2, data);
		int rc2 = ChunkClientTestHooks::PrepareList(c, &singleReq, list);
		check("PrepareList(single) forwards to OnPrepareSingle() -> 0 (base no-op)", rc2 == 0);

		CDumpReqDescr emptyReq; /* GetType() == 0, freshly constructed */
		int rc3 = ChunkClientTestHooks::PrepareList(c, &emptyReq, list);
		check("PrepareList(unknown type) returns 0", rc3 == 0);
	}

	printf("[12] OnPrepareMicro() -- real, separately reachable\n");
	{
		CChunkClient c(owner);
		COmegaPtrArray list(5, 5, 1);
		const unsigned char data[1] = {0xee};
		CDumpReqDescr req;
		req.SetMicro(0x21, 1, data);
		int rc = c.OnPrepareMicro(&req, list);
		check("OnPrepareMicro() returns 1", rc == 1);
		check("OnPrepareMicro() appended one CChkItem", list.Count() == 1);
	}

	printf("[13] LoadRes/SaveRes/MergeRes/LoadResSync -- ungated (no IsXxx hook), real internal\n"
	       "     logic exercised even though OutMono() itself reports failure (no live receiver\n"
	       "     wired up in this bare host test)\n");
	{
		CChunkClient c(owner);
		COmegaPtrArray elems(5, 5, 1);
		unsigned char rec[0x38];
		memset(rec, 0, sizeof(rec));
		elems.Add(rec); /* raw stack buffer -- CopyElemArray() only memcpy's it, never frees it */

		bool rcLoadRes = c.LoadRes(0, elems, 3);
		check("LoadRes() runs its real body (state -> 6)", ChunkClientTestHooks::State(c) == 6);
		check("LoadRes() OutMono has no live receiver -> false", rcLoadRes == false);

		CChunkClient c2(owner);
		bool rcSaveRes = c2.SaveRes("resname", elems);
		check("SaveRes() runs its real body (state -> 3)", ChunkClientTestHooks::State(c2) == 3);
		check("SaveRes() OutMono has no live receiver -> false", rcSaveRes == false);

		CChunkClient c3(owner);
		bool rcMerge = c3.MergeRes(0, 0, &elems, 0xabcdU);
		check("MergeRes() runs its real body (state -> 9)", ChunkClientTestHooks::State(c3) == 9);
		check("MergeRes() stores flags into mByteCount", ChunkClientTestHooks::ByteCount(c3) == 0xabcdU);
		check("MergeRes() OutMono has no live receiver -> false", rcMerge == false);

		CChunkClient c4(owner);
		c4.LoadResSync((CResourceChunk *)0x1234, elems);
		check("LoadResSync() forwards through ChkApi's own vtbl+0x40",
		      g_capturedChkApiObj == &g_fakeChkApiObj && g_capturedCommId == 0xff &&
		          g_capturedChunk == (void *)0x1234 && g_capturedElems == &elems);
	}

	printf("[14] Exec(CMessage&) -- ECB dispatch\n");
	{
		CChunkClient c(owner);

		FakeMessage noFlag;
		memset(&noFlag, 0, sizeof(noFlag));
		noFlag.flagsAndCmd = 0xe0; /* 0x100 bit NOT set */
		int rc0 = c.Exec(*reinterpret_cast<CMessage *>(&noFlag));
		check("Exec() without 0x100 flag returns -1", rc0 == -1);

		FakeMessage setCommId;
		memset(&setCommId, 0, sizeof(setCommId));
		setCommId.flagsAndCmd = 0x100 | 0xe4;
		setCommId.payload = 0x77;
		int rc4 = c.Exec(*reinterpret_cast<CMessage *>(&setCommId));
		check("Exec(0xe4) returns 0", rc4 == 0);
		check("Exec(0xe4) sets mCommId from payload", ChunkClientTestHooks::CommId(c) == 0x77);

		FakeMessage acceptRq;
		memset(&acceptRq, 0, sizeof(acceptRq));
		acceptRq.flagsAndCmd = 0x100 | 0xe1;
		acceptRq.payload = 0x99;
		int rc1 = c.Exec(*reinterpret_cast<CMessage *>(&acceptRq));
		check("Exec(0xe1) returns 0", rc1 == 0);
		check("Exec(0xe1) sets mByteCount from payload", ChunkClientTestHooks::ByteCount(c) == 0x99);

		FakeMessage byteCount;
		memset(&byteCount, 0, sizeof(byteCount));
		byteCount.flagsAndCmd = 0x100 | 0xe3;
		int rc3idle = c.Exec(*reinterpret_cast<CMessage *>(&byteCount));
		check("Exec(0xe3) with idle state still returns 0", rc3idle == 0);

		FakeMessage unknown;
		memset(&unknown, 0, sizeof(unknown));
		unknown.flagsAndCmd = 0x100 | 0x7f;
		int rcU = c.Exec(*reinterpret_cast<CMessage *>(&unknown));
		check("Exec() with unknown ECB returns -1", rcU == -1);
	}

	printf("[15] Exec(0xe2) -- internal-abort path, drives FailAndReset()/Reset()\n");
	{
		CChunkClient c(owner);
		ChunkClientTestHooks::SetState(c, 7);         /* Abort in progress */
		ChunkClientTestHooks::SetPendingCount(c, 1);  /* one outstanding op */

		FakeMessage msg;
		memset(&msg, 0, sizeof(msg));
		msg.flagsAndCmd = 0x100 | 0xe2;
		msg.payload = 0; /* real: a COmegaPtrArray* work-list pointer, 0 is safe --
		                  * DeleteSelfViaVSlot()/OnSingleEnd() both null-check. */
		int rc = c.Exec(*reinterpret_cast<CMessage *>(&msg));
		check("Exec(0xe2) returns 0", rc == 0);
		check("Exec(0xe2) drove pendingCount to 0 -> mState reset to idle",
		      ChunkClientTestHooks::State(c) == 0);
		check("Exec(0xe2) tore down mChkItemArray/mScratch", ChunkClientTestHooks::ChkItemArray(c) == 0 &&
		                                                          ChunkClientTestHooks::Scratch(c) == 0);
	}

	printf("[16] Exec(0xe5) -- 2 real state-machine shapes (simple end vs. res-family end)\n");
	{
		/* Shape A: mState not in {3,6,9} (e.g. a SaveFile-style session) -->
		 * OnEnd(scratch, header) + mState=0 + Reset(), unconditionally.
		 */
		CChunkClient cA(owner);
		ChunkClientTestHooks::SetState(cA, 1);
		ChunkClientTestHooks::SetPendingCount(cA, 3); /* real: decremented BEFORE the branch */
		FakeMessage msgA;
		memset(&msgA, 0, sizeof(msgA));
		msgA.flagsAndCmd = 0x100 | 0xe5;
		int rcA = cA.Exec(*reinterpret_cast<CMessage *>(&msgA));
		check("Exec(0xe5) shape A returns 0", rcA == 0);
		check("Exec(0xe5) shape A resets mState unconditionally",
		      ChunkClientTestHooks::State(cA) == 0);

		/* Shape B: mState in {3,6,9} (a res-family session) -- OnSingleEnd(),
		 * then only reset once mPendingCount (already decremented) hits 0.
		 */
		CChunkClient cB(owner);
		ChunkClientTestHooks::SetState(cB, 6);
		ChunkClientTestHooks::SetPendingCount(cB, 2); /* -> 1 after decrement, NOT 0 yet */
		FakeMessage msgB;
		memset(&msgB, 0, sizeof(msgB));
		msgB.flagsAndCmd = 0x100 | 0xe5;
		msgB.payload = 0;
		int rcB = cB.Exec(*reinterpret_cast<CMessage *>(&msgB));
		check("Exec(0xe5) shape B returns 0", rcB == 0);
		check("Exec(0xe5) shape B does NOT reset state while pending > 0",
		      ChunkClientTestHooks::State(cB) == 6);
		check("Exec(0xe5) shape B pendingCount decremented", ChunkClientTestHooks::PendingCount(cB) == 1);

		CChunkClient cC(owner);
		ChunkClientTestHooks::SetState(cC, 9);
		ChunkClientTestHooks::SetPendingCount(cC, 1); /* -> 0 after decrement */
		FakeMessage msgC;
		memset(&msgC, 0, sizeof(msgC));
		msgC.flagsAndCmd = 0x100 | 0xe5;
		int rcC = cC.Exec(*reinterpret_cast<CMessage *>(&msgC));
		check("Exec(0xe5) shape B resets once pendingCount reaches 0", rcC == 0 &&
		                                                                   ChunkClientTestHooks::State(cC) == 0);
	}

	printf("========================================================================\n");
	if (g_fail) {
		printf("%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
